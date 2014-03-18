LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# List of static libraries to include in the package
LOCAL_STATIC_JAVA_LIBRARIES := com.yahoo.ymagine

# Build all java files in the java subdirectory
LOCAL_SRC_FILES := $(call all-java-files-under, src)

# Name of the APK to build
LOCAL_PACKAGE_NAME := com.yahoo.test.ymagine.imageview

LOCAL_MODULE_TAGS := tests

# Tell it to build an APK
include $(BUILD_PACKAGE)
