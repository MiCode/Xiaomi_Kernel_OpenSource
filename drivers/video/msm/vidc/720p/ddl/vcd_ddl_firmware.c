/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <media/msm/vidc_type.h>
#include "vcd_ddl_firmware.h"
#include "vcd_ddl_utils.h"

#define VCDFW_TOTALNUM_IMAGE  7
#define VCDFW_MAX_NO_IMAGE    2

struct vcd_firmware {
	u32 active_fw_img[VCDFW_TOTALNUM_IMAGE];
	struct ddl_buf_addr boot_code;

	struct ddl_buf_addr enc_mpeg4;
	struct ddl_buf_addr encH264;

	struct ddl_buf_addr dec_mpeg4;
	struct ddl_buf_addr decH264;
	struct ddl_buf_addr decH263;
	struct ddl_buf_addr dec_mpeg2;
	struct ddl_buf_addr dec_vc1;
};

static struct vcd_firmware vcd_firmware;


static void vcd_fw_change_endian(unsigned char *fw, u32 fw_size)
{
	u32 i = 0;
	unsigned char temp;
	for (i = 0; i < fw_size; i = i + 4) {
		temp = fw[i];
		fw[i] = fw[i + 3];
		fw[i + 3] = temp;

		temp = fw[i + 1];
		fw[i + 1] = fw[i + 2];
		fw[i + 2] = temp;
	}
	return;
}

static u32 vcd_fw_prepare(struct ddl_buf_addr *fw_details,
			 const unsigned char fw_array[],
			 const unsigned int fw_array_size, u32 change_endian)
{
	u32 *buffer;

	ddl_pmem_alloc(fw_details, fw_array_size,
		       DDL_LINEAR_BUFFER_ALIGN_BYTES);
	if (!fw_details->virtual_base_addr)
		return false;

	fw_details->buffer_size = fw_array_size / 4;

	buffer = fw_details->align_virtual_addr;

	memcpy(buffer, fw_array, fw_array_size);
	if (change_endian)
		vcd_fw_change_endian((unsigned char *)buffer, fw_array_size);
	return true;
}

u32 vcd_fw_init(void)
{
	u32 status = false;

	status = vcd_fw_prepare(&vcd_firmware.boot_code,
				vidc_command_control_fw,
				vidc_command_control_fw_size, false);

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.dec_mpeg4,
					vidc_mpg4_dec_fw,
					vidc_mpg4_dec_fw_size, true);
	}

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.decH264,
					vidc_h264_dec_fw,
					vidc_h264_dec_fw_size, true);
	}

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.decH263,
					vidc_h263_dec_fw,
					vidc_h263_dec_fw_size, true);
	}

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.enc_mpeg4,
					vidc_mpg4_enc_fw,
					vidc_mpg4_enc_fw_size, true);
	}

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.encH264,
					vidc_h264_enc_fw,
					vidc_h264_enc_fw_size, true);
	}

	if (status) {
		status = vcd_fw_prepare(&vcd_firmware.dec_vc1,
					vidc_vc1_dec_fw,
					vidc_vc1_dec_fw_size, true);
	}
	return status;
}


static u32 get_dec_fw_image(struct vcd_fw_details *fw_details)
{
	u32 status = true;
	switch (fw_details->codec) {
	case VCD_CODEC_DIVX_4:
	case VCD_CODEC_DIVX_5:
	case VCD_CODEC_DIVX_6:
	case VCD_CODEC_XVID:
	case VCD_CODEC_MPEG4:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.dec_mpeg4.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.dec_mpeg4.buffer_size;
			break;
		}
	case VCD_CODEC_H264:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.decH264.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.decH264.buffer_size;
			break;
		}
	case VCD_CODEC_VC1:
	case VCD_CODEC_VC1_RCV:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.dec_vc1.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.dec_vc1.buffer_size;
			break;
		}
	case VCD_CODEC_MPEG2:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.dec_mpeg2.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.dec_mpeg2.buffer_size;
			break;
		}
	case VCD_CODEC_H263:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.decH263.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.decH263.buffer_size;
			break;
		}
	default:
		{
			status = false;
			break;
		}
	}
	return status;
}

