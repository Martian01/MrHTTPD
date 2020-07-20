/*

mrhttpd v2.5.0
Copyright (c) 2007-2020  Martin Rogge <martin_rogge@users.sourceforge.net>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "mrhttpd.h"

#define RECEIVE_TIMEOUT 30
#define SEND_TIMEOUT 5

void setTimeout(const int socket) {
	struct timeval timeout;

	timeout.tv_sec  = RECEIVE_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	timeout.tv_sec  = SEND_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

int parseHeader(const int socket, MemPool *buffer, StringPool *headerPool) {
	ssize_t received;
	int cursor;
	int delim;

	stringPoolReset(headerPool);

_startHeader:
	memPoolReset(buffer);

_moreHeader:
	#if DEBUG & 8
	Log(socket, "parseHeader: read loop iteration.");
	#endif

	received = recv(socket, buffer->mem + buffer->current, buffer->size - buffer->current, 0);
	if (received == 0) {
		#if DEBUG & 8
		Log(socket, "parseHeader: recv strangeness. received=0");
		#endif
		return -ESTRANGE; // error
	}
	if (received < 0) {
		#if DEBUG & 8
		Log(socket, "parseHeader: recv error. received=%d, errno=%d", received, errno);
		#endif
		//if (errno == EAGAIN)
		//	goto _moreHeader;
		return received; // propagate error
	}
	#if DEBUG & 8
	Log(socket, "parseHeader: recv OK. received=%d", received);
	#endif
	buffer->current += received;
	cursor = 0; // cursor positioned on beginning of line

_nextHeaderLine:
	delim = memPoolLineBreak(buffer, cursor); // search for end of line
	#if DEBUG & 16
	Log(socket, "parseHeader: parser loop iteration. cursor=%d, delim=%d, end=%d", cursor, delim, buffer->current);
	#endif
	if (delim < 0) { // end of line not found
		if (cursor > 0) { // make some space and keep reading
			buffer->current -= cursor;
			memmove(buffer->mem, buffer->mem + cursor, buffer->current);
			goto _moreHeader;
		}
		if (buffer->current < buffer->size) { // buffer not full: keep reading
			goto _moreHeader;
		}
		return -1; // buffer full: we're bloated
	}
	buffer->mem[delim] = '\0'; // make cursor a zero-terminated string
	if (cursor != delim) { // line is not empty
		if (stringPoolAdd(headerPool, buffer->mem + cursor)) // add string to header pool
		  return -1; // header pool full or whatever
		#if DEBUG & 32
		Log(socket, "parseHeader: parser found header at index: %d", cursor);
		#endif
		cursor = delim + 2; // beginning of next line
		if (cursor < buffer->current)
			goto _nextHeaderLine; // find next line in buffer
		else
			goto _startHeader; // buffer exhausted, read more data
	}

	// else: empty line found == end of header

	cursor = delim + 2;
	if (cursor < buffer->current) { // there is some data left in the buffer, most likely the beginning of the request body.
		buffer->current -= cursor;
		memmove(buffer->mem, buffer->mem + cursor, buffer->current); // return the overspill via the memory pool.
		#if DEBUG & 64
		Log(socket, "parseHeader: overspill returned, amount=%d", buffer->current);
		#endif
	} else { // no data left in the buffer
		memPoolReset(buffer); // initialize the buffer
		#if DEBUG & 64
		Log(socket, "parseHeader: spot landing (no overspill).");
		#endif
	}

	return headerPool->current > 0 ? 0 : -1; // success if and only if there is a non-empty header line
}

/*#ifdef PUT_PATH

int parseMultiPartRequest(const int socket, const char *boundary,  MemPool *buffer, char *fileName) {
	ssize_t received;
	int parseHeader;
	int foundfile;
	int cursor;
	int delim;

	int beaconLength = strlen(boundary) + 2;
	if (beaconLength > 127)
		return -1; // out of memory
	char beacon[128] = "--";
	strcpy(beacon + 2, boundary);
	parseHeader = foundFile = 0;
	//cursor = buffer->mem;
	cursor = 0;
	goto _nextSegment;

_startMultiPart:
	memPoolReset(buffer);

_moreMultiPart:
	#if DEBUG & 8
	Log(socket, "parseMultiPart: read loop iteration.");
	#endif

	received = recv(socket, buffer->mem + buffer->current, buffer->size - buffer->current, 0);
	if (received == 0) {
		#if DEBUG & 8
		Log(socket, "parseMultiPart: recv strangeness. received=%d", received);
		#endif
		return -ESTRANGE; // error
	}
	if (received < 0) {
		#if DEBUG & 8
		Log(socket, "parseMultiPart: recv error. received=%d, errno=%d", received, errno);
		#endif
		//if (errno == EAGAIN)
		//	goto _moreHeader;
		return received; // propagate error
	}
	#if DEBUG & 8
	Log(socket, "parseMultiPart: recv OK. received=%d", received);
	#endif
	buffer->current += received;
	cursor = 0;

_nextMultiPartLine:
	delim = memPoolLineBreak(buffer, cursor); // search for end of line
	#if DEBUG & 16
	Log(socket, "parseMultiPart: parser loop iteration. cursor=%d, delim=%d, end=%d", cursor, delim, buffer->current);
	#endif
	if (delim < 0) { // end of line not found
		if (cursor > 0) { // make some space and keep reading
			buffer->current -= cursor;
			memmove(buffer->mem, buffer->mem + cursor, buffer->current);
			goto _moreMultiPart;
		}
		if (buffer->current < buffer->size) { // buffer not full: keep reading
			goto _moreMultiPart;
		}
		return -1; // buffer full: we're bloated
	}
	buffer->mem[delim] = '\0'; // make cursor a zero-terminated string
	if (!strncmp(cursor, beacon, beaconLength)) {
		if (cursor[beaconLength] == '\0') {
			parseHeader = 1;
			foundFile = 0;
			goto _endOfMultiPartLine;
		}
		else if (cursor[beaconLength] == '-' && cursor[beaconLength + 1] == '-' && cursor[beaconLength + 2] == '\0') {
			return -1; // file content not found
		}
	}
	if (parseHeader) {
		if (cursor == delim) {
			if (foundFile) {
				goto _foundMultiPartFileContent;
			} else
				parseHeader = 0;
		} else if (!strcmp(cursor, "Content-Disposition:", 20)) {
			cursor += 20;
			while (cursor != NULL && *cursor != '\0') {
				char *detail = strsep(&cursor, ";");
				if (detail != NULL && *detail != '\0') {
					detail = startOf(detail);
					detailLength = strlen(detail);
					if (!strncmp(detail, "filename=\"", 10) && detail[detailLength - 1] == '\"') {
						if (detailLength > 138)
							return -1; // file name too long
						detail[detailLength - 1] = '\0';
						strcpy(fileName, detail + 10);
						foundFile = 1;
					}
				}
			}
		}
	}

_endOfMultiPartLine:

	cursor = delim + 2; // beginning of next line
	if (cursor < buffer->current)
		goto _nextMultiPartLine; // find next line in buffer
	else
		goto _startMultiPart; // buffer exhausted, read more data

_foundMultiPartFileContent:

	cursor = delim + 2;
	if (cursor < buffer->current) { // there is some data left in the buffer, most likely the beginning of the request body.
		buffer->current -= cursor;
		memmove(buffer->mem, buffer->mem + cursor, buffer->current); // return the overspill via the memory pool.
		#if DEBUG & 64
		Log(socket, "parseMultiPart: file content returned, amount=%d", buffer->current);
		#endif
	} else { // no data left in the buffer
		memPoolReset(buffer); // initialize the buffer
		#if DEBUG & 64
		Log(socket, "parseMultiPart: spot landing (no file content).");
		#endif
	}

	return 0; // success if and only if there is a non-empty header line
}

#endif*/

