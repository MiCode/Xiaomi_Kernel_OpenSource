/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_afe_control.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

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
#include <linux/xlog.h>
#include <mach/irqs.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/mt_reg_base.h>
#include <asm/div64.h>
#include <linux/aee.h>
#include <mach/pmic_mt6325_sw.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>
#include <mach/mt_gpio.h>
#include <mach/mt_typedefs.h>

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
//#include <asm/mach-types.h>

#include <mach/mt_gpio.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>
#include <cust_gpio_usage.h>
#include <mach/eint.h>
#include "AudDrv_Common.h"

static DEFINE_SPINLOCK(afe_control_lock);
static DEFINE_SPINLOCK(afe_sram_control_lock);


// static  variable
static bool AudioDaiBtStatus = false;
static bool AudioAdcI2SStatus = false;
static bool Audio2ndAdcI2SStatus = false;
static bool AudioMrgStatus = false;
static bool  mAudioInit = false;
static bool mVOWStatus = false;
static unsigned int MCLKFS = 128;
static AudioDigtalI2S *AudioAdcI2S = NULL;
static AudioDigtalI2S *m2ndI2S = NULL; // input
static AudioDigtalI2S *m2ndI2Sout = NULL; // output

static AudioHdmi *mHDMIOutput = NULL;
static AudioMrgIf *mAudioMrg = NULL;
static AudioDigitalDAIBT *AudioDaiBt = NULL;

static AFE_MEM_CONTROL_T   *AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_HDMI + 1] = {NULL};
static struct snd_dma_buffer *Audio_dma_buf[Soc_Aud_Digital_Block_MEM_HDMI + 1] = {NULL};

static AudioIrqMcuMode *mAudioMcuMode[Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE] = {NULL};
static AudioMemIFAttribute *mAudioMEMIF[Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK] = {NULL};

static AudioAfeRegCache mAudioRegCache;
static AudioSramManager mAudioSramManager;
const unsigned int AudioSramPlaybackFullSize = 1024 * 48;
const unsigned int AudioSramPlaybackPartialSize = 1024 * 32;
const unsigned int AudioDramPlaybackSize = 1024 * 48;
const size_t AudioSramCaptureSize = 1024 * 48;
const size_t AudioDramCaptureSize = 1024 * 48;
const size_t AudioInterruptLimiter = 100;


static bool mExternalModemStatus = false;

extern bool get_voice_bt_status(void);
extern bool get_voice_status(void);
extern void Auddrv_Read_Efuse_HPOffset(void);

// mutex lock
static DEFINE_MUTEX(afe_control_mutex);
static DEFINE_SPINLOCK(auddrv_dl1_lock);
static DEFINE_SPINLOCK(auddrv_ul1_lock);


static const uint16_t kSideToneCoefficientTable16k[] =
{
    0x049C, 0x09E8, 0x09E0, 0x089C,
    0xFF54, 0xF488, 0xEAFC, 0xEBAC,
    0xfA40, 0x17AC, 0x3D1C, 0x6028,
    0x7538
};

static const uint16_t kSideToneCoefficientTable32k[] =
{
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
static void Clear_Mem_CopySize(Soc_Aud_Digital_Block MemBlock);
static kal_uint32 Get_Mem_MaxCopySize(Soc_Aud_Digital_Block MemBlock);

static bool CheckSize(uint32 size)
{
    if (size == 0)
    {
        printk("CheckSize size = 0\n");
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
    printk("%s state= %d\n", __func__, State);
    mAudioSramManager.mMemoryState |= State;
}

void ClearSramState(unsigned int State)
{
    printk("%s state= %d\n", __func__, State);
    mAudioSramManager.mMemoryState &= (~State);
}

unsigned int GetPLaybackSramFullSize(void)
{
    return AudioSramPlaybackFullSize;
}

unsigned int GetPLaybackSramPartial(void)
{
    return AudioSramPlaybackPartialSize;
}

unsigned int GetPLaybackDramSize(void)
{
    return AudioSramCaptureSize;
}

size_t GetCaptureSramSize(void)
{
    return AudioSramCaptureSize;
}

size_t GetCaptureDramSize(void)
{
    return AudioDramCaptureSize;
}

// function get internal mode status.
bool get_internalmd_status(void)
{
    bool ret = (get_voice_bt_status() || get_voice_status());
    return (mExternalModemStatus == true) ? false : ret;
}

#if 0
void DumpMemifSubStream(void)
{
    int i = 0;
    //printk("+%s \n ", __func__);
    for (i = 0 ; i <= Soc_Aud_Digital_Block_MEM_HDMI ; i++)
    {
        substreamList *head = AFE_Mem_Control_context[i]->substreamL;
        while (head != NULL)
        {
            //printk("%s AFE_Mem_Control_context[%d] head = %p head->substream = %p \n ", __func__,i, head,head->substream);
            head  = head->next;
        }
    }
    //printk("-%s \n ", __func__);
}
#endif


static void FillDatatoDlmemory(volatile unsigned int *memorypointer, unsigned int fillsize, unsigned short value)
{
    int addr  = 0;
    unsigned int tempvalue = value;
    tempvalue = tempvalue << 16;
    tempvalue |= value;
    // set memory to DC value
    for (addr = 0; addr < (fillsize >> 2) ; addr++)
    {
        *memorypointer = tempvalue;
        memorypointer++;
    }
}

static struct snd_dma_buffer *Dl1_Playback_dma_buf  = NULL;

static void SetDL1BufferwithBuf(void)
{
#define Dl1_MAX_BUFFER_SIZE     (48*1024)

    AudDrv_Allocate_mem_Buffer(NULL, Soc_Aud_Digital_Block_MEM_DL1,  Dl1_MAX_BUFFER_SIZE);
    Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
    Afe_Set_Reg(AFE_DL1_BASE , Dl1_Playback_dma_buf->addr, 0xffffffff);
    Afe_Set_Reg(AFE_DL1_END  , Dl1_Playback_dma_buf->addr + (Dl1_MAX_BUFFER_SIZE - 1), 0xffffffff);
}


void OpenAfeDigitaldl1(bool bEnable)
{
    volatile unsigned int *Sramdata;

    if (bEnable == true)
    {
        SetDL1BufferwithBuf();
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O03);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O04);
        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  44100);
        SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O03);
        SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O04);
        SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

        Sramdata = (unsigned int *)(Dl1_Playback_dma_buf->area);
        FillDatatoDlmemory(Sramdata , Dl1_Playback_dma_buf->bytes , 0);
        msleep(5);

        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false)
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
            SetI2SDacOut(44100, false, Soc_Aud_I2S_WLEN_WLEN_16BITS);
            SetI2SDacEnable(true);
        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
        }
        EnableAfe(true);
    }
    else
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
        SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
        if (GetI2SDacEnable() == false)
        {
            SetI2SDacEnable(false);
        }
        EnableAfe(false);
    }
}

void SetExternalModemStatus(const bool bEnable)
{
    printk("%s(), mExternalModemStatus : %d => %d\n", __func__, mExternalModemStatus, bEnable);
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

bool InitAfeControl(void)
{
    int i = 0;
    printk("InitAfeControl \n");
    // first time to init , reg init.
    Auddrv_Reg_map();
    AudDrv_Clk_Power_On();
    Auddrv_Bus_Init();

    Auddrv_Read_Efuse_HPOffset();

    AfeControlMutexLock();
    // allocate memory for pointers
    if (mAudioInit == false)
    {
        mAudioInit = true;
        mAudioMrg = kzalloc(sizeof(AudioMrgIf), GFP_KERNEL);
        AudioDaiBt = kzalloc(sizeof(AudioDigitalDAIBT), GFP_KERNEL);
        AudioAdcI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
        m2ndI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
        m2ndI2Sout =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
        mHDMIOutput = kzalloc(sizeof(AudioHdmi), GFP_KERNEL);
        for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE ; i ++)
        {
            mAudioMcuMode[i] = kzalloc(sizeof(AudioIrqMcuMode)  , GFP_KERNEL);
        }
        for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK ; i ++)
        {
            mAudioMEMIF[i] = kzalloc(sizeof(AudioMemIFAttribute), GFP_KERNEL);
        }
        for (i = 0; i <= Soc_Aud_Digital_Block_MEM_HDMI ; i ++)
        {
            AFE_Mem_Control_context[i]  = kzalloc(sizeof(AFE_MEM_CONTROL_T), GFP_KERNEL);
            AFE_Mem_Control_context[i]->substreamL = NULL;
            spin_lock_init(&AFE_Mem_Control_context[i]->substream_lock);
        }
        for (i = 0; i <=  Soc_Aud_Digital_Block_MEM_HDMI ; i ++)
        {
            Audio_dma_buf[i]  = kzalloc(sizeof(Audio_dma_buf), GFP_KERNEL);
        }
    }
    AudioDaiBtStatus = false;
    AudioAdcI2SStatus = false;
    Audio2ndAdcI2SStatus = false;
    AudioMrgStatus = false;
    memset((void *)&mAudioSramManager, 0, sizeof(AudioSramManager));

    mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(44100);

    for (i = AUDIO_APLL1_DIV0 ; i <=  AUDIO_APLL2_DIV5 ; i++)
    {
        EnableI2SDivPower(i , false);
    }

    // set APLL clock setting
    AfeControlMutexUnLock();
    return true;
}

bool ResetAfeControl(void)
{
    int i = 0;
    printk("ResetAfeControl \n");
    AfeControlMutexLock();
    mAudioInit = false;
    memset((void *)(mAudioMrg), 0, sizeof(AudioMrgIf));
    memset((void *)(AudioDaiBt), 0, sizeof(AudioDigitalDAIBT));
    for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE ; i ++)
    {
        memset((void *)(mAudioMcuMode[i]), 0, sizeof(AudioIrqMcuMode));
    }
    for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK ; i ++)
    {
        memset((void *)(mAudioMEMIF[i]), 0, sizeof(AudioMemIFAttribute));
    }
    for (i = 0; i < (Soc_Aud_Digital_Block_MEM_HDMI + 1) ; i ++)
    {
        memset((void *)(AFE_Mem_Control_context[i]), 0, sizeof(AFE_MEM_CONTROL_T));
    }
    AfeControlMutexUnLock();
    return true;
}


/*****************************************************************************
 * FUNCTION
 *  Register_aud_irq
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */

