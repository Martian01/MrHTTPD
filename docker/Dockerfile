#
# Build stage
#
FROM alpine:3 AS build
RUN apk add --update --no-cache bash make gcc musl-dev 

COPY * /tmp/build/
COPY html/* /tmp/build/html/
COPY docker/mrhttpd-docker.conf /tmp/build/mrhttpd.conf
RUN cd /tmp/build && make install


#
# Package stage
#
FROM alpine:3
COPY --from=build /usr/local/bin/mrhttpd /usr/local/bin/
COPY --from=build /opt/mrhttpd /opt/mrhttpd/
ENTRYPOINT ["/usr/local/bin/mrhttpd"]
