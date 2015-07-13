package com.yahoo.mobile.client.android.ymagine;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.os.Process;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class LibraryLoader {
    public static final String LOG_TAG = "LibraryLoader";
    public static final String CUSTOM_LIB_DIR = "lib";
    public static final String SYSTEM_LIB_DIR = "lib";
    public static final String SYSTEM_LIB64_DIR = "lib64";
    public static final String VENDOR_ROOT_DIR = "vendor";
    public static final String VENDOR_LIB_DIR = "lib";
    public static final String VENDOR_LIB64_DIR = "lib64";
    public static final long UPDATE_EPSILON_MS = 60 * 1000;

    private static long sUpdatedTime = 0;
    private static String sVersion = null;

    public static void loadLibraries(Context context, boolean fromAssets, boolean checkTimestamp, String[] libraries)
            throws UnsatisfiedLinkError, IllegalArgumentException {
        if (context == null) {
            throw new IllegalArgumentException("Null context");
        }
        Context appContext = context.getApplicationContext();

        if (!fromAssets && trySystemLibraries(appContext, false, checkTimestamp, libraries)) {
            if (!trySystemLibraries(appContext, true, checkTimestamp, libraries)) {
                throw new UnsatisfiedLinkError("Error loading libraries from APK");
            }
        } else {
            getPackageInfo(appContext);
            unpackAndLoadLibraries(appContext, fromAssets, libraries);
        }
    }

    public static void loadLibraries(Context context, boolean fromAssets, String[] libraries)
            throws UnsatisfiedLinkError, IllegalArgumentException {
        loadLibraries(context, fromAssets, true, libraries);
    }

    private static void getPackageInfo(Context appContext) {
        if (sVersion != null) {
            return;
        }
        PackageManager pm = appContext.getPackageManager();
        if (pm != null) {
            try {
                PackageInfo info = pm.getPackageInfo(appContext.getPackageName(), 0);
                sUpdatedTime = info.lastUpdateTime;
                if (sUpdatedTime == 0) {
                    sUpdatedTime = info.firstInstallTime;
                }
                sVersion = Integer.toString(info.versionCode) + "-" + Long.toString(sUpdatedTime);
            } catch (PackageManager.NameNotFoundException e) {
                Log.e(LOG_TAG, "Package information not found.", e);
            }
        }
        if (sVersion == null) {
            sVersion = "0";
        }
    }

    private static boolean trySystemLibraries(Context appContext, boolean load, boolean checkTimestamp, String[] libraries)
            throws UnsatisfiedLinkError {
        String libRoot = appContext.getApplicationInfo().nativeLibraryDir;
        File systemRootFile = android.os.Environment.getRootDirectory();
        File vendorRootFile = new File(systemRootFile, VENDOR_ROOT_DIR);
        /* Search path for shared libraries, as File objects */
        File preinstallRoot0 = new File(vendorRootFile, VENDOR_LIB64_DIR);
        File preinstallRoot1 = new File(vendorRootFile, VENDOR_LIB_DIR);
        File preinstallRoot2 = new File(systemRootFile, SYSTEM_LIB64_DIR);
        File preinstallRoot3 = new File(systemRootFile, SYSTEM_LIB_DIR);

        for (String libName : libraries) {
            String libTail = System.mapLibraryName(libName);

            File libFile = new File(libRoot, libTail);
            if (!libFile.exists()) {
                // Perhaps we are a pre-installed app.
                libFile = new File(preinstallRoot0, libTail);
                if (!libFile.exists()) {
                    libFile = new File(preinstallRoot1, libTail);
                    if (!libFile.exists()) {
                        libFile = new File(preinstallRoot2, libTail);
                        if (!libFile.exists()) {
                            libFile = new File(preinstallRoot3, libTail);
                        }
                    }
                }
            }
            if (load) {
                if (!libFile.exists()) {
                    Log.e(LOG_TAG, "Missing library " + libFile.getAbsolutePath());
                    throw new UnsatisfiedLinkError("Missing library: " + libName);
                }
                Log.i(LOG_TAG, "Loading library " + libFile.getAbsolutePath());
                System.load(libFile.getAbsolutePath());
            } else {
                if (!libFile.exists()) {
                    Log.e(LOG_TAG, "Can't find library " + libName);
                    return false;
                }
                if (checkTimestamp) {
                    getPackageInfo(appContext);
                    if (libFile.lastModified() + UPDATE_EPSILON_MS < sUpdatedTime) {
                        // The system didn't correctly update the .so files.
                        Log.e(LOG_TAG, "Not up to date library " + libFile.getAbsolutePath());
                        return false;
                    }
                }
                Log.i(LOG_TAG, "Found library " + libFile.getAbsolutePath());
            }
        }
        return true;
    }

    private static void unpackAndLoadLibraries(Context appContext, boolean fromAssets, String[] libraries)
            throws UnsatisfiedLinkError {
        boolean unpacked = false;
        File libRoot = unpackLibVersionedDir(appContext);
        for (String libName : libraries) {
            String libTail = System.mapLibraryName(libName);
            File libFile = new File(libRoot, libTail);
            if (!libFile.exists() && !unpacked) {
                unpackLibraries(appContext, fromAssets, libraries);
                unpacked = true;
            }
            if (libFile.exists()) {
                System.load(libFile.getAbsolutePath());
            } else {
                throw new UnsatisfiedLinkError("Missing library for unpack: " + libName);
            }
        }
    }

    private static void unpackLibraries(Context appContext, boolean fromAssets, String[] libraries)
            throws UnsatisfiedLinkError {
        cleanOldFiles(unpackLibDir(appContext), false);

        ZipFile zipFile = null;
        if (!fromAssets) {
            ApplicationInfo appInfo = appContext.getApplicationInfo();
            try {
                zipFile = new ZipFile(new File(appInfo.sourceDir), ZipFile.OPEN_READ);
            } catch (IOException e) {
                throw new UnsatisfiedLinkError("Error opening APK");
            }
        }

        File unpackTmpRoot = unpackTmpDir(appContext);
        File unpackInstallRoot = unpackLibVersionedDir(appContext);
        unpackTmpRoot.mkdirs();
        unpackInstallRoot.mkdirs();

        boolean success = true;
        for (String libName : libraries) {
            String libTail = System.mapLibraryName(libName);
            File unpackFile = new File(unpackTmpRoot, libTail);
            File installFile = new File(unpackInstallRoot, libTail);

            if (installFile.exists()) {
                // Hopefully there is no chance of a partially installed file that we should overwrite.
                continue;
            }
            Log.d(LOG_TAG, "Unpacking to " + unpackFile.getAbsolutePath() +
                    "; installing to " + installFile.getAbsolutePath());

            unpackFile.delete();
            try {
                if (!unpackFile.createNewFile()) {
                    throw new IOException("Unable to create unpack file.");
                }

                InputStream is = null;
                OutputStream os = null;
                try {
                    if (fromAssets) {
                        try {
                            is = appContext.getAssets().open("lib/" + Build.CPU_ABI + "/" + libTail);
                        } catch (IOException e) {
                            try {
                                is = appContext.getAssets().open("lib/" + Build.CPU_ABI2 + "/" + libTail);
                                Log.w(LOG_TAG, "Falling back from " + Build.CPU_ABI + " to " + Build.CPU_ABI2);
                            } catch (IOException e2) {
                                throw e;
                            }
                        }
                    } else {
                        ZipEntry zipEntry = zipFile.getEntry("lib/" + Build.CPU_ABI + "/" + libTail);
                        if (zipEntry == null) {
                            zipEntry = zipFile.getEntry("lib/" + Build.CPU_ABI2 + "/" + libTail);
                            if (zipEntry != null) {
                                Log.w(LOG_TAG, "Falling back from " + Build.CPU_ABI + " to " + Build.CPU_ABI2);
                            }
                        }
                        if (zipEntry == null) {
                            throw new IOException("APK is missing library: " + libName);
                        }
                        is = zipFile.getInputStream(zipEntry);
                    }

                    os = new FileOutputStream(unpackFile);

                    int count = 0;
                    byte[] buffer = new byte[16 * 1024];
                    while ((count = is.read(buffer)) > -1) {
                        os.write(buffer, 0, count);
                    }
                } finally {
                    if (is != null) {
                        try { is.close(); } catch (IOException e) { /* Do nothing */ }
                    }
                    if (os != null) {
                        try { os.close(); } catch (IOException e) { /* Do nothing */ }
                    }
                }

                // Change permissions to rwxr-xr-x
                unpackFile.setReadable(true, false);
                unpackFile.setExecutable(true, false);
                unpackFile.setWritable(true, true);

                if (!unpackFile.renameTo(installFile)) {
                    if (installFile.exists()) {
                        // This might be a race condition with a background process that is also installing.
                    } else {
                        throw new UnsatisfiedLinkError("Unable to install library: " + libName);
                    }
                }
            } catch (IOException e) {
                throw new UnsatisfiedLinkError(e.getMessage());
            }
        }

        if (zipFile != null) {
            try { zipFile.close(); } catch (IOException e) { /* Do nothing */ }
        }
    }

    private static void cleanOldFiles(File dir, boolean deleteDir) {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory() && file.getName().startsWith(sVersion)) {
                    // One of our unpack directories related to this version.
                    // We might have multiple processes concurrently creating unpack directories.
                    continue;
                }
                if (file.isDirectory()) {
                    cleanOldFiles(file, true);
                } else if (!file.delete()) {
                    Log.w(LOG_TAG, "Failed to remove " + file.getAbsolutePath());
                } else {
                    Log.d(LOG_TAG, "Deleted stale file " + file.getAbsolutePath());
                }
            }
        }
        if (deleteDir && !dir.delete()) {
            Log.w(LOG_TAG, "Failed to remove " + dir.getAbsolutePath());
        }
    }

    private static File unpackLibDir(Context appContext) {
        return appContext.getDir(CUSTOM_LIB_DIR, Context.MODE_PRIVATE);
    }

    private static File unpackLibVersionedDir(Context appContext) {
        File root = unpackLibDir(appContext);
        return new File(root, sVersion);
    }

    private static File unpackTmpDir(Context appContext) {
        File root = unpackLibDir(appContext);
        return new File(root, sVersion + "-" + Integer.toString(Process.myPid()));
    }

}
