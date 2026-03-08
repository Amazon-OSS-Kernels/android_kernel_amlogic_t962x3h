# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.

MKDIR = if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
BUILDDIR := out
TOPDIR          := ./
GET_LOCAL_DIR   = $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
export TOPDIR GET_LOCAL_DIR

INCLUDES :=
CFLAGS :=
OBJS :=
DEFINES :=

LOCAL_PATH := $(GET_LOCAL_DIR)
ROOTDIR :=$(patsubst  %/bootable/bootloader/ufbl-features/tests/bcb_test_tool,%, $(abspath $(LOCAL_PATH)))

#Intialize the UFBL makefile variables
UFBL_ENABLE     := yes
UFBL_PATH       := ../../../ufbl-features
UFBL_LIB_PATH   := ufbl-features/features
UFBL_PROJECT    := bcb_tests
export UFBL_ENABLE UFBL_PATH UFBL_LIB_PATH UFBL_PROJECT
TARGET_BUILD_VARIANT := userdebug

#call the makefile of UFBL tree
include $(UFBL_PATH)/build/ufbl.mk
#call the local source tree makefile
include src/bcb_test_rules.mk

LN_UFBL_LIBS := $(sort $(UFBL_LIBS))
ALLOBJS := $(OBJS)
ALLOBJS := $(addprefix $(BUILDDIR)/,$(ALLOBJS))

INCLUDES += -Iinclude -I../../features/bcb
CFLAGS += $(addprefix -D,$(DEFINES)) -g
CFLAGS += --std=c99 -Werror
CC = gcc
LD = ld

$(BUILDDIR)/bcb_test_tool: $(ALLOBJS)
	@$(MKDIR)
	$(CC) $(ALLOBJS)  -o $@

$(BUILDDIR)/%.o: %.c
	@$(MKDIR)
	@echo compiling $<
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)
