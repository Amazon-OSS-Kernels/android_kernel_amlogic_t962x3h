/*
 * mtk8183_ramdump.c
 *
 * Copyright 2011-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#include <platform/mt_typedefs.h>

#include <platform/mtk_wdt.h>
#include <platform/mt_logo.h>
#include <platform/boot_mode.h>
#include <target/cust_display.h>

#include "../../ram_compress.h"
#include "ramdump.h"
#include "mtk8168_ramdump.h"
#include <arch/arm/mmu.h>

static unsigned long s_dump_process = 0;
static unsigned long s_dump_total = 0;
unsigned char s_data_buf[4096] __attribute__ ((aligned (4)));

extern  const struct mdump_bank s_mdump_banks[];

extern BOOTMODE g_boot_mode;

/* module depend code */

static void* get_mdump_device(unsigned long long* poff)
{
	/* not support on mt8168 */
	return NULL;
}

static void store_block(void* dev, unsigned long long offset, const void* data, unsigned int len)
{
	/* not support on mt8168 */
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
static void store_header(void* dev, unsigned long long offset, unsigned char reason, const struct mdump_bank* banks)
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
		store_block(dev, offset, s_data_buf, MDUMP_BLOCK_SIZE);
	}
}

static void store_memory_bank(void* dev, unsigned long long offset, const struct mdump_bank* bank)
{
	unsigned char* mptr = (unsigned char*) (bank->address);
	unsigned int size = bank->size;
	do {
		unsigned int len = (size < MDUMP_STORE_BLOCK) ? size : MDUMP_STORE_BLOCK;
		watchdog_ping();
		dprintf(INFO, "MDump write %s context 0x%x\n", bank->name, (unsigned int)mptr);
		store_block(dev, offset, mptr, len);
		mt_disp_show_memory_dump_process(s_dump_process, s_dump_total);
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


#if 0
	buf->reboot_reason = MDUMP_REBOOT_PANIC;
#else

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
#endif


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

#ifdef CONFIG_MDUMP_COMPRESS
extern struct compress_segment_request segment_reqs[];
extern struct compress_fullram_request compress_req;
#endif

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

#ifdef CONFIG_MDUMP_COMPRESS
	compress_req.progress_report_callback = show_mdump_progress;
	compress_req.system_ping_callback = watchdog_ping;
#endif
}

int ramdump_to_eMMC(void)
{
	int idx = 0;
	unsigned char reason = 0;
	void* dev = NULL;
	unsigned long long offset = 0;

	reason = check_ramdump();
	if (reason == 0) {
		return 0;
	}

	dev = get_mdump_device(&offset);
	if (dev == NULL) {
		return 0;
	}

	s_dump_total = MDUMP_BLOCK_SIZE;
	for(idx=0; s_mdump_banks[idx].size != 0; ++idx) {
		unsigned int size = s_mdump_banks[idx].size;
		s_dump_total += (size + MDUMP_BLOCK_SIZE - 1) & ~(MDUMP_BLOCK_SIZE - 1);
	}

	mt_disp_show_memory_dump();
	store_header(dev, offset, reason, s_mdump_banks);
	offset += MDUMP_BLOCK_SIZE;
	for(idx=0; s_mdump_banks[idx].size != 0; ++idx) {
		unsigned int size = s_mdump_banks[idx].size;
		store_memory_bank(dev, offset, &s_mdump_banks[idx]);
		offset += (size + MDUMP_BLOCK_SIZE - 1) & ~(MDUMP_BLOCK_SIZE - 1);
	}
	mt_disp_show_boot_logo();

	return 0;
}


#ifdef CONFIG_MDUMP_COMPRESS
void __attribute__((weak)) adjust_mem_cfg(void) {};
#endif

int ramdump_in_ram_compress(void)
{
#ifdef CONFIG_MDUMP_COMPRESS
	unsigned char reason = 0;

	reason = check_ramdump();
	if (reason == 0) {
		return 0;
	}

	adjust_mem_cfg();

	show_mdump_logo();

	store_header(NULL, 0, reason, s_mdump_banks);
	// mt_disp_show_memory_dump();
	if(compress_mem_regions(&compress_req) > 0) {
		/* if compress succeeded, jump to special kernel mode */
		g_boot_mode = RAMDUMP_BOOT;
	}
#endif

	return 0;
}

#ifdef CONFIG_MDUMP_COMPRESS
void ramdump_get_mem_range(unsigned int *start, unsigned int *size)
{
	struct compress_file_header* header;
	*start = (unsigned int)compress_req.compress_to_phyaddr;
	header = (struct compress_file_header*)*start;

	if (!strncmp(header->header_signature, COMP_HEAD_SIGNATURE, COMP_SIGNATURE_SIZE)) {
		*size = header->total_file_size;
	} else {
		*size = 0;
	}
}
#endif
