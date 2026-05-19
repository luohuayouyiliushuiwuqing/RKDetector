cmake_minimum_required(VERSION 3.15)

project(rknn_model_zoo_utils)

add_library(fileutils STATIC
    file_utils.c
)
target_include_directories(fileutils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(DISABLE_RGA)
    add_definitions(-DDISABLE_RGA)
endif ()

add_definitions(-DLIBRGA_IM2D_HANDLE)

add_library(imageutils STATIC
    image_utils.c
)

target_include_directories(imageutils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STB_INCLUDES}
    ${LIBRGA_INCLUDES}
)

target_link_libraries(imageutils
    ${LIBRGA}
)

if (DISABLE_LIBJPEG)
    add_definitions(-DDISABLE_LIBJPEG)
else()
    target_link_libraries(imageutils
        ${LIBJPEG}
    )
    target_include_directories(imageutils PUBLIC
        ${LIBJPEG_INCLUDES}
    )
endif()


add_library(yoloutils STATIC
        yolo.cpp
)
target_include_directories(yoloutils PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBRGA_INCLUDES}
        ${LIBRKNNRT_INCLUDES}
)
