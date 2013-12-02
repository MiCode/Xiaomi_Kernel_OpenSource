/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Bus-Access-Manager (BAM) Hardware manager. */

#include <linux/types.h>	/* u32 */
#include <linux/kernel.h>	/* pr_info() */
#include <linux/io.h>		/* ioread32() */
#include <linux/bitops.h>	/* find_first_bit() */
#include <linux/errno.h>	/* ENODEV */
#include <linux/memory.h>

#include "bam.h"
#include "sps_bam.h"

/**
 *  Valid BAM Hardware version.
 *
 */
#define BAM_MIN_VERSION 2
#define BAM_MAX_VERSION 0x2f

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM

/* Maximum number of execution environment */
#define BAM_MAX_EES 8

#ifdef	CONFIG_SPS_SUPPORT_4K_GROUP

/**
  *  BAM Hardware registers.
  *
  */
#define CTRL                        (0x0)
#define REVISION                    (0x1000)
#define SW_REVISION                 (0x1004)
#define NUM_PIPES                   (0x1008)
#define TIMER                       (0x40)
#define TIMER_CTRL                  (0x44)
#define DESC_CNT_TRSHLD             (0x8)
#define IRQ_SRCS                    (0x3010)
#define IRQ_SRCS_MSK                (0x3014)
#define IRQ_SRCS_UNMASKED           (0x3018)
#define IRQ_STTS                    (0x14)
#define IRQ_CLR                     (0x18)
#define IRQ_EN                      (0x1c)
#define AHB_MASTER_ERR_CTRLS        (0x1024)
#define AHB_MASTER_ERR_ADDR         (0x1028)
#define AHB_MASTER_ERR_ADDR_MSB     (0x1104)
#define AHB_MASTER_ERR_DATA         (0x102c)
#define TRUST_REG                   (0x2000)
#define TEST_BUS_SEL                (0x1010)
#define TEST_BUS_REG                (0x1014)
#define CNFG_BITS                   (0x7c)
#define IRQ_SRCS_EE(n)             (0x3000 + 4096 * (n))
#define IRQ_SRCS_MSK_EE(n)         (0x3004 + 4096 * (n))
#define IRQ_SRCS_UNMASKED_EE(n)    (0x3008 + 4096 * (n))
#define PIPE_ATTR_EE(n)            (0x300c + 4096 * (n))

#define P_CTRL(n)                  (0x13000 + 4096 * (n))
#define P_RST(n)                   (0x13004 + 4096 * (n))
#define P_HALT(n)                  (0x13008 + 4096 * (n))
#define P_IRQ_STTS(n)              (0x13010 + 4096 * (n))
#define P_IRQ_CLR(n)               (0x13014 + 4096 * (n))
#define P_IRQ_EN(n)                (0x13018 + 4096 * (n))
#define P_TIMER(n)                 (0x1301c + 4096 * (n))
#define P_TIMER_CTRL(n)            (0x13020 + 4096 * (n))
#define P_PRDCR_SDBND(n)           (0x13024 + 4096 * (n))
#define P_CNSMR_SDBND(n)           (0x13028 + 4096 * (n))
#define P_TRUST_REG(n)             (0x2020 + 4 * (n))
#define P_EVNT_DEST_ADDR(n)        (0x1382c + 4096 * (n))
#define P_EVNT_DEST_ADDR_MSB(n)    (0x13934 + 4096 * (n))
#define P_EVNT_REG(n)              (0x13818 + 4096 * (n))
#define P_SW_OFSTS(n)              (0x13800 + 4096 * (n))
#define P_DATA_FIFO_ADDR(n)        (0x13824 + 4096 * (n))
#define P_DATA_FIFO_ADDR_MSB(n)    (0x13924 + 4096 * (n))
#define P_DESC_FIFO_ADDR(n)        (0x1381c + 4096 * (n))
#define P_DESC_FIFO_ADDR_MSB(n)    (0x13914 + 4096 * (n))
#define P_EVNT_GEN_TRSHLD(n)       (0x13828 + 4096 * (n))
#define P_FIFO_SIZES(n)            (0x13820 + 4096 * (n))
#define P_RETR_CNTXT(n)            (0x13834 + 4096 * (n))
#define P_SI_CNTXT(n)              (0x13838 + 4096 * (n))
#define P_DF_CNTXT(n)              (0x13830 + 4096 * (n))
#define P_AU_PSM_CNTXT_1(n)        (0x13804 + 4096 * (n))
#define P_PSM_CNTXT_2(n)           (0x13808 + 4096 * (n))
#define P_PSM_CNTXT_3(n)           (0x1380c + 4096 * (n))
#define P_PSM_CNTXT_3_MSB(n)       (0x13904 + 4096 * (n))
#define P_PSM_CNTXT_4(n)           (0x13810 + 4096 * (n))
#define P_PSM_CNTXT_5(n)           (0x13814 + 4096 * (n))

#else

/**
 *  BAM Hardware registers.
 *
 */
#define CTRL                        (0x0)
#define REVISION                    (0x4)
#define SW_REVISION                 (0x80)
#define NUM_PIPES                   (0x3c)
#define TIMER                       (0x40)
#define TIMER_CTRL                  (0x44)
#define DESC_CNT_TRSHLD             (0x8)
#define IRQ_SRCS                    (0xc)
#define IRQ_SRCS_MSK                (0x10)
#define IRQ_SRCS_UNMASKED           (0x30)
#define IRQ_STTS                    (0x14)
#define IRQ_CLR                     (0x18)
#define IRQ_EN                      (0x1c)
#define AHB_MASTER_ERR_CTRLS        (0x24)
#define AHB_MASTER_ERR_ADDR         (0x28)
#define AHB_MASTER_ERR_ADDR_MSB     (0x104)
#define AHB_MASTER_ERR_DATA         (0x2c)
#define TRUST_REG                   (0x70)
#define TEST_BUS_SEL                (0x74)
#define TEST_BUS_REG                (0x78)
#define CNFG_BITS                   (0x7c)
#define IRQ_SRCS_EE(n)             (0x800 + 128 * (n))
#define IRQ_SRCS_MSK_EE(n)         (0x804 + 128 * (n))
#define IRQ_SRCS_UNMASKED_EE(n)    (0x808 + 128 * (n))
#define PIPE_ATTR_EE(n)            (0x80c + 128 * (n))

#define P_CTRL(n)                  (0x1000 + 4096 * (n))
#define P_RST(n)                   (0x1004 + 4096 * (n))
#define P_HALT(n)                  (0x1008 + 4096 * (n))
#define P_IRQ_STTS(n)              (0x1010 + 4096 * (n))
#define P_IRQ_CLR(n)               (0x1014 + 4096 * (n))
#define P_IRQ_EN(n)                (0x1018 + 4096 * (n))
#define P_TIMER(n)                 (0x101c + 4096 * (n))
#define P_TIMER_CTRL(n)            (0x1020 + 4096 * (n))
#define P_PRDCR_SDBND(n)           (0x1024 + 4096 * (n))
#define P_CNSMR_SDBND(n)           (0x1028 + 4096 * (n))
#define P_TRUST_REG(n)             (0x1030 + 4096 * (n))
#define P_EVNT_DEST_ADDR(n)        (0x182c + 4096 * (n))
#define P_EVNT_DEST_ADDR_MSB(n)    (0x1934 + 4096 * (n))
#define P_EVNT_REG(n)              (0x1818 + 4096 * (n))
#define P_SW_OFSTS(n)              (0x1800 + 4096 * (n))
#define P_DATA_FIFO_ADDR(n)        (0x1824 + 4096 * (n))
#define P_DATA_FIFO_ADDR_MSB(n)    (0x1924 + 4096 * (n))
#define P_DESC_FIFO_ADDR(n)        (0x181c + 4096 * (n))
#define P_DESC_FIFO_ADDR_MSB(n)    (0x1914 + 4096 * (n))
#define P_EVNT_GEN_TRSHLD(n)       (0x1828 + 4096 * (n))
#define P_FIFO_SIZES(n)            (0x1820 + 4096 * (n))
#define P_RETR_CNTXT(n)            (0x1834 + 4096 * (n))
#define P_SI_CNTXT(n)              (0x1838 + 4096 * (n))
#define P_DF_CNTXT(n)              (0x1830 + 4096 * (n))
#define P_AU_PSM_CNTXT_1(n)        (0x1804 + 4096 * (n))
#define P_PSM_CNTXT_2(n)           (0x1808 + 4096 * (n))
#define P_PSM_CNTXT_3(n)           (0x180c + 4096 * (n))
#define P_PSM_CNTXT_3_MSB(n)       (0x1904 + 4096 * (n))
#define P_PSM_CNTXT_4(n)           (0x1810 + 4096 * (n))
#define P_PSM_CNTXT_5(n)           (0x1814 + 4096 * (n))

#endif /* CONFIG_SPS_SUPPORT_4K_GROUP */

/**
 *  BAM Hardware registers bitmask.
 *  format: <register>_<field>
 *
 */
/* CTRL */
#define BAM_MESS_ONLY_CANCEL_WB               0x100000
#define CACHE_MISS_ERR_RESP_EN                 0x80000
#define LOCAL_CLK_GATING                       0x60000
#define IBC_DISABLE                            0x10000
#define BAM_CACHED_DESC_STORE                   0x8000
#define BAM_DESC_CACHE_SEL                      0x6000
#define BAM_EN_ACCUM                              0x10
#define BAM_EN                                     0x2
#define BAM_SW_RST                                 0x1

/* REVISION */
#define BAM_INACTIV_TMR_BASE                0xff000000
#define BAM_CMD_DESC_EN                       0x800000
#define BAM_DESC_CACHE_DEPTH                  0x600000
#define BAM_NUM_INACTIV_TMRS                  0x100000
#define BAM_INACTIV_TMRS_EXST                  0x80000
#define BAM_HIGH_FREQUENCY_BAM                 0x40000
#define BAM_HAS_NO_BYPASS                      0x20000
#define BAM_SECURED                            0x10000
#define BAM_USE_VMIDMT                          0x8000
#define BAM_AXI_ACTIVE                          0x4000
#define BAM_CE_BUFFER_SIZE                      0x3000
#define BAM_NUM_EES                              0xf00
#define BAM_REVISION                              0xff

/* SW_REVISION */
#define BAM_MAJOR                           0xf0000000
#define BAM_MINOR                            0xfff0000
#define BAM_STEP                                0xffff

/* NUM_PIPES */
#define BAM_NON_PIPE_GRP                    0xff000000
#define BAM_PERIPH_NON_PIPE_GRP               0xff0000
#define BAM_DATA_ADDR_BUS_WIDTH                 0xC000
#define BAM_NUM_PIPES                             0xff

/* TIMER */
#define BAM_TIMER                               0xffff

/* TIMER_CTRL */
#define TIMER_RST                           0x80000000
#define TIMER_RUN                           0x40000000
#define TIMER_MODE                          0x20000000
#define TIMER_TRSHLD                            0xffff

