/******************************************************************************
*
 *
 * Filename:
 * ---------
 *    mt_soc_pcm_common
 *
 * Project:
 * --------
 *     mt_soc_pcm_common function
 *
 *
 * Description:
 * ------------
 *   common function
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
---
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *

*******************************************************************************/

#include "mt_soc_pcm_common.h"

/* Conventional and unconventional sample rate supported */
const unsigned int soc_fm_supported_sample_rates[] =
{
    32000,44100,48000
};

const unsigned int soc_voice_supported_sample_rates[] =
{
    8000,16000,32000
};

const unsigned int soc_normal_supported_sample_rates[] =
{
    8000, 11025, 12000, 16000, 22050, 24000, 32000,44100,48000
};

const unsigned int soc_high_supported_sample_rates[] =
{
    8000, 11025, 12000, 16000, 22050, 24000, 32000,44100,48000,88200,96000,176400,192000
};


unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream,unsigned long count)
{
    unsigned long bytes = count;
    struct snd_pcm_runtime *runtime = substream->runtime;
    if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
    {
        bytes = bytes << 2;
    }
    else
    {
        bytes = bytes << 1;
    }
    if (runtime->channels == 2)
    {
        bytes = bytes << 1;
    }
    else if (runtime->channels == 4)
    {
        bytes = bytes << 2;
    }   
    else if (runtime->channels != 1)
    {
        bytes = bytes << 3;
    }      
    //printk("%s bytes = %d count = %d\n",__func__,bytes,count);
    return bytes;
}


unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream,unsigned long bytes)
{
    unsigned long count  = bytes;
    struct snd_pcm_runtime *runtime = substream->runtime;
    if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
    {
        count = count >> 2;
    }
    else
    {
        count = count >> 1;
    }
    if (runtime->channels == 2)
    {
        count = count >>1;
    }
    else if (runtime->channels == 4)
    {
        count = count >> 2;
    }   
    else if (runtime->channels != 1)
    {
        count = count >> 3;
    }    
    //printk("%s bytes = %d count = %d\n",__func__,bytes,count);
    return count;
}


