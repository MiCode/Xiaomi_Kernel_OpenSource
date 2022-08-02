/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DPMAIF_REG_COM_H__
#define __CCCI_DPMAIF_REG_COM_H__

#include <linux/io.h>




/***********************************************************************
 *  DPMAIF AO/PD register define macro
 *
 ***********************************************************************/
/***********************************************************************
*void __iomem *dpmaif_ao_ul_base;        0x10014000
*void __iomem *dpmaif_ao_dl_base;        0x10014400
*void __iomem *dpmaif_ao_md_dl_base;     0x10014800

*void __iomem *dpmaif_pd_md_misc_base;   0x1022C000

*void __iomem *dpmaif_pd_ul_base;        0x1022D000
*void __iomem *dpmaif_pd_dl_base;        0x1022D100
*void __iomem *dpmaif_pd_rdma_base;      0x1022D200
*void __iomem *dpmaif_pd_wdma_base;      0x1022D300
*void __iomem *dpmaif_pd_misc_base;      0x1022D400
*void __iomem *dpmaif_pd_mmw_hpc_base;   0x1022D600
*void __iomem *dpmaif_pd_dl_lro_base;    0x1022D900
*void __iomem *dpmaif_ao_dl_sram_base;   0x1022DC00
*void __iomem *dpmaif_ao_ul_sram_base;   0x1022DD00
*void __iomem *dpmaif_ao_msic_sram_base; 0x1022DE00

*void __iomem *dpmaif_pd_sram_base;      0x1022E000
***********************************************************************/

#define BASE_DPMAIF_UL                0
#define BASE_DPMAIF_DL                0
#define BASE_DPMAIF_RDMA              0
#define BASE_DPMAIF_WDMA              0
#define BASE_DPMAIF_AP_MISC           0
#define BASE_DPMAIF_AO_UL             0
#define BASE_DPMAIF_AO_DL             0
#define BASE_DPMAIF_DL_AO_CFG         0
#define BASE_DPMAIF_PD_MD_MISC        0

#define BASE_DPMAIF_PD_SRAM_MISC      0
#define BASE_DPMAIF_PD_SRAM_UL        0
#define BASE_DPMAIF_PD_SRAM_DL        0

#define BASE_DPMAIF_MMW_HPC           0
#define BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  0


extern unsigned int         g_dpmf_ver;
extern struct dpmaif_ctrl  *g_dpmaif_ctl;
#define dpmaif_ctl          g_dpmaif_ctl


#define dpmaif_write32(b, a, v)	\
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_write16(b, a, v)	\
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_write8(b, a, v) \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_read32(b, a)	ioread32((void __iomem *)((b)+(a)))
#define dpmaif_read16(b, a)	ioread16((void __iomem *)((b)+(a)))
#define dpmaif_read8(b, a)	ioread8((void __iomem *)((b)+(a)))




#define DPMA_READ_PD_MISC(a) \
	dpmaif_read32(dpmaif_ctl->pd_misc_base, (a))
#define DPMA_WRITE_PD_MISC(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_misc_base, (a), v)

#define DPMA_READ_WDMA(a) \
	dpmaif_read32(dpmaif_ctl->pd_wdma_base, (a))
#define DPMA_WRITE_WDMA(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_wdma_base, (a), v)

#define DPMA_READ_AO_UL(a) \
	dpmaif_read32(dpmaif_ctl->ao_ul_base, (a))
#define DPMA_WRITE_AO_UL(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_ul_base, (a), v)

#define DPMA_READ_PD_DL(a) \
	dpmaif_read32(dpmaif_ctl->pd_dl_base, (a))
#define DPMA_WRITE_PD_DL(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_dl_base, (a), v)

#define DPMA_WRITE_AO_MISC_SRAM(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_msic_sram_base, (a), v)

#define DPMA_READ_AO_MD_DL(a) \
	dpmaif_read32(dpmaif_ctl->ao_md_dl_base, (a))
#define DPMA_WRITE_AO_MD_DL(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_md_dl_base, (a), v)

#define DPMA_READ_AO_DL(a) \
	dpmaif_read32(dpmaif_ctl->ao_dl_base, (a))
#define DPMA_WRITE_AO_DL(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_dl_base, (a), v)

#define DPMA_READ_AO_DL_SRAM(a) \
	dpmaif_read32(dpmaif_ctl->ao_dl_sram_base, (a))
#define DPMA_WRITE_AO_DL_SRAM(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_dl_sram_base, (a), v)

#define DPMA_READ_AO_UL_SRAM(a) \
	dpmaif_read32(dpmaif_ctl->ao_ul_sram_base, (a))
#define DPMA_WRITE_AO_UL_SRAM(a, v) \
	dpmaif_write32(dpmaif_ctl->ao_ul_sram_base, (a), v)

#define DPMA_READ_PD_UL(a) \
	dpmaif_read32(dpmaif_ctl->pd_ul_base, (a))
#define DPMA_WRITE_PD_UL(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_ul_base, (a), v)

#define DPMA_READ_PD_MD_MISC(a) \
	dpmaif_read32(dpmaif_ctl->pd_md_misc_base, (a))
#define DPMA_WRITE_PD_MD_MISC(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_md_misc_base, (a), v)


#define DPMA_READ_PD_DL_LRO(a) \
	dpmaif_read32(dpmaif_ctl->pd_dl_lro_base, (a))
#define DPMA_WRITE_PD_DL_LRO(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_dl_lro_base, (a), v)

#define DPMA_READ_PD_MMW_HPC(a) \
	dpmaif_read32(dpmaif_ctl->pd_mmw_hpc_base, (a))
#define DPMA_WRITE_PD_MMW_HPC(a, v) \
	dpmaif_write32(dpmaif_ctl->pd_mmw_hpc_base, (a), v)



