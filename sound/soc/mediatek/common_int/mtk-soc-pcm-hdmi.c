// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_hdmi.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio hdmi playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mt6799-hdmi.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-hdmi-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/* information about */

static struct afe_mem_control_t *pMemControl;
static bool mHDMIPrepareDone;
static struct audio_hdmi_format mAudioHDMIFormat;

static struct snd_dma_buffer *HDMI_dma_buf;

static DEFINE_SPINLOCK(auddrv_hdmi_lock);

/*
 *    function implementation
 */

static int mtk_hdmi_probe(struct platform_device *pdev);
static int mtk_pcm_hdmi_close(struct snd_pcm_substream *substream);
static int mtk_afe_hdmi_component_probe(struct snd_soc_component *component);

#define _DEBUG_TDM_KERNEL_ 1

static unsigned int table_sgen_golden_values[64] = {
	0x0FE50FE5, 0x285E1C44, 0x3F4A285E, 0x53C73414, 0x650C3F4A, 0x726F49E3,
	0x7B6C53C7, 0x7FAB5CDC, 0x7F02650C, 0x79776C43, 0x6F42726F, 0x60C67781,
	0x4E917B6C, 0x39587E27, 0x21EB7FAB, 0x09307FF4, 0xF01A7F02, 0xD7A17CD6,
	0xC0B67977, 0xAC3874ED, 0x9AF36F42, 0x8D906884, 0x849360C6, 0x80545818,
	0x80FD4E91, 0x86884449, 0x90BD3958, 0x9F3A2DDA, 0xB16E21EB, 0xC6A715A8,
	0xDE140930, 0xF6CFFCA1, 0x0FE5F01A, 0x285EE3BB, 0x3F4AD7A1, 0x53C7CBEB,
	0x650CC0B6, 0x726FB61C, 0x7B6CAC38, 0x7FABA323, 0x7F029AF3, 0x797793BC,
	0x6F428D90, 0x60C6887E, 0x4E918493, 0x395881D8, 0x21EB8054, 0x0930800B,
	0xF01A80FD, 0xD7A18329, 0xC0B68688, 0xAC388B12, 0x9AF390BD, 0x8D90977B,
	0x84939F3A, 0x8054A7E7, 0x80FDB16E, 0x8688BBB6, 0x90BDC6A7, 0x9F3AD225,
	0xB16EDE14, 0xC6A7EA57, 0xDE14F6CF, 0xF6CF035E};

static unsigned int table_sgen_4ch_golden_values[128] = {
	/* step   2              1              4              3 */
	/* ch2       ch1            ch4          ch3 */
	0x0FE50FE5, 0x0FE50FE5, 0x285E1C44, 0x3F4A3414, 0x3F4A285E, 0x650C53C7,
	0x53C73414, 0x7B6C6C43, 0x650C3F4A, 0x7F027B6C, 0x726F49E3, 0x6F427FF4,
	0x7B6C53C7, 0x4E917977, 0x7FAB5CDC, 0x21EB6884, 0x7F02650C, 0xF01A4E91,
	0x79776C43, 0xC0B62DDA, 0x6F42726F, 0x9AF30930, 0x60C67781, 0x8493E3BB,
	0x4E917B6C, 0x80FDC0B6, 0x39587E27, 0x90BDA323, 0x21EB7FAB, 0xB16E8D90,
	0x09307FF4, 0xDE1481D8, 0xF01A7F02, 0x0FE580FD, 0xD7A17CD6, 0x0FE58B12,
	0xC0B67977, 0x3F4A9F3A, 0xAC3874ED, 0x650CBBB6, 0x9AF36F42, 0x7B6CDE14,
	0x8D906884, 0x7F02035E, 0x849360C6, 0x6F420FE5, 0x80545818, 0x4E913414,
	0x80FD4E91, 0x21EB53C7, 0x86884449, 0xF01A6C43, 0x90BD3958, 0xC0B67B6C,
	0x9F3A2DDA, 0x9AF37FF4, 0xB16E21EB, 0x84937977, 0xC6A715A8, 0x80FD6884,
	0xDE140930, 0x90BD4E91, 0xF6CFFCA1, 0xB16E2DDA, 0x0FE5F01A, 0xDE140930,
	0x285EE3BB, 0x0FE5E3BB, 0x3F4AD7A1, 0x0FE5C0B6, 0x53C7CBEB, 0x3F4AA323,
	0x650CC0B6, 0x650C8D90, 0x726FB61C, 0x7B6C81D8, 0x7B6CAC38, 0x7F0280FD,
	0x7FABA323, 0x6F428B12, 0x7F029AF3, 0x4E919F3A, 0x797793BC, 0x21EBBBB6,
	0x6F428D90, 0xF01ADE14, 0x60C6887E, 0xC0B6035E, 0x4E918493, 0x9AF30FE5,
	0x395881D8, 0x84933414, 0x21EB8054, 0x80FD53C7, 0x0930800B, 0x90BD6C43,
	0xF01A80FD, 0xB16E7B6C, 0xD7A18329, 0xDE147FF4, 0xC0B68688, 0x0FE57977,
	0xAC388B12, 0x0FE56884, 0x9AF390BD, 0x3F4A4E91, 0x8D90977B, 0x650C2DDA,
	0x84939F3A, 0x7B6C0930, 0x8054A7E7, 0x7F02E3BB, 0x80FDB16E, 0x6F42C0B6,
	0x8688BBB6, 0x4E91A323, 0x90BDC6A7, 0x21EB8D90, 0x9F3AD225, 0xF01A81D8,
	0xB16EDE14, 0xC0B680FD, 0xC6A7EA57, 0x9AF38B12, 0xDE14F6CF, 0x84939F3A,
	0xF6CF035E, 0x80FDBBB6};

