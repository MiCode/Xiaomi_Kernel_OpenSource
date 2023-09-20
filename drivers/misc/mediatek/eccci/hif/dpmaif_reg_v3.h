/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __DPMAIF_REG_V3_H__
#define __DPMAIF_REG_V3_H__

#include <linux/io.h>
#include "ccci_config.h"


#if 0
void __iomem *dpmaif_ao_ul_base;        0x10014000
void __iomem *dpmaif_ao_dl_base;        0x10014400
void __iomem *dpmaif_ao_md_dl_base;     0x10014800

void __iomem *dpmaif_pd_md_misc_base;   0x1022C000

void __iomem *dpmaif_pd_ul_base;        0x1022D000
void __iomem *dpmaif_pd_dl_base;        0x1022D100
void __iomem *dpmaif_pd_rdma_base;      0x1022D200
void __iomem *dpmaif_pd_wdma_base;      0x1022D300
void __iomem *dpmaif_pd_misc_base;      0x1022D400
void __iomem *dpmaif_ao_dl_sram_base;   0x1022DC00
void __iomem *dpmaif_ao_ul_sram_base;   0x1022DD00
void __iomem *dpmaif_ao_msic_sram_base; 0x1022DE00

void __iomem *dpmaif_pd_sram_base;      0x1022E000
#endif

/* INFRA */
#define INFRA_RST0_REG_PD (0x0150)/* reset dpmaif reg */
#define INFRA_RST1_REG_PD (0x0154)/* clear dpmaif reset reg */
#define DPMAIF_PD_RST_MASK (1 << 2)
#define INFRA_RST0_REG_AO (0x0140)
#define INFRA_RST1_REG_AO (0x0144)
#define DPMAIF_AO_RST_MASK (1 << 6)
#define INFRA_DPMAIF_CTRL_REG  (0xC00)
#define DPMAIF_IP_BUSY_MASK   (0x3 << 12)
#define SW_CG_2_STA (0xAC)
#define SW_CG_3_STA (0xC8)

#define INFRA_PROT_DPMAIF_BIT		(1 << 10)

/***********************************************************************
 *  DPMAIF AO/PD register define macro
 *
 ***********************************************************************/
#if 0
#define BASE_NADDR_NRL2_DPMAIF_UL                0x1022D000
#define BASE_NADDR_NRL2_DPMAIF_DL                0x1022D100
#define BASE_NADDR_NRL2_DPMAIF_RDMA              0x1022D200
#define BASE_NADDR_NRL2_DPMAIF_WDMA              0x1022D300
#define BASE_NADDR_NRL2_DPMAIF_AP_MISC           0x1022D400
#define BASE_NADDR_NRL2_DPMAIF_AO_UL             0x10014000
#define BASE_NADDR_NRL2_DPMAIF_AO_DL             0x10014400

#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC      0x1022DE00
#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL        0x1022DD00
#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL        0x1022DC00

#define BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX     0x1022D900
#define BASE_NADDR_NRL2_DPMAIF_MMW_HPC                         0x1022D600
#endif

#define BASE_NADDR_NRL2_DPMAIF_UL                0
#define BASE_NADDR_NRL2_DPMAIF_DL                0
#define BASE_NADDR_NRL2_DPMAIF_RDMA              0
#define BASE_NADDR_NRL2_DPMAIF_WDMA              0
#define BASE_NADDR_NRL2_DPMAIF_AP_MISC           0
#define BASE_NADDR_NRL2_DPMAIF_AO_UL             0
#define BASE_NADDR_NRL2_DPMAIF_AO_DL             0
#define BASE_NADDR_NRL2_DPMAIF_DL_AO_CFG         0
#define BASE_NADDR_NRL2_DPMAIF_PD_MD_MISC        0

#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC      0   //xuxin-add
#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL        0   //xuxin-add
#define BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL        0   //xuxin-add

#define BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX     0  //xuxin-add
#define BASE_NADDR_NRL2_DPMAIF_MMW_HPC                         0  //xuxin-add


/***********************************************************************
 * 
 *  dpmaif_ul
 *
 ***********************************************************************/