/* DESC_CNT_TRSHLD */
#define BAM_DESC_CNT_TRSHLD                     0xffff

/* IRQ_SRCS */
#define BAM_IRQ                         0x80000000
#define P_IRQ                           0x7fffffff

/* IRQ_STTS */
#define IRQ_STTS_BAM_TIMER_IRQ                         0x10
#define IRQ_STTS_BAM_EMPTY_IRQ                          0x8
#define IRQ_STTS_BAM_ERROR_IRQ                          0x4
#define IRQ_STTS_BAM_HRESP_ERR_IRQ                      0x2

/* IRQ_CLR */
#define IRQ_CLR_BAM_TIMER_IRQ                          0x10
#define IRQ_CLR_BAM_EMPTY_CLR                           0x8
#define IRQ_CLR_BAM_ERROR_CLR                           0x4
#define IRQ_CLR_BAM_HRESP_ERR_CLR                       0x2

/* IRQ_EN */
#define IRQ_EN_BAM_TIMER_IRQ                           0x10
#define IRQ_EN_BAM_EMPTY_EN                             0x8
#define IRQ_EN_BAM_ERROR_EN                             0x4
#define IRQ_EN_BAM_HRESP_ERR_EN                         0x2

/* AHB_MASTER_ERR_CTRLS */
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HVMID         0x7c0000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_DIRECT_MODE    0x20000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HCID           0x1f000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HPROT            0xf00
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HBURST            0xe0
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HSIZE             0x18
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HWRITE             0x4
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HTRANS             0x3

/* TRUST_REG  */
#define LOCK_EE_CTRL                            0x2000
#define BAM_VMID                                0x1f00
#define BAM_RST_BLOCK                             0x80
#define BAM_EE                                     0x7

/* TEST_BUS_SEL */
#define BAM_SW_EVENTS_ZERO                    0x200000
#define BAM_SW_EVENTS_SEL                     0x180000
#define BAM_DATA_ERASE                         0x40000
#define BAM_DATA_FLUSH                         0x20000
#define BAM_CLK_ALWAYS_ON                      0x10000
#define BAM_TESTBUS_SEL                           0x7f

/* CNFG_BITS */
#define CNFG_BITS_AOS_OVERFLOW_PRVNT		 0x80000000
#define CNFG_BITS_MULTIPLE_EVENTS_DESC_AVAIL_EN  0x40000000
#define CNFG_BITS_MULTIPLE_EVENTS_SIZE_EN        0x20000000
#define CNFG_BITS_BAM_ZLT_W_CD_SUPPORT           0x10000000
#define CNFG_BITS_BAM_CD_ENABLE                   0x8000000
#define CNFG_BITS_BAM_AU_ACCUMED                  0x4000000
#define CNFG_BITS_BAM_PSM_P_HD_DATA               0x2000000
#define CNFG_BITS_BAM_REG_P_EN                    0x1000000
#define CNFG_BITS_BAM_WB_DSC_AVL_P_RST             0x800000
#define CNFG_BITS_BAM_WB_RETR_SVPNT                0x400000
#define CNFG_BITS_BAM_WB_CSW_ACK_IDL               0x200000
#define CNFG_BITS_BAM_WB_BLK_CSW                   0x100000
#define CNFG_BITS_BAM_WB_P_RES                      0x80000
#define CNFG_BITS_BAM_SI_P_RES                      0x40000
#define CNFG_BITS_BAM_AU_P_RES                      0x20000
#define CNFG_BITS_BAM_PSM_P_RES                     0x10000
#define CNFG_BITS_BAM_PSM_CSW_REQ                    0x8000
#define CNFG_BITS_BAM_SB_CLK_REQ                     0x4000
#define CNFG_BITS_BAM_IBC_DISABLE                    0x2000
#define CNFG_BITS_BAM_NO_EXT_P_RST                   0x1000
#define CNFG_BITS_BAM_FULL_PIPE                       0x800
#define CNFG_BITS_BAM_PIPE_CNFG                         0x4

/* PIPE_ATTR_EEn*/
#define BAM_ENABLED                              0x80000000
#define P_ATTR                                   0x7fffffff

/* P_ctrln */
#define P_LOCK_GROUP                          0x1f0000
#define P_WRITE_NWD                              0x800
#define P_PREFETCH_LIMIT                         0x600
#define P_AUTO_EOB_SEL                           0x180
#define P_AUTO_EOB                                0x40
#define P_SYS_MODE                                0x20
#define P_SYS_STRM                                0x10
#define P_DIRECTION                                0x8
#define P_EN                                       0x2

/* P_RSTn */
#define P_RST_P_SW_RST                             0x1

/* P_HALTn */
#define P_HALT_P_PROD_HALTED                       0x2
#define P_HALT_P_HALT                              0x1

/* P_TRUST_REGn */
#define BAM_P_VMID                              0x1f00
#define BAM_P_SUP_GROUP                           0xf8
#define BAM_P_EE                                   0x7

/* P_IRQ_STTSn */
#define P_IRQ_STTS_P_HRESP_ERR_IRQ                0x80
#define P_IRQ_STTS_P_PIPE_RST_ERR_IRQ             0x40
#define P_IRQ_STTS_P_TRNSFR_END_IRQ               0x20
#define P_IRQ_STTS_P_ERR_IRQ                      0x10
#define P_IRQ_STTS_P_OUT_OF_DESC_IRQ               0x8
#define P_IRQ_STTS_P_WAKE_IRQ                      0x4
#define P_IRQ_STTS_P_TIMER_IRQ                     0x2
#define P_IRQ_STTS_P_PRCSD_DESC_IRQ                0x1

/* P_IRQ_CLRn */
#define P_IRQ_CLR_P_HRESP_ERR_CLR                 0x80
#define P_IRQ_CLR_P_PIPE_RST_ERR_CLR              0x40
#define P_IRQ_CLR_P_TRNSFR_END_CLR                0x20
#define P_IRQ_CLR_P_ERR_CLR                       0x10
#define P_IRQ_CLR_P_OUT_OF_DESC_CLR                0x8
#define P_IRQ_CLR_P_WAKE_CLR                       0x4
#define P_IRQ_CLR_P_TIMER_CLR                      0x2
#define P_IRQ_CLR_P_PRCSD_DESC_CLR                 0x1

/* P_IRQ_ENn */
#define P_IRQ_EN_P_HRESP_ERR_EN                   0x80
#define P_IRQ_EN_P_PIPE_RST_ERR_EN                0x40
#define P_IRQ_EN_P_TRNSFR_END_EN                  0x20
#define P_IRQ_EN_P_ERR_EN                         0x10
#define P_IRQ_EN_P_OUT_OF_DESC_EN                  0x8
#define P_IRQ_EN_P_WAKE_EN                         0x4
#define P_IRQ_EN_P_TIMER_EN                        0x2
#define P_IRQ_EN_P_PRCSD_DESC_EN                   0x1

/* P_TIMERn */
#define P_TIMER_P_TIMER                         0xffff

/* P_TIMER_ctrln */
#define P_TIMER_RST                         0x80000000
#define P_TIMER_RUN                         0x40000000
#define P_TIMER_MODE                        0x20000000
#define P_TIMER_TRSHLD                          0xffff

/* P_PRDCR_SDBNDn */
#define P_PRDCR_SDBNDn_BAM_P_SB_UPDATED      0x1000000
#define P_PRDCR_SDBNDn_BAM_P_TOGGLE           0x100000
#define P_PRDCR_SDBNDn_BAM_P_CTRL              0xf0000
#define P_PRDCR_SDBNDn_BAM_P_BYTES_FREE         0xffff

/* P_CNSMR_SDBNDn */
#define P_CNSMR_SDBNDn_BAM_P_SB_UPDATED      0x1000000
#define P_CNSMR_SDBNDn_BAM_P_WAIT_4_ACK       0x800000
#define P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE       0x400000
#define P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE_R     0x200000
#define P_CNSMR_SDBNDn_BAM_P_TOGGLE           0x100000
#define P_CNSMR_SDBNDn_BAM_P_CTRL              0xf0000
#define P_CNSMR_SDBNDn_BAM_P_BYTES_AVAIL        0xffff

/* P_EVNT_regn */
#define P_BYTES_CONSUMED                    0xffff0000
#define P_DESC_FIFO_PEER_OFST                   0xffff

/* P_SW_ofstsn */
#define SW_OFST_IN_DESC                     0xffff0000
#define SW_DESC_OFST                            0xffff

/* P_EVNT_GEN_TRSHLDn */
#define P_EVNT_GEN_TRSHLD_P_TRSHLD              0xffff

/* P_FIFO_sizesn */
#define P_DATA_FIFO_SIZE                    0xffff0000
#define P_DESC_FIFO_SIZE                        0xffff

#define P_RETR_CNTXT_RETR_DESC_OFST            0xffff0000
#define P_RETR_CNTXT_RETR_OFST_IN_DESC             0xffff
#define P_SI_CNTXT_SI_DESC_OFST                    0xffff
#define P_DF_CNTXT_WB_ACCUMULATED              0xffff0000
#define P_DF_CNTXT_DF_DESC_OFST                    0xffff
#define P_AU_PSM_CNTXT_1_AU_PSM_ACCUMED        0xffff0000
#define P_AU_PSM_CNTXT_1_AU_ACKED                  0xffff
#define P_PSM_CNTXT_2_PSM_DESC_VALID           0x80000000
#define P_PSM_CNTXT_2_PSM_DESC_IRQ             0x40000000
#define P_PSM_CNTXT_2_PSM_DESC_IRQ_DONE        0x20000000
#define P_PSM_CNTXT_2_PSM_GENERAL_BITS         0x1e000000
#define P_PSM_CNTXT_2_PSM_CONS_STATE            0x1c00000
#define P_PSM_CNTXT_2_PSM_PROD_SYS_STATE         0x380000
#define P_PSM_CNTXT_2_PSM_PROD_B2B_STATE          0x70000
#define P_PSM_CNTXT_2_PSM_DESC_SIZE                0xffff
#define P_PSM_CNTXT_4_PSM_DESC_OFST            0xffff0000
#define P_PSM_CNTXT_4_PSM_SAVED_ACCUMED_SIZE       0xffff
#define P_PSM_CNTXT_5_PSM_BLOCK_BYTE_CNT       0xffff0000
#define P_PSM_CNTXT_5_PSM_OFST_IN_DESC             0xffff

#else

/* Maximum number of execution environment */
#define BAM_MAX_EES 4

/**
 *  BAM Hardware registers.
 *
 */
