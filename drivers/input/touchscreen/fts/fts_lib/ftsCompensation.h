/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*               FTS functions for getting Initialization Data		 *
*                                                                        *
**************************************************************************
**************************************************************************

*/


#include "ftsCrossCompile.h"
#include "ftsSoftware.h"



#define COMP_DATA_READ_RETRY			2



#define COMP_DATA_HEADER				8
#define COMP_DATA_GLOBAL				8


#define HEADER_SIGNATURE				0xA5
#define INVALID_ERROR_OFFS				0xFFFF


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


#define TIMEOUT_REQU_COMP_DATA			1000


#define CHIP_INFO_SIZE					141
#define EXTERNAL_RELEASE_INFO_SIZE			8

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
	u8 u8_loadCnt;
	u8 u8_infoVer;
	u16 u16_ftsdId;
	u8 u8_ftsdVer;
	u8 u8_ftsaId;
	u8 u8_ftsaVer;
	u8 u8_tchRptVer;
	u8 u8_extReleaseInfo[EXTERNAL_RELEASE_INFO_SIZE];
	u8 u8_custInfo[12];
	u16 u16_fwVer;
	u16 u16_cfgId;
	u32 u32_projId;
	u16 u16_scrXRes;
	u16 u16_scrYRes;
	u8 u8_scrForceLen;
	u8 u8_scrSenseLen;
	u8 u64_scrForceEn[8];
	u8 u64_scrSenseEn[8];
	u8 u8_msKeyLen;
	u8 u64_msKeyForceEn[8];
	u8 u64_msKeySenseEn[8];
	u8 u8_ssKeyLen;
	u8 u64_ssKeyForceEn[8];
	u8 u64_ssKeySenseEn[8];
	u8 u8_frcTchXLen;
	u8 u8_frcTchYLen;
	u8 u64_frcTchForceEn[8];
	u8 u64_frcTchSenseEn[8];
	u8 u8_msScrConfigTuneVer;
	u8 u8_msScrLpConfigTuneVer;
	u8 u8_msScrHwulpConfigTuneVer;
	u8 u8_msKeyConfigTuneVer;
	u8 u8_ssTchConfigTuneVer;
	u8 u8_ssKeyConfigTuneVer;
	u8 u8_ssHvrConfigTuneVer;
	u8 u8_frcTchConfigTuneVer;
	u8 u8_msScrCxmemTuneVer;
	u8 u8_msScrLpCxmemTuneVer;
	u8 u8_msScrHwulpCxmemTuneVer;
	u8 u8_msKeyCxmemTuneVer;
	u8 u8_ssTchCxmemTuneVer;
	u8 u8_ssKeyCxmemTuneVer;
	u8 u8_ssHvrCxmemTuneVer;
	u8 u8_frcTchCxmemTuneVer;
	u32 u32_mpPassFlag;
	u32 u32_featEn;
	u32 u32_echoEn;
	u8 u8_errSign;
	u16 u16_errOffset;
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
