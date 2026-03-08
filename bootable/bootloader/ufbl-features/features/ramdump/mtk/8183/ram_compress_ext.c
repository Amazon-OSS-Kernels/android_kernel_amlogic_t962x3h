/*
 * ram_compress_ext.c
 *
 * Copyright 2020, Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <arch/arm/mmu.h>
#include <platform/mt_reg_base.h>
#include "miniz.h"
#include "ram_compress_ext.h"

static mz_uint8* calcstride(mz_uint32* stride, mz_uint32* avail, struct compress_descriptor *dp, struct compress_req *req)
{
	mz_uint32 mem_avail = 0;
	mz_uint8* compbufaddr = NULL;

	mem_avail = (dp->total_processed - dp->total_compressed)
			+ (req->total_processed - req->total_written);

	/* if existing memory gap (space between the end of compressed content and the beginning address
           of the ram that is going to be compressed next) is less than 3MBytes, then use default compress
           buffer; otherwise, use this memory gap as much as possible for compress */
	if(mem_avail <= INPLACE_COMPRESS_THRESHOLD) {
		*stride = dp->first_buffer_size - 32*1024; // leave 32K buffer zone at the end for safe
		*avail = *stride;
		compbufaddr = dp->first_compress_buffer;
	}
	else {
		*stride = (mem_avail / COMPRESS_1MB) * COMPRESS_1MB - 256*1024;
		*avail = (mem_avail / COMPRESS_1MB) * COMPRESS_1MB; // leave 256K buffer for race issue
		compbufaddr = req->copy_base+req->total_written+2*sizeof(mz_uint32);
	}

	/* Obey the upper limit -- this limit is set to prevent compress takes too long and triggers
           watchdog timeout */
	if(*stride > dp->chunk_upper_limit)
		*stride = dp->chunk_upper_limit;

	/* check if this is the last chunk to compress */
	if((*stride + req->total_processed) > req->region_len)
		*stride = req->region_len - req->total_processed;

	return compbufaddr;
}

static int process_one_compressreq(struct compress_descriptor *dp, struct compress_req *req)
{
	mz_uint8 *srcaddr, *destaddr, *compbuf;
	mz_uint32 stride, compbuflen;
	int ret;

	if(req == NULL || req->start_phyaddr == 0 || req->region_len == 0
                 || req->copy_base == NULL || dp == NULL) {
		return MZ_PARAM_ERROR;
	}

	req->total_processed = 0;
	req->total_written = 0;

	while(req->total_processed < req->region_len) {
		srcaddr = (mz_uint8 *)(req->start_phyaddr + req->total_processed);
		destaddr = req->copy_base + req->total_written;
		compbuf = calcstride(&stride, &compbuflen, dp, req);
		memset(compbuf, 0, stride);
		dprintf(CRITICAL, "compressing %u bytes from %p to %u bytes buffer at %p\r\n",
							stride, srcaddr, compbuflen, compbuf);

		/* watchdog pet */
		if(dp->system_ping_callback)
			dp->system_ping_callback();
		ret = mz_compress(compbuf, (mz_ulong *)&compbuflen, srcaddr, stride);
		if(ret != MZ_OK) {
			/* compress failed, return immediately */
			dprintf(CRITICAL, "Compress failed! base = 0x%x, ret = %d \r\n",
								(unsigned int)srcaddr, ret);
			return ret;
		}

		/* watchdog pet */
		if(dp->system_ping_callback)
			dp->system_ping_callback();
		/* copy compressed length info to destination */
		memcpy(destaddr, (void *)&compbuflen, sizeof(compbuflen));
		/* copy original mem chunck size (for uncompress purpose later) */
		memcpy(destaddr+sizeof(mz_uint32), (void *)&(stride), sizeof(stride));
		/* copy compressed content if not in-place compression */
		if(compbuf == dp->first_compress_buffer) {
			memcpy(destaddr+2*sizeof(mz_uint32), compbuf, compbuflen);
		}

		++req->num_of_chunks;
		req->total_written += compbuflen + 2*sizeof(mz_uint32);
		req->total_processed += stride;
		/* update progress bar */
		if(dp->progress_report_callback) {
			dp->progress_report_callback(dp->total_processed+req->total_processed, dp->total_to_compress);
		}
	}


	return MZ_OK;
}

#define VA_WINDOW_SIZE		(0x40000000ULL)
#define VA_WINDOW_SRC_BASE	(0xC0000000ULL)
#define VA_WINDOW_DEST_BASE	(0x80000000ULL)

