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

/* SSUSB_USB3_SYS_CSR REGISTER DEFINITION */

#define U3D_LINK_HP_TIMER                         (SSUSB_USB3_SYS_CSR_BASE+0x0200)
#define U3D_LINK_CMD_TIMER                        (SSUSB_USB3_SYS_CSR_BASE+0x0204)
#define U3D_LINK_PM_TIMER                         (SSUSB_USB3_SYS_CSR_BASE+0x0208)
#define U3D_LINK_UX_INACT_TIMER                   (SSUSB_USB3_SYS_CSR_BASE+0x020C)
#define U3D_LINK_POWER_CONTROL                    (SSUSB_USB3_SYS_CSR_BASE+0x0210)
#define U3D_LINK_ERR_COUNT                        (SSUSB_USB3_SYS_CSR_BASE+0x0214)
#define U3D_LTSSM_TRANSITION                      (SSUSB_USB3_SYS_CSR_BASE+0x0218)
#define U3D_LINK_RETRY_CTRL                       (SSUSB_USB3_SYS_CSR_BASE+0x0220)
#define U3D_SYS_FAST_SIMULATIION                  (SSUSB_USB3_SYS_CSR_BASE+0x0224)
#define U3D_LINK_CAPABILITY_CTRL                  (SSUSB_USB3_SYS_CSR_BASE+0x0228)
#define U3D_LINK_DEBUG_INFO                       (SSUSB_USB3_SYS_CSR_BASE+0x022C)
#define U3D_USB3_U1_REJECT                        (SSUSB_USB3_SYS_CSR_BASE+0x0240)
#define U3D_USB3_U2_REJECT                        (SSUSB_USB3_SYS_CSR_BASE+0x0244)
#define U3D_DEV_NOTIF_0                           (SSUSB_USB3_SYS_CSR_BASE+0x0290)
#define U3D_DEV_NOTIF_1                           (SSUSB_USB3_SYS_CSR_BASE+0x0294)
#define U3D_VENDOR_DEV_TEST                       (SSUSB_USB3_SYS_CSR_BASE+0x0298)
#define U3D_VENDOR_DEF_DATA_LOW                   (SSUSB_USB3_SYS_CSR_BASE+0x029C)
#define U3D_VENDOR_DEF_DATA_HIGH                  (SSUSB_USB3_SYS_CSR_BASE+0x02A0)
#define U3D_HOST_SET_PORT_CTRL                    (SSUSB_USB3_SYS_CSR_BASE+0x02A4)
#define U3D_LINK_CAP_CONTROL                      (SSUSB_USB3_SYS_CSR_BASE+0x02AC)
#define U3D_PORT_CONF_TIMEOUT                     (SSUSB_USB3_SYS_CSR_BASE+0x02B0)
#define U3D_TIMING_PULSE_CTRL                     (SSUSB_USB3_SYS_CSR_BASE+0x02B4)
#define U3D_ISO_TIMESTAMP                         (SSUSB_USB3_SYS_CSR_BASE+0x02B8)
#define U3D_RECEIVE_PKT_INTR_EN                   (SSUSB_USB3_SYS_CSR_BASE+0x02C0)
#define U3D_RECEIVE_PKT_INTR                      (SSUSB_USB3_SYS_CSR_BASE+0x02C4)
#define U3D_CRC_ERR_INTR_EN                       (SSUSB_USB3_SYS_CSR_BASE+0x02C8)
#define U3D_CRC_ERR_INTR                          (SSUSB_USB3_SYS_CSR_BASE+0x02CC)
#define U3D_PORT_STATUS_INTR_EN                   (SSUSB_USB3_SYS_CSR_BASE+0x02D0)
#define U3D_PORT_STATUS_INTR                      (SSUSB_USB3_SYS_CSR_BASE+0x02D4)
#define U3D_RECOVERY_COUNT                        (SSUSB_USB3_SYS_CSR_BASE+0x02D8)
#define U3D_T2R_LOOPBACK_TEST                     (SSUSB_USB3_SYS_CSR_BASE+0x02DC)

