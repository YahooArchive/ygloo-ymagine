LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

YMAGINE_DIR:=$(LOCAL_PATH)/../../..

include $(YMAGINE_DIR)/jni/config.mk

LOCAL_MODULE := ymagine
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := utils.c
LOCAL_SRC_FILES += main_info.c
LOCAL_SRC_FILES += main_decode.c
LOCAL_SRC_FILES += main_tile.c
LOCAL_SRC_FILES += main_design.c
LOCAL_SRC_FILES += main_transcode.c
ifeq ($(YMAGINE_CONFIG_VIDEO),true)
LOCAL_SRC_FILES += main_video.cpp
endif
LOCAL_SRC_FILES += main_shape.c
LOCAL_SRC_FILES += main_psnr.c
LOCAL_SRC_FILES += main_blur.c
LOCAL_SRC_FILES += main_convolution.c
LOCAL_SRC_FILES += ymagine.c

LOCAL_CFLAGS += -Wall -Werror
ifeq ($(BUILD_JAVASCRIPT),true)
LOCAL_CFLAGS += -O2
LOCAL_CFLAGS += -g0
LOCAL_CFLAGS += -DYMAGINE_JAVASCRIPT=1
else
LOCAL_CFLAGS += -O3
endif

LOCAL_ARM_MODE := arm
ENABLE_JPEG_FAST := true

LOCAL_CFLAGS += -DJPEGTEST_TURBO=1

LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/include
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/plugins/vision/jni/include/
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/plugins/video/jni/include/
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/src/include
LOCAL_C_INCLUDES += $(YMAGINE_DIR)/jni/src
LOCAL_C_INCLUDES += $(YOSAL_ROOT)/include
LOCAL_C_INCLUDES += $(JPEGTURBO_ROOT)

LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_main
ifeq ($(YMAGINE_CONFIG_VIDEO),true)
LOCAL_CFLAGS += -DHAVE_PLUGIN_VIDEO=1
LOCAL_C_INCLUDES += $(VPX_ROOT)/lbvpx/vpx
LOCAL_C_INCLUDES += $(WEBM_ROOT)
LOCAL_C_INCLUDES += $(WEBMTOOLS_ROOT)/shared

LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_video_main
LOCAL_STATIC_LIBRARIES += libyahoo_vpx
LOCAL_STATIC_LIBRARIES += libyahoo_webm
endif
ifeq ($(YMAGINE_CONFIG_CLASSIFIER),true)
LOCAL_CFLAGS += -DHAVE_CCV -DHAVE_PLUGIN_VISION=1
LOCAL_C_INCLUDES += $(CCV_ROOT)/lib
LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_vision_main
LOCAL_STATIC_LIBRARIES += libyahoo_ymagine_vision_arch
endif
ifeq ($(YMAGINE_CONFIG_XMP),true)
LOCAL_STATIC_LIBRARIES += libyahoo_expat
endif
ifeq ($(YMAGINE_CONFIG_DLIB),true)
LOCAL_CFLAGS += -DHAVE_DLIB=1
LOCAL_C_INCLUDES += $(DLIB_ROOT)
LOCAL_STATIC_LIBRARIES += libyahoo_dlib
endif
ifeq ($(YMAGINE_CONFIG_VISION),true)
LOCAL_STATIC_LIBRARIES += libyahoo_ccv
LOCAL_STATIC_LIBRARIES += libyahoo_ccv_sqlite
LOCAL_STATIC_LIBRARIES += libyahoo_ccv_compat
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

# LOCAL_SHARED_LIBRARIES += libc libutils libcutils
LOCAL_LDLIBS += $(CCV_LDLIBS)

# Enable global optimization (especially with Emscripten)
ifeq ($(BUILD_JAVASCRIPT),true)
LOCAL_LDFLAGS += -O2
# Drop debug support
LOCAL_LDFLAGS += -g0
# Compiles the source code using the Embind bindings to connect C/C++ and JavaScript
LOCAL_CFLAGS += --bind
# Keep mem file inlined
LOCAL_LDFLAGS += --memory-init-file 0
LOCAL_LDFLAGS += -s EXPORTED_FUNCTIONS="['_main', '_main_ymagine_js']"

# LOCAL_CFLAGS += -s FULL_ES2=1 -s EXPORTED_FUNCTIONS=@$(EXPORTS_FILE) -std=c++11

ifeq (1,1)
LOCAL_CFLAGS += -DYMAGINE_JAVASCRIPT_CLOSURE=1
LOCAL_LDFLAGS += --closure 1
# LOCAL_LDFLAGS += --pre-js $(LOCAL_PATH)/init.js
# LOCAL_LDFLAGS += --embed-file $(LOCAL_PATH)/data@data
# LOCAL_LDFLAGS += --preload-file $(LOCAL_PATH)/data@data
else
LOCAL_CFLAGS += -DYMAGINE_JAVASCRIPT_CLOSURE=0
LOCAL_LDFLAGS += --closure 0
endif
else
LOCAL_LDFLAGS += -O3
endif

include $(BUILD_EXECUTABLE)
