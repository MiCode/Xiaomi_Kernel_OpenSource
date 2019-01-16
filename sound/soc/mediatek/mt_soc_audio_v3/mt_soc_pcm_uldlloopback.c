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
 *   uldlloopback.c
 *
 * Project:
 * --------
 *   MT6595  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio uldlloopback
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
#include "mt_soc_afe_control.h"
#include "mt_soc_pcm_common.h"

/*
 *    function implementation
 */

static int mtk_uldlloopback_probe(struct platform_device *pdev);
static int mtk_uldlloopbackpcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_uldlloopbackpcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_uldlloopback_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_uldlloopback_hardware =
{
    .info = (SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_RESUME |
    SNDRV_PCM_INFO_MMAP_VALID),
    .formats =      SND_SOC_STD_MT_FMTS,
    .rates =        SOC_HIGH_USE_RATE,
    .rate_min =     SOC_NORMAL_USE_RATE_MIN,
    .rate_max =     SOC_NORMAL_USE_RATE_MAX,
    .channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
    .channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
    .buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
    .period_bytes_max = MAX_PERIOD_SIZE,
    .periods_min =      MIN_PERIOD_SIZE,
    .periods_max =      MAX_PERIOD_SIZE,
    .fifo_size =        0,
};

static struct snd_soc_pcm_runtime *pruntimepcm = NULL;

static struct snd_pcm_hw_constraint_list constraints_sample_rates =
{
    .count = ARRAY_SIZE(soc_high_supported_sample_rates),
    .list = soc_high_supported_sample_rates,
    .mask = 0,
};

static int mtk_uldlloopback_open(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    int err = 0;
    int ret = 0;

    printk("%s \n", __func__);
    AudDrv_Clk_On();
    AudDrv_ADC_Clk_On();	
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("%s  with mtk_uldlloopback_open \n",__func__);
        runtime->rate = 16000;
        return 0;
    }

    runtime->hw = mtk_uldlloopback_hardware;
    memcpy((void *)(&(runtime->hw)), (void *)&mtk_uldlloopback_hardware , sizeof(struct snd_pcm_hardware));

    ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                                     &constraints_sample_rates);
    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

    if (ret < 0)
    {
        printk("snd_pcm_hw_constraint_integer failed\n");
    }

    //print for hw pcm information
    printk("mtk_uldlloopback_open runtime rate = %d channels = %d \n", runtime->rate, runtime->channels);
    runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
    runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        printk("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_voice_constraints\n");
    }
    else
    {

    }

    if (err < 0)
    {
        printk("mtk_uldlloopbackpcm_close\n");
        mtk_uldlloopbackpcm_close(substream);
        return err;
    }
    printk("mtk_uldlloopback_open return\n");
    return 0;
}

static int mtk_uldlloopbackpcm_close(struct snd_pcm_substream *substream)
{
    printk("%s \n", __func__);
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("%s  with SNDRV_PCM_STREAM_CAPTURE \n",__func__);
        return 0;
    }

    // interconnection setting
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04, Soc_Aud_InterConnectionOutput_O01);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03, Soc_Aud_InterConnectionOutput_O03);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04, Soc_Aud_InterConnectionOutput_O04);

    // stop I2S
    Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, false);

    EnableAfe (false);

    AudDrv_Clk_Off();
    AudDrv_ADC_Clk_Off();
    return 0;
}

static int mtk_uldlloopbackpcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    printk("%s cmd = %d \n", __func__, cmd);

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

static int mtk_uldlloopback_pcm_copy(struct snd_pcm_substream *substream,
                                     int channel, snd_pcm_uframes_t pos,
                                     void __user *dst, snd_pcm_uframes_t count)
{
    count = count << 2;
    return 0;
}

static int mtk_uldlloopback_silence(struct snd_pcm_substream *substream,
                                    int channel, snd_pcm_uframes_t pos,
                                    snd_pcm_uframes_t count)
{
    printk("dummy_pcm_silence \n");
    return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_uldlloopback_page(struct snd_pcm_substream *substream,
                                          unsigned long offset)
{
    printk("dummy_pcm_page \n");
    return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static int mtk_uldlloopback_pcm_prepare(struct snd_pcm_substream *substream)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    uint32 eSamplingRate = SampleRateTransform(runtime->rate);
    uint32 dVoiceModeSelect = 0;
    uint32 Audio_I2S_Dac = 0;
    uint32 u32AudioI2S = 0;

    SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
    SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_ADC, true);

    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("%s  with mtk_uldlloopback_pcm_prepare \n",__func__);
        return 0;
    }

    printk("%s rate = %d\n", __func__,runtime->rate);

    Afe_Set_Reg(AFE_TOP_CON0, 0x00000000, 0xffffffff);
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

    // interconnection setting
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04, Soc_Aud_InterConnectionOutput_O01);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03, Soc_Aud_InterConnectionOutput_O03);
    SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04, Soc_Aud_InterConnectionOutput_O04);


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

    Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, ((dVoiceModeSelect << 19) | dVoiceModeSelect) << 17, 0x001E0000);
    Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF); // up8x txif sat on
    Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, ((dVoiceModeSelect < 3) ? 1 : 3) << 10, 0x00000C00);


    Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, 0x03117580, 0xffffffff);

    // ADDA , samplerate is 16K
    Afe_Set_Reg(AFE_I2S_CON, 0x00000409, 0xffffffff);
    Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x1, 0xffffffff);

    Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x00000001, 0x1);
    Afe_Set_Reg(AFE_ADDA_UL_SRC_CON1, 0x00000000, 0xffffffff);

    SetDLSrc2(runtime->rate);
    Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
    Audio_I2S_Dac |= (SampleRateTransform(runtime->rate) << 8);
    Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
    Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
    Audio_I2S_Dac |= (Soc_Aud_I2S_WLEN_WLEN_16BITS << 1);
    Afe_Set_Reg(AFE_I2S_CON1, Audio_I2S_Dac|0x1, MASK_ALL);

    Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
    Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, 0xFFFF0000, 0xffffffff); //  get dl gain


    // 2nd I2S Out
    u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12 ; //Low jitter mode
    u32AudioI2S |= SampleRateTransform(runtime->rate) << 8;
    u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; // us3 I2s format
    u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1; // 32 BITS

    printk("u32AudioI2S= 0x%x\n", u32AudioI2S);
    Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);

    EnableAfe (true);

    return 0;
}

