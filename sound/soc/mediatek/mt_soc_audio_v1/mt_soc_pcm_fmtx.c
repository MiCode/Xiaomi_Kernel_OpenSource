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
 *   mt_soc_pcm_fmtx.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio fmtx data1 playback
 *
 * Author:
 * -------
 *
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

#include <mach/mtk_wcn_cmb_stub.h>
//extern  int mtk_wcn_cmb_stub_audio_ctrl(CMB_STUB_AIF_X state);


static AFE_MEM_CONTROL_T *pMemControl = NULL;
static bool fake_buffer = 1;
static struct snd_dma_buffer *FMTX_Playback_dma_buf  = NULL;
static unsigned int mPlaybackSramState = SRAM_STATE_FREE;

static DEFINE_SPINLOCK(auddrv_FMTxCtl_lock);

static struct device *mDev = NULL;

/*
 *    function implementation
 */

void StartAudioPcmHardware(void);
void StopAudioPcmHardware(void);
static int mtk_fmtx_probe(struct platform_device *pdev);
static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_fmtx_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_fmtx_probe(struct snd_soc_platform *platform);

static int fmtx_hdoutput_control = true;

static const char *fmtx_HD_output[] = {"Off", "On"};

static const struct soc_enum Audio_fmtx_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fmtx_HD_output), fmtx_HD_output),
};


static int Audio_fmtx_hdoutput_Get(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d\n", fmtx_hdoutput_control);
    ucontrol->value.integer.value[0] = fmtx_hdoutput_control;
    return 0;
}

static int Audio_fmtx_hdoutput_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(fmtx_HD_output))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    if ((ucontrol->value.integer.value[0] == true) &&(fmtx_hdoutput_control == false))
    {
        // set APLL clock setting
        EnableApll1(true);
        EnableApll2(true);
        EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
        EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
        AudDrv_APLL1Tuner_Clk_On();
        AudDrv_APLL2Tuner_Clk_On();
    }
    else if ((ucontrol->value.integer.value[0] == false) &&(fmtx_hdoutput_control == true))
    {
        // set APLL clock setting
        EnableApll1(false);
        EnableApll2(false);
        EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
        EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
        AudDrv_APLL1Tuner_Clk_Off();
        AudDrv_APLL2Tuner_Clk_Off();
    }

    fmtx_hdoutput_control = ucontrol->value.integer.value[0];
    return 0;
}


static const struct snd_kcontrol_new Audio_snd_fmtx_controls[] =
{
    SOC_ENUM_EXT("Audio_FMTX_hd_Switch", Audio_fmtx_Enum[0], Audio_fmtx_hdoutput_Get, Audio_fmtx_hdoutput_Set),
};


static struct snd_pcm_hardware mtk_fmtx_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_ADV_MT_FMTS,
    .rates =        SOC_HIGH_USE_RATE,
    .rate_min =     SOC_HIGH_USE_RATE_MIN,
    .rate_max =     SOC_NORMAL_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min =      MIN_PERIOD_SIZE,
    .periods_max =      MAX_PERIOD_SIZE,
    .fifo_size =        0,
};

static int mtk_pcm_fmtx_stop(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;

    //AFE_BLOCK_T *Afe_Block = &(pMemControl->rBlock);
    PRINTK_AUD_FMTX("mtk_pcm_fmtx_stop \n");

    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

    // here to turn off digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O01);

    //    if (GetMrgI2SEnable() == false)
    //    {
    SetMrgI2SEnable(false, runtime->rate);
    //    }

    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

    SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, false);

    Set2ndI2SOutEnable(false);

    EnableAfe(false);

    RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
    AudDrv_Clk_Off();

    return 0;
}

