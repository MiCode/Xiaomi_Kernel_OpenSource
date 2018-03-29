/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_PD_REG_H
#define _TYPEC_PD_REG_H

#define PD_REG_BASE 0x100

/* PD_REG REGISTER DEFINITION */

#define PD_TX_PARAMETER                           (PD_REG_BASE+0x0004)
#define PD_TX_DATA_OBJECT0_0                      (PD_REG_BASE+0x0010)
#define PD_TX_DATA_OBJECT0_1                      (PD_REG_BASE+0x0012)
#define PD_TX_DATA_OBJECT1_0                      (PD_REG_BASE+0x0014)
#define PD_TX_DATA_OBJECT1_1                      (PD_REG_BASE+0x0016)
#define PD_TX_DATA_OBJECT2_0                      (PD_REG_BASE+0x0018)
#define PD_TX_DATA_OBJECT2_1                      (PD_REG_BASE+0x001A)
#define PD_TX_DATA_OBJECT3_0                      (PD_REG_BASE+0x001C)
#define PD_TX_DATA_OBJECT3_1                      (PD_REG_BASE+0x001E)
#define PD_TX_DATA_OBJECT4_0                      (PD_REG_BASE+0x0020)
#define PD_TX_DATA_OBJECT4_1                      (PD_REG_BASE+0x0022)
#define PD_TX_DATA_OBJECT5_0                      (PD_REG_BASE+0x0024)
#define PD_TX_DATA_OBJECT5_1                      (PD_REG_BASE+0x0026)
#define PD_TX_DATA_OBJECT6_0                      (PD_REG_BASE+0x0028)
#define PD_TX_DATA_OBJECT6_1                      (PD_REG_BASE+0x002A)
#define PD_TX_HEADER                              (PD_REG_BASE+0x002C)
#define PD_TX_CTRL                                (PD_REG_BASE+0x002E)
#define PD_RX_PARAMETER                           (PD_REG_BASE+0x0044)
#define PD_RX_PREAMBLE_PROTECT_PARAMETER_0        (PD_REG_BASE+0x0048)
#define PD_RX_PREAMBLE_PROTECT_PARAMETER_2        (PD_REG_BASE+0x004C)
#define PD_RX_STATUS                              (PD_REG_BASE+0x004E)
#define PD_RX_DATA_OBJECT0_0                      (PD_REG_BASE+0x0050)
#define PD_RX_DATA_OBJECT0_1                      (PD_REG_BASE+0x0052)
#define PD_RX_DATA_OBJECT1_0                      (PD_REG_BASE+0x0054)
#define PD_RX_DATA_OBJECT1_1                      (PD_REG_BASE+0x0056)
#define PD_RX_DATA_OBJECT2_0                      (PD_REG_BASE+0x0058)
#define PD_RX_DATA_OBJECT2_1                      (PD_REG_BASE+0x005A)
#define PD_RX_DATA_OBJECT3_0                      (PD_REG_BASE+0x005C)
#define PD_RX_DATA_OBJECT3_1                      (PD_REG_BASE+0x005E)
#define PD_RX_DATA_OBJECT4_0                      (PD_REG_BASE+0x0060)
#define PD_RX_DATA_OBJECT4_1                      (PD_REG_BASE+0x0062)
#define PD_RX_DATA_OBJECT5_0                      (PD_REG_BASE+0x0064)
#define PD_RX_DATA_OBJECT5_1                      (PD_REG_BASE+0x0066)
#define PD_RX_DATA_OBJECT6_0                      (PD_REG_BASE+0x0068)
#define PD_RX_DATA_OBJECT6_1                      (PD_REG_BASE+0x006A)
#define PD_RX_HEADER                              (PD_REG_BASE+0x006C)
#define PD_RX_1P25X_UI_TRAIN_RESULT               (PD_REG_BASE+0x0070)
#define PD_RX_RCV_BUF_SW_RST                      (PD_REG_BASE+0x0074)
#define PD_HR_CTRL                                (PD_REG_BASE+0x0080)
#define PD_AD_STATUS                              (PD_REG_BASE+0x0084)
#define PD_CRC_RCV_TIMEOUT_VAL_0                  (PD_REG_BASE+0x0090)
#define PD_CRC_RCV_TIMEOUT_VAL_1                  (PD_REG_BASE+0x0092)
#define PD_HR_COMPLETE_TIMEOUT_VAL_0              (PD_REG_BASE+0x0094)
#define PD_HR_COMPLETE_TIMEOUT_VAL_1              (PD_REG_BASE+0x0096)
#define PD_IDLE_DETECTION_0                       (PD_REG_BASE+0x0098)
#define PD_IDLE_DETECTION_1                       (PD_REG_BASE+0x009A)
#define PD_INTERFRAMEGAP_VAL                      (PD_REG_BASE+0x009C)
#define PD_RX_GLITCH_MASK_WINDOW                  (PD_REG_BASE+0x009E)
#define PD_TIMER0_VAL_0                           (PD_REG_BASE+0x00A0)
#define PD_TIMER0_VAL_1                           (PD_REG_BASE+0x00A2)
#define PD_TIMER0_ENABLE                          (PD_REG_BASE+0x00A4)
#define PD_TIMER1_VAL                             (PD_REG_BASE+0x00A8)
#define PD_TIMER1_TICK_CNT                        (PD_REG_BASE+0x00AA)
#define PD_TIMER1_ENABLE                          (PD_REG_BASE+0x00AC)
#define PD_TX_SLEW_RATE_CALI_CTRL                 (PD_REG_BASE+0x00B0)
#define PD_TX_SLEW_CK_STABLE_TIME                 (PD_REG_BASE+0x00B2)
#define PD_TX_MON_CK_TARGET                       (PD_REG_BASE+0x00B4)
#define PD_TX_SLEW_CK_TARGET                      (PD_REG_BASE+0x00B6)
#define PD_TX_SLEW_RATE_CALI_RESULT               (PD_REG_BASE+0x00B8)
#define PD_TX_SLEW_RATE_FM_OUT                    (PD_REG_BASE+0x00BA)
#define PD_LOOPBACK                               (PD_REG_BASE+0x00BC)
#define PD_MSG_ID_SW_MODE                         (PD_REG_BASE+0x00BE)
#define PD_INTR_EN_0                              (PD_REG_BASE+0x00C0)
#define PD_INTR_EN_1                              (PD_REG_BASE+0x00C2)
#define PD_INTR_0                                 (PD_REG_BASE+0x00C8)
#define PD_INTR_1                                 (PD_REG_BASE+0x00CA)
#define PD_PHY_RG_PD_0                            (PD_REG_BASE+0x00F0)
#define PD_PHY_RG_PD_1                            (PD_REG_BASE+0x00F2)
#define PD_PHY_SW_FORCE_MODE_ENABLE               (PD_REG_BASE+0x00F4)
#define PD_PHY_SW_FORCE_MODE_VAL_0                (PD_REG_BASE+0x00F8)

