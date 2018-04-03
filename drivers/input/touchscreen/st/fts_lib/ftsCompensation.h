/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*			   FTS functions for getting Initialization Data		 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsCrossCompile.h"
#include "ftsSoftware.h"

#define COMP_DATA_READ_RETRY			2

/* Bytes dimension of Compensation Data Format */

#define COMP_DATA_HEADER				8
#define COMP_DATA_GLOBAL				8

#define HEADER_SIGNATURE				0xA5

/* Possible Compensation/Frame Data Type */
#define GENERAL_TUNING					0x0100
#define MS_TOUCH_ACTIVE					0x0200
#define MS_TOUCH_LOW_POWER				0x0400
#define MS_TOUCH_ULTRA_LOW_POWER		0x0800
#define MS_KEY							0x1000
#define SS_TOUCH						0x2000
#define SS_KEY							0x4000
#define SS_HOVER						0x8000
#define SS_PROXIMITY					0x0001
#define CHIP_INFO						0xFFFF

#define TIMEOUT_REQU_COMP_DATA			1000			/* ms */

/* CHIP INFO */
#define CHIP_INFO_SIZE					138/* bytes to read from framebuffer (exclude the signature and the type because already checked during the reading) */
#define EXTERNAL_RELEASE_INFO_SIZE			8/* bytes */

typedef struct {
	int force_node, sense_node;
	u16 type;
} DataHeader;

typedef struct {
	DataHeader header;
	u8 tuning_ver;
	u8 cx1;
	u8 *node_data;
	int node_data_size;
} MutualSenseData;

typedef struct {
	DataHeader header;
	u8 tuning_ver;
	u8 f_ix1, s_ix1;
	u8 f_cx1, s_cx1;
	u8 f_max_n, s_max_n;

	u8 *ix2_fm;
	u8 *ix2_sn;
	u8 *cx2_fm;
	u8 *cx2_sn;

} SelfSenseData;

typedef struct {
	DataHeader header;
	u8 ftsd_lp_timer_cal0;
	u8 ftsd_lp_timer_cal1;
	u8 ftsd_lp_timer_cal2;

	u8 ftsd_lp_timer_cal3;
	u8 ftsa_lp_timer_cal0;
	u8 ftsa_lp_timer_cal1;

} GeneralData;

typedef struct {
	u8 u8_loadCnt;						 /*  03 - Load Counter */
	u8 u8_infoVer;						 /*  04 - New chip info version */
	u16 u16_ftsdId;						/*  05 - FTSD ID */
	u8 u8_ftsdVer;						 /*  07 - FTSD version */
	u8 u8_ftsaId;						  /*  08 - FTSA ID */
	u8 u8_ftsaVer;						 /*  09 - FTSA version */
	u8 u8_tchRptVer;					   /*  0A - Touch report version (e.g. ST, Samsung etc) */
	u8 u8_extReleaseInfo[EXTERNAL_RELEASE_INFO_SIZE];			   /*  0B - External release information */
	u8 u8_custInfo[12];					/*  13 - Customer information */
	u16 u16_fwVer;						 /*  1F - Firmware version */
	u16 u16_cfgId;						 /*  21 - Configuration ID */
	u32 u32_projId;						/*  23 - Project ID */
	u16 u16_scrXRes;					   /*  27 - X resolution on main screen */
	u16 u16_scrYRes;					   /*  29 - Y resolution on main screen */
	u8 u8_scrForceLen;					 /*  2B - Number of force channel on main screen */
	u8 u8_scrSenseLen;					 /*  2C - Number of sense channel on main screen */
	u8 u64_scrForceEn[8];					/*  2D - Force channel enabled on main screen */
	u8 u64_scrSenseEn[8];					/*  35 - Sense channel enabled on main screen */
	u8 u8_msKeyLen;						/*  3D - Number of MS Key channel */
	u8 u64_msKeyForceEn[8];				  /*  3E - MS Key force channel enable */
	u8 u64_msKeySenseEn[8];				  /*  46 - MS Key sense channel enable */
	u8 u8_ssKeyLen;						/*  4E - Number of SS Key channel */
	u8 u64_ssKeyForceEn[8];				  /*  4F - SS Key force channel enable */
	u8 u64_ssKeySenseEn[8];				  /*  57 - SS Key sense channel enable */
	u8 u8_frcTchXLen;					  /*  5F - Number of force touch force channel */
	u8 u8_frcTchYLen;					  /*  60 - Number of force touch sense channel */
	u8 u64_frcTchForceEn[8];				 /*  61 - Force touch force channel enable */
	u8 u64_frcTchSenseEn[8];				 /*  69 - Force touch sense channel enable */
	u8 u8_msScrConfigTuneVer;			  /*  71 - MS screen tuning version in config */
	u8 u8_msScrLpConfigTuneVer;			/*  72 - MS screen LP mode tuning version in config */
	u8 u8_msScrHwulpConfigTuneVer;		 /*  73 - MS screen ultra low power mode tuning version in config */
	u8 u8_msKeyConfigTuneVer;			  /*  74 - MS Key tuning version in config */
	u8 u8_ssTchConfigTuneVer;			  /*  75 - SS touch tuning version in config */
	u8 u8_ssKeyConfigTuneVer;			  /*  76 - SS Key tuning version in config */
	u8 u8_ssHvrConfigTuneVer;			  /*  77 - SS hover tuning version in config */
	u8 u8_frcTchConfigTuneVer;			 /*  78 - Force touch tuning version in config */
	u8 u8_msScrCxmemTuneVer;			   /*  79 - MS screen tuning version in cxmem */
	u8 u8_msScrLpCxmemTuneVer;			 /*  7A - MS screen LP mode tuning version in cxmem */
	u8 u8_msScrHwulpCxmemTuneVer;		  /*  7B - MS screen ultra low power mode tuning version in cxmem */
	u8 u8_msKeyCxmemTuneVer;			   /*  7C - MS Key tuning version in cxmem */
	u8 u8_ssTchCxmemTuneVer;			   /*  7D - SS touch tuning version in cxmem */
	u8 u8_ssKeyCxmemTuneVer;			   /*  7E - SS Key tuning version in cxmem */
	u8 u8_ssHvrCxmemTuneVer;			   /*  7F - SS hover tuning version in cxmem */
	u8 u8_frcTchCxmemTuneVer;			  /*  80 - Force touch tuning version in cxmem */
	u32 u32_mpPassFlag;			   /*  81 - Mass production pass flag */
	u32 u32_featEn;						/*  85 - Supported features */
	u32 u32_echoEn;				/*  89 - enable of particular features: first bit is Echo Enables */
} chipInfo;

int requestCompensationData(u16 type);
int readCompensationDataHeader(u16 type, DataHeader *header, u16 *address);
int readMutualSenseGlobalData(u16 *address, MutualSenseData *global);
int readMutualSenseNodeData(u16 address, MutualSenseData *node);
int readMutualSenseCompensationData(u16 type, MutualSenseData *data);
int readSelfSenseGlobalData(u16 *address, SelfSenseData *global);
int readSelfSenseNodeData(u16 address, SelfSenseData *node);
int readSelfSenseCompensationData(u16 type, SelfSenseData *data);
int readGeneralGlobalData(u16 address, GeneralData *global);
int readGeneralCompensationData(u16 type, GeneralData *data);
int defaultChipInfo(int i2cError);
int readChipInfo(int doRequest);
