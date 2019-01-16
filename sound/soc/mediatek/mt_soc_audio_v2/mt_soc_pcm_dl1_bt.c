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
 *   mt_soc_dl1_bt.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 data1 playback
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

static AFE_MEM_CONTROL_T *pdl1btMemControl = NULL;

static DEFINE_SPINLOCK(auddrv_DL1BTCtl_lock);

static struct device *mDev = NULL;

/*
 *    function implementation
 */

void StartAudioDl1BtPcmHardware(void);
void StopAudioDl1BtPcmHardware(void);
static int mtk_dl1bt_probe(struct platform_device *pdev);
static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream);
static int mtk_asoc_Dl1Bt_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_dl1bt_pcm_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_ADV_MT_FMTS,
    .rates =        SOC_HIGH_USE_RATE,
    .rate_min =     SOC_HIGH_USE_RATE_MIN,
    .rate_max =     SOC_HIGH_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
    .periods_max =      SOC_NORMAL_USE_PERIODS_MAX,
    .fifo_size =        0,
};

static int mtk_pcm_dl1Bt_stop(struct snd_pcm_substream *substream)
{

    PRINTK_AUDDRV("mtk_pcm_dl1Bt_stop \n");

    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

    // here to turn off digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O02);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O02);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

    SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
    {
        SetDaiBtEnable(false);
    }

    EnableAfe(false);
    RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
    AudDrv_Clk_Off();

    return 0;
}

static snd_pcm_uframes_t mtk_dl1bt_pcm_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    kal_uint32 Frameidx = 0;
    kal_int32 Afe_consumed_bytes = 0;
    unsigned long flags;

    AFE_BLOCK_T *Afe_Block = &pdl1btMemControl->rBlock;
    //struct snd_pcm_runtime *runtime = substream->runtime;
    PRINTK_AUD_DL1(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__, Afe_Block->u4DMAReadIdx);

    spin_lock_irqsave(&pdl1btMemControl->substream_lock, flags);

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
        spin_unlock_irqrestore(&pdl1btMemControl->substream_lock, flags);

        return audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
    }
    else
    {
        Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
        spin_unlock_irqrestore(&pdl1btMemControl->substream_lock, flags);
        return Frameidx;
    }
}


static int mtk_pcm_dl1bt_hw_params(struct snd_pcm_substream *substream,
                                   struct snd_pcm_hw_params *hw_params)
{
    int ret = 0;
    PRINTK_AUDDRV("mtk_pcm_dl1bt_hw_params \n");

    /* runtime->dma_bytes has to be set manually to allow mmap */
    substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

    // here to allcoate sram to hardware ---------------------------
    AudDrv_Allocate_mem_Buffer(mDev, Soc_Aud_Digital_Block_MEM_DL1, substream->runtime->dma_bytes);
    //substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE;
    substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
    substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;

    PRINTK_AUDDRV(" dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
                  substream->runtime->dma_bytes, substream->runtime->dma_area, (long)substream->runtime->dma_addr);
    return ret;
}

static int mtk_pcm_dl1bt_hw_free(struct snd_pcm_substream *substream)
{
    PRINTK_AUDDRV("mtk_pcm_dl1bt_hw_free \n");
    return 0;
}

static struct snd_pcm_hw_constraint_list constraints_dl1_sample_rates =
{
    .count = ARRAY_SIZE(soc_voice_supported_sample_rates),
    .list = soc_voice_supported_sample_rates,
    .mask = 0,
};

static int mPlaybackSramState = 0;
static int mtk_dl1bt_pcm_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct snd_pcm_runtime *runtime = substream->runtime;
    AfeControlSramLock();
    if (GetSramState() == SRAM_STATE_FREE)
    {
        mtk_dl1bt_pcm_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
        mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
        SetSramState(mPlaybackSramState);
    }
    else
    {
        mtk_dl1bt_pcm_hardware.buffer_bytes_max = GetPLaybackSramPartial();
        mPlaybackSramState = SRAM_STATE_PLAYBACKPARTIAL;
        SetSramState(mPlaybackSramState);
    }
    AfeControlSramUnLock();

    PRINTK_AUDDRV("mtk_dl1bt_pcm_open\n");
    AudDrv_Clk_On();

    // get dl1 memconptrol and record substream
    pdl1btMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
    runtime->hw = mtk_dl1bt_pcm_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl1bt_pcm_hardware , sizeof(struct snd_pcm_hardware));

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_dl1_sample_rates);
    if (ret < 0)
    {
        PRINTK_AUDDRV("snd_pcm_hw_constraint_list failed\n");
    }

    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
    if (ret < 0)
    {
        PRINTK_AUDDRV("snd_pcm_hw_constraint_integer failed\n");
    }
    //print for hw pcm information
    PRINTK_AUDDRV("mtk_dl1bt_pcm_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
                  runtime->rate, runtime->channels, substream->pcm->device);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        PRINTK_AUDDRV("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_playback_constraints\n");
    }
    else
    {

    }

    if (ret < 0)
    {
        PRINTK_AUDDRV("mtk_Dl1Bt_close\n");
        mtk_Dl1Bt_close(substream);
        return ret;
    }
    return 0;
}

