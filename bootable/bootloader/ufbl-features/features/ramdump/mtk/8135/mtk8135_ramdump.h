/*
 * mtk8135_ramdump.h
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef __MTK_RAMDUMP_H__
#define __MTK_RAMDUMP_H__

#define MDUMP_MODULE	"LK"
#define MDUMP_STORE_BLOCK	0x0100000

#define MDUMP_COLD_RESET	0x00
#define MDUMP_FORCE_RESET	0x1E
#define MDUMP_REBOOT_WATCHDOG	0x2D
#define MDUMP_REBOOT_PANIC	0x3C
#define MDUMP_REBOOT_NORMAL	0xC3
#define MDUMP_REBOOT_HARDWARE	0xE1

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define MDUMP_BUFFER_SIG	0x4842444dU	/* MDBH */

#ifndef CONFIG_MDUMP_MESSAGE_SIZE
#define CONFIG_MDUMP_MESSAGE_SIZE	65536
#endif

#ifndef CONFIG_MDUMP_BUFFER_ADDRESS
#define CONFIG_MDUMP_BUFFER_ADDRESS	0x81FC0000
#endif

#ifndef COMPRESS_SCRATCH_ADDRESS
#define COMPRESS_SCRATCH_ADDRESS        0x81F00000      // for compression data structure (size: 0x4df40)
#define COMPRESS_SCRATCH_SIZE           0x60000         // 384K total space for inflate data structure
#define COMPRESS_FIRST_BUFFER           0x81F60000      // for holding initial compressed content
#define COMPRESS_FIRST_BUFFER_SIZE      0x60000         // 384K buffer
#define COMPRESS_START_ADDRESS          0x90000000      // start address for storing compressed content
#endif

struct mdump_buffer {
	u32   signature;
	u8  reboot_reason;
	u8  backup_reason;
	u16 enable_flags;
	char           stage1_messages[CONFIG_MDUMP_MESSAGE_SIZE - 12];
	char           zero_pad1[4];
	char           stage2_messages[CONFIG_MDUMP_MESSAGE_SIZE - 4];
	char           zero_pad2[4];
};

extern void mdump_mark_reboot_reason(int);

#define MDUMP_HEADER_SIG 0x524448504d55444dULL	/* MDUMPHDR */

/* This block size will support all(maybe?) type block devices */
#define MDUMP_BLOCK_SIZE	4096

#define MDUMP_BANK_MAXIMUM	\
	((MDUMP_BLOCK_SIZE - 16) / sizeof(struct mdump_bank))

struct mdump_bank {
	char         name[8];
	unsigned int size; /* bank size */
	unsigned int address;
} __attribute__ ((packed));

struct mdump_header {
	unsigned long long signature;
	unsigned char      reboot_reason;
	unsigned char      bank_count;
	unsigned short     block_size;
	unsigned int       crc32;
	struct mdump_bank  entries[MDUMP_BANK_MAXIMUM];
} __attribute__ ((packed));

#endif /* __RAMDUMP_H__ */

