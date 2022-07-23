cmake_minimum_required(VERSION 3.10)
project(app C)

set(CMAKE_C_STANDARD 99)

add_executable(mp4info src/mp4info.c)
add_executable(mp4info_simple src/mp4info_simple.c)
add_executable(extract_images src/extract_images.c)
add_executable(extract_rgb src/extract_rgb.c)
add_executable(filter_example src/filter_to_ppm.c)
add_executable(remux src/remux.c)
add_executable(transcode src/transcode.c)

target_link_libraries(mp4info avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(mp4info_simple avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(extract_images avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(extract_rgb avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(filter_example avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(remux avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(transcode avcodec avformat avfilter avdevice swresample swscale avutil)

