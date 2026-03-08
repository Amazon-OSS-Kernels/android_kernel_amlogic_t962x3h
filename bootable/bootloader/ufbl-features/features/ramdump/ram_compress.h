/*
 * ramdump_compress.h
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#ifndef __RAM_COMPRESS_H__
#define __RAM_COMPRESS_H__

#include "miniz.h"

#define COMP_SIGNATURE_SIZE  16
#define COMP_HEAD_SIGNATURE  "UFBLCOMPDUMPSTA"
#define COMP_TAIL_SIGNATURE  "UFBLCOMPDUMPEND"

#define COMPRESS_1MB                    (1024*1024)       // compress unit size
#define INPLACE_COMPRESS_THRESHOLD      (3*COMPRESS_1MB)  // if over 3MB space saved, then do in-place compression

enum {
	BOOTC_NORMAL_COMPRESS = 1,
	BOOTC_ALL_SAME = 2,
	BOOTC_NO_COMPRESS = 3
};

struct compress_segment_request {
	mz_uint8* start_phyaddr;
	mz_uint32 seg_origin_size;
	mz_uint32 seg_order;
	mz_uint32 compress_type;
	mz_uint32 same_val_in_byte;
};

struct compress_fullram_request {
	mz_uint8* compress_to_phyaddr;
	mz_uint32 total_memsize;
	mz_uint8* scratch_phyaddr;
	mz_uint32 scratch_area_size;
	mz_uint8* first_compress_buffer;
	mz_uint32 first_buffer_size;
	mz_uint32 chunk_upper_limit;
	mz_uint32 num_of_segments;
	struct compress_segment_request* seg_reqs;
	void (*progress_report_callback)(unsigned long processed, unsigned long total);
	void (*system_ping_callback)(void);
};

struct compress_file_header {
	char header_signature[COMP_SIGNATURE_SIZE];
	mz_uint32 num_of_segments;
	mz_uint32 total_file_size;
};

struct compress_segment_header {
	mz_uint32 origin_size;
	mz_uint32 compressed_size;
	mz_uint32 order;
	mz_uint32 compress_type;
	mz_uint32 val;
};

struct compress_descriptor {
	mz_uint8* first_compress_buffer;
	mz_uint32 first_buffer_size;
	mz_uint32 chunk_upper_limit;
	mz_uint32 total_to_compress;
	mz_uint32 total_processed;
	mz_uint32 total_compressed;
	void (*progress_report_callback)(unsigned long processed, unsigned long total);
	void (*system_ping_callback)(void);
};

struct compress_req {
        mz_uint8*  start_phyaddr;
        mz_uint32  region_len;
        mz_uint8*  copy_base;
        mz_uint32  total_processed;
        mz_uint32  total_written;
	mz_uint32  num_of_chunks;
};

int compress_mem_regions(struct compress_fullram_request* req);

#endif /* __RAM_COMPRESS_H__ */
