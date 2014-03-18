# Build for all ABI, and let package manager install the correct one
APP_ABI := armeabi armeabi-v7a x86
# Or, in development mode, pick only one target to accelerate build
# APP_ABI := armeabi-v7a

APP_PLATFORM := android-9

# Our native guidelines request to try hard to not rely on STL.
# If need one, choose
# here with implementation to use.
# static STLport library
#APP_STL := stlport_static
# shared STLport library
#APP_STL := stlport_shared
# default C++ runtime library
#APP_STL := system
# GNU libstdc++ as a static library
#APP_STL := gnustl_static
