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
 *   mt_soc_pcm_hp_impedance.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 impedance setting
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
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "mt_soc_codec_63xx.h"

#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <linux/time.h>
#include <mach/pmic_mt6331_6332_sw.h>
#include <cust_pmic.h>
#include <cust_battery_meter.h>
#include <linux/dma-mapping.h>

static AFE_MEM_CONTROL_T *pHp_impedance_MemControl = NULL;
//static int mPlaybackSramState = 0;
static const int DCoffsetDefault = 1622;
static const int DCoffsetVariance = 90;  // 5%

static const int mDcRangestep = 7;
static const int HpImpedancePhase1Step = 150;
static const int HpImpedancePhase2Step = 400;
static const int HpImpedancePhase1AdcValue = 1200;
static const int HpImpedancePhase2AdcValue = 7200;
static struct snd_dma_buffer *Dl1_Playback_dma_buf  = NULL;

#ifndef CONFIG_MTK_FPGA
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
#endif

/*
 *    function implementation
 */

void StartAudioPcmHardware(void);
void StopAudioPcmHardware(void);
static int mtk_soc_hp_impedance_probe(struct platform_device *pdev);
static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_pcm_hp_impedance_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_ADV_MT_FMTS,
    .rates =           SOC_HIGH_USE_RATE,
    .rate_min =     SOC_HIGH_USE_RATE_MIN,
    .rate_max =     SOC_HIGH_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
    .periods_max =    SOC_NORMAL_USE_PERIODS_MAX,
    .fifo_size =        0,
};

/*static int mtk_pcm_hp_impedance_stop(struct snd_pcm_substream *substream)
{
    PRINTK_AUDDRV("mtk_pcm_hp_impedance_stop \n");
    return 0;
}*/

static snd_pcm_uframes_t mtk_pcm_hp_impedance_pointer(struct snd_pcm_substream *substream)
{
    return 0;
}

static void SetDL1Buffer(struct snd_pcm_substream *substream,
                         struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AFE_BLOCK_T *pblock = &pHp_impedance_MemControl->rBlock;
    pblock->pucPhysBufAddr =  runtime->dma_addr;
    pblock->pucVirtBufAddr =  runtime->dma_area;
    pblock->u4BufferSize = runtime->dma_bytes;
    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;
    printk("SetDL1Buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
           pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
    // set dram address top hardware
    Afe_Set_Reg(AFE_DL1_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_DL1_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
    memset((void*)pblock->pucVirtBufAddr,0,pblock->u4BufferSize);

}


static int mtk_pcm_hp_impedance_params(struct snd_pcm_substream *substream,
                                       struct snd_pcm_hw_params *hw_params)
{
    int ret = 0;
    printk("mtk_pcm_hp_impedance_params \n");

    /* runtime->dma_bytes has to be set manually to allow mmap */
    substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

    substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
    substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
    SetDL1Buffer(substream, hw_params);

    PRINTK_AUDDRV("dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
                  substream->runtime->dma_bytes, substream->runtime->dma_area, (unsigned int)substream->runtime->dma_addr);
    return ret;
}

static int mtk_pcm_hp_impedance_hw_free(struct snd_pcm_substream *substream)
{
    PRINTK_AUDDRV("mtk_pcm_hp_impedance_hw_free \n");
    return 0;
}


static struct snd_pcm_hw_constraint_list constraints_hp_impedance_sample_rates =
{
    .count = ARRAY_SIZE(soc_high_supported_sample_rates),
    .list = soc_high_supported_sample_rates,
    .mask = 0,
};

static int mtk_pcm_hp_impedance_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    PRINTK_AUDDRV("mtk_pcm_hp_impedance_open\n");
    AudDrv_Clk_On();
    AudDrv_Emi_Clk_On();
    pHp_impedance_MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
    runtime->hw = mtk_pcm_hp_impedance_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hp_impedance_hardware , sizeof(struct snd_pcm_hardware));

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_hp_impedance_sample_rates);

    if (ret < 0)
    {
        PRINTK_AUDDRV("snd_pcm_hw_constraint_integer failed\n");
    }


    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        PRINTK_AUDDRV("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_playback_constraints\n");
    }
    else
    {

    }

    if (ret < 0)
    {
        PRINTK_AUDDRV("mtk_soc_pcm_hp_impedance_close\n");
        mtk_soc_pcm_hp_impedance_close(substream);
        return ret;
    }
    return 0;
}


bool mPrepareDone = false;
static int mtk_pcm_hp_impedance_prepare(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    printk("mtk_pcm_hp_impedance_prepare \n");
    if (mPrepareDone == false)
    {
        if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
        {
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O03);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O04);
        }
        else
        {
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O03);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O04);
        }
        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  runtime->rate);
        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false)
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
            SetI2SDacOut(substream->runtime->rate);
            SetI2SDacEnable(true);
        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
        }

        SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O03);
        SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O04);

        SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
        SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
        SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

        EnableAfe(true);
        mPrepareDone = true;
    }
    return 0;
}