#define CTRL                        (0xf80)
#define REVISION                    (0xf84)
#define NUM_PIPES                   (0xfbc)
#define DESC_CNT_TRSHLD             (0xf88)
#define IRQ_SRCS                    (0xf8c)
#define IRQ_SRCS_MSK                (0xf90)
#define IRQ_SRCS_UNMASKED           (0xfb0)
#define IRQ_STTS                    (0xf94)
#define IRQ_CLR                     (0xf98)
#define IRQ_EN                      (0xf9c)
#define IRQ_SIC_SEL                 (0xfa0)
#define AHB_MASTER_ERR_CTRLS        (0xfa4)
#define AHB_MASTER_ERR_ADDR         (0xfa8)
#define AHB_MASTER_ERR_DATA         (0xfac)
/* The addresses for IRQ_DEST and PERIPH_IRQ_DEST become reserved */
#define IRQ_DEST                    (0xfb4)
#define PERIPH_IRQ_DEST             (0xfb8)
#define TEST_BUS_REG                (0xff8)
#define CNFG_BITS                   (0xffc)
#define TEST_BUS_SEL                (0xff4)
#define TRUST_REG                   (0xff0)
#define IRQ_SRCS_EE(n)             (0x1800 + 128 * (n))
#define IRQ_SRCS_MSK_EE(n)         (0x1804 + 128 * (n))
#define IRQ_SRCS_UNMASKED_EE(n)    (0x1808 + 128 * (n))

#define P_CTRL(n)                  (0x0000 + 128 * (n))
#define P_RST(n)                   (0x0004 + 128 * (n))
#define P_HALT(n)                  (0x0008 + 128 * (n))
#define P_IRQ_STTS(n)              (0x0010 + 128 * (n))
#define P_IRQ_CLR(n)               (0x0014 + 128 * (n))
#define P_IRQ_EN(n)                (0x0018 + 128 * (n))
#define P_TIMER(n)                 (0x001c + 128 * (n))
#define P_TIMER_CTRL(n)            (0x0020 + 128 * (n))
#define P_PRDCR_SDBND(n)            (0x0024 + 128 * (n))
#define P_CNSMR_SDBND(n)            (0x0028 + 128 * (n))
#define P_TRUST_REG(n)             (0x0030 + 128 * (n))
#define P_EVNT_DEST_ADDR(n)        (0x102c + 64 * (n))
#define P_EVNT_REG(n)              (0x1018 + 64 * (n))
#define P_SW_OFSTS(n)              (0x1000 + 64 * (n))
#define P_DATA_FIFO_ADDR(n)        (0x1024 + 64 * (n))
#define P_DESC_FIFO_ADDR(n)        (0x101c + 64 * (n))
#define P_EVNT_GEN_TRSHLD(n)       (0x1028 + 64 * (n))
#define P_FIFO_SIZES(n)            (0x1020 + 64 * (n))
#define P_IRQ_DEST_ADDR(n)         (0x103c + 64 * (n))
#define P_RETR_CNTXT(n)           (0x1034 + 64 * (n))
#define P_SI_CNTXT(n)             (0x1038 + 64 * (n))
#define P_AU_PSM_CNTXT_1(n)       (0x1004 + 64 * (n))
#define P_PSM_CNTXT_2(n)          (0x1008 + 64 * (n))
#define P_PSM_CNTXT_3(n)          (0x100c + 64 * (n))
#define P_PSM_CNTXT_4(n)          (0x1010 + 64 * (n))
#define P_PSM_CNTXT_5(n)          (0x1014 + 64 * (n))

/**
 *  BAM Hardware registers bitmask.
 *  format: <register>_<field>
 *
 */
/* CTRL */
#define IBC_DISABLE                            0x10000
#define BAM_CACHED_DESC_STORE                   0x8000
#define BAM_DESC_CACHE_SEL                      0x6000
/* BAM_PERIPH_IRQ_SIC_SEL is an obsolete field; This bit is reserved now */
#define BAM_PERIPH_IRQ_SIC_SEL                  0x1000
#define BAM_EN_ACCUM                              0x10
#define BAM_EN                                     0x2
#define BAM_SW_RST                                 0x1

/* REVISION */
#define BAM_INACTIV_TMR_BASE                0xff000000
#define BAM_INACTIV_TMRS_EXST                  0x80000
#define BAM_HIGH_FREQUENCY_BAM                 0x40000
#define BAM_HAS_NO_BYPASS                      0x20000
#define BAM_SECURED                            0x10000
#define BAM_NUM_EES                              0xf00
#define BAM_REVISION                              0xff

/* NUM_PIPES */
#define BAM_NON_PIPE_GRP                    0xff000000
#define BAM_PERIPH_NON_PIPE_GRP               0xff0000
#define BAM_DATA_ADDR_BUS_WIDTH                 0xC000
#define BAM_NUM_PIPES                             0xff

/* DESC_CNT_TRSHLD */
#define BAM_DESC_CNT_TRSHLD                     0xffff

/* IRQ_SRCS */
#define BAM_IRQ                         0x80000000
#define P_IRQ                           0x7fffffff

#define IRQ_STTS_BAM_EMPTY_IRQ                          0x8
#define IRQ_STTS_BAM_ERROR_IRQ                          0x4
#define IRQ_STTS_BAM_HRESP_ERR_IRQ                      0x2
#define IRQ_CLR_BAM_EMPTY_CLR                           0x8
#define IRQ_CLR_BAM_ERROR_CLR                           0x4
#define IRQ_CLR_BAM_HRESP_ERR_CLR                       0x2
#define IRQ_EN_BAM_EMPTY_EN                             0x8
#define IRQ_EN_BAM_ERROR_EN                             0x4
#define IRQ_EN_BAM_HRESP_ERR_EN                         0x2
#define IRQ_SIC_SEL_BAM_IRQ_SIC_SEL              0x80000000
#define IRQ_SIC_SEL_P_IRQ_SIC_SEL                0x7fffffff
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HVMID         0x7c0000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_DIRECT_MODE    0x20000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HCID           0x1f000
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HPROT            0xf00
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HBURST            0xe0
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HSIZE             0x18
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HWRITE             0x4
#define AHB_MASTER_ERR_CTRLS_BAM_ERR_HTRANS             0x3
#define CNFG_BITS_BAM_AU_ACCUMED                  0x4000000
#define CNFG_BITS_BAM_PSM_P_HD_DATA               0x2000000
#define CNFG_BITS_BAM_REG_P_EN                    0x1000000
#define CNFG_BITS_BAM_WB_DSC_AVL_P_RST             0x800000
#define CNFG_BITS_BAM_WB_RETR_SVPNT                0x400000
#define CNFG_BITS_BAM_WB_CSW_ACK_IDL               0x200000
#define CNFG_BITS_BAM_WB_BLK_CSW                   0x100000
#define CNFG_BITS_BAM_WB_P_RES                      0x80000
#define CNFG_BITS_BAM_SI_P_RES                      0x40000
#define CNFG_BITS_BAM_AU_P_RES                      0x20000
#define CNFG_BITS_BAM_PSM_P_RES                     0x10000
#define CNFG_BITS_BAM_PSM_CSW_REQ                    0x8000
#define CNFG_BITS_BAM_SB_CLK_REQ                     0x4000
#define CNFG_BITS_BAM_IBC_DISABLE                    0x2000
#define CNFG_BITS_BAM_NO_EXT_P_RST                   0x1000
#define CNFG_BITS_BAM_FULL_PIPE                       0x800
#define CNFG_BITS_BAM_PIPE_CNFG                         0x4

/* TEST_BUS_SEL */
#define BAM_DATA_ERASE                         0x40000
#define BAM_DATA_FLUSH                         0x20000
#define BAM_CLK_ALWAYS_ON                      0x10000
#define BAM_TESTBUS_SEL                           0x7f

/* TRUST_REG  */
#define BAM_VMID                                0x1f00
#define BAM_RST_BLOCK                             0x80
#define BAM_EE                                     0x3

/* P_TRUST_REGn */
#define BAM_P_VMID                              0x1f00
#define BAM_P_EE                                   0x3

/* P_PRDCR_SDBNDn */
#define P_PRDCR_SDBNDn_BAM_P_SB_UPDATED      0x1000000
#define P_PRDCR_SDBNDn_BAM_P_TOGGLE           0x100000
#define P_PRDCR_SDBNDn_BAM_P_CTRL              0xf0000
#define P_PRDCR_SDBNDn_BAM_P_BYTES_FREE         0xffff
/* P_CNSMR_SDBNDn */
#define P_CNSMR_SDBNDn_BAM_P_SB_UPDATED      0x1000000
#define P_CNSMR_SDBNDn_BAM_P_WAIT_4_ACK       0x800000
#define P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE       0x400000
#define P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE_R     0x200000
#define P_CNSMR_SDBNDn_BAM_P_TOGGLE           0x100000
#define P_CNSMR_SDBNDn_BAM_P_CTRL              0xf0000
#define P_CNSMR_SDBNDn_BAM_P_BYTES_AVAIL        0xffff

/* P_ctrln */
#define P_PREFETCH_LIMIT                         0x600
#define P_AUTO_EOB_SEL                           0x180
#define P_AUTO_EOB                                0x40
#define P_SYS_MODE                             0x20
#define P_SYS_STRM                             0x10
#define P_DIRECTION                             0x8
#define P_EN                                    0x2

#define P_RST_P_SW_RST                                 0x1

#define P_HALT_P_PROD_HALTED                           0x2
#define P_HALT_P_HALT                                  0x1

#define P_IRQ_STTS_P_TRNSFR_END_IRQ                   0x20
#define P_IRQ_STTS_P_ERR_IRQ                          0x10
#define P_IRQ_STTS_P_OUT_OF_DESC_IRQ                   0x8
#define P_IRQ_STTS_P_WAKE_IRQ                          0x4
#define P_IRQ_STTS_P_TIMER_IRQ                         0x2
#define P_IRQ_STTS_P_PRCSD_DESC_IRQ                    0x1

#define P_IRQ_CLR_P_TRNSFR_END_CLR                    0x20
#define P_IRQ_CLR_P_ERR_CLR                           0x10
#define P_IRQ_CLR_P_OUT_OF_DESC_CLR                    0x8
#define P_IRQ_CLR_P_WAKE_CLR                           0x4
#define P_IRQ_CLR_P_TIMER_CLR                          0x2
#define P_IRQ_CLR_P_PRCSD_DESC_CLR                     0x1

#define P_IRQ_EN_P_TRNSFR_END_EN                      0x20
#define P_IRQ_EN_P_ERR_EN                             0x10
#define P_IRQ_EN_P_OUT_OF_DESC_EN                      0x8
#define P_IRQ_EN_P_WAKE_EN                             0x4
#define P_IRQ_EN_P_TIMER_EN                            0x2
#define P_IRQ_EN_P_PRCSD_DESC_EN                       0x1

#define P_TIMER_P_TIMER                             0xffff

/* P_TIMER_ctrln */
#define P_TIMER_RST                0x80000000
#define P_TIMER_RUN                0x40000000
#define P_TIMER_MODE               0x20000000
#define P_TIMER_TRSHLD                 0xffff

/* P_EVNT_regn */
#define P_BYTES_CONSUMED             0xffff0000
#define P_DESC_FIFO_PEER_OFST            0xffff

/* P_SW_ofstsn */
#define SW_OFST_IN_DESC              0xffff0000
#define SW_DESC_OFST                     0xffff

#define P_EVNT_GEN_TRSHLD_P_TRSHLD                  0xffff

