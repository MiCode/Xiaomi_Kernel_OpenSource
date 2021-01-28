/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/******************************************************************************
 * cam_regs.h - MT6758 cam registers
 *
 * DESCRIPTION:
 *     This file provide register addresses and chip dependency infos in CAMSYS.
 *
 ******************************************************************************/

/**
 *    CAM interrupt status
 */
/* normal siganl */
#define VS_INT_ST       (1L<<0)
#define TG_INT1_ST      (1L<<1)
#define TG_INT2_ST      (1L<<2)
#define EXPDON_ST       (1L<<3)
#define SOF_INT_ST      (1L<<6)
#define HW_PASS1_DON_ST (1L<<12)
#define SW_PASS1_DON_ST (1L<<14)

/* err status */
#define TG_ERR_ST       (1L<<4)
#define TG_GBERR_ST     (1L<<5)
#define CQ_CODE_ERR_ST  (1L<<9)
#define CQ_APB_ERR_ST   (1L<<10)
#define CQ_VS_ERR_ST    (1L<<11)
#define DMA_ERR_ST      (1L<<29)


/* warming status */ /* CAMCTL_R1_CAMCTL_INT4_EN */
#define IMGO_ERR_ST     (1L<<0)
#define RRZO_ERR_ST     (1L<<2)
#define LCSO_ERR_ST     (1L<<4)
#define AAO_ERR_ST      (1L<<5)
#define FLKO_ERR_ST     (1L<<6)
#define UFEO_ERR_ST     (1L<<9)
#define AFO_ERR_ST      (1L<<10)
#define UFGO_ERR_ST     (1L<<11)
#define RSSO_ERR_ST     (1L<<12)
#define LMVO_ERR_ST     (1L<<13)
#define PDO_ERR_ST      (1L<<18)


/* err status_2 */

/* Dma_en */
enum{
	_IMGO_R1_EN_   = (1L<<0),
	_LTMSO_R1_EN_  = (1L<<1),
	_RRZO_R1_EN_   = (1L<<2),
	_RAWI_R2_EN_   = (1L<<3),
	_LCESO_R1_EN_  = (1L<<4),
	_AAO_R1_EN_    = (1L<<5),
	_FLKO_R1_EN_   = (1L<<6),
	_BPCI_R1_EN_   = (1L<<7),
	_LSCI_R1_EN_   = (1L<<8),
	_UFEO_R1_EN_   = (1L<<9),
	_AFO_R1_EN_    = (1L<<10),
	_UFGO_R1_EN_   = (1L<<11),
	_RSSO_R1_EN_   = (1L<<12),
	_LMVO_R1_EN_   = (1L<<13),
	_YUVBO_R1_EN_  = (1L<<14),
	_TSFSO_R1_EN_  = (1L<<15),
	_PDO_R1_EN_    = (1L<<16),
	_MQEO_R1_EN_   = (1L<<17),
	_MAEO_R1_EN_   = (1L<<18),
	_BPCI_R2_EN_   = (1L<<19),
	_BPCI_R3_EN_   = (1L<<20),
	_BPCI_R4_EN_   = (1L<<21),
	_RAWI_R3_EN_   = (1L<<22),
	_RAWI_R4_EN_   = (1L<<23),
	_RAWI_R5_EN_   = (1L<<24),
	_PDI_R1_EN_    = (1L<<25),
	_CRZO_R1_EN_   = (1L<<26),
	_CRZBO_R1_EN_  = (1L<<27),
	_UFDI_R2_EN_   = (1L<<28),
	_BPCO_R1_EN_   = (1L<<29),
	_LHSO_R1_EN_   = (1L<<30),
	_YUVCO_R1_EN_  = (1L<<31)
} ENUM_DMA_EN;

enum{
	_LCESHO_R1_EN_ = (1L<<0),
	_CRZO_R2_EN_   = (1L<<1),
	_CRZBO_R2_EN_  = (1L<<2),
	_RSSO_R2_EN_   = (1L<<3),
	_MLSCI_R1_EN_  = (1L<<4),
	_YUVO_R1_EN_   = (1L<<5)
} ENUM_DMA2_EN;

/**
 *    CAMSV_DMA_SOFT_RSTSTAT
 */
#define IMGO_SOFT_RST_STAT        (1L<<0)
#define RRZO_SOFT_RST_STAT        (1L<<1)
#define AAO_SOFT_RST_STAT         (1L<<2)
#define AFO_SOFT_RST_STAT         (1L<<3)
#define LCSO_SOFT_RST_STAT        (1L<<4)
#define UFEO_SOFT_RST_STAT        (1L<<5)
#define PDO_SOFT_RST_STAT         (1L<<6)
#define BPCI_SOFT_RST_STAT        (1L<<16)
#define CACI_SOFT_RST_STAT        (1L<<17)
#define LSCI_SOFT_RST_STAT        (1L<<18)

/**
 *    CAM DMA done status
 */
#define IMGO_DONE_ST        (1L<<0)
#define LTMSO_R1_DONE_ST    (1L<<1)
#define RRZO_DONE_ST        (1L<<2)

#define LCESO_DONE_ST       (1L<<4)
#define AAO_DONE_ST         (1L<<5)
#define FLKO_DONE_ST        (1L<<6)
#define LHSO_DONE_ST        (1L<<7)
#define RSSO_R2_DONE_ST     (1L<<8)
#define UFEO_DONE_ST        (1L<<9)
#define AFO_DONE_ST         (1L<<10)
#define UFGO_DONE_ST        (1L<<11)
#define RSSO_DONE_ST        (1L<<12)
#define LMVO_DONE_ST        (1L<<13)
#define YUVO_DONE_ST        (1L<<14)
#define YUVBO_DONE_ST       (1L<<15)
#define YUVCO_DONE_ST       (1L<<16)
#define TSFSO_DONE_ST       (1L<<17)
#define PDO_DONE_ST         (1L<<18)


#define BPCO_DONE_ST        (1L<<21)
#define CRZO_DONE_ST        (1L<<22)
#define CRZBO_DONE_ST       (1L<<23)
#define CRZO_R2_DONE_ST     (1L<<24)
#define CRZBO_R2_DONE_ST    (1L<<25)
#define RAWI_R2_DONE_ST     (1L<<26)



#define UFDI_R2_DONE_ST     (1L<<30)
#define LSCI_DONE_ST        (1L<<31)

/**
 *    CAMSV interrupt status
 */
