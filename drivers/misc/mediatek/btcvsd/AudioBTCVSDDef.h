/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _AUDIO_BTCVSD_DEF_H_
#define _AUDIO_BTCVSD_DEF_H_

#define BT_SCO_PACKET_120 120
#define BT_SCO_PACKET_180 180

#define BT_CVSD_TX_NREADY    (1<<21)
#define BT_CVSD_RX_READY     (1<<22)
#define BT_CVSD_TX_UNDERFLOW (1<<23)
#define BT_CVSD_RX_OVERFLOW  (1<<24)
#define BT_CVSD_INTERRUPT    (1<<31)

#define BT_CVSD_CLEAR (BT_CVSD_TX_NREADY|BT_CVSD_RX_READY|BT_CVSD_TX_UNDERFLOW|BT_CVSD_RX_OVERFLOW|BT_CVSD_INTERRUPT)

/*TX*/
#define SCO_TX_ENCODE_SIZE           (60) /*60 byte (60*8 samples)*/
#define SCO_TX_PACKER_BUF_NUM        (8) /*8*/
#define SCO_TX_PACKET_MASK           (0x7) /*0x7*/
#define SCO_TX_PCM64K_BUF_SIZE       (SCO_TX_ENCODE_SIZE*2*8) /* 60 * 2 * 8 byte*/

/*RX*/
#define SCO_RX_PLC_SIZE              (30)
#define SCO_RX_PACKER_BUF_NUM        (16)   /*16*/
#define SCO_RX_PACKET_MASK           (0xF)   /*0xF*/
#define SCO_RX_PCM64K_BUF_SIZE       (SCO_RX_PLC_SIZE*2*8)
#define SCO_RX_PCM8K_BUF_SIZE        (SCO_RX_PLC_SIZE*2)

#define BTSCO_CVSD_RX_FRAME SCO_RX_PACKER_BUF_NUM
#define BTSCO_CVSD_RX_INBUF_SIZE (BTSCO_CVSD_RX_FRAME*SCO_RX_PLC_SIZE)
#define BTSCO_CVSD_PACKET_VALID_SIZE 2
#define BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE (BTSCO_CVSD_RX_FRAME*(SCO_RX_PLC_SIZE+BTSCO_CVSD_PACKET_VALID_SIZE))

#define BTSCO_CVSD_TX_FRAME SCO_TX_PACKER_BUF_NUM
#define BTSCO_CVSD_TX_OUTBUF_SIZE (BTSCO_CVSD_TX_FRAME*SCO_TX_ENCODE_SIZE)

#endif