/* P_FIFO_sizesn */
#define P_DATA_FIFO_SIZE           0xffff0000
#define P_DESC_FIFO_SIZE               0xffff

#define P_RETR_CNTXT_RETR_DESC_OFST            0xffff0000
#define P_RETR_CNTXT_RETR_OFST_IN_DESC             0xffff
#define P_SI_CNTXT_SI_DESC_OFST                    0xffff
#define P_AU_PSM_CNTXT_1_AU_PSM_ACCUMED        0xffff0000
#define P_AU_PSM_CNTXT_1_AU_ACKED                  0xffff
#define P_PSM_CNTXT_2_PSM_DESC_VALID           0x80000000
#define P_PSM_CNTXT_2_PSM_DESC_IRQ             0x40000000
#define P_PSM_CNTXT_2_PSM_DESC_IRQ_DONE        0x20000000
#define P_PSM_CNTXT_2_PSM_GENERAL_BITS         0x1e000000
#define P_PSM_CNTXT_2_PSM_CONS_STATE            0x1c00000
#define P_PSM_CNTXT_2_PSM_PROD_SYS_STATE         0x380000
#define P_PSM_CNTXT_2_PSM_PROD_B2B_STATE          0x70000
#define P_PSM_CNTXT_2_PSM_DESC_SIZE                0xffff
#define P_PSM_CNTXT_4_PSM_DESC_OFST            0xffff0000
#define P_PSM_CNTXT_4_PSM_SAVED_ACCUMED_SIZE       0xffff
#define P_PSM_CNTXT_5_PSM_BLOCK_BYTE_CNT       0xffff0000
#define P_PSM_CNTXT_5_PSM_OFST_IN_DESC             0xffff
#endif

#define BAM_ERROR   (-1)

/* AHB buffer error control */
enum bam_nonsecure_reset {
	BAM_NONSECURE_RESET_ENABLE  = 0,
	BAM_NONSECURE_RESET_DISABLE = 1,
};

/**
 *
 * Read register with debug info.
 *
 * @base - bam base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 bam_read_reg(void *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	SPS_DBG("sps:bam 0x%p(va) read reg 0x%x r_val 0x%x.\n",
			base, offset, val);
	return val;
}

/**
 * Read register masked field with debug info.
 *
 * @base - bam base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 *
 * @return u32
 */
static inline u32 bam_read_reg_field(void *base, u32 offset, const u32 mask)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 val = ioread32(base + offset);
	val &= mask;		/* clear other bits */
	val >>= shift;
	SPS_DBG("sps:bam 0x%p(va) read reg 0x%x mask 0x%x r_val 0x%x.\n",
			base, offset, mask, val);
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - bam base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void bam_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
	SPS_DBG("sps:bam 0x%p(va) write reg 0x%x w_val 0x%x.\n",
			base, offset, val);
}

/**
 * Write register masked field with debug info.
 *
 * @base - bam base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void bam_write_reg_field(void *base, u32 offset,
				       const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;		/* clear written bits */
	val = tmp | (val << shift);
	iowrite32(val, base + offset);
	SPS_DBG("sps:bam 0x%p(va) write reg 0x%x w_val 0x%x.\n",
			base, offset, val);
}

/**
 * Initialize a BAM device
 *
 */
int bam_init(void *base, u32 ee,
		u16 summing_threshold,
		u32 irq_mask, u32 *version,
		u32 *num_pipes, u32 options)
{
	u32 cfg_bits;
	u32 ver = 0;

	SPS_DBG2("sps:%s:bam=0x%p(va).ee=%d.", __func__, base, ee);

	ver = bam_read_reg_field(base, REVISION, BAM_REVISION);

	if ((ver < BAM_MIN_VERSION) || (ver > BAM_MAX_VERSION)) {
		SPS_ERR("sps:bam 0x%p(va) Invalid BAM REVISION 0x%x.\n",
				base, ver);
		return -ENODEV;
	} else
		SPS_DBG2("sps:REVISION of BAM 0x%p is 0x%x.\n",
				base, ver);

	if (summing_threshold == 0) {
		summing_threshold = 4;
		SPS_ERR(
			"sps:bam 0x%p(va) summing_threshold is zero, "
				"use default 4.\n", base);
	}

	if (options & SPS_BAM_NO_EXT_P_RST)
		cfg_bits = 0xffffffff & ~(3 << 11);
	else
		cfg_bits = 0xffffffff & ~(1 << 11);

	bam_write_reg_field(base, CTRL, BAM_SW_RST, 1);
	/* No delay needed */
	bam_write_reg_field(base, CTRL, BAM_SW_RST, 0);

	bam_write_reg_field(base, CTRL, BAM_EN, 1);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	bam_write_reg_field(base, CTRL, CACHE_MISS_ERR_RESP_EN, 0);

	if (options & SPS_BAM_NO_LOCAL_CLK_GATING)
		bam_write_reg_field(base, CTRL, LOCAL_CLK_GATING, 0);
	else
		bam_write_reg_field(base, CTRL, LOCAL_CLK_GATING, 1);

	if (enhd_pipe) {
		if (options & SPS_BAM_CANCEL_WB)
			bam_write_reg_field(base, CTRL,
					BAM_MESS_ONLY_CANCEL_WB, 1);
		else
			bam_write_reg_field(base, CTRL,
					BAM_MESS_ONLY_CANCEL_WB, 0);
	}
#endif
	bam_write_reg(base, DESC_CNT_TRSHLD, summing_threshold);

	bam_write_reg(base, CNFG_BITS, cfg_bits);

	/*
	 *  Enable Global BAM Interrupt - for error reasons ,
	 *  filter with mask.
	 *  Note: Pipes interrupts are disabled until BAM_P_IRQ_enn is set
	 */
	bam_write_reg_field(base, IRQ_SRCS_MSK_EE(ee), BAM_IRQ, 1);

	bam_write_reg(base, IRQ_EN, irq_mask);

	*num_pipes = bam_read_reg_field(base, NUM_PIPES, BAM_NUM_PIPES);

	*version = ver;

	return 0;
}

/**
 * Set BAM global execution environment
 *
 * @base - BAM virtual base address
 *
 * @ee - BAM execution environment index
 *
 * @vmid - virtual master identifier
 *
 * @reset - enable/disable BAM global software reset
 */
static void bam_set_ee(void *base, u32 ee, u32 vmid,
			enum bam_nonsecure_reset reset)
{
	bam_write_reg_field(base, TRUST_REG, BAM_EE, ee);
	bam_write_reg_field(base, TRUST_REG, BAM_VMID, vmid);
	bam_write_reg_field(base, TRUST_REG, BAM_RST_BLOCK, reset);
}

/**
 * Set the pipe execution environment
 *
 * @base - BAM virtual base address
 *
 * @pipe - pipe index
 *
 * @ee - BAM execution environment index
 *
 * @vmid - virtual master identifier
 */
static void bam_pipe_set_ee(void *base, u32 pipe, u32 ee, u32 vmid)
{
	bam_write_reg_field(base, P_TRUST_REG(pipe), BAM_P_EE, ee);
	bam_write_reg_field(base, P_TRUST_REG(pipe), BAM_P_VMID, vmid);
}

/**
 * Initialize BAM device security execution environment
 */
int bam_security_init(void *base, u32 ee, u32 vmid, u32 pipe_mask)
{
	u32 version;
	u32 num_pipes;
	u32 mask;
	u32 pipe;

	SPS_DBG2("sps:%s:bam=0x%p(va).", __func__, base);

	/*
	 * Discover the hardware version number and the number of pipes
	 * supported by this BAM
	 */
	version = bam_read_reg_field(base, REVISION, BAM_REVISION);
	num_pipes = bam_read_reg_field(base, NUM_PIPES, BAM_NUM_PIPES);
	if (version < 3 || version > 0x1F) {
		SPS_ERR(
			"sps:bam 0x%p(va) security is not supported for this "
				"BAM version 0x%x.\n", base, version);
		return -ENODEV;
	}

	if (num_pipes > BAM_MAX_PIPES) {
		SPS_ERR(
			"sps:bam 0x%p(va) the number of pipes is more than "
				"the maximum number allowed.\n", base);
		return -ENODEV;
	}

	for (pipe = 0, mask = 1; pipe < num_pipes; pipe++, mask <<= 1)
		if ((mask & pipe_mask) != 0)
			bam_pipe_set_ee(base, pipe, ee, vmid);

	/* If MSbit is set, assign top-level interrupt to this EE */
	mask = 1UL << 31;
	if ((mask & pipe_mask) != 0)
		bam_set_ee(base, ee, vmid, BAM_NONSECURE_RESET_ENABLE);

	return 0;
}

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
static inline u32 bam_get_pipe_attr(void *base, u32 ee, bool global)
{
	u32 val;

	if (global)
		val = bam_read_reg_field(base, PIPE_ATTR_EE(ee), BAM_ENABLED);
	else
		val = bam_read_reg_field(base, PIPE_ATTR_EE(ee), P_ATTR);

	return val;
}
#else
static inline u32 bam_get_pipe_attr(void *base, u32 ee, bool global)
{
	return 0;
}
#endif

/**
 * Verify that a BAM device is enabled and gathers the hardware
 * configuration.
 *
 */
int bam_check(void *base, u32 *version, u32 ee, u32 *num_pipes)
{
	u32 ver = 0;
	u32 enabled = 0;

	SPS_DBG2("sps:%s:bam=0x%p(va).", __func__, base);

	if (!enhd_pipe)
		enabled = bam_read_reg_field(base, CTRL, BAM_EN);
	else
		enabled = bam_get_pipe_attr(base, ee, true);

	if (!enabled) {
		SPS_ERR("sps:%s:bam 0x%p(va) is not enabled.\n",
				__func__, base);
		return -ENODEV;
	}

	ver = bam_read_reg(base, REVISION) & BAM_REVISION;

	/*
	 *  Discover the hardware version number and the number of pipes
	 *  supported by this BAM
	 */
	*num_pipes = bam_read_reg_field(base, NUM_PIPES, BAM_NUM_PIPES);
	*version = ver;

	/* Check BAM version */
	if ((ver < BAM_MIN_VERSION) || (ver > BAM_MAX_VERSION)) {
		SPS_ERR("sps:%s:bam 0x%p(va) Invalid BAM version 0x%x.\n",
				__func__, base, ver);
		return -ENODEV;
	}

	return 0;
}

/**
 * Disable a BAM device
 *
 */
void bam_exit(void *base, u32 ee)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).ee=%d.", __func__, base, ee);

	bam_write_reg_field(base, IRQ_SRCS_MSK_EE(ee), BAM_IRQ, 0);

	bam_write_reg(base, IRQ_EN, 0);

	/* Disable the BAM */
	bam_write_reg_field(base, CTRL, BAM_EN, 0);
}

/**
 * Output BAM register content
 * including the TEST_BUS register content under
 * different TEST_BUS_SEL values.
 */
