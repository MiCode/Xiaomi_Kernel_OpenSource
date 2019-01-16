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
 *   mt_soc_pcm_mod_dai.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio MOD DAI path
 *
 * Author:
 * -------
 * JY Huang
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
#include "mt_soc_pcm_common.h"

//information about
AFE_MEM_CONTROL_T  *MOD_DAI_Control_context;
static struct snd_dma_buffer *Capture_dma_buf  = NULL;
static AudioDigtalI2S *mAudioDigitalI2S = NULL;
static bool mModDaiUseSram = false;
static DEFINE_SPINLOCK(auddrv_ModDaiInCtl_lock);

/*
 *    function implementation
 */
static void StartAudioCaptureHardware(struct snd_pcm_substream *substream);
static void StopAudioCaptureHardware(struct snd_pcm_substream *substream);
void StartAudioCaptureAnalogHardware(void);
void StopAudioCaptureAnalogHardware(void);
static int mtk_mod_dai_probe(struct platform_device *pdev);
static int mtk_mod_dai_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_mod_dai_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_mod_dai_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_mod_dai_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_STD_MT_FMTS,
    .rates =        SOC_NORMAL_USE_RATE,
    .rate_min =     SOC_NORMAL_USE_RATE_MIN,
    .rate_max =     SOC_NORMAL_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
    .period_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
    .periods_min =      MOD_DAI_MIN_PERIOD_SIZE,
    .periods_max =      MOD_DAI_MAX_PERIOD_SIZE,
    .fifo_size =        0,
};

static void StopAudioCaptureHardware(struct snd_pcm_substream *substream)
{
    printk("StopAudioCaptureHardware \n");

    // here to set interrupt
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);

    /* JY: MOD_DAI?
    SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC_2, false);
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC_2) == false)
    {
        Set2ndI2SAdcEnable(false);
    }*/

    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, false);

    // here to turn off digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O12);

    EnableAfe(false);
}

/*
static void ConfigAdcI2S(struct snd_pcm_substream *substream)
{
    mAudioDigitalI2S->mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
    mAudioDigitalI2S->mBuffer_Update_word = 8;
    mAudioDigitalI2S->mFpga_bit_test = 0;
    mAudioDigitalI2S->mFpga_bit = 0;
    mAudioDigitalI2S->mloopback = 0;
    mAudioDigitalI2S->mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
    mAudioDigitalI2S->mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
    mAudioDigitalI2S->mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
    mAudioDigitalI2S->mI2S_SAMPLERATE = (substream->runtime->rate);
}
*/

static void StartAudioCaptureHardware(struct snd_pcm_substream *substream)
{
    printk("StartAudioCaptureHardware \n");

    //ConfigAdcI2S(substream);
    //Set2ndI2SAdcIn(mAudioDigitalI2S);//To do, JY

    SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_MOD_DAI, AFE_WLEN_16_BIT);
    SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O12);

    /* To Do, JY, MOD_DAI?
    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC_2) == false)
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC_2, true);
        Set2ndI2SAdcEnable(true);
    }
    else
    {
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC_2, true);
    }
    */
    // here to set interrupt
    SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->period_size);
    SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->rate);
    SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);

    SetSampleRate(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream->runtime->rate);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, true);

    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O12);
    EnableAfe(true);

}

static int mtk_mod_dai_pcm_prepare(struct snd_pcm_substream *substream)
{
    printk("mtk_mod_dai_pcm_prepare substream->rate = %d  substream->channels = %d \n", substream->runtime->rate, substream->runtime->channels);
    return 0;
}

static int mtk_mod_dai_alsa_stop(struct snd_pcm_substream *substream)
{
    AFE_BLOCK_T *pModDai_Block = &(MOD_DAI_Control_context->rBlock);
    printk("mtk_mod_dai_alsa_stop \n");
    StopAudioCaptureHardware(substream);
    pModDai_Block->u4DMAReadIdx  = 0;
    pModDai_Block->u4WriteIdx  = 0;
    pModDai_Block->u4DataRemained = 0;
    RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream);
    return 0;
}