/* PD_REG FIELD DEFINITION */

/* PD_TX_PARAMETER */
#define REG_PD_TX_AUTO_SEND_CR_EN                 (0x1<<14) /* 14:14 */
#define REG_PD_TX_AUTO_SEND_HR_EN                 (0x1<<13) /* 13:13 */
#define REG_PD_TX_AUTO_SEND_SR_EN                 (0x1<<12) /* 12:12 */
#define REG_PD_TX_RETRY_CNT                       (0xf<<8) /* 11:8 */
#define REG_PD_TX_HALF_UI_CYCLE_CNT               (0xff<<0) /* 7:0 */

/* PD_TX_DATA_OBJECT0_0 */
#define REG_PD_TX_DATA_OBJ0_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT0_1 */
#define REG_PD_TX_DATA_OBJ0_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT1_0 */
#define REG_PD_TX_DATA_OBJ1_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT1_1 */
#define REG_PD_TX_DATA_OBJ1_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT2_0 */
#define REG_PD_TX_DATA_OBJ2_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT2_1 */
#define REG_PD_TX_DATA_OBJ2_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT3_0 */
#define REG_PD_TX_DATA_OBJ3_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT3_1 */
#define REG_PD_TX_DATA_OBJ3_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT4_0 */
#define REG_PD_TX_DATA_OBJ4_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT4_1 */
#define REG_PD_TX_DATA_OBJ4_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT5_0 */
#define REG_PD_TX_DATA_OBJ5_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT5_1 */
#define REG_PD_TX_DATA_OBJ5_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT6_0 */
#define REG_PD_TX_DATA_OBJ6_0                     (0xffff<<0) /* 15:0 */

/* PD_TX_DATA_OBJECT6_1 */
#define REG_PD_TX_DATA_OBJ6_1                     (0xffff<<0) /* 15:0 */

/* PD_TX_HEADER */
#define REG_PD_TX_HDR_CABLE_PLUG                  (0x1<<15) /* 15:15 */
#define REG_PD_TX_HDR_NUM_DATA_OBJ                (0x7<<12) /* 14:12 */
#define REG_PD_TX_HDR_PORT_POWER_ROLE             (0x1<<8) /* 8:8 */
#define REG_PD_TX_HDR_SPEC_VER                    (0x3<<6) /* 7:6 */
#define REG_PD_TX_HDR_PORT_DATA_ROLE              (0x1<<5) /* 5:5 */
#define REG_PD_TX_HDR_MSG_TYPE                    (0xf<<0) /* 3:0 */

/* PD_TX_CTRL */
#define REG_PD_TX_OS                              (0x7<<4) /* 6:4 */
#define PD_TX_BIST_CARRIER_MODE2_START            (0x1<<1) /* 1:1 */
#define PD_TX_START                               (0x1<<0) /* 0:0 */

