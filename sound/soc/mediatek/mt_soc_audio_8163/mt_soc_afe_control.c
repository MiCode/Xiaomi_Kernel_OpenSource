/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
//#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <asm/div64.h>
#include <mt-plat/aee.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>

#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

//#include <mt-plat/mt_boot.h>
//#include <mt-plat/mt_boot_common.h>

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_digital_type.h"
#include "AudDrv_Def.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_afe_connection.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Gpio.h"


static DEFINE_SPINLOCK(afe_control_lock);
static DEFINE_SPINLOCK(afe_sram_control_lock);


/* static  variable */
static bool AudioDaiBtStatus;
static bool AudioAdcI2SStatus;
static bool Audio2ndAdcI2SStatus;
static bool AudioMrgStatus;
static bool mAudioInit;
static bool mVOWStatus;
static unsigned int MCLKFS = 128;
static struct AudioDigtalI2S *AudioAdcI2S;
static struct AudioDigtalI2S *m2ndI2S;	/* input */
static struct AudioDigtalI2S *m2ndI2Sout;	/* output */
static bool mFMEnable;
static bool mOffloadEnable;
static bool mOffloadSWMode;

static struct AudioHdmi *mHDMIOutput;
static struct AudioMrgIf *mAudioMrg;
static struct AudioDigitalDAIBT *AudioDaiBt;

static struct AFE_MEM_CONTROL_T *AFE_Mem_Control_context[
	Soc_Aud_Digital_Block_MEM_HDMI + 1];
static struct snd_dma_buffer *Audio_dma_buf[Soc_Aud_Digital_Block_MEM_HDMI + 1];

static struct AudioIrqMcuMode *mAudioMcuMode[
	Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE];
static struct AudioMemIFAttribute *mAudioMEMIF[
	Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK];

static struct AudioAfeRegCache mAudioRegCache;
static struct AudioSramManager mAudioSramManager;
const unsigned int AudioSramPlaybackFullSize = 1024 * 36;
const unsigned int AudioSramPlaybackPartialSize = 1024 * 36;
const unsigned int AudioDramPlaybackSize = 1024 * 36;
const size_t AudioSramCaptureSize = 1024 * 36;
const size_t AudioDramCaptureSize = 1024 * 36;
const size_t AudioInterruptLimiter = 100;
static int Aud_APLL_DIV_APLL1_cntr;
static int Aud_APLL_DIV_APLL2_cntr;
static int aud_hdmi_clk_cntr;

static bool mExternalModemStatus;

/* mutex lock */
static DEFINE_MUTEX(afe_control_mutex);
static DEFINE_SPINLOCK(auddrv_dl1_lock);
static DEFINE_SPINLOCK(auddrv_ul1_lock);


static const uint16_t kSideToneCoefficientTable16k[] = {
	0x049C, 0x09E8, 0x09E0, 0x089C,
	0xFF54, 0xF488, 0xEAFC, 0xEBAC,
	0xfA40, 0x17AC, 0x3D1C, 0x6028,
	0x7538
};

static const uint16_t kSideToneCoefficientTable32k[] = {
	0xff58, 0x0063, 0x0086, 0x00bf,
	0x0100, 0x013d, 0x0169, 0x0178,
	0x0160, 0x011c, 0x00aa, 0x0011,
	0xff5d, 0xfea1, 0xfdf6, 0xfd75,
	0xfd39, 0xfd5a, 0xfde8, 0xfeea,
	0x005f, 0x0237, 0x0458, 0x069f,
	0x08e2, 0x0af7, 0x0cb2, 0x0df0,
	0x0e96
};

/*
 *    function implementation
 */
static irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id);
static void Clear_Mem_CopySize(enum Soc_Aud_Digital_Block MemBlock);
static kal_uint32 Get_Mem_MaxCopySize(enum Soc_Aud_Digital_Block MemBlock);

static bool CheckSize(uint32 size)
{
	if (size == 0) {
		pr_err("CheckSize size = 0\n");
		return true;
	}
	return false;
}

void AfeControlMutexLock(void)
{
	mutex_lock(&afe_control_mutex);
}

void AfeControlMutexUnLock(void)
{
	mutex_unlock(&afe_control_mutex);
}

void AfeControlSramLock(void)
{
	spin_lock(&afe_sram_control_lock);
}

void AfeControlSramUnLock(void)
{
	spin_unlock(&afe_sram_control_lock);
}


unsigned int GetSramState(void)
{
	return mAudioSramManager.mMemoryState;
}

void SetSramState(unsigned int State)
{
	pr_debug("%s state= %d\n", __func__, State);
	mAudioSramManager.mMemoryState |= State;
}

void ClearSramState(unsigned int State)
{
	pr_debug("%s state= %d\n", __func__, State);
	mAudioSramManager.mMemoryState &= (~State);
}

unsigned int GetPLaybackSramFullSize(void)
{
	unsigned int Sramsize = AudioSramPlaybackFullSize;

	if (AudioSramPlaybackFullSize > AFE_INTERNAL_SRAM_SIZE)
		Sramsize = AFE_INTERNAL_SRAM_SIZE;
	return Sramsize;
}

unsigned int GetPLaybackSramPartial(void)
{
	unsigned int Sramsize = AudioSramPlaybackPartialSize;

	if (Sramsize > AFE_INTERNAL_SRAM_SIZE)
		Sramsize = AFE_INTERNAL_SRAM_SIZE;
	return Sramsize;
}

unsigned int GetPLaybackDramSize(void)
{
	return AudioDramPlaybackSize;
}

size_t GetCaptureSramSize(void)
{
	unsigned int Sramsize = AudioSramCaptureSize;

	if (Sramsize > AFE_INTERNAL_SRAM_SIZE)
		Sramsize = AFE_INTERNAL_SRAM_SIZE;
	return Sramsize;
}

size_t GetCaptureDramSize(void)
{
	return AudioDramCaptureSize;
}

void SetFMEnableFlag(bool bEnable)
{
	mFMEnable = bEnable;
}

void SetOffloadEnableFlag(bool bEnable)
{
	mOffloadEnable = bEnable;
}

void SetOffloadSWMode(bool bEnable)
{
	mOffloadSWMode = bEnable;
}

bool ConditionEnterSuspend(void)
{
	if ((mFMEnable == true) || (mOffloadEnable == true))
		return false;
	{
		return true;
	}
}

/* function get internal mode status */
bool get_internalmd_status(void)
{
	bool ret = (get_voice_bt_status() || get_voice_status());

	return (mExternalModemStatus == true) ? false : ret;
}

static void FillDatatoDlmemory(unsigned int *memorypointer,
			       unsigned int fillsize, unsigned short value)
{
	int addr = 0;
	unsigned int tempvalue = value;

	tempvalue = tempvalue << 16;
	tempvalue |= value;
	/* set memory to DC value */
	for (addr = 0; addr < (fillsize >> 2); addr++) {
		*memorypointer = tempvalue;
		memorypointer++;
	}
}

static struct snd_dma_buffer *Dl1_Playback_dma_buf;

static void SetDL1BufferwithBuf(void)
{
#define Dl1_MAX_BUFFER_SIZE     (48*1024)
	AudDrv_Allocate_mem_Buffer(NULL,
		Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	Afe_Set_Reg(AFE_DL1_BASE, Dl1_Playback_dma_buf->addr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END,
		Dl1_Playback_dma_buf->addr + (Dl1_MAX_BUFFER_SIZE - 1),
		0xffffffff);
}


void OpenAfeDigitaldl1(bool bEnable)
{
	unsigned int *Sramdata;

	if (bEnable == true) {
		SetDL1BufferwithBuf();
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_16_BIT);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_16_BIT);

		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O03);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					  Soc_Aud_InterConnectionOutput_O04);

		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, 44100);

		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I05,
			Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I06,
			Soc_Aud_InterConnectionOutput_O04);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

		Sramdata = (unsigned int *)(Dl1_Playback_dma_buf->area);
		FillDatatoDlmemory(Sramdata, Dl1_Playback_dma_buf->bytes, 0);
		/* msleep(5); */
		usleep_range(5*1000, 20*1000);

		if (GetMemoryPathEnable(
			Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
				 true);
			SetI2SDacOut(44100, false,
				 Soc_Aud_I2S_WLEN_WLEN_16BITS);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(
				Soc_Aud_Digital_Block_I2S_OUT_DAC, true);

		EnableAfe(true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		EnableAfe(false);
	}
}

void SetExternalModemStatus(const bool bEnable)
{
	pr_debug("%s(), mExternalModemStatus: %d => %d\n", __func__,
		mExternalModemStatus, bEnable);
	mExternalModemStatus = bEnable;
}




bool InitAfeControl(void)
{
	int i = 0;

	pr_debug("InitAfeControl\n");

	/* first time to init, reg init */
	Auddrv_Reg_map();
	AudDrv_Clk_Power_On();
	Auddrv_Bus_Init();
	Auddrv_Read_Efuse_HPOffset();
	AfeControlMutexLock();
	/* allocate memory for pointers */
	if (mAudioInit == false) {
		mAudioInit = true;
		mAudioMrg = kzalloc(sizeof(struct AudioMrgIf), GFP_KERNEL);
		AudioDaiBt =
			kzalloc(sizeof(struct AudioDigitalDAIBT), GFP_KERNEL);
		AudioAdcI2S =
			kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2Sout = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		mHDMIOutput = kzalloc(sizeof(struct AudioHdmi), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++) {
			mAudioMcuMode[i] =
				 kzalloc(sizeof(struct AudioIrqMcuMode),
				 GFP_KERNEL);
		}

		for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK;
			i++) {
			mAudioMEMIF[i] =
				kzalloc(sizeof(struct AudioMemIFAttribute),
				GFP_KERNEL);
		}

		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++) {
			AFE_Mem_Control_context[i] =
				 kzalloc(sizeof(struct AFE_MEM_CONTROL_T),
				GFP_KERNEL);
			AFE_Mem_Control_context[i]->substreamL = NULL;
			spin_lock_init(
				&AFE_Mem_Control_context[i]->substream_lock);
		}

		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++)
			Audio_dma_buf[i] =
				 kzalloc(sizeof(Audio_dma_buf), GFP_KERNEL);
	}
	AudioDaiBtStatus = false;
	AudioAdcI2SStatus = false;
	Audio2ndAdcI2SStatus = false;
	AudioMrgStatus = false;
	memset((void *)&mAudioSramManager, 0,
	sizeof(struct AudioSramManager));
	mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(44100);

	for (i = AUDIO_APLL1_DIV0; i <= AUDIO_APLL12_DIV3; i++)
		EnableI2SDivPower(i, false);

	EnableHDMIDivPower(AUDIO_APLL_HDMI_BCK_DIV, false);
	EnableSpdifDivPower(AUDIO_APLL_SPDIF_DIV, false);
	EnableSpdif2DivPower(AUDIO_APLL_SPDIF2_DIV, false);
	/* set APLL clock setting */
	AfeControlMutexUnLock();
	return true;
}

bool ResetAfeControl(void)
{
	int i = 0;

	pr_debug("ResetAfeControl\n");
	AfeControlMutexLock();
	mAudioInit = false;
	memset((void *)(mAudioMrg), 0, sizeof(struct AudioMrgIf));
	memset((void *)(AudioDaiBt), 0, sizeof(struct AudioDigitalDAIBT));

	for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++) {
		memset((void *)(mAudioMcuMode[i]), 0,
			sizeof(struct AudioIrqMcuMode));
	}

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++) {
		memset((void *)(mAudioMEMIF[i]), 0,
			sizeof(struct AudioMemIFAttribute));
	}

	for (i = 0; i < (Soc_Aud_Digital_Block_MEM_HDMI + 1); i++)
		memset((void *)(AFE_Mem_Control_context[i]), 0,
			sizeof(struct AFE_MEM_CONTROL_T));

	AfeControlMutexUnLock();
	return true;
}



bool Register_Aud_Irq(void *dev, uint32 afe_irq_number)
{
	int ret;

#ifdef CONFIG_OF
	ret =
		request_irq(afe_irq_number, AudDrv_IRQ_handler,
		IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
	if (ret)
		pr_err("Register_Aud_Irq AFE IRQ register fail!!!\n");
#else
	pr_debug("%s dev name =%s\n", __func__, dev_name(dev));
	ret =
	    request_irq(MT8163_AFE_MCU_IRQ_LINE, AudDrv_IRQ_handler,
			IRQF_TRIGGER_LOW /*IRQF_TRIGGER_FALLING */,
			"Afe_ISR_Handle", dev);
#endif
	return ret;
}

static int irqcount;

irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id)
{
	/* unsigned long flags; */
	kal_uint32 u4RegValue;
	kal_uint32 u4tmpValue;
	kal_uint32 u4tmpValue1;
	kal_uint32 u4tmpValue2;

#if 0
	AudDrv_Clk_On();
#endif
	u4RegValue = Afe_Get_Reg(AFE_IRQ_MCU_STATUS);
	u4RegValue &= 0xff;
	u4tmpValue = Afe_Get_Reg(AFE_IRQ_MCU_EN);
	u4tmpValue &= 0xff;
	u4tmpValue1 = Afe_Get_Reg(AFE_IRQ_CNT5);
	u4tmpValue1 &= 0x0003ffff;
	u4tmpValue2 = Afe_Get_Reg(AFE_IRQ_DEBUG);
	u4tmpValue2 &= 0x0003ffff;


	if (u4RegValue == 0) {
		pr_debug("%s u4RegValue == 0 irqcount = %d\n",
			 __func__, irqcount);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 6, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 2, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 3, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 4, 0xff);
		Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 5, 0xff);
		irqcount++;
		if (irqcount > AudioInterruptLimiter) {
			SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
			SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
			irqcount = 0;
		}
		goto AudDrv_IRQ_handler_exit;
	}
	if (u4RegValue & INTERRUPT_IRQ1_MCU) {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DL1]->mState == true)
			Auddrv_DL1_Interrupt_Handler();
	}
	if (u4RegValue & INTERRUPT_IRQ2_MCU) {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL]->mState == true)
			Auddrv_UL1_Interrupt_Handler();
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_AWB]->mState == true)
			Auddrv_AWB_Interrupt_Handler();
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DAI]->mState == true)
			Auddrv_DAI_Interrupt_Handler();
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->
			mState == true)
			Auddrv_UL2_Interrupt_Handler();
	}
	if (u4RegValue & INTERRUPT_IRQ7_MCU) {
		if ((mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DL2]->mState == true)
		    && (mOffloadSWMode == true))
			Auddrv_DL2_Interrupt_Handler();
	}
	if (u4RegValue & INTERRUPT_IRQ5_MCU) {
		if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_HDMI]->mState == true)
			Auddrv_HDMI_Interrupt_Handler();
	}
	/* clear irq */
	Afe_Set_Reg(AFE_IRQ_MCU_CLR, u4RegValue, 0xff);
AudDrv_IRQ_handler_exit:
#if 0
	AudDrv_Clk_Off();
#endif
	return IRQ_HANDLED;
}