static kal_int32 Previous_Hw_cur = 0;
static snd_pcm_uframes_t mtk_mod_dai_pcm_pointer(struct snd_pcm_substream *substream)
{
    kal_int32 HW_memory_index = 0;
    kal_int32 HW_Cur_ReadIdx = 0;
    kal_uint32 Frameidx =0;
    AFE_BLOCK_T *pModDai_Block = &(MOD_DAI_Control_context->rBlock);
    PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_pointer pModDai_Block->u4WriteIdx= 0x%x \n", pModDai_Block->u4WriteIdx);
    if (MOD_DAI_Control_context->interruptTrigger == 1)
    {
        // get total bytes to copysinewavetohdmi
        Frameidx =audio_bytes_to_frame(substream , pModDai_Block->u4WriteIdx);
        return Frameidx;

        HW_Cur_ReadIdx = Afe_Get_Reg(AFE_MOD_DAI_CUR);
        if (HW_Cur_ReadIdx == 0)
        {
            PRINTK_AUD_MODDAI("[Auddrv] mtk_mod_dai_pcm_pointer  HW_Cur_ReadIdx ==0 \n");
            HW_Cur_ReadIdx = pModDai_Block->pucPhysBufAddr;
        }
        HW_memory_index = (HW_Cur_ReadIdx - pModDai_Block->pucPhysBufAddr);
        Previous_Hw_cur = HW_memory_index;
        PRINTK_AUD_MODDAI("[Auddrv] mtk_mod_dai_pcm_pointer =0x%x HW_memory_index = 0x%x\n", HW_Cur_ReadIdx, HW_memory_index);
        MOD_DAI_Control_context->interruptTrigger = 0;
        return (HW_memory_index >> 1);
    }
    return (Previous_Hw_cur >> 1);

}

static void SetMODDAIBuffer(struct snd_pcm_substream *substream,
                         struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    AFE_BLOCK_T *pblock = &MOD_DAI_Control_context->rBlock;
    printk("SetMODDAIBuffer\n");
    pblock->pucPhysBufAddr =  runtime->dma_addr;
    pblock->pucVirtBufAddr =  runtime->dma_area;
    pblock->u4BufferSize = runtime->dma_bytes;
    pblock->u4SampleNumMask = 0x001f;  // 32 byte align
    pblock->u4WriteIdx     = 0;
    pblock->u4DMAReadIdx    = 0;
    pblock->u4DataRemained  = 0;
    pblock->u4fsyncflag     = false;
    pblock->uResetFlag      = true;
    printk("u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
           pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
    // set dram address top hardware
    Afe_Set_Reg(AFE_MOD_DAI_BASE , pblock->pucPhysBufAddr , 0xffffffff);
    Afe_Set_Reg(AFE_MOD_DAI_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);

}

static int mtk_mod_dai_pcm_hw_params(struct snd_pcm_substream *substream,
                                     struct snd_pcm_hw_params *hw_params)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
    int ret = 0;
    printk("mtk_mod_dai_pcm_hw_params \n");

    dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
    dma_buf->dev.dev = substream->pcm->card->dev;
    dma_buf->private_data = NULL;
    printk("mod_dai_dma_buf = %p mod_dai_dma_buf->area = %p\n", Capture_dma_buf, Capture_dma_buf->area);

    if (mModDaiUseSram == true)
    {
        runtime->dma_bytes = params_buffer_bytes(hw_params);
        printk("mtk_mod_dai_pcm_hw_params mModDaiUseSram dma_bytes = %zu \n", runtime->dma_bytes);
        substream->runtime->dma_area = (unsigned char *)Get_Afe_SramModDaiBase_Pointer();
        substream->runtime->dma_addr = Get_Afe_Sram_ModDai_Phys_Addr();
    }
    else if (Capture_dma_buf->area)
    {//Use SRAM here
        printk("mtk_mod_dai_pcm_hw_params Capture_dma_buf->area\n");
        runtime->dma_bytes = Capture_dma_buf->bytes;
        runtime->dma_area = Capture_dma_buf->area;
        runtime->dma_addr = Capture_dma_buf->addr;
        runtime->buffer_size = Capture_dma_buf->bytes;
    }
    else
    {
        printk("mtk_mod_dai_pcm_hw_params snd_pcm_lib_malloc_pages\n");
        ret =  snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
    }
    printk("mtk_mod_dai_pcm_hw_params dma_bytes = %zu dma_area = %p dma_addr = 0x%x\n",
           runtime->dma_bytes, runtime->dma_area, (unsigned int)runtime->dma_addr);

    //printk("runtime->hw.buffer_bytes_max = 0x%lx \n", runtime->hw.buffer_bytes_max);
    SetMODDAIBuffer(substream, hw_params);

    printk("dma_bytes = %zu dma_area = %p dma_addr = 0x%x\n",
           substream->runtime->dma_bytes, substream->runtime->dma_area, (unsigned int)substream->runtime->dma_addr);
    return ret;
}

