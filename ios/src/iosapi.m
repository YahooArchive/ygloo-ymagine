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

#include "ymagine/ymagine.h"
#include "ymagine/plugins/vision.h"

#define GETTHEME_DEFAULT_RESIZE_VAL 64

@implementation Ymagine

+(Vbitmap *) createVbitmapWithUIImage : (UIImage *) image
{
    return [Ymagine createVbitmapWithUIImage:image withResizeValue:-1 grayScale:NO];
}

+(Vbitmap *) createVbitmapWithUIImage : (UIImage *) image withResizeValue: (NSInteger) maxSize grayScale: (BOOL) grayScale
{
    CGImageRef imageRef = [image CGImage];
    int width = (int) CGImageGetWidth(imageRef);
    int height = (int) CGImageGetHeight(imageRef);
    int maxSizeInt = (int) maxSize;

    if (maxSize >= 0)
    {
        if (maxSize == 0) return NULL;

        //@To-Do : should import the logic from Ymagine to take maxWidth &&
        //          maxHeight and define outputWidth && outputHeight to preserve ratio
        if (width > maxSize || height > maxSize) {
            if (width > height) {
                height = (height * maxSizeInt) / width;
                width = maxSizeInt;
            } else {
                width = (width * maxSizeInt) / height;
                height = maxSizeInt;
            }
        }
    }

    CGColorSpaceRef colorSpace = grayScale ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();

    /* Create Vbitmap for this pixel buffer */
    int colorMode = grayScale ? VBITMAP_COLOR_GRAYSCALE : VBITMAP_COLOR_RGBA;
    Vbitmap *vbitmap = VbitmapInitMemory(colorMode);

    if ((VbitmapResize(vbitmap, width, height) != YMAGINE_OK) || (VbitmapLock(vbitmap) != YMAGINE_OK))
    {
        CGColorSpaceRelease(colorSpace);
        [Ymagine releaseVbitmap:vbitmap];
        return NULL;
    }
    unsigned char *buffer = VbitmapBuffer(vbitmap);

    if (buffer == NULL)
    {
        CGColorSpaceRelease(colorSpace);
        [Ymagine releaseVbitmap:vbitmap];
        return NULL;
    }

    NSUInteger bytesPerRow = VbitmapPitch(vbitmap);
    NSUInteger bitsPerComponent = 8;

    /* bitmapInfo hard coded to kCGImageAlphaNoneSkipLast for RGBA */
    CGImageAlphaInfo alphaInfo = grayScale ? kCGImageAlphaNone : kCGImageAlphaNoneSkipLast;
    CGBitmapInfo bitmapInfo = kCGBitmapAlphaInfoMask & alphaInfo;

    CGContextRef context = CGBitmapContextCreate(buffer, width, height,
                                                 bitsPerComponent, bytesPerRow, colorSpace,
                                                 bitmapInfo);

    CGColorSpaceRelease(colorSpace);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);

    CGContextRelease(context);
    if (VbitmapUnlock(vbitmap) != YMAGINE_OK)
    {
        [Ymagine releaseVbitmap:vbitmap];
        return NULL;
    }
    return vbitmap;
}

static void freeProviderData(void *info, const void *data, size_t size) {
    free((unsigned char *)data);
}

