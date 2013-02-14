/* arch/arm/mach-msm/qdsp5/adsp_video_verify_cmd.c
 *
 * Verificion code for aDSP VDEC packets from userspace.
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2010, 2012 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/android_pmem.h>

#include <mach/qdsp5/qdsp5vdeccmdi.h>
#include "adsp.h"
#include <mach/debug_mm.h>

#define MAX_FLUSH_SIZE 160

static inline void *high_low_short_to_ptr(unsigned short high,
					  unsigned short low)
{
	return (void *)((((unsigned long)high) << 16) | ((unsigned long)low));
}

static inline void ptr_to_high_low_short(void *ptr, unsigned short *high,
					 unsigned short *low)
{
	*high = (unsigned short)((((unsigned long)ptr) >> 16) & 0xffff);
	*low = (unsigned short)((unsigned long)ptr & 0xffff);
}

static int pmem_fixup_high_low(unsigned short *high,
				unsigned short *low,
				unsigned short size_high,
				unsigned short size_low,
				struct msm_adsp_module *module,
				unsigned long *addr, unsigned long *size,
				struct file **filp, unsigned long *offset)
{
	void *phys_addr;
	unsigned long phys_size;
	unsigned long kvaddr;

	phys_addr = high_low_short_to_ptr(*high, *low);
	phys_size = (unsigned long)high_low_short_to_ptr(size_high, size_low);
	MM_DBG("virt %x %x\n", (unsigned int)phys_addr,
			(unsigned int)phys_size);
	if (phys_addr) {
		if (adsp_ion_fixup_kvaddr(module, &phys_addr,
			 &kvaddr, phys_size, filp, offset)) {
			MM_ERR("ah%x al%x sh%x sl%x addr %x size %x\n",
					*high, *low, size_high,
					size_low, (unsigned int)phys_addr,
					(unsigned int)phys_size);
			return -EINVAL;
		}
	}
	ptr_to_high_low_short(phys_addr, high, low);
	MM_DBG("phys %x %x\n", (unsigned int)phys_addr,
			(unsigned int)phys_size);
	if (addr)
		*addr = kvaddr;
	if (size)
		*size = phys_size;
	return 0;
}

static int verify_vdec_pkt_cmd(struct msm_adsp_module *module,
			       void *cmd_data, size_t cmd_size)
{
	void *phys_addr;
	unsigned short cmd_id = ((unsigned short *)cmd_data)[0];
	viddec_cmd_subframe_pkt *pkt;
	unsigned long subframe_pkt_addr;
	unsigned long subframe_pkt_size;
	unsigned short *frame_header_pkt;
	int i, num_addr, col_addr = 0, skip;
	int start_pos = 0, xdim_pos = 1, ydim_pos = 2;
	unsigned short *frame_buffer_high, *frame_buffer_low;
	unsigned long frame_buffer_size;
	unsigned short frame_buffer_size_high, frame_buffer_size_low;
	struct file *filp = NULL;
	unsigned long offset = 0;
	unsigned long Codec_Id = 0;

	MM_DBG("cmd_size %d cmd_id %d cmd_data %x\n", cmd_size, cmd_id,
					(unsigned int)cmd_data);
	if (cmd_id != VIDDEC_CMD_SUBFRAME_PKT) {
		MM_INFO("adsp_video: unknown video packet %u\n", cmd_id);
		return 0;
	}
	if (cmd_size < sizeof(viddec_cmd_subframe_pkt))
		return -1;

	pkt = (viddec_cmd_subframe_pkt *)cmd_data;
	phys_addr = high_low_short_to_ptr(pkt->subframe_packet_high,
				pkt->subframe_packet_low);

	if (pmem_fixup_high_low(&(pkt->subframe_packet_high),
				&(pkt->subframe_packet_low),
				pkt->subframe_packet_size_high,
				pkt->subframe_packet_size_low,
				module,
				&subframe_pkt_addr,
				&subframe_pkt_size,
				&filp, &offset))
		return -1;
	Codec_Id = pkt->codec_selection_word;
	/*Invalidate cache before accessing the cached pmem buffer*/
	if (adsp_ion_do_cache_op(module, phys_addr, (void *)subframe_pkt_addr,
		subframe_pkt_size*2, offset, ION_IOC_INV_CACHES)){
		MM_ERR("Cache operation failed for" \
			" phys addr high %x addr low %x\n",
			pkt->subframe_packet_high, pkt->subframe_packet_low);
		return -EINVAL;
	}
	/* deref those ptrs and check if they are a frame header packet */
	frame_header_pkt = (unsigned short *)subframe_pkt_addr;
	switch (frame_header_pkt[0]) {
	case 0xB201: /* h.264 vld in dsp */
	   if (Codec_Id == 0x8) {
		num_addr = 16;
		skip = 0;
		start_pos = 5;
	   } else {
	       num_addr = 16;
	       skip = 0;
	       start_pos = 6;
	       col_addr = 17;
	   }
		break;
	case 0x8201: /* h.264 vld in arm */
		num_addr = 16;
		skip = 0;
		start_pos = 6;
		break;
	case 0x4D01: /* mpeg-4 and h.263 vld in arm */
		num_addr = 3;
		skip = 0;
		start_pos = 5;
		break;
	case 0x9201: /*For Real Decoder*/
		num_addr = 2;
		skip = 0;
		start_pos = 5;
		break;
	case 0xBD01: /* mpeg-4 and h.263 vld in dsp */
		num_addr = 3;
		skip = 0;
		start_pos = 6;
		if (((frame_header_pkt[5] & 0x000c) >> 2) == 0x2) /* B-frame */
			start_pos = 8;
		break;
	case 0x0001: /* wmv */
		num_addr = 2;
		skip = 0;
		start_pos = 5;
		break;
	case 0xC201: /*WMV main profile*/
		 num_addr = 3;
		 skip = 0;
		 start_pos = 6;
		 break;
	case 0xDD01: /* VP6 */
		num_addr = 3;
		skip = 0;
		start_pos = 10;
		break;
	case 0xFD01: /* VP8 */
		num_addr = 3;
		skip = 0;
		start_pos = 24;
		break;
	default:
		return 0;
	}

	frame_buffer_high = &frame_header_pkt[start_pos];
	frame_buffer_low = &frame_header_pkt[start_pos + 1];
	frame_buffer_size = (frame_header_pkt[xdim_pos] *
			     frame_header_pkt[ydim_pos] * 3) / 2;
	ptr_to_high_low_short((void *)frame_buffer_size,
			      &frame_buffer_size_high,
			      &frame_buffer_size_low);
	for (i = 0; i < num_addr; i++) {
		if (frame_buffer_high && frame_buffer_low) {
			if (pmem_fixup_high_low(frame_buffer_high,
						frame_buffer_low,
						frame_buffer_size_high,
						frame_buffer_size_low,
						module,
						NULL, NULL, NULL, NULL))
				return -EINVAL;
	   }
		frame_buffer_high += 2;
		frame_buffer_low += 2;
	}
	/* Patch the output buffer. */
	frame_buffer_high += 2*skip;
	frame_buffer_low += 2*skip;
	if (frame_buffer_high && frame_buffer_low) {
		if (pmem_fixup_high_low(frame_buffer_high,
					frame_buffer_low,
					frame_buffer_size_high,
					frame_buffer_size_low,
					module,
					NULL, NULL, NULL, NULL))
			return -EINVAL;
	}
	if (col_addr) {
		frame_buffer_high += 2;
		frame_buffer_low += 2;
		/* Patch the Co-located buffers.*/
		frame_buffer_size =  (72 * frame_header_pkt[xdim_pos] *
					frame_header_pkt[ydim_pos]) >> 16;
		ptr_to_high_low_short((void *)frame_buffer_size,
					&frame_buffer_size_high,
					&frame_buffer_size_low);
		for (i = 0; i < col_addr; i++) {
			if (frame_buffer_high && frame_buffer_low) {
				if (pmem_fixup_high_low(frame_buffer_high,
						frame_buffer_low,
						frame_buffer_size_high,
						frame_buffer_size_low,
						module,
						NULL, NULL, NULL, NULL))
					return -EINVAL;
			}
			frame_buffer_high += 2;
			frame_buffer_low += 2;
		}
	}
	/*Flush the cached mem subframe packet before sending to DSP*/
	if (adsp_ion_do_cache_op(module,  phys_addr, (void *)subframe_pkt_addr,
		MAX_FLUSH_SIZE, offset, ION_IOC_CLEAN_CACHES)){
		MM_ERR("Cache operation failed for" \
			" phys addr high %x addr low %x\n",
			pkt->subframe_packet_high, pkt->subframe_packet_low);
		return -EINVAL;
	}
	return 0;
}

int adsp_video_verify_cmd(struct msm_adsp_module *module,
			 unsigned int queue_id, void *cmd_data,
			 size_t cmd_size)
{
	switch (queue_id) {
	case QDSP_mpuVDecPktQueue:
		return verify_vdec_pkt_cmd(module, cmd_data, cmd_size);
	default:
		MM_INFO("unknown video queue %u\n", queue_id);
		return 0;
	}
}

