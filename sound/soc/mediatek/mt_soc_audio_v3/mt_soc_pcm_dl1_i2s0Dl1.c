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
 *   mt_soc_pcm_I2S0dl1.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio I2S0dl1 and Dl1 playback
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

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"

#define MAGIC_NUMBER 0xFFFFFFC0
static DEFINE_SPINLOCK(auddrv_I2S0dl1_lock);
static AFE_MEM_CONTROL_T *pI2S0dl1MemControl = NULL;
static struct snd_dma_buffer *Dl1_Playback_dma_buf  = NULL;
static unsigned int mPlaybackSramState = SRAM_STATE_FREE;

/*
 *    function implementation
 */

static int mtk_I2S0dl1_probe(struct platform_device *pdev);
static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform);

static int mI2S0dl1_hdoutput_control = false;
static bool mPrepareDone = false;

static struct device *mDev = NULL;

static const char *I2S0dl1_HD_output[] = {"Off", "On"};

static const struct soc_enum Audio_I2S0dl1_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(I2S0dl1_HD_output), I2S0dl1_HD_output),
};


static int Audio_I2S0dl1_hdoutput_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d\n", mI2S0dl1_hdoutput_control);
    ucontrol->value.integer.value[0] = mI2S0dl1_hdoutput_control;
    return 0;
}

static int Audio_I2S0dl1_hdoutput_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(I2S0dl1_HD_output))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
	
    mI2S0dl1_hdoutput_control = ucontrol->value.integer.value[0];
   
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true )
    {
        printk("return HDMI enabled\n");
        return 0;
    }
	
    if (mI2S0dl1_hdoutput_control)
    {
        // set APLL clock setting
        AudDrv_Clk_On();
        EnableApll1(true);
        EnableApll2(true);
        EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
        EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
        AudDrv_APLL1Tuner_Clk_On();
        AudDrv_APLL2Tuner_Clk_On();
    }
    else
    {
        // set APLL clock setting
        EnableApll1(false);
        EnableApll2(false);
        EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
        EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
        AudDrv_APLL1Tuner_Clk_Off();
        AudDrv_APLL2Tuner_Clk_Off();
        AudDrv_Clk_Off();
    }
    return 0;
}


static const struct snd_kcontrol_new Audio_snd_I2S0dl1_controls[] =
{
    SOC_ENUM_EXT("Audio_I2S0dl1_hd_Switch", Audio_I2S0dl1_Enum[0], Audio_I2S0dl1_hdoutput_Get, Audio_I2S0dl1_hdoutput_Set),
};

static struct snd_pcm_hardware mtk_I2S0dl1_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =   SND_SOC_ADV_MT_FMTS,
    .rates =        SOC_HIGH_USE_RATE,
    .rate_min =     SOC_HIGH_USE_RATE_MIN,
    .rate_max =     SOC_HIGH_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
    .period_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
    .periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
    .periods_max =     SOC_NORMAL_USE_PERIODS_MAX,
    .fifo_size =        0,
};


static int mtk_pcm_I2S0dl1_stop(struct snd_pcm_substream *substream)
{
    //AFE_BLOCK_T *Afe_Block = &(pI2S0dl1MemControl->rBlock);

    printk("%s \n", __func__);

    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

    // here start digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O01);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O03);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O04);

    ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);

    return 0;
}

static snd_pcm_uframes_t mtk_pcm_I2S0dl1_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    kal_uint32 Frameidx = 0;
    kal_int32 Afe_consumed_bytes = 0;
    AFE_BLOCK_T *Afe_Block = &pI2S0dl1MemControl->rBlock;
    //unsigned int flags;
    //struct snd_pcm_runtime *runtime = substream->runtime;

    Auddrv_Dl1_Spinlock_lock();
    PRINTK_AUD_DL1(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__, Afe_Block->u4DMAReadIdx);

    // get total bytes to copy
    //Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
    //return Frameidx;

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true)
    {
        HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
        if (HW_Cur_ReadIdx == 0)
        {
            PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx ==0 \n");
            HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
        }

        HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
        if (HW_memory_index >=  Afe_Block->u4DMAReadIdx)
        {
            Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
        }
        else
        {
            Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index - Afe_Block->u4DMAReadIdx ;
        }

        Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);

        Afe_Block->u4DataRemained -= Afe_consumed_bytes;
        Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
        Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
        PRINTK_AUD_DL1("[Auddrv] HW_Cur_ReadIdx =0x%x HW_memory_index = 0x%x Afe_consumed_bytes  = 0x%x\n", HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);
        Auddrv_Dl1_Spinlock_unlock();

        return audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
    }
    else
    {
        Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
        Auddrv_Dl1_Spinlock_unlock();
        return Frameidx;
    }

}