static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream)
{
    PRINTK_AUDDRV("%s \n", __func__);
    AfeControlSramLock();
    ClearSramState(mPlaybackSramState);
    mPlaybackSramState = GetSramState();
    AfeControlSramUnLock();
    AudDrv_Clk_Off();
    return 0;
}

static int mtk_dl1bt_pcm_prepare(struct snd_pcm_substream *substream)
{
    return 0;
}

static bool SetVoipDAIBTAttribute(int sample_rate)
{
    AudioDigitalDAIBT daibt_attribute;
    memset((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

#if 0 // temp for merge only support
    daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_BT;
#else
    daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
#endif
    daibt_attribute.mDAI_BT_MODE = (sample_rate == 8000) ? Soc_Aud_DATBT_MODE_Mode8K : Soc_Aud_DATBT_MODE_Mode16K;
    daibt_attribute.mDAI_DEL = Soc_Aud_DAI_DEL_HighWord; // suggest always HighWord
    daibt_attribute.mBT_LEN  = 0;
    daibt_attribute.mDATA_RDY = true;
    daibt_attribute.mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
    daibt_attribute.mBT_ON = true;
    daibt_attribute.mDAIBT_ON = false;
    SetDaiBt(&daibt_attribute);
    return true;
}


static int mtk_pcm_dl1bt_start(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AudDrv_Clk_On();
    SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
    if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
    {
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O02); // BT SCO only support 16 bit
    }
    else
    {
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
        SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
        SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O02);
    }

    // here start digital part
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O02);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O02);
    SetConnection(Soc_Aud_InterCon_ConnectionShift, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O02);
    SetConnection(Soc_Aud_InterCon_ConnectionShift, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O02);

    // set dl1 sample ratelimit_state
    SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
    SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

    // here to set interrupt
    SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->period_size >> 1);
    SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
    {
        //set merge interface
        SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
    }
    else
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
    }

    SetVoipDAIBTAttribute(runtime->rate);
    SetDaiBtEnable(true);

    EnableAfe(true);

    return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    PRINTK_AUDDRV("mtk_pcm_trigger cmd = %d\n", cmd);
    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_pcm_dl1bt_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_pcm_dl1Bt_stop(substream);
    }
    return -EINVAL;
}

static int mtk_pcm_dl1bt_copy(struct snd_pcm_substream *substream,
                              int channel, snd_pcm_uframes_t pos,
                              void __user *dst, snd_pcm_uframes_t count)
{
    AFE_BLOCK_T  *Afe_Block = NULL;
    unsigned long flags;
    char *data_w_ptr = (char *)dst;
    int copy_size = 0, Afe_WriteIdx_tmp;
    PRINTK_AUD_DL1("mtk_pcm_dl1bt_copy pos = %lu count = %lu\n ", pos, count);

    // get total bytes to copy
    count = audio_frame_to_bytes(substream , count);

    // check which memif nned to be write
    Afe_Block = &pdl1btMemControl->rBlock;

    // handle for buffer management

    PRINTK_AUD_DL1("AudDrv_write WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x \n",
                   Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

    if (Afe_Block->u4BufferSize == 0)
    {
        printk("AudDrv_write: u4BufferSize=0 Error");
        return 0;
    }

    spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
    copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;  //  free space of the buffer
    spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);
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
    PRINTK_AUD_DL1("copy_size=0x%x, count=0x%lx \n", copy_size, count);

    if (copy_size != 0)
    {
        spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
        Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
        spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);

        if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) // copy once
        {
            if (!access_ok(VERIFY_READ, data_w_ptr, copy_size))
            {
                PRINTK_AUDDRV("AudDrv_write 0ptr invalid data_w_ptr=%p, size=%d", data_w_ptr, copy_size);
                PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {
                PRINTK_AUD_DL1("memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p copy_size = 0x%x\n",
                               Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
                if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr, copy_size))
                {
                    PRINTK_AUDDRV("AudDrv_write Fail copy from user \n");
                    return -1;
                }
            }

            spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
            Afe_Block->u4DataRemained += copy_size;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);
            data_w_ptr += copy_size;
            count -= copy_size;

            PRINTK_AUD_DL1("AudDrv_write finish1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x, count=%lu \r\n",
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
                printk("AudDrv_write 1ptr invalid data_w_ptr=%p, size_1=%d", data_w_ptr, size_1);
                printk("AudDrv_write u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
            }
            else
            {

                PRINTK_AUD_DL1("mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p size_1 = %x\n",
                               Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);
                if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr , size_1)))
                {
                    PRINTK_AUDDRV("AudDrv_write Fail 1 copy from user");
                    return -1;
                }
            }
            spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
            Afe_Block->u4DataRemained += size_1;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
            spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);

            if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2))
            {
                PRINTK_AUDDRV("AudDrv_write 2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d", data_w_ptr, size_1, size_2);
                PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
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
            spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);

            Afe_Block->u4DataRemained += size_2;
            Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
            Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
            spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);
            count -= copy_size;
            data_w_ptr += copy_size;

            PRINTK_AUD_DL1("AudDrv_write finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
                           copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
        }
    }
    return 0;
    PRINTK_AUD_DL1("pcm_copy return \n");
}