/* normal signal */
#define SV_VS1_ST           (1L<<0)
#define SV_TG_ST1           (1L<<1)
#define SV_TG_ST2           (1L<<2)
#define SV_EXPDON_ST        (1L<<3)
#define SV_SOF_INT_ST       (1L<<7)
#define SV_HW_PASS1_DON_ST  (1L<<10)
#define SV_SW_PASS1_DON_ST  (1L<<20)
/* err status */
#define SV_TG_ERR           (1L<<4)
#define SV_TG_GBERR         (1L<<5)
#define SV_IMGO_ERR         (1L<<16)
#define SV_IMGO_OVERRUN     (1L<<17)

/**
 *    IRQ signal mask
 */
#define INT_ST_MASK_CAM     (                  \
			      VS_INT_ST       |\
			      TG_INT1_ST      |\
			      TG_INT2_ST      |\
			      EXPDON_ST       |\
			      HW_PASS1_DON_ST |\
			      SOF_INT_ST      |\
			      SW_PASS1_DON_ST)
/**
 *    dma done mask
 */
#define DMA_ST_MASK_CAM     (              \
			     IMGO_DONE_ST |\
			     UFEO_DONE_ST |\
			     RRZO_DONE_ST |\
			     EISO_DONE_ST |\
			     FLKO_DONE_ST |\
			     AFO_DONE_ST  |\
			     LCSO_DONE_ST |\
			     AAO_DONE_ST  |\
			     BPCI_DONE_ST |\
			     LSCI_DONE_ST |\
			     PDO_DONE_ST)

/**
 *    IRQ Warning Mask
 */
#define INT_ST_MASK_CAM_WARN    (                 \
				 PDO_ERR_ST      |\
				 UFGO_ERR_ST     |\
				 UFEO_ERR_ST     |\
				 RRZO_ERR_ST     |\
				 AFO_ERR_ST      |\
				 IMGO_ERR_ST     |\
				 AAO_ERR_ST      |\
				 FLKO_ERR_ST     |\
				 RSSO_ERR_ST     |\
				 LMVO_ERR_ST     |\
				 LCSO_ERR_ST)

#define INT_ST_MASK_CAM_WARN_2 0
/**
 *    IRQ Error Mask
 */
#define INT_ST_MASK_CAM_ERR     (                 \
				 TG_ERR_ST       |\
				 TG_GBERR_ST     |\
				 CQ_CODE_ERR_ST  |\
				 CQ_APB_ERR_ST   |\
				 CQ_VS_ERR_ST    |\
				 DMA_ERR_ST)


/**
 *    IRQ signal mask
 */
#define INT_ST_MASK_CAMSV       (                    \
				 SV_VS1_ST          |\
				 SV_TG_ST1          |\
				 SV_TG_ST2          |\
				 SV_EXPDON_ST       |\
				 SV_SOF_INT_ST      |\
				 SV_HW_PASS1_DON_ST |\
				 SV_SW_PASS1_DON_ST)
/**
 *    IRQ Error Mask
 */
#define INT_ST_MASK_CAMSV_ERR   (                   \
				 SV_TG_ERR         |\
				 SV_TG_GBERR       |\
				 SV_IMGO_ERR       |\
				 SV_IMGO_OVERRUN)

/**
 *    DMA CAMSV IMGO/UFO done mask
 */
#define DMA_ST_MASK_CAMSV_IMGO_OR_UFO     (              \
			     IMGO_SOFT_RST_STAT |\
			     UFEO_SOFT_RST_STAT)

#define CAMSYS_REG_CG_CON                      (ISP_CAMSYS_CONFIG_BASE + 0x0000)
#define CAMSYS_REG_CG_SET                      (ISP_CAMSYS_CONFIG_BASE + 0x0004)
#define CAMSYS_REG_CG_CLR                      (ISP_CAMSYS_CONFIG_BASE + 0x0008)
#define CAMSYS_REG_HALT1_EN                    (ISP_CAMSYS_CONFIG_BASE + 0x0350)
#define CAMSYS_REG_HALT2_EN                    (ISP_CAMSYS_CONFIG_BASE + 0x0354)
#define CAMSYS_REG_HALT3_EN                    (ISP_CAMSYS_CONFIG_BASE + 0x0358)
#define CAMSYS_REG_HALT4_EN                    (ISP_CAMSYS_CONFIG_BASE + 0x035C)
#define CAMSYS_REG_HALT1_SEC_EN                (ISP_CAMSYS_CONFIG_BASE + 0x0360)
#define CAMSYS_REG_HALT2_SEC_EN                (ISP_CAMSYS_CONFIG_BASE + 0x0364)
#define CAMSYS_REG_HALT3_SEC_EN                (ISP_CAMSYS_CONFIG_BASE + 0x0368)
#define CAMSYS_REG_HALT4_SEC_EN                (ISP_CAMSYS_CONFIG_BASE + 0x036C)



#define CAM_REG_CTL_EN(module)                  (isp_devs[module].regs + 0x0004)
#define CAM_REG_CTL_EN2(module)                 (isp_devs[module].regs + 0x0008)
#define CAM_REG_CTL_EN3(module)                 (isp_devs[module].regs + 0x000C)
#define CAM_REG_CTL_EN4(module)                 (isp_devs[module].regs + 0x0010)
#define CAM_REG_CTL_DMA_EN(module)              (isp_devs[module].regs + 0x0014)
#define CAM_REG_CTL_DMA2_EN(module)             (isp_devs[module].regs + 0x0018)
#define CAM_REG_CTL_FMT_SEL(module)             (isp_devs[module].regs + 0x0024)
#define CAM_REG_CTL_FMT2_SEL(module)            (isp_devs[module].regs + 0x0028)
#define CAM_REG_CTL_SEL(module)                 (isp_devs[module].regs + 0x001C)
#define CAM_REG_CTL_SEL2(module)                (isp_devs[module].regs + 0x0020)