static void SetDL1Buffer(struct snd_pcm_substream *substream,
                         struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AFE_BLOCK_T *pblock = &pI2S0dl1MemControl->rBlock;
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
    memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

}

static int mtk_pcm_I2S0dl1_hw_params(struct snd_pcm_substream *substream,
                                     struct snd_pcm_hw_params *hw_params)
{
    int ret = 0;
    substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
    //printk("mtk_pcm_hw_params dma_bytes = %d\n",substream->runtime->dma_bytes);
#if 1
    if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL)
    {
        //substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE;
        substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
        substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
        AudDrv_Allocate_DL1_Buffer(mDev, substream->runtime->dma_bytes);
    }
    else
    {
        substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
        substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
        substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
        SetDL1Buffer(substream, hw_params);
    }
#else //old
    // here to allcoate sram to hardware ---------------------------
    AudDrv_Allocate_mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1, substream->runtime->dma_bytes);
#ifdef AUDIO_MEMORY_SRAM
    //substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE;
    substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
    substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
#else
    pI2S0dl1MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
    Afe_Block = &pI2S0dl1MemControl->rBlock;;

    substream->runtime->dma_area = (unsigned char *)Afe_Block->pucVirtBufAddr;
    substream->runtime->dma_addr = Afe_Block->pucPhysBufAddr;
#endif
    // -------------------------------------------------------
#endif
    printk("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
                  substream->runtime->dma_bytes, substream->runtime->dma_area, (long)substream->runtime->dma_addr);

    return ret;
}

static int mtk_pcm_I2S0dl1_hw_free(struct snd_pcm_substream *substream)
{
    return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates =
{
    .count = ARRAY_SIZE(soc_high_supported_sample_rates),
    .list = soc_high_supported_sample_rates,
    .mask = 0,
};

static int mtk_pcm_I2S0dl1_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    AfeControlSramLock();
    if (GetSramState() == SRAM_STATE_FREE)
    {
        mtk_I2S0dl1_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
        mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
        SetSramState(mPlaybackSramState);
    }
    else
    {
        mtk_I2S0dl1_hardware.buffer_bytes_max = GetPLaybackDramSize();
        mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
    }
    AfeControlSramUnLock();
    if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
    {
        AudDrv_Emi_Clk_On();
    }

    printk("mtk_I2S0dl1_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n",mtk_I2S0dl1_hardware.buffer_bytes_max,mPlaybackSramState);
    runtime->hw = mtk_I2S0dl1_hardware;

    AudDrv_Clk_On();
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_I2S0dl1_hardware , sizeof(struct snd_pcm_hardware));
    pI2S0dl1MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_sample_rates);

    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        printk("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_I2S0dl1_playback_constraints\n");
    }
    else
    {
        printk("SNDRV_PCM_STREAM_CAPTURE mtkalsa_I2S0dl1_playback_constraints\n");
    }

    if (ret < 0)
    {
        printk("ret < 0 mtk_pcm_I2S0dl1_close\n");
        mtk_pcm_I2S0dl1_close(substream);
        return ret;
    }

    //printk("mtk_pcm_I2S0dl1_open return\n");
    return 0;
}

static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream)
{
    printk("%s \n", __func__);

    if (mPrepareDone == true)
    {
        // stop DAC output
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
        if (GetI2SDacEnable() == false)
        {
            SetI2SDacEnable(false);
        }
        // stop I2S output
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);
        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
        {
            Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);
            //Afe_Set_Reg(AFE_I2S_CON, 0x0, 0x1);//K2 TODO: fix fm playback then mp3, i2s_con is misconfigured...
        }

        RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

        EnableAfe(false);
        mPrepareDone = false;
    }

    if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
    {
        AudDrv_Emi_Clk_Off();
    }
    AfeControlSramLock();
    ClearSramState(mPlaybackSramState);
    mPlaybackSramState = GetSramState();
    AfeControlSramUnLock();
    AudDrv_Clk_Off();
    return 0;
}