static snd_pcm_uframes_t mtk_pcm_fmtx_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    kal_uint32 Frameidx = 0;
    kal_int32 Afe_consumed_bytes = 0;

    AFE_BLOCK_T *Afe_Block = &pMemControl->rBlock;
    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_pointer] Afe_Block->u4DMAReadIdx = 0x%x\n", Afe_Block->u4DMAReadIdx);

    Auddrv_Dl1_Spinlock_lock();

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true)
    {
        HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
        if (HW_Cur_ReadIdx == 0)
        {
            PRINTK_AUD_FMTX("[Auddrv] HW_Cur_ReadIdx ==0 \n");
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
        PRINTK_AUD_DL1("[Auddrv] HW_Cur_ReadIdx =0x%x HW_memory_index = 0x%x Afe_consumed_bytes	= 0x%x\n", HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);
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


static void SetFMTXBuffer(struct snd_pcm_substream *substream,
                          struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AFE_BLOCK_T *pblock = &pMemControl->rBlock;
    pblock->pucPhysBufAddr =  runtime->dma_addr;
    pblock->pucVirtBufAddr =  runtime->dma_area;
    pblock->u4BufferSize = runtime->dma_bytes;
    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;
    printk("SetFMTXBuffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
           pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
    // set dram address top hardware
    Afe_Set_Reg(AFE_DL1_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_DL1_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
    memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

}


static int mtk_pcm_fmtx_hw_params(struct snd_pcm_substream *substream,
                                  struct snd_pcm_hw_params *hw_params)
{
    int ret = 0;
    substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
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
        substream->runtime->dma_area = FMTX_Playback_dma_buf->area;
        substream->runtime->dma_addr = FMTX_Playback_dma_buf->addr;
        SetFMTXBuffer(substream, hw_params);
    }
    // -------------------------------------------------------
    printk("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
           substream->runtime->dma_bytes, substream->runtime->dma_area, (long)substream->runtime->dma_addr);

    return ret;
}



#if 0
static int mtk_pcm_fmtx_hw_params(struct snd_pcm_substream *substream,
                                  struct snd_pcm_hw_params *hw_params)
{
    struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
    int ret = 0;
    PRINTK_AUD_FMTX("mtk_pcm_fmtx_hw_params \n");
    if (fake_buffer)
    {
        /* runtime->dma_bytes has to be set manually to allow mmap */
        substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

        // here to allcoate sram to hardware ---------------------------
        AudDrv_Allocate_mem_Buffer(mDev, Soc_Aud_Digital_Block_MEM_DL1, substream->runtime->dma_bytes);
        substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
        substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;

        // -------------------------------------------------------
        PRINTK_AUD_FMTX("1 dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
                        substream->runtime->dma_bytes, substream->runtime->dma_area, (unsigned int)substream->runtime->dma_addr);
        return 0;
    }
    else
    {
        dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
        dma_buf->dev.dev = substream->pcm->card->dev;
        dma_buf->private_data = NULL;
        ret =  snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
    }
    PRINTK_AUD_FMTX("2 dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
                    substream->runtime->dma_bytes, substream->runtime->dma_area, (unsigned int)substream->runtime->dma_addr);
    return ret;
}
#endif


static int mtk_pcm_fmtx_hw_free(struct snd_pcm_substream *substream)
{
    PRINTK_AUD_FMTX("mtk_pcm_fmtx_hw_free \n");
    if (fake_buffer)
    {
        return 0;
    }
    return snd_pcm_lib_free_pages(substream);
}


static struct snd_pcm_hw_constraint_list constraints_fmtx_sample_rates =
{
    .count = ARRAY_SIZE(soc_fm_supported_sample_rates),
    .list = soc_fm_supported_sample_rates,
    .mask = 0,
};

#if 0
static int mtk_pcm_fmtx_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    PRINTK_AUD_FMTX("mtk_pcm_fmtx_open\n");
    AudDrv_Clk_On();

    // get dl1 memconptrol and record substream
    pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
    runtime->hw = mtk_fmtx_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_fmtx_hardware , sizeof(struct snd_pcm_hardware));

    //PRINTK_AUDDRV("runtime->hw->rates= 0x%x mtk_pcm_hardware = = 0x%x\n ", runtime->hw.rates, &mtk_pcm_hardware);

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_fmtx_sample_rates);
    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

    if (ret < 0)
    {
        PRINTK_AUD_FMTX("[mtk_pcm_fmtx_open]snd_pcm_hw_constraint_integer failed\n");
    }
    //print for hw pcm information
    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_open] runtime rate = %d channels = %d substream->pcm->device = %d\n",
                    runtime->rate, runtime->channels, substream->pcm->device);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        PRINTK_AUD_FMTX("[mtk_pcm_fmtx_open]SNDRV_PCM_FMTX_PLAYBACK mtkalsa_playback_constraints\n");
    }
    else
    {

    }

    if (ret < 0)
    {
        PRINTK_AUD_FMTX("[mtk_pcm_fmtx_open]mtk_pcm_fmtx_close\n");
        mtk_pcm_fmtx_close(substream);
        return ret;
    }
    //PRINTK_AUDDRV("mtk_pcm_open return\n");
    return 0;
}
#endif

