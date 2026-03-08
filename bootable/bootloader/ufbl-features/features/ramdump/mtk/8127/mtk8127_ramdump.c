/*
 * mtk8127_ramdump.c
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <compiler.h>
#include <debug.h>
#include <string.h>
#include <arch.h>
#include <platform.h>
#include <target.h>
#include <stdlib.h>
#include <string.h>
#include <arch/ops.h>

#include <mt_partition.h>
#include <platform/mtk_wdt.h>
#include <platform/mt_logo.h>
#include <platform/boot_mode.h>
#include "mmc.h"
#include "partition_parser.h"
#include "../../ram_compress.h"
#include "ramdump.h"
#include "mtk8127_ramdump.h"

static unsigned long s_dump_process = 0;
static unsigned long s_dump_total = 0;
static unsigned char s_data_buf[4096];

static const struct mdump_bank s_mdump_banks[] = {
#if 0
	{ "ISRAM",  0x00010000, 0x00100000 },
	{ "SYSRAM", 0x00040000, 0x12000000 },
#endif
	{ "DRAM",   0x37C00000, 0x80000000 },
	{ "",     0x00000000, 0x00000000 }
};

extern BOOTMODE g_boot_mode;

/* module depend code */

static void* get_mdump_device(unsigned long long* poff, unsigned int* part_id)
{
	int idx;
	part_dev_t* dev;

	dev = mt_part_get_device();
	if (dev == NULL) {
		dprintf(CRITICAL, "MDump can't found mdump device.\n");
		return NULL;
	}
	idx = partition_get_index("MDUMP");
	if (idx < 0) {
		dprintf(CRITICAL, "MDump can't found partition.\n");
		return NULL;
	}
	*part_id = partition_get_region(idx);
	dprintf(CRITICAL, "Here mdump partition part_id=%d\n", *part_id);
	*poff = partition_get_offset(idx);
	dprintf(CRITICAL, "Here mdump partition offset=0x%llx\n", *poff);
	return dev;
}

static void store_block(void* dev, unsigned long long offset, const void* data, unsigned int len, unsigned int part_id)
{
	part_dev_t* pdev = (part_dev_t*)dev;
	pdev->write(pdev, (uchar*)data, offset, len, part_id);
	s_dump_process += len;
	// mt_disp_show_memory_dump_process(s_dump_process, s_dump_total);
}

extern void mtk_wdt_restart(void);

void watchdog_ping(void)
{
	mtk_wdt_restart();
}

static void flush_mdump_buffer(struct mdump_buffer * buf)
{
	arch_clean_invalidate_cache_range((addr_t)buf, sizeof(struct mdump_buffer));
}

/* module independ code */
static void store_header(void* dev, unsigned long long offset, unsigned char reason, const struct mdump_bank* banks, unsigned int part_id)
{
	unsigned int idx;
	struct mdump_header* hdr = (struct mdump_header*) s_data_buf;

	memset(hdr, 0, MDUMP_BLOCK_SIZE);
	hdr->signature = MDUMP_HEADER_SIG;
	hdr->reboot_reason = reason;
	hdr->bank_count = 0;
	hdr->block_size = MDUMP_BLOCK_SIZE;
	for(idx=0; idx<MDUMP_BANK_MAXIMUM; ++idx) {
		if (banks[idx].size == 0) {
			break;
		}
		hdr->entries[idx] = banks[idx];
		++(hdr->bank_count);
	}

	if(dev) {
		// if provided eMMC device handle, then write the block to eMMC
		store_block(dev, offset, s_data_buf, MDUMP_BLOCK_SIZE, part_id);
	}
}

static void store_memory_bank(void* dev, unsigned long long offset, const struct mdump_bank* bank, unsigned int part_id)
{
	unsigned char* mptr = (unsigned char*) (bank->address);
	unsigned int size = bank->size;
	do {
		unsigned int len = (size < MDUMP_STORE_BLOCK) ? size : MDUMP_STORE_BLOCK;
		watchdog_ping();
		dprintf(INFO, "MDump write %s context 0x%x\n", bank->name, (unsigned int)mptr);
		store_block(dev, offset, mptr, len, part_id);
		// mt_disp_show_memory_dump_process(s_dump_process, s_dump_total);
		mptr += len;
		size -= len;
		offset += len;
	} while (size != 0);
}


