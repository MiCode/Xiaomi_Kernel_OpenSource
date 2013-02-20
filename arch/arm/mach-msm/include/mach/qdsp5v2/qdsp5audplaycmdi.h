#ifndef QDSP5AUDPLAYCMDI_H
#define QDSP5AUDPLAYCMDI_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
	Q D S P 5  A U D I O   P L A Y  T A S K   C O M M A N D S

GENERAL DESCRIPTION
   Command Interface for AUDPLAYTASK on QDSP5

REFERENCES
   None

EXTERNALIZED FUNCTIONS

  audplay_cmd_dec_data_avail
  Send buffer to AUDPLAY task


Copyright (c) 1992-2009, The Linux Foundation. All rights reserved.

This software is licensed under the terms of the GNU General Public
License version 2, as published by the Free Software Foundation, and
may be copied, distributed, and modified under those terms.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#define AUDPLAY_CMD_BITSTREAM_DATA_AVAIL		0x0000
#define AUDPLAY_CMD_BITSTREAM_DATA_AVAIL_LEN	\
	sizeof(struct audplay_cmd_bitstream_data_avail)

/* Type specification of dec_data_avail message sent to AUDPLAYTASK
*/
struct audplay_cmd_bitstream_data_avail{
	/*command ID*/
	unsigned int cmd_id;

	/* Decoder ID for which message is being sent */
	unsigned int decoder_id;

	/* Start address of data in ARM global memory */
	unsigned int buf_ptr;

	/* Number of 16-bit words of bit-stream data contiguously
	* available at the above-mentioned address
	*/
	unsigned int buf_size;

	/* Partition number used by audPlayTask to communicate with DSP's RTOS
	* kernel
	*/
	unsigned int partition_number;

} __attribute__((packed));

#define AUDPLAY_CMD_CHANNEL_INFO 0x0001
#define AUDPLAY_CMD_CHANNEL_INFO_LEN \
  sizeof(struct audplay_cmd_channel_info)

struct audplay_cmd_channel_select {
  unsigned int cmd_id;
  unsigned int stream_id;
  unsigned int channel_select;
} __attribute__((packed));

struct audplay_cmd_threshold_update {
  unsigned int cmd_id;
  unsigned int threshold_update;
  unsigned int threshold_value;
} __attribute__((packed));

union audplay_cmd_channel_info {
  struct audplay_cmd_channel_select ch_select;
  struct audplay_cmd_threshold_update thr_update;
};

#define AUDPLAY_CMD_HPCM_BUF_CFG 0x0003
#define AUDPLAY_CMD_HPCM_BUF_CFG_LEN \
  sizeof(struct audplay_cmd_hpcm_buf_cfg)

struct audplay_cmd_hpcm_buf_cfg {
	unsigned int cmd_id;
	unsigned int hostpcm_config;
	unsigned int feedback_frequency;
	unsigned int byte_swap;
	unsigned int max_buffers;
	unsigned int partition_number;
} __attribute__((packed));

#define AUDPLAY_CMD_BUFFER_REFRESH 0x0004
#define AUDPLAY_CMD_BUFFER_REFRESH_LEN \
  sizeof(struct audplay_cmd_buffer_update)

struct audplay_cmd_buffer_refresh {
	unsigned int cmd_id;
	unsigned int num_buffers;
	unsigned int buf_read_count;
	unsigned int buf0_address;
	unsigned int buf0_length;
	unsigned int buf1_address;
	unsigned int buf1_length;
} __attribute__((packed));

#define AUDPLAY_CMD_BITSTREAM_DATA_AVAIL_NT2            0x0005
#define AUDPLAY_CMD_BITSTREAM_DATA_AVAIL_NT2_LEN    \
	sizeof(struct audplay_cmd_bitstream_data_avail_nt2)

/* Type specification of dec_data_avail message sent to AUDPLAYTASK
 * for NT2 */
struct audplay_cmd_bitstream_data_avail_nt2 {
	/*command ID*/
	unsigned int cmd_id;

	/* Decoder ID for which message is being sent */
	unsigned int decoder_id;

	/* Start address of data in ARM global memory */
	unsigned int buf_ptr;

	/* Number of 16-bit words of bit-stream data contiguously
	*  available at the above-mentioned address
	*/
	unsigned int buf_size;

	/* Partition number used by audPlayTask to communicate with DSP's RTOS
	* kernel
	*/
	unsigned int partition_number;

	/* bitstream write pointer */
	unsigned int dspBitstreamWritePtr;

} __attribute__((packed));

#define AUDPLAY_CMD_OUTPORT_FLUSH 0x0006

struct audplay_cmd_outport_flush {
	unsigned int cmd_id;
} __attribute__((packed));

#endif /* QDSP5AUDPLAYCMD_H */
