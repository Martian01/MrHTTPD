#
# Build stage
#
FROM alpine:3.11 AS build
RUN apk add --update --no-cache bash make gcc musl-dev 

COPY * /tmp/build/
COPY html/* /tmp/build/html/
RUN cd /tmp/build && mv mrhttpd-docker.conf mrhttpd.conf && make install


#
# Package stage
#
FROM alpine:3.11
COPY --from=build /usr/local/bin/mrhttpd /usr/local/bin/
COPY --from=build /opt/mrhttpd /opt/mrhttpd/
ENTRYPOINT ["/usr/local/bin/mrhttpd"]
