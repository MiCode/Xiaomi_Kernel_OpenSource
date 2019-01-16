#ifndef __DDP_DPI_REG_H__
#define __DDP_DPI_REG_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    unsigned EN         : 1;
	unsigned rsv_1      : 31;
}DPI_REG_EN, *PDPI_REG_EN;

typedef struct
{
    unsigned BG_EN          : 1;
    unsigned RGB_SWAP       : 1;
    unsigned INTL_EN        : 1;
    unsigned TDFP_EN        : 1;
    unsigned CLPF_EN        : 1;
    unsigned YUV422_EN      : 1;
    unsigned RGB2YUV_EN     : 1;
    unsigned R601_SEL       : 1;
    unsigned EMBSYNC_EN     : 1;
    unsigned rsv_7          : 7;
    unsigned VS_LODD_EN     : 1;
    unsigned VS_LEVEN_EN    : 1;
    unsigned VS_RODD_EN     : 1;
    unsigned VS_REVEN_EN    :1;
    unsigned FAKE_DE_LODD   :1;
    unsigned FAKE_DE_LEVEN  :1;
    unsigned FAKE_DE_RODD   :1;
    unsigned FAKE_DE_REVEN  :1;
    unsigned rsv_8           : 8;
} DPI_REG_CNTL, *PDPI_REG_CNTL;


typedef struct
{
    unsigned VSYNC          : 1;
    unsigned VDE            : 1;
    unsigned UNDERFLOW      :1;
    unsigned rsv_3          : 29;
} DPI_REG_INTERRUPT, *PDPI_REG_INTERRUPT;


typedef struct
{
    UINT16 WIDTH;
    UINT16 HEIGHT;
} DPI_REG_SIZE, *PDPI_REG_SIZE;


typedef struct
{
    unsigned CH_SWAP  : 3;
    unsigned BIT_SWAP    : 1;
    unsigned B_MASK     : 1;
    unsigned G_MASK     : 1;
    unsigned R_MASK     : 1;
    unsigned rsv_7        : 1;
    unsigned DE_MASK     : 1;
    unsigned HS_MASK     : 1;
    unsigned VS_MASK     : 1;
    unsigned rsv_11       : 1;	
    unsigned DE_POL     : 1;	
    unsigned HSYNC_POL     : 1;	
    unsigned VSYNC_POL     : 1;	
    unsigned CLK_POL     : 1;	
    unsigned DPI_O_EN     : 1;	
    unsigned DUAL_EDGE_SEL     : 1;
    unsigned OUT_BIT     : 2;
    unsigned YC_MAP     : 3;
    unsigned rsv_23   : 9;
} DPI_REG_OUTPUT_SETTING, *PDPI_REG_OUTPUT_SETTING;



typedef struct
{
    unsigned DDR_EN     : 1;
    unsigned DDR_SEL    : 1;
    unsigned DDR_4PHASE : 1;
    unsigned rsv_3        : 1;
    unsigned DDR_WIDTH     : 2;
    unsigned rsv_6        : 2;
    unsigned DDR_PAD_MODE     : 1;
    unsigned rsv_9        : 23;
    
} DPI_REG_DDR_SETTING, *PDPI_REG_DDR_SETTING;

typedef struct
{
    unsigned V_CNT         : 13;
    unsigned rsv_13        : 3;
    unsigned DPI_BUSY      : 1;
    unsigned OUT_EN        : 1;
    unsigned rsv_18        : 2;
	unsigned FIELD         : 1;
	unsigned TDLR          : 1;
    unsigned rsv_22        : 10;
} DPI_REG_STATUS, *PDPI_REG_STATUS;


typedef struct
{
    unsigned OEN_EN         : 1;
    unsigned rsv_1        : 31;
} DPI_REG_TMODE, *PDPI_REG_TMODE;


typedef struct
{
    unsigned CHKSUM         : 24;
    unsigned rsv_24        : 6;
    unsigned CHKSUM_RDY     :1;
    unsigned CHKSUM_EN       :1;
} DPI_REG_CHKSUM, *PDPI_REG_CHKSUM;


