/*
 * ram_compress.c
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <string.h>
#include <stdlib.h>
#include "miniz.h"
#include "ram_compress.h"

static mz_uint8* calcstride(mz_uint32* stride, struct compress_descriptor *dp, struct compress_req *req)
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
		compbufaddr = dp->first_compress_buffer;
	}
	else {
		*stride = (mem_avail / COMPRESS_1MB) * COMPRESS_1MB - 256*1024;
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

	if(req == NULL || req->start_phyaddr == NULL || req->region_len == 0
                 || req->copy_base == NULL || dp == NULL) {
		return MZ_PARAM_ERROR;
	}

	req->total_processed = 0;
	req->total_written = 0;

	while(req->total_processed < req->region_len) {
		srcaddr = req->start_phyaddr + req->total_processed;
		destaddr = req->copy_base + req->total_written;
		compbuf = calcstride(&stride, dp, req);
		memset(compbuf, 0, stride);
		dprintf(CRITICAL, "compressing %d bytes from membase: 0x%p\r\n",
							stride, srcaddr);

		/* watchdog pet */
		if(dp->system_ping_callback)
			dp->system_ping_callback();
		compbuflen = stride;
		ret = mz_compress(compbuf, (mz_ulong *)&compbuflen, srcaddr, stride);
		if(ret != MZ_OK) {
			/* compress failed, return immediately */
			dprintf(CRITICAL, "Compress failed! base = 0x%x, ret = %d \r\n",
								(unsigned int)srcaddr, ret);
			return ret;
		}

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

int compress_mem_regions(struct compress_fullram_request* comp_req)
{
	int ret;
	mz_uint8* compfilebase = NULL;
	mz_uint8* curfilebase = NULL;

	struct compress_req onereq;
	struct compress_descriptor compdp;
	struct compress_segment_request* creq = NULL;
	struct compress_segment_header* seg_hdr;
	struct compress_file_header* header;

	mz_uint32 segcnt = 0;

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
		dprintf(CRITICAL, "========= now process segment %d \n", segcnt);
		creq = &(comp_req->seg_reqs[segcnt]);
		seg_hdr = (struct compress_segment_header *)curfilebase;
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
                        /* Can memcpy handle mem overlay case??? */
			memcpy((mz_uint8 *)(curfilebase+sizeof(struct compress_segment_header)), creq->start_phyaddr, creq->seg_origin_size);
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
			onereq.copy_base = curfilebase + sizeof(struct compress_segment_header);

			ret = process_one_compressreq(&compdp, &onereq);
			if(ret != MZ_OK) {
				dprintf(CRITICAL, "compress region 0x%x with size 0x%x failed! \r\n",
						(unsigned int)(onereq.start_phyaddr), onereq.region_len);
				// nothing else we can do ...
				return -1;
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
		dprintf(CRITICAL, "+++++Compress done, orig sie: 0x%lx, compressed to: 0x%lx\n", creq->seg_origin_size, seg_hdr->compressed_size);


		/* update progress bar */
		if(compdp.progress_report_callback)
			compdp.progress_report_callback(compdp.total_processed, compdp.total_to_compress);
	}


	/* setup the compress file head */
	memset((mz_uint8 *)compfilebase, 0, sizeof(struct compress_file_header));
	header = (struct compress_file_header *)compfilebase;
	snprintf(header->header_signature, COMP_SIGNATURE_SIZE, COMP_HEAD_SIGNATURE);
	header->num_of_segments = comp_req->num_of_segments;
	header->total_file_size = curfilebase - compfilebase + COMP_SIGNATURE_SIZE;

	/* add tail signature */
	memset(curfilebase, 0, COMP_SIGNATURE_SIZE);
	snprintf((char *)curfilebase, COMP_SIGNATURE_SIZE, COMP_TAIL_SIGNATURE);

	dprintf(CRITICAL, "ram compression block is ready.\n");
	dprintf(CRITICAL, "total file size is %lu bytes(0x%lx hex), %d MBytes\n",
            header->total_file_size, header->total_file_size, header->total_file_size/(1024*1024));

	return header->total_file_size;
}

