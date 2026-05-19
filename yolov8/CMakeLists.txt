cmake_minimum_required(VERSION 3.10)

project(rknn_yolov8_demo)

if (ENABLE_ASAN)
    message(STATUS "BUILD WITH ADDRESS SANITIZER")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif ()

set(rknpu_yolov8_file rknpu2/yolov8.cc)

add_subdirectory(../3rdparty 3rdparty.out)
add_subdirectory(../utils utils.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

file(GLOB SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_executable(${PROJECT_NAME}
        main.cc
        postprocess.cc
        ${rknpu_yolov8_file}
)

target_link_libraries(${PROJECT_NAME}
        imageutils
        fileutils
        ${LIBRKNNRT}
        ${LIBOPENCV}
        dl
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

target_include_directories(${PROJECT_NAME} PRIVATE
        cpp
        ${LIBRKNNRT_INCLUDES}
        ${LIBOPENCV_INCLUDES}
)

# zero-copy variant
add_executable(${PROJECT_NAME}_zero_copy
        main.cc
        postprocess.cc
        rknpu2/yolov8_zero_copy.cc
)

target_compile_definitions(${PROJECT_NAME}_zero_copy PRIVATE ZERO_COPY)

target_link_libraries(${PROJECT_NAME}_zero_copy
        imageutils
        fileutils
        ${LIBRKNNRT}
        ${LIBOPENCV}
        dl
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME}_zero_copy Threads::Threads)

target_include_directories(${PROJECT_NAME}_zero_copy PRIVATE
        cpp
        ${LIBRKNNRT_INCLUDES}
        ${LIBOPENCV_INCLUDES}
)
install(TARGETS ${PROJECT_NAME}_zero_copy DESTINATION cpp)

install(TARGETS ${PROJECT_NAME} DESTINATION cpp)
install(FILES model/drone.png DESTINATION model)
install(FILES model/drone.txt DESTINATION model)
file(GLOB RKNN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/model/*.rknn")
install(FILES ${RKNN_FILES} DESTINATION model)
