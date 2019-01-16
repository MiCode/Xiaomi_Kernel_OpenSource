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
 *   mtk_pcm_bt_dai.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio bt to dai capture
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
#include "mt_soc_pcm_common.h"

//information about
static AFE_MEM_CONTROL_T  *Bt_Dai_Control_context = NULL;
static struct snd_dma_buffer *Bt_Dai_Capture_dma_buf  = NULL;

static DEFINE_SPINLOCK(auddrv_BTDaiInCtl_lock);

/*
 *    function implementation
 */
static void StartAudioBtDaiHardware(struct snd_pcm_substream *substream);
static void StopAudioBtDaiHardware(struct snd_pcm_substream *substream);
static int mtk_bt_dai_probe(struct platform_device *pdev);
static int mtk_bt_dai_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_bt_dai_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_bt_dai_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_btdai_hardware =
{
    .info = (SNDRV_PCM_INFO_INTERLEAVED),
    .formats =      SND_SOC_STD_MT_FMTS,
    .rates =           SOC_NORMAL_USE_RATE,
    .rate_min =     SOC_NORMAL_USE_RATE_MIN,
    .rate_max =     SOC_NORMAL_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = BT_DAI_MAX_BUFFER_SIZE,
    .period_bytes_max = BT_DAI_MAX_BUFFER_SIZE,
    .periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
    .periods_max =      SOC_NORMAL_USE_PERIODS_MAX,
    .fifo_size =        0,
};

static void StopAudioBtDaiHardware(struct snd_pcm_substream *substream)
{
    printk("StopAudioBtDaiHardware \n");

    // here to set interrupt
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);

    // here to turn off digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O11);

    EnableAfe(false);
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


static void StartAudioBtDaiHardware(struct snd_pcm_substream *substream)
{
    printk("StartAudioBtDaiHardware period_size = %d\n", (int)substream->runtime->period_size);

    // here to set interrupt
    SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->period_size>>1);
    SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->rate);
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);

    SetSampleRate(Soc_Aud_Digital_Block_MEM_DAI, substream->runtime->rate);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, true);

    // here to turn off digital part
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I02, Soc_Aud_InterConnectionOutput_O11);

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
        SetVoipDAIBTAttribute(substream->runtime->rate);
        SetDaiBtEnable(true);
    }
    else
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
    }

    EnableAfe(true);
}

static int mtk_bt_dai_pcm_prepare(struct snd_pcm_substream *substream)
{
    printk("mtk_bt_dai_pcm_prepare substream->rate = %d  substream->channels = %d \n", substream->runtime->rate, substream->runtime->channels);
    return 0;
}

static int mtk_bt_dai_alsa_stop(struct snd_pcm_substream *substream)
{
    //AFE_BLOCK_T *Dai_Block = &(Bt_Dai_Control_context->rBlock);
    printk("mtk_bt_dai_alsa_stop \n");

    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, false);

    SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
    {
        SetDaiBtEnable(false);
    }
    StopAudioBtDaiHardware(substream);
    RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI,substream);
    return 0;
}

static kal_int32 Previous_Hw_cur = 0;
static snd_pcm_uframes_t mtk_bt_dai_pcm_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    AFE_BLOCK_T *Dai_Block = &(Bt_Dai_Control_context->rBlock);
    kal_uint32 Frameidx =0;
    PRINTK_AUD_DAI("mtk_bt_dai_pcm_pointer Dai_Block->u4DMAReadIdx;= 0x%x \n", Dai_Block->u4WriteIdx);
    // get total bytes to copy
    Frameidx =audio_bytes_to_frame(substream , Dai_Block->u4WriteIdx);
    return Frameidx;

    if (Bt_Dai_Control_context->interruptTrigger == 1)
    {
        // get total bytes to copy
        Frameidx =audio_bytes_to_frame(substream , Dai_Block->u4DMAReadIdx);
        return Frameidx;

        HW_Cur_ReadIdx = Align64ByteSize(Afe_Get_Reg(AFE_DAI_CUR));
        if (HW_Cur_ReadIdx == 0)
        {
            printk("[Auddrv] mtk_bt_dai_pcm_pointer  HW_Cur_ReadIdx ==0 \n");
            HW_Cur_ReadIdx = Dai_Block->pucPhysBufAddr;
        }
        HW_memory_index = (HW_Cur_ReadIdx - Dai_Block->pucPhysBufAddr);
        Previous_Hw_cur = HW_memory_index;
        printk("[Auddrv] mtk_bt_dai_pcm_pointer =0x%x HW_memory_index = 0x%x\n", HW_Cur_ReadIdx, HW_memory_index);
        Bt_Dai_Control_context->interruptTrigger = 0;
        return (HW_memory_index / substream->runtime->channels);
    }
    return (Previous_Hw_cur / substream->runtime->channels);
}


