/*
 * spid.h: Intel software platform ID definitions
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _ASM_X86_SPID_H
#define _ASM_X86_SPID_H

/* In MCG, this comes from include/asm/sfi.h.  But ours is a non-SFI platform */
#define SPID_FRU_SIZE	10

#include <linux/sfi.h>
/* size of SPID cmdline : androidboot.spid=vend:cust:manu:plat:prod:hard */
#define SPID_CMDLINE_SIZE 46
#define SPID_PARAM_NAME "androidboot.spid="
#define SPID_DEFAULT_VALUE "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx"

#define spid_attr(_name) \
static struct kobj_attribute _name##_attr = { \
	.attr = {                             \
		.name = __stringify(_name),   \
		.mode = 0444,                 \
	},                                    \
	.show   = _name##_show,               \
}


#define INTEL_PLATFORM_SSN_SIZE	32
extern struct soft_platform_id spid;
extern char intel_platform_ssn[INTEL_PLATFORM_SSN_SIZE + 1];

int __init sfi_handle_spid(struct sfi_table_header *table);

struct soft_platform_id {
	u16 customer_id; /*Defines the final customer for the product */
	u16 vendor_id; /* Defines who owns the final product delivery */
	u16 manufacturer_id; /* Defines who build the hardware. This can be
			      * different for the same product */
	u16 platform_family_id; /* Defined by vendor and defines the family of
				 * the product with the same root components */
	u16 product_line_id; /* Defined by vendor and defines the name of the
			      * product. This can be used to differentiate the
			      * feature set for the same product family (low
			      * cost vs full feature). */
	u16 hardware_id; /* Defined by vendor and defines the physical hardware
			  * component set present on the PCB/FAB */
	u8  fru[SPID_FRU_SIZE]; /* Field Replaceabl Unit */
} __packed;

/* Customer_ID table */
enum {
	CUSTOMER_INTEL,
	CUSTOMER_INTEL_RSVD1,
	CUSTOMER_INTEL_RSVD2,
	CUSTOMER_INTEL_RSVD3,
	CUSTOMER_INTEL_RSVD4,
	CUSTOMER_INTEL_RSVD5,
	CUSTOMER_INTEL_RSVD6,
	CUSTOMER_RSVD,
	CUSTOMER_UNKNOWN = 0xFFFF
};

/* Vendor_ID table */
enum {
	VENDOR_INTEL,
	VENDOR_INTEL_RSVD1,
	VENDOR_INTEL_RSVD2,
	VENDOR_INTEL_RSVD3,
	VENDOR_INTEL_RSVD4,
	VENDOR_INTEL_RSVD5,
	VENDOR_INTEL_RSVD6,
	VENDOR_RSVD,
	VENDOR_UNKNOWN = 0xFFFF
};

/* Manufacturer_ID table for Vendor_ID == VENDOR_INTEL */
enum {
	MANUFACTURER_FAB1,
	MANUFACTURER_FAB2,
	MANUFACTURER_FAB3,
	MANUFACTURER_FAB4,
	MANUFACTURER_FAB5,
	MANUFACTURER_FAB6,
	MANUFACTURER_FAB7,
	MANUFACTURER_FAB8,
	MANUFACTURER_FAB9,
	MANUFACTURER_RSVD,
	MANUFACTURER_UNKNOWN = 0xFFFF
};