extern BOOT_ARGUMENT *g_boot_arg;


int check_ramdump(void)
{
	struct mdump_buffer *buf = (struct mdump_buffer *)CONFIG_MDUMP_BUFFER_ADDRESS;

	dprintf(INFO, "[%s]Check mdump buffer on addresss 0x%x.\n", MDUMP_MODULE, CONFIG_MDUMP_BUFFER_ADDRESS);
	if (buf->signature != MDUMP_BUFFER_SIG) {
		//we should trigger a crash here.
		return 0;
	}

	// MTK said DDR memory may not be cleared when quickly power on
	// from power cut.
	// Check boot reason again here to have second chance to initialize
	// MDump.
	// TODO: need to make clear whether it is a by-design behavior.
	if (g_boot_arg->boot_reason == BR_POWER_KEY ||
			g_boot_arg->boot_reason == BR_USB) {
		buf->reboot_reason = MDUMP_COLD_RESET;
		buf->backup_reason = MDUMP_COLD_RESET;
		buf->enable_flags = 0;
		memset(buf->zero_pad1, 0, sizeof(buf->zero_pad1));
		memset(buf->zero_pad2, 0, sizeof(buf->zero_pad2));
		if (g_boot_arg->boot_reason == BR_POWER_KEY)
			dprintf(INFO, "Skip MDump due to Cold reset.\n");
		else
			dprintf(INFO, "Skip MDump due to USB/AC boot after Power off.\n");
		flush_mdump_buffer(buf);
		return 0;
	}


	dprintf(INFO, "[%s]MDump reboot reason: 0x%x\n", MDUMP_MODULE, buf->reboot_reason);
	buf->backup_reason = buf->reboot_reason;
	buf->reboot_reason = MDUMP_COLD_RESET;
	flush_mdump_buffer(buf);

	switch (buf->backup_reason) {
	case MDUMP_COLD_RESET:
		dprintf(INFO, "[%s]Cold reset.\n", MDUMP_MODULE);
		return 0;
	case MDUMP_REBOOT_NORMAL:
		dprintf(INFO, "[%s]Software warm reboot.\n", MDUMP_MODULE);
		return 0;
	case MDUMP_REBOOT_WATCHDOG:
		dprintf(INFO, "[%s]Watchdog reboot.\n", MDUMP_MODULE);
		break;
	case MDUMP_REBOOT_PANIC:
		dprintf(INFO, "[%s]Software panic reboot.\n", MDUMP_MODULE);
		break;
	case MDUMP_REBOOT_HARDWARE:
		dprintf(INFO, "[%s]Hardware reason reboot.\n", MDUMP_MODULE);
		break;
	default:
		dprintf(INFO, "[%s]Unknown reboot reason.\n", MDUMP_MODULE);
		break;
	}
	if (buf->enable_flags == 0) {
		dprintf(INFO, "[%s]MDump feature is disabled.\n", MDUMP_MODULE);
		return 0;
	}
	return buf->backup_reason;
}

void ramdump_init(void)
{
	struct mdump_buffer *buf = (struct mdump_buffer *)CONFIG_MDUMP_BUFFER_ADDRESS;

	if (buf->signature != MDUMP_BUFFER_SIG) {
		// Skip print message due to log system is not available at this point.
		// Couldn't ckeck BR_POWER_KEY here, since g_boot_arg->boot_reason
		// has not been initialized at this point.
		buf->signature = MDUMP_BUFFER_SIG;
		buf->reboot_reason = MDUMP_COLD_RESET;
		buf->backup_reason = MDUMP_COLD_RESET;
		buf->enable_flags = 0;
		memset(buf->zero_pad1, 0, sizeof(buf->zero_pad1));
		memset(buf->zero_pad2, 0, sizeof(buf->zero_pad2));
	}
}

