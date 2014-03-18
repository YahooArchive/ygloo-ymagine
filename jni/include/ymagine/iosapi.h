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

#ifndef _YMAGINE_IOSAPI_H
#define _YMAGINE_IOSAPI_H 1

#ifdef __APPLE__
#  if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
#    ifndef HAVE_IOS
#      define HAVE_IOS 1
#    endif /* !HAVE_IOS */
#  endif
#endif /* __APPLE__ */

#ifdef HAVE_IOS
#import <Foundation/Foundation.h>


/*!
 * @class Ymagine
 * @abstract This class provides several static functions to interface with
 * the Ymagine image manipulation library. The underlying C API can be used
 * in paralell.
 */
@interface Ymagine : NSObject

/*!:
 * @method createVbitmapWithUIImage:
 * @abstract This method creates an in-memory Vbitamp from an existing UIImage.
 * This will trigger decoding of the JPEG/PNG data of the given UIImage if necessary.
 * @param image to be converted
 * @result Returns the newly created Vbitmap
 */
+(Vbitmap *) createVbitmapWithUIImage : (UIImage *) image;

/*!
 * @method createVbitmapWithUIImage:withResizeValue:grayScale
 * @abstract This method, like the previous one, creates an in-memory Vbitamp from an existing UIImage.
 * This will trigger decoding of the JPEG/PNG data of the given UIImage if necessary. At the same time
 * the Vbitmap is resized, if necessary, to have a maximum width/height of maxSize. The aspect
 * ratio of the image is preserved.
 * @param image to be converted
 * @param maxSize maximum width/height
 * @param grayScale YES if the Vbitmap should be grayscale only
 * @result Returns the newly created Vbitmap
 */
+(Vbitmap *) createVbitmapWithUIImage : (UIImage *) image withResizeValue: (NSInteger) maxSize
                             grayScale: (BOOL) grayScale;

/*!
 * @method releaseVbitmap
 * @abstract Releases the given Vbitmap and frees all related memory
 * @discussion This is necessary, since Vbitmaps are C structures and thus not
 * reference counted by ARC. If a method of this class returns a Vbitmap, it has
 * to be released using this method.
 * @param vbitmap to be released
 */
+(void) releaseVbitmap: (Vbitmap *) vbitmap;

/*!
 * @method createUIImageWithVbitmap
 * @abstract Create a UIImage from the given Vbitmap
 * @param vbitmap to be converted
 * @result Returns the newly created UIImage
 */
+(UIImage *) createUIImageWithVbitmap: (Vbitmap *) vbitmap;

/*!
 * @method createUIImageWithNSData:resizeValue:
 * @abstract shorthand for createUIImageWithNSData:maxWidth:maxHeight: where
 * maxWidth == maxHeight
 */
+(UIImage *) createUIImageWithNSData : (NSData *) data resizeValue: (NSInteger) maxSize;

/*!
 * @method createUIImageWithNSData:maxWidth:maxHeight:
 * @abstract Decodes the given JPEG data and returns it as a UIImage
 * @discussion This method does not use the JPEG decoder provided by UIKit, it's
 * a drop-in replacement for UIImage imageWithData. This method offers significantly
 * better performance than UIImage imageWithData when decoding high resoltuion JPEGs
 * into low resolution UIImages. Use this when you are forced to display a high resolution
 * JPEG in a low resolution.
 * @param data the raw JPEG data
 * @param maxWidth maximum width of the decoded image
 * @param maxHeight maximum height of the decoded image
 * @result Returns the newly created UIImage
 */
+(UIImage *) createUIImageWithNSData : (NSData *) data maxWidth: (NSInteger) maxWidth maxHeight: (NSInteger) maxHeight;

/*!
 * @method getThemeColor:
 * @abstract Determines the most dominant color in the given image
 * @discussion Even if no maximum size is passed, an implicit resize might be
 * happening to ensure efficiency.
 * @param image to be analyzed
 * @result Returns a UIColor representing the most dominant color
 */
+(UIColor *) getThemeColor: (UIImage *) image;

/*!
 * @method getThemeColor:withResizeValue:
 * @abstract Determines the most dominant color in the given image after resizing to
 * a given max width and height.
 * @param image to be analyzed
 * @param maxSize maximum width/height
 * @result Returns a UIColor representing the most dominant color
 */
+(UIColor *) getThemeColor: (UIImage *) image withResizeValue: (NSInteger) maxSize;

/*!
 * @method getThemeColor:maxColors:
 * @abstract Determines the N most dominant colors in the given image.
 * @param image to be analyzed
 * @param maxColors maximum number of colors to be returned
 * @result Returns an NSArray of UIColor objects representing the most dominant colors
 */
+(NSArray *) getThemeColors: (UIImage *) image maxColors: (NSInteger) maxColors;

/*!
 * @method getThemeColor:maxColors:withResizeValue:
 * @abstract Determines the N most dominant colors in the given image after resizing to
 * a given max width and height.
 * @discussion Even if no maximum size is passed, an implicit resize might be
 * happening to ensure efficiency.
 * @param image to be analyzed
 * @param maxColors maximum number of colors to be returned
 * @param maxSize maximum width/height
 * @result Returns a UIColor representing the most dominant color
 */
+(NSArray *) getThemeColors: (UIImage *) image maxColors: (NSInteger) maxColors withResizeValue: (NSInteger) maxSize;

/*!
 * @method blurImage:withResizeValue:andGrayScale:andRadius:
 * @abstract Blur the given image using the given radius after resizing it to
 * the given maximum width and height and optionally converting it to greyscale.
 * @param image to be blurred
 * @param resizeValue maximum width/height
 * @param grayScale YES if a grey scale image should be returned
 * @param radius to be used in the (approximate) gaussian blur
 * @result Returns a blurred UIImage
 */
+(UIImage *) blurImage: (UIImage *) image withResizeValue: (NSInteger) resizeValue andGrayScale:(BOOL)grayScale andRadius: (NSInteger) radius;

/*!
 * @method blurImage:withResizeValue:andRadius:
 * @abstract Shorthand for blurImage:withResizeValue:andGrayScale:andRadius: assuming
 * no grayscale.
 * @param image to be blurred
 * @param resizeValue maximum width/height
 * @param radius to be used in the (approximate) gaussian blur
 * @result Returns a blurred UIImage
 */
+(UIImage *) blurImage: (UIImage *) image withResizeValue: (NSInteger) resizeValue andRadius: (NSInteger) radius;

/*!
 * @method blurImage:andRadius:
 * @abstract Shorthand for blurImage:withResizeValue:andGrayScale:andRadius: assuming
 * no grayscale and a default resize value.
 * @param image to be blurred
 * @param radius to be used in the (approximate) gaussian blur
 * @result Returns a blurred UIImage
 */
+(UIImage *) blurImage: (UIImage *) image withRadius: (NSInteger) radius;

+(BOOL) detectLoadModel: (NSString *) name;
+(NSArray *) detect: (UIImage *) image count: (NSInteger) count;

@end
#endif /* HAVE_IOS */

#endif /* _YMAGINE_IOSAPI_H */