/* INFRA */
#define INFRA_RST0_REG_PD      (0x0150)/* reset dpmaif reg */
#define INFRA_RST1_REG_PD      (0x0154)/* clear dpmaif reset reg */
#define DPMAIF_PD_RST_MASK     (1 << 2)
#define INFRA_RST0_REG_AO      (0x0140)
#define INFRA_RST1_REG_AO      (0x0144)
#define DPMAIF_AO_RST_MASK     (1 << 6)
#define INFRA_DPMAIF_CTRL_REG  (0xC00)
#define DPMAIF_IP_BUSY_MASK    (0x3 << 12)
#define SW_CG_2_STA            (0xAC)
#define SW_CG_3_STA            (0xC8)

#define INFRA_PROT_DPMAIF_BIT  (1 << 10)

#define INFRA_TOPAXI_PROTECTEN_1_SET      (0x2A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR      (0x2AC)
#define INFRA_TOPAXI_PROTECTEN_1          (0x250)
#define DPMAIF_SLEEP_PROTECT_CTRL         (0x1<<4)
#define INFRA_TOPAXI_PROTECT_READY_STA1_1 (0x258)



/***********************************************************************
 *
 *  dpmaif_ul
 *
 ***********************************************************************/
#define NRL2_DPMAIF_UL_ADD_DESC         (BASE_DPMAIF_UL + 0x00)
#define NRL2_DPMAIF_UL_RESTORE_RIDX     (BASE_DPMAIF_UL + 0x04)
#define NRL2_DPMAIF_UL_CHNL_ARB1        (BASE_DPMAIF_UL + 0x14)
#define NRL2_DPMAIF_UL_CACHE_CON0       (BASE_DPMAIF_UL + 0x70)
#define NRL2_DPMAIF_UL_RDMA_CFG0        (BASE_DPMAIF_UL + 0x74)
#define NRL2_DPMAIF_UL_DBG_STA0         (BASE_DPMAIF_UL + 0x80)
#define NRL2_DPMAIF_UL_DBG_STA1         (BASE_DPMAIF_UL + 0x84)
#define NRL2_DPMAIF_UL_DBG_STA2         (BASE_DPMAIF_UL + 0x88)
#define NRL2_DPMAIF_UL_DBG_STA3         (BASE_DPMAIF_UL + 0x8C)
#define NRL2_DPMAIF_UL_DBG_STA4         (BASE_DPMAIF_UL + 0x90)
#define NRL2_DPMAIF_UL_DBG_STA5         (BASE_DPMAIF_UL + 0x94)
#define NRL2_DPMAIF_UL_DBG_STA6         (BASE_DPMAIF_UL + 0x98)
#define NRL2_DPMAIF_UL_DBG_STA7         (BASE_DPMAIF_UL + 0x9C)
#define NRL2_DPMAIF_UL_DBG_STA8         (BASE_DPMAIF_UL + 0xA0)
#define NRL2_DPMAIF_UL_DBG_STA9         (BASE_DPMAIF_UL + 0xA4)
#define NRL2_DPMAIF_UL_RESERVE_RW       (BASE_DPMAIF_UL + 0xA8)
#define NRL2_DPMAIF_UL_RESERVE_AO_RW    (BASE_DPMAIF_UL + 0xAC)
#define NRL2_DPMAIF_UL_ADD_DESC_CH0     (BASE_DPMAIF_UL + 0xB0)
#define NRL2_DPMAIF_UL_ADD_DESC_CH1     (BASE_DPMAIF_UL + 0xB4)
#define NRL2_DPMAIF_UL_ADD_DESC_CH2     (BASE_DPMAIF_UL + 0xB8)
#define NRL2_DPMAIF_UL_ADD_DESC_CH3     (BASE_DPMAIF_UL + 0xBC)
#define NRL2_DPMAIF_UL_ADD_DESC_CH4     (BASE_DPMAIF_UL + 0xE0)


/***********************************************************************
 *
 *  dpmaif_dl
 *
 ***********************************************************************/