static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream)
{
    printk("%s \n", __func__);
    //struct snd_pcm_runtime *runtime = substream->runtime;
    if (mPrepareDone == true)
    {
        mPrepareDone = false;
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
        SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
        EnableAfe(false);
    }
    AudDrv_Emi_Clk_Off();
    AudDrv_Clk_Off();
    return 0;
}

/*static int mtk_pcm_hp_impedance_start(struct snd_pcm_substream *substream)
{
    return 0;
}*/

static int mtk_pcm_hp_impedance_trigger(struct snd_pcm_substream *substream, int cmd)
{
    PRINTK_AUDDRV("mtk_pcm_hp_impedance_trigger cmd = %d\n", cmd);
    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            break;
    }
    return -EINVAL;
}

static int mtk_pcm_hp_impedance_copy(struct snd_pcm_substream *substream,
                                     int channel, snd_pcm_uframes_t pos,
                                     void __user *dst, snd_pcm_uframes_t count)
{
    return 0;
}

static int mtk_pcm_hp_impedance_silence(struct snd_pcm_substream *substream,
                                        int channel, snd_pcm_uframes_t pos,
                                        snd_pcm_uframes_t count)
{
    PRINTK_AUDDRV("%s \n", __func__);
    return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_hp_impedance_page(struct snd_pcm_substream *substream,
                                              unsigned long offset)
{
    PRINTK_AUDDRV("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static unsigned short mhp_impedance = 0;
static unsigned short mAuxAdc_Offset = 0;
static int Audio_HP_ImpeDance_Set(struct snd_kcontrol *kcontrol,
                                  struct snd_ctl_elem_value *ucontrol)
{
    //int Buffer_on_value = 0 , Buffer_off_value = 0, Buffer_diff = 0;
    const int off_counter = 20/*, on_counter  = 20 , Const_DC_OFFSET = 2048*/;
    //unsigned short memcontent = 0;
    printk("%s \n", __func__);
    AudDrv_Clk_On();
    // set dc value to hardware
    mhp_impedance = ucontrol->value.integer.value[0];
    msleep(1 * 1000);
    printk("3 %s \n", __func__);
    // start get adc value
    if (mAuxAdc_Offset == 0)
    {
        OpenAnalogTrimHardware(true);
        setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
        setOffsetTrimBufferGain(3);
        EnableTrimbuffer(true);
        msleep(5);
#ifndef CONFIG_MTK_FPGA
        mAuxAdc_Offset = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, off_counter, 0);
#else
        mAuxAdc_Offset = 0;
#endif

        printk("mAuxAdc_Offset= %d \n", mAuxAdc_Offset);

        memset((void *)Get_Afe_SramBase_Pointer(), ucontrol->value.integer.value[0], AFE_INTERNAL_SRAM_SIZE);
        msleep(5 * 1000);
        printk("4 %s \n", __func__);

        EnableTrimbuffer(false);
        OpenAnalogTrimHardware(false);
    }

    if (mhp_impedance  == 1)
    {
        printk("start open hp impedance setting\n");
        OpenHeadPhoneImpedanceSetting(true);
        setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
        setOffsetTrimBufferGain(3);
        EnableTrimbuffer(true);
        setHpGainZero();
        memset((void *)Get_Afe_SramBase_Pointer(), ucontrol->value.integer.value[0], AFE_INTERNAL_SRAM_SIZE);
        msleep(5 * 1000);
        printk("5 %s  \n", __func__);
    }
    else if (mhp_impedance == 0)
    {
        printk("stop hp impedance setting\n");
        OpenHeadPhoneImpedanceSetting(false);
        setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
        EnableTrimbuffer(false);
        SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
    }
    else
    {
        unsigned short  value = 0;
        for (value = 0; value < 10000 ; value += 200)
        {
            int i = 0;
            unsigned short temp = value;
            static unsigned short dcoffset = 0;
            volatile unsigned short *Sramdata = Get_Afe_SramBase_Pointer();
            printk("set sram to dc value = %d \n", temp);
            for (i = 0; i < AFE_INTERNAL_SRAM_SIZE >> 1 ; i ++)
            {
                *Sramdata = temp;
                Sramdata++;
            }
            Sramdata = Get_Afe_SramBase_Pointer();
            //printk("Sramdata = %p \n", Sramdata);
            //printk("Sramdata = 0x%x Sramdata+1 = 0x%x Sramdata+2 = 0x%x Sramdata+3 = 0x%x\n", *Sramdata++, *Sramdata++, *Sramdata++, *Sramdata++);
            //printk("Sramdata = %p \n", Sramdata);
            //printk("Sramdata = 0x%x Sramdata+1 = 0x%x Sramdata+2 = 0x%x Sramdata+3 = 0x%x\n", *Sramdata++, *Sramdata++, *Sramdata++, *Sramdata++);
            //memset((void *)Get_Afe_SramBase_Pointer(), ucontrol->value.integer.value[0], AFE_INTERNAL_SRAM_SIZE);
            msleep(20);
#ifndef CONFIG_MTK_FPGA
            dcoffset = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, off_counter, 0);
#else
            dcoffset = 0;
#endif
            printk("dcoffset= %d \n", dcoffset);
            msleep(3 * 1000);
        }
    }

    AudDrv_Clk_Off();
    return 0;
}

static int phase1table[] = {7, 13};
static unsigned short Phase1Check(unsigned short adcvalue, unsigned int adcoffset)
{
    unsigned int AdcDiff = adcvalue - adcoffset;
    if (adcvalue < adcoffset)
    {
        return 0;
    }
    if(AdcDiff > 300)
    {
        return AUDIO_HP_IMPEDANCE32;
    }
    else if (AdcDiff >= phase1table[1])
    {
        return AUDIO_HP_IMPEDANCE256;
    }
    else if ((AdcDiff >= phase1table[0]) && (AdcDiff <= phase1table[1]))
    {
        return AUDIO_HP_IMPEDANCE128;
    }
    else
    {
        return 0;
    }
}

static int phase2table[] = {10, 24};
static unsigned short Phase2Check(unsigned short adcvalue, unsigned int adcoffset)
{
    unsigned int AdcDiff = adcvalue - adcoffset;
    if (adcvalue < adcoffset)
    {
        return AUDIO_HP_IMPEDANCE16;
    }
    if (AdcDiff < phase2table[0])
    {
        return AUDIO_HP_IMPEDANCE16;
    }
    else if (AdcDiff >= phase2table[1])
    {
        return AUDIO_HP_IMPEDANCE64;
    }
    else
    {
        return AUDIO_HP_IMPEDANCE32;
    }
}

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

static unsigned short  dcinit_value = 0;
static void CheckDcinitValue(void)
{
    if (dcinit_value > (DCoffsetDefault + DCoffsetVariance))
    {
        printk("%s dcinit_value = %d\n", __func__, dcinit_value);
        dcinit_value = DCoffsetDefault;
    }
    else if (dcinit_value < (DCoffsetDefault - DCoffsetVariance))
    {
        printk("%s dcinit_value = %d\n", __func__, dcinit_value);
        dcinit_value = DCoffsetDefault;
    }
}

static void ApplyDctoDl(void)
{
    unsigned short  value = 0 , average = 0;
    unsigned short dcoffset , dcoffset2, dcoffset3;
    printk("%s\n", __func__);
    dcinit_value = DCoffsetDefault;
    for (value = 0; value <= (HpImpedancePhase2AdcValue+HpImpedancePhase2Step) ; value += HpImpedancePhase1Step)
    {
        volatile unsigned int *Sramdata = (volatile unsigned int *)Dl1_Playback_dma_buf->area;
        FillDatatoDlmemory(Sramdata , Dl1_Playback_dma_buf->bytes , value);
        // apply to dram

        // add dcvalue for phase boost
        if (value > HpImpedancePhase1AdcValue)
        {
            value += HpImpedancePhase1Step;
        }

        // save for DC =0 offset
        if (value  == 0)
        {
            //get adc value
            msleep(1);
#ifndef CONFIG_MTK_FPGA
            dcoffset = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset2 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset3 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
#else
            dcoffset = 0;
            dcoffset2 = 0;
            dcoffset3 = 0;
#endif
            average = (dcoffset + dcoffset2 + dcoffset3) / 3;
            dcinit_value = average;
            CheckDcinitValue();
            printk("dcinit_value = %d average = %d value = %d \n", dcinit_value, average, value);
        }

        // start checking
        if (value == HpImpedancePhase1AdcValue)
        {
            //get adc value
            msleep(1);
#ifndef CONFIG_MTK_FPGA
            dcoffset = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset2 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset3 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
#else
            dcoffset = 0;
            dcoffset2 = 0;
            dcoffset3 = 0;
#endif
            average = (dcoffset + dcoffset2 + dcoffset3) / 3;
            mhp_impedance = Phase1Check(average, dcinit_value);
            printk("value = %d average = %d dcinit_value = %d mhp_impedance = %d \n ", value, average, dcinit_value, mhp_impedance);
            if (mhp_impedance)
            {
                break;
            }
        }
        else if (value >= HpImpedancePhase2AdcValue)
        {
            //get adc value
            msleep(1);
#ifndef CONFIG_MTK_FPGA
            dcoffset = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset2 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
            dcoffset3 = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 5, 0);
#else
            dcoffset = 0;
            dcoffset2 = 0;
            dcoffset3 = 0;
#endif
            average = (dcoffset + dcoffset2 + dcoffset3) / 3;
            mhp_impedance = Phase2Check(average, dcinit_value);
            printk("value = %d average = %d dcinit_value = %d \n ", value, average, dcinit_value);
            break;
        }
    }
}