#define CAM_REG_CTL_START(module)               (isp_devs[module].regs + 0x0054)
#define CAM_REG_CTL_START_ST(module)            (isp_devs[module].regs + 0x0058)
#define CAM_REG_CTL_RAW_INT_EN(module)          (isp_devs[module].regs + 0x00C0)
#define CAM_REG_CTL_RAW_INT_STATUS(module)      (isp_devs[module].regs + 0x00C4)
#define CAM_REG_CTL_RAW_INT_STATUSX(module)     (isp_devs[module].regs + 0x00C8)
#define CAM_REG_CTL_RAW_INT2_EN(module)         (isp_devs[module].regs + 0x00CC)
#define CAM_REG_CTL_RAW_INT2_STATUS(module)     (isp_devs[module].regs + 0x00D0)
#define CAM_REG_CTL_RAW_INT5_STATUS(module)     (isp_devs[module].regs + 0x00F4)
#define CAM_REG_CTL_RAW_INT2_STATUSX(module)    (isp_devs[module].regs + 0x00D4)
#define CAM_REG_CTL_RAW_INT3_EN(module)         (isp_devs[module].regs + 0x00D8)
#define CAM_REG_CTL_RAW_INT3_STATUS(module)     (isp_devs[module].regs + 0x00DC)
#define CAM_REG_CTL_RAW_INT3_STATUSX(module)    (isp_devs[module].regs + 0x00E0)
#define CAM_REG_CTL_RAW_INT4_EN(module)         (isp_devs[module].regs + 0x00E4)
#define CAM_REG_CTL_RAW_INT4_STATUS(module)     (isp_devs[module].regs + 0x00E8)
#define CAM_REG_CTL_RAW_INT4_STATUSX(module)    (isp_devs[module].regs + 0x00EC)
#define CAM_REG_CTL_SW_CTL(module)              (isp_devs[module].regs + 0x0034)
#define CAM_REG_CTL_CD_DONE_SEL(module)         (isp_devs[module].regs + 0x003C)
#define CAM_REG_CTL_TWIN_STATUS(module)         (isp_devs[module].regs + 0x008C)
#define CAM_REG_CTL_MISC(module)                (isp_devs[module].regs + 0x0030)


/* FBC_DMAO_CTL */
#define CAM_REG_FBC_IMGO_CTL1(module)           (isp_devs[module].regs + 0x0100)
#define CAM_REG_FBC_IMGO_CTL2(module)           (isp_devs[module].regs + 0x0104)
#define CAM_REG_FBC_LTMSO_CTL1(module)          (isp_devs[module].regs + 0x01D0)
#define CAM_REG_FBC_LTMSO_CTL2(module)          (isp_devs[module].regs + 0x01D4)
#define CAM_REG_FBC_RRZO_CTL1(module)           (isp_devs[module].regs + 0x0108)
#define CAM_REG_FBC_RRZO_CTL2(module)           (isp_devs[module].regs + 0x010C)
#define CAM_REG_FBC_LCESO_CTL1(module)          (isp_devs[module].regs + 0x0120)
#define CAM_REG_FBC_LCESO_CTL2(module)          (isp_devs[module].regs + 0x0124)
#define CAM_REG_FBC_AAO_CTL1(module)            (isp_devs[module].regs + 0x0138)
#define CAM_REG_FBC_AAO_CTL2(module)            (isp_devs[module].regs + 0x013C)
#define CAM_REG_FBC_FLKO_CTL1(module)           (isp_devs[module].regs + 0x0150)
#define CAM_REG_FBC_FLKO_CTL2(module)           (isp_devs[module].regs + 0x0154)
#define CAM_REG_FBC_UFEO_CTL1(module)           (isp_devs[module].regs + 0x0110)
#define CAM_REG_FBC_UFEO_CTL2(module)           (isp_devs[module].regs + 0x0114)
#define CAM_REG_FBC_AFO_CTL1(module)            (isp_devs[module].regs + 0x0130)
#define CAM_REG_FBC_AFO_CTL2(module)            (isp_devs[module].regs + 0x0134)
#define CAM_REG_FBC_UFGO_CTL1(module)           (isp_devs[module].regs + 0x0118)
#define CAM_REG_FBC_UFGO_CTL2(module)           (isp_devs[module].regs + 0x011C)
#define CAM_REG_FBC_RSSO_CTL1(module)           (isp_devs[module].regs + 0x0160)
#define CAM_REG_FBC_RSSO_CTL2(module)           (isp_devs[module].regs + 0x0164)
#define CAM_REG_FBC_LMVO_CTL1(module)           (isp_devs[module].regs + 0x0158)
#define CAM_REG_FBC_LMVO_CTL2(module)           (isp_devs[module].regs + 0x015C)
#define CAM_REG_FBC_YUVBO_CTL1(module)          (isp_devs[module].regs + 0x0178)
#define CAM_REG_FBC_YUVBO_CTL2(module)          (isp_devs[module].regs + 0x017C)
#define CAM_REG_FBC_TSFSO_CTL1(module)          (isp_devs[module].regs + 0x0148)
#define CAM_REG_FBC_TSFSO_CTL2(module)          (isp_devs[module].regs + 0x014C)
#define CAM_REG_FBC_PDO_CTL1(module)            (isp_devs[module].regs + 0x0140)
#define CAM_REG_FBC_PDO_CTL2(module)            (isp_devs[module].regs + 0x0144)
#define CAM_REG_FBC_CRZO_CTL1(module)           (isp_devs[module].regs + 0x0188)
#define CAM_REG_FBC_CRZO_CTL2(module)           (isp_devs[module].regs + 0x018C)
#define CAM_REG_FBC_CRZBO_CTL1(module)          (isp_devs[module].regs + 0x0190)
#define CAM_REG_FBC_CRZBO_CTL2(module)          (isp_devs[module].regs + 0x0194)
#define CAM_REG_FBC_YUVCO_CTL1(module)          (isp_devs[module].regs + 0x0180)
#define CAM_REG_FBC_YUVCO_CTL2(module)          (isp_devs[module].regs + 0x0184)
#define CAM_REG_FBC_CRZO_R2_CTL1(module)        (isp_devs[module].regs + 0x0198)
#define CAM_REG_FBC_CRZO_R2_CTL2(module)        (isp_devs[module].regs + 0x019C)
#define CAM_REG_FBC_CRZBO_R2_CTL1(module)       (isp_devs[module].regs + 0x01A0)
#define CAM_REG_FBC_CRZBO_R2_CTL2(module)       (isp_devs[module].regs + 0x01A4)
#define CAM_REG_FBC_RSSO_R2_CTL1(module)        (isp_devs[module].regs + 0x01C8)
#define CAM_REG_FBC_RSSO_R2_CTL2(module)        (isp_devs[module].regs + 0x01CC)
#define CAM_REG_FBC_YUVO_CTL1(module)           (isp_devs[module].regs + 0x0170)
#define CAM_REG_FBC_YUVO_CTL2(module)           (isp_devs[module].regs + 0x0174)