typedef struct
{
    unsigned HPW       : 8;
    unsigned HBP       : 8;
    unsigned HFP       : 8;
    unsigned HSYNC_POL : 1;
    unsigned DE_POL    : 1;
    unsigned rsv_26    : 6;
} DPI_REG_TGEN_HCNTL, *PDPI_REG_TGEN_HCNTL;

typedef struct
{
    unsigned HBP       : 12;
	unsigned rsv_12    : 4;
    unsigned HFP       : 12;
    unsigned rsv_28    : 4;
} DPI_REG_TGEN_HPORCH, *PDPI_REG_TGEN_HPORCH;

typedef struct
{
    unsigned VPW_LODD  : 8;
	unsigned VPW_HALF_LODD  : 1;
    unsigned rsv_9    : 23;
} DPI_REG_TGEN_VWIDTH_LODD, *PDPI_REG_TGEN_VWIDTH_LODD;

typedef struct
{
    unsigned VBP_LODD  : 8;
	unsigned VBP_HALF_LODD  : 1;
    unsigned rsv_9    : 7;
    unsigned VFP_LODD  : 8;
	unsigned VFP_HALF_LODD  : 1;
    unsigned rsv_25    : 7;
} DPI_REG_TGEN_VPORCH_LODD, *PDPI_REG_TGEN_VPORCH_LODD;

typedef struct
{
    unsigned VPW_LEVEN  : 7;
	unsigned VPW_HALF_LEVEN  : 1;
    unsigned rsv_8    : 24;
} DPI_REG_TGEN_VWIDTH_LEVEN, *PDPI_REG_TGEN_VWIDTH_LEVEN;

typedef struct
{
    unsigned VBP_LEVEN  : 8;
	unsigned VBP_HALF_LEVEN  : 1;
    unsigned rsv_9    : 7;
    unsigned VFP_LEVEN  : 8;
	unsigned VFP_HALF_LEVEN  : 1;
    unsigned rsv_25    : 7;
} DPI_REG_TGEN_VPORCH_LEVEN, *PDPI_REG_TGEN_VPORCH_LEVEN;

typedef struct
{
    unsigned VPW_RODD  : 8;
	unsigned VPW_HALF_RODD  : 1;
    unsigned rsv_9    : 23;
} DPI_REG_TGEN_VWIDTH_RODD, *PDPI_REG_TGEN_VWIDTH_RODD;

typedef struct
{
    unsigned VBP_RODD  : 8;
	unsigned VBP_HALF_RODD  : 1;
    unsigned rsv_9    : 7;
    unsigned VFP_RODD  : 8;
	unsigned VFP_HALF_RODD  : 1;
    unsigned rsv_25    : 7;
} DPI_REG_TGEN_VPORCH_RODD, *PDPI_REG_TGEN_VPORCH_RODD;

typedef struct
{
    unsigned VPW_REVEN  : 7;
	unsigned VPW_HALF_REVEN  : 1;
    unsigned rsv_8    : 24;
} DPI_REG_TGEN_VWIDTH_REVEN, *PDPI_REG_TGEN_VWIDTH_REVEN;

typedef struct
{
    unsigned VBP_REVEN  : 8;
	unsigned VBP_HALF_REVEN  : 1;
    unsigned rsv_9    : 7;
    unsigned VFP_REVEN  : 8;
	unsigned VFP_HALF_REVEN  : 1;
    unsigned rsv_25    : 7;
} DPI_REG_TGEN_VPORCH_REVEN, *PDPI_REG_TGEN_VPORCH_REVEN;

typedef struct
{
    unsigned ESAV_VOFST_LODD  : 12;
	unsigned rsv_12   : 4;
    unsigned ESAV_VWID_LODD  : 12;
	unsigned rsv_28   : 4;
} DPI_REG_ESAV_VTIM_LOAD, *PDPI_REG_ESAV_VTIM_LOAD;