static int Audio_HP_ImpeDance_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("+ %s()\n", __func__);
    AudDrv_Clk_On();
    if (OpenHeadPhoneImpedanceSetting(true) == true)
    {
        setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
        setOffsetTrimBufferGain(2);
        EnableTrimbuffer(true);
        setHpGainZero();
        ApplyDctoDl();
        SetSdmLevel(AUDIO_SDM_LEVEL_MUTE);
        msleep(1);
        OpenHeadPhoneImpedanceSetting(false);
        setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
        EnableTrimbuffer(false);
        SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
    }
    else
    {
        printk("Audio_HP_ImpeDance_Get just do nothing \n");
    }
    AudDrv_Clk_Off();
    ucontrol->value.integer.value[0] = mhp_impedance;
    printk("- %s()\n", __func__);
    return 0;
}

static const struct snd_kcontrol_new Audio_snd_hp_impedance_controls[] =
{
    SOC_SINGLE_EXT("Audio HP ImpeDance Setting", SND_SOC_NOPM, 0, 65536, 0, Audio_HP_ImpeDance_Get, Audio_HP_ImpeDance_Set),
};


static struct snd_pcm_ops mtk_hp_impedance_ops =
{
    .open =     mtk_pcm_hp_impedance_open,
    .close =    mtk_soc_pcm_hp_impedance_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_pcm_hp_impedance_params,
    .hw_free =  mtk_pcm_hp_impedance_hw_free,
    .prepare =  mtk_pcm_hp_impedance_prepare,
    .trigger =  mtk_pcm_hp_impedance_trigger,
    .pointer =  mtk_pcm_hp_impedance_pointer,
    .copy =     mtk_pcm_hp_impedance_copy,
    .silence =  mtk_pcm_hp_impedance_silence,
    .page =     mtk_pcm_hp_impedance_page,
};