#define NRL2_DPMAIF_DL_BAT_INIT                (BASE_DPMAIF_DL  + 0x00)
#define NRL2_DPMAIF_DL_BAT_ADD                 (BASE_DPMAIF_DL  + 0x04)
#define NRL2_DPMAIF_DL_BAT_INIT_CON0           (BASE_DPMAIF_DL  + 0x08)
#define NRL2_DPMAIF_DL_BAT_INIT_CON1           (BASE_DPMAIF_DL  + 0x0C)
#define NRL2_DPMAIF_DL_BAT_INIT_CON2           (BASE_DPMAIF_DL  + 0x10)
#define NRL2_DPMAIF_DL_PIT_INIT_CON5           (BASE_DPMAIF_DL  + 0x14)
#define NRL2_DPMAIF_DL_STA13                   (BASE_DPMAIF_DL  + 0x18)
#define NRL2_DPMAIF_DL_STA14                   (BASE_DPMAIF_DL  + 0x1C)
#define NRL2_DPMAIF_DL_PIT_INIT                (BASE_DPMAIF_DL  + 0x20)
#define NRL2_DPMAIF_DL_PIT_ADD                 (BASE_DPMAIF_DL  + 0x24)
#define NRL2_DPMAIF_DL_PIT_INIT_CON0           (BASE_DPMAIF_DL  + 0x28)
#define NRL2_DPMAIF_DL_PIT_INIT_CON1           (BASE_DPMAIF_DL  + 0x2C)
#define NRL2_DPMAIF_DL_PIT_INIT_CON2           (BASE_DPMAIF_DL  + 0x30)
#define NRL2_DPMAIF_DL_PIT_INIT_CON3           (BASE_DPMAIF_DL  + 0x34)
#define NRL2_DPMAIF_DL_MISC_CON0               (BASE_DPMAIF_DL  + 0x40)
#define NRL2_DPMAIF_DL_STA12                   (BASE_DPMAIF_DL  + 0x4C)
#define NRL2_DPMAIF_DL_BAT_INIT_CON3           (BASE_DPMAIF_DL  + 0x50)
#define NRL2_DPMAIF_DL_PIT_INIT_CON4           (BASE_DPMAIF_DL  + 0x54)
#define NRL2_DPMAIF_DL_STA9                    (BASE_DPMAIF_DL  + 0x58)
#define NRL2_DPMAIF_DL_STA10                   (BASE_DPMAIF_DL  + 0x5C)
#define NRL2_DPMAIF_DL_STA11                   (BASE_DPMAIF_DL  + 0x60)
#define NRL2_DPMAIF_DL_FRG_STA5                (BASE_DPMAIF_DL  + 0x64)
#define NRL2_DPMAIF_DL_FRG_STA6                (BASE_DPMAIF_DL  + 0x68)
#define NRL2_DPMAIF_DL_DBG_STA16               (BASE_DPMAIF_DL  + 0x6C)
#define NRL2_DPMAIF_DL_DBG_FRG0                (BASE_DPMAIF_DL  + 0x70)
#define NRL2_DPMAIF_DL_DBG_FRG1                (BASE_DPMAIF_DL  + 0x74)
#define NRL2_DPMAIF_DL_DBG_FRG2                (BASE_DPMAIF_DL  + 0x78)
#define NRL2_DPMAIF_DL_DBG_FRG3                (BASE_DPMAIF_DL  + 0x7C)
#define NRL2_DPMAIF_DL_STA0                    (BASE_DPMAIF_DL  + 0x80)
#define NRL2_DPMAIF_DL_STA1                    (BASE_DPMAIF_DL  + 0x84)
#define NRL2_DPMAIF_DL_STA2                    (BASE_DPMAIF_DL  + 0x88)
#define NRL2_DPMAIF_DL_STA3                    (BASE_DPMAIF_DL  + 0x8C)
#define NRL2_DPMAIF_DL_STA4                    (BASE_DPMAIF_DL  + 0x90)
#define NRL2_DPMAIF_DL_STA5                    (BASE_DPMAIF_DL  + 0x94)
#define NRL2_DPMAIF_DL_STA6                    (BASE_DPMAIF_DL  + 0x98)
#define NRL2_DPMAIF_DL_STA7                    (BASE_DPMAIF_DL  + 0x9C)
#define NRL2_DPMAIF_DL_STA8                    (BASE_DPMAIF_DL  + 0xA0)
#define NRL2_DPMAIF_DL_DBG_STA15               (BASE_DPMAIF_DL  + 0xA4)
#define NRL2_DPMAIF_DL_RESERVE_RW              (BASE_DPMAIF_DL  + 0xA8)
#define NRL2_DPMAIF_DL_RESERVE_AO_RW           (BASE_DPMAIF_DL  + 0xAC)
#define NRL2_DPMAIF_DL_DBG_STA0                (BASE_DPMAIF_DL  + 0xB0)
#define NRL2_DPMAIF_DL_DBG_STA1                (BASE_DPMAIF_DL  + 0xB4)
#define NRL2_DPMAIF_DL_DBG_STA2                (BASE_DPMAIF_DL  + 0xB8)
#define NRL2_DPMAIF_DL_DBG_STA3                (BASE_DPMAIF_DL  + 0xBC)
#define NRL2_DPMAIF_DL_DBG_STA4                (BASE_DPMAIF_DL  + 0xC0)
#define NRL2_DPMAIF_DL_DBG_STA5                (BASE_DPMAIF_DL  + 0xC4)
#define NRL2_DPMAIF_DL_DBG_STA6                (BASE_DPMAIF_DL  + 0xC8)
#define NRL2_DPMAIF_DL_DBG_STA7                (BASE_DPMAIF_DL  + 0xCC)
#define NRL2_DPMAIF_DL_DBG_STA8                (BASE_DPMAIF_DL  + 0xD0)
#define NRL2_DPMAIF_DL_DBG_STA9                (BASE_DPMAIF_DL  + 0xD4)
#define NRL2_DPMAIF_DL_DBG_STA10               (BASE_DPMAIF_DL  + 0xD8)
#define NRL2_DPMAIF_DL_DBG_STA11               (BASE_DPMAIF_DL  + 0xDC)
#define NRL2_DPMAIF_DL_FRG_STA0                (BASE_DPMAIF_DL  + 0xE0)
#define NRL2_DPMAIF_DL_FRG_STA1                (BASE_DPMAIF_DL  + 0xE4)
#define NRL2_DPMAIF_DL_FRG_STA2                (BASE_DPMAIF_DL  + 0xE8)
#define NRL2_DPMAIF_DL_FRG_STA3                (BASE_DPMAIF_DL  + 0xEC)
#define NRL2_DPMAIF_DL_FRG_STA4                (BASE_DPMAIF_DL  + 0xF0)
#define NRL2_DPMAIF_DL_DBG_STA12               (BASE_DPMAIF_DL  + 0xF4)
#define NRL2_DPMAIF_DL_DBG_STA13               (BASE_DPMAIF_DL  + 0xF8)
#define NRL2_DPMAIF_DL_DBG_STA14               (BASE_DPMAIF_DL  + 0xFC)

/***********************************************************************
 *
 *  dpmaif_rwdma
 *
 ***********************************************************************/
