/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
/* #include <mach/irqs.h> */
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
/* #include <mach/mt_reg_base.h> */
#include <asm/div64.h>
/* #include <linux/aee.h> */
/* #include <mach/pmic_mt6325_sw.h> */
/* #include <mach/upmu_common.h> */
/* #include <mach/upmu_hw.h> */
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mt-plat/upmu_common.h> */
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
/* #include <asm/mach-types.h> */

#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

/* #include <mach/mt_boot.h> */
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_boot_common.h>

/* #include <cust_eint.h> */
/* #include <cust_gpio_usage.h> */
/* #include <mach/eint.h> */
#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"


#include "mt_soc_digital_type.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_afe_connection.h"
#include "mt_soc_pcm_common.h"
#include <linux/of.h>
#include <linux/of_irq.h>
/*#define HDMI_PASS_I2S_DEBUG*/
#define MT8127_AFE_MCU_IRQ_LINE (104 + 32)

static DEFINE_SPINLOCK(afe_control_lock);
static DEFINE_SPINLOCK(afe_sram_control_lock);


/* static  variable */

static bool AudioDaiBtStatus;
static bool AudioAdcI2SStatus;
static bool mAudioInit;
static bool mVOWStatus;
/*static unsigned int MCLKFS = 128;*/
static struct AudioDigtalI2S *AudioAdcI2S;
static struct AudioDigtalI2S *m2ndI2S;	/* input */
static struct AudioDigtalI2S *m2ndI2Sout;	/* output */
static bool mFMEnable;
static bool mOffloadEnable;
static bool mOffloadSWMode;

static struct AudioHdmi *mHDMIOutput;
static struct AudioMrgIf *mAudioMrg;
static struct AudioDigitalDAIBT *AudioDaiBt;
static const bool audio_adc_i2s_status;

static struct AFE_MEM_CONTROL_T *AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI + 1] = { NULL };
static struct snd_dma_buffer *Audio_dma_buf[Soc_Aud_Digital_Block_MEM_HDMI + 1] = { NULL };

static struct AudioIrqMcuMode *audio_mcu_mode[Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE] = { NULL };
static struct mt_afe_mem_if_attribute *audio_mem_if[Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK] = { NULL };

static struct AudioAfeRegCache mAudioRegCache;
static struct AudioSramManager mAudioSramManager;
const unsigned int AudioSramPlaybackFullSize = 1024 * 16;
const unsigned int AudioSramPlaybackPartialSize = 1024 * 16;
const unsigned int AudioDramPlaybackSize = 1024 * 16;
const size_t AudioSramCaptureSize = 1024 * 16;
const size_t AudioDramCaptureSize = 1024 * 16;
const size_t AudioInterruptLimiter = 100;
#if 0
static int Aud_APLL_DIV_APLL1_cntr;
static int Aud_APLL_DIV_APLL2_cntr;
#endif
static unsigned int audio_irq_id = MT8127_AFE_MCU_IRQ_LINE;
static struct device *mach_dev;

static bool mExternalModemStatus;

/*static function*/
static void mt_afe_clean_predistortion(void);
static bool mt_afe_set_dl_src2(uint32_t sample_rate);
/*static struct mt_afe_mem_control_t *afe_mem_control_context[MT_AFE_MEM_CTX_COUNT] = { NULL };*/

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
 *    static function declaration
 */
static int mt_afe_register_irq(void *dev);
static irqreturn_t mt_afe_irq_handler(int irq, void *dev_id);



/*
 *    function implementation
 */
static void clear_mem_copysize(enum Soc_Aud_Digital_Block MemBlock);
static uint32_t get_mem_maxcopysize(enum Soc_Aud_Digital_Block MemBlock);

static bool CheckSize(uint32_t size)
{
	if (size == 0) {
		pr_err("CheckSize size = 0\n");
		return true;
	}
	return false;
}

void afe_control_mutex_lock(void)/*AfeControlMutexLock*/
{
	mutex_lock(&afe_control_mutex);
}

void afe_control_mutex_unlock(void)/*AfeControlMutexUnLock*/
{
	mutex_unlock(&afe_control_mutex);
}

void afe_control_sram_lock(void) /*AfeControlSramLock*/
{
	spin_lock(&afe_sram_control_lock);
}

void afe_control_sram_unlock(void)/*AfeControlSramUnLock*/
{
	spin_unlock(&afe_sram_control_lock);
}


unsigned int get_sramstate(void)/*GetSramState*/
{
	return mAudioSramManager.mMemoryState;
}

void set_sramstate(unsigned int State)/*SetSramState*/
{
	PRINTK_AUDDRV("%s state= %d\n", __func__, State);
	mAudioSramManager.mMemoryState |= State;
}

void clear_sramstate(unsigned int State)/*ClearSramState*/
{
	PRINTK_AUDDRV("%s state= %d\n", __func__, State);
	mAudioSramManager.mMemoryState &= (~State);
}


unsigned int get_playback_sram_fullsize(void)/*GetPLaybackSramFullSize*/
{
	unsigned int Sramsize = AudioSramPlaybackFullSize;

	if (AudioSramPlaybackFullSize > AFE_INTERNAL_SRAM_SIZE)
		Sramsize = AFE_INTERNAL_SRAM_SIZE;
	return Sramsize;
}

unsigned int GetPLaybackSramPartial(void)
{
	unsigned int Sramsize = AudioSramPlaybackPartialSize;

	return Sramsize;
}

unsigned int GetPLaybackDramSize(void)
{
	return AudioDramPlaybackSize;
}

size_t GetCaptureSramSize(void)
{
	unsigned int Sramsize = AudioSramCaptureSize;

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

static void FillDatatoDlmemory(volatile unsigned int *memorypointer,
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
	afe_allocate_mem_buffer(NULL, Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = afe_get_mem_buffer(Soc_Aud_Digital_Block_MEM_DL1);
	mt_afe_set_reg(AFE_DL1_BASE, Dl1_Playback_dma_buf->addr, 0xffffffff);
	mt_afe_set_reg(AFE_DL1_END, Dl1_Playback_dma_buf->addr + (Dl1_MAX_BUFFER_SIZE - 1),
		    0xffffffff);
}


void OpenAfeDigitaldl1(bool bEnable)
{
	volatile unsigned int *Sramdata;

	if (bEnable == true) {
		SetDL1BufferwithBuf();
		mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_I2S, 44100);

		mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
			      Soc_Aud_InterConnectionOutput_O03);
		mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
			      Soc_Aud_InterConnectionOutput_O04);

	mt_afe_enable_memory_path(Soc_Aud_Digital_Block_MEM_DL1);
		Sramdata = (unsigned int *)(Dl1_Playback_dma_buf->area);
		FillDatatoDlmemory(Sramdata, Dl1_Playback_dma_buf->bytes, 0);
		/* msleep(5); */
		usleep_range(5*1000, 20*1000);

		if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(44100);
			mt_afe_enable_i2s_dac();
		} else
		mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
		mt_afe_enable_afe(true);
	} else {
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_MEM_DL1);

		if (!mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC))
			mt_afe_disable_i2s_dac();

		mt_afe_enable_afe(false);
	}
}

void SetExternalModemStatus(const bool bEnable)
{
	PRINTK_AUDDRV("%s(), mExternalModemStatus: %d => %d\n", __func__,
		mExternalModemStatus, bEnable);
	mExternalModemStatus = bEnable;
}


/*****************************************************************************
 * FUNCTION
 *  InitAfeControl ,ResetAfeControl
 *
 * DESCRIPTION
 *  afe init function
 *
 *****************************************************************************
 */

void mt_afe_init_control(void *dev) /*InitAfeControl*/
{

	int i = 0;

	afe_control_mutex_lock();
	/* allocate memory for pointers */
	if (mAudioInit == false) {
		mAudioInit = true;
		mAudioMrg = devm_kzalloc(dev, sizeof(struct AudioMrgIf), GFP_KERNEL);
		AudioDaiBt = devm_kzalloc(dev, sizeof(struct AudioDigitalDAIBT), GFP_KERNEL);
		AudioAdcI2S = devm_kzalloc(dev, sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2S = devm_kzalloc(dev, sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2Sout = devm_kzalloc(dev, sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		mHDMIOutput = devm_kzalloc(dev, sizeof(struct AudioHdmi), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++)
			audio_mcu_mode[i] = devm_kzalloc(dev, sizeof(struct AudioIrqMcuMode), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
			audio_mem_if[i] = devm_kzalloc(dev, sizeof(struct mt_afe_mem_if_attribute), GFP_KERNEL);

		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++) {
			AFE_Mem_Control_context[i] = devm_kzalloc(dev, sizeof(struct AFE_MEM_CONTROL_T), GFP_KERNEL);
			AFE_Mem_Control_context[i]->substreamL = NULL;
			spin_lock_init(&AFE_Mem_Control_context[i]->substream_lock);
			}

		for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI; i++)
			Audio_dma_buf[i] = devm_kzalloc(dev, sizeof(Audio_dma_buf), GFP_KERNEL);
	}
	AudioDaiBtStatus = false;
	AudioAdcI2SStatus = false;
	memset((void *)&mAudioSramManager, 0, sizeof(struct AudioSramManager));
	mAudioMrg->Mrg_I2S_SampleRate = mt_afe_rate_to_idx(44100);
	/* set APLL clock setting */
	afe_control_mutex_unlock();
}
bool ResetAfeControl(void)
{
	int i = 0;

	PRINTK_AUDDRV("ResetAfeControl\n");
	afe_control_mutex_lock();
	mAudioInit = false;
	memset((void *)(mAudioMrg), 0, sizeof(struct AudioMrgIf));
	memset((void *)(AudioDaiBt), 0, sizeof(struct AudioDigitalDAIBT));

	for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++)
		memset((void *)(audio_mcu_mode[i]), 0, sizeof(struct AudioIrqMcuMode));

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
		memset((void *)(audio_mem_if[i]), 0, sizeof(struct mt_afe_mem_if_attribute));

	for (i = 0; i < (Soc_Aud_Digital_Block_MEM_HDMI + 1); i++)
		memset((void *)(AFE_Mem_Control_context[i]), 0, sizeof(struct AFE_MEM_CONTROL_T));

	afe_control_mutex_unlock();
	return true;
}


static int mt_afe_register_irq(void *dev)
{
	const int ret = request_irq(audio_irq_id, mt_afe_irq_handler,
				IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
	if (unlikely(ret < 0))
		pr_err("%s %d\n", __func__, ret);

	return ret;
}

static int irqcount;
/*****************************************************************************
 * FUNCTION
 *  mt_afe_irq_handler / AudDrv_magic_tasklet
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
*/
irqreturn_t mt_afe_irq_handler(int irq, void *dev_id)/*AudDrv_IRQ_handler*/
{
	/* unsigned long flags; */
	uint32_t volatile u4RegValue;
	uint32_t volatile u4tmpValue;
	uint32_t volatile u4tmpValue1;
	uint32_t volatile u4tmpValue2;

	mt_afe_main_clk_on();
	u4RegValue = mt_afe_get_reg(AFE_IRQ_MCU_STATUS);
	u4RegValue &= 0xff;
	u4tmpValue = mt_afe_get_reg(AFE_IRQ_MCU_EN);
	u4tmpValue &= 0xff;
	u4tmpValue1 = mt_afe_get_reg(AFE_IRQ_MCU_CNT5);
	u4tmpValue1 &= 0x0003ffff;
	u4tmpValue2 = mt_afe_get_reg(AFE_IRQ5_MCU_EN_CNT_MON);
	u4tmpValue2 &= 0x0003ffff;
	/* here is error handle, for interrupt is trigger but not status,
	clear all interrupt with bit 6 */
	if (u4RegValue == 0) {
		PRINTK_AUDDRV("u4RegValue == 0 irqcount =0\n");
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 6, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 1, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 2, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 3, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 4, 0xff);
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << 5, 0xff);
		irqcount++;
		if (irqcount > AudioInterruptLimiter) {
			mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
			mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
			irqcount = 0;
		}
		goto AudDrv_IRQ_handler_exit;
	}
	if (u4RegValue & INTERRUPT_IRQ1_MCU) {
		if (audio_mem_if[Soc_Aud_Digital_Block_MEM_DL1]->state == true)
			afe_dl1_interrupt_handler();
	}
	if (u4RegValue & INTERRUPT_IRQ2_MCU) {
		if (audio_mem_if[Soc_Aud_Digital_Block_MEM_VUL]->state == true)
			afe_ul1_interrupt_handler();
		if (audio_mem_if[Soc_Aud_Digital_Block_MEM_AWB]->state == true)
			afe_awb_interrupt_handler();
		if (audio_mem_if[Soc_Aud_Digital_Block_MEM_DAI]->state == true)
			afe_dai_interrupt_handler();
	}
	if (u4RegValue & INTERRUPT_IRQ7_MCU) {
		if ((audio_mem_if[Soc_Aud_Digital_Block_MEM_DL2]->state == true)
		    && (mOffloadSWMode == true))
			afe_dl2_interrupt_handler();
	}
	if (u4RegValue & INTERRUPT_IRQ5_MCU) {
		if (audio_mem_if[Soc_Aud_Digital_Block_MEM_HDMI]->state == true)
			afe_hdmi_interrupt_handler();
	}
	/* clear irq */
	mt_afe_set_reg(AFE_IRQ_MCU_CLR, u4RegValue, 0xff);
AudDrv_IRQ_handler_exit:
	mt_afe_main_clk_off();
	return IRQ_HANDLED;
}


uint32_t GetApllbySampleRate(uint32_t SampleRate)
{
	if (SampleRate == 176400 || SampleRate == 88200 || SampleRate == 44100
	    || SampleRate == 22050 || SampleRate == 11025)
		return Soc_Aud_APLL1;
	else
		return Soc_Aud_APLL2;
}
static bool CheckMemIfEnable(void)
{
	int i = 0;

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++) {
		if ((audio_mem_if[i]->state) == true) {
			/* PRINTK_AUDDRV("CheckMemIfEnable == true\n"); */
			return true;
		}
	}
	/* PRINTK_AUDDRV("CheckMemIfEnable == false\n"); */
	return false;
}