static int mtk_mod_dai_pcm_hw_free(struct snd_pcm_substream *substream)
{
    printk("mtk_mod_dai_pcm_hw_free \n");
    if (Capture_dma_buf->area)
    {
        return 0;
    }
    else
    {
        return snd_pcm_lib_free_pages(substream);
    }
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates =
{
    .count = ARRAY_SIZE(soc_normal_supported_sample_rates),
    .list = soc_normal_supported_sample_rates,
};

static int mtk_mod_dai_pcm_open(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret = 0;
    AudDrv_Clk_On();

    printk("%s \n", __func__);
    MOD_DAI_Control_context = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_MOD_DAI);

    // can allocate sram_dbg
    AfeControlSramLock();
    if ((GetSramState() ==  SRAM_STATE_FREE) || ((GetSramState() & SRAM_STATE_SPH_SPK_MNTR_PROCESS_DL) != 0))
    {
        mtk_mod_dai_hardware.buffer_bytes_max = MOD_DAI_MAX_BUFFER_SIZE;
        printk("mtk_mod_dai_pcm_open use sram %zu\n", mtk_mod_dai_hardware.buffer_bytes_max);
        SetSramState(SRAM_STATE_SPH_SPK_MNTR_CAPTURE_DL);
        mModDaiUseSram = true;
    }
    else
    {
        printk("mtk_mod_dai_pcm_open use dram \n");
        mtk_mod_dai_hardware.buffer_bytes_max = MOD_DAI_MAX_BUFFER_SIZE;
    }
    AfeControlSramUnLock();
    
    runtime->hw = mtk_mod_dai_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_mod_dai_hardware , sizeof(struct snd_pcm_hardware));
    printk("runtime->hw->rates = 0x%x \n ", runtime->hw.rates);

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_sample_rates);
    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    printk("mtk_mod_dai_pcm_open runtime rate = %d channels = %d \n", runtime->rate, runtime->channels);
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("SNDRV_PCM_STREAM_CAPTURE mtkalsa_capture_constraints\n");
    }
    else
    {

    }

    if (ret < 0)
    {
        printk("mtk_mod_dai_pcm_close\n");
        mtk_mod_dai_pcm_close(substream);
        return ret;
    }
    //if(mModDaiUseSram == true)
    //{
    //    AudDrv_Emi_Clk_On();
    //}
    printk("mtk_mod_dai_pcm_open return\n");
    return 0;
}

static int mtk_mod_dai_pcm_close(struct snd_pcm_substream *substream)
{
    //if(mModDaiUseSram == true)
    //{
    //    AudDrv_Emi_Clk_Off();
    //}
    if ((GetSramState()&SRAM_STATE_SPH_SPK_MNTR_CAPTURE_DL))
    {
        ClearSramState(SRAM_STATE_SPH_SPK_MNTR_CAPTURE_DL);
        mModDaiUseSram = false;
    }
    AudDrv_Clk_Off();
    return 0;
}

static int mtk_mod_dai_alsa_start(struct snd_pcm_substream *substream)
{
    printk("mtk_mod_dai_alsa_start \n");
    SetMemifSubStream(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream);
    StartAudioCaptureHardware(substream);
    return 0;
}

static int mtk_mod_dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    printk("mtk_mod_dai_pcm_trigger cmd = %d\n", cmd);

    switch (cmd)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
            return mtk_mod_dai_alsa_start(substream);
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            return mtk_mod_dai_alsa_stop(substream);
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

