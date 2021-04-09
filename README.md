# `libav-examples`
A collection of FFmpeg libav examples.

## Usage
#### Requires
* gcc
* cmake
* ffmpeg
* ffmpeg-dev

#### cmake
```
cmake .
./bin/mp4info input.mp4
```

#### gcc
```
gcc -L/opt/ffmpeg/lib -I/opt/ffmpeg/include/ src/mp4info.c -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil -o mp4info

./bin/mp4info input.mp4
```

## Docker
```
docker-compose run libav-examples
cmake .
./bin/mp4info input.mp4
```

## Related
* https://www.ffmpeg.org/doxygen/4.1/examples.html
* https://github.com/leandromoreira/ffmpeg-libav-tutorial