static int mtk_pcm_fmtx_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    AfeControlSramLock();
    if (GetSramState() == SRAM_STATE_FREE)
    {
        mtk_fmtx_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
        mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
        SetSramState(mPlaybackSramState);
    }
    else
    {
        mtk_fmtx_hardware.buffer_bytes_max = GetPLaybackDramSize();
        mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
    }
    AfeControlSramUnLock();
    if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
    {
        AudDrv_Emi_Clk_On();
    }

    printk("mtk_I2S0dl1_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n", mtk_fmtx_hardware.buffer_bytes_max, mPlaybackSramState);
    runtime->hw = mtk_fmtx_hardware;

    AudDrv_Clk_On();
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_fmtx_hardware , sizeof(struct snd_pcm_hardware));
    pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_fmtx_sample_rates);

    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        printk("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_fmtx_playback_constraints\n");
    }
    else
    {
        printk("SNDRV_PCM_STREAM_CAPTURE mtkalsa_fmtx_playback_constraints\n");
    }

    if (ret < 0)
    {
        printk("ret < 0 mtkalsa_fmtx_playback close\n");
        mtk_pcm_fmtx_close(substream);
        return ret;
    }
    //printk("mtk_pcm_I2S0dl1_open return\n");
    return 0;
}



static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream)
{
    PRINTK_AUD_FMTX("%s \n", __func__);
    //   mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_0);

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

static int mtk_pcm_fmtx_prepare(struct snd_pcm_substream *substream)
{
    return 0;
}

static int mtk_pcm_fmtx_start(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AudDrv_Clk_On();

    //    mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2);

    SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
    if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
    {
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00); // FM Tx only support 16 bit
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01); // FM Tx only support 16 bit
    }
    else
    {
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01);
    }

    // here start digital part
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O01);

    // set dl1 sample ratelimit_state
    SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
    SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);

    // start MRG I2S Out
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, true);
    SetMrgI2SEnable(true, runtime->rate) ;

    // start 2nd I2S Out

    Set2ndI2SOutAttribute(runtime->rate) ;
    Set2ndI2SOutEnable(true);

    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

    // here to set interrupt
    SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, (runtime->period_size * 2 / 3));
    SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

    EnableAfe(true);

    return 0;
}

static int mtk_pcm_fmtx_trigger(struct snd_pcm_substream *substream, int cmd)
{
    printk("mtk_pcm_fmtx_trigger cmd = %d\n", cmd);
    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_pcm_fmtx_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_pcm_fmtx_stop(substream);
    }
    return -EINVAL;
}

static int mtk_pcm_fmtx_copy(struct snd_pcm_substream *substream,
                             int channel, snd_pcm_uframes_t pos,
                             void __user *dst, snd_pcm_uframes_t count)
{
    AFE_BLOCK_T  *Afe_Block = NULL;
    unsigned long flags;
    char *data_w_ptr = (char *)dst;
    int copy_size = 0, Afe_WriteIdx_tmp;

    // get total bytes to copy
    count = audio_frame_to_bytes(substream , count);

    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] pos = %lu count = %lu\n ", pos, count);

    // check which memif nned to be write
    Afe_Block = &pMemControl->rBlock;

    // handle for buffer management

    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy]AudDrv_write WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x \n",
                    Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

    if (Afe_Block->u4BufferSize == 0)
    {
        printk("AudDrv_write: u4BufferSize=0 Error");
        return 0;
    }

    spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
    copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;  //  free space of the buffer
    spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);
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
    PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x \n", copy_size, count);

    if (copy_size != 0)
    {
        spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
        Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
        spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);

        if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) // copy once
        {
            if (!access_ok(VERIFY_READ, data_w_ptr, copy_size))
            {
                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] 0ptr invalid data_w_ptr=0x%x, size=%d", (kal_uint32)data_w_ptr, copy_size);
                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {
                PRINTK_AUD_FMTX("memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p copy_size = 0x%x\n",
                                Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
                if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr, copy_size))
                {
                    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] Fail copy from user \n");
                    return -1;
                }
            }

            spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
            Afe_Block->u4DataRemained += copy_size;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);
            data_w_ptr += copy_size;
            count -= copy_size;

            PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] finish1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x, count=%x \r\n",
                            copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained, count);

        }
        else  // copy twice
        {
            kal_uint32 size_1 = 0, size_2 = 0;
            size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
            size_2 = Align64ByteSize((copy_size - size_1));
            PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x \n", size_1, size_2);
            if (!access_ok(VERIFY_READ, data_w_ptr, size_1))
            {
                printk("[mtk_pcm_fmtx_copy] 1ptr invalid data_w_ptr=%p, size_1=%d", data_w_ptr, size_1);
                printk("[mtk_pcm_fmtx_copy] u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy]mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %x data_w_ptr = %p size_1 = %x\n",
                                Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);
                if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr , size_1)))
                {
                    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] Fail 1 copy from user");
                    return -1;
                }
            }
            spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
            Afe_Block->u4DataRemained += size_1;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
            spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);

            if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2))
            {
                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] 2ptr invalid data_w_ptr=%x, size_1=%d, size_2=%d", (kal_uint32)data_w_ptr, size_1, size_2);
                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy]mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %x data_w_ptr+size_1 = %p size_2 = %x\n",
                                Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr + size_1, size_2);
                if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), (data_w_ptr + size_1), size_2)))
                {
                    PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] Fail 2  copy from user");
                    return -1;
                }
            }
            spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);

            Afe_Block->u4DataRemained += size_2;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);
            count -= copy_size;
            data_w_ptr += copy_size;

            PRINTK_AUD_FMTX("[mtk_pcm_fmtx_copy] finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
                            copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
        }
    }
    return 0;
}