typedef struct
{
    unsigned ESAV_VVOFST_LEVEN  : 12;
	unsigned rsv_12   : 4;
    unsigned ESAV_VWID_LEVEN  : 12;
	unsigned rsv_28   : 4;
} DPI_REG_ESAV_VTIM_LEVEN, *PDPI_REG_ESAV_VTIM_LEVEN;

typedef struct
{
    unsigned ESAV_VOFST_ROAD  : 12;
	unsigned rsv_12   : 4;
    unsigned ESAV_VWID_RODD  : 12;
	unsigned rsv_28   : 4;
} DPI_REG_ESAV_VTIM_ROAD, *PDPI_REG_ESAV_VTIM_ROAD;

typedef struct
{
    unsigned ESAV_VOFST_REVEN  : 12;
	unsigned rsv_12   : 4;
    unsigned ESAV_VWID_REVEN  : 12;
	unsigned rsv_28   : 4;
} DPI_REG_ESAV_VTIM_REVEN, *PDPI_REG_ESAV_VTIM_REVEN;


typedef struct
{
    unsigned ESAV_FOFST_ODD  : 12;
	unsigned rsv_12   : 4;
    unsigned ESAV_FOFST_EVEN  : 12;
	unsigned rsv_28   : 4;
} DPI_REG_ESAV_FTIM, *PDPI_REG_ESAV_FTIM;

typedef struct
{
    unsigned VPW       : 8;
    unsigned VBP       : 8;
    unsigned VFP       : 8;
    unsigned VSYNC_POL : 1;
    unsigned rsv_25    : 7;
} DPI_REG_TGEN_VCNTL, *PDPI_REG_TGEN_VCNTL;

typedef struct
{
    unsigned BG_RIGHT  : 13;
	unsigned rsv_13    : 3;
    unsigned BG_LEFT   : 13;
    unsigned rsv_29    : 3;
} DPI_REG_BG_HCNTL, *PDPI_REG_BG_HCNTL;


typedef struct
{
    unsigned BG_BOT   : 13;
	unsigned rsv_13    : 3;
    unsigned BG_TOP   : 13;
    unsigned rsv_29   : 3;
} DPI_REG_BG_VCNTL, *PDPI_REG_BG_VCNTL;


typedef struct
{
    unsigned BG_B     : 8;
	unsigned BG_G     : 8;
    unsigned BG_R     : 8;
    unsigned rsv_24   : 8;
} DPI_REG_BG_COLOR, *PDPI_REG_BG_COLOR;




typedef struct
{
    unsigned FIFO_VALID_SET     : 5;
	unsigned rsv_3     : 3;
    unsigned FIFO_RST_SEL     : 1;
    unsigned rsv_9   : 23;
} DPI_REG_FIFO_CTL, *PDPI_REG_FIFO_CTL;

typedef struct
{
    unsigned ESAV_CODE0     : 12;
    unsigned rsv_12         : 4;
	unsigned ESAV_CODE1      : 12;
    unsigned rsv_28        : 4;
} DPI_REG_ESAV_CODE_SET0, *PDPI_REG_ESAV_CODE_SET0;


typedef struct
{
    unsigned ESAV_CODE2     : 12;
    unsigned rsv_12         : 4;
	unsigned ESAV_CODE3_MSB      : 1;
    unsigned rsv_17        : 15;
} DPI_REG_ESAV_CODE_SET1, *PDPI_REG_ESAV_CODE_SET1;

typedef struct
{
	unsigned CLPF_TYPE     : 2;
	unsigned rsv2          : 2;
	unsigned ROUND_EN      : 1;
	unsigned rsv5          : 27;
}DPI_REG_CLPF_SETTING, *PDPI_REG_CLPF_SETTING;

typedef struct
{
	unsigned Y_LIMIT_BOT   : 12;
	unsigned rsv12         : 4;
	unsigned Y_LIMIT_TOP   : 12;
	unsigned rsv28         : 4;
}DPI_REG_Y_LIMIT, *PDPI_REG_Y_LIMIT;

