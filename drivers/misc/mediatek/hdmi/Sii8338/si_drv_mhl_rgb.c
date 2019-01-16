#ifdef RGB_BOARD
#ifndef __KERNEL__
#include <string.h>
#include "hal_local.h"
#include "si_common.h"
#else
#include <linux/string.h>
#include "sii_hal.h"
#endif
#include "si_drv_ext.h"
#include "si_platform.h"
#include "si_drv_mhl_tx.h"
#include "si_mhl_tx_api.h"
static bool_t valid_input;
extern video_data_t video_data;
static bool_t RGBIsr(AVModeChange_t *pAVModeChange, AVMode_t *pAVMode)
{
#if 0
	bool_t video_change = false;
	video_change = CheckExtVideo();
	if (video_change) {
		video_data.inputColorSpace = GetExt_inputColorSpace();
		video_data.inputVideoCode = GetExt_inputVideoCode();
		video_data.inputcolorimetryAspectRatio = GetExt_inputcolorimetryAspectRatio();
		video_data.input_AR = GetExt_inputAR();
	}
#endif
	switch (video_data.inputVideoCode) {
	case 1:
		pAVMode->video_mode = VM_VGA;
		break;
	case 2:
	case 3:
		pAVMode->video_mode = VM_480P;
		break;
	case 4:
		pAVMode->video_mode = VM_720P60;
		break;
	case 17:
	case 18:
		pAVMode->video_mode = VM_576P;
		break;
	case 19:
		pAVMode->video_mode = VM_720P50;
		break;
	default:
		pAVMode->video_mode = VM_INVALID;
		break;
	}
	pAVModeChange->video_change = video_change;
	if (pAVMode->video_mode == VM_INVALID)
		return false;
	return true;
}

static void AudioIsr(AVModeChange_t *pAVModeChange, AVMode_t *pAVMode)
{
	inAudioTypes_t newaudiomode = AUD_INVALID;
	newaudiomode = GetExt_AudioType();
	if (!pinDbgSw5) {
		newaudiomode = AUD_SPDIF;
	}
	if (pAVMode->audio_mode != newaudiomode) {
		TX_DEBUG_PRINT(("Audio input mode change, new mode: 0x%02x, old mode: 0x%02x.\n",
				(int)newaudiomode, (int)pAVMode->audio_mode));
		pAVMode->audio_mode = newaudiomode;
		pAVModeChange->audio_change = true;
	}
}

bool_t SiiVideoInputIsValid(void)
{
	if (pinDbgSw6)
		return valid_input;
	else
		return true;
}

void AVModeDetect(AVModeChange_t *pAVModeChange, AVMode_t *pAVMode)
{
	TX_DEBUG_PRINT(("AVModeDetect"));
	valid_input = RGBIsr(pAVModeChange, pAVMode);
	AudioIsr(pAVModeChange, pAVMode);
}

void SiiInitExtVideo(void)
{
	InitExtVideo();
}

void SiiTriggerExtInt(void)
{
	TriggerExtInt();
}
#endif
