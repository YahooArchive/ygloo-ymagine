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

package com.yahoo.ymagine;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.lang.System;

public class Ymagine {
    private static final String[] DEFAULT_NATIVE_LIBRARIES = { "yahoo_ymagine" };
    private static String[] sNativeLibraries = DEFAULT_NATIVE_LIBRARIES;
    private enum NativeStatus {
        UNINITIALIZED,
        NEED_WORKAROUND,
        DISABLED,
        ENABLED
    }

    public enum ScaleType {
        /* ordinal must match the ones in ymagine.h */
        LETTERBOX,
        CROP,
        FIT
    }

    public enum AdjustMode {
        /* ordinal must match the ones in format.h */
        NONE,
        INNER,
        OUTER
    }

    public enum ImageFormat {
        /* ordinal must match the ones in format.h */
        UNKNOWN,
        JPEG,
        WEBP,
        PNG
    }

    public enum MetaMode {
        /* ordinal must match the ones in format.h */
        NONE,
        COMMENTS,
        ALL
    }

    /* Those definitions must match the ones in format.h */
    private static final int CROP_NONE = 0;
    private static final int CROP_ABSOLUTE = 1;
    private static final int CROP_RELATIVE = 2;

    @SuppressWarnings("unused")
    public static class Options {
        /**
         * Maximum width of resulting image, -1 default to width of input image
         */
        private int maxWidth;
        /**
         * Maximum height of resulting image, -1 default to height of input image
         */
        private int maxHeight;
        private Shader shader;
        private int scaleType;
        private int adjustMode;
        private int metaMode;
        private int outputFormat;
        private int quality;
        private float sharpen;
        private float rotate;
        private float blur;
        private int backgroundColor;
        private int offsetCropMode;
        private int sizeCropMode;
        private int cropAbsoluteX;
        private int cropAbsoluteY;
        private int cropAbsoluteWidth;
        private int cropAbsoluteHeight;
        /**
         * Relative x of crop region, range in [0, 1.0]
         */
        private float cropRelativeX;
        /**
         * Relative y of crop region, range in [0, 1.0]
         */
        private float cropRelativeY;
        /**
         * Relative width of crop region, range in [0, 1.0]
         */
        private float cropRelativeWidth;
        /**
         * Relative height of crop region, range in [0, 1.0]
         */
        private float cropRelativeHeight;

        public Options() {
            sharpen = 0.0f;
            rotate = 0.0f;
            blur = 0.0f;
            maxWidth = -1;
            maxHeight = -1;
            quality = -1;
            backgroundColor = Color.argb(0, 0, 0, 0);
            scaleType = ScaleType.LETTERBOX.ordinal();
            adjustMode = AdjustMode.NONE.ordinal();
            metaMode = MetaMode.ALL.ordinal();
            outputFormat = ImageFormat.JPEG.ordinal();
            shader = null;
            offsetCropMode = CROP_NONE;
            sizeCropMode = CROP_NONE;
        }

        public void setOutputFormat(ImageFormat format) {
            outputFormat = format.ordinal();
        }

        public void setSharpen(float sharpen) {
            this.sharpen = sharpen;
        }

        public void setRotate(float rotate) {
            this.rotate = rotate;
        }

        public void setBlur(float radius) {
            this.blur = radius;
        }

        /**
         * Set background color
         *
         * @param color ARGB color represented int form @see colorARGB
         */
        public void setBackgroundColor(int color) {
            this.backgroundColor = color;
        }

        public void setCropOffset(int x, int y) {
            offsetCropMode = CROP_ABSOLUTE;
            cropAbsoluteX = x;
            cropAbsoluteY = y;
        }

        public void setCropOffsetRelative(float relativeX, float relativeY) {
            offsetCropMode = CROP_RELATIVE;
            cropRelativeX = relativeX;
            cropRelativeY = relativeY;
        }

        public void setCropSize(int width, int height) {
            sizeCropMode = CROP_ABSOLUTE;
            cropAbsoluteWidth = width;
            cropAbsoluteHeight = height;
        }

        public void setCropSizeRelative(float relativeWidth, float relativeHeight) {
            sizeCropMode = CROP_RELATIVE;
            cropRelativeWidth = relativeWidth;
            cropRelativeHeight = relativeHeight;
        }

        public void setCrop(int x, int y, int width, int height) {
            setCropOffset(x, y);
            setCropSize(width, height);
        }

        public void setCropRelative(float x, float y, float width, float height) {
            setCropOffsetRelative(x, y);
            setCropSizeRelative(width, height);
        }

        public void setScaleType(ScaleType type) {
            scaleType = type.ordinal();
        }

        public void setAdjustMode(AdjustMode mode) {
            adjustMode = mode.ordinal();
        }

        public void setMetaMode(MetaMode mode) {
            metaMode = mode.ordinal();
        }

        public void setShader(Shader shader) {
            this.shader = shader;
        }

        public void setMaxSize(int maxWidth, int maxHeight) {
            this.maxWidth = maxWidth;
            this.maxHeight = maxHeight;
        }

        public void setQuality(int quality) {
            this.quality = quality;
        }
    }

    // Global setting for enabling or disabling JNI interface. Set
    // to NativeStatus.UNINITIALIZED for automatic initialization, or
    // NativeStatus.DISABLED for forcing off.
    private static NativeStatus sHasNative = NativeStatus.UNINITIALIZED;

    // Settable parameter to allow caller to turn native API off
    private static boolean sEnabled = true;

    // Directory for system libraries
    private static final String SYSTEM_LIB_DIR = "/system/vendor/lib";

