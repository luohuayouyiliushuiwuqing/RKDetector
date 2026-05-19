cmake_minimum_required(VERSION 3.10)

project(rknn_yolov8_demo)

if (ENABLE_ASAN)
    message(STATUS "BUILD WITH ADDRESS SANITIZER")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif ()


add_subdirectory(../3rdparty 3rdparty.out)
add_subdirectory(../utils utils.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Common sources shared between v8 and 26
set(COMMON_SRCS
    main.cpp
    inference.cpp
    postprocess_common.cpp
)

# yolov8 target
add_executable(rknn_yolov8_demo
    ${COMMON_SRCS}
    postprocess_v8.cpp
)

target_link_libraries(rknn_yolov8_demo
    imageutils
    fileutils
    yoloutils
    ${LIBRKNNRT}
    ${LIBOPENCV}
    dl
    Threads::Threads
)

target_include_directories(rknn_yolov8_demo PRIVATE
    ${LIBRKNNRT_INCLUDES}
    ${LIBOPENCV_INCLUDES}
)

# yolo26 target
add_executable(rknn_yolo26_demo
    ${COMMON_SRCS}
    postprocess_26.cpp
)

target_link_libraries(rknn_yolo26_demo
    imageutils
    fileutils
    yoloutils
    ${LIBRKNNRT}
    ${LIBOPENCV}
    dl
    Threads::Threads
)

target_include_directories(rknn_yolo26_demo PRIVATE
    ${LIBRKNNRT_INCLUDES}
    ${LIBOPENCV_INCLUDES}
)

install(TARGETS rknn_yolov8_demo rknn_yolo26_demo DESTINATION .)
install(FILES model/drone.png DESTINATION model)
install(FILES model/drone.txt DESTINATION model)
file(GLOB RKNN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/model/*.rknn")
install(FILES ${RKNN_FILES} DESTINATION model)
