#ifndef _DDP_REG_H_
#define _DDP_REG_H_
#include <mach/sync_write.h>
#include <mach/mt_typedefs.h>
//#include <mach/mt_reg_base.h>
#include "display_recorder.h"
#include "cmdq_record.h"
#include "cmdq_core.h"
#include "ddp_hal.h"

// MIPITX and DSI

typedef struct
{
    unsigned RG_DSI_LDOCORE_EN                	: 1;
    unsigned RG_DSI_CKG_LDOOUT_EN		: 1;
    unsigned RG_DSI_BCLK_SEL                		: 2;
    unsigned RG_DSI_LD_IDX_SEL                	: 3;
    unsigned rsv_7							: 1;
    unsigned RG_DSI_PHYCLK_SEL                	: 2;
    unsigned RG_DSI_DSICLK_FREQ_SEL         : 1;
    unsigned RG_DSI_LPTX_CLMP_EN                : 1;
    unsigned rsv_12						: 20;
} MIPITX_DSI_CON_REG, *PMIPITX_DSI_CON_REG;


typedef struct
{
    unsigned RG_DSI_LNTC_LDOOUT_EN     		: 1;
    unsigned RG_DSI_LNTC_CKLANE_EN    		: 1;
    unsigned RG_DSI_LNTC_LPTX_IPLUS1     		: 1;
    unsigned RG_DSI_LNTC_LPTX_IPLUS2     		: 1;
    unsigned RG_DSI_LNTC_LPTX_IMINUS     		: 1;
    unsigned RG_DSI_LNTC_LPCD_IPLUS     		: 1;
    unsigned RG_DSI_LNTC_LPCD_IMINUS		: 1;
    unsigned rsv_7           						: 1;
    unsigned RG_DSI_LNTC_RT_CODE     			: 4;
    unsigned rsv_12           						: 20;
} MIPITX_DSI_CLOCK_LANE_REG, *PMIPITX_DSI_CLOCK_LANE_REG;


typedef struct
{
    unsigned RG_DSI_LNT0_LDOOUT_EN     		: 1;
    unsigned RG_DSI_LNT0_CKLANE_EN 		    	: 1;
    unsigned RG_DSI_LNT0_LPTX_IPLUS1     		: 1;
    unsigned RG_DSI_LNT0_LPTX_IPLUS2     		: 1;
    unsigned RG_DSI_LNT0_LPTX_IMINUS     		: 1;
    unsigned RG_DSI_LNT0_LPCD_IPLUS     		: 1;
    unsigned RG_DSI_LNT0_LPCD_IMINUS     		: 1;
    unsigned rsv_7           						: 1;
    unsigned RG_DSI_LNT0_RT_CODE     			: 4;
    unsigned rsv_11           						: 20;
} MIPITX_DSI_DATA_LANE0_REG, *PMIPITX_DSI_DATA_LANE0_REG;


typedef struct
{
    unsigned RG_DSI_LNT1_LDOOUT_EN     		: 1;
    unsigned RG_DSI_LNT1_CKLANE_EN 		    	: 1;
    unsigned RG_DSI_LNT1_LPTX_IPLUS1     		: 1;
    unsigned RG_DSI_LNT1_LPTX_IPLUS2     		: 1;
    unsigned RG_DSI_LNT1_LPTX_IMINUS     		: 1;
    unsigned RG_DSI_LNT1_LPCD_IPLUS     		: 1;
    unsigned RG_DSI_LNT1_LPCD_IMINUS     		: 1;
    unsigned rsv_7           						: 1;
    unsigned RG_DSI_LNT1_RT_CODE     			: 4;
    unsigned rsv_11           						: 20;
} MIPITX_DSI_DATA_LANE1_REG, *PMIPITX_DSI_DATA_LANE1_REG;


typedef struct
{
    unsigned RG_DSI_LNT2_LDOOUT_EN     		: 1;
    unsigned RG_DSI_LNT2_CKLANE_EN 		    	: 1;
    unsigned RG_DSI_LNT2_LPTX_IPLUS1     		: 1;
    unsigned RG_DSI_LNT2_LPTX_IPLUS2     		: 1;
    unsigned RG_DSI_LNT2_LPTX_IMINUS     		: 1;
    unsigned RG_DSI_LNT2_LPCD_IPLUS     		: 1;
    unsigned RG_DSI_LNT2_LPCD_IMINUS     		: 1;
    unsigned rsv_7           						: 1;
    unsigned RG_DSI_LNT2_RT_CODE     			: 4;
    unsigned rsv_11           						: 20;
} MIPITX_DSI_DATA_LANE2_REG, *PMIPITX_DSI_DATA_LANE2_REG;

typedef struct
{
    unsigned RG_DSI_LNT3_LDOOUT_EN     		: 1;
    unsigned RG_DSI_LNT3_CKLANE_EN 		    	: 1;
    unsigned RG_DSI_LNT3_LPTX_IPLUS1     		: 1;
    unsigned RG_DSI_LNT3_LPTX_IPLUS2     		: 1;
    unsigned RG_DSI_LNT3_LPTX_IMINUS     		: 1;
    unsigned RG_DSI_LNT3_LPCD_IPLUS     		: 1;
    unsigned RG_DSI_LNT3_LPCD_IMINUS     		: 1;
    unsigned rsv_7           						: 1;
    unsigned RG_DSI_LNT3_RT_CODE     			: 4;
    unsigned rsv_11           						: 20;
}MIPITX_DSI_DATA_LANE3_REG, *PMIPITX_DSI_DATA_LANE3_REG;

typedef struct
{
    unsigned RG_DSI_LNT_INTR_EN				: 1;
    unsigned RG_DSI_LNT_HS_BIAS_EN			: 1;
    unsigned RG_DSI_LNT_IMP_CAL_EN			: 1;
    unsigned RG_DSI_LNT_TESTMODE_EN		: 1;
    unsigned RG_DSI_LNT_IMP_CAL_CODE		: 4;
    unsigned RG_DSI_LNT_AIO_SEL				: 3;
    unsigned RG_DSI_PAD_TIE_LOW_EN			: 1;
    unsigned rsv_12							: 1;
    unsigned RG_DSI_PRESERVE					: 3;
    unsigned rsv_16							: 16;
} MIPITX_DSI_TOP_CON_REG, *PMIPITX_DSI_TOP_CON_REG;


typedef struct
{
    unsigned RG_DSI_BG_CORE_EN			: 1;
    unsigned RG_DSI_BG_CKEN				: 1;
    unsigned RG_DSI_BG_DIV				: 2;
    unsigned RG_DSI_BG_FAST_CHARGE		: 1;
    unsigned RG_DSI_V12_SEL				: 3;
    unsigned RG_DSI_V10_SEL				: 3;
    unsigned RG_DSI_V072_SEL				: 3;
    unsigned RG_DSI_V04_SEL				: 3;
    unsigned RG_DSI_V032_SEL				: 3;
    unsigned RG_DSI_V02_SEL				: 3;
    unsigned rsv_23						: 1;
    unsigned RG_DSI_BG_R1_TRIM			: 4;
    unsigned RG_DSI_BG_R2_TRIM			: 4;
} MIPITX_DSI_BG_CON_REG, *PMIPITX_DSI_BG_CON_REG;


typedef struct
{
    unsigned RG_DSI0_MPPLL_PLL_EN		: 1;
    unsigned RG_DSI0_MPPLL_PREDIV		: 2;
    unsigned RG_DSI0_MPPLL_TXDIV0		: 2;
    unsigned RG_DSI0_MPPLL_TXDIV1		: 2;
    unsigned RG_DSI0_MPPLL_POSDIV		: 3;
    unsigned RG_DSI0_MPPLL_MONVC_EN	: 1;
    unsigned RG_DSI0_MPPLL_MONREF_EN	: 1;
    unsigned RG_DSI0_MPPLL_VDO_EN		: 1;
    unsigned rsv_13						: 19;
} MIPITX_DSI_PLL_CON0_REG, *PMIPITX_DSI_PLL_CON0_REG;


typedef struct
{
    unsigned RG_DSI0_MPPLL_SDM_FRA_EN			: 1;
    unsigned RG_DSI0_MPPLL_SDM_SSC_PH_INIT	: 1;
    unsigned RG_DSI0_MPPLL_SDM_SSC_EN			: 1;
    unsigned rsv_3									: 13;
    unsigned RG_DSI0_MPPLL_SDM_SSC_PRD		: 16;
} MIPITX_DSI_PLL_CON1_REG, *PMIPITX_DSI_PLL_CON1_REG;

typedef struct
{
    unsigned RG_DSI0_MPPLL_SDM_PCW_0_7			: 8;
    unsigned RG_DSI0_MPPLL_SDM_PCW_8_15			: 8;
    unsigned RG_DSI0_MPPLL_SDM_PCW_16_23			: 8;
    unsigned RG_DSI0_MPPLL_SDM_PCW_H				: 7;
    unsigned rsv_31									: 1;
} MIPITX_DSI_PLL_CON2_REG, *PMIPITX_DSI_PLL_CON2_REG;


typedef struct
{
    unsigned RG_DSI0_MPPLL_SDM_SSC_DELTA1			: 16;
    unsigned RG_DSI0_MPPLL_SDM_SSC_DELTA			: 16;
} MIPITX_DSI_PLL_CON3_REG, *PMIPITX_DSI_PLL_CON3_REG;


typedef struct
{
    unsigned RG_DSI0_MPPLL_SDM_PCW_CHG		: 1;
    unsigned rsv_1									: 31;
} MIPITX_DSI_PLL_CHG_REG, *PMIPITX_DSI_PLL_CHG_REG;


typedef struct
{
    unsigned RG_MPPLL_TST_EN			: 1;
    unsigned RG_MPPLL_TSTCK_EN		: 1;
    unsigned RG_MPPLL_TSTSEL			: 2;
    unsigned RG_MPPLL_S2QDIV			: 2;
    unsigned RG_MPPLL_PLLOUT_EN		: 1;
    unsigned RG_MPPLL_PRESERVE		: 5;
    unsigned rsv_12 					: 20;
} MIPITX_DSI_PLL_TOP_REG, *PMIPITX_DSI_PLL_TOP_REG;


typedef struct
{
    unsigned DA_DSI_MPPLL_SDM_PWR_ON		: 1;
    unsigned DA_DSI_MPPLL_SDM_ISO_EN		: 1;
    unsigned rsv_2 								: 6;
    unsigned AD_DSI0_MPPLL_SDM_PWR_ACK	: 1;
    unsigned rsv_9 								: 23;
} MIPITX_DSI_PLL_PWR_REG, *PMIPITX_DSI_PLL_PWR_REG;


typedef struct
{
    unsigned RGS_DSI_LNT_IMP_CAL_OUTPUT		: 1;
    unsigned rsv_1 									: 31;
} MIPITX_DSI_RGS_REG, *PMIPITX_DSI_RGS_REG;


typedef struct
{
    unsigned RG_DSI0_GPI0_EN			: 1;
    unsigned RG_DSI0_GPI1_EN			: 1;
    unsigned RG_DSI0_GPI2_EN			: 1;
    unsigned RG_DSI0_GPI3_EN			: 1;
    unsigned RG_DSI0_GPI4_EN			: 1;
    unsigned RG_DSI0_GPI5_EN			: 1;
    unsigned RG_DSI0_GPI6_EN			: 1;
    unsigned RG_DSI0_GPI7_EN			: 1;
    unsigned RG_DSI0_GPI8_EN			: 1;
    unsigned RG_DSI0_GPI9_EN			: 1;
    unsigned RG_DSI0_GPI_SMT_EN		: 1;
    unsigned RG_DSI0_GPI_DRIVE_EN	: 1;
    unsigned rsv_12 					: 20;
} MIPITX_DSI_GPI_EN_REG, *PMIPITX_DSI_GPI_EN_REG;

typedef struct
{
    unsigned RG_DSI0_GPI0_PD			: 1;
    unsigned RG_DSI0_GPI1_PD			: 1;
    unsigned RG_DSI0_GPI2_PD			: 1;
    unsigned RG_DSI0_GPI3_PD			: 1;
    unsigned RG_DSI0_GPI4_PD			: 1;
    unsigned RG_DSI0_GPI5_PD			: 1;
    unsigned RG_DSI0_GPI6_PD			: 1;
    unsigned RG_DSI0_GPI7_PD			: 1;
    unsigned RG_DSI0_GPI8_PD			: 1;
    unsigned RG_DSI0_GPI9_PD			: 1;
    unsigned rsv_10 					: 6;
    unsigned RG_DSI0_GPI0_PU			: 1;
    unsigned RG_DSI0_GPI1_PU			: 1;
    unsigned RG_DSI0_GPI2_PU			: 1;
    unsigned RG_DSI0_GPI3_PU			: 1;
    unsigned RG_DSI0_GPI4_PU			: 1;
    unsigned RG_DSI0_GPI5_PU			: 1;
    unsigned RG_DSI0_GPI6_PU			: 1;
    unsigned RG_DSI0_GPI7_PU			: 1;
    unsigned RG_DSI0_GPI8_PU			: 1;
    unsigned RG_DSI0_GPI9_PU			: 1;
    unsigned rsv_26 					: 6;
} MIPITX_DSI_GPI_PULL_REG, *PMIPITX_DSI_GPI_PULL_REG;


typedef struct
{
    unsigned MIPI_TX_PHY0_SEL		: 3;
    unsigned rsv_3						: 1;
    unsigned MIPI_TX_PHY1_SEL		: 3;
    unsigned rsv_7						: 1;
    unsigned MIPI_TX_PHY2_SEL		: 3;
    unsigned rsv_11					: 1;
    unsigned MIPI_TX_PHY3_SEL		: 3;
    unsigned rsv_15					: 1;
    unsigned MIPI_TX_PHYC_SEL		: 3;
    unsigned rsv_19					: 1;
    unsigned MIPI_TX_LPRX_SEL			: 3;
    unsigned rsv_23					: 9;
} MIPITX_DSI_PHY_SEL_REG, *PMIPITX_DSI_PHY_SEL_REG;


typedef struct
{
    unsigned SW_CTRL_EN		: 1;
    unsigned rsv_1 				: 31;
} MIPITX_DSI_SW_CTRL_REG, *PMIPITX_DSI_SW_CTRL_REG;


typedef struct
{
    unsigned SW_LNTC_LPTX_PRE_OE		: 1;
    unsigned SW_LNTC_LPTX_OE			: 1;
    unsigned SW_LNTC_LPTX_DP			: 1;
    unsigned SW_LNTC_LPTX_DN			: 1;
    unsigned SW_LNTC_HSTX_PRE_OE		: 1;
    unsigned SW_LNTC_HSTX_OE			: 1;
    unsigned SW_LNTC_HSTX_RDY			: 1;
    unsigned SW_LNTC_LPRX_EN			: 1;
    unsigned SW_LNTC_HSTX_DATA			: 8;
    unsigned rsv_16						: 16;
} MIPITX_DSI_SW_CTRL_CON0_REG, *PMIPITX_DSI_SW_CTRL_CON0_REG;


typedef struct
{
    unsigned SW_LNT0_LPTX_PRE_OE			: 1;
    unsigned SW_LNT0_LPTX_OE				: 1;
    unsigned SW_LNT0_LPTX_DP				: 1;
    unsigned SW_LNT0_LPTX_DN				: 1;
    unsigned SW_LNT0_HSTX_PRE_OE			: 1;
    unsigned SW_LNT0_HSTX_OE				: 1;
    unsigned SW_LNT0_HSTX_RDY				: 1;
    unsigned SW_LNT0_LPRX_EN				: 1;
    unsigned SW_LNT1_LPTX_PRE_OE			: 1;
    unsigned SW_LNT1_LPTX_OE				: 1;
    unsigned SW_LNT1_LPTX_DP				: 1;
    unsigned SW_LNT1_LPTX_DN				: 1;
    unsigned SW_LNT1_HSTX_PRE_OE			: 1;
    unsigned SW_LNT1_HSTX_OE				: 1;
    unsigned SW_LNT1_HSTX_RDY				: 1;
    unsigned SW_LNT1_LPRX_EN				: 1;
    unsigned SW_LNT2_LPTX_PRE_OE			: 1;
    unsigned SW_LNT2_LPTX_OE				: 1;
    unsigned SW_LNT2_LPTX_DP				: 1;
    unsigned SW_LNT2_LPTX_DN				: 1;
    unsigned SW_LNT2_HSTX_PRE_OE			: 1;
    unsigned SW_LNT2_HSTX_OE				: 1;
    unsigned SW_LNT2_HSTX_RDY				: 1;
    unsigned SW_LNT2_LPRX_EN				: 1;
    unsigned SW_LNT3_LPTX_PRE_OE			: 1;
    unsigned SW_LNT3_LPTX_OE				: 1;
    unsigned SW_LNT3_LPTX_DP					: 1;
    unsigned SW_LNT3_LPTX_DN				: 1;
    unsigned SW_LNT3_HSTX_PRE_OE			: 1;
    unsigned SW_LNT3_HSTX_OE				: 1;
    unsigned SW_LNT3_HSTX_RDY				: 1;
    unsigned SW_LNT3_LPRX_EN				: 1;
} MIPITX_DSI_SW_CTRL_CON1_REG, *PMIPITX_DSI_SW_CTRL_CON1_REG;

typedef struct
{
    unsigned SW_LNT_HSTX_DATA		: 8;
    unsigned rsv_8 						: 24;
} MIPITX_DSI_SW_CTRL_CON2_REG, *PMIPITX_DSI_SW_CTRL_CON2_REG;

typedef struct
{
    unsigned MIPI_TX_DBG_SEL				: 4;
    unsigned MIPI_TX_DBG_OUT_EN			: 1;
    unsigned MIPI_TX_GPIO_MODE_EN		: 1;
    unsigned MIPI_TX_APB_ASYNC_CNT_EN	: 1;
    unsigned rsv_7 							: 25;
} MIPITX_DSI_DBG_CON_REG, *PMIPITX_DSI_DBG_CON_REG;

typedef struct
{
    unsigned MIPI_TX_APB_ASYNC_ERR				: 1;
    unsigned MIPI_TX_APB_ASYNC_ERR_ADDR		: 10;
    unsigned rsv_11 								: 21;
} MIPITX_DSI_APB_ASYNC_STA_REG, *PMIPITX_DSI_APB_ASYNC_STA_REG;





typedef struct
{
    unsigned DSI_START	: 1;
    unsigned rsv_1		: 1;
    unsigned SLEEPOUT_START	: 1;
    unsigned rsv_3		: 13;
    unsigned VM_CMD_START	: 1;
    unsigned rsv_17		: 15;
} DSI_START_REG, *PDSI_START_REG;


typedef struct
{
    unsigned rsv_0		   : 1;
    unsigned BUF_UNDERRUN  : 1;
    unsigned rsv_2		   : 2;
    unsigned ESC_ENTRY_ERR : 1;
    unsigned ESC_SYNC_ERR : 1;
    unsigned CTRL_ERR      : 1;
    unsigned CONTENT_ERR   : 1;
    unsigned rsv_8		   : 24;
} DSI_STATUS_REG, *PDSI_STATUS_REG;


typedef struct
{
    unsigned RD_RDY			: 1;
    unsigned CMD_DONE		: 1;
    unsigned TE_RDY         : 1;
    unsigned VM_DONE        : 1;
    unsigned EXT_TE         : 1;
    unsigned VM_CMD_DONE    : 1;
    unsigned SLEEPOUT_DONE    : 1;
    unsigned rsv_7			: 25;
} DSI_INT_ENABLE_REG, *PDSI_INT_ENABLE_REG;


typedef struct
{
    unsigned RD_RDY			: 1;
    unsigned CMD_DONE		: 1;
    unsigned TE_RDY         		: 1;
    unsigned VM_DONE        		: 1;
    unsigned EXT_TE         		: 1;
    unsigned VM_CMD_DONE    	: 1;
    unsigned SLEEPOUT_DONE    : 1;
    unsigned rsv_7				: 24;
    unsigned BUSY          			: 1;
} DSI_INT_STATUS_REG, *PDSI_INT_STATUS_REG;


typedef struct
{
    unsigned DSI_RESET		: 1;
    unsigned DSI_EN			: 1;
    unsigned DPHY_RESET		: 1;
    unsigned rsv_3				: 1;
    unsigned DSI_DUAL_EN		: 1;
    unsigned rsv_5				: 27;
} DSI_COM_CTRL_REG, *PDSI_COM_CTRL_REG;


typedef enum
{
    DSI_CMD_MODE 					= 0,
    DSI_SYNC_PULSE_VDO_MODE 		= 1,
    DSI_SYNC_EVENT_VDO_MODE 		= 2,
    DSI_BURST_VDO_MODE 			= 3
} DSI_MODE_CTRL;


typedef struct
{
    unsigned MODE						: 2;
    unsigned rsv_2	    					: 14;
    unsigned FRM_MODE   				: 1;
    unsigned MIX_MODE   				: 1;
    unsigned V2C_SWITCH_ON   			: 1;
    unsigned C2V_SWITCH_ON   			: 1;
    unsigned SLEEP_MODE   				: 1;
    unsigned rsv_21     					: 11;
} DSI_MODE_CTRL_REG, *PDSI_MODE_CTRL_REG;


typedef enum
{
	ONE_LANE 		= 1,
	TWO_LANE 		= 2,
	THREE_LANE 	= 3,
	FOUR_LANE 		= 4
} DSI_LANE_NUM;