static int mtk_mod_dai_pcm_copy(struct snd_pcm_substream *substream,
                                int channel, snd_pcm_uframes_t pos,
                                void __user *dst, snd_pcm_uframes_t count)
{

    AFE_MEM_CONTROL_T *pMOD_DAI_MEM_ConTrol = NULL;
    AFE_BLOCK_T  *pModDai_Block = NULL;
    char *Read_Data_Ptr = (char *)dst;
    ssize_t DMA_Read_Ptr = 0 , read_size = 0, read_count = 0;
    unsigned long flags;

    PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy pos = %lu, count = %lu \n ", pos, count);

    count =audio_frame_to_bytes(substream , count);    // get total bytes to copy

    // check which memif nned to be write
    pMOD_DAI_MEM_ConTrol = MOD_DAI_Control_context;
    pModDai_Block = &(pMOD_DAI_MEM_ConTrol->rBlock);

    if (pMOD_DAI_MEM_ConTrol == NULL)
    {
        printk("cannot find MEM control !!!!!!!\n");
        msleep(50);
        return 0;
    }

    if (pModDai_Block->u4BufferSize <= 0)
    {
        msleep(50);
        printk("pModDai_Block->u4BufferSize <= 0  =%d\n", pModDai_Block->u4BufferSize);
        return 0;
    }

    if (CheckNullPointer((void *)pModDai_Block->pucVirtBufAddr))
    {
        printk("CheckNullPointer  pucVirtBufAddr = %p\n", pModDai_Block->pucVirtBufAddr);
        return 0;
    }

    spin_lock_irqsave(&auddrv_ModDaiInCtl_lock, flags);
    if (pModDai_Block->u4DataRemained >  pModDai_Block->u4BufferSize)
    {
        PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy u4DataRemained=%x > u4BufferSize=%x\r\n" , pModDai_Block->u4DataRemained, pModDai_Block->u4BufferSize);
        pModDai_Block->u4DataRemained = 0;
        pModDai_Block->u4DMAReadIdx   = pModDai_Block->u4WriteIdx;
    }
    if (count >  pModDai_Block->u4DataRemained)
    {
        read_size = pModDai_Block->u4DataRemained;
    }
    else
    {
        read_size = count;
    }

    DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
    spin_unlock_irqrestore(&auddrv_ModDaiInCtl_lock, flags);

    PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy finish0, read_count:%x, read_size:%lx, u4DataRemained:%x, u4DMAReadIdx:0x%x, u4WriteIdx:%x \r\n",
                   read_count, read_size, pModDai_Block->u4DataRemained, pModDai_Block->u4DMAReadIdx, pModDai_Block->u4WriteIdx);

    if (DMA_Read_Ptr + read_size < pModDai_Block->u4BufferSize)
    {
        if (DMA_Read_Ptr != pModDai_Block->u4DMAReadIdx)
        {
            //printk("mtk_mod_dai_pcm_copy 1, read_size:%lx, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x \r\n",
            //       read_size, pModDai_Block->u4DataRemained, DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);
        }

        if (copy_to_user((void __user *)Read_Data_Ptr, (pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), read_size))
        {

            //printk("mtk_mod_dai_pcm_copy Fail 1 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:0x%lx,read_size:%lx \r\n", Read_Data_Ptr, pModDai_Block->pucVirtBufAddr, pModDai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += read_size;
        spin_lock(&auddrv_ModDaiInCtl_lock);
        pModDai_Block->u4DataRemained -= read_size;
        pModDai_Block->u4DMAReadIdx += read_size;
        pModDai_Block->u4DMAReadIdx %= pModDai_Block->u4BufferSize;
        DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ModDaiInCtl_lock);

        Read_Data_Ptr += read_size;
        count -= read_size;

        PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy finish1, copy size:%x, u4DMAReadIdx:0x%x, u4WriteIdx:%x, u4DataRemained:%x \r\n",
                       read_size, pModDai_Block->u4DMAReadIdx, pModDai_Block->u4WriteIdx, pModDai_Block->u4DataRemained);
    }

    else
    {
        uint32 size_1 = pModDai_Block->u4BufferSize - DMA_Read_Ptr;
        uint32 size_2 = read_size - size_1;

        if (DMA_Read_Ptr != pModDai_Block->u4DMAReadIdx)
        {

            //printk("mtk_mod_dai_pcm_copy 2, read_size1:%x, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x \r\n",
            //       size_1, pModDai_Block->u4DataRemained, DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)Read_Data_Ptr, (pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_1))
        {

            //printk("mtk_mod_dai_pcm_copy Fail 2 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x, DMA_Read_Ptr:0x%lx,read_size:%lx \r\n",
            //       Read_Data_Ptr, pModDai_Block->pucVirtBufAddr, pModDai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return 0;
        }

        read_count += size_1;
        spin_lock(&auddrv_ModDaiInCtl_lock);
        pModDai_Block->u4DataRemained -= size_1;
        pModDai_Block->u4DMAReadIdx += size_1;
        pModDai_Block->u4DMAReadIdx %= pModDai_Block->u4BufferSize;
        DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ModDaiInCtl_lock);


        PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy finish2, copy size_1:%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, u4DataRemained:%x \r\n",
                       size_1, pModDai_Block->u4DMAReadIdx, pModDai_Block->u4WriteIdx, pModDai_Block->u4DataRemained);

        if (DMA_Read_Ptr != pModDai_Block->u4DMAReadIdx)
        {

            //printk("mtk_mod_dai_pcm_copy 3, read_size2:%x, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x \r\n",
            //       size_2, pModDai_Block->u4DataRemained, DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);
        }
        if (copy_to_user((void __user *)(Read_Data_Ptr + size_1), (pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_2))
        {

            //printk("mtk_mod_dai_pcm_copy Fail 3 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x , DMA_Read_Ptr:0x%lx, read_size:%lx \r\n", Read_Data_Ptr, pModDai_Block->pucVirtBufAddr, pModDai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
            return read_count << 2;
        }

        read_count += size_2;
        spin_lock(&auddrv_ModDaiInCtl_lock);
        pModDai_Block->u4DataRemained -= size_2;
        pModDai_Block->u4DMAReadIdx += size_2;
        DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
        spin_unlock(&auddrv_ModDaiInCtl_lock);

        count -= read_size;
        Read_Data_Ptr += read_size;

        PRINTK_AUD_MODDAI("mtk_mod_dai_pcm_copy finish3, copy size_2:%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x u4DataRemained:%x \r\n",
                       size_2, pModDai_Block->u4DMAReadIdx, pModDai_Block->u4WriteIdx, pModDai_Block->u4DataRemained);
    }

    return read_count >> 1;
}