#define NRL2_DPMAIF_RDMA_CON0                  (BASE_DPMAIF_RDMA + 0x000)
#define NRL2_DPMAIF_RDMA_CON1                  (BASE_DPMAIF_RDMA + 0x004)
#define NRL2_DPMAIF_RDMA_CON2                  (BASE_DPMAIF_RDMA + 0x008)
#define NRL2_DPMAIF_RDMA_CON3                  (BASE_DPMAIF_RDMA + 0x00C)
#define NRL2_DPMAIF_RDMA_CON4                  (BASE_DPMAIF_RDMA + 0x010)
#define NRL2_DPMAIF_RDMA_CON5                  (BASE_DPMAIF_RDMA + 0x014)
#define NRL2_DPMAIF_RDMA_CON6                  (BASE_DPMAIF_RDMA + 0x018)
#define NRL2_DPMAIF_RDMA_CON7                  (BASE_DPMAIF_RDMA + 0x01C)
#define NRL2_DPMAIF_RDMA_CON8                  (BASE_DPMAIF_RDMA + 0x020)
#define NRL2_DPMAIF_RDMA_CON9                  (BASE_DPMAIF_RDMA + 0x024)
#define NRL2_DPMAIF_RDMA_CON10                 (BASE_DPMAIF_RDMA + 0x028)
#define NRL2_DPMAIF_RDMA_CON11                 (BASE_DPMAIF_RDMA + 0x02C)
#define NRL2_DPMAIF_RDMA_CON12                 (BASE_DPMAIF_RDMA + 0x030)
#define NRL2_DPMAIF_RDMA_CON13                 (BASE_DPMAIF_RDMA + 0x034)
#define NRL2_DPMAIF_RDMA_CON14                 (BASE_DPMAIF_RDMA + 0x038)
#define NRL2_DPMAIF_RDMA_CON15                 (BASE_DPMAIF_RDMA + 0x03C)
#define NRL2_DPMAIF_RDMA_CON16                 (BASE_DPMAIF_RDMA + 0x040)
#define NRL2_DPMAIF_RDMA_CON17                 (BASE_DPMAIF_RDMA + 0x044)
#define NRL2_DPMAIF_RDMA_CON18                 (BASE_DPMAIF_RDMA + 0x048)
#define NRL2_DPMAIF_RDMA_EXCEP_STA             (BASE_DPMAIF_RDMA + 0x080)
#define NRL2_DPMAIF_RDMA_EXCEP_MASK            (BASE_DPMAIF_RDMA + 0x084)
#define NRL2_DPMAIF_RDMA_DBG_CON0              (BASE_DPMAIF_RDMA + 0x090)
#define NRL2_DPMAIF_RDMA_DBG_CON1              (BASE_DPMAIF_RDMA + 0x094)
#define NRL2_DPMAIF_RDMA_DBG_CON2              (BASE_DPMAIF_RDMA + 0x098)
#define NRL2_DPMAIF_RDMA_DBG_CON3              (BASE_DPMAIF_RDMA + 0x09C)
#define NRL2_DPMAIF_RDMA_DBG_CON4              (BASE_DPMAIF_RDMA + 0x0A0)
#define NRL2_DPMAIF_RDMA_DBG_CON5              (BASE_DPMAIF_RDMA + 0x0A4)
#define NRL2_DPMAIF_RDMA_DBG_CON6              (BASE_DPMAIF_RDMA + 0x0A8)
#define NRL2_DPMAIF_RDMA_DBG_CON7              (BASE_DPMAIF_RDMA + 0x0AC)
#define NRL2_DPMAIF_RDMA_DBG_CON8              (BASE_DPMAIF_RDMA + 0x0B0)
#define NRL2_DPMAIF_RDMA_DBG_CON9              (BASE_DPMAIF_RDMA + 0x0B4)
#define NRL2_DPMAIF_RDMA_DBG_CON10             (BASE_DPMAIF_RDMA + 0x0B8)
#define NRL2_DPMAIF_RDMA_DBG_CON11             (BASE_DPMAIF_RDMA + 0x0BC)
#define NRL2_DPMAIF_RDMA_DBG_CON12             (BASE_DPMAIF_RDMA + 0x0C0)
#define NRL2_DPMAIF_RDMA_DBG_CON13             (BASE_DPMAIF_RDMA + 0x0C4)

#define NRL2_DPMAIF_WDMA_WR_CMD_CON0           (BASE_DPMAIF_WDMA + 0x000)
#define NRL2_DPMAIF_WDMA_WR_CMD_CON1           (BASE_DPMAIF_WDMA + 0x004)
#define NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON2      (BASE_DPMAIF_WDMA + 0x010)
#define NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3      (BASE_DPMAIF_WDMA + 0x014)