static unsigned int table_sgen_8ch_golden_values[] = {
	0x0FE50000, 0x0FE50FE5, 0x0FE50FE5, 0x0FE50FE5, 0x285E0000, 0x3F4A3414,
	0x285E1C44, 0x3F4A3414, 0x3F4A0000, 0x650C53C7, 0x3F4A285E, 0x650C53C7,
	0x53C70000, 0x7B6C6C43, 0x53C73414, 0x7B6C6C43, 0x650C0000, 0x7F027B6C,
	0x650C3F4A, 0x7F027B6C, 0x726F0000, 0x6F427FF4, 0x726F49E3, 0x6F427FF4,
	0x7B6C0000, 0x4E917977, 0x7B6C53C7, 0x4E917977, 0x7FAB0000, 0x21EB6884,
	0x7FAB5CDC, 0x21EB6884, 0x7F020000, 0xF01A4E91, 0x7F02650C, 0xF01A4E91,
	0x79770000, 0xC0B62DDA, 0x79776C43, 0xC0B62DDA, 0x6F420000, 0x9AF30930,
	0x6F42726F, 0x9AF30930, 0x60C60000, 0x8493E3BB, 0x60C67781, 0x8493E3BB,
	0x4E910000, 0x80FDC0B6, 0x4E917B6C, 0x80FDC0B6, 0x39580000, 0x90BDA323,
	0x39587E27, 0x90BDA323, 0x21EB0000, 0xB16E8D90, 0x21EB7FAB, 0xB16E8D90,
	0x09300000, 0xDE1481D8, 0x09307FF4, 0xDE1481D8, 0xF01A0000, 0x0FE580FD,
	0xF01A7F02, 0x0FE580FD, 0xD7A10000, 0x0FE58B12, 0xD7A17CD6, 0x0FE58B12,
	0xC0B60000, 0x3F4A9F3A, 0xC0B67977, 0x3F4A9F3A, 0xAC380000, 0x650CBBB6,
	0xAC3874ED, 0x650CBBB6, 0x9AF30000, 0x7B6CDE14, 0x9AF36F42, 0x7B6CDE14,
	0x8D900000, 0x7F02035E, 0x8D906884, 0x7F02035E, 0x84930000, 0x6F420FE5,
	0x849360C6, 0x6F420FE5, 0x80540000, 0x4E913414, 0x80545818, 0x4E913414,
	0x80FD0000, 0x21EB53C7, 0x80FD4E91, 0x21EB53C7, 0x86880000, 0xF01A6C43,
	0x86884449, 0xF01A6C43, 0x90BD0000, 0xC0B67B6C, 0x90BD3958, 0xC0B67B6C,
	0x9F3A0000, 0x9AF37FF4, 0x9F3A2DDA, 0x9AF37FF4, 0xB16E0000, 0x84937977,
	0xB16E21EB, 0x84937977, 0xC6A70000, 0x80FD6884, 0xC6A715A8, 0x80FD6884,
	0xDE140000, 0x90BD4E91, 0xDE140930, 0x90BD4E91, 0xF6CF0000, 0xB16E2DDA,
	0xF6CFFCA1, 0xB16E2DDA, 0x0FE50000, 0xDE140930, 0x0FE5F01A, 0xDE140930,
	0x285E0000, 0x0FE5E3BB, 0x285EE3BB, 0x0FE5E3BB, 0x3F4A0000, 0x0FE5C0B6,
	0x3F4AD7A1, 0x0FE5C0B6, 0x53C70000, 0x3F4AA323, 0x53C7CBEB, 0x3F4AA323,
	0x650C0000, 0x650C8D90, 0x650CC0B6, 0x650C8D90, 0x726F0000, 0x7B6C81D8,
	0x726FB61C, 0x7B6C81D8, 0x7B6C0000, 0x7F0280FD, 0x7B6CAC38, 0x7F0280FD,
	0x7FAB0000, 0x6F428B12, 0x7FABA323, 0x6F428B12, 0x7F020000, 0x4E919F3A,
	0x7F029AF3, 0x4E919F3A, 0x79770000, 0x21EBBBB6, 0x797793BC, 0x21EBBBB6,
	0x6F420000, 0xF01ADE14, 0x6F428D90, 0xF01ADE14, 0x60C60000, 0xC0B6035E,
	0x60C6887E, 0xC0B6035E, 0x4E910000, 0x9AF30FE5, 0x4E918493, 0x9AF30FE5,
	0x39580000, 0x84933414, 0x395881D8, 0x84933414, 0x21EB0000, 0x80FD53C7,
	0x21EB8054, 0x80FD53C7, 0x09300000, 0x90BD6C43, 0x0930800B, 0x90BD6C43,
	0xF01A0000, 0xB16E7B6C, 0xF01A80FD, 0xB16E7B6C, 0xD7A10000, 0xDE147FF4,
	0xD7A18329, 0xDE147FF4, 0xC0B60000, 0x0FE57977, 0xC0B68688, 0x0FE57977,
	0xAC380000, 0x0FE56884, 0xAC388B12, 0x0FE56884, 0x9AF30000, 0x3F4A4E91,
	0x9AF390BD, 0x3F4A4E91, 0x8D900000, 0x650C2DDA, 0x8D90977B, 0x650C2DDA,
	0x84930000, 0x7B6C0930, 0x84939F3A, 0x7B6C0930, 0x80540000, 0x7F02E3BB,
	0x8054A7E7, 0x7F02E3BB, 0x80FD0000, 0x6F42C0B6, 0x80FDB16E, 0x6F42C0B6,
	0x86880000, 0x4E91A323, 0x8688BBB6, 0x4E91A323, 0x90BD0000, 0x21EB8D90,
	0x90BDC6A7, 0x21EB8D90, 0x9F3A0000, 0xF01A81D8, 0x9F3AD225, 0xF01A81D8,
	0xB16E0000, 0xC0B680FD, 0xB16EDE14, 0xC0B680FD, 0xC6A70000, 0x9AF38B12,
	0xC6A7EA57, 0x9AF38B12, 0xDE140000, 0x84939F3A, 0xDE14F6CF, 0x84939F3A,
	0xF6CF0000, 0x80FDBBB6, 0xF6CF035E, 0x80FDBBB6,
};

