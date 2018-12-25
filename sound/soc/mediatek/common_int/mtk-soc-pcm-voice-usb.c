/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *	mt-soc-pcm-voice-usb.c
 *
 * Project:
 * --------
 *	MT6797
 *
 * Description:
 * ------------
 *	Platform driver for usb phone call
 *
 * Author:
 * -------
 *	Kai Chieh Chuang
 *
 *---------------------------------------------------------------------------
 *
 *
 ****************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common-func.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-digital-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/* debug */
#define NUM_DBG_LOG 60
#define DBG_LOG_LENGTH 256

struct usb_dbg_log {
	unsigned int idx;
	char log[DBG_LOG_LENGTH];
};

static struct usb_dbg_log dbg_log[NUM_DBG_LOG];
static unsigned int dbg_log_idx;

static void print_usb_dbg_log(void)
{
	unsigned int i = 0;

	for (i = 0; i < NUM_DBG_LOG; i++) {
		pr_debug("%s(), idx %u, %s\n", __func__, dbg_log[i].idx,
		       dbg_log[i].log);
	}
}

/*
 *    function implementation
 */
static bool usb_prepare_done[2] = {false, false};
static bool usb_use_dram[2] = {false, false};
static int usb_mem_blk[2] = {Soc_Aud_Digital_Block_MEM_DL2,
			     Soc_Aud_Digital_Block_MEM_AWB};

static bool voice_usb_status;
bool get_voice_usb_status(void)
{
	return voice_usb_status;
}
EXPORT_SYMBOL(get_voice_usb_status);

static struct audio_digital_pcm Voice1Pcm = {
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct audio_digital_pcm Voice2IntPcm = {
	.mBclkOutInv = false,
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO,
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

static int usb_md_select;
static const char *const md_choose[] = {"md1", "md2"};
static const struct soc_enum speech_usb_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(md_choose), md_choose),
};

static int Audio_USB_MD_Select_Control_Get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), usb_md_select = %d\n", __func__, usb_md_select);
	ucontrol->value.integer.value[0] = usb_md_select;
	return 0;
}

static int Audio_USB_MD_Select_Control_Set(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(md_choose)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	usb_md_select = ucontrol->value.integer.value[0];
	pr_debug("%s(), usb_md_select = %d\n", __func__, usb_md_select);
	return 0;
}

enum {
	VOICE_UL_PRIMARY,
	VOICE_UL_USB,
};

static int usb_ul_select;
static const char *const ul_choose[] = {"primary", "usb"};
static const struct soc_enum voice_usb_ul_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_choose), ul_choose),
};

static int Audio_USB_MD_UL_Select_Ctl_Get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), usb_ul_select = %d\n", __func__, usb_ul_select);
	ucontrol->value.integer.value[0] = usb_ul_select;
	return 0;
}

