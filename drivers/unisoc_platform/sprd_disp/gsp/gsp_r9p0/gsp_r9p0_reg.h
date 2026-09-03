/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R9P0_REG_H
#define _GSP_R9P0_REG_H

#include "gsp_debug.h"

#define R9P0_IMGL_NUM 2
#define R9P0_OSDL_NUM 2
#define R9P0_IMGSEC_NUM 0
#define R9P0_OSDSEC_NUM 1

/* Global config reg */
#define R9P0_GSP_BASE_OFFSET		0x0
#define R9P0_GSP_GLB_CFG(base)		(base + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_INT(base)		(base + 0x004 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_MOD_CFG(base)		(base + 0x008 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_SECURE_CFG(base)	(base + 0x00C + R9P0_GSP_BASE_OFFSET)

/*Destination reg 1*/
#define R9P0_DES_DATA_CFG(base)		(base + 0x010 + R9P0_GSP_BASE_OFFSET)
#define R9P0_DES_Y_ADDR(base)		(base + 0x014 + R9P0_GSP_BASE_OFFSET)
#define R9P0_DES_U_ADDR(base)		(base + 0x018 + R9P0_GSP_BASE_OFFSET)
#define R9P0_DES_V_ADDR(base)		(base + 0x01C + R9P0_GSP_BASE_OFFSET)
#define R9P0_DES_PITCH(base)		(base + 0x020 + R9P0_GSP_BASE_OFFSET)
#define R9P0_BACK_RGB(base)		(base + 0x024 + R9P0_GSP_BASE_OFFSET)
#define R9P0_WORK_AREA_SIZE(base)	(base + 0x028 + R9P0_GSP_BASE_OFFSET)
#define R9P0_WORK_AREA_XY(base)		(base + 0x02C + R9P0_GSP_BASE_OFFSET)

/*LAYERIMG*/
#define R9P0_LIMG_CFG(base)		(base + 0x030 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_Y_ADDR(base)		(base + 0x034 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_U_ADDR(base)		(base + 0x038 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_V_ADDR(base)		(base + 0x03C + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_PITCH(base)		(base + 0x040 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_CLIP_START(base)	(base + 0x044 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_CLIP_SIZE(base)	(base + 0x048 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_DES_START(base)	(base + 0x04C + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_PALLET_RGB(base)	(base + 0x050 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_CK(base)		(base + 0x054 + R9P0_GSP_BASE_OFFSET)
#define R9P0_Y2Y_Y_PARAM(base)		(base + 0x058 + R9P0_GSP_BASE_OFFSET)
#define R9P0_Y2Y_U_PARAM(base)		(base + 0x05C + R9P0_GSP_BASE_OFFSET)
#define R9P0_Y2Y_V_PARAM(base)		(base + 0x060 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_DES_SIZE(base)	(base + 0x064 + R9P0_GSP_BASE_OFFSET)

#define R9P0_LIMG_BASE_ADDR(base)	(base + 0x030 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LIMG_OFFSET 0x040

/*LAYEROSD*/
#define R9P0_LOSD_CFG(base)		(base + 0x0B0 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_R_ADDR(base)		(base + 0x0B4 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_PITCH(base)		(base + 0x0B8 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_CLIP_START(base)	(base + 0x0BC + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_CLIP_SIZE(base)	(base + 0x0C0 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_DES_START(base)	(base + 0x0C4 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_PALLET_RGB(base)	(base + 0x0C8 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_CK(base)		(base + 0x0CC + R9P0_GSP_BASE_OFFSET)

#define R9P0_LOSD_BASE_ADDR(base)	(base + 0x0B0 + R9P0_GSP_BASE_OFFSET)
#define R9P0_LOSD_OFFSET 0x020
#define R9P0_LOSD_SEC_ADDR(base)	(base + 0x0D0 + R9P0_GSP_BASE_OFFSET)

#define R9P0_GSP_IP_REV(base)		(base + 0x204 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG_CFG(base)	(base + 0x208 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG1(base)		(base + 0x20C + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG2(base)		(base + 0x210 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG3(base)		(base + 0x214 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG4(base)		(base + 0x218 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG5(base)		(base + 0x21C + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG6(base)		(base + 0x220 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG7(base)		(base + 0x224 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG8(base)		(base + 0x228 + R9P0_GSP_BASE_OFFSET)
#define R9P0_GSP_DEBUG9(base)		(base + 0x22c + R9P0_GSP_BASE_OFFSET)