void bam_output_register_content(void *base, u32 ee)
{
	u32 num_pipes;
	u32 i;
	u32 pipe_attr = 0;

	print_bam_test_bus_reg(base, 0);

	print_bam_selected_reg(base, BAM_MAX_EES);

	num_pipes = bam_read_reg_field(base, NUM_PIPES,
					BAM_NUM_PIPES);
	SPS_INFO("sps:bam 0x%p(va) has %d pipes.",
			base, num_pipes);

	pipe_attr = enhd_pipe ?
		bam_get_pipe_attr(base, ee, false) : 0x0;

	if (!enhd_pipe || !pipe_attr)
		for (i = 0; i < num_pipes; i++)
			print_bam_pipe_selected_reg(base, i);
	else {
		for (i = 0; i < num_pipes; i++) {
			if (pipe_attr & (1UL << i))
				print_bam_pipe_selected_reg(base, i);
		}
	}
}

/**
 * Get BAM IRQ source and clear global IRQ status
 */
u32 bam_check_irq_source(void *base, u32 ee, u32 mask,
				enum sps_callback_case *cb_case)
{
	u32 source = bam_read_reg(base, IRQ_SRCS_EE(ee));
	u32 clr = source & (1UL << 31);

	if (clr) {
		u32 status = 0;
		status = bam_read_reg(base, IRQ_STTS);

		if (status & IRQ_STTS_BAM_ERROR_IRQ) {
			SPS_ERR("sps:bam 0x%p(va);bam irq status="
				"0x%x.\nsps: BAM_ERROR_IRQ\n",
				base, status);
			bam_output_register_content(base, ee);
			*cb_case = SPS_CALLBACK_BAM_ERROR_IRQ;
		} else if (status & IRQ_STTS_BAM_HRESP_ERR_IRQ) {
			SPS_ERR("sps:bam 0x%p(va);bam irq status="
				"0x%x.\nsps: BAM_HRESP_ERR_IRQ\n",
				base, status);
			bam_output_register_content(base, ee);
			*cb_case = SPS_CALLBACK_BAM_HRESP_ERR_IRQ;
#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
		} else if (status & IRQ_STTS_BAM_TIMER_IRQ) {
			SPS_DBG1("sps:bam 0x%p(va);receive BAM_TIMER_IRQ\n",
					base);
			*cb_case = SPS_CALLBACK_BAM_TIMER_IRQ;
#endif
		} else
			SPS_INFO("sps:bam 0x%p(va);bam irq status=0x%x.\n",
					base, status);

		bam_write_reg(base, IRQ_CLR, status);
	}

	source &= (mask|(1UL << 31));
	return source;
}

/**
 * Initialize a BAM pipe
 */
int bam_pipe_init(void *base, u32 pipe,	struct bam_pipe_parameters *param,
					u32 ee)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).pipe=%d.", __func__, base, pipe);

	/* Reset the BAM pipe */
	bam_write_reg(base, P_RST(pipe), 1);
	/* No delay needed */
	bam_write_reg(base, P_RST(pipe), 0);

	/* Enable the Pipe Interrupt at the BAM level */
	bam_write_reg_field(base, IRQ_SRCS_MSK_EE(ee), (1 << pipe), 1);

	bam_write_reg(base, P_IRQ_EN(pipe), param->pipe_irq_mask);

	bam_write_reg_field(base, P_CTRL(pipe), P_DIRECTION, param->dir);
	bam_write_reg_field(base, P_CTRL(pipe), P_SYS_MODE, param->mode);

	bam_write_reg(base, P_EVNT_GEN_TRSHLD(pipe), param->event_threshold);

	bam_write_reg(base, P_DESC_FIFO_ADDR(pipe),
			SPS_GET_LOWER_ADDR(param->desc_base));
	bam_write_reg_field(base, P_FIFO_SIZES(pipe), P_DESC_FIFO_SIZE,
			    param->desc_size);

	bam_write_reg_field(base, P_CTRL(pipe), P_SYS_STRM,
			    param->stream_mode);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	if (SPS_LPAE)
		bam_write_reg(base, P_DESC_FIFO_ADDR_MSB(pipe),
				SPS_GET_UPPER_ADDR(param->desc_base));

	bam_write_reg_field(base, P_CTRL(pipe), P_LOCK_GROUP,
				param->lock_group);

	SPS_DBG("sps:bam=0x%p(va).pipe=%d.lock_group=%d.\n",
			base, pipe, param->lock_group);
#endif

	if (param->mode == BAM_PIPE_MODE_BAM2BAM) {
		u32 peer_dest_addr = param->peer_phys_addr +
				      P_EVNT_REG(param->peer_pipe);

		bam_write_reg(base, P_DATA_FIFO_ADDR(pipe),
			      SPS_GET_LOWER_ADDR(param->data_base));
		bam_write_reg_field(base, P_FIFO_SIZES(pipe),
				    P_DATA_FIFO_SIZE, param->data_size);

		bam_write_reg(base, P_EVNT_DEST_ADDR(pipe), peer_dest_addr);

		SPS_DBG2("sps:bam=0x%p(va).pipe=%d.peer_bam=0x%x."
			"peer_pipe=%d.\n",
			base, pipe,
			(u32) param->peer_phys_addr,
			param->peer_pipe);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
		if (SPS_LPAE) {
			bam_write_reg(base, P_EVNT_DEST_ADDR_MSB(pipe), 0x0);
			bam_write_reg(base, P_DATA_FIFO_ADDR_MSB(pipe),
				      SPS_GET_UPPER_ADDR(param->data_base));
		}

		bam_write_reg_field(base, P_CTRL(pipe), P_WRITE_NWD,
					param->write_nwd);

		SPS_DBG("sps:%s WRITE_NWD bit for this bam2bam pipe.",
			param->write_nwd ? "Set" : "Do not set");
#endif
	}

	/* Pipe Enable - at last */
	bam_write_reg_field(base, P_CTRL(pipe), P_EN, 1);

	return 0;
}

/**
 * Reset the BAM pipe
 *
 */
void bam_pipe_exit(void *base, u32 pipe, u32 ee)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).pipe=%d.", __func__, base, pipe);

	bam_write_reg(base, P_IRQ_EN(pipe), 0);

	/* Disable the Pipe Interrupt at the BAM level */
	bam_write_reg_field(base, IRQ_SRCS_MSK_EE(ee), (1 << pipe), 0);

	/* Pipe Disable */
	bam_write_reg_field(base, P_CTRL(pipe), P_EN, 0);
}

/**
 * Enable a BAM pipe
 *
 */
void bam_pipe_enable(void *base, u32 pipe)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).pipe=%d.", __func__, base, pipe);

	if (bam_read_reg_field(base, P_CTRL(pipe), P_EN))
		SPS_DBG2("sps:bam=0x%p(va).pipe=%d is already enabled.\n",
				base, pipe);
	else
		bam_write_reg_field(base, P_CTRL(pipe), P_EN, 1);
}

/**
 * Diasble a BAM pipe
 *
 */
void bam_pipe_disable(void *base, u32 pipe)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).pipe=%d.", __func__, base, pipe);

	bam_write_reg_field(base, P_CTRL(pipe), P_EN, 0);
}

/**
 * Check if a BAM pipe is enabled.
 *
 */
int bam_pipe_is_enabled(void *base, u32 pipe)
{
	return bam_read_reg_field(base, P_CTRL(pipe), P_EN);
}

/**
 * Configure interrupt for a BAM pipe
 *
 */
void bam_pipe_set_irq(void *base, u32 pipe, enum bam_enable irq_en,
		      u32 src_mask, u32 ee)
{
	SPS_DBG2("sps:%s:bam=0x%p(va).pipe=%d.", __func__, base, pipe);
	if (src_mask & BAM_PIPE_IRQ_RST_ERROR) {
		if (enhd_pipe)
			bam_write_reg_field(base, IRQ_EN,
					IRQ_EN_BAM_ERROR_EN, 0);
		else {
			src_mask &= ~BAM_PIPE_IRQ_RST_ERROR;
			SPS_DBG2("sps: SPS_O_RST_ERROR is not supported\n");
		}
	}
	if (src_mask & BAM_PIPE_IRQ_HRESP_ERROR) {
		if (enhd_pipe)
			bam_write_reg_field(base, IRQ_EN,
					IRQ_EN_BAM_HRESP_ERR_EN, 0);
		else {
			src_mask &= ~BAM_PIPE_IRQ_HRESP_ERROR;
			SPS_DBG2("sps: SPS_O_HRESP_ERROR is not supported\n");
		}
	}

	bam_write_reg(base, P_IRQ_EN(pipe), src_mask);
	bam_write_reg_field(base, IRQ_SRCS_MSK_EE(ee), (1 << pipe), irq_en);
}

/**
 * Configure a BAM pipe for satellite MTI use
 *
 */
void bam_pipe_satellite_mti(void *base, u32 pipe, u32 irq_gen_addr, u32 ee)
{
	bam_write_reg(base, P_IRQ_EN(pipe), 0);
#ifndef CONFIG_SPS_SUPPORT_NDP_BAM
	bam_write_reg(base, P_IRQ_DEST_ADDR(pipe), irq_gen_addr);
	bam_write_reg_field(base, IRQ_SIC_SEL, (1 << pipe), 1);
#endif
	bam_write_reg_field(base, IRQ_SRCS_MSK, (1 << pipe), 1);
}

/**
 * Configure MTI for a BAM pipe
 *
 */
void bam_pipe_set_mti(void *base, u32 pipe, enum bam_enable irq_en,
		      u32 src_mask, u32 irq_gen_addr)
{
	/*
	 * MTI use is only supported on BAMs when global config is controlled
	 * by a remote processor.
	 * Consequently, the global configuration register to enable SIC (MTI)
	 * support cannot be accessed.
	 * The remote processor must be relied upon to enable the SIC and the
	 * interrupt. Since the remote processor enable both SIC and interrupt,
	 * the interrupt enable mask must be set to zero for polling mode.
	 */
#ifndef CONFIG_SPS_SUPPORT_NDP_BAM
	bam_write_reg(base, P_IRQ_DEST_ADDR(pipe), irq_gen_addr);
#endif
	if (!irq_en)
		src_mask = 0;

	bam_write_reg(base, P_IRQ_EN(pipe), src_mask);
}

/**
 * Get and Clear BAM pipe IRQ status
 *
 */
u32 bam_pipe_get_and_clear_irq_status(void *base, u32 pipe)
{
	u32 status = 0;

	status = bam_read_reg(base, P_IRQ_STTS(pipe));
	bam_write_reg(base, P_IRQ_CLR(pipe), status);

	return status;
}

/**
 * Set write offset for a BAM pipe
 *
 */
void bam_pipe_set_desc_write_offset(void *base, u32 pipe, u32 next_write)
{
	/*
	 * It is not necessary to perform a read-modify-write masking to write
	 * the P_DESC_FIFO_PEER_OFST value, since the other field in the
	 * register (P_BYTES_CONSUMED) is read-only.
	 */
	bam_write_reg_field(base, P_EVNT_REG(pipe), P_DESC_FIFO_PEER_OFST,
			    next_write);
}

