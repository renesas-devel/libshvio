LOCAL_PATH:= $(call my-dir)

# shvio-convert
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := external/libshvio/include
LOCAL_CFLAGS := -DVERSION=\"1.0.0\"
LOCAL_SRC_FILES := shvio-convert.c
LOCAL_SHARED_LIBRARIES := libshvio
LOCAL_MODULE := shvio-convert
include $(BUILD_EXECUTABLE)
