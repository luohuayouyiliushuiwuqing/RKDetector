#ifndef _RKDETECTOR_RK_IMAGE_UTILS_H_
#define _RKDETECTOR_RK_IMAGE_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "rk_common.h"

    /**
 * @brief LetterBox
 * 
 */
    typedef struct
    {
        int   x_pad;
        int   y_pad;
        float scale;
    } letterbox_t;

    /**
 * @brief Convert image for resize and pixel format change
 * 
 * @param src_image [in] Source Image
 * @param dst_image [out] Target Image
 * @param src_box [in] Crop rectangle on source image
 * @param dst_box [in] Crop rectangle on target image
 * @param color [in] Pading color if dst_box can not fill target image
 * @return int 
 */
    int      convert_image(image_buffer_t* src_image,
                           image_buffer_t* dst_image,
                           image_rect_t*   src_box,
                           image_rect_t*   dst_box,
                           char            color);

    /**
 * @brief Convert image with letterbox
 * 
 * @param src_image [in] Source Image
 * @param dst_image [out] Target Image
 * @param letterbox [out] Letterbox
 * @param color [in] Fill color on target image
 * @return int 
 */
    int      convert_image_with_letterbox(image_buffer_t* src_image,
                                          image_buffer_t* dst_image,
                                          letterbox_t*    letterbox,
                                          char            color);

    /**
* @brief Get the image size
*
* @param image [in] Image
* @return int image size
*/
    int      get_image_size(image_buffer_t* image);

    uint32_t cvtColor(void*          src,
                      image_format_t src_format,
                      uint64_t       src_width,
                      uint64_t       src_height,
                      void*          dst,
                      image_format_t dst_format,
                      uint64_t       dst_width,
                      uint64_t       dst_height);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _RKDETECTOR_RK_IMAGE_UTILS_H_