/* Platform_Family_ID table for Vendor_ID == VENDOR_INTEL */
enum {
	INTEL_MFLD_PHONE  = 0x0000,
	INTEL_MFLD_TABLET = 0x0001,
	INTEL_CLVTP_PHONE = 0x0002,
	INTEL_CLVT_TABLET = 0x0003,
	INTEL_MRFL_PHONE  = 0x0004,
	INTEL_MRFL_TABLET = 0x0005,
	INTEL_BYT_PHONE   = 0x0006,
	INTEL_BYT_TABLET  = 0x0007,
	INTEL_MOFD_PHONE  = 0x0008,
	INTEL_MOFD_TABLET = 0x0009,
	INTEL_CHT_PHONE   = 0x000A,
	INTEL_CHT_TABLET  = 0x000B,
	INTEL_PLATFORM_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MFLD_PHONE */
enum {
	INTEL_MFLD_PHONE_BB15_PRO = 0x0000,
	INTEL_MFLD_PHONE_BB15_ENG = 0x8000,
	INTEL_MFLD_PHONE_BB20_PRO = 0x0001,
	INTEL_MFLD_PHONE_BB20_ENG = 0x8001,
	INTEL_MFLD_PHONE_OR_PRO   = 0x0002,
	INTEL_MFLD_PHONE_OR_ENG   = 0x8002,
	INTEL_MFLD_PHONE_AT_PRO   = 0x0003,
	INTEL_MFLD_PHONE_AT_ENG   = 0x8003,
	INTEL_MFLD_PHONE_LEX_PRO  = 0x0004,
	INTEL_MFLD_PHONE_LEX_ENG  = 0x8004,
	INTEL_MFLD_PHONE_UNKNOWN  = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MFLD_TABLET */
enum {
	INTEL_MFLD_TABLET_RR_PRO  = 0x0000,
	INTEL_MFLD_TABLET_RR_ENG  = 0x8000,
	INTEL_MFLD_TABLET_FM_PRO  = 0x0001,
	INTEL_MFLD_TABLET_FM_ENG  = 0x8001,
	INTEL_MFLD_TABLET_FVA_PRO = 0x0002,
	INTEL_MFLD_TABLET_FVA_ENG = 0x8002,
	INTEL_MFLD_TABLET_SLP_PRO = 0x0003,
	INTEL_MFLD_TABLET_SLP_ENG = 0x8003,
	INTEL_MFLD_TABLET_YKB_PRO = 0x0004,
	INTEL_MFLD_TABLET_YKB_ENG = 0x8004,
	INTEL_MFLD_TABLET_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_CLVTP_PHONE */
enum {
	INTEL_CLVTP_PHONE_RHB_PRO = 0x0000,
	INTEL_CLVTP_PHONE_RHB_ENG = 0x8000,
	INTEL_CLVTP_PHONE_VB_PRO  = 0x0001,
	INTEL_CLVTP_PHONE_VB_ENG  = 0x8001,
	INTEL_CLVTP_PHONE_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_CLVT_TABLET */
enum {
	INTEL_CLVT_TABLET_TBD_PRO = 0x0000,
	INTEL_CLVT_TABLET_TBD_ENG = 0x8000,
	INTEL_CLVT_TABLET_SLP_PRO = 0x0001,
	INTEL_CLVT_TABLET_SLP_ENG = 0x8001,
	INTEL_CLVT_TABLET_HLS_PRO = 0x0002,
	INTEL_CLVT_TABLET_HLS_ENG = 0x8002,
	INTEL_CLVT_TABLET_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MRFL_PHONE */
enum {
	INTEL_MRFL_PHONE_SB_PRO = 0x0000,
	INTEL_MRFL_PHONE_SB_ENG = 0x8000,
	INTEL_MRFL_PHONE_BB_PRO = 0x0001,
	INTEL_MRFL_PHONE_BB_ENG = 0x8001,
	INTEL_MRFL_PHONE_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MRFL_TABLET */
enum {
	INTEL_MRFL_TABLET_TBD_PRO = 0x0000,
	INTEL_MRFL_TABLET_TBD_ENG = 0x8000,
	INTEL_MRFL_TABLET_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_BYT_PHONE */
enum {
	INTEL_BYT_PHONE_TBD_PRO = 0x0000,
	INTEL_BYT_PHONE_TBD_ENG = 0x8000,
	INTEL_BYT_PHONE_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_BYT_TABLET */
enum {
	INTEL_BYT_TABLET_BLK_PRO = 0x0000,
	INTEL_BYT_TABLET_BLK_ENG = 0x8000,
	INTEL_BYT_TABLET_BLB_PRO = 0x0001,
	INTEL_BYT_TABLET_BLB_ENG = 0x8001,
	INTEL_BYT_TABLET_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MOFD_PHONE */
enum {
	INTEL_MOFD_PHONE_MP_PRO = 0x0000,
	INTEL_MOFD_PHONE_MP_ENG = 0x8000,
	INTEL_MOFD_PHONE_V0_PRO = 0x0001,
	INTEL_MOFD_PHONE_V0_ENG = 0x8001,
	INTEL_MOFD_PHONE_V1_PRO = 0x0002,
	INTEL_MOFD_PHONE_V1_ENG = 0x8002,
	INTEL_MOFD_PHONE_V2_PRO = 0x0003,
	INTEL_MOFD_PHONE_V2_ENG = 0x8003,
	INTEL_MOFD_PHONE_GARNET_PRO = 0x0004,
	INTEL_MOFD_PHONE_GARNET_ENG = 0x8004,
	INTEL_MOFD_PHONE_LNP_PRO = 0x0005,
	INTEL_MOFD_PHONE_LNP_ENG = 0x8005,
	INTEL_MOFD_PHONE_UNKNOWN = 0xFFFF
};

/* Product_Line_ID table for Platform_Family_ID == INTEL_MOFD_TABLET */
enum {
	INTEL_MOFD_TABLET_TBD_PRO = 0x0000,
	INTEL_MOFD_TABLET_TBD_ENG = 0x8000,
	INTEL_MOFD_TABLET_UNKNOWN = 0xFFFF
};


/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_PHONE_BB15 */
enum {
	MFLD_PHONE_BB15_PR20, /* CRAK C0 */
	MFLD_PHONE_BB15_PR31, /* CRAK D0 */
	MFLD_PHONE_BB15_PR32, /* CRAK D0 */
	MFLD_PHONE_BB15_PR33, /* CRAK D1 - 1.6GHz */
	MFLD_PHONE_BB15_PR34, /* CRAK D1 - 1.6GHz, alt eMMC, DDR2 */
	MFLD_PHONE_BB15_PR35, /* CRAK D1 - 1.6GHz, alt eMMC, DDR2 */
	MFLD_PHONE_BB15_PR36, /* CRAK D1 - 1.6GHz, alt eMMC, DDR2, MSIC C2 */
	MFLD_PHONE_BB15_PR40, /* CRAK D1 - 2.0GHz */
	MFLD_PHONE_BB15_PR2A,
	MFLD_PHONE_BB15_PR3A,
	MFLD_PHONE_BB15_PR3B,
	MFLD_PHONE_BB15_4MVV,
	MFLD_PHONE_BB15_4MSV,
	MFLD_PHONE_BB15_ICDK,
	MFLD_PHONE_BB15_4MVV3,
	MFLD_PHONE_BB15_RSVD,
	MFLD_PHONE_BB15_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_PHONE_BB20 */
enum {
	MFLD_PHONE_BB20_TBD,
	MFLD_PHONE_BB20_RSVD,
	MFLD_PHONE_BB20_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_PHONE_OR */
enum {
	MFLD_PHONE_OR_NHDV1,    /* CRAK D0 - 1.6G */
	MFLD_PHONE_OR_NHDV2,    /* CRAK D1 - 1.6G */
	MFLD_PHONE_OR_NHDV3,    /* CRAK D1 - 1.6G */
	MFLD_PHONE_OR_NHDV31R,  /* CRAK D1 - 1.6G */
	MFLD_PHONE_OR_NHDV31A,  /* CAAK D1 - 1.6G */
	MFLD_PHONE_OR_NHDV30F,  /* CRAK D1 - 2.0G */
	MFLD_PHONE_OR_NHDV31A1, /* CAAK D1 - 2.0G */
	MFLD_PHONE_OR_NHDV30D,  /* CRAK D2 - 1.6G */
	MFLD_PHONE_OR_NHDV30G,  /* CRAK D2 - 2.0G */
	MFLD_PHONE_OR_NHDV31A2, /* CAAK D2 - 1.6G */
	MFLD_PHONE_OR_NHDV31A3, /* CAAK D2 - 2.0G */
	MFLD_PHONE_OR_NHDV30E,  /* CRAK D1, Samsung eMMC for part quals */
	MFLD_PHONE_OR_RSVD,
	MFLD_PHONE_OR_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_PHONE_AT */
enum {
	MFLD_PHONE_AT_LA, /* CAAK D1 */
	MFLD_PHONE_AT_LA_RSVD,
	MFLD_PHONE_AT_LA_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_PHONE_LEX */
enum {
	MFLD_PHONE_LEX_PR11, /* RYS/PNW 1GHz CREK D1 */
	MFLD_PHONE_LEX_PR1M, /* RYS/PNW 1GHz CREK D1 */
	MFLD_PHONE_LEX_PR21, /* RYS/PNW 1GHz CSEK D1 */
	MFLD_PHONE_LEX_PR2M, /* BND/PNW 1GHz CSEK D1 */
	MFLD_PHONE_LEX_DV1,  /* BND/PNW 1.2GHz CSEK D1 */
	MFLD_PHONE_LEX_RSVD,
	MFLD_PHONE_LEX_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_TABLET_RR */
enum {
	MFLD_TABLET_RR_DV10, /* CRAK D0 */
	MFLD_TABLET_RR_DV15, /* CRAK D0/D1 */
	MFLD_TABLET_RR_DV20, /* CRAK D1 */
	MFLD_TABLET_RR_DV21, /* CRAK D1 */
	MFLD_TABLET_RR_RSVD,
	MFLD_TABLET_RR_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_TABLET_FM */
enum {
	MFLD_TABLET_FM_EV20, /* CRAK D0 */
	MFLD_TABLET_FM_DV10, /* CRAK D1 */
	MFLD_TABLET_FM_RSVD,
	MFLD_TABLET_FM_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_TABLET_FVA */
enum {
	MFLD_TABLET_FVA_EV10P, /* CRAK Dx */
	MFLD_TABLET_FVA_EV10,  /* CRAK Dx */
	MFLD_TABLET_FVA_EV20,  /* CRAK Dx */
	MFLD_TABLET_FVA_DV10,  /* CRAK Dx */
	MFLD_TABLET_FVA_RSVD,
	MFLD_TABLET_FVA_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_TABLET_SLP */
enum {
	MFLD_TABLET_SLP_EV05,  /* CRAK Dx */
	MFLD_TABLET_SLP_EV10,  /* CRAK Dx */
	MFLD_TABLET_SLP_EV20,  /* CRAK Dx */
	MFLD_TABLET_SLP_DV10,  /* CRAK Dx */
	MFLD_TABLET_SLP_EVL10, /* CRAK Dx */
	MFLD_TABLET_SLP_EVL20, /* CRAK Dx */
	MFLD_TABLET_SLP_DVL10, /* CRAK Dx */
	MFLD_TABLET_SLP_RSVD,
	MFLD_TABLET_SLP_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MFLD_TABLET_YKB */
enum {
	MFLD_TABLET_YKB_DV10, /* CRAK Dx */
	MFLD_TABLET_YKB_RSVD,
	MFLD_TABLET_YKB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CLVTP_PHONE_RHB */
enum {
	CLVTP_PHONE_RHB_CCVV0,  /* Clover City VV0 FAB A CLV/CLV+ A0*/
	CLVTP_PHONE_RHB_CCVV1,  /* Clover City VV1 FAB B CLV+ A0*/
	CLVTP_PHONE_RHB_CCVV2,  /* Clover City VV2 FAB C CLV+ A0*/
	CLVTP_PHONE_RHB_CLEV,   /* Clover Lake CRB EV */
	CLVTP_PHONE_RHB_PR01,   /* RHB PR0.1 CLV A0 C-CLASS */
	CLVTP_PHONE_RHB_PR02,   /* RHB PR0.2 CLV A0 C-CLASS */
	CLVTP_PHONE_RHB_PR10PM, /* CLV+ A0 */
	CLVTP_PHONE_RHB_CCVV1P, /* Clover City Pre-VV1 Fab B CLV+ A0 */
	CLVTP_PHONE_RHB_PR10P,  /* RHB Pre-PR1.0 CLV A0 C- CLASS */
	CLVTP_PHONE_RHB_PR10M,  /* RHB Macro PR1.0 CLV+ A0 */
	CLVTP_PHONE_RHB_PR10,   /* RHB PR1.0 CLV+ A0 C-CLASS */
	CLVTP_PHONE_RHB_PR15M,  /* RHB Macro PR1.5 CLV+ A0 */
	CLVTP_PHONE_RHB_PR15,   /* RHB PR1.5 CLV+ A0 C-CLASS */
	CLVTP_PHONE_RHB_PR20M,  /* RHB Macro PR2.0 CLV+ B0 */
	CLVTP_PHONE_RHB_PR20,   /* RHB PR2.0 CLV+ B0 C-CLASS */
	CLVTP_PHONE_RHB_PR30M,  /* RHB Macro PR3.0 CLV+ B0 */
	CLVTP_PHONE_RHB_CCVV3,  /* Clover City VV3 FAB D CLV+ A0 */
	CLVTP_PHONE_RHB_PR30,   /* RHB PR3.0 CLV+ B0 C-CLASS */
	CLVTP_PHONE_RHB_DV1,    /* RHB Dv1 */
	CLVTP_PHONE_RHB_PR20A,  /* CLV+ B0 C-Class-touch panel sensor GFF */
	CLVTP_PHONE_RHB_CCVV2VB, /* Clover City VV2-Victoria Bay FAB B CLV B0 */
	CLVTP_PHONE_RHB_PR19M,  /* Macro PR1.9 CLV+ B0 */
	CLVTP_PHONE_RHB_PR199M, /* Macro PR1.99 CLV+ B0 */
	CLVTP_PHONE_RSVD0,
	CLVTP_PHONE_RHB_PR20B,  /* CLV+ B0 C-Class-touch panel sensor
					GFF-LPDDR2 */
	CLVTP_PHONE_RHB_PR30A,  /* CLV+ B1 C-Class */
	CLVTP_PHONE_RHB_PR30AM, /* Macro CLV+ B1 C-Class */
	CLVTP_PHONE_RHB_PR31,   /* CLV+ B2 C-Class */
	CLVTP_PHONE_RHB_PR31M,  /* Macro CLV+ B2 C-Class */
	CLVTP_PHONE_RHB_CCVV3A, /* Clover City VV3 FAB B CLV+ B0 */
	CLVTP_PHONE_RHB_CCVV3B, /* Clover City VV3 FAB B CLV+ B1 */
	CLVTP_PHONE_RHB_CCVV3C, /* Clover City VV3 FAB B CLV+ B2 */
	CLVTP_PHONE_RSVD1,
	CLVTP_PHONE_RSVD2,
	CLVTP_PHONE_RSVD3,
	CLVTP_PHONE_RSVD4,
	CLVTP_PHONE_RHB_RDP,    /* Clovertrail RDP CLV+ B2 */
	CLVTP_PHONE_RHB_VVLITE = 0x29, /* VV-Lite CLV+ B2 */
	CLVTP_PHONE_RHB_RSVD,
	CLVTP_PHONE_RHB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CLVTP_PHONE_VB */
enum {
	CLVTP_PHONE_VB_PR1A = 0x17,    /* Victoria Bay PR1 CLV+ B1 */
	CLVTP_PHONE_VB_PR1B = 0x23,    /* Victoria Bay PR1 CLV+ B2 */
	CLVTP_PHONE_VB_PR20 = 0x25,    /* Victoria Bay PR2 CLV+ B2 */
	CLVTP_PHONE_VB_PR21,           /* Victoria Bay PR2.1 CLV+ B2 */
	CLVTP_PHONE_VB_PR30,           /* Victoria Bay PR3 B2 */
	CLVTP_PHONE_VB_PR21NA,         /* Victoria Bay PR2.1 NA CLV+ B2 */
	CLVTP_PHONE_VB_RSVD,
	CLVTP_PHONE_VB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CLVT_TABLET_TBD */
enum {
	CLVT_TABLET_TBD_CLEVA, /* Clover Lake EV - CRB - FAB A */
	CLVT_TABLET_TBD_CLEVB, /* Clover Lake EV - CRB - FAB B */
	CLVT_TABLET_TBD_CLEVC, /* Clover Lake EV - CRB - FAB C */
	CLVT_TABLET_TBD_CLEVD, /* Clover Lake EV - CRB - FAB D */
	CLVT_TABLET_TBD_VV2SS, /* Clover City VV2-Samtab-FAB B CLV+ B0 */
	CLVT_TABLET_TBD_RSVD,
	CLVT_TABLET_TBD_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CLVT_TABLET_SLP */
enum {
	CLVT_TABLET_SLP_EV10, /* CRAK Bx */
	CLVT_TABLET_SLP_RSVD,
	CLVT_TABLET_SLP_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CLVT_TABLET_HLS */
enum {
	CLVT_TABLET_HLS_FLG, /*Flag Hill EVT */
	CLVT_TABLET_HLS_GRAND, /* Grand Hill EVT */
	CLVT_TABLET_HLS_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MRFL_PHONE_SB */
enum {
	MRFL_PHONE_SR_VVA,  /* SilverRidge VV FAB A */
	MRFL_PHONE_SR_VVB,  /* SilverRidge VV FAB B */
	MRFL_PHONE_SR_VVC,  /* SilverRidge VV FAB C */
	MRFL_PHONE_SR_VVD,  /* SilverRidge VV FAB D */
	MRFL_PHONE_SR_SVA,  /* SilverRidge SV FAB A */
	MRFL_PHONE_SR_SVB,  /* SilverRidge SV FAB B */
	MRFL_PHONE_SR_SVC,  /* SilverRidge SV FAB C */
	MRFL_PHONE_SR_SVD,  /* SilverRidge SV FAB D */
	MRFL_PHONE_SB_PR0M, /* Salt Bay PR0-Macro (A0) */
	MRFL_PHONE_SB_PR0,  /* Salt Bay PR0-FF (A0) */
	MRFL_PHONE_SB_PR1M, /* Salt Bay PR1-Macro (A0) */
	MRFL_PHONE_SB_PR1,  /* Salt Bay PR1-FF (A0) */
	MRFL_PHONE_SB_PR2M, /* Salt Bay PR2-Macro (B0) */
	MRFL_PHONE_SB_PR2,  /* Salt Bay PR2-FF (B0) */
	MRFL_PHONE_SB_PR3M, /* Salt Bay PR3-Macro (B0) */
	MRFL_PHONE_SB_PR3,  /* Salt Bay PR3-FF (B0) */
	MRFL_PHONE_SB_PR11,  /* Salt Bay PR1.1-FF (A0) */
	MRFL_PHONE_SB_PR11M,  /* Salt Bay PR1.1-Macro (A0) */
	MRFL_PHONE_SB_PR2_A1M,  /* Salt Bay PR2-Macro (A1) */
	MRFL_PHONE_SB_PR2_A1,  /* Salt Bay PR2-FF (A1) */
	MRFL_PHONE_SB_PR2_7260,  /* Salt Bay PR2-FF + 7260 (B0) */
	MRFL_PHONE_SB_PR25, /* Salt Bay PR2-FF (B1) */
	MRFL_PHONE_SB_PR2_B1, /* Salt Bay PR2-FF (B1) */
	MRFL_PHONE_SB_PR27, /* Salt Bay PR2-FF + 7260 (ES2) */
	MRFL_PHONE_SB_PR27_2_01, /* Salt Bay PR2-FF + 7260 (ES2.01) */
	MRFL_PHONE_SB_RSVD,
	MRFL_PHONE_SB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MRFL_PHONE_BB */
enum {
	MRFL_PHONE_BB_PRHA, /* Bodega Bay PRh FAB A */
	MRFL_PHONE_BB_PRHB, /* Bodega Bay PRh FAB B */
	MRFL_PHONE_BB_RSVD,
	MRFL_PHONE_BB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MRFL_TABLET_TBD */
enum {
	MRFL_TABLET_TBD_TBD,
	MRFL_TABLET_TBD_RSVD,
	MRFL_TABLET_TBD_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_BYT_PHONE_TBD */
enum {
	BYT_PHONE_TBD_TBD,
	BYT_PHONE_TBD_RSVD,
	BYT_PHONE_TBD_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_BYT_TABLET_BLK */
enum {
	BYT_TABLET_BLK_RVP1, /* Bay Lake CRB/RVP FAB 1 */
	BYT_TABLET_BLK_RVP2, /* Bay Lake CRB/RVP FAB 2 */
	BYT_TABLET_BLK_RVP3, /* Bay Lake CRB/RVP FAB 3 */
	BYT_TABLET_BLK_10PR11, /* Bay Lake FFRD-10 PR1.1 */
	BYT_TABLET_BLK_8PR0, /* Bay Lake FFRD-8 PR0*/
	BYT_TABLET_BLK_8PR1, /* Bay Lake FFRD-8 PR1 */
	BYT_TABLET_BLK_8RVP1, /* Bay Lake RVP-8 */
	BYT_TABLET_BLK_CRV2, /*BYT T CR V2*/
	BYT_TABLET_BLK_RSVD,
	BYT_TABLET_BLK_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_BYT_TABLET_BLB */
enum {
	BYT_TABLET_BLB_CRBV3, /* Bayley Bay CRB Fab3 Rev3 */
	BYT_TABLET_BLB_RSVD,
	BYT_TABLET_BLB_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_MP */
enum {
	MOFD_PHONE_MP_VVA, /* Mountain Prairie VV FAB A */
	MOFD_PHONE_MP_VVB, /* Mountain Prairie VV FAB B */
	MOFD_PHONE_MP_VVC, /* Mountain Prairie VV FAB C */
	MOFD_PHONE_MP_VVD, /* Mountain Prairie VV FAB D */
	MOFD_PHONE_MP_RSVD,
	MOFD_PHONE_MP_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_V0 */
enum {
	MOFD_PHONE_V0_PR0, /* FAB A */
	MOFD_PHONE_V0_PR1, /* FAB B */
	MOFD_PHONE_V0_PR2, /* FAB C */
	MOFD_PHONE_V0_PR3, /* FAB D */
	MOFD_PHONE_V0_RSVD,
	MOFD_PHONE_V0_UNKNOWN = 0xFFFF
};
/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_V1 */
enum {
	MOFD_PHONE_V1_PR0, /* FAB A */
	MOFD_PHONE_V1_PR1, /* FAB B */
	MOFD_PHONE_V1_PR2, /* FAB C */
	MOFD_PHONE_V1_PR3, /* FAB D */
	MOFD_PHONE_V1_RSVD,
	MOFD_PHONE_V1_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_V2 */
enum {
	MOFD_PHONE_V2_PR0, /* FAB A */
	MOFD_PHONE_V2_PR1, /* FAB B */
	MOFD_PHONE_V2_PR2, /* FAB C */
	MOFD_PHONE_V2_PR3, /* FAB D */
	MOFD_PHONE_V2_RSVD,
	MOFD_PHONE_V2_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_GARNET */
enum {
	MOFD_PHONE_GARNET_PR0, /* FAB A */
	MOFD_PHONE_GARNET_PR1, /* FAB B */
	MOFD_PHONE_GARNET_PR2, /* FAB C */
	MOFD_PHONE_GARNET_PR3, /* FAB D */
	MOFD_PHONE_GARNET_RSVD,
	MOFD_PHONE_GARNET_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_PHONE_LNP */
enum {
	MOFD_PHONE_LNP_PR0, /* FAB A */
	MOFD_PHONE_LNP_PR1, /* FAB B */
	MOFD_PHONE_LNP_PR2, /* FAB C */
	MOFD_PHONE_LNP_PR3, /* FAB D */
	MOFD_PHONE_LNP_RSVD,
	MOFD_PHONE_LNP_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_MOFD_TABLET_TBD */
enum {
	MOFD_TABLET_BB_FAB_A,
	MOFD_TABLET_TBD_RSVD,
	MOFD_TABLET_TBD_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CHT_PHONE_TBD */
enum {
	CHT_PHONE_TBD_TBD,
	CHT_PHONE_TBD_RSVD,
	CHT_PHONE_TBD_UNKNOWN = 0xFFFF
};

/* Hardware_ID table for Product_Line_ID == INTEL_CHT_TABLET */
enum {
	CHT_TABLET_RVP1, /* Cherry Trail RVP Fab 1 */
	CHT_TABLET_RVP2, /* Cherry Trail RVP Fab 2 */
	CHT_TABLET_RVP3, /* Cherry Trail RVP Fab 3 */
	CHT_TABLET_FRD_PR0, /* Cherry Trail FFRD PR0 */
	CHT_TABLET_FRD_PR1, /* Cherry Trail FFRD PR1 */
	CHT_TABLET_FRD_PR2, /* Cherry Trail FFRD PR2 */
	CHT_TABLET_UNKNOWN = 0xFFFF
};

/* Macros for SPID based checks */

#define SPID_CUSTOMER_ID(customer) ( \
	(spid.customer_id == CUSTOMER_##customer))
#define SPID_VENDOR_ID(vendor) ( \
	(spid.vendor_id == VENDOR_##vendor))
#define SPID_PLATFORM_ID(vendor, platform, devtype) ( \
	(spid.platform_family_id == vendor##_##platform##_##devtype))
#define SPID_PRODUCT_ID(vendor, platform, devtype, product, type) (\
	(spid.product_line_id == \
	vendor##_##platform##_##devtype##_##product##_##type))
#define SPID_PRODUCT(vendor, platform, devtype, product) (\
	((spid.product_line_id & 0x7FFF) == \
	vendor##_##platform##_##devtype##_##product##_PRO))
#define SPID_HARDWARE_ID(platform, devtype, product, hardware) (\
	(spid.hardware_id == platform##_##devtype##_##product##_##hardware))

#define INTEL_MID_BOARDV1(devtype, platform) ( \
	SPID_CUSTOMER_ID(INTEL) && \
	SPID_VENDOR_ID(INTEL) && \
	SPID_PLATFORM_ID(INTEL, platform, devtype))

#define INTEL_MID_BOARDV2(devtype, platform, product, type) ( \
	INTEL_MID_BOARDV1(devtype, platform) && \
	SPID_PRODUCT_ID(INTEL, platform, devtype, product, type))

#define INTEL_MID_BOARDV3(devtype, platform, product, type, hardware) ( \
	INTEL_MID_BOARDV2(devtype, platform, product, type) && \
	SPID_HARDWARE_ID(platform, devtype, product, hardware))



/* INTEL_MID_BOARD - Returns true if arugments matches SPID contents
 * @ level:	1, 2, 3
		- 1 for verifying platform_id,
		- 2 for verifying platform_type & product_id,
		- 3 for verifying platform_type, product_id & hardware_id.
 * @ devtype:	PHONE or TABLET
 * @ arg3:	platform_type - MFLD,CLVTP,CLVT,MRFL.
 * @ arg4:	product ID - product id supported by
		platform_type passed in arg3.
 * @ arg5:	PRO or ENG.
 * @ arg6:	hardware_id -Hardware IDs supported by above
		platform_type & product_id.
 *
 * Example:	INTEL_MID_BOARD(1,PHONE,MFLD)
 *		INTEL_MID_BOARD(2,PHONE,MFLD,BB15,PRO)
 *		INTEL_MID_BOARD(3,PHONE,MFLD,BB15,PRO,PR20),
 *
 */
#define INTEL_MID_BOARD(level, devtype, ...) ( \
	INTEL_MID_BOARDV##level(devtype, __VA_ARGS__))

#endif /* _ASM_X86_SPID_H */