uint32 GetApllbySampleRate(uint32 SampleRate)
{
	if (SampleRate == 176400 || SampleRate == 88200 || SampleRate == 44100
	    || SampleRate == 22050 || SampleRate == 11025)
		return Soc_Aud_APLL1;
	else
		return Soc_Aud_APLL2;
}

void SetckSel(uint32 I2snum, uint32 SampleRate)
{
	uint32 ApllSource = 0;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL2)
	ApllSource = 1;

	switch (I2snum) {
	case Soc_Aud_I2S0:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 8, 1 << 8);
		break;
	case Soc_Aud_I2S1:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 9, 1 << 9);
		break;
	case Soc_Aud_I2S2:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 10, 1 << 10);
		break;
	case Soc_Aud_I2S3:
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 11, 1 << 11);
		break;
	case Soc_Aud_HDMI_BCK:	/* hdmi i2s bck */
		SetClkCfg(AUDIO_CLK_AUD_DIV0, ApllSource << 0, 1 << 0);
		break;
	case Soc_Aud_SPDIF:
	case Soc_Aud_HDMI_MCK:	/* hdmi i2s mclk or spdif bck */
		SetClkCfg(AUDIO_CLK_AUD_DIV1, ApllSource << 0, 1 << 0);
		break;
	case Soc_Aud_SPDIF2:	/* hdmi i2s mclk or spdif bck */
		SetClkCfg(AUDIO_CLK_AUD_DIV2, ApllSource << 0, 1 << 0);
		break;
	}
	pr_debug("%s I2snum = %d ApllSource = %d\n",
		 __func__, I2snum, ApllSource);
}

static int APLLUsage = Soc_Aud_APLL_NOUSE;
static int APLLCounter;
void EnableALLbySampleRate(uint32 SampleRate)
{
	pr_debug("%s APLLUsage = %d APLLCounter = %d SampleRate = %d\n",
		__func__, APLLUsage,
		 APLLCounter, SampleRate);
	if ((GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1) &&
		(APLLUsage == Soc_Aud_APLL_NOUSE)) {
		/* enable APLL1 */
		APLLUsage = Soc_Aud_APLL1;
		APLLCounter++;
		if (APLLCounter == true) {
			AudDrv_ANA_Clk_On();
			AudDrv_Clk_On();
			EnableApll1(true);
			AudDrv_APLL1Tuner_Clk_On();
		}
	} else if ((GetApllbySampleRate(SampleRate) == Soc_Aud_APLL2)
		   && (APLLUsage == Soc_Aud_APLL_NOUSE)) {
		/* enable APLL2 */
		APLLUsage = Soc_Aud_APLL2;
		APLLCounter++;
		if (APLLCounter == true) {
			AudDrv_ANA_Clk_On();
			AudDrv_Clk_On();
			EnableApll2(true);
			AudDrv_APLL2Tuner_Clk_On();
		}
	}
}

void DisableALLbySampleRate(uint32 SampleRate)
{
	pr_debug("%s APLLUsage = %d APLLCounter = %d SampleRate = %d\n",
		__func__, APLLUsage,
		 APLLCounter, SampleRate);
	if ((GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1) &&
		(APLLUsage == Soc_Aud_APLL1)) {
		/* disable APLL1 */
		APLLCounter--;
		if (APLLCounter == 0) {
			/* disable APLL1 */
			APLLUsage = Soc_Aud_APLL_NOUSE;
			AudDrv_APLL1Tuner_Clk_Off();
			EnableApll1(false);
			AudDrv_Clk_Off();
			AudDrv_ANA_Clk_Off();
		}
	} else if ((GetApllbySampleRate(SampleRate) == Soc_Aud_APLL2)
		   && (APLLUsage == Soc_Aud_APLL2)) {
		APLLCounter--;
		if (APLLCounter == 0) {
			/* disable APLL2 */
			APLLUsage = Soc_Aud_APLL_NOUSE;
			AudDrv_APLL2Tuner_Clk_Off();
			EnableApll2(false);
			AudDrv_Clk_Off();
			AudDrv_ANA_Clk_Off();
		}
	}
}

void EnableApll(uint32 SampleRate, bool bEnable)
{
	pr_debug("%s SampleRate = %d\n", __func__, SampleRate);
	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		EnableApll1(bEnable);
	else if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL2)
		EnableApll2(bEnable);
}

void EnableApllTuner(uint32 SampleRate, bool bEnable)
{
	pr_debug("%s SampleRate = %d\n", __func__, SampleRate);
	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1) {
		if (bEnable)
			AudDrv_APLL1Tuner_Clk_On();
		else
			AudDrv_APLL1Tuner_Clk_Off();
	} else if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL2) {
		if (bEnable)
			AudDrv_APLL2Tuner_Clk_On();
		else
			AudDrv_APLL2Tuner_Clk_Off();
	}
}

uint32 SetCLkMclk(uint32 I2snum, uint32 SampleRate)
{
	uint32 I2S_APll = 0;
	uint32 I2s_ck_div = 0;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		I2S_APll = 22579200 * 4;
	else
		I2S_APll = 24576000 * 4;
	SetckSel(I2snum, SampleRate);	/* set I2Sx mck source */
	switch (I2snum) {
	case Soc_Aud_I2S0:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div, 0x0000007f);
		break;
	case Soc_Aud_I2S1:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 8, 0x00007f00);
		break;
	case Soc_Aud_I2S2:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 16, 0x007f0000);
		break;
	case Soc_Aud_I2S3:
		I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 24, 0x7f000000);
		break;
	case Soc_Aud_HDMI_MCK:
	case Soc_Aud_SPDIF:
		I2s_ck_div = (I2S_APll / 128 / SampleRate) - 1;
		SetClkCfg(AUDIO_CLK_AUD_DIV1, I2s_ck_div << 8, 0x00007f00);
		break;
	case Soc_Aud_SPDIF2:
		I2s_ck_div = (I2S_APll / 128 / SampleRate) - 1;
		SetClkCfg(AUDIO_CLK_AUD_DIV2, I2s_ck_div << 8, 0x00007f00);
		break;
	}
	pr_debug("%s I2snum = %d I2s_ck_div = %d I2S_APll = %d\n",
		__func__, I2snum, I2s_ck_div, I2S_APll);
	return I2s_ck_div;
}

void SetCLkBclk(uint32 MckDiv, uint32 SampleRate,
	 uint32 Channels, uint32 Wlength)
{
	/* BCK set only required in 6595 TDM function div4/div5 */
	uint32 I2S_APll = 0;
	uint32 I2S_Bclk = 0;
	uint32 I2s_Bck_div = 0;

	pr_debug("%s MckDiv %d Rate %d  ch %d Wlength %d\n",
		__func__, MckDiv, SampleRate, Channels, Wlength);
	MckDiv++;
	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		I2S_APll = 22579200 * 4;
	else
		I2S_APll = 24576000 * 4;
	I2S_Bclk = SampleRate * Channels * (Wlength + 1) * 16;
	I2s_Bck_div = (I2S_APll / MckDiv) / I2S_Bclk;
	pr_debug("%s I2S_APll = %dv I2S_Bclk = %d  I2s_Bck_div = %d\n",
	 __func__, I2S_APll, I2S_Bclk, I2s_Bck_div);
	I2s_Bck_div--;

	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, I2s_Bck_div << 24, 0x07000000);
	else
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, I2s_Bck_div << 28, 0x70000000);

}

uint32 SetCLkHdmiBclk(uint32 MckDiv, uint32 SampleRate,
	uint32 Channels, uint32 bitDepth)
{
	uint32 I2S_APll = 0;
	uint32 I2S_Bclk = 0;
	uint32 I2s_Bck_div = 0;

	pr_debug("%s MckDiv %d rate %d ch %d depth %d\n",
		__func__, MckDiv, SampleRate, Channels, bitDepth);
	MckDiv++;
	if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
		I2S_APll = 22579200 * 4;
	else
		I2S_APll = 24576000 * 4;
	I2S_Bclk = SampleRate * Channels * bitDepth;
	I2s_Bck_div = I2S_APll / I2S_Bclk - 1;
	SetckSel(Soc_Aud_HDMI_BCK, SampleRate);
	SetClkCfg(AUDIO_CLK_AUD_DIV0, I2s_Bck_div << 8, 0x00007f00);
	return I2s_Bck_div;
}

void EnableI2SDivPower(uint32 Diveder_name, bool bEnable)
{
	if (bEnable) {
		/* AUDIO_APLL1_DIV0 */
		SetClkCfg(AUDIO_CLK_AUDDIV_0, 0 << Diveder_name,
			 1 << Diveder_name);
	} else
		SetClkCfg(AUDIO_CLK_AUDDIV_0, 1 << Diveder_name,
			1 << Diveder_name);
}

void EnableHDMIDivPower(uint32 Diveder_name, bool bEnable)
{
	if (bEnable)
		SetClkCfg(AUDIO_CLK_AUD_DIV0, 0 << Diveder_name,
			 1 << Diveder_name);
	else
		SetClkCfg(AUDIO_CLK_AUD_DIV0, 1 << Diveder_name,
			 1 << Diveder_name);
}

void EnableSpdifDivPower(uint32 Diveder_name, bool bEnable)
{
	if (bEnable)
		SetClkCfg(AUDIO_CLK_AUD_DIV1, 0 << Diveder_name,
			 1 << Diveder_name);
	else
		SetClkCfg(AUDIO_CLK_AUD_DIV1, 1 << Diveder_name,
			 1 << Diveder_name);
}

void EnableSpdif2DivPower(uint32 Diveder_name, bool bEnable)
{
	if (bEnable)
		SetClkCfg(AUDIO_CLK_AUD_DIV2, 0 << Diveder_name,
			 1 << Diveder_name);
	else
		SetClkCfg(AUDIO_CLK_AUD_DIV2, 1 << Diveder_name,
			 1 << Diveder_name);
}

void EnableApll1(bool bEnable)
{
	pr_debug("%s bEnable = %d", __func__, bEnable);
	if (bEnable) {
		if (Aud_APLL_DIV_APLL1_cntr == 0) {
#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* set hf_faud_1_ck from apll1_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x1 << 16, 0x1 << 16);
#endif
		/* apll1_div0_pdn power down */
		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x1, 0x1);

		Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x03000000, 0x07000000);

			AudDrv_APLL22M_Clk_On();
			/* apll1_div0_pdn power up */
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0x1);

#if 0
			/* apll2_div0_pdn power down */
			SetClkCfg(AUDIO_CLK_AUDDIV_0, 0x0, 0x2);
#endif

#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* turn on  hf_faud_2_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x0 << 23, 0x1 << 23);
#endif
		}
		Aud_APLL_DIV_APLL1_cntr++;
	} else {
		Aud_APLL_DIV_APLL1_cntr--;
		if (Aud_APLL_DIV_APLL1_cntr == 0) {
			AudDrv_APLL22M_Clk_Off();
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x1, 0x1);
#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* turn off hf_faud_2_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x1 << 23, 0x1 << 23);
#endif
		}
	}
}

void EnableApll2(bool bEnable)
{
	pr_debug("%s bEnable = %d\n", __func__, bEnable);
	if (bEnable) {
		if (Aud_APLL_DIV_APLL2_cntr == 0) {
#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* set hf_faud_2_ck from apll2_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x1 << 24, 0x1 << 24);
#endif
			/* apll2_div0_pdn power down */
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x2, 0x2);
			/* apll2_ck_div0, 98.3030/4 = 24.576M */
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x3 << 28, 0x7 << 28);
			AudDrv_APLL22M_Clk_On();
			AudDrv_APLL24M_Clk_On();
			/* apll2_div0_pdn power up */
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0x2);
#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* turn on hf_faud_2_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x0 << 31, 0x1 << 31);
#endif
		}
		Aud_APLL_DIV_APLL2_cntr++;
	} else {
		Aud_APLL_DIV_APLL2_cntr--;
		if (Aud_APLL_DIV_APLL2_cntr == 0) {
			AudDrv_APLL24M_Clk_Off();
			AudDrv_APLL22M_Clk_Off();
			/* apll2_div0_pdn power down */
			Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x2, 0x2);
#ifndef COMMON_CLOCK_FRAMEWORK_API
			/* turn off hf_faud_2_ck */
			SetClkCfg(AUDIO_CLK_CFG_6, 0x1 << 31, 0x1 << 31);
#endif
		}
	}
}

void SetHdmiClkOn(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_control_lock, flags);
	pr_debug("%s aud_hdmi_clk_cntr:%d\n", __func__,
		 aud_hdmi_clk_cntr);
	if (aud_hdmi_clk_cntr == 0)
		Afe_Set_Reg(AUDIO_TOP_CON0, 0 << 20, 1 << 20);
	aud_hdmi_clk_cntr++;
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

void SetHdmiClkOff(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_control_lock, flags);
	pr_debug("%s aud_hdmi_clk_cntr:%d\n",
		 __func__, aud_hdmi_clk_cntr);
	aud_hdmi_clk_cntr--;
	if (aud_hdmi_clk_cntr == 0)
		Afe_Set_Reg(AUDIO_TOP_CON0, 1 << 20, 1 << 20);
	else if (aud_hdmi_clk_cntr < 0) {
		pr_err("%s aud_hdmi_clk_cntr:%d<0\n",
			 __func__, aud_hdmi_clk_cntr);
		AUDIO_ASSERT(true);
		aud_hdmi_clk_cntr = 0;
	}
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

static bool CheckMemIfEnable(void)
{
	int i = 0;

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++) {
		if ((mAudioMEMIF[i]->mState) == true) {
			/* pr_debug("CheckMemIfEnable == true\n"); */
			return true;
		}
	}
	/* pr_debug("CheckMemIfEnable == false\n"); */
	return false;
}


/* record VOW status for AFE GPIO control */
void SetVOWStatus(bool bEnable)
{
	unsigned long flags;

	if (mVOWStatus != bEnable) {
		spin_lock_irqsave(&afe_control_lock, flags);
		mVOWStatus = bEnable;
		pr_debug("SetVOWStatus, mVOWStatus= %d\n", mVOWStatus);
		spin_unlock_irqrestore(&afe_control_lock, flags);
	}
}

/*****************************************************************************
 * FUNCTION
 *  Auddrv_Reg_map
 *
 * DESCRIPTION
 * Auddrv_Reg_map
 *
 *****************************************************************************
 */
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY
static unsigned int pin_audclk, pin_audmiso, pin_audmosi;
static unsigned int pin_mode_audclk, pin_mode_audmosi, pin_mode_audmiso;
#endif
#endif

void EnableAfe(bool bEnable)
{
	unsigned long flags;
	bool MemEnable = false;
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY

	int ret;

	ret = GetGPIO_Info(1, &pin_audclk, &pin_mode_audclk);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL1!!!\n");
		return;
	}
	ret = GetGPIO_Info(2, &pin_audmiso, &pin_mode_audmiso);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL2!!!\n");
		return;
	}
	ret = GetGPIO_Info(3, &pin_audmosi, &pin_mode_audmosi);
	if (ret < 0) {
		pr_err("EnableAfe GetGPIO_Info FAIL3!!!\n");
		return;
	}