typedef struct
{
    unsigned	VC_NUM				: 2;
    unsigned	LANE_NUM			: 4;
    unsigned	DIS_EOT			: 1;
    unsigned	BLLP_EN			: 1;
    unsigned	TE_FREERUN      		: 1;
    unsigned	EXT_TE_EN   		: 1;
    unsigned	EXT_TE_EDGE     		: 1;
    unsigned	TE_AUTO_SYNC     	: 1;
    unsigned	MAX_RTN_SIZE		: 4;
    unsigned	HSTX_CKLP_EN    	: 1;
    unsigned	TYPE1_BTA_SEL    	: 1;
    unsigned	TE_WITH_CMD_EN    	: 1;
    unsigned	rsv_19				: 13;
} DSI_TXRX_CTRL_REG, *PDSI_TXRX_CTRL_REG;


typedef enum
{
    PACKED_PS_16BIT_RGB565=0,
    LOOSELY_PS_18BIT_RGB666=1,
    PACKED_PS_24BIT_RGB888=2,
    PACKED_PS_18BIT_RGB666=3
} DSI_PS_TYPE;


typedef struct
{
    unsigned 	DSI_PS_WC		: 14;
    unsigned	rsv_14			: 2;
    unsigned	DSI_PS_SEL		: 2;
    unsigned	rsv_18			: 6;
    unsigned  	RGB_SWAP		: 1;
    unsigned	BYTE_SWAP		: 1;
    unsigned	rsv_26			: 6;
} DSI_PSCTRL_REG, *PDSI_PSCTRL_REG;


typedef struct
{
    unsigned 	VSA_NL		: 10;
    unsigned 	rsv_11		: 22;
} DSI_VSA_NL_REG, *PDSI_VSA_NL_REG;


typedef struct
{
    unsigned 	VBP_NL		: 10;
    unsigned 	rsv_11		: 22;
} DSI_VBP_NL_REG, *PDSI_VBP_NL_REG;


typedef struct
{
    unsigned 	VFP_NL		: 10;
    unsigned 	rsv_11		: 22;
} DSI_VFP_NL_REG, *PDSI_VFP_NL_REG;


typedef struct
{
    unsigned 	VACT_NL	: 12;
    unsigned 	rsv_12		: 20;
} DSI_VACT_NL_REG, *PDSI_VACT_NL_REG;


typedef struct
{
    unsigned 	HSA_WC	: 12;
    unsigned 	rsv_12		: 20;
} DSI_HSA_WC_REG, *PDSI_HSA_WC_REG;


typedef struct
{
    unsigned 	HBP_WC	: 12;
    unsigned 	rsv_12		: 20;
} DSI_HBP_WC_REG, *PDSI_HBP_WC_REG;


typedef struct
{
    unsigned 	HFP_WC	: 12;
    unsigned 	rsv_12		: 20;
} DSI_HFP_WC_REG, *PDSI_HFP_WC_REG;

typedef struct
{
    unsigned 	BLLP_WC	: 12;
    unsigned 	rsv_12		: 20;
} DSI_BLLP_WC_REG, *PDSI_BLLP_WC_REG;

typedef struct
{
    unsigned 	CMDQ_SIZE	: 6;
    unsigned 	rsv_6		: 26;
} DSI_CMDQ_CTRL_REG, *PDSI_CMDQ_CTRL_REG;

typedef struct
{
    unsigned char byte0;
    unsigned char byte1;
    unsigned char byte2;
    unsigned char byte3;
} DSI_RX_DATA_REG, *PDSI_RX_DATA_REG;


typedef struct
{
    unsigned DSI_RACK	        : 1;
    unsigned DSI_RACK_BYPASS	: 1;
    unsigned rsv2		: 30;
} DSI_RACK_REG, *PDSI_RACK_REG;


typedef struct
{
    unsigned TRIG0			: 1;//remote rst
    unsigned TRIG1			: 1;//TE
    unsigned TRIG2			: 1;//ack
    unsigned TRIG3			: 1;//rsv
    unsigned RX_ULPS    	: 1;
    unsigned DIRECTION  	: 1;
    unsigned RX_LPDT		: 1;
    unsigned rsv7			: 1;
    unsigned RX_POINTER	: 4;
    unsigned rsv12			: 20;
} DSI_TRIG_STA_REG, *PDSI_TRIG_STA_REG;


typedef struct
{
    unsigned RWMEM_CONTI	: 16;
    unsigned rsv16          : 16;
} DSI_MEM_CONTI_REG, *PDSI_MEM_CONTI_REG;


typedef struct
{
    unsigned FRM_BC		: 21;
    unsigned rsv21		: 11;
} DSI_FRM_BC_REG, *PDSI_FRM_BC_REG;


typedef struct
{
    unsigned PHY_RST	: 1;
    unsigned rsv1		: 4;
    unsigned HTXTO_RST	: 1;
    unsigned LRXTO_RST	: 1;
    unsigned BTATO_RST	: 1;
    unsigned rsv8		: 24;
} DSI_PHY_CON_REG, *PDSI_PHY_CON_REG;


typedef struct
{
    unsigned LC_HS_TX_EN	: 1;
    unsigned LC_ULPM_EN		: 1;
    unsigned LC_WAKEUP_EN	: 1;
    unsigned rsv3			: 29;
} DSI_PHY_LCCON_REG, *PDSI_PHY_LCCON_REG;


typedef struct
{
    unsigned L0_HS_TX_EN		: 1;
    unsigned L0_ULPM_EN		: 1;
    unsigned L0_WAKEUP_EN	: 1;
    unsigned Lx_ULPM_AS_L0	: 1;
    unsigned L0_RX_FILTER_EN	: 1;
    unsigned rsv3				: 27;
} DSI_PHY_LD0CON_REG, *PDSI_PHY_LD0CON_REG;


typedef struct
{
    unsigned char LPX;
    unsigned char HS_PRPR;
    unsigned char HS_ZERO;
    unsigned char HS_TRAIL;
} DSI_PHY_TIMCON0_REG, *PDSI_PHY_TIMCON0_REG;


typedef struct
{
    unsigned char TA_GO;
    unsigned char TA_SURE;
    unsigned char TA_GET;
    unsigned char DA_HS_EXIT;
} DSI_PHY_TIMCON1_REG, *PDSI_PHY_TIMCON1_REG;


typedef struct
{
    unsigned char CONT_DET;
    unsigned char rsv8;
    unsigned char CLK_ZERO;
    unsigned char CLK_TRAIL;
} DSI_PHY_TIMCON2_REG, *PDSI_PHY_TIMCON2_REG;


typedef struct
{
    unsigned char CLK_HS_PRPR;
    unsigned char CLK_HS_POST;
    unsigned char CLK_HS_EXIT;
    unsigned 	  rsv24		: 8;
} DSI_PHY_TIMCON3_REG, *PDSI_PHY_TIMCON3_REG;


typedef struct
{
    unsigned ULPS_WAKEUP	: 20;
    unsigned rsv20			: 12;
} DSI_PHY_TIMCON4_REG, *PDSI_PHY_TIMCON4_REG;


typedef struct
{
    DSI_PHY_TIMCON0_REG	CTRL0;
    DSI_PHY_TIMCON1_REG	CTRL1;
    DSI_PHY_TIMCON2_REG	CTRL2;
    DSI_PHY_TIMCON3_REG	CTRL3;
} DSI_PHY_TIMCON_REG, *PDSI_PHY_TIMCON_REG;


typedef struct
{
    unsigned			CHECK_SUM	: 16;
    unsigned			rsv16       	: 16;
} DSI_CKSM_OUT_REG, *PDSI_CKSM_OUT_REG;


typedef struct
{
    unsigned			DPHY_CTL_STATE_C	: 9;
    unsigned            rsv9        : 7;
    unsigned			DPHY_HS_TX_STATE_C	: 5;
    unsigned 	  	rsv21				: 11;
} DSI_STATE_DBG0_REG, *PDSI_STATE_DBG0_REG;


typedef struct
{
    unsigned			CTL_STATE_C	: 15;
    unsigned 	  	rsv15					: 1;
    unsigned			HS_TX_STATE_0	: 5;
    unsigned			rsv21       	: 3;
    unsigned			ESC_STATE_0			: 8;
} DSI_STATE_DBG1_REG, *PDSI_STATE_DBG1_REG;


typedef struct
{
    unsigned			RX_ESC_STATE			: 10;
    unsigned 	  	rsv10					: 6;
    unsigned			TA_T2R_STATE	: 5;
    unsigned 	  	rsv21					: 3;
    unsigned			TA_R2T_STATE	: 5;
    unsigned 	  	rsv29					: 3;
} DSI_STATE_DBG2_REG, *PDSI_STATE_DBG2_REG;


typedef struct
{
    unsigned			CTL_STATE_1			: 5;
    unsigned 	  	rsv5						: 3;
    unsigned			HS_TX_STATE_1				: 5;
    unsigned 	  	rsv13						: 3;
    unsigned			CTL_STATE_2 : 5;
    unsigned 	  	rsv21						: 3;
    unsigned			HS_TX_STATE_2			: 5;
    unsigned 	  	rsv29						: 3;
} DSI_STATE_DBG3_REG, *PDSI_STATE_DBG3_REG;


typedef struct
{
    unsigned			CTL_STATE_3    	: 5;
    unsigned			rsv5						: 3;
    unsigned			HS_TX_STATE_3	: 5;
    unsigned			rsv13						: 19;
} DSI_STATE_DBG4_REG, *PDSI_STATE_DBG4_REG;


typedef struct
{
    unsigned			WAKEUP_CNT    	: 20;
    unsigned			rsv20						: 8;
    unsigned			WAKEUP_STATE	: 4;
} DSI_STATE_DBG5_REG, *PDSI_STATE_DBG5_REG;


typedef struct
{
    unsigned			CMCTL_STATE    	: 14;
    unsigned			rsv14						: 2;
    unsigned			CMDQ_STATE	: 6;
    unsigned			rsv22						: 10;
} DSI_STATE_DBG6_REG, *PDSI_STATE_DBG6_REG;


typedef struct
{
    unsigned			VMCTL_STATE    	: 11;
    unsigned			rsv11						: 1;
    unsigned			VFP_PERIOD	: 1;
    unsigned			VACT_PERIOD	: 1;
    unsigned			VBP_PERIOD	: 1;
    unsigned			VSA_PERIOD	: 1;
    unsigned			rsv16						: 16;
} DSI_STATE_DBG7_REG, *PDSI_STATE_DBG7_REG;


typedef struct
{
    unsigned			WORD_COUNTER    	: 14;
    unsigned			rsv14						: 18;
} DSI_STATE_DBG8_REG, *PDSI_STATE_DBG8_REG;


typedef struct
{
    unsigned			LINE_COUNTER    	: 22;
    unsigned			rsv22						: 10;
} DSI_STATE_DBG9_REG, *PDSI_STATE_DBG9_REG;


typedef struct
{
    unsigned			DEBUG_OUT_SEL    	: 5;
    unsigned			rsv5						: 27;
} DSI_DEBUG_SEL_REG, *PDSI_DEBUG_SEL_REG;


typedef struct
{
    unsigned            BIST_MODE           : 1;
    unsigned            BIST_ENABLE         : 1;
    unsigned            BIST_FIX_PATTERN    : 1;
    unsigned            BIST_SPC_PATTERN    : 1;
    unsigned            BIST_HS_FREE        : 1;
    unsigned	      rsv_05			:1;
    unsigned            SELF_PAT_MODE           : 1;
    unsigned            rsv_07          : 1;
    unsigned            BIST_LANE_NUM       : 4;
    unsigned            rsv12               : 4;
    unsigned            BIST_TIMING         : 8;
    unsigned            rsv24         : 8;
}DSI_BIST_CON_REG, *PDSI_BIST_CON_REG;

typedef struct
{
    unsigned			VM_CMD_EN			:1;
    unsigned			LONG_PKT			:1;
    unsigned			TIME_SEL			:1;
    unsigned			TS_VSA_EN			:1;
    unsigned			TS_VBP_EN			:1;
    unsigned			TS_VFP_EN			:1;
    unsigned			rsv6				:2;
    unsigned			CM_DATA_ID			:8;
    unsigned			CM_DATA_0			:8;
    unsigned			CM_DATA_1			:8;
}DSI_VM_CMD_CON_REG, *PDSI_VM_CMD_CON_REG;

typedef struct
{
    unsigned			_3D_MODE			:2;
    unsigned			_3D_FMT				:2;
    unsigned			_3D_VSYNC			:1;
    unsigned			_3D_LR				:1;
    unsigned			_3D_EN				:1;
    unsigned			rsv08				:25;
}DSI_3D_CON_REG, *PDSI_3D_CON_REG;


typedef struct
{
    DSI_START_REG				DSI_START;				// 0000
    DSI_STATUS_REG  			DSI_STA;				// 0004
    DSI_INT_ENABLE_REG		DSI_INTEN;				// 0008
    DSI_INT_STATUS_REG		DSI_INTSTA;			// 000C
    DSI_COM_CTRL_REG			DSI_COM_CTRL;			// 0010
    DSI_MODE_CTRL_REG		DSI_MODE_CTRL;		// 0014
    DSI_TXRX_CTRL_REG			DSI_TXRX_CTRL;		// 0018
    DSI_PSCTRL_REG			DSI_PSCTRL;			// 001C
    DSI_VSA_NL_REG			DSI_VSA_NL;			// 0020
    DSI_VBP_NL_REG			DSI_VBP_NL;			// 0024
    DSI_VFP_NL_REG			DSI_VFP_NL;			// 0028
    DSI_VACT_NL_REG			DSI_VACT_NL;			// 002C
    UINT32                  			rsv_30[8];               		// 0030..004C
    DSI_HSA_WC_REG			DSI_HSA_WC;			// 0050
    DSI_HBP_WC_REG			DSI_HBP_WC;			// 0054
    DSI_HFP_WC_REG			DSI_HFP_WC;			// 0058
    DSI_BLLP_WC_REG			DSI_BLLP_WC;			// 005C

    DSI_CMDQ_CTRL_REG		DSI_CMDQ_SIZE;		// 0060
    UINT32                  			DSI_HSTX_CKL_WC;    	// 0064
    UINT32                  			DSI_HSTX_CKL_WC_AUTO_RESULT;    	// 0068
    UINT32					rsv_006C[2];      			// 0068..0070
    DSI_RX_DATA_REG			DSI_RX_DATA0;			// 0074
    DSI_RX_DATA_REG			DSI_RX_DATA1;			// 0078
    DSI_RX_DATA_REG			DSI_RX_DATA2;			// 007c
    DSI_RX_DATA_REG			DSI_RX_DATA3;			// 0080
    DSI_RACK_REG				DSI_RACK;				// 0084
    DSI_TRIG_STA_REG			DSI_TRIG_STA;			// 0088
    UINT32   	        			rsv_008C;      			// 008C
    DSI_MEM_CONTI_REG		DSI_MEM_CONTI;		// 0090
    DSI_FRM_BC_REG			DSI_FRM_BC;			// 0094
    DSI_3D_CON_REG			DSI_3D_CON;			// 0098

    UINT32   	        			rsv_009C[25];     			// 0104..0100
    UINT32					DSI_PHY_PCPAT;		// 00100

    DSI_PHY_LCCON_REG		DSI_PHY_LCCON;		// 0104
    DSI_PHY_LD0CON_REG		DSI_PHY_LD0CON;		// 0108
    UINT32   	        			rsv_010C;      			// 010C
    DSI_PHY_TIMCON0_REG		DSI_PHY_TIMECON0;	// 0110
    DSI_PHY_TIMCON1_REG		DSI_PHY_TIMECON1;	// 0114
    DSI_PHY_TIMCON2_REG		DSI_PHY_TIMECON2;	// 0118
    DSI_PHY_TIMCON3_REG		DSI_PHY_TIMECON3;	// 011C
    DSI_PHY_TIMCON4_REG		DSI_PHY_TIMECON4;	// 0120
    UINT32   	        			rsv_0124[3];      			// 0124..012c
    DSI_VM_CMD_CON_REG      	DSI_VM_CMD_CON;		//0130
    UINT32                  			DSI_VM_CMD_DATA0;    	// 0134
    UINT32                  			DSI_VM_CMD_DATA4;    	// 0138
    UINT32                  			DSI_VM_CMD_DATA8;    	// 013C
    UINT32                  			DSI_VM_CMD_DATAC;    	// 0140
    DSI_CKSM_OUT_REG			DSI_CKSM_OUT;			// 0144
    DSI_STATE_DBG0_REG		DSI_STATE_DBG0;		// 0148
    DSI_STATE_DBG1_REG		DSI_STATE_DBG1;		// 014C
    DSI_STATE_DBG2_REG		DSI_STATE_DBG2;		// 0150
    DSI_STATE_DBG3_REG		DSI_STATE_DBG3;		// 0154
    DSI_STATE_DBG4_REG		DSI_STATE_DBG4;		// 0158
    DSI_STATE_DBG5_REG		DSI_STATE_DBG5;		// 015C
    DSI_STATE_DBG6_REG		DSI_STATE_DBG6;		// 0160
    DSI_STATE_DBG7_REG		DSI_STATE_DBG7;		// 0164
    DSI_STATE_DBG8_REG		DSI_STATE_DBG8;		// 0168
    DSI_STATE_DBG9_REG		DSI_STATE_DBG9;		// 016C
    DSI_DEBUG_SEL_REG			DSI_DEBUG_SEL;		// 0170
    UINT32    				DSI_SWAP_PORT;                		// 0174
    UINT32    					DSI_BIST_PATTERN;                  	// 0178
    DSI_BIST_CON_REG 			DSI_BIST_CON;                          	// 017C
} volatile DSI_REGS, *PDSI_REGS;


typedef struct
{
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
} DSI_CMDQ, *PDSI_CMDQ;

typedef struct
{
	DSI_CMDQ data[128];
} DSI_CMDQ_REGS, *PDSI_CMDQ_REGS;

typedef struct
{
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
} DSI_VM_CMDQ, *PDSI_VM_CMDQ;

typedef struct
{
	DSI_VM_CMDQ data[4];
} DSI_VM_CMDQ_REGS, *PDSI_VM_CMDQ_REGS;

typedef struct
{
    MIPITX_DSI_CON_REG				MIPITX_DSI_CON;				// 0000
    MIPITX_DSI_CLOCK_LANE_REG			MIPITX_DSI_CLOCK_LANE;		// 0004
    MIPITX_DSI_DATA_LANE0_REG			MIPITX_DSI_DATA_LANE0;		// 0008
    MIPITX_DSI_DATA_LANE1_REG			MIPITX_DSI_DATA_LANE1;		// 000C
    MIPITX_DSI_DATA_LANE2_REG			MIPITX_DSI_DATA_LANE2;		// 0010
    MIPITX_DSI_DATA_LANE3_REG			MIPITX_DSI_DATA_LANE3;		// 0014
    UINT32                  					rsv_18[10];               				// 0018..003C

    MIPITX_DSI_TOP_CON_REG			MIPITX_DSI_TOP_CON;			// 0040
    MIPITX_DSI_BG_CON_REG				MIPITX_DSI_BG_CON;			// 0044
    UINT32                  					rsv_48[2];						// 0048..004C
    MIPITX_DSI_PLL_CON0_REG			MIPITX_DSI_PLL_CON0;			// 0050
    MIPITX_DSI_PLL_CON1_REG			MIPITX_DSI_PLL_CON1;			// 0054
    MIPITX_DSI_PLL_CON2_REG			MIPITX_DSI_PLL_CON2;			// 0058
    MIPITX_DSI_PLL_CON3_REG			MIPITX_DSI_PLL_CON3;			// 005C
    MIPITX_DSI_PLL_CHG_REG			MIPITX_DSI_PLL_CHG;			// 0060
    MIPITX_DSI_PLL_TOP_REG				MIPITX_DSI_PLL_TOP;			// 0064
    MIPITX_DSI_PLL_PWR_REG			MIPITX_DSI_PLL_PWR;			// 0068
    UINT32                  					rsv_6C;               					// 006C
    MIPITX_DSI_RGS_REG				MIPITX_DSI_RGS;				// 0070
    MIPITX_DSI_GPI_EN_REG				MIPITX_DSI_GPI_EN;			// 0074
    MIPITX_DSI_GPI_PULL_REG			MIPITX_DSI_GPI_PULL;			// 0078
    MIPITX_DSI_PHY_SEL_REG				MIPITX_DSI_PHY_SEL;			// 007C

    MIPITX_DSI_SW_CTRL_REG			MIPITX_DSI_SW_CTRL_EN;		// 0080
    MIPITX_DSI_SW_CTRL_CON0_REG		MIPITX_DSI_SW_CTRL_CON0;	// 0084
    MIPITX_DSI_SW_CTRL_CON1_REG		MIPITX_DSI_SW_CTRL_CON1;	// 0088
    MIPITX_DSI_SW_CTRL_CON2_REG		MIPITX_DSI_SW_CTRL_CON2;	// 008C
    MIPITX_DSI_DBG_CON_REG			MIPITX_DSI_DBG_CON;			// 0090
    UINT32							MIPITX_DSI_DBG_OUT;			// 0084
    MIPITX_DSI_APB_ASYNC_STA_REG		MIPITX_DSI_APB_ASYNC_STA;	// 0098

} volatile DSI_PHY_REGS, *PDSI_PHY_REGS;

