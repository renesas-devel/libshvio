LOCAL_PATH:= $(call my-dir)

# shvio-convert
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../include \
	external/libuiomux/include
LOCAL_CFLAGS := -DVERSION=\"1.0.0\"
LOCAL_SRC_FILES := shvio-convert.c
LOCAL_SHARED_LIBRARIES := libshvio libuiomux
LOCAL_MODULE := shvio-convert
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
