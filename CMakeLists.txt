cmake_minimum_required(VERSION 3.10)
project(app C)

set(CMAKE_C_STANDARD 99)

add_executable(mp4info src/mp4info.c)
add_executable(extract_images src/extract_images.c)
add_executable(extract_rgb src/extract_rgb.c)
add_executable(filter_example src/filter_to_ppm.c)

target_link_libraries(mp4info avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(extract_images avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(extract_rgb avcodec avformat avfilter avdevice swresample swscale avutil)
target_link_libraries(filter_example avcodec avformat avfilter avdevice swresample swscale avutil)