#define NRL2_DPMAIF_UL_ADD_DESC         (BASE_NADDR_NRL2_DPMAIF_UL + 0x00 )
#define NRL2_DPMAIF_UL_RESTORE_RIDX     (BASE_NADDR_NRL2_DPMAIF_UL + 0x04 )
#define NRL2_DPMAIF_UL_CHNL_ARB1        (BASE_NADDR_NRL2_DPMAIF_UL + 0x14 )
#define NRL2_DPMAIF_UL_CACHE_CON0       (BASE_NADDR_NRL2_DPMAIF_UL + 0x70 )
#define NRL2_DPMAIF_UL_RDMA_CFG0        (BASE_NADDR_NRL2_DPMAIF_UL + 0x74 )
#define NRL2_DPMAIF_UL_DBG_STA0         (BASE_NADDR_NRL2_DPMAIF_UL + 0x80 )
#define NRL2_DPMAIF_UL_DBG_STA1         (BASE_NADDR_NRL2_DPMAIF_UL + 0x84 )
#define NRL2_DPMAIF_UL_DBG_STA2         (BASE_NADDR_NRL2_DPMAIF_UL + 0x88 )
#define NRL2_DPMAIF_UL_DBG_STA3         (BASE_NADDR_NRL2_DPMAIF_UL + 0x8C )
#define NRL2_DPMAIF_UL_DBG_STA4         (BASE_NADDR_NRL2_DPMAIF_UL + 0x90 )
#define NRL2_DPMAIF_UL_DBG_STA5         (BASE_NADDR_NRL2_DPMAIF_UL + 0x94 )
#define NRL2_DPMAIF_UL_DBG_STA6         (BASE_NADDR_NRL2_DPMAIF_UL + 0x98 )
#define NRL2_DPMAIF_UL_DBG_STA7         (BASE_NADDR_NRL2_DPMAIF_UL + 0x9C )
#define NRL2_DPMAIF_UL_DBG_STA8         (BASE_NADDR_NRL2_DPMAIF_UL + 0xA0 )
#define NRL2_DPMAIF_UL_DBG_STA9         (BASE_NADDR_NRL2_DPMAIF_UL + 0xA4 )
#define NRL2_DPMAIF_UL_RESERVE_RW       (BASE_NADDR_NRL2_DPMAIF_UL + 0xA8 )
#define NRL2_DPMAIF_UL_RESERVE_AO_RW    (BASE_NADDR_NRL2_DPMAIF_UL + 0xAC )
#define NRL2_DPMAIF_UL_ADD_DESC_CH0     (BASE_NADDR_NRL2_DPMAIF_UL + 0xB0 )
#define NRL2_DPMAIF_UL_ADD_DESC_CH1     (BASE_NADDR_NRL2_DPMAIF_UL + 0xB4 )
#define NRL2_DPMAIF_UL_ADD_DESC_CH2     (BASE_NADDR_NRL2_DPMAIF_UL + 0xB8 )
#define NRL2_DPMAIF_UL_ADD_DESC_CH3     (BASE_NADDR_NRL2_DPMAIF_UL + 0xBC )
#define NRL2_DPMAIF_UL_ADD_DESC_CH4     (BASE_NADDR_NRL2_DPMAIF_UL + 0xE0 )

/***********************************************************************
 * 
 *  dpmaif_dl
 *
 ***********************************************************************/
#define NRL2_DPMAIF_DL_BAT_INIT                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x00 )
#define NRL2_DPMAIF_DL_BAT_ADD                 (BASE_NADDR_NRL2_DPMAIF_DL  + 0x04 )
#define NRL2_DPMAIF_DL_BAT_INIT_CON0           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x08 )
#define NRL2_DPMAIF_DL_BAT_INIT_CON1           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x0C )
#define NRL2_DPMAIF_DL_BAT_INIT_CON2           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x10 )
#define NRL2_DPMAIF_DL_PIT_INIT_CON5           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x14 ) //xuxin-add
#define NRL2_DPMAIF_DL_STA13                   (BASE_NADDR_NRL2_DPMAIF_DL  + 0x18 ) //xuxin-add
#define NRL2_DPMAIF_DL_STA14                   (BASE_NADDR_NRL2_DPMAIF_DL  + 0x1C ) //xuxin-add
#define NRL2_DPMAIF_DL_PIT_INIT                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x20 )
#define NRL2_DPMAIF_DL_PIT_ADD                 (BASE_NADDR_NRL2_DPMAIF_DL  + 0x24 )
#define NRL2_DPMAIF_DL_PIT_INIT_CON0           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x28 )
#define NRL2_DPMAIF_DL_PIT_INIT_CON1           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x2C )
#define NRL2_DPMAIF_DL_PIT_INIT_CON2           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x30 )
#define NRL2_DPMAIF_DL_PIT_INIT_CON3           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x34 )
#define NRL2_DPMAIF_DL_MISC_CON0               (BASE_NADDR_NRL2_DPMAIF_DL  + 0x40 )
#define NRL2_DPMAIF_DL_STA12                   (BASE_NADDR_NRL2_DPMAIF_DL  + 0x4C )
#define NRL2_DPMAIF_DL_BAT_INIT_CON3           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x50 )
#define NRL2_DPMAIF_DL_PIT_INIT_CON4           (BASE_NADDR_NRL2_DPMAIF_DL  + 0x54 )
#define NRL2_DPMAIF_DL_STA9                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x58 )
#define NRL2_DPMAIF_DL_STA10                   (BASE_NADDR_NRL2_DPMAIF_DL  + 0x5C )
#define NRL2_DPMAIF_DL_STA11                   (BASE_NADDR_NRL2_DPMAIF_DL  + 0x60 )
#define NRL2_DPMAIF_DL_FRG_STA5                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x64 )
#define NRL2_DPMAIF_DL_FRG_STA6                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x68 )
#define NRL2_DPMAIF_DL_DBG_STA16               (BASE_NADDR_NRL2_DPMAIF_DL  + 0x6C )
#define NRL2_DPMAIF_DL_DBG_FRG0                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x70 )
#define NRL2_DPMAIF_DL_DBG_FRG1                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x74 )
#define NRL2_DPMAIF_DL_DBG_FRG2                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x78 )
#define NRL2_DPMAIF_DL_DBG_FRG3                (BASE_NADDR_NRL2_DPMAIF_DL  + 0x7C )
#define NRL2_DPMAIF_DL_STA0                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x80 )
#define NRL2_DPMAIF_DL_STA1                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x84 )
#define NRL2_DPMAIF_DL_STA2                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x88 )
#define NRL2_DPMAIF_DL_STA3                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x8C )
#define NRL2_DPMAIF_DL_STA4                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x90 )
#define NRL2_DPMAIF_DL_STA5                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x94 )
#define NRL2_DPMAIF_DL_STA6                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x98 )
#define NRL2_DPMAIF_DL_STA7                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0x9C )
#define NRL2_DPMAIF_DL_STA8                    (BASE_NADDR_NRL2_DPMAIF_DL  + 0xA0 )
#define NRL2_DPMAIF_DL_DBG_STA15               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xA4 )
#define NRL2_DPMAIF_DL_RESERVE_RW              (BASE_NADDR_NRL2_DPMAIF_DL  + 0xA8 )
#define NRL2_DPMAIF_DL_RESERVE_AO_RW           (BASE_NADDR_NRL2_DPMAIF_DL  + 0xAC )
#define NRL2_DPMAIF_DL_DBG_STA0                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xB0 )
#define NRL2_DPMAIF_DL_DBG_STA1                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xB4 )
#define NRL2_DPMAIF_DL_DBG_STA2                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xB8 )
#define NRL2_DPMAIF_DL_DBG_STA3                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xBC )
#define NRL2_DPMAIF_DL_DBG_STA4                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xC0 )
#define NRL2_DPMAIF_DL_DBG_STA5                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xC4 )
#define NRL2_DPMAIF_DL_DBG_STA6                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xC8 )
#define NRL2_DPMAIF_DL_DBG_STA7                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xCC )
#define NRL2_DPMAIF_DL_DBG_STA8                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xD0 )
#define NRL2_DPMAIF_DL_DBG_STA9                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xD4 )
#define NRL2_DPMAIF_DL_DBG_STA10               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xD8 )
#define NRL2_DPMAIF_DL_DBG_STA11               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xDC )
#define NRL2_DPMAIF_DL_FRG_STA0                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xE0 )
#define NRL2_DPMAIF_DL_FRG_STA1                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xE4 )
#define NRL2_DPMAIF_DL_FRG_STA2                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xE8 )
#define NRL2_DPMAIF_DL_FRG_STA3                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xEC )
#define NRL2_DPMAIF_DL_FRG_STA4                (BASE_NADDR_NRL2_DPMAIF_DL  + 0xF0 )
#define NRL2_DPMAIF_DL_DBG_STA12               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xF4 )
#define NRL2_DPMAIF_DL_DBG_STA13               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xF8 )
#define NRL2_DPMAIF_DL_DBG_STA14               (BASE_NADDR_NRL2_DPMAIF_DL  + 0xFC )
/***********************************************************************
 * 
 *  dpmaif_rwdma
 *
 ***********************************************************************/
