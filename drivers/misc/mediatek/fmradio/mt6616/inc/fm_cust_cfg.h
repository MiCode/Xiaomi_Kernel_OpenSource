/*
 *
 * (C) Copyright 20011
 * MediaTek <www.MediaTek.com>
 * Hongcheng Xia<Hongcheng.Xia@MediaTek.com>
 *
 * FM Radio Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __FM_CUST_CFG_H__
#define __FM_CUST_CFG_H__

/* scan sort algorithm */
enum {
	FM_SCAN_SORT_NON = 0,
	FM_SCAN_SORT_UP,
	FM_SCAN_SORT_DOWN,
	FM_SCAN_SORT_MAX
};

/* ***************************************************************************************** */
/* ***********************************FM config for customer *********************************** */
/* ***************************************************************************************** */
/* RX */
#define FM_RX_RSSI_TH_LONG    0xFF01	/* FM radio long antenna RSSI threshold(11.375dBuV) */
#define FM_RX_RSSI_TH_SHORT   0xFEE0	/* FM radio short antenna RSSI threshold(-1dBuV) */
#define FM_RX_CQI_TH          0x00E9	/* FM radio Channel quality indicator threshold(0x0000~0x00FF) */
#define FM_RX_MR_TH           0x01BD	/* FM radio MR threshold */
#define FM_RX_SEEK_SPACE      1	/* FM radio seek space,1:100KHZ; 2:200KHZ */
#define FM_RX_SCAN_CH_SIZE    40	/* FM radio scan max channel size */
#define FM_RX_BAND            1	/* FM radio band, 1:87.5MHz~108.0MHz; 2:76.0MHz~90.0MHz; 3:76.0MHz~108.0MHz; 4:special */
#define FM_RX_BAND_FREQ_L     875	/* FM radio special band low freq(Default 87.5MHz) */
#define FM_RX_BAND_FREQ_H     1080	/* FM radio special band high freq(Default 108.0MHz) */
#define FM_RX_SCAN_SORT_SELECT FM_SCAN_SORT_NON
#define FM_RX_FAKE_CH_NUM      1
#define FM_RX_FAKE_CH_RSSI     40
#define FM_RX_FAKE_CH_1        1075
#define FM_RX_FAKE_CH_2        0
#define FM_RX_FAKE_CH_3        0
#define FM_RX_FAKE_CH_4        0
#define FM_RX_FAKE_CH_5        0

/* TX */
#define FM_TX_PWR_LEVEL_MAX  120
#define FM_TX_SCAN_HOLE_LOW  923	/* 92.3MHz~95.4MHz should not show to user */
#define FM_TX_SCAN_HOLE_HIGH 954	/* 92.3MHz~95.4MHz should not show to user */

#endif				/* __FM_CUST_CFG_H__ */