bool Register_Aud_Irq(void *dev, uint32 afe_irq_number)
{
    int ret;
#ifdef CONFIG_OF
    ret = request_irq(afe_irq_number, AudDrv_IRQ_handler, IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
    if (ret)
    {
        printk("Register_Aud_Irq AFE IRQ register fail!!!\n");
    }
#else
    printk("%s dev name =%s \n", __func__, dev_name(dev));
    ret = request_irq(MT6735_AFE_MCU_IRQ_LINE, AudDrv_IRQ_handler, IRQF_TRIGGER_LOW/*IRQF_TRIGGER_FALLING*/, "Afe_ISR_Handle", dev);
#endif
    return ret;
}

static int irqcount = 0;
/*****************************************************************************
 * FUNCTION
 *  AudDrv_IRQ_handler / AudDrv_magic_tasklet
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */
irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id)
{
    //unsigned long flags;
    kal_uint32 volatile u4RegValue;
    kal_uint32 volatile u4tmpValue;
    kal_uint32 volatile u4tmpValue1;
    kal_uint32 volatile u4tmpValue2;

    AudDrv_Clk_On();
    u4RegValue = Afe_Get_Reg(AFE_IRQ_MCU_STATUS);
    u4RegValue &= 0xff;

    u4tmpValue = Afe_Get_Reg(AFE_IRQ_MCU_EN);
    u4tmpValue &= 0xff;

    u4tmpValue1 = Afe_Get_Reg(AFE_IRQ_CNT5);
    u4tmpValue1 &= 0x0003ffff;

    u4tmpValue2 = Afe_Get_Reg(AFE_IRQ_DEBUG);
    u4tmpValue2 &= 0x0003ffff;

    // here is error handle , for interrupt is trigger but not status , clear all interrupt with bit 6
    if (u4RegValue == 0)
    {
        printk("u4RegValue == 0 irqcount =0\n");
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 6 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 1 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 2 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 3 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 4 , 0xff);
        Afe_Set_Reg(AFE_IRQ_MCU_CLR, 1 << 5 , 0xff);
        irqcount++;
        if (irqcount > AudioInterruptLimiter)
        {
            SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
            SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
            irqcount = 0;
        }
        goto AudDrv_IRQ_handler_exit;
    }

    if (u4RegValue & INTERRUPT_IRQ1_MCU)
    {
        if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DL1]->mState == true)
        {
            Auddrv_DL1_Interrupt_Handler();
        }
    }
    if (u4RegValue & INTERRUPT_IRQ2_MCU)
    {
        if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL]->mState == true)
        {
            Auddrv_UL1_Interrupt_Handler();
        }
        if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_AWB]->mState == true)
        {
            Auddrv_AWB_Interrupt_Handler();
        }
        if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_DAI]->mState == true)
        {
            Auddrv_DAI_Interrupt_Handler();
        }
        if (mAudioMEMIF[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->mState == true)
        {
            Auddrv_UL2_Interrupt_Handler();
        }
    }
    if (u4RegValue & INTERRUPT_IRQ3_MCU)
    {
    }
    if (u4RegValue & INTERRUPT_IRQ4_MCU)
    {
    }
    if (u4RegValue & INTERRUPT_IRQ5_MCU)
    {
        Auddrv_HDMI_Interrupt_Handler();
    }
    // clear irq
    Afe_Set_Reg(AFE_IRQ_MCU_CLR, u4RegValue , 0xff);
AudDrv_IRQ_handler_exit:
    AudDrv_Clk_Off();
    return IRQ_HANDLED;
}

uint32 GetApllbySampleRate(uint32 SampleRate)
{
    if (SampleRate == 176400 || SampleRate == 88200 || SampleRate == 44100 || SampleRate == 22050 || SampleRate == 11025)
    {
        return Soc_Aud_APLL1;
    }
    else
    {
        return Soc_Aud_APLL2;
    }
}

void SetckSel(uint32 I2snum, uint32 SampleRate)
{
    uint32  ApllSource = GetApllbySampleRate(SampleRate);
    switch (I2snum)
    {
        case Soc_Aud_I2S0:
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 4, 1 << 4);
            break;
        case Soc_Aud_I2S1:
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 5, 1 << 5);
            break;
        case Soc_Aud_I2S2:
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 6, 1 << 6);
            break;
        case Soc_Aud_I2S3:
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, ApllSource << 7, 1 << 7);
            break;
    }
    printk("%s I2snum = %d ApllSource = %d \n", __func__, I2snum, ApllSource);

}

uint32  SetCLkMclk(uint32 I2snum, uint32 SampleRate)
{
    uint32  I2S_APll = 0;
    uint32 I2s_ck_div = 0;
    if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
    {
        I2S_APll = 22579200 * 8;
    }
    else
    {
        I2S_APll = 24576000 * 8 ;
    }

    SetckSel(I2snum, SampleRate); // set I2Sx mck source

    switch (I2snum)
    {
        case Soc_Aud_I2S0:
            I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div, 0x000000ff);
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div, 0x000000ff);
            break;
        case Soc_Aud_I2S1:
            I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 8, 0x0000ff00);
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 8, 0x0000ff00);
            break;
        case Soc_Aud_I2S2:
            I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 16, 0x00ff0000);
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 16, 0x00ff0000);
            break;
        case Soc_Aud_I2S3:
            I2s_ck_div = (I2S_APll / MCLKFS / SampleRate) - 1;
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_1, I2s_ck_div << 24, 0xff000000);
            Afe_Set_Reg(AUDIO_CLK_AUDDIV_2, I2s_ck_div << 24, 0xff000000);
            break;
    }
    printk("%s I2snum = %d I2s_ck_div = %d I2S_APll = %d\n", __func__, I2snum, I2s_ck_div, I2S_APll);
    return I2s_ck_div;
}

void  SetCLkBclk(uint32 MckDiv, uint32 SampleRate, uint32 Channels , uint32 Wlength)
{
    uint32  I2S_APll = 0;
    uint32  I2S_Bclk = 0;
    uint32 I2s_Bck_div = 0;
    printk("%s MckDiv = %dv SampleRate = %d  Channels = %d Wlength = %d\n", __func__, MckDiv, SampleRate, Channels, Wlength);
    MckDiv++;

    if (GetApllbySampleRate(SampleRate) == Soc_Aud_APLL1)
    {
        I2S_APll = 22579200 * 8;
    }
    else
    {
        I2S_APll = 24576000 * 8 ;
    }
    I2S_Bclk = SampleRate * Channels * (Wlength + 1) * 16;
    I2s_Bck_div = (I2S_APll / MckDiv) / I2S_Bclk;

    printk("%s I2S_APll = %dv I2S_Bclk = %d  I2s_Bck_div = %d\n", __func__, I2S_APll, I2S_Bclk, I2s_Bck_div);
    I2s_Bck_div--;
    // bclk dived is not in AUDDIV_3
    //Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, I2s_Bck_div, 0x0000000f);
    //Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, I2s_Bck_div << 4, 0x000000f0);
}


void EnableI2SDivPower(uint32 Diveder_name, bool bEnable)
{
    if (bEnable)
    {
        //AUDIO_APLL1_DIV0
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, 0 << Diveder_name, 1 << Diveder_name);
    }
    else
    {
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_3, 1 << Diveder_name , 1 << Diveder_name);
    }
}

void EnableApll1(bool bEnable)
{
    if (bEnable)
    {
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x3, 0x3);
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x07000000, 0x0f000000);
        AudDrv_APLL22M_Clk_On();
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0x3);
        SetClkCfg(AUDIO_CLK_CFG_6, 0x00010000, 0x00810000);
    }
    else
    {
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x07000000, 0x0f000000);
        AudDrv_APLL22M_Clk_Off();
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x3, 0x3);
        SetClkCfg(AUDIO_CLK_CFG_6, 0x00810000, 0x00810000);
    }
}

void EnableApll2(bool bEnable)
{
    if (bEnable)
    {
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0xc, 0xc);
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x70000000, 0xf0000000);
        AudDrv_APLL24M_Clk_On();
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0x0, 0xc);
        SetClkCfg(AUDIO_CLK_CFG_6, 0x01000000, 0x81000000);
    }
    else
    {
        SetClkCfg(AUDIO_CLK_CFG_7, 0x00000080, 0x00000083);
        AudDrv_APLL24M_Clk_Off();
        Afe_Set_Reg(AUDIO_CLK_AUDDIV_0, 0xc, 0xc);
        SetClkCfg(AUDIO_CLK_CFG_6, 0x81000000, 0x81000000);
    }
}

static bool CheckMemIfEnable(void)
{
    int i = 0;
    for (i = 0 ;  i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK ;  i++)
    {
        if ((mAudioMEMIF[i]->mState) ==  true)
        {
            //printk("CheckMemIfEnable == true\n");
            return true;
        }
    }
    //printk("CheckMemIfEnable == false\n");
    return false;
}


// record VOW status for AFE GPIO control
void SetVOWStatus(bool bEnable)
{
    unsigned long flags;
    if (mVOWStatus != bEnable)
    {
        spin_lock_irqsave(&afe_control_lock, flags);
        mVOWStatus = bEnable;
        printk("SetVOWStatus, mVOWStatus= %d\n", mVOWStatus);
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
static unsigned int pin_audclk, pin_audmiso, pin_audmosi;
static unsigned int pin_mode_audclk, pin_mode_audmosi, pin_mode_audmiso;
#endif
void EnableAfe(bool bEnable)
{
    unsigned long flags;
    bool MemEnable = false;
#ifdef CONFIG_OF
    int ret;
    ret = GetGPIO_Info(1, &pin_audclk, &pin_mode_audclk);
    if (ret < 0)
    {
        printk("EnableAfe GetGPIO_Info FAIL1!!! \n");
        return;
    }

    ret = GetGPIO_Info(2, &pin_audmiso, &pin_mode_audmiso);
    if (ret < 0)
    {
        printk("EnableAfe GetGPIO_Info FAIL2!!! \n");
        return;
    }

    ret = GetGPIO_Info(3, &pin_audmosi, &pin_mode_audmosi);
    if (ret < 0)
    {
        printk("EnableAfe GetGPIO_Info FAIL3!!! \n");
        return;
    }
#endif
    spin_lock_irqsave(&afe_control_lock, flags);
    MemEnable = CheckMemIfEnable();
    if (false == bEnable && false == MemEnable)
    {
        Afe_Set_Reg(AFE_DAC_CON0, 0x0, 0x0);
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
        mt_set_gpio_mode(pin_audclk, GPIO_MODE_00); //GPIO24, AUD_CLK_MOSI.
        if (mVOWStatus != true) //this GPIO only use in record and VOW
        {
            mt_set_gpio_mode(pin_audmiso, GPIO_MODE_00); //GPIO25, AUD_DAT_MISO
        }
        mt_set_gpio_mode(pin_audmosi, GPIO_MODE_00); //GPIO26, AUD_DAT_MOSI
#else
        mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_00); //GPIO24, AUD_CLK_MOSI.
        if (mVOWStatus != true) //this GPIO only use in record and VOW
        {
            mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_00); //GPIO25, AUD_DAT_MISO
        }
        mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_00); //GPIO26, AUD_DAT_MOSI
#endif
#endif
    }
    else if (true == bEnable && true == MemEnable)
    {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
        mt_set_gpio_mode(pin_audclk, GPIO_MODE_01); //GPIO24, AUD_CLK_MOSI
        if (mVOWStatus != true) //this GPIO only use in record and VOW
        {
            mt_set_gpio_mode(pin_audmiso, GPIO_MODE_01); //GPIO25, AUD_DAT_MISO
        }
        mt_set_gpio_mode(pin_audmosi, GPIO_MODE_01); //GPIO26, AUD_DAT_MOSI
#else
        mt_set_gpio_mode(GPIO_AUD_CLK_MOSI_PIN, GPIO_MODE_01); //GPIO24, AUD_CLK_MOSI
        if (mVOWStatus != true) //this GPIO only use in record and VOW
        {
            mt_set_gpio_mode(GPIO_AUD_DAT_MISO_PIN, GPIO_MODE_01); //GPIO25, AUD_DAT_MISO
        }
        mt_set_gpio_mode(GPIO_AUD_DAT_MOSI_PIN, GPIO_MODE_01); //GPIO26, AUD_DAT_MOSI
#endif
#endif
        Afe_Set_Reg(AFE_DAC_CON0, 0x1, 0x1);
    }
    spin_unlock_irqrestore(&afe_control_lock, flags);
}