/* SSUSB_USB3_SYS_CSR FIELD DEFINITION */

/* U3D_LINK_HP_TIMER */
#define CHP_TIMEOUT_VALUE                         (0x7f<<8)	/* 14:8 */
#define PHP_TIMEOUT_VALUE                         (0xf<<0)	/* 3:0 */

/* U3D_LINK_CMD_TIMER */
#define NO_LC_TIMEOUT_VALUE                       (0xf<<8)	/* 11:8 */
#define LDN_TIMEOUT_VALUE                         (0xf<<4)	/* 7:4 */
#define LUP_TIMEOUT_VALUE                         (0xf<<0)	/* 3:0 */

/* U3D_LINK_PM_TIMER */
#define LPMA_SENT_CNT_VALUE                       (0xf<<16)	/* 19:16 */
#define PM_ENTRY_TIMEOUT_VALUE                    (0xf<<8)	/* 11:8 */
#define PM_LC_TIMEOUT_VALUE                       (0xf<<0)	/* 3:0 */

/* U3D_LINK_UX_INACT_TIMER */
#define DEV_U2_INACT_TIMEOUT_VALUE                (0xff<<16)	/* 23:16 */
#define U2_INACT_TIMEOUT_VALUE                    (0xff<<8)	/* 15:8 */
#define U1_INACT_TIMEOUT_VALUE                    (0xff<<0)	/* 7:0 */

/* U3D_LINK_POWER_CONTROL */
#define SW_U2_ACCEPT_ENABLE                       (0x1<<9)	/* 9:9 */
#define SW_U1_ACCEPT_ENABLE                       (0x1<<8)	/* 8:8 */
#define UX_EXIT                                   (0x1<<5)	/* 5:5 */
#define LGO_U3                                    (0x1<<4)	/* 4:4 */
#define LGO_U2                                    (0x1<<3)	/* 3:3 */
#define LGO_U1                                    (0x1<<2)	/* 2:2 */
#define SW_U2_REQUEST_ENABLE                      (0x1<<1)	/* 1:1 */
#define SW_U1_REQUEST_ENABLE                      (0x1<<0)	/* 0:0 */

/* U3D_LINK_ERR_COUNT */
#define CLR_LINK_ERR_CNT                          (0x1<<16)	/* 16:16 */
#define LINK_ERROR_COUNT                          (0xffff<<0)	/* 15:0 */

/* U3D_LTSSM_TRANSITION */
#define GO_HOT_RESET                              (0x1<<3)	/* 3:3 */
#define GO_WARM_RESET                             (0x1<<2)	/* 2:2 */
#define GO_RXDETECT                               (0x1<<1)	/* 1:1 */
#define GO_SS_DISABLE                             (0x1<<0)	/* 0:0 */

/* U3D_LINK_RETRY_CTRL */
#define TX_LRTY_DPP_EN                            (0x1<<0)	/* 0:0 */

/* U3D_SYS_FAST_SIMULATIION */
#define SYS_SPEED_MS_TO_US                        (0x1<<0)	/* 0:0 */

/* U3D_LINK_CAPABILITY_CTRL */
#define INSERT_CRC32_ERR_DP_NUM                   (0xff<<16)	/* 23:16 */
#define INSERT_CRC32_ERR_EN                       (0x1<<8)	/* 8:8 */
#define ZLP_CRC32_CHK_DIS                         (0x1<<0)	/* 0:0 */

/* U3D_LINK_DEBUG_INFO */
#define CLR_TX_DATALEN_OVER_1024                  (0x1<<1)	/* 1:1 */
#define TX_DATALEN_OVER_1024                      (0x1<<0)	/* 0:0 */

/* U3D_USB3_U1_REJECT */
#define CLR_USB3_U1_REJECT_CNT                    (0x1<<16)	/* 16:16 */
#define USB3_U1_REJECT_CNT                        (0xffff<<0)	/* 15:0 */