+(UIImage *) createUIImageWithVbitmap: (Vbitmap *) vbitmap
{
    CGImageAlphaInfo alphaInfo;
    if (vbitmap == NULL) return nil;
    int width = VbitmapWidth(vbitmap);
    int height = VbitmapHeight(vbitmap);
    BOOL grayScale = NO;
    switch (VbitmapColormode(vbitmap)) {
        case VBITMAP_COLOR_RGB:
            grayScale = NO;
            alphaInfo = kCGImageAlphaNone;
            break;
        case VBITMAP_COLOR_RGBA:
            grayScale = NO;
            alphaInfo = kCGImageAlphaNoneSkipLast;
            break;
        case VBITMAP_COLOR_GRAYSCALE:
            grayScale = YES;
            alphaInfo = kCGImageAlphaNone;
            break;
        default:
            return nil;
    }

    NSUInteger bytesPerPixel = VbitmapBpp(vbitmap);
    NSUInteger bytesPerRow = VbitmapPitch(vbitmap);
    NSUInteger bitsPerComponent = 8;

    if (VbitmapLock(vbitmap) != YMAGINE_OK) return nil;
    unsigned char *buffer = VbitmapBuffer(vbitmap);
    unsigned char *bufferCopy = malloc(bytesPerRow * height);
    memcpy(bufferCopy, buffer, bytesPerRow * height);
    VbitmapUnlock(vbitmap);

    CGColorSpaceRef colorSpace = grayScale ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();

    CGDataProviderRef provider = CGDataProviderCreateWithData(vbitmap,
                                                              bufferCopy,
                                                              bytesPerRow * height,
                                                              (CGDataProviderReleaseDataCallback)&freeProviderData);

    /* bitmapInfo hard coded to kCGImageAlphaNoneSkipLast for RGBA */
    CGBitmapInfo bitmapInfo = kCGBitmapAlphaInfoMask & alphaInfo;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    CGImageRef imageRef = CGImageCreate(width,
                                        height,
                                        bitsPerComponent,
                                        bytesPerPixel * 8,
                                        bytesPerRow,
                                        colorSpace,
                                        bitmapInfo,
                                        provider,NULL,NO,renderingIntent);
    UIImage *newImage = [[UIImage alloc] initWithCGImage:imageRef];

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    CGImageRelease(imageRef);

    return newImage;
}

+(UIImage *) createUIImageWithNSData : (NSData *) data maxWidth: (NSInteger) maxWidth maxHeight: (NSInteger) maxHeight
{
  UIImage* img = nil;
  const char* buffer = [data bytes];
  Ychannel* channel = YchannelInitByteArray(buffer, [data length]);
  Vbitmap* bitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);

  int rc = YmagineDecodeResize(bitmap, channel, maxWidth, maxHeight, YMAGINE_SCALE_LETTERBOX);

  if (rc >= 0) {
    img = [self createUIImageWithVbitmap:bitmap];
  }

  VbitmapRelease(bitmap);
  YchannelResetBuffer(channel);
  YchannelRelease(channel);

  return img;
}

+(UIImage *) createUIImageWithNSData : (NSData *) data resizeValue: (NSInteger) maxSize
{
  return [self createUIImageWithNSData:data maxWidth:maxSize maxHeight:maxSize];
}

+(void) releaseVbitmap: (Vbitmap *) vbitmap
{
    if (vbitmap != NULL) {
        VbitmapRelease(vbitmap);
    }
    return;
}

+(UIImage *) blurImage: (UIImage *) image withResizeValue: (NSInteger) resizeValue andGrayScale:(BOOL)grayScale andRadius: (NSInteger) radius
{
    Vbitmap *vbitmap = [Ymagine createVbitmapWithUIImage:image withResizeValue:resizeValue grayScale:grayScale];
    if (vbitmap == NULL) {
        return nil;
    }
    if (Ymagine_blur(vbitmap, (int)radius) != YMAGINE_OK) {
        [Ymagine releaseVbitmap: vbitmap];
        return nil;
    }
    UIImage *img = [Ymagine createUIImageWithVbitmap:vbitmap];
    [Ymagine releaseVbitmap: vbitmap];
    return img;
}

+(UIImage *) blurImage: (UIImage *) image withResizeValue: (NSInteger) resizeValue andRadius: (NSInteger) radius
{
    return [Ymagine blurImage:image withResizeValue:resizeValue andGrayScale:NO andRadius:radius];
}

+(UIImage *) blurImage: (UIImage *) image withRadius: (NSInteger) radius
{
    return [Ymagine blurImage:image withResizeValue:-1 andRadius:radius];
}

+(UIColor *) getThemeColor: (UIImage *) image
{
    return [Ymagine getThemeColor: image withResizeValue:GETTHEME_DEFAULT_RESIZE_VAL];
}