#define CAM_REG_CAMCQ_CQ_EN(module)             (isp_devs[module].regs + 0x0200)
#define CAM_REG_CQ_THR0_CTL(module)             (isp_devs[module].regs + 0x0204)
#define CAM_REG_CQ_THR0_BASEADDR(module)        (isp_devs[module].regs + 0x0208)
#define CAM_REG_CQ_THR1_CTL(module)             (isp_devs[module].regs + 0x0210)
#define CAM_REG_CQ_THR1_BASEADDR(module)        (isp_devs[module].regs + 0x0214)
#define CAM_REG_CQ_THR2_CTL(module)             (isp_devs[module].regs + 0x021C)
#define CAM_REG_CQ_THR2_BASEADDR(module)        (isp_devs[module].regs + 0x0220)
#define CAM_REG_CQ_THR3_CTL(module)             (isp_devs[module].regs + 0x0228)
#define CAM_REG_CQ_THR3_BASEADDR(module)        (isp_devs[module].regs + 0x022C)
#define CAM_REG_CQ_THR4_CTL(module)             (isp_devs[module].regs + 0x0234)
#define CAM_REG_CQ_THR4_BASEADDR(module)        (isp_devs[module].regs + 0x0238)
#define CAM_REG_CQ_THR5_CTL(module)             (isp_devs[module].regs + 0x0240)
#define CAM_REG_CQ_THR5_BASEADDR(module)        (isp_devs[module].regs + 0x0244)
#define CAM_REG_CQ_THR6_CTL(module)             (isp_devs[module].regs + 0x024C)
#define CAM_REG_CQ_THR6_BASEADDR(module)        (isp_devs[module].regs + 0x0250)
#define CAM_REG_CQ_THR7_CTL(module)             (isp_devs[module].regs + 0x0258)
#define CAM_REG_CQ_THR7_BASEADDR(module)        (isp_devs[module].regs + 0x025C)
#define CAM_REG_CQ_THR8_CTL(module)             (isp_devs[module].regs + 0x0264)
#define CAM_REG_CQ_THR8_BASEADDR(module)        (isp_devs[module].regs + 0x0268)
#define CAM_REG_CQ_THR9_CTL(module)             (isp_devs[module].regs + 0x0270)
#define CAM_REG_CQ_THR9_BASEADDR(module)        (isp_devs[module].regs + 0x0274)
#define CAM_REG_CQ_THR10_CTL(module)            (isp_devs[module].regs + 0x027C)
#define CAM_REG_CQ_THR10_BASEADDR(module)       (isp_devs[module].regs + 0x0280)
#define CAM_REG_CQ_THR11_CTL(module)            (isp_devs[module].regs + 0x0288)
#define CAM_REG_CQ_THR11_BASEADDR(module)       (isp_devs[module].regs + 0x028C)
#define CAM_REG_CQ_THR12_CTL(module)            (isp_devs[module].regs + 0x0294)
#define CAM_REG_CQ_THR12_BASEADDR(module)       (isp_devs[module].regs + 0x0298)
#define CAM_REG_CQ_THR13_CTL(module)            (isp_devs[module].regs + 0x02A0)
#define CAM_REG_CQ_THR13_BASEADDR(module)       (isp_devs[module].regs + 0x02A4)
#define CAM_REG_CQ_THR14_CTL(module)            (isp_devs[module].regs + 0x02AC)
#define CAM_REG_CQ_THR14_BASEADDR(module)       (isp_devs[module].regs + 0x02B0)
#define CAM_REG_CQ_THR15_CTL(module)            (isp_devs[module].regs + 0x02B8)
#define CAM_REG_CQ_THR15_BASEADDR(module)       (isp_devs[module].regs + 0x02BC)
#define CAM_REG_CQ_THR16_CTL(module)            (isp_devs[module].regs + 0x02C4)
#define CAM_REG_CQ_THR16_BASEADDR(module)       (isp_devs[module].regs + 0x02C8)
#define CAM_REG_CQ_THR17_CTL(module)            (isp_devs[module].regs + 0x02D0)
#define CAM_REG_CQ_THR17_BASEADDR(module)       (isp_devs[module].regs + 0x02D4)
#define CAM_REG_CQ_THR18_CTL(module)            (isp_devs[module].regs + 0x02DC)
#define CAM_REG_CQ_THR18_BASEADDR(module)       (isp_devs[module].regs + 0x02E0)
#define CAM_REG_CQ_THR19_CTL(module)            (isp_devs[module].regs + 0x02E8)
#define CAM_REG_CQ_THR19_BASEADDR(module)       (isp_devs[module].regs + 0x02EC)
#define CAM_REG_CQ_THR20_CTL(module)            (isp_devs[module].regs + 0x02F4)
#define CAM_REG_CQ_THR20_BASEADDR(module)       (isp_devs[module].regs + 0x02F8)
#define CAM_REG_CQ_THR21_CTL(module)            (isp_devs[module].regs + 0x0300)
#define CAM_REG_CQ_THR21_BASEADDR(module)       (isp_devs[module].regs + 0x0304)
#define CAM_REG_CQ_THR22_CTL(module)            (isp_devs[module].regs + 0x030C)
#define CAM_REG_CQ_THR22_BASEADDR(module)       (isp_devs[module].regs + 0x0310)
#define CAM_REG_CQ_THR23_CTL(module)            (isp_devs[module].regs + 0x0318)
#define CAM_REG_CQ_THR23_BASEADDR(module)       (isp_devs[module].regs + 0x031C)
#define CAM_REG_CQ_THR24_CTL(module)            (isp_devs[module].regs + 0x0324)
#define CAM_REG_CQ_THR24_BASEADDR(module)       (isp_devs[module].regs + 0x0328)
#define CAM_REG_CQ_THR25_CTL(module)            (isp_devs[module].regs + 0x0330)
#define CAM_REG_CQ_THR25_BASEADDR(module)       (isp_devs[module].regs + 0x0334)
#define CAM_REG_CQ_THR26_CTL(module)            (isp_devs[module].regs + 0x033C)
#define CAM_REG_CQ_THR26_BASEADDR(module)       (isp_devs[module].regs + 0x0340)
#define CAM_REG_CQ_THR27_CTL(module)            (isp_devs[module].regs + 0x0348)
#define CAM_REG_CQ_THR27_BASEADDR(module)       (isp_devs[module].regs + 0x024C)
#define CAM_REG_CQ_THR28_CTL(module)            (isp_devs[module].regs + 0x0354)
#define CAM_REG_CQ_THR28_BASEADDR(module)       (isp_devs[module].regs + 0x0358)


#define CAM_REG_TG_SEN_MODE(module)             (isp_devs[module].regs + 0x3B00)
#define CAM_REG_TG_VF_CON(module)               (isp_devs[module].regs + 0x3B04)
#define CAM_REG_TG_INTER_ST(module)             (isp_devs[module].regs + 0x3B3C)
#define CAM_REG_TG_SUB_PERIOD(module)           (isp_devs[module].regs + 0x3B74)

#define CAM_REG_RRZ_IN_IMG(module)              (isp_devs[module].regs + 0x1104)
#define CAM_REG_RRZ_OUT_IMG(module)             (isp_devs[module].regs + 0x1108)

