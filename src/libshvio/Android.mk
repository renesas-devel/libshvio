LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
	external/libshvio/include \

#LOCAL_CFLAGS := -DDEBUG

LOCAL_SRC_FILES := \
	veu.c

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_MODULE := libshvio
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
