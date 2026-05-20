cmake_minimum_required(VERSION 3.10)

project(rknn_demo)

add_subdirectory(3rdparty 3rdparty.out)
add_subdirectory(utils utils.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Common sources shared between v8 and 26
set(COMMON_SRCS
        main.cpp
        inference.cpp
        postprocess_common.cpp
)

set(UTILS_LIBS
        imageutils
        fileutils
        ${LIBRKNNRT}
        ${LIBOPENCV}
        dl
        Threads::Threads
)

include_directories(
        ${LIBRKNNRT_INCLUDES}
        ${LIBOPENCV_INCLUDES}
)


# yolov8 target
add_executable(rknn_yolov8_demo
        ${COMMON_SRCS}
        postprocess_v8.cpp
)

target_link_libraries(rknn_yolov8_demo
        ${UTILS_LIBS}
)

# yolo26 target
add_executable(rknn_yolo26_demo
        ${COMMON_SRCS}
        postprocess_26.cpp
)

target_link_libraries(rknn_yolo26_demo
        ${UTILS_LIBS}
)


install(TARGETS rknn_yolov8_demo rknn_yolo26_demo DESTINATION .)
install(FILES model/drone.png DESTINATION model)
install(FILES model/drone.txt DESTINATION model)
file(GLOB RKNN_FILES "model/*.rknn")
install(FILES ${RKNN_FILES} DESTINATION model)