static int Audio_USB_MD_UL_Select_Ctl_Set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ul_choose)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	/* ul source change to usb mic after enabled */
	if (usb_prepare_done[SNDRV_PCM_STREAM_CAPTURE] &&
	    usb_ul_select != ucontrol->value.integer.value[0] &&
	    usb_ul_select == VOICE_UL_PRIMARY) {
		pr_debug("%s(), ul source change to usb mic!\n", __func__);
		/* disconnect interconnection to primary mic */
		if (usb_md_select) {
			/* i3i4 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		} else {
			/* i3i4 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		}

		/* disable adda ul path */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, false);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL) == false)
			set_adc_enable(false);

		/* connect between usb mic (dl2) with modem */
		if (usb_md_select) {
			/* i7i8 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		} else {
			/* i7i8 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		}
	}

	usb_ul_select = ucontrol->value.integer.value[0];
	pr_debug("%s(), usb_ul_select = %d\n", __func__, usb_ul_select);
	return 0;
}

static int usb_lpbk_enable;
static const char *const on_off_str[] = {"Off", "On"};
static const struct soc_enum voice_usb_lpbk_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(on_off_str), on_off_str),
};

static int Audio_USB_Lpbk_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), enable = %d\n", __func__, usb_lpbk_enable);
	ucontrol->value.integer.value[0] = usb_lpbk_enable;
	return 0;
}

static int Audio_USB_Lpbk_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(on_off_str)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	usb_lpbk_enable = ucontrol->value.integer.value[0];
	pr_debug("%s(), enable = %d\n", __func__, usb_lpbk_enable);
	if (usb_lpbk_enable) {
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MEM_DL2,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);
	} else {
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL2,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);
	}
	return 0;
}

enum USB_DBG_TYPE {
	USB_DBG_ASSERT_AT_STOP = 0x1 << 0,
	USB_DBG_BUFFER_LEVEL = 0x1 << 1,
	USB_DBG_ECHO_REF_ALIGN = 0x1 << 2,
};

static int usb_debug_enable;
static bool disconnect_debug_path;
static int Audio_USB_Debug_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = usb_debug_enable;
	return 0;
}

static int Audio_USB_Debug_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_debug_enable = ucontrol->value.integer.value[0];
	pr_debug("%s(), usb_debug_enable = 0x%x\n", __func__, usb_debug_enable);

	return 0;
}

static int usb_echo_ref_delay_us;
struct memif_lpbk usb_memif_lpbk;
static void set_echo_ref_path(int connect)
{
	if (usb_md_select) {
		SetIntfConnection(connect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
		SetIntfConnection(connect, Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);
	} else {
		SetIntfConnection(connect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
		SetIntfConnection(connect, Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);
	}
}
static int Audio_USB_Echo_Ref_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = usb_echo_ref_delay_us;
	return 0;
}

static int Audio_USB_Echo_Ref_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	usb_echo_ref_delay_us = ucontrol->value.integer.value[0];
	pr_debug("%s(), usb_echo_ref_delay_us = %d\n", __func__,
		usb_echo_ref_delay_us);

	if (usb_echo_ref_delay_us > 0) {
		usb_memif_lpbk.dl_memif = Soc_Aud_Digital_Block_MEM_DL1;
		usb_memif_lpbk.ul_memif = Soc_Aud_Digital_Block_MEM_MOD_DAI;

		usb_memif_lpbk.delay_us = usb_echo_ref_delay_us;

		set_echo_ref_path(Soc_Aud_InterCon_Connection);

		memif_lpbk_enable(&usb_memif_lpbk);
	} else {
		memif_lpbk_disable(&usb_memif_lpbk);

		set_echo_ref_path(Soc_Aud_InterCon_DisConnect);
	}
	return 0;
}

static const struct snd_kcontrol_new speech_usb_controls[] = {
	SOC_ENUM_EXT("USB_Modem_Select", speech_usb_enum[0],
		     Audio_USB_MD_Select_Control_Get,
		     Audio_USB_MD_Select_Control_Set),
	SOC_ENUM_EXT("USB_Voice_UL_Select", voice_usb_ul_enum[0],
		     Audio_USB_MD_UL_Select_Ctl_Get,
		     Audio_USB_MD_UL_Select_Ctl_Set),
	SOC_ENUM_EXT("USB_Voice_Loopback_Switch", voice_usb_lpbk_enum[0],
		     Audio_USB_Lpbk_Get, Audio_USB_Lpbk_Set),
	SOC_SINGLE_EXT("USB_Voice_Debug", SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       Audio_USB_Debug_Get, Audio_USB_Debug_Set),
	SOC_SINGLE_EXT("USB_Voice_Echo_Ref", SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       Audio_USB_Echo_Ref_Get, Audio_USB_Echo_Ref_Set),
};

static void usb_md1_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {
		/* connect */
		if (usb_ul_select == VOICE_UL_PRIMARY) {
			/* i3i4 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		} else {
			/* dl2 i7i8 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		}
		/* pcm2 i14i21 --> awb o5o6*/
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		Voice1Pcm.mPcmModeWidebandSel = SampleRateTransform(
			runtime->rate, Soc_Aud_Digital_Block_MODEM_PCM_2_O);

		Voice1Pcm.mAsyncFifoSel =
			Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
		SetModemPcmConfig(MODEM_1, Voice1Pcm);
		SetModemPcmEnable(MODEM_1, true);
	} else {
		/* disconnect */
		/* i3i4 -> pcm2 o17o28 */
		if (usb_ul_select == VOICE_UL_PRIMARY) {
			/* i3i4 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		} else {
			/* dl2 i7i8 -> pcm2 o17o28 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
		}
		/* pcm2 i14 --> awb o5o6 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetModemPcmEnable(MODEM_1, false);
	}
}

static void usb_md2_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {
		/* connect */
		if (usb_ul_select == VOICE_UL_PRIMARY) {
			/* i3i4 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		} else {
			/* dl2 i7i8 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_Connection,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		}
		/* pcm1 i9i22 --> awb o5o6 */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		Voice2IntPcm.mPcmModeWidebandSel = SampleRateTransform(
			runtime->rate, Soc_Aud_Digital_Block_MODEM_PCM_1_O);

		/* Voice2IntPcm.mAsyncFifoSel =
		 * Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
		 */
		SetModemPcmConfig(MODEM_EXTERNAL, Voice2IntPcm);
		SetModemPcmEnable(MODEM_EXTERNAL, true);
	} else {
		/* disconnect */
		/* i3i4 -> pcm1 o7o8 */
		if (usb_ul_select == VOICE_UL_PRIMARY) {
			/* i3i4 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_ADDA_UL,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		} else {
			/* dl2 i7i8 -> pcm1 o7o8 */
			SetIntfConnection(Soc_Aud_InterCon_DisConnect,
					  Soc_Aud_AFE_IO_Block_MEM_DL2,
					  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);
		}
		/* pcm1 i9i22 --> awb o5o6 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetModemPcmEnable(MODEM_EXTERNAL, false);
	}
}

static int mtk_voice_usb_close(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_debug("%s(), stream %d, prepare %d\n", __func__, stream,
		usb_prepare_done[stream]);

	if (usb_prepare_done[substream->stream]) {
		usb_prepare_done[substream->stream] = false;
		RemoveMemifSubStream(usb_mem_blk[stream], substream);

		/* resume pbuf size */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			set_memif_pbuf_size(usb_mem_blk[stream],
					    MEMIF_PBUF_SIZE_256_BYTES);

		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (usb_ul_select == VOICE_UL_PRIMARY) {
				SetMemoryPathEnable(
					Soc_Aud_Digital_Block_ADDA_UL, false);
				if (GetMemoryPathEnable(
					    Soc_Aud_Digital_Block_ADDA_UL) ==
				    false)
					set_adc_enable(false);
			}

			if (usb_md_select)
				usb_md2_enable(false, substream->runtime);
			else
				usb_md1_enable(false, substream->runtime);

			voice_usb_status = false;

			if (usb_debug_enable & USB_DBG_ECHO_REF_ALIGN ||
			    disconnect_debug_path) {
				SetConnection(
					Soc_Aud_InterCon_DisConnect,
					Soc_Aud_InterConnectionInput_I07,
					Soc_Aud_InterConnectionOutput_O03);
				SetConnection(
					Soc_Aud_InterCon_DisConnect,
					Soc_Aud_InterConnectionInput_I03,
					Soc_Aud_InterConnectionOutput_O03);
				SetConnection(
					Soc_Aud_InterCon_DisConnect,
					Soc_Aud_InterConnectionInput_I05,
					Soc_Aud_InterConnectionOutput_O04);
				/* stop DAC output */
				SetMemoryPathEnable(
					Soc_Aud_Digital_Block_I2S_OUT_DAC,
					false);
				if (GetI2SDacEnable() == false)
					SetI2SDacEnable(false);

				disconnect_debug_path = false;
			}
		}
	}

	EnableAfe(false);
	AudDrv_Clk_Off();

	return 0;
}

static int mtk_voice_usb_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err = 0;
	int ret = 0;

	AudDrv_Clk_On();

	pr_debug("%s()\n", __func__);

	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (err < 0) {
		pr_warn("mtk_voice_usb_close\n");
		mtk_voice_usb_close(substream);
		return err;
	}

	pr_debug("%s(), return\n", __func__);
	return 0;
}