/* record VOW status for AFE GPIO control */
void SetVOWStatus(bool bEnable)
{
	unsigned long flags;

	if (mVOWStatus != bEnable) {
		spin_lock_irqsave(&afe_control_lock, flags);
		mVOWStatus = bEnable;
		PRINTK_AUDDRV("SetVOWStatus, mVOWStatus= %d\n", mVOWStatus);
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

int mt_afe_platform_init(void *dev)
{
	struct device *pdev = dev;
	int ret = 0;
	unsigned int irq_id = 0;

	PRINTK_AUDDRV("%s ", __func__);
	if (!pdev->of_node) {
		pr_warn("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	irq_id = irq_of_parse_and_map(pdev->of_node, 0);
	if (irq_id)
		audio_irq_id = irq_id;
	else
		pr_warn("%s irq_of_parse_and_map invalid irq\n", __func__);
#if 0
	ret = of_property_read_u32(pdev->of_node, BOARD_CHANNEL_TYPE_PROPERTY,
				 &board_channel_type);
	if (ret) {
		pr_warn("%s read property %s fail in node %s\n", __func__,
			BOARD_CHANNEL_TYPE_PROPERTY, pdev->of_node->full_name);
	}
#endif
	ret = mt_afe_reg_remap(dev);
	if (ret)
		return ret;

	ret = mt_afe_init_clock(dev);
	if (ret)
		return ret;
#if 0
	pm_runtime_enable(dev);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_warn("%s pm_runtime_get_sync fail %d\n", __func__, ret);
		return ret;
	}
	audio_power_status = true;
#endif
	mt_afe_power_off_default_clock();

	mt_afe_register_irq(dev);

	mt_afe_apb_bus_init();

	/*mt_afe_init_control(dev);*/

	mach_dev = dev;



	return ret;
}

void mt_afe_platform_deinit(void *dev)
{
	mt_afe_reg_unmap();
#if 0
	if (audio_power_status) {
		pm_runtime_put_sync(dev);
		audio_power_status = false;
	}

	pm_runtime_disable(dev);
#endif
	mt_afe_deinit_clock(dev);
}

void mt_afe_enable_afe(bool bEnable) /*EnableAfe*/
{
	unsigned long flags;
	bool MemEnable = false;
#ifdef CONFIG_OF
#ifdef CONFIG_MTK_LEGACY

	int ret;

	ret = GetGPIO_Info(1, &pin_audclk, &pin_mode_audclk);
	if (ret < 0) {
		pr_err("mt_afe_enable_afe GetGPIO_Info FAIL1!!!\n");
		return;
	}
	ret = GetGPIO_Info(2, &pin_audmiso, &pin_mode_audmiso);
	if (ret < 0) {
		pr_err("mt_afe_enable_afe GetGPIO_Info FAIL2!!!\n");
		return;
	}
	ret = GetGPIO_Info(3, &pin_audmosi, &pin_mode_audmosi);
	if (ret < 0) {
		pr_err("mt_afe_enable_afe GetGPIO_Info FAIL3!!!\n");
		return;
	}
#endif
#endif

	spin_lock_irqsave(&afe_control_lock, flags);
	MemEnable = CheckMemIfEnable();

	if (false == bEnable && false == MemEnable)
		mt_afe_set_reg(AFE_DAC_CON0, 0x0, 0x0);
	if (true == bEnable && true == MemEnable)
		mt_afe_set_reg(AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe_control_lock, flags);
}

bool mt_afe_set_sample_rate(uint32_t Aud_block, uint32_t SampleRate) /*SetSampleRate*/
{
	/* PRINTK_AUDDRV("%s Aud_block = %d SampleRate = %d\n", __func__, Aud_block, SampleRate); */
	SampleRate = mt_afe_rate_to_idx(SampleRate);
	switch (Aud_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			mt_afe_set_reg(AFE_DAC_CON1, SampleRate, 0x0000000f);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:{
			mt_afe_set_reg(AFE_DAC_CON1, SampleRate << 4, 0x000000f0);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:{
			mt_afe_set_reg(AFE_DAC_CON1, SampleRate << 8, 0x00000f00);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			mt_afe_set_reg(AFE_DAC_CON1, SampleRate << 12, 0x0000f000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			mt_afe_set_reg(AFE_DAC_CON1, SampleRate << 16, 0x000f0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				mt_afe_set_reg(AFE_DAC_CON1, 0 << 20, 1 << 20);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				mt_afe_set_reg(AFE_DAC_CON1, 1 << 20, 1 << 20);
			else
				return false;
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				mt_afe_set_reg(AFE_DAC_CON1, 0 << 30, 3 << 30);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				mt_afe_set_reg(AFE_DAC_CON1, 1 << 30, 3 << 30);
			else
				return false;
			break;
		}
		return true;
	}
	return false;
}

bool mt_afe_set_channels(uint32_t Memory_Interface, uint32_t channel)
{
	const bool bMono = (channel == 1) ? true : false;
	/* PRINTK_AUDDRV("mt_afe_set_channels Memory_Interface = %d channels = %d\n", Memory_Interface, channel); */
	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_DL1:{
			mt_afe_set_reg(AFE_DAC_CON1, bMono << 21, 1 << 21);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:{
			mt_afe_set_reg(AFE_DAC_CON1, bMono << 24, 1 << 24);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:{
			mt_afe_set_reg(AFE_DAC_CON1, bMono << 27, 1 << 27);
			break;
		}
	default:
		pr_warn("mt_afe_set_channels Memory_Interface = %d, channel = %d, bMono = %d\n",
			Memory_Interface, channel, bMono);
		return false;
	}
	return true;
}


bool Set2ndI2SOutAttribute(uint32_t sampleRate)
{
	PRINTK_AUDDRV("+%s(), sampleRate = %d\n", __func__, sampleRate);
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
	uint32_t u32AudioI2S = 0;

	memcpy((void *)m2ndI2Sout, (void *)DigtalI2S, sizeof(struct AudioDigtalI2S));
	u32AudioI2S = mt_afe_rate_to_idx(m2ndI2Sout->mI2S_SAMPLERATE) << 8;
	u32AudioI2S |= m2ndI2Sout->mLR_SWAP << 31;
	u32AudioI2S |= m2ndI2Sout->mI2S_HDEN << 12;
	u32AudioI2S |= m2ndI2Sout->mINV_LRCK << 5;
	u32AudioI2S |= m2ndI2Sout->mI2S_FMT << 3;
	u32AudioI2S |= m2ndI2Sout->mI2S_WLEN << 1;
	mt_afe_set_reg(AFE_I2S_CON3, u32AudioI2S, AFE_MASK_ALL);
	return true;
}

bool Set2ndI2SOutEnable(bool benable)
{
	if (benable)
		mt_afe_set_reg(AFE_I2S_CON3, 0x1, 0x1);
	else
		mt_afe_set_reg(AFE_I2S_CON3, 0x0, 0x1);
	return true;
}

bool SetDaiBt(struct AudioDigitalDAIBT *mAudioDaiBt)
{
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mUSE_MRGIF_INPUT = mAudioDaiBt->mUSE_MRGIF_INPUT;
	AudioDaiBt->mDAI_BT_MODE = mAudioDaiBt->mDAI_BT_MODE;
	AudioDaiBt->mDAI_DEL = mAudioDaiBt->mDAI_DEL;
	AudioDaiBt->mBT_LEN = mAudioDaiBt->mBT_LEN;
	AudioDaiBt->mDATA_RDY = mAudioDaiBt->mDATA_RDY;
	AudioDaiBt->mBT_SYNC = mAudioDaiBt->mBT_SYNC;
	return true;
}

bool SetDaiBtEnable(bool bEanble)
{
	PRINTK_AUDDRV("%s bEanble = %d\n", __func__, bEanble);
	if (bEanble == true) {	/* turn on dai bt */
		mt_afe_set_reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
		if (mAudioMrg->MrgIf_En == true) {
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);	/* use merge */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
		} else {	/* turn on merge and daiBT */
			/* set Mrg_I2S Samping Rate */
			mt_afe_set_reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);

			/* set Mrg_I2S enable */
			mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);

			/* Turn on Merge Interface */
			mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);

			udelay(100);

			/* use merge */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);

			/* data ready */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);

			/* Turn on DAIBT */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
		}
		AudioDaiBt->mBT_ON = true;
		AudioDaiBt->mDAIBT_ON = true;
		mAudioMrg->MrgIf_En = true;
	} else {
		if (mAudioMrg->Mergeif_I2S_Enable == true) {
			mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x3);	/* Turn off DAIBT */
		} else {
			mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x3);	/* Turn on DAIBT */
			udelay(100);
			mt_afe_set_reg(AFE_MRGIF_CON, 0 << 16, 1 << 16);	/* set Mrg_I2S enable */
			mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);	/* Turn on Merge Interface */
			mAudioMrg->MrgIf_En = false;
		}
		AudioDaiBt->mBT_ON = false;
		AudioDaiBt->mDAIBT_ON = false;
	}
	return true;
}

bool GetMrgI2SEnable(void)
{
	return audio_mem_if[Soc_Aud_Digital_Block_MRG_I2S_OUT]->state;
}

bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate)
{
	PRINTK_AUDDRV("%s bEnable = %d\n", __func__, bEnable);
	if (bEnable == true) {
		/* To enable MrgI2S */
		if (mAudioMrg->MrgIf_En == true) {
			/*
			Merge Interface already turn on
			if sample Rate change, then it need to restart with new setting
			else do nothing.
			*/
			if (mAudioMrg->Mrg_I2S_SampleRate != mt_afe_rate_to_idx(sampleRate)) {
				/* Turn off Merge Interface first to switch I2S sampling rate */

				mt_afe_set_reg(AFE_MRGIF_CON, 0, 1 << 16);	/* Turn off I2S */

				if (AudioDaiBt->mDAIBT_ON == true) {
					/* Turn off DAIBT first */
					mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x1);
				}

				udelay(100);
				mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);	/* Turn off Merge Interface */
				udelay(100);
				mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */

				if (AudioDaiBt->mDAIBT_ON == true) {
					/* use merge */
					mt_afe_set_reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
					mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);

					/* data ready */
					mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);

					/* Turn on DAIBT */
					mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
				}
				mAudioMrg->Mrg_I2S_SampleRate = mt_afe_rate_to_idx(sampleRate);
				mt_afe_set_reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);
				mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);	/* set Mrg_I2S enable */
			}
		} else {
			/* turn on merge Interface from off state */
			mAudioMrg->Mrg_I2S_SampleRate = mt_afe_rate_to_idx(sampleRate);
			mt_afe_set_reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000);
			mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);	/* set Mrg_I2S enable */
			udelay(100);
			mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */
			udelay(100);
			if (AudioDaiBt->mDAIBT_ON == true) {
				/* use merge */
				mt_afe_set_reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
				mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);

				mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);	/* data ready */
				mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);	/* Turn on DAIBT */
			}
		}
		mAudioMrg->MrgIf_En = true;
		mAudioMrg->Mergeif_I2S_Enable = true;
	} else {
		if (mAudioMrg->MrgIf_En == true) {
			mt_afe_set_reg(AFE_MRGIF_CON, 0, 1 << 16);	/* Turn off I2S */
			if (AudioDaiBt->mDAIBT_ON == false) {
				udelay(100);
				/* DAIBT also not using, then it's OK to disable Merge Interface */
				mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);
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
	uint32_t Audio_I2S_Adc = 0;

	memcpy((void *)AudioAdcI2S, (void *)DigtalI2S, sizeof(struct AudioDigtalI2S));
	if (false == AudioAdcI2SStatus) {
		uint32_t eSamplingRate = mt_afe_rate_to_idx(AudioAdcI2S->mI2S_SAMPLERATE);
		uint32_t dVoiceModeSelect = 0;

		mt_afe_set_reg(AFE_ADDA_TOP_CON0, 0, 0x1);	/* Using Internal ADC */

		if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
			dVoiceModeSelect = 0;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
			dVoiceModeSelect = 1;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
			dVoiceModeSelect = 2;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_48K)
			dVoiceModeSelect = 3;

		mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, (dVoiceModeSelect << 19) | (dVoiceModeSelect << 17),
				0x001E0000);
		mt_afe_set_reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF);	/* up8x txif sat on */
		mt_afe_set_reg(AFE_ADDA_NEWIF_CFG1, ((dVoiceModeSelect < 3) ? 1 : 3) << 10,
			    0x00000C00);
	} else {
		mt_afe_set_reg(AFE_ADDA_TOP_CON0, 1, 0x1);	/* Using External ADC */
		Audio_I2S_Adc |= (AudioAdcI2S->mLR_SWAP << 31);
		Audio_I2S_Adc |= (AudioAdcI2S->mBuffer_Update_word << 24);
		Audio_I2S_Adc |= (AudioAdcI2S->mINV_LRCK << 23);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit_test << 22);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit << 21);
		Audio_I2S_Adc |= (AudioAdcI2S->mloopback << 20);
		Audio_I2S_Adc |= (mt_afe_rate_to_idx(AudioAdcI2S->mI2S_SAMPLERATE) << 8);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_FMT << 3);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_WLEN << 1);
		PRINTK_AUDDRV("%s Audio_I2S_Adc = 0x%x\n", __func__, Audio_I2S_Adc);
		mt_afe_set_reg(AFE_I2S_CON2, Audio_I2S_Adc, MASK_ALL);
	}
	return true;
}

bool mt_afe_enable_sinegen_hw(uint32_t connection, bool direction)
{
	PRINTK_AUDDRV("+%s(), connection = %d, direction = %d\n", __func__,
		 connection, direction);
	if (direction) {
		switch (connection) {
		case Soc_Aud_InterConnectionInput_I00:
		case Soc_Aud_InterConnectionInput_I01:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x04662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I02:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x14662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I03:
		case Soc_Aud_InterConnectionInput_I04:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x24662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I05:
		case Soc_Aud_InterConnectionInput_I06:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x34662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I07:
		case Soc_Aud_InterConnectionInput_I08:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x44662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I09:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x54662662, 0xffffffff);
		case Soc_Aud_InterConnectionInput_I10:
		case Soc_Aud_InterConnectionInput_I11:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x64662662, 0xffffffff);
			break;
/*			 YC_TO_DO
		case Soc_Aud_InterConnectionInput_I12:
		case Soc_Aud_InterConnectionInput_I13:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x746C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I14:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x846C2662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I15:
		case Soc_Aud_InterConnectionInput_I16:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x946C2662, 0xffffffff);
			break;*/
		}
	} else {
		switch (connection) {
		case Soc_Aud_InterConnectionOutput_O00:
		case Soc_Aud_InterConnectionOutput_O01:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x746c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O02:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x846c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O03:
		case Soc_Aud_InterConnectionOutput_O04:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x946c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O05:
		case Soc_Aud_InterConnectionOutput_O06:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xa46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O07:
		case Soc_Aud_InterConnectionOutput_O08:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xb46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O09:
		case Soc_Aud_InterConnectionOutput_O10:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xc46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O11:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xd46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O12:
			if (Soc_Aud_I2S_SAMPLERATE_I2S_8K ==
			    audio_mem_if[Soc_Aud_Digital_Block_MEM_MOD_DAI]->sample_rate)
				mt_afe_set_reg(AFE_SGEN_CON0, 0xe40e80e8, 0xffffffff);
			else if (Soc_Aud_I2S_SAMPLERATE_I2S_16K ==
				 audio_mem_if[Soc_Aud_Digital_Block_MEM_MOD_DAI]->sample_rate)
				mt_afe_set_reg(AFE_SGEN_CON0, 0xe40f00f0, 0xffffffff);
			else {
				mt_afe_set_reg(AFE_SGEN_CON0, 0xe46c26c2, 0xffffffff);	/* Default */
			}
			break;
		default:
			break;
		}
	}
	return true;
}