#define NRL2_DPMAIF_RDMA_CON0          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x000 )
#define NRL2_DPMAIF_RDMA_CON1          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x004 )
#define NRL2_DPMAIF_RDMA_CON2          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x008 )
#define NRL2_DPMAIF_RDMA_CON3          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x00C )
#define NRL2_DPMAIF_RDMA_CON4          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x010 )
#define NRL2_DPMAIF_RDMA_CON5          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x014 )
#define NRL2_DPMAIF_RDMA_CON6          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x018 )
#define NRL2_DPMAIF_RDMA_CON7          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x01C )
#define NRL2_DPMAIF_RDMA_CON8          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x020 )
#define NRL2_DPMAIF_RDMA_CON9          (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x024 )
#define NRL2_DPMAIF_RDMA_CON10         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x028 )
#define NRL2_DPMAIF_RDMA_CON11         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x02C )
#define NRL2_DPMAIF_RDMA_CON12         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x030 )
#define NRL2_DPMAIF_RDMA_CON13         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x034 )
#define NRL2_DPMAIF_RDMA_CON14         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x038 )
#define NRL2_DPMAIF_RDMA_CON15         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x03C )
#define NRL2_DPMAIF_RDMA_CON16         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x040 )
#define NRL2_DPMAIF_RDMA_CON17         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x044 )
#define NRL2_DPMAIF_RDMA_CON18         (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x048 )
#define NRL2_DPMAIF_RDMA_EXCEP_STA     (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x080 )
#define NRL2_DPMAIF_RDMA_EXCEP_MASK    (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x084 )
#define NRL2_DPMAIF_RDMA_DBG_CON0      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x090 )
#define NRL2_DPMAIF_RDMA_DBG_CON1      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x094 )
#define NRL2_DPMAIF_RDMA_DBG_CON2      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x098 )
#define NRL2_DPMAIF_RDMA_DBG_CON3      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x09C )
#define NRL2_DPMAIF_RDMA_DBG_CON4      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0A0 )
#define NRL2_DPMAIF_RDMA_DBG_CON5      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0A4 )
#define NRL2_DPMAIF_RDMA_DBG_CON6      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0A8 )
#define NRL2_DPMAIF_RDMA_DBG_CON7      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0AC )
#define NRL2_DPMAIF_RDMA_DBG_CON8      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0B0 )
#define NRL2_DPMAIF_RDMA_DBG_CON9      (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0B4 )
#define NRL2_DPMAIF_RDMA_DBG_CON10     (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0B8 )
#define NRL2_DPMAIF_RDMA_DBG_CON11     (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0BC )
#define NRL2_DPMAIF_RDMA_DBG_CON12     (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0C0 )
#define NRL2_DPMAIF_RDMA_DBG_CON13     (BASE_NADDR_NRL2_DPMAIF_RDMA + 0x0C4 )