/***********************************************************************
 *
 *  dpmaif_ap_misc
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AP_MISC_AP_L1TISAR0        (BASE_DPMAIF_AP_MISC + 0x10)
#define NRL2_DPMAIF_AP_MISC_AP_L1TIMR0         (BASE_DPMAIF_AP_MISC + 0x14)
#define NRL2_DPMAIF_AP_MISC_BUS_CONFIG0        (BASE_DPMAIF_AP_MISC + 0x20)
#define NRL2_DPMAIF_AP_MISC_TOP_AP_CFG         (BASE_DPMAIF_AP_MISC + 0x24)
#define NRL2_DPMAIF_AP_MISC_EMI_BUS_STATUS0    (BASE_DPMAIF_AP_MISC + 0x30)
#define NRL2_DPMAIF_AP_MISC_PCIE_BUS_STATUS0   (BASE_DPMAIF_AP_MISC + 0x34)
#define NRL2_DPMAIF_AP_MISC_AP_DMA_ERR_STA     (BASE_DPMAIF_AP_MISC + 0x40)
#define NRL2_DPMAIF_AP_MISC_APDL_L2TISAR0      (BASE_DPMAIF_AP_MISC + 0x50)
#define NRL2_DPMAIF_AP_MISC_AP_IP_BUSY         (BASE_DPMAIF_AP_MISC + 0x60)
#define NRL2_DPMAIF_AP_MISC_CG_EN              (BASE_DPMAIF_AP_MISC + 0x68)
#define NRL2_DPMAIF_AP_MISC_CODA_VER           (BASE_DPMAIF_AP_MISC + 0x6C)
#define NRL2_DPMAIF_AP_MISC_APB_DBG_SRAM       (BASE_DPMAIF_AP_MISC + 0x70)

#define NRL2_DPMAIF_AP_MISC_HPC_SRAM_USAGE     (BASE_DPMAIF_AP_MISC + 0x74)
#define NRL2_DPMAIF_AP_MISC_MPIT_CACHE_INV     (BASE_DPMAIF_AP_MISC + 0x80)
#define NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG      (BASE_DPMAIF_AP_MISC + 0x90)

#define NRL2_DPMAIF_AP_MISC_MEM_CLR            (BASE_DPMAIF_AP_MISC + 0x94)


/***********************************************************************
 *
 *  dpmaif_ul_ao
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_UL_INIT_SET                (BASE_DPMAIF_AO_UL + 0x00)
#define NRL2_DPMAIF_AO_UL_CHNL_ARB0               (BASE_DPMAIF_AO_UL + 0x1C)
#define NRL2_DPMAIF_AO_UL_AP_L2TIMR0              (BASE_DPMAIF_AO_UL + 0x80)
#define NRL2_DPMAIF_AO_UL_AP_L2TIMCR0             (BASE_DPMAIF_AO_UL + 0x84)
#define NRL2_DPMAIF_AO_UL_AP_L2TIMSR0             (BASE_DPMAIF_AO_UL + 0x88)
#define NRL2_DPMAIF_AO_UL_AP_L1TIMR0              (BASE_DPMAIF_AO_UL + 0x8C)
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMR0            (BASE_DPMAIF_AO_UL + 0x90)
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0           (BASE_DPMAIF_AO_UL + 0x94)
#define NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0           (BASE_DPMAIF_AO_UL + 0x98)
#define NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK   (BASE_DPMAIF_AO_UL + 0x9C)

#define NRL2_DPMAIF_AO_UL_CHNL0_STA0              (BASE_DPMAIF_AO_UL + 0x60)


/***********************************************************************
 *
 *  dpmaif_dl_ao
 *
 ***********************************************************************/
#define NRL2_DPMAIF_AO_DL_INIT_SET                (BASE_DPMAIF_AO_DL + 0x00)
#define NRL2_DPMAIF_AO_DL_INIT_RESTORE_CFG        (BASE_DPMAIF_AO_DL + 0x04)
#define NRL2_DPMAIF_AO_DL_MISC_IRQ_MASK           (BASE_DPMAIF_AO_DL + 0x0C)
#define NRL2_DPMAIF_AO_DL_LROPIT_INIT_CON5        (BASE_DPMAIF_AO_DL + 0x28)
#define NRL2_DPMAIF_AO_DL_LROPIT_TRIG_THRES       (BASE_DPMAIF_AO_DL + 0x34)

#define NRL2_DPMAIF_AO_DL_MISC_AO_ABI_EN          (BASE_DPMAIF_AO_DL + 0x60)
#define NRL2_DPMAIF_AO_DL_MISC_AO_SBI_EN          (BASE_DPMAIF_AO_DL + 0x64)
#define NRL2_DPMAIF_AO_DL_AO_PA_OFS_L             (BASE_DPMAIF_AO_DL + 0x68)
#define NRL2_DPMAIF_AO_DL_AO_PA_OFS_H             (BASE_DPMAIF_AO_DL + 0x6C)
#define NRL2_DPMAIF_AO_DL_AO_SCP_DL_MASK          (BASE_DPMAIF_AO_DL + 0x70)
#define NRL2_DPMAIF_AO_DL_AO_SCP_UL_MASK          (BASE_DPMAIF_AO_DL + 0x74)

#define NRL2_DPMAIF_AO_DL_LRO_STA0                (BASE_DPMAIF_PD_SRAM_DL + 0x90)
#define NRL2_DPMAIF_AO_DL_LRO_STA1                (BASE_DPMAIF_PD_SRAM_DL + 0x94)
#define NRL2_DPMAIF_AO_DL_LRO_STA2                (BASE_DPMAIF_PD_SRAM_DL + 0x98)
#define NRL2_DPMAIF_AO_DL_LRO_STA3                (BASE_DPMAIF_PD_SRAM_DL + 0x9C)
#define NRL2_DPMAIF_AO_DL_LRO_STA4                (BASE_DPMAIF_PD_SRAM_DL + 0xA0)
#define NRL2_DPMAIF_AO_DL_LRO_STA5                (BASE_DPMAIF_PD_SRAM_DL + 0xA4)
#define NRL2_DPMAIF_AO_DL_LRO_STA6                (BASE_DPMAIF_PD_SRAM_DL + 0xA8)
#define NRL2_DPMAIF_AO_DL_LRO_STA8                (BASE_DPMAIF_PD_SRAM_DL + 0xB0)
#define NRL2_DPMAIF_AO_DL_LRO_STA9                (BASE_DPMAIF_PD_SRAM_DL + 0xB4)
#define NRL2_DPMAIF_AO_DL_LRO_STA10               (BASE_DPMAIF_PD_SRAM_DL + 0xB8)
#define NRL2_DPMAIF_AO_DL_LRO_STA11               (BASE_DPMAIF_PD_SRAM_DL + 0xBC)
#define NRL2_DPMAIF_AO_DL_LRO_STA12               (BASE_DPMAIF_PD_SRAM_DL + 0xC0)
#define NRL2_DPMAIF_AO_DL_LRO_STA13               (BASE_DPMAIF_PD_SRAM_DL + 0xC4)
#define NRL2_DPMAIF_AO_DL_LRO_STA14               (BASE_DPMAIF_PD_SRAM_DL + 0xC8)