static int mtk_voice_usb_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream = substream->stream;

	pr_debug("%s(), stream %d, rate = %d, ch = %d psize = %lu, prepare %d\n",
		__func__, stream, runtime->rate, runtime->channels,
		runtime->period_size, usb_prepare_done[stream]);

	if (!usb_prepare_done[stream]) {
		SetMemifSubStream(usb_mem_blk[stream], substream);

		/* set memif format */
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE)
			SetMemIfFetchFormatPerSample(
				usb_mem_blk[stream],
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		else
			SetMemIfFetchFormatPerSample(usb_mem_blk[stream],
						     AFE_WLEN_16_BIT);

		SetSampleRate(usb_mem_blk[stream], runtime->rate);
		SetChannels(usb_mem_blk[stream], runtime->channels);
		if (runtime->channels == 1)
			SetMemifMonoSel(usb_mem_blk[stream], false);

		/* set pbuf size for latency */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			set_memif_pbuf_size(usb_mem_blk[stream],
					    MEMIF_PBUF_SIZE_32_BYTES);

		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (usb_ul_select == VOICE_UL_PRIMARY) {
				if (GetMemoryPathEnable(
					    Soc_Aud_Digital_Block_ADDA_UL) ==
				    false) {
					SetMemoryPathEnable(
						Soc_Aud_Digital_Block_ADDA_UL,
						true);
					set_adc_in(substream->runtime->rate);
					set_adc_enable(true);
				} else {
					SetMemoryPathEnable(
						Soc_Aud_Digital_Block_ADDA_UL,
						true);
				}
			}

			if (usb_md_select)
				usb_md2_enable(true, runtime);
			else
				usb_md1_enable(true, runtime);

			voice_usb_status = true;

			/* setup for echo ref path */
			usb_memif_lpbk.format = runtime->format;
			usb_memif_lpbk.rate = runtime->rate;
			usb_memif_lpbk.channel = runtime->channels;

			if (usb_debug_enable & USB_DBG_ECHO_REF_ALIGN) {
				SetConnection(
					Soc_Aud_InterCon_Connection,
					Soc_Aud_InterConnectionInput_I07,
					Soc_Aud_InterConnectionOutput_O03);
				SetConnection(
					Soc_Aud_InterCon_Connection,
					Soc_Aud_InterConnectionInput_I03,
					Soc_Aud_InterConnectionOutput_O03);
				SetConnection(
					Soc_Aud_InterCon_Connection,
					Soc_Aud_InterConnectionInput_I05,
					Soc_Aud_InterConnectionOutput_O04);
				/* start I2S DAC out */
				if (GetMemoryPathEnable(
					Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
				    false) {
					SetMemoryPathEnable(
					Soc_Aud_Digital_Block_I2S_OUT_DAC,
					true);
					SetI2SDacOut(
					substream->runtime->rate, false,
					runtime->format ==
					SNDRV_PCM_FORMAT_S32_LE ||
					runtime->format ==
					SNDRV_PCM_FORMAT_U32_LE);
					SetI2SDacEnable(true);
				} else {
					SetMemoryPathEnable(
					Soc_Aud_Digital_Block_I2S_OUT_DAC,
					true);
				}
				disconnect_debug_path = true;
			}
		}

		usb_prepare_done[stream] = true;
	}

	ClearMemBlock(usb_mem_blk[stream]);

	EnableAfe(true);
	return 0;
}