static void SetDAIBuffer(struct snd_pcm_substream *substream,
                         struct snd_pcm_hw_params *hw_params)
{
    AFE_BLOCK_T *pblock = &Bt_Dai_Control_context->rBlock;
    struct snd_pcm_runtime *runtime = substream->runtime;
    PRINTK_AUD_DAI("SetDAIBuffer\n");
    pblock->pucPhysBufAddr =  runtime->dma_addr;
    pblock->pucVirtBufAddr =  runtime->dma_area;
    pblock->u4BufferSize = runtime->dma_bytes;
    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;
    printk("dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
           pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
    // set sram address top hardware
    Afe_Set_Reg(AFE_DAI_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_DAI_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);

}

static int mtk_bt_dai_pcm_hw_params(struct snd_pcm_substream *substream,
                                    struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
    int ret = 0;

    printk("mtk_bt_dai_pcm_hw_params \n");

    dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
    dma_buf->dev.dev = substream->pcm->card->dev;
    dma_buf->private_data = NULL;

    if (Bt_Dai_Capture_dma_buf->area)
    {
        printk("mtk_bt_dai_pcm_hw_params Bt_Dai_Capture_dma_buf->area\n");
        runtime->dma_bytes = params_buffer_bytes(hw_params);
        runtime->dma_area = Bt_Dai_Capture_dma_buf->area;
        runtime->dma_addr = Bt_Dai_Capture_dma_buf->addr;
    }
    else
    {
        printk("mtk_bt_dai_pcm_hw_params snd_pcm_lib_malloc_pages\n");
        ret =  snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
    }
    printk("mtk_bt_dai_pcm_hw_params dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
           runtime->dma_bytes, runtime->dma_area, (long)runtime->dma_addr);

    printk("runtime->hw.buffer_bytes_max = %zu \n", runtime->hw.buffer_bytes_max);
    SetDAIBuffer(substream, hw_params);

    printk("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
           substream->runtime->dma_bytes, substream->runtime->dma_area, (long)substream->runtime->dma_addr);
    return ret;
}

static int mtk_bt_dai_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
    printk("mtk_bt_dai_capture_pcm_hw_free \n");
    if (Bt_Dai_Capture_dma_buf->area)
    {
        return 0;
    }
    else
    {
        return snd_pcm_lib_free_pages(substream);
    }
}


static struct snd_pcm_hw_constraint_list bt_dai_constraints_sample_rates =
{
    .count = ARRAY_SIZE(soc_voice_supported_sample_rates),
    .list = soc_voice_supported_sample_rates,
};

static int mtk_bt_dai_pcm_open(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret = 0;

    printk("mtk_bt_dai_pcm_open\n");

    Bt_Dai_Control_context = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DAI);
    runtime->hw = mtk_btdai_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_btdai_hardware , sizeof(struct snd_pcm_hardware));
    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &bt_dai_constraints_sample_rates);
    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    AudDrv_Clk_On();

    //print for hw pcm information
    printk("mtk_bt_dai_pcm_open runtime rate = %d channels = %d \n", runtime->rate, runtime->channels);
    runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
    runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("SNDRV_PCM_STREAM_CAPTURE \n");
    }
    else
    {
        return -1;
    }

    if (ret < 0)
    {
        printk("mtk_bt_dai_pcm_close\n");
        mtk_bt_dai_pcm_close(substream);
        return ret;
    }
    AudDrv_Emi_Clk_On();
    printk("mtk_bt_dai_pcm_open return\n");
    return 0;
}

static int mtk_bt_dai_pcm_close(struct snd_pcm_substream *substream)
{
    AudDrv_Clk_Off();
    AudDrv_Emi_Clk_Off();
    return 0;
}

static int mtk_bt_dai_alsa_start(struct snd_pcm_substream *substream)
{
    printk("mtk_bt_dai_alsa_start \n");
    SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI, substream);
    StartAudioBtDaiHardware(substream);
    return 0;
}

static int mtk_bt_dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    printk("mtk_bt_dai_pcm_trigger cmd = %d\n", cmd);

    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_bt_dai_alsa_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_bt_dai_alsa_stop(substream);
    }
    return -EINVAL;
}

static bool CheckNullPointer(void *pointer)
{
    if (pointer == NULL)
    {
        printk("CheckNullPointer pointer = NULL");
        return true;
    }
    return false;
}