static void copysinewavetohdmi(unsigned int channels)
{
	uint8_t *Bufferaddr;
	int Hdmi_Buffer_length;
	unsigned int arraybytes;
	unsigned int *SinewaveArr;
	int i;

	Bufferaddr = HDMI_dma_buf->area;
	Hdmi_Buffer_length = HDMI_dma_buf->bytes;
	SinewaveArr = NULL;
	arraybytes = 0;
	i = 0;

	if (channels == 2) {
		arraybytes = ARRAY_SIZE(table_sgen_golden_values) *
			     sizeof(table_sgen_golden_values[0]);
		SinewaveArr = table_sgen_golden_values;
	}
	if (channels == 4) {
		arraybytes = ARRAY_SIZE(table_sgen_4ch_golden_values) *
			     sizeof(table_sgen_4ch_golden_values[0]);
		SinewaveArr = table_sgen_4ch_golden_values;
	}
	if (channels == 8) {
		arraybytes = ARRAY_SIZE(table_sgen_8ch_golden_values) *
			     sizeof(table_sgen_8ch_golden_values[0]);
		SinewaveArr = table_sgen_8ch_golden_values;
	}
	if (channels == 0) {
		memset_io((void *)(Bufferaddr), 0x7f7f7f7f,
			  Hdmi_Buffer_length); /* using for observe data */
		pr_info("use fix pattern Bufferaddr = %p Hhdmi_Buffer_length = %d\n",
			Bufferaddr, Hdmi_Buffer_length);
		return;
	}

	pr_debug("%s buffer area = %p arraybytes = %d bufferlength = %zu\n",
		__func__, HDMI_dma_buf->area, arraybytes, HDMI_dma_buf->bytes);
	for (i = 0; i < HDMI_dma_buf->bytes; i += arraybytes) {
		memcpy((void *)(Bufferaddr + i), (void *)SinewaveArr,
		       arraybytes);
	}

	for (i = 0; i < 512; i++)
		pr_debug("Bufferaddr[%d] = %x\n", i, *(Bufferaddr + i));
}

static void SetHDMIAddress(void)
{
	Afe_Set_Reg(AFE_HDMI_BASE, HDMI_dma_buf->addr, 0xffffffff);
	Afe_Set_Reg(AFE_HDMI_END,
		    HDMI_dma_buf->addr + (HDMI_dma_buf->bytes - 1), 0xffffffff);
}

