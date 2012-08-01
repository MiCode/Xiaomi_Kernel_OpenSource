/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#ifndef _VCD_DDL_METADATA_H_
#define _VCD_DDL_METADATA_H_

#define DDL_MAX_DEC_METADATATYPE          10
#define DDL_MAX_ENC_METADATATYPE          3
#define DDL_METADATA_EXTRAPAD_SIZE      256
#define DDL_METADATA_HDR_SIZE            20
#define DDL_METADATA_EXTRADATANONE_SIZE  24
#define DDL_METADATA_ALIGNSIZE(x) ((x) = (((x) + 0x7) & ~0x7))
#define DDL_METADATA_MANDATORY \
	(VCD_METADATA_DATANONE | VCD_METADATA_QCOMFILLER)
#define DDL_METADATA_VC1_PAYLOAD_SIZE         (38*4)
#define DDL_METADATA_SEI_PAYLOAD_SIZE          100
#define DDL_METADATA_SEI_MAX                     5
#define DDL_METADATA_VUI_PAYLOAD_SIZE          256
#define DDL_METADATA_PASSTHROUGH_PAYLOAD_SIZE   68
#define DDL_METADATA_EXT_PAYLOAD_SIZE         (640)
#define DDL_METADATA_USER_PAYLOAD_SIZE        (2048)
#define DDL_METADATA_CLIENT_INPUTBUFSIZE       256
#define DDL_METADATA_TOTAL_INPUTBUFSIZE \
	(DDL_METADATA_CLIENT_INPUTBUFSIZE * VCD_MAX_NO_CLIENT)

#define DDL_METADATA_CLIENT_INPUTBUF(main_buffer, client_buffer,\
	channel_id) { \
	(client_buffer)->align_physical_addr = (u8 *) \
	((u8 *)(main_buffer)->align_physical_addr + \
	(DDL_METADATA_CLIENT_INPUTBUFSIZE * channel_id)); \
	(client_buffer)->align_virtual_addr = (u8 *) \
	((u8 *)(main_buffer)->align_virtual_addr + \
	(DDL_METADATA_CLIENT_INPUTBUFSIZE * channel_id)); \
	(client_buffer)->virtual_base_addr = 0;	\
	}

#define DDL_METADATA_HDR_VERSION_INDEX 0
#define DDL_METADATA_HDR_PORT_INDEX    1
#define DDL_METADATA_HDR_TYPE_INDEX    2

#define DDL_METADATA_USER_DUMP_DISABLE_MODE 0
#define DDL_METADATA_USER_DUMP_OFFSET_MODE  1
#define DDL_METADATA_USER_DUMP_FULL_MODE    2

void ddl_set_default_meta_data_hdr(struct ddl_client_context *ddl);
u32 ddl_get_metadata_params(struct ddl_client_context *ddl,
	struct vcd_property_hdr *property_hdr, void *property_value);
u32 ddl_set_metadata_params(struct ddl_client_context *ddl,
	struct vcd_property_hdr *property_hdr, void *property_value);
void ddl_set_default_metadata_flag(struct ddl_client_context *ddl);
void ddl_set_default_decoder_metadata_buffer_size(struct ddl_decoder_data
	*decoder, struct vcd_property_frame_size *frame_size,
	struct vcd_buffer_requirement *output_buf_req);
void ddl_set_default_encoder_metadata_buffer_size(
	struct ddl_encoder_data *encoder);
void ddl_vidc_metadata_enable(struct ddl_client_context *ddl);
u32 ddl_vidc_encode_set_metadata_output_buf(struct ddl_client_context *ddl);
void ddl_vidc_decode_set_metadata_output(struct ddl_decoder_data *decoder);
void ddl_process_encoder_metadata(struct ddl_client_context *ddl);
void ddl_process_decoder_metadata(struct ddl_client_context *ddl);
void ddl_set_mp2_dump_default(struct ddl_decoder_data *decoder, u32 flag);

#endif
