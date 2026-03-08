#
# Copyright 2019 Amazon.com, Inc. or its Affiliates. All rights reserved.
#
LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/../../include

ifneq ($(strip $(MTK_PLATFORM)),)
INCLUDES += -I$(strip $(UFBL_HOST_PATH))/app/mt_boot
endif

ifneq (, $(filter ABC ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8135/mtk8135_ramdump.o
endif
ifneq (, $(filter ABC ABC ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8127/mtk8127_ramdump.o
endif
ifneq (, $(filter ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8173/mtk8173_ramdump.o
endif
ifneq (, $(filter ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8173b/mtk8173b_ramdump.o \
    $(LOCAL_DIR)/mtk/8173b/$(UFBL_PROJECT)_memcfg.o
endif
ifneq (, $(filter ABC ABC ABC ABC ABC ABC ABC ABC ABC ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8163/mtk8163_ramdump.o \
    $(LOCAL_DIR)/mtk/8163/$(UFBL_PROJECT)_memcfg.o
endif
ifneq (, $(filter abc123 mt8183_echo, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8183/mtk8183_ramdump.o \
    $(LOCAL_DIR)/mtk/8183/$(UFBL_PROJECT)_memcfg.o
endif

ifneq (, $(filter ABC, $(strip $(UFBL_PROJECT))))
OBJS += \
    $(LOCAL_DIR)/mtk/8168/mtk8168_ramdump.o \
    $(LOCAL_DIR)/mtk/8168/$(UFBL_PROJECT)_memcfg.o
endif

ifneq (, $(filter ABC ABC, $(strip $(UFBL_PROJECT))))
INCLUDES += -I$(LOCAL_DIR)
OBJS += \
    $(LOCAL_DIR)/miniz.o \
    $(LOCAL_DIR)/mtk/8183/ABC_ramdump.o \
    $(LOCAL_DIR)/mtk/8183/ram_compress_ext.o
else
OBJS += \
    $(LOCAL_DIR)/miniz.o \
    $(LOCAL_DIR)/ram_compress.o \
    $(LOCAL_DIR)/ramdump.o

endif