#endif
#endif

	spin_lock_irqsave(&afe_control_lock, flags);
	MemEnable = CheckMemIfEnable();

	if (false == bEnable && false == MemEnable) {
		Afe_Set_Reg(AFE_DAC_CON0, 0x0, 0x0);
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY

		mt_set_gpio_mode(pin_audclk, GPIO_MODE_00);
		if (mVOWStatus != true)
			mt_set_gpio_mode(pin_audmiso, GPIO_MODE_00);

		mt_set_gpio_mode(pin_audmosi, GPIO_MODE_00);
#else
		AudDrv_GPIO_PMIC_Select(bEnable);
#endif
#else
		mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_00);
		if (mVOWStatus != true)
			mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_00);

		mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_00);
#endif
#endif

	} else if (true == bEnable && true == MemEnable) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY

		mt_set_gpio_mode(pin_audclk, GPIO_MODE_01);
		if (mVOWStatus != true)
			mt_set_gpio_mode(pin_audmiso, GPIO_MODE_01);

		mt_set_gpio_mode(pin_audmosi, GPIO_MODE_01);
#else
		AudDrv_GPIO_PMIC_Select(bEnable);
#endif
#else
		mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_01);
		if (mVOWStatus != true)
			mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_01);
		mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_01);
#endif
#endif
		Afe_Set_Reg(AFE_DAC_CON0, 0x1, 0x1);
	}
	spin_unlock_irqrestore(&afe_control_lock, flags);
}

uint32 SampleRateTransform(uint32 SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_8K;
	case 11025:
		return Soc_Aud_I2S_SAMPLERATE_I2S_11K;
	case 12000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_12K;
	case 16000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_16K;
	case 22050:
		return Soc_Aud_I2S_SAMPLERATE_I2S_22K;
	case 24000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_24K;
	case 32000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_32K;
	case 44100:
		return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
	case 48000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_48K;
	case 88000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_88K;
	case 96000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_96K;
	case 174000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_174K;
	case 192000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_192K;
	case 260000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_260K;
	default:
		break;
	}
	return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
}

bool SetSampleRate(uint32 Aud_block, uint32 SampleRate)
{
	SampleRate = SampleRateTransform(SampleRate);
	switch (Aud_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate, 0x0000000f);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 4, 0x000000f0);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 8, 0x00000f00);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 12, 0x0000f000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 16, 0x000f0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				Afe_Set_Reg(AFE_DAC_CON0, 0 << 24, 3 << 24);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				Afe_Set_Reg(AFE_DAC_CON0, 1 << 24, 3 << 24);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
				Afe_Set_Reg(AFE_DAC_CON0, 2 << 24, 3 << 24);
			else
				return false;
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				Afe_Set_Reg(AFE_DAC_CON1, 0 << 30, 3 << 30);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				Afe_Set_Reg(AFE_DAC_CON1, 1 << 30, 3 << 30);
			else
				return false;
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:{
			Afe_Set_Reg(AFE_DAC_CON0, SampleRate << 20, 0x00f00000);
			break;
		}
		return true;
	}
	return false;
}

bool SetChannels(uint32 Memory_Interface, uint32 channel)
{
	const bool bMono = (channel == 1) ? true : false;

	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 21, 1 << 21);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 24, 1 << 24);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_DAC_CON1, bMono << 27, 1 << 27);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:{
			Afe_Set_Reg(AFE_DAC_CON0, bMono << 10, 1 << 10);
			break;
		}
	default:
		pr_warn("SetChannels wrong Mem_Interface = %d\n",
			Memory_Interface);
		return false;
	}
	return true;
}


bool Set2ndI2SOutAttribute(uint32_t sampleRate)
{
	pr_debug("+%s(), sampleRate = %d\n", __func__, sampleRate);
	m2ndI2Sout->mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	m2ndI2Sout->mI2S_SLAVE = Soc_Aud_I2S_SRC_MASTER_MODE;
	m2ndI2Sout->mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	m2ndI2Sout->mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	m2ndI2Sout->mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	m2ndI2Sout->mI2S_HDEN = Soc_Aud_NORMAL_CLOCK;
	m2ndI2Sout->mI2S_SAMPLERATE = sampleRate;
	Set2ndI2SOut(m2ndI2Sout);
	return true;
}

bool Set2ndI2SOut(struct AudioDigtalI2S *DigtalI2S)
{
	uint32 u32AudioI2S = 0;

	memcpy((void *)m2ndI2Sout, (void *)DigtalI2S,
	sizeof(struct AudioDigtalI2S));
	u32AudioI2S = SampleRateTransform(m2ndI2Sout->mI2S_SAMPLERATE) << 8;
	u32AudioI2S |= m2ndI2Sout->mLR_SWAP << 31;
	u32AudioI2S |= m2ndI2Sout->mI2S_HDEN << 12;
	u32AudioI2S |= m2ndI2Sout->mINV_LRCK << 5;
	u32AudioI2S |= m2ndI2Sout->mI2S_FMT << 3;
	u32AudioI2S |= m2ndI2Sout->mI2S_WLEN << 1;
	Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S, AFE_MASK_ALL);
	return true;
}

bool Set2ndI2SOutEnable(bool benable)
{
	if (benable)
		Afe_Set_Reg(AFE_I2S_CON3, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);
	return true;
}

bool SetDaiBt(struct AudioDigitalDAIBT *mAudioDaiBt)
{
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mUSE_MRGIF_INPUT =
		mAudioDaiBt->mUSE_MRGIF_INPUT;
	AudioDaiBt->mDAI_BT_MODE = mAudioDaiBt->mDAI_BT_MODE;
	AudioDaiBt->mDAI_DEL = mAudioDaiBt->mDAI_DEL;
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mDATA_RDY = mAudioDaiBt->mDATA_RDY;
	AudioDaiBt->mBT_SYNC = mAudioDaiBt->mBT_SYNC;
	return true;
}

bool SetDaiBtEnable(bool bEanble)
{
	pr_debug("%s bEanble = %d\n", __func__, bEanble);
	if (bEanble == true) {	/* turn on dai bt */
		Afe_Set_Reg(AFE_DAIBT_CON0,
			AudioDaiBt->mDAI_BT_MODE << 9,
			0x1 << 9);
		if (mAudioMrg->MrgIf_En == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
		} else {	/* turn on merge and daiBT */
			/* set Mrg_I2S Samping Rate */
			Afe_Set_Reg(AFE_MRGIF_CON,
				mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);

			/* set Mrg_I2S enable */
			Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);

			/* Turn on Merge Interface */
			Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);

			udelay(100);

			/* use merge */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);

			/* data ready */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);

			/* Turn on DAIBT */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
		}
		AudioDaiBt->mBT_ON = true;
		AudioDaiBt->mDAIBT_ON = true;
		mAudioMrg->MrgIf_En = true;
	} else {
		if (mAudioMrg->Mergeif_I2S_Enable == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);
		} else {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);
			udelay(100);
			Afe_Set_Reg(AFE_MRGIF_CON, 0 << 16, 1 << 16);
			Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);
			mAudioMrg->MrgIf_En = false;
		}
		AudioDaiBt->mBT_ON = false;
		AudioDaiBt->mDAIBT_ON = false;
	}
	return true;
}

bool GetMrgI2SEnable(void)
{
	return mAudioMEMIF[Soc_Aud_Digital_Block_MRG_I2S_OUT]->mState;
}

bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate)
{
	pr_debug("%s bEnable = %d\n", __func__, bEnable);
	if (bEnable == true) {
		/* To enable MrgI2S */
		if (mAudioMrg->MrgIf_En == true) {
			if (mAudioMrg->Mrg_I2S_SampleRate !=
				 SampleRateTransform(sampleRate)) {

				Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16);

				if (AudioDaiBt->mDAIBT_ON == true) {
					/* Turn off DAIBT first */
					Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x1);
				}

				udelay(100);
				Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);
				udelay(100);
				Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);

				if (AudioDaiBt->mDAIBT_ON == true) {
					/* use merge */
					Afe_Set_Reg(AFE_DAIBT_CON0,
						AudioDaiBt->mDAI_BT_MODE << 9,
						0x1 << 9);
					Afe_Set_Reg(AFE_DAIBT_CON0,
						0x1 << 12, 0x1 << 12);

					/* data ready */
					Afe_Set_Reg(AFE_DAIBT_CON0,
						0x1 << 3, 0x1 << 3);

					/* Turn on DAIBT */
					Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
				}
				mAudioMrg->Mrg_I2S_SampleRate =
					SampleRateTransform(sampleRate);
				Afe_Set_Reg(AFE_MRGIF_CON,
					mAudioMrg->Mrg_I2S_SampleRate << 20,
					 0xF00000);
				Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
			}
		} else {
			mAudioMrg->Mrg_I2S_SampleRate =
				 SampleRateTransform(sampleRate);
			Afe_Set_Reg(AFE_MRGIF_CON,
				mAudioMrg->Mrg_I2S_SampleRate << 20,
				0xF00000);
			Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
			udelay(100);
			Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);
			udelay(100);
			if (AudioDaiBt->mDAIBT_ON == true) {
				/* use merge */
				Afe_Set_Reg(AFE_DAIBT_CON0,
					 AudioDaiBt->mDAI_BT_MODE << 9,
					  0x1 << 9);
				Afe_Set_Reg(AFE_DAIBT_CON0,
					 0x1 << 12, 0x1 << 12);

				Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
				Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
			}
		}
		mAudioMrg->MrgIf_En = true;
		mAudioMrg->Mergeif_I2S_Enable = true;
	} else {
		if (mAudioMrg->MrgIf_En == true) {
			Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16);
			if (AudioDaiBt->mDAIBT_ON == false) {
				udelay(100);
				Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);
				mAudioMrg->MrgIf_En = false;
			}
		}
		mAudioMrg->Mergeif_I2S_Enable = false;
	}
	return true;
}

bool Set2ndI2SAdcIn(struct AudioDigtalI2S *DigtalI2S)
{
	/* todo */
	return true;
}

bool SetI2SAdcIn(struct AudioDigtalI2S *DigtalI2S)
{
	uint32 Audio_I2S_Adc = 0;

	memcpy((void *)AudioAdcI2S, (void *)DigtalI2S,
		sizeof(struct AudioDigtalI2S));
	if (false == AudioAdcI2SStatus) {
		uint32 eSamplingRate =
		 SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE);
		uint32 dVoiceModeSelect = 0;

		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1);	/* Using Internal ADC */

		if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
			dVoiceModeSelect = 0;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
			dVoiceModeSelect = 1;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
			dVoiceModeSelect = 2;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_48K)
			dVoiceModeSelect = 3;

		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0,
			(dVoiceModeSelect << 19) |
			(dVoiceModeSelect << 17), 0x001E0000);
		/* up8x txif sat on */
		Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF);
		Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1,
			 ((dVoiceModeSelect < 3) ? 1 : 3) << 10,
			    0x00000C00);
	} else {
		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 1, 0x1);	/* Using External ADC */
		Audio_I2S_Adc |= (AudioAdcI2S->mLR_SWAP << 31);
		Audio_I2S_Adc |= (AudioAdcI2S->mBuffer_Update_word << 24);
		Audio_I2S_Adc |= (AudioAdcI2S->mINV_LRCK << 23);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit_test << 22);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit << 21);
		Audio_I2S_Adc |= (AudioAdcI2S->mloopback << 20);
		Audio_I2S_Adc |=
		 (SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE) << 8);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_FMT << 3);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_WLEN << 1);
		pr_debug("%s Audio_I2S_Adc = 0x%x\n", __func__, Audio_I2S_Adc);
		Afe_Set_Reg(AFE_I2S_CON2, Audio_I2S_Adc, MASK_ALL);
	}
	return true;
}

bool EnableSideGenHw(uint32 connection, bool direction, bool Enable)
{
	pr_debug("+%s(), connection = %d, direction = %d, Enable= %d\n",
		__func__, connection, direction, Enable);
	if (Enable && direction) {
		switch (connection) {
		case Soc_Aud_InterConnectionInput_I00:
		case Soc_Aud_InterConnectionInput_I01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x048C2762, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x146C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I03:
		case Soc_Aud_InterConnectionInput_I04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x24862862, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I05:
		case Soc_Aud_InterConnectionInput_I06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x348C28C2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I07:
		case Soc_Aud_InterConnectionInput_I08:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x446C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I09:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x546C2662, 0xffffffff);
		case Soc_Aud_InterConnectionInput_I10:
		case Soc_Aud_InterConnectionInput_I11:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x646C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I12:
		case Soc_Aud_InterConnectionInput_I13:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x746C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I14:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x846C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I15:
		case Soc_Aud_InterConnectionInput_I16:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x946C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I17:
		case Soc_Aud_InterConnectionInput_I18:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xa46C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I19:
		case Soc_Aud_InterConnectionInput_I20:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xb46C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I21:
		case Soc_Aud_InterConnectionInput_I22:
			break;
			Afe_Set_Reg(AFE_SGEN_CON0, 0xc46C2662, 0xffffffff);
		default:
			break;
		}
	} else if (Enable) {
		switch (connection) {
		case Soc_Aud_InterConnectionOutput_O00:
		case Soc_Aud_InterConnectionOutput_O01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x0c7c27c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x1c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O03:
		case Soc_Aud_InterConnectionOutput_O04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x2c8c28c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O05:
		case Soc_Aud_InterConnectionOutput_O06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x3c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O07:
		case Soc_Aud_InterConnectionOutput_O08:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x4c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O09:
		case Soc_Aud_InterConnectionOutput_O10:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x5c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O11:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x6c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O12:
			if (Soc_Aud_I2S_SAMPLERATE_I2S_8K ==
				mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]
					->mSampleRate)
				Afe_Set_Reg(AFE_SGEN_CON0,
					0x7c0e80e8, 0xffffffff);
			else if (Soc_Aud_I2S_SAMPLERATE_I2S_16K ==
				mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]
					->mSampleRate)
				Afe_Set_Reg(AFE_SGEN_CON0, 0x7c0f00f0,
					 0xffffffff);
			else {
				Afe_Set_Reg(AFE_SGEN_CON0, 0x7c6c26c2,
					 0xffffffff);
			}
			break;
		case Soc_Aud_InterConnectionOutput_O13:
		case Soc_Aud_InterConnectionOutput_O14:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x8c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O15:
		case Soc_Aud_InterConnectionOutput_O16:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x9c6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O17:
		case Soc_Aud_InterConnectionOutput_O18:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xac6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O19:
		case Soc_Aud_InterConnectionOutput_O20:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xbc6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O21:
		case Soc_Aud_InterConnectionOutput_O22:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xcc6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O23:
		case Soc_Aud_InterConnectionOutput_O24:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xdc6c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O25:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xec6c26c2, 0xffffffff);
		default:
			break;
		}
	} else {
		Afe_Set_Reg(AFE_SGEN_CON0, 0xf0000000, 0xffffffff);
	}
	return true;
}

bool SetSideGenSampleRate(uint32 SampleRate)
{
	uint32 sine_mode_ch1 = 0;
	uint32 sine_mode_ch2 = 0;

	pr_debug("+%s(), SampleRate = %d\n", __func__, SampleRate);
	sine_mode_ch1 = SampleRateTransform(SampleRate) << 8;
	sine_mode_ch2 = SampleRateTransform(SampleRate) << 20;
	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch1, 0x00000f00);
	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch2, 0x00f00000);
	return true;
}