#define CAM_REG_IMGO_BASE_ADDR(module)          (isp_devs[module].regs + 0x4720)
#define CAM_REG_IMGO_XSIZE(module)              (isp_devs[module].regs + 0x472C)
#define CAM_REG_IMGO_YSIZE(module)              (isp_devs[module].regs + 0x4730)

#define CAM_REG_IMGO_DRS(module)                (isp_devs[module].regs + 0x4728)
#define CAM_REG_IMGO_CON(module)                (isp_devs[module].regs + 0x4738)
#define CAM_REG_IMGO_CON2(module)               (isp_devs[module].regs + 0x473C)
#define CAM_REG_IMGO_CON3(module)               (isp_devs[module].regs + 0x4740)

#define CAM_REG_RRZO_BASE_ADDR(module)          (isp_devs[module].regs + 0x4800)
#define CAM_REG_RRZO_XSIZE(module)              (isp_devs[module].regs + 0x480C)
#define CAM_REG_RRZO_YSIZE(module)              (isp_devs[module].regs + 0x4810)

#define CAM_REG_RRZO_DRS(module)                (isp_devs[module].regs + 0x4808)
#define CAM_REG_RRZO_CON(module)                (isp_devs[module].regs + 0x4818)
#define CAM_REG_RRZO_CON2(module)               (isp_devs[module].regs + 0x481C)
#define CAM_REG_RRZO_CON3(module)               (isp_devs[module].regs + 0x4820)

#define CAM_REG_PDO_DRS(module)                 (isp_devs[module].regs + 0x4168)
#define CAM_REG_PDO_CON(module)                 (isp_devs[module].regs + 0x4178)
#define CAM_REG_PDO_CON2(module)                (isp_devs[module].regs + 0x417C)
#define CAM_REG_PDO_CON3(module)                (isp_devs[module].regs + 0x4180)

#define CAM_REG_TSFSO_DRS(module)               (isp_devs[module].regs + 0x42C8)
#define CAM_REG_TSFSO_CON(module)               (isp_devs[module].regs + 0x42D8)
#define CAM_REG_TSFSO_CON2(module)              (isp_devs[module].regs + 0x42DC)
#define CAM_REG_TSFSO_CON3(module)              (isp_devs[module].regs + 0x42E0)

#define CAM_REG_AAO_DRS(module)                 (isp_devs[module].regs + 0x43A8)
#define CAM_REG_AAO_CON(module)                 (isp_devs[module].regs + 0x43B8)
#define CAM_REG_AAO_CON2(module)                (isp_devs[module].regs + 0x43BC)
#define CAM_REG_AAO_CON3(module)                (isp_devs[module].regs + 0x43C0)

#define CAM_REG_AFO_DRS(module)                 (isp_devs[module].regs + 0x4418)
#define CAM_REG_AFO_CON(module)                 (isp_devs[module].regs + 0x4428)
#define CAM_REG_AFO_CON2(module)                (isp_devs[module].regs + 0x442C)
#define CAM_REG_AFO_CON3(module)                (isp_devs[module].regs + 0x4430)

#define CAM_REG_FLKO_DRS(module)                (isp_devs[module].regs + 0x4488)
#define CAM_REG_FLKO_CON(module)                (isp_devs[module].regs + 0x4498)
#define CAM_REG_FLKO_CON2(module)               (isp_devs[module].regs + 0x449C)
#define CAM_REG_FLKO_CON3(module)               (isp_devs[module].regs + 0x44A0)


#define CAM_REG_LTMSO_DRS(module)               (isp_devs[module].regs + 0x44F8)
#define CAM_REG_LTMSO_CON(module)               (isp_devs[module].regs + 0x4508)
#define CAM_REG_LTMSO_CON2(module)              (isp_devs[module].regs + 0x450C)
#define CAM_REG_LTMSO_CON3(module)              (isp_devs[module].regs + 0x4510)

#define CAM_REG_LCESO_DRS(module)               (isp_devs[module].regs + 0x4568)
#define CAM_REG_LCESO_CON(module)               (isp_devs[module].regs + 0x4578)
#define CAM_REG_LCESO_CON2(module)              (isp_devs[module].regs + 0x457C)
#define CAM_REG_LCESO_CON3(module)              (isp_devs[module].regs + 0x4580)

#define CAM_REG_RSSO_DRS(module)                (isp_devs[module].regs + 0x4648)
#define CAM_REG_RSSO_CON(module)                (isp_devs[module].regs + 0x4658)
#define CAM_REG_RSSO_CON2(module)               (isp_devs[module].regs + 0x465C)
#define CAM_REG_RSSO_CON3(module)               (isp_devs[module].regs + 0x4660)

#define CAM_REG_LMVO_DRS(module)                (isp_devs[module].regs + 0x46B8)
#define CAM_REG_LMVO_CON(module)                (isp_devs[module].regs + 0x46C8)
#define CAM_REG_LMVO_CON2(module)               (isp_devs[module].regs + 0x46CC)
#define CAM_REG_LMVO_CON3(module)               (isp_devs[module].regs + 0x46D0)

#define CAM_REG_UFEO_DRS(module)                (isp_devs[module].regs + 0x4798)
#define CAM_REG_UFEO_CON(module)                (isp_devs[module].regs + 0x47A8)
#define CAM_REG_UFEO_CON2(module)               (isp_devs[module].regs + 0x47AC)
#define CAM_REG_UFEO_CON3(module)               (isp_devs[module].regs + 0x47B0)

#define CAM_REG_UFGO_DRS(module)                (isp_devs[module].regs + 0x4878)
#define CAM_REG_UFGO_CON(module)                (isp_devs[module].regs + 0x4888)
#define CAM_REG_UFGO_CON2(module)               (isp_devs[module].regs + 0x488C)
#define CAM_REG_UFGO_CON3(module)               (isp_devs[module].regs + 0x4890)

#define CAM_REG_YUVO_DRS(module)                (isp_devs[module].regs + 0x48E8)
#define CAM_REG_YUVO_CON(module)                (isp_devs[module].regs + 0x48F8)
#define CAM_REG_YUVO_CON2(module)               (isp_devs[module].regs + 0x48FC)
#define CAM_REG_YUVO_CON3(module)               (isp_devs[module].regs + 0x4900)

#define CAM_REG_YUVBO_DRS(module)               (isp_devs[module].regs + 0x4958)
#define CAM_REG_YUVBO_CON(module)               (isp_devs[module].regs + 0x4968)
#define CAM_REG_YUVBO_CON2(module)              (isp_devs[module].regs + 0x496C)
#define CAM_REG_YUVBO_CON3(module)              (isp_devs[module].regs + 0x4970)

