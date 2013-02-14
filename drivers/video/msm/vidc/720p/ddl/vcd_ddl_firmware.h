/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#ifndef _VCD_DDL_FIRMWARE_H_
#define _VCD_DDL_FIRMWARE_H_
#include <media/msm/vcd_property.h>

#define VCD_FW_BIG_ENDIAN     0x0
#define VCD_FW_LITTLE_ENDIAN  0x1

struct vcd_fw_details {
	enum vcd_codec codec;
	u32 *fw_buffer_addr;
	u32 fw_size;
};

#define VCD_FW_PROP_BASE         0x0

#define VCD_FW_ENDIAN       (VCD_FW_PROP_BASE + 0x1)
#define VCD_FW_BOOTCODE     (VCD_FW_PROP_BASE + 0x2)
#define VCD_FW_DECODE     (VCD_FW_PROP_BASE + 0x3)
#define VCD_FW_ENCODE     (VCD_FW_PROP_BASE + 0x4)

extern unsigned char *vidc_command_control_fw;
extern u32 vidc_command_control_fw_size;
extern unsigned char *vidc_mpg4_dec_fw;
extern u32 vidc_mpg4_dec_fw_size;
extern unsigned char *vidc_h263_dec_fw;
extern u32 vidc_h263_dec_fw_size;
extern unsigned char *vidc_h264_dec_fw;
extern u32 vidc_h264_dec_fw_size;
extern unsigned char *vidc_mpg4_enc_fw;
extern u32 vidc_mpg4_enc_fw_size;
extern unsigned char *vidc_h264_enc_fw;
extern u32 vidc_h264_enc_fw_size;
extern unsigned char *vidc_vc1_dec_fw;
extern u32 vidc_vc1_dec_fw_size;

u32 vcd_fw_init(void);
u32 vcd_get_fw_property(u32 prop_id, void *prop_details);
u32 vcd_fw_transact(u32 add, u32 decoding, enum vcd_codec codec);
void vcd_fw_release(void);

#endif