static int mHdmi_sidegen_control;
static int mHdmi_display_control;
static const char *const HDMI_SIDEGEN[] = {"Off", "On"};
static const char *const HDMI_DISPLAY[] = {"MHL", "SLIMPORT"};
static bool irq5_user;

static const struct soc_enum Audio_Hdmi_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(HDMI_SIDEGEN), HDMI_SIDEGEN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(HDMI_DISPLAY), HDMI_DISPLAY),
};

static int Audio_hdmi_SideGen_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), mHdmi_sidegen_control = %d\n", __func__, mHdmi_sidegen_control);
	ucontrol->value.integer.value[0] = mHdmi_sidegen_control;
	return 0;
}

bool set_tdm_lrck_inverse_by_channel(int out_channel)
{
	if (out_channel > 2)
		return SetTDMLrckInverse(false);
	else
		return SetTDMLrckInverse(true);
}

static int Audio_hdmi_SideGen_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct audio_hdmi_format *ptrAudioHDMIFormat;
	unsigned int runsamplerate = 44100;
	unsigned int outchannel = 2;
	/* unsigned int HDMIchannel = 8; */
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16_LE;

	ptrAudioHDMIFormat = &mAudioHDMIFormat;

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(HDMI_SIDEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mHdmi_sidegen_control = ucontrol->value.integer.value[0];

	if (mHdmi_sidegen_control) {
		unsigned int MclkDiv = 0;

		pr_debug("%s(),mHdmi_sidegen_control\n", __func__);

		/* Open */
		AudDrv_Clk_On();

		mtk_Hdmi_Configuration_Init((void *)ptrAudioHDMIFormat);

		mtk_Hdmi_Clock_Set((void *)ptrAudioHDMIFormat);

		EnableApll1(true);
		EnableApll2(true);

		EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_MCKDIV, true);
		EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_BCKDIV, true);

		AudDrv_APLL1Tuner_Clk_On();
		AudDrv_APLL2Tuner_Clk_On();

		/* Hw params */
		SetHighAddr(Soc_Aud_Digital_Block_MEM_HDMI, true,
			    HDMI_dma_buf->addr);
		AudDrv_Emi_Clk_On();
		SetHDMIAddress();

		/* Prepare */
		mtk_Hdmi_Configuration_Set((void *)ptrAudioHDMIFormat,
					   runsamplerate, outchannel, format,
					   mHdmi_display_control);

		MclkDiv =
			SetCLkMclk(ptrAudioHDMIFormat->mI2Snum, runsamplerate);

		/* SET hdmi channels , samplerate and formats */
		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_HDMI,
			ptrAudioHDMIFormat->mMemIfFetchFormatPerSample);
		SetHDMIdatalength(ptrAudioHDMIFormat->mHDMI_Data_Lens);
		SetTDMDatalength(ptrAudioHDMIFormat->mTDM_Data_Lens);

		/*SetCLkBclk(MclkDiv, runtime->rate, runtime->channels,
		 * Soc_Aud_I2S_WLEN_WLEN_16BITS);
		 */
		SetCLkBclk(MclkDiv, runsamplerate, outchannel,
			   ptrAudioHDMIFormat->mClock_Data_Lens);

		SetTDMLrckWidth(ptrAudioHDMIFormat->mTDM_LRCK);
		SetTDMbckcycle(ptrAudioHDMIFormat->mClock_Data_Lens);

		SetTDMChannelsSdata(ptrAudioHDMIFormat->msDATA_Channels);

		SetTDMDatalength(ptrAudioHDMIFormat->mTDM_Data_Lens);
		/* SetTDMLrckInverse(true); */
		SetTDMBckInverse(false);

		set_tdm_lrck_inverse_by_channel(outchannel);

		/* SetHDMIsamplerate(runtime->rate); */
		SetHDMIChannels(outchannel);

		SetTDMI2Smode(Soc_Aud_I2S_FORMAT_I2S);

		mtk_Hdmi_Set_Sdata((void *)ptrAudioHDMIFormat);

		SetHDMIClockInverse();
		mtk_Hdmi_Set_Interconnection((void *)ptrAudioHDMIFormat);

		/* Start */
		SetHDMIEnable(true);

		SetTDMEnable(true);

		AudDrv_TDM_Clk_On();

		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI, true);

#ifdef _DEBUG_TDM_KERNEL_
		SetConnection(Soc_Aud_InterCon_Connection,
			      Soc_Aud_InterConnectionInput_I00,
			      Soc_Aud_InterConnectionOutput_O09);
		SetConnection(Soc_Aud_InterCon_Connection,
			      Soc_Aud_InterConnectionInput_I01,
			      Soc_Aud_InterConnectionOutput_O10);
		SetHDMIDebugEnable(true);