static struct snd_soc_platform_driver mtk_soc_platform =
{
    .ops        = &mtk_hp_impedance_ops,
    .pcm_new    = mtk_asoc_pcm_hp_impedance_new,
    .probe      = mtk_asoc_dhp_impedance_probe,
};

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev)
{
    PRINTK_AUDDRV("%s \n", __func__);

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_HP_IMPEDANCE_PCM);
    }
    PRINTK_AUDDRV("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_soc_platform);
}

static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd)
{
    int ret = 0;
    PRINTK_AUDDRV("%s\n", __func__);
    return ret;
}


static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform)
{
    PRINTK_AUDDRV("mtk_asoc_dhp_impedance_probe\n");
    //add  controls
    snd_soc_add_platform_controls(platform, Audio_snd_hp_impedance_controls,
                                  ARRAY_SIZE(Audio_snd_hp_impedance_controls));
    // allocate dram
    AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
    Dl1_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
    return 0;
}

static int mtk_hp_impedance_remove(struct platform_device *pdev)
{
    PRINTK_AUDDRV("%s \n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id Mt_soc_pcm_hp_impedance_of_ids[] =
{
    { .compatible = "mediatek,Mt_soc_pcm_hp_impedance", },
    {}
};
#endif

static struct platform_driver mtk_hp_impedance_driver =
{
    .driver = {
        .name = MT_SOC_HP_IMPEDANCE_PCM,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = Mt_soc_pcm_hp_impedance_of_ids,
#endif
    },
    .probe = mtk_soc_hp_impedance_probe,
    .remove = mtk_hp_impedance_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_hp_impedance_dev;
#endif

static int __init mtk_soc_hp_impedance_platform_init(void)
{
    int ret;
    PRINTK_AUDDRV("%s \n", __func__);
#ifndef CONFIG_OF
    soc_mtk_hp_impedance_dev = platform_device_alloc(MT_SOC_HP_IMPEDANCE_PCM, -1);
    if (!soc_mtk_hp_impedance_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtk_hp_impedance_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtk_hp_impedance_dev);
        return ret;
    }
#endif
    ret = platform_driver_register(&mtk_hp_impedance_driver);
    return ret;

}
module_init(mtk_soc_hp_impedance_platform_init);

static void __exit mtk_soc_hp_impedance_platform_exit(void)
{
    PRINTK_AUDDRV("%s \n", __func__);

    platform_driver_unregister(&mtk_hp_impedance_driver);
}
module_exit(mtk_soc_hp_impedance_platform_exit);

MODULE_DESCRIPTION("hp impedance module platform driver");
MODULE_LICENSE("GPL");