/* U3D_USB3_U2_REJECT */
#define CLR_USB3_U2_REJECT_CNT                    (0x1<<16)	/* 16:16 */
#define USB3_U2_REJECT_CNT                        (0xffff<<0)	/* 15:0 */

/* U3D_DEV_NOTIF_0 */
#define DEV_NOTIF_TYPE_SPECIFIC_LOW               (0xffffff<<8)	/* 31:8 */
#define DEV_NOTIF_TYPE                            (0xf<<4)	/* 7:4 */
#define SEND_DEV_NOTIF                            (0x1<<0)	/* 0:0 */

/* U3D_DEV_NOTIF_1 */
#define DEV_NOTIF_TYPE_SPECIFIC_HIGH              (0xffffffff<<0)	/* 31:0 */

/* U3D_VENDOR_DEV_TEST */
#define VENDOR_DEV_TEST_VALUE                     (0xff<<16)	/* 23:16 */
#define SEND_VENDOR_DEV_TEST                      (0x1<<0)	/* 0:0 */

/* U3D_VENDOR_DEF_DATA_LOW */
#define VENDOR_DEF_DATA_LOW                       (0xffffffff<<0)	/* 31:0 */

/* U3D_VENDOR_DEF_DATA_HIGH */
#define VENDOR_DEF_DATA_HIGH                      (0xffffffff<<0)	/* 31:0 */

/* U3D_HOST_SET_PORT_CTRL */
#define SEND_U2_INACT_TIMEOUT                     (0x1<<2)	/* 2:2 */
#define FORCE_LINK_PM_ACPT                        (0x1<<1)	/* 1:1 */
#define SEND_SET_LINK_FUNC                        (0x1<<0)	/* 0:0 */

/* U3D_LINK_CAP_CONTROL */
#define TIEBREAKER                                (0xf<<16)	/* 19:16 */
#define NUM_HP_BUF                                (0xff<<8)	/* 15:8 */
#define LINK_SPEED                                (0xff<<0)	/* 7:0 */

/* U3D_PORT_CONF_TIMEOUT */
#define TPORT_CONF_TIMEOUT_VALUE                  (0x1f<<0)	/* 4:0 */

/* U3D_TIMING_PULSE_CTRL */
#define CNT_1MS_VALUE                             (0xf<<28)	/* 31:28 */
#define CNT_100US_VALUE                           (0xf<<24)	/* 27:24 */
#define CNT_10US_VALUE                            (0xf<<20)	/* 23:20 */
#define CNT_1US_VALUE                             (0xff<<0)	/* 7:0 */

/* U3D_ISO_TIMESTAMP */
#define ISO_TIMESTAMP                             (0x7ffffff<<0)	/* 26:0 */

/* U3D_RECEIVE_PKT_INTR_EN */
#define RECV_SET_LINK_FUNC_INTR_EN                (0x1<<2)	/* 2:2 */
#define RECV_U2_INACT_INTR_EN                     (0x1<<1)	/* 1:1 */
#define RECV_ITP_INTR_EN                          (0x1<<0)	/* 0:0 */

/* U3D_RECEIVE_PKT_INTR */
#define RECV_SET_LINK_FUNC_INTR                   (0x1<<2)	/* 2:2 */
#define RECV_U2_INACT_INTR                        (0x1<<1)	/* 1:1 */
#define RECV_ITP_INTR                             (0x1<<0)	/* 0:0 */

/* U3D_CRC_ERR_INTR_EN */
#define CRC16_ERR_INTR_EN                         (0x1<<2)	/* 2:2 */
#define CRC5_ERR_INTR_EN                          (0x1<<1)	/* 1:1 */
#define CRC32_ERR_INTR_EN                         (0x1<<0)	/* 0:0 */

/* U3D_CRC_ERR_INTR */
#define CRC16_ERR_INTR                            (0x1<<2)	/* 2:2 */
#define CRC5_ERR_INTR                             (0x1<<1)	/* 1:1 */
#define CRC32_ERR_INTR                            (0x1<<0)	/* 0:0 */

