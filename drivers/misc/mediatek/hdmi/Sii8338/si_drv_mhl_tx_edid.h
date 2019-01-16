#ifndef _SI_DRV_MHL_TX_EDID_H_
#define _SI_DRV_MHL_TX_EDID_H_
#include "si_platform.h"
void SiiHdmiTxLiteEdidInitialize(void);
#define MAX_V_DESCRIPTORS			20
#define MAX_A_DESCRIPTORS			10
#define MAX_SPEAKER_CONFIGURATIONS	 4
#define AUDIO_DESCR_SIZE			 3
typedef struct {
	uint8_t VideoDescriptor[MAX_V_DESCRIPTORS];
	uint8_t AudioDescriptor[MAX_A_DESCRIPTORS][3];
	uint8_t SpkrAlloc[MAX_SPEAKER_CONFIGURATIONS];
	bool_t UnderScan;
	bool_t BasicAudio;
	bool_t YCbCr_4_4_4;
	bool_t YCbCr_4_2_2;
	bool_t HDMI_Sink;
	uint8_t CEC_A_B;
	uint8_t CEC_C_D;
	uint8_t ColorimetrySupportFlags;
	uint8_t MetadataProfile;
	bool_t _3D_Supported;
} Type_EDID_Descriptors;
enum EDID_ErrorCodes {
	EDID_OK,
	EDID_INCORRECT_HEADER,
	EDID_CHECKSUM_ERROR,
	EDID_NO_861_EXTENSIONS,
	EDID_SHORT_DESCRIPTORS_OK,
	EDID_LONG_DESCRIPTORS_OK,
	EDID_EXT_TAG_ERROR,
	EDID_REV_ADDR_ERROR,
	EDID_V_DESCR_OVERFLOW,
	EDID_UNKNOWN_TAG_CODE,
	EDID_NO_DETAILED_DESCRIPTORS,
	EDID_DDC_BUS_REQ_FAILURE,
	EDID_DDC_BUS_RELEASE_FAILURE
};
uint8_t SiiDrvMhlTxReadEdid(void);
#define IsHDMI_Sink()			(EDID_Data.HDMI_Sink)
#define IsCEC_DEVICE()			(((EDID_Data.CEC_A_B != 0xFF) && (EDID_Data.CEC_C_D != 0xFF)) ? true : false)
#define SinkSupportYCbCr444()   (EDID_Data.YCbCr_4_4_4)
#define SinkSupportYCbCr422()   (EDID_Data.YCbCr_4_2_2)
extern Type_EDID_Descriptors EDID_Data;
#endif