int mt_afe_disable_sinegen_hw(void)
{
	/* don't set [31:28] as 0 when disable sinetone HW */
	/* because it will repalce i00/i01 input with sine gen output. */
	/* Set 0xf is correct way to disconnect sinetone HW to any I/O. */
	mt_afe_set_reg(AFE_SGEN_CON0, 0xf0000000, 0xffffffff);
	return 0;
}

bool SetSideGenSampleRate(uint32_t SampleRate)
{
	uint32_t sine_mode_ch1 = 0;
	uint32_t sine_mode_ch2 = 0;

	PRINTK_AUDDRV("+%s(), SampleRate = %d\n", __func__, SampleRate);
	sine_mode_ch1 = mt_afe_sinegen_rate_to_idx(SampleRate) << 8;
	sine_mode_ch2 = mt_afe_sinegen_rate_to_idx(SampleRate) << 20;
	mt_afe_set_reg(AFE_SGEN_CON0, sine_mode_ch1, 0x00000f00);
	mt_afe_set_reg(AFE_SGEN_CON0, sine_mode_ch2, 0x00f00000);
	return true;
}

bool Set2ndI2SAdcEnable(bool bEnable)
{
	/* todo? */
	return true;
}

bool SetI2SAdcEnable(bool bEnable)
{
	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, bEnable ? 1 : 0, 0x01);
	audio_mem_if[Soc_Aud_Digital_Block_I2S_IN_ADC]->state = bEnable;
	if (bEnable == true)
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
	else if (audio_mem_if[Soc_Aud_Digital_Block_I2S_OUT_DAC]->state == false &&
		 audio_mem_if[Soc_Aud_Digital_Block_I2S_IN_ADC]->state == false)
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
	return true;
}

bool Set2ndI2SEnable(bool bEnable)
{
	mt_afe_set_reg(AFE_I2S_CON, bEnable, 0x1);
	return true;
}

bool CleanPreDistortion(void)
{
	PRINTK_AUDDRV("%s\n", __func__);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
	return true;
}

bool SetDLSrc2(uint32_t SampleRate)
{
	uint32_t AfeAddaDLSrc2Con0, AfeAddaDLSrc2Con1;

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

	if (AfeAddaDLSrc2Con0 == 0 || AfeAddaDLSrc2Con0 == 3) {	/* 8k or 16k voice mode */
		AfeAddaDLSrc2Con0 =
			(AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11) | (0x01 << 5);
	} else
		AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11);

	/* SA suggest apply -0.3db to audio/speech path */
	AfeAddaDLSrc2Con0 = AfeAddaDLSrc2Con0 | (0x01 << 1);	/* for voice mode degrade 0.3db */
	AfeAddaDLSrc2Con1 = 0xf74f0000;
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, AfeAddaDLSrc2Con0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON1, AfeAddaDLSrc2Con1, MASK_ALL);
	return true;
}

bool SetI2SDacOut(uint32_t SampleRate, bool lowjitter, bool I2SWLen)
{
	uint32_t Audio_I2S_Dac = 0;

	PRINTK_AUDDRV("SetI2SDacOut SampleRate %d, lowjitter %d, I2SWLen %d\n", SampleRate,
		 lowjitter, I2SWLen);
	CleanPreDistortion();
	SetDLSrc2(SampleRate);
	Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	Audio_I2S_Dac |= (mt_afe_rate_to_idx(SampleRate) << 8);
	Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	Audio_I2S_Dac |= (I2SWLen << 1);
	Audio_I2S_Dac |= (lowjitter << 12);	/* low gitter mode */
	mt_afe_set_reg(AFE_I2S_CON1, Audio_I2S_Dac, MASK_ALL);
	return true;
}

bool SetHwDigitalGainMode(uint32_t GainType, uint32_t SampleRate, uint32_t SamplePerStep)
{
	uint32_t value = 0;

	PRINTK_AUDDRV("SetHwDigitalGainMode GainType = %d, SampleRate = %d, SamplePerStep= %d\n",
		 GainType, SampleRate, SamplePerStep);

	value = (SamplePerStep << 8) | (mt_afe_rate_to_idx(SampleRate) << 4);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		mt_afe_set_reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		mt_afe_set_reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		return false;
	}
	return true;
}

bool SetHwDigitalGainEnable(int GainType, bool Enable)
{
	PRINTK_AUDDRV("+%s(), GainType = %d, Enable = %d\n", __func__, GainType, Enable);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			mt_afe_set_reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);
		}
		mt_afe_set_reg(AFE_GAIN1_CON0, Enable, 0x1);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			mt_afe_set_reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);
		}
		mt_afe_set_reg(AFE_GAIN2_CON0, Enable, 0x1);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetHwDigitalGain(uint32_t Gain, int GainType)
{
	PRINTK_AUDDRV("+%s(), Gain = 0x%x, gain type = %d\n", __func__, Gain, GainType);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		mt_afe_set_reg(AFE_GAIN1_CON1, Gain, 0xffffffff);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		mt_afe_set_reg(AFE_GAIN2_CON1, Gain, 0xffffffff);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetModemPcmConfig(int modem_index, struct AudioDigitalPCM p_modem_pcm_attribute)
{
	uint32_t reg_pcm2_intf_con = 0;
	uint32_t reg_pcm_intf_con1 = 0;

	PRINTK_AUDDRV("+%s()\n", __func__);
	if (modem_index == MODEM_1) {
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x1) << 13;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x1) << 12;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mSingelMicSel & 0x1) << 7;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mAsyncFifoSel & 0x1) << 6;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmWordLength & 0x1) << 5;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x3) << 3;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmFormat & 0x3) << 1;
		PRINTK_AUDDRV("%s(), PCM2_INTF_CON(0x%lx) = 0x%x\n", __func__, PCM2_INTF_CON,
			 reg_pcm2_intf_con);
		mt_afe_set_reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
		if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			mt_afe_set_reg(AFE_ASRC2_CON1, 0x00098580, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON4, 0x00098580, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON7, 0x0004c2c0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON1, 0x00098580, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON4, 0x00098580, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON7, 0x0004c2c0, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			mt_afe_set_reg(AFE_ASRC2_CON1, 0x0004c2c0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON4, 0x0004c2c0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON7, 0x00026160, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON1, 0x0004c2c0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON4, 0x0004c2c0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON7, 0x00026160, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
			mt_afe_set_reg(AFE_ASRC2_CON1, 0x00026160, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON4, 0x00026160, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC2_CON7, 0x000130b0, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON1, 0x00026160, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON4, 0x00026160, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC3_CON7, 0x000130b0, 0xffffffff);
		}
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {	/* MODEM_2 use PCM_INTF_CON1 (0x530) */
		if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			mt_afe_set_reg(AFE_ASRC_CON1, 0x00065900, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON4, 0x00065900, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON7, 0x00032C80, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON1, 0x00065900, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON4, 0x00065900, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON7, 0x00032C80, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			mt_afe_set_reg(AFE_ASRC_CON1, 0x00032C80, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON4, 0x00032C80, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON7, 0x00019640, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON1, 0x00032C80, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON4, 0x00032C80, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON7, 0x00019640, 0xffffffff);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_32K) {
			mt_afe_set_reg(AFE_ASRC_CON1, 0x00019640, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON4, 0x00019640, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC_CON7, 0x0000CB20, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON1, 0x00019640, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON4, 0x00019640, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON6, 0x007F188F, 0xffffffff);
			mt_afe_set_reg(AFE_ASRC4_CON7, 0x0000CB20, 0xffffffff);
		}
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mBclkOutInv & 0x01) << 22;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x01) << 19;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x01) << 18;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtModemSel & 0x01) << 17;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncLength & 0x1F) << 9;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01) << 8;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSingelMicSel & 0x01) << 7;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mAsyncFifoSel & 0x01) << 6;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSlaveModeSel & 0x01) << 5;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x03) << 3;
		reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmFormat & 0x03) << 1;
		PRINTK_AUDDRV("%s(), PCM_INTF_CON1(0x%lx) = 0x%x\n", __func__, PCM_INTF_CON1,
			 reg_pcm_intf_con1);
		mt_afe_set_reg(PCM_INTF_CON1, reg_pcm_intf_con1, MASK_ALL);
	}
	return true;
}

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on)
{
	uint32_t dNeedDisableASM = 0, mPcm1AsyncFifo;

	PRINTK_AUDDRV("+%s(), modem_index = %d, modem_pcm_on = %d\n", __func__,
		 modem_index, modem_pcm_on);
	if (modem_index == MODEM_1) {	/* MODEM_1 use PCM2_INTF_CON (0x53C) */
		/* todo:: temp for use fifo */
		mt_afe_set_reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
		audio_mem_if[Soc_Aud_Digital_Block_MODEM_PCM_1_O]->state = modem_pcm_on;
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {	/* MODEM_2 use PCM_INTF_CON1 (0x530) */
		if (modem_pcm_on == true) {	/* turn on ASRC before Modem PCM on */
			/* selects internal MD2/MD3 PCM interface (0x538[8]) */
			mt_afe_set_reg(PCM_INTF_CON2, (modem_index - 1) << 8, 0x100);
			mPcm1AsyncFifo = (mt_afe_get_reg(PCM_INTF_CON1) & 0x0040) >> 6;
			if (mPcm1AsyncFifo == 0) {
				mt_afe_set_reg(AFE_ASRC_CON0, 0x86083031, MASK_ALL);
				mt_afe_set_reg(AFE_ASRC4_CON0, 0x06003031, MASK_ALL);
			}
			mt_afe_set_reg(PCM_INTF_CON1, 0x1, 0x1);
		} else if (modem_pcm_on == false) {	/* turn off ASRC after Modem PCM off */
			mt_afe_set_reg(PCM_INTF_CON1, 0x0, 0x1);
			mt_afe_set_reg(AFE_ASRC_CON6, 0x00000000, MASK_ALL);
			dNeedDisableASM = (mt_afe_get_reg(AFE_ASRC_CON0) & 0x1) ? 1 : 0;
			mt_afe_set_reg(AFE_ASRC_CON0, 0, (1 << 4 | 1 << 5 | dNeedDisableASM));
			mt_afe_set_reg(AFE_ASRC_CON0, 0x0, 0x1);
			mt_afe_set_reg(AFE_ASRC4_CON6, 0x00000000, MASK_ALL);
			mt_afe_set_reg(AFE_ASRC4_CON0, 0, (1 << 4 | 1 << 5));
			mt_afe_set_reg(AFE_ASRC4_CON0, 0x0, 0x1);
		}
		audio_mem_if[Soc_Aud_Digital_Block_MODEM_PCM_2_O]->state = modem_pcm_on;
	} else {
		pr_warn("%s(), no such modem_index: %d!!\n", __func__, modem_index);
		return false;
	}
	return true;
}


bool EnableSideToneFilter(bool stf_on)
{
	/* MD max support 16K sampling rate */
	const uint8_t kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable16k) / sizeof(uint16_t);

	PRINTK_AUDDRV("+%s(), stf_on = %d\n", __func__, stf_on);
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	if (stf_on == false) {
		/* bypass STF result & disable */
		const bool bypass_stf_on = true;
		uint32_t reg_value = (bypass_stf_on << 31) | (stf_on << 8);

		mt_afe_set_reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
		PRINTK_AUDDRV("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __func__, AFE_SIDETONE_CON1,
			 reg_value);
		/* set side tone gain = 0 */
		mt_afe_set_reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		PRINTK_AUDDRV("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __func__, AFE_SIDETONE_GAIN, 0);
	} else {
		const bool bypass_stf_on = false;
		/* using STF result & enable & set half tap num */
		uint32_t write_reg_value =
		    (bypass_stf_on << 31) | (stf_on << 8) | kSideToneHalfTapNum;
		/* set side tone coefficient */
		const bool enable_read_write = true;	/* enable read/write side tone coefficient */
		const bool read_write_sel = true;	/* for write case */
		const bool sel_ch2 = false;	/* using uplink ch1 as STF input */
		uint32_t read_reg_value = mt_afe_get_reg(AFE_SIDETONE_CON0);
		size_t coef_addr = 0;

		PRINTK_AUDDRV("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __func__, AFE_SIDETONE_GAIN, 0);
		/* set side tone gain */
		mt_afe_set_reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		mt_afe_set_reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
		PRINTK_AUDDRV("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __func__,
			 AFE_SIDETONE_CON1, write_reg_value);
		for (coef_addr = 0; coef_addr < kSideToneHalfTapNum; coef_addr++) {
			bool old_write_ready = (read_reg_value >> 29) & 0x1;
			bool new_write_ready = 0;
			int try_cnt = 0;

			write_reg_value = enable_read_write << 25 |
			    read_write_sel << 24 |
			    sel_ch2 << 23 |
			    coef_addr << 16 | kSideToneCoefficientTable16k[coef_addr];
			mt_afe_set_reg(AFE_SIDETONE_CON0, write_reg_value, 0x39FFFFF);
			PRINTK_AUDDRV("%s(), AFE_SIDETONE_CON0[0x%lx] = 0x%x\n", __func__,
				 AFE_SIDETONE_CON0, write_reg_value);
			/* wait until flag write_ready changed (means write done) */
			for (try_cnt = 0; try_cnt < 10; try_cnt++) {	/* max try 10 times */
				/* msleep(3); */
				/* usleep_range(3 * 1000, 20 * 1000); */
				read_reg_value = mt_afe_get_reg(AFE_SIDETONE_CON0);
				new_write_ready = (read_reg_value >> 29) & 0x1;
				if (new_write_ready == old_write_ready)	{
					udelay(3);
					if (try_cnt == 10) {
						BUG_ON(new_write_ready != old_write_ready);
						return false;
					}
				} else /* flip => ok */
					break;
			}
		}
	}
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	PRINTK_AUDDRV("-%s(), stf_on = %d\n", __func__, stf_on);
	return true;
}


uint32_t mt_afe_rate_to_idx(uint32_t SampleRate)
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
	default:
		break;
	}
	return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
}