static int mtk_voice_usb_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream = substream->stream;
	int ret = 0;

	runtime->dma_bytes = params_buffer_bytes(hw_params);

#if 0
	if (AllocateAudioSram(&runtime->dma_addr,
			      &runtime->dma_area,
			      runtime->dma_bytes,
			      substream,
			      params_format(hw_params), false) == 0) {
		usb_use_dram[stream] = false;
		SetHighAddr(usb_mem_blk[stream], false,
			    substream->runtime->dma_addr);
	} else {
#endif
	ret = snd_pcm_lib_malloc_pages(substream, runtime->dma_bytes);
	if (ret < 0) {
		pr_err("%s(), allocate dram fail, ret %d\n", __func__, ret);
		return ret;
	}
	usb_use_dram[stream] = true;
	SetHighAddr(usb_mem_blk[stream], true, substream->runtime->dma_addr);
	AudDrv_Emi_Clk_On();

	set_mem_block(substream, hw_params,
		      Get_Mem_ControlT(usb_mem_blk[stream]),
		      usb_mem_blk[stream]);

	pr_debug("%s(), substream %p, stream %d, dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx, use_dram %d\n",
		__func__, substream, stream, runtime->dma_bytes,
		runtime->dma_area, (long)runtime->dma_addr,
		usb_use_dram[stream]);

	return ret;
}

static int mtk_voice_usb_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s(), substream = %p, stream %d\n", __func__, substream,
		substream->stream);

	if (usb_use_dram[substream->stream]) {
		AudDrv_Emi_Clk_Off();
		usb_use_dram[substream->stream] = false;
		return snd_pcm_lib_free_pages(substream);
	} else {
		return freeAudioSram((void *)substream);
	}
}

static int mtk_voice_usb_start(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_debug("%s(), stream %d\n", __func__, stream);

	SetMemoryPathEnable(usb_mem_blk[stream], true);
	/* fill some buffer fisrt, or need wait 2 irq period at first read*/
	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		udelay(300);

	/* here to set interrupt */
	irq_add_user(substream, irq_request_number(usb_mem_blk[stream]),
		     substream->runtime->rate, substream->runtime->period_size);

	EnableAfe(true);

	return 0;
}

static int mtk_voice_usb_stop(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_debug("%s(), stream %d\n", __func__, stream);

	if (usb_debug_enable & USB_DBG_ASSERT_AT_STOP) {
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			print_usb_dbg_log();
	}

	irq_remove_user(substream, irq_request_number(usb_mem_blk[stream]));

	SetMemoryPathEnable(usb_mem_blk[stream], false);

	ClearMemBlock(usb_mem_blk[stream]);
	return 0;
}

static int mtk_voice_usb_trigger(struct snd_pcm_substream *substream, int cmd)
{

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_voice_usb_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_voice_usb_stop(substream);
	}
	return -EINVAL;
}