/**
 * Get write offset for a BAM pipe
 *
 */
u32 bam_pipe_get_desc_write_offset(void *base, u32 pipe)
{
	return bam_read_reg_field(base, P_EVNT_REG(pipe),
				  P_DESC_FIFO_PEER_OFST);
}

/**
 * Get read offset for a BAM pipe
 *
 */
u32 bam_pipe_get_desc_read_offset(void *base, u32 pipe)
{
	return bam_read_reg_field(base, P_SW_OFSTS(pipe), SW_DESC_OFST);
}

/**
 * Configure inactivity timer count for a BAM pipe
 *
 */
void bam_pipe_timer_config(void *base, u32 pipe, enum bam_pipe_timer_mode mode,
			 u32 timeout_count)
{
	u32 for_all_pipes = 0;

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for_all_pipes = bam_read_reg_field(base, REVISION,
						BAM_NUM_INACTIV_TMRS);
#endif

	if (for_all_pipes) {
#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
		bam_write_reg_field(base, TIMER_CTRL, TIMER_MODE, mode);
		bam_write_reg_field(base, TIMER_CTRL, TIMER_TRSHLD,
				    timeout_count);
#endif
	} else {
		bam_write_reg_field(base, P_TIMER_CTRL(pipe), P_TIMER_MODE,
					mode);
		bam_write_reg_field(base, P_TIMER_CTRL(pipe), P_TIMER_TRSHLD,
				    timeout_count);
	}
}

/**
 * Reset inactivity timer for a BAM pipe
 *
 */
void bam_pipe_timer_reset(void *base, u32 pipe)
{
	u32 for_all_pipes = 0;

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for_all_pipes = bam_read_reg_field(base, REVISION,
						BAM_NUM_INACTIV_TMRS);
#endif

	if (for_all_pipes) {
#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
		/* reset */
		bam_write_reg_field(base, TIMER_CTRL, TIMER_RST, 0);
		/* active */
		bam_write_reg_field(base, TIMER_CTRL, TIMER_RST, 1);
#endif
	} else {
		/* reset */
		bam_write_reg_field(base, P_TIMER_CTRL(pipe), P_TIMER_RST, 0);
		/* active */
		bam_write_reg_field(base, P_TIMER_CTRL(pipe), P_TIMER_RST, 1);
	}
}

/**
 * Get inactivity timer count for a BAM pipe
 *
 */
u32 bam_pipe_timer_get_count(void *base, u32 pipe)
{
	return bam_read_reg(base, P_TIMER(pipe));
}

/* output the content of BAM-level registers */
void print_bam_reg(void *virt_addr)
{
	int i, n;
	u32 *bam = (u32 *) virt_addr;
	u32 ctrl;
	u32 ver;
	u32 pipes;

	if (bam == NULL)
		return;

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	ctrl = bam[0x0 / 4];
	ver = bam[0x4 / 4];
	pipes = bam[0x3c / 4];
#else
	ctrl = bam[0xf80 / 4];
	ver = bam[0xf84 / 4];
	pipes = bam[0xfbc / 4];
#endif

	SPS_INFO("\nsps:<bam-begin> --- Content of BAM-level registers---\n");

	SPS_INFO("BAM_CTRL: 0x%x.\n", ctrl);
	SPS_INFO("BAM_REVISION: 0x%x.\n", ver);
	SPS_INFO("NUM_PIPES: 0x%x.\n", pipes);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for (i = 0x0; i < 0x80; i += 0x10)
#else
	for (i = 0xf80; i < 0x1000; i += 0x10)
#endif
		SPS_INFO("bam addr 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i,
			bam[i / 4], bam[(i / 4) + 1],
			bam[(i / 4) + 2], bam[(i / 4) + 3]);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for (i = 0x800, n = 0; n++ < 8; i += 0x80)
#else
	for (i = 0x1800, n = 0; n++ < 4; i += 0x80)
#endif
		SPS_INFO("bam addr 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i,
			bam[i / 4], bam[(i / 4) + 1],
			bam[(i / 4) + 2], bam[(i / 4) + 3]);

	SPS_INFO("\nsps:<bam-begin> --- Content of BAM-level registers ---\n");
}

/* output the content of BAM pipe registers */
void print_bam_pipe_reg(void *virt_addr, u32 pipe_index)
{
	int i;
	u32 *bam = (u32 *) virt_addr;
	u32 pipe = pipe_index;

	if (bam == NULL)
		return;

	SPS_INFO("\nsps:<pipe-begin> --- Content of Pipe %d registers ---\n",
			pipe);

	SPS_INFO("-- Pipe Management Registers --\n");

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for (i = 0x1000 + 0x1000 * pipe; i < 0x1000 + 0x1000 * pipe + 0x80;
	    i += 0x10)
#else
	for (i = 0x0000 + 0x80 * pipe; i < 0x0000 + 0x80 * (pipe + 1);
	    i += 0x10)
#endif
		SPS_INFO("bam addr 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i,
			bam[i / 4], bam[(i / 4) + 1],
			bam[(i / 4) + 2], bam[(i / 4) + 3]);

	SPS_INFO("-- Pipe Configuration and Internal State Registers --\n");

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	for (i = 0x1800 + 0x1000 * pipe; i < 0x1800 + 0x1000 * pipe + 0x40;
	    i += 0x10)
#else
	for (i = 0x1000 + 0x40 * pipe; i < 0x1000 + 0x40 * (pipe + 1);
	    i += 0x10)
#endif
		SPS_INFO("bam addr 0x%x: 0x%x,0x%x,0x%x,0x%x.\n", i,
			bam[i / 4], bam[(i / 4) + 1],
			bam[(i / 4) + 2], bam[(i / 4) + 3]);

	SPS_INFO("\nsps:<pipe-end> --- Content of Pipe %d registers ---\n",
			pipe);
}

/* output the content of selected BAM-level registers */
void print_bam_selected_reg(void *virt_addr, u32 ee)
{
	void *base = virt_addr;

	u32 bam_ctrl;
	u32 bam_revision;
	u32 bam_rev_num;
	u32 bam_rev_ee_num;

	u32 bam_num_pipes;
	u32 bam_pipe_num;
	u32 bam_data_addr_bus_width;

	u32 bam_desc_cnt_trshld;
	u32 bam_desc_cnt_trd_val;

	u32 bam_irq_en;
	u32 bam_irq_stts;

	u32 bam_irq_src_ee = 0;
	u32 bam_irq_msk_ee = 0;
	u32 bam_irq_unmsk_ee = 0;
	u32 bam_pipe_attr_ee = 0;

	u32 bam_ahb_err_ctrl;
	u32 bam_ahb_err_addr;
	u32 bam_ahb_err_data;
	u32 bam_cnfg_bits;

	u32 bam_sw_rev = 0;
	u32 bam_timer = 0;
	u32 bam_timer_ctrl = 0;
	u32 bam_ahb_err_addr_msb = 0;

	if (base == NULL)
		return;

	bam_ctrl = bam_read_reg(base, CTRL);
	bam_revision = bam_read_reg(base, REVISION);
	bam_rev_num = bam_read_reg_field(base, REVISION, BAM_REVISION);
	bam_rev_ee_num = bam_read_reg_field(base, REVISION, BAM_NUM_EES);

	bam_num_pipes = bam_read_reg(base, NUM_PIPES);
	bam_pipe_num = bam_read_reg_field(base, NUM_PIPES, BAM_NUM_PIPES);
	bam_data_addr_bus_width = bam_read_reg_field(base, NUM_PIPES,
					BAM_DATA_ADDR_BUS_WIDTH);

	bam_desc_cnt_trshld = bam_read_reg(base, DESC_CNT_TRSHLD);
	bam_desc_cnt_trd_val = bam_read_reg_field(base, DESC_CNT_TRSHLD,
					BAM_DESC_CNT_TRSHLD);

	bam_irq_en = bam_read_reg(base, IRQ_EN);
	bam_irq_stts = bam_read_reg(base, IRQ_STTS);

	if (ee < BAM_MAX_EES) {
		bam_irq_src_ee = bam_read_reg(base, IRQ_SRCS_EE(ee));
		bam_irq_msk_ee = bam_read_reg(base, IRQ_SRCS_MSK_EE(ee));
		bam_irq_unmsk_ee = bam_read_reg(base, IRQ_SRCS_UNMASKED_EE(ee));
	}

	bam_ahb_err_ctrl = bam_read_reg(base, AHB_MASTER_ERR_CTRLS);
	bam_ahb_err_addr = bam_read_reg(base, AHB_MASTER_ERR_ADDR);
	bam_ahb_err_data = bam_read_reg(base, AHB_MASTER_ERR_DATA);
	bam_cnfg_bits = bam_read_reg(base, CNFG_BITS);

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	bam_sw_rev = bam_read_reg(base, SW_REVISION);
	bam_timer = bam_read_reg(base, TIMER);
	bam_timer_ctrl = bam_read_reg(base, TIMER_CTRL);
	bam_ahb_err_addr_msb = SPS_LPAE ?
		bam_read_reg(base, AHB_MASTER_ERR_ADDR_MSB) : 0;
	if (ee < BAM_MAX_EES)
		bam_pipe_attr_ee = enhd_pipe ?
			bam_read_reg(base, PIPE_ATTR_EE(ee)) : 0x0;
#endif


	SPS_INFO("\nsps:<bam-begin> --- BAM-level registers ---\n\n");

	SPS_INFO("BAM_CTRL: 0x%x\n", bam_ctrl);
	SPS_INFO("BAM_REVISION: 0x%x\n", bam_revision);
	SPS_INFO("    REVISION: 0x%x\n", bam_rev_num);
	SPS_INFO("    NUM_EES: %d\n", bam_rev_ee_num);
	SPS_INFO("BAM_SW_REVISION: 0x%x\n", bam_sw_rev);
	SPS_INFO("BAM_NUM_PIPES: %d\n", bam_num_pipes);
	SPS_INFO("BAM_DATA_ADDR_BUS_WIDTH: %d\n",
			((bam_data_addr_bus_width == 0x0) ? 32 : 36));
	SPS_INFO("    NUM_PIPES: %d\n", bam_pipe_num);
	SPS_INFO("BAM_DESC_CNT_TRSHLD: 0x%x\n", bam_desc_cnt_trshld);
	SPS_INFO("    DESC_CNT_TRSHLD: 0x%x (%d)\n", bam_desc_cnt_trd_val,
			bam_desc_cnt_trd_val);

	SPS_INFO("BAM_IRQ_EN: 0x%x\n", bam_irq_en);
	SPS_INFO("BAM_IRQ_STTS: 0x%x\n", bam_irq_stts);

	if (ee < BAM_MAX_EES) {
		SPS_INFO("BAM_IRQ_SRCS_EE(%d): 0x%x\n", ee, bam_irq_src_ee);
		SPS_INFO("BAM_IRQ_SRCS_MSK_EE(%d): 0x%x\n", ee, bam_irq_msk_ee);
		SPS_INFO("BAM_IRQ_SRCS_UNMASKED_EE(%d): 0x%x\n", ee,
				bam_irq_unmsk_ee);
		SPS_INFO("BAM_PIPE_ATTR_EE(%d): 0x%x\n", ee, bam_pipe_attr_ee);
	}

	SPS_INFO("BAM_AHB_MASTER_ERR_CTRLS: 0x%x\n", bam_ahb_err_ctrl);
	SPS_INFO("BAM_AHB_MASTER_ERR_ADDR: 0x%x\n", bam_ahb_err_addr);
	SPS_INFO("BAM_AHB_MASTER_ERR_ADDR_MSB: 0x%x\n", bam_ahb_err_addr_msb);
	SPS_INFO("BAM_AHB_MASTER_ERR_DATA: 0x%x\n", bam_ahb_err_data);

	SPS_INFO("BAM_CNFG_BITS: 0x%x\n", bam_cnfg_bits);
	SPS_INFO("BAM_TIMER: 0x%x\n", bam_timer);
	SPS_INFO("BAM_TIMER_CTRL: 0x%x\n", bam_timer_ctrl);

	SPS_INFO("\nsps:<bam-end> --- BAM-level registers ---\n\n");
}