    private static final int JPEG_DEFAULT_QUALITY = 80;

    private native static int native_version();
    private native static int[] native_quantize(String filename, int ncolors, int maxWidth, int maxHeight);

    private native static int native_RGBtoHSV(int huv);
    private native static int native_HSVtoRGB(int rgb);

    private native static int native_transcodeStream(InputStream is, OutputStream os, Options options);
    private native static int native_encodeStream(Vbitmap vbitmap, OutputStream os, Options options);

    /**
     * Attempt to load a native library
     *
     * @param libName to load
     */
    static public void loadLibrary(String libName) {
        try {
            java.lang.System.loadLibrary(libName);
            return;
        } catch (UnsatisfiedLinkError e) {
            if (SYSTEM_LIB_DIR != null) {
                String vm = System.getProperty("java.vm.vendor");
                if (vm != null && vm.equals("The Android Project")) {
                    // Failed to load, try to resolve from system
                    String libTail = java.lang.System.mapLibraryName(libName);
                    File libDir = new File(SYSTEM_LIB_DIR);
                    File libFile = new File(libDir, libTail);
                    if (libFile.isFile()) {
                        try {
                            java.lang.System.load(libFile.getAbsolutePath());
                            return;
                        } catch (UnsatisfiedLinkError e2) {
                        }
                    }
                }
            }

            // Re-throw initial UnsatisfiedLinkError
            // Log.e(LOG_TAG, "failed to load library " + libName);
            throw e;
        }
    }

    static public synchronized boolean init() {
        if (sHasNative == NativeStatus.UNINITIALIZED) {
            try {
                loadLibrary("yahoo_ymagine");
                sHasNative = NativeStatus.ENABLED;
            } catch (UnsatisfiedLinkError e) {
                System.out.println("Native code library failed to load" + e);
                sHasNative = NativeStatus.DISABLED;
            }
        }

        if (sHasNative != NativeStatus.ENABLED) {
            return false;
        }
        if (!sEnabled) {
            return false;
        }

        return true;
    }

    static public synchronized boolean init(boolean force) {
        if (force && sHasNative == NativeStatus.UNINITIALIZED) {
            sHasNative = NativeStatus.ENABLED;
        }

        return init();
    }

    static public synchronized void setNativeLibraries(String[] nativeLibraries) {
        sNativeLibraries = nativeLibraries;
    }

    /**
     * Test if the native image processing library was loaded successfully
     *
     * @return true on success
     */
    static public boolean hasNative() {
        return init();
    }

    /**
     * Get version number of the current library
     * Version number is encoded as (major * 10000 + minor * 100 + release)
     * @return version
     */
    public static int getVersion() {
        if (hasNative()) {
            return native_version();
        }
        return 0;
    }

    /**
     * Convert the ARGB color to its HSV equivalent
     * @param rgb Color in AARRGGBB format
     * @return Color in AAHHSSVV format
     */
    public static int RGBtoHSV(int rgb) {
        if (hasNative()) {
            return native_RGBtoHSV(rgb);
        }
        return 0;
    }

    /**
     * Convert the HSV color to its ARGB equivalent
     * @param hsv Color in AAHHSSVV format
     * @return Color in AARRGGBB format
     */
    public static int HSVtoRGB(int hsv) {
        if (hasNative()) {
            return native_HSVtoRGB(hsv);
        }
        return 0;
    }

    /**
     * Return the Hue of a HSV color in degrees, in the range [0..360[
     * @param hsv Color in AAHHSSVV format
     * @return Hue in degrees
     */
    public static int getHue(int hsv) {
        return (hsv >> 16) & 0xff;
    }

    public static int[] quantize(String filename, int ncolors, int maxWidth, int maxHeight) {
        final int colors[];
        if (hasNative()) {
            colors = native_quantize(filename, ncolors, maxWidth, maxHeight);
        } else {
            colors = null;
        }

        return colors;
    }

    public static int[] getDominantColors(String filename, int ncolors, int maxWidth, int maxHeight) {
        final int regions[] = Ymagine.quantize(filename, ncolors, maxWidth, maxHeight);
        if (regions == null) {
            return  null;
        }

        int nregions = regions.length / 2;
        int colors[] = new int[nregions];
        for (int i = 0; i < nregions; i++) {
            colors[i] = regions[2 * i];
        }

        return colors;
    }

    public static int getDominantHue(String filename, int ncolors, int maxWidth, int maxHeight) {
        int[] colors = getDominantColors(filename, ncolors, maxWidth, maxHeight);
        if (colors == null || colors.length < 1) {
            return 0;
        }

        int hsv = RGBtoHSV(colors[0]);
        int hue = getHue(hsv);

        return (hue * 360) / 256;
    }

    public static int getDominantHue(String filename) {
        return getDominantHue(filename, 8, 64, 64);
    }

    public static int transcode(InputStream is, OutputStream os, int maxWidth, int maxHeight) {
        Options options = new Options();
        options.setMaxSize(maxWidth, maxHeight);
        return transcode(is, os, options);
    }

    public static int transcode(InputStream is, OutputStream os, Options options) {
        if (hasNative()) {
            return native_transcodeStream(is, os, options);
        }
        return -1;
    }

    public static int encode(Vbitmap vbitmap, OutputStream os, Options options) {
        if (hasNative()) {
            return native_encodeStream(vbitmap, os, options);
        }
        return -1;
    }

    public static int encode(Vbitmap vbitmap, OutputStream os) {
        return encode(vbitmap, os, null);
    }
}