static snd_pcm_uframes_t
mtk_voice_usb_pointer_play(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	int stream = substream->stream;
	struct afe_mem_control_t *mem_ctl =
		Get_Mem_ControlT(usb_mem_blk[stream]);
	struct afe_block_t *Afe_Block = &mem_ctl->rBlock;
	unsigned long flags;

	spin_lock_irqsave(&mem_ctl->substream_lock, flags);
	/*
	pr_debug("%s(), Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		 Afe_Block->u4DMAReadIdx);
	*/

	if (GetMemoryPathEnable(usb_mem_blk[stream]) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL2_CUR);
		if (HW_Cur_ReadIdx == 0) {
			pr_debug("%s(), HW_Cur_ReadIdx == 0\n", __func__);
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes =
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize +
					     HW_memory_index -
					     Afe_Block->u4DMAReadIdx;
		}

		/* sram (device memory) need 8 byte algin for arm64*/
		Afe_consumed_bytes = word_size_align(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		spin_unlock_irqrestore(&mem_ctl->substream_lock, flags);

		/*
		pr_debug("%s(),Rdx %x, Re%x, Cur%x, index %x, consum %x\n",
			__func__, Afe_Block->u4DMAReadIdx,
			Afe_Block->u4DataRemained, HW_Cur_ReadIdx,
			HW_memory_index, Afe_consumed_bytes);
		*/

		Frameidx = audio_bytes_to_frame(substream,
						Afe_Block->u4DMAReadIdx);
	} else {
		Frameidx = audio_bytes_to_frame(substream,
						Afe_Block->u4DMAReadIdx);
		spin_unlock_irqrestore(&mem_ctl->substream_lock, flags);
	}
	return Frameidx;
}

static snd_pcm_uframes_t
mtk_voice_usb_pointer_cap(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct afe_block_t *Awb_Block =
		&Get_Mem_ControlT(usb_mem_blk[stream])->rBlock;

	/*
	pr_debug("%s(), Awb_Block->u4WriteIdx = 0x%x\n", __func__,
		Awb_Block->u4WriteIdx);
	*/
	if (GetMemoryPathEnable(usb_mem_blk[stream]) == true) {
		/* sram (device memory) need 8 byte algin for arm64*/
		HW_Cur_ReadIdx = word_size_align(Afe_Get_Reg(AFE_AWB_CUR));
		if (HW_Cur_ReadIdx == 0) {
			pr_warn("%s(), HW_Cur_ReadIdx == 0\n", __func__);
			HW_Cur_ReadIdx = Awb_Block->pucPhysBufAddr;
		}
		HW_memory_index = HW_Cur_ReadIdx - Awb_Block->pucPhysBufAddr;

		/* update for data get to hardware */
		Hw_Get_bytes = (HW_Cur_ReadIdx - Awb_Block->pucPhysBufAddr) -
			       Awb_Block->u4WriteIdx;

		if (Hw_Get_bytes < 0)
			Hw_Get_bytes += Awb_Block->u4BufferSize;

		Awb_Block->u4WriteIdx += Hw_Get_bytes;
		Awb_Block->u4WriteIdx %= Awb_Block->u4BufferSize;
		Awb_Block->u4DataRemained += Hw_Get_bytes;

		/*
		pr_debug("%s RIdx=%x WIdx =%x Remain=%x Size=%x,bytes=%x\n",
			__func__, Awb_Block->u4DMAReadIdx,
			Awb_Block->u4WriteIdx, Awb_Block->u4DataRemained,
			Awb_Block->u4BufferSize, Hw_Get_bytes);
		*/

		/* buffer overflow */
		if (Awb_Block->u4DataRemained > Awb_Block->u4BufferSize) {
			pr_warn("%s overflow RIdx:%x, Wdx:%x, Re:%x,Size:%x\n",
				__func__, Awb_Block->u4DMAReadIdx,
				Awb_Block->u4WriteIdx,
				Awb_Block->u4DataRemained,
				Awb_Block->u4BufferSize);
		}

		/*
		pr_debug("%s(), HW_Cur_ReadIdx 0x%x, HW_memory_index 0x%x\n",
			__func__, HW_Cur_ReadIdx, HW_memory_index);
		*/

		if (usb_debug_enable & USB_DBG_ASSERT_AT_STOP) {
			struct timespec time;

			getrawmonotonic(&time);
			dbg_log[dbg_log_idx % NUM_DBG_LOG].idx = dbg_log_idx;
			snprintf(
				dbg_log[dbg_log_idx % NUM_DBG_LOG].log,
				DBG_LOG_LENGTH,
				"%ld.%09ld, %s(), ReadIdx 0x%x, WriteIdx 0x%x, Remained 0x%x, BufferSize 0x%x, Get_bytes 0x%x, HW_Cur_ReadIdx 0x%x, HW_memory_index 0x%x",
				time.tv_sec, time.tv_nsec, __func__,
				Awb_Block->u4DMAReadIdx, Awb_Block->u4WriteIdx,
				Awb_Block->u4DataRemained,
				Awb_Block->u4BufferSize, Hw_Get_bytes,
				HW_Cur_ReadIdx, HW_memory_index);

			dbg_log_idx++;
		}

		return audio_bytes_to_frame(substream, HW_memory_index);
	}
	return 0;
}

