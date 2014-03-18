/**
 * Copyright 2013 Yahoo! Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may
 * obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License. See accompanying LICENSE file.
 */

/**
* @file   vbitmap.h
* @addtogroup Vbitmap
* @brief  Vbitmap abstract type for images
*/

#ifndef _YMAGINE_VBITMAP_H
#define _YMAGINE_VBITMAP_H 1

#include "ymagine/ymagine.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Vbitmap Vbitmap
 *
 * This module provides API for creating and edditing Vbitmaps
 * which is an abstract type for an image into Ymagine
 *
 * @{
 */

#define VBITMAP_NONE      0
#define VBITMAP_ANDROID   1
#define VBITMAP_MEMORY    2
#define VBITMAP_STATIC    3
#define VBITMAP_GLTEXTURE 4

#define VBITMAP_COLOR_RGBA       0
#define VBITMAP_COLOR_RGB        1
#define VBITMAP_COLOR_GRAYSCALE  2

YOSAL_OBJECT_EXPORT(Vbitmap)
typedef struct VbitmapXmpStruct VbitmapXmp;

/* Vbitmap constructors */

/**
 * @brief Constructs a Vbitmap with no backing store for pixel buffer
 * @ingroup Vbitmap
 *
 * @return pointer to a Vbitmap
 */
Vbitmap*
VbitmapInitNone();

/**
 * @brief Constructs a Vbitmap for android
 * @ingroup Vbitmap
 *
 * @param jenv This is the argument for the java virtual machine
 * @param jbitmap This is the java bitmap
 * @return pointer to a Vbitmap
 */
Vbitmap*
VbitmapInitAndroid(JNIEnv *jenv, jobject jbitmap);

/**
 * @brief Constructs a Vbitmap whose buffer's memory is controlled by the library
 * @ingroup Vbitmap
 *
 * On creation, the Vbitmap as a null size. Caller should callVbitmapResize() to set the size
 * of the Vbitmap, and have it dynamically allocate the matching pixel buffer.
 *
 * When the user is done with the Vbitmap, must call VbitmapRelease() to release the Vbitmap.
 *
 * @param colormode This is one of the colormodes that are prefixed by VBITMAP_COLOR
 * @return pointer to a Vbitmap
 */
Vbitmap*
VbitmapInitMemory(int colormode);

/**
 * @brief Constructs a Vbitmap from a static memory buffer own by the caller
 * @ingroup Vbitmap
 *
 * When the user is done with the Vbitmap, must call VbitmapRelease() to release the Vbitmap.
 *
 * @param colormode
 * @param width
 * @param height
 * @param pitch
 * @param pixels
 * @return pointer to a Vbitmap
 */
Vbitmap*
VbitmapInitStatic(int colormode, int width, int height, int pitch,
		  unsigned char *pixels);

/**
 * @brief Retain the Vbitmap
 * @ingroup Vbitmap
 *
 * @param vbitmap that will be retained.
 * @return reference to the retained vbitmap (ptr)
 */
Vbitmap*
VbitmapRetain(Vbitmap *vbitmap);

/* Destructor */

/**
 * @brief If reference counter for Vbitmap is less or equal to 1,
 *        release the Vbitmap object, otherwise decrement the vbitmap
 *        reference count.
 * @ingroup Vbitmap
 *
 * @param vbitmap whose reference count will be decreased
 * @return If succesful YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapRelease(Vbitmap *vbitmap);

/* Methods */

/**
 * @brief Resizes the Vbitmap
 * @ingroup Vbitmap
 *
 * After creating the bitmap with VbitmapInitMemory(), users must resize the bitmap
 * in order to create space for the buffer. This function allocates memory for the
 * buffer and releasing the memory is done through VbitmapRelease().
 *
 * @param vbitmap that will be resized
 * @param width the horizontal pixel length
 * @param height the vertical pixel length
 * @return If succesfully locks the Vbitmap YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapResize(Vbitmap *vbitmap, int width, int height);

/**
 * @brief
 * @ingroup Vbitmap
 *
 * Given a vbitmap, this method returns a Java bitmap object.
 * @param vbitmap Bitmap that will be converted into Java bitmap
 * @return jobject that is created by the vbitmap
 */
jobject
VbitmapGetAndroid(Vbitmap *vbitmap);

/**
 * @brief Locks the Vbitmap
 * @ingroup Vbitmap
 *
 * Users must lock the bitmap before accessing the buffer and
 * must unlock when they are done with it.
 *
 * @param vbitmap that will be locked
 * @return If succesfully locks the Vbitmap YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapLock(Vbitmap *vbitmap);

/**
 * @brief Unlocks the Vbitmap
 * @ingroup Vbitmap
 *
 * Users must lock the bitmap before accessing the buffer and
 * must unlock when they are done with it.
 *
 * @param vbitmap will be unlocked
 * @return If succesfully unlocks the Vbitmap YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapUnlock(Vbitmap *vbitmap);

/**
 * @brief Writes on the buffer
 * @ingroup Vbitmap
 *
 * @param vbitmap that will be written on
 * @param buffer that contains the new content
 * @param npixels length of the buffer to be written
 * @return If succesfully writes on buffer YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapWrite(Vbitmap *vbitmap, void *buffer, int npixels);

/**
 * @brief Returns the colormode of the vbitmap
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return Colormode
 */
int
VbitmapColormode(Vbitmap *vbitmap);


/**
 * @brief Returns the bytes per pixel for the buffer
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return bytes per pixel
 */
int
VbitmapBpp(Vbitmap *vbitmap);

/**
 * @brief Returns the width of the image
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return width of the image
 */
int
VbitmapWidth(Vbitmap *vbitmap);

/**
 * @brief Returns the height of the image
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return height of the image
 */
int
VbitmapHeight(Vbitmap *vbitmap);

/**
 * @brief Returns the pitch of the image
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return pitch of the image
 */
int
VbitmapPitch(Vbitmap *vbitmap);

/**
 * @brief Returns the buffer that contains the data for the image
 * @ingroup Vbitmap
 *
 * Before accessing the buffer, vbitmap must first be locked with VbitmapLock().
 * Afterwards, vbitmap must be unlocked with VbitmapUnlock().
 *
 * @param vbitmap
 * @return buffer
 */
unsigned char*
VbitmapBuffer(Vbitmap *vbitmap);

/**
 * @brief Get type of a Vbitmap
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return Type of vbitmap
 */
int
VbitmapType(Vbitmap *vbitmap);

/**
 * @brief Set the rectangular sub-region of a Vbitmap which should be
 * considered for future operation on this bitmap
 * @ingroup Vbitmap
 *
 * When the caller wants to apply a function over only a certain portion
 * of the image, VbitmapRegionSelect() method enables should be called.
 *
 * @param vbitmap that active region will be applied on
 * @param xmin top left X coordinate
 * @param ymin top left Y coordinate
 * @param width of the active region
 * @param height of the active region
 * @return If successful YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapRegionSelect(Vbitmap *vbitmap, int xmin, int ymin, int width, int height);

/**
 * @brief Deselects the region and making the active region the whole image
 * @ingroup Vbitmap
 *
 * @param vbitmap
 * @return If successful YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VbitmapRegionReset(Vbitmap *vbitmap);

/**
 * @brief Returns the width of the active region of the image
 * @ingroup Vbitmap
 *
 * If there isn't any active region, it returns the width of the original image
 *
 * @param vbitmap
 * @return width of the active region of the image
 */
int
VbitmapRegionWidth(Vbitmap *vbitmap);

/**
 * @brief Returns the height of the active region of the image
 * @ingroup Vbitmap
 *
 * If there isn't any active region, it returns the height of the original image
 *
 * @param vbitmap
 * @return height of the active region of the image
 */
int
VbitmapRegionHeight(Vbitmap *vbitmap);

/**
 * @brief Returns the buffer that contains the data for active region of the image
 * @ingroup Vbitmap
 *
 * Before accessing the buffer, vbitmap must first be locked with VbitmapLock().
 * Afterwards, vbitmap must be unlocked with VbitmapUnlock().
 *
 * If there isn't any active region, it returns the buffer of the original image.
 * It returns NULL, if the region is empty and/or doesn't intersect with the Vbitmap full buffer.
 *
 * @param vbitmap
 * @return pointer to top left pixel of the region, or null
 */
unsigned char*
VbitmapRegionBuffer(Vbitmap *vbitmap);

/**
 * @brief Populate a memory backed RGB(A) vbitmap from an NV21 buffer. Optionally downscale (for free)
 * by a factor of 2 during the conversion process.
 *
 * @param vbitmap to populate
 * @param nv21buffer pointer to NV21 pixel data
 * @param width of NV21 source image
 * @param height of NV21 source image
 * @param downscale YMAGINE_SCALE_HALF_AVERAGE or YMAGINE_SCALE_HALF_QUICK or YMAGINE_SCALE_NONE
 *
 * @return YMAGINE_OK on success
 */
int
VbitmapWriteNV21Buffer(Vbitmap *vbitmap, const unsigned char *nv21buffer, int width, int height, int downscale);

/**
 * @brief Set XMP meta-data for this bitmap
 *
 * @param vbitmap to attach meta-data to
 * @param xmp pointer to XMP private data
 *
 * @return reference to current VbitmapXmp record for this bitmap
 */
VbitmapXmp*
VbitmapSetXMP(Vbitmap *vbitmap, VbitmapXmp *xmp);

/**
 * @brief Get XMP panoramic meta-data for this bitmap
 *
 * @param vbitmap to set meta-data about
 *
 * @return reference to current VbitmapXmp record for this bitmap
 */
VbitmapXmp*
VbitmapGetXMP(Vbitmap *vbitmap);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_VBITMAP_H */