uint32_t mt_afe_sinegen_rate_to_idx(uint32_t SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_8K;
	case 11025:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_11K;
	case 12000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_12K;
	case 16000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_16K;
	case 22050:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_22K;
	case 24000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_24K;
	case 32000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_32K;
	case 44100:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_44K;
	case 48000:
		return Soc_Aud_SINEGEN_SAMPLERATE_I2S_48K;
	default:
		break;
	}
	return Soc_Aud_SINEGEN_SAMPLERATE_I2S_44K;
}


int mt_afe_enable_memory_path(uint32_t block) /*SetMemoryPathEnable*/
{
	unsigned long flags;

	if (block >= Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return -EINVAL;

	spin_lock_irqsave(&afe_control_lock, flags);

	if (audio_mem_if[block]->user_count == 0)
		audio_mem_if[block]->state = true;

	audio_mem_if[block]->user_count++;

	if (block < Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE)
		mt_afe_set_reg(AFE_DAC_CON0, 1 << (block + 1), 1 << (block + 1));

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return 0;
}

int mt_afe_disable_memory_path(uint32_t block) /*SetMemoryPathEnable*/
{
	unsigned long flags;

	if (block >= Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		return -EINVAL;

	spin_lock_irqsave(&afe_control_lock, flags);

	audio_mem_if[block]->user_count--;
	if (audio_mem_if[block]->user_count == 0)
		audio_mem_if[block]->state = false;

	if (audio_mem_if[block]->user_count < 0) {
		pr_warn("%s block %u user count %d < 0\n",
			__func__, block, audio_mem_if[block]->user_count);
		audio_mem_if[block]->user_count = 0;
	}

	if (block < Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE)
		mt_afe_set_reg(AFE_DAC_CON0, 0, 1 << (block + 1));

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return 0;
}
bool mt_afe_get_memory_path_state(uint32_t block)
{
	unsigned long flags;
	bool state = false;

	spin_lock_irqsave(&afe_control_lock, flags);

	if (block < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
		state = audio_mem_if[block]->state;

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return state;
}

static void mt_afe_clean_predistortion(void)
{
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
}

static bool mt_afe_set_dl_src2(uint32_t sample_rate)
{
	uint32_t afe_adda_dl_src2_con0, afe_adda_dl_src2_con1;

	if (likely(sample_rate == 44100))
		afe_adda_dl_src2_con0 = 7;
	else if (sample_rate == 8000)
		afe_adda_dl_src2_con0 = 0;
	else if (sample_rate == 11025)
		afe_adda_dl_src2_con0 = 1;
	else if (sample_rate == 12000)
		afe_adda_dl_src2_con0 = 2;
	else if (sample_rate == 16000)
		afe_adda_dl_src2_con0 = 3;
	else if (sample_rate == 22050)
		afe_adda_dl_src2_con0 = 4;
	else if (sample_rate == 24000)
		afe_adda_dl_src2_con0 = 5;
	else if (sample_rate == 32000)
		afe_adda_dl_src2_con0 = 6;
	else if (sample_rate == 48000)
		afe_adda_dl_src2_con0 = 8;
	else
		afe_adda_dl_src2_con0 = 7;	/* Default 44100 */

	/* ASSERT(0); */
	if (afe_adda_dl_src2_con0 == 0 || afe_adda_dl_src2_con0 == 3) {	/* 8k or 16k voice mode */
		afe_adda_dl_src2_con0 =
		    (afe_adda_dl_src2_con0 << 28) | (0x03 << 24) | (0x03 << 11) | (0x01 << 5);
	} else {
		afe_adda_dl_src2_con0 = (afe_adda_dl_src2_con0 << 28) | (0x03 << 24) | (0x03 << 11);
	}
	/* SA suggest apply -0.3db to audio/speech path */
	/* 2013.02.22 for voice mode degrade 0.3 db */
	afe_adda_dl_src2_con0 = afe_adda_dl_src2_con0 | (0x01 << 1);
	afe_adda_dl_src2_con1 = 0xf74f0000;

	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, afe_adda_dl_src2_con0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON1, afe_adda_dl_src2_con1, MASK_ALL);
	return true;
}

void mt_afe_set_i2s_dac_out(uint32_t sample_rate) /*SetI2SDacOut*/
{
	uint32_t audio_i2s_dac = 0;

	mt_afe_clean_predistortion();
	mt_afe_set_dl_src2(sample_rate);

	audio_i2s_dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	audio_i2s_dac |= (mt_afe_rate_to_idx(sample_rate) << 8);
	audio_i2s_dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	audio_i2s_dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	audio_i2s_dac |= (Soc_Aud_I2S_WLEN_WLEN_16BITS << 1);
	mt_afe_set_reg(AFE_I2S_CON1, audio_i2s_dac, MASK_ALL);

}


int mt_afe_enable_i2s_dac(void) /*SetI2SDacEnable(true)*/
{
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
	mt_afe_set_reg(AFE_I2S_CON1, 0x1, 0x1);
	mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
	/* For FPGA Pin the same with DAC */
	/* mt_afe_set_reg(FPGA_CFG1, 0, 0x10); */
	return 0;
}

int mt_afe_disable_i2s_dac(void) /*SetI2SDacEnable(false)*/
{
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, 0x0, 0x01);
	mt_afe_set_reg(AFE_I2S_CON1, 0x0, 0x1);

	if (!audio_mem_if[Soc_Aud_Digital_Block_I2S_OUT_DAC]->state &&
	    !audio_mem_if[Soc_Aud_Digital_Block_I2S_IN_ADC]->state) {
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
	}
	/* For FPGA Pin the same with DAC */
	/* mt_afe_set_reg(FPGA_CFG1, 1 << 4, 0x10); */
	return 0;
}

int mt_afe_enable_i2s_adc(void)
{
	if (!audio_adc_i2s_status) {
		mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
	} else {
		mt_afe_set_reg(AFE_I2S_CON2, 0x1, 0x1);
	}
	return 0;
}

int mt_afe_disable_i2s_adc(void)
{
	if (!audio_adc_i2s_status) {
		mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, 0x0, 0x1);
		if (audio_mem_if[Soc_Aud_Digital_Block_I2S_OUT_DAC]->state == false &&
		    audio_mem_if[Soc_Aud_Digital_Block_I2S_IN_ADC]->state == false) {
			mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
		}
	} else {
		mt_afe_set_reg(AFE_I2S_CON2, 0x0, 0x1);
	}
	return 0;
}


void mt_afe_set_2nd_i2s_out(uint32_t sample_rate, uint32_t clock_mode)
{
	uint32_t reg_value = 0;

	reg_value |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	reg_value |= (mt_afe_rate_to_idx(sample_rate) << 8);
	reg_value |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	reg_value |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	reg_value |= (Soc_Aud_I2S_WLEN_WLEN_16BITS << 1);
	mt_afe_set_reg(AFE_I2S_CON3, reg_value, 0xFFFFFFFE);
}


int mt_afe_enable_2nd_i2s_out(void)
{
	mt_afe_set_reg(AFE_I2S_CON3, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_2nd_i2s_out(void)
{
	mt_afe_set_reg(AFE_I2S_CON3, 0x0, 0x1);
	return 0;
}

void mt_afe_set_2nd_i2s_in(struct AudioDigtalI2S *mDigitalI2S) /*Set2ndI2SIn*/
{
	uint32_t reg_value = 0;

	memcpy((void *)m2ndI2S, (void *)mDigitalI2S, sizeof(struct AudioDigtalI2S));
	if (!m2ndI2S->mI2S_SLAVE)	/* Master setting SampleRate only */
		mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_I2S, m2ndI2S->mI2S_SAMPLERATE);

	reg_value |= (1 << 31);	/* enable phase_shift_fix for better quality */
	reg_value |= (m2ndI2S->mI2S_IN_PAD_SEL << 28);
	reg_value |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	reg_value |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	reg_value |= (m2ndI2S->mI2S_SLAVE << 2);
	reg_value |= (m2ndI2S->mI2S_WLEN << 1);
	mt_afe_set_reg(AFE_I2S_CON, reg_value, 0xFFFFFFFE);
	if (!m2ndI2S->mI2S_SLAVE)
		mt_afe_set_reg(FPGA_CFG1, 1 << 8, 0x0100);
	else
		mt_afe_set_reg(FPGA_CFG1, 0, 0x0100);
}

int mt_afe_enable_2nd_i2s_in(void)/*Set2ndI2SInEnable(false)*/
{
	mt_afe_set_reg(AFE_I2S_CON, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_2nd_i2s_in(void)/*Set2ndI2SInEnable(false)*/
{
	mt_afe_set_reg(AFE_I2S_CON, 0x0, 0x1);
	return 0;
}

bool checkUplinkMEMIfStatus(void)
{
	int i = 0;

	for (i = Soc_Aud_Digital_Block_MEM_VUL; i <= Soc_Aud_Digital_Block_MEM_MOD_DAI; i++) {
		if (audio_mem_if[i]->state == true)
			return true;
	}
	return false;
}

bool SetHDMIChannels(uint32_t Channels)
{
	unsigned int register_value = 0;

	register_value |= (Channels << 4);
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, register_value, 0x000000F0);
	return true;
}

bool SetHDMIEnable(bool bEnable)
{
	if (bEnable)
		mt_afe_set_reg(AFE_HDMI_OUT_CON0, 0x1, 0x1);
	else
		mt_afe_set_reg(AFE_HDMI_OUT_CON0, 0x0, 0x1);
	return true;
}

void SetHdmiPcmInterConnection(unsigned int connection_state, unsigned int channels)
{
	/* O20~O27: L/R/LS/RS/C/LFE/CH7/CH8 */
	switch (channels) {
	case 8:
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I26,
			Soc_Aud_InterConnectionOutput_O26);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I27,
			Soc_Aud_InterConnectionOutput_O27);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I24,
			Soc_Aud_InterConnectionOutput_O22);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I25,
			Soc_Aud_InterConnectionOutput_O23);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I22,
			Soc_Aud_InterConnectionOutput_O24);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I23,
			Soc_Aud_InterConnectionOutput_O25);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I20,
			Soc_Aud_InterConnectionOutput_O20);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I21,
			Soc_Aud_InterConnectionOutput_O21);
		break;
	case 6:
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I24,
			Soc_Aud_InterConnectionOutput_O22);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I25,
			Soc_Aud_InterConnectionOutput_O23);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I22,
			Soc_Aud_InterConnectionOutput_O24);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I23,
			Soc_Aud_InterConnectionOutput_O25);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I20,
			Soc_Aud_InterConnectionOutput_O20);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I21,
			Soc_Aud_InterConnectionOutput_O21);
		break;
	case 4:
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I22,
			Soc_Aud_InterConnectionOutput_O24);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I23,
			Soc_Aud_InterConnectionOutput_O25);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I20,
			Soc_Aud_InterConnectionOutput_O20);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I21,
			Soc_Aud_InterConnectionOutput_O21);
		break;
	case 2:
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I20,
			Soc_Aud_InterConnectionOutput_O20);
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I21,
			Soc_Aud_InterConnectionOutput_O21);
		break;
	case 1:
		mt_afe_set_hdmi_connection(connection_state, Soc_Aud_InterConnectionInput_I20,
			Soc_Aud_InterConnectionOutput_O20);
		break;

	default:
		pr_warn("%s unsupported channels %u\n", __func__, channels);
		break;
	}

}

bool SetHDMIConnection(uint32_t ConnectionState, uint32_t Input, uint32_t Output)
{
	if (ConnectionState)
		mt_afe_set_reg(AFE_HDMI_CONN0, (Input << (3 * Input)), (0x7 << (3 * Output)));
	else
		mt_afe_set_reg(AFE_HDMI_CONN0, 0x0, 0xFFFFFFFF);
	return true;
}

bool mt_afe_set_irq_state(uint32_t Irqmode, bool bEnable)
{
	PRINTK_AUDDRV("+%s(), Irqmode = %d, bEnable = %d\n", __func__, Irqmode, bEnable);
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			if (checkUplinkMEMIfStatus() == false)
				mt_afe_set_reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (bEnable << 12), (1 << 12));
			break;
		}
	default:
		break;
	}
	/* PRINTK_AUDDRV("-%s(), Irqmode = %d, bEnable = %d\n", __FUNCTION__, Irqmode, bEnable); */
	return true;
}

