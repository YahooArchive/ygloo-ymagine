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

package com.yahoo.mobile.client.android.ymagine;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;

import com.yahoo.ymagine.Shader;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 *
 *
 * @see <a href="http://developer.android.com/reference/android/graphics/BitmapFactory.html">http://developer.android.com/reference/android/graphics/BitmapFactory.html</a>
 */
public class BitmapFactory {
    private static final String LOG_TAG = "ymagine::bitmap";

    private static final boolean DEBUG_INSTR = false;
    private static final boolean DEBUG_PERF = false;

    // Those definitions must match the ones in bitmap.h
    private static final int SCALE_LETTERBOX = 0;
    private static final int SCALE_CROP = 1;
    private static final int SCALE_FIT = 2;

    // Default resolution to run quantize on
    private static final int THUMBNAIL_SIZE = 64;

    private static final String[] NATIVE_LIBRARIES = { "yahoo_ymagine" };
    private enum NativeStatus {
        UNINITIALIZED,
        NEED_WORKAROUND,
        DISABLED,
        ENABLED
    }

    // Global setting for enabling or disabling JNI interface. Set to NativeStatus.UNINITIALIZED
    // for automatic initialization, NativeStatus.DISABLED for forcing native support off
    private static NativeStatus sHasNative = NativeStatus.UNINITIALIZED;

    // Set-able parameter to allow caller to turn native API off
    private static boolean sEnabled = true;

    /**
     * All supported image formats
     */
    public enum ImageFormat {
        JPEG,
        PNG,
        GIF,
        WEBP,
        UNKNOWN
    }

    /**
     * List of valid supported compose modes
     *
     * This array must match the one named in compose.h
     */
    public enum ComposeMode {
        REPLACE,
        OVER,
        UNDER,
        PLUS,
        MINUS,
        ADD,
        SUBTRACT,
        DIFFERENCE,
        BUMP,
        MAP,
        MIX,
        MULT,
        LUMINANCE,
        LUMINANCEINV,
        COLORIZE
    }


    /**
     * Options for image operations
     *
     * @see <a href="http://developer.android.com/reference/android/graphics/BitmapFactory.Options.html">http://developer.android.com/reference/android/graphics/BitmapFactory.Options.html</a>
     */
    public static class Options extends android.graphics.BitmapFactory.Options {

        /**
         * Maximum width of resulting image
         */
        public int inMaxWidth;

        /**
         * Maximum height of resulting image
         */
        public int inMaxHeight;

        /**
         * Preserve aspect ratio or not
         */
        public boolean inKeepRatio;

        /**
         * Allow cropping, if not the result might be letterboxed.
         */
        public boolean inCrop;

        /**
         * Scale to fit or not
         */
        public boolean inFit;

        /**
         * If the ymagine library should be used for processing
         */
        public boolean inNative;

        /**
         * Input stream providing image data
         */
        public OutputStream inStream;

        /**
         *  android.graphics.BitmapFactory.Options has inBitmap only for API >= 11
         */
        public Bitmap inBitmap;

        // Filtering option
        public boolean inFilterBlur;
        public Shader inShader;

        /**
         * Hint for quality vs. speed
         */
        public int inQuality;
        // Panoramic meta data
        // Subset of the XMP data described at https://developers.google.com/photo-sphere/metadata/
        public int outPanoMode;
        public int outPanoCroppedWidth;
        public int outPanoCroppedHeight;
        public int outPanoFullWidth;
        public int outPanoFullHeight;
        public int outPanoX;
        public int outPanoY;

        /**
         * Default values are set in constructor
         */
        public Options() {
            inMaxWidth = -1;
            inMaxHeight = -1;
            inKeepRatio = true;
            inCrop = false;
            inFit = false;
            inBitmap = null;
            inStream = null;
            // Default to using our native decoder
            inNative = true;
            // No filter by default
            inFilterBlur = false;
            inShader = null;
            // Default to good quality
            inQuality = 80;
            // No panoramic/sphere by default
            outPanoMode = 0;
            outPanoCroppedWidth = 0;
            outPanoCroppedHeight = 0;
            outPanoFullWidth = 0;
            outPanoFullHeight = 0;
            outPanoX = 0;
            outPanoY = 0;
        }
    }

