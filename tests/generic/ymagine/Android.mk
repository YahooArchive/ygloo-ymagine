LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

YMAGINE_DIR:=$(LOCAL_PATH)/../../..

include $(YMAGINE_DIR)/jni/config.mk

LOCAL_MODULE := test-ymagine
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := ymagine.c

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
ifeq ($(YMAGINE_CONFIG_VPX),true)
LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_video_main
LOCAL_STATIC_LIBRARIES += libvpx
endif
ifeq ($(YMAGINE_CONFIG_CLASSIFIER),true)
LOCAL_CFLAGS += -DHAVE_CCV -DHAVE_PLUGIN_VISION=1
LOCAL_C_INCLUDES += $(CCV_ROOT)/lib
LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_vision_main
LOCAL_STATIC_LIBRARIES += libyahoo_ccv
endif
ifeq ($(YMAGINE_CONFIG_XMP),true)
LOCAL_STATIC_LIBRARIES += libyahoo_expat
endif
LOCAL_STATIC_LIBRARIES += libyahoo_jpegturbo
LOCAL_STATIC_LIBRARIES += libyahoo_webpdec
LOCAL_STATIC_LIBRARIES += libyahoo_yosal

# LOCAL_SHARED_LIBRARIES += libc libutils libcutils

include $(BUILD_EXECUTABLE)