bool mt_afe_set_irq_rate(uint32_t Irqmode, uint32_t SampleRate)
{
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(SampleRate) << 4),
				    0x000000f0);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(SampleRate) << 8),
				    0x00000f00);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(SampleRate) << 16),
				    0x000f0000);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(SampleRate) << 24),
				    0x0f000000);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool mt_afe_set_irq_counter(uint32_t Irqmode, uint32_t Counter)
{
	uint32_t CurrentCount = 0;

	PRINTK_AUDDRV(" %s Irqmode = %d Counter = %d\n", __func__, Irqmode, Counter);
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CNT1, Counter, 0xffffffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:{
			CurrentCount = mt_afe_get_reg(AFE_IRQ_MCU_CNT2);
			if (CurrentCount == 0)
				mt_afe_set_reg(AFE_IRQ_MCU_CNT2, Counter, 0xffffffff);
			else if (Counter < CurrentCount) {
				PRINTK_AUDDRV("update counter latency CurrentCount = %d Counter = %d\n",
					 CurrentCount, Counter);
				mt_afe_set_reg(AFE_IRQ_MCU_CNT2, Counter, 0xffffffff);
			} else {
				PRINTK_AUDDRV
				    ("not to add counter latency CurrentCount = %d Counter = %d\n",
				     CurrentCount, Counter);
			}
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:{
			mt_afe_set_reg(AFE_IRQ_MCU_CNT1, Counter << 20, 0xfff00000);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:{
			/* ox3BC [0~17] , ex 24bit , stereo, 48BCKs @CNT */
			mt_afe_set_reg(AFE_IRQ_MCU_CNT5, Counter, 0x0003ffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE:{
			/* ox3BC [0~17] , ex 24bit , stereo, 48BCKs @CNT */
			mt_afe_set_reg(AFE_IRQ_MCU_CNT7, Counter, 0xffffffff);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool SetMemDuplicateWrite(uint32_t InterfaceType, int dupwrite)
{
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DAI:{
			mt_afe_set_reg(AFE_DAC_CON1, dupwrite << 29, 1 << 29);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:{
			mt_afe_set_reg(AFE_DAC_CON1, dupwrite << 31, 1 << 31);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S)
{
	uint32_t Audio_I2S_Adc = 0;

	memcpy((void *)m2ndI2S, (void *)mDigitalI2S, sizeof(struct AudioDigtalI2S));
	if (!m2ndI2S->mI2S_SLAVE)	/* Master setting SampleRate only */
		mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_I2S, m2ndI2S->mI2S_SAMPLERATE);
	Audio_I2S_Adc |= (m2ndI2S->mINV_LRCK << 5);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_FMT << 3);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_SLAVE << 2);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_WLEN << 1);
	Audio_I2S_Adc |= (m2ndI2S->mI2S_IN_PAD_SEL << 28);
	Audio_I2S_Adc |= 1 << 31;	/* Default enable phase_shift_fix for better quality */
	PRINTK_AUDDRV("Set2ndI2SIn Audio_I2S_Adc= 0x%x\n", Audio_I2S_Adc);
	mt_afe_set_reg(AFE_I2S_CON, Audio_I2S_Adc, 0xfffffffe);
	if (!m2ndI2S->mI2S_SLAVE)
		mt_afe_set_reg(FPGA_CFG1, 1 << 8, 0x0100);
	else
		mt_afe_set_reg(FPGA_CFG1, 0, 0x0100);
	return true;
}

bool Set2ndI2SInEnable(bool bEnable)
{
	PRINTK_AUDDRV("Set2ndI2SInEnable bEnable = %d\n", bEnable);
	m2ndI2S->mI2S_EN = bEnable;
	mt_afe_set_reg(AFE_I2S_CON, bEnable, 0x1);
	audio_mem_if[Soc_Aud_Digital_Block_I2S_IN_2]->state = bEnable;
	return true;
}

bool mt_afe_set_i2s_asrc_config(bool bIsUseASRC, unsigned int dToSampleRate)/*SetI2SASRCConfig*/
{
	PRINTK_AUDDRV("+%s() bIsUseASRC [%d] dToSampleRate [%d]\n", __func__, bIsUseASRC, dToSampleRate);
	if (true == bIsUseASRC) {
		BUG_ON(!(dToSampleRate == 44100 || dToSampleRate == 48000));
		mt_afe_set_reg(AFE_CONN4, 0, 1 << 30);
		/* To target sample rate */
		mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_I2S, dToSampleRate);
		mt_afe_set_reg(AFE_ASRC_CON13, 0, 1 << 16);	/* 0:Stereo 1:Mono */
		if (dToSampleRate == 44100) {
			mt_afe_set_reg(AFE_ASRC_CON14, 0xDC8000, AFE_MASK_ALL);
			mt_afe_set_reg(AFE_ASRC_CON15, 0xA00000, AFE_MASK_ALL);
			mt_afe_set_reg(AFE_ASRC_CON17, 0x1FBD, AFE_MASK_ALL);
		} else {
			mt_afe_set_reg(AFE_ASRC_CON14, 0x600000, AFE_MASK_ALL);
			mt_afe_set_reg(AFE_ASRC_CON15, 0x400000, AFE_MASK_ALL);
			mt_afe_set_reg(AFE_ASRC_CON17, 0xCB2, AFE_MASK_ALL);
		}
		mt_afe_set_reg(AFE_ASRC_CON16, 0x00075987, AFE_MASK_ALL);	/* Calibration setting */
		mt_afe_set_reg(AFE_ASRC_CON20, 0x00001b00, AFE_MASK_ALL);	/* Calibration setting */
	} else
		mt_afe_set_reg(AFE_CONN4, 1 << 30, 1 << 30);
	return true;
}

int mt_afe_enable_i2s_asrc(void) /*SetI2SASRCEnable(true)*/
{
	mt_afe_set_reg(AFE_ASRC_CON0, ((1 << 6) | (1 << 0)), ((1 << 6) | (1 << 0)));
	return 0;
}

int mt_afe_disable_i2s_asrc(void) /*SetI2SASRCEnable(false)*/
{
	uint32_t dNeedDisableASM = (mt_afe_get_reg(AFE_ASRC_CON0) & 0x0030) ? 1 : 0;

	mt_afe_set_reg(AFE_ASRC_CON0, 0, (1 << 6 | dNeedDisableASM));
	return 0;
}


bool SetoutputConnectionFormat(uint32_t ConnectionFormat, uint32_t Output)
{
	/* PRINTK_AUDDRV("+%s(), Data Format = %d, Output = %d\n", __FUNCTION__,
	   ConnectionFormat, Output); */
	mt_afe_set_reg(AFE_CONN_24BIT, (ConnectionFormat << Output), (1 << Output));
	return true;
}
#if 0
bool SetHDMIMCLK(void)
{
	uint32_t mclksamplerate = mHDMIOutput->mSampleRate * 256;
	uint32_t hdmi_APll = GetHDMIApLLSource();
	uint32_t hdmi_mclk_div = 0;

	PRINTK_AUDDRV("%s\n", __func__);
	if (hdmi_APll == APLL_SOURCE_24576)
		hdmi_APll = 24576000;
	else
		hdmi_APll = 22579200;
	PRINTK_AUDDRV("%s hdmi_mclk_div = %d mclksamplerate = %d\n", __func__,
		 hdmi_mclk_div, mclksamplerate);
	hdmi_mclk_div = (hdmi_APll / mclksamplerate / 2) - 1;
	mHDMIOutput->mHdmiMckDiv = hdmi_mclk_div;
	PRINTK_AUDDRV("%s hdmi_mclk_div = %d\n", __func__, hdmi_mclk_div);
	mt_afe_set_reg(FPGA_CFG1, hdmi_mclk_div << 24, 0x3f000000);
	SetCLkMclk(Soc_Aud_I2S3, mHDMIOutput->mSampleRate);
	return true;
}

bool SetHDMIBCLK(void)
{
	mHDMIOutput->mBckSamplerate = mHDMIOutput->mSampleRate * mHDMIOutput->mChannels;
	PRINTK_AUDDRV("%s mBckSamplerate = %d mSampleRate = %d mChannels = %d\n", __func__,
		 mHDMIOutput->mBckSamplerate, mHDMIOutput->mSampleRate, mHDMIOutput->mChannels);
	mHDMIOutput->mBckSamplerate *= (mHDMIOutput->mI2S_WLEN + 1) * 16;
	PRINTK_AUDDRV("%s mBckSamplerate = %d mApllSamplerate = %d\n", __func__,
		 mHDMIOutput->mBckSamplerate, mHDMIOutput->mApllSamplerate);
	mHDMIOutput->mHdmiBckDiv =
	    (mHDMIOutput->mApllSamplerate / mHDMIOutput->mBckSamplerate / 2) - 1;
	PRINTK_AUDDRV("%s mHdmiBckDiv = %d\n", __func__, mHDMIOutput->mHdmiBckDiv);
	mt_afe_set_reg(FPGA_CFG1, (mHDMIOutput->mHdmiBckDiv) << 16, 0x00ff0000);
	return true;
}
#endif
uint32_t GetHDMIApLLSource(void)
{
	PRINTK_AUDDRV("%s ApllSource = %d\n", __func__, mHDMIOutput->mApllSource);
	return mHDMIOutput->mApllSource;
}
#if 0
bool SetHDMIApLL(uint32_t ApllSource)
{
	PRINTK_AUDDRV("%s ApllSource = %d\n", __func__, ApllSource);
	if (ApllSource == APLL_SOURCE_24576) {
		mt_afe_set_reg(FPGA_CFG1, 0 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_24576;
		mHDMIOutput->mApllSamplerate = 24576000;
	} else if (ApllSource == APLL_SOURCE_225792) {
		mt_afe_set_reg(FPGA_CFG1, 1 << 31, 1 << 31);
		mHDMIOutput->mApllSource = APLL_SOURCE_225792;
		mHDMIOutput->mApllSamplerate = 22579200;
	}
	return true;
}

bool SetHDMIsamplerate(uint32_t samplerate)
{
	uint32_t SampleRateinedx = mt_afe_rate_to_idx(samplerate);

	mHDMIOutput->mSampleRate = samplerate;
	PRINTK_AUDDRV("%s samplerate = %d\n", __func__, samplerate);
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

#endif
bool SetHDMIdatalength(uint32_t length)
{
	PRINTK_AUDDRV("%s length = %d\n", __func__, length);
	mHDMIOutput->mI2S_WLEN = length;
	return true;
}

bool SetTDMLrckWidth(uint32_t cycles)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMbckcycle(uint32_t cycles)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMChannelsSdata(uint32_t channels)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMDatalength(uint32_t length)
{
	pr_warn("%s not support!!!\n", __func__);
	return true;
}

bool SetTDMI2Smode(uint32_t mode)
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

/*****************************************************************************
 * FUNCTION
 *  afe_allocate_dl1_buffer / AudDrv_Free_DL1_Buffer
 *
 * DESCRIPTION
 *  allocate DL1 Buffer
 *

******************************************************************************/
int afe_allocate_dl1_buffer(struct device *pDev, uint32_t Afe_Buf_Length) /*AudDrv_Allocate_DL1_Buffer*/
{
#ifdef AUDIO_MEMORY_SRAM
	uint32_t u4PhyAddr = 0;
#endif
	struct AFE_BLOCK_T *pblock;

	PRINTK_AUDDRV("%s Afe_Buf_Length = %d\n", __func__, Afe_Buf_Length);
	pblock = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	pblock->u4BufferSize = Afe_Buf_Length;
#ifdef AUDIO_MEMORY_SRAM
	if (Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE) {
		PRINTK_AUDDRV("Afe_Buf_Length > AUDDRV_DL1_MAX_BUFFER_LENGTH\n");
		return -1;
	}
#endif
	/* allocate memory */
	{
#ifdef AUDIO_MEMORY_SRAM
		/* todo , there should be a sram manager to allocate memory for low power */
		u4PhyAddr = mt_afe_get_sram_phy_addr();
		pblock->pucPhysBufAddr = u4PhyAddr;
#ifdef AUDIO_MEM_IOREMAP
		PRINTK_AUDDRV("afe_allocate_dl1_buffer length AUDIO_MEM_IOREMAP = %d\n",
			      Afe_Buf_Length);
		pblock->pucVirtBufAddr = (uint8_t *) mt_afe_get_sram_base_ptr();
#else
		pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif
#else
		PRINTK_AUDDRV("afe_allocate_dl1_buffer use dram\n");
		pblock->pucVirtBufAddr =
		    dma_alloc_coherent(pDev, pblock->u4BufferSize, &pblock->pucPhysBufAddr,
				       GFP_KERNEL);
#endif
	}
	PRINTK_AUDDRV("afe_allocate_dl1_buffer Afe_Buf_Length = %dpucVirtBufAddr = %p\n",
		      Afe_Buf_Length, pblock->pucVirtBufAddr);
	/* check 32 bytes align */
	if ((pblock->pucPhysBufAddr & 0x1f) != 0)
		PRINTK_AUDDRV("[Auddrv] afe_allocate_dl1_buffer is not aligned (0x%x)\n",
			      pblock->pucPhysBufAddr);
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	/* set sram address top hardware */
	mt_afe_set_reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	mt_afe_set_reg(AFE_DL1_END, pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);
#ifdef AUDIO_MEM_IOREMAP
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
#else
	memset(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
#endif
	return 0;
}

int afe_allocate_mem_buffer(struct device *pDev, enum Soc_Aud_Digital_Block MemBlock,
			       uint32_t Buffer_length) /*AudDrv_Allocate_mem_Buffer*/
{
	switch (MemBlock) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL2:
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
	case Soc_Aud_Digital_Block_MEM_VUL:
	case Soc_Aud_Digital_Block_MEM_HDMI:{
			PRINTK_AUDDRV("%s MemBlock =%d Buffer_length = %d\n", __func__, MemBlock,
				 Buffer_length);
			if (Audio_dma_buf[MemBlock] != NULL) {
				PRINTK_AUDDRV
				    ("afe_allocate_mem_buffer MemBlock = %d dma_alloc_coherent\n",
				     MemBlock);
				if (Audio_dma_buf[MemBlock]->area == NULL) {
					PRINTK_AUDDRV("dma_alloc_coherent\n");
					Audio_dma_buf[MemBlock]->area =
					    dma_alloc_coherent(pDev, Buffer_length,
							       &Audio_dma_buf[MemBlock]->addr,
							       GFP_KERNEL);
					if (Audio_dma_buf[MemBlock]->area)
						Audio_dma_buf[MemBlock]->bytes = Buffer_length;
				}
				PRINTK_AUDDRV("area = %p\n", Audio_dma_buf[MemBlock]->area);
			}
		}
		break;
	case Soc_Aud_Digital_Block_MEM_I2S:
		pr_warn("currently not support\n");
	default:
		pr_warn("%s not support\n", __func__);
	}
	return true;
}

struct AFE_MEM_CONTROL_T *get_mem_control_t(enum Soc_Aud_Digital_Block MemBlock)/*Get_Mem_ControlT*/
{
	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI)
		return AFE_Mem_Control_context[MemBlock];

	pr_err("%s error\n", __func__);
	return NULL;
}

bool set_memif_substream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)/*SetMemifSubStream*/
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	PRINTK_AUDDRV("+%s MemBlock = %d substream = %p\n", __func__, MemBlock, substream);
	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	if (head == NULL) {	/* first item is NULL */
		/*pr_warn("%s head == NULL\n", __func__);*/
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
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	PRINTK_AUDDRV("-%s MemBlock = %d\n", __func__, MemBlock);
	/* DumpMemifSubStream(); */
	return true;
}

bool ClearMemBlock(enum Soc_Aud_Digital_Block MemBlock)
{
	struct AFE_BLOCK_T *pBlock = NULL;

	if (MemBlock >= 0 && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI) {
		pBlock = &AFE_Mem_Control_context[MemBlock]->rBlock;
#ifdef AUDIO_MEM_IOREMAP
		if (pBlock->pucVirtBufAddr == (uint8_t *) mt_afe_get_sram_base_ptr())
			memset_io(pBlock->pucVirtBufAddr, 0, pBlock->u4BufferSize);
		else {
#endif
			memset(pBlock->pucVirtBufAddr, 0, pBlock->u4BufferSize);
#ifdef AUDIO_MEM_IOREMAP
		}
#endif
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

bool RemoveMemifSubStream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)
{
	struct substreamList *head;
	struct substreamList *temp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	if (AFE_Mem_Control_context[MemBlock]->MemIfNum == 0)
		PRINTK_AUDDRV("%s AFE_Mem_Control_context[%d]->MemIfNum == 0\n", __func__, MemBlock);
	else
		AFE_Mem_Control_context[MemBlock]->MemIfNum--;
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	PRINTK_AUDDRV("+ %s MemBlock = %d substream = %p\n", __func__, MemBlock, substream);
	if (head == NULL) {	/* no object */
		/* do nothing */
	} else {
		/* condition for first item hit */
		if (head->substream == substream) {
			/* PRINTK_AUDDRV("%s head->substream = %p\n ", __func__, head->substream); */
			AFE_Mem_Control_context[MemBlock]->substreamL = head->next;
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
	else
		PRINTK_AUDDRV("%s substreram is not NULL MemBlock = %d\n", __func__, MemBlock);
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	PRINTK_AUDDRV("- %s MemBlock = %d\n", __func__, MemBlock);
	return true;
}

static unsigned long dl1_flags;
void afe_dl1_spinlock_lock(void)/*Auddrv_Dl1_Spinlock_lock*/
{
	spin_lock_irqsave(&auddrv_dl1_lock, dl1_flags);
}

void afe_dl1_spinlock_unlock(void)/*Auddrv_Dl1_Spinlock_unlock*/
{
	spin_unlock_irqrestore(&auddrv_dl1_lock, dl1_flags);
}

static unsigned long ul1_flags;
void afe_ul1_spinlock_lock(void)/*Auddrv_UL1_Spinlock_lock*/
{
	spin_lock_irqsave(&auddrv_ul1_lock, ul1_flags);
}

void afe_ul1_spinlock_unlock(void)/*Auddrv_UL1_Spinlock_unlock*/
{
	spin_unlock_irqrestore(&auddrv_ul1_lock, ul1_flags);
}

void afe_hdmi_interrupt_handler(void)/*afe_hdmi_interrupt_handler*/
{				/* irq5 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI];
	int32_t Afe_consumed_bytes = 0;
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	unsigned long flags;
	struct AFE_BLOCK_T *Afe_Block =  &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI]->rBlock);

	if (Mem_Block == NULL) {
		pr_err("%s Mem_Block == NULL\n", __func__);
		return;
	}
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_HDMI) == false) {
		pr_err("%s, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_HDMI) == false\n",
		     __func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	HW_Cur_ReadIdx = mt_afe_get_reg(AFE_HDMI_OUT_CUR);
	if (HW_Cur_ReadIdx == 0) {
		PRINTK_AUDDRV("[%s] HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	PRINTK_AUD_HDMI
	    ("[%s] 0 HW_Cur_ReadIdx=%x HW_memory_index=%x Afe_Block->pucPhysBufAddr=%x\n",
	    __func__, HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}

	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;

	/*
	   if ((Afe_consumed_bytes & 0x1f) != 0)
	   {
	   pr_warn("[Auddrv_HDMI_Interrupt] DMA address is not aligned 32 bytes\n");
	   }
	*/

	PRINTK_AUD_HDMI("[%s] 1 ReadIdx:%x WriteIdx:%x DataRemained:%x", __func__,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained);

	PRINTK_AUD_HDMI(" Afe_consumed_bytes:%x HW_memory_index:%x\n",
		Afe_consumed_bytes, HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes || Afe_Block->u4DataRemained <= 0
	    || Afe_Block->u4DataRemained > Afe_Block->u4BufferSize) {
		/* buffer underflow --> clear  whole buffer */
		/* memset(Afe_Block->pucVirtBufAddr, 0, Afe_Block->u4BufferSize); */
		PRINTK_AUD_HDMI("+[%s] 2 underflow ReadIdx:%x WriteIdx:%x, DataRemained:%x,",
			__func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
			Afe_Block->u4DataRemained);
		PRINTK_AUD_HDMI(" Afe_consumed_bytes:%x HW_memory_index:0x%x\n",
			Afe_consumed_bytes, HW_memory_index);

		Afe_Block->u4DMAReadIdx = HW_memory_index;
		Afe_Block->u4WriteIdx = Afe_Block->u4DMAReadIdx;
		Afe_Block->u4DataRemained = Afe_Block->u4BufferSize;
		PRINTK_AUD_HDMI
		("-[%s] 2 underflow ReadIdx:%x WriteIdx:%x DataRemained:%x Afe_consumed_bytes:%x\n",
		__func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
		Afe_Block->u4DataRemained, Afe_consumed_bytes);

	} else {
		PRINTK_AUD_HDMI
		    ("+[%s] 3 normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
		    __func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained,
		    Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
		PRINTK_AUD_HDMI
		    ("-[%s] 3 normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n",
		    __func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained,
		    Afe_Block->u4WriteIdx);
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI]->interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
	PRINTK_AUD_HDMI
		("-[%s]4 ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n", __func__,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);
}


void afe_awb_interrupt_handler(void)/*Auddrv_AWB_Interrupt_Handler*/
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_AWB];
	uint32_t HW_Cur_ReadIdx = 0;
	uint32_t MaxCopySize = 0;
	int32_t Hw_Get_bytes = 0;
	struct substreamList *temp = NULL;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;
	uint32_t temp_cnt = 0;

	if (Mem_Block == NULL) {
		pr_err("-%s()Mem_Block == NULL\n", __func__);
		return;
	}
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_AWB) == false) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		pr_err("-%s(), mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_AWB) == %d\n",
			__func__, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_AWB));
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = align64bytesize(mt_afe_get_reg(AFE_AWB_CUR));
	PRINTK_AUD_AWB("afe_awb_interrupt_handler HW_Cur_ReadIdx = 0x%x\n", HW_Cur_ReadIdx);
	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	MaxCopySize = get_mem_maxcopysize(Soc_Aud_Digital_Block_MEM_AWB);
	PRINTK_AUD_AWB
		("%s 1 mBlock = %p MaxCopySize = 0x%x u4BufferSize = 0x%x\n",
		__func__, mBlock, MaxCopySize, mBlock->u4BufferSize);

	if (MaxCopySize) {
		if (MaxCopySize > mBlock->u4BufferSize)
			MaxCopySize = mBlock->u4BufferSize;

		mBlock->u4DataRemained -= MaxCopySize;
		mBlock->u4DMAReadIdx += MaxCopySize;
		mBlock->u4DMAReadIdx %= mBlock->u4BufferSize;
		clear_mem_copysize(Soc_Aud_Digital_Block_MEM_AWB);

		PRINTK_AUD_AWB("%s update read pointer u4DMAReadIdx:0x%x,", __func__,
		     mBlock->u4DMAReadIdx);
		PRINTK_AUD_AWB
		    (" u4WriteIdx:0x%x, pucPhysBufAddr:0x%x mBlock->u4MaxCopySize:0x%x\n",
		     mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize);
	}

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;

	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUD_AWB
	    ("+%s Hw_Get_bytes:0x%x, HW_Cur_ReadIdx:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x,",
	    __func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx);
	PRINTK_AUD_AWB
	    (" pucPhysBufAddr:0x%x, mBlock->u4MaxCopySize:0x%x, mBlock->u4DataRemained:0x%x\n",
	    mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize, mBlock->u4DataRemained);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		PRINTK_AUD_AWB
			("%s overflow u4DMAReadIdx:%x u4WriteIdx:%x u4DataRemained:%x u4BufferSize:%x\n",
			__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained,
			mBlock->u4BufferSize);
		mBlock->u4DataRemained %= mBlock->u4BufferSize;
	}
	Mem_Block->interruptTrigger = 1;
	temp = Mem_Block->substreamL;
	while (temp != NULL) {
		if (temp->substream != NULL) {
			temp_cnt = Mem_Block->MemIfNum;
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(temp->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
			if (temp_cnt != Mem_Block->MemIfNum) {
				PRINTK_AUD_AWB("%s temp_cnt = %u, Mem_Block->MemIfNum = %u\n", __func__,
					 temp_cnt, Mem_Block->MemIfNum);
				temp = Mem_Block->substreamL;
			}
		}
		if (temp != NULL)
			temp = temp->next;
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
	PRINTK_AUD_AWB
	    ("-%s u4DMAReadIdx:0x%x, u4WriteIdx:0x%x mBlock->u4DataRemained:0x%x\n", __func__,
	     mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained);
}

void afe_dai_interrupt_handler(void)/*Auddrv_DAI_Interrupt_Handler*/
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DAI];
	uint32_t HW_Cur_ReadIdx = 0;
	int32_t Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL)
		return;
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DAI) == false) {
		pr_err("%s, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DAI) == false\n",
		     __func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = align64bytesize(mt_afe_get_reg(AFE_DAI_CUR));
	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;

	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUD_DAI("%s Hw_Get_bytes:0x%x, HW_Cur_ReadIdx:0x%x, u4DMAReadIdx:0x%x,",
		__func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx);
	PRINTK_AUD_DAI
		(" u4WriteIdx:0x%x, pucPhysBufAddr:0x%x, Mem_Block->MemIfNum:%d\n",
		mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		PRINTK_AUD_DAI
		("%s overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x\n",
		mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained,
		mBlock->u4BufferSize);
		/*
		   mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
		   mBlock->u4DMAReadIdx = mBlock->u4WriteIdx - mBlock->u4BufferSize / 2;
		   if (mBlock->u4DMAReadIdx < 0)
		   {
		   mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
		   }
		*/
	}
	Mem_Block->interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void afe_dl1_interrupt_handler(void)/*Auddrv_DL1_Interrupt_Handler*/
{				/* irq1 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1];
	int32_t Afe_consumed_bytes = 0;
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T *Afe_Block = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
	unsigned long flags;

	if (Mem_Block == NULL)
		return;
	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL1) == false) {
		pr_err("%s, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL1) == false\n",
			__func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	HW_Cur_ReadIdx = mt_afe_get_reg(AFE_DL1_CUR);
	if (HW_Cur_ReadIdx == 0) {
		PRINTK_AUDDRV("[%s] HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
	PRINTK_AUD_DL1
	    ("[%s] HW_Cur_ReadIdx:0x%x HW_memory_index:0x%x Afe_Block->pucPhysBufAddr:0x%x\n",
	    __func__, HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}
	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;	/* 64 bytes align */
	/*
	   if ((Afe_consumed_bytes & 0x1f) != 0)
	   {
	   pr_warn("[Auddrv] DMA address is not aligned 32 bytes\n");
	   }
	*/
	PRINTK_AUD_DL1
		("+%s ReadIdx:%x, WriteIdx:%x, DataRemained:%x, Afe_consumed_bytes:%x,",
		__func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
		Afe_Block->u4DataRemained, Afe_consumed_bytes);
	PRINTK_AUD_DL1(" HW_memory_index:%x\n", HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes || Afe_Block->u4DataRemained <= 0
	    || Afe_Block->u4DataRemained > Afe_Block->u4BufferSize)
		pr_warn("%s underflow\n", __func__);
	else {
		PRINTK_AUD_DL1
			("+%s normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n", __func__,
			Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->interruptTrigger = 1;

	PRINTK_AUD_DL1
		("-%s ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n", __func__,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void afe_dl2_interrupt_handler(void)/*Auddrv_DL2_Interrupt_Handler*/
{				/* irq2 ISR handler */
#define MAGIC_NUMBER 0xFFFFFFC0
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2];
	int32_t Afe_consumed_bytes = 0;
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T *Afe_Block = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->rBlock);
	unsigned long flags;

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL2) == false) {
		pr_err("%s, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL2) == false\n",
			__func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	HW_Cur_ReadIdx = mt_afe_get_reg(AFE_DL2_CUR);
	if (HW_Cur_ReadIdx == 0) {
		PRINTK_AUD_DL2("[%s] DL2 HW_Cur_ReadIdx == 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}
	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	PRINTK_AUD_DL2
	    ("[%s] HW_Cur_ReadIdx:0x%x HW_memory_index:0x%x Afe_Block->pucPhysBufAddr:0x%x\n",
	    __func__, HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx)
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	else {
		Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
			- Afe_Block->u4DMAReadIdx;
	}
	Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;	/* 64 bytes align */

	/*
	   if ((Afe_consumed_bytes & 0x1f) != 0)
	   {
	   pr_warn("[Auddrv] DMA address is not aligned 32 bytes\n");
	   }
	*/

	PRINTK_AUD_DL2
	("+%s ReadIdx:%x WriteIdx:%x DataRemained:%x Afe_consumed_bytes:%x HW_memory_index:%x\n",
	__func__, Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained,
	Afe_consumed_bytes, HW_memory_index);

	if (Afe_Block->u4DataRemained < Afe_consumed_bytes || Afe_Block->u4DataRemained <= 0
	    || Afe_Block->u4DataRemained > Afe_Block->u4BufferSize)
		PRINTK_AUDDRV("%s underflow\n", __func__);
	else {
		PRINTK_AUD_DL2
			("+%s normal ReadIdx:%x, DataRemained:%x, WriteIdx:%x\n", __func__,
			Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->interruptTrigger = 1;

	PRINTK_AUD_DL2
		("-%s ReadIdx:%x ,DataRemained:%x, WriteIdx:%x\n", __func__,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
	if (AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->offloadstream)
		AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->offloadCbk
		    (AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL2]->offloadstream);
}


struct snd_dma_buffer *afe_get_mem_buffer(enum Soc_Aud_Digital_Block MemBlock)/*Get_Mem_Buffer*/
{
	PRINTK_AUDDRV("%s MemBlock = %d\n", __func__, MemBlock);
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

void afe_ul1_interrupt_handler(void)/*Auddrv_UL1_Interrupt_Handler*/
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL];
	uint32_t HW_Cur_ReadIdx = 0;
	int32_t Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;
	unsigned long flags;

	if (Mem_Block == NULL) {
		pr_err("%s Mem_Block == NULL\n", __func__);
		return;
	}

	spin_lock_irqsave(&Mem_Block->substream_lock, flags);
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_VUL) == false) {
		pr_err("%s, mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_VUL) == false\n",
			__func__);
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	mBlock = &Mem_Block->rBlock;
	HW_Cur_ReadIdx = align64bytesize(mt_afe_get_reg(AFE_VUL_CUR));

	if (CheckSize(HW_Cur_ReadIdx)) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}
	if (mBlock->pucVirtBufAddr == NULL) {
		spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
		return;
	}

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUD_UL1
		("%s Hw_Get_bytes:%x, HW_Cur_ReadIdx:%x, u4DMAReadIdx:%x, u4WriteIdx:0x%x,",
		__func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx);
	PRINTK_AUD_UL1
		(" pucPhysBufAddr:%x, Mem_Block->MemIfNum:%d\n", mBlock->pucPhysBufAddr,
		Mem_Block->MemIfNum);

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx %= mBlock->u4BufferSize;
	mBlock->u4DataRemained += Hw_Get_bytes;

	/* buffer overflow */
	if (mBlock->u4DataRemained > mBlock->u4BufferSize) {
		PRINTK_AUDDRV
		("%s overflow u4DMAReadIdx:%x u4WriteIdx:%x u4DataRemained:%x u4BufferSize:%x\n",
		__func__, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained,
		mBlock->u4BufferSize);
	}
	AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL]->interruptTrigger = 1;
	if (Mem_Block->substreamL != NULL) {
		if (Mem_Block->substreamL->substream != NULL) {
			spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
			snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
			spin_lock_irqsave(&Mem_Block->substream_lock, flags);
		}
	}
	spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
}

void clear_mem_copysize(enum Soc_Aud_Digital_Block MemBlock)/*Clear_Mem_CopySize*/
{
	struct substreamList *head;
	/* unsigned long flags; */
	/* spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	PRINTK_AUDDRV("%s MemBlock = %d\n", __func__, MemBlock);
	while (head != NULL) {	/* first item is NULL */
		head->u4MaxCopySize = 0;
		head = head->next;
	}
	/* spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags); */
}

uint32_t get_mem_copysizebystream(enum Soc_Aud_Digital_Block MemBlock,
				    struct snd_pcm_substream *substream)/*Get_Mem_CopySizeByStream*/
{
	struct substreamList *head;
	unsigned long flags;
	uint32_t MaxCopySize;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* PRINTK_AUDDRV("%s MemBlock = %d\n", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (head->substream == substream) {
			MaxCopySize = head->u4MaxCopySize;
			spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock,
					       flags);
			return MaxCopySize;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	return 0;
}

uint32_t get_mem_maxcopysize(enum Soc_Aud_Digital_Block MemBlock)/*Get_Mem_MaxCopySize*/
{
	struct substreamList *head;
	uint32_t MaxCopySize;
	/*
	   unsigned long flags;
	   spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	*/
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	MaxCopySize = 0;
	/* PRINTK_AUDDRV("+%s MemBlock = %d\n ", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (MaxCopySize < head->u4MaxCopySize)
			MaxCopySize = head->u4MaxCopySize;
		head = head->next;
	}
	/*
	   spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	   PRINTK_AUDDRV("-%s MemBlock = %d\n", __func__, MemBlock);
	 */
	return MaxCopySize;
}

void set_mem_copysizebystream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream,
			      uint32_t size)/*Set_Mem_CopySizeByStream*/
{
	struct substreamList *head;
	unsigned long flags;

	spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
	/* PRINTK_AUDDRV("+%s MemBlock = %d\n", __func__, MemBlock); */
	while (head != NULL) {	/* first item is NULL */
		if (head->substream == substream) {
			head->u4MaxCopySize += size;
			break;
		}
		head = head->next;
	}
	spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	/* PRINTK_AUDDRV("-%s MemBlock = %d\n ", __func__, MemBlock); */
}


bool backup_audio_register(void)/*BackUp_Audio_Register*/
{
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	mAudioRegCache.REG_AUDIO_TOP_CON1 = mt_afe_get_reg(AUDIO_TOP_CON1);
	mAudioRegCache.REG_AUDIO_TOP_CON2 = mt_afe_get_reg(AUDIO_TOP_CON2);
	mAudioRegCache.REG_AUDIO_TOP_CON3 = mt_afe_get_reg(AUDIO_TOP_CON3);
	mAudioRegCache.REG_AFE_DAC_CON0 = mt_afe_get_reg(AFE_DAC_CON0);
	mAudioRegCache.REG_AFE_DAC_CON1 = mt_afe_get_reg(AFE_DAC_CON1);
	mAudioRegCache.REG_AFE_I2S_CON = mt_afe_get_reg(AFE_I2S_CON);
	mAudioRegCache.REG_AFE_DAIBT_CON0 = mt_afe_get_reg(AFE_DAIBT_CON0);
	mAudioRegCache.REG_AFE_CONN0 = mt_afe_get_reg(AFE_CONN0);
	mAudioRegCache.REG_AFE_CONN1 = mt_afe_get_reg(AFE_CONN1);
	mAudioRegCache.REG_AFE_CONN2 = mt_afe_get_reg(AFE_CONN2);
	mAudioRegCache.REG_AFE_CONN3 = mt_afe_get_reg(AFE_CONN3);
	mAudioRegCache.REG_AFE_CONN4 = mt_afe_get_reg(AFE_CONN4);
	mAudioRegCache.REG_AFE_I2S_CON1 = mt_afe_get_reg(AFE_I2S_CON1);
	mAudioRegCache.REG_AFE_I2S_CON2 = mt_afe_get_reg(AFE_I2S_CON2);
	mAudioRegCache.REG_AFE_MRGIF_CON = mt_afe_get_reg(AFE_MRGIF_CON);
	mAudioRegCache.REG_AFE_DL1_BASE = mt_afe_get_reg(AFE_DL1_BASE);
	mAudioRegCache.REG_AFE_DL1_CUR = mt_afe_get_reg(AFE_DL1_CUR);
	mAudioRegCache.REG_AFE_DL1_END = mt_afe_get_reg(AFE_DL1_END);
	mAudioRegCache.REG_AFE_DL1_D2_BASE = mt_afe_get_reg(AFE_DL1_D2_BASE);
	mAudioRegCache.REG_AFE_DL1_D2_CUR = mt_afe_get_reg(AFE_DL1_D2_CUR);
	mAudioRegCache.REG_AFE_DL1_D2_END = mt_afe_get_reg(AFE_DL1_D2_END);
	mAudioRegCache.REG_AFE_VUL_D2_BASE = mt_afe_get_reg(AFE_VUL_D2_BASE);
	mAudioRegCache.REG_AFE_VUL_D2_END = mt_afe_get_reg(AFE_VUL_D2_END);
	mAudioRegCache.REG_AFE_VUL_D2_CUR = mt_afe_get_reg(AFE_VUL_D2_CUR);
	mAudioRegCache.REG_AFE_I2S_CON3 = mt_afe_get_reg(AFE_I2S_CON3);
	mAudioRegCache.REG_AFE_DL2_BASE = mt_afe_get_reg(AFE_DL2_BASE);
	mAudioRegCache.REG_AFE_DL2_CUR = mt_afe_get_reg(AFE_DL2_CUR);
	mAudioRegCache.REG_AFE_DL2_END = mt_afe_get_reg(AFE_DL2_END);
	mAudioRegCache.REG_AFE_CONN5 = mt_afe_get_reg(AFE_CONN5);
	mAudioRegCache.REG_AFE_CONN_24BIT = mt_afe_get_reg(AFE_CONN_24BIT);
	mAudioRegCache.REG_AFE_AWB_BASE = mt_afe_get_reg(AFE_AWB_BASE);
	mAudioRegCache.REG_AFE_AWB_END = mt_afe_get_reg(AFE_AWB_END);
	mAudioRegCache.REG_AFE_AWB_CUR = mt_afe_get_reg(AFE_AWB_CUR);
	mAudioRegCache.REG_AFE_VUL_BASE = mt_afe_get_reg(AFE_VUL_BASE);
	mAudioRegCache.REG_AFE_VUL_END = mt_afe_get_reg(AFE_VUL_END);
	mAudioRegCache.REG_AFE_VUL_CUR = mt_afe_get_reg(AFE_VUL_CUR);
	mAudioRegCache.REG_AFE_DAI_BASE = mt_afe_get_reg(AFE_DAI_BASE);
	mAudioRegCache.REG_AFE_DAI_END = mt_afe_get_reg(AFE_DAI_END);
	mAudioRegCache.REG_AFE_DAI_CUR = mt_afe_get_reg(AFE_DAI_CUR);
	mAudioRegCache.REG_AFE_CONN6 = mt_afe_get_reg(AFE_CONN6);
	mAudioRegCache.REG_AFE_MEMIF_MSB = mt_afe_get_reg(AFE_MEMIF_MSB);
	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON0 = mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON0);
	mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON1 = mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON1);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON0 = mt_afe_get_reg(AFE_ADDA_UL_SRC_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON1 = mt_afe_get_reg(AFE_ADDA_UL_SRC_CON1);
	mAudioRegCache.REG_AFE_ADDA_TOP_CON0 = mt_afe_get_reg(AFE_ADDA_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_UL_DL_CON0 = mt_afe_get_reg(AFE_ADDA_UL_DL_CON0);
	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG0 = mt_afe_get_reg(AFE_ADDA_NEWIF_CFG0);
	mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG1 = mt_afe_get_reg(AFE_ADDA_NEWIF_CFG1);
	mAudioRegCache.REG_AFE_SIDETONE_CON0 = mt_afe_get_reg(AFE_SIDETONE_CON0);
	mAudioRegCache.REG_AFE_SIDETONE_COEFF = mt_afe_get_reg(AFE_SIDETONE_COEFF);
	mAudioRegCache.REG_AFE_SIDETONE_CON1 = mt_afe_get_reg(AFE_SIDETONE_CON1);
	mAudioRegCache.REG_AFE_SIDETONE_GAIN = mt_afe_get_reg(AFE_SIDETONE_GAIN);
	mAudioRegCache.REG_AFE_SGEN_CON0 = mt_afe_get_reg(AFE_SGEN_CON0);
	mAudioRegCache.REG_AFE_TOP_CON0 = mt_afe_get_reg(AFE_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON0 = mt_afe_get_reg(AFE_ADDA_PREDIS_CON0);
	mAudioRegCache.REG_AFE_ADDA_PREDIS_CON1 = mt_afe_get_reg(AFE_ADDA_PREDIS_CON1);
	mAudioRegCache.REG_AFE_MOD_DAI_BASE = mt_afe_get_reg(AFE_MOD_DAI_BASE);
	mAudioRegCache.REG_AFE_MOD_DAI_END = mt_afe_get_reg(AFE_MOD_DAI_END);
	mAudioRegCache.REG_AFE_MOD_DAI_CUR = mt_afe_get_reg(AFE_MOD_DAI_CUR);
	mAudioRegCache.REG_AFE_IRQ_MCU_CON = mt_afe_get_reg(AFE_IRQ_MCU_CON);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT1 = mt_afe_get_reg(AFE_IRQ_MCU_CNT1);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT2 = mt_afe_get_reg(AFE_IRQ_MCU_CNT2);
	mAudioRegCache.REG_AFE_IRQ_MCU_EN = mt_afe_get_reg(AFE_IRQ_MCU_EN);
	mAudioRegCache.REG_AFE_MEMIF_MAXLEN = mt_afe_get_reg(AFE_MEMIF_MAXLEN);
	mAudioRegCache.REG_AFE_MEMIF_PBUF_SIZE = mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE);
	mAudioRegCache.REG_AFE_IRQ_MCU_CNT7 = mt_afe_get_reg(AFE_IRQ_MCU_CNT7);
	mAudioRegCache.REG_AFE_APLL1_TUNER_CFG = mt_afe_get_reg(AFE_APLL1_TUNER_CFG);
	mAudioRegCache.REG_AFE_APLL2_TUNER_CFG = mt_afe_get_reg(AFE_APLL2_TUNER_CFG);
	mAudioRegCache.REG_AFE_GAIN1_CON0 = mt_afe_get_reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN1_CON1 = mt_afe_get_reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN1_CON2 = mt_afe_get_reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN1_CON3 = mt_afe_get_reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN1_CONN = mt_afe_get_reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN1_CUR = mt_afe_get_reg(AFE_GAIN1_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CON0 = mt_afe_get_reg(AFE_GAIN1_CON0);
	mAudioRegCache.REG_AFE_GAIN2_CON1 = mt_afe_get_reg(AFE_GAIN1_CON1);
	mAudioRegCache.REG_AFE_GAIN2_CON2 = mt_afe_get_reg(AFE_GAIN1_CON2);
	mAudioRegCache.REG_AFE_GAIN2_CON3 = mt_afe_get_reg(AFE_GAIN1_CON3);
	mAudioRegCache.REG_AFE_GAIN2_CONN = mt_afe_get_reg(AFE_GAIN1_CONN);
	mAudioRegCache.REG_AFE_GAIN2_CUR = mt_afe_get_reg(AFE_GAIN2_CUR);
	mAudioRegCache.REG_AFE_GAIN2_CONN2 = mt_afe_get_reg(AFE_GAIN2_CONN2);
	mAudioRegCache.REG_AFE_GAIN2_CONN3 = mt_afe_get_reg(AFE_GAIN2_CONN3);
	mAudioRegCache.REG_AFE_GAIN1_CONN2 = mt_afe_get_reg(AFE_GAIN1_CONN2);
	mAudioRegCache.REG_AFE_GAIN1_CONN3 = mt_afe_get_reg(AFE_GAIN1_CONN3);
	mAudioRegCache.REG_AFE_CONN7 = mt_afe_get_reg(AFE_CONN7);
	mAudioRegCache.REG_AFE_CONN8 = mt_afe_get_reg(AFE_CONN8);
	mAudioRegCache.REG_AFE_CONN9 = mt_afe_get_reg(AFE_CONN9);
	mAudioRegCache.REG_AFE_CONN10 = mt_afe_get_reg(AFE_CONN10);
	mAudioRegCache.REG_FPGA_CFG2 = mt_afe_get_reg(FPGA_CFG2);
	mAudioRegCache.REG_FPGA_CFG3 = mt_afe_get_reg(FPGA_CFG3);
	mAudioRegCache.REG_FPGA_CFG0 = mt_afe_get_reg(FPGA_CFG0);
	mAudioRegCache.REG_FPGA_CFG1 = mt_afe_get_reg(FPGA_CFG1);
	mAudioRegCache.REG_AFE_ASRC_CON0 = mt_afe_get_reg(AFE_ASRC_CON0);
	mAudioRegCache.REG_AFE_ASRC_CON1 = mt_afe_get_reg(AFE_ASRC_CON1);
	mAudioRegCache.REG_AFE_ASRC_CON2 = mt_afe_get_reg(AFE_ASRC_CON2);
	mAudioRegCache.REG_AFE_ASRC_CON3 = mt_afe_get_reg(AFE_ASRC_CON3);
	mAudioRegCache.REG_AFE_ASRC_CON4 = mt_afe_get_reg(AFE_ASRC_CON4);
	mAudioRegCache.REG_AFE_ASRC_CON5 = mt_afe_get_reg(AFE_ASRC_CON5);
	mAudioRegCache.REG_AFE_ASRC_CON6 = mt_afe_get_reg(AFE_ASRC_CON6);
	mAudioRegCache.REG_AFE_ASRC_CON7 = mt_afe_get_reg(AFE_ASRC_CON7);
	mAudioRegCache.REG_AFE_ASRC_CON8 = mt_afe_get_reg(AFE_ASRC_CON8);
	mAudioRegCache.REG_AFE_ASRC_CON9 = mt_afe_get_reg(AFE_ASRC_CON9);
	mAudioRegCache.REG_AFE_ASRC_CON10 = mt_afe_get_reg(AFE_ASRC_CON10);
	mAudioRegCache.REG_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON11);
	mAudioRegCache.REG_PCM_INTF_CON = mt_afe_get_reg(PCM_INTF_CON1);
	mAudioRegCache.REG_PCM_INTF_CON2 = mt_afe_get_reg(PCM_INTF_CON2);
	mAudioRegCache.REG_PCM2_INTF_CON = mt_afe_get_reg(PCM2_INTF_CON);
	mAudioRegCache.REG_AUDIO_CLK_AUDDIV_0 = mt_afe_get_reg(AUDIO_CLK_AUDDIV_0);
	mAudioRegCache.REG_AUDIO_CLK_AUDDIV_1 = mt_afe_get_reg(AUDIO_CLK_AUDDIV_1);
	mAudioRegCache.REG_AFE_ASRC4_CON0 = mt_afe_get_reg(AFE_ASRC4_CON0);
	mAudioRegCache.REG_AFE_ASRC4_CON1 = mt_afe_get_reg(AFE_ASRC4_CON1);
	mAudioRegCache.REG_AFE_ASRC4_CON2 = mt_afe_get_reg(AFE_ASRC4_CON2);
	mAudioRegCache.REG_AFE_ASRC4_CON3 = mt_afe_get_reg(AFE_ASRC4_CON3);
	mAudioRegCache.REG_AFE_ASRC4_CON4 = mt_afe_get_reg(AFE_ASRC4_CON4);
	mAudioRegCache.REG_AFE_ASRC4_CON5 = mt_afe_get_reg(AFE_ASRC4_CON5);
	mAudioRegCache.REG_AFE_ASRC4_CON6 = mt_afe_get_reg(AFE_ASRC4_CON6);
	mAudioRegCache.REG_AFE_ASRC4_CON7 = mt_afe_get_reg(AFE_ASRC4_CON7);
	mAudioRegCache.REG_AFE_ASRC4_CON8 = mt_afe_get_reg(AFE_ASRC4_CON8);
	mAudioRegCache.REG_AFE_ASRC4_CON9 = mt_afe_get_reg(AFE_ASRC4_CON9);
	mAudioRegCache.REG_AFE_ASRC4_CON10 = mt_afe_get_reg(AFE_ASRC4_CON10);
	mAudioRegCache.REG_AFE_ASRC4_CON11 = mt_afe_get_reg(AFE_ASRC4_CON11);
	mAudioRegCache.REG_AFE_ASRC4_CON12 = mt_afe_get_reg(AFE_ASRC4_CON12);
	mAudioRegCache.REG_AFE_ASRC4_CON13 = mt_afe_get_reg(AFE_ASRC4_CON13);
	mAudioRegCache.REG_AFE_ASRC4_CON14 = mt_afe_get_reg(AFE_ASRC4_CON14);
	mAudioRegCache.REG_AFE_ASRC_CON13 = mt_afe_get_reg(AFE_ASRC_CON13);
	mAudioRegCache.REG_AFE_ASRC_CON14 = mt_afe_get_reg(AFE_ASRC_CON14);
	mAudioRegCache.REG_AFE_ASRC_CON15 = mt_afe_get_reg(AFE_ASRC_CON15);
	mAudioRegCache.REG_AFE_ASRC_CON16 = mt_afe_get_reg(AFE_ASRC_CON16);
	mAudioRegCache.REG_AFE_ASRC_CON17 = mt_afe_get_reg(AFE_ASRC_CON17);
	mAudioRegCache.REG_AFE_ASRC_CON18 = mt_afe_get_reg(AFE_ASRC_CON18);
	mAudioRegCache.REG_AFE_ASRC_CON19 = mt_afe_get_reg(AFE_ASRC_CON19);
	mAudioRegCache.REG_AFE_ASRC_CON20 = mt_afe_get_reg(AFE_ASRC_CON20);
	mAudioRegCache.REG_AFE_ASRC_CON21 = mt_afe_get_reg(AFE_ASRC_CON21);
	mAudioRegCache.REG_AFE_ASRC2_CON0 = mt_afe_get_reg(AFE_ASRC2_CON0);
	mAudioRegCache.REG_AFE_ASRC2_CON1 = mt_afe_get_reg(AFE_ASRC2_CON1);
	mAudioRegCache.REG_AFE_ASRC2_CON2 = mt_afe_get_reg(AFE_ASRC2_CON2);
	mAudioRegCache.REG_AFE_ASRC2_CON3 = mt_afe_get_reg(AFE_ASRC2_CON3);
	mAudioRegCache.REG_AFE_ASRC2_CON4 = mt_afe_get_reg(AFE_ASRC2_CON4);
	mAudioRegCache.REG_AFE_ASRC2_CON5 = mt_afe_get_reg(AFE_ASRC2_CON5);
	mAudioRegCache.REG_AFE_ASRC2_CON6 = mt_afe_get_reg(AFE_ASRC2_CON6);
	mAudioRegCache.REG_AFE_ASRC2_CON7 = mt_afe_get_reg(AFE_ASRC2_CON7);
	mAudioRegCache.REG_AFE_ASRC2_CON8 = mt_afe_get_reg(AFE_ASRC2_CON8);
	mAudioRegCache.REG_AFE_ASRC2_CON9 = mt_afe_get_reg(AFE_ASRC2_CON9);
	mAudioRegCache.REG_AFE_ASRC2_CON10 = mt_afe_get_reg(AFE_ASRC2_CON10);
	mAudioRegCache.REG_AFE_ASRC2_CON11 = mt_afe_get_reg(AFE_ASRC2_CON11);
	mAudioRegCache.REG_AFE_ASRC2_CON12 = mt_afe_get_reg(AFE_ASRC2_CON12);
	mAudioRegCache.REG_AFE_ASRC2_CON13 = mt_afe_get_reg(AFE_ASRC2_CON13);
	mAudioRegCache.REG_AFE_ASRC2_CON14 = mt_afe_get_reg(AFE_ASRC2_CON14);
	mAudioRegCache.REG_AFE_ASRC3_CON0 = mt_afe_get_reg(AFE_ASRC3_CON0);
	mAudioRegCache.REG_AFE_ASRC3_CON1 = mt_afe_get_reg(AFE_ASRC3_CON1);
	mAudioRegCache.REG_AFE_ASRC3_CON2 = mt_afe_get_reg(AFE_ASRC3_CON2);
	mAudioRegCache.REG_AFE_ASRC3_CON3 = mt_afe_get_reg(AFE_ASRC3_CON3);
	mAudioRegCache.REG_AFE_ASRC3_CON4 = mt_afe_get_reg(AFE_ASRC3_CON4);
	mAudioRegCache.REG_AFE_ASRC3_CON5 = mt_afe_get_reg(AFE_ASRC3_CON5);
	mAudioRegCache.REG_AFE_ASRC3_CON6 = mt_afe_get_reg(AFE_ASRC3_CON6);
	mAudioRegCache.REG_AFE_ASRC3_CON7 = mt_afe_get_reg(AFE_ASRC3_CON7);
	mAudioRegCache.REG_AFE_ASRC3_CON8 = mt_afe_get_reg(AFE_ASRC3_CON8);
	mAudioRegCache.REG_AFE_ASRC3_CON9 = mt_afe_get_reg(AFE_ASRC3_CON9);
	mAudioRegCache.REG_AFE_ASRC3_CON10 = mt_afe_get_reg(AFE_ASRC3_CON10);
	mAudioRegCache.REG_AFE_ASRC3_CON11 = mt_afe_get_reg(AFE_ASRC3_CON11);
	mAudioRegCache.REG_AFE_ASRC3_CON12 = mt_afe_get_reg(AFE_ASRC3_CON12);
	mAudioRegCache.REG_AFE_ASRC3_CON13 = mt_afe_get_reg(AFE_ASRC3_CON13);
	mAudioRegCache.REG_AFE_ASRC3_CON14 = mt_afe_get_reg(AFE_ASRC3_CON14);
	mAudioRegCache.REG_AFE_ADDA4_TOP_CON0 = mt_afe_get_reg(AFE_ADDA4_TOP_CON0);
	mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON0 = mt_afe_get_reg(AFE_ADDA4_UL_SRC_CON0);
	mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON1 = mt_afe_get_reg(AFE_ADDA4_UL_SRC_CON1);
	mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG0 = mt_afe_get_reg(AFE_ADDA4_NEWIF_CFG0);
	mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG1 = mt_afe_get_reg(AFE_ADDA4_NEWIF_CFG1);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_02_01 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_02_01);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_04_03 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_04_03);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_06_05 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_06_05);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_08_07 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_08_07);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_10_09 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_10_09);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_12_11 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_12_11);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_14_13 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_14_13);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_16_15 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_16_15);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_18_17 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_18_17);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_20_19 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_20_19);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_22_21 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_22_21);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_24_23 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_24_23);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_26_25 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_26_25);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_28_27 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_28_27);
	mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_30_29 = mt_afe_get_reg(AFE_ADDA4_ULCF_CFG_30_29);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return true;
}