#define CAM_REG_YUVCO_DRS(module)               (isp_devs[module].regs + 0x49C8)
#define CAM_REG_YUVCO_CON(module)               (isp_devs[module].regs + 0x49D8)
#define CAM_REG_YUVCO_CON2(module)              (isp_devs[module].regs + 0x49DC)
#define CAM_REG_YUVCO_CON3(module)              (isp_devs[module].regs + 0x49E0)

#define CAM_REG_CRZO_DRS(module)                (isp_devs[module].regs + 0x4A38)
#define CAM_REG_CRZO_CON(module)                (isp_devs[module].regs + 0x4A48)
#define CAM_REG_CRZO_CON2(module)               (isp_devs[module].regs + 0x4A4C)
#define CAM_REG_CRZO_CON3(module)               (isp_devs[module].regs + 0x4A50)

#define CAM_REG_CRZBO_DRS(module)               (isp_devs[module].regs + 0x4AA8)
#define CAM_REG_CRZBO_CON(module)               (isp_devs[module].regs + 0x4AB8)
#define CAM_REG_CRZBO_CON2(module)              (isp_devs[module].regs + 0x4ABC)
#define CAM_REG_CRZBO_CON3(module)              (isp_devs[module].regs + 0x4AC0)

#define CAM_REG_CRZO_R2_DRS(module)             (isp_devs[module].regs + 0x4B18)
#define CAM_REG_CRZO_R2_CON(module)             (isp_devs[module].regs + 0x4B28)
#define CAM_REG_CRZO_R2_CON2(module)            (isp_devs[module].regs + 0x4B2C)
#define CAM_REG_CRZO_R2_CON3(module)            (isp_devs[module].regs + 0x4B30)

#define CAM_REG_CRZBO_R2_DRS(module)            (isp_devs[module].regs + 0x4B88)
#define CAM_REG_CRZBO_R2_CON(module)            (isp_devs[module].regs + 0x4B98)
#define CAM_REG_CRZBO_R2_CON2(module)           (isp_devs[module].regs + 0x4B9C)
#define CAM_REG_CRZBO_R2_CON3(module)           (isp_devs[module].regs + 0x4BA0)

#define CAM_REG_RSSO_R2_DRS(module)             (isp_devs[module].regs + 0x4BF8)
#define CAM_REG_RSSO_R2_CON(module)             (isp_devs[module].regs + 0x4C08)
#define CAM_REG_RSSO_R2_CON2(module)            (isp_devs[module].regs + 0x4C0C)
#define CAM_REG_RSSO_R2_CON3(module)            (isp_devs[module].regs + 0x4C10)

#define CAM_REG_RAWI_R2_DRS(module)             (isp_devs[module].regs + 0x4108)
#define CAM_REG_RAWI_R2_CON(module)             (isp_devs[module].regs + 0x4118)
#define CAM_REG_RAWI_R2_CON2(module)            (isp_devs[module].regs + 0x411C)
#define CAM_REG_RAWI_R2_CON3(module)            (isp_devs[module].regs + 0x4120)
#define CAM_REG_RAWI_R2_CON4(module)            (isp_devs[module].regs + 0x4128)

#define CAM_REG_UFDI_R2_DRS(module)             (isp_devs[module].regs + 0x4268)
#define CAM_REG_UFDI_R2_CON(module)             (isp_devs[module].regs + 0x4278)
#define CAM_REG_UFDI_R2_CON2(module)            (isp_devs[module].regs + 0x427C)
#define CAM_REG_UFDI_R2_CON3(module)            (isp_devs[module].regs + 0x4280)
#define CAM_REG_UFDI_R2_CON4(module)            (isp_devs[module].regs + 0x4288)

#define CAM_REG_PDI_DRS(module)                 (isp_devs[module].regs + 0x4138)
#define CAM_REG_PDI_CON(module)                 (isp_devs[module].regs + 0x4148)
#define CAM_REG_PDI_CON2(module)                (isp_devs[module].regs + 0x414C)
#define CAM_REG_PDI_CON3(module)                (isp_devs[module].regs + 0x4150)
#define CAM_REG_PDI_CON4(module)                (isp_devs[module].regs + 0x4158)

#define CAM_REG_BPCI_ADDR(module)               (isp_devs[module].regs + 0x41D0)
#define CAM_REG_BPCI_DRS(module)                (isp_devs[module].regs + 0x41D8)
#define CAM_REG_BPCI_CON(module)                (isp_devs[module].regs + 0x41E8)
#define CAM_REG_BPCI_CON2(module)               (isp_devs[module].regs + 0x41EC)
#define CAM_REG_BPCI_CON3(module)               (isp_devs[module].regs + 0x41F0)
#define CAM_REG_BPCI_CON4(module)               (isp_devs[module].regs + 0x41F8)

#define CAM_REG_BPCI_R2_DRS(module)             (isp_devs[module].regs + 0x4208)
#define CAM_REG_BPCI_R2_CON(module)             (isp_devs[module].regs + 0x4218)
#define CAM_REG_BPCI_R2_CON2(module)            (isp_devs[module].regs + 0x421C)
#define CAM_REG_BPCI_R2_CON3(module)            (isp_devs[module].regs + 0x4220)
#define CAM_REG_BPCI_R2_CON4(module)            (isp_devs[module].regs + 0x4228)

#define CAM_REG_LSCI_DRS(module)                (isp_devs[module].regs + 0x4298)
#define CAM_REG_LSCI_CON(module)                (isp_devs[module].regs + 0x42A8)
#define CAM_REG_LSCI_CON2(module)               (isp_devs[module].regs + 0x42AC)
#define CAM_REG_LSCI_CON3(module)               (isp_devs[module].regs + 0x42B0)
#define CAM_REG_LSCI_CON4(module)               (isp_devs[module].regs + 0x42B8)

//ERR_STAT
#define CAM_REG_RAWI_R2_ERR_STAT(module)        (isp_devs[module].regs + 0x4020)
#define CAM_REG_PDI_ERR_STAT(module)            (isp_devs[module].regs + 0x4024)
#define CAM_REG_PDO_ERR_STAT(module)            (isp_devs[module].regs + 0x4028)
#define CAM_REG_BPCI_ERR_STAT(module)           (isp_devs[module].regs + 0x402C)
#define CAM_REG_BPCI_R2_ERR_STAT(module)        (isp_devs[module].regs + 0x4030)
#define CAM_REG_UFDI_R2_ERR_STAT(module)        (isp_devs[module].regs + 0x4038)
#define CAM_REG_LSCI_ERR_STAT(module)           (isp_devs[module].regs + 0x403C)