static int mtk_pcm_I2S0dl1_prepare(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    uint32 MclkDiv3;
    uint32 u32AudioI2S = 0;
    bool mI2SWLen;
    if (mPrepareDone == false)
    {
        printk("%s format = %d SNDRV_PCM_FORMAT_S32_LE = %d SNDRV_PCM_FORMAT_U32_LE = %d \n", __func__, runtime->format, SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_U32_LE);
        SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

        if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
        {
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O03);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O04);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O00);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O01);
            mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
        }
        else
        {
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O03);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O04);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00);
            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01);
            mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
        }

        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  runtime->rate);


        // I2S out Setting
        u32AudioI2S = SampleRateTransform(runtime->rate) << 8;
        u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; // us3 I2s format
        u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1; //32bit

        if (mI2S0dl1_hdoutput_control == true)
        {
            MclkDiv3 = SetCLkMclk(Soc_Aud_I2S1, runtime->rate); //select I2S
            MclkDiv3 = SetCLkMclk(Soc_Aud_I2S3, runtime->rate); //select I2S
            SetCLkBclk(MclkDiv3,  runtime->rate, runtime->channels, Soc_Aud_I2S_WLEN_WLEN_32BITS);
            u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12 ; //Low jitter mode
        }
        else
        {
            u32AudioI2S &=  ~(Soc_Aud_LOW_JITTER_CLOCK << 12) ;
        }

        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
            //Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Dac | 0x1, MASK_ALL); //K2 TODO: fix fm playback then mp3, i2s_con is misconfigured...
            Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
        }

        // start I2S DAC out
        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false)
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
            SetI2SDacOut(substream->runtime->rate, mI2S0dl1_hdoutput_control, mI2SWLen);
            SetI2SDacEnable(true);
        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
        }
        // here to set interrupt_distributor
        SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->period_size);
        SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);

        EnableAfe(true);
        mPrepareDone = true;
    }
    return 0;
}

static int mtk_pcm_I2S0dl1_start(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    printk("%s\n", __func__);
    // here start digital part

    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O01);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O03);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O04);

    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

    SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
    SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

    EnableAfe(true);

    return 0;
}

static int mtk_pcm_I2S0dl1_trigger(struct snd_pcm_substream *substream, int cmd)
{
    //printk("mtk_pcm_I2S0dl1_trigger cmd = %d\n", cmd);

    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_pcm_I2S0dl1_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_pcm_I2S0dl1_stop(substream);
    }
    return -EINVAL;
}

static int mtk_pcm_I2S0dl1_copy(struct snd_pcm_substream *substream,
                                int channel, snd_pcm_uframes_t pos,
                                void __user *dst, snd_pcm_uframes_t count)
{
    AFE_BLOCK_T  *Afe_Block = NULL;
    int copy_size = 0, Afe_WriteIdx_tmp;
    unsigned long flags;
    //struct snd_pcm_runtime *runtime = substream->runtime;
    char *data_w_ptr = (char *)dst;
    PRINTK_AUD_DL1("mtk_pcm_copy pos = %lu count = %lu\n ", pos, count);
    // get total bytes to copy
    count = audio_frame_to_bytes(substream , count);

    // check which memif nned to be write
    Afe_Block = &pI2S0dl1MemControl->rBlock;

    // handle for buffer management

    PRINTK_AUD_DL1(" WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x \n",
                   Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

    if (Afe_Block->u4BufferSize == 0)
    {
        printk(" u4BufferSize=0 Error");
        return 0;
    }

    spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
    copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;  //  free space of the buffer
    spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

    if (count <=  copy_size)
    {
        if (copy_size < 0)
        {
            copy_size = 0;
        }
        else
        {
            copy_size = count;
        }
    }

    copy_size = Align64ByteSize(copy_size);
    PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x \n", copy_size, (unsigned int)count);

    if (copy_size != 0)
    {
        spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
        Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
        spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

        if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) // copy once
        {
            if (!access_ok(VERIFY_READ, data_w_ptr, copy_size))
            {
                PRINTK_AUDDRV("0ptr invalid data_w_ptr=%p, size=%d", data_w_ptr, copy_size);
                PRINTK_AUDDRV(" u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_DL1("memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p copy_size = 0x%x\n",
                               Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
                if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr, copy_size))
                {
                    PRINTK_AUDDRV(" Fail copy from user \n");
                    return -1;
                }
            }

            spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
            Afe_Block->u4DataRemained += copy_size;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
            data_w_ptr += copy_size;
            count -= copy_size;

            PRINTK_AUD_DL1(" finish1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x, count=%x \r\n",
                           copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, (unsigned int)count);

        }
        else  // copy twice
        {
            kal_uint32 size_1 = 0, size_2 = 0;
            size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
            size_2 = Align64ByteSize((copy_size - size_1));
            PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x \n", size_1, size_2);
            if (!access_ok(VERIFY_READ, data_w_ptr, size_1))
            {
                printk(" 1ptr invalid data_w_ptr=%p, size_1=%d", data_w_ptr, size_1);
                printk(" u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_DL1("mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p size_1 = %x\n",
                               Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);
                if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr , size_1)))
                {
                    PRINTK_AUDDRV(" Fail 1 copy from user");
                    return -1;
                }
            }
            spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
            Afe_Block->u4DataRemained += size_1;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
            spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

            if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2))
            {
                PRINTK_AUDDRV("2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d", data_w_ptr, size_1, size_2);
                PRINTK_AUDDRV("u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_DL1("mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr+size_1 = %p size_2 = %x\n",
                               Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr + size_1, size_2);
                if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), (data_w_ptr + size_1), size_2)))
                {
                    PRINTK_AUDDRV("AudDrv_write Fail 2  copy from user");
                    return -1;
                }
            }
            spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);

            Afe_Block->u4DataRemained += size_2;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
            count -= copy_size;
            data_w_ptr += copy_size;

            PRINTK_AUD_DL1(" finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
                           copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
        }
    }
    PRINTK_AUD_DL1("pcm_copy return \n");
    return 0;
}