uint32 SampleRateTransform(uint32 SampleRate)
{
    switch (SampleRate)
    {
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
    //printk("%s Aud_block = %d SampleRate = %d\n", __func__, Aud_block, SampleRate);
    SampleRate = SampleRateTransform(SampleRate);
    switch (Aud_block)
    {
        case Soc_Aud_Digital_Block_MEM_DL1:
        {
            Afe_Set_Reg(AFE_DAC_CON1, SampleRate , 0x0000000f);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_DL2:
        {
            Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 4 , 0x000000f0);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_I2S:
        {
            Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 8 , 0x00000f00);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_AWB:
        {
            Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 12, 0x0000f000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL:
        {
            Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 16, 0x000f0000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_DAI:
        {
            if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
            {
                Afe_Set_Reg(AFE_DAC_CON0, 0 << 24 , 3 << 24);
            }
            else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
            {
                Afe_Set_Reg(AFE_DAC_CON0, 1 << 24 , 3 << 24);
            }
            else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
            {
                Afe_Set_Reg(AFE_DAC_CON0, 2 << 24 , 3 << 24);
            }
            else
            {
                return false;
            }
            break;
        }
        case Soc_Aud_Digital_Block_MEM_MOD_DAI:
        {
            if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
            {
                Afe_Set_Reg(AFE_DAC_CON1, 0 << 30, 3 << 30);
            }
            else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
            {
                Afe_Set_Reg(AFE_DAC_CON1, 1 << 30, 3 << 30);
            }
            else
            {
                return false;
            }
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
        {
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

    //printk("SetChannels Memory_Interface = %d channels = %d\n", Memory_Interface, channel);

    switch (Memory_Interface)
    {
        case Soc_Aud_Digital_Block_MEM_DL1:
        {
            Afe_Set_Reg(AFE_DAC_CON1, bMono << 21, 1 << 21);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_AWB:
        {
            Afe_Set_Reg(AFE_DAC_CON1, bMono << 24, 1 << 24);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL:
        {
            Afe_Set_Reg(AFE_DAC_CON1, bMono << 27, 1 << 27);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
        {
            Afe_Set_Reg(AFE_DAC_CON0, bMono << 10, 1 << 10);
            break;
        }
        default:
            printk("SetChannels  Memory_Interface = %d, channel = %d, bMono = %d\n", Memory_Interface, channel, bMono);
            return false;
    }
    return true;
}


bool Set2ndI2SOutAttribute(uint32_t sampleRate)
{
    printk("+%s(), sampleRate = %d\n", __FUNCTION__, sampleRate);
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

bool Set2ndI2SOut(AudioDigtalI2S *DigtalI2S)
{
    uint32 u32AudioI2S = 0;
    memcpy((void *)m2ndI2Sout, (void *)DigtalI2S, sizeof(AudioDigtalI2S));
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
    {
        Afe_Set_Reg(AFE_I2S_CON3,  0x1, 0x1);
    }
    else
    {
        Afe_Set_Reg(AFE_I2S_CON3,  0x0, 0x1);
    }
    return true;
}

bool SetDaiBt(AudioDigitalDAIBT *mAudioDaiBt)
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
    printk("%s bEanble = %d\n", __func__, bEanble);
    if (bEanble == true)  // turn on dai bt
    {
        Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9);
        if (mAudioMrg->MrgIf_En == true)
        {
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12); // use merge
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3); // data ready
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);    //Turn on DAIBT
        }
        else  // turn on merge and daiBT
        {
            Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000); // set Mrg_I2S Samping Rate
            Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16); // set Mrg_I2S enable
            Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);    // Turn on Merge Interface
            udelay(100);
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12); // use merge
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3); // data ready
            Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);    //Turn on DAIBT
        }
        AudioDaiBt->mBT_ON  = true;
        AudioDaiBt->mDAIBT_ON = true;
        mAudioMrg->MrgIf_En = true;
    }
    else
    {
        if (mAudioMrg->Mergeif_I2S_Enable == true)
        {
            Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);    //Turn off DAIBT
        }
        else
        {
            Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);    //Turn on DAIBT
            udelay(100);
            Afe_Set_Reg(AFE_MRGIF_CON, 0 << 16, 1 << 16); // set Mrg_I2S enable
            Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);    // Turn on Merge Interface
            mAudioMrg->MrgIf_En = false;
        }
        AudioDaiBt->mBT_ON  = false;
        AudioDaiBt->mDAIBT_ON = false;
    }
    return true;
}

bool GetMrgI2SEnable()
{
    return mAudioMEMIF[Soc_Aud_Digital_Block_MRG_I2S_OUT]->mState ;
}

bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate)
{
    printk("%s bEnable = %d\n", __func__, bEnable);
    if (bEnable == true)
    {
        // To enable MrgI2S
        if (mAudioMrg->MrgIf_En == true)
        {
            // Merge Interface already turn on.
            //if sample Rate change, then it need to restart with new setting; else do nothing.
            if (mAudioMrg->Mrg_I2S_SampleRate != SampleRateTransform(sampleRate))
            {
                //Turn off Merge Interface first to switch I2S sampling rate
                Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16); // Turn off I2S
                if (AudioDaiBt->mDAIBT_ON == true)
                {
                    Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x1);    //Turn off DAIBT first
                }
                udelay(100);
                Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);    // Turn off Merge Interface
                udelay(100);
                Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);    // Turn on Merge Interface
                if (AudioDaiBt->mDAIBT_ON == true)
                {
                    Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9); // use merge
                    Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12); // use merge
                    Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3); // data ready
                    Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);    //Turn on DAIBT
                }
                mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(sampleRate);
                Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000); // set Mrg_I2S Samping Rate
                Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16); // set Mrg_I2S enable
            }
        }
        else
        {
            // turn on merge Interface from off state
            mAudioMrg->Mrg_I2S_SampleRate = SampleRateTransform(sampleRate);
            Afe_Set_Reg(AFE_MRGIF_CON, mAudioMrg->Mrg_I2S_SampleRate << 20, 0xF00000); // set Mrg_I2S Samping rates
            Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);                        // set Mrg_I2S enable
            udelay(100);
            Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);                              // Turn on Merge Interface
            udelay(100);
            if (AudioDaiBt->mDAIBT_ON == true)
            {
                Afe_Set_Reg(AFE_DAIBT_CON0, AudioDaiBt->mDAI_BT_MODE << 9, 0x1 << 9); // use merge
                Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12); // use merge
                Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3); // data ready
                Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);    //Turn on DAIBT
            }
        }
        mAudioMrg->MrgIf_En = true;
        mAudioMrg->Mergeif_I2S_Enable = true;
    }
    else
    {
        if (mAudioMrg->MrgIf_En == true)
        {
            Afe_Set_Reg(AFE_MRGIF_CON, 0, 1 << 16); // Turn off I2S
            if (AudioDaiBt->mDAIBT_ON == false)
            {
                udelay(100);
                // DAIBT also not using, then it's OK to disable Merge Interface
                Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);    // Turn off Merge Interface
                mAudioMrg->MrgIf_En = false;
            }
        }
        mAudioMrg->Mergeif_I2S_Enable = false;
    }
    return true;
}
bool Set2ndI2SAdcIn(AudioDigtalI2S *DigtalI2S)
{
    //K2 todo?
    return true;
}

bool SetI2SAdcIn(AudioDigtalI2S *DigtalI2S)
{
    uint32 Audio_I2S_Adc = 0;
    memcpy((void *)AudioAdcI2S, (void *)DigtalI2S, sizeof(AudioDigtalI2S));

    if (false == AudioAdcI2SStatus)
    {
        uint32 eSamplingRate = SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE);
        uint32 dVoiceModeSelect = 0;
        Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1); //Using Internal ADC
        if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
        {
            dVoiceModeSelect = 0;
        }
        else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
        {
            dVoiceModeSelect = 1;
        }
        else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
        {
            dVoiceModeSelect = 2;
        }
        else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_48K)
        {
            dVoiceModeSelect = 3;
        }
        else
        {
        }
        Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, (dVoiceModeSelect << 19) | (dVoiceModeSelect << 17), 0x001E0000);
        Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF); // up8x txif sat on
        Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, ((dVoiceModeSelect < 3) ? 1 : 3) << 10, 0x00000C00);
    }
    else
    {
        Afe_Set_Reg(AFE_ADDA_TOP_CON0, 1, 0x1); //Using External ADC
        Audio_I2S_Adc |= (AudioAdcI2S->mLR_SWAP << 31);
        Audio_I2S_Adc |= (AudioAdcI2S->mBuffer_Update_word << 24);
        Audio_I2S_Adc |= (AudioAdcI2S->mINV_LRCK << 23);
        Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit_test << 22);
        Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit << 21);
        Audio_I2S_Adc |= (AudioAdcI2S->mloopback << 20);
        Audio_I2S_Adc |= (SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE) << 8);
        Audio_I2S_Adc |= (AudioAdcI2S->mI2S_FMT << 3);
        Audio_I2S_Adc |= (AudioAdcI2S->mI2S_WLEN << 1);
        printk("%s Audio_I2S_Adc = 0x%x", __FUNCTION__, Audio_I2S_Adc);
        Afe_Set_Reg(AFE_I2S_CON2, Audio_I2S_Adc, MASK_ALL);
    }
    return true;
}

bool EnableSideGenHw(uint32 connection , bool direction  , bool  Enable)
{
    printk("+%s(), connection = %d, direction = %d, Enable= %d\n", __FUNCTION__, connection, direction, Enable);
    if (Enable && direction)
    {
        switch (connection)
        {
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
    }
    else if (Enable)
    {
        switch (connection)
        {
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
                if (Soc_Aud_I2S_SAMPLERATE_I2S_8K == mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate) //MD connect BT Verify (8K SamplingRate)
                {
                    Afe_Set_Reg(AFE_SGEN_CON0, 0x7c0e80e8, 0xffffffff);
                }
                else if (Soc_Aud_I2S_SAMPLERATE_I2S_16K == mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate)
                {
                    Afe_Set_Reg(AFE_SGEN_CON0, 0x7c0f00f0, 0xffffffff);
                }
                else
                {
                    Afe_Set_Reg(AFE_SGEN_CON0, 0x7c6c26c2, 0xffffffff);    //Default
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
    }
    else
    {
        //don't set [31:28] as 0 when disable sinetone HW, because it will repalce i00/i01 input with sine gen output.
        //Set 0xf is correct way to disconnect sinetone HW to any I/O.
        Afe_Set_Reg(AFE_SGEN_CON0, 0xf0000000 , 0xffffffff);
    }
    return true;
}

bool SetSideGenSampleRate(uint32 SampleRate)
{
    uint32 sine_mode_ch1 = 0;
    uint32 sine_mode_ch2 = 0;

    printk("+%s(), SampleRate = %d\n", __FUNCTION__, SampleRate);

    sine_mode_ch1 = SampleRateTransform(SampleRate) << 8;
    sine_mode_ch2 = SampleRateTransform(SampleRate) << 20;

    Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch1, 0x00000f00);
    Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch2, 0x00f00000);
    return true;
}

bool Set2ndI2SAdcEnable(bool bEnable)
{
    // K2 todo?
    return true;
}

bool SetI2SAdcEnable(bool bEnable)
{
    Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, bEnable ? 1 : 0, 0x01);
    mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState = bEnable;
    if (bEnable == true)
    {
        Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
    }
    else if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState == false &&
             mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false &&
             mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC_2]->mState == false)
    {
        Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
    }
    return true;
}

bool Set2ndI2SEnable(bool bEnable)
{
    Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
    return true;
}

bool CleanPreDistortion()
{
    //printk("%s \n", __FUNCTION__);
    Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
    Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
    return true;
}

