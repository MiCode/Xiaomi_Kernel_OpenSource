#ifndef QDSP5AUDPLAYMSG_H
#define QDSP5AUDPLAYMSG_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

       Q D S P 5  A U D I O   P L A Y  T A S K   M S G

GENERAL DESCRIPTION
  Message sent by AUDPLAY task

REFERENCES
  None

  
Copyright (c) 1992-2009, The Linux Foundation. All rights reserved.

This software is licensed under the terms of the GNU General Public
License version 2, as published by the Free Software Foundation, and
may be copied, distributed, and modified under those terms.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/
/*===========================================================================

                      EDIT HISTORY FOR FILE

This section contains comments describing changes made to this file.
Notice that changes are listed in reverse chronological order.
  
$Header: //source/qcom/qct/multimedia2/Audio/drivers/QDSP5Driver/QDSP5Interface/main/latest/qdsp5audplaymsg.h#3 $

===========================================================================*/
#define AUDPLAY_MSG_DEC_NEEDS_DATA		0x0001
#define AUDPLAY_MSG_DEC_NEEDS_DATA_MSG_LEN	\
	sizeof(audplay_msg_dec_needs_data)

typedef struct{
   /* reserved*/
  unsigned int dec_id;           

  /*The read pointer offset of external memory till which bitstream 
    has been dme’d in*/
  unsigned int adecDataReadPtrOffset;  

  /*	The buffer size of external memory. */
  unsigned int adecDataBufSize;
  
  unsigned int 	bitstream_free_len;
  unsigned int	bitstream_write_ptr;
  unsigned int	bitstarem_buf_start;
  unsigned int	bitstream_buf_len;
} __attribute__((packed)) audplay_msg_dec_needs_data;

#define AUDPLAY_UP_STREAM_INFO 0x0003
#define AUDPLAY_UP_STREAM_INFO_LEN \
  sizeof(struct audplay_msg_stream_info)

struct audplay_msg_stream_info {
  unsigned int decoder_id;
  unsigned int channel_info;
  unsigned int sample_freq;
  unsigned int bitstream_info;
  unsigned int bit_rate;
} __attribute__((packed));

#define AUDPLAY_MSG_BUFFER_UPDATE 0x0004
#define AUDPLAY_MSG_BUFFER_UPDATE_LEN \
  sizeof(struct audplay_msg_buffer_update)

struct audplay_msg_buffer_update {
  unsigned int buffer_write_count;
  unsigned int num_of_buffer;
  unsigned int buf0_address;
  unsigned int buf0_length;
  unsigned int buf1_address;
  unsigned int buf1_length;
} __attribute__((packed));

#define ADSP_MESSAGE_ID 0xFFFF
#endif /* QDSP5AUDPLAYMSG_H */