/* PD_RX_PARAMETER */
#define REG_PD_RX_PING_MSG_RCV_EN                 (0x1<<8) /* 8:8 */
#define REG_PD_RX_PRE_TRAIN_BIT_CNT               (0x3<<6) /* 7:6 */
#define REG_PD_RX_PRE_PROTECT_EN                  (0x1<<5) /* 5:5 */
#define REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT (0x1<<4) /* 4:4 */
#define REG_PD_RX_CABLE_RST_RCV_EN                (0x1<<3) /* 3:3 */
#define REG_PD_RX_SOP_DPRIME_RCV_EN               (0x1<<2) /* 2:2 */
#define REG_PD_RX_SOP_PRIME_RCV_EN                (0x1<<1) /* 1:1 */
#define REG_PD_RX_EN                              (0x1<<0) /* 0:0 */

/* PD_RX_PREAMBLE_PROTECT_PARAMETER_0 */
#define REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN (0xff<<0) /* 7:0 */

/* PD_RX_PREAMBLE_PROTECT_PARAMETER_2 */
#define REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX    (0x1ff<<0) /* 8:0 */

/* PD_RX_STATUS */
#define RO_PD_RX_OS                               (0x7<<8) /* 10:8 */

/* PD_RX_DATA_OBJECT0_0 */
#define RO_PD_RX_DATA_OBJ0_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT0_1 */
#define RO_PD_RX_DATA_OBJ0_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT1_0 */
#define RO_PD_RX_DATA_OBJ1_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT1_1 */
#define RO_PD_RX_DATA_OBJ1_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT2_0 */
#define RO_PD_RX_DATA_OBJ2_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT2_1 */
#define RO_PD_RX_DATA_OBJ2_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT3_0 */
#define RO_PD_RX_DATA_OBJ3_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT3_1 */
#define RO_PD_RX_DATA_OBJ3_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT4_0 */
#define RO_PD_RX_DATA_OBJ4_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT4_1 */
#define RO_PD_RX_DATA_OBJ4_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT5_0 */
#define RO_PD_RX_DATA_OBJ5_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT5_1 */
#define RO_PD_RX_DATA_OBJ5_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT6_0 */
#define RO_PD_RX_DATA_OBJ6_0                      (0xffff<<0) /* 15:0 */

/* PD_RX_DATA_OBJECT6_1 */
#define RO_PD_RX_DATA_OBJ6_1                      (0xffff<<0) /* 15:0 */

/* PD_RX_HEADER */
#define RO_PD_RX_HDR                              (0xffff<<0) /* 15:0 */

/* PD_RX_1P25X_UI_TRAIN_RESULT */
#define RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X       (0x3ff<<0) /* 9:0 */

/* PD_RX_RCV_BUF_SW_RST */
#define PD_PRL_RCV_FIFO_SW_RST                    (0x1<<0) /* 0:0 */

/* PD_HR_CTRL */
#define W1_PD_PE_CR_CPL                           (0x1<<1) /* 1:1 */
#define W1_PD_PE_HR_CPL                           (0x1<<0) /* 0:0 */

/* PD_AD_STATUS */
#define RO_PD_AD_PD_CC2_OVP_STATUS                (0x1<<2) /* 2:2 */
#define RO_PD_AD_PD_CC1_OVP_STATUS                (0x1<<1) /* 1:1 */
#define RO_PD_AD_PD_VCONN_UVP_STATUS              (0x1<<0) /* 0:0 */

/* PD_CRC_RCV_TIMEOUT_VAL_0 */
#define REG_PD_CRC_RCV_TIMEOUT_VAL_0              (0xffff<<0) /* 15:0 */

/* PD_CRC_RCV_TIMEOUT_VAL_1 */
#define REG_PD_CRC_RCV_TIMEOUT_VAL_1              (0xffff<<0) /* 15:0 */

/* PD_HR_COMPLETE_TIMEOUT_VAL_0 */
#define REG_PD_HR_CPL_TIMEOUT_VAL_0               (0xffff<<0) /* 15:0 */

/* PD_HR_COMPLETE_TIMEOUT_VAL_1 */
#define REG_PD_HR_CPL_TIMEOUT_VAL_1               (0xffff<<0) /* 15:0 */

/* PD_IDLE_DETECTION_0 */
#define REG_PD_IDLE_DET_WIN_VAL                   (0xffff<<0) /* 15:0 */

/* PD_IDLE_DETECTION_1 */
#define RO_PD_IDLE_DET_STATUS                     (0x1<<8) /* 8:8 */
#define REG_PD_IDLE_DET_TRANS_CNT                 (0x7<<0) /* 2:0 */

/* PD_INTERFRAMEGAP_VAL */
#define REG_PD_INTERFRAMEGAP_VAL                  (0xffff<<0) /* 15:0 */