#define CAM_REG_TSFSO_ERR_STAT(module)          (isp_devs[module].regs + 0x4040)
#define CAM_REG_AAO_ERR_STAT(module)            (isp_devs[module].regs + 0x4048)
#define CAM_REG_AFO_ERR_STAT(module)            (isp_devs[module].regs + 0x404C)
#define CAM_REG_FLKO_ERR_STAT(module)           (isp_devs[module].regs + 0x4050)
#define CAM_REG_LTMSO_ERR_STAT(module)          (isp_devs[module].regs + 0x4054)
#define CAM_REG_LCESO_ERR_STAT(module)          (isp_devs[module].regs + 0x4058)
#define CAM_REG_RSSO_A_ERR_STAT(module)         (isp_devs[module].regs + 0x4060)
#define CAM_REG_LMVO_ERR_STAT(module)           (isp_devs[module].regs + 0x4064)
#define CAM_REG_IMGO_ERR_STAT(module)           (isp_devs[module].regs + 0x4068)
#define CAM_REG_UFEO_ERR_STAT(module)           (isp_devs[module].regs + 0x406C)
#define CAM_REG_RRZO_ERR_STAT(module)           (isp_devs[module].regs + 0x4070)
#define CAM_REG_UFGO_ERR_STAT(module)           (isp_devs[module].regs + 0x4074)
#define CAM_REG_YUVO_ERR_STAT(module)           (isp_devs[module].regs + 0x4078)
#define CAM_REG_YUVBO_ERR_STAT(module)          (isp_devs[module].regs + 0x407C)
#define CAM_REG_YUVCO_ERR_STAT(module)          (isp_devs[module].regs + 0x4080)
#define CAM_REG_CRZO_ERR_STAT(module)           (isp_devs[module].regs + 0x4084)
#define CAM_REG_CRZBO_ERR_STAT(module)          (isp_devs[module].regs + 0x4088)
#define CAM_REG_CRZO_R2_ERR_STAT(module)        (isp_devs[module].regs + 0x408C)
#define CAM_REG_CRZBO_R2_ERR_STAT(module)       (isp_devs[module].regs + 0x4090)
#define CAM_REG_RSSO_R2_ERR_STAT(module)        (isp_devs[module].regs + 0x4094)

/* debug */
#define CAM_REG_DBG_SET(module)                 (isp_devs[module].regs + 0x0080)
#define CAM_REG_DBG_PORT(module)                (isp_devs[module].regs + 0x0084)
#define CAM_REG_DMA_DEBUG_SEL(module)           (isp_devs[module].regs + 0x40B4)

#define CAM_REG_IMGO_FH_SPARE_2(module)         (isp_devs[module].regs + 0x4750)
#define CAM_REG_RRZO_FH_SPARE_2(module)         (isp_devs[module].regs + 0x4830)

#define CAM_REG_IMGO_FH_SPARE_12(module)        (isp_devs[module].regs + 0x4778)
#define CAM_REG_RRZO_FH_SPARE_12(module)        (isp_devs[module].regs + 0x4858)

#define CAM_REG_RRZ_HORI_STEP(module)           (isp_devs[module].regs + 0x110C)
#define CAM_REG_RRZ_VERT_STEP(module)           (isp_devs[module].regs + 0x1110)

/* CAMSV */
#define CAMSV_REG_MODULE_EN(module)             (isp_devs[module].regs + 0x0010)
#define CAMSV_REG_INT_EN(module)                (isp_devs[module].regs + 0x0018)
#define CAMSV_REG_INT_STATUS(module)            (isp_devs[module].regs + 0x001C)
#define CAMSV_REG_SW_CTL(module)                (isp_devs[module].regs + 0x0020)
#define CAMSV_REG_FBC_IMGO_CTL1(module)         (isp_devs[module].regs + 0x0A00)
#define CAMSV_REG_FBC_IMGO_CTL2(module)         (isp_devs[module].regs + 0x0A04)
#define CAMSV_REG_IMGO_BASE_ADDR(module)        (isp_devs[module].regs + 0x0420)
#define CAMSV_REG_TG_SEN_MODE(module)           (isp_devs[module].regs + 0x0130)
#define CAMSV_REG_TG_VF_CON(module)             (isp_devs[module].regs + 0x0134)
#define CAMSV_REG_TG_INTER_ST(module)           (isp_devs[module].regs + 0x016C)
#define CAMSV_REG_TG_TIME_STAMP(module)         (isp_devs[module].regs + 0x01A0)
#define CAMSV_REG_DMA_SOF_RSTSTAT(module)       (isp_devs[module].regs + 0x0400)

union FBC_CTRL_1 {
	struct { /* 0x1A010100 */
		unsigned int FBC_NUM         :  6;      /*  0.. 5, 0x0000003F */
		unsigned int rsv_6           :  2;      /*  6.. 7, 0x000000C0 */
		unsigned int FBC_RESET       :  1;      /*  8.. 8, 0x00000100 */
		unsigned int rsv_9           :  6;      /*  9..14, 0x00007E00 */
		unsigned int FBC_EN          :  1;      /* 15..15, 0x00008000 */
		unsigned int FBC_MODE        :  1;      /* 16..16, 0x00010000 */
		unsigned int LOCK_EN         :  1;      /* 17..17, 0x00020000 */
		unsigned int rsv_18          :  2;      /* 18..19, 0x000C0000 */
		unsigned int DROP_TIMING     :  1;      /* 20..20, 0x00100000 */
		unsigned int rsv_21          :  3;      /* 21..23, 0x00E00000 */
		unsigned int SUB_RATIO       :  8;      /* 24..31, 0xFF000000 */
	} Bits;
	unsigned int Raw;
};

union FBC_CTRL_2 {
	struct { /* 0x1A010104 */
		unsigned int  FBC_CNT        :  7;      /*  0.. 6, 0x0000007F */
		unsigned int  rsv_7          :  1;      /*  7.. 7, 0x00000080 */
		unsigned int  RCNT           :  6;      /*  8..13, 0x00003F00 */
		unsigned int  rsv_14         :  2;      /* 14..15, 0x0000C000 */
		unsigned int  WCNT           :  6;      /* 16..21, 0x003F0000 */
		unsigned int  rsv_22         :  2;      /* 22..23, 0x00C00000 */
		unsigned int  DROP_CNT       :  8;      /* 24..31, 0xFF000000 */
	} Bits;
	unsigned int Raw;
};

union CAMCQ_CQ_CTL_ {
	struct { /* 0x1A010204 */
		unsigned int  CAMCQ_CQ_EN         :  1;/*  0.. 0, 0x00000001 */
		unsigned int  rsv_1               :  3;/*  1.. 3, 0x0000000E */
		unsigned int  CAMCQ_CQ_MODE       :  2;/*  4.. 5, 0x00000030 */
		unsigned int  rsv_6               :  2;/*  6.. 7, 0x000000C0 */
		unsigned int  CAMCQ_CQ_DONE_SEL   :  1;/*  8.. 8, 0x00000100 */
		unsigned int  rsv_9               : 23;/*  9..31, 0xFFFFFE00 */
	} Bits;
	unsigned int Raw;
};