static mz_uint32 va_window_remap(mz_uint64 phyaddr_start, mz_uint32 phyaddr_size, mz_uint32 viraddr_base)
{
	mz_uint64 phyaddr;
	mz_uint64 phyaddr_end;
	mz_uint32 viraddr;
	mz_uint32 addr_offset;

	addr_offset = phyaddr_start % SECTION_SIZE;
	phyaddr = phyaddr_start - addr_offset;
	phyaddr_end = phyaddr_start + phyaddr_size;
	viraddr = viraddr_base;

	// Check input arguments
	if (!IS_BLOCK_ALIGNED(viraddr_base)) {
		dprintf(CRITICAL, "remap failed! VA window (0x%x) should be aligned with 1GB address.\r\n",
		        viraddr_base);
		return 0;
	}
	if ((phyaddr_end - phyaddr) > VA_WINDOW_SIZE) {
		dprintf(CRITICAL, "remap failed! Can't fit VA window: from 0x%llx, size 0x%x\r\n",
		        phyaddr_start, phyaddr_size);
		return 0;
	}

	while (phyaddr < phyaddr_end) {
		// Use SECTION descriptor instead of BLOCK descriptor on page table
		if (NO_ERROR != arch_mmu_map_ext((uint64_t)phyaddr,
						 (uint32_t)viraddr,
						 MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA,
						 (uint)SECTION_SIZE)) {
			dprintf(CRITICAL, "remap failed! arch_mmu_map_ext error: phyaddr = 0x%llx, viraddr = 0x%x\r\n",
			        phyaddr, viraddr);
			return 0;
		}
		phyaddr += SECTION_SIZE;
		viraddr += SECTION_SIZE;
	}

	return viraddr_base + addr_offset;
}