/* PD_RX_GLITCH_MASK_WINDOW */
#define REG_PD_RX_UI_1P25X_ADJ                    (0x1<<12) /* 12:12 */
#define REG_PD_RX_UI_1P25X_ADJ_CNT                (0xf<<8) /* 11:8 */
#define REG_PD_RX_GLITCH_MASK_WIN_VAL             (0x3f<<0) /* 5:0 */

/* PD_TIMER0_VAL_0 */
#define REG_PD_TIMER0_VAL_0                       (0xffff<<0) /* 15:0 */

/* PD_TIMER0_VAL_1 */
#define REG_PD_TIMER0_VAL_1                       (0xffff<<0) /* 15:0 */

/* PD_TIMER0_ENABLE */
#define PD_TIMER0_EN                              (0x1<<0) /* 0:0 */

/* PD_TIMER1_VAL */
#define REG_PD_TIMER1_VAL_0                       (0xffff<<0) /* 15:0 */

/* PD_TIMER1_TICK_CNT */
#define RO_PD_TIMER1_TICK_CNT                     (0xffff<<0) /* 15:0 */

/* PD_TIMER1_ENABLE */
#define PD_TIMER1_EN                              (0x1<<0) /* 0:0 */

/* PD_TX_SLEW_RATE_CALI_CTRL */
#define REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT       (0xf<<4) /* 7:4 */
#define REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN         (0x1<<1) /* 1:1 */
#define REG_PD_TX_SLEW_CALI_AUTO_EN               (0x1<<0) /* 0:0 */

/* PD_TX_SLEW_CK_STABLE_TIME */
#define REG_PD_TX_SLEW_CK_STABLE_TIME             (0xfff<<0) /* 11:0 */

/* PD_TX_MON_CK_TARGET */
#define REG_PD_TX_MON_CK_TARGET_CYC_CNT           (0xffff<<0) /* 15:0 */

/* PD_TX_SLEW_CK_TARGET */
#define REG_PD_TX_SLEW_CK_TARGET_CYC_CNT          (0xff<<0) /* 7:0 */

/* PD_TX_SLEW_RATE_CALI_RESULT */
#define RO_PD_TXSLEW_I_CALI                       (0x7<<4) /* 6:4 */
#define RO_PD_TX_SLEW_CALI_FAIL                   (0x1<<1) /* 1:1 */
#define RO_PD_TX_SLEW_CALI_OK                     (0x1<<0) /* 0:0 */

/* PD_TX_SLEW_RATE_FM_OUT */
#define RO_PD_FM_OUT                              (0xffff<<0) /* 15:0 */

/* PD_LOOPBACK */
#define RO_PD_LB_ERR_CNT                          (0xff<<8) /* 15:8 */
#define RO_PD_LB_OK                               (0x1<<5) /* 5:5 */
#define RO_PD_LB_RUN                              (0x1<<4) /* 4:4 */
#define PD_LB_CHK_EN                              (0x1<<1) /* 1:1 */
#define PD_LB_EN                                  (0x1<<0) /* 0:0 */

/* PD_MSG_ID_SW_MODE */
#define PD_SW_MSG_ID_SYNC                         (0x1<<4) /* 4:4 */
#define REG_PD_SW_MSG_ID                          (0x7<<0) /* 2:0 */

/* PD_INTR_EN_0 */
#define REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN         (0x1<<15) /* 15:15 */
#define REG_PD_RX_DUPLICATE_INTR_EN               (0x1<<14) /* 14:14 */
#define REG_PD_RX_LENGTH_MIS_INTR_EN              (0x1<<13) /* 13:13 */
#define REG_PD_RX_RCV_MSG_INTR_EN                 (0x1<<12) /* 12:12 */
#define REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN (0x1<<9) /* 9:9 */
#define REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN (0x1<<8) /* 8:8 */
#define REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN       (0x1<<7) /* 7:7 */
#define REG_PD_TX_AUTO_SR_DONE_INTR_EN            (0x1<<6) /* 6:6 */
#define REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN         (0x1<<5) /* 5:5 */
#define REG_PD_TX_DIS_BUS_REIDLE_INTR_EN          (0x1<<4) /* 4:4 */
#define REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN (0x1<<3) /* 3:3 */
#define REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN (0x1<<2) /* 2:2 */
#define REG_PD_TX_RETRY_ERR_INTR_EN               (0x1<<1) /* 1:1 */
#define REG_PD_TX_DONE_INTR_EN                    (0x1<<0) /* 0:0 */