static int mtk_pcm_fmtx_silence(struct snd_pcm_substream *substream,
                                int channel, snd_pcm_uframes_t pos,
                                snd_pcm_uframes_t count)
{
    PRINTK_AUD_FMTX("%s \n", __func__);
    return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_fmtx_page(struct snd_pcm_substream *substream,
                                      unsigned long offset)
{
    PRINTK_AUD_FMTX("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_fmtx_ops =
{
    .open =     mtk_pcm_fmtx_open,
    .close =    mtk_pcm_fmtx_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_pcm_fmtx_hw_params,
    .hw_free =  mtk_pcm_fmtx_hw_free,
    .prepare =  mtk_pcm_fmtx_prepare,
    .trigger =  mtk_pcm_fmtx_trigger,
    .pointer =  mtk_pcm_fmtx_pointer,
    .copy =     mtk_pcm_fmtx_copy,
    .silence =  mtk_pcm_fmtx_silence,
    .page =     mtk_pcm_fmtx_page,
};

static struct snd_soc_platform_driver mtk_fmtx_soc_platform =
{
    .ops        = &mtk_fmtx_ops,
    .pcm_new    = mtk_asoc_pcm_fmtx_new,
    .probe      = mtk_afe_fmtx_probe,
};

static int mtk_fmtx_probe(struct platform_device *pdev)
{
    PRINTK_AUD_FMTX("%s \n", __func__);

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_FM_MRGTX_PCM);
    }

    PRINTK_AUD_FMTX("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

    mDev = &pdev->dev;

    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_fmtx_soc_platform);
}

static int mtk_asoc_pcm_fmtx_new(struct snd_soc_pcm_runtime *rtd)
{
    int ret = 0;
    PRINTK_AUD_FMTX("%s\n", __func__);
    return ret;
}

static int mtk_afe_fmtx_probe(struct snd_soc_platform *platform)
{
    PRINTK_AUD_FMTX("mtk_afe_afe_probe\n");
    snd_soc_add_platform_controls(platform, Audio_snd_fmtx_controls,
                                  ARRAY_SIZE(Audio_snd_fmtx_controls));
    AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1, Dl1_MAX_BUFFER_SIZE);
    FMTX_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);

    return 0;
}

static int mtk_fmtx_remove(struct platform_device *pdev)
{
    PRINTK_AUD_FMTX("%s \n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fmtx_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_fmtx", },
    {}
};
#endif

static struct platform_driver mtk_fmtx_driver =
{
    .driver = {
        .name = MT_SOC_FM_MRGTX_PCM,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_fmtx_of_ids,
#endif
    },
    .probe = mtk_fmtx_probe,
    .remove = mtk_fmtx_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkfmtx_dev;
#endif

static int __init mtk_soc_platform_init(void)
{
    int ret;
    PRINTK_AUD_FMTX("%s \n", __func__);
#ifndef CONFIG_OF
    soc_mtkfmtx_dev = platform_device_alloc(MT_SOC_FM_MRGTX_PCM, -1);
    if (!soc_mtkfmtx_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtkfmtx_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtkfmtx_dev);
        return ret;
    }
#endif
    ret = platform_driver_register(&mtk_fmtx_driver);
    return ret;

}
module_init(mtk_soc_platform_init);

static void __exit mtk_soc_platform_exit(void)
{
    PRINTK_AUD_FMTX("%s \n", __func__);

    platform_driver_unregister(&mtk_fmtx_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");


