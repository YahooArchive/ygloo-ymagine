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

public class Shader {
   /* Pointer to native Shader object. This is set by native constructor,
    * and is then passed to all native methods as first argument
    *
    * @hide
    */
    private long mNativeHandle = 0;

    
    // Keep to finalize native resources
    private final ShaderFinalizer mFinalizer;

    private static native void native_destructor(long nativeHandle);
    private static native long native_create();

    private static native int native_saturation(long nativeHandle, float value);
    private static native int native_exposure(long nativeHandle, float value);
    private static native int native_contrast(long nativeHandle, float value);
    private static native int native_brightness(long nativeHandle, float value);
    private static native int native_temperature(long nativeHandle, float value);
    private static native int native_whitebalance(long nativeHandle, float value);
    private static native int native_vignette(long nativeHandle, Vbitmap vbitmap, int composemode);
    private static native int native_preset(long nativeHandle, InputStream preset);

   /**
    * @noinspection UnusedDeclaration
    */
   /*
    * Private constructor that must received an already allocated native
    * Shader reference (i.e. pointer, as long).
    *
    * This can be called from JNI code.
    */
    private Shader(long nativeHandle) {
        // we delete this in our finalizer
        mNativeHandle = nativeHandle;
        mFinalizer = new ShaderFinalizer(nativeHandle);
    }
    
    public Shader() {
        this(native_create());
    }
    
    private static class ShaderFinalizer {
        private final long mNativeHandle;
        
        ShaderFinalizer(long nativeHandle) {
            mNativeHandle = nativeHandle;
        }
        
        @Override
        public void finalize() {
            try {
                super.finalize();
            } catch (Throwable t) {
                // Ignore
            } finally {
                native_destructor(mNativeHandle);
            }
        }
    }

    public static Shader create() {
        Shader shader = null;
        long nativeHandle = native_create();
        if (nativeHandle != 0L) {
            shader = new Shader(nativeHandle);
        }

        return shader;
    }


    public void recycle() {
        long nativeHandle = mNativeHandle;

        mNativeHandle = 0L;

        native_destructor(nativeHandle);
    }

    public int saturation(float value) {
        return native_saturation(mNativeHandle, value);
    }

    public int exposure(float value) {
        return native_exposure(mNativeHandle, value);
    }

    public int contrast(float value) {
        return native_contrast(mNativeHandle, value);
    }

    public int brightness(float value) {
        return native_brightness(mNativeHandle, value);
    }

    public int temperature(float value) {
        return native_temperature(mNativeHandle, value);
    }

    public int whitebalance(float value) {
        return native_whitebalance(mNativeHandle, value);
    }

    public int vignette(Vbitmap bitmap, int composemode) {
        return native_vignette(mNativeHandle, bitmap, composemode);
    }

    /**
     * set preset for Shader
     *
     * Preset is a mapping which maps a color from color space 0 - 255 to
     * the same 0 - 255 color space.
     *
     * @param preset InputStream containing array of bytes which serve as preset
     *        for pixels. The array of bytes should be of size 3*256. First section
     *        of 256 bytes is mapping for red channel, second section of 256 bytes
     *        is mapping for green channel, last section of 256 bytes is mapping for
     *        blue channel.
     *
     * @return true if success
     */
    public boolean preset(InputStream preset) {
        return (native_preset(mNativeHandle, preset) >= 0);
    }
    
    static {
        Ymagine.Init();
    }
}