bool SetDLSrc2(uint32 SampleRate)
{
    uint32 AfeAddaDLSrc2Con0, AfeAddaDLSrc2Con1;
    if (SampleRate == 8000)
    {
        AfeAddaDLSrc2Con0 = 0;
    }
    else if (SampleRate == 11025)
    {
        AfeAddaDLSrc2Con0 = 1;
    }
    else if (SampleRate == 12000)
    {
        AfeAddaDLSrc2Con0 = 2;
    }
    else if (SampleRate == 16000)
    {
        AfeAddaDLSrc2Con0 = 3;
    }
    else if (SampleRate == 22050)
    {
        AfeAddaDLSrc2Con0 = 4;
    }
    else if (SampleRate == 24000)
    {
        AfeAddaDLSrc2Con0 = 5;
    }
    else if (SampleRate == 32000)
    {
        AfeAddaDLSrc2Con0 = 6;
    }
    else if (SampleRate == 44100)
    {
        AfeAddaDLSrc2Con0 = 7;
    }
    else if (SampleRate == 48000)
    {
        AfeAddaDLSrc2Con0 = 8;
    }
    else
    {
        AfeAddaDLSrc2Con0 = 7;    //Default 44100
    }
    //ASSERT(0);
    if (AfeAddaDLSrc2Con0 == 0 || AfeAddaDLSrc2Con0 == 3) //8k or 16k voice mode
    {
        AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11) | (0x01 << 5);
    }
    else
    {
        AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11);
    }
    //SA suggest apply -0.3db to audio/speech path
    AfeAddaDLSrc2Con0 = AfeAddaDLSrc2Con0 | (0x01 << 1); //2013.02.22 for voice mode degrade 0.3 db
    AfeAddaDLSrc2Con1 = 0xf74f0000;

    Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, AfeAddaDLSrc2Con0, MASK_ALL);
    Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, AfeAddaDLSrc2Con1, MASK_ALL);
    return true;

}

bool SetI2SDacOut(uint32 SampleRate , bool lowjitter, bool I2SWLen)
{
    uint32 Audio_I2S_Dac = 0;
    printk("SetI2SDacOut SampleRate %d, lowjitter %d, I2SWLen %d\n",SampleRate,lowjitter,I2SWLen);
    CleanPreDistortion();
    SetDLSrc2(SampleRate);
    Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
    Audio_I2S_Dac |= (SampleRateTransform(SampleRate) << 8);
    Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
    Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
    Audio_I2S_Dac |= (I2SWLen << 1);
    Audio_I2S_Dac |= (lowjitter << 12);    // low gitter mode
    Afe_Set_Reg(AFE_I2S_CON1, Audio_I2S_Dac, MASK_ALL);
    return true;
}

bool SetHwDigitalGainMode(uint32 GainType, uint32 SampleRate, uint32 SamplePerStep)
{
    //printk("SetHwDigitalGainMode GainType = %d, SampleRate = %d, SamplePerStep= %d\n", GainType, SampleRate, SamplePerStep);
    uint32 value = 0;
    value = (SamplePerStep << 8) | (SampleRateTransform(SampleRate) << 4);
    switch (GainType)
    {
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
    printk("+%s(), GainType = %d, Enable = %d\n", __FUNCTION__, GainType, Enable);
    switch (GainType)
    {
        case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
            if (Enable)
            {
                Afe_Set_Reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);    //Let current gain be 0 to ramp up
            }
            Afe_Set_Reg(AFE_GAIN1_CON0, Enable, 0x1);
            break;
        case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
            if (Enable)
            {
                Afe_Set_Reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);    //Let current gain be 0 to ramp up
            }
            Afe_Set_Reg(AFE_GAIN2_CON0, Enable, 0x1);
            break;
        default:
            printk("%s with no match type\n", __func__);
            return false;
    }
    return true;
}

bool  SetHwDigitalGain(uint32 Gain , int GainType)
{
    printk("+%s(), Gain = 0x%x, gain type = %d\n", __func__, Gain, GainType);
    switch (GainType)
    {
        case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
            Afe_Set_Reg(AFE_GAIN1_CON1, Gain, 0xffffffff);
            break;
        case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
            Afe_Set_Reg(AFE_GAIN2_CON1, Gain, 0xffffffff);
            break;
        default:
            printk("%s with no match type\n", __func__);
            return false;
    }
    return true;
}

bool SetModemPcmConfig(int modem_index , AudioDigitalPCM p_modem_pcm_attribute)
{
    uint32 reg_pcm2_intf_con = 0;
    uint32 reg_pcm_intf_con1 = 0;
    printk("+%s()\n", __func__);
    if (modem_index == MODEM_1)
    {
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mTxLchRepeatSel     & 0x1) << 13;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mVbt16kModeSel      & 0x1) << 12;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mSingelMicSel       & 0x1) << 7;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mAsyncFifoSel       & 0x1) << 6;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmWordLength      & 0x1) << 5;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x3) << 3;
        reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmFormat          & 0x3) << 1;
        printk("%s(), PCM2_INTF_CON(0x%lx) = 0x%x\n", __FUNCTION__, PCM2_INTF_CON, reg_pcm2_intf_con);
        Afe_Set_Reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
        if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K)
        {
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
        }
        else if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_16K)
        {
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
        }
        else if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_32K)
        {
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

    }
    else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL)    // MODEM_2 use PCM_INTF_CON1 (0x530) !!!
    {
        if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K)
        {
            Afe_Set_Reg(AFE_ASRC_CON1, 0x00098580, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON4, 0x00098580, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON7, 0x0004c2c0, 0xffffffff);

            Afe_Set_Reg(AFE_ASRC4_CON1, 0x00098580, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON4, 0x00098580, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON7, 0x0004c2c0, 0xffffffff);
        }
        else if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_16K)
        {
            Afe_Set_Reg(AFE_ASRC_CON1, 0x0004c2c0, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON4, 0x0004c2c0, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON7, 0x00026160, 0xffffffff);

            Afe_Set_Reg(AFE_ASRC4_CON1, 0x0004c2c0, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON4, 0x0004c2c0, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON7, 0x00026160, 0xffffffff);
        }
        else if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_32K)
        {
            Afe_Set_Reg(AFE_ASRC_CON1, 0x00026160, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON4, 0x00026160, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC_CON7, 0x000130b0, 0xffffffff);

            Afe_Set_Reg(AFE_ASRC4_CON1, 0x00026160, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON2, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON3, 0x00400000, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON4, 0x00026160, 0xffffffff);
            Afe_Set_Reg(AFE_ASRC4_CON7, 0x000130b0, 0xffffffff);
        }

        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mBclkOutInv        & 0x01) << 22;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mTxLchRepeatSel        & 0x01) << 19;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mVbt16kModeSel         & 0x01) << 18;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtModemSel       & 0x01) << 17;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncLength  & 0x1F) << 9;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01) << 8;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSingelMicSel      & 0x01) << 7;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mAsyncFifoSel      & 0x01) << 6;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSlaveModeSel      & 0x01) << 5;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmModeWidebandSel    & 0x03) << 3;
        reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmFormat             & 0x03) << 1;

        printk("%s(), PCM_INTF_CON1(0x%lx) = 0x%x", __FUNCTION__, PCM_INTF_CON, reg_pcm_intf_con1);
        Afe_Set_Reg(PCM_INTF_CON, reg_pcm_intf_con1, MASK_ALL);

    }
    return true;
}

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on)
{
    uint32 dNeedDisableASM = 0, mPcm1AsyncFifo;
    printk("+%s(), modem_index = %d, modem_pcm_on = %d\n", __FUNCTION__, modem_index, modem_pcm_on);

    if (modem_index == MODEM_1) // MODEM_1 use PCM2_INTF_CON (0x53C) !!!
    {
        // todo:: temp for use fifo
        Afe_Set_Reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
        mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_1_O]->mState = modem_pcm_on;
    }
    else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) // MODEM_2 use PCM_INTF_CON1 (0x530) !!!
    {
        if (modem_pcm_on == true) // turn on ASRC before Modem PCM on
        {
            mPcm1AsyncFifo  = (Afe_Get_Reg(PCM_INTF_CON) & 0x0040) >> 6;
            if (mPcm1AsyncFifo == 0)
            {
                Afe_Set_Reg(AFE_ASRC_CON6, 0x005f188f, MASK_ALL);
                Afe_Set_Reg(AFE_ASRC_CON0, 0x86083031, MASK_ALL);

                Afe_Set_Reg(AFE_ASRC4_CON6, 0x005f188f, MASK_ALL);
                Afe_Set_Reg(AFE_ASRC4_CON0, 0x06003031, MASK_ALL);
            }
            Afe_Set_Reg(PCM_INTF_CON, 0x1, 0x1);
        }
        else if (modem_pcm_on == false) // turn off ASRC after Modem PCM off
        {
            Afe_Set_Reg(PCM_INTF_CON, 0x0, 0x1);

            Afe_Set_Reg(AFE_ASRC_CON6, 0x00000000, MASK_ALL);
            dNeedDisableASM  = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x1) ? 1 : 0;
            Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 4 | 1 << 5 | dNeedDisableASM));
            Afe_Set_Reg(AFE_ASRC_CON0, 0x0, 0x1);

            Afe_Set_Reg(AFE_ASRC4_CON6, 0x00000000, MASK_ALL);
            Afe_Set_Reg(AFE_ASRC4_CON0, 0, (1 << 4 | 1 << 5));
            Afe_Set_Reg(AFE_ASRC4_CON0, 0x0, 0x1);
        }
        mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_2_O]->mState = modem_pcm_on;
    }
    else
    {
        printk("%s(), no such modem_index: %d!!", __FUNCTION__, modem_index);
        return false;
    }
    return true;
}


bool EnableSideToneFilter(bool stf_on)
{

    // MD max support 16K sampling rate
    const uint8_t kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable16k) / sizeof(uint16_t);
    printk("+%s(), stf_on = %d\n", __func__, stf_on);
    AudDrv_Clk_On();

    if (stf_on == false)
    {
        // bypass STF result & disable
        const bool bypass_stf_on = true;
        uint32_t reg_value = (bypass_stf_on << 31) | (stf_on << 8);
        Afe_Set_Reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
        printk("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __FUNCTION__, AFE_SIDETONE_CON1, reg_value);

        // set side tone gain = 0
        Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
        printk("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __FUNCTION__, AFE_SIDETONE_GAIN, 0);
    }
    else
    {
        const bool bypass_stf_on = false;
        // using STF result & enable & set half tap num
        uint32_t write_reg_value = (bypass_stf_on << 31) | (stf_on << 8) | kSideToneHalfTapNum;

        // set side tone coefficient
        const bool enable_read_write = true; // enable read/write side tone coefficient
        const bool read_write_sel = true;    // for write case
        const bool sel_ch2 = false;          // using uplink ch1 as STF input
        uint32_t   read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
        size_t coef_addr = 0;

        printk("%s(), AFE_SIDETONE_GAIN[0x%lx] = 0x%x\n", __FUNCTION__, AFE_SIDETONE_GAIN, 0);
        // set side tone gain
        Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
        Afe_Set_Reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
        printk("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n", __FUNCTION__, AFE_SIDETONE_CON1, write_reg_value);

        for (coef_addr = 0; coef_addr < kSideToneHalfTapNum; coef_addr++)
        {
            bool old_write_ready = (read_reg_value >> 29) & 0x1;
            bool new_write_ready = 0;
            int try_cnt = 0;
            write_reg_value = enable_read_write << 25 |
                              read_write_sel    << 24 |
                              sel_ch2           << 23 |
                              coef_addr         << 16 |
                              kSideToneCoefficientTable16k[coef_addr];
            Afe_Set_Reg(AFE_SIDETONE_CON0, write_reg_value, 0x39FFFFF);
            printk("%s(), AFE_SIDETONE_CON0[0x%lx] = 0x%x\n", __FUNCTION__, AFE_SIDETONE_CON0, write_reg_value);

            // wait until flag write_ready changed (means write done)
            for (try_cnt = 0; try_cnt < 10; try_cnt++)  // max try 10 times
            {
                msleep(3);
                read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
                new_write_ready = (read_reg_value >> 29) & 0x1;
                if (new_write_ready != old_write_ready) // flip => ok
                {
                    break;
                }
                else
                {
                    BUG_ON(new_write_ready != old_write_ready);
                    return false;
                }
            }
        }
    }
    AudDrv_Clk_Off();
    printk("-%s(), stf_on = %d\n", __FUNCTION__, stf_on);
    return true;
}


bool SetMemoryPathEnable(uint32 Aud_block, bool bEnable)
{
    printk("%s Aud_block = %d bEnable = %d \n", __func__, Aud_block, bEnable);
    if (Aud_block >= Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
    {
        return false;
    }
    // set for counter
    if (bEnable == true)
    {
        if (mAudioMEMIF[Aud_block]->mUserCount == 0)
        {
            mAudioMEMIF[Aud_block]->mState = true;
        }
        mAudioMEMIF[Aud_block]->mUserCount++;
    }
    else
    {
        mAudioMEMIF[Aud_block]->mUserCount--;
        if (mAudioMEMIF[Aud_block]->mUserCount == 0)
        {
            mAudioMEMIF[Aud_block]->mState = false;
        }
        if (mAudioMEMIF[Aud_block]->mUserCount < 0)
        {
            mAudioMEMIF[Aud_block]->mUserCount = 0;
            printk("warning , user count <0 \n");
        }
    }
    printk("%s Aud_block = %d mAudioMEMIF[Aud_block]->mUserCount = %d \n", __func__, Aud_block, mAudioMEMIF[Aud_block]->mUserCount);

    if (Aud_block  > Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE)
    {
        return true;
    }

    if ((bEnable == true) && (mAudioMEMIF[Aud_block]->mUserCount == 1))
    {
        Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1) , 1 << (Aud_block + 1));
    }
    else if ((bEnable == false) && (mAudioMEMIF[Aud_block]->mUserCount == 0))
    {
        Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1), 1 << (Aud_block + 1));
    }

    return true;
}