/* PD_INTR_EN_1 */
#define REG_PD_TIMER1_TIMEOUT_INTR_EN             (0x1<<13) /* 13:13 */
#define REG_PD_TIMER0_TIMEOUT_INTR_EN             (0x1<<12) /* 12:12 */
#define REG_PD_AD_PD_CC2_OVP_INTR_EN              (0x1<<10) /* 10:10 */
#define REG_PD_AD_PD_CC1_OVP_INTR_EN              (0x1<<9) /* 9:9 */
#define REG_PD_AD_PD_VCONN_UVP_INTR_EN            (0x1<<8) /* 8:8 */
#define REG_PD_HR_TRANS_FAIL_INTR_EN              (0x1<<3) /* 3:3 */
#define REG_PD_HR_RCV_DONE_INTR_EN                (0x1<<2) /* 2:2 */
#define REG_PD_HR_TRANS_DONE_INTR_EN              (0x1<<1) /* 1:1 */
#define REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN       (0x1<<0) /* 0:0 */

/* PD_INTR_0 */
#define PD_RX_TRANS_GCRC_FAIL_INTR                (0x1<<15) /* 15:15 */
#define PD_RX_DUPLICATE_INTR                      (0x1<<14) /* 14:14 */
#define PD_RX_LENGTH_MIS_INTR                     (0x1<<13) /* 13:13 */
#define PD_RX_RCV_MSG_INTR                        (0x1<<12) /* 12:12 */
#define PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR (0x1<<9) /* 9:9 */
#define PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR (0x1<<8) /* 8:8 */
#define PD_TX_AUTO_SR_RETRY_ERR_INTR              (0x1<<7) /* 7:7 */
#define PD_TX_AUTO_SR_DONE_INTR                   (0x1<<6) /* 6:6 */
#define PD_TX_CRC_RCV_TIMEOUT_INTR                (0x1<<5) /* 5:5 */
#define PD_TX_DIS_BUS_REIDLE_INTR                 (0x1<<4) /* 4:4 */
#define PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR      (0x1<<3) /* 3:3 */
#define PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR        (0x1<<2) /* 2:2 */
#define PD_TX_RETRY_ERR_INTR                      (0x1<<1) /* 1:1 */
#define PD_TX_DONE_INTR                           (0x1<<0) /* 0:0 */

/* PD_INTR_1 */
#define PD_TIMER1_TIMEOUT_INTR                    (0x1<<13) /* 13:13 */
#define PD_TIMER0_TIMEOUT_INTR                    (0x1<<12) /* 12:12 */
#define PD_AD_PD_CC2_OVP_INTR                     (0x1<<10) /* 10:10 */
#define PD_AD_PD_CC1_OVP_INTR                     (0x1<<9) /* 9:9 */
#define PD_AD_PD_VCONN_UVP_INTR                   (0x1<<8) /* 8:8 */
#define PD_HR_TRANS_FAIL_INTR                     (0x1<<3) /* 3:3 */
#define PD_HR_RCV_DONE_INTR                       (0x1<<2) /* 2:2 */
#define PD_HR_TRANS_DONE_INTR                     (0x1<<1) /* 1:1 */
#define PD_HR_TRANS_CPL_TIMEOUT_INTR              (0x1<<0) /* 0:0 */

/* PD_PHY_RG_PD_0 */
#define REG_TYPE_C_PHY_RG_PD_UVP_SEL              (0x1<<12) /* 12:12 */
#define REG_TYPE_C_PHY_RG_PD_UVP_EN               (0x1<<11) /* 11:11 */
#define REG_TYPE_C_PHY_RG_PD_UVP_VTH              (0x7<<8) /* 10:8 */
#define REG_TYPE_C_PHY_RG_PD_RX_MODE              (0x3<<2) /* 3:2 */
#define REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN         (0x1<<1) /* 1:1 */
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN         (0x1<<0) /* 0:0 */

/* PD_PHY_RG_PD_1 */
#define REG_TYPE_C_PHY_RG_PD_TX_AMP               (0xf<<8) /* 11:8 */
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_I             (0x7<<0) /* 2:0 */

/* PD_PHY_SW_FORCE_MODE_ENABLE */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW  (0x1<<5) /* 5:5 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW    (0x1<<4) /* 4:4 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN   (0x1<<3) /* 3:3 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA (0x1<<2) /* 2:2 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN   (0x1<<1) /* 1:1 */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN (0x1<<0) /* 0:0 */

/* PD_PHY_SW_FORCE_MODE_VAL_0 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW     (0x3<<6) /* 7:6 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW       (0x3<<4) /* 5:4 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN      (0x1<<3) /* 3:3 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA    (0x1<<2) /* 2:2 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN      (0x1<<1) /* 1:1 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN    (0x1<<0) /* 0:0 */


/* PD_REG FIELD OFFSET DEFINITION */

/* PD_TX_PARAMETER */
#define REG_PD_TX_AUTO_SEND_CR_EN_OFST            (14)
#define REG_PD_TX_AUTO_SEND_HR_EN_OFST            (13)
#define REG_PD_TX_AUTO_SEND_SR_EN_OFST            (12)
#define REG_PD_TX_RETRY_CNT_OFST                  (8)
#define REG_PD_TX_HALF_UI_CYCLE_CNT_OFST          (0)

/* PD_TX_DATA_OBJECT0_0 */
#define REG_PD_TX_DATA_OBJ0_0_OFST                (0)