bool Set2ndI2SAdcEnable(bool bEnable)
{
	/* todo? */
	return true;
}

bool SetI2SAdcEnable(bool bEnable)
{
	Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, bEnable ? 1 : 0, 0x01);
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState = bEnable;
	if (bEnable == true)
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
	else if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState
		 == false &&
		 mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState ==
			 false &&
		 mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC_2]->mState
			 == false)
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
	return true;
}

bool Set2ndI2SEnable(bool bEnable)
{
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	return true;
}

bool CleanPreDistortion(void)
{
	pr_debug("%s\n", __func__);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
	return true;
}

bool SetDLSrc2(uint32 SampleRate)
{
	uint32 AfeAddaDLSrc2Con0, AfeAddaDLSrc2Con1;

	if (SampleRate == 8000)
		AfeAddaDLSrc2Con0 = 0;
	else if (SampleRate == 11025)
		AfeAddaDLSrc2Con0 = 1;
	else if (SampleRate == 12000)
		AfeAddaDLSrc2Con0 = 2;
	else if (SampleRate == 16000)
		AfeAddaDLSrc2Con0 = 3;
	else if (SampleRate == 22050)
		AfeAddaDLSrc2Con0 = 4;
	else if (SampleRate == 24000)
		AfeAddaDLSrc2Con0 = 5;
	else if (SampleRate == 32000)
		AfeAddaDLSrc2Con0 = 6;
	else if (SampleRate == 44100)
		AfeAddaDLSrc2Con0 = 7;
	else if (SampleRate == 48000)
		AfeAddaDLSrc2Con0 = 8;
	else
		AfeAddaDLSrc2Con0 = 7;	/* Default 44100 */

	if (AfeAddaDLSrc2Con0 == 0 || AfeAddaDLSrc2Con0 == 3) {
		/* 8k or 16k voice mode */
		AfeAddaDLSrc2Con0 =
			(AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) |
				 (0x03 << 11) | (0x01 << 5);
	} else
		AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) |
			 (0x03 << 24) | (0x03 << 11);

	/* SA suggest apply -0.3db to audio/speech path */
	/* for voice mode degrade 0.3db */
	AfeAddaDLSrc2Con0 = AfeAddaDLSrc2Con0 | (0x01 << 1);
	AfeAddaDLSrc2Con1 = 0xf74f0000;
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, AfeAddaDLSrc2Con0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, AfeAddaDLSrc2Con1, MASK_ALL);
	return true;
}

bool SetI2SDacOut(uint32 SampleRate, bool lowjitter, bool I2SWLen)
{
	uint32 Audio_I2S_Dac = 0;

	pr_debug("SetI2SDacOut SampleRate %d, lowjitter %d, I2SWLen %d\n",
		SampleRate, lowjitter, I2SWLen);
	CleanPreDistortion();
	SetDLSrc2(SampleRate);
	Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	Audio_I2S_Dac |= (SampleRateTransform(SampleRate) << 8);
	Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	Audio_I2S_Dac |= (I2SWLen << 1);
	Audio_I2S_Dac |= (lowjitter << 12);	/* low gitter mode */
	Afe_Set_Reg(AFE_I2S_CON1, Audio_I2S_Dac, MASK_ALL);
	return true;
}

bool SetHwDigitalGainMode(uint32 GainType,
	 uint32 SampleRate, uint32 SamplePerStep)
{
	uint32 value = 0;

	pr_debug("SetHwDigitalGainMode GainType = %d, SampleRate = %d, SamplePerStep= %d\n",
		 GainType, SampleRate, SamplePerStep);

	value = (SamplePerStep << 8) | (SampleRateTransform(SampleRate) << 4);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		return false;
	}
	return true;
}

bool SetHwDigitalGainEnable(int GainType, bool Enable)
{
	pr_debug("+%s(), GainType = %d, Enable = %d\n",
		 __func__, GainType, Enable);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			Afe_Set_Reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);
		}
		Afe_Set_Reg(AFE_GAIN1_CON0, Enable, 0x1);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			Afe_Set_Reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);
		}
		Afe_Set_Reg(AFE_GAIN2_CON0, Enable, 0x1);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetHwDigitalGain(uint32 Gain, int GainType)
{
	pr_debug("+%s(), Gain = 0x%x, gain type = %d\n",
		 __func__, Gain, GainType);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON1, Gain, 0xffffffff);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON1, Gain, 0xffffffff);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetModemPcmConfig(int modem_index,
	struct AudioDigitalPCM p_modem_pcm_attribute)
{
	uint32 reg_pcm2_intf_con = 0;
	uint32 reg_pcm_intf_con1 = 0;

	pr_debug("+%s()\n", __func__);
	if (modem_index == MODEM_1) {
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mTxLchRepeatSel & 0x1) << 13;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mVbt16kModeSel & 0x1) << 12;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mSingelMicSel & 0x1) << 7;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mAsyncFifoSel & 0x1) << 6;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mPcmWordLength & 0x1) << 5;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x3) << 3;
		reg_pcm2_intf_con |=
			 (p_modem_pcm_attribute.mPcmFormat & 0x3) << 1;
		pr_debug("%s(), PCM2_INTF_CON(0x%lx) = 0x%x\n",
			 __func__, PCM2_INTF_CON,
			 reg_pcm2_intf_con);
		Afe_Set_Reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
		if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			 Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x00098580, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x0004c2c0, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x0004c2c0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x00026160, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
			Afe_Set_Reg(AFE_ASRC2_CON1, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON4, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC2_CON7, 0x000130b0, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON1, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON4, 0x00026160, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC3_CON7, 0x000130b0, 0xffffffff);
		}
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) */
		if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00065900, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00065900, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x00032C80, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON1, 0x00065900, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON4, 0x00065900, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON7, 0x00032C80, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00032C80, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00032C80, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x00019640, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON1, 0x00032C80, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON4, 0x00032C80, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON7, 0x00019640, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00019640, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00019640, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x0000CB20, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON1, 0x00019640, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON4, 0x00019640, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			Afe_Set_Reg(AFE_ASRC4_CON7, 0x0000CB20, 0xffffffff);
		}
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mBclkOutInv & 0x01) << 22;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mTxLchRepeatSel & 0x01) << 19;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mVbt16kModeSel & 0x01) << 18;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mExtModemSel & 0x01) << 17;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mExtendBckSyncLength & 0x1F)
				<< 9;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01)
				<< 8;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mSingelMicSel & 0x01) << 7;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mAsyncFifoSel & 0x01) << 6;
		reg_pcm_intf_con1 |=
		 (p_modem_pcm_attribute.mSlaveModeSel & 0x01) << 5;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mPcmModeWidebandSel & 0x03) << 3;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mPcmFormat & 0x03) << 1;
		pr_debug("%s(), PCM_INTF_CON1(0x%lx) = 0x%x\n",
			 __func__, PCM_INTF_CON,
			 reg_pcm_intf_con1);
		Afe_Set_Reg(PCM_INTF_CON, reg_pcm_intf_con1, MASK_ALL);
	}
	return true;
}

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on)
{
	uint32 dNeedDisableASM = 0, mPcm1AsyncFifo;

	pr_debug("+%s(), modem_index = %d, modem_pcm_on = %d\n", __func__,
		 modem_index, modem_pcm_on);
	if (modem_index == MODEM_1) {
		Afe_Set_Reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_1_O]->mState =
			 modem_pcm_on;
	} else if (modem_index == MODEM_2 ||
		 modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) */
		if (modem_pcm_on == true) {
			Afe_Set_Reg(PCM_INTF_CON2,
				 (modem_index - 1) << 8, 0x100);
			mPcm1AsyncFifo = (Afe_Get_Reg(PCM_INTF_CON)
				 & 0x0040) >> 6;
			if (mPcm1AsyncFifo == 0) {
				Afe_Set_Reg(AFE_ASRC_CON0,
					 0x86083031, MASK_ALL);
				Afe_Set_Reg(AFE_ASRC4_CON0,
					 0x06003031, MASK_ALL);
			}
			Afe_Set_Reg(PCM_INTF_CON, 0x1, 0x1);
		} else if (modem_pcm_on == false) {
			Afe_Set_Reg(PCM_INTF_CON, 0x0, 0x1);
			Afe_Set_Reg(AFE_ASRC_CON6, 0x00000000, MASK_ALL);
			dNeedDisableASM = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x1) ?
				 1 : 0;
			Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 4 | 1 << 5 |
				 dNeedDisableASM));
			Afe_Set_Reg(AFE_ASRC_CON0, 0x0, 0x1);
			Afe_Set_Reg(AFE_ASRC4_CON6, 0x00000000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC4_CON0, 0, (1 << 4 | 1 << 5));
			Afe_Set_Reg(AFE_ASRC4_CON0, 0x0, 0x1);
		}
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_2_O]->mState
			 = modem_pcm_on;
	} else {
		pr_warn("%s(), no such modem_index: %d!!\n",
			 __func__, modem_index);
		return false;
	}
	return true;
}


bool EnableSideToneFilter(bool stf_on)
{
	/* MD max support 16K sampling rate */
	const uint8_t kSideToneHalfTapNum =
		sizeof(kSideToneCoefficientTable16k) / sizeof(uint16_t);

	pr_debug("+%s(), stf_on = %d\n", __func__, stf_on);
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	if (stf_on == false) {
		/* bypass STF result & disable */
		const bool bypass_stf_on = true;
		uint32_t reg_value = (bypass_stf_on << 31) | (stf_on << 8);

		Afe_Set_Reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n",
			__func__, AFE_SIDETONE_CON1, reg_value);
		/* set side tone gain = 0 */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n",
			__func__, AFE_SIDETONE_GAIN, 0);
	} else {
		const bool bypass_stf_on = false;
		/* using STF result & enable & set half tap num */
		uint32_t write_reg_value =
		    (bypass_stf_on << 31) | (stf_on << 8) | kSideToneHalfTapNum;
		/* set side tone coefficient */
		/* enable read/write side tone coefficient */
		const bool enable_read_write = true;
		const bool read_write_sel = true;	/* for write case */
		const bool sel_ch2 = false;
		uint32_t read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
		size_t coef_addr = 0;

		pr_debug("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n",
			__func__, AFE_SIDETONE_GAIN, 0);
		/* set side tone gain */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		Afe_Set_Reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n",
			 __func__,
			 AFE_SIDETONE_CON1, write_reg_value);
		for (coef_addr = 0; coef_addr < kSideToneHalfTapNum;
			coef_addr++) {
			bool old_write_ready = (read_reg_value >> 29) & 0x1;
			bool new_write_ready = 0;
			int try_cnt = 0;

			write_reg_value = enable_read_write << 25 |
			    read_write_sel << 24 |
			    sel_ch2 << 23 |
			    coef_addr << 16 |
			    kSideToneCoefficientTable16k[coef_addr];
			Afe_Set_Reg(AFE_SIDETONE_CON0,
				write_reg_value, 0x39FFFFF);
			pr_debug("%s(), AFE_SIDETONE_CON0[0x%lx] = 0x%x\n",
				__func__, AFE_SIDETONE_CON0, write_reg_value);
			for (try_cnt = 0; try_cnt < 10; try_cnt++) {
				/* msleep(3); */
				/* usleep_range(3 * 1000, 20 * 1000); */
				read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
				new_write_ready = (read_reg_value >> 29) & 0x1;
				if (new_write_ready == old_write_ready)	{
					udelay(3);
					if (try_cnt == 10) {
						WARN_ON(new_write_ready
							!=
							old_write_ready);
						return false;
					}
				} else
					break;
			}
		}
	}
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	pr_debug("-%s(), stf_on = %d\n", __func__, stf_on);
	return true;
}


bool SetMemoryPathEnable(uint32 Aud_block, bool bEnable)
{
	pr_debug("%s Aud_block = %d bEnable = %d\n",
		 __func__, Aud_block, bEnable);
	if (Aud_block >= Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return false;
	/* set for counter */
	if (bEnable == true) {
		if (mAudioMEMIF[Aud_block]->mUserCount == 0)
			mAudioMEMIF[Aud_block]->mState = true;
		mAudioMEMIF[Aud_block]->mUserCount++;
	} else {
		mAudioMEMIF[Aud_block]->mUserCount--;
		if (mAudioMEMIF[Aud_block]->mUserCount < 0) {
			mAudioMEMIF[Aud_block]->mUserCount = 0;
			pr_warn("warning , user count <0\n");
		}
		if (mAudioMEMIF[Aud_block]->mUserCount == 0)
			mAudioMEMIF[Aud_block]->mState = false;
	}
	pr_warn("%s Aud_block = %d mUserCount = %d mState = %d\n",
		__func__, Aud_block,
		mAudioMEMIF[Aud_block]->mUserCount,
			 mAudioMEMIF[Aud_block]->mState);

	if (Aud_block > Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE)
		return true;
	if ((bEnable == true) && (mAudioMEMIF[Aud_block]->mUserCount == 1))
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1),
			 1 << (Aud_block + 1));
	else if ((bEnable == false) &&
		 (mAudioMEMIF[Aud_block]->mUserCount == 0))
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1),
			 1 << (Aud_block + 1));
	return true;
}

bool GetMemoryPathEnable(uint32 Aud_block)
{
	if (Aud_block < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return mAudioMEMIF[Aud_block]->mState;
	return false;
}

bool SetI2SDacEnable(bool bEnable)
{
	pr_debug("%s bEnable = %d\n", __func__, bEnable);
	if (bEnable) {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
		Afe_Set_Reg(FPGA_CFG1, 0, 0x10);
	} else {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
		if ((mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState
			== false)
			&&
			(mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState
			== false)) {
			Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
		}
		Afe_Set_Reg(FPGA_CFG1, 1 << 4, 0x10);
	}
	return true;
}

bool GetI2SDacEnable(void)
{
	return mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState;
}

bool checkUplinkMEMIfStatus(void)
{
	int i = 0;

	for (i = Soc_Aud_Digital_Block_MEM_VUL;
		i <= Soc_Aud_Digital_Block_MEM_VUL_DATA2; i++) {
		if (mAudioMEMIF[i]->mState == true)
			return true;
	}
	return false;
}

bool SetHDMIChannels(uint32 Channels)
{
	unsigned int register_value = 0;

	register_value |= (Channels << 4);
	Afe_Set_Reg(AFE_HDMI_OUT_CON0, register_value, 0x000000F0);
	return true;
}

bool SetHDMIEnable(bool bEnable)
{
	if (bEnable)
		Afe_Set_Reg(AFE_HDMI_OUT_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_HDMI_OUT_CON0, 0x0, 0x1);
	return true;
}

void SetHdmiPcmInterConnection(unsigned int connection_state,
	unsigned int channels)
{
	/* O30~O37: L/R/LS/RS/C/LFE/CH7/CH8 */
	switch (channels) {
	case 8:
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I36,
				  Soc_Aud_InterConnectionOutput_O36);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I37,
				  Soc_Aud_InterConnectionOutput_O37);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I34,
				  Soc_Aud_InterConnectionOutput_O34);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I35,
				  Soc_Aud_InterConnectionOutput_O35);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I32,
				  Soc_Aud_InterConnectionOutput_O32);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I33,
				  Soc_Aud_InterConnectionOutput_O33);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I30,
				  Soc_Aud_InterConnectionOutput_O30);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I31,
				  Soc_Aud_InterConnectionOutput_O31);
		break;
	case 6:
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I34,
				  Soc_Aud_InterConnectionOutput_O34);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I35,
				  Soc_Aud_InterConnectionOutput_O35);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I32,
				  Soc_Aud_InterConnectionOutput_O32);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I33,
				  Soc_Aud_InterConnectionOutput_O33);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I30,
				  Soc_Aud_InterConnectionOutput_O30);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I31,
				  Soc_Aud_InterConnectionOutput_O31);
		break;
	case 4:
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I32,
				  Soc_Aud_InterConnectionOutput_O32);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I33,
				  Soc_Aud_InterConnectionOutput_O33);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I30,
				  Soc_Aud_InterConnectionOutput_O30);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I31,
				  Soc_Aud_InterConnectionOutput_O31);
		break;
	case 2:
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I30,
				  Soc_Aud_InterConnectionOutput_O30);
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I31,
				  Soc_Aud_InterConnectionOutput_O31);
		break;
	case 1:
		SetHDMIConnection(connection_state,
			 Soc_Aud_InterConnectionInput_I30,
				  Soc_Aud_InterConnectionOutput_O30);
		break;
	default:
		pr_warn("%s unsupported channels %u\n", __func__, channels);
		break;
	}
}