+(UIColor *) getThemeColor: (UIImage *) image withResizeValue: (NSInteger) maxSize
{
    uint32_t color;
    Vbitmap *vbitmap = [Ymagine createVbitmapWithUIImage:image withResizeValue:maxSize grayScale:NO];
    if (vbitmap != NULL) {
        color = getThemeColor(vbitmap);
        VbitmapRelease(vbitmap);
    } else {
        color = 0xFF6103FF;
    }

    float alpha = ((float) ((color >> 24) & 0xFF)) / 255.0f;
    float red = ((float) ((color >> 16) & 0xFF)) / 255.0f;
    float green = ((float) ((color >> 8) & 0xFF)) / 255.0f;
    float blue = ((float) ((color >> 0) & 0xFF)) / 255.0f;

    return [UIColor colorWithRed:red green:green blue:blue alpha:alpha];
}

+(BOOL) detectLoadModel: (NSString *) name
{
    const char *cname = NULL;
    if (name != nil) {
        cname = [name UTF8String];
    }
    if (cname == NULL || cname[0] == '\0') {
        cname = "@face";
    }

    if (detect_load_model(cname) != YMAGINE_OK) {
        return NO;
    }

    return YES;
}

+(NSArray *) detect: (UIImage *) image count: (NSInteger) count
{
    if (count <= 0 || image == nil) return nil;

    /* Create Vbitmap for this pixel buffer */
    Vbitmap *vbitmap = [Ymagine createVbitmapWithUIImage:image withResizeValue:-1 grayScale:YES];
    if (vbitmap == NULL) return nil;

    int coords[count*4];
    int scores[count];
    int mindetect = 0;

    int nfound = detect_run(vbitmap, mindetect, (int)count, coords, scores);

    [Ymagine releaseVbitmap:vbitmap];

    if (nfound <= 0) return nil;
    if (nfound > count) return nil;

    NSMutableArray *coordsArr = [[NSMutableArray alloc] initWithCapacity:nfound*4];
    for (int i = 0; i < 4 * nfound; i++)
    {
        [coordsArr addObject:[NSNumber numberWithInt:coords[i]]];
    }

    return [NSArray arrayWithArray:coordsArr];
}

+(NSArray *) getThemeColors: (UIImage *) image maxColors: (NSInteger) maxColors
{
    return [Ymagine getThemeColors:image maxColors:maxColors withResizeValue:GETTHEME_DEFAULT_RESIZE_VAL];
}

+(NSArray *) getThemeColors: (UIImage *) image maxColors: (NSInteger) maxColors withResizeValue: (NSInteger) maxSize
{
    if (maxColors <= 0 || image == nil) return nil;

    /* Create Vbitmap for this pixel buffer */
    Vbitmap *vbitmap = [Ymagine createVbitmapWithUIImage:image withResizeValue:maxSize grayScale:NO];
    if (vbitmap == NULL) return nil;

    int colors[maxColors];
    int scores[maxColors];
    int ncolors = getThemeColors(vbitmap, (int)maxColors, colors, scores);

    NSMutableArray *colorArray = nil;

    if (ncolors > 0) {
        colorArray = [[NSMutableArray alloc] initWithCapacity:ncolors];
        float alpha, red, green, blue;
        int color;

        for (int i = 0; i < ncolors; i++) {
            color = colors[i];
            alpha = ((float) ((color >> 24) & 0xFF)) / 255.0f;
            red = ((float) ((color >> 16) & 0xFF)) / 255.0f;
            green = ((float) ((color >> 8) & 0xFF)) / 255.0f;
            blue = ((float) ((color >> 0) & 0xFF)) / 255.0f;

            [colorArray addObject:[UIColor colorWithRed:red green:green blue:blue alpha:alpha]];
        }
    }
    [Ymagine releaseVbitmap:vbitmap];
    return [NSArray arrayWithArray:colorArray];
}

@end
