cmake_minimum_required(VERSION 3.15)

project(rknn_model_zoo_3rdparty_libs)

set(TARGET_LIB_ARCH aarch64)

# rknn runtime
set(RKNN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/rknpu2)
set(LIBRKNNRT ${RKNN_PATH}/${CMAKE_SYSTEM_NAME}/${TARGET_LIB_ARCH}/librknnrt.so)
set(LIBRKNNRT_INCLUDES ${RKNN_PATH}/include PARENT_SCOPE)
#install(PROGRAMS ${LIBRKNNRT} DESTINATION lib)
set(LIBRKNNRT ${LIBRKNNRT} PARENT_SCOPE)

# rga
set(RGA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/librga)
set(LIBRGA ${RGA_PATH}/${CMAKE_SYSTEM_NAME}/${TARGET_LIB_ARCH}/librga.a PARENT_SCOPE)
set(LIBRGA_INCLUDES ${RGA_PATH}/include PARENT_SCOPE)
#install(PROGRAMS ${RGA_PATH}/${CMAKE_SYSTEM_NAME}/${TARGET_LIB_ARCH}/librga.so DESTINATION lib)

# opencv
set(OPENCV_PATH ${CMAKE_CURRENT_SOURCE_DIR}/opencv2)
set(OPENCV_LIB_DIR ${OPENCV_PATH}/libs/${TARGET_LIB_ARCH})
set(LIBOPENCV
    ${OPENCV_LIB_DIR}/libopencv_imgcodecs.so
    ${OPENCV_LIB_DIR}/libopencv_imgproc.so
    ${OPENCV_LIB_DIR}/libopencv_core.so
    PARENT_SCOPE
)
set(LIBOPENCV_INCLUDES ${OPENCV_PATH}/include PARENT_SCOPE)
# install all opencv shared libraries
file(GLOB OPENCV_SHARED_LIBS "${OPENCV_LIB_DIR}/*.so*")
#install(PROGRAMS ${OPENCV_SHARED_LIBS} DESTINATION lib)