#define NRL2_DPMAIF_WDMA_WR_CMD_CON0               (BASE_NADDR_NRL2_DPMAIF_WDMA + 0x000 )
#define NRL2_DPMAIF_WDMA_WR_CMD_CON1               (BASE_NADDR_NRL2_DPMAIF_WDMA + 0x004 )
#define NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON2          (BASE_NADDR_NRL2_DPMAIF_WDMA + 0x010 )
#define NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3          (BASE_NADDR_NRL2_DPMAIF_WDMA + 0x014 )

/***********************************************************************
 * 
 *  dpmaif_ap_misc
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AP_MISC_AP_L2TISAR0         (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x00 )
#define NRL2_DPMAIF_AP_MISC_AP_L1TISAR0         (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x10 )
#define NRL2_DPMAIF_AP_MISC_AP_L1TIMR0          (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x14 )
#define NRL2_DPMAIF_AP_MISC_BUS_CONFIG0         (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x20 )
#define NRL2_DPMAIF_AP_MISC_TOP_AP_CFG          (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x24 )
#define NRL2_DPMAIF_AP_MISC_EMI_BUS_STATUS0     (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x30 )
#define NRL2_DPMAIF_AP_MISC_PCIE_BUS_STATUS0    (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x34 )
#define NRL2_DPMAIF_AP_MISC_AP_DMA_ERR_STA      (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x40 )
#define NRL2_DPMAIF_AP_MISC_APDL_L2TISAR0       (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x50 )
#define NRL2_DPMAIF_AP_MISC_AP_IP_BUSY          (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x60 )
#define NRL2_DPMAIF_AP_MISC_CG_EN               (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x68 )
#define NRL2_DPMAIF_AP_MISC_CODA_VER            (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x6C )
#define NRL2_DPMAIF_AP_MISC_APB_DBG_SRAM        (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x70 )

#define NRL2_DPMAIF_AP_MISC_HPC_SRAM_USAGE      (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x74 ) //xuxin-add
#define NRL2_DPMAIF_AP_MISC_MPIT_CACHE_INV      (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x80 ) //xuxin-add
#define NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG       (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x90 ) //xuxin-add

#define NRL2_DPMAIF_AP_MISC_MEM_CLR             (BASE_NADDR_NRL2_DPMAIF_AP_MISC + 0x94 ) //xuxin-add
/***********************************************************************
 * 
 *  dpmaif_ul_ao
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_UL_INIT_SET                (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x0 )  //xuxin-add, has use

#define NRL2_DPMAIF_AO_UL_CHNL_ARB0               (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x1C )
#define NRL2_DPMAIF_AO_UL_AP_L2TIMR0              (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x80 )
#define NRL2_DPMAIF_AO_UL_AP_L2TIMCR0             (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x84 )
#define NRL2_DPMAIF_AO_UL_AP_L2TIMSR0             (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x88 )
#define NRL2_DPMAIF_AO_UL_AP_L1TIMR0              (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x8C )
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMR0            (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x90 )
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0           (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x94 )
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0           (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x98 )
#define NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK   (BASE_NADDR_NRL2_DPMAIF_AO_UL + 0x9C )

/***********************************************************************
 * 
 *  dpmaif_ul_pd_sram
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_UL_MD_RDY_CNT_TH			  (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x0 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_ULQN_MAX_PKT_SZ            (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x4 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH_WEIGHT0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x8 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH_WEIGHT1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0xC ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL0_CON0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x10 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL0_CON1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x14 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL0_CON2              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x18 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL1_CON0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x20 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL1_CON1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x24 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL1_CON2              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x28 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL2_CON0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x30 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL2_CON1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x34 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL2_CON2              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x38 ) ///xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL3_CON0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x40 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL3_CON1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x44 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL3_CON2              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x48 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL4_CON0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x50 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL4_CON1              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x54 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CHNL4_CON2              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x58 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH0_STA                 (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x70 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH1_STA                 (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x74 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH2_STA                 (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x78 ) //xxuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH3_STA                 (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x7C ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH4_STA                 (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x80 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH_WIDX01               (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x84 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH_WIDX23               (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x88 ) //xuxin-change, base from AO_UL
#define NRL2_DPMAIF_AO_UL_CH_WIDX4                (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x8C ) //xuxin-change, base from AO_UL
/*
#define NRL2_DPMAIF_AO_UL_CHNL0_STA0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x60 )
#define NRL2_DPMAIF_AO_UL_CHNL1_STA0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x64 )
#define NRL2_DPMAIF_AO_UL_CHNL2_STA0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x68 )
#define NRL2_DPMAIF_AO_UL_CHNL3_STA0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0x6C )
#define NRL2_DPMAIF_AO_UL_CHNL4_STA0              (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_UL + 0xDC )
*/

