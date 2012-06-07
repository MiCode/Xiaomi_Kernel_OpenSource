/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#ifndef QDSP5AUDRECCMDI_H
#define QDSP5AUDRECCMDI_H

/*
 * AUDRECTASK COMMANDS
 * ARM uses 2 queues to communicate with the AUDRECTASK
 * 1.uPAudRec[i]CmdQueue, where i=0,1,2
 * Location :MEMC
 * Buffer Size : 5
 * No of Buffers in a queue : 2
 * 2.uPAudRec[i]BitstreamQueue, where i=0,1,2
 * Location : MEMC
 * Buffer Size : 5
 * No of buffers in a queue : 3
 */

/*
 * Commands on uPAudRec[i]CmdQueue, where i=0,1,2
 */

/*
 * Command to configure memory for enabled encoder
 */

#define AUDREC_CMD_MEM_CFG_CMD 0x0000
#define AUDREC_CMD_ARECMEM_CFG_LEN	\
	sizeof(struct audrec_cmd_arecmem_cfg)

struct audrec_cmd_arecmem_cfg {
	unsigned short cmd_id;
	unsigned short audrec_up_pkt_intm_count;
	unsigned short audrec_ext_pkt_start_addr_msw;
	unsigned short audrec_ext_pkt_start_addr_lsw;
	unsigned short audrec_ext_pkt_buf_number;
} __attribute__((packed));

/*
 * Command to configure pcm input memory
 */

#define AUDREC_CMD_PCM_CFG_ARM_TO_ENC 0x0001
#define AUDREC_CMD_PCM_CFG_ARM_TO_ENC_LEN	\
	sizeof(struct audrec_cmd_pcm_cfg_arm_to_enc)

struct audrec_cmd_pcm_cfg_arm_to_enc {
	unsigned short cmd_id;
	unsigned short config_update_flag;
	unsigned short enable_flag;
	unsigned short sampling_freq;
	unsigned short channels;
	unsigned short frequency_of_intimation;
	unsigned short max_number_of_buffers;
} __attribute__((packed));

#define AUDREC_PCM_CONFIG_UPDATE_FLAG_ENABLE -1
#define AUDREC_PCM_CONFIG_UPDATE_FLAG_DISABLE 0

#define AUDREC_ENABLE_FLAG_VALUE -1
#define AUDREC_DISABLE_FLAG_VALUE 0

/*
 * Command to intimate available pcm buffer
 */

#define AUDREC_CMD_PCM_BUFFER_PTR_REFRESH_ARM_TO_ENC 0x0002
#define AUDREC_CMD_PCM_BUFFER_PTR_REFRESH_ARM_TO_ENC_LEN \
  sizeof(struct audrec_cmd_pcm_buffer_ptr_refresh_arm_enc)

struct audrec_cmd_pcm_buffer_ptr_refresh_arm_enc {
	unsigned short cmd_id;
	unsigned short num_buffers;
	unsigned short buffer_write_cnt_msw;
	unsigned short buffer_write_cnt_lsw;
	unsigned short buf_address_length[8];/*this array holds address
						and length details of
						two buffers*/
} __attribute__((packed));

/*
 * Command to flush
 */

#define AUDREC_CMD_FLUSH 0x0003
#define AUDREC_CMD_FLUSH_LEN	\
	sizeof(struct audrec_cmd_flush)

struct audrec_cmd_flush {
	unsigned short cmd_id;
} __attribute__((packed));

/*
 * Commands on uPAudRec[i]BitstreamQueue, where i=0,1,2
 */

/*
 * Command to indicate current packet read count
 */

#define UP_AUDREC_PACKET_EXT_PTR 0x0000
#define UP_AUDREC_PACKET_EXT_PTR_LEN	\
	sizeof(up_audrec_packet_ext_ptr)

struct up_audrec_packet_ext_ptr {
	unsigned short cmd_id;
	unsigned short audrec_up_curr_read_count_lsw;
	unsigned short audrec_up_curr_read_count_msw;
} __attribute__((packed));

#endif /* QDSP5AUDRECCMDI_H */