/* PD_TX_DATA_OBJECT0_1 */
#define REG_PD_TX_DATA_OBJ0_1_OFST                (0)

/* PD_TX_DATA_OBJECT1_0 */
#define REG_PD_TX_DATA_OBJ1_0_OFST                (0)

/* PD_TX_DATA_OBJECT1_1 */
#define REG_PD_TX_DATA_OBJ1_1_OFST                (0)

/* PD_TX_DATA_OBJECT2_0 */
#define REG_PD_TX_DATA_OBJ2_0_OFST                (0)

/* PD_TX_DATA_OBJECT2_1 */
#define REG_PD_TX_DATA_OBJ2_1_OFST                (0)

/* PD_TX_DATA_OBJECT3_0 */
#define REG_PD_TX_DATA_OBJ3_0_OFST                (0)

/* PD_TX_DATA_OBJECT3_1 */
#define REG_PD_TX_DATA_OBJ3_1_OFST                (0)

/* PD_TX_DATA_OBJECT4_0 */
#define REG_PD_TX_DATA_OBJ4_0_OFST                (0)

/* PD_TX_DATA_OBJECT4_1 */
#define REG_PD_TX_DATA_OBJ4_1_OFST                (0)

/* PD_TX_DATA_OBJECT5_0 */
#define REG_PD_TX_DATA_OBJ5_0_OFST                (0)

/* PD_TX_DATA_OBJECT5_1 */
#define REG_PD_TX_DATA_OBJ5_1_OFST                (0)

/* PD_TX_DATA_OBJECT6_0 */
#define REG_PD_TX_DATA_OBJ6_0_OFST                (0)

/* PD_TX_DATA_OBJECT6_1 */
#define REG_PD_TX_DATA_OBJ6_1_OFST                (0)

/* PD_TX_HEADER */
#define REG_PD_TX_HDR_CABLE_PLUG_OFST             (15)
#define REG_PD_TX_HDR_NUM_DATA_OBJ_OFST           (12)
#define REG_PD_TX_HDR_PORT_POWER_ROLE_OFST        (8)
#define REG_PD_TX_HDR_SPEC_VER_OFST               (6)
#define REG_PD_TX_HDR_PORT_DATA_ROLE_OFST         (5)
#define REG_PD_TX_HDR_MSG_TYPE_OFST               (0)

/* PD_TX_CTRL */
#define REG_PD_TX_OS_OFST                         (4)
#define PD_TX_BIST_CARRIER_MODE2_START_OFST       (1)
#define PD_TX_START_OFST                          (0)

/* PD_RX_PARAMETER */
#define REG_PD_RX_PING_MSG_RCV_EN_OFST            (8)
#define REG_PD_RX_PRE_TRAIN_BIT_CNT_OFST          (6)
#define REG_PD_RX_PRE_PROTECT_EN_OFST             (5)
#define REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT_OFST (4)
#define REG_PD_RX_CABLE_RST_RCV_EN_OFST           (3)
#define REG_PD_RX_SOP_DPRIME_RCV_EN_OFST          (2)
#define REG_PD_RX_SOP_PRIME_RCV_EN_OFST           (1)
#define REG_PD_RX_EN_OFST                         (0)

/* PD_RX_PREAMBLE_PROTECT_PARAMETER_0 */
#define REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN_OFST (0)

/* PD_RX_PREAMBLE_PROTECT_PARAMETER_2 */
#define REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_OFST (0)

/* PD_RX_STATUS */
#define RO_PD_RX_OS_OFST                          (8)

/* PD_RX_DATA_OBJECT0_0 */
#define RO_PD_RX_DATA_OBJ0_0_OFST                 (0)

/* PD_RX_DATA_OBJECT0_1 */
#define RO_PD_RX_DATA_OBJ0_1_OFST                 (0)

/* PD_RX_DATA_OBJECT1_0 */
#define RO_PD_RX_DATA_OBJ1_0_OFST                 (0)

/* PD_RX_DATA_OBJECT1_1 */
#define RO_PD_RX_DATA_OBJ1_1_OFST                 (0)

/* PD_RX_DATA_OBJECT2_0 */
#define RO_PD_RX_DATA_OBJ2_0_OFST                 (0)

/* PD_RX_DATA_OBJECT2_1 */
#define RO_PD_RX_DATA_OBJ2_1_OFST                 (0)

/* PD_RX_DATA_OBJECT3_0 */
#define RO_PD_RX_DATA_OBJ3_0_OFST                 (0)

/* PD_RX_DATA_OBJECT3_1 */
#define RO_PD_RX_DATA_OBJ3_1_OFST                 (0)

/* PD_RX_DATA_OBJECT4_0 */
#define RO_PD_RX_DATA_OBJ4_0_OFST                 (0)

/* PD_RX_DATA_OBJECT4_1 */
#define RO_PD_RX_DATA_OBJ4_1_OFST                 (0)