#endif

		copysinewavetohdmi(outchannel);

		if (!irq5_user) {
			irq_add_user(&irq5_user,
				     irq_request_number(
					     Soc_Aud_Digital_Block_MEM_HDMI),
				     runsamplerate, 1024);
			irq5_user = true;
		}
		EnableAfe(true);
		SetHDMIDumpReg();

	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI, false);
		SetHDMIEnable(false);
		SetTDMEnable(false);

		AudDrv_Emi_Clk_Off();

		AudDrv_APLL1Tuner_Clk_Off();
		AudDrv_APLL2Tuner_Clk_Off();

		EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_MCKDIV, false);
		EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_BCKDIV, false);

		EnableApll1(false);
		EnableApll2(false);

		EnableAfe(false);
		AudDrv_TDM_Clk_Off(); /* disable HDMI CK */
		AudDrv_Clk_Off();
	}
	return 0;
}

static int Audio_hdmi_disport_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_hdmi_slimport_Get = %d\n", mHdmi_display_control);
	ucontrol->value.integer.value[0] = mHdmi_display_control;
	return 0;
}

static int Audio_hdmi_disport_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_hdmi_slimport_Set = %d\n", mHdmi_display_control);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(HDMI_DISPLAY)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mHdmi_display_control = ucontrol->value.integer.value[0];
	return 0;
}
static const struct snd_kcontrol_new Audio_snd_hdmi_controls[] = {
	SOC_ENUM_EXT("Audio_Hdmi_SideGen_Switch", Audio_Hdmi_Enum[0],
		     Audio_hdmi_SideGen_Get, Audio_hdmi_SideGen_Set),
	SOC_ENUM_EXT("Audio_Hdmi_Display_Switch", Audio_Hdmi_Enum[1],
		     Audio_hdmi_disport_Get, Audio_hdmi_disport_Set),
};

static struct snd_pcm_hardware mtk_hdmi_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = HDMI_USE_CHANNELS_MIN,
	.channels_max = HDMI_USE_CHANNELS_MAX,
	.buffer_bytes_max = HDMI_MAX_BUFFER_SIZE,
	.period_bytes_max = HDMI_MAX_PERIODBYTE_SIZE,
	.periods_min = HDMI_MIN_PERIOD_SIZE,
	.periods_max = HDMI_MAX_8CH_24BIT_PERIOD_SIZE,
	.fifo_size = 0,
};

static int mtk_pcm_hdmi_stop(struct snd_pcm_substream *substream)
{
	struct afe_block_t *Afe_Block = &(pMemControl->rBlock);

	pr_debug("%s()\n", __func__);

	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_HDMI));

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI, false);

#ifdef _DEBUG_TDM_KERNEL_
	SetHDMIDebugEnable(false);
/*msleep(1); */
#endif
	SetTDMEnable(false); /* disable TDM */
	SetHDMIEnable(false);

	AudDrv_TDM_Clk_Off(); /* disable HDMI CK */
	EnableAfe(false);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_HDMI, substream);

	Afe_Block->u4DMAReadIdx = 0;
	Afe_Block->u4WriteIdx = 0;
	Afe_Block->u4DataRemained = 0;

	return 0;
}

static snd_pcm_uframes_t
mtk_pcm_hdmi_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	struct afe_block_t *Afe_Block = &(pMemControl->rBlock);
#if defined(HDMI_DEBUG_LOG)
	pr_debug(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
			Afe_Block->u4DMAReadIdx);
#endif
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_HDMI_CUR);
		if (HW_Cur_ReadIdx == 0) {
#if defined(HDMI_DEBUG_LOG)
			pr_debug("[Auddrv] HW_Cur_ReadIdx ==0\n");
#endif
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}
		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
		if (HW_memory_index >= Afe_Block->u4DMAReadIdx) {
			Afe_consumed_bytes =
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		} else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize +
					     HW_memory_index -
					     Afe_Block->u4DMAReadIdx;
		}
		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
#if defined(HDMI_DEBUG_LOG)
		pr_debug(
			"[Auddrv] HW_Cur_ReadIdx = 0x%x, HW_memory_index = 0x%x, Afe_consumed_bytes = 0x%x\n",
			HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);
#endif
		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	return Frameidx;
}

