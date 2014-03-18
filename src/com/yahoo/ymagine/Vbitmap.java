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

public class Vbitmap {
   /* Pointer to native Vbitmap object. This is set by native constructor,
    * and is then passed to all native methods as first argument
    *
    * @hide
    */
    private long mNativeHandle = 0;

    
    // Keep to finalize native resources
    private final VbitmapFinalizer mFinalizer;

    private static native void native_destructor(long nativeHandle);
    private static native long native_create();

    private native long native_retain(long nativeHandle);
    private native long native_release(long nativeHandle);
    private native int native_getWidth(long nativeHandle);
    private native int native_getHeight(long nativeHandle);
    private native int native_decodeFile(long nativeHandle, String filename, int maxWidth, int maxHeight);
    private native int native_decodeStream(long nativeHandle, InputStream inStream, int maxWidth, int maxHeight);

   /**
    * @noinspection UnusedDeclaration
    */
   /*
    * Private constructor that must received an already allocated native
    * Vbitmap reference (i.e. pointer, as long).
    *
    * This can be called from JNI code.
    */
    private Vbitmap(long nativeHandle) {
        // we delete this in our finalizer
        mNativeHandle = nativeHandle;
        mFinalizer = new VbitmapFinalizer(nativeHandle);
    }
    
    public Vbitmap() {
        this(native_create());
    }

    private static class VbitmapFinalizer {
        private final long mNativeHandle;
        
        VbitmapFinalizer(long nativeHandle) {
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

    public static Vbitmap create() {
        Vbitmap vbitmap = null;
        long nativeHandle = native_create();
        if (nativeHandle != 0L) {
            vbitmap = new Vbitmap(nativeHandle);
        }

        return vbitmap;
    }

    private synchronized long retain() {
        long handle = native_retain(mNativeHandle);
        if (handle == 0L) {
            return 0L;
        }

        return handle;
    }

    private synchronized long release() {
        long handle = native_release(mNativeHandle);
        if (handle == 0L) {
            return 0L;
        }

        return 0L;
    }

    public int getWidth() {
        if (mNativeHandle == 0L) {
            return 0;
        }

        return native_getWidth(mNativeHandle);
    }

    public int getHeight() {
        if (mNativeHandle == 0L) {
            return 0;
        }

        return native_getHeight(mNativeHandle);
    }

    public int decode(String filename, int maxWidth, int maxHeight) {
        if (mNativeHandle == 0L) {
            return -1;
        }

        return native_decodeFile(mNativeHandle, filename, maxWidth, maxHeight);
    }


    public int decode(InputStream inStream, int maxWidth, int maxHeight) {
        if (mNativeHandle == 0L) {
            return -1;
        }

        return native_decodeStream(mNativeHandle, inStream, maxWidth, maxHeight);
    }

    public void recycle() {
        long nativeHandle = mNativeHandle;

        mNativeHandle = 0L;

        native_destructor(nativeHandle);
    }

    static {
        Ymagine.Init();
    }
}