/***********************************************************************
 * 
 *  dpmaif_dl_ao
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_DL_INIT_SET					(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x0 ) //xuxin-add, has use
#define NRL2_DPMAIF_AO_DL_INIT_RESTORE_CFG          (BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x4 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_MISC_IRQ_MASK          	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0xC ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_INIT_CON5	        (BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x28 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TRIG_THRES	        (BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x34 ) //xuxin-add, no used

#define NRL2_DPMAIF_AO_DL_MISC_AO_ABI_EN          	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x60 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_MISC_AO_SBI_EN          	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x64 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_AO_PA_OFS_L              	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x68 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_AO_PA_OFS_H             	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x6C ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_AO_SCP_DL_MASK          	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x70 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_AO_SCP_UL_MASK          	(BASE_NADDR_NRL2_DPMAIF_AO_DL + 0x74 ) //xuxin-add, no used

/***********************************************************************
 * 
 *  dpmaif_dl_pd_sram
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_DL_PKTINFO_CON0            (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x0 ) //xuxin-change, base from AO_DL
#define NRL2_DPMAIF_AO_DL_PKTINFO_CON1            (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x4 ) //xuxin-change, base from AO_DL
#define NRL2_DPMAIF_AO_DL_PKTINFO_CON2            (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x8 ) //xuxin-change, base from AO_DL
#define NRL2_DPMAIF_AO_DL_RDY_CHK_THRES           (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xC ) //xuxin-change, base from AO_DL
#define NRL2_DPMAIF_AO_DL_RDY_CHK_FRG_THRES       (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x10 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_REORDER_BITMAP_CACHE    (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x14 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_REORDER_CACHE           (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x18 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_REORDER_THRES           (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x1C ) //xuxin-change, base from AO_DL, offset change

#define NRL2_DPMAIF_AO_DL_LRO_AGG_CFG           		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x20 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT0           	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x24 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT1           	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x28 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT2           	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x2C ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT3           	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x30 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT4           	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x34 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_HPC_CNTL           		    (BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x38 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_PIT_SEQ_END             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x40 ) //xuxin-add, no used

#define NRL2_DPMAIF_AO_DL_BAT_STA0                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xD0 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_BAT_STA1                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xD4 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_BAT_STA2                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xD8 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_BAT_STA3                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xDC ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_BAT_STA4                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xE0 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_PIT_STA0                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xE4 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA1                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xE8 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA2                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xEC ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA3                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x60 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA4                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x64 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA5                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x68 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_PIT_STA6                		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x6C ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_FRGBAT_STA0             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x70 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_FRGBAT_STA1             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x74 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_FRGBAT_STA2             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x78 ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_FRGBAT_STA3             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x7C ) //xuxin-change, base from AO_DL, offset change
#define NRL2_DPMAIF_AO_DL_FRGBAT_STA4             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x80 ) //xuxin-add, no used

#define NRL2_DPMAIF_AO_DL_LRO_STA0             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x90 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA1             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x94 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA2             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x98 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA3             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0x9C ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA4             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xA0 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA5             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xA4 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA6             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xA8 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA8             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xB0 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA9             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xB4 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA10             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xB8 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA11             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xBC ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA12             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xC0 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA13             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xC4 ) //xuxin-add, no used
#define NRL2_DPMAIF_AO_DL_LRO_STA14             		(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_DL + 0xC8 ) //xuxin-add, no used

/***********************************************************************
 *
 *  PD_SRAM_MISC
 *
 ***********************************************************************/
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP0_2 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x0C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP3_5 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x10)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP6_7_BANK1_MAP0 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x14)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP1_3 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x18)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP4_6 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x1C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP7_BANK4_MAP0_1 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x20)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP2_4 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x24)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP5_7 \
	(BASE_NADDR_NRL2_DPMAIF_PD_SRAM_MISC + 0x28)

/***********************************************************************
 *
 *  dpmaif_hpc
 *
 ***********************************************************************/
#define NRL2_DPMAIF_HPC_ENTRY_STS0              (BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x004 ) //xuxin-add, no used
#define NRL2_DPMAIF_HPC_ENTRY_STS1          	(BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x008 ) //xuxin-add, no used
#define NRL2_DPMAIF_HPC_CI_CO0                 	(BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x010 ) //xuxin-add, no used
#define NRL2_DPMAIF_HPC_RULE_TGL                (BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x038 ) //xuxin-add, no used
#define NRL2_DPMAIF_HPC_HASH_0TO3            	(BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x040 ) //xuxin-add, no used
#define NRL2_DPMAIF_HPC_OPT_IDX01             	(BASE_NADDR_NRL2_DPMAIF_MMW_HPC  + 0x050 ) //xuxin-add, no used
/***********************************************************************
 *
 *  dpmaif_lro
 *
 ***********************************************************************/
#define NRL2_DPMAIF_DL_LROPIT_INIT             (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_ADD              (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x10 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON0        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x14 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON1        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x18 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON2        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x1C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON5        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x28 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON3        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x20 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON4        (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x24 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_SW_TRIG          (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x30 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT5         (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x50 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT6         (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x54 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT7         (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x58 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT8         (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x5C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_DBG_SEL             (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x64 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_SEL       (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x68 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_DBG_OUT0            (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x6C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT0      (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x70 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT1      (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x74 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT2      (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x78 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT3      (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x7C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA0                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x80 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA1                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x84 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA2                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x88 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA3                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x8C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STAA                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA4                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x90 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STAB                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC4 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA5                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x94 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA6                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x98 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA7                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x9C ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA8                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STAC                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC8 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STA9                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA4 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LRO_STAD                (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XCC ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_STA                 (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA8 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMER0           (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xAC ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMER1           (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMER2           (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB4 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMER3           (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB8 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROPIT_TIMER4           (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xBC ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_WIDX1               (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_WIDX2               (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD4 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_WIDX3               (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD8 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_WIDX4               (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xDC ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_AGG_STA_IDX             (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xE0 ) //xuxin-add, no used
#define NRL2_DPMAIF_DL_LROTIMER_GATED          (BASE_NADDR_NRL2_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x34 ) //xuxin-add, no used

