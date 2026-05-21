cmake_minimum_required(VERSION 3.10)

project(RKDetector)

add_subdirectory(3rdparty 3rdparty.out)
add_subdirectory(utils utils.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(SRCS
        src/main.cpp
        src/NPUScheduler.cpp
        src/RKDetector.cpp
        src/LabelTools.cpp
        src/NPULoadMonitor.cpp
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
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

add_executable(rknn_demo ${SRCS})

target_link_libraries(rknn_demo
        ${UTILS_LIBS}
)

install(TARGETS rknn_demo DESTINATION .)
install(FILES model/drone.png DESTINATION model)
install(FILES model/drone.txt DESTINATION model)
file(GLOB RKNN_FILES "model/*.rknn")
install(FILES ${RKNN_FILES} DESTINATION model)