union CAMCTL_START_ {
	struct { /* 0x1A010054 */
		unsigned int  CQ_THR0_START     :  1; /*  0.. 0, 0x00000001 */
		unsigned int  CQ_THR1_START     :  1; /*  1.. 1, 0x00000002 */
		unsigned int  CQ_THR2_START     :  1; /*  2.. 2, 0x00000004 */
		unsigned int  CQ_THR3_START     :  1; /*  3.. 3, 0x00000008 */
		unsigned int  CQ_THR4_START     :  1; /*  4.. 4, 0x00000010 */
		unsigned int  CQ_THR5_START     :  1; /*  5.. 5, 0x00000020 */
		unsigned int  CQ_THR6_START     :  1; /*  6.. 6, 0x00000040 */
		unsigned int  CQ_THR7_START     :  1; /*  7.. 7, 0x00000080 */
		unsigned int  CQ_THR8_START     :  1; /*  8.. 8, 0x00000100 */
		unsigned int  CQ_THR9_START     :  1; /*  9.. 9, 0x00000200 */
		unsigned int  CQ_THR10_START    :  1; /* 10..10, 0x00000400 */
		unsigned int  CQ_THR11_START    :  1; /* 11..11, 0x00000800 */
		unsigned int  CQ_THR12_START    :  1; /* 12..12, 0x00001000 */
		unsigned int  CQ_THR13_START    :  1; /* 13..13, 0x00002000 */
		unsigned int  CQ_THR14_START    :  1; /* 14..14, 0x00004000 */
		unsigned int  CQ_THR15_START    :  1; /* 15..15, 0x00008000 */
		unsigned int  CQ_THR16_START    :  1; /* 16..16, 0x00010000 */
		unsigned int  CQ_THR17_START    :  1; /* 17..17, 0x00020000 */
		unsigned int  CQ_THR18_START    :  1; /* 18..18, 0x00040000 */
		unsigned int  CQ_THR19_START    :  1; /* 19..19, 0x00080000 */
		unsigned int  CQ_THR20_START    :  1; /* 20..20, 0x00100000 */
		unsigned int  CQ_THR21_START    :  1; /* 21..21, 0x00200000 */
		unsigned int  CQ_THR22_START    :  1; /* 22..22, 0x00400000 */
		unsigned int  CQ_THR23_START    :  1; /* 23..23, 0x00800000 */
		unsigned int  CQ_THR24_START    :  1; /* 24..24, 0x01000000 */
		unsigned int  CQ_THR25_START    :  1; /* 25..25, 0x02000000 */
		unsigned int  CQ_THR26_START    :  1; /* 26..26, 0x04000000 */
		unsigned int  CQ_THR27_START    :  1; /* 27..27, 0x08000000 */
		unsigned int  CQ_THR28_START    :  1; /* 28..28, 0x10000000 */
		unsigned int  rsv_29            :  3; /* 29..31, 0xE0000000 */
	} Bits;
	unsigned int Raw;
};

union CAMCTL_INT5_STATUS_ {
	struct { /* 0x1A0100F4 */
		unsigned int  CQ_THR0_DONE_ST   :  1;  /*  0.. 0, 0x00000001 */
		unsigned int  CQ_THR1_DONE_ST   :  1;  /*  1.. 1, 0x00000002 */
		unsigned int  CQ_THR2_DONE_ST   :  1;  /*  2.. 2, 0x00000004 */
		unsigned int  CQ_THR3_DONE_ST   :  1;  /*  3.. 3, 0x00000008 */
		unsigned int  CQ_THR4_DONE_ST   :  1;  /*  4.. 4, 0x00000010 */
		unsigned int  CQ_THR5_DONE_ST   :  1;  /*  5.. 5, 0x00000020 */
		unsigned int  CQ_THR6_DONE_ST   :  1;  /*  6.. 6, 0x00000040 */
		unsigned int  CQ_THR7_DONE_ST   :  1;  /*  7.. 7, 0x00000080 */
		unsigned int  CQ_THR8_DONE_ST   :  1;  /*  8.. 8, 0x00000100 */
		unsigned int  CQ_THR9_DONE_ST   :  1;  /*  9.. 9, 0x00000200 */
		unsigned int  CQ_THR10_DONE_ST  :  1;  /* 10..10, 0x00000400 */
		unsigned int  CQ_THR11_DONE_ST  :  1;  /* 11..11, 0x00000800 */
		unsigned int  CQ_THR12_DONE_ST  :  1;  /* 12..12, 0x00001000 */
		unsigned int  CQ_THR13_DONE_ST  :  1;  /* 13..13, 0x00002000 */
		unsigned int  CQ_THR14_DONE_ST  :  1;  /* 14..14, 0x00004000 */
		unsigned int  CQ_THR15_DONE_ST  :  1;  /* 15..15, 0x00008000 */
		unsigned int  CQ_THR16_DONE_ST  :  1;  /* 16..16, 0x00010000 */
		unsigned int  CQ_THR17_DONE_ST  :  1;  /* 17..17, 0x00020000 */
		unsigned int  CQ_THR18_DONE_ST  :  1;  /* 18..18, 0x00040000 */
		unsigned int  CQ_THR19_DONE_ST  :  1;  /* 19..19, 0x00080000 */
		unsigned int  CQ_THR20_DONE_ST  :  1;  /* 20..20, 0x00100000 */
		unsigned int  CQ_THR21_DONE_ST  :  1;  /* 21..21, 0x00200000 */
		unsigned int  CQ_THR22_DONE_ST  :  1;  /* 22..22, 0x00400000 */
		unsigned int  CQ_THR23_DONE_ST  :  1;  /* 23..23, 0x00800000 */
		unsigned int  CQ_THR24_DONE_ST  :  1;  /* 24..24, 0x01000000 */
		unsigned int  CQ_THR25_DONE_ST  :  1;  /* 25..25, 0x02000000 */
		unsigned int  CQ_THR26_DONE_ST  :  1;  /* 26..26, 0x04000000 */
		unsigned int  CQ_THR27_DONE_ST  :  1;  /* 27..27, 0x08000000 */
		unsigned int  CQ_THR28_DONE_ST  :  1;  /* 28..28, 0x10000000 */
		unsigned int  rsv_29            :  3;  /* 29..31, 0xE0000000 */
	} Bits;
	unsigned int Raw;
};