bool GetMemoryPathEnable(uint32 Aud_block)
{
    if (Aud_block < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK)
    {
        return mAudioMEMIF[Aud_block]->mState;
    }
    return false;
}

bool SetI2SDacEnable(bool bEnable)
{
    printk("%s bEnable = %d", __func__, bEnable);
    if (bEnable)
    {
        Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
        Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
        Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
        Afe_Set_Reg(FPGA_CFG1, 0, 0x10);    //For FPGA Pin the same with DAC
    }
    else
    {
        Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
        Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);

        if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState == false &&
            mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false)
        {
            Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
        }
        Afe_Set_Reg(FPGA_CFG1, 1 << 4, 0x10);    //For FPGA Pin the same with DAC
    }
    return true;
}

bool GetI2SDacEnable()
{
    return mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState ;
}

bool checkUplinkMEMIfStatus()
{
    int i = 0;
    for (i = Soc_Aud_Digital_Block_MEM_VUL ; i <= Soc_Aud_Digital_Block_MEM_VUL_DATA2 ; i ++)
    {
        if (mAudioMEMIF[i]->mState  == true)
        {
            return true;
        }
    }
    return false;
}

bool SetHDMIChannels(uint32 Channels)
{
    // k2 removed
    printk("+%s(), not supported!!!\n", __func__);
    return true;
}

bool SetHDMIEnable(bool bEnable)
{
    // k2 removed
    printk("+%s(), not supported!!!\n", __func__);
    return true;
}

bool SetHDMIConnection(uint32 ConnectionState, uint32 Input , uint32 Output)
{
    // k2 removed
    return true;
}

bool SetConnection(uint32 ConnectionState, uint32 Input , uint32 Output)
{
    return SetConnectionState(ConnectionState, Input, Output);
}

bool SetIrqEnable(uint32 Irqmode, bool bEnable)
{
    //printk("+%s(), Irqmode = %d, bEnable = %d\n", __FUNCTION__, Irqmode, bEnable);
    switch (Irqmode)
    {
        case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
        {
            if (checkUplinkMEMIfStatus() == false)
            {
                Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
            }
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_EN, (bEnable << Irqmode), (1 << Irqmode));

            Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_EN, (bEnable << Irqmode), (1 << Irqmode));

            Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << 12), (1 << 12));
            break;
        }
        default:
            break;
    }
    //printk("-%s(), Irqmode = %d, bEnable = %d\n", __FUNCTION__, Irqmode, bEnable);
    return true;
}

bool SetIrqMcuSampleRate(uint32  Irqmode, uint32 SampleRate)
{
    switch (Irqmode)
    {
        case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CON, (SampleRateTransform(SampleRate) << 4), 0x000000f0);
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CON, (SampleRateTransform(SampleRate) << 8), 0x00000f00);
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CON, (SampleRateTransform(SampleRate) << 16), 0x000f0000);
            break;
        }

        default:
            return false;
    }
    return true;
}

bool SetIrqMcuCounter(uint32 Irqmode, uint32 Counter)
{
    //printk(" %s Irqmode = %d Counter = %d ", __func__, Irqmode, Counter);
    uint32 CurrentCount = 0;
    switch (Irqmode)
    {
        case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CNT1, Counter, 0xffffffff);
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
        {
            CurrentCount = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
            if (CurrentCount == 0)
            {
                Afe_Set_Reg(AFE_IRQ_MCU_CNT2, Counter, 0xffffffff);
            }
            else if ((Counter < CurrentCount))
            {
                printk("update counter latency CurrentCount = %d Counter = %d", CurrentCount, Counter);
                Afe_Set_Reg(AFE_IRQ_MCU_CNT2, Counter, 0xffffffff);
            }
            else
            {
                printk("not to add counter latency CurrentCount = %d Counter = %d", CurrentCount, Counter);
            }
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_MCU_CNT1, Counter << 20, 0xfff00000);
            break;
        }
        case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
        {
            Afe_Set_Reg(AFE_IRQ_CNT5, Counter , 0x0003ffff);  // ox3BC [0~17] , ex 24bit , stereo, 48BCKs @CNT

            break;
        }
        default:
            return false;
    }
    return true;
}

bool SetMemDuplicateWrite(uint32 InterfaceType, int dupwrite)
{
    switch (InterfaceType)
    {
        case Soc_Aud_Digital_Block_MEM_DAI:
        {
            Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 29, 1 << 29);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_MOD_DAI:
        {
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
    AudioDigtalI2S I2S2ndIn_attribute;
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

bool Set2ndI2SIn(AudioDigtalI2S *mDigitalI2S)
{
    uint32 Audio_I2S_Adc = 0;
    memcpy((void *)m2ndI2S, (void *)mDigitalI2S, sizeof(AudioDigtalI2S));
    if (!m2ndI2S->mI2S_SLAVE) //Master setting SampleRate only
    {
        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, m2ndI2S->mI2S_SAMPLERATE);
    }
    Audio_I2S_Adc |= (m2ndI2S->mINV_LRCK << 5);
    Audio_I2S_Adc |= (m2ndI2S->mI2S_FMT << 3);
    Audio_I2S_Adc |= (m2ndI2S->mI2S_SLAVE << 2);
    Audio_I2S_Adc |= (m2ndI2S->mI2S_WLEN << 1);
    Audio_I2S_Adc |= (m2ndI2S->mI2S_IN_PAD_SEL << 28);
    Audio_I2S_Adc |= 1 << 31;//Default enable phase_shift_fix for better quality
    printk("Set2ndI2SIn Audio_I2S_Adc= 0x%x", Audio_I2S_Adc);
    Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Adc, 0xfffffffe);
    if (!m2ndI2S->mI2S_SLAVE)
    {
        Afe_Set_Reg(FPGA_CFG1, 1 << 8, 0x0100);
    }
    else
    {
        Afe_Set_Reg(FPGA_CFG1, 0, 0x0100);
    }
    return true;
}

bool Set2ndI2SInEnable(bool bEnable)
{
    printk("Set2ndI2SInEnable bEnable = %d", bEnable);
    m2ndI2S->mI2S_EN = bEnable;
    Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
    mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_2]->mState = bEnable;
    return true;
}

bool SetI2SASRCConfig(bool bIsUseASRC, unsigned int dToSampleRate)
{
    printk("+%s() bIsUseASRC [%d] dToSampleRate [%d]\n", __FUNCTION__, bIsUseASRC, dToSampleRate);
    if (true == bIsUseASRC)
    {
        BUG_ON(!(dToSampleRate == 44100 || dToSampleRate == 48000));
        Afe_Set_Reg(AFE_CONN4, 0, 1 << 30);
        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, dToSampleRate);//To target sample rate
        Afe_Set_Reg(AFE_ASRC_CON13, 0, 1 << 16); //0:Stereo 1:Mono
        if (dToSampleRate == 44100)
        {
            Afe_Set_Reg(AFE_ASRC_CON14, 0xDC8000, AFE_MASK_ALL);
            Afe_Set_Reg(AFE_ASRC_CON15, 0xA00000, AFE_MASK_ALL);
            Afe_Set_Reg(AFE_ASRC_CON17, 0x1FBD, AFE_MASK_ALL);
        }
        else
        {
            Afe_Set_Reg(AFE_ASRC_CON14, 0x600000, AFE_MASK_ALL);
            Afe_Set_Reg(AFE_ASRC_CON15, 0x400000, AFE_MASK_ALL);
            Afe_Set_Reg(AFE_ASRC_CON17, 0xCB2, AFE_MASK_ALL);
        }

        Afe_Set_Reg(AFE_ASRC_CON16, 0x00075987, AFE_MASK_ALL);//Calibration setting
        Afe_Set_Reg(AFE_ASRC_CON20, 0x00001b00, AFE_MASK_ALL);//Calibration setting
    }
    else
    {
        Afe_Set_Reg(AFE_CONN4, 1 << 30, 1 << 30);
    }
    return true;
}

bool SetI2SASRCEnable(bool bEnable)
{
    if (true == bEnable)
    {
        Afe_Set_Reg(AFE_ASRC_CON0, ((1 << 6) | (1 << 0)), ((1 << 6) | (1 << 0)));
    }
    else
    {
        uint32 dNeedDisableASM  = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x0030) ? 1 : 0;
        Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 6 | dNeedDisableASM));
    }
    return true;
}

bool  SetMemIfFetchFormatPerSample(uint32 InterfaceType, uint32 eFetchFormat)
{
    mAudioMEMIF[InterfaceType]->mFetchFormatPerSample = eFetchFormat;
    /*
    printk("+%s(), InterfaceType = %d, eFetchFormat = %d, mAudioMEMIF[InterfaceType].mFetchFormatPerSample = %d\n", __FUNCTION__
           , InterfaceType, eFetchFormat, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample);*/
    switch (InterfaceType)
    {
        case Soc_Aud_Digital_Block_MEM_DL1:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 16 , 0x00030000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 12 , 0x00003000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_DL2:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 18 , 0x0000c000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_I2S:
        {
            //Afe_Set_Reg(AFE_DAC_CON1, mAudioMEMIF[InterfaceType].mSampleRate << 8 , 0x00000f00);
            printk("Unsupport MEM_I2S");
            break;
        }
        case Soc_Aud_Digital_Block_MEM_AWB:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 20, 0x00300000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 22, 0x00C00000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 14, 0x000C0000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_DAI:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 24, 0x03000000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_MOD_DAI:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 26, 0x0C000000);
            break;
        }
        case Soc_Aud_Digital_Block_MEM_HDMI:
        {
            Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, mAudioMEMIF[InterfaceType]->mFetchFormatPerSample << 28, 0x30000000);
            break;
        }
        default:
            return false;
    }
    return true;
}

bool SetoutputConnectionFormat(uint32 ConnectionFormat, uint32  Output)
{
    //printk("+%s(), Data Format = %d, Output = %d\n", __FUNCTION__, ConnectionFormat, Output);
    Afe_Set_Reg(AFE_CONN_24BIT, (ConnectionFormat << Output), (1 << Output));
    return true;
}