#ifndef BUILD_LK
STATIC_ASSERT(0x0050 == offsetof(DSI_PHY_REGS, MIPITX_DSI_PLL_CON0));
STATIC_ASSERT(0x0070 == offsetof(DSI_PHY_REGS, MIPITX_DSI_RGS));
STATIC_ASSERT(0x0080 == offsetof(DSI_PHY_REGS, MIPITX_DSI_SW_CTRL_EN));
STATIC_ASSERT(0x0090 == offsetof(DSI_PHY_REGS, MIPITX_DSI_DBG_CON));
STATIC_ASSERT(0x002C == offsetof(DSI_REGS, DSI_VACT_NL));
STATIC_ASSERT(0x0104 == offsetof(DSI_REGS, DSI_PHY_LCCON));
STATIC_ASSERT(0x011C == offsetof(DSI_REGS, DSI_PHY_TIMECON3));
STATIC_ASSERT(0x017C == offsetof(DSI_REGS, DSI_BIST_CON));
STATIC_ASSERT(0x0100 == offsetof(DSI_REGS, DSI_PHY_PCPAT));


STATIC_ASSERT(0x0098 == offsetof(DSI_REGS, DSI_3D_CON));
#endif



extern unsigned long dispsys_reg[DISP_REG_NUM];
extern unsigned int ddp_reg_pa_base[DISP_REG_NUM];
extern unsigned long mipi_tx_reg;
extern unsigned long dsi_reg_va;

// after apply device tree va/pa is not mapped by a fixed offset
static inline unsigned long disp_addr_convert(unsigned long va) 
{ 
  unsigned int i = 0;
  for(i=0;i<DISP_REG_NUM;i++)
  {
      if(dispsys_reg[i]==(va&(~0xfffl)))
      {
        // xlog_printk(ANDROID_LOG_ERROR, "DDP", "va=0x%x, pa=0x%x! \n", va,  (ddp_reg_pa_base[i]+(va&0xfffl)));
        return (ddp_reg_pa_base[i]+(va&0xfffl));
     }
  }
  xlog_printk(ANDROID_LOG_ERROR, "DDP", "can not find reg addr for va=0x%x! \n", va);
  return 0;
}

// DTS will assign reigister address dynamically, so can not define to 0x1000
//#define DISP_INDEX_OFFSET 0x1000
#define DISP_RDMA_INDEX_OFFSET (dispsys_reg[DISP_REG_RDMA1] - dispsys_reg[DISP_REG_RDMA0])
#define DISP_OVL_INDEX_OFFSET  (dispsys_reg[DISP_REG_OVL1] - dispsys_reg[DISP_REG_OVL0])
#define DISP_WDMA_INDEX_OFFSET (dispsys_reg[DISP_REG_WDMA1] - dispsys_reg[DISP_REG_WDMA0])
#define DISP_SPLIT_INDEX_OFFSET (dispsys_reg[DISP_REG_SPLIT1] - dispsys_reg[DISP_REG_SPLIT0])
#define DISP_COLOR_INDEX_OFFSET (dispsys_reg[DISP_REG_COLOR1] - dispsys_reg[DISP_REG_COLOR0])
#define DISP_DSI_INDEX_OFFSET (dispsys_reg[DISP_REG_DSI1] - dispsys_reg[DISP_REG_DSI0])

#define DDP_REG_BASE_MMSYS_CONFIG	(dispsys_reg[DISP_REG_CONFIG		])
#define DDP_REG_BASE_DISP_OVL0   	(dispsys_reg[DISP_REG_OVL0		    ])
#define DDP_REG_BASE_DISP_OVL1   	(dispsys_reg[DISP_REG_OVL1		    ])
#define DDP_REG_BASE_DISP_RDMA0  	(dispsys_reg[DISP_REG_RDMA0		    ])
#define DDP_REG_BASE_DISP_RDMA1  	(dispsys_reg[DISP_REG_RDMA1		    ])
#define DDP_REG_BASE_DISP_RDMA2  	(dispsys_reg[DISP_REG_RDMA2		    ])
#define DDP_REG_BASE_DISP_WDMA0  	(dispsys_reg[DISP_REG_WDMA0		    ])
#define DDP_REG_BASE_DISP_WDMA1  	(dispsys_reg[DISP_REG_WDMA1		    ])
#define DDP_REG_BASE_DISP_COLOR0 	(dispsys_reg[DISP_REG_COLOR0		])
#define DDP_REG_BASE_DISP_COLOR1 	(dispsys_reg[DISP_REG_COLOR1		])
#define DDP_REG_BASE_DISP_AAL    	(dispsys_reg[DISP_REG_AAL		    ])
#define DDP_REG_BASE_DISP_GAMMA  	(dispsys_reg[DISP_REG_GAMMA		    ])
#define DDP_REG_BASE_DISP_MERGE  	(dispsys_reg[DISP_REG_MERGE		    ])
#define DDP_REG_BASE_DISP_SPLIT0 	(dispsys_reg[DISP_REG_SPLIT0		])
#define DDP_REG_BASE_DISP_SPLIT1 	(dispsys_reg[DISP_REG_SPLIT1		])
#define DDP_REG_BASE_DISP_UFOE   	(dispsys_reg[DISP_REG_UFOE		    ])
#define DDP_REG_BASE_DSI0        	(dispsys_reg[DISP_REG_DSI0		    ])
#define DDP_REG_BASE_DSI1        	(dispsys_reg[DISP_REG_DSI1		    ])
#define DDP_REG_BASE_DPI         	(dispsys_reg[DISP_REG_DPI		    ])
#define DDP_REG_BASE_DISP_PWM0   	(dispsys_reg[DISP_REG_PWM0		    ])
#define DDP_REG_BASE_DISP_PWM1   	(dispsys_reg[DISP_REG_PWM1		    ])
#define DDP_REG_BASE_MM_MUTEX    	(dispsys_reg[DISP_REG_MM_MUTEX	    ])
#define DDP_REG_BASE_SMI_LARB0   	(dispsys_reg[DISP_REG_SMI_LARB0	    ])
#define DDP_REG_BASE_SMI_COMMON  	(dispsys_reg[DISP_REG_SMI_COMMON	])
#define DDP_REG_BASE_DISP_OD     	(dispsys_reg[DISP_REG_OD			])

#define DDP_REG_BASE_MIPITX0     	(dispsys_reg[DISP_REG_MIPITX0])
#define DDP_REG_BASE_MIPITX1     	(dispsys_reg[DISP_REG_MIPITX1])

//#define MIPI_TX_REG_BASE			(mipi_tx_reg)


#if 0
/* TDODO: get base reg addr from system header */
#define DDP_REG_BASE_MMSYS_CONFIG MMSYS_CONFIG_BASE	/* 0xf4000000 */
#define DDP_REG_BASE_DISP_OVL0    OVL0_BASE	/* 0xf400C000 */
#define DDP_REG_BASE_DISP_OVL1    OVL1_BASE	/* 0xf400D000 */
#define DDP_REG_BASE_DISP_RDMA0   DISP_RDMA0_BASE	/* 0xf400E000 */
#define DDP_REG_BASE_DISP_RDMA1   DISP_RDMA1_BASE	/* 0xf400F000 */
#define DDP_REG_BASE_DISP_RDMA2   DISP_RDMA2_BASE	/* 0xf4010000 */
#define DDP_REG_BASE_DISP_WDMA0   DISP_WDMA0_BASE	/* 0xf4011000 */
#define DDP_REG_BASE_DISP_WDMA1   DISP_WDMA1_BASE	/* 0xf4012000 */
#define DDP_REG_BASE_DISP_COLOR0  COLOR0_BASE	/* 0xf4013000 */
#define DDP_REG_BASE_DISP_COLOR1  COLOR1_BASE	/* 0xf4014000 */
#define DDP_REG_BASE_DISP_AAL     DISP_AAL_BASE	/* 0xf4015000 */
#define DDP_REG_BASE_DISP_GAMMA   DISP_GAMMA_BASE	/* 0xf4016000 */
#define DDP_REG_BASE_DISP_MERGE   DISP_MERGE_BASE	/* 0xf4017000 */
#define DDP_REG_BASE_DISP_SPLIT0  DISP_SPLIT0_BASE	/* 0xf4018000 */
#define DDP_REG_BASE_DISP_SPLIT1  DISP_SPLIT1_BASE	/* 0xf4019000 */
#define DDP_REG_BASE_DISP_UFOE    DISP_UFOE_BASE	/* 0xf401A000 */
#define DDP_REG_BASE_DSI0         DSI0_BASE	/* 0xf401B000 */
#define DDP_REG_BASE_DSI1         DSI1_BASE	/* 0xf401C000 */
#define DDP_REG_BASE_DPI          DPI_BASE	/* 0xf401D000 */
#define DDP_REG_BASE_DISP_PWM0    DISP_PWM0_BASE	/* 0xf401E000 */
#define DDP_REG_BASE_DISP_PWM1    DISP_PWM1_BASE	/* 0xf401F000 */
#define DDP_REG_BASE_MM_MUTEX     MM_MUTEX_BASE	/* 0xf4020000 */
#define DDP_REG_BASE_SMI_LARB0    SMI_LARB0_BASE	/* 0xf4021000 */
#define DDP_REG_BASE_SMI_COMMON   SMI_COMMON_BASE	/* 0xf4022000 */
#define DDP_REG_BASE_DISP_OD      DISP_OD_BASE	/* 0xf4023000 */
#endif

#define DISPSYS_REG_ADDR_MIN 		DDP_REG_BASE_MMSYS_CONFIG
#define DISPSYS_REG_ADDR_MAX 		DDP_REG_BASE_DISP_OD

#define DISPSYS_CONFIG_BASE	         	DDP_REG_BASE_MMSYS_CONFIG
#define DISPSYS_OVL0_BASE		         	DDP_REG_BASE_DISP_OVL0
#define DISPSYS_OVL1_BASE		         	DDP_REG_BASE_DISP_OVL1
#define DISPSYS_RDMA0_BASE		       DDP_REG_BASE_DISP_RDMA0
#define DISPSYS_RDMA1_BASE		       DDP_REG_BASE_DISP_RDMA1
#define DISPSYS_RDMA2_BASE		       DDP_REG_BASE_DISP_RDMA2
#define DISPSYS_WDMA0_BASE		       DDP_REG_BASE_DISP_WDMA0
#define DISPSYS_WDMA1_BASE		       DDP_REG_BASE_DISP_WDMA1
#define DISPSYS_COLOR0_BASE		       DDP_REG_BASE_DISP_COLOR0
#define DISPSYS_COLOR1_BASE		       DDP_REG_BASE_DISP_COLOR1
#define DISPSYS_AAL_BASE		        	DDP_REG_BASE_DISP_AAL
#define DISPSYS_GAMMA_BASE		       DDP_REG_BASE_DISP_GAMMA
#define DISPSYS_MERGE_BASE		      		DDP_REG_BASE_DISP_MERGE
#define DISPSYS_SPLIT0_BASE		       DDP_REG_BASE_DISP_SPLIT0
#define DISPSYS_SPLIT1_BASE		       DDP_REG_BASE_DISP_SPLIT1
#define DISPSYS_UFOE_BASE		         	DDP_REG_BASE_DISP_UFOE
#define DISPSYS_DSI0_BASE		         	DDP_REG_BASE_DSI0
#define DISPSYS_DSI1_BASE		         	DDP_REG_BASE_DSI1
#define DISPSYS_DPI_BASE	           		DDP_REG_BASE_DPI
#define DISPSYS_PWM0_BASE		         	DDP_REG_BASE_DISP_PWM0
#define DISPSYS_PWM1_BASE		         	DDP_REG_BASE_DISP_PWM1
#define DISPSYS_MUTEX_BASE		       	DDP_REG_BASE_MM_MUTEX
#define DISPSYS_SMI_LARB0_BASE		DDP_REG_BASE_SMI_LARB0
#define DISPSYS_SMI_COMMON_BASE		DDP_REG_BASE_SMI_COMMON
#define DISPSYS_OD_BASE             DDP_REG_BASE_DISP_OD

#if 0
/* --------------------------------------------------------------------------- */
/* Type Casting */
/* --------------------------------------------------------------------------- */

#define AS_INT32(x)     (*(INT32 *)(x))
#define AS_INT16(x)     (*(INT16 *)(x))
#define AS_INT8(x)      (*(INT8  *)(x))

#define AS_UINT32(x)    (*(UINT32 *)(x))
#define AS_UINT16(x)    (*(UINT16 *)(x))
#define AS_UINT8(x)     (*(UINT8  *)(x))
#endif

/* --------------------------------------------------------------------------- */
/* Register Manipulations */
/* --------------------------------------------------------------------------- */

#define READ_REGISTER_UINT32(reg) \
    (*(volatile UINT32 * const)(reg))

#define WRITE_REGISTER_UINT32(reg, val) \
    (*(volatile UINT32 * const)(reg)) = (val)

#define READ_REGISTER_UINT16(reg) \
    (*(volatile UINT16 * const)(reg))

#define WRITE_REGISTER_UINT16(reg, val) \
    (*(volatile UINT16 * const)(reg)) = (val)

#define READ_REGISTER_UINT8(reg) \
    (*(volatile UINT8 * const)(reg))

#define WRITE_REGISTER_UINT8(reg, val) \
    (*(volatile UINT8 * const)(reg)) = (val)

#if 0
#define INREG8(x)           READ_REGISTER_UINT8((UINT8*)(x))
#define OUTREG8(x, y)       WRITE_REGISTER_UINT8((UINT8*)(x), (UINT8)(y))
#define SETREG8(x, y)       OUTREG8(x, INREG8(x)|(y))
#define CLRREG8(x, y)       OUTREG8(x, INREG8(x)&~(y))
#define MASKREG8(x, y, z)   OUTREG8(x, (INREG8(x)&~(y))|(z))

#define INREG16(x)          READ_REGISTER_UINT16((UINT16*)(x))
#define OUTREG16(x, y)      WRITE_REGISTER_UINT16((UINT16*)(x),(UINT16)(y))
#define SETREG16(x, y)      OUTREG16(x, INREG16(x)|(y))
#define CLRREG16(x, y)      OUTREG16(x, INREG16(x)&~(y))
#define MASKREG16(x, y, z)  OUTREG16(x, (INREG16(x)&~(y))|(z))

#define INREG32(x)          READ_REGISTER_UINT32((UINT32*)(x))
#define OUTREG32(x, y)      WRITE_REGISTER_UINT32((UINT32*)(x), (UINT32)(y))
#define SETREG32(x, y)      OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)      OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)  OUTREG32(x, (INREG32(x)&~(y))|(z))
#endif

// ---------------------------------------------------------------------------
//  Register Field Access
// ---------------------------------------------------------------------------

#define REG_FLD(width, shift) \
    ((unsigned int)((((width) & 0xFF) << 16) | ((shift) & 0xFF)))