/* PD_RX_DATA_OBJECT5_0 */
#define RO_PD_RX_DATA_OBJ5_0_OFST                 (0)

/* PD_RX_DATA_OBJECT5_1 */
#define RO_PD_RX_DATA_OBJ5_1_OFST                 (0)

/* PD_RX_DATA_OBJECT6_0 */
#define RO_PD_RX_DATA_OBJ6_0_OFST                 (0)

/* PD_RX_DATA_OBJECT6_1 */
#define RO_PD_RX_DATA_OBJ6_1_OFST                 (0)

/* PD_RX_HEADER */
#define RO_PD_RX_HDR_OFST                         (0)

/* PD_RX_1P25X_UI_TRAIN_RESULT */
#define RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_OFST  (0)

/* PD_RX_RCV_BUF_SW_RST */
#define PD_PRL_RCV_FIFO_SW_RST_OFST               (0)

/* PD_HR_CTRL */
#define W1_PD_PE_CR_CPL_OFST                      (1)
#define W1_PD_PE_HR_CPL_OFST                      (0)

/* PD_AD_STATUS */
#define RO_PD_AD_PD_CC2_OVP_STATUS_OFST           (2)
#define RO_PD_AD_PD_CC1_OVP_STATUS_OFST           (1)
#define RO_PD_AD_PD_VCONN_UVP_STATUS_OFST         (0)

/* PD_CRC_RCV_TIMEOUT_VAL_0 */
#define REG_PD_CRC_RCV_TIMEOUT_VAL_0_OFST         (0)

/* PD_CRC_RCV_TIMEOUT_VAL_1 */
#define REG_PD_CRC_RCV_TIMEOUT_VAL_1_OFST         (0)

/* PD_HR_COMPLETE_TIMEOUT_VAL_0 */
#define REG_PD_HR_CPL_TIMEOUT_VAL_0_OFST          (0)

/* PD_HR_COMPLETE_TIMEOUT_VAL_1 */
#define REG_PD_HR_CPL_TIMEOUT_VAL_1_OFST          (0)

/* PD_IDLE_DETECTION_0 */
#define REG_PD_IDLE_DET_WIN_VAL_OFST              (0)

/* PD_IDLE_DETECTION_1 */
#define RO_PD_IDLE_DET_STATUS_OFST                (8)
#define REG_PD_IDLE_DET_TRANS_CNT_OFST            (0)

/* PD_INTERFRAMEGAP_VAL */
#define REG_PD_INTERFRAMEGAP_VAL_OFST             (0)

/* PD_RX_GLITCH_MASK_WINDOW */
#define REG_PD_RX_UI_1P25X_ADJ_OFST               (12)
#define REG_PD_RX_UI_1P25X_ADJ_CNT_OFST           (8)
#define REG_PD_RX_GLITCH_MASK_WIN_VAL_OFST        (0)

/* PD_TIMER0_VAL_0 */
#define REG_PD_TIMER0_VAL_0_OFST                  (0)

/* PD_TIMER0_VAL_1 */
#define REG_PD_TIMER0_VAL_1_OFST                  (0)

/* PD_TIMER0_ENABLE */
#define PD_TIMER0_EN_OFST                         (0)

/* PD_TIMER1_VAL */
#define REG_PD_TIMER1_VAL_0_OFST                  (0)

/* PD_TIMER1_TICK_CNT */
#define RO_PD_TIMER1_TICK_CNT_OFST                (0)

/* PD_TIMER1_ENABLE */
#define PD_TIMER1_EN_OFST                         (0)

/* PD_TX_SLEW_RATE_CALI_CTRL */
#define REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT_OFST  (4)
#define REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN_OFST    (1)
#define REG_PD_TX_SLEW_CALI_AUTO_EN_OFST          (0)

/* PD_TX_SLEW_CK_STABLE_TIME */
#define REG_PD_TX_SLEW_CK_STABLE_TIME_OFST        (0)

/* PD_TX_MON_CK_TARGET */
#define REG_PD_TX_MON_CK_TARGET_CYC_CNT_OFST      (0)

/* PD_TX_SLEW_CK_TARGET */
#define REG_PD_TX_SLEW_CK_TARGET_CYC_CNT_OFST     (0)

/* PD_TX_SLEW_RATE_CALI_RESULT */
#define RO_PD_TXSLEW_I_CALI_OFST                  (4)
#define RO_PD_TX_SLEW_CALI_FAIL_OFST              (1)
#define RO_PD_TX_SLEW_CALI_OK_OFST                (0)

/* PD_TX_SLEW_RATE_FM_OUT */
#define RO_PD_FM_OUT_OFST                         (0)

/* PD_LOOPBACK */
#define RO_PD_LB_ERR_CNT_OFST                     (8)
#define RO_PD_LB_OK_OFST                          (5)
#define RO_PD_LB_RUN_OFST                         (4)
#define PD_LB_CHK_EN_OFST                         (1)
#define PD_LB_EN_OFST                             (0)