bool SetHDMIConnection(uint32 ConnectionState, uint32 Input, uint32 Output)
{
	if (ConnectionState)
		Afe_Set_Reg(AFE_HDMI_CONN0, (Input << (3 * Input)),
			 (0x7 << (3 * Output)));
	else
		Afe_Set_Reg(AFE_HDMI_CONN0, 0x0, 0xFFFFFFFF);
	return true;
}

bool SetConnection(uint32 ConnectionState, uint32 Input, uint32 Output)
{
	return SetConnectionState(ConnectionState, Input, Output);
}

bool SetIrqEnable(uint32 Irqmode, bool bEnable)
{
	pr_warn("+%s(), Irqmode = %d, bEnable = %d\n", __func__,
		 Irqmode, bEnable);
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode),
				 (1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			if (checkUplinkMEMIfStatus() == false)
				Afe_Set_Reg(AFE_IRQ_MCU_CON,
					(bEnable << Irqmode),
					(1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_EN, (bEnable << Irqmode),
				 (1 << Irqmode));
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode),
				 (1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_EN, (bEnable << Irqmode),
				 (1 << Irqmode));
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << 12),
				 (1 << 12));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_EN, (bEnable << Irqmode),
				 (1 << Irqmode));
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << 14),
				 (1 << 14));
			break;
		}
	default:
		break;
	}
	return true;
}

bool SetIrqMcuSampleRate(uint32 Irqmode, uint32 SampleRate)
{
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (
			SampleRateTransform(SampleRate) << 4),
				    0x000000f0);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (
			SampleRateTransform(SampleRate) << 8),
				    0x00000f00);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (
			SampleRateTransform(SampleRate) << 16),
				    0x000f0000);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (
			SampleRateTransform(SampleRate) << 24),
				    0x0f000000);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool SetIrqMcuCounter(uint32 Irqmode, uint32 Counter)
{
	uint32 CurrentCount = 0;

	pr_debug(" %s Irqmode = %d Counter = %d\n", __func__,
		 Irqmode, Counter);
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CNT1, Counter, 0xffffffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			CurrentCount = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
			if (CurrentCount == 0) {
				Afe_Set_Reg(AFE_IRQ_MCU_CNT2,
					 Counter, 0xffffffff);
			} else if (Counter < CurrentCount) {
				pr_debug("update counter CurCount %d Counter %d\n",
					CurrentCount, Counter);
				Afe_Set_Reg(AFE_IRQ_MCU_CNT2,
					Counter, 0xffffffff);
			} else {
				pr_debug
				("error CurCount %d Counter %d\n",
				CurrentCount, Counter);
			}
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			Afe_Set_Reg(AFE_IRQ_MCU_CNT1,
				Counter << 20, 0xfff00000);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:{
			/* ox3BC [0~17] , ex 24bit , stereo, 48BCKs @CNT */
			Afe_Set_Reg(AFE_IRQ_CNT5, Counter, 0x0003ffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE:{
			/* ox3BC [0~17] , ex 24bit , stereo, 48BCKs @CNT */
			Afe_Set_Reg(AFE_IRQ_MCU_CNT7, Counter, 0xffffffff);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool SetMemDuplicateWrite(uint32 InterfaceType, int dupwrite)
{
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DAI:{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 29, 1 << 29);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 31, 1 << 31);
			break;
		}
	default:
		return false;
	}
	return true;
}


bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode)
{
	struct AudioDigtalI2S I2S2ndIn_attribute;

	memset((void *)&I2S2ndIn_attribute, 0, sizeof(I2S2ndIn_attribute));
	I2S2ndIn_attribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	I2S2ndIn_attribute.mI2S_SLAVE = bIsSlaveMode;
	I2S2ndIn_attribute.mI2S_SAMPLERATE = sampleRate;
	I2S2ndIn_attribute.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	I2S2ndIn_attribute.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	I2S2ndIn_attribute.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	Set2ndI2SIn(&I2S2ndIn_attribute);
	return true;
}

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S)
{
	uint32 Audio_I2S_Adc = 0;

	memcpy((void *)m2ndI2S, (void *)mDigitalI2S,
		sizeof(struct AudioDigtalI2S));
	if (!m2ndI2S->mI2S_SLAVE)	/* Master setting SampleRate only */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,
			 m2ndI2S->mI2S_SAMPLERATE);
	Audio_I2S_Adc |= (m2ndI2S->mINV_LRCK << 5);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_FMT << 3);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_SLAVE << 2);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_WLEN << 1);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_IN_PAD_SEL << 28);
	Audio_I2S_Adc |= 1 << 31;
	pr_debug("Set2ndI2SIn Audio_I2S_Adc= 0x%x\n", Audio_I2S_Adc);
	Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Adc, 0xfffffffe);
	if (!m2ndI2S->mI2S_SLAVE)
		Afe_Set_Reg(FPGA_CFG1, 1 << 8, 0x0100);
	else
		Afe_Set_Reg(FPGA_CFG1, 0, 0x0100);
	return true;
}

bool Set2ndI2SInEnable(bool bEnable)
{
	pr_debug("Set2ndI2SInEnable bEnable = %d\n", bEnable);
	m2ndI2S->mI2S_EN = bEnable;
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_2]->mState = bEnable;
	return true;
}

bool SetI2SASRCConfig(bool bIsUseASRC, unsigned int dToSampleRate)
{
	pr_debug("+%s() bIsUseASRC [%d] dToSampleRate [%d]\n",
		__func__, bIsUseASRC, dToSampleRate);
	if (true == bIsUseASRC) {
		WARN_ON(!(dToSampleRate == 44100 ||
			 dToSampleRate == 48000));
		Afe_Set_Reg(AFE_CONN4, 0, 1 << 30);
		/* To target sample rate */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, dToSampleRate);
		Afe_Set_Reg(AFE_ASRC_CON13, 0, 1 << 16);
		if (dToSampleRate == 44100) {
			Afe_Set_Reg(AFE_ASRC_CON14, 0xDC8000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON15, 0xA00000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON17, 0x1FBD, AFE_MASK_ALL);
		} else {
			Afe_Set_Reg(AFE_ASRC_CON14, 0x600000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON15, 0x400000, AFE_MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON17, 0xCB2, AFE_MASK_ALL);
		}
		/* Calibration setting */
		Afe_Set_Reg(AFE_ASRC_CON16, 0x00075987, AFE_MASK_ALL);
		/* Calibration setting */
		Afe_Set_Reg(AFE_ASRC_CON20, 0x00001b00, AFE_MASK_ALL);
	} else
		Afe_Set_Reg(AFE_CONN4, 1 << 30, 1 << 30);
	return true;
}

bool SetI2SASRCEnable(bool bEnable)
{
	if (true == bEnable)
		Afe_Set_Reg(AFE_ASRC_CON0, ((1 << 6) | (1 << 0)),
			 ((1 << 6) | (1 << 0)));
	else {
		uint32 dNeedDisableASM =
			 (Afe_Get_Reg(AFE_ASRC_CON0) & 0x0030) ? 1 : 0;

		Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 6 | dNeedDisableASM));
	}
	return true;
}