/**********************************************************************
*  
*
*	Common define for DPMAIF driver
*
*
***********************************************************************/
/*DPMAIF PD UL/AO UL CONFIG*/
#define DPMAIF_PD_UL_ADD_DESC             NRL2_DPMAIF_UL_ADD_DESC
#define DPMAIF_PD_UL_RESTORE_RIDX         NRL2_DPMAIF_UL_RESTORE_RIDX

#define DPMAIF_PD_UL_MD_RDY_CNT_TH        NRL2_DPMAIF_AO_UL_MD_RDY_CNT_TH
#define DPMAIF_PD_UL_CHNL_ARB0            NRL2_DPMAIF_AO_UL_CHNL_ARB0
#define DPMAIF_PD_UL_CHNL_ARB1            NRL2_DPMAIF_UL_CHNL_ARB1

#define DPMAIF_PD_UL_CHNL0_CON0           NRL2_DPMAIF_AO_UL_CHNL0_CON0
#define DPMAIF_PD_UL_CHNL0_CON1           NRL2_DPMAIF_AO_UL_CHNL0_CON1
#define DPMAIF_PD_UL_CHNL0_CON2           NRL2_DPMAIF_AO_UL_CHNL0_CON2  //xuxin-add, no used
//#define DPMAIF_PD_UL_CHNL0_STA0           NRL2_DPMAIF_AO_UL_CHNL0_STA0

#define DPMAIF_PD_UL_CHNL1_CON0           NRL2_DPMAIF_AO_UL_CHNL1_CON0
#define DPMAIF_PD_UL_CHNL1_CON1           NRL2_DPMAIF_AO_UL_CHNL1_CON1
//#define DPMAIF_PD_UL_CHNL1_STA0           NRL2_DPMAIF_AO_UL_CHNL1_STA0

#define DPMAIF_PD_UL_CHNL2_CON0           NRL2_DPMAIF_AO_UL_CHNL2_CON0
#define DPMAIF_PD_UL_CHNL2_CON1           NRL2_DPMAIF_AO_UL_CHNL2_CON1
//#define DPMAIF_PD_UL_CHNL2_STA0           NRL2_DPMAIF_AO_UL_CHNL2_STA0

#define DPMAIF_PD_UL_CHNL3_CON0           NRL2_DPMAIF_AO_UL_CHNL3_CON0
#define DPMAIF_PD_UL_CHNL3_CON1           NRL2_DPMAIF_AO_UL_CHNL3_CON1
//#define DPMAIF_PD_UL_CHNL3_STA0           NRL2_DPMAIF_AO_UL_CHNL3_STA0

#define DPMAIF_PD_UL_CACHE_CON0           NRL2_DPMAIF_UL_CACHE_CON0
#define DPMAIF_PD_UL_ADD_DESC_CH          NRL2_DPMAIF_UL_ADD_DESC_CH0
#define DPMAIF_PD_UL_ADD_DESC_CH0         NRL2_DPMAIF_UL_ADD_DESC_CH0  //xuxin-add, no used
#define DPMAIF_PD_UL_ADD_DESC_CH1         NRL2_DPMAIF_UL_ADD_DESC_CH1  //xuxin-add, no used
#define DPMAIF_PD_UL_ADD_DESC_CH2         NRL2_DPMAIF_UL_ADD_DESC_CH2  //xuxin-add, no used
#define DPMAIF_PD_UL_ADD_DESC_CH3         NRL2_DPMAIF_UL_ADD_DESC_CH3  //xuxin-add, no used
#define DPMAIF_PD_UL_ADD_DESC_CH4         NRL2_DPMAIF_UL_ADD_DESC_CH4

#define DPMAIF_PD_UL_CH_WIDX01            NRL2_DPMAIF_AO_UL_CH_WIDX01  //xuxin-add, no used
#define DPMAIF_PD_UL_CH_WIDX23            NRL2_DPMAIF_AO_UL_CH_WIDX23  //xuxin-add, no used
#define DPMAIF_PD_ULQN_MAX_PKT_SZ         NRL2_DPMAIF_AO_ULQN_MAX_PKT_SZ  //xuxin-add, no used

#define DPMAIF_PD_UL_DBG_STA0             NRL2_DPMAIF_UL_DBG_STA0
#define DPMAIF_PD_UL_DBG_STA1             NRL2_DPMAIF_UL_DBG_STA1
#define DPMAIF_PD_UL_DBG_STA2             NRL2_DPMAIF_UL_DBG_STA2
#define DPMAIF_PD_UL_DBG_STA3             NRL2_DPMAIF_UL_DBG_STA3
#define DPMAIF_PD_UL_DBG_STA4             NRL2_DPMAIF_UL_DBG_STA4
#define DPMAIF_PD_UL_DBG_STA5             NRL2_DPMAIF_UL_DBG_STA5
#define DPMAIF_PD_UL_DBG_STA6             NRL2_DPMAIF_UL_DBG_STA6
#define DPMAIF_PD_UL_DBG_STA7             NRL2_DPMAIF_UL_DBG_STA7
#define DPMAIF_PD_UL_DBG_STA8             NRL2_DPMAIF_UL_DBG_STA8
#define DPMAIF_PD_UL_DBG_STA9             NRL2_DPMAIF_UL_DBG_STA9