static int mtk_mod_dai_pcm_silence(struct snd_pcm_substream *substream,
                                   int channel, snd_pcm_uframes_t pos,
                                   snd_pcm_uframes_t count)
{
    printk("dummy_pcm_silence \n");
    return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_mod_dai_pcm_page(struct snd_pcm_substream *substream,
                                         unsigned long offset)
{
    printk("%s \n", __func__);
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static struct snd_pcm_ops mtk_afe_mod_dai_ops =
{
    .open =     mtk_mod_dai_pcm_open,
    .close =    mtk_mod_dai_pcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_mod_dai_pcm_hw_params,
    .hw_free =  mtk_mod_dai_pcm_hw_free,
    .prepare =  mtk_mod_dai_pcm_prepare,
    .trigger =  mtk_mod_dai_pcm_trigger,
    .pointer =  mtk_mod_dai_pcm_pointer,
    .copy =     mtk_mod_dai_pcm_copy,
    .silence =  mtk_mod_dai_pcm_silence,
    .page =     mtk_mod_dai_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform =
{
    .ops        = &mtk_afe_mod_dai_ops,
    .pcm_new    = mtk_asoc_mod_dai_pcm_new,
    .probe      = mtk_afe_mod_dai_probe,
};

static int mtk_mod_dai_probe(struct platform_device *pdev)
{
    printk("mtk_mod_dai_probe\n");

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_MOD_DAI_PCM);
    }

    printk("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_soc_platform);
}

static int mtk_asoc_mod_dai_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
    printk("mtk_asoc_mod_dai_pcm_new \n");
    return 0;
}


static int mtk_afe_mod_dai_probe(struct snd_soc_platform *platform)
{
    printk("mtk_afe_mod_dai_probe\n");
    AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_MOD_DAI, MOD_DAI_MAX_BUFFER_SIZE);
    Capture_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_MOD_DAI);
    mAudioDigitalI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
    return 0;
}


static int mtk_mod_dai_remove(struct platform_device *pdev)
{
    pr_debug("%s\n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_mod_dai_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_mod_dai", },
    {}
};
#endif

static struct platform_driver mtk_afe_mod_dai_driver =
{
    .driver = {
        .name = MT_SOC_MOD_DAI_PCM,
        .owner = THIS_MODULE,
        #ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_mod_dai_of_ids,
        #endif        
    },
    .probe = mtk_mod_dai_probe,
    .remove = mtk_mod_dai_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_mod_dai_dev;
#endif

static int __init mtk_soc_mod_dai_platform_init(void)
{
    int ret = 0;
    printk("%s\n", __func__);
	#ifndef CONFIG_OF
    soc_mtkafe_mod_dai_dev = platform_device_alloc(MT_SOC_MOD_DAI_PCM, -1);
    if (!soc_mtkafe_mod_dai_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtkafe_mod_dai_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtkafe_mod_dai_dev);
        return ret;
    }
	#endif
    ret = platform_driver_register(&mtk_afe_mod_dai_driver);
    return ret;
}
module_init(mtk_soc_mod_dai_platform_init);

static void __exit mtk_soc_mod_dai_platform_exit(void)
{

    printk("%s\n", __func__);
    platform_driver_unregister(&mtk_afe_mod_dai_driver);
}

module_exit(mtk_soc_mod_dai_platform_exit);

MODULE_DESCRIPTION("AFE mod dai module platform driver");
MODULE_LICENSE("GPL");


