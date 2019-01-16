#ifndef __SI_DRV_MHL_TX_H__
#define __SI_DRV_MHL_TX_H__
#ifndef __KERNEL__
#include "si_c99support.h"
#endif
#include "si_drv_mhl_tx_edid.h"

/* Video Mode Constants */
/* ==================================================== */
#define VMD_ASPECT_RATIO_4x3			0x01
#define VMD_ASPECT_RATIO_16x9			0x02

/* ==================================================== */
/* Video mode define ( = VIC code, please see CEA-861 spec) */
#define HDMI_640X480P		1
#define HDMI_480I60_4X3	6
#define HDMI_480I60_16X9	7
#define HDMI_576I50_4X3	21
#define HDMI_576I50_16X9	22
#define HDMI_480P60_4X3	2
#define HDMI_480P60_16X9	3
#define HDMI_576P50_4X3	17
#define HDMI_576P50_16X9	18
#define HDMI_720P60			4
#define HDMI_720P50			19
#define HDMI_1080I60		5
#define HDMI_1080I50		20
#define HDMI_1080P24		32
#define HDMI_1080P25		33
#define HDMI_1080P30		34
/* #define HDMI_1080P60          16 //MHL doesn't supported */
/* #define HDMI_1080P50          31 //MHL doesn't supported */

typedef struct {
	uint8_t inputColorSpace;
	uint8_t outputColorSpace;
	uint8_t inputVideoCode;
	uint8_t outputVideoCode;
	uint8_t inputcolorimetryAspectRatio;
	uint8_t outputcolorimetryAspectRatio;
	uint8_t input_AR;
	uint8_t output_AR;
} video_data_t;
enum {
	ACR_N_value_192k = 24576,
	ACR_N_value_96k = 12288,
	ACR_N_value_48k = 6144,
	ACR_N_value_176k = 25088,
	ACR_N_value_88k = 12544,
	ACR_N_value_44k = 6272,
	ACR_N_value_32k = 4096,
	ACR_N_value_default = 6144,
};
#define COLOR_SPACE_RGB 0x00
#define COLOR_SPACE_YCBCR422 0x01
#define COLOR_SPACE_YCBCR444 0x02
typedef enum {
	VM_VGA = 0,
	VM_480P,
	VM_576P,
	VM_720P60,
	VM_720P50,
	VM_INVALID
} inVideoTypes_t;
typedef enum {
	I2S_192 = 0,
	I2S_96,
	I2S_48,
	I2S_176,
	I2S_88,
	I2S_44,
	I2S_32,
	TDM_192,
	TDM_96,
	TDM_48,
	TDM_176,
	TDM_88,
	TDM_44,
	TDM_32,
	TDM_192_8ch,
	AUD_SPDIF,
	AUD_TYP_NUM,
	AUD_INVALID
} inAudioTypes_t;
typedef struct {
	uint8_t regAUD_mode;
	uint8_t regAUD_ctrl;
	uint8_t regAUD_freq;
	uint8_t regAUD_src;
	uint8_t regAUD_tdm_ctrl;
	uint8_t regAUD_path;
} audioConfig_t;
typedef struct tagAVModeChange {
	bool_t video_change;
	bool_t audio_change;
} AVModeChange_t;
typedef struct tagAVMode {
	inVideoTypes_t video_mode;
	inAudioTypes_t audio_mode;
} AVMode_t;
/* bool_t SiiVideoInputIsValid(void); */
extern void AVModeDetect(AVModeChange_t *pAVModeChange, AVMode_t *pAVMode);

void SiiMhlTxTmdsEnable(void);

#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_VIDEO | MHL_DEV_LD_GUI)
#endif