ssize_t sendMemPool(const int socket, const MemPool *mp) {
	return sendBuffer(socket, mp->mem, mp->current);
}

ssize_t sendBuffer(const int socket, const char *buf, const ssize_t count) {
	ssize_t totalSent = 0, sent;

	while (totalSent < count) {
		#if DEBUG & 2
		Log(socket, "sendBuffer: loop iteration.");
		#endif
		sent = send(socket, buf + totalSent, count - totalSent, MSG_NOSIGNAL);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer: send strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer: send error. sent=%d, errno=%d", sent, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return sent; // propagate error
		}
		totalSent += sent;
		#if DEBUG & 2
		Log(socket, "sendBuffer: inside loop. sent=%d", sent);
		#endif
	}

	#if DEBUG & 2
	Log(socket, "sendBuffer: return OK. totalSent=%d", totalSent);
	#endif
	return totalSent;
}

#if USE_SENDFILE == 1

ssize_t sendFile(const int socket, const int fd, const ssize_t count) {
	ssize_t totalSent = 0, sent;

	while (totalSent < count) {
		#if DEBUG & 2
		Log(socket, "sendFile: loop iteration.");
		#endif

		sent = sendfile(socket, fd, NULL, count - totalSent);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "sendFile: pipe strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0 ) {
			#if DEBUG & 2
			Log(socket, "sendFile: pipe error. sent=%d, errno=%d", sent, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return sent; // propagate error
		}
		#if DEBUG & 2
		Log(socket, "sendFile: inside loop. sent=%d", sent);
		#endif
		totalSent += sent;
	}

	#if DEBUG & 2
	Log(socket, "sendFile: return OK. totalSent=%d", totalSent);
	#endif
	return totalSent;
}

#else

ssize_t sendFile(const int socket, const int fd, const ssize_t count) {
	char buf[16384];
	ssize_t received, sent;
	ssize_t totalReceived = 0, totalSent = 0;

	#if DEBUG & 2
	Log(socket, "pipeToSocket: entering.");
	#endif
	while (totalReceived < count) {
		int toBeRead = count - totalReceived;
		received = read(fd, buf, toBeRead >= sizeof(buf) ? sizeof(buf) : toBeRead);
		if (received == 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: side exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
			#endif
			return totalSent;
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: read error. received=%d, errno=%d", received, errno);
			#endif
			return received; // propagate error
		}
		sent = sendBuffer(socket, buf, received);
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: send error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		totalSent += sent;
		totalReceived += received;
	}
	#if DEBUG & 2
	Log(socket, "pipeToSocket: main exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
	#endif
	return totalSent;
}

#endif

#ifdef PUT_PATH

ssize_t pipeToFile(const int socket, const int fd, const ssize_t count) {
	char buf[16384];
	ssize_t received, sent;
	ssize_t totalReceived = 0, totalSent = 0;

	#if DEBUG & 2
	Log(socket, "pipeToFile:s entering.");
	#endif
	while (totalReceived < count) {
		int toBeRead = count - totalReceived;
		received = recv(socket, buf, toBeRead >= sizeof(buf) ? sizeof(buf) : toBeRead, 0);
		if (received == 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: side exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
			#endif
			return totalSent;
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: recv error. received=%d, errno=%d", received, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return received; // propagate error
		}
		sent = write(fd, buf, received);
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: write error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		totalSent += sent;
		totalReceived += received;
	}
	#if DEBUG & 2
			Log(socket, "pipeToFile: main exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
	#endif
	return totalSent;
}

#endif