typedef struct
{
	unsigned C_LIMIT_BOT   : 12;
	unsigned rsv12         : 4;
	unsigned C_LIMIT_TOP   : 12;
	unsigned rsv28         : 4;
}DPI_REG_C_LIMIT, *PDPI_REG_C_LIMIT;

typedef struct
{
	unsigned UV_SWAP       : 1;
	unsigned rsv1          : 3;
	unsigned CR_DELSEL     : 1;
	unsigned CB_DELSEL     : 1;
	unsigned Y_DELSEL      : 1;
	unsigned DE_DELSEL     : 1;
	unsigned rsv8         : 24;
}DPI_REG_YUV422_SETTING, *PDPI_REG_YUV422_SETTING;

typedef struct
{
	unsigned EMBVSYNC_R_CR     : 1;
	unsigned EMBVSYNC_G_Y      : 1;
    unsigned EMBVSYNC_B_CB     : 1;
    unsigned rsv_3         	   : 1;
	unsigned ESAV_F_INV        : 1;
	unsigned ESAV_V_INV        : 1;
	unsigned ESAV_H_INV        : 1;
    unsigned rsv_7             : 1;
	unsigned ESAV_CODE_MAN     : 1;
    unsigned rsv_9             : 3;
    unsigned VS_OUT_SEL         :3;
    unsigned rsv_15             :17;
}DPI_REG_EMBSYNC_SETTING;

typedef struct
{
	unsigned PAT_EN         :1;
	unsigned rsv_1          :3;
	unsigned PAT_SEL        :3;
	unsigned rsv_6          :1;
	unsigned PAT_B_MAN      :8;
	unsigned PAT_G_MAN      :8;
	unsigned PAT_R_MAN      :8;
}DPI_REG_PATTERN;

typedef struct
{
	unsigned MATRIX_C00     :13;
	unsigned rsv_13         :3;
	unsigned MATRIX_C01     :13;
	unsigned rsv_29         :3;
}DPI_REG_MATRIX_COEFF_SET0;

typedef struct
{
	unsigned MATRIX_C02     :13;
	unsigned rsv_13         :3;
	unsigned MATRIX_C10     :13;
	unsigned rsv_29         :3;
}DPI_REG_MATRIX_COEFF_SET1;

typedef struct
{
	unsigned MATRIX_C11     :13;
	unsigned rsv_13         :3;
	unsigned MATRIX_C12     :13;
	unsigned rsv_29         :3;
}DPI_REG_MATRIX_COEFF_SET2;

typedef struct
{
	unsigned MATRIX_C20     :13;
	unsigned rsv_13         :3;
	unsigned MATRIX_C21     :13;
	unsigned rsv_29         :3;
}DPI_REG_MATRIX_COEFF_SET3;

typedef struct
{
	unsigned MATRIX_C22     :13;
	unsigned rsv_13         :19;
}DPI_REG_MATRIX_COEFF_SET4;

typedef struct
{
	unsigned MATRIX_PRE_ADD_0  :9;
	unsigned rsv_9             :7;
	unsigned MATRIX_PRE_ADD_1  :9;
	unsigned rsv_24            :7;
}DPI_REG_MATRIX_PREADD_SET0;

typedef struct
{
	unsigned MATRIX_PRE_ADD_2  :9;
	unsigned rsv_9             :23;
}DPI_REG_MATRIX_PREADD_SET1;

typedef struct
{
	unsigned MATRIX_POST_ADD_0  :13;
	unsigned rsv_13             :3;
	unsigned MATRIX_POST_ADD_1  :13;
	unsigned rsv_24             :3;
}DPI_REG_MATRIX_POSTADD_SET0;

typedef struct
{
	unsigned MATRIX_POST_ADD_2  :13;
	unsigned rsv_13             :19;
}DPI_REG_MATRIX_POSTADD_SET1;