static int mtk_uldlloopback_pcm_hw_params(struct snd_pcm_substream *substream,
                                          struct snd_pcm_hw_params *hw_params)
{
    int ret = 0;
    PRINTK_AUDDRV("mtk_uldlloopback_pcm_hw_params \n");
    return ret;
}

static int mtk_uldlloopback_pcm_hw_free(struct snd_pcm_substream *substream)
{
    PRINTK_AUDDRV("mtk_uldlloopback_pcm_hw_free \n");
    return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_afe_ops =
{
    .open =     mtk_uldlloopback_open,
    .close =    mtk_uldlloopbackpcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mtk_uldlloopback_pcm_hw_params,
    .hw_free =  mtk_uldlloopback_pcm_hw_free,
    .prepare =  mtk_uldlloopback_pcm_prepare,
    .trigger =  mtk_uldlloopbackpcm_trigger,
    .copy =     mtk_uldlloopback_pcm_copy,
    .silence =  mtk_uldlloopback_silence,
    .page =     mtk_uldlloopback_page,
};

static struct snd_soc_platform_driver mtk_soc_dummy_platform =
{
    .ops        = &mtk_afe_ops,
    .pcm_new    = mtk_asoc_uldlloopbackpcm_new,
    .probe      = mtk_afe_uldlloopback_probe,
};

static int mtk_uldlloopback_probe(struct platform_device *pdev)
{
    printk("mtk_uldlloopback_probe\n");

    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (!pdev->dev.dma_mask)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_ULDLLOOPBACK_PCM);
    }

    printk("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_platform(&pdev->dev,
                                     &mtk_soc_dummy_platform);
}

static int mtk_asoc_uldlloopbackpcm_new(struct snd_soc_pcm_runtime *rtd)
{
    int ret = 0;
    pruntimepcm  = rtd;
    printk("%s\n", __func__);
    return ret;
}


static int mtk_afe_uldlloopback_probe(struct snd_soc_platform *platform)
{
    printk("mtk_afe_uldlloopback_probe\n");
    return 0;
}


static int mtk_afe_uldlloopback_remove(struct platform_device *pdev)
{
    pr_debug("%s\n", __func__);
    snd_soc_unregister_platform(&pdev->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_uldlloopback_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_pcm_uldlloopback", },
    {}
};
#endif

static struct platform_driver mtk_afe_uldllopback_driver =
{
    .driver = {
        .name = MT_SOC_ULDLLOOPBACK_PCM,
        .owner = THIS_MODULE,
        #ifdef CONFIG_OF
        .of_match_table = mt_soc_pcm_uldlloopback_of_ids,
        #endif        
    },
    .probe = mtk_uldlloopback_probe,
    .remove = mtk_afe_uldlloopback_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_uldlloopback_dev;
#endif

static int __init mtk_soc_uldlloopback_platform_init(void)
{
    int ret = 0;
    printk("%s\n", __func__);
    #ifndef CONFIG_OF
    soc_mtkafe_uldlloopback_dev = platform_device_alloc(MT_SOC_ULDLLOOPBACK_PCM , -1);
    if (!soc_mtkafe_uldlloopback_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtkafe_uldlloopback_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtkafe_uldlloopback_dev);
        return ret;
    }
    #endif
    ret = platform_driver_register(&mtk_afe_uldllopback_driver);

    return ret;

}
module_init(mtk_soc_uldlloopback_platform_init);

static void __exit mtk_soc_uldlloopback_platform_exit(void)
{

    printk("%s\n", __func__);
    platform_driver_unregister(&mtk_afe_uldllopback_driver);
}
module_exit(mtk_soc_uldlloopback_platform_exit);

MODULE_DESCRIPTION("loopback module platform driver");
MODULE_LICENSE("GPL");