bool SetHDMIMCLK()
{
    uint32 mclksamplerate = mHDMIOutput->mSampleRate * 256;
    uint32 hdmi_APll = GetHDMIApLLSource();
    uint32 hdmi_mclk_div = 0;
    printk("%s \n", __func__);
    if (hdmi_APll == APLL_SOURCE_24576)
    {
        hdmi_APll = 24576000;
    }
    else
    {
        hdmi_APll = 22579200;
    }
    printk("%s hdmi_mclk_div = %d mclksamplerate = %d\n", __func__, hdmi_mclk_div, mclksamplerate);
    hdmi_mclk_div = (hdmi_APll / mclksamplerate / 2) - 1;
    mHDMIOutput->mHdmiMckDiv = hdmi_mclk_div;
    printk("%s hdmi_mclk_div = %d\n", __func__, hdmi_mclk_div);
    Afe_Set_Reg(FPGA_CFG1, hdmi_mclk_div << 24, 0x3f000000);

    SetCLkMclk(Soc_Aud_I2S3, mHDMIOutput->mSampleRate);
    return true;
}

bool SetHDMIBCLK()
{
    mHDMIOutput->mBckSamplerate = mHDMIOutput->mSampleRate * mHDMIOutput->mChannels;
    printk("%s mBckSamplerate = %d mSampleRate = %d mChannels = %d\n", __func__, mHDMIOutput->mBckSamplerate, mHDMIOutput->mSampleRate , mHDMIOutput->mChannels);
    mHDMIOutput->mBckSamplerate *= (mHDMIOutput->mI2S_WLEN + 1) * 16;
    printk("%s mBckSamplerate = %d mApllSamplerate = %d\n", __func__, mHDMIOutput->mBckSamplerate, mHDMIOutput->mApllSamplerate);
    mHDMIOutput->mHdmiBckDiv = (mHDMIOutput->mApllSamplerate / mHDMIOutput->mBckSamplerate / 2) - 1;
    printk("%s mHdmiBckDiv = %d\n", __func__, mHDMIOutput->mHdmiBckDiv);
    Afe_Set_Reg(FPGA_CFG1, (mHDMIOutput->mHdmiBckDiv) << 16, 0x00ff0000);
    return true;
}

uint32 GetHDMIApLLSource()
{
    printk("%s ApllSource = %d \n", __func__, mHDMIOutput->mApllSource);
    return mHDMIOutput->mApllSource ;
}

bool SetHDMIApLL(uint32 ApllSource)
{
    printk("%s ApllSource = %d", __func__, ApllSource);
    if (ApllSource == APLL_SOURCE_24576)
    {
        Afe_Set_Reg(FPGA_CFG1, 0 << 31, 1 << 31);
        mHDMIOutput->mApllSource = APLL_SOURCE_24576;
        mHDMIOutput->mApllSamplerate = 24576000;
    }
    else if (ApllSource == APLL_SOURCE_225792)
    {
        Afe_Set_Reg(FPGA_CFG1, 1 << 31, 1 << 31);
        mHDMIOutput->mApllSource = APLL_SOURCE_225792;
        mHDMIOutput->mApllSamplerate = 22579200;
    }
    return true;
}

bool SetHDMIdatalength(uint32 length)
{
    printk("%s length = %d\n ", __func__, length);
    mHDMIOutput->mI2S_WLEN = length;
    return true;
}

bool SetHDMIsamplerate(uint32 samplerate)
{
    uint32 SampleRateinedx = SampleRateTransform(samplerate);
    mHDMIOutput->mSampleRate = samplerate;
    printk("%s samplerate = %d \n", __func__, samplerate);
    switch (SampleRateinedx)
    {
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
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

bool SetTDMbckcycle(uint32 cycles)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

bool SetTDMChannelsSdata(uint32 channels)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

bool SetTDMDatalength(uint32 length)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

bool SetTDMI2Smode(uint32 mode)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

bool SetTDMLrckInverse(bool enable)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}
bool SetTDMBckInverse(bool enable)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}
bool SetTDMEnable(bool enable)
{
    printk("%s mt6572 not support!!!\n", __func__);
    return true;
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_Allocate_DL1_Buffer / AudDrv_Free_DL1_Buffer
 *
 * DESCRIPTION
 *  allocate DL1 Buffer
 *

******************************************************************************/
int AudDrv_Allocate_DL1_Buffer(struct device *pDev, kal_uint32 Afe_Buf_Length)
{
#ifdef AUDIO_MEMORY_SRAM
    kal_uint32 u4PhyAddr = 0;
#endif
    AFE_BLOCK_T *pblock;
    printk("%s Afe_Buf_Length = %d \n ", __func__, Afe_Buf_Length);

    pblock = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
    pblock->u4BufferSize = Afe_Buf_Length;

#ifdef AUDIO_MEMORY_SRAM
    if (Afe_Buf_Length > AFE_INTERNAL_SRAM_SIZE)
    {
        PRINTK_AUDDRV("Afe_Buf_Length > AUDDRV_DL1_MAX_BUFFER_LENGTH \n");
        return -1;
    }
#endif
    // allocate memory
    {
#ifdef AUDIO_MEMORY_SRAM
        // todo , there should be a sram manager to allocate memory for low power.
        u4PhyAddr = AFE_INTERNAL_SRAM_PHY_BASE;
        pblock->pucPhysBufAddr = u4PhyAddr;

#ifdef AUDIO_MEM_IOREMAP
        PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer length AUDIO_MEM_IOREMAP  = %d\n",
                      Afe_Buf_Length);
        pblock->pucVirtBufAddr = (kal_uint8 *)Get_Afe_SramBase_Pointer();
#else
        pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif

#else
        PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer use dram");
        pblock->pucVirtBufAddr = dma_alloc_coherent(pDev, pblock->u4BufferSize, &pblock->pucPhysBufAddr, GFP_KERNEL);
#endif
    }
    PRINTK_AUDDRV("AudDrv_Allocate_DL1_Buffer Afe_Buf_Length = %dpucVirtBufAddr = %p\n", Afe_Buf_Length, pblock->pucVirtBufAddr);

    // check 32 bytes align
    if ((pblock->pucPhysBufAddr & 0x1f) != 0)
    {
        PRINTK_AUDDRV("[Auddrv] AudDrv_Allocate_DL1_Buffer is not aligned (0x%x) \n", pblock->pucPhysBufAddr);
    }

    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;

    // set sram address top hardware
    Afe_Set_Reg(AFE_DL1_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_DL1_END  , pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);
    memset(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
    return 0;
}

int AudDrv_Allocate_mem_Buffer(struct device *pDev, Soc_Aud_Digital_Block  MemBlock, uint32 Buffer_length)
{
    switch (MemBlock)
    {
        case Soc_Aud_Digital_Block_MEM_DL1:
        case Soc_Aud_Digital_Block_MEM_DL2:
        case Soc_Aud_Digital_Block_MEM_DAI:
        case Soc_Aud_Digital_Block_MEM_AWB:
        case Soc_Aud_Digital_Block_MEM_MOD_DAI:
        case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
        case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
        case Soc_Aud_Digital_Block_MEM_HDMI:
        {
            printk("%s MemBlock =%d Buffer_length = %d\n ", __func__, MemBlock, Buffer_length);
            if (Audio_dma_buf[MemBlock]  != NULL)
            {
                printk("AudDrv_Allocate_mem_Buffer MemBlock = %d dma_alloc_coherent\n", MemBlock);
                if (Audio_dma_buf[MemBlock] ->area == NULL)
                {
                    printk("dma_alloc_coherent\n");
                    Audio_dma_buf[MemBlock] ->area = dma_alloc_coherent(pDev, Buffer_length,
                                                                        &Audio_dma_buf[MemBlock] ->addr, GFP_KERNEL);
                    if (Audio_dma_buf[MemBlock] ->area)
                    {
                        Audio_dma_buf[MemBlock] ->bytes = Buffer_length;
                    }
                }
                printk("area = %p \n", Audio_dma_buf[MemBlock] ->area);
            }
        }
        break;
        case Soc_Aud_Digital_Block_MEM_VUL:
        {
            printk("%s MemBlock =%d Buffer_length = %d\n ", __func__, MemBlock, Buffer_length);
            if (Audio_dma_buf[MemBlock]  != NULL)
            {
                printk("AudDrv_Allocate_mem_Buffer MemBlock = %d dma_alloc_coherent\n", MemBlock);
                if (Audio_dma_buf[MemBlock] ->area == NULL)
                {
                    printk("dma_alloc_coherent\n");
                    Audio_dma_buf[MemBlock] ->area = dma_alloc_coherent(pDev, Buffer_length,
                                                                        &Audio_dma_buf[MemBlock] ->addr, GFP_KERNEL);
                    if (Audio_dma_buf[MemBlock] ->area)
                    {
                        Audio_dma_buf[MemBlock] ->bytes = Buffer_length;
                    }
                }
                printk("area = %p \n", Audio_dma_buf[MemBlock] ->area);
            }
            break;
        }
        case Soc_Aud_Digital_Block_MEM_I2S:
            printk("currently not support \n");
        default:
            printk("%s not support \n", __func__);
    }
    return true;
}

AFE_MEM_CONTROL_T  *Get_Mem_ControlT(Soc_Aud_Digital_Block MemBlock)
{
    if (MemBlock >= 0  && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI)
    {
        return AFE_Mem_Control_context[MemBlock];
    }
    else
    {
        printk("%s error \n", __func__);
        return NULL;
    }
}

bool SetMemifSubStream(Soc_Aud_Digital_Block MemBlock , struct snd_pcm_substream *substream)
{
    substreamList *head;
    substreamList *temp = NULL;
    unsigned long flags;
    printk("+%s MemBlock = %d substream = %p \n ", __func__, MemBlock, substream);
    spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
    if (head == NULL) // frst item is NULL
    {
        //printk("%s head == NULL\n ", __func__);
        temp = kzalloc(sizeof(substreamList), GFP_ATOMIC);
        temp->substream = substream;
        temp->next = NULL;
        AFE_Mem_Control_context[MemBlock]->substreamL = temp;
    }
    else  // find out Null pointer
    {
        while (head->next != NULL)
        {
            head = head->next;
        }
        // head->next is NULL
        temp = kzalloc(sizeof(substreamList), GFP_ATOMIC);
        temp->substream = substream;
        temp->next = NULL;
        head->next = temp;
    }
    spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    printk("-%s MemBlock = %d \n ", __func__, MemBlock);
    //DumpMemifSubStream();
    return true;
}

bool ClearMemBlock(Soc_Aud_Digital_Block MemBlock)
{
    if (MemBlock >= 0  && MemBlock <= Soc_Aud_Digital_Block_MEM_HDMI)
    {
        AFE_BLOCK_T *pBlock = &AFE_Mem_Control_context[MemBlock]->rBlock;
        memset(pBlock->pucVirtBufAddr, 0, pBlock->u4BufferSize);
        pBlock->u4WriteIdx     = 0;
        pBlock->u4DMAReadIdx    = 0;
        pBlock->u4DataRemained  = 0;
        pBlock->u4fsyncflag     = false;
        pBlock->uResetFlag      = true;
    }
    else
    {
        printk("%s error \n", __func__);
        return NULL;
    }
    return true;
}

bool RemoveMemifSubStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)
{
    substreamList *head;
    substreamList *temp = NULL;
    unsigned long flags;
    spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    head = AFE_Mem_Control_context[MemBlock]->substreamL;
    printk("+ %s MemBlock = %d substream = %p \n ", __func__, MemBlock, substream);
    if (head == NULL) // no object
    {
        // do nothing
    }
    else
    {
        // condition for first item hit
        if (head->substream == substream)
        {
            //printk("%s head->substream = %p \n ", __func__, head->substream);
            AFE_Mem_Control_context[MemBlock]->substreamL = head->next;

            head->substream = NULL;
            kfree(head);
            head = NULL;
            //DumpMemifSubStream();
        }
        else
        {
            temp = head;
            head = head->next;
            while (head)
            {
                if (head->substream == substream)
                {
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
    spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    //DumpMemifSubStream();

    if (AFE_Mem_Control_context[MemBlock]->substreamL == NULL)
    {
        ClearMemBlock(MemBlock);
    }
    else
    {
        printk("%s substreram is not NULL MemBlock = %d \n", __func__, MemBlock);
    }

    printk("- %s MemBlock = %d \n ", __func__, MemBlock);
    return true;
}

static unsigned long dl1_flags = 0;
void Auddrv_Dl1_Spinlock_lock(void)
{
    spin_lock_irqsave(&auddrv_dl1_lock, dl1_flags);
}

void Auddrv_Dl1_Spinlock_unlock(void)
{
    spin_unlock_irqrestore(&auddrv_dl1_lock, dl1_flags);
}

static unsigned long ul1_flags = 0;
void Auddrv_UL1_Spinlock_lock(void)
{
    spin_lock_irqsave(&auddrv_ul1_lock, ul1_flags);
}
void Auddrv_UL1_Spinlock_unlock(void)
{
    spin_unlock_irqrestore(&auddrv_ul1_lock, ul1_flags);
}

void Auddrv_HDMI_Interrupt_Handler(void)  // irq5 ISR handler
{
    // K2 removed
}


void Auddrv_AWB_Interrupt_Handler(void)
{
    AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_AWB];
    kal_uint32 HW_Cur_ReadIdx = 0;
    kal_uint32 MaxCopySize = 0;
    kal_int32 Hw_Get_bytes = 0;
    substreamList *Temp = NULL;
    AFE_BLOCK_T  *mBlock = NULL;
	unsigned long flags;

    if (Mem_Block == NULL)
    {
        return;
    }
    spin_lock_irqsave(&Mem_Block->substream_lock, flags);

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) == false)
    {
        //printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB) == false, return\n ", __func__);
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    mBlock = &Mem_Block->rBlock;
    HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_AWB_CUR));
    PRINTK_AUD_AWB("Auddrv_AWB_Interrupt_Handler HW_Cur_ReadIdx = 0x%x\n ", HW_Cur_ReadIdx);

    if (CheckSize(HW_Cur_ReadIdx))
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }
    if (mBlock->pucVirtBufAddr  == NULL)
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    MaxCopySize = Get_Mem_MaxCopySize(Soc_Aud_Digital_Block_MEM_AWB);
    PRINTK_AUD_AWB("1  mBlock = %p MaxCopySize = 0x%x u4BufferSize = 0x%x \n", mBlock, MaxCopySize, mBlock->u4BufferSize);
    if (MaxCopySize)
    {
        if (MaxCopySize > mBlock->u4BufferSize)
        {
            MaxCopySize = mBlock->u4BufferSize;
        }
        mBlock->u4DataRemained -= MaxCopySize;
        mBlock->u4DMAReadIdx +=  MaxCopySize;
        mBlock->u4DMAReadIdx %=  mBlock->u4BufferSize;
        Clear_Mem_CopySize(Soc_Aud_Digital_Block_MEM_AWB);
        PRINTK_AUD_AWB("update read pointer u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, pucPhysBufAddr:0x%x mBlock->u4MaxCopySize =0x%x\n",
                       mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize);
    }

    // HW already fill in
    Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
    if (Hw_Get_bytes < 0)
    {
        Hw_Get_bytes += mBlock->u4BufferSize;
    }

    PRINTK_AUD_AWB("+Auddrv_Handle_Mem_context Hw_Get_bytes:0x%x, HW_Cur_ReadIdx:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, pucPhysBufAddr:0x%x mBlock->u4MaxCopySize =0x%x mBlock->u4DataRemained = 0x%x\n",
                   Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, mBlock->u4MaxCopySize, mBlock->u4DataRemained);

    mBlock->u4WriteIdx  += Hw_Get_bytes;
    mBlock->u4WriteIdx  %= mBlock->u4BufferSize;
    mBlock->u4DataRemained += Hw_Get_bytes;
    // buffer overflow
    if (mBlock->u4DataRemained > mBlock->u4BufferSize)
    {
        printk("Auddrv_Handle_Mem_context buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x \n",
               mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained, mBlock->u4BufferSize);
        mBlock->u4DataRemained -= mBlock->u4BufferSize;
    }
    Mem_Block->interruptTrigger = 1;

    spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);

    if (Mem_Block->substreamL != NULL)
    {
        substreamList *temp = Mem_Block->substreamL;
        while (temp != NULL)
        {
            //printk("temp = %p temp->substream = %p\n", temp, temp->substream);
            if (temp->substream != NULL)
            {
                snd_pcm_period_elapsed(temp->substream);
            }
            temp = temp->next;
        }
    }
    PRINTK_AUD_AWB("-Auddrv_Handle_Mem_context u4DMAReadIdx:0x%x, u4WriteIdx:0x%x mBlock->u4DataRemained = 0x%x \n", mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained);

}