bool afe_restore_audio_register(void)/*Restore_Audio_Register*/
{
	/* TODO? */
	return true;
}

unsigned int align64bytesize(unsigned int insize)/*Align64ByteSize*/
{
#define MAGIC_NUMBER 0xFFFFFFC0
	unsigned int align_size;

	align_size = insize & MAGIC_NUMBER;
	return align_size;
}


bool set_offload_cbk(enum Soc_Aud_Digital_Block block, void *offloadstream,
		   void (*offloadCbk)(void *stream))/*SetOffloadCbk*/
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[block];

	Mem_Block->offloadCbk = offloadCbk;
	Mem_Block->offloadstream = offloadstream;
	PRINTK_AUDDRV("%s stream:%p, callback:%p\n", __func__, offloadstream, offloadCbk);
	return true;
}

bool clr_offload_cbk(enum Soc_Aud_Digital_Block block, void *offloadstream)/*ClrOffloadCbk*/
{
	struct AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[block];

	if (Mem_Block->offloadstream != offloadstream) {
		pr_err("%s fail, original:%p, specified:%p\n", __func__, Mem_Block->offloadstream,
		       offloadstream);
		return false;
	}
	PRINTK_AUDDRV("%s %p\n", __func__, offloadstream);
	Mem_Block->offloadstream = NULL;
	return true;
}