int ramdump_to_eMMC(void)
{
	int idx = 0;
	unsigned char reason = 0;
	void* dev = NULL;
	unsigned long long offset = 0;
	unsigned int part_id = 0;

	reason = check_ramdump();
	if (reason == 0) {
		return 0;
	}

	dev = get_mdump_device(&offset, &part_id);
	if (dev == NULL) {
		return 0;
	}

	s_dump_total = MDUMP_BLOCK_SIZE;
	for(idx=0; s_mdump_banks[idx].size != 0; ++idx) {
		unsigned int size = s_mdump_banks[idx].size;
		s_dump_total += (size + MDUMP_BLOCK_SIZE - 1) & ~(MDUMP_BLOCK_SIZE - 1);
	}

	mt_disp_show_mdump_logo();
	store_header(dev, offset, reason, s_mdump_banks, part_id);
	offset += MDUMP_BLOCK_SIZE;
	for(idx=0; s_mdump_banks[idx].size != 0; ++idx) {
		unsigned int size = s_mdump_banks[idx].size;
		store_memory_bank(dev, offset, &s_mdump_banks[idx], part_id);
		offset += (size + MDUMP_BLOCK_SIZE - 1) & ~(MDUMP_BLOCK_SIZE - 1);
	}
	mt_disp_show_boot_logo();

	return 0;
}

#ifdef CONFIG_MDUMP_COMPRESS
struct compress_segment_request segment_reqs[5] = {
	{ /* segment 1: region 0x8D000000-0xB7BFFFFF; normal compress */
	.start_phyaddr = (mz_uint8 *)0x8D000000,
	.seg_origin_size = 0x2AC00000,
	.seg_order = 5,
	.compress_type = BOOTC_NORMAL_COMPRESS,
	},
	{ /* segment 2: region 0x80000000-0x81DFFFFF; normal compress */
	.start_phyaddr = (mz_uint8 *)0x80000000,
	.seg_origin_size = 0x1E00000,
	.seg_order = 2,
	.compress_type = BOOTC_NORMAL_COMPRESS,
	},
	{ /* segment 3: region 0x81E00000-0x81FFFFFF;
	     lk/mdump/scratch area, skip (fill area with 0) */
	.start_phyaddr = (mz_uint8 *)0x81E00000,
	.seg_origin_size = 0x200000,
	.seg_order = 3,
	.compress_type = BOOTC_ALL_SAME,
	.same_val_in_byte = 0,
	},
	{ /* segment 4: region 0x82000000-0x8CFFFFFF; normal compress */
	.start_phyaddr = (mz_uint8 *)0x82000000,
	.seg_origin_size = 0xB000000,
	.seg_order = 4,
	.compress_type = BOOTC_NORMAL_COMPRESS,
	},
	{ /* segment 5: mdump header; no compress */
	.start_phyaddr = (mz_uint8 *)s_data_buf,
	.seg_origin_size = MDUMP_BLOCK_SIZE,
	.seg_order = 1,
	.compress_type = BOOTC_NO_COMPRESS,
	},
};

struct compress_fullram_request compress_req = {
	.compress_to_phyaddr = (mz_uint8 *)0x8D000000,
	.total_memsize = 0x37C00000,
	.scratch_phyaddr = (mz_uint8 *)0x81F00000,
	.scratch_area_size = 0x60000,
	.first_compress_buffer = (mz_uint8 *)0x81F60000,
	.first_buffer_size = 0x60000,
	.chunk_upper_limit = 64*COMPRESS_1MB,
	.num_of_segments = 5,
	.seg_reqs = segment_reqs,
	.progress_report_callback = mt_disp_show_mdump_progress,
	.system_ping_callback = watchdog_ping,
};
#endif

int ramdump_in_ram_compress(void)
{
#ifdef CONFIG_MDUMP_COMPRESS
	unsigned char reason = 0;

	reason = check_ramdump();
	if (reason == 0) {
		return 0;
	}

	store_header(NULL, 0, reason, s_mdump_banks, 0);
	mt_disp_show_mdump_logo();
	if(compress_mem_regions(&compress_req) > 0) {
		/* if compress succeeded, jump to special kernel mode */
		g_boot_mode = RAMDUMP_BOOT;
	}
	else {
		mt_disp_show_boot_logo();
	}
#endif

	return 0;
}
