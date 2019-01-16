/******************************************************************************
*
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_common
 *
 * Project:
 * --------
 *   mt_soc_common function
 *
 * Description:
 * ------------
 *   Common function
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

#ifndef AUDIO_MT_SOC_COMMON_H
#define AUDIO_MT_SOC_COMMON_H

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_analog_type.h"


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
#include <sound/mt_soc_audio.h>

#define EFUSE_HP_TRIM

/*
define for PCM settings
*/
#define MAX_PCM_DEVICES     4
#define MAX_PCM_SUBSTREAMS  128
#define MAX_MIDI_DEVICES

/*
     PCM buufer size and pperiod size setting
*/
#define BT_DAI_MAX_BUFFER_SIZE     (16*1024)
#define BT_DAI_MIN_PERIOD_SIZE     1
#define BT_DAI_MAX_PERIOD_SIZE     BT_DAI_MAX_BUFFER_SIZE

#define Dl1_MAX_BUFFER_SIZE     (48*1024)
#define Dl1_MIN_PERIOD_SIZE       1
#define Dl1_MAX_PERIOD_SIZE     Dl1_MAX_BUFFER_SIZE

#define MAX_BUFFER_SIZE     (48*1024)
#define MIN_PERIOD_SIZE       1
#define MAX_PERIOD_SIZE     MAX_BUFFER_SIZE

#define UL1_MAX_BUFFER_SIZE     (48*1024)
#define UL1_MIN_PERIOD_SIZE       1
#define UL1_MAX_PERIOD_SIZE     UL1_MAX_BUFFER_SIZE

#define UL2_MAX_BUFFER_SIZE     (64*1024)
#define UL2_MIN_PERIOD_SIZE       1
#define UL2_MAX_PERIOD_SIZE     UL2_MAX_BUFFER_SIZE

#define AWB_MAX_BUFFER_SIZE     (64*1024)
#define AWB_MIN_PERIOD_SIZE       1
#define AWB_MAX_PERIOD_SIZE     AWB_MAX_BUFFER_SIZE

#define HDMI_MAX_BUFFER_SIZE     (192*1024)
#define HDMI_MIN_PERIOD_SIZE       1
#define HDMI_MAX_PERIODBYTE_SIZE     HDMI_MAX_BUFFER_SIZE
#define HDMI_MAX_2CH_16BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(2*2)) // 2 channels , 16bits
#define HDMI_MAX_8CH_16BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(8*2)) // 8 channels , 16bits
#define HDMI_MAX_2CH_24BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(2*2*2)) // 2 channels , 24bits
#define HDMI_MAX_8CH_24BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(8*2*2)) // 8 channels , 24bits




#define MRGRX_MAX_BUFFER_SIZE     (64*1024)
#define MRGRX_MIN_PERIOD_SIZE       1
#define MRGRX_MAX_PERIOD_SIZE     MRGRX_MAX_BUFFER_SIZE

#define FM_I2S_MAX_BUFFER_SIZE     (64*1024)
#define FM_I2S_MIN_PERIOD_SIZE       1
#define FM_I2S_MAX_PERIOD_SIZE     MRGRX_MAX_BUFFER_SIZE


#define SND_SOC_ADV_MT_FMTS (\
			       SNDRV_PCM_FMTBIT_S16_LE |\
			       SNDRV_PCM_FMTBIT_S16_BE |\
			       SNDRV_PCM_FMTBIT_U16_LE |\
			       SNDRV_PCM_FMTBIT_U16_BE |\
			       SNDRV_PCM_FMTBIT_S24_LE |\
			       SNDRV_PCM_FMTBIT_S24_BE |\
			       SNDRV_PCM_FMTBIT_U24_LE |\
			       SNDRV_PCM_FMTBIT_U24_BE |\
			       SNDRV_PCM_FMTBIT_S32_LE |\
			       SNDRV_PCM_FMTBIT_S32_BE |\
                                  SNDRV_PCM_FMTBIT_U32_LE |\
                                  SNDRV_PCM_FMTBIT_U32_BE)

#define SND_SOC_STD_MT_FMTS (\
			       SNDRV_PCM_FMTBIT_S16_LE |\
			       SNDRV_PCM_FMTBIT_S16_BE |\
			       SNDRV_PCM_FMTBIT_U16_LE |\
			       SNDRV_PCM_FMTBIT_U16_BE)

#define SOC_NORMAL_USE_RATE        SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define SOC_NORMAL_USE_RATE_MIN        8000
#define SOC_NORMAL_USE_RATE_MAX       48000
#define SOC_NORMAL_USE_CHANNELS_MIN    1
#define SOC_NORMAL_USE_CHANNELS_MAX    2
#define SOC_NORMAL_USE_PERIODS_MIN     1
#define SOC_NORMAL_USE_PERIODS_MAX     4
#define SOC_NORMAL_USE_BUFFERSIZE_MAX     48*1024


#define SOC_HIGH_USE_RATE        SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000
#define SOC_HIGH_USE_RATE_MIN        8000
#define SOC_HIGH_USE_RATE_MAX       192000
#define SOC_HIGH_USE_CHANNELS_MIN    1
#define SOC_HIGH_USE_CHANNELS_MAX    8

/* Conventional and unconventional sample rate supported */
static unsigned int soc_fm_supported_sample_rates[] =
{
    32000,44100,48000
};

static unsigned int soc_voice_supported_sample_rates[] =
{
    8000,16000,32000
};

/* Conventional and unconventional sample rate supported */
static unsigned int soc_normal_supported_sample_rates[] =
{
    8000, 11025, 12000, 16000, 22050, 24000, 32000,44100,48000
};

/* Conventional and unconventional sample rate supported */
static unsigned int soc_high_supported_sample_rates[] =
{
    8000, 11025, 12000, 16000, 22050, 24000, 32000,44100,48000,88200,96000,176400,192000
};

unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream,unsigned long count);
unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream,unsigned long count);


#endif