static void SetHDMIBuffer(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *hw_params)
{
	kal_uint32 u4tmpMrg1;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct afe_block_t *pblock = &(pMemControl->rBlock);

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f; /* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s, dma_bytes = %d, dma_area = %p, dma_addr = 0x%x\n",
			__func__, pblock->u4BufferSize, pblock->pucVirtBufAddr,
			pblock->pucPhysBufAddr);
#endif
	Afe_Set_Reg(AFE_HDMI_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_HDMI_END,
		    pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);

	u4tmpMrg1 = Afe_Get_Reg(AFE_HDMI_BASE);
	u4tmpMrg1 &= 0x00ffffff;
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s(), AFE_HDMI_BASE = 0x%x\n", __func__, u4tmpMrg1);
#endif
}

static int mtk_pcm_hdmi_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s(), HDMI_dma_buf->area\n", __func__);
#endif
	HDMI_dma_buf->bytes = substream->runtime->dma_bytes =
		params_buffer_bytes(hw_params);
	runtime->dma_area = HDMI_dma_buf->area;
	runtime->dma_addr = HDMI_dma_buf->addr;
	SetHighAddr(Soc_Aud_Digital_Block_MEM_HDMI, true, runtime->dma_addr);
	AudDrv_Emi_Clk_On();

#if defined(HDMI_DEBUG_LOG)
	pr_debug("2 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
			substream->runtime->dma_bytes,
			substream->runtime->dma_area,
			(long)substream->runtime->dma_addr);
#endif
	SetHDMIBuffer(substream, hw_params);
	return ret;
}

static int mtk_pcm_hdmi_hw_free(struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();
	return 0;
}

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {8000,  11025, 12000, 16000,
						22050, 24000, 32000, 44100,
						48000, 88200, 96000, 192000};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_hdmi_open(struct snd_pcm_substream *substream)
{
	struct audio_hdmi_format *ptrAudioHDMIFormat;
	struct snd_pcm_runtime *runtime;
	int ret;

	runtime = substream->runtime;

	ptrAudioHDMIFormat = &mAudioHDMIFormat;

	pr_debug("%s()\n", __func__);

	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_HDMI);

	runtime->hw = mtk_hdmi_hardware;

	AudDrv_Clk_On();

	mtk_Hdmi_Configuration_Init((void *)ptrAudioHDMIFormat);

	mtk_Hdmi_Clock_Set((void *)ptrAudioHDMIFormat);

	memcpy((void *)(&(runtime->hw)), (void *)&mtk_hdmi_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	/* print for hw pcm information */
#if defined(HDMI_DEBUG_LOG)
	pr_debug(
		"%s(), runtime rate = %d, channels = %d, substream->pcm->device = %d\n",
		__func__, runtime->rate, runtime->channels, substream->pcm->device);
#endif
	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_MMAP_VALID;

	EnableApll1(true);
	EnableApll2(true);

	EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_MCKDIV, true);
	EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_BCKDIV, true);

	AudDrv_APLL1Tuner_Clk_On();
	AudDrv_APLL2Tuner_Clk_On();
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s(), return\n", __func__);
#endif
	return 0;
}

static int mtk_pcm_hdmi_close(struct snd_pcm_substream *substream)
{
	struct audio_hdmi_format *ptrAudioHDMIFormat;

	ptrAudioHDMIFormat = &mAudioHDMIFormat;

	pr_debug("%s\n", __func__);

	AudDrv_APLL1Tuner_Clk_Off();
	AudDrv_APLL2Tuner_Clk_Off();

	mHDMIPrepareDone = false;

	EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_MCKDIV, false);
	EnableI2SCLKDiv(ptrAudioHDMIFormat->mI2S_BCKDIV, false);
	EnableApll1(false);
	EnableApll2(false);

	AudDrv_Clk_Off();

	return 0;
}

static int mtk_pcm_hdmi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;
	struct audio_hdmi_format *ptrAudioHDMIFormat;
	unsigned int MclkDiv;

	runtime = substream->runtime;
	ptrAudioHDMIFormat = &mAudioHDMIFormat;

	pr_debug("%s format =%d, rate = %d  ch = %d size = %lu, control=%d\n",
		__func__, runtime->format, runtime->rate, runtime->channels,
		runtime->period_size, mHdmi_display_control);

	if (mHDMIPrepareDone == false) {

		mtk_Hdmi_Configuration_Set((void *)ptrAudioHDMIFormat,
					   runtime->rate, runtime->channels,
					   runtime->format,
					   mHdmi_display_control);

		MclkDiv =
			SetCLkMclk(ptrAudioHDMIFormat->mI2Snum, runtime->rate);

		/* SET hdmi channels , samplerate and formats */

		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_HDMI,
			ptrAudioHDMIFormat->mMemIfFetchFormatPerSample);
		SetHDMIdatalength(ptrAudioHDMIFormat->mHDMI_Data_Lens);
		SetTDMDatalength(ptrAudioHDMIFormat->mTDM_Data_Lens);

		/*SetCLkBclk(MclkDiv, runtime->rate, runtime->channels,
		 * Soc_Aud_I2S_WLEN_WLEN_16BITS);
		 */
		SetCLkBclk(MclkDiv, runtime->rate,
			   ptrAudioHDMIFormat->mHDMI_Channels,
			   ptrAudioHDMIFormat->mClock_Data_Lens);
		SetTDMLrckWidth(ptrAudioHDMIFormat->mTDM_LRCK);
		SetTDMbckcycle(ptrAudioHDMIFormat->mClock_Data_Lens);

		SetTDMChannelsSdata(ptrAudioHDMIFormat->msDATA_Channels);

		SetTDMDatalength(ptrAudioHDMIFormat->mTDM_Data_Lens);
		/*SetTDMLrckInverse(true);*/
		SetTDMBckInverse(false);

		if (runtime->channels > 2)
			SetTDMLrckInverse(false);
		else
			SetTDMLrckInverse(true);

