#include "si_common.h"
#include "si_mhl_tx_api.h"
#include "si_drv_mhl_tx.h"
#include "si_platform.h"
#include "si_drv_ext.h"
#ifdef AMF_SRC
#include "AMF_Lib.h"
static AVModeDescription_t AVModeDescription;
static void AVModeReport(void)
{
	uint16_t OldDebugFormat;
	OldDebugFormat = SiiOsDebugGetConfig();
	SiiOsDebugSetConfig(SII_OS_DEBUG_FORMAT_SIMPLE);
	TX_DEBUG_PRINT(("HF:%d", (int)AVModeDescription.HDMIVideoFormat));
	switch (AVModeDescription.HDMIVideoFormat) {
	case VMD_HDMIFORMAT_CEA_VIC:
		TX_DEBUG_PRINT((" (CEA VIC)"));
		break;
	case VMD_HDMIFORMAT_HDMI_VIC:
		TX_DEBUG_PRINT((" (HDMI VIC)"));
		break;
	case VMD_HDMIFORMAT_3D:
		TX_DEBUG_PRINT((" (3D)"));
		break;
	case VMD_HDMIFORMAT_PC:
		TX_DEBUG_PRINT((" (PC)"));
		break;
	default:
		TX_DEBUG_PRINT((" (Illegal)"));
		break;
	}
	TX_DEBUG_PRINT((" VIC:%d", (int)AVModeDescription.VIC));
	TX_DEBUG_PRINT((" A:%x", (int)AVModeDescription.AspectRatio));
	switch (AVModeDescription.AspectRatio) {
	case VMD_ASPECT_RATIO_4x3:
		TX_DEBUG_PRINT((" (4x3)"));
		break;
	case VMD_ASPECT_RATIO_16x9:
		TX_DEBUG_PRINT((" (16x9)"));
		break;
	default:
		TX_DEBUG_PRINT((" (Illegal)"));
		break;
	}
	TX_DEBUG_PRINT((" ICS:%d", (int)AVModeDescription.ColorSpace));
	switch (AVModeDescription.ColorSpace) {
	case VMD_COLOR_SPACE_RGB:
		TX_DEBUG_PRINT((" (RGB)"));
		break;
	case VMD_COLOR_SPACE_YCBCR422:
		TX_DEBUG_PRINT((" (YCBCR422)"));
		break;
	case VMD_COLOR_SPACE_YCBCR444:
		TX_DEBUG_PRINT((" (YCBCR444)"));
		break;
	default:
		TX_DEBUG_PRINT((" (Illegal)"));
		break;
	}
	TX_DEBUG_PRINT((" ICD:%d", (int)AVModeDescription.ColorDepth));
	switch (AVModeDescription.ColorDepth) {
	case VMD_COLOR_DEPTH_8BIT:
		TX_DEBUG_PRINT((" (8BIT)"));
		break;
	case VMD_COLOR_DEPTH_10BIT:
		TX_DEBUG_PRINT((" (10BIT)"));
		break;
	case VMD_COLOR_DEPTH_12BIT:
		TX_DEBUG_PRINT((" (12BIT)"));
		break;
	case VMD_COLOR_DEPTH_16BIT:
		TX_DEBUG_PRINT((" (16BIT)"));
		break;
	default:
		TX_DEBUG_PRINT((" (Illegal)"));
		break;
	}
	TX_DEBUG_PRINT((" HA:%d", (int)AVModeDescription.HDCPAuthenticated));
	if (AVModeDescription.HDCPAuthenticated == VMD_HDCP_AUTHENTICATED) {
		TX_DEBUG_PRINT((" (Authenticated)\n"));
	} else {
		TX_DEBUG_PRINT((" (NOT Authenticated)\n"));
	}
	if (AVModeDescription.HDMIVideoFormat == VMD_HDMIFORMAT_3D) {
		TX_DEBUG_PRINT((" 3D:%d", (int)AVModeDescription.ThreeDStructure));
		switch (AVModeDescription.ThreeDStructure) {
		case VMD_3D_EXT_DATA_MAP:
			TX_DEBUG_PRINT((" (Ext Data Map)\n"));
			break;
		case VMD_3D_FRAMEPACKING:
			TX_DEBUG_PRINT((" (Frame Packing)\n"));
			break;
		case VMD_3D_FIELDALTERNATIVE:
			TX_DEBUG_PRINT((" (Field Alternating)\n"));
			break;
		case VMD_3D_LINEALTERNATIVE:
			TX_DEBUG_PRINT(("(Line Alternating)\n"));
			break;
		case VMD_3D_SIDEBYSIDEFULL:
			TX_DEBUG_PRINT((" (Side-by-side Full)\n"));
			break;
		case VMD_3D_LDEPTH:
			TX_DEBUG_PRINT((" (L Depth)\n"));
			break;
		case VMD_3D_LDEPTHGRAPHICS:
			TX_DEBUG_PRINT((" (L Depth Graphics)\n"));
			break;
		case VMD_3D_SIDEBYSIDEHALF:
			TX_DEBUG_PRINT((" (Side-by-side half)"));
			TX_DEBUG_PRINT((" 3Dx:%d\n", (int)AVModeDescription.ThreeDExtData));
			break;
		default:
			TX_DEBUG_PRINT((" (Illegal)"));
			break;
		}
	}
	switch (AVModeDescription.Audiotype) {
	case 0x00:
		TX_DEBUG_PRINT((" AUD: 44.1kHz "));
		break;
	case 0x01:
		TX_DEBUG_PRINT((" AUD: not indicated "));
		break;
	case 0x02:
		TX_DEBUG_PRINT((" AUD: 48kHz "));
		break;
	case 0x03:
		TX_DEBUG_PRINT((" AUD: 32kHz "));
		break;
	case 0x04:
		TX_DEBUG_PRINT((" AUD: 22.05kHz "));
		break;
	case 0x06:
		TX_DEBUG_PRINT((" AUD: 24kHz "));
		break;
	case 0x08:
		TX_DEBUG_PRINT((" AUD: 88.2kHz "));
		break;
	case 0x09:
		TX_DEBUG_PRINT((" AUD: 768kHz "));
		break;
	case 0x0A:
		TX_DEBUG_PRINT((" AUD: 96kHz "));
		break;
	case 0x0C:
		TX_DEBUG_PRINT((" AUD: 176.4kHz "));
		break;
	case 0x0E:
		TX_DEBUG_PRINT((" AUD: 192kHz "));
		break;
	default:
		TX_DEBUG_PRINT((" AUD: Illegal"));
		break;
	}
	TX_DEBUG_PRINT(("\n"));
	SiiOsDebugSetConfig(OldDebugFormat);
}