/* U3D_PORT_STATUS_INTR_EN */
#define LMP_ADV_ERR_INTR_EN                       (0x1<<2)	/* 2:2 */
#define LMP_ADV_DONE_INTR_EN                      (0x1<<1)	/* 1:1 */
#define LINK_ADV_DONE_INTR_EN                     (0x1<<0)	/* 0:0 */

/* U3D_PORT_STATUS_INTR */
#define LMP_ADV_ERR_INTR                          (0x1<<2)	/* 2:2 */
#define LMP_ADV_DONE_INTR                         (0x1<<1)	/* 1:1 */
#define LINK_ADV_DONE_INTR                        (0x1<<0)	/* 0:0 */

/* U3D_RECOVERY_COUNT */
#define CLR_RECOV_CNT                             (0x1<<16)	/* 16:16 */
#define RECOV_CNT                                 (0xffff<<0)	/* 15:0 */

/* U3D_T2R_LOOPBACK_TEST */
#define T2R_LOOPBACK                              (0x1<<0)	/* 0:0 */


/* SSUSB_USB3_SYS_CSR FIELD OFFSET DEFINITION */

/* U3D_LINK_HP_TIMER */
#define CHP_TIMEOUT_VALUE_OFST                    (8)
#define PHP_TIMEOUT_VALUE_OFST                    (0)

/* U3D_LINK_CMD_TIMER */
#define NO_LC_TIMEOUT_VALUE_OFST                  (8)
#define LDN_TIMEOUT_VALUE_OFST                    (4)
#define LUP_TIMEOUT_VALUE_OFST                    (0)

/* U3D_LINK_PM_TIMER */
#define LPMA_SENT_CNT_VALUE_OFST                  (16)
#define PM_ENTRY_TIMEOUT_VALUE_OFST               (8)
#define PM_LC_TIMEOUT_VALUE_OFST                  (0)

/* U3D_LINK_UX_INACT_TIMER */
#define DEV_U2_INACT_TIMEOUT_VALUE_OFST           (16)
#define U2_INACT_TIMEOUT_VALUE_OFST               (8)
#define U1_INACT_TIMEOUT_VALUE_OFST               (0)

/* U3D_LINK_POWER_CONTROL */
#define SW_U2_ACCEPT_ENABLE_OFST                  (9)
#define SW_U1_ACCEPT_ENABLE_OFST                  (8)
#define UX_EXIT_OFST                              (5)
#define LGO_U3_OFST                               (4)
#define LGO_U2_OFST                               (3)
#define LGO_U1_OFST                               (2)
#define SW_U2_REQUEST_ENABLE_OFST                 (1)
#define SW_U1_REQUEST_ENABLE_OFST                 (0)

/* U3D_LINK_ERR_COUNT */
#define CLR_LINK_ERR_CNT_OFST                     (16)
#define LINK_ERROR_COUNT_OFST                     (0)

/* U3D_LTSSM_TRANSITION */
#define GO_HOT_RESET_OFST                         (3)
#define GO_WARM_RESET_OFST                        (2)
#define GO_RXDETECT_OFST                          (1)
#define GO_SS_DISABLE_OFST                        (0)

/* U3D_LINK_RETRY_CTRL */
#define TX_LRTY_DPP_EN_OFST                       (0)

/* U3D_SYS_FAST_SIMULATIION */
#define SYS_SPEED_MS_TO_US_OFST                   (0)

/* U3D_LINK_CAPABILITY_CTRL */
#define INSERT_CRC32_ERR_DP_NUM_OFST              (16)
#define INSERT_CRC32_ERR_EN_OFST                  (8)
#define ZLP_CRC32_CHK_DIS_OFST                    (0)

/* U3D_LINK_DEBUG_INFO */
#define CLR_TX_DATALEN_OVER_1024_OFST             (1)
#define TX_DATALEN_OVER_1024_OFST                 (0)

/* U3D_USB3_U1_REJECT */
#define CLR_USB3_U1_REJECT_CNT_OFST               (16)
#define USB3_U1_REJECT_CNT_OFST                   (0)