/*DPMAIF PD DL CONFIG*/
#define DPMAIF_PD_DL_BAT_INIT             NRL2_DPMAIF_DL_BAT_INIT
#define DPMAIF_PD_DL_BAT_ADD              NRL2_DPMAIF_DL_BAT_ADD
#define DPMAIF_PD_DL_BAT_INIT_CON0        NRL2_DPMAIF_DL_BAT_INIT_CON0
#define DPMAIF_PD_DL_BAT_INIT_CON1        NRL2_DPMAIF_DL_BAT_INIT_CON1
#define DPMAIF_PD_DL_BAT_INIT_CON2        NRL2_DPMAIF_DL_BAT_INIT_CON2
#define DPMAIF_PD_DL_BAT_INIT_CON3        NRL2_DPMAIF_DL_BAT_INIT_CON3  //xuxin-add, no used
#define DPMAIF_PD_DL_PIT_INIT             NRL2_DPMAIF_DL_PIT_INIT
#define DPMAIF_PD_DL_PIT_ADD              NRL2_DPMAIF_DL_PIT_ADD
#define DPMAIF_PD_DL_PIT_INIT_CON0        NRL2_DPMAIF_DL_PIT_INIT_CON0
#define DPMAIF_PD_DL_PIT_INIT_CON1        NRL2_DPMAIF_DL_PIT_INIT_CON1
#define DPMAIF_PD_DL_PIT_INIT_CON2        NRL2_DPMAIF_DL_PIT_INIT_CON2
#define DPMAIF_PD_DL_PIT_INIT_CON3        NRL2_DPMAIF_DL_PIT_INIT_CON3
#define DPMAIF_PD_DL_PIT_INIT_CON4        NRL2_DPMAIF_DL_PIT_INIT_CON4  //xuxin-add, no used
#define DPMAIF_PD_DL_PIT_INIT_CON5        NRL2_DPMAIF_DL_PIT_INIT_CON5  //xuxin-add, no used
#define DPMAIF_PD_DL_MISC_CON0            NRL2_DPMAIF_DL_MISC_CON0
#define DPMAIF_PD_DL_STA8                 NRL2_DPMAIF_DL_STA8
#define DPMAIF_PD_DL_STA13                NRL2_DPMAIF_DL_STA13  //xuxin-add, no used
#define DPMAIF_PD_DL_STA14                NRL2_DPMAIF_DL_STA14  //xuxin-add, no used
#define DPMAIF_PD_DL_DBG_STA0             NRL2_DPMAIF_DL_DBG_STA0
#define DPMAIF_PD_DL_DBG_STA1             NRL2_DPMAIF_DL_DBG_STA1
#define DPMAIF_PD_DL_DBG_STA7             NRL2_DPMAIF_DL_DBG_STA7

#define DPMAIF_PD_DL_STA0                 NRL2_DPMAIF_DL_STA0
#define DPMAIF_PD_DL_DBG_STA14            NRL2_DPMAIF_DL_DBG_STA14

/*DPMAIF PD AP MSIC/AO UL MISC CONFIG*/
#define DPMAIF_PD_AP_UL_L2TISAR0          NRL2_DPMAIF_AP_MISC_AP_L2TISAR0
#define DPMAIF_PD_AP_UL_L2TIMR0           NRL2_DPMAIF_AO_UL_AP_L2TIMR0
#define DPMAIF_PD_AP_UL_L2TICR0           NRL2_DPMAIF_AO_UL_AP_L2TIMCR0
#define DPMAIF_PD_AP_UL_L2TISR0           NRL2_DPMAIF_AO_UL_AP_L2TIMSR0

#define DPMAIF_PD_AP_L1TISAR0             NRL2_DPMAIF_AP_MISC_AP_L1TISAR0
#define DPMAIF_PD_AP_L1TIMR0              NRL2_DPMAIF_AP_MISC_AP_L1TIMR0

#define DPMAIF_PD_AP_DL_L2TISAR0          NRL2_DPMAIF_AP_MISC_APDL_L2TISAR0
#define DPMAIF_PD_AP_DL_L2TIMR0           NRL2_DPMAIF_AO_UL_APDL_L2TIMR0
#define DPMAIF_PD_AP_DL_L2TICR0           NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0  //xuxin-add, no used
#define DPMAIF_PD_AP_DL_L2TISR0           NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0

#define DPMAIF_PD_AP_IP_BUSY              NRL2_DPMAIF_AP_MISC_AP_IP_BUSY
#define DPMAIF_PD_AP_DLUL_IP_BUSY_MASK    NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK

#define DPMAIF_PD_BUS_CONFIG0             NRL2_DPMAIF_AP_MISC_BUS_CONFIG0
#define DPMAIF_PD_TOP_AP_CFG              NRL2_DPMAIF_AP_MISC_TOP_AP_CFG
#define DPMAIF_PD_BUS_STATUS0             NRL2_DPMAIF_AP_MISC_EMI_BUS_STATUS0
#define DPMAIF_PD_AP_DMA_ERR_STA          NRL2_DPMAIF_AP_MISC_AP_DMA_ERR_STA

#define DPMAIF_PD_AP_DL_L2TIMSR0          NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0