static int mtk_pcm_dl1bt_silence(struct snd_pcm_substream *substream,
                                 int channel, snd_pcm_uframes_t pos,
                                 snd_pcm_uframes_t count)
{
    PRINTK_AUDDRV("%s \n", __func__);
    return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
                                 unsigned long offset)
{
    PRINTK_AUDDRV("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_d1lbt_ops =
{
    .open =     mtk_dl1bt_pcm_open,
    .close =    mtk_Dl1Bt_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_pcm_dl1bt_hw_params,
    .hw_free =  mtk_pcm_dl1bt_hw_free,
    .prepare =  mtk_dl1bt_pcm_prepare,
    .trigger =  mtk_pcm_trigger,
    .pointer =  mtk_dl1bt_pcm_pointer,
    .copy =     mtk_pcm_dl1bt_copy,
    .silence =  mtk_pcm_dl1bt_silence,
    .page =     mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_dl1bt_platform =
{
    .ops        = &mtk_d1lbt_ops,
    .pcm_new    = mtk_asoc_Dl1Bt_pcm_new,
    .probe      = mtk_asoc_dl1bt_probe,
};

static int mtk_dl1bt_probe(struct platform_device *pdev)
{
    PRINTK_AUDDRV("%s \n", __func__);
    
    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (pdev->dev.dma_mask == NULL)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_OUT);
    }

    PRINTK_AUDDRV("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

    mDev = &pdev->dev;

    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_soc_dl1bt_platform);
}

static int mtk_asoc_Dl1Bt_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
    int ret = 0;
    PRINTK_AUDDRV("%s\n", __func__);
    return ret;
}


static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform)
{
    PRINTK_AUDDRV("mtk_asoc_dl1bt_probe\n");
    return 0;
}

static int mtk_asoc_dl1bt_remove(struct platform_device *pdev)
{
    PRINTK_AUDDRV("%s \n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_bt_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_dl1_bt", },
    {}
};
#endif

static struct platform_driver mtk_dl1bt_driver =
{
    .driver = {
        .name = MT_SOC_VOIP_BT_OUT,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_dl1_bt_of_ids,
#endif
    },
    .probe = mtk_dl1bt_probe,
    .remove = mtk_asoc_dl1bt_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_dl1bt_dev;
#endif

static int __init mtk_soc_dl1bt_platform_init(void)
{
    int ret;
    PRINTK_AUDDRV("%s \n", __func__);
#ifndef CONFIG_OF
    soc_mtk_dl1bt_dev = platform_device_alloc(MT_SOC_VOIP_BT_OUT, -1);
    if (!soc_mtk_dl1bt_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtk_dl1bt_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtk_dl1bt_dev);
        return ret;
    }
#endif
    ret = platform_driver_register(&mtk_dl1bt_driver);
    return ret;

}
module_init(mtk_soc_dl1bt_platform_init);

static void __exit mtk_soc_dl1bt_platform_exit(void)
{
    PRINTK_AUDDRV("%s \n", __func__);

    platform_driver_unregister(&mtk_dl1bt_driver);
}
module_exit(mtk_soc_dl1bt_platform_exit);

MODULE_DESCRIPTION("AFE dl1bt module platform driver");
MODULE_LICENSE("GPL");