#define NRL2_DPMAIF_AO_DL_RDY_CHK_THRES           (BASE_DPMAIF_AO_DL + 0x0C)



/***********************************************************************
 *
 *  dpmaif_hpc
 *
 ***********************************************************************/
#define NRL2_DPMAIF_HPC_ENTRY_STS0                (BASE_DPMAIF_MMW_HPC  + 0x004)
#define NRL2_DPMAIF_HPC_ENTRY_STS1                (BASE_DPMAIF_MMW_HPC  + 0x008)
#define NRL2_DPMAIF_HPC_CI_CO0                    (BASE_DPMAIF_MMW_HPC  + 0x010)
#define NRL2_DPMAIF_HPC_RULE_TGL                  (BASE_DPMAIF_MMW_HPC  + 0x038)
#define NRL2_DPMAIF_HPC_HASH_0TO3                 (BASE_DPMAIF_MMW_HPC  + 0x040)
#define NRL2_DPMAIF_HPC_OPT_IDX01                 (BASE_DPMAIF_MMW_HPC  + 0x050)
/***********************************************************************
 *
 *  dpmaif_lro
 *
 ***********************************************************************/
#define NRL2_DPMAIF_DL_LROPIT_INIT             (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x00)
#define NRL2_DPMAIF_DL_LROPIT_ADD              (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x10)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON0        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x14)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON1        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x18)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON2        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x1C)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON5        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x28)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON3        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x20)
#define NRL2_DPMAIF_DL_LROPIT_INIT_CON4        (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x24)
#define NRL2_DPMAIF_DL_LROPIT_SW_TRIG          (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x30)
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT5         (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x50)
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT6         (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x54)
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT7         (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x58)
#define NRL2_DPMAIF_DL_LROPIT_TIMEOUT8         (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x5C)
#define NRL2_DPMAIF_DL_LRO_DBG_SEL             (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x64)
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_SEL       (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x68)
#define NRL2_DPMAIF_DL_LRO_DBG_OUT0            (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x6C)
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT0      (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x70)
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT1      (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x74)
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT2      (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x78)
#define NRL2_DPMAIF_DL_LRO_CACHE_DBG_OUT3      (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x7C)
#define NRL2_DPMAIF_DL_LRO_STA0                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x80)
#define NRL2_DPMAIF_DL_LRO_STA1                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x84)
#define NRL2_DPMAIF_DL_LRO_STA2                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x88)
#define NRL2_DPMAIF_DL_LRO_STA3                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x8C)
#define NRL2_DPMAIF_DL_LRO_STAA                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC0)
#define NRL2_DPMAIF_DL_LRO_STA4                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x90)
#define NRL2_DPMAIF_DL_LRO_STAB                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC4)
#define NRL2_DPMAIF_DL_LRO_STA5                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x94)
#define NRL2_DPMAIF_DL_LRO_STA6                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x98)
#define NRL2_DPMAIF_DL_LRO_STA7                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x9C)
#define NRL2_DPMAIF_DL_LRO_STA8                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA0)
#define NRL2_DPMAIF_DL_LRO_STAC                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XC8)
#define NRL2_DPMAIF_DL_LRO_STA9                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA4)
#define NRL2_DPMAIF_DL_LRO_STAD                (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0XCC)
#define NRL2_DPMAIF_DL_AGG_STA                 (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xA8)
#define NRL2_DPMAIF_DL_LROPIT_TIMER0           (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xAC)
#define NRL2_DPMAIF_DL_LROPIT_TIMER1           (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB0)
#define NRL2_DPMAIF_DL_LROPIT_TIMER2           (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB4)
#define NRL2_DPMAIF_DL_LROPIT_TIMER3           (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xB8)
#define NRL2_DPMAIF_DL_LROPIT_TIMER4           (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xBC)
#define NRL2_DPMAIF_DL_AGG_WIDX1               (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD0)
#define NRL2_DPMAIF_DL_AGG_WIDX2               (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD4)
#define NRL2_DPMAIF_DL_AGG_WIDX3               (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xD8)
#define NRL2_DPMAIF_DL_AGG_WIDX4               (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xDC)
#define NRL2_DPMAIF_DL_AGG_STA_IDX             (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0xE0)
#define NRL2_DPMAIF_DL_LROTIMER_GATED          (BASE_DPMAIF_DL_LRO_MERCURY_REMOVEAO_IDX  + 0x340)



#define NRL2_DPMAIF_MISC_AO_CFG0                  (BASE_DPMAIF_DL_AO_CFG + 0x00)
#define NRL2_DPMAIF_MISC_AO_CFG1                  (BASE_DPMAIF_DL_AO_CFG + 0x04)
#define NRL2_DPMAIF_MISC_AO_REMAP_DOMAIN          (BASE_DPMAIF_DL_AO_CFG + 0x20)
#define NRL2_DPMAIF_MISC_AO_REMAP_CACHE           (BASE_DPMAIF_DL_AO_CFG + 0x24)
#define NRL2_DPMAIF_MISC_AO_REMAP_ALIGN           (BASE_DPMAIF_DL_AO_CFG + 0x28)
#define NRL2_DPMAIF_MISC_AO_REMAP_LOCK            (BASE_DPMAIF_DL_AO_CFG + 0x2C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPA      (BASE_DPMAIF_DL_AO_CFG + 0x30)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPB      (BASE_DPMAIF_DL_AO_CFG + 0x34)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPC      (BASE_DPMAIF_DL_AO_CFG + 0x38)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPD      (BASE_DPMAIF_DL_AO_CFG + 0x3C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPA      (BASE_DPMAIF_DL_AO_CFG + 0x40)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPB      (BASE_DPMAIF_DL_AO_CFG + 0x44)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPC      (BASE_DPMAIF_DL_AO_CFG + 0x48)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPD      (BASE_DPMAIF_DL_AO_CFG + 0x4C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPA      (BASE_DPMAIF_DL_AO_CFG + 0x50)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPB      (BASE_DPMAIF_DL_AO_CFG + 0x54)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPC      (BASE_DPMAIF_DL_AO_CFG + 0x58)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPD      (BASE_DPMAIF_DL_AO_CFG + 0x5C)
#define NRL2_DPMAIF_AXI_MAS_SECURE                (BASE_DPMAIF_DL_AO_CFG + 0x60)