#define DPMAIF_PD_AP_CG_EN                NRL2_DPMAIF_AP_MISC_CG_EN
#define DPMAIF_PD_AP_CODA_VER             NRL2_DPMAIF_AP_MISC_CODA_VER

/*DPMAIF AO UL CONFIG*/
#define DPMAIF_AO_UL_CHNL0_STA            NRL2_DPMAIF_AO_UL_CH0_STA
#define DPMAIF_AO_UL_CHNL1_STA            NRL2_DPMAIF_AO_UL_CH1_STA
#define DPMAIF_AO_UL_CHNL2_STA            NRL2_DPMAIF_AO_UL_CH2_STA
#define DPMAIF_AO_UL_CHNL3_STA            NRL2_DPMAIF_AO_UL_CH3_STA

#define DPMAIF_AO_UL_CH_WEIGHT1           NRL2_DPMAIF_AO_UL_CH_WEIGHT1

/*DPMAIF AO DL CONFIG*/
#define DPMAIF_AO_DL_PKTINFO_CONO         NRL2_DPMAIF_AO_DL_PKTINFO_CON0
#define DPMAIF_AO_DL_PKTINFO_CON1         NRL2_DPMAIF_AO_DL_PKTINFO_CON1
#define DPMAIF_AO_DL_PKTINFO_CON2         NRL2_DPMAIF_AO_DL_PKTINFO_CON2
#define DPMAIF_AO_DL_RDY_CHK_THRES        NRL2_DPMAIF_AO_DL_RDY_CHK_THRES
#define DPMAIF_AO_DL_BAT_STA0             NRL2_DPMAIF_AO_DL_BAT_STA0
#define DPMAIF_AO_DL_BAT_STA1             NRL2_DPMAIF_AO_DL_BAT_STA1
#define DPMAIF_AO_DL_BAT_STA2             NRL2_DPMAIF_AO_DL_BAT_STA2
#define DPMAIF_AO_DL_BAT_STA3             NRL2_DPMAIF_AO_DL_BAT_STA3  //xuxin-add, no used
#define DPMAIF_AO_DL_PIT_STA0             NRL2_DPMAIF_AO_DL_PIT_STA0
#define DPMAIF_AO_DL_PIT_STA1             NRL2_DPMAIF_AO_DL_PIT_STA1
#define DPMAIF_AO_DL_PIT_STA2             NRL2_DPMAIF_AO_DL_PIT_STA2
#define DPMAIF_AO_DL_PIT_STA3             NRL2_DPMAIF_AO_DL_PIT_STA3
#define DPMAIF_AO_DL_PIT_STA4             NRL2_DPMAIF_AO_DL_PIT_STA4  //xuxin-add, no used
#define DPMAIF_AO_DL_RDY_CHK_FRG_THRES    NRL2_DPMAIF_AO_DL_RDY_CHK_FRG_THRES
#define DPMAIF_AO_DL_FRGBAT_STA2          NRL2_DPMAIF_AO_DL_FRGBAT_STA2

#define DPMAIF_AO_DL_FRG_CHK_THRES        NRL2_DPMAIF_AO_DL_RDY_CHK_FRG_THRES  //xuxin-add, no used
#define DPMAIF_AO_DL_FRG_STA0             NRL2_DPMAIF_AO_DL_FRGBAT_STA0  //xuxin-add, no used
#define DPMAIF_AO_DL_FRG_STA1             NRL2_DPMAIF_AO_DL_FRGBAT_STA1  //xuxin-add, no used
#define DPMAIF_AO_DL_FRG_STA2             NRL2_DPMAIF_AO_DL_FRGBAT_STA2  //xuxin-add, no used

#define DPMAIF_AO_DL_REORDER_THRES        NRL2_DPMAIF_AO_DL_REORDER_THRES

/*DPMAIF PD MD MISC CONFIG */
#define DPMAIF_MISC_AO_CFG0               (BASE_NADDR_NRL2_DPMAIF_DL_AO_CFG + 0x00)
#define DPMAIF_MISC_AO_MSIC_CFG           (BASE_NADDR_NRL2_DPMAIF_DL_AO_CFG + 0x64)

#define DPMAIF_AXI_MAS_SECURE             NRL2_DPMAIF_AXI_MAS_SECURE
#define DPMAIF_AP_MISC_APB_DBG_SRAM       NRL2_DPMAIF_AP_MISC_APB_DBG_SRAM

/*DPMAIF PD MD MISC CONFIG: 0x1022C000 */
#define DPMAIF_PD_MD_IP_BUSY              NRL2_DPMAIF_PD_MD_IP_BUSY
#define DPMAIF_PD_MD_IP_BUSY_MASK         NRL2_DPMAIF_PD_MD_IP_BUSY_MASK

#define NRL2_DPMAIF_AXI_MAS_SECURE		\
				(BASE_NADDR_NRL2_DPMAIF_DL_AO_CFG + 0x60)
#define NRL2_DPMAIF_PD_MD_IP_BUSY		\
				(BASE_NADDR_NRL2_DPMAIF_PD_MD_MISC + 0x0000)
#define NRL2_DPMAIF_PD_MD_IP_BUSY_MASK		\
				(BASE_NADDR_NRL2_DPMAIF_PD_MD_MISC + 0x0040)

#endif /* __DPMAIF_REG_V3_H__ */