/* output the content of selected BAM pipe registers */
void print_bam_pipe_selected_reg(void *virt_addr, u32 pipe_index)
{
	void *base = virt_addr;
	u32 pipe = pipe_index;

	u32 p_ctrl;
	u32 p_sys_mode;
	u32 p_direction;
	u32 p_lock_group = 0;

	u32 p_irq_en;
	u32 p_irq_stts;
	u32 p_irq_stts_eot;
	u32 p_irq_stts_int;

	u32 p_prd_sdbd;
	u32 p_bytes_free;
	u32 p_prd_ctrl;
	u32 p_prd_toggle;
	u32 p_prd_sb_updated;

	u32 p_con_sdbd;
	u32 p_bytes_avail;
	u32 p_con_ctrl;
	u32 p_con_toggle;
	u32 p_con_ack_toggle;
	u32 p_con_ack_toggle_r;
	u32 p_con_wait_4_ack;
	u32 p_con_sb_updated;

	u32 p_sw_offset;
	u32 p_read_pointer;
	u32 p_evnt_reg;
	u32 p_write_pointer;

	u32 p_evnt_dest;
	u32 p_evnt_dest_msb = 0;
	u32 p_desc_fifo_addr;
	u32 p_desc_fifo_addr_msb = 0;
	u32 p_desc_fifo_size;
	u32 p_data_fifo_addr;
	u32 p_data_fifo_addr_msb = 0;
	u32 p_data_fifo_size;
	u32 p_fifo_sizes;

	u32 p_evnt_trd;
	u32 p_evnt_trd_val;

	u32 p_retr_ct;
	u32 p_retr_offset;
	u32 p_si_ct;
	u32 p_si_offset;
	u32 p_df_ct = 0;
	u32 p_df_offset = 0;
	u32 p_au_ct1;
	u32 p_psm_ct2;
	u32 p_psm_ct3;
	u32 p_psm_ct3_msb = 0;
	u32 p_psm_ct4;
	u32 p_psm_ct5;

	u32 p_timer;
	u32 p_timer_ctrl;

	if (base == NULL)
		return;

	p_ctrl = bam_read_reg(base, P_CTRL(pipe));
	p_sys_mode = bam_read_reg_field(base, P_CTRL(pipe), P_SYS_MODE);
	p_direction = bam_read_reg_field(base, P_CTRL(pipe), P_DIRECTION);

	p_irq_en = bam_read_reg(base, P_IRQ_EN(pipe));
	p_irq_stts = bam_read_reg(base, P_IRQ_STTS(pipe));
	p_irq_stts_eot = bam_read_reg_field(base, P_IRQ_STTS(pipe),
					P_IRQ_STTS_P_TRNSFR_END_IRQ);
	p_irq_stts_int = bam_read_reg_field(base, P_IRQ_STTS(pipe),
					P_IRQ_STTS_P_PRCSD_DESC_IRQ);

	p_prd_sdbd = bam_read_reg(base, P_PRDCR_SDBND(pipe));
	p_bytes_free = bam_read_reg_field(base, P_PRDCR_SDBND(pipe),
					P_PRDCR_SDBNDn_BAM_P_BYTES_FREE);
	p_prd_ctrl = bam_read_reg_field(base, P_PRDCR_SDBND(pipe),
					P_PRDCR_SDBNDn_BAM_P_CTRL);
	p_prd_toggle = bam_read_reg_field(base, P_PRDCR_SDBND(pipe),
					P_PRDCR_SDBNDn_BAM_P_TOGGLE);
	p_prd_sb_updated = bam_read_reg_field(base, P_PRDCR_SDBND(pipe),
					P_PRDCR_SDBNDn_BAM_P_SB_UPDATED);
	p_con_sdbd = bam_read_reg(base, P_CNSMR_SDBND(pipe));
	p_bytes_avail = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_BYTES_AVAIL);
	p_con_ctrl = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_CTRL);
	p_con_toggle = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_TOGGLE);
	p_con_ack_toggle = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE);
	p_con_ack_toggle_r = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_ACK_TOGGLE_R);
	p_con_wait_4_ack = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_WAIT_4_ACK);
	p_con_sb_updated = bam_read_reg_field(base, P_CNSMR_SDBND(pipe),
					P_CNSMR_SDBNDn_BAM_P_SB_UPDATED);

	p_sw_offset = bam_read_reg(base, P_SW_OFSTS(pipe));
	p_read_pointer = bam_read_reg_field(base, P_SW_OFSTS(pipe),
						SW_DESC_OFST);
	p_evnt_reg = bam_read_reg(base, P_EVNT_REG(pipe));
	p_write_pointer = bam_read_reg_field(base, P_EVNT_REG(pipe),
						P_DESC_FIFO_PEER_OFST);

	p_evnt_dest = bam_read_reg(base, P_EVNT_DEST_ADDR(pipe));
	p_desc_fifo_addr = bam_read_reg(base, P_DESC_FIFO_ADDR(pipe));
	p_desc_fifo_size = bam_read_reg_field(base, P_FIFO_SIZES(pipe),
						P_DESC_FIFO_SIZE);
	p_data_fifo_addr = bam_read_reg(base, P_DATA_FIFO_ADDR(pipe));
	p_data_fifo_size = bam_read_reg_field(base, P_FIFO_SIZES(pipe),
						P_DATA_FIFO_SIZE);
	p_fifo_sizes = bam_read_reg(base, P_FIFO_SIZES(pipe));

	p_evnt_trd = bam_read_reg(base, P_EVNT_GEN_TRSHLD(pipe));
	p_evnt_trd_val = bam_read_reg_field(base, P_EVNT_GEN_TRSHLD(pipe),
					P_EVNT_GEN_TRSHLD_P_TRSHLD);

	p_retr_ct = bam_read_reg(base, P_RETR_CNTXT(pipe));
	p_retr_offset = bam_read_reg_field(base, P_RETR_CNTXT(pipe),
					P_RETR_CNTXT_RETR_DESC_OFST);
	p_si_ct = bam_read_reg(base, P_SI_CNTXT(pipe));
	p_si_offset = bam_read_reg_field(base, P_SI_CNTXT(pipe),
					P_SI_CNTXT_SI_DESC_OFST);
	p_au_ct1 = bam_read_reg(base, P_AU_PSM_CNTXT_1(pipe));
	p_psm_ct2 = bam_read_reg(base, P_PSM_CNTXT_2(pipe));
	p_psm_ct3 = bam_read_reg(base, P_PSM_CNTXT_3(pipe));
	p_psm_ct4 = bam_read_reg(base, P_PSM_CNTXT_4(pipe));
	p_psm_ct5 = bam_read_reg(base, P_PSM_CNTXT_5(pipe));

	p_timer = bam_read_reg(base, P_TIMER(pipe));
	p_timer_ctrl = bam_read_reg(base, P_TIMER_CTRL(pipe));

#ifdef CONFIG_SPS_SUPPORT_NDP_BAM
	p_evnt_dest_msb = SPS_LPAE ?
		bam_read_reg(base, P_EVNT_DEST_ADDR_MSB(pipe)) : 0;

	p_desc_fifo_addr_msb = SPS_LPAE ?
		bam_read_reg(base, P_DESC_FIFO_ADDR_MSB(pipe)) : 0;
	p_data_fifo_addr_msb = SPS_LPAE ?
		bam_read_reg(base, P_DATA_FIFO_ADDR_MSB(pipe)) : 0;

	p_psm_ct3_msb = SPS_LPAE ? bam_read_reg(base, P_PSM_CNTXT_3(pipe)) : 0;
	p_lock_group = bam_read_reg_field(base, P_CTRL(pipe), P_LOCK_GROUP);
	p_df_ct = bam_read_reg(base, P_DF_CNTXT(pipe));
	p_df_offset = bam_read_reg_field(base, P_DF_CNTXT(pipe),
					P_DF_CNTXT_DF_DESC_OFST);