static int mtk_pcm_I2S0dl1_silence(struct snd_pcm_substream *substream,
                                   int channel, snd_pcm_uframes_t pos,
                                   snd_pcm_uframes_t count)
{
    printk("%s \n", __func__);
    return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_I2S0dl1_pcm_page(struct snd_pcm_substream *substream,
                                         unsigned long offset)
{
    printk("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_I2S0dl1_ops =
{
    .open =     mtk_pcm_I2S0dl1_open,
    .close =    mtk_pcm_I2S0dl1_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_pcm_I2S0dl1_hw_params,
    .hw_free =  mtk_pcm_I2S0dl1_hw_free,
    .prepare =  mtk_pcm_I2S0dl1_prepare,
    .trigger =  mtk_pcm_I2S0dl1_trigger,
    .pointer =  mtk_pcm_I2S0dl1_pointer,
    .copy =     mtk_pcm_I2S0dl1_copy,
    .silence =  mtk_pcm_I2S0dl1_silence,
    .page =     mtk_I2S0dl1_pcm_page,
};

static struct snd_soc_platform_driver mtk_I2S0dl1_soc_platform =
{
    .ops        = &mtk_I2S0dl1_ops,
    .pcm_new    = mtk_asoc_pcm_I2S0dl1_new,
    .probe      = mtk_afe_I2S0dl1_probe,
};

static int mtk_I2S0dl1_probe(struct platform_device *pdev)
{
    printk("%s \n", __func__);

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_I2S0DL1_PCM);
    }

    printk("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
 
    mDev = &pdev->dev;

    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_I2S0dl1_soc_platform);
}

static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd)
{
    int ret = 0;
    printk("%s\n", __func__);
    return ret;
}


static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform)
{
    printk("mtk_afe_I2S0dl1_probe\n");
    snd_soc_add_platform_controls(platform, Audio_snd_I2S0dl1_controls,
                                  ARRAY_SIZE(Audio_snd_I2S0dl1_controls));
    // allocate dram
    AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
    Dl1_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
    return 0;
}

static int mtk_I2S0dl1_remove(struct platform_device *pdev)
{
    printk("%s \n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_i2s0Dl1_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_dl1_i2s0Dl1", },
    {}
};
#endif

static struct platform_driver mtk_I2S0dl1_driver =
{
    .driver = {
        .name = MT_SOC_I2S0DL1_PCM,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_dl1_i2s0Dl1_of_ids,
#endif
    },
    .probe = mtk_I2S0dl1_probe,
    .remove = mtk_I2S0dl1_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkI2S0dl1_dev;
#endif

static int __init mtk_I2S0dl1_soc_platform_init(void)
{
    int ret;
    printk("%s \n", __func__);
#ifndef CONFIG_OF
    soc_mtkI2S0dl1_dev = platform_device_alloc(MT_SOC_I2S0DL1_PCM, -1);
    if (!soc_mtkI2S0dl1_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtkI2S0dl1_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtkI2S0dl1_dev);
        return ret;
    }
#endif

    ret = platform_driver_register(&mtk_I2S0dl1_driver);
    return ret;

}
module_init(mtk_I2S0dl1_soc_platform_init);

static void __exit mtk_I2S0dl1_soc_platform_exit(void)
{
    printk("%s \n", __func__);

    platform_driver_unregister(&mtk_I2S0dl1_driver);
}
module_exit(mtk_I2S0dl1_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");