#define R9P0_SCALE_COEF_ADDR(base)	(base + 0x300 + R9P0_GSP_BASE_OFFSET)
#define R9P0_SCALE_COEF_OFFSET		0x200

/*HDR2SDR*/
#define R9P0_HDR_OFFSET 0x100
#define R9P0_HDR0_CFG(base, index)      (base + 0x1810 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR1_CFG(base, index)      (base + 0x1814 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR2_CFG(base, index)      (base + 0x1818 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR3_CFG(base, index)      (base + 0x181c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR4_CFG(base, index)      (base + 0x1820 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR5_CFG(base, index)      (base + 0x1824 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR6_CFG(base, index)      (base + 0x1828 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR7_CFG(base, index)      (base + 0x182c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR8_CFG(base, index)      (base + 0x1830 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR9_CFG(base, index)      (base + 0x1834 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR10_CFG(base, index)     (base + 0x1838 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR11_CFG(base, index)     (base + 0x183c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR12_CFG(base, index)     (base + 0x1840 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR13_CFG(base, index)     (base + 0x1844 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR14_CFG(base, index)     (base + 0x1848 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR15_CFG(base, index)     (base + 0x184c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR16_CFG(base, index)     (base + 0x1850 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR17_CFG(base, index)     (base + 0x1854 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR18_CFG(base, index)     (base + 0x1858 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR19_CFG(base, index)     (base + 0x185c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR20_CFG(base, index)     (base + 0x1860 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR21_CFG(base, index)     (base + 0x1864 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR22_CFG(base, index)     (base + 0x1868 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR23_CFG(base, index)     (base + 0x186c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR24_CFG(base, index)     (base + 0x1870 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR25_CFG(base, index)     (base + 0x1874 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR26_CFG(base, index)     (base + 0x1878 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR27_CFG(base, index)     (base + 0x187c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR28_CFG(base, index)     (base + 0x1880 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR29_CFG(base, index)     (base + 0x1884 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR30_CFG(base, index)     (base + 0x1888 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR31_CFG(base, index)     (base + 0x188c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR32_CFG(base, index)     (base + 0x1890 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR33_CFG(base, index)     (base + 0x1894 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR34_CFG(base, index)     (base + 0x1898 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR35_CFG(base, index)     (base + 0x189c + index * R9P0_HDR_OFFSET)
#define R9P0_HDR36_CFG(base, index)     (base + 0x18a0 + index * R9P0_HDR_OFFSET)
#define R9P0_HDR37_CFG(base, index)     (base + 0x18a4 + index * R9P0_HDR_OFFSET)