int compress_mem_regions(struct compress_fullram_request* comp_req)
{
	int ret;
	mz_uint64 compfilebase;
	mz_uint64 curfilebase;
	mz_uint32 curfilebase_remapping;

	struct compress_req onereq;
	struct compress_descriptor compdp;
	struct compress_segment_request* creq = NULL;
	struct compress_segment_request* creq_original;
	struct compress_segment_request creq_remapping;
	struct compress_segment_header* seg_hdr;
	struct compress_file_header* header;

	mz_uint32 segcnt = 0;
	mz_uint32 skip_num = 0;

	if(!comp_req || !comp_req->num_of_segments) return -1;

	/* set physical mem location and size limit for compression data structures */
	mz_setbase((mz_uint32)comp_req->scratch_phyaddr, comp_req->scratch_area_size);

	/* setup compress descriptor */
	memset(&compdp, 0, sizeof(compdp));
	compdp.first_compress_buffer = comp_req->first_compress_buffer;
	compdp.first_buffer_size = comp_req->first_buffer_size;
	compdp.chunk_upper_limit = comp_req->chunk_upper_limit;
	compdp.total_to_compress = comp_req->total_memsize;
	compdp.system_ping_callback = comp_req->system_ping_callback;
	compdp.progress_report_callback = comp_req->progress_report_callback;
	compdp.total_processed = 0;

	compfilebase = comp_req->compress_to_phyaddr;
	curfilebase = compfilebase + sizeof(struct compress_file_header); // skip the header;


	/* going through all ram segments and compress them one by one */
	for(segcnt = 0; segcnt < comp_req->num_of_segments; segcnt++) {
		creq_original = &(comp_req->seg_reqs[segcnt]);
		if (!creq_original->start_phyaddr || !creq_original->seg_origin_size) {
			/* skip non-exist ram segment */
			dprintf(CRITICAL, "========= Skip segment %d: from 0x%llx, size 0x%x\n",
			        segcnt, creq_original->start_phyaddr, creq_original->seg_origin_size);
			skip_num++;
			continue;
		}

		/* create source virtual address window for ram data */
		memcpy(&creq_remapping, creq_original, sizeof(struct compress_segment_request));
		creq_remapping.start_phyaddr = va_window_remap(creq_original->start_phyaddr, creq_original->seg_origin_size, VA_WINDOW_SRC_BASE);
		if (creq_remapping.start_phyaddr == 0) {
			dprintf(CRITICAL, "remap source virtual address window 0x%llx with size 0x%x failed! \r\n",
			        creq_original->start_phyaddr, creq_original->seg_origin_size);
			ret = -1;
			goto end;
		}
		creq = &creq_remapping;

		/* create destination virtual address window for compress buffer */
		curfilebase_remapping = va_window_remap(curfilebase, VA_WINDOW_SIZE - (curfilebase % SECTION_SIZE), VA_WINDOW_DEST_BASE);
		if (curfilebase_remapping == 0) {
			dprintf(CRITICAL, "remap destination virtual address window 0x%llx failed! \r\n", curfilebase);
			ret = -1;
			goto end;
		}

		dprintf(CRITICAL, "========= now process segment %d: from 0x%llx => 0x%llx, size 0x%x to 0x%llx => 0x%x\n",
		        segcnt, creq_original->start_phyaddr, creq->start_phyaddr, creq->seg_origin_size, curfilebase, curfilebase_remapping);
		seg_hdr = (struct compress_segment_header *)curfilebase_remapping;
		if(creq->compress_type == BOOTC_ALL_SAME) {
			memset(seg_hdr, 0, sizeof(struct compress_segment_header));
			seg_hdr->order = creq->seg_order;
			seg_hdr->origin_size = creq->seg_origin_size;
			seg_hdr->compress_type = creq->compress_type;
			/* fill up certain region with specific value (in byte) */
			seg_hdr->val = creq->same_val_in_byte;
			seg_hdr->compressed_size = sizeof(struct compress_segment_header);
		}
		else if(creq->compress_type == BOOTC_NO_COMPRESS) {
			/* no compression, just simple mem to mem copy */
			if (creq->seg_origin_size + sizeof(struct compress_segment_header) > VA_WINDOW_SIZE) {
				/* header + data size is larger then 1GB window size */
				dprintf(CRITICAL, "non-compress region is too large! \r\n");
				ret = -1;
				goto end;
			}
			/* Can memcpy handle mem overlay case??? */
			memcpy((mz_uint8 *)(curfilebase_remapping+sizeof(struct compress_segment_header)), (mz_uint8 *)creq->start_phyaddr, creq->seg_origin_size);
			memset(seg_hdr, 0, sizeof(struct compress_segment_header));
			seg_hdr->order = creq->seg_order;
			seg_hdr->origin_size = creq->seg_origin_size;
			seg_hdr->compress_type = creq->compress_type;
			seg_hdr->compressed_size = sizeof(struct compress_segment_header) + creq->seg_origin_size;
		}
		else { /* need to perform compression on this mem region */
			/* build up compress request */
			memset((mz_uint8 *)&onereq, 0, sizeof(onereq));
			onereq.start_phyaddr = creq->start_phyaddr;
			onereq.region_len = creq->seg_origin_size;
			onereq.copy_base = (mz_uint8 *)(curfilebase_remapping + sizeof(struct compress_segment_header));

			ret = process_one_compressreq(&compdp, &onereq);
			if(ret != MZ_OK) {
				dprintf(CRITICAL, "compress region 0x%llx with size 0x%x failed! \r\n",
						onereq.start_phyaddr, onereq.region_len);
				// nothing else we can do ...
				ret = -1;
				goto end;
			}
			/*
			 * We should NOT modify the region until it is compressed.
			 * Otherwise, it will corrupt the original content if start_phyaddr==compress_to_phyaddr
                         */
			memset(seg_hdr, 0, sizeof(struct compress_segment_header));
			seg_hdr->order = creq->seg_order;
			seg_hdr->origin_size = creq->seg_origin_size;
			seg_hdr->compress_type = creq->compress_type;
			seg_hdr->compressed_size = sizeof(struct compress_segment_header) + onereq.total_written;
			seg_hdr->val = onereq.num_of_chunks;

		}

		/* make sure each segment starts with 32bit aligned address */
		//if(seg_hdr->compressed_size % 4)
		//	seg_hdr->compressed_size += 4 - (seg_hdr->compressed_size % 4);
		if(seg_hdr->compressed_size % 8) {
			seg_hdr->compressed_size += 8 - (seg_hdr->compressed_size % 8);
		}

		/* ready to move to next segment */
		curfilebase += seg_hdr->compressed_size;
		compdp.total_compressed = curfilebase - compfilebase;
		compdp.total_processed += creq->seg_origin_size;
		dprintf(CRITICAL, "+++++Compress done, orig size: 0x%x, compressed to: 0x%x\n", creq->seg_origin_size, seg_hdr->compressed_size);

		if ((curfilebase - DRAM_PHY_ADDR) >= compdp.total_to_compress) {
			dprintf(CRITICAL, "compressed data exceed compress buffer size! \r\n");
			ret = -1;
			goto end;
		}

		/* update progress bar */
		if(compdp.progress_report_callback)
			compdp.progress_report_callback(compdp.total_processed, compdp.total_to_compress);
	}

	/* add tail signature */
	curfilebase_remapping = va_window_remap(curfilebase, COMP_SIGNATURE_SIZE, VA_WINDOW_DEST_BASE);
	memset((mz_uint8 *)curfilebase_remapping, 0, COMP_SIGNATURE_SIZE);
	snprintf((char *)curfilebase_remapping, COMP_SIGNATURE_SIZE, COMP_TAIL_SIGNATURE);

	/* setup the compress file head */
	memset((mz_uint8 *)compfilebase, 0, sizeof(struct compress_file_header));
	header = (struct compress_file_header *)compfilebase;
	snprintf(header->header_signature, COMP_SIGNATURE_SIZE, COMP_HEAD_SIGNATURE);
	header->num_of_segments = comp_req->num_of_segments - skip_num;
	header->total_file_size = curfilebase - compfilebase + COMP_SIGNATURE_SIZE;

	dprintf(CRITICAL, "ram compression block is ready.\n");
	dprintf(CRITICAL, "total file size is %u bytes(0x%x hex), %d MBytes\n",
            header->total_file_size, header->total_file_size, header->total_file_size/(1024*1024));

end:
	/* restore ARM MMU page table to flat mapping for LK to load image into RAM later */
	va_window_remap(VA_WINDOW_DEST_BASE, VA_WINDOW_SIZE, VA_WINDOW_DEST_BASE);
	va_window_remap(VA_WINDOW_SRC_BASE, VA_WINDOW_SIZE, VA_WINDOW_SRC_BASE);

	return ret;
}