static int mtk_bt_dai_pcm_copy(struct snd_pcm_substream *substream,
                               int channel, snd_pcm_uframes_t pos,
                               void __user *dst, snd_pcm_uframes_t count)
{
    AFE_MEM_CONTROL_T *pDAI_MEM_ConTrol = NULL;
    AFE_BLOCK_T  *Dai_Block = NULL;
    char *Read_Data_Ptr = (char *)dst;
    ssize_t DMA_Read_Ptr = 0 , read_size = 0, read_count = 0;
    unsigned long flags;
    printk("%s  pos = %lu count = %lu\n ", __func__, pos, count);

    // get total bytes to copy
    count = Align64ByteSize(audio_frame_to_bytes(substream , count));

    // check which memif nned to be write
    pDAI_MEM_ConTrol = Bt_Dai_Control_context;
    Dai_Block = &(pDAI_MEM_ConTrol->rBlock);

    if (pDAI_MEM_ConTrol == NULL)
    {
        printk("cannot find MEM control !!!!!!!\n");
        msleep(50);
        return 0;
    }

    if (Dai_Block->u4BufferSize <= 0)
    {
        msleep(50);
        return 0;
    }

    if (CheckNullPointer((void *)Dai_Block->pucVirtBufAddr))
    {
        printk("CheckNullPointer  pucVirtBufAddr = %p\n", Dai_Block->pucVirtBufAddr);
        return 0;
    }

    spin_lock_irqsave(&auddrv_BTDaiInCtl_lock, flags);
    if (Dai_Block->u4DataRemained >  Dai_Block->u4BufferSize)
    {
        PRINTK_AUD_DAI("!!!!!!!!!!!!mtk_bt_dai_pcm_copy u4DataRemained=%x > u4BufferSize=%x" , Dai_Block->u4DataRemained, Dai_Block->u4BufferSize);
        Dai_Block->u4DataRemained = 0;
        Dai_Block->u4DMAReadIdx   = Dai_Block->u4WriteIdx;
    }
    if (count >  Dai_Block->u4DataRemained)
    {
        read_size = Dai_Block->u4DataRemained;
    }
    else
    {
        read_size = count;
    }

    DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
    spin_unlock_irqrestore(&auddrv_BTDaiInCtl_lock, flags);

    PRINTK_AUD_DAI("mtk_bt_dai_pcm_copy finish0, read_count:0x%x, read_size:0x%x, u4DataRemained:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x \r\n",
           read_count, read_size, Dai_Block->u4DataRemained, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx);

    if (DMA_Read_Ptr + read_size < Dai_Block->u4BufferSize)
    {
        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {
            printk("mtk_bt_dai_pcm_copy 1, read_size:%zu, DataRemained:0x%x, DMA_Read_Ptr:%zu, DMAReadIdx:0x%x \r\n",
                   read_size, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }

        if (copy_to_user((void __user *)Read_Data_Ptr, (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), read_size))
        {

            printk("mtk_bt_dai_pcm_copy Fail 1 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:%zu,read_size:%zu", Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += read_size;
        spin_lock(&auddrv_BTDaiInCtl_lock);
        Dai_Block->u4DataRemained -= read_size;
        Dai_Block->u4DMAReadIdx += read_size;
        Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_BTDaiInCtl_lock);

        Read_Data_Ptr += read_size;
        count -= read_size;

        PRINTK_AUD_DAI("mtk_bt_dai_pcm_copy finish1, copy size:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, u4DataRemained:0x%x \r\n",
               read_size, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
    }

    else
    {
        uint32 size_1 = Dai_Block->u4BufferSize - DMA_Read_Ptr;
        uint32 size_2 = read_size - size_1;

        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {

            printk("mtk_bt_dai_pcm_copy 2, read_size1:0x%x, DataRemained:0x%x, DMA_Read_Ptr:%zu, DMAReadIdx:0x%x \r\n",
                   size_1, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)Read_Data_Ptr, (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_1))
        {

            printk("mtk_bt_dai_pcm_copy Fail 2 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:%zu,read_size:%zu",
                   Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += size_1;
        spin_lock(&auddrv_BTDaiInCtl_lock);
        Dai_Block->u4DataRemained -= size_1;
        Dai_Block->u4DMAReadIdx += size_1;
        Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_BTDaiInCtl_lock);

        PRINTK_AUD_DAI("mtk_bt_dai_pcm_copy finish2, copy size_1:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, u4DataRemained:0x%x \r\n",
               size_1, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);

        if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx)
        {

            printk("mtk_bt_dai_pcm_copy 3, read_size2:%x, DataRemained:%x, DMA_Read_Ptr:%zu, DMAReadIdx:%x \r\n",
                   size_2, Dai_Block->u4DataRemained, DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)(Read_Data_Ptr + size_1), (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_2))
        {

            printk("mtk_bt_dai_pcm_copy Fail 3 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x , DMA_Read_Ptr:%zu, read_size:%zu", Read_Data_Ptr, Dai_Block->pucVirtBufAddr, Dai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return read_count << 2;
        }

        read_count += size_2;
        spin_lock(&auddrv_BTDaiInCtl_lock);
        Dai_Block->u4DataRemained -= size_2;
        Dai_Block->u4DMAReadIdx += size_2;
        DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_BTDaiInCtl_lock);

        count -= read_size;
        Read_Data_Ptr += read_size;

        PRINTK_AUD_DAI("mtk_bt_dai_pcm_copy finish3, copy size_2:0x%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x u4DataRemained:0x%x \r\n",
               size_2, Dai_Block->u4DMAReadIdx, Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
    }

    return audio_bytes_to_frame(substream,count);
}

static int mtk_bt_dai_capture_pcm_silence(struct snd_pcm_substream *substream,
                                          int channel, snd_pcm_uframes_t pos,
                                          snd_pcm_uframes_t count)
{
    printk("dummy_pcm_silence \n");
    return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_bt_dai_capture_pcm_page(struct snd_pcm_substream *substream,
                                                unsigned long offset)
{
    printk("dummy_pcm_page \n");
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static struct snd_pcm_ops mtk_bt_dai_ops =
{
    .open =     mtk_bt_dai_pcm_open,
    .close =    mtk_bt_dai_pcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_bt_dai_pcm_hw_params,
    .hw_free =  mtk_bt_dai_capture_pcm_hw_free,
    .prepare =  mtk_bt_dai_pcm_prepare,
    .trigger =  mtk_bt_dai_pcm_trigger,
    .pointer =  mtk_bt_dai_pcm_pointer,
    .copy =     mtk_bt_dai_pcm_copy,
    .silence =  mtk_bt_dai_capture_pcm_silence,
    .page =     mtk_bt_dai_capture_pcm_page,
};

static struct snd_soc_platform_driver mtk_bt_dai_soc_platform =
{
    .ops        = &mtk_bt_dai_ops,
    .pcm_new    = mtk_asoc_bt_dai_pcm_new,
    .probe      = mtk_asoc_bt_dai_probe,
};

static int mtk_bt_dai_probe(struct platform_device *pdev)
{
    printk("mtk_bt_dai_probe\n");

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_IN);
    }

    printk("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_bt_dai_soc_platform);
}

static int mtk_asoc_bt_dai_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
    printk("mtk_asoc_bt_dai_pcm_new \n");
    return 0;
}

static int mtk_asoc_bt_dai_probe(struct snd_soc_platform *platform)
{
    printk("mtk_asoc_bt_dai_probe\n");
    AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DAI, BT_DAI_MAX_BUFFER_SIZE);
    Bt_Dai_Capture_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DAI);
    return 0;
}

static int mtk_bt_dai_remove(struct platform_device *pdev)
{
    pr_debug("%s\n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_bt_dai_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_bt_dai", },
    {}
};
#endif

static struct platform_driver mtk_bt_dai_capture_driver =
{
    .driver = {
        .name = MT_SOC_VOIP_BT_IN,
        .owner = THIS_MODULE,
        #ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_bt_dai_of_ids,
        #endif        
    },
    .probe = mtk_bt_dai_probe,
    .remove = mtk_bt_dai_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_bt_dai_capture_dev;
#endif

static int __init mtk_soc_bt_dai_platform_init(void)
{
    int ret = 0;
    printk("%s\n", __func__);
	#ifndef CONFIG_OF
    soc_bt_dai_capture_dev = platform_device_alloc(MT_SOC_VOIP_BT_IN, -1);
    if (!soc_bt_dai_capture_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_bt_dai_capture_dev);
    if (ret != 0)
    {
        platform_device_put(soc_bt_dai_capture_dev);
        return ret;
    }
	#endif
    ret = platform_driver_register(&mtk_bt_dai_capture_driver);
    return ret;
}

static void __exit mtk_soc_bt_dai_platform_exit(void)
{
    printk("%s\n", __func__);
    platform_driver_unregister(&mtk_bt_dai_capture_driver);
}

module_init(mtk_soc_bt_dai_platform_init);
module_exit(mtk_soc_bt_dai_platform_exit);

MODULE_DESCRIPTION("BT DAI module platform driver");
MODULE_LICENSE("GPL");