/*SetHDMIsamplerate(runtime->rate);*/

#ifdef __2CH_TO_8CH
		SetHDMIChannels(8);
#else
		SetHDMIChannels(runtime->channels);
#endif
		SetTDMI2Smode(Soc_Aud_I2S_FORMAT_I2S);

		mtk_Hdmi_Set_Sdata((void *)ptrAudioHDMIFormat);

		SetHDMIClockInverse();
		mtk_Hdmi_Set_Interconnection((void *)ptrAudioHDMIFormat);

		mHDMIPrepareDone = true;
	}

	return 0;
}

static int mtk_pcm_hdmi_start(struct snd_pcm_substream *substream)
{

	/* unsigned int u32AudioI2S = 0; */

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_HDMI, substream);

	SetHDMIEnable(true);
	SetTDMEnable(true);
	/*Afe_Set_Reg(AUDIO_AUDIO_TOP_CON0, 0 << 20,  1 << 20); //  enable HDMI
	 * CK
	 */
	AudDrv_TDM_Clk_On();

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI, true);

#ifdef _DEBUG_TDM_KERNEL_
	SetConnection(Soc_Aud_InterCon_Connection,
		      Soc_Aud_InterConnectionInput_I00,
		      Soc_Aud_InterConnectionOutput_O09);
	SetConnection(Soc_Aud_InterCon_Connection,
		      Soc_Aud_InterConnectionInput_I01,
		      Soc_Aud_InterConnectionOutput_O10);
	SetHDMIDebugEnable(true);
#endif

#ifdef _TONE_TEST
	copysinewavetohdmi(substream->runtime->channels);
#endif

	/* here to set interrupt */
	/* ALPS01889945 , stereo , multi channel switch A/V sync issue */
	/* 32bit , stereo , 64 BCK  for one count, (hal size)8192 bytes/(64/8) =
	 * 1024 count
	 */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_HDMI),
		     substream->runtime->rate, 1024);

	EnableAfe(true);

	SetHDMIDumpReg();

	return 0;
}

