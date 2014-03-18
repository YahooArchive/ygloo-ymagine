// Portions of this file taken from
// http://src.chromium.org/viewvc/chrome/trunk/src/base/android/java/src/org/chromium/base/library_loader/LibraryLoaderHelper.java

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.yahoo.mobile.client.android.ymagine;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * The class provides helper functions to extract native libraries from APK,
 * and load libraries from there.
 * <p/>
 * The class should be package-visible only, but made public for testing
 * purpose.
 */
public class LibraryLoaderHelper {
    private static final String TAG = "LibraryLoaderHelper";

    private static final String LIB_DIR = "lib";

    /**
     * One-way switch becomes true if native libraries were unpacked
     * from APK.
     */
    private static boolean sLibrariesWereUnpacked = false;

    /**
     * Version code extracted from the PackageInfo.
     */
    private static int sVersionCode = 0;

    /**
     * Try to load native libraries. We attempt to load libraries in the following order:
     * 1.) {@link java.lang.System#loadLibrary(String)}.
     * 2.) {@link java.lang.System#load(String)}.
     * 3.) {@link #tryLoadLibrariesUsingWorkaround(android.content.Context, String, String[])}
     *
     * @param context Context used when extracting libraries from APK.
     * @param libraries Names of libraries to load.
     *
     * @return <code>true</code> if loading was successful, otherwise <code>false</code>.
     */
    public static boolean tryLoadLibraries(Context context, String[] libraries) {
        for (String library : libraries) {
            if (!loadLibrary(library) &&
                    !tryLoadLibrariesUsingWorkaround(context, library, libraries)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Try to load a native library using a workaround of
     * http://b/13216167.
     * <p/>
     * Workaround for b/13216167 was adapted from code in
     * https://googleplex-android-review.git.corp.google.com/#/c/433061
     * <p/>
     * More details about http://b/13216167:
     * PackageManager may fail to update shared library.
     * <p/>
     * Native library directory in an updated package is a symbolic link
     * to a directory in /data/app-lib/<package name>, for example:
     * /data/data/com.android.chrome/lib -> /data/app-lib/com.android.chrome[-1].
     * When updating the application, the PackageManager create a new directory,
     * e.g., /data/app-lib/com.android.chrome-2, and remove the old symlink and
     * recreate one to the new directory. However, on some devices (e.g. Sony Xperia),
     * the symlink was updated, but fails to extract new native libraries from
     * the new apk.
     * <p/>
     * We make the following change to alleviate the issue:
     * first try to load the library using System.loadLibrary,
     * if that failed due to the library file was not found,
     * search the named library in a /data/data/com.android.chrome/app_lib/{versionCode}
     * directory.
     * <p/>
     * If named library is not in /data/data/com.android.chrome/app_lib/{versionCode} directory,
     * extract native libraries from apk and cache in the directory.
     * <p/>
     * This function doesn't throw UnsatisfiedLinkError, the caller needs to
     * check the return value.
     */
    public static boolean tryLoadLibrariesUsingWorkaround(Context context, String library,
            String[] allLibraries) {
        if (context == null) {
            return false;
        }
        File libFile = getWorkaroundLibFile(context, library);
        if (!libFile.exists() && !unpackLibrariesOnce(context, allLibraries)) {
            return false;
        }
        try {
            System.load(libFile.getAbsolutePath());
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Error loading libs", e);
            return false;
        }
        return true;
    }

    // Helper for loading a shared library with non-default search path
    static private final String SYSTEM_LIBDIR = "system/vendor/lib";

    private static boolean loadLibrary(String libName) {
        try {
            System.loadLibrary(libName);
            return true;
        } catch (UnsatisfiedLinkError e) {
            // Failed to load, try to resolve from system
            String libTail = System.mapLibraryName(libName);
            File libDir = new File(Environment.getRootDirectory(), SYSTEM_LIBDIR);
            File libFile = new File(libDir, libTail);
            if (libFile.isFile()) {
                try {
                    System.load(libFile.getAbsolutePath());
                    return true;
                } catch (UnsatisfiedLinkError e2) {
                    return false;
                }
            }
        }
        return false;
    }

    /**
     * Returns the directory for holding extracted native libraries.
     * It may create the directory if it doesn't exist.
     *
     * @param context
     * @return the directory file object
     */
    private static File getWorkaroundLibDir(Context context) {
        return context.getDir(LIB_DIR, Context.MODE_PRIVATE);
    }

    private static File getVersionedWorkaroundLibDir(Context context) {
        File libDir = getWorkaroundLibDir(context);
        libDir = new File(libDir, Integer.toString(getVersionCode(context)));
        libDir.mkdir();
        return libDir;
    }

    private static int getVersionCode(Context context) {
        if (sVersionCode == 0) {
            PackageManager packageManager = context.getPackageManager();
            if (packageManager != null) {
                try {
                    PackageInfo info = packageManager.getPackageInfo(context.getPackageName(), 0);
                    sVersionCode = info.versionCode;
                } catch (PackageManager.NameNotFoundException e) {
                    Log.e(TAG, "Failed to load version", e);
                }
            }
        }

        return sVersionCode;
    }

    private static File getWorkaroundLibFile(Context context, String library) {
        String libName = System.mapLibraryName(library);
        return new File(getVersionedWorkaroundLibDir(context), libName);
    }

    /**
     * Unpack native libraries from the APK file. The method is supposed to
     * be called only once. It deletes existing files in unpacked directory
     * before unpacking.
     *
     * @param context
     * @return true when unpacking was successful, false when failed or called
     * more than once.
     */
    private static boolean unpackLibrariesOnce(Context context, String[] libraries) {
        if (sLibrariesWereUnpacked) {
            return false;
        }
        sLibrariesWereUnpacked = true;

        File libDir = getWorkaroundLibDir(context);
        deleteDirectorySync(libDir);

        try {
            ApplicationInfo appInfo = context.getApplicationInfo();
            ZipFile file = new ZipFile(new File(appInfo.sourceDir), ZipFile.OPEN_READ);
            for (String libName : libraries) {
                String jniNameInApk = "lib/" + Build.CPU_ABI + "/" +
                        System.mapLibraryName(libName);

                final ZipEntry entry = file.getEntry(jniNameInApk);
                if (entry == null) {
                    Log.e(TAG, appInfo.sourceDir + " doesn't have file " + jniNameInApk);
                    file.close();
                    deleteDirectorySync(libDir);
                    return false;
                }

                File outputFile = getWorkaroundLibFile(context, libName);

                Log.i(TAG, "Extracting native libraries into " + outputFile.getAbsolutePath());

                assert !outputFile.exists();

                try {
                    if (!outputFile.createNewFile()) {
                        throw new IOException();
                    }

                    InputStream is = null;
                    FileOutputStream os = null;
                    try {
                        is = file.getInputStream(entry);
                        os = new FileOutputStream(outputFile);
                        int count = 0;
                        byte[] buffer = new byte[16 * 1024];
                        while ((count = is.read(buffer)) > 0) {
                            os.write(buffer, 0, count);
                        }
                    } finally {
                        try {
                            if (is != null)
                                is.close();
                        } finally {
                            if (os != null)
                                os.close();
                        }
                    }
                    // Change permission to rwxr-xr-x
                    outputFile.setReadable(true, false);
                    outputFile.setExecutable(true, false);
                    outputFile.setWritable(true);
                } catch (IOException e) {
                    if (outputFile.exists()) {
                        if (!outputFile.delete()) {
                            Log.e(TAG, "Failed to delete " + outputFile.getAbsolutePath());
                        }
                    }
                    file.close();
                    throw e;
                }
            }
            file.close();
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to unpack native libraries", e);
            deleteDirectorySync(libDir);
            return false;
        }
    }

    private static void deleteDirectorySync(File dir) {
        try {
            File[] files = dir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!file.delete()) {
                        Log.e(TAG, "Failed to remove " + file.getAbsolutePath());
                    }
                }
            }
            if (!dir.delete()) {
                Log.w(TAG, "Failed to remove " + dir.getAbsolutePath());
            }
            return;
        } catch (Exception e) {
            Log.e(TAG, "Failed to remove old libs, ", e);
        }
    }
}