void Auddrv_DAI_Interrupt_Handler(void)
{
    AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DAI];
    kal_uint32 HW_Cur_ReadIdx = 0;
    kal_int32 Hw_Get_bytes = 0;
    AFE_BLOCK_T  *mBlock = NULL;
    unsigned long flags;

    if (Mem_Block == NULL)
    {
        return;
    }

    spin_lock_irqsave(&Mem_Block->substream_lock, flags);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI) == false)
    {
        //printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI) == false, return\n ", __func__);
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    mBlock = &Mem_Block->rBlock;
    HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_DAI_CUR));

    if (CheckSize(HW_Cur_ReadIdx))
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }
    if (mBlock->pucVirtBufAddr  == NULL)
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    // HW already fill in
    Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
    if (Hw_Get_bytes < 0)
    {
        Hw_Get_bytes += mBlock->u4BufferSize;
    }

    PRINTK_AUD_DAI("Auddrv_DAI_Interrupt_Handler Hw_Get_bytes:0x%x, HW_Cur_ReadIdx:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, pucPhysBufAddr:0x%x Mem_Block->MemIfNum = %d \n",
                   Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

    mBlock->u4WriteIdx  += Hw_Get_bytes;
    mBlock->u4WriteIdx  %= mBlock->u4BufferSize;
    mBlock->u4DataRemained += Hw_Get_bytes;

    // buffer overflow
    if (mBlock->u4DataRemained > mBlock->u4BufferSize)
    {
        PRINTK_AUD_DAI("Auddrv_DAI_Interrupt_Handler buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x \n",
                       mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained, mBlock->u4BufferSize);
        /*
        mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
        mBlock->u4DMAReadIdx = mBlock->u4WriteIdx - mBlock->u4BufferSize / 2;
        if (mBlock->u4DMAReadIdx < 0)
        {
            mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
        }*/
    }
    Mem_Block->interruptTrigger = 1;

    spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
    if (Mem_Block->substreamL != NULL)
    {
        if (Mem_Block->substreamL->substream != NULL)
        {
            snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
        }
    }
}

void Auddrv_DL1_Interrupt_Handler(void)  // irq1 ISR handler
{
#define MAGIC_NUMBER 0xFFFFFFC0

    AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1];
    kal_int32 Afe_consumed_bytes = 0;
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    AFE_BLOCK_T *Afe_Block = &(AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->rBlock);
    unsigned long flags;

    if (Mem_Block == NULL)
    {
        return;
    }

    spin_lock_irqsave(&Mem_Block->substream_lock, flags);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == false)
    {
        //printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == false, return\n ", __func__);
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
    if (HW_Cur_ReadIdx == 0)
    {
        PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx ==0 \n");
        HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
    }
    HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

    PRINTK_AUD_DL1("[Auddrv] HW_Cur_ReadIdx=0x%x HW_memory_index = 0x%x Afe_Block->pucPhysBufAddr = 0x%x\n",
                   HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);


    // get hw consume bytes
    if (HW_memory_index > Afe_Block->u4DMAReadIdx)
    {
        Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
    }
    else
    {
        Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index - Afe_Block->u4DMAReadIdx ;
    }

    Afe_consumed_bytes = Afe_consumed_bytes & MAGIC_NUMBER;         //64 bytes align

    /*
    if ((Afe_consumed_bytes & 0x1f) != 0)
    {
        printk("[Auddrv] DMA address is not aligned 32 bytes \n");
    }*/

    PRINTK_AUD_DL1("+Auddrv_DL1_Interrupt_Handler ReadIdx:%x WriteIdx:%x, DataRemained:%x, Afe_consumed_bytes:%x HW_memory_index = %x \n",
                   Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx, Afe_Block->u4DataRemained, Afe_consumed_bytes, HW_memory_index);

    if (Afe_Block->u4DataRemained < Afe_consumed_bytes || Afe_Block->u4DataRemained <= 0 || Afe_Block->u4DataRemained  > Afe_Block->u4BufferSize)
    {
        printk("DL_Handling underflow \n");
    }
    else
    {

        PRINTK_AUD_DL1("+DL_Handling normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x \n",
                       Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);
        Afe_Block->u4DataRemained -= Afe_consumed_bytes;
        Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
        Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
    }
    AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_DL1]->interruptTrigger = 1 ;
    PRINTK_AUD_DL1("-DL_Handling normal ReadIdx:%x ,DataRemained:%x, WriteIdx:%x \n",
                   Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, Afe_Block->u4WriteIdx);

    spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
    if (Mem_Block->substreamL != NULL)
    {
        if (Mem_Block->substreamL->substream != NULL)
        {
            snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
        }
    }
}

struct snd_dma_buffer *Get_Mem_Buffer(Soc_Aud_Digital_Block MemBlock)
{
    printk("%s MemBlock = %d\n", __func__, MemBlock);
    switch (MemBlock)
    {
        case Soc_Aud_Digital_Block_MEM_DL1:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_DL2:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_VUL:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_DAI:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_AWB:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_MOD_DAI:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
            return Audio_dma_buf[MemBlock] ;
        case Soc_Aud_Digital_Block_MEM_HDMI:
            return Audio_dma_buf[MemBlock];
        case Soc_Aud_Digital_Block_MEM_I2S:
            printk("currently not support \n");
            break;
        default:
            break;
    }
    return NULL;
}

void Auddrv_UL1_Interrupt_Handler(void)
{
    AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL];

    kal_uint32 HW_Cur_ReadIdx = 0;
    kal_int32 Hw_Get_bytes = 0;
    AFE_BLOCK_T  *mBlock = NULL;
    unsigned long flags;

    if (Mem_Block == NULL)
    {
        printk("Mem_Block == NULL  \n ");
        return;
    }

    spin_lock_irqsave(&Mem_Block->substream_lock, flags);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false)
    {
        //printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL) == false, return\n ", __func__);
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    mBlock = &Mem_Block->rBlock;
    HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_VUL_CUR));
    if (CheckSize(HW_Cur_ReadIdx))
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }
    if (mBlock->pucVirtBufAddr  == NULL)
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    // HW already fill in
    Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
    if (Hw_Get_bytes < 0)
    {
        Hw_Get_bytes += mBlock->u4BufferSize;
    }

    PRINTK_AUD_UL1("Auddrv_Handle_Mem_context Hw_Get_bytes:%x, HW_Cur_ReadIdx:%x, u4DMAReadIdx:%x, u4WriteIdx:0x%x, pucPhysBufAddr:%x Mem_Block->MemIfNum = %d \n",
                   Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

    mBlock->u4WriteIdx  += Hw_Get_bytes;
    mBlock->u4WriteIdx  %= mBlock->u4BufferSize;
    mBlock->u4DataRemained += Hw_Get_bytes;

    // buffer overflow
    if (mBlock->u4DataRemained > mBlock->u4BufferSize)
    {
        printk("Auddrv_Handle_Mem_context buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x \n",
               mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained, mBlock->u4BufferSize);
    }
    AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL]->interruptTrigger = 1;

    spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
    if (Mem_Block->substreamL != NULL)
    {
        if (Mem_Block->substreamL->substream != NULL)
        {
            snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
        }
    }
}

