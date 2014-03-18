/**
* @file   seam.h
* @addtogroup Seam
* @brief  Seam carving
*/
#ifndef _YMAGINE_SEAM_H
#define _YMAGINE_SEAM_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Seam Seam
 *
 * This module provides API for seam carving
 *
 * @{
 */

typedef struct VbitmapSeamMapStruct VbitmapSeamMap;

VbitmapSeamMap*
VbitmapSeamMap_create(int width, int height);

int
VbitmapSeamMap_release(VbitmapSeamMap *seammap);

VbitmapSeamMap*
Vbitmap_seamPrepare(Vbitmap *vbitmap);

/**
 * @brief Resize using seam carving
 * @ingroup Seam
 *
 *
 * Resize image using seam carving to change aspect ratio
 *
 * @param vbitmap Input image to resize
 * @param outbitmap Output image with the resized bitmap
 * @return YMAGINE_OK if seam carving is succesfull, else YMAGINE_ERROR
 */
int
Vbitmap_seamCarve(Vbitmap *vbitmap, VbitmapSeamMap *seamMap, Vbitmap *outbitmap);

int
Vbitmap_seamRender(Vbitmap *vbitmap, VbitmapSeamMap *seamMap, int toremove);

int
Vbitmap_seamDump(VbitmapSeamMap *seamMap, Ychannel *channel);

int
Vbitmap_sobel(Vbitmap *outbitmap, Vbitmap *input);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_SEAM_H */