bool SetMemIfFetchFormatPerSample(uint32 InterfaceType, uint32 eFetchFormat)
{
	mAudioMEMIF[InterfaceType]->mFetchFormatPerSample = eFetchFormat;

	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 16,
				    0x00030000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 12,
				    0x00003000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 18,
				    0x000c0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:{

			pr_warn("Unsupport MEM_I2S!!\n");
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 20,
				    0x00300000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 22,
				    0x00C00000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 14,
				    0x00004000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 24,
				    0x03000000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 26,
				    0x0C000000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_HDMI:{
			Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE,
				    mAudioMEMIF[InterfaceType]->
				    mFetchFormatPerSample << 28,
				    0x30000000);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool SetoutputConnectionFormat(uint32 ConnectionFormat, uint32 Output)
{
	Afe_Set_Reg(AFE_CONN_24BIT, (ConnectionFormat << Output),
	(1 << Output));
	return true;
}

bool SetHDMIMCLK(void)
{
	uint32 mclksamplerate = mHDMIOutput->mSampleRate * 256;
	uint32 hdmi_APll = GetHDMIApLLSource();
	uint32 hdmi_mclk_div = 0;

	pr_debug("%s\n", __func__);
	if (hdmi_APll == APLL_SOURCE_24576)
		hdmi_APll = 24576000;
	else
		hdmi_APll = 22579200;
	pr_debug("%s hdmi_mclk_div = %d mclksamplerate = %d\n", __func__,
		 hdmi_mclk_div, mclksamplerate);
	hdmi_mclk_div = (hdmi_APll / mclksamplerate / 2) - 1;
	mHDMIOutput->mHdmiMckDiv = hdmi_mclk_div;
	pr_debug("%s hdmi_mclk_div = %d\n", __func__, hdmi_mclk_div);
	Afe_Set_Reg(FPGA_CFG1, hdmi_mclk_div << 24, 0x3f000000);
	SetCLkMclk(Soc_Aud_I2S3, mHDMIOutput->mSampleRate);
	return true;
}

bool SetHDMIBCLK(void)
{
	mHDMIOutput->mBckSamplerate =
		mHDMIOutput->mSampleRate * mHDMIOutput->mChannels;
	pr_debug("%s mBckSamplerate = %d mSampleRate = %d mChannels = %d\n",
		__func__, mHDMIOutput->mBckSamplerate,
		mHDMIOutput->mSampleRate, mHDMIOutput->mChannels);
	mHDMIOutput->mBckSamplerate *= (mHDMIOutput->mI2S_WLEN + 1) * 16;
	pr_debug("%s mBckSamplerate = %d mApllSamplerate = %d\n", __func__,
		 mHDMIOutput->mBckSamplerate, mHDMIOutput->mApllSamplerate);
	mHDMIOutput->mHdmiBckDiv =
	(mHDMIOutput->mApllSamplerate / mHDMIOutput->mBckSamplerate / 2) - 1;
	pr_debug("%s mHdmiBckDiv = %d\n", __func__, mHDMIOutput->mHdmiBckDiv);
	Afe_Set_Reg(FPGA_CFG1, (mHDMIOutput->mHdmiBckDiv) << 16, 0x00ff0000);
	return true;
}

uint32 GetHDMIApLLSource(void)
{
	pr_debug("%s ApllSource = %d\n", __func__, mHDMIOutput->mApllSource);
	return mHDMIOutput->mApllSource;
}

bool SetHDMIApLL(uint32 ApllSource)
{
	pr_debug("%s ApllSource = %d\n", __func__, ApllSource);
	if (ApllSource == APLL_SOURCE_24576) {
		Afe_Set_Reg(FPGA_CFG1, 0 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_24576;
		mHDMIOutput->mApllSamplerate = 24576000;
	} else if (ApllSource == APLL_SOURCE_225792) {
		Afe_Set_Reg(FPGA_CFG1, 1 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_225792;
		mHDMIOutput->mApllSamplerate = 22579200;
	}
	return true;
}

bool SetHDMIdatalength(uint32 length)
{
	pr_debug("%s length = %d\n", __func__, length);
	mHDMIOutput->mI2S_WLEN = length;
	return true;
}

bool SetHDMIsamplerate(uint32 samplerate)
{
	uint32 SampleRateinedx = SampleRateTransform(samplerate);

	mHDMIOutput->mSampleRate = samplerate;
	pr_debug("%s samplerate = %d\n", __func__, samplerate);
	switch (SampleRateinedx) {
	case Soc_Aud_I2S_SAMPLERATE_I2S_8K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_11K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_12K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_16K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_22K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_24K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_32K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_44K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_48K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_88K:
		SetHDMIApLL(APLL_SOURCE_225792);
		break;
	case Soc_Aud_I2S_SAMPLERATE_I2S_96K:
		SetHDMIApLL(APLL_SOURCE_24576);
		break;
	default:
		break;
	}
	return true;
}

bool SetTDMLrckWidth(uint32 cycles)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMbckcycle(uint32 cycles)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMChannelsSdata(uint32 channels)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMDatalength(uint32 length)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMI2Smode(uint32 mode)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMLrckInverse(bool enable)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMBckInverse(bool enable)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMEnable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_TDM_CON1, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_TDM_CON1, 0x0, 0x1);
	return true;
}

void SetHdmiTdm1Config(unsigned int channels, unsigned int i2s_wlen)
{
	unsigned int register_value = 0;

	register_value |= (MT_AFE_TDM_BCK_INVERSE << 1);
	register_value |= (MT_AFE_TDM_LRCK_NOT_INVERSE << 2);
	register_value |= (MT_AFE_TDM_1_BCK_CYCLE_DELAY << 3);
	/* aligned for I2S mode */
	register_value |= (MT_AFE_TDM_ALIGNED_TO_MSB << 4);
	register_value |= (MT_AFE_TDM_2CH_FOR_EACH_SDATA << 10);
	if (i2s_wlen == AFE_DATA_WLEN_32BIT) {
		register_value |= (MT_AFE_TDM_WLLEN_32BIT << 8);
		register_value |= (MT_AFE_TDM_32_BCK_CYCLES << 12);
		/* LRCK TDM WIDTH */
		register_value |= (((MT_AFE_TDM_WLLEN_32BIT << 4) - 1) << 24);
	} else {
		register_value |= (MT_AFE_TDM_WLLEN_16BIT << 8);
		register_value |= (MT_AFE_TDM_16_BCK_CYCLES << 12);
		/* LRCK TDM WIDTH */
		register_value |= (((MT_AFE_TDM_WLLEN_16BIT << 4) - 1) << 24);
	}
	Afe_Set_Reg(AFE_TDM_CON1, register_value, 0xFFFFFFFE);
}

void SetHdmiTdm2Config(unsigned int channels)
{
	unsigned int register_value = 0;

	switch (channels) {
	case 7:
	case 8:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_START_FROM_034_O35 << 8);
		register_value |= (CHANNEL_START_FROM_036_O37 << 12);
		break;
	case 5:
	case 6:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_START_FROM_034_O35 << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	case 3:
	case 4:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_DATA_IS_ZERO << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	case 1:
	case 2:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_DATA_IS_ZERO << 4);
		register_value |= (CHANNEL_DATA_IS_ZERO << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	default:
		return;
	}
	Afe_Set_Reg(AFE_TDM_CON2, register_value, 0x0000FFFF);
}


int AudDrv_Allocate_DL1_Buffer(struct device *pDev, kal_uint32 Afe_Buf_Length)
{
#ifdef AUDIO_MEMORY_SRAM
	kal_uint32 u4PhyAddr = 0;
#endif
	struct AFE_BLOCK_T *pblock;

	pr_debug("%s Afe_Buf_Length = %d\n", __func__, Afe_Buf_Length);
	pblock = &(
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	pblock->u4BufferSize = Afe_Buf_Length;
#ifdef AUDIO_MEMORY_SRAM
	if (Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE) {
		pr_debug("Afe_Buf_Len > AUDDRV_DL1_MAX_BUFFER_LENGTH\n");
		return -1;
	}
#endif
	/* allocate memory */
	{
#ifdef AUDIO_MEMORY_SRAM
		u4PhyAddr = AFE_INTERNAL_SRAM_PHY_BASE;
		pblock->pucPhysBufAddr = u4PhyAddr;
#ifdef AUDIO_MEM_IOREMAP
		pr_debug("Allocate_DL1_Buffer len %d\n",
			Afe_Buf_Length);
		pblock->pucVirtBufAddr =
			(kal_uint8 *) Get_Afe_SramBase_Pointer();
#else
		pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif
#else
		pr_debug("AudDrv_Allocate_DL1_Buffer use dram\n");
		pblock->pucVirtBufAddr =
		    dma_alloc_coherent(pDev, pblock->u4BufferSize,
			&pblock->pucPhysBufAddr,
			GFP_KERNEL);
#endif
	}
	pr_debug("Allocate_DL1_Buffer len %d virAddr %p\n",
		Afe_Buf_Length, pblock->pucVirtBufAddr);
	/* check 32 bytes align */
	if ((pblock->pucPhysBufAddr & 0x1f) != 0)
		pr_debug("Allocate_DL1_Buffer not aligned (0x%x)\n",
			pblock->pucPhysBufAddr);
	pblock->u4SampleNumMask = 0x001f;
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	/* set sram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END,
		pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);
#ifdef AUDIO_MEM_IOREMAP
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
#else
	memset(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
#endif
	return 0;
}

int AudDrv_Allocate_mem_Buffer(struct device *pDev,
	enum Soc_Aud_Digital_Block MemBlock, uint32 Buffer_length)
{
	switch (MemBlock) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL2:
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
	case Soc_Aud_Digital_Block_MEM_HDMI:{
		pr_debug("%s MemBlock =%d Buffer_length = %d\n",
		 __func__, MemBlock, Buffer_length);
		if (Audio_dma_buf[MemBlock] != NULL) {
		pr_debug
		    ("AudDrv_Allocate_mem_Buffer MemBlock %d\n",
		     MemBlock);
		if (Audio_dma_buf[MemBlock]->area == NULL) {
			pr_debug("dma_alloc_coherent\n");
			Audio_dma_buf[MemBlock]->area =
			    dma_alloc_coherent(pDev, Buffer_length,
				       &Audio_dma_buf[MemBlock]->addr,
				       GFP_KERNEL);
			if (Audio_dma_buf[MemBlock]->area)
			Audio_dma_buf[MemBlock]->bytes =
			 Buffer_length;
		}
		pr_debug("area = %p\n",
		 Audio_dma_buf[MemBlock]->area);
		}
	}
	break;
	case Soc_Aud_Digital_Block_MEM_VUL:{
		pr_debug("%s MemBlock =%d Buffer_length = %d\n",
		__func__, MemBlock, Buffer_length);
		if (Audio_dma_buf[MemBlock] != NULL) {
			pr_debug
			    ("AudDrv_Allocate_mem_Buffer MemBlock %d\n",
			     MemBlock);
			if (Audio_dma_buf[MemBlock]->area == NULL) {
				pr_debug("dma_alloc_coherent\n");
				Audio_dma_buf[MemBlock]->area =
				dma_alloc_coherent(pDev, Buffer_length,
					&Audio_dma_buf[MemBlock]->addr,
					GFP_KERNEL);
				if (Audio_dma_buf[MemBlock]->area)
					Audio_dma_buf[MemBlock]->bytes
						= Buffer_length;
				}
				pr_debug("area = %p\n",
					 Audio_dma_buf[MemBlock]->area);
			}
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:
		pr_warn("currently not support\n");
	default:
		pr_warn("%s not support\n", __func__);
	}
	return true;
}

struct AFE_MEM_CONTROL_T *Get_Mem_ControlT(enum Soc_Aud_Digital_Block MemBlock)
{
	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI)
		return AFE_Mem_Control_context[MemBlock];

	pr_err("%s error\n", __func__);
	return NULL;
}

bool SetMemifSubStream(enum Soc_Aud_Digital_Block MemBlock,
	struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	pr_debug("+%s MemBlock = %d substream = %p\n",
	 __func__, MemBlock, substream);
	spin_lock_irqsave(
	&AFE_Mem_Control_context[MemBlock]->substream_lock,
	 flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	if (head == NULL) {	/* first item is NULL */
		pr_warn("%s head == NULL\n", __func__);
		temp = kzalloc(sizeof(struct substreamList), GFP_ATOMIC);
		temp->substream = substream;
		temp->next = NULL;
		AFE_Mem_Control_context[MemBlock]->substreamL = temp;
	} else {		/* find out Null pointer */
		while (head->next != NULL)
			head = head->next;
		/* head->next is NULL */
		temp = kzalloc(sizeof(struct substreamList), GFP_ATOMIC);
		temp->substream = substream;
		temp->next = NULL;
		head->next = temp;
	}
	AFE_Mem_Control_context[MemBlock]->MemIfNum++;
	spin_unlock_irqrestore(
		&AFE_Mem_Control_context[MemBlock]->substream_lock,
		flags);
	pr_debug("-%s MemBlock = %d\n", __func__, MemBlock);
	return true;
}

bool ClearMemBlock(enum Soc_Aud_Digital_Block MemBlock)
{
	pr_warn("%s MemBlock = %d\n", __func__, MemBlock);

	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI) {
		struct AFE_BLOCK_T *pBlock =
			&AFE_Mem_Control_context[MemBlock]->rBlock;
#ifdef AUDIO_MEM_IOREMAP
		if (pBlock->pucVirtBufAddr ==
		(kal_uint8 *) Get_Afe_SramBase_Pointer()) {
			memset_io(pBlock->pucVirtBufAddr,
				 0, pBlock->u4BufferSize);
		} else {
#endif
			memset(pBlock->pucVirtBufAddr, 0, pBlock->u4BufferSize);
#ifdef AUDIO_MEM_IOREMAP
		}
#endif
		pr_warn("%s MemBlock %d reset done\n", __func__, MemBlock);
		pBlock->u4WriteIdx = 0;
		pBlock->u4DMAReadIdx = 0;
		pBlock->u4DataRemained = 0;
		pBlock->u4fsyncflag = false;
		pBlock->uResetFlag = true;
	} else {
		pr_err("%s error\n", __func__);
		return NULL;
	}
	return true;
}

bool RemoveMemifSubStream(enum Soc_Aud_Digital_Block MemBlock,
	struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->
	substream_lock, flags);
	if (AFE_Mem_Control_context[MemBlock]->MemIfNum == 0) {
		pr_debug("%s AFE_Mem_Control_context[%d]->MemIfNum == 0\n",
			__func__, MemBlock);
	} else {
		AFE_Mem_Control_context[MemBlock]->MemIfNum--;
	}
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	pr_debug("+ %s MemBlock = %d substream = %p\n",
	__func__, MemBlock, substream);
	if (head == NULL) {	/* no object */
		/* do nothing */
	} else {
		/* condition for first item hit */
		if (head->substream == substream) {
			AFE_Mem_Control_context[MemBlock]->substreamL
			= head->next;
			head->substream = NULL;
			kfree(head);
			head = NULL;
			/* DumpMemifSubStream(); */
		} else {
			temp = head;
			head = head->next;
			while (head) {
				if (head->substream == substream) {
					temp->next = head->next;
					head->substream = NULL;
					kfree(head);
					head = NULL;
					break;
				}
				temp = head;
				head = head->next;
			}
		}
	}
	/* DumpMemifSubStream(); */
	if (AFE_Mem_Control_context[MemBlock]->substreamL == NULL)
		ClearMemBlock(MemBlock);
	else {
		pr_debug("%s substreram is not NULL MemBlock = %d\n",
		__func__, MemBlock);
	}
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->
		substream_lock,
		flags);
	pr_debug("- %s MemBlock = %d\n", __func__, MemBlock);
	return true;
}

static unsigned long dl1_flags;
void Auddrv_Dl1_Spinlock_lock(void)
{
	spin_lock_irqsave(&auddrv_dl1_lock, dl1_flags);
}

void Auddrv_Dl1_Spinlock_unlock(void)
{
	spin_unlock_irqrestore(&auddrv_dl1_lock, dl1_flags);
}

static unsigned long ul1_flags;
void Auddrv_UL1_Spinlock_lock(void)
{
	spin_lock_irqsave(&auddrv_ul1_lock, ul1_flags);
}

void Auddrv_UL1_Spinlock_unlock(void)
{
	spin_unlock_irqrestore(&auddrv_ul1_lock, ul1_flags);
}

