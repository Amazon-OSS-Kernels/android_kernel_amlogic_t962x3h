/*
 * Copyright (c) 2022 Amazon.com, Inc. or its affiliates.  All rights reserved.
 */

#ifndef _AMZN_TEMP_UNLOCK_AML_IMPL_H_
#define _AMZN_TEMP_UNLOCK_AML_IMPL_H_

// This head file must align with optee-os's amzn_temp_unlock.h

// OPTEE_SUPPORT_TEMP_UNLOCK_MAX_REBOOT_CNT is max support count from optee-os,
// the real temp unlock count is defined by CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT
// in UFBL project makefile
#define OPTEE_SUPPORT_TEMP_UNLOCK_MAX_REBOOT_CNT    15

#ifndef CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT
#error "CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT must be defined in UFBL"
#endif

#if (CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT > OPTEE_SUPPORT_TEMP_UNLOCK_MAX_REBOOT_CNT)
#error "CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT is greater than OPTEE_SUPPORT_TEMP_UNLOCK_MAX_REBOOT_CNT"
#endif

#define AMZN_TEMP_UNLOCK_BOOT_TAG_MAGIC        0x42545455
#define AMZN_TEMP_UNLOCK_HMAC_HASH_SIZE        32
#define AMZN_TEMP_UNLOCK_HMAC_BUFFER_SIZE      (OPTEE_SUPPORT_TEMP_UNLOCK_MAX_REBOOT_CNT * AMZN_TEMP_UNLOCK_HMAC_HASH_SIZE)

#define TA_TEMP_UNLOCK_UUID \
	{ 0x25205219, 0x9f7c, 0x11ec, \
	{ 0x87, 0x4b, 0xac, 0xde, 0x48, 0x00, 0x11, 0x22 } }
#define CMD_GET_TEMP_UNLOCK_DATA               1

struct boot_tag_temp_unlock {
    uint32_t magic;
    uint32_t temp_unlock_reboot_cnt;
    uint8_t temp_unlock_hmac[AMZN_TEMP_UNLOCK_HMAC_BUFFER_SIZE];
} __attribute__ ((packed));

#endif