FROM alpine:3.13
LABEL MAINTAINER Alfred Gutierrez <alf.g.jr@gmail.com>

RUN apk add --update \
  build-base \
  cmake \
  pkgconfig \
  ffmpeg \
  ffmpeg-dev \
  ca-certificates

ADD . /src/libav

WORKDIR /src/libav