const struct Hdmi_Clock_Setting hdmi_clock_settings[] = {
	{32000, APLL_D24, 0},	/* 32k */
	{44100, APLL_D16, 0},	/* 44.1k */
	{48000, APLL_D16, 0},	/* 48k */
	{88200, APLL_D8, 0},	/* 88.2k */
	{96000, APLL_D8, 0},	/* 96k */
	{176400, APLL_D4, 0},	/* 176.4k */
	{192000, APLL_D4, 0}	/* 192k */
};

unsigned int get_sample_rate_index(unsigned int sample_rate)
{
	switch (sample_rate) {
	case 32000:
		return 0;
	case 44100:
		return 1;
	case 48000:
		return 2;
	case 88200:
		return 3;
	case 96000:
		return 4;
	case 176400:
		return 5;
	case 192000:
		return 6;
	}
	return 0;
}

void set_hdmi_out_control(unsigned int channels)
{
	unsigned int register_value = 0;

	register_value |= (channels << 4);
	register_value |= (SOC_HDMI_INPUT_16BIT << 1);
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, register_value, 0xffffffff);
}

void set_hdmi_out_control_enable(bool enable)
{
	unsigned int register_value = 0;

	if (enable)
		register_value |= 1;

	mt_afe_set_reg(AFE_HDMI_OUT_CON0, register_value, 0x1);
}