void Clear_Mem_CopySize(Soc_Aud_Digital_Block MemBlock)
{
    substreamList *head;
    //unsigned long flags;
    //spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    head = AFE_Mem_Control_context[MemBlock]->substreamL;
    //printk("+%s MemBlock = %d \n ", __func__, MemBlock);
    while (head != NULL) // frst item is NULL
    {
        head->u4MaxCopySize = 0;
        head = head->next;
    }
    //spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    //printk("-%s MemBlock = %d \n ", __func__, MemBlock);
}

kal_uint32 Get_Mem_CopySizeByStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream)
{
    substreamList *head;
    unsigned long flags;
	kal_uint32 MaxCopySize;
    spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    head = AFE_Mem_Control_context[MemBlock]->substreamL;
    //printk("+%s MemBlock = %d \n ", __func__, MemBlock);
    while (head != NULL) // frst item is NULL
    {
        if (head->substream == substream)
        {
            MaxCopySize = head->u4MaxCopySize;
            spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
            return MaxCopySize;
        }
        head = head->next;
    }
    spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    //printk("-%s MemBlock = %d \n ", __func__, MemBlock);
    return 0;
}

kal_uint32 Get_Mem_MaxCopySize(Soc_Aud_Digital_Block MemBlock)
{
    substreamList *head;
    //unsigned long flags;
    kal_uint32 MaxCopySize;
    //spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    head = AFE_Mem_Control_context[MemBlock]->substreamL;
    MaxCopySize = 0;
    //printk("+%s MemBlock = %d \n ", __func__, MemBlock);
    while (head != NULL) // frst item is NULL
    {
        if (MaxCopySize < head->u4MaxCopySize)
        {
            MaxCopySize = head->u4MaxCopySize;
        }
        head = head->next;
    }
    //spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    //printk("-%s MemBlock = %d \n ", __func__, MemBlock);
    return MaxCopySize;
}

void Set_Mem_CopySizeByStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream, uint32 size)
{
    substreamList *head;
    unsigned long flags;
    spin_lock_irqsave(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
	head = AFE_Mem_Control_context[MemBlock]->substreamL;
    //printk("+%s MemBlock = %d \n ", __func__, MemBlock);
    while (head != NULL) // frst item is NULL
    {
        if (head->substream == substream)
        {
            head->u4MaxCopySize += size;
            break;
        }
        head = head->next;
    }
    spin_unlock_irqrestore(&AFE_Mem_Control_context[MemBlock]->substream_lock, flags);
    //printk("-%s MemBlock = %d \n ", __func__, MemBlock);
}

void Auddrv_UL2_Interrupt_Handler(void)
{
    AFE_MEM_CONTROL_T *Mem_Block = AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2];
    kal_uint32 HW_Cur_ReadIdx = 0;
    kal_int32 Hw_Get_bytes = 0;
    AFE_BLOCK_T  *mBlock = NULL;
    unsigned long flags;

    PRINTK_AUD_UL2("Auddrv_UL2_Interrupt_Handler \n ");

    if (Mem_Block == NULL)
    {
        printk("Mem_Block == NULL  \n ");
        return;
    }

    spin_lock_irqsave(&Mem_Block->substream_lock, flags);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2) == false)
    {
        //printk("%s(), GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2) == false, return\n ", __func__);
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    mBlock = &Mem_Block->rBlock;
    HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_VUL_D2_CUR));
    PRINTK_AUD_UL2("Auddrv_UL2_Interrupt_Handler HW_Cur_ReadIdx = 0x%x\n ", HW_Cur_ReadIdx);

    if (CheckSize(HW_Cur_ReadIdx))
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }
    if (mBlock->pucVirtBufAddr  == NULL)
    {
        spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
        return;
    }

    // HW already fill in
    Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;
    if (Hw_Get_bytes < 0)
    {
        Hw_Get_bytes += mBlock->u4BufferSize;
    }

    PRINTK_AUD_UL2("Auddrv_UL2_Interrupt_Handler Hw_Get_bytes:%x, HW_Cur_ReadIdx:%x, u4DMAReadIdx:%x, u4WriteIdx:0x%x, pucPhysBufAddr:%x Mem_Block->MemIfNum = %d \n",
                   Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->pucPhysBufAddr, Mem_Block->MemIfNum);

    mBlock->u4WriteIdx  += Hw_Get_bytes;
    mBlock->u4WriteIdx  %= mBlock->u4BufferSize;
    mBlock->u4DataRemained += Hw_Get_bytes;

    // buffer overflow
    if (mBlock->u4DataRemained > mBlock->u4BufferSize)
    {
        PRINTK_AUD_UL1("Auddrv_UL2_Interrupt_Handler buffer overflow u4DMAReadIdx:%x, u4WriteIdx:%x, u4DataRemained:%x, u4BufferSize:%x \n",
                       mBlock->u4DMAReadIdx, mBlock->u4WriteIdx, mBlock->u4DataRemained, mBlock->u4BufferSize);
        mBlock->u4DataRemained = mBlock->u4BufferSize / 2;
        mBlock->u4DMAReadIdx = mBlock->u4WriteIdx - mBlock->u4BufferSize / 2;
        if (mBlock->u4DMAReadIdx < 0)
        {
            mBlock->u4DMAReadIdx += mBlock->u4BufferSize;
        }
    }
    AFE_Mem_Control_context[Soc_Aud_Digital_Block_MEM_VUL_DATA2]->interruptTrigger = 1;

    spin_unlock_irqrestore(&Mem_Block->substream_lock, flags);
    if (Mem_Block->substreamL != NULL)
    {
        if (Mem_Block->substreamL->substream != NULL)
        {
            snd_pcm_period_elapsed(Mem_Block->substreamL->substream);
        }
    }
}

bool BackUp_Audio_Register(void)
{
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

    mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON0 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0);
    mAudioRegCache.REG_AFE_ADDA_DL_SRC2_CON1 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1);
    mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON0 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0);
    mAudioRegCache.REG_AFE_ADDA_UL_SRC_CON1 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1);
    mAudioRegCache.REG_AFE_ADDA_TOP_CON0 = Afe_Get_Reg(AFE_ADDA_TOP_CON0);
    mAudioRegCache.REG_AFE_ADDA_UL_DL_CON0 = Afe_Get_Reg(AFE_ADDA_UL_DL_CON0);

    mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG0 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0);
    mAudioRegCache.REG_AFE_ADDA_NEWIF_CFG1 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1);

    mAudioRegCache.REG_AFE_SIDETONE_CON0 = Afe_Get_Reg(AFE_SIDETONE_CON0);
    mAudioRegCache.REG_AFE_SIDETONE_COEFF = Afe_Get_Reg(AFE_SIDETONE_COEFF);
    mAudioRegCache.REG_AFE_SIDETONE_CON1 = Afe_Get_Reg(AFE_SIDETONE_CON1);
    mAudioRegCache.REG_AFE_SIDETONE_GAIN = Afe_Get_Reg(AFE_SIDETONE_GAIN);
    mAudioRegCache.REG_AFE_SGEN_CON0 = Afe_Get_Reg(AFE_SGEN_CON0);
    mAudioRegCache.REG_AFE_TOP_CON0 = Afe_Get_Reg(AFE_TOP_CON0);
    mAudioRegCache.REG_AFE_ADDA_PREDIS_CON0 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON0);
    mAudioRegCache.REG_AFE_ADDA_PREDIS_CON1 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON1);

    mAudioRegCache.REG_AFE_MOD_DAI_BASE = Afe_Get_Reg(AFE_MOD_DAI_BASE);
    mAudioRegCache.REG_AFE_MOD_DAI_END = Afe_Get_Reg(AFE_MOD_DAI_END);
    mAudioRegCache.REG_AFE_MOD_DAI_CUR = Afe_Get_Reg(AFE_MOD_DAI_CUR);
    mAudioRegCache.REG_AFE_IRQ_MCU_CON = Afe_Get_Reg(AFE_IRQ_MCU_CON);
    mAudioRegCache.REG_AFE_IRQ_MCU_CNT1 = Afe_Get_Reg(AFE_IRQ_MCU_CNT1);
    mAudioRegCache.REG_AFE_IRQ_MCU_CNT2 = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
    mAudioRegCache.REG_AFE_IRQ_MCU_EN = Afe_Get_Reg(AFE_IRQ_MCU_EN);
    mAudioRegCache.REG_AFE_MEMIF_MAXLEN = Afe_Get_Reg(AFE_MEMIF_MAXLEN);
    mAudioRegCache.REG_AFE_MEMIF_PBUF_SIZE = Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE);
    mAudioRegCache.REG_AFE_IRQ_MCU_CNT7 = Afe_Get_Reg(AFE_IRQ_MCU_CNT7); // K2 add

    mAudioRegCache.REG_AFE_APLL1_TUNER_CFG = Afe_Get_Reg(AFE_APLL1_TUNER_CFG);
    mAudioRegCache.REG_AFE_APLL2_TUNER_CFG = Afe_Get_Reg(AFE_APLL2_TUNER_CFG);
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
    mAudioRegCache.REG_AFE_CONN10 = Afe_Get_Reg(AFE_CONN10); // K2 add

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
    //K2 add
    mAudioRegCache.REG_AUDIO_CLK_AUDDIV_0 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_0);
    mAudioRegCache.REG_AUDIO_CLK_AUDDIV_1 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_1);
    mAudioRegCache.REG_AUDIO_CLK_AUDDIV_2 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_2);
    mAudioRegCache.REG_AUDIO_CLK_AUDDIV_3 = Afe_Get_Reg(AUDIO_CLK_AUDDIV_3);
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
    //K2 add
    mAudioRegCache.REG_AFE_ADDA4_TOP_CON0 = Afe_Get_Reg(AFE_ADDA4_TOP_CON0);
    mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON0 = Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON0);
    mAudioRegCache.REG_AFE_ADDA4_UL_SRC_CON1 = Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON1);
    mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG0 = Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG0);
    mAudioRegCache.REG_AFE_ADDA4_NEWIF_CFG1 = Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG1);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_02_01 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_02_01);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_04_03 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_04_03);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_06_05 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_06_05);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_08_07 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_08_07);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_10_09 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_10_09);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_12_11 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_12_11);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_14_13 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_14_13);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_16_15 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_16_15);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_18_17 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_18_17);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_20_19 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_20_19);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_22_21 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_22_21);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_24_23 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_24_23);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_26_25 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_26_25);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_28_27 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_28_27);
    mAudioRegCache.REG_AFE_ADDA4_ULCF_CFG_30_29 = Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_30_29);

    AudDrv_Clk_Off();
    return true;
}

bool Restore_Audio_Register(void)
{
    // K2 TODO?
    return true;
}

unsigned int Align64ByteSize(unsigned int insize)
{
#define MAGIC_NUMBER 0xFFFFFFC0

    unsigned int align_size;
    align_size = insize & MAGIC_NUMBER;
    return align_size;
}