static snd_pcm_uframes_t
mtk_voice_usb_pointer(struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_voice_usb_pointer_play(substream);
	else
		return mtk_voice_usb_pointer_cap(substream);
}
#if 0
static DEFINE_SPINLOCK(auddrv_usb_cap_lock);
static int mtk_voice_usb_copy_cap(struct snd_pcm_substream *substream,
				  int channel, snd_pcm_uframes_t pos,
				  void __user *dst, snd_pcm_uframes_t count)
{
	struct afe_mem_control_t *pAWB_MEM_ConTrol = NULL;
	struct afe_block_t  *Awb_Block = NULL;
	char *Read_Data_Ptr = (char *)dst;
	kal_int32 DMA_Read_Ptr = 0, read_size = 0, read_count = 0;
	unsigned long flags;
	int stream = substream->stream;

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	pr_debug("%s(), pos = %lu count = %lu\n", __func__, pos, count);

	/* check which memif nned to be write */
	pAWB_MEM_ConTrol = Get_Mem_ControlT(usb_mem_blk[stream]);
	Awb_Block = &(pAWB_MEM_ConTrol->rBlock);

	if (pAWB_MEM_ConTrol == NULL) {
		pr_err("cannot find MEM control !!!!!!!\n");
		msleep(50);
		return 0;
	}

	if (Awb_Block->u4BufferSize <= 0) {
		msleep(50);
		return 0;
	}

	if (!Awb_Block->pucVirtBufAddr) {
		pr_err("%s(), pucVirtBufAddr == NULL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&auddrv_usb_cap_lock, flags);
	if (Awb_Block->u4DataRemained > Awb_Block->u4BufferSize) {
		pr_debug("%s(), u4DataRemained %x > u4BufferSize %x\n",
			__func__, Awb_Block->u4DataRemained,
			Awb_Block->u4BufferSize);
		Awb_Block->u4DataRemained = 0;
		Awb_Block->u4DMAReadIdx   = Awb_Block->u4WriteIdx;
	}
	if (count >  Awb_Block->u4DataRemained)
		read_size = Awb_Block->u4DataRemained;
	else
		read_size = count;

	DMA_Read_Ptr = Awb_Block->u4DMAReadIdx;
	spin_unlock_irqrestore(&auddrv_usb_cap_lock, flags);

	pr_debug("%s f0,rcount:%x,r_size:%x,Remain:%x, RIdx:%x, WIdx:%x\n",
		__func__, read_count, read_size, Awb_Block->u4DataRemained,
		Awb_Block->u4DMAReadIdx, Awb_Block->u4WriteIdx);

	if (DMA_Read_Ptr + read_size < Awb_Block->u4BufferSize) {
		if (DMA_Read_Ptr != Awb_Block->u4DMAReadIdx) {
			pr_warn("%s 1, read_size:0x%x, Remained:0x%x, Ptr:0x%x, ReadIdx:0x%x\n",
				__func__, read_size, Awb_Block->u4DataRemained,
				DMA_Read_Ptr, Awb_Block->u4DMAReadIdx);
		}

		if (copy_to_user((void __user *)Read_Data_Ptr,
				 (Awb_Block->pucVirtBufAddr + DMA_Read_Ptr),
				 read_size)) {
			pr_err("%s Fail 1 copy to user Ptr:%p, Addr:%p, ReadIdx:0x%x, Read_Ptr:0x%x, size:%x",
			       __func__, Read_Data_Ptr,
			       Awb_Block->pucVirtBufAddr,
			       Awb_Block->u4DMAReadIdx,
			       DMA_Read_Ptr,
			       read_size);
			return 0;
		}

		read_count += read_size;
		spin_lock(&auddrv_usb_cap_lock);
		Awb_Block->u4DataRemained -= read_size;
		Awb_Block->u4DMAReadIdx += read_size;
		Awb_Block->u4DMAReadIdx %= Awb_Block->u4BufferSize;
		DMA_Read_Ptr = Awb_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_usb_cap_lock);

		Read_Data_Ptr += read_size;
		count -= read_size;

		pr_debug("%s f1, size:%x, RIdx:%x, Widx:%x, Remain%x\n",
			__func__, read_size, Awb_Block->u4DMAReadIdx,
			Awb_Block->u4WriteIdx,
			Awb_Block->u4DataRemained);
	} else {
		unsigned int size_1 = Awb_Block->u4BufferSize - DMA_Read_Ptr;
		unsigned int size_2 = read_size - size_1;

		if (DMA_Read_Ptr != Awb_Block->u4DMAReadIdx) {
			pr_warn("%s 2, size1:0x%x, Remained:0x%x, Read_Ptr:0x%x, ReadIdx:0x%x\n",
				__func__, size_1, Awb_Block->u4DataRemained,
				DMA_Read_Ptr, Awb_Block->u4DMAReadIdx);
		}
		if (copy_to_user((void __user *)Read_Data_Ptr,
				 (Awb_Block->pucVirtBufAddr + DMA_Read_Ptr),
				 size_1)) {
			pr_err("%s Fail2 Ptr:%p, Add:%p, r:%x, Ptr:%x, size:%x",
			       __func__, Read_Data_Ptr,
			       Awb_Block->pucVirtBufAddr,
			       Awb_Block->u4DMAReadIdx, DMA_Read_Ptr,
			       read_size);
			return 0;
		}

		read_count += size_1;
		spin_lock(&auddrv_usb_cap_lock);
		Awb_Block->u4DataRemained -= size_1;
		Awb_Block->u4DMAReadIdx += size_1;
		Awb_Block->u4DMAReadIdx %= Awb_Block->u4BufferSize;
		DMA_Read_Ptr = Awb_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_usb_cap_lock);

		pr_debug("%s 2, size_1:0x%x, ridx:0x%x, widx:0x%x, empty:0x%x\n",
			__func__, size_1, Awb_Block->u4DMAReadIdx,
			Awb_Block->u4WriteIdx, Awb_Block->u4DataRemained);

		if (DMA_Read_Ptr != Awb_Block->u4DMAReadIdx) {
			pr_warn("%s 3, size2:%x, empty:%x, rp:0x%x, ridx:%x\n",
				__func__, size_2, Awb_Block->u4DataRemained,
				DMA_Read_Ptr, Awb_Block->u4DMAReadIdx);
		}
		if (copy_to_user((void __user *)(Read_Data_Ptr + size_1),
				 (Awb_Block->pucVirtBufAddr +
				 DMA_Read_Ptr), size_2)) {
			pr_warn("%s Fail userPtr:%p,Addr:%p,ridx:0x%x,Ptr:0x%x,size:0x%x",
				__func__, Read_Data_Ptr,
				Awb_Block->pucVirtBufAddr,
				Awb_Block->u4DMAReadIdx,
				DMA_Read_Ptr, read_size);
			return read_count << 2;
		}

		read_count += size_2;
		spin_lock(&auddrv_usb_cap_lock);
		Awb_Block->u4DataRemained -= size_2;
		Awb_Block->u4DMAReadIdx += size_2;
		DMA_Read_Ptr = Awb_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_usb_cap_lock);

		count -= read_size;
		Read_Data_Ptr += read_size;

		pr_debug("%s(),3, size_2:%x, Rdx:%x,WIdx:%x Re%x\n",
			__func__, size_2, Awb_Block->u4DMAReadIdx,
			Awb_Block->u4WriteIdx,
			Awb_Block->u4DataRemained);
	}

	return read_count >> 2;
}