/* U3D_USB3_U2_REJECT */
#define CLR_USB3_U2_REJECT_CNT_OFST               (16)
#define USB3_U2_REJECT_CNT_OFST                   (0)

/* U3D_DEV_NOTIF_0 */
#define DEV_NOTIF_TYPE_SPECIFIC_LOW_OFST          (8)
#define DEV_NOTIF_TYPE_OFST                       (4)
#define SEND_DEV_NOTIF_OFST                       (0)

/* U3D_DEV_NOTIF_1 */
#define DEV_NOTIF_TYPE_SPECIFIC_HIGH_OFST         (0)

/* U3D_VENDOR_DEV_TEST */
#define VENDOR_DEV_TEST_VALUE_OFST                (16)
#define SEND_VENDOR_DEV_TEST_OFST                 (0)

/* U3D_VENDOR_DEF_DATA_LOW */
#define VENDOR_DEF_DATA_LOW_OFST                  (0)

/* U3D_VENDOR_DEF_DATA_HIGH */
#define VENDOR_DEF_DATA_HIGH_OFST                 (0)

/* U3D_HOST_SET_PORT_CTRL */
#define SEND_U2_INACT_TIMEOUT_OFST                (2)
#define FORCE_LINK_PM_ACPT_OFST                   (1)
#define SEND_SET_LINK_FUNC_OFST                   (0)

/* U3D_LINK_CAP_CONTROL */
#define TIEBREAKER_OFST                           (16)
#define NUM_HP_BUF_OFST                           (8)
#define LINK_SPEED_OFST                           (0)

/* U3D_PORT_CONF_TIMEOUT */
#define TPORT_CONF_TIMEOUT_VALUE_OFST             (0)

/* U3D_TIMING_PULSE_CTRL */
#define CNT_1MS_VALUE_OFST                        (28)
#define CNT_100US_VALUE_OFST                      (24)
#define CNT_10US_VALUE_OFST                       (20)
#define CNT_1US_VALUE_OFST                        (0)

/* U3D_ISO_TIMESTAMP */
#define ISO_TIMESTAMP_OFST                        (0)

/* U3D_RECEIVE_PKT_INTR_EN */
#define RECV_SET_LINK_FUNC_INTR_EN_OFST           (2)
#define RECV_U2_INACT_INTR_EN_OFST                (1)
#define RECV_ITP_INTR_EN_OFST                     (0)

/* U3D_RECEIVE_PKT_INTR */
#define RECV_SET_LINK_FUNC_INTR_OFST              (2)
#define RECV_U2_INACT_INTR_OFST                   (1)
#define RECV_ITP_INTR_OFST                        (0)

/* U3D_CRC_ERR_INTR_EN */
#define CRC16_ERR_INTR_EN_OFST                    (2)
#define CRC5_ERR_INTR_EN_OFST                     (1)
#define CRC32_ERR_INTR_EN_OFST                    (0)

/* U3D_CRC_ERR_INTR */
#define CRC16_ERR_INTR_OFST                       (2)
#define CRC5_ERR_INTR_OFST                        (1)
#define CRC32_ERR_INTR_OFST                       (0)

/* U3D_PORT_STATUS_INTR_EN */
#define LMP_ADV_ERR_INTR_EN_OFST                  (2)
#define LMP_ADV_DONE_INTR_EN_OFST                 (1)
#define LINK_ADV_DONE_INTR_EN_OFST                (0)

/* U3D_PORT_STATUS_INTR */
#define LMP_ADV_ERR_INTR_OFST                     (2)
#define LMP_ADV_DONE_INTR_OFST                    (1)
#define LINK_ADV_DONE_INTR_OFST                   (0)

/* U3D_RECOVERY_COUNT */
#define CLR_RECOV_CNT_OFST                        (16)
#define RECOV_CNT_OFST                            (0)

/* U3D_T2R_LOOPBACK_TEST */
#define T2R_LOOPBACK_OFST                         (0)

/* //////////////////////////////////////////////////////////////////// */