void set_hdmi_i2s(void)
{
	unsigned int register_value = 0;

	register_value |= (SOC_HDMI_I2S_32BIT << 4);
	register_value |= (SOC_HDMI_I2S_FIRST_BIT_1T_DELAY << 3);
	register_value |= (SOC_HDMI_I2S_LRCK_NOT_INVERSE << 2);
	register_value |= (SOC_HDMI_I2S_BCLK_INVERSE << 1);
	mt_afe_set_reg(AFE_8CH_I2S_OUT_CON, register_value, 0xffffffff);
}

void set_hdmi_i2s_enable(bool enable)
{
	unsigned int register_value = 0;

	if (enable)
		register_value |= 1;

	mt_afe_set_reg(AFE_8CH_I2S_OUT_CON, register_value, 0x1);
}

void set_hdmi_clock_source(unsigned int sample_rate)
{
	unsigned int sample_rate_index = get_sample_rate_index(sample_rate);

	mt_afe_set_hdmi_clock_source(hdmi_clock_settings[sample_rate_index].SAMPLE_RATE,
				hdmi_clock_settings[sample_rate_index].CLK_APLL_SEL);
}

void set_hdmi_bck_div(unsigned int sample_rate)
{
	unsigned int register_value = 0;

	register_value |=
#ifdef HDMI_PASS_I2S_DEBUG
	(hdmi_clock_settings[get_sample_rate_index(sample_rate)].HDMI_BCK_DIV) << 8 | (0x20);
	mt_afe_set_reg(AUDIO_TOP_CON3, register_value , 0x3F20);
#else
	    (hdmi_clock_settings[get_sample_rate_index(sample_rate)].HDMI_BCK_DIV) << 8;
	mt_afe_set_reg(AUDIO_TOP_CON3, register_value, 0x3F00);
#endif
}