void Auddrv_HDMI_Interrupt_Handler(void)
{				/* irq5 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI];
	kal_int32 Afe_consumed_bytes = 0;
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	unsigned long flags;
	struct AFE_BLOCK_T *Afe_Block =
		&(AFE_Mem_Control_context[
			Soc_Aud_Digital_Block_MEM_HDMI]->rBlock);

	if (Mem_Block == NULL) {
		pr_err("%s Mem_Block == NULL\n", __func__);
		return;
	}
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == false) {
		pr_err("%s, GetMemoryPathEnable fail %d\n",
		__func__, Soc_Aud_Digital_Block_MEM_HDMI);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_HDMI_CUR);
	if (HW_Cur_ReadIdx == 0) {
		pr_debug("[%s] HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	pr_debug
	("%s 0 cur=%x mem_index=%x phyAddr=%x\n",
	__func__, HW_Cur_ReadIdx, HW_memory_index,
	Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}

	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;

	pr_debug("%s 1 rp:%x wp:%x remained:%x",
	__func__, Afe_Block->u4DMAReadIdx,
	Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained);

	pr_debug("consumed:%x mem_index:%x\n",
	Afe_consumed_bytes, HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes ||
		Afe_Block->u4DataRemained <= 0
		|| Afe_Block->u4DataRemained > Afe_Block->u4BufferSize) {

		pr_debug("%s 2 underflow rp:%x wp:%x, remained:%x",
		__func__, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4WriteIdx,
		Afe_Block->u4DataRemained);
		pr_debug(" consumed:%x mem_index:0x%x\n",
		Afe_consumed_bytes, HW_memory_index);

		Afe_Block->u4DMAReadIdx = HW_memory_index;
		Afe_Block->u4WriteIdx = Afe_Block->u4DMAReadIdx;
		Afe_Block->u4DataRemained = Afe_Block->u4BufferSize;
		pr_debug
		("%s 2 underflow rp:%x wp:%x remained:%x consumed:%x\n",
		__func__, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained,
		Afe_consumed_bytes);

	} else {
		pr_debug
	    ("%s 3 normal rp:%x ,remained:%x, wp:%x\n",
	    __func__, Afe_Block->u4DMAReadIdx,
	    Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
		pr_debug
		("%s 3 normal rp:%x, remained:%x, wp:%x\n",
		__func__, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained,
		Afe_Block->u4WriteIdx);
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI]->
		interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
				&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(
				Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
	pr_debug
	("[%s]4 ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
	 __func__, Afe_Block->u4DMAReadIdx,
	 Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);
}


void Auddrv_AWB_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_AWB];
	kal_uint32 HW_Cur_ReadIdx = 0;
	kal_uint32 MaxCopySize = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct substreamList *temp = NULL;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;
	kal_uint32 temp_cnt = 0;

	if (Mem_Block == NULL) {
		pr_err("-%s()Mem_Block == NULL\n", __func__);
		return;
	}
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) == false) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		pr_err("%s, GetMemoryPathEnable fail %d\n",
		__func__, Soc_Aud_Digital_Block_MEM_AWB);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_AWB_CUR));
	pr_debug("Auddrv_AWB_Interrupt_Handler cur = 0x%x\n",
	HW_Cur_ReadIdx);
	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	MaxCopySize = Get_Mem_MaxCopySize(Soc_Aud_Digital_Block_MEM_AWB);
	pr_debug
	("%s 1 mBlock = %p CopySize = 0x%x size = 0x%x\n",
	__func__, mBlock, MaxCopySize, mBlock->u4BufferSize);

	if (MaxCopySize) {
		if (MaxCopySize > mBlock->u4BufferSize)
			MaxCopySize = mBlock->u4BufferSize;

		mBlock->u4DataRemained -= MaxCopySize;
		mBlock->u4DMAReadIdx += MaxCopySize;
		mBlock->u4DMAReadIdx %= mBlock->u4BufferSize;
		Clear_Mem_CopySize(Soc_Aud_Digital_Block_MEM_AWB);

		pr_debug("%s update rp:0x%x,",
		 __func__, mBlock->u4DMAReadIdx);
		pr_debug
		("wp:0x%x, phyAddr:0x%x copysize:0x%x\n",
		mBlock->u4WriteIdx, mBlock->pucPhysBufAddr,
		mBlock->u4MaxCopySize);
	}

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr)
	 - mBlock->u4WriteIdx;

	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	pr_debug
	("%s Hw_Get_bytes:0x%x, cur:0x%x, rp:0x%x, wp:0x%x",
	__func__, Hw_Get_bytes, HW_Cur_ReadIdx,
	mBlock->u4DMAReadIdx, mBlock->u4WriteIdx);
	pr_debug
	    ("phyAddr:0x%x, copySize:0x%x, remained:0x%x\n",
	    mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize,
	    mBlock->u4DataRemained);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		pr_debug
		("%s overflow rp:%x wp:%x remained:%x size:%x\n",
		__func__, mBlock->u4DMAReadIdx,
		mBlock->u4WriteIdx,
		mBlock->u4DataRemained,
		mBlock->u4BufferSize);
		mBlock->u4DataRemained %= mBlock->u4BufferSize;
	}
	Mem_Block->interruptTrigger = 1;
	temp = Mem_Block->substreamL;
	while (temp != NULL) {
		if (temp->substream != NULL) {
			temp_cnt = Mem_Block->MemIfNum;
			spin_unlock_irqrestore(
				&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(temp->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
			if (temp_cnt != Mem_Block->MemIfNum) {
				pr_debug("%s temp_cnt %u, MemIfNum %u\n",
				__func__, temp_cnt, Mem_Block->MemIfNum);
				temp = Mem_Block->substreamL;
			}
		}
		if (temp != NULL)
			temp = temp->next;
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
	pr_debug
	("-%s rp:0x%x, wp:0x%x remained:0x%x\n",
	__func__, mBlock->u4DMAReadIdx,
	mBlock->u4WriteIdx,
	mBlock->u4DataRemained);
}

void Auddrv_DAI_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DAI];
	kal_uint32 HW_Cur_ReadIdx = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL)
		return;
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI) == false) {
		pr_err("%s, GetMemoryPathEnable fail %d\n",
		__func__, Soc_Aud_Digital_Block_MEM_DAI);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_DAI_CUR));
	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr)
		 - mBlock->u4WriteIdx;

	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	pr_debug("%s Hw_Get_bytes:0x%x, cur:0x%x, rp:0x%x",
	__func__, Hw_Get_bytes, HW_Cur_ReadIdx,
	mBlock->u4DMAReadIdx);

	pr_debug
	(" wp:0x%x, phyAddr:0x%x, memIfNum:%d\n",
	mBlock->u4WriteIdx, mBlock->pucPhysBufAddr,
	Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		pr_debug
		("overflow rp:%x, wp:%x, remained:%x, size:%x\n",
		mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
		mBlock->u4DataRemained,	mBlock->u4BufferSize);

	}
	Mem_Block->interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
				&Mem_Block->substream_lock,
				 flags);
			snd_pcm_period_elapsed(
				Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Auddrv_DL1_Interrupt_Handler(void)
{				/* irq1 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1];
	kal_int32 Afe_consumed_bytes = 0;
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T *Afe_Block =
		&(AFE_Mem_Control_context[
			Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	unsigned long flags;

	if (Mem_Block == NULL)
		return;
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == false) {
		pr_err("%s, GetMemoryPathEnable fail %d\n",
		__func__, Soc_Aud_Digital_Block_MEM_DL1);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
	if (HW_Cur_ReadIdx == 0) {
		pr_debug("[%s] HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
	pr_debug
	("%s cur:0x%x mem_index:0x%x phyAddr:0x%x\n",
	__func__, HW_Cur_ReadIdx, HW_memory_index,
	Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}
	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;

	pr_debug
	("%s ReadIdx:%x, wp:%x, remained:%x, consumed:%x",
	__func__, Afe_Block->u4DMAReadIdx,
	Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained,
	Afe_consumed_bytes);
	pr_debug(" HW_memory_index:%x\n", HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes ||
		Afe_Block->u4DataRemained <= 0
		|| Afe_Block->u4DataRemained > Afe_Block->u4BufferSize)
		pr_debug("%s underflow\n", __func__);
	else {
		pr_debug
		("%s normal rp:%x,remained:%x, wp:%x\n",
		__func__, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->
		interruptTrigger = 1;

	pr_debug
		("-%s ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
		__func__, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
			&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(
			Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Auddrv_DL2_Interrupt_Handler(void)
{				/* irq2 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2];
	kal_int32 Afe_consumed_bytes = 0;
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T *Afe_Block =
		&(AFE_Mem_Control_context[
		Soc_Aud_Digital_Block_MEM_DL2]->rBlock);
	unsigned long flags;

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2) == false) {
		pr_err("%s, GetMemoryPathEnable fail %d\n",
		__func__, Soc_Aud_Digital_Block_MEM_DL2);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL2_CUR);
	if (HW_Cur_ReadIdx == 0) {
		pr_debug("[%s] DL2 HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	pr_debug
	    ("%s cur:0x%x mem_index:0x%x phyAdrr:0x%x\n",
	    __func__, HW_Cur_ReadIdx,
	    HW_memory_index, Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}
	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;


	pr_debug
	("%s rp:%x wp:%x remained:%x consumed:%x mem_index:%x\n",
	__func__, Afe_Block->u4DMAReadIdx,
	Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained,
	Afe_consumed_bytes, HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes ||
		Afe_Block->u4DataRemained <= 0
		|| Afe_Block->u4DataRemained > Afe_Block->u4BufferSize)
		pr_debug("%s underflow\n", __func__);
	else {
		pr_debug
			("%s normal rp:%x, remained:%x, wp:%x\n",
			__func__, Afe_Block->u4DMAReadIdx,
			Afe_Block->u4DataRemained,
			Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->
	interruptTrigger = 1;

	pr_debug
		("-%s ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
		__func__,
		Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
			&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(
			Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

struct snd_dma_buffer *Get_Mem_Buffer(enum Soc_Aud_Digital_Block MemBlock)
{
	pr_debug("%s MemBlock = %d\n", __func__, MemBlock);
	switch (MemBlock) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DL2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_VUL:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DAI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_AWB:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_HDMI:
		return Audio_dma_buf[MemBlock];
	case Soc_Aud_Digital_Block_MEM_I2S:
		pr_warn("currently not support\n");
		break;
	default:
		break;
	}
	return NULL;
}

void Auddrv_UL1_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL];
	kal_uint32 HW_Cur_ReadIdx = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL) {
		pr_err("%s Mem_Block == NULL\n", __func__);
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false) {
		pr_err("%s, GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false\n",
			__func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_VUL_CUR));

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr)
		- mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	pr_debug
		("%s Hw_Get_bytes:%x, cur:%x, rp:%x, wp:%x",
			__func__, Hw_Get_bytes,
			HW_Cur_ReadIdx,	mBlock->u4DMAReadIdx,
			mBlock->u4WriteIdx);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		pr_debug
		("%s overflow rp:%x wp:%x remained:%x size:%x\n",
			__func__, mBlock->u4DMAReadIdx,
			mBlock->u4WriteIdx, mBlock->u4DataRemained,
			mBlock->u4BufferSize);
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL]->
		interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
			&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(
			Mem_Block->substreamL->substream);
			spin_lock_irqsave(
			&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void Clear_Mem_CopySize(enum Soc_Aud_Digital_Block MemBlock)
{
	struct substreamList *head;
	/* unsigned long flags; */

	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	pr_debug("%s MemBlock = %d\n", __func__, MemBlock);
	while (head != NULL) {	/* first item is NULL */
		head->u4MaxCopySize = 0;
		head = head->next;
	}

}