/* PD_MSG_ID_SW_MODE */
#define PD_SW_MSG_ID_SYNC_OFST                    (4)
#define REG_PD_SW_MSG_ID_OFST                     (0)

/* PD_INTR_EN_0 */
#define REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN_OFST    (15)
#define REG_PD_RX_DUPLICATE_INTR_EN_OFST          (14)
#define REG_PD_RX_LENGTH_MIS_INTR_EN_OFST         (13)
#define REG_PD_RX_RCV_MSG_INTR_EN_OFST            (12)
#define REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_OFST (9)
#define REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_OFST (8)
#define REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN_OFST  (7)
#define REG_PD_TX_AUTO_SR_DONE_INTR_EN_OFST       (6)
#define REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN_OFST    (5)
#define REG_PD_TX_DIS_BUS_REIDLE_INTR_EN_OFST     (4)
#define REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_OFST (3)
#define REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_OFST (2)
#define REG_PD_TX_RETRY_ERR_INTR_EN_OFST          (1)
#define REG_PD_TX_DONE_INTR_EN_OFST               (0)

/* PD_INTR_EN_1 */
#define REG_PD_TIMER1_TIMEOUT_INTR_EN_OFST        (13)
#define REG_PD_TIMER0_TIMEOUT_INTR_EN_OFST        (12)
#define REG_PD_AD_PD_CC2_OVP_INTR_EN_OFST         (10)
#define REG_PD_AD_PD_CC1_OVP_INTR_EN_OFST         (9)
#define REG_PD_AD_PD_VCONN_UVP_INTR_EN_OFST       (8)
#define REG_PD_HR_TRANS_FAIL_INTR_EN_OFST         (3)
#define REG_PD_HR_RCV_DONE_INTR_EN_OFST           (2)
#define REG_PD_HR_TRANS_DONE_INTR_EN_OFST         (1)
#define REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN_OFST  (0)

/* PD_INTR_0 */
#define PD_RX_TRANS_GCRC_FAIL_INTR_OFST           (15)
#define PD_RX_DUPLICATE_INTR_OFST                 (14)
#define PD_RX_LENGTH_MIS_INTR_OFST                (13)
#define PD_RX_RCV_MSG_INTR_OFST                   (12)
#define PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_OFST (9)
#define PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_OFST (8)
#define PD_TX_AUTO_SR_RETRY_ERR_INTR_OFST         (7)
#define PD_TX_AUTO_SR_DONE_INTR_OFST              (6)
#define PD_TX_CRC_RCV_TIMEOUT_INTR_OFST           (5)
#define PD_TX_DIS_BUS_REIDLE_INTR_OFST            (4)
#define PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_OFST (3)
#define PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_OFST   (2)
#define PD_TX_RETRY_ERR_INTR_OFST                 (1)
#define PD_TX_DONE_INTR_OFST                      (0)

/* PD_INTR_1 */
#define PD_TIMER1_TIMEOUT_INTR_OFST               (13)
#define PD_TIMER0_TIMEOUT_INTR_OFST               (12)
#define PD_AD_PD_CC2_OVP_INTR_OFST                (10)
#define PD_AD_PD_CC1_OVP_INTR_OFST                (9)
#define PD_AD_PD_VCONN_UVP_INTR_OFST              (8)
#define PD_HR_TRANS_FAIL_INTR_OFST                (3)
#define PD_HR_RCV_DONE_INTR_OFST                  (2)
#define PD_HR_TRANS_DONE_INTR_OFST                (1)
#define PD_HR_TRANS_CPL_TIMEOUT_INTR_OFST         (0)

/* PD_PHY_RG_PD_0 */
#define REG_TYPE_C_PHY_RG_PD_UVP_SEL_OFST         (12)
#define REG_TYPE_C_PHY_RG_PD_UVP_EN_OFST          (11)
#define REG_TYPE_C_PHY_RG_PD_UVP_VTH_OFST         (8)
#define REG_TYPE_C_PHY_RG_PD_RX_MODE_OFST         (2)
#define REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN_OFST    (1)
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN_OFST    (0)

/* PD_PHY_RG_PD_1 */
#define REG_TYPE_C_PHY_RG_PD_TX_AMP_OFST          (8)
#define REG_TYPE_C_PHY_RG_PD_TXSLEW_I_OFST        (0)

/* PD_PHY_SW_FORCE_MODE_ENABLE */
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW_OFST (5)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW_OFST (4)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN_OFST (3)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA_OFST (2)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN_OFST (1)
#define REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN_OFST (0)

/* PD_PHY_SW_FORCE_MODE_VAL_0 */
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW_OFST (6)
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW_OFST  (4)
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN_OFST (3)
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA_OFST (2)
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN_OFST (1)
#define REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN_OFST (0)

#endif