struct R9P0_GSP_GLB_CFG_REG {
	union {
		struct {
			uint32_t GSP_RUN0		:  1;
			uint32_t Reserved1		:  1;
			uint32_t GSP_BUSY0		:  1;
			uint32_t Reserved2		:  1;
			uint32_t REG_BUSY		:  1;
			uint32_t Reserved3		:  3;
			uint32_t ERR_FLG		:  1;
			uint32_t ERR_CODE		:  6;
			uint32_t Reserved4		:  1;
			uint32_t GCLK_FORCE_EN		:  1;
			uint32_t ACLK_FORCE_EN		:  1;
			uint32_t AXI_GAP_WB		:  6;
			uint32_t AXI_GAP_RB		:  6;
			uint32_t AWCACHE_EN		:  1;
			uint32_t AXIM_CLR		:  1;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_INT_REG {
	union {
		struct {
			uint32_t INT_GSP_RAW		:  1;
			uint32_t INT_GERR_RAW		:  1;
			uint32_t INT_FBCDPL_RAW		:  1;
			uint32_t INT_FBCDHD_RAW		:  1;
			uint32_t INT_GSP_EN		:  1;
			uint32_t INT_GERR_EN		:  1;
			uint32_t INT_FBCDPL_EN		:  1;
			uint32_t INT_FBCDHD_EN		:  1;
			uint32_t INT_GSP_CLR		:  1;
			uint32_t INT_GERR_CLR		:  1;
			uint32_t INT_FBCDPL_CLR		:  1;
			uint32_t INT_FBCDHD_CLR		:  1;
			uint32_t Reserved1		:  4;
			uint32_t INT_GSP_STS		:  1;
			uint32_t INT_GERR_STS		:  1;
			uint32_t INT_FBCDPL0_STS	:  1;
			uint32_t INT_FBCDPL1_STS	:  1;
			uint32_t INT_FBCDPL2_STS	:  1;
			uint32_t INT_FBCDPL3_STS	:  1;
			uint32_t INT_FBCDHD0_STS	:  1;
			uint32_t INT_FBCDHD1_STS	:  1;
			uint32_t INT_FBCDHD2_STS	:  1;
			uint32_t INT_FBCDHD3_STS	:  1;
			uint32_t Reserved2		:  6;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_MOD_CFG_REG {
	union {
		struct {
			uint32_t PMARGB_EN		:  1;
			uint32_t ARLEN_MOD		:  1;
			uint32_t IFBCE_AWLEN_MOD	:  2;
			uint32_t Reserved1		:  2;
			uint32_t AWQOS			:  4;
			uint32_t ARQOSL			:  4;
			uint32_t ARQOSH			:  4;
			uint32_t BLK_TIMER		:  13;
			uint32_t CORE_NUM		:  1;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_SECURE_CFG_REG {
	union {
		struct {
			uint32_t SECURE_MOD		:  1;
			uint32_t NONSEC_AWPROT		:  3;
			uint32_t NONSEC_ARPROT		:  3;
			uint32_t SECURE_AWPROT		:  3;
			uint32_t SECURE_ARPROT		:  3;
			uint32_t Reserved1		:  19;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_DATA_CFG_REG {
	union {
		struct {
			uint32_t Y_ENDIAN_MOD		:  4;
			uint32_t UV_ENDIAN_MOD		:  4;
			uint32_t Reserved1		:  1;
			uint32_t A_SWAP_MOD		:  1;
			uint32_t ROT_MOD		:  3;
			uint32_t R2Y_MOD		:  3;
			uint32_t DES_IMG_FORMAT		:  3;
			uint32_t Reserved2		:  1;
			uint32_t RSWAP_MOD		:  3;
			uint32_t Reserved3		:  3;
			uint32_t FBCE_MOD		:  2;
			uint32_t DITHER_EN		:  1;
			uint32_t BK_EN			:  1;
			uint32_t BK_BLD			:  1;
			uint32_t Reserved4		:  1;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_Y_ADDR_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_Y_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_U_ADDR_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_U_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_V_ADDR_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1	:  32;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_PITCH_REG {
	union {
		struct {
			uint32_t DES_PITCH		:  13;
			uint32_t Reserved1		:  3;
			uint32_t DES_HEIGHT		:  13;
			uint32_t Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_Y_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_Y_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_U_ADDR1_REG {
	union {
		struct {
			uint32_t Reserved		:  4;
			uint32_t DES_U_BASE_ADDR1	:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_V_ADDR1_REG {
	union {
		struct {
			uint32_t DES_V_BASE_ADDR1	:  32;
		};
		uint32_t	value;
	};
};

struct R9P0_DES_PITCH1_REG {
	union {
		struct {
			uint32_t DES_PITCH		:  13;
			uint32_t Reserved1		:  3;
			uint32_t DES_HEIGHT		:  13;
			uint32_t Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_BACK_RGB_REG {
	union {
		struct {
			uint32_t   BACKGROUND_B		:  8;
			uint32_t   BACKGROUND_G		:  8;
			uint32_t   BACKGROUND_R		:  8;
			uint32_t   BACKGROUND_A		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_WORK_AREA_SIZE_REG {
	union {
		struct {
			uint32_t WORK_AREA_W	  	:  13;
			uint32_t Reserved1	  	:  3;
			uint32_t WORK_AREA_H	  	:  13;
			uint32_t Reserved2	  	:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_WORK_AREA_XY_REG {
	union {
		struct {
			uint32_t WORK_AREA_X	  	:  13;
			uint32_t Reserved1	  	:  3;
			uint32_t WORK_AREA_Y	  	:  13;
			uint32_t Reserved2	  	:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_IP_REV_REG {
	union {
		struct {
			uint32_t PATCH_NUM		:  4;
			uint32_t GSP_IP_REV		:  12;
			uint32_t Reserved1		:  16;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG_CFG_REG {
	union {
		struct {
			uint32_t SCL_CLR1		:  1;
			uint32_t SCL_CLR2		:  1;
			uint32_t CACHE_DIS		:  1;
			uint32_t SCLH_NONADJ		:  1;
			uint32_t SCLW_NONADJ		:  1;
			uint32_t FBCE_CG_EB		:  1;
			uint32_t Reserved1		:  26;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG1_REG {
	union {
		struct {
			uint32_t   WADR_NORM_ST		:  2;
			uint32_t   WADR_FBC_ST		:  2;
			uint32_t   SCL0_VER_ST		:  2;
			uint32_t   SCL0_HOR_ST		:  3;
			uint32_t   SCL1_VER_ST		:  2;
			uint32_t   SCL1_HOR_ST		:  3;
			uint32_t   MNG_CORE0_ST		:  3;
			uint32_t   CORE0_OGRID_ST	:  3;
			uint32_t   CORE1_OGRID_ST	:  3;
			uint32_t   Reserved1		:  9;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG2_REG {
	union {
		struct {
			uint32_t   FBCE_RBUF_RDY	:  1;
			uint32_t   FBCE_WBUF_RDY	:  1;
			uint32_t   WROT0_RBUF_RDY0	:  1;
			uint32_t   WROT0_WBUF_RDY0	:  1;
			uint32_t   WROT0_RBUF_RDY1	:  1;
			uint32_t   WROT0_WBUF_RDY1	:  1;
			uint32_t   WROT0_RBUF_RDY2	:  1;
			uint32_t   WROT0_WBUF_RDY2	:  1;
			uint32_t   WROT1_RBUF_RDY0	:  1;
			uint32_t   WROT1_WBUF_RDY0	:  1;
			uint32_t   WROT1_RBUF_RDY1	:  1;
			uint32_t   WROT1_WBUF_RDY1	:  1;
			uint32_t   WROT1_RBUF_RDY2	:  1;
			uint32_t   WROT1_WBUF_RDY2	:  1;
			uint32_t   Reserved1		:  10;
			uint32_t   SCL_OUT_EMP0		:  1;
			uint32_t   SCL_OUT_FULL0	:  1;
			uint32_t   SCL_OUT_EMP1		:  1;
			uint32_t   SCL_OUT_FULL1	:  1;
			uint32_t   BLD_OUT_EMP		:  1;
			uint32_t   BLD_OUT_FULL		:  1;
			uint32_t   Reserved2		:  2;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG3_REG {
	union {
		struct {
			uint32_t   LAYER0_IBLK_INFO	:  28;
			uint32_t   Reserved1		:  4;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG4_REG {
	union {
		struct {
			uint32_t   LAYER1_IBLK_INFO	:  28;
			uint32_t   Reserved1		:  4;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG5_REG {
	union {
		struct {
			uint32_t   LAYER2_IBLK_INFO	:  28;
			uint32_t   Reserved1		:  4;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG6_REG {
	union {
		struct {
			uint32_t   LAYER3_IBLK_INFO	:  28;
			uint32_t   Reserved1		:  4;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG7_REG {
	union {
		struct {
			uint32_t   LAYER0_DBG_STS	:  16;
			uint32_t   LAYER1_DBG_STS	:  16;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG8_REG {
	union {
		struct {
			uint32_t   LAYER2_DBG_STS	:  16;
			uint32_t   LAYER3_DBG_STS	:  16;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_DEBUG9_REG {
	union {
		struct {
			uint32_t   BLF_OBLK_INFO	:  28;
			uint32_t   Reserved1		:  4;
		};
		uint32_t	value;
	};
};

/* LAYERIMG */
struct R9P0_LAYERIMG_CFG_REG {
	union {
		struct {
			uint32_t   Y_ENDIAN_MOD		:  4;
			uint32_t   UV_ENDIAN_MOD	:  4;
			uint32_t   RGB_SWAP_MOD		:  3;
			uint32_t   A_SWAP_MOD		:  1;
			uint32_t   PMARGB_MOD		:  1;
			uint32_t   ROT_SRC		:  3;
			uint32_t   IMG_FORMAT		:  3;
			uint32_t   CK_EN		:  1;
			uint32_t   PALLET_EN		:  1;
			uint32_t   FBCD_MOD		:  2;
			uint32_t   Y2R_MOD		:  3;
			uint32_t   Y2Y_MOD		:  1;
			uint32_t   ZNUM_L		:  2;
			uint32_t   H2R_MOD		:  1;
			uint32_t   SCALE_EN		:  1;
			uint32_t   Limg_en		:  1;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_Y_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   Y_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};


struct R9P0_LAYERIMG_U_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   U_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_V_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   V_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   HEIGHT		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_SIZE_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   DES_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B		:  8;
			uint32_t   PALLET_G		:  8;
			uint32_t   PALLET_R		:  8;
			uint32_t   PALLET_A		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_CK_REG {
	union {
		struct {
			uint32_t   CK_B			:  8;
			uint32_t   CK_G			:  8;
			uint32_t   CK_R			:  8;
			uint32_t   BLOCK_ALPHA		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_Y2Y_Y_PARAM_REG {
	union {
		struct {
			uint32_t   Y_CONTRAST		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   Y_BRIGHTNESS		:  9;
			uint32_t   Reserved2		:  7;
		};
		uint32_t	value;
	};
};

struct R9P0_Y2Y_U_PARAM_REG {
	union {
		struct {
			uint32_t   U_SATURATION		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   U_OFFSET		:  8;
			uint32_t   Reserved2		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_Y2Y_V_PARAM_REG {
	union {
		struct {
			uint32_t   V_SATURATION		:  10;
			uint32_t   Reserved1		:  6;
			uint32_t   V_OFFSET		:  8;
			uint32_t   Reserved2		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYERIMG_DES_SCL_SIZE_REG {
	union {
		struct {
			uint32_t DES_SCL_W	  	:  13;
			uint32_t HTAP_MOD	  	:  2;
			uint32_t Reserved1	  	:  1;
			uint32_t DES_SCL_H	  	:  13;
			uint32_t VTAP_MOD	  	:  2;
			uint32_t Reserved2	  	:  1;
		};
		uint32_t	value;
	};
};


/* LAYEROSD */
struct R9P0_LAYEROSD_CFG_REG {
	union {
		struct {
			uint32_t   ENDIAN		:  4;
			uint32_t   RGB_SWAP		:  3;
			uint32_t   A_SWAP		:  1;
			uint32_t   PMARGB_MOD		:  1;
			uint32_t   Reserved1		:  7;
			uint32_t   IMG_FORMAT		:  2;
			uint32_t   CK_EN		:  1;
			uint32_t   PALLET_EN		:  1;
			uint32_t   FBCD_MOD		:  1;
			uint32_t   ZNUM_L		:  2;
			uint32_t   Reserved2		:  8;
			uint32_t   Losd_en		:  1;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_R_ADDR_REG {
	union {
		struct {
			uint32_t   Reserved1		:  4;
			uint32_t   R_BASE_ADDR		:  28;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_PITCH_REG {
	union {
		struct {
			uint32_t   PITCH		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   HEIGHT		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_CLIP_START_REG {
	union {
		struct {
			uint32_t   CLIP_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_CLIP_SIZE_REG {
	union {
		struct {
			uint32_t   CLIP_SIZE_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   CLIP_SIZE_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_DES_START_REG {
	union {
		struct {
			uint32_t   DES_START_X		:  13;
			uint32_t   Reserved1		:  3;
			uint32_t   DES_START_Y		:  13;
			uint32_t   Reserved2		:  3;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_PALLET_RGB_REG {
	union {
		struct {
			uint32_t   PALLET_B		:  8;
			uint32_t   PALLET_G		:  8;
			uint32_t   PALLET_R		:  8;
			uint32_t   PALLET_A		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_LAYEROSD_CK_REG {
	union {
		struct {
			uint32_t   CK_B			:  8;
			uint32_t   CK_G			:  8;
			uint32_t   CK_R			:  8;
			uint32_t   BLOCK_ALPHA		:  8;
		};
		uint32_t	value;
	};
};

struct R9P0_GSP_CTL_REG_T {
	struct R9P0_GSP_GLB_CFG_REG glb_cfg;
	struct R9P0_GSP_INT_REG int_cfg;
	struct R9P0_GSP_MOD_CFG_REG mod_cfg;
	struct R9P0_GSP_SECURE_CFG_REG secure_cfg;

	struct R9P0_DES_DATA_CFG_REG des_data_cfg;
	struct R9P0_DES_Y_ADDR_REG des_y_addr;
	struct R9P0_DES_U_ADDR_REG  des_u_addr;
	struct R9P0_DES_V_ADDR_REG  des_v_addr;
	struct R9P0_DES_PITCH_REG  des_pitch;
	struct R9P0_BACK_RGB_REG  back_rgb;
	struct R9P0_WORK_AREA_SIZE_REG  work_area_size;
	struct R9P0_WORK_AREA_XY_REG  work_area_xy;

	struct R9P0_LAYERIMG_CFG_REG  limg_cfg[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_Y_ADDR_REG  limg_y_addr[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_U_ADDR_REG  limg_u_addr[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_V_ADDR_REG  limg_v_addr[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_PITCH_REG  limg_pitch[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_CLIP_START_REG  limg_clip_start[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_CLIP_SIZE_REG  limg_clip_size[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_DES_START_REG  limg_des_start[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_PALLET_RGB_REG  limg_pallet_rgb[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_CK_REG  limg_ck[R9P0_IMGL_NUM];
	struct R9P0_Y2Y_Y_PARAM_REG  y2y_y_param[R9P0_IMGL_NUM];
	struct R9P0_Y2Y_U_PARAM_REG  y2y_u_param[R9P0_IMGL_NUM];
	struct R9P0_Y2Y_V_PARAM_REG  y2y_v_param[R9P0_IMGL_NUM];
	struct R9P0_LAYERIMG_DES_SCL_SIZE_REG  limg_des_scl_size[R9P0_IMGL_NUM];

	struct R9P0_LAYEROSD_CFG_REG  losd_cfg[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_R_ADDR_REG  losd_r_addr[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_PITCH_REG  losd_pitch[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_CLIP_START_REG  losd_clip_start[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_CLIP_SIZE_REG  losd_clip_size[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_DES_START_REG  losd_des_start[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_PALLET_RGB_REG  losd_pallet_rgb[R9P0_OSDL_NUM];
	struct R9P0_LAYEROSD_CK_REG  losd_ck[R9P0_OSDL_NUM];

	struct R9P0_LAYEROSD_CFG_REG  osdsec_cfg[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_R_ADDR_REG  osdsec_r_addr[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_PITCH_REG  osdsec_pitch[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_CLIP_START_REG  osdsec_clip_start[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_CLIP_SIZE_REG  osdsec_clip_size[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_DES_START_REG  osdsec_des_start[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_PALLET_RGB_REG  osdsec_pallet_rgb[R9P0_OSDSEC_NUM];
	struct R9P0_LAYEROSD_CK_REG  osdsec_ck[R9P0_OSDSEC_NUM];

	struct R9P0_GSP_IP_REV_REG ip_rev;
	struct R9P0_GSP_DEBUG_CFG_REG debug_cfg;
	struct R9P0_GSP_DEBUG1_REG debug1_cfg;
	struct R9P0_GSP_DEBUG2_REG debug2_cfg;
	struct R9P0_GSP_DEBUG3_REG debug3_cfg;
	struct R9P0_GSP_DEBUG4_REG debug4_cfg;
	struct R9P0_GSP_DEBUG5_REG debug5_cfg;
	struct R9P0_GSP_DEBUG6_REG debug6_cfg;
	struct R9P0_GSP_DEBUG7_REG debug7_cfg;
	struct R9P0_GSP_DEBUG8_REG debug8_cfg;
	struct R9P0_GSP_DEBUG9_REG debug9_cfg;
};

#endif
