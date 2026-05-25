add_definitions(-DLIBRGA_IM2D_HANDLE)

add_library(imageutils STATIC
        rk_image_utils.cpp
)

target_include_directories(imageutils PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBRGA_INCLUDES}
)

target_link_libraries(imageutils
        ${LIBRGA}
)