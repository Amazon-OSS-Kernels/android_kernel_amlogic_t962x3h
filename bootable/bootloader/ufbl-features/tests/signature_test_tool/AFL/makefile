# Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#Run make by either typing make openssl, or make libtomcrypt
#make help wil provide a help menu
#If the build directory is not set up, then set it up
define MKDIR
	if [ ! -d $(dir $1) ]; \
		then \
			mkdir -p $(dir $1); \
		fi
endef

BUILDDIR := out
TOPDIR          := ./
GET_LOCAL_DIR   = $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
export TOPDIR GET_LOCAL_DIR
LOCAL_PATH := $(GET_LOCAL_DIR)
ROOTDIR := $(patsubst  %/bootable/bootloader/ufbl-features/tests/signature_test_tool/AFL,%, $(abspath $(LOCAL_PATH)))
#Intialize the UFBL makefile variables
UFBL_ENABLE     := yes
UFBL_PROJECT    := signature_tests
UFBL_PATH       := $(ROOTDIR)/bootable/bootloader/ufbl-features
UFBL_LIB_PATH   := ufbl-features/features
export UFBL_ENABLE UFBL_PATH UFBL_LIB_PATH UFBL_PROJECT
TARGET_BUILD_VARIANT := userdebug
AFL_HOME	:= $(ROOTDIR)/fireos/external/AFL
LIBRARY_HOME	:= ../lib

CFLAGS :=
OBJS :=
DEFINES := UFBL_SUPPORTS_WCSTOMBS CONFIG_UFBL_FUZZER

#call the makefile of UFBL tree
ifeq (help,$(MAKECMDGOALS))
else ifeq (libtomcrypt,$(MAKECMDGOALS))
	include src/AFL_signature_rules_ltc.mk
	include $(UFBL_PATH)/build/ufbl.mk
else ifneq (clean,$(MAKECMDGOALS))
	MAKECMDGOALS := all
	include src/AFL_signature_rules_openssl.mk
	include $(UFBL_PATH)/build/ufbl.mk
endif

LN_UFBL_LIBS := $(sort $(UFBL_LIBS))
#This object does not exist yet. The makefile will generate it
OBJS +=
ALLOBJS := $(OBJS)
ALLOBJS := $(addprefix $(BUILDDIR)/,$(ALLOBJS))

#Following for compiler warning for fsync
DEFINES += _POSIX_C_SOURCE=200809L

INCLUDES +=
CFLAGS += $(addprefix -D,$(DEFINES))
CFLAGS += --std=c99 -g
CC = $(AFL_HOME)/afl-gcc

all : $(BUILDDIR)/signature_test_tool
	@echo "Please specify either make openssl or make libtomcrypt"
	@echo "make without any additional arguments will default to openssl"

openssl: $(BUILDDIR)/signature_test_tool

libtomcrypt: $(BUILDDIR)/signature_test_tool

$(AFL_HOME)/afl-gcc:
	@pushd $(LOCAL_PATH) && cd $(AFL_HOME) && make && popd


$(LIBRARY_HOME)/libtomcrypt.a:
	@. ../lib/make_libtomcrypt.sh

$(LIBRARY_HOME)/libtommath.a:
	@. ../lib/make_libtommath.sh

$(BUILDDIR)/signature_test_tool: $(ALLOBJS)
	@$(call MKDIR,$@)
	@$(CC) $(ALLOBJS) -o $@ $(LIBRARIES)

$(BUILDDIR)/%.o: %.c $(AFL_HOME)/afl-gcc $(LIBRARY_HOME)/libtomcrypt.a $(LIBRARY_HOME)/libtommath.a
	@$(call MKDIR,$@)
	@echo compiling $<
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(LIBRARIES)

clean:
	@rm -rf $(BUILDDIR)