    /**
     * Test if the native image processing library was loaded successfully
     *
     * @return true on success
     */
    static public synchronized boolean init(Context context) {
        if ( (sHasNative == NativeStatus.UNINITIALIZED) || (sHasNative == NativeStatus.NEED_WORKAROUND && context != null) ) {
            if (LibraryLoaderHelper.tryLoadLibraries(context, NATIVE_LIBRARIES)) {
                sHasNative = NativeStatus.ENABLED;
            } else if (context == null) {
                /*
                 * First attempt to load libraries the standard way failed. If a context is provided, will try again
                 * using the load work-around
                 */
                sHasNative = NativeStatus.NEED_WORKAROUND;
            } else {
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

    static public boolean hasNative() {
        return init(null);
    }

    /**
     * Copy contents of one bitmap into another bitmap
     * @param src source bitmap
     * @param dst destination bitmap
     * @return the destination bitmap
     */
    public static Bitmap copyBitmap(Bitmap src, Bitmap dst) {
        int height = dst.getHeight();
        int width = dst.getWidth();

        if (hasNative()) {
            return native_copyBitmap(src, dst, width, height, SCALE_CROP);
        }

        return null;
    }

    /**
     * Populate contents of given bitmap (in place) with the image described by the given NV21 buffer.
     * This method will perform an implicit YUV to RGB conversion.
     *
     * @param bitmap destination
     * @param data NV21 (YUV) source
     * @param width
     * @param height
     * @return destination bitmap
     */
    public static Bitmap decodeNV21ByteArray(Bitmap bitmap, byte[] data, int width, int height) {
        if (hasNative()) {
            return native_decodeNV21ByteArray(bitmap, data, width, height);
        }

        return null;
    }

    /**
     * Decode an immutable bitmap from the specified byte array.
     *
     * @param data input byte array
     * @param offset offset to begin reading at from byte array
     * @param length length of data in byte array
     * @return
     */
    public static Bitmap decodeByteArray(byte[] data, int offset, int length) {
        return decodeByteArray(data, offset, length, null, null);
    }

    /**
     * Decode an immutable bitmap from the specified byte array using Options opts.
     *
     * @param data input byte array
     * @param offset offset to begin reading at from byte array
     * @param length length of data in byte array
     * @param opts Options to use during decoding
     * @return bitmap created from byte array
     */
    public static Bitmap decodeByteArray(byte[] data, int offset, int length,
            BitmapFactory.Options opts) {
        return decodeByteArray(data, offset, length, null, opts);
    }

    /**
     * Decode an immutable bitmap from the specified byte array using Options opts and given
     * output padding.
     *
     * @param data input byte array
     * @param offset offset to begin reading at from byte array
     * @param length length of data in byte array
     * @param opts Options to use during decoding
     * @param outPadding output padding
     * @return bitmap created from byte array
     */
    public static Bitmap decodeByteArray(byte[] data, int offset, int length,
            Rect outPadding, BitmapFactory.Options opts) {
        Bitmap bm = null;
        ByteArrayInputStream stream = null;

        try {
            stream = new ByteArrayInputStream(data, offset, length);
            bm = doDecode(stream, outPadding, opts);
        } catch (Exception e) {
            // On error, silently fallback to returning null
       } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException e) {
                    // do nothing
                }
            }
        }
        return bm;
    }

    /**
     * Decode contents of file at path pathName into a bitmap.
     *
     * @param pathName path to file to be decoded
     * @return Bitmap created from file contents
     */
    public static Bitmap decodeFile(String pathName) {
        return decodeFile(pathName, null, null);
    }

    /**
     * Decode a file path into a bitmap using Options opts.
     *
     * @param pathName path to file to be decoded
     * @param opts Options to use during decoding
     * @return Bitmap created from file contents
     */
    public static Bitmap decodeFile(String pathName, BitmapFactory.Options opts) {
        return decodeFile(pathName, null, opts);
    }