static DEFINE_SPINLOCK(auddrv_usb_play_lock);
static int mtk_voice_usb_copy_play(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos,
				   void __user *dst, snd_pcm_uframes_t count)
{
	struct afe_block_t  *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;
	int stream = substream->stream;

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &Get_Mem_ControlT(usb_mem_blk[stream])->rBlock;

	/* handle for buffer management */
	pr_debug("%s(), WriteIdx = 0x%x, ReadIdx = 0x%x, DataRemained = 0x%x\n",
		__func__, Afe_Block->u4WriteIdx,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("%s(), error, u4BufferSize = 0", __func__);
		return 0;
	}

	AudDrv_checkDLISRStatus();

	spin_lock_irqsave(&auddrv_usb_play_lock, flags);
	copy_size = Afe_Block->u4BufferSize -
		    Afe_Block->u4DataRemained;  /* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_usb_play_lock, flags);

	if (count <=  copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	/*copy_size = word_size_align(copy_size);*/
	pr_debug("%s(), copy_size = 0x%x, count = 0x%x\n",
		__func__, copy_size, (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_usb_play_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_usb_play_lock, flags);
		/* copy once */
		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_warn("0ptr invalid data_w_ptr=%p, size=%d u4BufferSize=%d, u4DataRemained=%d",
				data_w_ptr, copy_size,
				Afe_Block->u4BufferSize,
				Afe_Block->u4DataRemained);
			} else {
				pr_debug("Idx= %p w_ptr = %p size = 0x%x\n",
					Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp,
					data_w_ptr, copy_size);
				if (copy_from_user((Afe_Block->pucVirtBufAddr +
				    Afe_WriteIdx_tmp),
				    data_w_ptr,
				    copy_size)) {
					pr_warn("[AudioWarn] Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_usb_play_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_usb_play_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			pr_debug("1,size:%x, W:%x, R=%x, Remain:%x, count=%x\n",
				copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained, (unsigned int)count);

		} else { /* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = (Afe_Block->u4BufferSize - Afe_WriteIdx_tmp);
			size_2 = (copy_size - size_1);
			pr_debug("size_1=0x%x, size_2=0x%x\n", size_1, size_2);

			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn("1ptr invalid data_w_ptr=%p, size_1=%d u4BufferSize=%d, u4DataRemained=%d",
					data_w_ptr, size_1,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
			} else {
				pr_debug("Idx= %p data_w_ptr = %p size_1 = %x\n",
					Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp,
					data_w_ptr, size_1);
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
				      Afe_WriteIdx_tmp),
				      data_w_ptr,
				      size_1))) {
					pr_warn(" Fail 1 copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_usb_play_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_usb_play_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr +
			size_1, size_2)) {
				pr_warn("2ptr data_w_ptr=%p, size_1=%d, size_2=%d Size=%d, u4DataRemained=%d",
					data_w_ptr, size_1, size_2,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained));
			} else {
				pr_debug("Idx= %p w_ptr+size_1=%p size_2=%x\n",
					Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp,
					data_w_ptr + size_1, size_2);
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					(data_w_ptr + size_1), size_2))) {
					pr_warn("write Fail 2 copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_usb_play_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_usb_play_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			pr_debug("2, size:%x, WIdx:%x,RIdx=%x Remai:%x\n",
				copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
		}
	}
	pr_debug("%s(), return\n", __func__);
	return 0;
}
#endif
static int mtk_voice_usb_copy(struct snd_pcm_substream *substream, int channel,
			      snd_pcm_uframes_t pos, void __user *dst,
			      snd_pcm_uframes_t count)
{
#if 1
	int stream = substream->stream;

	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       Get_Mem_ControlT(usb_mem_blk[stream]),
			       usb_mem_blk[stream]);
#else
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_voice_usb_copy_play(substream, channel, pos, dst,
					       count);
	else
		return mtk_voice_usb_copy_cap(substream, channel, pos, dst,
					      count);
#endif
}

static struct snd_pcm_ops mtk_voice_usb_ops = {
	.open = mtk_voice_usb_open,
	.close = mtk_voice_usb_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_voice_usb_hw_params,
	.hw_free = mtk_voice_usb_hw_free,
	.prepare = mtk_voice_usb_prepare,
	.trigger = mtk_voice_usb_trigger,
	.pointer = mtk_voice_usb_pointer,
	.copy = mtk_voice_usb_copy,
};

static int mtk_voice_usb_platform_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, speech_usb_controls,
				      ARRAY_SIZE(speech_usb_controls));
	return 0;
}

static int mtk_voice_usb_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	size = mtk_pcm_hardware.buffer_bytes_max;

	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						     card->dev, size, size);
}

static void mtk_voice_usb_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static struct snd_soc_platform_driver mtk_soc_voice_usb_platform = {
	.ops = &mtk_voice_usb_ops,
	.probe = mtk_voice_usb_platform_probe,
	.pcm_new = mtk_voice_usb_pcm_new,
	.pcm_free = mtk_voice_usb_pcm_free,
};

static int mtk_voice_usb_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_USB);

	usb_memif_lpbk.dev = &pdev->dev;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_voice_usb_platform);
}

static int mtk_voice_usb_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_soc_pcm_voice_usb_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_voice_usb",
	},
	{} };

static struct platform_driver mtk_voice_usb_driver = {
	.driver = {

			.name = MT_SOC_VOICE_USB,
			.owner = THIS_MODULE,
			.of_match_table = mt_soc_pcm_voice_usb_of_ids,
		},
	.probe = mtk_voice_usb_probe,
	.remove = mtk_voice_usb_remove,
};

module_platform_driver(mtk_voice_usb_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
