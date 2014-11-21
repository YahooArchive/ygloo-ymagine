LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

YMAGINE_DIR:=$(LOCAL_PATH)/../../..

include $(YMAGINE_DIR)/jni/config.mk


LOCAL_MODULE := test-ymagine-unit
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := psnr_html.c ymagine_test.c

LOCAL_CFLAGS += -O3 -Wall -Werror

LOCAL_ARM_MODE := arm
ENABLE_JPEG_FAST := true

LOCAL_CFLAGS += -DJPEGTEST_TURBO=1

LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/include
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/plugins/vision/jni/include/
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/src/include
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/src
LOCAL_C_INCLUDES += $(YOSAL_ROOT)/include
LOCAL_C_INCLUDES += $(JPEGTURBO_ROOT)

LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_main
ifeq ($(YMAGINE_CONFIG_XMP),true)
LOCAL_STATIC_LIBRARIES += libyahoo_expat
endif
LOCAL_STATIC_LIBRARIES += libyahoo_jpegturbo
ifeq ($(YMAGINE_CONFIG_BITMAP_WEBP),true)
LOCAL_STATIC_LIBRARIES += libyahoo_webpdec
LOCAL_STATIC_LIBRARIES += libyahoo_webpenc
endif
LOCAL_STATIC_LIBRARIES += libyahoo_yosal
ifeq ($(YMAGINE_CONFIG_BITMAP_PNG),true)
LOCAL_STATIC_LIBRARIES += libyahoo_png
LOCAL_STATIC_LIBRARIES += libyahoo_zlib
endif

LOCAL_LDLIBS += $(CCV_LDLIBS)

include $(BUILD_EXECUTABLE)