#define REG_FLD_WIDTH(field) \
    ((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) \
    ((unsigned int)((field) & 0xFF))

#define REG_FLD_MASK(field) \
    (((unsigned int)(1 << REG_FLD_WIDTH(field)) - 1) << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val) \
    (((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define DISP_REG_GET(reg32) __raw_readl((void*)(reg32))
#define DISP_REG_GET_FIELD(field, reg32) ((*(volatile unsigned int*)(reg32) & REG_FLD_MASK(field)) >> REG_FLD_SHIFT(field))

// polling register until masked bit is 1
#define DDP_REG_POLLING(reg32, mask) \
    do { \
            while(!((DISP_REG_GET(reg32))&mask)) ; \
    } while (0)

// Polling register until masked bit is 0
#define DDP_REG_POLLING_NEG(reg32, mask) \
    do { \
          while((DISP_REG_GET(reg32))&mask) ; \
    } while (0)

#define DISP_CPU_REG_SET(reg32, val) \
	do{\
		mt_reg_sync_writel(val, (volatile unsigned int*)(reg32));\
		}while(0)

#define DISP_CPU_REG_SET_FIELD(field, reg32, val)  \
    do {                                \
           mt_reg_sync_writel( (*(volatile unsigned int*)(reg32) & ~REG_FLD_MASK(field))|REG_FLD_VAL((field), (val)), reg32);  \
    } while (0)


#define DISP_REG_MASK(handle, reg32, val, mask) 	\
	do { \
		dprec_reg_op(handle, reg32, val, mask);\
		 if(handle==NULL) \
		 { \
			   mt_reg_sync_writel((unsigned int)(INREG32(reg32)&~(mask))|(val),(volatile void*)(reg32) );\
		 } \
		 else \
		 { \
			   cmdqRecWrite(handle, disp_addr_convert(reg32), val, mask); \
		 }	\
	} while (0)

#define DISP_REG_SET(handle,reg32, val) \
        do { \
		     dprec_reg_op(handle, (reg32), val, 0x00000000);\
             if(handle==0) \
             { \
			mt_reg_sync_writel(val, (volatile void*)(reg32));\
             } \
             else \
             { \
			cmdqRecWrite(handle, disp_addr_convert(reg32), val, ~0); \
             }  \
        } while (0)


#define DISP_REG_SET_FIELD(handle,field, reg32, val)  \
        do {  \
		     dprec_reg_op(handle, (unsigned int)(reg32), val<<REG_FLD_SHIFT(field), REG_FLD_MASK(field));\
             if(handle==0)  \
             { \
			 mt_reg_sync_writel( (__raw_readl(reg32) & ~REG_FLD_MASK(field))|REG_FLD_VAL((field), (val)), (reg32));  \
             } \
             else \
             { \
			cmdqRecWrite(handle, disp_addr_convert(reg32), val<<REG_FLD_SHIFT(field), REG_FLD_MASK(field)); \
             } \
        } while (0)


// TODO: waiting for CMDQ enabled
#if 1
/* Helper macros for local command queue */
#define DISP_CMDQ_BEGIN(__cmdq, scenario) { \
    cmdqRecHandle __cmdq; \
    cmdqRecCreate(scenario, &__cmdq);\
    cmdqRecReset(__cmdq);\
    ddp_insert_config_allow_rec(__cmdq);


#define DISP_CMDQ_REG_SET(__cmdq, reg32, val, mask) \
	 DISP_REG_MASK(__cmdq,reg32,val,mask);

#define DISP_CMDQ_CONFIG_STREAM_DIRTY(__cmdq)	\
	 ddp_insert_config_dirty_rec(__cmdq);\

#define DISP_CMDQ_END(__cmdq) \
	 cmdqRecFlush(__cmdq); \
	 cmdqRecDestroy(__cmdq); \
	 }
#else
/* Helper macros for local command queue */
#define DISP_CMDQ_BEGIN(__cmdq, scenario) { cmdqRecHandle __cmdq=NULL;

#define DISP_CMDQ_REG_SET(__cmdq, reg32, val, mask)  \
	do{\
		unsigned int ori_val = DISP_REG_GET(reg32);\
		DISP_REG_SET(NULL, reg32, (ori_val & ~(mask))|(val));\
		}while(0)

#define DISP_CMDQ_CONFIG_STREAM_DIRTY(__cmdq)	{}

#define DISP_CMDQ_END(__cmdq) }

#endif

// field definition
// -------------------------------------------------------------
// AAL
#define DISP_AAL_EN                             (DISPSYS_AAL_BASE + 0x000)
#define DISP_AAL_RST                            (DISPSYS_AAL_BASE + 0x004)
#define DISP_AAL_INTEN                          (DISPSYS_AAL_BASE + 0x008)
#define DISP_AAL_INTSTA                         (DISPSYS_AAL_BASE + 0x00c)
#define DISP_AAL_STATUS                         (DISPSYS_AAL_BASE + 0x010)
#define DISP_AAL_CFG                            (DISPSYS_AAL_BASE + 0x020)
#define DISP_AAL_IPUT                           (DISPSYS_AAL_BASE + 0x024)
#define DISP_AAL_OPUT                           (DISPSYS_AAL_BASE + 0x028)

#define DISP_AAL_SIZE                           (DISPSYS_AAL_BASE + 0x030)
#define DISP_AAL_CCORR(idx)                     (DISPSYS_AAL_BASE + 0x080 + (idx) * 4)
#define DISP_AAL_CABC_00                        (DISPSYS_AAL_BASE + 0x20c)
#define DISP_AAL_CABC_02                        (DISPSYS_AAL_BASE + 0x214)
#define DISP_AAL_STATUS_00                      (DISPSYS_AAL_BASE + 0x224)
/* 00 ~ 32: max histogram */
#define DISP_AAL_STATUS_32                      (DISPSYS_AAL_BASE + 0x2a4)
/* bit 8: dre_gain_force_en */
#define DISP_AAL_DRE_GAIN_FILTER_00             (DISPSYS_AAL_BASE + 0x354)
#define DISP_AAL_DRE_FLT_FORCE(idx)             (DISPSYS_AAL_BASE + 0x358 + (idx) * 4)
#define DISP_AAL_DRE_CRV_CAL_00                 (DISPSYS_AAL_BASE + 0x344)
#define DISP_AAL_DRE_MAPPING_00                 (DISPSYS_AAL_BASE + 0x3b0)
#define DISP_AAL_CABC_GAINLMT_TBL(idx)          (DISPSYS_AAL_BASE + 0x40c + (idx) * 4)
#define DISP_AAL_GAMMA_LUT                      (DISPSYS_AAL_BASE + 0x700)


#define DISP_PWM_EN_OFF                         (0x00)
#define DISP_PWM_COMMIT_OFF                     (0x08)
#define DISP_PWM_CON_0_OFF                      (0x10)
#define DISP_PWM_CON_1_OFF                      (0x14)


// field definition
// -------------------------------------------------------------
// DISP OD
#define DISP_OD_EN                              (DISPSYS_OD_BASE + 0x000)
#define DISP_OD_RESET                           (DISPSYS_OD_BASE + 0x004)
#define DISP_OD_INTEN                           (DISPSYS_OD_BASE + 0x008)
#define DISP_OD_INTS                            (DISPSYS_OD_BASE + 0x00C)
#define DISP_OD_STATUS                          (DISPSYS_OD_BASE + 0x010)
#define DISP_OD_CFG                             (DISPSYS_OD_BASE + 0x020)
#define DISP_OD_INPUT_COUNT                     (DISPSYS_OD_BASE + 0x024)
#define DISP_OD_OUTPUT_COUNT                    (DISPSYS_OD_BASE + 0x028)
#define DISP_OD_CHKS_UM                         (DISPSYS_OD_BASE + 0x02c)
#define DISP_OD_SIZE                            (DISPSYS_OD_BASE + 0x030)
#define DISP_OD_HSYNC_WIDTH                     (DISPSYS_OD_BASE + 0x040)
#define DISP_OD_VSYNC_WIDTH                     (DISPSYS_OD_BASE + 0x044)
#define DISP_OD_MISC                            (DISPSYS_OD_BASE + 0x048)

// -------------------------------------------------------------
// COLOR
#define CFG_MAIN_FLD_M_REG_RESET                            			REG_FLD(1, 31)
#define CFG_MAIN_FLD_M_DISP_RESET                            		REG_FLD(1, 30)
#define CFG_MAIN_FLD_COLOR_DBUF_EN                            		REG_FLD(1, 29)
#define CFG_MAIN_FLD_C_PP_CM_DBG_SEL                            		REG_FLD(4, 16)
#define CFG_MAIN_FLD_SEQ_SEL                            				REG_FLD(1, 13)
#define CFG_MAIN_FLD_ALLBP                           					REG_FLD(1, 7)
#define CFG_MAIN_FLD_HEBP                            					REG_FLD(1, 4)
#define CFG_MAIN_FLD_SEBP                            					REG_FLD(1, 3)
#define CFG_MAIN_FLD_YEBP                            					REG_FLD(1, 2)
#define CFG_MAIN_FLD_P2CBP                            					REG_FLD(1, 1)
#define CFG_MAIN_FLD_C2PBP                            					REG_FLD(1, 0)
#define START_FLD_DISP_COLOR_START                      			REG_FLD(1, 0)

#define DISP_COLOR_CFG_MAIN             (DISPSYS_COLOR0_BASE+0x400)
#define DISP_COLOR_PXL_MAIN             (DISPSYS_COLOR0_BASE+0x404)
#define DISP_COLOR_LNE_MAIN             (DISPSYS_COLOR0_BASE+0x408)
#define DISP_COLOR_WIN_X_MAIN           (DISPSYS_COLOR0_BASE+0x40C)
#define DISP_COLOR_WIN_Y_MAIN           (DISPSYS_COLOR0_BASE+0x410)
#define DISP_COLOR_DBG_CFG_MAIN         (DISPSYS_COLOR0_BASE+0x420)
#define DISP_COLOR_C_BOOST_MAIN         (DISPSYS_COLOR0_BASE+0x428)
#define DISP_COLOR_C_BOOST_MAIN_2       (DISPSYS_COLOR0_BASE+0x42C)
#define DISP_COLOR_G_PIC_ADJ_MAIN_1     (DISPSYS_COLOR0_BASE+0x434)
#define DISP_COLOR_G_PIC_ADJ_MAIN_2     (DISPSYS_COLOR0_BASE+0x438)
#define DISP_COLOR_Y_SLOPE_1_0_MAIN     (DISPSYS_COLOR0_BASE+0x4A0)
#define DISP_COLOR_LOCAL_HUE_CD_0       (DISPSYS_COLOR0_BASE+0x620)
#define DISP_COLOR_TWO_D_WINDOW_1       (DISPSYS_COLOR0_BASE+0x740)
#define DISP_COLOR_TWO_D_W1_RESULT      (DISPSYS_COLOR0_BASE+0x74C)
#define DISP_COLOR_PART_SAT_GAIN1_0     (DISPSYS_COLOR0_BASE+0x7FC)
#define DISP_COLOR_PART_SAT_GAIN1_1     (DISPSYS_COLOR0_BASE+0x800)
#define DISP_COLOR_PART_SAT_GAIN1_2     (DISPSYS_COLOR0_BASE+0x804)
#define DISP_COLOR_PART_SAT_GAIN1_3     (DISPSYS_COLOR0_BASE+0x808)
#define DISP_COLOR_PART_SAT_GAIN1_4     (DISPSYS_COLOR0_BASE+0x80C)
#define DISP_COLOR_PART_SAT_GAIN2_0     (DISPSYS_COLOR0_BASE+0x810)
#define DISP_COLOR_PART_SAT_GAIN2_1     (DISPSYS_COLOR0_BASE+0x814)
#define DISP_COLOR_PART_SAT_GAIN2_2     (DISPSYS_COLOR0_BASE+0x818)
#define DISP_COLOR_PART_SAT_GAIN2_3	    (DISPSYS_COLOR0_BASE+0x81C)
#define DISP_COLOR_PART_SAT_GAIN2_4     (DISPSYS_COLOR0_BASE+0x820)
#define DISP_COLOR_PART_SAT_GAIN3_0     (DISPSYS_COLOR0_BASE+0x824)
#define DISP_COLOR_PART_SAT_GAIN3_1     (DISPSYS_COLOR0_BASE+0x828)
#define DISP_COLOR_PART_SAT_GAIN3_2     (DISPSYS_COLOR0_BASE+0x82C)
#define DISP_COLOR_PART_SAT_GAIN3_3     (DISPSYS_COLOR0_BASE+0x830)
#define DISP_COLOR_PART_SAT_GAIN3_4     (DISPSYS_COLOR0_BASE+0x834)
#define DISP_COLOR_PART_SAT_POINT1_0    (DISPSYS_COLOR0_BASE+0x838)
#define DISP_COLOR_PART_SAT_POINT1_1    (DISPSYS_COLOR0_BASE+0x83C)
#define DISP_COLOR_PART_SAT_POINT1_2    (DISPSYS_COLOR0_BASE+0x840)
#define DISP_COLOR_PART_SAT_POINT1_3    (DISPSYS_COLOR0_BASE+0x844)
#define DISP_COLOR_PART_SAT_POINT1_4    (DISPSYS_COLOR0_BASE+0x848)
#define DISP_COLOR_PART_SAT_POINT2_0    (DISPSYS_COLOR0_BASE+0x84C)
#define DISP_COLOR_PART_SAT_POINT2_1    (DISPSYS_COLOR0_BASE+0x850)
#define DISP_COLOR_PART_SAT_POINT2_2    (DISPSYS_COLOR0_BASE+0x854)
#define DISP_COLOR_PART_SAT_POINT2_3    (DISPSYS_COLOR0_BASE+0x858)
#define DISP_COLOR_PART_SAT_POINT2_4    (DISPSYS_COLOR0_BASE+0x85C)

#define DISP_COLOR_START        (DISPSYS_COLOR0_BASE+0xC00)
#define DISP_COLOR_INTEN        (DISPSYS_COLOR0_BASE+0xC04)
#define DISP_COLOR_OUT_SEL      (DISPSYS_COLOR0_BASE+0xC08)
#define DISP_COLOR_CK_ON        (DISPSYS_COLOR0_BASE+0xC28)
#define DISP_COLOR_INTERNAL_IP_WIDTH    (DISPSYS_COLOR0_BASE+0xC50)
#define DISP_COLOR_INTERNAL_IP_HEIGHT   (DISPSYS_COLOR0_BASE+0xC54)
#define DISP_COLOR_CM1_EN       (DISPSYS_COLOR0_BASE+0xC60)
#define DISP_COLOR_CM2_EN       (DISPSYS_COLOR0_BASE+0xCA0)



// -------------------------------------------------------------
// Config
#define DISP_REG_CONFIG_MMSYS_INTEN                          		(DISPSYS_CONFIG_BASE + 0x0)
#define DISP_REG_CONFIG_MMSYS_INTSTA                         		(DISPSYS_CONFIG_BASE + 0x4)
#define DISP_REG_CONFIG_MJC_APB_TX_CON		                   	(DISPSYS_CONFIG_BASE + 0x8)
#define DISP_REG_CONFIG_PWM_APB_ERR_ADDR	                   	(DISPSYS_CONFIG_BASE + 0xc)
#define DISP_REG_CONFIG_ISP_MOUT_EN                          		(DISPSYS_CONFIG_BASE + 0x01c)
#define DISP_REG_CONFIG_MDP_RDMA0_MOUT_EN                    	(DISPSYS_CONFIG_BASE + 0x020)
#define DISP_REG_CONFIG_MDP_PRZ0_MOUT_EN                     	(DISPSYS_CONFIG_BASE + 0x024)
#define DISP_REG_CONFIG_MDP_PRZ1_MOUT_EN                     	(DISPSYS_CONFIG_BASE + 0x028)
#define DISP_REG_CONFIG_MDP_PRZ2_MOUT_EN                     	(DISPSYS_CONFIG_BASE + 0x02C)
#define DISP_REG_CONFIG_MDP_TDSHP0_MOUT_EN                   	(DISPSYS_CONFIG_BASE + 0x030)
#define DISP_REG_CONFIG_MDP_TDSHP1_MOUT_EN                   	(DISPSYS_CONFIG_BASE + 0x034)
#define DISP_REG_CONFIG_MDP0_MOUT_EN                         		(DISPSYS_CONFIG_BASE + 0x038)
#define DISP_REG_CONFIG_MDP1_MOUT_EN                         		(DISPSYS_CONFIG_BASE + 0x03C)
#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN                   	(DISPSYS_CONFIG_BASE + 0x040)
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN                    	(DISPSYS_CONFIG_BASE + 0x044)
#define DISP_REG_CONFIG_DISP_OD_MOUT_EN                   		(DISPSYS_CONFIG_BASE + 0x048)
#define DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN                   	(DISPSYS_CONFIG_BASE + 0x04C)
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN                    	(DISPSYS_CONFIG_BASE + 0x050)
#define DISP_REG_CONFIG_MMSYS_MOUT_RST                       		(DISPSYS_CONFIG_BASE + 0x054)
#define DISP_REG_CONFIG_MDP_PRZ0_SEL_IN                      		(DISPSYS_CONFIG_BASE + 0x058)
#define DISP_REG_CONFIG_MDP_PRZ1_SEL_IN                      		(DISPSYS_CONFIG_BASE + 0x05C)
#define DISP_REG_CONFIG_MDP_PRZ2_SEL_IN                      		(DISPSYS_CONFIG_BASE + 0x060)
#define DISP_REG_CONFIG_MDP_TDSHP0_SEL_IN                    	(DISPSYS_CONFIG_BASE + 0x064)
#define DISP_REG_CONFIG_MDP_TDSHP1_SEL_IN                    	(DISPSYS_CONFIG_BASE + 0x068)
#define DISP_REG_CONFIG_MDP0_SEL_IN                          		(DISPSYS_CONFIG_BASE + 0x06C)
#define DISP_REG_CONFIG_MDP1_SEL_IN                          		(DISPSYS_CONFIG_BASE + 0x070)
#define DISP_REG_CONFIG_MDP_CROP_SEL_IN                      		(DISPSYS_CONFIG_BASE + 0x074)
#define DISP_REG_CONFIG_MDP_WDMA_SEL_IN                      	(DISPSYS_CONFIG_BASE + 0x078)
#define DISP_REG_CONFIG_MDP_WROT0_SEL_IN                     	(DISPSYS_CONFIG_BASE + 0x07C)
#define DISP_REG_CONFIG_MDP_WROT1_SEL_IN                     	(DISPSYS_CONFIG_BASE + 0x080)
#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN                   	(DISPSYS_CONFIG_BASE + 0x084)
#define DISP_REG_CONFIG_DISP_COLOR1_SEL_IN                   	(DISPSYS_CONFIG_BASE + 0x088)
#define DISP_REG_CONFIG_DISP_AAL_SEL_IN                      		(DISPSYS_CONFIG_BASE + 0x08C)
#define DISP_REG_CONFIG_DISP_PATH0_SEL_IN                   	 	(DISPSYS_CONFIG_BASE + 0x090)
#define DISP_REG_CONFIG_DISP_PATH1_SEL_IN                    	(DISPSYS_CONFIG_BASE + 0x094)
#define DISP_REG_CONFIG_DISP_MODULE_WDMA0_SEL_IN                    	(DISPSYS_CONFIG_BASE + 0x098)
#define DISP_REG_CONFIG_DISP_WDMA1_SEL_IN                    	(DISPSYS_CONFIG_BASE + 0x09C)
#define DISP_REG_CONFIG_DISP_UFOE_SEL_IN                     		(DISPSYS_CONFIG_BASE + 0x0A0)
#define DISP_REG_CONFIG_DSI0_SEL_IN                          		(DISPSYS_CONFIG_BASE + 0x0A4)
#define DISP_REG_CONFIG_DSI1_SEL_IN                          		(DISPSYS_CONFIG_BASE + 0x0A8)
#define DISP_REG_CONFIG_DPI_SEL_IN                           			(DISPSYS_CONFIG_BASE + 0x0AC)
#define DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN               	(DISPSYS_CONFIG_BASE + 0x0B0)
#define DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN               	(DISPSYS_CONFIG_BASE + 0x0B4)
#define DISP_REG_CONFIG_DISP_RDMA2_SOUT_SEL_IN               	(DISPSYS_CONFIG_BASE + 0x0B8)
#define DISP_REG_CONFIG_DISP_COLOR0_SOUT_SEL_IN              	(DISPSYS_CONFIG_BASE + 0x0BC)
#define DISP_REG_CONFIG_DISP_COLOR1_SOUT_SEL_IN              	(DISPSYS_CONFIG_BASE + 0x0C0)
#define DISP_REG_CONFIG_DISP_PATH0_SOUT_SEL_IN               	(DISPSYS_CONFIG_BASE + 0x0C4)
#define DISP_REG_CONFIG_DISP_PATH1_SOUT_SEL_IN               	(DISPSYS_CONFIG_BASE + 0x0C8)
#define DISP_REG_CONFIG_MMSYS_MISC                           		(DISPSYS_CONFIG_BASE + 0x0F0)
#define DISP_REG_CONFIG_MMSYS_CG_CON0                        		(DISPSYS_CONFIG_BASE + 0x100)
#define DISP_REG_CONFIG_MMSYS_CG_SET0                        		(DISPSYS_CONFIG_BASE + 0x104)
#define DISP_REG_CONFIG_MMSYS_CG_CLR0                        		(DISPSYS_CONFIG_BASE + 0x108)
#define DISP_REG_CONFIG_MMSYS_CG_CON1                        		(DISPSYS_CONFIG_BASE + 0x110)
#define DISP_REG_CONFIG_MMSYS_CG_SET1                        		(DISPSYS_CONFIG_BASE + 0x114)
#define DISP_REG_CONFIG_MMSYS_CG_CLR1                        		(DISPSYS_CONFIG_BASE + 0x118)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS0                    	(DISPSYS_CONFIG_BASE + 0x120)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET0                (DISPSYS_CONFIG_BASE + 0x124)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR0                (DISPSYS_CONFIG_BASE + 0x128)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS1                    	(DISPSYS_CONFIG_BASE + 0x130)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_SET1                (DISPSYS_CONFIG_BASE + 0x134)
#define DISP_REG_CONFIG_MMSYS_HW_DCM_DIS_CLR1                (DISPSYS_CONFIG_BASE + 0x138)
#define DISP_REG_CONFIG_MMSYS_SW0_RST_B                      	(DISPSYS_CONFIG_BASE + 0x140)
#define DISP_REG_CONFIG_MMSYS_SW1_RST_B                      	(DISPSYS_CONFIG_BASE + 0x144)
#define DISP_REG_CONFIG_MMSYS_LCM_RST_B                      		(DISPSYS_CONFIG_BASE + 0x150)
#define DISP_REG_CONFIG_SMI_N21MUX_CFG_WR                    	(DISPSYS_CONFIG_BASE + 0x168)
#define DISP_REG_CONFIG_SMI_N21MUX_CFG_RD                    	(DISPSYS_CONFIG_BASE + 0x16c)
#define DISP_REG_CONFIG_ELA2GMC_BASE_ADDR                    	(DISPSYS_CONFIG_BASE + 0x170)
#define DISP_REG_CONFIG_ELA2GMC_BASE_ADDR_END                (DISPSYS_CONFIG_BASE + 0x174)
#define DISP_REG_CONFIG_ELA2GMC_FINAL_ADDR                   	(DISPSYS_CONFIG_BASE + 0x178)
#define DISP_REG_CONFIG_ELA2GMC_STATUS                       		(DISPSYS_CONFIG_BASE + 0x17c)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_EN	                 	(DISPSYS_CONFIG_BASE + 0x200)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_RST                    	(DISPSYS_CONFIG_BASE + 0x204)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_CON0	                 	(DISPSYS_CONFIG_BASE + 0x208)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_CON1	                 	(DISPSYS_CONFIG_BASE + 0x20c)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_RD_ADDR           	(DISPSYS_CONFIG_BASE + 0x210)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_WR_ADDR              	(DISPSYS_CONFIG_BASE + 0x214)
#define DISP_REG_CONFIG_DISP_FAKE_ENG_STATE	                 	(DISPSYS_CONFIG_BASE + 0x218)
#define DISP_REG_CONFIG_MMSYS_MBIST_CON                      		(DISPSYS_CONFIG_BASE + 0x800)
#define DISP_REG_CONFIG_MMSYS_MBIST_DONE                     	(DISPSYS_CONFIG_BASE + 0x804)
#define DISP_REG_CONFIG_MMSYS_MBIST_HOLDB                    	(DISPSYS_CONFIG_BASE + 0x808)
#define DISP_REG_CONFIG_MMSYS_MBIST_MODE                     	(DISPSYS_CONFIG_BASE + 0x80c)
#define DISP_REG_CONFIG_MMSYS_MBIST_FAIL0                    	(DISPSYS_CONFIG_BASE + 0x810)
#define DISP_REG_CONFIG_MMSYS_MBIST_FAIL1                    	(DISPSYS_CONFIG_BASE + 0x814)
#define DISP_REG_CONFIG_MMSYS_MBIST_FAIL2                    	(DISPSYS_CONFIG_BASE + 0x818)
#define DISP_REG_CONFIG_MMSYS_MBIST_BSEL0	                 	(DISPSYS_CONFIG_BASE + 0x820)
#define DISP_REG_CONFIG_MMSYS_MBIST_BSEL1	                 	(DISPSYS_CONFIG_BASE + 0x824)
#define DISP_REG_CONFIG_MMSYS_MBIST_BSEL2	                 	(DISPSYS_CONFIG_BASE + 0x828)
#define DISP_REG_CONFIG_MMSYS_MBIST_BSEL3	                 	(DISPSYS_CONFIG_BASE + 0x82c)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL0                    	(DISPSYS_CONFIG_BASE + 0x830)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL1                    	(DISPSYS_CONFIG_BASE + 0x834)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL2                    	(DISPSYS_CONFIG_BASE + 0x838)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL3                    	(DISPSYS_CONFIG_BASE + 0x83c)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL4                    	(DISPSYS_CONFIG_BASE + 0x840)
#define DISP_REG_CONFIG_MMSYS_MEM_DELSEL5                    	(DISPSYS_CONFIG_BASE + 0x844)
#define DISP_REG_CONFIG_MMSYS_DEBUG_OUT_SEL                  	(DISPSYS_CONFIG_BASE + 0x880)
#define DISP_REG_CONFIG_MMSYS_DUMMY                          		(DISPSYS_CONFIG_BASE + 0x890)
#define DISP_REG_CONFIG_MMSYS_MBIST_RP_RST_B                 	(DISPSYS_CONFIG_BASE + 0x8a0)
#define DISP_REG_CONFIG_MMSYS_MBIST_RP_FAIL                  	(DISPSYS_CONFIG_BASE + 0x8a4)
#define DISP_REG_CONFIG_MMSYS_MBIST_RP_OK                    	(DISPSYS_CONFIG_BASE + 0x8a8)
#define DISP_REG_CONFIG_DISP_DL_VALID_0                      		(DISPSYS_CONFIG_BASE + 0x8b0)
#define DISP_REG_CONFIG_DISP_DL_VALID_1                      		(DISPSYS_CONFIG_BASE + 0x8b4)
#define DISP_REG_CONFIG_DISP_DL_READY_0                      		(DISPSYS_CONFIG_BASE + 0x8b8)
#define DISP_REG_CONFIG_DISP_DL_READY_1                      		(DISPSYS_CONFIG_BASE + 0x8bc)
#define DISP_REG_CONFIG_MDP_DL_VALID_0                       		(DISPSYS_CONFIG_BASE + 0x8c0)
#define DISP_REG_CONFIG_MDP_DL_VALID_1                       		(DISPSYS_CONFIG_BASE + 0x8c4)
#define DISP_REG_CONFIG_MDP_DL_READY_0                       		(DISPSYS_CONFIG_BASE + 0x8c8)
#define DISP_REG_CONFIG_MDP_DL_READY_1                       		(DISPSYS_CONFIG_BASE + 0x8cc)
#define DISP_REG_CONFIG_SMI_LARB0_GREQ                       		(DISPSYS_CONFIG_BASE + 0x8d0)
#define DISP_REG_CONFIG_C08                       		            (DISPSYS_CONFIG_BASE + 0xc08)
#define DISP_REG_CONFIG_C09                                          0xf0206040
#define DISP_REG_CONFIG_C10                                          0xf0206044
#define DISP_REG_CLK_CFG_0_MM_CLK                                    0xf0000040  //BIT31 0
#define DISP_REG_CLK_CFG_0_CLR                                       0xf0000048  //BIT31  1 clear for mms
#define DISP_REG_CLK_CFG_1_CLR                                       0xf0000058  //bit7 1 clear for pwm
#define DISP_REG_CLK_CFG_6_DPI                                       0xf00000A0  //bit7 0
#define DISP_REG_CLK_CFG_6_CLR                                       0xf00000A8  // bit7 1 clear for dpi
#define DISP_REG_VENCPLL_CON0                                        0xf0209260  // bit0 1

#define MMSYS_INTEN_FLD_MMSYS_INTEN                            				REG_FLD(8, 0)
#define MMSYS_INSTA_FLD_MMSYS_INSTA                           				REG_FLD(1, 0)
#define MJC_APB_TX_CON_FLD_MJC_APB_COUNTER_EN         				REG_FLD(1, 0)
#define MJC_APB_TX_CON_FLD_MJC_APB_ERR_ADDR         				REG_FLD(16, 16)
#define PWM_APB_ERR_ADDR_FLD_PWM0_APB_ERR_ADDR    				REG_FLD(12, 0)
#define PWM_APB_ERR_ADDR_FLD_PWM1_APB_ERR_ADDR    				REG_FLD(12, 16)
#define ISP_MOUT_EN_FLD_ISP_MOUT_EN                            				REG_FLD(2, 0)
#define MDP_RDMA0_MOUT_EN_FLD_MDP_RDMA0_MOUT_EN        			REG_FLD(3, 0)
#define MDP_PRZ0_MOUT_EN_FLD_MDP_PRZ0_MOUT_EN               		REG_FLD(3, 0)
#define MDP_PRZ1_MOUT_EN_FLD_MDP_PRZ1_MOUT_EN               		REG_FLD(3, 0)
#define MDP_PRZ2_MOUT_EN_FLD_MDP_PRZ2_MOUT_EN               		REG_FLD(2, 0)
#define MDP_TDSHP0_MOUT_EN_FLD_MDP_TDSHP0_MOUT_EN      		REG_FLD(2, 0)
#define MDP_TDSHP1_MOUT_EN_FLD_MDP_TDSHP1_MOUT_EN      		REG_FLD(2, 0)
#define MDP0_MOUT_EN_FLD_MDP0_MOUT_EN                          			REG_FLD(3, 0)
#define MDP1_MOUT_EN_FLD_MDP1_MOUT_EN                          			REG_FLD(3, 0)
#define DISP_OVL0_MOUT_EN_FLD_DISP_OVL0_MOUT_EN  				REG_FLD(2, 0)
#define DISP_OVL1_MOUT_EN_FLD_DISP_OVL1_MOUT_EN    				REG_FLD(2, 0)
#define DISP_OD_MOUT_EN_FLD_DISP_OD_MOUT_EN   					REG_FLD(3, 0)
#define DISP_GAMMA_MOUT_EN_FLD_DISP_GAMMA_MOUT_EN   			REG_FLD(3, 0)
#define DISP_UFOE_MOUT_EN_FLD_DISP_UFOE_MOUT_EN   				REG_FLD(4, 0)
#define MMSYS_MOUT_RST_FLD_MMSYS_MOUT_RST                 			REG_FLD(14, 0)
#define MDP_PRZ0_SEL_IN_FLD_MDP_PRZ0_SEL_IN                   			REG_FLD(1, 0)
#define MDP_PRZ1_SEL_IN_FLD_MDP_PRZ1_SEL_IN                   			REG_FLD(2, 0)
#define MDP_PRZ2_SEL_IN_FLD_MDP_PRZ2_SEL_IN                   			REG_FLD(1, 0)
#define MDP_TDSHP0_SEL_IN_FLD_MDP_TDSHP0_SEL_IN 				REG_FLD(1, 0)
#define MDP_TDSHP1_SEL_IN_FLD_MDP_TDSHP1_SEL_IN 				REG_FLD(1, 0)
#define MDP0_SEL_IN_FLD_MDP0_SEL_IN                            				REG_FLD(1, 0)
#define MDP1_SEL_IN_FLD_MDP1_SEL_IN                           	 			REG_FLD(2, 0)
#define MDP_CROP_SEL_IN_FLD_MDP_CROP_SEL_IN                    			REG_FLD(1, 0)
#define MDP_WDMA_SEL_IN_FLD_MDP_WDMA_SEL_IN              			REG_FLD(1, 0)
#define MDP_WROT0_SEL_IN_FLD_MDP_WROT0_SEL_IN           			REG_FLD(1, 0)
#define MDP_WROT1_SEL_IN_FLD_MDP_WROT1_SEL_IN               			REG_FLD(1, 0)
#define DISP_COLOR0_SEL_IN_FLD_DISP_COLOR0_SEL_IN               		REG_FLD(1, 0)
#define DISP_COLOR1_SEL_IN_FLD_DISP_COLOR1_SEL_IN               		REG_FLD(1, 0)
#define DISP_AAL_SEL_IN_FLD_DISP_AAL_SEL_IN                    			REG_FLD(1, 0)
#define DISP_PATH0_SEL_IN_FLD_DISP_PATH0_SEL_IN               			REG_FLD(1, 0)
#define DISP_PATH1_SEL_IN_FLD_DISP_PATH1_SEL_IN               			REG_FLD(2, 0)
#define DISP_MODULE_WDMA0_SEL_IN_FLD_DISP_MODULE_WDMA0_SEL_IN               		REG_FLD(2, 0)
#define DISP_WDMA1_SEL_IN_FLD_DISP_WDMA1_SEL_IN               		REG_FLD(1, 0)
#define DISP_UFO_SEL_IN_FLD_DISP_UFO_SEL_IN                    			REG_FLD(1, 0)
#define DSI0_SEL_IN_FLD_DSI0_SEL_IN                            				REG_FLD(2, 0)
#define DSI1_SEL_IN_FLD_DSI1_SEL_IN                            				REG_FLD(2, 0)
#define DPI_SEL_IN_FLD_DPI_SEL_IN                              					REG_FLD(2, 0)
#define DISP_RDMA0_SOUT_SEL_IN_FLD_DISP_RDMA0_SOUT_SEL_IN     	REG_FLD(1, 0)
#define DISP_RDMA1_SOUT_SEL_IN_FLD_DISP_RDMA1_SOUT_SEL_IN     	REG_FLD(1, 0)
#define DISP_RDMA2_SOUT_SEL_IN_FLD_DISP_RDMA2_SOUT_SEL_IN     	REG_FLD(1, 0)
#define DISP_COLOR0_SOUT_SEL_IN_FLD_DISP_COLOR0_SOUT_SEL_IN     REG_FLD(1, 0)
#define DISP_COLOR1_SOUT_SEL_IN_FLD_DISP_COLOR1_SOUT_SEL_IN     REG_FLD(1, 0)
#define DISP_PATH0_SOUT_SEL_IN_FLD_DISP_PATH0_SOUT_SEL_IN     	REG_FLD(1, 0)
#define DISP_PATH1_SOUT_SEL_IN_FLD_DISP_PATH1_SOUT_SEL_IN     	REG_FLD(2, 0)
#define MMSYS_MISC_FLD_SMI_LARB0_TEST_MODE					     	REG_FLD(1, 0)
#define MMSYS_CG_CON0_FLD_CG0                                  					REG_FLD(32, 0)
#define MMSYS_CG_SET0_FLD_CG0                                  					REG_FLD(32, 0)
#define MMSYS_CG_CLR0_FLD_CG0                                  					REG_FLD(32, 0)
#define MMSYS_CG_CON1_FLD_CG1                                  					REG_FLD(32, 0)
#define MMSYS_CG_SET1_FLD_CG1                                  					REG_FLD(32, 0)
#define MMSYS_CG_CLR1_FLD_CG1                                  					REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS0_FLD_DCM_DIS0                         			REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS_SET0_FLD_DCM_DIS0                     			REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS_CLR0_FLD_DCM_DIS0                     			REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS1_FLD_DCM_DIS1                         			REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS_SET1_FLD_DCM_DIS1                     			REG_FLD(32, 0)
#define MMSYS_HW_DCM_DIS_CLR1_FLD_DCM_DIS1                     			REG_FLD(32, 0)
#define MMSYS_SW0_RST_B_FLD_SW0_RST_B                            			REG_FLD(32, 0)
#define MMSYS_SW1_RST_B_FLD_SW1_RST_B                            			REG_FLD(32, 0)
#define MMSYS_LCM_RST_B_FLD_LCM_RSTB                           				REG_FLD(1, 0)
#define SMI_N21MUX_CFG_WR_FLD_SMI_N21MUX_CFG_WR              		REG_FLD(32, 0)
#define SMI_N21MUX_CFG_RD_FLD_SMI_N21MUX_CFG_RD              		REG_FLD(32, 0)
#define ELA2GMC_BASE_ADDR_FLD_ELA2GMC_BASE_ADDR              		REG_FLD(32, 0)
#define ELA2GMC_BASE_ADDR_END_FLD_ELA2GMC_BASE_ADDR_END		REG_FLD(32, 0)
#define ELA2GMC_FINAL_ADDR_FLD_ELA2GMC_FINAL_ADDR              		REG_FLD(32, 0)
#define ELA2GMC_STATUS_FLD_ELA2GMC_STATUS              				REG_FLD(1, 0)
#define DISP_FAKE_ENG_EN_FLD_DFE_START                         			REG_FLD(1, 1)
#define DISP_FAKE_ENG_EN_FLD_DFE_MUX_SEL                       			REG_FLD(1, 0)
#define DISP_FAKE_ENG_RST_FLD_DISP_FAKE_ENG_RST          			REG_FLD(1, 0)
#define DISP_FAKE_ENG_CON0_FLD_DFE_DRE_EN                      			REG_FLD(1, 23)
#define DISP_FAKE_ENG_CON0_FLD_DFE_LOOP_MODE        				REG_FLD(1, 22)
#define DISP_FAKE_ENG_CON0_FLD_DFE_TEST_LEN                    			REG_FLD(20, 0)
#define DISP_FAKE_ENG_CON1_FLD_DFE_WR_DIS                      			REG_FLD(1, 11)
#define DISP_FAKE_ENG_CON1_FLD_DFE_RD_DIS                      			REG_FLD(1, 10)
#define DISP_FAKE_ENG_CON1_FLD_DFE_SLOW_DOWN             			REG_FLD(10, 0)
#define DISP_FAKE_ENG_RD_ADDR_FLD_DISP_FAKE_ENG_RD_ADDR		REG_FLD(32, 0)
#define DISP_FAKE_ENG_WR_ADDR_FLD_DISP_FAKE_ENG_WR_ADDR    	REG_FLD(32, 0)
#define DISP_FAKE_ENG_STATE_FLD_DFE_RD_ST                      			REG_FLD(3, 12)
#define DISP_FAKE_ENG_STATE_FLD_DFE_WR_ST                      			REG_FLD(4, 8)
#define DISP_FAKE_ENG_STATE_FLD_DFE_BUSY                       			REG_FLD(1, 0)
#define MMSYS_MBIST_CON_FLD_MMSYS_MBIST_BACKGROUND    			REG_FLD(3, 16)
#define MMSYS_MBIST_CON_FLD_MMSYS_MBIST_RSTB                 			REG_FLD(1, 15)
#define MMSYS_MBIST_CON_FLD_MMSYS_MBIST_SCANOUT_SEL   			REG_FLD(4, 8)
#define MMSYS_MBIST_CON_FLD_MMSYS_MBIST_DEBUG              			REG_FLD(1, 7)
#define MMSYS_MBIST_CON_FLD_MMSYS_MBIST_FAILOUT_SEL   			REG_FLD(6, 0)
#define MMSYS_MBIST_DONE_FLD_MMSYS_MBIST_DONE             			REG_FLD(23, 0)
#define MMSYS_MBIST_HOLDB_FLD_MMSYS_MBIST_HOLDB             		REG_FLD(23, 0)
#define MMSYS_MBIST_MODE_FLD_MMSYS_MBIST_MODE             			REG_FLD(23, 0)
#define MMSYS_MBIST_FAIL0_FLD_MMSYS_MBIST_FAIL0            			REG_FLD(32, 0)
#define MMSYS_MBIST_FAIL1_FLD_MMSYS_MBIST_FAIL1           			REG_FLD(32, 0)
#define MMSYS_MBIST_FAIL2_FLD_MMSYS_MBIST_FAIL2           			REG_FLD(24, 0)
#define MMSYS_MBIST_BSEL0_FLD_MDP_TDSHP0_MBIST_BSEL    			REG_FLD(16, 0)
#define MMSYS_MBIST_BSEL0_FLD_MDP_TDSHP1_MBIST_BSEL    			REG_FLD(16, 16)
#define MMSYS_MBIST_BSEL1_FLD_MDP_RDMA0_MBIST_BSEL       		REG_FLD(8, 0)
#define MMSYS_MBIST_BSEL1_FLD_MDP_RDMA1_MBIST_BSEL         		REG_FLD(8, 8)
#define MMSYS_MBIST_BSEL1_FLD_MDP_WROT0_MBIST_BSEL       		REG_FLD(8, 16)
#define MMSYS_MBIST_BSEL1_FLD_MDP_WROT1_MBIST_BSEL       		REG_FLD(8, 24)
#define MMSYS_MBIST_BSEL2_FLD_MDP_RSZ0_MBIST_BSEL      			REG_FLD(12, 0)
#define MMSYS_MBIST_BSEL2_FLD_MDP_RSZ1_MBIST_BSEL      			REG_FLD(12, 16)
#define MMSYS_MBIST_BSEL3_FLD_MDP_RSZ2_MBIST_BSEL      			REG_FLD(12, 0)
#define MMSYS_MBIST_BSEL3_FLD_DISP_UFOE_MBIST_BSEL      			REG_FLD(12, 16)
#define MMSYS_MEM_DELSEL0_FLD_MDP_WDMA_MEM_DELSEL_1  		REG_FLD(4, 28)
#define MMSYS_MEM_DELSEL0_FLD_DISP_RDMA_MEM_DELSEL    			REG_FLD(4, 24)
#define MMSYS_MEM_DELSEL0_FLD_DISP_OVL_MEM_DELSEL   			REG_FLD(4, 20)
#define MMSYS_MEM_DELSEL0_FLD_DISP_WDMA_MEM_DELSEL_1			REG_FLD(4, 16)
#define MMSYS_MEM_DELSEL0_FLD_DISP_AAL_MEM_DELSEL  			REG_FLD(4, 12)
#define MMSYS_MEM_DELSEL0_FLD_DISP_GAMMA_MEM_DELSEL    		REG_FLD(4, 8)
#define MMSYS_MEM_DELSEL0_FLD_DISP_UFOE_MEM_DELSEL_0           	REG_FLD(4, 4)
#define MMSYS_MEM_DELSEL0_FLD_DSI_MEM_DELSEL     				REG_FLD(4, 0)
#define MMSYS_MEM_DELSEL1_FLD_OD_DRAMWR_DELSEL	       		REG_FLD(2, 26)
#define MMSYS_MEM_DELSEL1_FLD_OD_ALGBUF2_DELSEL	       		REG_FLD(2, 24)
#define MMSYS_MEM_DELSEL1_FLD_OD_ALIGNBUF1_DELSEL	       		REG_FLD(2, 22)
#define MMSYS_MEM_DELSEL1_FLD_MDP_RDMA_MEM_DELSEL_0       		REG_FLD(2, 20)
#define MMSYS_MEM_DELSEL1_FLD_MDP_WDMA_MEM_DELSEL_0       		REG_FLD(2, 18)
#define MMSYS_MEM_DELSEL1_FLD_DISP_WDMA_MEM_DELSEL_0       		REG_FLD(2, 16)
#define MMSYS_MEM_DELSEL1_FLD_MDP_RDMA_MEM_DELSEL_3       		REG_FLD(4, 12)
#define MMSYS_MEM_DELSEL1_FLD_MDP_RDMA_MEM_DELSEL_2       		REG_FLD(4, 8)
#define MMSYS_MEM_DELSEL1_FLD_MDP_RDMA_MEM_DELSEL_1	        	REG_FLD(4, 4)
#define MMSYS_MEM_DELSEL1_FLD_MDP_TDSHP_MEM_DELSEL          		REG_FLD(4, 0)
#define MMSYS_MEM_DELSEL2_FLD_MDP_WROT_MEM_DELSEL              	REG_FLD(20, 0)
#define MMSYS_MEM_DELSEL3_FLD_MDP_RSZ_MEM_DELSEL              		REG_FLD(24, 0)
#define MMSYS_MEM_DELSEL4_FLD_MDP_RSZ2_MEM_DELSEL              		REG_FLD(24, 0)
#define MMSYS_MEM_DELSEL5_FLD_DISP_UFOE_MEM_DELSEL_1              	REG_FLD(4, 0)
#define MMSYS_MEM_DELSEL5_FLD_DISP_UFOE_MEM_DELSEL_2              	REG_FLD(4, 4)
#define MMSYS_MEM_DELSEL5_FLD_OD_LINE_BUF_DELSEL	              	REG_FLD(4, 8)
#define MMSYS_MEM_DELSEL5_FLD_OD_296X8_DELSEL	              		REG_FLD(4, 12)
#define MMSYS_MEM_DELSEL5_FLD_OD_272X8_DELSEL	              		REG_FLD(4, 16)
#define MMSYS_MEM_DELSEL5_FLD_OD_IP_BUF_DELSEL	              		REG_FLD(4, 20)
#define MMSYS_MEM_DELSEL5_FLD_OD_MOTBUF_DELSEL	              	REG_FLD(4, 24)
#define MMSYS_MEM_DELSEL5_FLD_OD_DRAMRD_DELSEL	              	REG_FLD(4, 28)
#define MMSYS_DEBUG_OUT_SEL_FLD_MMSYS_DEBUG_OUT_SEL            	REG_FLD(5, 0)
#define MMSYS_DUMMY_FLD_MMSYS_DUMMY                            			REG_FLD(32, 0)
#define MMSYS_MBIST_RP_RST_B_FLD_MMSYS_MBIST_RP_RST_B          	REG_FLD(1, 0)
#define MMSYS_MBIST_RP_FAIL_FLD_DISP_RDMA2_MBIST_RP_FAIL       	REG_FLD(2, 10)
#define MMSYS_MBIST_RP_FAIL_FLD_DISP_RDMA1_MBIST_RP_FAIL       	REG_FLD(2, 8)
#define MMSYS_MBIST_RP_FAIL_FLD_DISP_RDMA0_MBIST_RP_FAIL       	REG_FLD(2, 6)
#define MMSYS_MBIST_RP_FAIL_FLD_DISP_OVL1_MBIST_RP_FAIL        	REG_FLD(2, 4)
#define MMSYS_MBIST_RP_FAIL_FLD_DISP_OVL0_MBIST_RP_FAIL        	REG_FLD(2, 2)
#define MMSYS_MBIST_RP_FAIL_FLD_MDP_WROT1_MBIST_RP_FAIL        	REG_FLD(1, 1)
#define MMSYS_MBIST_RP_FAIL_FLD_MDP_WROT0_MBIST_RP_FAIL        	REG_FLD(1, 0)
#define MMSYS_MBIST_RP_OK_FLD_DISP_RDMA2_MBIST_RP_OK           	REG_FLD(2, 10)
#define MMSYS_MBIST_RP_OK_FLD_DISP_RDMA1_MBIST_RP_OK           	REG_FLD(2, 8)
#define MMSYS_MBIST_RP_OK_FLD_DISP_RDMA0_MBIST_RP_OK           	REG_FLD(2, 6)
#define MMSYS_MBIST_RP_OK_FLD_DISP_OVL1_MBIST_RP_OK            		REG_FLD(2, 4)
#define MMSYS_MBIST_RP_OK_FLD_DISP_OVL0_MBIST_RP_OK            		REG_FLD(2, 2)
#define MMSYS_MBIST_RP_OK_FLD_MDP_WROT1_MBIST_RP_OK            	REG_FLD(1, 1)
#define MMSYS_MBIST_RP_OK_FLD_MDP_WROT0_MBIST_RP_OK            	REG_FLD(1, 0)
#define MMSYS_DISP_DL_VALID_0_FLD_DISP_DL_VALID_0                    	REG_FLD(32, 0)
#define MMSYS_DISP_DL_VALID_1_FLD_DISP_DL_VALID_1                     	REG_FLD(24, 0)
#define MMSYS_DISP_DL_READY_0_FLD_DISP_DL_READY_0                    	REG_FLD(32, 0)
#define MMSYS_DISP_DL_READY_1_FLD_DISP_DL_READY_1                    	REG_FLD(24, 0)
#define MMSYS_MDP_DL_VALID_0_FLD_MDP_DL_VALID_0                    	REG_FLD(32, 0)
#define MMSYS_MDP_DL_VALID_1_FLD_MDP_DL_VALID_1                     	REG_FLD(10, 0)
#define MMSYS_MDP_DL_READY_0_FLD_MDP_DL_READY_0                    	REG_FLD(32, 0)
#define MMSYS_MDP_DL_READY_1_FLD_MDP_DL_READY_1                    	REG_FLD(10, 0)
#define SMI_LARB0_GREQ_FLD_SMI_LARB0_GREQ			                    	REG_FLD(14, 0)

// GAMMA
#define DISP_REG_GAMMA_EN								(DISPSYS_GAMMA_BASE + 0x000)
#define DISP_REG_GAMMA_RESET							(DISPSYS_GAMMA_BASE + 0x004)
#define DISP_REG_GAMMA_INTEN							(DISPSYS_GAMMA_BASE + 0x008)
#define DISP_REG_GAMMA_INTSTA							(DISPSYS_GAMMA_BASE + 0x00c)
#define DISP_REG_GAMMA_STATUS						(DISPSYS_GAMMA_BASE + 0x010)
#define DISP_REG_GAMMA_CFG							(DISPSYS_GAMMA_BASE + 0x020)
#define DISP_REG_GAMMA_INPUT_COUNT					(DISPSYS_GAMMA_BASE + 0x024)
#define DISP_REG_GAMMA_OUTPUT_COUNT					(DISPSYS_GAMMA_BASE + 0x028)
#define DISP_REG_GAMMA_CHKSUM						(DISPSYS_GAMMA_BASE + 0x02c)
#define DISP_REG_GAMMA_SIZE							(DISPSYS_GAMMA_BASE + 0x030)
#define DISP_GAMMA_CCORR_0							(DISPSYS_GAMMA_BASE + 0x080)
#define DISP_GAMMA_CCORR_1							(DISPSYS_GAMMA_BASE + 0x084)
#define DISP_GAMMA_CCORR_2							(DISPSYS_GAMMA_BASE + 0x088)
#define DISP_GAMMA_CCORR_3							(DISPSYS_GAMMA_BASE + 0x08C)
#define DISP_GAMMA_CCORR_4							(DISPSYS_GAMMA_BASE + 0x090)
#define DISP_REG_GAMMA_DUMMY_REG					(DISPSYS_GAMMA_BASE + 0x0c0)
#define DISP_REG_GAMMA_DITHER_0						(DISPSYS_GAMMA_BASE + 0x100)
#define DISP_REG_GAMMA_DITHER_5						(DISPSYS_GAMMA_BASE + 0x114)
#define DISP_REG_GAMMA_DITHER_6						(DISPSYS_GAMMA_BASE + 0x118)
#define DISP_REG_GAMMA_DITHER_7						(DISPSYS_GAMMA_BASE + 0x11c)
#define DISP_REG_GAMMA_DITHER_8						(DISPSYS_GAMMA_BASE + 0x120)
#define DISP_REG_GAMMA_DITHER_9						(DISPSYS_GAMMA_BASE + 0x124)
#define DISP_REG_GAMMA_DITHER_10						(DISPSYS_GAMMA_BASE + 0x128)
#define DISP_REG_GAMMA_DITHER_11						(DISPSYS_GAMMA_BASE + 0x12c)
#define DISP_REG_GAMMA_DITHER_12						(DISPSYS_GAMMA_BASE + 0x130)
#define DISP_REG_GAMMA_DITHER_13						(DISPSYS_GAMMA_BASE + 0x134)
#define DISP_REG_GAMMA_DITHER_14						(DISPSYS_GAMMA_BASE + 0x138)
#define DISP_REG_GAMMA_DITHER_15						(DISPSYS_GAMMA_BASE + 0x13c)
#define DISP_REG_GAMMA_DITHER_16						(DISPSYS_GAMMA_BASE + 0x140)
#define DISP_REG_GAMMA_DITHER_17						(DISPSYS_GAMMA_BASE + 0x144)
#define DISP_REG_GAMMA_LUT							(DISPSYS_GAMMA_BASE + 0x700)

#define EN_FLD_GAMMA_EN                             		REG_FLD(1, 0)
#define RESET_FLD_GAMMA_RESET                       	REG_FLD(1, 0)
#define INTEN_FLD_OF_END_INT_EN                      	REG_FLD(1, 1)
#define INTEN_FLD_IF_END_INT_EN                      	REG_FLD(1, 0)
#define INTSTA_FLD_OF_END_INT                      		REG_FLD(1, 1)
#define INTSTA_FLD_IF_END_INT                    		REG_FLD(1, 0)
#define STATUS_FLD_IN_VALID	                   		REG_FLD(1, 7)
#define STATUS_FLD_IN_READY	                   		REG_FLD(1, 6)
#define STATUS_FLD_OUT_VALID	                   		REG_FLD(1, 5)
#define STATUS_FLD_OUT_READY	                   		REG_FLD(1, 4)
#define STATUS_FLD_OF_UNFINISH	                   	REG_FLD(1, 1)
#define STATUS_FLD_IF_UNFINISH	                     REG_FLD(1, 0)
#define CFG_FLD_CHKSUM_SEL	                   		REG_FLD(2, 29)
#define CFG_FLD_CHKSUM_EN	                   			REG_FLD(1, 28)
#define CFG_FLD_CCORR_GAMMA_OFF                  	REG_FLD(1, 5)
#define CFG_FLD_CCORR_EN			                  	REG_FLD(1, 4)
#define CFG_FLD_DITHER_EN			                  	REG_FLD(1, 2)
#define CFG_FLD_GAMMA_LUT_EN                 		REG_FLD(1, 1)
#define CFG_FLD_RELAY_MODE                 			REG_FLD(1, 0)
#define INPUT_COUNT_FLD_INP_LINE_CNT 			REG_FLD(13, 16)
#define INPUT_COUNT_FLD_INP_PIX_CNT 			REG_FLD(13, 0)
#define OUTPUT_COUNT_FLD_OUTP_LINE_CNT 		REG_FLD(13, 16)
#define OUTPUT_COUNT_FLD_OUTP_PIX_CNT 		REG_FLD(13, 0)
#define CHKSUM_FLD_CHKSUM 					REG_FLD(30, 0)
#define SIZE_FLD_HSIZE 							REG_FLD(13, 16)
#define SIZE_FLD_VSIZE 							REG_FLD(13, 0)
#define CCORR_0_FLD_CCORR_C00 					REG_FLD(12, 16)
#define CCORR_0_FLD_CCORR_C01 					REG_FLD(12, 0)
#define CCORR_1_FLD_CCORR_C02 					REG_FLD(12, 16)
#define CCORR_1_FLD_CCORR_C10 					REG_FLD(12, 0)
#define CCORR_2_FLD_CCORR_C11 					REG_FLD(12, 16)
#define CCORR_2_FLD_CCORR_C12 					REG_FLD(12, 0)
#define CCORR_3_FLD_CCORR_C20 					REG_FLD(12, 16)
#define CCORR_3_FLD_CCORR_C21 					REG_FLD(12, 0)
#define CCORR_4_FLD_CCORR_C22 					REG_FLD(12, 16)
#define DUMMY_REG_FLD_DUMMY_REG				REG_FLD(32, 0)
#define DITHER_0_FLD_CRC_CLR					REG_FLD(1, 24)
#define DITHER_0_FLD_CRC_START				REG_FLD(1, 20)
#define DITHER_0_FLD_CRC_CEN					REG_FLD(1, 16)
#define DITHER_0_FLD_FRAME_DONE_DEL			REG_FLD(8, 8)
#define DITHER_0_FLD_OUT_SEL					REG_FLD(1, 4)
#define DITHER_5_FLD_W_DEMO					REG_FLD(16, 0)
#define DITHER_6_FLD_WRAP_MODE				REG_FLD(1, 16)
#define DITHER_6_FLD_LEFT_EN					REG_FLD(2, 14)
#define DITHER_6_FLD_FPHASE_R					REG_FLD(1, 13)
#define DITHER_6_FLD_FPHASE_EN				REG_FLD(1, 12)
#define DITHER_6_FLD_FPHASE					REG_FLD(6, 4)
#define DITHER_6_FLD_ROUND_EN					REG_FLD(1, 3)
#define DITHER_6_FLD_RDITHER_EN				REG_FLD(1, 2)
#define DITHER_6_FLD_LFSR_EN					REG_FLD(1, 1)
#define DITHER_6_FLD_EDITHER_EN				REG_FLD(1, 0)
#define DITHER_7_FLD_DRMOD_B					REG_FLD(2, 8)
#define DITHER_7_FLD_DRMOD_G					REG_FLD(2, 4)
#define DITHER_7_FLD_DRMOD_R					REG_FLD(2, 0)
#define GAMMA_DITHER_8_FLD_INK_DATA_R		REG_FLD(12, 16)
#define DITHER_8_FLD_INK						REG_FLD(1, 0)
#define GAMMA_DITHER_9_FLD_INK_DATA_B		REG_FLD(12, 16)
#define GAMMA_DITHER_9_FLD_INK_DATA_G		REG_FLD(12, 0)
#define DITHER_10_FLD_FPHASE_BIT				REG_FLD(3, 8)
#define DITHER_10_FLD_FPHASE_SEL				REG_FLD(2, 4)
#define DITHER_10_FLD_FPHASE_CTRL				REG_FLD(2, 0)
#define DITHER_11_FLD_SUB_B					REG_FLD(2, 12)
#define DITHER_11_FLD_SUB_G					REG_FLD(2, 8)
#define DITHER_11_FLD_SUB_R					REG_FLD(2, 4)
#define DITHER_11_FLD_SUBPIX_EN				REG_FLD(1, 0)
#define DITHER_12_FLD_H_ACTIVE				REG_FLD(16, 16)
#define DITHER_12_FLD_TABLE_EN				REG_FLD(2, 4)
#define DITHER_12_FLD_LSB_OFF					REG_FLD(1, 0)
#define DITHER_13_FLD_RSHIFT_B					REG_FLD(3, 8)
#define DITHER_13_FLD_RSHIFT_G				REG_FLD(3, 4)
#define DITHER_13_FLD_RSHIFT_R					REG_FLD(3, 0)
#define DITHER_14_FLD_DEBUG_MODE				REG_FLD(2, 8)
#define DITHER_14_FLD_DIFF_SHIFT				REG_FLD(3, 4)
#define DITHER_14_FLD_TESTPIN_EN				REG_FLD(1, 0)
#define DITHER_15_FLD_LSB_ERR_SHIFT_R			REG_FLD(3, 28)
#define DITHER_15_FLD_LSB_OVFLW_BIT_R		REG_FLD(3, 24)
#define DITHER_15_FLD_LSB_ADD_LSHIFT_R		REG_FLD(3, 20)
#define DITHER_15_FLD_LSB_INPUT_RSHIFT_R		REG_FLD(3, 16)
#define DITHER_15_FLD_LSB_NEW_BIT_MODE		REG_FLD(1, 0)
#define DITHER_16_FLD_LSB_ERR_SHIFT_B			REG_FLD(3, 28)
#define DITHER_16_FLD_OVFLW_BIT_B				REG_FLD(3, 24)
#define DITHER_16_FLD_ADD_LSHIFT_B			REG_FLD(3, 20)
#define DITHER_16_FLD_INPUT_RSHIFT_B			REG_FLD(3, 16)
#define DITHER_16_FLD_LSB_ERR_SHIFT_G			REG_FLD(3, 12)
#define DITHER_16_FLD_OVFLW_BIT_G				REG_FLD(3, 8)
#define DITHER_16_FLD_ADD_LSHIFT_G			REG_FLD(3, 4)
#define DITHER_16_FLD_INPUT_RSHIFT_G			REG_FLD(3, 0)
#define DITHER_17_FLD_CRC_RDY					REG_FLD(1, 16)
#define DITHER_17_FLD_CRC_OUT					REG_FLD(16, 0)
#define LUT_FLD_GAMMA_LUT_R					REG_FLD(10, 20)
#define LUT_FLD_GAMMA_LUT_G					REG_FLD(10, 10)
#define LUT_FLD_GAMMA_LUT_B					REG_FLD(10, 0)

// -------------------------------------------------------------
// MERGE
#define DISP_REG_MERGE_ENABLE                                		(DISPSYS_MERGE_BASE + 0x000)
#define DISP_REG_MERGE_SW_RESET                              		(DISPSYS_MERGE_BASE + 0x004)
#define DISP_REG_MERGE_DEBUG                                 		(DISPSYS_MERGE_BASE + 0x008)

#define ENABLE_FLD_MERGE_EN                             			REG_FLD(1, 0)
#define SW_RESET_FLD_MERGE_SW_RST                    		REG_FLD(1, 0)
#define DEBUG_FLD_MERGE_FSM                             			REG_FLD(3, 29)
#define DEBUG_FLD_OUT_PIXEL_CNT                         			REG_FLD(24, 0)

// -------------------------------------------------------------
// MUTEX
#define DISP_REG_CONFIG_MUTEX_INTEN                              	(DISPSYS_MUTEX_BASE + 0x000)
#define DISP_REG_CONFIG_MUTEX_INTSTA                                (DISPSYS_MUTEX_BASE + 0x004)
#define DISP_REG_CONFIG_MUTEX0_EN                                	(DISPSYS_MUTEX_BASE + 0x020)
#define DISP_REG_CONFIG_MUTEX0_RST                               	(DISPSYS_MUTEX_BASE + 0x028)
#define DISP_REG_CONFIG_MUTEX0_MOD                               	(DISPSYS_MUTEX_BASE + 0x02C)
#define DISP_REG_CONFIG_MUTEX0_SOF                               	(DISPSYS_MUTEX_BASE + 0x030)
#define DISP_REG_CONFIG_MUTEX1_EN                                	(DISPSYS_MUTEX_BASE + 0x040)
#define DISP_REG_CONFIG_MUTEX1_RST                               	(DISPSYS_MUTEX_BASE + 0x048)
#define DISP_REG_CONFIG_MUTEX1_MOD                               	(DISPSYS_MUTEX_BASE + 0x04C)
#define DISP_REG_CONFIG_MUTEX1_SOF                               	(DISPSYS_MUTEX_BASE + 0x050)
#define DISP_REG_CONFIG_MUTEX2_EN                                	(DISPSYS_MUTEX_BASE + 0x060)
#define DISP_REG_CONFIG_MUTEX2_RST                               	(DISPSYS_MUTEX_BASE + 0x068)
#define DISP_REG_CONFIG_MUTEX2_MOD                               	(DISPSYS_MUTEX_BASE + 0x06C)
#define DISP_REG_CONFIG_MUTEX2_SOF                               	(DISPSYS_MUTEX_BASE + 0x070)
#define DISP_REG_CONFIG_MUTEX3_EN                                	(DISPSYS_MUTEX_BASE + 0x080)
#define DISP_REG_CONFIG_MUTEX3_RST                               	(DISPSYS_MUTEX_BASE + 0x088)
#define DISP_REG_CONFIG_MUTEX3_MOD                               	(DISPSYS_MUTEX_BASE + 0x08C)
#define DISP_REG_CONFIG_MUTEX3_SOF                               	(DISPSYS_MUTEX_BASE + 0x090)
#define DISP_REG_CONFIG_MUTEX4_EN                                	(DISPSYS_MUTEX_BASE + 0x0A0)
#define DISP_REG_CONFIG_MUTEX4_RST                               	(DISPSYS_MUTEX_BASE + 0x0A8)
#define DISP_REG_CONFIG_MUTEX4_MOD                               	(DISPSYS_MUTEX_BASE + 0x0AC)
#define DISP_REG_CONFIG_MUTEX4_SOF                               	(DISPSYS_MUTEX_BASE + 0x0B0)
#define DISP_REG_CONFIG_MUTEX5_EN                                	(DISPSYS_MUTEX_BASE + 0x0C0)
#define DISP_REG_CONFIG_MUTEX5_RST                               	(DISPSYS_MUTEX_BASE + 0x0C8)
#define DISP_REG_CONFIG_MUTEX5_MOD                               	(DISPSYS_MUTEX_BASE + 0x0CC)
#define DISP_REG_CONFIG_MUTEX5_SOF                               	(DISPSYS_MUTEX_BASE + 0x0D0)
#define DISP_REG_CONFIG_DEBUG_OUT_SEL                          	    (DISPSYS_MUTEX_BASE + 0x200)

#define DISP_REG_CONFIG_MUTEX_EN(n)   								(DISP_REG_CONFIG_MUTEX0_EN + (0x20 * n))
#define DISP_REG_CONFIG_MUTEX_RST(n)   								(DISP_REG_CONFIG_MUTEX0_RST + (0x20 * n))
#define DISP_REG_CONFIG_MUTEX_MOD(n)   							    (DISP_REG_CONFIG_MUTEX0_MOD + (0x20 * n))
#define DISP_REG_CONFIG_MUTEX_SOF(n)   								(DISP_REG_CONFIG_MUTEX0_SOF + (0x20 * n))


#define INTEN_FLD_MUTEX_INTEN                                  		REG_FLD(12, 0)
#define INTSTA_FLD_MUTEX_INTSTA                                		REG_FLD(12, 0)
#define EN_FLD_MUTEX0_EN                           				    REG_FLD(1, 0)
#define RST_FLD_MUTEX0_RST                         				    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                     				        REG_FLD(26, 0)
#define SOF_FLD_MUTEX0_SOF_TIMING       				            REG_FLD(1, 4)
#define SOF_FLD_MUTEX0_SOF                     				        REG_FLD(4, 0)
#define EN_FLD_MUTEX1_EN                           				    REG_FLD(1, 0)
#define RST_FLD_MUTEX1_RST                         				    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                   					    REG_FLD(26, 0)
#define SOF_FLD_MUTEX1_SOF_TIMING       				            REG_FLD(1, 4)
#define SOF_FLD_MUTEX1_SOF                      				    REG_FLD(4, 0)
#define EN_FLD_MUTEX2_EN                           				    REG_FLD(1, 0)
#define RST_FLD_MUTEX2_RST                        				    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                      				    REG_FLD(26, 0)
#define SOF_FLD_MUTEX2_SOF_TIMING     					            REG_FLD(1, 4)
#define SOF_FLD_MUTEX2_SOF                     				        REG_FLD(4, 0)
#define EN_FLD_MUTEX3_EN                           				    REG_FLD(1, 0)
#define RST_FLD_MUTEX3_RST                         				    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                 					        REG_FLD(26, 0)
#define SOF_FLD_MUTEX3_SOF_TIMING        				            REG_FLD(1, 4)
#define SOF_FLD_MUTEX3_SOF                     				        REG_FLD(4, 0)
#define EN_FLD_MUTEX4_EN                           				    REG_FLD(1, 0)
#define RST_FLD_MUTEX4_RST                         				    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                  					    REG_FLD(26, 0)
#define SOF_FLD_MUTEX4_SOF_TIMING        				            REG_FLD(1, 4)
#define SOF_FLD_MUTEX4_SOF                      				    REG_FLD(4, 0)
#define EN_FLD_MUTEX5_EN                          				    REG_FLD(1, 0)
#define RST_FLD_MUTEX5_RST                     					    REG_FLD(1, 0)
#define MOD_FLD_MUTEX0_MOD                					        REG_FLD(26, 0)
#define SOF_FLD_MUTEX5_SOF_TIMING     					            REG_FLD(1, 4)
#define SOF_FLD_MUTEX5_SOF                    					    REG_FLD(4, 0)
#define DEBUG_OUT_SEL_FLD_DEBUG_OUT_SEL                  	        REG_FLD(2, 0)

// -------------------------------------------------------------
// OD
#define DISP_REG_OD_EN           (DISPSYS_OD_BASE+0x000)
#define DISP_REG_OD_RESET        (DISPSYS_OD_BASE+0x004)
#define DISP_REG_OD_INTEN        (DISPSYS_OD_BASE+0x008)
#define DISP_REG_OD_INTSTA       (DISPSYS_OD_BASE+0x00C)
#define DISP_REG_OD_STATUS       (DISPSYS_OD_BASE+0x010)
#define DISP_REG_OD_CFG          (DISPSYS_OD_BASE+0x020)
#define DISP_REG_OD_INPUT_COUNT	 (DISPSYS_OD_BASE+0x024)
#define DISP_REG_OD_OUTPUT_COUNT (DISPSYS_OD_BASE+0x028)
#define DISP_REG_OD_CHKSUM       (DISPSYS_OD_BASE+0x02C)
#define DISP_REG_OD_SIZE	     (DISPSYS_OD_BASE+0x030)
#define DISP_REG_OD_HSYNC_WIDTH  (DISPSYS_OD_BASE+0x040)
#define DISP_REG_OD_VSYNC_WIDTH	 (DISPSYS_OD_BASE+0x044)
#define DISP_REG_OD_MISC         (DISPSYS_OD_BASE+0x048)
#define DISP_REG_OD_DUMMY_REG    (DISPSYS_OD_BASE+0x0C0)
#define DISP_REG_OD_DITHER_0	   (DISPSYS_OD_BASE+0x100)
#define DISP_REG_OD_DITHER_5     (DISPSYS_OD_BASE+0x114)
#define DISP_REG_OD_DITHER_6     (DISPSYS_OD_BASE+0x118)
#define DISP_REG_OD_DITHER_7	   (DISPSYS_OD_BASE+0x11C)
#define DISP_REG_OD_DITHER_8	   (DISPSYS_OD_BASE+0x120)
#define DISP_REG_OD_DITHER_9	   (DISPSYS_OD_BASE+0x124)
#define DISP_REG_OD_DITHER_10	   (DISPSYS_OD_BASE+0x128)
#define DISP_REG_OD_DITHER_11	   (DISPSYS_OD_BASE+0x12C)
#define DISP_REG_OD_DITHER_12	   (DISPSYS_OD_BASE+0x130)
#define DISP_REG_OD_DITHER_13	   (DISPSYS_OD_BASE+0x134)
#define DISP_REG_OD_DITHER_14	   (DISPSYS_OD_BASE+0x138)
#define DISP_REG_OD_DITHER_15	   (DISPSYS_OD_BASE+0x13C)
#define DISP_REG_OD_DITHER_16	   (DISPSYS_OD_BASE+0x140)
#define DISP_REG_OD_DITHER_17    (DISPSYS_OD_BASE+0x144)

#define OD_INPUT_COUNT_FLD_INP_PIX_CNT				REG_FLD(13, 0)
#define OD_INPUT_COUNT_FLD_INP_LINE_CNT				REG_FLD(13, 16)
#define OD_OUTPUT_COUNT_FLD_OUTP_PIX_CNT			REG_FLD(13, 0)
#define OD_OUTPUT_COUNT_FLD_OUTP_LINE_CNT			REG_FLD(13, 16)

// -------------------------------------------------------------
// OVL
#define DISP_REG_OVL_STA			(DISPSYS_OVL0_BASE + 0x000)
#define DISP_REG_OVL_INTEN			(DISPSYS_OVL0_BASE + 0x004)
#define DISP_REG_OVL_INTSTA			(DISPSYS_OVL0_BASE + 0x008)
#define DISP_REG_OVL_EN				(DISPSYS_OVL0_BASE + 0x00C)
#define DISP_REG_OVL_TRIG			(DISPSYS_OVL0_BASE + 0x010)
#define DISP_REG_OVL_RST			(DISPSYS_OVL0_BASE + 0x014)
#define DISP_REG_OVL_ROI_SIZE			(DISPSYS_OVL0_BASE + 0x020)
#define DISP_REG_OVL_DATAPATH_CON		(DISPSYS_OVL0_BASE + 0x024)
#define DISP_REG_OVL_ROI_BGCLR			(DISPSYS_OVL0_BASE + 0x028)
#define DISP_REG_OVL_SRC_CON			(DISPSYS_OVL0_BASE + 0x02C)
#define DISP_REG_OVL_L0_CON			(DISPSYS_OVL0_BASE + 0x030)
#define DISP_REG_OVL_L0_SRCKEY			(DISPSYS_OVL0_BASE + 0x034)
#define DISP_REG_OVL_L0_SRC_SIZE		(DISPSYS_OVL0_BASE + 0x038)
#define DISP_REG_OVL_L0_OFFSET			(DISPSYS_OVL0_BASE + 0x03C)
#define DISP_REG_OVL_L0_ADDR			(DISPSYS_OVL0_BASE + 0xf40)
#define DISP_REG_OVL_L0_PITCH			(DISPSYS_OVL0_BASE + 0x044)
#define DISP_REG_OVL_L1_CON			(DISPSYS_OVL0_BASE + 0x050)
#define DISP_REG_OVL_L1_SRCKEY			(DISPSYS_OVL0_BASE + 0x054)
#define DISP_REG_OVL_L1_SRC_SIZE		(DISPSYS_OVL0_BASE + 0x058)
#define DISP_REG_OVL_L1_OFFSET			(DISPSYS_OVL0_BASE + 0x05C)
#define DISP_REG_OVL_L1_ADDR			(DISPSYS_OVL0_BASE + 0xf60)
#define DISP_REG_OVL_L1_PITCH			(DISPSYS_OVL0_BASE + 0x064)
#define DISP_REG_OVL_L2_CON			(DISPSYS_OVL0_BASE + 0x070)
#define DISP_REG_OVL_L2_SRCKEY			(DISPSYS_OVL0_BASE + 0x074)
#define DISP_REG_OVL_L2_SRC_SIZE		(DISPSYS_OVL0_BASE + 0x078)
#define DISP_REG_OVL_L2_OFFSET			(DISPSYS_OVL0_BASE + 0x07C)
#define DISP_REG_OVL_L2_ADDR			(DISPSYS_OVL0_BASE + 0xf80)
#define DISP_REG_OVL_L2_PITCH			(DISPSYS_OVL0_BASE + 0x084)
#define DISP_REG_OVL_L3_CON			(DISPSYS_OVL0_BASE + 0x090)
#define DISP_REG_OVL_L3_SRCKEY			(DISPSYS_OVL0_BASE + 0x094)
#define DISP_REG_OVL_L3_SRC_SIZE		(DISPSYS_OVL0_BASE + 0x098)
#define DISP_REG_OVL_L3_OFFSET			(DISPSYS_OVL0_BASE + 0x09C)
#define DISP_REG_OVL_L3_ADDR			(DISPSYS_OVL0_BASE + 0xfA0)
#define DISP_REG_OVL_L3_PITCH			(DISPSYS_OVL0_BASE + 0x0A4)
#define DISP_REG_OVL_RDMA0_CTRL			(DISPSYS_OVL0_BASE + 0x0C0)
#define DISP_REG_OVL_RDMA0_FIFO_CTRL		(DISPSYS_OVL0_BASE + 0x0D0)
#define DISP_REG_OVL_RDMA1_CTRL			(DISPSYS_OVL0_BASE + 0x0E0)
#define DISP_REG_OVL_RDMA1_FIFO_CTRL		(DISPSYS_OVL0_BASE + 0x0F0)
#define DISP_REG_OVL_RDMA2_CTRL			(DISPSYS_OVL0_BASE + 0x100)
#define DISP_REG_OVL_RDMA2_FIFO_CTRL		(DISPSYS_OVL0_BASE + 0x110)
#define DISP_REG_OVL_RDMA3_CTRL			(DISPSYS_OVL0_BASE + 0x120)
#define DISP_REG_OVL_RDMA3_FIFO_CTRL		(DISPSYS_OVL0_BASE + 0x130)
#define DISP_REG_OVL_DEBUG_MON_SEL		(DISPSYS_OVL0_BASE + 0x1D4)
#define DISP_REG_OVL_RDMA0_MEM_GMC_S2		(DISPSYS_OVL0_BASE + 0x1E0)
#define DISP_REG_OVL_RDMA1_MEM_GMC_S2		(DISPSYS_OVL0_BASE + 0x1E4)
#define DISP_REG_OVL_RDMA2_MEM_GMC_S2		(DISPSYS_OVL0_BASE + 0x1E8)
#define DISP_REG_OVL_RDMA3_MEM_GMC_S2		(DISPSYS_OVL0_BASE + 0x1EC)
#define DISP_REG_OVL_DUMMY_REG			(DISPSYS_OVL0_BASE + 0x200)
#define DISP_REG_OVL_FLOW_CTRL_DBG		(DISPSYS_OVL0_BASE + 0x240)
#define DISP_REG_OVL_ADDCON_DBG			(DISPSYS_OVL0_BASE + 0x244)
#define DISP_REG_OVL_RDMA0_DBG			(DISPSYS_OVL0_BASE + 0x24C)
#define DISP_REG_OVL_RDMA1_DBG			(DISPSYS_OVL0_BASE + 0x250)
#define DISP_REG_OVL_RDMA2_DBG			(DISPSYS_OVL0_BASE + 0x254)
#define DISP_REG_OVL_RDMA3_DBG			(DISPSYS_OVL0_BASE + 0x258)


#define DATAPATH_CON_FLD_LAYER_SMI_ID_EN	REG_FLD(1, 0)

#define SRC_CON_FLD_L3_EN			REG_FLD(1, 3)
#define SRC_CON_FLD_L2_EN			REG_FLD(1, 2)
#define SRC_CON_FLD_L1_EN			REG_FLD(1, 1)
#define SRC_CON_FLD_L0_EN			REG_FLD(1, 0)

#define L_CON_FLD_SKEN				REG_FLD(1, 30)
#define L_CON_FLD_LARC				REG_FLD(2, 28)
#define L_CON_FLD_BTSW				REG_FLD(1, 24)
#define L_CON_FLD_MTX				REG_FLD(4, 16)
#define L_CON_FLD_CFMT				REG_FLD(4, 12)
#define L_CON_FLD_H_FLIP			REG_FLD(1, 10)
#define L_CON_FLD_V_FLIP			REG_FLD(1, 9)
#define L_CON_FLD_AEN				REG_FLD(1, 8)
#define L_CON_FLD_APHA				REG_FLD(8, 0)

#define L_PITCH_FLD_LSP				REG_FLD(16, 0)
#define L_PITCH_FLD_SUR_ALFA		REG_FLD(16, 16)

#define ADDCON_DBG_FLD_L3_WIN_HIT		REG_FLD(1, 31)
#define ADDCON_DBG_FLD_L2_WIN_HIT		REG_FLD(1, 30)
#define ADDCON_DBG_FLD_ROI_Y			REG_FLD(13, 16)
#define ADDCON_DBG_FLD_L1_WIN_HIT		REG_FLD(1, 15)
#define ADDCON_DBG_FLD_L0_WIN_HIT		REG_FLD(1, 14)
#define ADDCON_DBG_FLD_ROI_X			REG_FLD(13, 0)

// -------------------------------------------------------------
// RDMA
#define DISP_REG_RDMA_INT_ENABLE                              	(DISPSYS_RDMA0_BASE+0x000)
#define DISP_REG_RDMA_INT_STATUS                             		(DISPSYS_RDMA0_BASE+0x004)
#define DISP_REG_RDMA_GLOBAL_CON                               	(DISPSYS_RDMA0_BASE+0x010)
#define DISP_REG_RDMA_SIZE_CON_0                                   	(DISPSYS_RDMA0_BASE+0x014)
#define DISP_REG_RDMA_SIZE_CON_1                                   	(DISPSYS_RDMA0_BASE+0x018)
#define DISP_REG_RDMA_TARGET_LINE                                  	(DISPSYS_RDMA0_BASE+0x01C)
#define DISP_REG_RDMA_MEM_CON                                      	(DISPSYS_RDMA0_BASE+0x024)
#define DISP_REG_RDMA_MEM_SRC_PITCH                      		(DISPSYS_RDMA0_BASE+0x02C)
#define DISP_REG_RDMA_MEM_GMC_SETTING_0                 	(DISPSYS_RDMA0_BASE+0x030)
#define DISP_REG_RDMA_MEM_SLOW_CON                           	(DISPSYS_RDMA0_BASE+0x034)
#define DISP_REG_RDMA_MEM_GMC_SETTING_1                 	(DISPSYS_RDMA0_BASE+0x038)
#define DISP_REG_RDMA_FIFO_CON                                     	(DISPSYS_RDMA0_BASE+0x040)
#define DISP_REG_RDMA_FIFO_LOG                                     	(DISPSYS_RDMA0_BASE+0x044)
#define DISP_REG_RDMA_C00                                          		(DISPSYS_RDMA0_BASE+0x054)
#define DISP_REG_RDMA_C01                                          		(DISPSYS_RDMA0_BASE+0x058)
#define DISP_REG_RDMA_C02                                          		(DISPSYS_RDMA0_BASE+0x05C)
#define DISP_REG_RDMA_C10                                          		(DISPSYS_RDMA0_BASE+0x060)
#define DISP_REG_RDMA_C11                                          		(DISPSYS_RDMA0_BASE+0x064)
#define DISP_REG_RDMA_C12                                          		(DISPSYS_RDMA0_BASE+0x068)
#define DISP_REG_RDMA_C20                                          		(DISPSYS_RDMA0_BASE+0x06C)
#define DISP_REG_RDMA_C21                                          		(DISPSYS_RDMA0_BASE+0x070)
#define DISP_REG_RDMA_C22                                          		(DISPSYS_RDMA0_BASE+0x074)
#define DISP_REG_RDMA_PRE_ADD_0                                	(DISPSYS_RDMA0_BASE+0x078)
#define DISP_REG_RDMA_PRE_ADD_1                                    	(DISPSYS_RDMA0_BASE+0x07C)
#define DISP_REG_RDMA_PRE_ADD_2                                    	(DISPSYS_RDMA0_BASE+0x080)
#define DISP_REG_RDMA_POST_ADD_0                              	(DISPSYS_RDMA0_BASE+0x084)
#define DISP_REG_RDMA_POST_ADD_1                                	(DISPSYS_RDMA0_BASE+0x088)
#define DISP_REG_RDMA_POST_ADD_2                                	(DISPSYS_RDMA0_BASE+0x08C)
#define DISP_REG_RDMA_DUMMY                                        	(DISPSYS_RDMA0_BASE+0x090)
#define DISP_REG_RDMA_DEBUG_OUT_SEL                            	(DISPSYS_RDMA0_BASE+0x094)
#define DISP_REG_RDMA_IN_PXL_CNT                            	(DISPSYS_RDMA0_BASE+0x0f0)
#define DISP_REG_RDMA_IN_LINE_CNT                            	(DISPSYS_RDMA0_BASE+0x0f4)
#define DISP_REG_RDMA_OUT_PXL_CNT                            	(DISPSYS_RDMA0_BASE+0x0f8)
#define DISP_REG_RDMA_OUT_LINE_CNT                            	(DISPSYS_RDMA0_BASE+0x0fC)
#define DISP_REG_RDMA_MEM_START_ADDR                     	(DISPSYS_RDMA0_BASE+0xf00)

#define INT_ENABLE_FLD_TARGET_LINE_INT_EN               	REG_FLD(1, 5)
#define INT_ENABLE_FLD_FIFO_UNDERFLOW_INT_EN          	REG_FLD(1, 4)
#define INT_ENABLE_FLD_EOF_ABNORMAL_INT_EN              	REG_FLD(1, 3)
#define INT_ENABLE_FLD_FRAME_END_INT_EN                    	REG_FLD(1, 2)
#define INT_ENABLE_FLD_FRAME_START_INT_EN                	REG_FLD(1, 1)
#define INT_ENABLE_FLD_REG_UPDATE_INT_EN                 	REG_FLD(1, 0)
#define INT_STATUS_FLD_TARGET_LINE_INT_FLAG           	REG_FLD(1, 5)
#define INT_STATUS_FLD_FIFO_UNDERFLOW_INT_FLAG   	REG_FLD(1, 4)
#define INT_STATUS_FLD_EOF_ABNORMAL_INT_FLAG           	REG_FLD(1, 3)
#define INT_STATUS_FLD_FRAME_END_INT_FLAG                 	REG_FLD(1, 2)
#define INT_STATUS_FLD_FRAME_START_INT_FLAG          	REG_FLD(1, 1)
#define INT_STATUS_FLD_REG_UPDATE_INT_FLAG              	REG_FLD(1, 0)
#define GLOBAL_CON_FLD_SMI_BUSY                                	REG_FLD(1, 12)
#define GLOBAL_CON_FLD_RESET_STATE                             	REG_FLD(3, 8)
#define GLOBAL_CON_FLD_SOFT_RESET                              	REG_FLD(1, 4)
#define GLOBAL_CON_FLD_MODE_SEL                                	REG_FLD(1, 1)
#define GLOBAL_CON_FLD_ENGINE_EN                               	REG_FLD(1, 0)
#define SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL                	REG_FLD(4, 20)
#define SIZE_CON_0_FLD_MATRIX_WIDE_GAMUT_EN            	REG_FLD(1, 18)
#define SIZE_CON_0_FLD_MATRIX_ENABLE                           	REG_FLD(1, 17)
#define SIZE_CON_0_FLD_MATRIX_EXT_MTX_EN                	REG_FLD(1, 16)
#define SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH              	REG_FLD(12, 0)
#define SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT             	REG_FLD(20, 0)
#define TARGET_LINE_FLD_TARGET_LINE                            	REG_FLD(20, 0)
#define MEM_CON_FLD_MEM_MODE_HORI_BLOCK_NUM      	REG_FLD(8, 24)
#define MEM_CON_FLD_MEM_MODE_INPUT_COSITE          	REG_FLD(1, 13)
#define MEM_CON_FLD_MEM_MODE_INPUT_UPSAMPLE          	REG_FLD(1, 12)
#define MEM_CON_FLD_MEM_MODE_INPUT_SWAP          		REG_FLD(1, 8)
#define MEM_CON_FLD_MEM_MODE_INPUT_FORMAT          	REG_FLD(4, 4)
#define MEM_CON_FLD_MEM_MODE_TILE_INTERLACE          	REG_FLD(1, 1)
#define MEM_CON_FLD_MEM_MODE_TILE_EN                           REG_FLD(1, 0)
#define MEM_SRC_PITCH_FLD_MEM_MODE_SRC_PITCH        	REG_FLD(16, 0)
#define MEM_GMC_SETTING_0_FLD_PRE_ULTRA_THRESHOLD_HIGH_OFS 	REG_FLD(8, 24)
#define MEM_GMC_SETTING_0_FLD_ULTRA_THRESHOLD_HIGH_OFS         	REG_FLD(8, 16)
#define MEM_GMC_SETTING_0_FLD_PRE_ULTRA_THRESHOLD_LOW_OFS  	REG_FLD(8, 8)
#define MEM_GMC_SETTING_0_FLD_ULTRA_THRESHOLD_LOW              		REG_FLD(8, 0)
#define MEM_SLOW_CON_FLD_MEM_MODE_SLOW_COUNT                   		REG_FLD(16, 16)
#define MEM_SLOW_CON_FLD_MEM_MODE_SLOW_EN                     		 	REG_FLD(1, 0)
#define MEM_GMC_SETTING_1_FLD_ISSUE_REQ_THRESHOLD              		REG_FLD(8, 0)
#define FIFO_CON_FLD_FIFO_UNDERFLOW_EN                         			REG_FLD(1, 31)
#define FIFO_CON_FLD_FIFO_PSEUDO_SIZE                          				REG_FLD(10, 16)
#define FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD               		REG_FLD(10, 0)
#define FIFO_LOG_FLD_RDMA_FIFO_LOG                   			REG_FLD(10, 0)
#define C00_FLD_DISP_RDMA_C00                                  			REG_FLD(13, 0)
#define C01_FLD_DISP_RDMA_C01                                  			REG_FLD(13, 0)
#define C02_FLD_DISP_RDMA_C02                                  			REG_FLD(13, 0)
#define C10_FLD_DISP_RDMA_C10                                  			REG_FLD(13, 0)
#define C11_FLD_DISP_RDMA_C11                                  			REG_FLD(13, 0)
#define C12_FLD_DISP_RDMA_C12                                  			REG_FLD(13, 0)
#define C20_FLD_DISP_RDMA_C20                                  			REG_FLD(13, 0)
#define C21_FLD_DISP_RDMA_C21                                  			REG_FLD(13, 0)
#define C22_FLD_DISP_RDMA_C22                                  			REG_FLD(13, 0)
#define PRE_ADD_0_FLD_DISP_RDMA_PRE_ADD_0         			REG_FLD(9, 0)
#define PRE_ADD_1_FLD_DISP_RDMA_PRE_ADD_1         			REG_FLD(9, 0)
#define PRE_ADD_2_FLD_DISP_RDMA_PRE_ADD_2        			REG_FLD(9, 0)
#define POST_ADD_0_FLD_DISP_RDMA_POST_ADD_0      			REG_FLD(9, 0)
#define POST_ADD_1_FLD_DISP_RDMA_POST_ADD_1    			REG_FLD(9, 0)
#define POST_ADD_2_FLD_DISP_RDMA_POST_ADD_2    			REG_FLD(9, 0)
#define DUMMY_FLD_DISP_RDMA_DUMMY                              		REG_FLD(32, 0)
#define DEBUG_OUT_SEL_FLD_DISP_RDMA_DEBUG_OUT_SEL   	REG_FLD(4, 0)
#define MEM_START_ADDR_FLD_MEM_MODE_START_ADDR  		REG_FLD(32, 0)

// -------------------------------------------------------------
// SPLIT
#define DISP_REG_SPLIT_ENABLE                            				(DISPSYS_SPLIT0_BASE+0x00)
#define DISP_REG_SPLIT_SW_RESET                          			    (DISPSYS_SPLIT0_BASE+0x04)
#define DISP_REG_SPLIT_DEBUG                             				(DISPSYS_SPLIT0_BASE+0x08)

#define ENABLE_FLD_SPLIT_EN                              				REG_FLD(1, 0)
#define W_RESET_FLD_SPLIT_SW_RST                        			    REG_FLD(1, 0)
#define DEBUG_FLD_SPLIT_FSM                              				REG_FLD(3, 29)
#define DEBUG_FLD_IN_PIXEL_CNT                           				REG_FLD(24, 0)

// -------------------------------------------------------------
// UFO
#define DISP_REG_UFO_START                                     			(DISPSYS_UFOE_BASE+0x000)

#define DISP_REG_UFO_INTEN                                     			(DISPSYS_UFOE_BASE+0x004)
#define DISP_REG_UFO_INTSTA                                    			(DISPSYS_UFOE_BASE+0x008)
#define DISP_REG_UFO_DBUF                                      			(DISPSYS_UFOE_BASE+0x00C)
#define DISP_REG_UFO_CRC                                       				(DISPSYS_UFOE_BASE+0x014)
#define DISP_REG_UFO_SW_SCRATCH                                		(DISPSYS_UFOE_BASE+0x018)
#define DISP_REG_UFO_CR0P6_PAD	                                		(DISPSYS_UFOE_BASE+0x020)
#define DISP_REG_UFO_LR_OVERLAP							(DISPSYS_UFOE_BASE+0x024)
#define DISP_REG_UFO_CK_ON                                     			(DISPSYS_UFOE_BASE+0x028)
#define DISP_REG_UFO_FRAME_WIDTH                               		(DISPSYS_UFOE_BASE+0x050)
#define DISP_REG_UFO_FRAME_HEIGHT                              		(DISPSYS_UFOE_BASE+0x054)
#define DISP_REG_UFO_OUTEN								(DISPSYS_UFOE_BASE+0x058)

#define DISP_REG_UFO_R0_CRC                                    			(DISPSYS_UFOE_BASE+0x0F0)

#define DISP_REG_UFO_CFG_0B                           				(DISPSYS_UFOE_BASE+0x100)
#define DISP_REG_UFO_CFG_1B                                    			(DISPSYS_UFOE_BASE+0x104)
#define DISP_REG_UFO_CFG_2B                                    			(DISPSYS_UFOE_BASE+0x108)
#define DISP_REG_UFO_CFG_3B                                    			(DISPSYS_UFOE_BASE+0x10C)
#define DISP_REG_UFO_CFG_4B                                    			(DISPSYS_UFOE_BASE+0x110)

#define DISP_REG_UFO_RO_0B                                     			(DISPSYS_UFOE_BASE+0x120)
#define DISP_REG_UFO_RO_1B                                     			(DISPSYS_UFOE_BASE+0x124)
#define DISP_REG_UFO_RO_2B                                     			(DISPSYS_UFOE_BASE+0x128)
#define DISP_REG_UFO_RO_3B                                     			(DISPSYS_UFOE_BASE+0x12C)
#define DISP_REG_UFO_RO_4B                                     			(DISPSYS_UFOE_BASE+0x130)

#define START_FLD_DISP_UFO_START                      				REG_FLD(1, 0)
#define START_FLD_DISP_UFO_OUT_SEL                    			REG_FLD(1, 1)
#define START_FLD_DISP_UFO_BYPASS                     				REG_FLD(1, 2)
#define START_FLD_DISP_UFO_LR_EN                     				REG_FLD(1, 3)
#define START_FLD_DISP_UFO_SW_RST_ENGINE              		REG_FLD(1, 8)

#define CFG_0B_FLD_DISP_UFO_CFG_VLC_EN     	               			REG_FLD(1, 0)
#define CFG_0B_FLD_DISP_UFO_CFG_COM_RATIO    	               			REG_FLD(2,1)
#define CFG_1B_FLD_DISP_UFO_CFG_1B                    			REG_FLD(32, 0)
#define CFG_2B_FLD_DISP_UFO_CFG_2B                    			REG_FLD(32, 0)
#define CFG_3B_FLD_DISP_UFO_CFG_3B                    			REG_FLD(32, 0)
#define CFG_4B_FLD_DISP_UFO_CFG_4B                    			REG_FLD(32, 0)

#define RO_0B_FLD_DISP_UFO_RO_0B                      				REG_FLD(32, 0)
#define RO_1B_FLD_DISP_UFO_RO_1B                      				REG_FLD(32, 0)
#define RO_2B_FLD_DISP_UFO_RO_2B                      				REG_FLD(32, 0)
#define RO_3B_FLD_DISP_UFO_RO_3B                      				REG_FLD(32, 0)
#define RO_4B_FLD_DISP_UFO_RO_4B                     				REG_FLD(32, 0)
#define START_FLD_DISP_UFO_DBG_SEL                    			REG_FLD(8, 16)

#define CR0P6_PAD_FLD_DISP_UFO_STR_PAD_NUM                   			REG_FLD(3, 0)

#define INTEN_FLD_DISP_UFO_INTEN_FR_UNDERRUN    			REG_FLD(1, 2)
#define INTEN_FLD_DISP_UFO_INTEN_FR_DONE              		REG_FLD(1, 1)
#define INTEN_FLD_DISP_UFO_INTEN_FR_COMPLETE     		 	REG_FLD(1, 0)
#define INTSTA_FLD_DISP_UFO_INTSTA_FR_UNDERRUN    		REG_FLD(1, 2)
#define INTSTA_FLD_DISP_UFO_INTSTA_FR_DONE            		REG_FLD(1, 1)
#define INTSTA_FLD_DISP_UFO_INTSTA_FR_COMPLETE   		REG_FLD(1, 0)

#define DBUF_FLD_DISP_UFO_DBUF_DIS                    			REG_FLD(5, 0)
#define CRC_FLD_DISP_UFO_CRC_CLR                      				REG_FLD(1, 2)
#define CRC_FLD_DISP_UFO_CRC_START                    			REG_FLD(1, 1)
#define CRC_FLD_DISP_UFO_CRC_CEN                      				REG_FLD(1, 0)

#define SW_SCRATCH_FLD_DISP_UFO_SW_SCRATCH  			REG_FLD(32, 0)
#define CK_ON_FLD_DISP_UFO_CK_ON                      				REG_FLD(1, 0)
#define FRAME_WIDTH_FLD_DISP_UFO_FRAME_WIDTH  			REG_FLD(12, 0)
#define FRAME_HEIGHT_FLD_DISP_UFO_FRAME_HEIGHT 		REG_FLD(13, 0)
#define R0_CRC_FLD_DISP_UFO_ENGINE_END                			REG_FLD(1, 17)
#define R0_CRC_FLD_DISP_UFO_CRC_RDY_0                 			REG_FLD(1, 16)
#define R0_CRC_FLD_DISP_UFO_CRC_OUT_0                 			REG_FLD(16, 0)

// -------------------------------------------------------------
// WDMA
#define DISP_REG_WDMA_INTEN                                   			(DISPSYS_WDMA0_BASE+0x000)
#define DISP_REG_WDMA_INTSTA                                  			(DISPSYS_WDMA0_BASE+0x004)
#define DISP_REG_WDMA_EN                                      			(DISPSYS_WDMA0_BASE+0x008)
#define DISP_REG_WDMA_RST                                     			(DISPSYS_WDMA0_BASE+0x00C)
#define DISP_REG_WDMA_SMI_CON                                 			(DISPSYS_WDMA0_BASE+0x010)
#define DISP_REG_WDMA_CFG                                     			(DISPSYS_WDMA0_BASE+0x014)
#define DISP_REG_WDMA_SRC_SIZE                                			(DISPSYS_WDMA0_BASE+0x018)
#define DISP_REG_WDMA_CLIP_SIZE                               			(DISPSYS_WDMA0_BASE+0x01C)
#define DISP_REG_WDMA_CLIP_COORD                              		(DISPSYS_WDMA0_BASE+0x020)
#define DISP_REG_WDMA_DST_W_IN_BYTE                           		(DISPSYS_WDMA0_BASE+0x028)
#define DISP_REG_WDMA_ALPHA                                   			(DISPSYS_WDMA0_BASE+0x02C)
#define DISP_REG_WDMA_BUF_CON1                                			(DISPSYS_WDMA0_BASE+0x038)
#define DISP_REG_WDMA_BUF_CON2                                			(DISPSYS_WDMA0_BASE+0x03C)
#define DISP_REG_WDMA_C00                                     			(DISPSYS_WDMA0_BASE+0x040)
#define DISP_REG_WDMA_C02                                     			(DISPSYS_WDMA0_BASE+0x044)
#define DISP_REG_WDMA_C10                                     			(DISPSYS_WDMA0_BASE+0x048)
#define DISP_REG_WDMA_C12                                     			(DISPSYS_WDMA0_BASE+0x04C)
#define DISP_REG_WDMA_C20                                     			(DISPSYS_WDMA0_BASE+0x050)
#define DISP_REG_WDMA_C22                                     			(DISPSYS_WDMA0_BASE+0x054)
#define DISP_REG_WDMA_PRE_ADD0                                			(DISPSYS_WDMA0_BASE+0x058)
#define DISP_REG_WDMA_PRE_ADD2                                			(DISPSYS_WDMA0_BASE+0x05C)
#define DISP_REG_WDMA_POST_ADD0                               		(DISPSYS_WDMA0_BASE+0x060)
#define DISP_REG_WDMA_POST_ADD2                               		(DISPSYS_WDMA0_BASE+0x064)
#define DISP_REG_WDMA_DST_UV_PITCH                            		(DISPSYS_WDMA0_BASE+0x078)
#define DISP_REG_WDMA_DST_ADDR_OFFSET0                        	(DISPSYS_WDMA0_BASE+0x080)
#define DISP_REG_WDMA_DST_ADDR_OFFSET1                        	(DISPSYS_WDMA0_BASE+0x084)
#define DISP_REG_WDMA_DST_ADDR_OFFSET2                        	(DISPSYS_WDMA0_BASE+0x088)
#define DISP_REG_WDMA_FLOW_CTRL_DBG                          		(DISPSYS_WDMA0_BASE+0x0A0)
#define DISP_REG_WDMA_EXEC_DBG                                			(DISPSYS_WDMA0_BASE+0x0A4)
#define DISP_REG_WDMA_CT_DBG                                  			(DISPSYS_WDMA0_BASE+0x0A8)
#define DISP_REG_WDMA_DEBUG                                   			(DISPSYS_WDMA0_BASE+0x0AC)
#define DISP_REG_WDMA_DUMMY                                   			(DISPSYS_WDMA0_BASE+0x100)
#define DISP_REG_WDMA_DITHER_0                                			(DISPSYS_WDMA0_BASE+0xE00)
#define DISP_REG_WDMA_DITHER_5                                			(DISPSYS_WDMA0_BASE+0xE14)
#define DISP_REG_WDMA_DITHER_6                                			(DISPSYS_WDMA0_BASE+0xE18)
#define DISP_REG_WDMA_DITHER_7                                			(DISPSYS_WDMA0_BASE+0xE1C)
#define DISP_REG_WDMA_DITHER_8                                			(DISPSYS_WDMA0_BASE+0xE20)
#define DISP_REG_WDMA_DITHER_9                                			(DISPSYS_WDMA0_BASE+0xE24)
#define DISP_REG_WDMA_DITHER_10                               		(DISPSYS_WDMA0_BASE+0xE28)
#define DISP_REG_WDMA_DITHER_11                               		(DISPSYS_WDMA0_BASE+0xE2C)
#define DISP_REG_WDMA_DITHER_12                               		(DISPSYS_WDMA0_BASE+0xE30)
#define DISP_REG_WDMA_DITHER_13                               		(DISPSYS_WDMA0_BASE+0xE34)
#define DISP_REG_WDMA_DITHER_14                               		(DISPSYS_WDMA0_BASE+0xE38)
#define DISP_REG_WDMA_DITHER_15                               		(DISPSYS_WDMA0_BASE+0xE3C)
#define DISP_REG_WDMA_DITHER_16                               		(DISPSYS_WDMA0_BASE+0xE40)
#define DISP_REG_WDMA_DITHER_17                               		(DISPSYS_WDMA0_BASE+0xE44)
#define DISP_REG_WDMA_DST_ADDR0                               		(DISPSYS_WDMA0_BASE+0xF00)
#define DISP_REG_WDMA_DST_ADDR1                               		(DISPSYS_WDMA0_BASE+0xF04)
#define DISP_REG_WDMA_DST_ADDR2                               		(DISPSYS_WDMA0_BASE+0xF08)

#define INTEN_FLD_FRAME_UNDERRUN                          			REG_FLD(1, 1)
#define INTEN_FLD_FRAME_COMPLETE                          			REG_FLD(1, 0)
#define INTSTA_FLD_FRAME_UNDERRUN                         			REG_FLD(1, 1)
#define INTSTA_FLD_FRAME_COMPLETE                         			REG_FLD(1, 0)
#define EN_FLD_ENABLE                                     					REG_FLD(1, 0)
#define RST_FLD_SOFT_RESET                                				REG_FLD(1, 0)
#define SMI_CON_FLD_SMI_V_REPEAT_NUM                			REG_FLD(4, 24)
#define SMI_CON_FLD_SMI_U_REPEAT_NUM                  			REG_FLD(4, 20)
#define SMI_CON_FLD_SMI_Y_REPEAT_NUM                 			REG_FLD(4, 16)
#define SMI_CON_FLD_SLOW_COUNT                            			REG_FLD(8, 8)
#define SMI_CON_FLD_SLOW_LEVEL                          				REG_FLD(3, 5)
#define SMI_CON_FLD_SLOW_ENABLE                           			REG_FLD(1, 4)
#define SMI_CON_FLD_THRESHOLD                         				REG_FLD(4, 0)
#define CFG_FLD_DEBUG_SEL                                 				REG_FLD(4, 28)
#define CFG_FLD_INT_MTX_SEL                               				REG_FLD(4, 24)
#define CFG_FLD_SWAP                                  					REG_FLD(1, 16)
#define CFG_FLD_DNSP_SEL                                  				REG_FLD(1, 15)
#define CFG_FLD_EXT_MTX_EN                                				REG_FLD(1, 13)
#define CFG_FLD_VERTICAL_AVG                              				REG_FLD(1, 12)
#define CFG_FLD_CT_EN                                     					REG_FLD(1, 11)
#define CFG_FLD_OUT_FORMAT                                				REG_FLD(4, 4)
#define SRC_SIZE_FLD_HEIGHT                              		 		REG_FLD(14, 16)
#define SRC_SIZE_FLD_WIDTH                                				REG_FLD(14, 0)
#define CLIP_SIZE_FLD_HEIGHT                              				REG_FLD(14, 16)
#define CLIP_SIZE_FLD_WIDTH                               				REG_FLD(14, 0)
#define CLIP_COORD_FLD_Y_COORD                            			REG_FLD(14, 16)
#define CLIP_COORD_FLD_X_COORD                            			REG_FLD(14, 0)
#define DST_W_IN_BYTE_FLD_DST_W_IN_BYTE                   		REG_FLD(16, 0)
#define ALPHA_FLD_A_SEL                                   				REG_FLD(1, 31)
#define ALPHA_FLD_A_VALUE                                 				REG_FLD(8, 0)
#define BUF_CON1_FLD_ULTRA_ENABLE                         			REG_FLD(1, 31)
#define BUF_CON1_FLD_FRAME_END_ULTRA                      		REG_FLD(1, 28)
#define BUF_CON1_FLD_ISSUE_REQ_TH                         			REG_FLD(9, 16)
#define BUF_CON1_FLD_FIFO_PSEUDO_SIZE                     		REG_FLD(9, 0)
#define BUF_CON2_FLD_ULTRA_TH_HIGH_OFS                    		REG_FLD(8, 24)
#define BUF_CON2_FLD_PRE_ULTRA_TH_HIGH_OFS               	 	REG_FLD(8, 16)
#define BUF_CON2_FLD_ULTRA_TH_LOW_OFS                     		REG_FLD(8, 8)
#define BUF_CON2_FLD_PRE_ULTRA_TH_LOW                     		REG_FLD(8, 0)
#define C00_FLD_C01                                       					REG_FLD(13, 16)
#define C00_FLD_C00                                       					REG_FLD(13, 0)
#define C02_FLD_C02                                       					REG_FLD(13, 0)
#define C10_FLD_C11                                       					REG_FLD(13, 16)
#define C10_FLD_C10                                       					REG_FLD(13, 0)
#define C12_FLD_C12                                       					REG_FLD(13, 0)
#define C20_FLD_C21                                       					REG_FLD(13, 16)
#define C20_FLD_C20                                       					REG_FLD(13, 0)
#define C22_FLD_C22                                       					REG_FLD(13, 0)
#define PRE_ADD0_FLD_PRE_ADD_1                            			REG_FLD(9, 16)
#define PRE_ADD0_FLD_PRE_ADD_0                           			REG_FLD(9, 0)
#define PRE_ADD2_FLD_PRE_ADD_2                            			REG_FLD(9, 0)
#define POST_ADD0_FLD_POST_ADD_1                         			REG_FLD(9, 16)
#define POST_ADD0_FLD_POST_ADD_0                          			REG_FLD(9, 0)
#define POST_ADD2_FLD_POST_ADD_2                          			REG_FLD(9, 0)
#define DST_UV_PITCH_FLD_UV_DST_W_IN_BYTE              		REG_FLD(16, 0)
#define DST_ADDR_OFFSET0_FLD_WDMA_DESTINATION_ADDRESS_OFFSET0 	REG_FLD(28, 0)
#define DST_ADDR_OFFSET1_FLD_WDMA_DESTINATION_ADDRESS_OFFSET1 	REG_FLD(28, 0)
#define DST_ADDR_OFFSET2_FLD_WDMA_DESTINATION_ADDRESS_OFFSET2 	REG_FLD(28, 0)

#define FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL              				REG_FLD(10, 0)
#define EXEC_DBG_FLD_WDMA_IN_REQ                        				REG_FLD(1, 15)
#define EXEC_DBG_FLD_WDMA_IN_ACK                        				REG_FLD(1, 14)

#define EXEC_DBG_FLD_WDMA_STA_EXEC                        	REG_FLD(32, 0)
#define CT_DBG_FLD_WDMA_STA_CT                            	REG_FLD(32, 0)
#define DEBUG_FLD_WDMA_STA_DEBUG                          	REG_FLD(32, 0)
#define DUMMY_FLD_WDMA_DUMMY                              	REG_FLD(32, 0)
#define DITHER_0_FLD_CRC_CLR                              		REG_FLD(1, 24)
#define DITHER_0_FLD_CRC_START                            	REG_FLD(1, 20)
#define DITHER_0_FLD_CRC_CEN                              		REG_FLD(1, 16)
#define DITHER_0_FLD_FRAME_DONE_DEL                       	REG_FLD(8, 8)
#define DITHER_0_FLD_OUT_SEL                              		REG_FLD(1, 4)
#define DITHER_0_FLD_START                                		REG_FLD(1, 0)
#define DITHER_5_FLD_W_DEMO                               		REG_FLD(16, 0)
#define DITHER_6_FLD_WRAP_MODE                            	REG_FLD(1, 16)
#define DITHER_6_FLD_LEFT_EN                              		REG_FLD(2, 14)
#define DITHER_6_FLD_FPHASE_R                             		REG_FLD(1, 13)
#define DITHER_6_FLD_FPHASE_EN                            	REG_FLD(1, 12)
#define DITHER_6_FLD_FPHASE                               		REG_FLD(6, 4)
#define DITHER_6_FLD_ROUND_EN                             	REG_FLD(1, 3)
#define DITHER_6_FLD_RDITHER_EN                           	REG_FLD(1, 2)
#define DITHER_6_FLD_LFSR_EN                              		REG_FLD(1, 1)
#define DITHER_6_FLD_EDITHER_EN                           	REG_FLD(1, 0)
#define DITHER_7_FLD_DRMOD_B                              		REG_FLD(2, 8)
#define DITHER_7_FLD_DRMOD_G                              		REG_FLD(2, 4)
#define DITHER_7_FLD_DRMOD_R                             		REG_FLD(2, 0)
#define DITHER_8_FLD_INK_DATA_R                           	REG_FLD(10, 16)
#define DITHER_8_FLD_INK                                  		REG_FLD(1, 0)
#define DITHER_9_FLD_INK_DATA_B                          	 REG_FLD(10, 16)
#define DITHER_9_FLD_INK_DATA_G                           	REG_FLD(10, 0)
#define DITHER_10_FLD_FPHASE_BIT                       		REG_FLD(3, 8)
#define DITHER_10_FLD_FPHASE_SEL                          	REG_FLD(2, 4)
#define DITHER_10_FLD_FPHASE_CTRL                     		REG_FLD(2, 0)
#define DITHER_11_FLD_SUB_B                               		REG_FLD(2, 12)
#define DITHER_11_FLD_SUB_G                               		REG_FLD(2, 8)
#define DITHER_11_FLD_SUB_R                               		REG_FLD(2, 4)
#define DITHER_11_FLD_SUBPIX_EN                           	REG_FLD(1, 0)
#define DITHER_12_FLD_H_ACTIVE                            	REG_FLD(16, 16)
#define DITHER_12_FLD_TABLE_EN                            	REG_FLD(2, 4)
#define DITHER_12_FLD_LSB_OFF                             		REG_FLD(1, 0)
#define DITHER_13_FLD_RSHIFT_B                            		REG_FLD(3, 8)
#define DITHER_13_FLD_RSHIFT_G                            	REG_FLD(3, 4)
#define DITHER_13_FLD_RSHIFT_R                            		REG_FLD(3, 0)
#define DITHER_14_FLD_DEBUG_MODE                          	REG_FLD(2, 8)
#define DITHER_14_FLD_DIFF_SHIFT                          	REG_FLD(3, 4)
#define DITHER_14_FLD_TESTPIN_EN                          	REG_FLD(1, 0)
#define DITHER_15_FLD_LSB_ERR_SHIFT_R                     	REG_FLD(3, 28)
#define DITHER_15_FLD_OVFLW_BIT_R                         	REG_FLD(3, 24)
#define DITHER_15_FLD_ADD_lSHIFT_R                        	REG_FLD(3, 20)
#define DITHER_15_FLD_INPUT_RSHIFT_R                      	REG_FLD(3, 16)
#define DITHER_15_FLD_NEW_BIT_MODE                        	REG_FLD(1, 0)
#define DITHER_16_FLD_LSB_ERR_SHIFT_B                     	REG_FLD(3, 28)
#define DITHER_16_FLD_OVFLW_BIT_B                         	REG_FLD(3, 24)
#define DITHER_16_FLD_ADD_lSHIFT_B                        	REG_FLD(3, 20)
#define DITHER_16_FLD_INPUT_RSHIFT_B                      	REG_FLD(3, 16)
#define DITHER_16_FLD_lSB_ERR_SHIFT_G                     	REG_FLD(3, 12)
#define DITHER_16_FLD_OVFLW_BIT_G                         	REG_FLD(3, 8)
#define DITHER_16_FLD_ADD_lSHIFT_G                        	REG_FLD(3, 4)
#define DITHER_16_FLD_INPUT_RSHIFT_G                      	REG_FLD(3, 0)
#define DITHER_17_FLD_CRC_RDY                             		REG_FLD(1, 16)
#define DITHER_17_FLD_CRC_OUT                             		REG_FLD(16, 0)
#define DST_ADDR0_FLD_ADDRESS0                            	REG_FLD(32, 0)
#define DST_ADDR1_FLD_ADDRESS1                            	REG_FLD(32, 0)
#define DST_ADDR2_FLD_ADDRESS2                            	REG_FLD(32, 0)
#endif
