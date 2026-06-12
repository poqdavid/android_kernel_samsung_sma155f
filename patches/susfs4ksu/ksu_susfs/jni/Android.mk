LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := ksu_susfs
LOCAL_SRC_FILES := main.c
LOCAL_CFLAGS    := -O3 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC -flto=thin -ffunction-sections -fdata-sections -fvisibility=hidden
LOCAL_LDFLAGS   := -Wl,--gc-sections -flto=thin
LOCAL_STRIP     := true

include $(BUILD_EXECUTABLE)
