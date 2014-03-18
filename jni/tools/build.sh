#!/bin/sh

# This directory
APPNAME="$0"
THISDIR=`echo "$APPNAME"|sed 's%/[^/][^/]*$%%'`
if [ "$THISDIR" = "$APPNAME" ]; then
  THISDIR="."
fi
THISDIR=`cd "$THISDIR" && pwd`

JNIDIR="${THISDIR}/.."
TOPDIR="${JNIDIR}/.."

TOPDIR_VISION="${TOPDIR}/plugins/vision"
JNIDIR_VISION="${TOPDIR_VISION}/jni"

SDKDIR="$HOME/data/android/build/android-sdk-macosx"
NDKDIR="$HOME/data/android/build/android-ndk-macosx"

if [ "$ANDROID_HOME" != "" ]; then
  SDKDIR="$ANDROID_HOME"
fi
if [ "$ANDROID_NDK_HOME" != "" ]; then
  NDKDIR="$ANDROID_NDK_HOME"
fi

if [ -d "$NDKDIR" ]; then
  export PATH="$NDKDIR:$PATH"
fi
if [ -d "$SDKDIR" ]; then
  export PATH="$SDKDIR/tools:$PATH"
fi

ADB="$SDKDIR/platform-tools/adb"

if [ "$1" = "clean" ]; then
  rm -rf "${TOPDIR}/obj"
  rm -rf "${TOPDIR}/libs/armeabi"
  rm -rf "${TOPDIR}/libs/armeabi-v7a"
  rm -rf "${TOPDIR}/libs/x86"

  if [ "$2" = "vision" ]; then
    rm -rf "${TOPDIR_VISION}/obj"
    rm -rf "${TOPDIR_VISION}/libs/armeabi"
    rm -rf "${TOPDIR_VISION}/libs/armeabi-v7a"
    rm -rf "${TOPDIR_VISION}/libs/x86"
  fi

  exit
fi

if [ "$1" = "force" ]; then
  rm -f "${JNIDIR}/build.xml"
  "$SDKDIR/tools/android" update project -p "${JNIDIR}" -s || exit 1

  if [ "$2" = "vision" ]; then
    rm -f "${JNIDIR_VISION}/build.xml"
    "$SDKDIR/tools/android" update project -p "${JNIDIR_VISION}" -s || exit 1
  fi
fi

MODE=debug
if [ "$1" = "release" ]; then
  MODE=release
fi

echo "Building in \"$MODE\"mode"

# Options:
BUILDOPTS=""
# -B: force a complete rebuild
# BUILDOPTS="$BUILDOPTS -B"
# Enable verbose mode
BUILDOPTS="$BUILDOPTS V=1 NDK_LOG=1"
# Multi-thread
BUILDOPTS="$BUILDOPTS -j10"

if [ "$MODE" = "debug" ]; then
   BUILDOPTS="$BUILDOPTS NDK_DEBUG=1 APP_OPTIM=debug"
else
   BUILDOPTS="$BUILDOPTS NDK_DEBUG=0 APP_OPTIM=release"
   # Prevent -O2 from APP_CFLAGS to supersede the one in CFLAGS
   BUILDOPTS="$BUILDOPTS APP_CFLAGS="
fi

"$NDKDIR/ndk-build" -C "${JNIDIR}" $BUILDOPTS
if [ "$?" != 0 ]; then
  echo "Build failed"
  exit 1
fi

if [ "$2" = "vision" ]; then
  "$NDKDIR/ndk-build" -C "${JNIDIR_VISION}" $BUILDOPTS
  if [ "$?" != 0 ]; then
    echo "Build failed for vision plugin"
    exit 1
  fi
fi

echo ""
echo "*** Build completed ***"