typedef struct
{
    DPI_REG_EN        DPI_EN;           // 0000
    UINT32   		  DPI_RST;			// 0004
    DPI_REG_INTERRUPT INT_ENABLE;       // 0008
    DPI_REG_INTERRUPT INT_STATUS;       // 000C
    DPI_REG_CNTL      CNTL;             // 0010
	DPI_REG_OUTPUT_SETTING  OUTPUT_SETTING;  //0014
    DPI_REG_SIZE            SIZE;             // 0018
	DPI_REG_DDR_SETTING	    DDR_SETTING;			// 001c
	
	UINT32              TGEN_HWIDTH;      // 0020
    DPI_REG_TGEN_HPORCH TGEN_HPORCH;    // 0024
	DPI_REG_TGEN_VWIDTH_LODD TGEN_VWIDTH_LODD;	// 0028
    DPI_REG_TGEN_VPORCH_LODD TGEN_VPORCH_LODD;	// 002C    
    DPI_REG_BG_HCNTL    BG_HCNTL;        // 0030  
    DPI_REG_BG_VCNTL    BG_VCNTL;        // 0034  
    DPI_REG_BG_COLOR    BG_COLOR;        // 0038  
    DPI_REG_FIFO_CTL    FIFO_CTL;        //003C
    
    DPI_REG_STATUS      STATUS;          // 0040
    DPI_REG_TMODE       TMODE;           //0044
    DPI_REG_CHKSUM      CHKSUM;            //0048
    UINT32              rsv_4C;
    UINT32              DUMMY;            //0050
    UINT32              rsv_54[5];  

	DPI_REG_TGEN_VWIDTH_LEVEN   TGEN_VWIDTH_LEVEN; // 0068
    DPI_REG_TGEN_VPORCH_LEVEN   TGEN_VPORCH_LEVEN; // 006C

	DPI_REG_TGEN_VWIDTH_RODD    TGEN_VWIDTH_RODD;	// 0070
    DPI_REG_TGEN_VPORCH_RODD    TGEN_VPORCH_RODD;	// 0074
	DPI_REG_TGEN_VWIDTH_REVEN   TGEN_VWIDTH_REVEN; // 0078
    DPI_REG_TGEN_VPORCH_REVEN   TGEN_VPORCH_REVEN; // 007C
	DPI_REG_ESAV_VTIM_LOAD      ESAV_VTIM_LOAD;               // 0080
	DPI_REG_ESAV_VTIM_LEVEN     ESAV_VTIM_LEVEN;               // 0084
	DPI_REG_ESAV_VTIM_ROAD      ESAV_VTIM_ROAD;               // 0088
	DPI_REG_ESAV_VTIM_REVEN     ESAV_VTIM_REVEN;               // 008C
	
	DPI_REG_ESAV_FTIM       ESAV_FTIM;                // 0090	
	DPI_REG_CLPF_SETTING    CLPF_SETTING;    //0094
	DPI_REG_Y_LIMIT         Y_LIMIT;         //0098
	DPI_REG_C_LIMIT         C_LIMIT;         //009C
	DPI_REG_YUV422_SETTING  YUV422_SETTING;  //00A0
	DPI_REG_EMBSYNC_SETTING EMBSYNC_SETTING; //00A4
	DPI_REG_ESAV_CODE_SET0  ESAV_CODE_SET0;          // 00A8
	DPI_REG_ESAV_CODE_SET1  ESAV_CODE_SET1;          // 00AC
	
	} volatile DPI_REGS, *PDPI_REGS;

#ifndef BUILD_UBOOT
STATIC_ASSERT(0x0018 == offsetof(DPI_REGS, SIZE));
STATIC_ASSERT(0x0038 == offsetof(DPI_REGS, BG_COLOR));
STATIC_ASSERT(0x0070 == offsetof(DPI_REGS, TGEN_VWIDTH_RODD));
STATIC_ASSERT(0x00AC == offsetof(DPI_REGS, ESAV_CODE_SET1));
#endif
#ifdef __cplusplus
}
#endif

#endif // __DPI_REG_H__
