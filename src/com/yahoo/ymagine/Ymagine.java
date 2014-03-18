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
    // Global setting for enabling or disabling JNI interface. Set
    // to -1 for automatic initialization, 0 for forcing off, 1 for
    // forcing on
    private static int sHasNative = -1;

    // Settable parameter to allow caller to turn native API off
    private static boolean sEnabled = true;

    // Directory for system libraries
    private static final String SYSTEM_LIB_DIR = "/system/vendor/lib";

    // Those definitions must match the ones in bitmap.h
    private static final int SCALE_LETTERBOX = 0;
    private static final int SCALE_CROP = 1;
    private static final int SCALE_FIT = 2;

    private static final int JPEG_DEFAULT_QUALITY = 80;

    private native static int native_version();
    private native static int[] native_quantize(String filename, int ncolors, int maxWidth, int maxHeight);

    private native static int native_RGBtoHSV(int huv);
    private native static int native_HSVtoRGB(int rgb);

    private native static int native_transcodeStream(InputStream is, OutputStream os,
            int maxWidth, int maxHeight,
            int scalemode, int quality, Shader shader);
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

    static public int Init() {
        if (sHasNative < 0) {
            sHasNative = 0;
            try {
                loadLibrary("yahoo_ymagine");
                sHasNative = 1;
            } catch (UnsatisfiedLinkError e) {
                // Log.e(LOG_TAG, "Native code library failed to load" + e);
            }
        }
        
        return sHasNative;
    }


    /**
     * Test if the native image processing library was loaded successfully
     *
     * @return true on success
     */
    static public boolean hasNative() {
        if (sHasNative <= 0) {
            return false;
        }
        if (!sEnabled) {
            return false;
        }

        return true;
    }

    /**
     * Static initialization of the native interface
     */
    static {
        Init();
    }

    /**
     * Get version number of the current library
     * Version number is encoded as (major * 10000 + minor * 100 + release)
     * @return version
     */
    public static int getVersion() {
        return native_version();
    }

    /**
     * Convert the ARGB color to its HSV equivalent
     * @param rgb Color in AARRGGBB format
     * @return Color in AAHHSSVV format
     */
    public static int RGBtoHSV(int rgb) {
        return native_RGBtoHSV(rgb);
    }

    /**
     * Convert the HSV color to its ARGB equivalent
     * @param hsv Color in AAHHSSVV format
     * @return Color in AARRGGBB format
     */
    public static int HSVtoRGB(int hsv) {
        return native_HSVtoRGB(hsv);
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
        final int colors[] = native_quantize(filename, ncolors, maxWidth, maxHeight);

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
        return transcode(is, os, maxWidth, maxHeight, null);
    }

    /**
     * transcode and apply shader during transcoding
     *
     * @param shader @see Shader
     * @return greater or equal to 0 if success
     */
    public static int transcode(InputStream is, OutputStream os, int maxWidth, int maxHeight, Shader shader) {
        return native_transcodeStream(is, os, maxWidth, maxHeight, SCALE_LETTERBOX, 99, shader);
    }
}
