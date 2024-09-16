/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __CONN_PTA6_REGS_H__
#define __CONN_PTA6_REGS_H__

#define CONN_PTA6_BASE                                         0x1800C000

#define CONN_PTA6_WFSET_PTA_CTRL_ADDR                          (CONN_PTA6_BASE + 0x0000)
#define CONN_PTA6_PTA_CLK_CFG_ADDR                             (CONN_PTA6_BASE + 0x0004)
#define CONN_PTA6_BTSET_PTA_CTRL_ADDR                          (CONN_PTA6_BASE + 0x0008)
#define CONN_PTA6_RO_PTA_CTRL_ADDR                             (CONN_PTA6_BASE + 0x000C)
#define CONN_PTA6_GPS_BLANK_CFG_ADDR                           (CONN_PTA6_BASE + 0x0240)
#define CONN_PTA6_TMR_CTRL_1_ADDR                              (CONN_PTA6_BASE + 0x02C4)
#define CONN_PTA6_TMR_CTRL_3_ADDR                              (CONN_PTA6_BASE + 0x02CC)


#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_uart_apb_hw_en_ADDR   CONN_PTA6_WFSET_PTA_CTRL_ADDR
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_uart_apb_hw_en_MASK   0x00010000
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_uart_apb_hw_en_SHFT   16
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_lte_pta_en_ADDR       CONN_PTA6_WFSET_PTA_CTRL_ADDR
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_lte_pta_en_MASK       0x000000FC
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_lte_pta_en_SHFT       2
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_en_pta_arb_ADDR       CONN_PTA6_WFSET_PTA_CTRL_ADDR
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_en_pta_arb_MASK       0x00000002
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_en_pta_arb_SHFT       1
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_pta_en_ADDR           CONN_PTA6_WFSET_PTA_CTRL_ADDR
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_pta_en_MASK           0x00000001
#define CONN_PTA6_WFSET_PTA_CTRL_r_wfset_pta_en_SHFT           0

#define CONN_PTA6_PTA_CLK_CFG_r_pta_1m_cnt_ADDR                CONN_PTA6_PTA_CLK_CFG_ADDR
#define CONN_PTA6_PTA_CLK_CFG_r_pta_1m_cnt_MASK                0x000000FF
#define CONN_PTA6_PTA_CLK_CFG_r_pta_1m_cnt_SHFT                0

#define CONN_PTA6_BTSET_PTA_CTRL_r_btset_uart_apb_hw_en_ADDR   CONN_PTA6_BTSET_PTA_CTRL_ADDR
#define CONN_PTA6_BTSET_PTA_CTRL_r_btset_uart_apb_hw_en_MASK   0x00010000
#define CONN_PTA6_BTSET_PTA_CTRL_r_btset_uart_apb_hw_en_SHFT   16

#define CONN_PTA6_RO_PTA_CTRL_ro_uart_apb_hw_en_ADDR           CONN_PTA6_RO_PTA_CTRL_ADDR
#define CONN_PTA6_RO_PTA_CTRL_ro_uart_apb_hw_en_MASK           0x00010000
#define CONN_PTA6_RO_PTA_CTRL_ro_uart_apb_hw_en_SHFT           16
#define CONN_PTA6_RO_PTA_CTRL_ro_en_pta_arb_ADDR               CONN_PTA6_RO_PTA_CTRL_ADDR
#define CONN_PTA6_RO_PTA_CTRL_ro_en_pta_arb_MASK               0x00000002
#define CONN_PTA6_RO_PTA_CTRL_ro_en_pta_arb_SHFT               1
#define CONN_PTA6_RO_PTA_CTRL_ro_pta_en_ADDR                   CONN_PTA6_RO_PTA_CTRL_ADDR
#define CONN_PTA6_RO_PTA_CTRL_ro_pta_en_MASK                   0x00000001
#define CONN_PTA6_RO_PTA_CTRL_ro_pta_en_SHFT                   0

#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l5_blank_src_ADDR    CONN_PTA6_GPS_BLANK_CFG_ADDR
#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l5_blank_src_MASK    0x00000018
#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l5_blank_src_SHFT    3
#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l1_blank_src_ADDR    CONN_PTA6_GPS_BLANK_CFG_ADDR
#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l1_blank_src_MASK    0x00000006
#define CONN_PTA6_GPS_BLANK_CFG_r_idc_gps_l1_blank_src_SHFT    1
#define CONN_PTA6_GPS_BLANK_CFG_r_gps_blank_src_ADDR           CONN_PTA6_GPS_BLANK_CFG_ADDR
#define CONN_PTA6_GPS_BLANK_CFG_r_gps_blank_src_MASK           0x00000001
#define CONN_PTA6_GPS_BLANK_CFG_r_gps_blank_src_SHFT           0

#define CONN_PTA6_TMR_CTRL_1_r_idc_2nd_byte_tmout_ADDR         CONN_PTA6_TMR_CTRL_1_ADDR
#define CONN_PTA6_TMR_CTRL_1_r_idc_2nd_byte_tmout_MASK         0xFF000000
#define CONN_PTA6_TMR_CTRL_1_r_idc_2nd_byte_tmout_SHFT         24

#define CONN_PTA6_TMR_CTRL_3_r_gps_l5_blank_tmr_thld_ADDR      CONN_PTA6_TMR_CTRL_3_ADDR
#define CONN_PTA6_TMR_CTRL_3_r_gps_l5_blank_tmr_thld_MASK      0x0000FF00
#define CONN_PTA6_TMR_CTRL_3_r_gps_l5_blank_tmr_thld_SHFT      8
#define CONN_PTA6_TMR_CTRL_3_r_gps_l1_blank_tmr_thld_ADDR      CONN_PTA6_TMR_CTRL_3_ADDR
#define CONN_PTA6_TMR_CTRL_3_r_gps_l1_blank_tmr_thld_MASK      0x000000FF
#define CONN_PTA6_TMR_CTRL_3_r_gps_l1_blank_tmr_thld_SHFT      0

#endif /* __CONN_PTA6_REGS_H__ */