#endif

	SPS_INFO("\nsps:<pipe-begin> --- Registers of Pipe %d ---\n\n", pipe);

	SPS_INFO("BAM_P_CTRL: 0x%x\n", p_ctrl);
	SPS_INFO("    SYS_MODE: %d\n", p_sys_mode);
	if (p_direction)
		SPS_INFO("    DIRECTION:%d->Producer\n", p_direction);
	else
		SPS_INFO("    DIRECTION:%d->Consumer\n", p_direction);
	SPS_INFO("    LOCK_GROUP: 0x%x (%d)\n", p_lock_group, p_lock_group);

	SPS_INFO("BAM_P_IRQ_EN: 0x%x\n", p_irq_en);
	SPS_INFO("BAM_P_IRQ_STTS: 0x%x\n", p_irq_stts);
	SPS_INFO("    TRNSFR_END_IRQ(EOT): 0x%x\n", p_irq_stts_eot);
	SPS_INFO("    PRCSD_DESC_IRQ(INT): 0x%x\n", p_irq_stts_int);

	SPS_INFO("BAM_P_PRDCR_SDBND: 0x%x\n", p_prd_sdbd);
	SPS_INFO("    BYTES_FREE: 0x%x (%d)\n", p_bytes_free, p_bytes_free);
	SPS_INFO("    CTRL: 0x%x\n", p_prd_ctrl);
	SPS_INFO("    TOGGLE: %d\n", p_prd_toggle);
	SPS_INFO("    SB_UPDATED: %d\n", p_prd_sb_updated);
	SPS_INFO("BAM_P_CNSMR_SDBND: 0x%x\n", p_con_sdbd);
	SPS_INFO("    WAIT_4_ACK: %d\n", p_con_wait_4_ack);
	SPS_INFO("    BYTES_AVAIL: 0x%x (%d)\n", p_bytes_avail, p_bytes_avail);
	SPS_INFO("    CTRL: 0x%x\n", p_con_ctrl);
	SPS_INFO("    TOGGLE: %d\n", p_con_toggle);
	SPS_INFO("    ACK_TOGGLE: %d\n", p_con_ack_toggle);
	SPS_INFO("    ACK_TOGGLE_R: %d\n", p_con_ack_toggle_r);
	SPS_INFO("    SB_UPDATED: %d\n", p_con_sb_updated);

	SPS_INFO("BAM_P_SW_DESC_OFST: 0x%x\n", p_sw_offset);
	SPS_INFO("    SW_DESC_OFST: 0x%x\n", p_read_pointer);
	SPS_INFO("BAM_P_EVNT_REG: 0x%x\n", p_evnt_reg);
	SPS_INFO("    DESC_FIFO_PEER_OFST: 0x%x\n", p_write_pointer);

	SPS_INFO("BAM_P_RETR_CNTXT: 0x%x\n", p_retr_ct);
	SPS_INFO("    RETR_OFFSET: 0x%x\n", p_retr_offset);
	SPS_INFO("BAM_P_SI_CNTXT: 0x%x\n", p_si_ct);
	SPS_INFO("    SI_OFFSET: 0x%x\n", p_si_offset);
	SPS_INFO("BAM_P_DF_CNTXT: 0x%x\n", p_df_ct);
	SPS_INFO("    DF_OFFSET: 0x%x\n", p_df_offset);

	SPS_INFO("BAM_P_DESC_FIFO_ADDR: 0x%x\n", p_desc_fifo_addr);
	SPS_INFO("BAM_P_DESC_FIFO_ADDR_MSB: 0x%x\n", p_desc_fifo_addr_msb);
	SPS_INFO("BAM_P_DATA_FIFO_ADDR: 0x%x\n", p_data_fifo_addr);
	SPS_INFO("BAM_P_DATA_FIFO_ADDR_MSB: 0x%x\n", p_data_fifo_addr_msb);
	SPS_INFO("BAM_P_FIFO_SIZES: 0x%x\n", p_fifo_sizes);
	SPS_INFO("    DESC_FIFO_SIZE: 0x%x (%d)\n", p_desc_fifo_size,
							p_desc_fifo_size);
	SPS_INFO("    DATA_FIFO_SIZE: 0x%x (%d)\n", p_data_fifo_size,
							p_data_fifo_size);

	SPS_INFO("BAM_P_EVNT_DEST_ADDR: 0x%x\n", p_evnt_dest);
	SPS_INFO("BAM_P_EVNT_DEST_ADDR_MSB: 0x%x\n", p_evnt_dest_msb);
	SPS_INFO("BAM_P_EVNT_GEN_TRSHLD: 0x%x\n", p_evnt_trd);
	SPS_INFO("    EVNT_GEN_TRSHLD: 0x%x (%d)\n", p_evnt_trd_val,
							p_evnt_trd_val);

	SPS_INFO("BAM_P_AU_PSM_CNTXT_1: 0x%x\n", p_au_ct1);
	SPS_INFO("BAM_P_PSM_CNTXT_2: 0x%x\n", p_psm_ct2);
	SPS_INFO("BAM_P_PSM_CNTXT_3: 0x%x\n", p_psm_ct3);
	SPS_INFO("BAM_P_PSM_CNTXT_3_MSB: 0x%x\n", p_psm_ct3_msb);
	SPS_INFO("BAM_P_PSM_CNTXT_4: 0x%x\n", p_psm_ct4);
	SPS_INFO("BAM_P_PSM_CNTXT_5: 0x%x\n", p_psm_ct5);
	SPS_INFO("BAM_P_TIMER: 0x%x\n", p_timer);
	SPS_INFO("BAM_P_TIMER_CTRL: 0x%x\n", p_timer_ctrl);

	SPS_INFO("\nsps:<pipe-end> --- Registers of Pipe %d ---\n\n", pipe);
}

/* output descriptor FIFO of a pipe */
void print_bam_pipe_desc_fifo(void *virt_addr, u32 pipe_index, u32 option)
{
	void *base = virt_addr;
	u32 pipe = pipe_index;
	u32 desc_fifo_addr;
	u32 desc_fifo_size;
	u32 *desc_fifo;
	int i;
	char desc_info[MAX_MSG_LEN];

	if (base == NULL)
		return;

	desc_fifo_addr = bam_read_reg(base, P_DESC_FIFO_ADDR(pipe));
	desc_fifo_size = bam_read_reg_field(base, P_FIFO_SIZES(pipe),
						P_DESC_FIFO_SIZE);

	if (desc_fifo_addr == 0) {
		SPS_ERR("sps:%s:desc FIFO address of Pipe %d is NULL.\n",
			__func__, pipe);
		return;
	} else if (desc_fifo_size == 0) {
		SPS_ERR("sps:%s:desc FIFO size of Pipe %d is 0.\n",
			__func__, pipe);
		return;
	}

	SPS_INFO("\nsps:<desc-begin> --- descriptor FIFO of Pipe %d -----\n\n",
			pipe);

	SPS_INFO("BAM_P_DESC_FIFO_ADDR: 0x%x\n"
		"BAM_P_DESC_FIFO_SIZE: 0x%x (%d)\n\n",
		desc_fifo_addr, desc_fifo_size, desc_fifo_size);

	desc_fifo = (u32 *) phys_to_virt(desc_fifo_addr);

	if (option == 100) {
		SPS_INFO("----- start of data blocks -----\n");
		for (i = 0; i < desc_fifo_size; i += 8) {
			u32 *data_block_vir;
			u32 data_block_phy = desc_fifo[i / 4];

			if (data_block_phy) {
				data_block_vir =
					(u32 *) phys_to_virt(data_block_phy);

				SPS_INFO("desc addr:0x%x; data addr:0x%x:\n",
					desc_fifo_addr + i, data_block_phy);
				SPS_INFO("0x%x, 0x%x, 0x%x, 0x%x\n",
					data_block_vir[0], data_block_vir[1],
					data_block_vir[2], data_block_vir[3]);
				SPS_INFO("0x%x, 0x%x, 0x%x, 0x%x\n",
					data_block_vir[4], data_block_vir[5],
					data_block_vir[6], data_block_vir[7]);
				SPS_INFO("0x%x, 0x%x, 0x%x, 0x%x\n",
					data_block_vir[8], data_block_vir[9],
					data_block_vir[10], data_block_vir[11]);
				SPS_INFO("0x%x, 0x%x, 0x%x, 0x%x\n\n",
					data_block_vir[12], data_block_vir[13],
					data_block_vir[14], data_block_vir[15]);
			}
		}
		SPS_INFO("----- end of data blocks -----\n");
	} else if (option) {
		u32 size = option * 128;
		u32 current_desc = bam_pipe_get_desc_read_offset(base,
								pipe_index);
		u32 begin = 0;
		u32 end = desc_fifo_size;

		if (current_desc > size / 2)
			begin = current_desc - size / 2;

		if (desc_fifo_size > current_desc + size / 2)
			end = current_desc + size / 2;

		SPS_INFO("------------ begin of partial FIFO ------------\n\n");

		SPS_INFO("desc addr; desc content; desc flags\n");
		for (i = begin; i < end; i += 0x8) {
			u32 offset;
			u32 flags = desc_fifo[(i / 4) + 1] >> 16;

			memset(desc_info, 0, sizeof(desc_info));
			offset = scnprintf(desc_info, 40, "0x%x: 0x%x, 0x%x: ",
				desc_fifo_addr + i,
				desc_fifo[i / 4], desc_fifo[(i / 4) + 1]);

			if (flags & SPS_IOVEC_FLAG_INT)
				offset += scnprintf(desc_info + offset, 5,
							"INT ");
			if (flags & SPS_IOVEC_FLAG_EOT)
				offset += scnprintf(desc_info + offset, 5,
							"EOT ");
			if (flags & SPS_IOVEC_FLAG_EOB)
				offset += scnprintf(desc_info + offset, 5,
							"EOB ");
			if (flags & SPS_IOVEC_FLAG_NWD)
				offset += scnprintf(desc_info + offset, 5,
							"NWD ");
			if (flags & SPS_IOVEC_FLAG_CMD)
				offset += scnprintf(desc_info + offset, 5,
							"CMD ");
			if (flags & SPS_IOVEC_FLAG_LOCK)
				offset += scnprintf(desc_info + offset, 5,
							"LCK ");
			if (flags & SPS_IOVEC_FLAG_UNLOCK)
				offset += scnprintf(desc_info + offset, 5,
							"UNL ");
			if (flags & SPS_IOVEC_FLAG_IMME)
				offset += scnprintf(desc_info + offset, 5,
							"IMM ");

			SPS_INFO("%s\n", desc_info);
		}

		SPS_INFO("\n------------  end of partial FIFO  ------------\n");
	} else {
		SPS_INFO("---------------- begin of FIFO ----------------\n\n");

		for (i = 0; i < desc_fifo_size; i += 0x10)
			SPS_INFO("addr 0x%x: 0x%x, 0x%x, 0x%x, 0x%x.\n",
				desc_fifo_addr + i,
				desc_fifo[i / 4], desc_fifo[(i / 4) + 1],
				desc_fifo[(i / 4) + 2], desc_fifo[(i / 4) + 3]);

		SPS_INFO("\n----------------  end of FIFO  ----------------\n");
	}

	SPS_INFO("\nsps:<desc-end> --- descriptor FIFO of Pipe %d -----\n\n",
			pipe);
}

/* output BAM_TEST_BUS_REG with specified TEST_BUS_SEL */
void print_bam_test_bus_reg(void *base, u32 tb_sel)
{
	u32 i;
	u32 test_bus_selection[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			0x20, 0x21, 0x22, 0x23,
			0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
	u32 size = sizeof(test_bus_selection) / sizeof(u32);

	if (base == NULL) {
		SPS_ERR("sps:%s:BAM is NULL.\n", __func__);
		return;
	}

	if (tb_sel) {
		SPS_INFO("\nsps:Specified TEST_BUS_SEL value: 0x%x\n", tb_sel);
		bam_write_reg_field(base, TEST_BUS_SEL, BAM_TESTBUS_SEL,
					tb_sel);
		SPS_INFO("sps:BAM_TEST_BUS_REG:0x%x for TEST_BUS_SEL:0x%x\n\n",
			bam_read_reg(base, TEST_BUS_REG),
			bam_read_reg_field(base, TEST_BUS_SEL,
						BAM_TESTBUS_SEL));
	}

	SPS_INFO("\nsps:<testbus-begin> --- BAM TEST_BUS dump -----\n\n");

	/* output other selections */
	for (i = 0; i < size; i++) {
		bam_write_reg_field(base, TEST_BUS_SEL, BAM_TESTBUS_SEL,
					test_bus_selection[i]);

		SPS_INFO("sps:TEST_BUS_REG:0x%x\t  TEST_BUS_SEL:0x%x\n",
			bam_read_reg(base, TEST_BUS_REG),
			bam_read_reg_field(base, TEST_BUS_SEL,
					BAM_TESTBUS_SEL));
	}

	SPS_INFO("\nsps:<testbus-end> --- BAM TEST_BUS dump -----\n\n");
}