kal_uint32 Get_Mem_CopySizeByStream(enum Soc_Aud_Digital_Block MemBlock,
				    struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	unsigned long flags;
	kal_uint32 MaxCopySize;

	spin_lock_irqsave(
	&AFE_Mem_Control_context[MemBlock]->substream_lock,
	flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* pr_debug("%s MemBlock = %d\n", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (head->substream == substream) {
			MaxCopySize = head->u4MaxCopySize;
			spin_unlock_irqrestore(
			&AFE_Mem_Control_context[MemBlock]->substream_lock,
			flags);
			return MaxCopySize;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(
		&AFE_Mem_Control_context[MemBlock]->substream_lock,
		flags);
	return 0;
}

kal_uint32 Get_Mem_MaxCopySize(enum Soc_Aud_Digital_Block MemBlock)
{
	struct substreamList *head;
	kal_uint32 MaxCopySize;

	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	MaxCopySize = 0;
	/* pr_debug("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (MaxCopySize < head->u4MaxCopySize)
			MaxCopySize = head->u4MaxCopySize;
		head = head->next;
	}

	return MaxCopySize;
}

void Set_Mem_CopySizeByStream(enum Soc_Aud_Digital_Block MemBlock,
	struct snd_pcm_substream *substream,
	uint32 size)
{
	struct substreamList *head;
	unsigned long flags;

	spin_lock_irqsave(
		&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* pr_debug("+%s MemBlock = %d\n", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (head->substream == substream) {
			head->u4MaxCopySize += size;
			break;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(
		&AFE_Mem_Control_context[MemBlock]->substream_lock,
		flags);
	/* pr_debug("-%s MemBlock = %d\n ", __func__, MemBlock); */
}

void Auddrv_UL2_Interrupt_Handler(void)
{
	struct AFE_MEM_CONTROL_T *Mem_Block =
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2];
	kal_uint32 HW_Cur_ReadIdx = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	pr_debug("Auddrv_UL2_Interrupt_Handler\n");
	if (Mem_Block == NULL) {
		pr_err("%s Mem_Block == NULL\n", __func__);
		return;
	}
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2) == false) {
		pr_err("%s, GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2) == false\n",
			__func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_VUL_D2_CUR));

	pr_debug("%s HW_Cur_ReadIdx = 0x%x\n", __func__, HW_Cur_ReadIdx);

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr)
		 - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	pr_debug
		("%s Hw_Get_bytes:%x, cur:%x, rp:%x",
			__func__, Hw_Get_bytes,
			HW_Cur_ReadIdx, mBlock->u4DMAReadIdx);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		pr_debug
		("%s overflow rp:%x wp:%x remained:%x size:%x\n",
			__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx,
			mBlock->u4DataRemained,	mBlock->u4BufferSize);

		mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
		mBlock->u4DMAReadIdx = mBlock->u4WriteIdx
			- mBlock->u4BufferSize / 2;

		if (mBlock->u4DMAReadIdx < 0)
			mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
	}

	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->
		interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(
				&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(
				Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

bool BackUp_Audio_Register(void)
{
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	mAudioRegCache.REG_AUDIO_TOP_CON1 = Afe_Get_Reg(AUDIO_TOP_CON1);
	mAudioRegCache.REG_AUDIO_TOP_CON2 = Afe_Get_Reg(AUDIO_TOP_CON2);
	mAudioRegCache.REG_AUDIO_TOP_CON3 = Afe_Get_Reg(AUDIO_TOP_CON3);
	mAudioRegCache.REG_AFE_DAC_CON0 = Afe_Get_Reg(AFE_DAC_CON0);
	mAudioRegCache.REG_AFE_DAC_CON1 = Afe_Get_Reg(AFE_DAC_CON1);
	mAudioRegCache.REG_AFE_I2S_CON = Afe_Get_Reg(AFE_I2S_CON);
	mAudioRegCache.REG_AFE_DAIBT_CON0 = Afe_Get_Reg(AFE_DAIBT_CON0);
	mAudioRegCache.REG_AFE_CONN0 = Afe_Get_Reg(AFE_CONN0);
	mAudioRegCache.REG_AFE_CONN1 = Afe_Get_Reg(AFE_CONN1);
	mAudioRegCache.REG_AFE_CONN2 = Afe_Get_Reg(AFE_CONN2);
	mAudioRegCache.REG_AFE_CONN3 = Afe_Get_Reg(AFE_CONN3);
	mAudioRegCache.REG_AFE_CONN4 = Afe_Get_Reg(AFE_CONN4);
	mAudioRegCache.REG_AFE_I2S_CON1 = Afe_Get_Reg(AFE_I2S_CON1);
	mAudioRegCache.REG_AFE_I2S_CON2 = Afe_Get_Reg(AFE_I2S_CON2);
	mAudioRegCache.REG_AFE_MRGIF_CON = Afe_Get_Reg(AFE_MRGIF_CON);
	mAudioRegCache.REG_AFE_DL1_BASE = Afe_Get_Reg(AFE_DL1_BASE);
	mAudioRegCache.REG_AFE_DL1_CUR = Afe_Get_Reg(AFE_DL1_CUR);
	mAudioRegCache.REG_AFE_DL1_END = Afe_Get_Reg(AFE_DL1_END);
	mAudioRegCache.REG_AFE_DL1_D2_BASE = Afe_Get_Reg(AFE_DL1_D2_BASE);
	mAudioRegCache.REG_AFE_DL1_D2_CUR = Afe_Get_Reg(AFE_DL1_D2_CUR);
	mAudioRegCache.REG_AFE_DL1_D2_END = Afe_Get_Reg(AFE_DL1_D2_END);
	mAudioRegCache.REG_AFE_VUL_D2_BASE = Afe_Get_Reg(AFE_VUL_D2_BASE);
	mAudioRegCache.REG_AFE_VUL_D2_END = Afe_Get_Reg(AFE_VUL_D2_END);
	mAudioRegCache.REG_AFE_VUL_D2_CUR = Afe_Get_Reg(AFE_VUL_D2_CUR);
	mAudioRegCache.REG_AFE_I2S_CON3 = Afe_Get_Reg(AFE_I2S_CON3);
	mAudioRegCache.REG_AFE_DL2_BASE = Afe_Get_Reg(AFE_DL2_BASE);
	mAudioRegCache.REG_AFE_DL2_CUR = Afe_Get_Reg(AFE_DL2_CUR);
	mAudioRegCache.REG_AFE_DL2_END = Afe_Get_Reg(AFE_DL2_END);
	mAudioRegCache.REG_AFE_CONN5 = Afe_Get_Reg(AFE_CONN5);
	mAudioRegCache.REG_AFE_CONN_24BIT = Afe_Get_Reg(AFE_CONN_24BIT);
	mAudioRegCache.REG_AFE_AWB_BASE = Afe_Get_Reg(AFE_AWB_BASE);
	mAudioRegCache.REG_AFE_AWB_END = Afe_Get_Reg(AFE_AWB_END);
	mAudioRegCache.REG_AFE_AWB_CUR = Afe_Get_Reg(AFE_AWB_CUR);
	mAudioRegCache.REG_AFE_VUL_BASE = Afe_Get_Reg(AFE_VUL_BASE);
	mAudioRegCache.REG_AFE_VUL_END = Afe_Get_Reg(AFE_VUL_END);
	mAudioRegCache.REG_AFE_VUL_CUR = Afe_Get_Reg(AFE_VUL_CUR);
	mAudioRegCache.REG_AFE_DAI_BASE = Afe_Get_Reg(AFE_DAI_BASE);
	mAudioRegCache.REG_AFE_DAI_END = Afe_Get_Reg(AFE_DAI_END);
	mAudioRegCache.REG_AFE_DAI_CUR = Afe_Get_Reg(AFE_DAI_CUR);
	mAudioRegCache.REG_AFE_CONN6 = Afe_Get_Reg(AFE_CONN6);
	mAudioRegCache.REG_AFE_MEMIF_MSB = Afe_Get_Reg(AFE_MEMIF_MSB);
	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON0 =
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0);
	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON1 =
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON0 =
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON1 =
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1);
	mAudioRegCache.REG_AFE_ADDA_TOP_CON0 = Afe_Get_Reg(AFE_ADDA_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_DL_CON0 =
		Afe_Get_Reg(AFE_ADDA_UL_DL_CON0);
	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG0 =
		Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0);
	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG1 =
		Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1);
	mAudioRegCache.REG_AFE_SIDETONE_CON0 = Afe_Get_Reg(AFE_SIDETONE_CON0);
	mAudioRegCache.REG_AFE_SIDETONE_COEFF =
		Afe_Get_Reg(AFE_SIDETONE_COEFF);
	mAudioRegCache.REG_AFE_SIDETONE_CON1 = Afe_Get_Reg(AFE_SIDETONE_CON1);
	mAudioRegCache.REG_AFE_SIDETONE_GAIN = Afe_Get_Reg(AFE_SIDETONE_GAIN);
	mAudioRegCache.REG_AFE_SGEN_CON0 = Afe_Get_Reg(AFE_SGEN_CON0);
	mAudioRegCache.REG_AFE_TOP_CON0 = Afe_Get_Reg(AFE_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON0 =
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON1 =
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON1);
	mAudioRegCache.REG_AFE_MOD_DAI_BASE = Afe_Get_Reg(AFE_MOD_DAI_BASE);
	mAudioRegCache.REG_AFE_MOD_DAI_END = Afe_Get_Reg(AFE_MOD_DAI_END);
	mAudioRegCache.REG_AFE_MOD_DAI_CUR = Afe_Get_Reg(AFE_MOD_DAI_CUR);
	mAudioRegCache.REG_AFE_IRQ_MCU_CON = Afe_Get_Reg(AFE_IRQ_MCU_CON);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT1 = Afe_Get_Reg(AFE_IRQ_MCU_CNT1);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT2 = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
	mAudioRegCache.REG_AFE_IRQ_MCU_EN = Afe_Get_Reg(AFE_IRQ_MCU_EN);
	mAudioRegCache.REG_AFE_MEMIF_MAXLEN = Afe_Get_Reg(AFE_MEMIF_MAXLEN);
	mAudioRegCache.REG_AFE_MEMIF_PBUF_SIZE =
		Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT7 = Afe_Get_Reg(AFE_IRQ_MCU_CNT7);
	mAudioRegCache.REG_AFE_APLL1_TUNER_CFG =
		Afe_Get_Reg(AFE_APLL1_TUNER_CFG);
	mAudioRegCache.REG_AFE_APLL2_TUNER_CFG =
		Afe_Get_Reg(AFE_APLL2_TUNER_CFG);
	mAudioRegCache.REG_AFE_GAIN1_CON0 = Afe_Get_Reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN1_CON1 = Afe_Get_Reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN1_CON2 = Afe_Get_Reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN1_CON3 = Afe_Get_Reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN1_CONN = Afe_Get_Reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN1_CUR = Afe_Get_Reg(AFE_GAIN1_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CON0 = Afe_Get_Reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN2_CON1 = Afe_Get_Reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN2_CON2 = Afe_Get_Reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN2_CON3 = Afe_Get_Reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN2_CONN = Afe_Get_Reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN2_CUR = Afe_Get_Reg(AFE_GAIN2_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CONN2 = Afe_Get_Reg(AFE_GAIN2_CONN2);
	mAudioRegCache.REG_AFE_GAIN2_CONN3 = Afe_Get_Reg(AFE_GAIN2_CONN3);
	mAudioRegCache.REG_AFE_GAIN1_CONN2 = Afe_Get_Reg(AFE_GAIN1_CONN2);
	mAudioRegCache.REG_AFE_GAIN1_CONN3 = Afe_Get_Reg(AFE_GAIN1_CONN3);
	mAudioRegCache.REG_AFE_CONN7 = Afe_Get_Reg(AFE_CONN7);
	mAudioRegCache.REG_AFE_CONN8 = Afe_Get_Reg(AFE_CONN8);
	mAudioRegCache.REG_AFE_CONN9 = Afe_Get_Reg(AFE_CONN9);
	mAudioRegCache.REG_AFE_CONN10 = Afe_Get_Reg(AFE_CONN10);
	mAudioRegCache.REG_FPGA_CFG2 = Afe_Get_Reg(FPGA_CFG2);
	mAudioRegCache.REG_FPGA_CFG3 = Afe_Get_Reg(FPGA_CFG3);
	mAudioRegCache.REG_FPGA_CFG0 = Afe_Get_Reg(FPGA_CFG0);
	mAudioRegCache.REG_FPGA_CFG1 = Afe_Get_Reg(FPGA_CFG1);
	mAudioRegCache.REG_AFE_ASRC_CON0 = Afe_Get_Reg(AFE_ASRC_CON0);
	mAudioRegCache.REG_AFE_ASRC_CON1 = Afe_Get_Reg(AFE_ASRC_CON1);
	mAudioRegCache.REG_AFE_ASRC_CON2 = Afe_Get_Reg(AFE_ASRC_CON2);
	mAudioRegCache.REG_AFE_ASRC_CON3 = Afe_Get_Reg(AFE_ASRC_CON3);
	mAudioRegCache.REG_AFE_ASRC_CON4 = Afe_Get_Reg(AFE_ASRC_CON4);
	mAudioRegCache.REG_AFE_ASRC_CON5 = Afe_Get_Reg(AFE_ASRC_CON5);
	mAudioRegCache.REG_AFE_ASRC_CON6 = Afe_Get_Reg(AFE_ASRC_CON6);
	mAudioRegCache.REG_AFE_ASRC_CON7 = Afe_Get_Reg(AFE_ASRC_CON7);
	mAudioRegCache.REG_AFE_ASRC_CON8 = Afe_Get_Reg(AFE_ASRC_CON8);
	mAudioRegCache.REG_AFE_ASRC_CON9 = Afe_Get_Reg(AFE_ASRC_CON9);
	mAudioRegCache.REG_AFE_ASRC_CON10 = Afe_Get_Reg(AFE_ASRC_CON10);
	mAudioRegCache.REG_AFE_ASRC_CON11 = Afe_Get_Reg(AFE_ASRC_CON11);
	mAudioRegCache.REG_PCM_INTF_CON = Afe_Get_Reg(PCM_INTF_CON);
	mAudioRegCache.REG_PCM_INTF_CON2 = Afe_Get_Reg(PCM_INTF_CON2);
	mAudioRegCache.REG_PCM2_INTF_CON = Afe_Get_Reg(PCM2_INTF_CON);
	mAudioRegCache.REG_AUDIO_CLK_AUDDIV_0 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_0);
	mAudioRegCache.REG_AUDIO_CLK_AUDDIV_1 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_1);
	mAudioRegCache.REG_AFE_ASRC4_CON0 = Afe_Get_Reg(AFE_ASRC4_CON0);
	mAudioRegCache.REG_AFE_ASRC4_CON1 = Afe_Get_Reg(AFE_ASRC4_CON1);
	mAudioRegCache.REG_AFE_ASRC4_CON2 = Afe_Get_Reg(AFE_ASRC4_CON2);
	mAudioRegCache.REG_AFE_ASRC4_CON3 = Afe_Get_Reg(AFE_ASRC4_CON3);
	mAudioRegCache.REG_AFE_ASRC4_CON4 = Afe_Get_Reg(AFE_ASRC4_CON4);
	mAudioRegCache.REG_AFE_ASRC4_CON5 = Afe_Get_Reg(AFE_ASRC4_CON5);
	mAudioRegCache.REG_AFE_ASRC4_CON6 = Afe_Get_Reg(AFE_ASRC4_CON6);
	mAudioRegCache.REG_AFE_ASRC4_CON7 = Afe_Get_Reg(AFE_ASRC4_CON7);
	mAudioRegCache.REG_AFE_ASRC4_CON8 = Afe_Get_Reg(AFE_ASRC4_CON8);
	mAudioRegCache.REG_AFE_ASRC4_CON9 = Afe_Get_Reg(AFE_ASRC4_CON9);
	mAudioRegCache.REG_AFE_ASRC4_CON10 = Afe_Get_Reg(AFE_ASRC4_CON10);
	mAudioRegCache.REG_AFE_ASRC4_CON11 = Afe_Get_Reg(AFE_ASRC4_CON11);
	mAudioRegCache.REG_AFE_ASRC4_CON12 = Afe_Get_Reg(AFE_ASRC4_CON12);
	mAudioRegCache.REG_AFE_ASRC4_CON13 = Afe_Get_Reg(AFE_ASRC4_CON13);
	mAudioRegCache.REG_AFE_ASRC4_CON14 = Afe_Get_Reg(AFE_ASRC4_CON14);
	mAudioRegCache.REG_AFE_ASRC_CON13 = Afe_Get_Reg(AFE_ASRC_CON13);
	mAudioRegCache.REG_AFE_ASRC_CON14 = Afe_Get_Reg(AFE_ASRC_CON14);
	mAudioRegCache.REG_AFE_ASRC_CON15 = Afe_Get_Reg(AFE_ASRC_CON15);
	mAudioRegCache.REG_AFE_ASRC_CON16 = Afe_Get_Reg(AFE_ASRC_CON16);
	mAudioRegCache.REG_AFE_ASRC_CON17 = Afe_Get_Reg(AFE_ASRC_CON17);
	mAudioRegCache.REG_AFE_ASRC_CON18 = Afe_Get_Reg(AFE_ASRC_CON18);
	mAudioRegCache.REG_AFE_ASRC_CON19 = Afe_Get_Reg(AFE_ASRC_CON19);
	mAudioRegCache.REG_AFE_ASRC_CON20 = Afe_Get_Reg(AFE_ASRC_CON20);
	mAudioRegCache.REG_AFE_ASRC_CON21 = Afe_Get_Reg(AFE_ASRC_CON21);
	mAudioRegCache.REG_AFE_ASRC2_CON0 = Afe_Get_Reg(AFE_ASRC2_CON0);
	mAudioRegCache.REG_AFE_ASRC2_CON1 = Afe_Get_Reg(AFE_ASRC2_CON1);
	mAudioRegCache.REG_AFE_ASRC2_CON2 = Afe_Get_Reg(AFE_ASRC2_CON2);
	mAudioRegCache.REG_AFE_ASRC2_CON3 = Afe_Get_Reg(AFE_ASRC2_CON3);
	mAudioRegCache.REG_AFE_ASRC2_CON4 = Afe_Get_Reg(AFE_ASRC2_CON4);
	mAudioRegCache.REG_AFE_ASRC2_CON5 = Afe_Get_Reg(AFE_ASRC2_CON5);
	mAudioRegCache.REG_AFE_ASRC2_CON6 = Afe_Get_Reg(AFE_ASRC2_CON6);
	mAudioRegCache.REG_AFE_ASRC2_CON7 = Afe_Get_Reg(AFE_ASRC2_CON7);
	mAudioRegCache.REG_AFE_ASRC2_CON8 = Afe_Get_Reg(AFE_ASRC2_CON8);
	mAudioRegCache.REG_AFE_ASRC2_CON9 = Afe_Get_Reg(AFE_ASRC2_CON9);
	mAudioRegCache.REG_AFE_ASRC2_CON10 = Afe_Get_Reg(AFE_ASRC2_CON10);
	mAudioRegCache.REG_AFE_ASRC2_CON11 = Afe_Get_Reg(AFE_ASRC2_CON11);
	mAudioRegCache.REG_AFE_ASRC2_CON12 = Afe_Get_Reg(AFE_ASRC2_CON12);
	mAudioRegCache.REG_AFE_ASRC2_CON13 = Afe_Get_Reg(AFE_ASRC2_CON13);
	mAudioRegCache.REG_AFE_ASRC2_CON14 = Afe_Get_Reg(AFE_ASRC2_CON14);
	mAudioRegCache.REG_AFE_ASRC3_CON0 = Afe_Get_Reg(AFE_ASRC3_CON0);
	mAudioRegCache.REG_AFE_ASRC3_CON1 = Afe_Get_Reg(AFE_ASRC3_CON1);
	mAudioRegCache.REG_AFE_ASRC3_CON2 = Afe_Get_Reg(AFE_ASRC3_CON2);
	mAudioRegCache.REG_AFE_ASRC3_CON3 = Afe_Get_Reg(AFE_ASRC3_CON3);
	mAudioRegCache.REG_AFE_ASRC3_CON4 = Afe_Get_Reg(AFE_ASRC3_CON4);
	mAudioRegCache.REG_AFE_ASRC3_CON5 = Afe_Get_Reg(AFE_ASRC3_CON5);
	mAudioRegCache.REG_AFE_ASRC3_CON6 = Afe_Get_Reg(AFE_ASRC3_CON6);
	mAudioRegCache.REG_AFE_ASRC3_CON7 = Afe_Get_Reg(AFE_ASRC3_CON7);
	mAudioRegCache.REG_AFE_ASRC3_CON8 = Afe_Get_Reg(AFE_ASRC3_CON8);
	mAudioRegCache.REG_AFE_ASRC3_CON9 = Afe_Get_Reg(AFE_ASRC3_CON9);
	mAudioRegCache.REG_AFE_ASRC3_CON10 = Afe_Get_Reg(AFE_ASRC3_CON10);
	mAudioRegCache.REG_AFE_ASRC3_CON11 = Afe_Get_Reg(AFE_ASRC3_CON11);
	mAudioRegCache.REG_AFE_ASRC3_CON12 = Afe_Get_Reg(AFE_ASRC3_CON12);
	mAudioRegCache.REG_AFE_ASRC3_CON13 = Afe_Get_Reg(AFE_ASRC3_CON13);
	mAudioRegCache.REG_AFE_ASRC3_CON14 = Afe_Get_Reg(AFE_ASRC3_CON14);
	mAudioRegCache.REG_AFE_ADDA4_TOP_CON0 = Afe_Get_Reg(AFE_ADDA4_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON0 =
		Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON0);
	mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON1 =
		Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON1);
	mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG0 =
		Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG0);
	mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG1 =
		Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG1);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_02_01 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_02_01);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_04_03 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_04_03);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_06_05 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_06_05);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_08_07 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_08_07);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_10_09 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_10_09);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_12_11 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_12_11);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_14_13 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_14_13);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_16_15 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_16_15);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_18_17 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_18_17);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_20_19 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_20_19);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_22_21 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_22_21);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_24_23 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_24_23);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_26_25 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_26_25);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_28_27 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_28_27);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_30_29 =
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_30_29);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	return true;
}

bool Restore_Audio_Register(void)
{
	/* TODO? */
	return true;
}

unsigned int Align64ByteSize(unsigned int insize)
{
#define MAGIC_NUMBER 0xFFFFFFC0
	unsigned int align_size;

	align_size = insize & MAGIC_NUMBER;
	return align_size;
}


bool SetOffloadCbk(enum Soc_Aud_Digital_Block block, void *offloadstream,
		   void (*offloadCbk)(void *stream))
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[block];

	Mem_Block->offloadCbk = offloadCbk;
	Mem_Block->offloadstream = offloadstream;
	pr_debug("%s stream:%p, callback:%p\n",
		__func__, offloadstream, offloadCbk);
	return true;
}

bool ClrOffloadCbk(enum Soc_Aud_Digital_Block block, void *offloadstream)
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[block];

	if (Mem_Block->offloadstream != offloadstream) {
		pr_err("%s fail, original:%p, specified:%p\n",
		__func__, Mem_Block->offloadstream, offloadstream);
		return false;
	}
	pr_debug("%s %p\n", __func__, offloadstream);
	Mem_Block->offloadstream = NULL;
	return true;
}