    /**
     * Decode a file path into a bitmap using the given options and output padding.
     *
     * @param pathName path to file to be decoded
     * @param outPadding output padding
     * @param opts Options to use during decoding
     * @return Bitmap created from file contents
     */
    public static Bitmap decodeFile(String pathName,
            Rect outPadding, BitmapFactory.Options opts) {
        Bitmap bm = null;
        FileInputStream stream = null;

        try {
            stream = new FileInputStream(new File(pathName));
            bm = doDecode(stream, outPadding, opts);
        } catch (Exception e) {
            // On error, silently fallback to returning null
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException e) {
                    // do nothing
                }
            }
        }

        return bm;
    }

    /**
     * Decode a bitmap from a file descriptor with null options and null padding.
     *
     * @param fd to read Bitmap data from
     * @return Bitmap created with data read from fd
     */
    public static Bitmap decodeFileDescriptor(FileDescriptor fd) {
        return decodeFileDescriptor(fd, null, null);
    }

    /**
     * Decode a bitmap from the file descriptor.
     *
     * @param fd to read Bitmap data from
     * @param outPadding output padding
     * @param opts Options to be used during decoding
     * @return Bitmap created with data read from fd
     */
    public static Bitmap decodeFileDescriptor(FileDescriptor fd,
            Rect outPadding, BitmapFactory.Options opts) {
        Bitmap bm = null;
        FileInputStream stream = null;

        try {
            stream = new FileInputStream(fd);
            bm = doDecode(stream, outPadding, opts);
        } catch (Exception e) {
            // On error, silently fallback to returning null
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException e) {
                    // do nothing
                }
            }
        }

        return bm;
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeResource(Resources, int, Options) decodeResource} with null options.
     *
     * @param res
     * @param id
     * @return
     */
    public static Bitmap decodeResource(Resources res, int id) {
        return decodeResource(res, id, null);
    }

    /**
     * Decode a new bitmap from a resource file
     *
     * @param res Application resources
     * @param id of the resource to read the data from
     * @param opts Options to be used for decoding
     * @return Bitmap created with the data of the resource
     */
    public static Bitmap decodeResource(Resources res, int id, BitmapFactory.Options opts) {
        Bitmap bm = null;
        InputStream stream = null;

        try {
            final TypedValue value = new TypedValue();
            stream = res.openRawResource(id, value);
            bm = decodeResourceStream(res, value, stream, null, opts);
        } catch (Exception e) {
            // On error, silently fallback to returning null
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException e) {
                    // do nothing
                }
            }
        }

        return bm;
    }


    /**
     *  Decode a Bitmap from a Resource InputStream
     *
     * @param res Ressource the stream was obtained from
     * @param value Density
     * @param is InputStream
     * @param pad Padding
     * @param opts Options
     * @return Bitmap created from InputStream
     */
    public static Bitmap decodeResourceStream(Resources res, TypedValue value, InputStream is,
            Rect pad, BitmapFactory.Options opts) {
        if (opts == null) {
            opts = new Options();
        }

        if (opts.inDensity == 0 && value != null) {
            final int density = value.density;
            if (density == TypedValue.DENSITY_DEFAULT) {
                opts.inDensity = DisplayMetrics.DENSITY_DEFAULT;
            } else if (density != TypedValue.DENSITY_NONE) {
                opts.inDensity = density;
            }
        }

        if (opts.inTargetDensity == 0 && res != null) {
            opts.inTargetDensity = res.getDisplayMetrics().densityDpi;
        }

        return decodeStream(is, pad, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeStream(InputStream, Rect, Options) decodeStream} with
     * default Options and null Rect.
     *
     * @param is
     * @return Bitmap created from InputStream
     */
    public static Bitmap decodeStream(InputStream is) {
        return decodeStream(is, null, null);
    }

    /**
     * Decode an input stream into a bitmap using the given options and padding.
     *
     * @param is InputStream to read bitmap data from
     * @param outPadding output padding
     * @param opts Options to use for decoding
     * @return Bitmap created with data read from is
     */
    public static Bitmap decodeStream(InputStream is,
            Rect outPadding, BitmapFactory.Options opts) {
        return doDecode(is, outPadding, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeFile(String, Rect, Options) with null Rect
     * and null Options.
     *
     * @param bitmap
     * @param pathName
     * @return
     */
    public static Bitmap decodeInBitmap(Bitmap bitmap, String pathName) {
        BitmapFactory.Options opts = new BitmapFactory.Options();

        if (bitmap != null) {
            opts.inMaxWidth = bitmap.getWidth();
            opts.inMaxHeight = bitmap.getHeight();
            opts.inCrop = true;
            opts.inFit = true;
            opts.inKeepRatio = true;
            opts.inBitmap = bitmap;
        }

        return decodeFile(pathName, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeStream(InputStream, Rect, Options) decodeStream} with
     * default Options and null Rect.
     *
     * @param is
     * @param maxWidth of resulting Bitmap
     * @param maxHeight of resulting Bitmap
     * @return Bitmap
     */
    public static Bitmap decodeInBitmap(Bitmap bitmap, InputStream is) {
        BitmapFactory.Options opts = new BitmapFactory.Options();

        if (bitmap != null) {
            opts.inMaxWidth = bitmap.getWidth();
            opts.inMaxHeight = bitmap.getHeight();
            opts.inCrop = true;
            opts.inFit = true;
            opts.inKeepRatio = true;
            opts.inBitmap = bitmap;
        }

        return decodeStream(is, null, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeFile(String, Rect, Options) decodeFile} with
     * Options.inMaxWidth and Options.inMaxHeight set to the given values.
     *
     * @param pathName
     * @param maxWidth of resulting Bitmap
     * @param maxHeight of resulting Bitmap
     * @return Bitmap
     */
    public static Bitmap decode(String pathName, int maxWidth, int maxHeight) {
        BitmapFactory.Options opts = new BitmapFactory.Options();

        opts.inMaxWidth = maxWidth;
        opts.inMaxHeight = maxHeight;
        opts.inCrop = true;
        opts.inFit = false;
        opts.inKeepRatio = true;
        opts.inBitmap = null;

        return decodeFile(pathName, null, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#decodeStream(InputStream, Rect, Options) decodeStream} with
     * Options.inMaxWidth and Options.inMaxHeight set to the given values.
     *
     * @param is
     * @param maxWidth of resulting Bitmap
     * @param maxHeight of resulting Bitmap
     * @return Bitmap
     */
    public static Bitmap decode(InputStream is, int maxWidth, int maxHeight) {
        BitmapFactory.Options opts = new BitmapFactory.Options();

        opts.inMaxWidth = maxWidth;
        opts.inMaxHeight = maxHeight;
        opts.inCrop = true;
        opts.inFit = false;
        opts.inKeepRatio = true;
        opts.inBitmap = null;

        return decodeStream(is, null, opts);
    }

    /**
     * Alias for {@link com.yahoo.mobile.client.android.ymagine.BitmapFactory#quantize(Bitmap, int, int, int) quantize}
     * with default maxWidth and maxHeight.
     *
     * @param bitmap
     * @param ncolors
     * @return
     */
    public static int[] quantize(Bitmap bitmap, int ncolors) {
        return quantize(bitmap, ncolors, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
    }

    /**
     * Extract dominant colors in a bitmap after resizing the bitmap to the given size. Downsize+quantize
     * of large pictures is faster (with a small loss of accuracy, if any) than quantizing the large image.
     *
     * @param bitmap to be quantized
     * @param ncolors number of top colors to return
     * @param maxWidth maximum width to resize bitmap to before quantization
     * @param maxHeight maximum height to resize bitmap to before quantization
     *
     * @return
     */
    public static int[] quantize(Bitmap bitmap, int ncolors, int maxWidth, int maxHeight)
    {
        int nclusters = 0;

        if (bitmap == null || ncolors <= 0) {
            return null;
        }

        if (!hasNative()) {
            return null;
        }

        bitmap = scaleBitmap(bitmap, maxWidth, maxHeight, SCALE_FIT);
        if (bitmap == null) {
            return null;
        }

        if (ncolors > 16) {
            ncolors = 16;
        }

        int[] colors = native_quantize(bitmap, ncolors);
        if (colors == null || colors.length <= 0) {
            return null;
        }

        if (DEBUG_PERF) {
            for (int i = 0; i < nclusters; i++) {
                Log.i(LOG_TAG, String.format("Color[%d] = #%08x", i, colors[i]));
            }
        }

        return colors;
    }

    /**
     * @param bitmap
     * @return
     */
    public static int getThemeColor(Bitmap bitmap)
    {
        return getThemeColor(bitmap, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
    }

    public static int getThemeColor(Bitmap bitmap, int maxSize)
    {
        return getThemeColor(bitmap, maxSize, maxSize);
    }

    /**
     * Extract theme color from a bitmap
     */
    public static int getThemeColor(Bitmap bitmap, int maxWidth, int maxHeight)
    {
        if (bitmap == null) {
            return Color.TRANSPARENT;
        }
        if (hasNative()) {
            bitmap = scaleBitmap(bitmap, maxWidth, maxHeight, SCALE_FIT);
            return native_getThemeColor(bitmap);
        }
        return Color.WHITE;
    }

    /**
     * Blurs and returns a copy of a bitmap
     * @param bitmap bitmap to blur
     * @param rad radius of the blur
     */
    public static Bitmap blur(Bitmap bitmap, int rad) {
        return blur(bitmap, rad, -1, -1, false);
    }

    /**
     * Blurs and returns a copy of a bitmap
     * @param bitmap bitmap to blur
     * @param rad radius of the blur
     */
    public static Bitmap blur(Bitmap bitmap, int rad,
                              int maxWidth, int maxHeight) {
        return blur(bitmap, rad, maxWidth, maxHeight, false);
    }

    /**
     * Scales and then blurs and returns a copy of a bitmap
     * @param bitmap bitmap to blur
     * @param radius radius of the blur
     * @param maxWidth max width of output image
     * @param maxHeight max height of output image
     */
    public static Bitmap blur(Bitmap bitmap, int radius,
                              int maxWidth, int maxHeight,
                              boolean inPlace) {
        if (bitmap == null) {
            return null;
        }

        /* No need for full high-res blur */
        Bitmap b = scaleBitmap(bitmap, maxWidth, maxHeight, SCALE_FIT);
        /*
         * native method applies blur in place, so need to get sure
         * the bitmap passed as argument is mutable and, if inPlace
         * is false, is different from the input one
         */
        if (b != null) {
            if ( (!bitmap.isMutable()) ||
                 ( (b == bitmap) && !inPlace) ||
                 (b.getConfig() != Bitmap.Config.ARGB_8888) ) {
                /* Need to apply blur on a mutable copy */
                b = b.copy(Bitmap.Config.ARGB_8888, true);
            }
            if (b != null) {
                if (hasNative()) {
                    native_blur(b, radius);
                }
            }
        }

        return b;
    }

    /**
     * Apply colorize filter on bitmap pixels
     * @param bitmap bitmap on bottom
     * @param color color for the colorize effect
     * @return true if success
     */
    public static boolean colorize(Bitmap bitmap, int color) {
        if (bitmap == null) {
            return false;
        }
        if (!bitmap.isMutable()) {
            Log.e(LOG_TAG, "Bitmap not mutable, compose failed");
            return false;
        }
        if (!hasNative()) {
            return false;
        }

        return (native_colorize(bitmap, color) >= 0);
    }

    /**
     * Performs a specified compose function using a bitmap
     * with only specified color as one input
     * @param bitmap bitmap on bottom
     * @param color color on top
     * @param mode composition function to use
     * @return true if success
     */
    public static boolean compose(Bitmap bitmap, int color, ComposeMode mode) {
        if (bitmap== null) {
            return false;
        }
        if (!bitmap.isMutable()) {
            Log.e(LOG_TAG, "Bitmap not mutable, compose failed");
            return false;
        }
        if (!hasNative()) {
            return false;
        }

        return (native_compose(bitmap, color, mode.ordinal()) >= 0);
    }

    /**
     * apply shader to bitmap
     * @param shader @see Shader
     * @return true if success
     */
    public static boolean applyShader(Bitmap bitmap, Shader shader) {
        if (bitmap== null) {
            return false;
        }
        if (!bitmap.isMutable()) {
            Log.e(LOG_TAG, "Bitmap not mutable, compose failed");
            return false;
        }
        if (!hasNative()) {
            return false;
        }

        return (native_applyShader(bitmap, shader) >= 0);
    }

    // Debug helper to log full call stack
    private static void dumpStack(int minlevel) {
        final int skipstack = 3 + minlevel;
        StackTraceElement[] callStack = Thread.currentThread().getStackTrace();
        if (callStack.length >= skipstack) {
            Log.d(LOG_TAG, callStack[skipstack].getMethodName());
            for (int i = 0; i + skipstack < callStack.length; i++) {
                Log.d(LOG_TAG, " #" + i + " " + callStack[i + skipstack].toString());
            }
        }
    }

    private static int calculateInSampleSize(int inWidth, int inHeight, int reqWidth, int reqHeight) {
        int inSampleSize = 1;

        if (reqWidth <= 0 || reqHeight <= 0) {
            return inSampleSize;
        }
        if (inWidth <= 0 || inHeight <= 0) {
            return inSampleSize;
        }
        if (inHeight <= reqHeight && inWidth <= reqWidth) {
            return inSampleSize;
        }
        if (inWidth > inHeight) {
            inSampleSize = Math.round((float) inHeight / (float) reqHeight);
        } else {
            inSampleSize = Math.round((float) inWidth / (float) reqWidth);
        }
        return inSampleSize;
    }

    /**
     * Actual decoder for all kind of input. All the other decoding methods
     * are provided to support different arguments, but end up calling this
     * method
     */
    private static Bitmap doDecode(InputStream instream,
            Rect outPadding, BitmapFactory.Options opts)
    {
        Bitmap bm = null;
        long startDecode, endDecode;
        boolean trysystem = true;
        boolean completed = false;
        Bitmap inBitmap = null;
        int reqWidth = -1;
        int reqHeight = -1;
        int scaleMode = 0;
        int quality = 0;
        boolean useNative = false;

        if (DEBUG_PERF) {
            startDecode = System.nanoTime();
        }

        if (hasNative()) {
            useNative = true;
            if (opts != null) {
                useNative = opts.inNative;
            }
        }

        if (opts != null) {
            reqWidth = opts.inMaxWidth;
            reqHeight = opts.inMaxHeight;
            if (opts.inFit) {
                scaleMode = SCALE_FIT;
            } else if (opts.inCrop) {
                scaleMode = SCALE_CROP;
            } else {
                scaleMode = SCALE_LETTERBOX;
            }
            inBitmap = opts.inBitmap;
            quality = opts.inQuality;
        }

        BufferedInputStream bufferedstream = null;
        byte[] inheader = new byte[128];
        int nbytes = -1;

        try {
            bufferedstream = new BufferedInputStream(instream, 4 * 1024);
            bufferedstream.mark(inheader.length);
            nbytes = bufferedstream.read(inheader, 0, inheader.length);
            bufferedstream.reset();
        } catch (IOException e1) {
            Log.d(LOG_TAG, "Failed to read image magic header" + e1.getMessage());
            // e1.printStackTrace();
            nbytes = -1;
        }

        if (nbytes < 0) {
            /* Failed to determine image format due to I/O error, abort */
            if (bufferedstream != null) {
                try {
                    bufferedstream.close();
                } catch (IOException e) {
                }
            }
            return bm;
        }

        // Try to extract image format from input stream
        ImageFormat informat = getImageFormat(inheader, nbytes);

        if (useNative) {
            if (informat == ImageFormat.JPEG || informat == ImageFormat.WEBP) {
                if (opts != null && opts.inStream != null) {
                    // Run image transcoding, saving output as another
                    // jpeg image into stream
                    native_transcodeStream(bufferedstream, opts.inStream, reqWidth, reqHeight,
                                           scaleMode, quality);
                    completed = true;
                } else {
                    bm = native_decodeStreamOptions(bufferedstream, opts);
                    if (bm != null) {
                        completed = true;
                    } else if (opts != null && opts.inJustDecodeBounds) {
                        completed = true;
                    }
                }
            }
        }

        if (!completed) {
            if (trysystem) {
                // Fall back to standard BitmapFactory API
                if (opts == null) {
                    /* No special option requested, system decoder is compatible */
                    bm = android.graphics.BitmapFactory.decodeStream(bufferedstream, outPadding, null);
                } else {
                    // TODO(hassold): Emulate most of possible BitmapFactory options (scaling, ...)
                    // For now, takes our custom options as default
                    bm = android.graphics.BitmapFactory.decodeStream(bufferedstream, outPadding, opts);
                }
            }

            if (bm != null) {
                bm = scaleBitmap(bm, reqWidth, reqHeight, scaleMode);
                completed = true;
            }
        }

        if (bufferedstream != null) {
            try {
                bufferedstream.close();
            } catch (IOException e) {
            }

            bufferedstream = null;
        }

        if (DEBUG_PERF) {
            endDecode = System.nanoTime();
            if (bm != null) {
                Log.i(LOG_TAG, "doDecode in " +
                        (endDecode - startDecode) / 1000 + "us => " +
                        bm.getWidth() + "x" + bm.getHeight());
            }
            if (DEBUG_INSTR) {
                dumpStack(1);
            }
        }

        return bm;
    }

    /**
     * Determine image format of a byte buffer
     *
     * @param header Beginning of the image data
     * @param nbytes Length of header
     *
     * @return format of the Image
     */
    private static ImageFormat getImageFormat(byte[] header, int nbytes) {
        ImageFormat format = ImageFormat.UNKNOWN;

        if (header != null && header.length >= 8 && nbytes >= 8) {
            if (header[0] == (byte) 0xff && header[1] == (byte) 0xd8) {
                /* Only SOI [0xff,0xd8] is required as the first chunk,
                   next one can be any APPn [0xff,0xen], or even not
                   have any APPn, so don't look further */
                format = ImageFormat.JPEG;
            } else if (header[0] == (byte) 0x89 &&
                       header[1] == 'P' && header[2] == 'N' && header[3] == 'G' &&
                       header[4] == (byte) 0x0d && header[5] == (byte) 0x0a &&
                       header[6] == (byte) 0x1a && header[7] == (byte) 0x0a) {
                format = ImageFormat.PNG;
            } else if (header.length >= 16 && nbytes >= 16 &&
                       header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
                       header[8] == 'W' && header[9] == 'E' && header[10] == 'B' && header[11] == 'P' &&
                       header[12] == 'V' && header[13] == 'P' && header[14] == '8' && header[15] == ' ') {
                format = ImageFormat.WEBP;
            } else if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && header[3] == '8' &&
                       (header[4] == '7' || header[4] == '9') && header[5] == 'a') {
                format = ImageFormat.GIF;
            }
        }

        return format;
    }

    /**
     * Create scaled bitmap
     */
    private static Bitmap scaleBitmap(Bitmap bitmap,
                                      int maxWidth, int maxHeight,
                                      int scaleMode)
    {
        boolean useNative = false;

        if (bitmap == null) {
            return null;
        }

        int srcWidth = bitmap.getWidth();
        int srcHeight = bitmap.getHeight();

        int dstWidth = srcWidth;
        int dstHeight = srcHeight;

        if (maxWidth > 0 && dstWidth > maxWidth) {
            dstHeight = (dstHeight * maxWidth) / dstWidth;
            dstWidth = maxWidth;
        }
        if (maxHeight > 0 && dstHeight > maxHeight) {
            dstWidth = (dstWidth * maxHeight) / dstHeight;
            dstHeight = maxHeight;
        }

        if (dstWidth != srcWidth || dstHeight != srcHeight) {
            if (useNative) {
                bitmap = native_copyBitmap(bitmap, null,
                                           dstWidth, dstHeight,
                                           scaleMode);
            } else {
                bitmap = Bitmap.createScaledBitmap(bitmap,
                                                   dstWidth, dstHeight,
                                                   true);
            }
        }

        /* Vbitmap supports only RGBA config, enforce it */
        if (bitmap != null && bitmap.getConfig() != Bitmap.Config.ARGB_8888) {
            bitmap = bitmap.copy(Config.ARGB_8888, true);
        }

        return bitmap;
    }

    // native methods provided by JNI
    private static native Bitmap native_decodeFile(String pathName,
            Bitmap bitmap, int maxWidth, int maxHeight, int scalemode);

    private static native Bitmap native_decodeStream(InputStream is,
    Bitmap bitmap, int maxWidth, int maxHeight, int scalemode);

    private static native Bitmap native_decodeStreamOptions(InputStream is,
            BitmapFactory.Options opts);

    private static native Bitmap native_copyBitmap(Bitmap refbitmap,
    Bitmap bitmap, int maxWidth, int maxHeight, int scalemode);

    private static native Bitmap native_decodeNV21ByteArray(Bitmap bitmap, byte[] data, int width, int height);

    private static native int native_transcodeStream(InputStream is, OutputStream os,
     int maxWidth, int maxHeight,
     int scalemode, int quality);

    private static native int[] native_quantize(Bitmap bitmap, int ncolors);

    private static native int native_getThemeColor(Bitmap bitmap);

    private static native int native_blur(Bitmap bitmap, int radius);

    private static native int native_colorize(Bitmap bitmap, int color);

    private static native int native_compose(Bitmap bitmap, int color, int composeMode);

    private static native int native_applyShader(Bitmap bitmap, Shader shader);

    // Static initialization for JNI implementation
    static {
        hasNative();
    }
}