#define NRL2_DPMAIF_PD_MD_MISC_MD_L1TIMSR0        (BASE_DPMAIF_PD_MD_MISC + 0x001C)

#define NRL2_DPMAIF_PD_MD_IP_BUSY                 (BASE_DPMAIF_PD_MD_MISC + 0x0000)
#define NRL2_DPMAIF_PD_MD_IP_BUSY_MASK            (BASE_DPMAIF_PD_MD_MISC + 0x0040)
#define NRL2_DPMAIF_PD_MD_DL_RB_PIT_INIT          (BASE_DPMAIF_PD_MD_MISC + 0x0100)


/***********************************************************************
 *
 *  PD_SRAM_MISC
 *
 ***********************************************************************/
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP0_2             (BASE_DPMAIF_PD_SRAM_MISC + 0x0C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP3_5             (BASE_DPMAIF_PD_SRAM_MISC + 0x10)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP6_7_BANK1_MAP0  (BASE_DPMAIF_PD_SRAM_MISC + 0x14)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP1_3             (BASE_DPMAIF_PD_SRAM_MISC + 0x18)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP4_6             (BASE_DPMAIF_PD_SRAM_MISC + 0x1C)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP7_BANK4_MAP0_1  (BASE_DPMAIF_PD_SRAM_MISC + 0x20)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP2_4             (BASE_DPMAIF_PD_SRAM_MISC + 0x24)
#define NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP5_7             (BASE_DPMAIF_PD_SRAM_MISC + 0x28)


#define DPMAIF_UL_ALL_QUE_ARB_EN    (0xF << 8)


#define DPMAIF_HW_CHK_RB_PIT_NUM   64

#define DPMAIF_CHK_RB_PITNUM_MSK   0x000000FF

/*BASE_DPMAIF_WDMA*/
/*BASE_NADDR_NRL2_DPMAIF_WDMA*/
#define DPMAIF_DL_WDMA_CTRL_OSTD_OFST (28)
#define DPMAIF_DL_WDMA_CTRL_OSTD_MSK (0xF)
#define DPMAIF_DL_WDMA_CTRL_OSTD_VALUE (0xE)

#define DPMAIF_AWDOMAIN_BIT_MSK 0xF
#define DPMAIF_ARDOMAIN_BIT_MSK 0xF
#define DPMAIF_AWDOMAIN_BIT_OFT 0
#define DPMAIF_ARDOMAIN_BIT_OFT 8

#define DPMAIF_CACHE_BANK0_BIT_MSK 0x3F
#define DPMAIF_CACHE_BANK1_BIT_MSK 0x3F
#define DPMAIF_CACHE_BANK0_BIT_OFT 0
#define DPMAIF_CACHE_BANK1_BIT_OFT 8

#define DP_DOMAIN_ID 1
#define DP_BANK0_ID 6
#define DP_BANK1_ID 7

#define DPMAIF_MD_AO_REMAP_ENABLE (1 << 0)


#define DPMAIF_MEM_CLR_MASK             (1 << 0)
#define DPMAIF_SRAM_SYNC_MASK           (1 << 0)
#define DPMAIF_UL_INIT_DONE_MASK        (1 << 0)
#define DPMAIF_DL_INIT_DONE_MASK        (1 << 0)



/*DPMAIF PD AP MSIC/AO UL MISC CONFIG*/

#define DPMAIF_PD_AP_DL_L2TICR0           NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0
#define DPMAIF_AO_DL_RDY_CHK_THRES        NRL2_DPMAIF_AO_DL_RDY_CHK_THRES

#define DPMAIF_PD_DL_PIT_INIT             NRL2_DPMAIF_DL_PIT_INIT
#define DPMAIF_PD_DL_PIT_INIT_CON0        NRL2_DPMAIF_DL_PIT_INIT_CON0
#define DPMAIF_PD_DL_PIT_INIT_CON1        NRL2_DPMAIF_DL_PIT_INIT_CON1
#define DPMAIF_PD_DL_PIT_INIT_CON2        NRL2_DPMAIF_DL_PIT_INIT_CON2
#define DPMAIF_PD_DL_PIT_INIT_CON3        NRL2_DPMAIF_DL_PIT_INIT_CON3

#define DPMAIF_PD_DL_BAT_INIT             NRL2_DPMAIF_DL_BAT_INIT
#define DPMAIF_PD_DL_BAT_INIT_CON0        NRL2_DPMAIF_DL_BAT_INIT_CON0
#define DPMAIF_PD_DL_BAT_INIT_CON1        NRL2_DPMAIF_DL_BAT_INIT_CON1
#define DPMAIF_PD_DL_BAT_INIT_CON2        NRL2_DPMAIF_DL_BAT_INIT_CON2
#define DPMAIF_PD_DL_BAT_INIT_CON3        NRL2_DPMAIF_DL_BAT_INIT_CON3
#define DPMAIF_PD_DL_BAT_ADD              NRL2_DPMAIF_DL_BAT_ADD

#define DPMAIF_PD_AP_IP_BUSY              NRL2_DPMAIF_AP_MISC_AP_IP_BUSY

#define DPMAIF_PD_AP_DL_L2TISAR0          NRL2_DPMAIF_AP_MISC_APDL_L2TISAR0

#define DPMAIF_PD_AP_UL_L2TISAR0          (BASE_DPMAIF_AP_MISC + 0x00)


#define UL_INT_DONE_OFFSET      0

/* === RX interrupt mask === */
#define DPMAIF_DL_INT_ERR_MSK                    (0x07 << 1)
#define DPMAIF_DL_INT_EMPTY_MSK                  (0x03 << 4)
#define DPMAIF_DL_INT_MTU_ERR_MSK                (0x01 << 6)
#define DPMAIF_DL_INT_QDONE_MSK                  (0x01 << 0)
#define DPMAIF_DL_INT_SKB_LEN_ERR(q_num)         (1 << 1)
#define DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num)      (1 << 2)
#define DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num)      (1 << 3)

#define AP_DL_L2INTR_ERR_En_Msk	DPMAIF_DL_INT_MTU_ERR_MSK

/* DPMAIF_DL_INT_EMPTY_MSK | */
#define AP_DL_L2INTR_En_Msk \
	(AP_DL_L2INTR_ERR_En_Msk | DPMAIF_DL_INT_QDONE_MSK)


/* DL */
/* DPMAIF_PD_DL_BAT/PIT_ADD */
#define DPMAIF_DL_ADD_UPDATE                (1 << 31)
#define DPMAIF_DL_ADD_NOT_READY             (1 << 31)
#define DPMAIF_DL_BAT_FRG_ADD               (1 << 16)

#define DPMAIF_DL_BAT_INIT_ALLSET           (1 << 0)
#define DPMAIF_DL_BAT_FRG_INIT              (1 << 16)
#define DPMAIF_DL_BAT_INIT_EN               (1 << 31)
#define DPMAIF_DL_BAT_INIT_NOT_READY        (1 << 31)
#define DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT  (0 << 0)

#define DPMAIF_DL_PIT_INIT_ALLSET           (1 << 0)
#define DPMAIF_DL_PIT_INIT_ONLY_ENABLE_BIT  (0 << 0)
#define DPMAIF_DL_PIT_INIT_EN               (1 << 31)
#define DPMAIF_DL_PIT_INIT_NOT_READY        (1 << 31)

#define DPMAIF_PKT_ALIGN64_MODE        0
#define DPMAIF_PKT_ALIGN128_MODE       1

#define DPMAIF_BAT_REMAIN_SZ_BASE      16
#define DPMAIF_BAT_BUFFER_SZ_BASE      128
#define DPMAIF_FRG_BAT_BUFFER_SZ_BASE  128

#define DPMAIF_PIT_EN_MSK              0x01

#define DPMAIF_PIT_ADDRH_MSK           0xFF000000

#define DPMAIF_BAT_EN_MSK              (1 << 16)
#define DPMAIF_BAT_SIZE_MSK            0xFFFF
#define DPMAIF_BAT_ADDRH_MSK           0xFF000000

#define DPMAIF_BAT_BID_MAXCNT_MSK      0xFFFF0000
#define DPMAIF_BAT_REMAIN_MINSZ_MSK    0x0000FF00
#define DPMAIF_PIT_CHK_NUM_MSK         0xFF000000
#define DPMAIF_BAT_BUF_SZ_MSK          0x0001FF00
#define DPMAIF_BAT_RSV_LEN_MSK         0x000000FF
#define DPMAIF_PKT_ALIGN_MSK           (0x3 << 22)


#define DPMAIF_PKT_ALIGN_EN            (1 << 23)

#define DPMAIF_DL_BAT_WRIDX_MSK        0xFFFF

#define DPMAIF_BAT_CHECK_THRES_MSK     (0x3F << 16)
#define DPMAIF_FRG_CHECK_THRES_MSK     (0xFF)
#define DPMAIF_AO_DL_ISR_MSK           (0x7F)

#define DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK   (1 << 28)
#define DPMAIF_FRG_BAT_BUF_FEATURE_EN       (1 << 28)
#define DPMAIF_FRG_BAT_BUF_SZ_MSK           (0xFF << 8)
#define DPMAIF_CHKSUM_ON_MSK                (1 << 31)


/*DPMAIF_PD_DL_DBG_STA7*/
#define DPMAIF_DL_FIFO_PUSH_RIDX       (0x3F << 20)
#define DPMAIF_DL_FIFO_PUSH_SHIFT      20
#define DPMAIF_DL_FIFO_PUSH_MSK        0x3F

#define DPMAIF_DL_FIFO_PUSH_IDLE_STS   (1 << 16)

#define DPMAIF_DL_FIFO_POP_RIDX        (0x3F << 5)
#define DPMAIF_DL_FIFO_POP_SHIFT       5
#define DPMAIF_DL_FIFO_POP_MSK         0x3F

#define DPMAIF_DL_FIFO_POP_IDLE_STS    (1 << 0)

#define DPMAIF_DL_FIFO_IDLE_STS  (DPMAIF_DL_FIFO_POP_IDLE_STS | DPMAIF_DL_FIFO_PUSH_IDLE_STS)


#define DPMAIF_DRB_ADDRH_MSK           0xFF000000
#define DPMAIF_DRB_SIZE_MSK            0x0000FFFF


#define DPMAIF_UL_ADD_NOT_READY        (1 << 31)
#define DPMAIF_UL_ADD_UPDATE           (1 << 31)
#define DPMAIF_ULQ_ADD_DESC_CH_n(q_num)     \
	((NRL2_DPMAIF_UL_ADD_DESC_CH0) + (0x04 * (q_num)))

#define DP_DL_INT_LRO0_QDONE_SET       (0x01 << 13)
#define DP_DL_INT_LRO1_QDONE_SET       (0x01 << 14)

#define DPMAIF_DL_MAX_BAT_SKB_CNT_STS  (1 << 15)
#define DPMAIF_DL_MAX_BAT_SKB_CNT_MSK  (0xFFFF0000)

#endif /* __CCCI_DPMAIF_REG_COM_H__ */