bool_t CheckExtVideo(void)
{
	bool_t video_change = false;
	if (AMF_Poll(false) != 0) {
		video_change = true;
		AMF_GetAVModeDescription(&AVModeDescription);
		PRINT("RX AV mode from AMF:\n");
		AVModeReport();
		if (AVModeDescription.VIC == 0) {
			video_change = false;
		}
	}
	return video_change;
}

unsigned char GetExt_AudioType(void)
{
	inAudioTypes_t audio_type;
	if (AVModeDescription.VIC == 0) {
		audio_type = AUD_INVALID;
	} else {
		switch (AVModeDescription.Audiotype) {
		case 0x00:
			audio_type = I2S_44;
			break;
		case 0x02:
			audio_type = I2S_48;
			break;
		case 0x03:
			audio_type = I2S_32;
			break;
		case 0x08:
			audio_type = I2S_88;
			break;
		case 0x0A:
			audio_type = I2S_96;
			break;
		case 0x0C:
			audio_type = I2S_176;
			break;
		case 0x0E:
			audio_type = I2S_192;
			break;
		default:
			audio_type = AUD_INVALID;
			break;
		}
	}
	return audio_type;
}

uint8_t GetExt_inputColorSpace(void)
{
	return AVModeDescription.ColorSpace;
}

uint8_t GetExt_inputVideoCode(void)
{
	return AVModeDescription.VIC;
}

uint8_t GetExt_inputcolorimetryAspectRatio(void)
{
	if (AVModeDescription.AspectRatio == VMD_ASPECT_RATIO_4x3)
		return 0x18;
	else
		return 0x28;
}

uint8_t GetExt_inputAR(void)
{
	return AVModeDescription.AspectRatio;
}

void InitExtVideo(void)
{
	TX_DEBUG_PRINT(("AMFlib initialize.\n"));
	AMF_Init(false);
}

void TriggerExtInt(void)
{
#ifdef __KERLNEL__
	AMF_TriggerSWInt();
#endif
}
#else
static void AVModeReport(void)
{
	return;
}

bool_t CheckExtVideo(void)
{
	return false;
}

unsigned char GetExt_AudioType(void)
{
	return 0;
}

uint8_t GetExt_inputColorSpace(void)
{
	return 0;
}

uint8_t GetExt_inputVideoCode(void)
{
	return 0;
}

uint8_t GetExt_inputcolorimetryAspectRatio(void)
{
	return 0;
}

uint8_t GetExt_inputAR(void)
{
	return 0;
}

void InitExtVideo(void)
{
}

void TriggerExtInt(void)
{
}
#endif