static int mtk_pcm_hdmi_trigger(struct snd_pcm_substream *substream, int cmd)
{
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s(), cmd = %d\n", __func__, cmd);
#endif
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_hdmi_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_hdmi_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_hdmi_copy(struct snd_pcm_substream *substream, int channel,
			     snd_pcm_uframes_t pos, void __user *dst,
			     snd_pcm_uframes_t count)
{
	struct afe_block_t *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;

	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &(pMemControl->rBlock);

	/* handle for buffer management */

#if defined(HDMI_DEBUG_LOG)
	pr_debug(
		"%s(), count = %d, WriteIdx = %x, ReadIdx = %x, DataRemained = %x\n",
		__func__, (kal_uint32)count, Afe_Block->u4WriteIdx,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
#endif

	if (Afe_Block->u4BufferSize == 0) {
#if defined(HDMI_DEBUG_LOG)
		pr_debug("AudDrv_write: u4BufferSize=0 Error");
#endif
		return 0;
	}

	spin_lock_irqsave(&auddrv_hdmi_lock, flags);
	copy_size = Afe_Block->u4BufferSize -
		    Afe_Block->u4DataRemained; /* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);
	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_hdmi_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size <
		    Afe_Block->u4BufferSize) { /* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
#if defined(HDMI_DEBUG_LOG)
				pr_debug(
					"%s(), 0ptr invalid data_w_ptr = %p, size = %d, u4BufferSize = %d, u4DataRemained = %d",
					__func__,
					data_w_ptr, copy_size,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
#endif
			} else {
#if defined(HDMI_DEBUG_LOG)
				pr_debug(
					"%s(), WriteIdx = 0x%x, data_w_ptr = %p, copy_size = 0x%x\n",
					__func__, Afe_Block->pucVirtBufAddr +
							  Afe_WriteIdx_tmp,
					data_w_ptr, copy_size);
#endif
				if (copy_from_user((Afe_Block->pucVirtBufAddr +
						    Afe_WriteIdx_tmp),
						   data_w_ptr, copy_size)) {
#if defined(HDMI_DEBUG_LOG)
					pr_debug("AudDrv_write Fail copy from user\n");
#endif
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_hdmi_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;
#if defined(HDMI_DEBUG_LOG)
			pr_debug(
				"%s(), finish1, copy_size = %x, WriteIdx = %x, ReadIdx = %x, Remained = %x \r\n",
				__func__, copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
#endif

		} else { /* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = Afe_Block->u4BufferSize - Afe_WriteIdx_tmp;
			size_2 = copy_size - size_1;
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
#if defined(HDMI_DEBUG_LOG)
				pr_warn("HDMI_write 1ptr invalid data_w_ptr = %p, size_1 = %d, u4BufferSize = %d, u4DataRemained = %d",
					data_w_ptr, size_1,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
#endif
			} else {
#if defined(HDMI_DEBUG_LOG)
				pr_debug(
					"%s(), WriteIdx = %x, data_w_ptr = %p, size_1 = %x\n",
					__func__, Afe_Block->pucVirtBufAddr +
							  Afe_WriteIdx_tmp,
					data_w_ptr, size_1);
#endif
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
						     Afe_WriteIdx_tmp),
						    data_w_ptr, size_1))) {
#if defined(HDMI_DEBUG_LOG)
					pr_warn("HDMI_write Fail 1 copy from user");
#endif
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_hdmi_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1,
				       size_2)) {
#if defined(HDMI_DEBUG_LOG)
				pr_warn(
					"HDMI_write 2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d u4BufferSize=%d, u4DataRemained=%d",
					data_w_ptr, size_1, size_2,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
#endif
			} else {
#if defined(HDMI_DEBUG_LOG)
				pr_debug(
					"%s(), WriteIdx = %x, data_w_ptr+size_1 = %p, size_2 = %x\n",
					__func__, Afe_Block->pucVirtBufAddr +
							  Afe_WriteIdx_tmp,
					data_w_ptr + size_1, size_2);
#endif
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
						     Afe_WriteIdx_tmp),
						    (data_w_ptr + size_1),
						    size_2))) {
#if defined(HDMI_DEBUG_LOG)
					pr_warn(
						"%s(), Fail 2 copy from user", __func__);
#endif
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_hdmi_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;
#if defined(HDMI_DEBUG_LOG)
			pr_debug(
				"%s(), finish2, size = %x, WriteIdx = %x, ReadIdx = %x, DataRemained = %x \r\n",
				__func__, copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
#endif
		}
	}

	return 0;
	/* pr_debug("dummy_pcm_copy return\n"); */
}

static int mtk_pcm_hdmi_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_hdmi_ops = {
	.open = mtk_pcm_hdmi_open,
	.close = mtk_pcm_hdmi_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hdmi_hw_params,
	.hw_free = mtk_pcm_hdmi_hw_free,
	.prepare = mtk_pcm_hdmi_prepare,
	.trigger = mtk_pcm_hdmi_trigger,
	.pointer = mtk_pcm_hdmi_pointer,
	.copy = mtk_pcm_hdmi_copy,
	.silence = mtk_pcm_hdmi_silence,
	.page = mtk_pcm_page,
};

static const struct snd_soc_component_driver mtk_hdmi_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_hdmi_ops,
	.probe = mtk_afe_hdmi_component_probe,
};

static int mtk_hdmi_probe(struct platform_device *pdev)
{
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_HDMI_PCM);
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
#endif
	return snd_soc_register_component(&pdev->dev, &mtk_hdmi_soc_component);
}

static int mtk_afe_hdmi_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(component->dev,
				   Soc_Aud_Digital_Block_MEM_HDMI,
				   HDMI_MAX_BUFFER_SIZE);
	HDMI_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_HDMI);

	snd_soc_add_component_controls(component, Audio_snd_hdmi_controls,
				      ARRAY_SIZE(Audio_snd_hdmi_controls));
	return 0;
}

static int mtk_afe_remove(struct platform_device *pdev)
{
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_hdmi_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_hdmi",
	},
	{} };
#endif

static struct platform_driver mtk_hdmi_driver = {
	.driver = {

			.name = MT_SOC_HDMI_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_hdmi_of_ids,
#endif
		},
	.probe = mtk_hdmi_probe,
	.remove = mtk_afe_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkhdmi_dev;
#endif

static int __init mtk_hdmi_soc_platform_init(void)
{
	int ret;
#if defined(HDMI_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
#ifndef CONFIG_OF
	soc_mtkhdmi_dev = platform_device_alloc(MT_SOC_HDMI_PCM, -1);
	if (!soc_mtkhdmi_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkhdmi_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkhdmi_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_hdmi_driver);
	return ret;
}
module_init(mtk_hdmi_soc_platform_init);

static void __exit mtk_hdmi_soc_platform_exit(void)
{

	platform_driver_unregister(&mtk_hdmi_driver);
}
module_exit(mtk_hdmi_soc_platform_exit);

/* EXPORT_SYMBOL(Auddrv_Hdmi_Interrupt_Handler); */

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