static u32 get_enc_fw_image(struct vcd_fw_details *fw_details)
{
	u32 status = true;
	switch (fw_details->codec) {
	case VCD_CODEC_H263:
	case VCD_CODEC_MPEG4:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.enc_mpeg4.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.enc_mpeg4.buffer_size;
			break;
		}
	case VCD_CODEC_H264:
		{
			fw_details->fw_buffer_addr =
			    vcd_firmware.encH264.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.encH264.buffer_size;
			break;
		}
	default:
		{
			status = false;
			break;
		}
	}
	return status;
}

u32 vcd_get_fw_property(u32 prop_id, void *prop_details)
{
	u32 status = true;
	struct vcd_fw_details *fw_details;
	switch (prop_id) {
	case VCD_FW_ENDIAN:
		{
			*(u32 *) prop_details = VCD_FW_BIG_ENDIAN;
			break;
		}
	case VCD_FW_BOOTCODE:
		{
			fw_details =
			    (struct vcd_fw_details *)prop_details;
			fw_details->fw_buffer_addr =
			    vcd_firmware.boot_code.align_physical_addr;
			fw_details->fw_size =
			    vcd_firmware.boot_code.buffer_size;
			break;
		}
	case VCD_FW_DECODE:
		{
			fw_details =
			    (struct vcd_fw_details *)prop_details;
			status = get_dec_fw_image(fw_details);
			break;
		}
	case VCD_FW_ENCODE:
		{
			fw_details =
			    (struct vcd_fw_details *)prop_details;
			status = get_enc_fw_image(fw_details);
			break;
		}
	default:
		{
			status = false;
			break;
		}
	}
	return status;
}

u32 vcd_fw_transact(u32 add, u32 decoding, enum vcd_codec codec)
{
	u32 status = true;
	u32 index = 0, active_fw = 0, loop_count;

	if (decoding) {
		switch (codec) {
		case VCD_CODEC_DIVX_4:
		case VCD_CODEC_DIVX_5:
		case VCD_CODEC_DIVX_6:
		case VCD_CODEC_XVID:
		case VCD_CODEC_MPEG4:
			{
				index = 0;
				break;
			}
		case VCD_CODEC_H264:
			{
				index = 1;
				break;
			}
		case VCD_CODEC_H263:
			{
				index = 2;
				break;
			}
		case VCD_CODEC_MPEG2:
			{
				index = 3;
				break;
			}
		case VCD_CODEC_VC1:
		case VCD_CODEC_VC1_RCV:
			{
				index = 4;
				break;
			}
		default:
			{
				status = false;
				break;
			}
		}
	} else {
		switch (codec) {
		case VCD_CODEC_H263:
		case VCD_CODEC_MPEG4:
			{
				index = 5;
				break;
			}
		case VCD_CODEC_H264:
			{
				index = 6;
				break;
			}
		default:
			{
				status = false;
				break;
			}
		}
	}

	if (!status)
		return status;

	if (!add &&
	    vcd_firmware.active_fw_img[index]
	    ) {
		--vcd_firmware.active_fw_img[index];
		return status;
	}

	for (loop_count = 0; loop_count < VCDFW_TOTALNUM_IMAGE;
	     ++loop_count) {
		if (vcd_firmware.active_fw_img[loop_count])
			++active_fw;
	}

	if (active_fw < VCDFW_MAX_NO_IMAGE ||
	    vcd_firmware.active_fw_img[index] > 0) {
		++vcd_firmware.active_fw_img[index];
	} else {
		status = false;
	}
	return status;
}

void vcd_fw_release(void)
{
	ddl_pmem_free(&vcd_firmware.boot_code);
	ddl_pmem_free(&vcd_firmware.enc_mpeg4);
	ddl_pmem_free(&vcd_firmware.encH264);
	ddl_pmem_free(&vcd_firmware.dec_mpeg4);
	ddl_pmem_free(&vcd_firmware.decH264);
	ddl_pmem_free(&vcd_firmware.decH263);
	ddl_pmem_free(&vcd_firmware.dec_mpeg2);
	ddl_pmem_free(&vcd_firmware.dec_vc1);
}
