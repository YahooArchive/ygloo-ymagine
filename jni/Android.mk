LOCAL_PATH:=$(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../build/config/config.mk
include $(YMAGINE_ROOT)/jni/config.mk

# Set to true to build a debuggable shared library
YMAGINE_DEBUG_BUILD:=false
# Set to true to enable verbose logs
YMAGINE_DEBUG_LOG:=false

# Detect debug build in NDK
ifneq ($(NDK_DEBUG),)
ifneq ($(NDK_DEBUG),0)
YMAGINE_DEBUG_BUILD:=true
YMAGINE_DEBUG_LOG:=true
endif
endif

# Detect debug build in NativeSDK build system
ifneq ($(BUILD_DEBUG),)
ifneq ($(BUILD_DEBUG),false)
ifneq ($(BUILD_DEBUG),0)
YMAGINE_DEBUG_BUILD:=true
YMAGINE_DEBUG_LOG:=true
endif
endif
endif

ifneq ($(NDK_ROOT),)
JPEGTURBO_BUILD_TEST:=false
# Native SDK dependencies
include $(YOSAL_ROOT)/Android.mk
##### Build 3rdparties static libraries
include $(ZLIB_ROOT)/Android.mk
include $(LZF_ROOT)/Android.mk
include $(JPEGTURBO_ROOT)/Android.mk
include $(PNG_ROOT)/Android.mk
include $(WEBP_ROOT)/Android.mk
include $(EXPAT_ROOT)/Android.mk
endif

# Set to true to enable support for Perl
YMAGINE_PERL:=false
ifeq ($(TARGET_OS),darwin)
YMAGINE_PERL:=true
endif
ifeq ($(TARGET_OS),linux)
YMAGINE_PERL:=true
endif
ifeq ($(YMAGINE_PERL),true)
PERLINCDIR:=$(shell perl -MConfig -e 'print $$Config{archlib}' 2> /dev/null || :)
ifeq (,$(wildcard $(PERLINCDIR)))
YMAGINE_PERL:=false
endif
endif

YMAGINE_MAIN_C_INCLUDES :=
YMAGINE_MAIN_SRC_FILES :=
YMAGINE_MAIN_CFLAGS :=
YMAGINE_MAIN_LDFLAGS :=

ifeq ($(YMAGINE_DEBUG_LOG),true)
YMAGINE_MAIN_CFLAGS += -DYMAGINE_DEBUG=1
endif

YMAGINE_MAIN_CFLAGS += -Wall -Werror

# Dependency to core layer
YMAGINE_MAIN_C_INCLUDES += $(YOSAL_ROOT)/include

# Public API
YMAGINE_MAIN_C_INCLUDES += $(YMAGINE_ROOT)/jni/include
# Private API
YMAGINE_MAIN_C_INCLUDES += $(YMAGINE_ROOT)/jni/src/include
YMAGINE_MAIN_C_INCLUDES += $(YMAGINE_ROOT)/jni/src

YMAGINE_MAIN_SRC_FILES += src/compat/androidndk.c
YMAGINE_MAIN_SRC_FILES += src/graphics/vbitmap.c

ifeq ($(YMAGINE_CONFIG_BITMAP),true)
YMAGINE_MAIN_CFLAGS += -DHAVE_BITMAPFACTORY=1
YMAGINE_MAIN_SRC_FILES += src/formats/format.c
YMAGINE_MAIN_SRC_FILES += src/formats/vformat.c
ifeq ($(YMAGINE_CONFIG_BITMAP_JPEG),true)
YMAGINE_MAIN_SRC_FILES += src/formats/jpeg/jpegio.c
YMAGINE_MAIN_SRC_FILES += src/formats/jpeg/jpeg.c
YMAGINE_MAIN_SRC_FILES += src/formats/jpeg/exif.c
YMAGINE_MAIN_C_INCLUDES += $(JPEGTURBO_ROOT)
endif
ifeq ($(YMAGINE_CONFIG_XMP),true)
YMAGINE_MAIN_SRC_FILES += src/graphics/xmp.c
YMAGINE_MAIN_C_INCLUDES += $(EXPAT_ROOT)/lib
YMAGINE_MAIN_CFLAGS += -DHAVE_JPEG_XMP=1
endif
YMAGINE_MAIN_SRC_FILES += src/formats/webp/webp.c
ifeq ($(YMAGINE_CONFIG_BITMAP_WEBP),true)
YMAGINE_MAIN_CFLAGS += -DHAVE_WEBP=1
YMAGINE_MAIN_C_INCLUDES += $(WEBP_ROOT)/include
YMAGINE_MAIN_C_INCLUDES += $(WEBP_ROOT)/src
YMAGINE_MAIN_STATIC_LIBRARIES += libyahoo_webpdec
YMAGINE_MAIN_STATIC_LIBRARIES += libyahoo_webpenc
endif
YMAGINE_MAIN_SRC_FILES += src/formats/png/png.c
ifeq ($(YMAGINE_CONFIG_BITMAP_PNG),true)
YMAGINE_MAIN_CFLAGS += -DHAVE_PNG=1
YMAGINE_MAIN_C_INCLUDES += $(PNG_ROOT)
YMAGINE_MAIN_STATIC_LIBRARIES += libyahoo_png
endif
YMAGINE_MAIN_SRC_FILES += src/formats/gif/gif.c
ifeq ($(YMAGINE_CONFIG_BITMAP_GIF),true)
YMAGINE_MAIN_CFLAGS += -DHAVE_GIF=1
endif

YMAGINE_MAIN_SRC_FILES += src/graphics/bitmap.c
YMAGINE_MAIN_SRC_FILES += src/graphics/transformer.c
YMAGINE_MAIN_SRC_FILES += src/graphics/quantize.c
YMAGINE_MAIN_SRC_FILES += src/graphics/color.c
YMAGINE_MAIN_SRC_FILES += src/graphics/region.c

YMAGINE_MAIN_SRC_FILES += src/filters/blursuperfast.c
YMAGINE_MAIN_SRC_FILES += src/filters/blur.c
YMAGINE_MAIN_SRC_FILES += src/filters/compose.c
YMAGINE_MAIN_SRC_FILES += src/filters/sobel.c
YMAGINE_MAIN_SRC_FILES += src/filters/seam.c

YMAGINE_MAIN_SRC_FILES += src/filters/colorize.c
YMAGINE_MAIN_SRC_FILES += src/filters/rotate.c

YMAGINE_MAIN_SRC_FILES += src/shaders/coloreffect.c
YMAGINE_MAIN_SRC_FILES += src/shaders/pixelshader.c

YMAGINE_MAIN_SRC_FILES += src/java/bitmapapi.c
YMAGINE_MAIN_SRC_FILES += src/java/ymagineapi.c

YMAGINE_MAIN_SRC_FILES += src/design/orb.c

YMAGINE_MAIN_SRC_FILES += src/simple/simple.c
endif

LOCAL_PATH:=$(YMAGINE_ROOT)/jni
include $(CLEAR_VARS)

LOCAL_MODULE := libyahoo_ymagine_main
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := $(YMAGINE_MAIN_C_INCLUDES)
LOCAL_SRC_FILES := $(YMAGINE_MAIN_SRC_FILES)
LOCAL_CFLAGS := $(YMAGINE_MAIN_CFLAGS)
LOCAL_LDFLAGS := $(YMAGINE_MAIN_LDFLAGS)

# Force ARM32 mode
LOCAL_ARM_MODE := arm
ifeq ($(YMAGINE_DEBUG_BUILD),true)
LOCAL_CFLAGS += -DDEBUG -UNDEBUG -O0 -g
else
LOCAL_CFLAGS += -O3
LOCAL_CFLAGS += -fstrict-aliasing
ifneq ($(TARGET_COMPILER),clang)
# -fprefetch-loop-arrays is a GCC specific option, not supported
# by clang, and not all architectures. Automatically enabled if
# optimization level O2 or higher so not needed here with O3
# LOCAL_CFLAGS += -fprefetch-loop-arrays
endif
LOCAL_CFLAGS += -fstrict-aliasing
endif

# If static library has to be linked inside a larger shared library later,
# all code has to be compiled as PIC (Position Independant Code)
LOCAL_CFLAGS += -fPIC -DPIC

include $(BUILD_STATIC_LIBRARY)

#### Build shared library
ifeq ($(YMAGINE_BUILD_SHARED),true)

LOCAL_PATH:=$(YMAGINE_ROOT)/jni
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(YMAGINE_MAIN_C_INCLUDES)
LOCAL_SRC_FILES := $(YMAGINE_MAIN_SRC_FILES)
LOCAL_CFLAGS := $(YMAGINE_MAIN_CFLAGS)
LOCAL_LDFLAGS := $(YMAGINE_MAIN_LDFLAGS)
LOCAL_STATIC_LIBRARIES := $(YMAGINE_STATIC_LIBRARIES)
LOCAL_SHARED_LIBRARIES := $(YMAGINE_SHARED_LIBRARIES)

# Ensure local symbols of dependencies are used, and not
# global symbols from other shared libraries loaded in the process
# (e.g. libjpeg.so, etc.)
ifeq (linux,$(TARGET_OS))
LOCAL_LDFLAGS += -Wl,-Bsymbolic
endif
ifeq (linux,$(TARGET_OS))
# LOCAL_LDFLAGS += -Wl,--no-allow-shlib-undefined
endif

# If static library has to be linked inside a larger shared library later,
# all code has to be compiled as PIC (Position Independant Code)
LOCAL_CFLAGS += -fPIC -DPIC

# Main JNI entry point
LOCAL_SRC_FILES += src/java/jniload.cpp

ifeq ($(YMAGINE_BUILD_ANDROID),true)
LOCAL_CFLAGS += -DYMAGINE_JNI_BITMAPFACTORY=1
endif
# Enable generic Ymagine API on both Android and generic Java
LOCAL_CFLAGS += -DYMAGINE_JNI_GENERIC=1

# Force ARM32 mode
LOCAL_ARM_MODE := arm
ifeq ($(YMAGINE_DEBUG_BUILD),true)
LOCAL_CFLAGS += -DDEBUG -UNDEBUG -O0 -g
else
LOCAL_CFLAGS += -Os
LOCAL_CFLAGS += -fstrict-aliasing
endif

ifeq ($(YMAGINE_BUILD_ANDROID),true)
LOCAL_LDLIBS += -llog
# Requires Android 2.2 (API level 8, Froyo)
LOCAL_LDLIBS += -ljnigraphics
# Requires Android 2.3 (API level 9, Gingerbread)
LOCAL_LDLIBS += -landroid

# LOCAL_STATIC_LIBRARIES += cpufeatures
ifeq ($(NDK_ROOT),)
LOCAL_SHARED_LIBRARIES  += libcutils libutils
endif
endif

ifeq ($(TARGET_OS),darwin)
LOCAL_LDLIBS += -framework JavaVM
endif

LOCAL_MODULE := libyahoo_ymagine
LOCAL_MODULE_TAGS := optional

# If building with AOSP tree before ICS (NoOp for NDK build)
LOCAL_PRELINK_MODULE:=false

include $(BUILD_SHARED_LIBRARY)

# $(call import-module,android/cpufeatures)

# Build Perl XS enabled version of shared library
#### Build shared library
ifeq ($(YMAGINE_PERL),true)

LOCAL_PATH:=$(YMAGINE_ROOT)/jni
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(YMAGINE_MAIN_C_INCLUDES)
LOCAL_SRC_FILES := $(YMAGINE_MAIN_SRC_FILES)
LOCAL_CFLAGS := $(YMAGINE_MAIN_CFLAGS)
LOCAL_LDFLAGS := $(YMAGINE_MAIN_LDFLAGS)
LOCAL_STATIC_LIBRARIES := $(YMAGINE_STATIC_LIBRARIES)
LOCAL_SHARED_LIBRARIES := $(YMAGINE_SHARED_LIBRARIES)

# If static library has to be linked inside a larger shared library later,
# all code has to be compiled as PIC (Position Independant Code)
LOCAL_CFLAGS += -fPIC -DPIC

LOCAL_SRC_FILES += src/perl/xs_ymagine.c

LOCAL_CFLAGS += -DHAVE_YMAGINE_PERL_XS=1
LOCAL_CFLAGS += -I$(PERLINCDIR)/CORE

# Force ARM32 mode
LOCAL_ARM_MODE := arm
ifeq ($(YMAGINE_DEBUG_BUILD),true)
LOCAL_CFLAGS += -DDEBUG -UNDEBUG -O0 -g
else
LOCAL_CFLAGS += -Os
LOCAL_CFLAGS += -fstrict-aliasing
endif

# Either link the XS module explicitely with libperl
# LOCAL_LDLIBS += -L $(PERLINCDIR)/CORE -lperl
ifeq ($(TARGET_OS),darwin)
# On MacOSX, ask the linker to ignore undefined symbols and resolve them at runtime
LOCAL_LDLIBS += -Bsymbolic -Wl,-undefined,dynamic_lookup
endif
ifeq ($(TARGET_OS),linux)
# LOCAL_LDLIBS += -Bsymbolic -Wl,-z,nodefs
LOCAL_LDLIBS += -L $(PERLINCDIR)/CORE -Wl,-rpath,$(PERLINCDIR)/CORE -Wl,-rpath-link,$(PERLINCDIR)/CORE -lperl
endif

LOCAL_MODULE := libyahoo_ymaginexs
LOCAL_MODULE_TAGS := optional

# If building with AOSP tree before ICS (NoOp for NDK build)
LOCAL_PRELINK_MODULE:=false

include $(BUILD_SHARED_LIBRARY)

# $(call import-module,android/cpufeatures)
endif

endif
