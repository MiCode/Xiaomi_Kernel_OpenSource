/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_offload_gdma.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio offload_gdma playback
 *
 * Author:
 * -------
 * Doug Wang
 *
 *------------------------------------------------------------------------------
 *
 *******************************************************************************/
#if 1
/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    E X T E R N A L   R E F E R E N C E
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_OffloadCommon.h"
#include <linux/wakelock.h>
#include <linux/spinlock.h>


#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#endif

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    V A R I A B L E S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/

struct snd_buffer {
	unsigned char *area;   /* virtual pointer */
	dma_addr_t addr;    /* physical address */
	size_t bytes;               /* buffer size in bytes */
};

static AFE_MEM_CONTROL_T *pMemControl;
static int mPlaybackSramState;
static struct snd_dma_buffer *Dl2_Playback_dma_buf;
/* static DEFINE_SPINLOCK(auddrv_DLCtl_lock); */
static DEFINE_SPINLOCK(offload_lock);
static struct device *mDev;
#ifdef CONFIG_WAKELOCK
struct wake_lock Offload_suspend_lock;
#endif

/*
 *    function implementation
 */


static bool mPrepareDone;

#define USE_RATE        (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN        8000
#define USE_RATE_MAX        192000
#define USE_CHANNELS_MIN    1
#define USE_CHANNELS_MAX    2
#define USE_PERIODS_MIN     512
#define USE_PERIODS_MAX     8192
#define OFFLOAD_SIZE_MB     24
#define OFFLOAD_SIZE_BYTES  (OFFLOAD_SIZE_MB<<20)

static struct snd_pcm_hardware mtk_pcm_dl2_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats          = SND_SOC_ADV_MT_FMTS,
	.rates            = SOC_HIGH_USE_RATE,
	.rate_min         = SOC_HIGH_USE_RATE_MIN,
	.rate_max         = SOC_HIGH_USE_RATE_MAX,
	.channels_min     = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max     = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl2_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl2_MAX_PERIOD_SIZE,
	.periods_min      = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max      = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size        = 0,
};


static struct AFE_OFFLOAD_T afe_offload_block = {
	.u4WriteIdx        = 0,
	.u4ReadIdx         = 0,
	.state             = OFFLOAD_STATE_INIT,
	.compr_stream      = NULL,
	.pcm_stream        = NULL,
	.samplerate        = 0,
	.channels          = 0,
	.period_size       = 0,
	.hw_buffer_size    = 0,
	.hw_buffer_area    = NULL,
	.hw_buffer_addr    = 0,
	.data_buffer_size  = 0,
	.data_buffer_area  = NULL,
	.temp_buffer_size  = 0,
	.temp_buffer_area  = NULL,
	.copied_total      = 0,
	.transferred       = 0,
	.copied            = 0,
	.firstbuf          = false,
	.wakelock          = false,
};


/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D V 1   I N T E R N A L   O P E R A T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/

int mtk_compr_offload_gdma_read(void *stream)
{
	/* unsigned long flags; */
	AFE_BLOCK_T *Afe_Block;
	char *data_w_ptr;
	int copy_size, Afe_WriteIdx_tmp, avail_size;
	struct snd_compr_stream *pstream;
	struct snd_compr_runtime *runtime;

	copy_size = 0;
	/* check which memif nned to be write */
	Afe_Block = &pMemControl->rBlock;

	pstream = (struct snd_compr_stream *)stream;
	runtime = (struct snd_compr_runtime *)pstream->runtime;
	PRINTK_AUD_DL2("%s HW WriteIdx=0x%x, HW ReadIdx=0x%x, HW DataRemained=0x%x,  DataWIdx=0x%x, DataRIdx=0x%x\n",
		       __func__, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained,
		       afe_offload_block.u4WriteIdx, afe_offload_block.u4ReadIdx);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("AudDrv_write: u4BufferSize=0 Error");
		return 0;
	}
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;  /* free space of the buffer */

	if (afe_offload_block.u4WriteIdx >= afe_offload_block.u4ReadIdx)
		avail_size = afe_offload_block.u4WriteIdx - afe_offload_block.u4ReadIdx;
	else
		avail_size = afe_offload_block.data_buffer_size
		+ afe_offload_block.u4WriteIdx - afe_offload_block.u4ReadIdx;

	if (avail_size <=  copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = avail_size;

		if (afe_offload_block.state == OFFLOAD_STATE_RUNNING) {
			PRINTK_AUD_DL2("%s, SetWakeup by no data avail\n", __func__);
			OffloadService_SetWriteblocked(false);
			OffloadService_ReleaseWriteblocked();
		}
	}

	PRINTK_AUD_DL2("%s copysize:%x, availsize:%x, state:%x\n",
		__func__, copy_size, avail_size, afe_offload_block.state);

	copy_size = Align64ByteSize(copy_size);

	if (copy_size != 0) {
		kal_uint32 size_1 = 0, size_2 = 0;/* , temp_idx = 0; */

		if (afe_offload_block.state == OFFLOAD_STATE_RUNNING ||
			afe_offload_block.state == OFFLOAD_STATE_DRAIN) {
			if (afe_offload_block.u4WriteIdx >= afe_offload_block.u4ReadIdx) {
				size_1 = afe_offload_block.u4WriteIdx - afe_offload_block.u4ReadIdx;
				size_2 = 0;
			} else {
				size_1 = afe_offload_block.data_buffer_size - afe_offload_block.u4ReadIdx;
				size_2 = afe_offload_block.u4WriteIdx;
			}

			if (copy_size <= size_1) {
				data_w_ptr = (char *)afe_offload_block.data_buffer_area + afe_offload_block.u4ReadIdx;
				memcpy(afe_offload_block.temp_buffer_area, data_w_ptr, copy_size);
				afe_offload_block.u4ReadIdx += copy_size;
			} else {
				data_w_ptr = (char *)afe_offload_block.data_buffer_area + afe_offload_block.u4ReadIdx;
				memcpy(afe_offload_block.temp_buffer_area, data_w_ptr, size_1);
				data_w_ptr = (char *)afe_offload_block.data_buffer_area;
				memcpy(afe_offload_block.temp_buffer_area + size_1, data_w_ptr, copy_size - size_1);
				afe_offload_block.u4ReadIdx = copy_size - size_1;
			}
			afe_offload_block.copied_total += copy_size;
			afe_offload_block.copied += copy_size;
			if (afe_offload_block.copied >= (afe_offload_block.data_buffer_size >> 1)) {
				PRINTK_AUD_DL2("%s, SetWakeup by request data\n", __func__);
				afe_offload_block.copied = 0;
				afe_offload_block.transferred = 0;
				OffloadService_SetWriteblocked(false);
				OffloadService_ReleaseWriteblocked();
			}
		} else {
			memset_io(afe_offload_block.temp_buffer_area, 0, copy_size);
		}

		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		data_w_ptr = (char *)afe_offload_block.temp_buffer_area;


		if (Afe_WriteIdx_tmp + copy_size <= Afe_Block->u4BufferSize) { /* copy once */
			if (!memcpy((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr, copy_size)) {
				PRINTK_AUD_DL2("AudDrv_write Fail 0 copy from databuf");
				return -1;
			}

			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
		} else { /* copy twice */
			size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
			/* PRINTK_AUD_DL2("size_1=0x%x, size_2=0x%x\n", size_1, size_2); */
			/* Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1); */
			if ((!memcpy((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr , size_1))) {
				PRINTK_AUD_DL2("AudDrv_write Fail 1 copy from databuf");
				return -1;
			}
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;

			/* Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr + size_1, size_2); */
			if ((!memcpy((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), (data_w_ptr + size_1), size_2))) {
				PRINTK_AUD_DL2("AudDrv_write Fail 2  copy from databuf");
				return -1;
			}

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;

		}
	}
	return 0;
}

static int mtk_compr_offload_gdma_int_copy(struct snd_compr_runtime *runtime, char __user *buf, size_t count)
{
	void *dstn;
	size_t copy;
	u64 app_pointer = div64_u64(runtime->total_bytes_available, runtime->buffer_size);

	app_pointer = runtime->total_bytes_available - (app_pointer * runtime->buffer_size);
	dstn = afe_offload_block.data_buffer_area + app_pointer;
	PRINTK_AUD_DL2("%s, dstn:%p, app_pointer:%lu\n", __func__, dstn, (unsigned long)app_pointer);

	if (count <= afe_offload_block.data_buffer_size - app_pointer) {
		if (copy_from_user(dstn, buf, count)) {
			PRINTK_AUD_DL2("%s copy fail01\n", __func__);
			return 0;
		}
		if (afe_offload_block.u4WriteIdx >= afe_offload_block.data_buffer_size)
			afe_offload_block.u4WriteIdx -= afe_offload_block.data_buffer_size;

		afe_offload_block.u4WriteIdx += count;
	} else {
		copy = afe_offload_block.data_buffer_size - app_pointer;
		if (copy_from_user(dstn, buf, copy)) {
			PRINTK_AUD_DL2("%s copy fail02\n", __func__);
			return 0;
		}
		if (copy_from_user(afe_offload_block.data_buffer_area, buf + copy, count - copy)) {
			PRINTK_AUD_DL2("%s copy fail03\n", __func__);
			return -EFAULT;
		}
		afe_offload_block.u4WriteIdx = count - copy;
	}
	afe_offload_block.transferred += count;

	return 0;

}


void mtk_compr_offload_gdma_int_setVolume(int vol)
{
	SetHwDigitalGain(vol, Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1);
}

void mtk_compr_offload_gdma_int_wakelock(bool enable)
{
#ifdef CONFIG_WAKELOCK
	spin_lock(&offload_lock);
	if (enable ^ afe_offload_block.wakelock) {
		if (enable)
			wake_lock(&Offload_suspend_lock);
		else
			wake_unlock(&Offload_suspend_lock);
		afe_offload_block.wakelock = enable;
	}
	spin_unlock(&offload_lock);
#endif
}


static int mtk_compr_offload_gdma_int_prepare(struct snd_compr_stream *stream)
{
	bool mI2SWLen;
	uint32 MclkDiv3;
	uint32 u32AudioI2S = 0;
	/* struct snd_compr_stream *runtime = stream->runtime; */
	pr_warn("%s, prepareddone:%x %x\n", __func__, mPrepareDone, afe_offload_block.pcmformat);

	if (mPrepareDone == false) {

		if (afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_S32_LE ||
			afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O04);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O00);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O01);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O13);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O14);

			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O04);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O13);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O14);

			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  afe_offload_block.samplerate);

		u32AudioI2S = SampleRateTransform(afe_offload_block.samplerate) << 8;
		u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; /* us3 I2s format */
		u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1; /* 32bit */

		MclkDiv3 = SetCLkMclk(Soc_Aud_I2S1, afe_offload_block.samplerate); /* select I2S */
		MclkDiv3 = SetCLkMclk(Soc_Aud_I2S3, afe_offload_block.samplerate); /* select I2S */
		SetCLkBclk(MclkDiv3,  afe_offload_block.samplerate,
			afe_offload_block.channels, Soc_Aud_I2S_WLEN_WLEN_32BITS);
		u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
			Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
		}
		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
			SetI2SDacOut(afe_offload_block.samplerate, true, mI2SWLen);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		}

		mPrepareDone = true;
	}
	return 0;
}

static int mtk_compr_offload_gdma_int_start(struct snd_compr_stream *stream)
{
	pr_warn("%s, rate:%x, channels:%x\n", __func__, afe_offload_block.samplerate, afe_offload_block.channels);

	if (!mPrepareDone)
		mtk_compr_offload_gdma_int_prepare(stream);

	/* here start digital part */
	afe_offload_block.state = OFFLOAD_STATE_PREPARE;

	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O13);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O14);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I10, Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I11, Soc_Aud_InterConnectionOutput_O04);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O13);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O14);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I10, Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I11, Soc_Aud_InterConnectionOutput_O02);


	/* Set HW_GAIN */
	SetHwDigitalGainMode(Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1, afe_offload_block.samplerate, 0x80);
	SetHwDigitalGainEnable(Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1, true);
	SetHwDigitalGain(OffloadService_GetVolume(), Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1);


	irq_add_user(&afe_offload_block,
		     Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
		     afe_offload_block.samplerate,
		     afe_offload_block.period_size);
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL2, afe_offload_block.samplerate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL2, afe_offload_block.channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, true);
	SetOffloadEnableFlag(true);

	EnableAfe(true);
	return 0;
}

static int mtk_compr_offload_gdma_int_resume(struct snd_compr_stream *stream)
{
	afe_offload_block.state = afe_offload_block.pre_state;

	irq_add_user(&afe_offload_block,
		     Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
		     afe_offload_block.samplerate,
		     afe_offload_block.period_size);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, true);
	SetOffloadEnableFlag(true);
	OffloadService_ReleaseWriteblocked();

	return 0;
}

static int mtk_compr_offload_gdma_int_pause(struct snd_compr_stream *stream)
{
	afe_offload_block.pre_state = afe_offload_block.state;
	afe_offload_block.state = OFFLOAD_STATE_PAUSED;
	irq_remove_user(&afe_offload_block, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, false);
	SetOffloadEnableFlag(false);

	OffloadService_ReleaseWriteblocked();

	return 0;
}

static int mtk_compr_offload_gdma_int_stop(struct snd_compr_stream *stream)
{
	pr_warn("%s\n", __func__);
	afe_offload_block.state = OFFLOAD_STATE_IDLE;
	irq_remove_user(&afe_offload_block, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, false);
	SetOffloadEnableFlag(false);

	/* here start digital part */
#if 1
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O15);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O16);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I12, Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I13, Soc_Aud_InterConnectionOutput_O04);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O15);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O16);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I12, Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I13, Soc_Aud_InterConnectionOutput_O02);
#else
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I07, Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I08, Soc_Aud_InterConnectionOutput_O04);
#endif
	SetHwDigitalGainEnable(Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1, false);
	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL2);

	afe_offload_block.transferred  = 0;
	afe_offload_block.copied       = 0;
	afe_offload_block.copied_total = 0;
	OffloadService_SetWriteblocked(false);
	afe_offload_block.u4ReadIdx     = 0;
	afe_offload_block.u4WriteIdx    = 0;

	OffloadService_ReleaseWriteblocked();

	return 0;
}


static int mtk_compr_offload_gdma_int_drain(struct snd_compr_stream *stream)
{
	OffloadService_SetWriteblocked(true);
	afe_offload_block.state = OFFLOAD_STATE_DRAIN;
	OffloadService_ReleaseWriteblocked();
	return 0;
}


static void SetDL2Buffer(void)
{
	AFE_BLOCK_T *pblock = &pMemControl->rBlock;

	pblock->pucPhysBufAddr  = afe_offload_block.hw_buffer_addr;
	pblock->pucVirtBufAddr  = afe_offload_block.hw_buffer_area;
	pblock->u4BufferSize    = afe_offload_block.hw_buffer_size;
	pblock->u4SampleNumMask = 0x001f;  /* 32 byte align */
	pblock->u4WriteIdx      = 0;
	pblock->u4DMAReadIdx    = 0;
	pblock->u4DataRemained  = 0;
	pblock->u4fsyncflag     = false;
	pblock->uResetFlag      = true;
	pr_warn("SetDL2Buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL2_BASE , pblock->pucPhysBufAddr , 0xffffffff);
	Afe_Set_Reg(AFE_DL2_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
	memset_io((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}


/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D V 1   D R I V E R   O P E R A T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/

/*****************************************************************************
 * mtk_compr_offload_gdma_free
****************************************************************************/
static int mtk_compr_offload_gdma_free(struct snd_compr_stream *stream)
{
	PRINTK_AUDDRV("%s\n", __func__);
	if (mPrepareDone == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
			Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

		/* RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL2, &afe_offload_block.pcm_stream); */

		EnableAfe(false);
		mPrepareDone = false;
	}

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_Off();

	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();
	AudDrv_Clk_Off();
	ClrOffloadCbk(Soc_Aud_Digital_Block_MEM_DL2, stream);

	if (afe_offload_block.data_buffer_area != NULL) {
		vfree(afe_offload_block.data_buffer_area);
		afe_offload_block.data_buffer_area = NULL;
	}

	if (afe_offload_block.temp_buffer_area != NULL) {
		kfree(afe_offload_block.temp_buffer_area);
		afe_offload_block.temp_buffer_area = NULL;
	}

	/* memset_io(afe_offload_block, 0, sizeof(afe_offload_block)); */
	OffloadService_SetWriteblocked(false);
	afe_offload_block.state = OFFLOAD_STATE_INIT;
	SetOffloadEnableFlag(false);
#ifdef CONFIG_WAKELOCK
	mtk_compr_offload_gdma_int_wakelock(false);
	wake_lock_destroy(&Offload_suspend_lock);
#endif

	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_open
****************************************************************************/
#if 0
static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};
#endif
static int mtk_compr_offload_gdma_open(struct snd_compr_stream *stream)
{
	/* int ret = 0; */
	/* struct snd_compr_runtime *runtime = stream->runtime; */
	pr_warn("%s\n", __func__);
	afe_offload_block.pcm_stream =
		kmalloc(sizeof(struct snd_pcm_substream), GFP_KERNEL);
	if (afe_offload_block.pcm_stream == NULL) {
		pr_err("%s, allocate pcm sub stream fail\n", __func__);
		mtk_compr_offload_gdma_free(stream);
		return -ENOMEM;
	}

	AfeControlSramLock();
#if 1
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_pcm_dl2_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
	} else
#endif

	{
		mtk_pcm_dl2_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
	}

	AfeControlSramUnLock();


	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_On();

	pr_warn("%s, mtk_pcm_dl2_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n",
	       __func__, mtk_pcm_dl2_hardware.buffer_bytes_max, mPlaybackSramState);

	AudDrv_Clk_On();
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL2);


	if (stream->direction == SND_COMPRESS_PLAYBACK)
		pr_warn("%s, SNDRV_COMPRESS_STREAM_PLAYBACK mtkcompress_dl2playback_constraints\n", __func__);
	else
		pr_warn("%s, SNDRV_COMPRESS_STREAM_CAPTURE mtkcompress_dl2playback_constraints\n", __func__);


	afe_offload_block.compr_stream  = stream;
	/* SetOffloadCbk(Soc_Aud_Digital_Block_MEM_DL2, stream,mtk_compr_offload_gdma_read); */
	OffloadService_SetVolumeCbk(mtk_compr_offload_gdma_int_setVolume);
	afe_offload_block.state         = OFFLOAD_STATE_IDLE;
	afe_offload_block.transferred   = 0;
	afe_offload_block.copied_total  = 0;
	afe_offload_block.copied        = 0;
	OffloadService_SetWriteblocked(false);
	afe_offload_block.firstbuf      = false;
	afe_offload_block.u4ReadIdx     = 0;
	afe_offload_block.u4WriteIdx    = 0;
#ifdef CONFIG_WAKELOCK
	wake_lock_init(&Offload_suspend_lock, WAKE_LOCK_SUSPEND, "Offload wakelock");
	mtk_compr_offload_gdma_int_wakelock(true);
#endif
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_params
****************************************************************************/
static int mtk_compr_offload_gdma_get_params(struct snd_compr_stream *stream,
					     struct snd_codec *params)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_params
****************************************************************************/
static int mtk_compr_offload_gdma_set_params(struct snd_compr_stream *stream,
					     struct snd_compr_params *params)
{
	struct snd_codec codec = params->codec;

	afe_offload_block.samplerate = codec.sample_rate;
	afe_offload_block.period_size = codec.reserved[0];
	afe_offload_block.channels = codec.ch_out;
	afe_offload_block.data_buffer_size = codec.reserved[1];
	afe_offload_block.pcmformat = codec.format;

	afe_offload_block.data_buffer_area = vmalloc(afe_offload_block.data_buffer_size);

	if (!(afe_offload_block.data_buffer_area))
		pr_warn("%s fail to allocate data buffer, size:%x\n", __func__, afe_offload_block.data_buffer_size);

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL) {
		afe_offload_block.hw_buffer_size = AFE_INTERNAL_SRAM_SIZE;
		afe_offload_block.hw_buffer_area = (kal_int8 *)Get_Afe_SramBase_Pointer();
		afe_offload_block.hw_buffer_addr = AFE_INTERNAL_SRAM_PHY_BASE;
	} else {
		afe_offload_block.hw_buffer_size = Dl2_MAX_BUFFER_SIZE;
		afe_offload_block.hw_buffer_area = Dl2_Playback_dma_buf->area;
		afe_offload_block.hw_buffer_addr = Dl2_Playback_dma_buf->addr;
	}

	afe_offload_block.temp_buffer_size = afe_offload_block.hw_buffer_size;
	afe_offload_block.temp_buffer_area = kmalloc(afe_offload_block.temp_buffer_size, GFP_KERNEL);
	if (afe_offload_block.temp_buffer_area == NULL)
		pr_warn("%s fail to allocate temp buffer, size:%x\n", __func__, afe_offload_block.temp_buffer_size);

	SetDL2Buffer();

	PRINTK_AUD_DL2("%s, rate:%x, period:%x, hw_buf_size:%x area:%p addr:%x data_buf_size:%x\n", __func__,
		       afe_offload_block.samplerate, afe_offload_block.period_size,
		       afe_offload_block.hw_buffer_size,
		       afe_offload_block.hw_buffer_area,
		       afe_offload_block.hw_buffer_addr,
		       afe_offload_block.data_buffer_size);

	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_caps
****************************************************************************/
static int mtk_compr_offload_gdma_get_caps(struct snd_compr_stream *stream,
					   struct snd_compr_caps *caps)
{
	caps->num_codecs        = 2;
	caps->codecs[0]         = SND_AUDIOCODEC_PCM;
	caps->codecs[1]         = SND_AUDIOCODEC_MP3;
	caps->min_fragment_size = 8192;
	caps->max_fragment_size = 0x7FFFFFFF;
	caps->min_fragments     = 2;
	caps->max_fragments     = 1875;
	pr_warn("%s\n", __func__);
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_codec_caps
****************************************************************************/
static int mtk_compr_offload_gdma_get_codec_caps(struct snd_compr_stream *stream,
						 struct snd_compr_codec_caps *codec)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_set_metadata
****************************************************************************/
static int mtk_compr_offload_gdma_set_metadata(struct snd_compr_stream *stream,
					       struct snd_compr_metadata *metadata)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_metadata
****************************************************************************/
static int mtk_compr_offload_gdma_get_metadata(struct snd_compr_stream *stream,
					       struct snd_compr_metadata *metadata)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_get_mmap
****************************************************************************/
static int mtk_compr_offload_gdma_mmap(struct snd_compr_stream *stream,
				       struct vm_area_struct *vma)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}

int mtk_compr_offload_gdma_copy(struct snd_compr_stream *stream, char __user *buf, size_t count)
{
	struct snd_compr_runtime *runtime = stream->runtime;

	mtk_compr_offload_gdma_int_wakelock(true);
	switch (afe_offload_block.state) {
	case OFFLOAD_STATE_INIT:
	case OFFLOAD_STATE_IDLE:
		/* PRINTK_AUDDRV("Not to write in state:%x\n", afe_offload_block.state); */
		break;
	case OFFLOAD_STATE_PREPARE:
		mtk_compr_offload_gdma_int_copy(runtime, buf, count);
		if (afe_offload_block.transferred >= 16384) {
			PRINTK_AUD_DL2("%s buffer full under preparing\n", __func__);
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
			afe_offload_block.transferred = 0;
			afe_offload_block.firstbuf = true;
		}
		break;
	case OFFLOAD_STATE_RUNNING:
		mtk_compr_offload_gdma_int_copy(runtime, buf, count);
		if (afe_offload_block.firstbuf) {
			if (afe_offload_block.transferred >= (afe_offload_block.data_buffer_size - 32768)) {
				PRINTK_AUD_DL2("%s buffer full under first buffer\n", __func__);
				OffloadService_SetWriteblocked(true);
				afe_offload_block.firstbuf = false;
				mtk_compr_offload_gdma_int_wakelock(false);
			}
		} else {
			if (afe_offload_block.transferred >= (afe_offload_block.data_buffer_size >> 1)) {
				PRINTK_AUD_DL2("%s buffer full under running\n", __func__);
				OffloadService_SetWriteblocked(true);
				mtk_compr_offload_gdma_int_wakelock(false);
			}
		}
		break;
	case OFFLOAD_STATE_DRAIN:
		break;
	default:
		break;
	}
	return count;

}

/*****************************************************************************
 * mtk_compr_offload_gdma_ack
****************************************************************************/
#if 0
static int mtk_compr_offload_gdma_ack(struct snd_compr_stream *stream, size_t bytes)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	return 0;
}
#endif

/*****************************************************************************
 * mtk_compr_offload_gdma_trigger
****************************************************************************/
static int mtk_compr_offload_gdma_trigger(struct snd_compr_stream *stream, int cmd)
{
	PRINTK_AUD_DL2("%s cmd:%x\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		return mtk_compr_offload_gdma_int_start(stream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_compr_offload_gdma_int_resume(stream);
	case SNDRV_PCM_TRIGGER_STOP:
		return mtk_compr_offload_gdma_int_stop(stream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_compr_offload_gdma_int_pause(stream);
	case SND_COMPR_TRIGGER_DRAIN:
		return mtk_compr_offload_gdma_int_drain(stream);
	}
	return 0;
}

/*****************************************************************************
 * mtk_compr_offload_gdma_pointer
****************************************************************************/
static int mtk_compr_offload_gdma_pointer(struct snd_compr_stream *stream,
					  struct snd_compr_tstamp *tstamp)
{

	if (afe_offload_block.state == OFFLOAD_STATE_INIT || afe_offload_block.state == OFFLOAD_STATE_IDLE) {
		tstamp->sampling_rate = 0;
		return -1;
	}

	tstamp->copied_total = afe_offload_block.copied_total;
	tstamp->sampling_rate = afe_offload_block.samplerate;
	tstamp->pcm_io_frames = afe_offload_block.copied_total >> 3;

	return 0;
}

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                D U M M Y   O P E R A T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
#if 0
static int mtk_pcm_offload_gdma_open(struct snd_pcm_substream *substream)
{
	PRINTK_AUD_DL2("%s\n", __func__);
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	/* int ret = 0; */
	/* ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE, &constraints_sample_rates); */
	return 0;
}

static int mtk_pcm_offload_gdma_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_offload_gdma_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	return 0;
}

static int mtk_pcm_offload_gdma_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_offload_gdma_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_offload_gdma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_offload_gdma_pointer(struct snd_pcm_substream *substream)
{
	return 0;
}
#endif
/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                S O U N D   S O C   P L A T F O R M   D R I V E R   R E G I S T R A T I O N
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
static struct snd_compr_ops mtk_offload_gdma_compr_ops = {
	.open            = mtk_compr_offload_gdma_open,
	.free            = mtk_compr_offload_gdma_free,
	.set_params      = mtk_compr_offload_gdma_set_params,
	.get_params      = mtk_compr_offload_gdma_get_params,
	.set_metadata    = mtk_compr_offload_gdma_set_metadata,
	.get_metadata    = mtk_compr_offload_gdma_get_metadata,
	.trigger         = mtk_compr_offload_gdma_trigger,
	.pointer         = mtk_compr_offload_gdma_pointer,
	.copy            = mtk_compr_offload_gdma_copy,
	.mmap            = mtk_compr_offload_gdma_mmap,
	/* .ack             = mtk_compr_offload_gdma_ack, */
	.ack             = NULL,
	.get_caps        = mtk_compr_offload_gdma_get_caps,
	.get_codec_caps  = mtk_compr_offload_gdma_get_codec_caps,
};
#if 0
static struct snd_pcm_ops mtk_offload_gdma_ops = {
	.open      = mtk_pcm_offload_gdma_open,
	.close     = mtk_pcm_offload_gdma_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.prepare   = mtk_pcm_offload_gdma_prepare,
	.trigger   = mtk_pcm_offload_gdma_trigger,
	.pointer   = mtk_pcm_offload_gdma_pointer,
	.hw_params = mtk_pcm_offload_gdma_hw_params,
	.hw_free   = mtk_pcm_offload_gdma_hw_free,
};
#endif

static int mtk_asoc_offload_gdma_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	PRINTK_AUD_DL2("%s\n", __func__);
	return ret;
}

static int mtk_asoc_offload_gdma_probe(struct snd_soc_platform *platform)
{
	PRINTK_AUD_DL2("mtk_asoc_offload_gdma_probe\n");

	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL2, Dl2_MAX_BUFFER_SIZE);
	Dl2_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL2);
	return 0;
}

static struct snd_soc_platform_driver mtk_offload_gdma_soc_platform = {
	/* .ops        = &mtk_offload_gdma_ops, */
	.compr_ops  = &mtk_offload_gdma_compr_ops,
	.pcm_new    = mtk_asoc_offload_gdma_new,
	.probe      = mtk_asoc_offload_gdma_probe,
};

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                        P L A T F O R M   D R I V E R   R E G I S T R A T I O N
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
static int mtk_offload_gdma_probe(struct platform_device *pdev)
{
	int ret = 0;

	PRINTK_AUD_DL2("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_OFFLOAD_GDMA_PCM);

	PRINTK_AUD_DL2("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	ret = snd_soc_register_platform(&pdev->dev,
					&mtk_offload_gdma_soc_platform);

	PRINTK_AUD_DL2("%s: snd_soc_register_platform result:%u\n", __func__, ret);

	return ret;
}

static int mtk_offload_gdma_remove(struct platform_device *pdev)
{
	pr_warn("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_offload_gdma_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_offload_gdma", },
	{}
};
#endif

static struct platform_driver mtk_offload_gdma_driver = {
	.driver = {
		.name = MT_SOC_OFFLOAD_GDMA_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_offload_gdma_of_ids,
#endif
	},
	.probe = mtk_offload_gdma_probe,
	.remove = mtk_offload_gdma_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkoffload_gdma_dev;
#endif

static int __init mtk_soc_offload_gdma_init(void)
{
	int ret;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkoffload_gdma_dev = platform_device_alloc(MT_SOC_OFFLOAD_GDMA_PCM, -1);
	if (!soc_mtkoffload_gdma_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkoffload_gdma_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkoffload_gdma_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_offload_gdma_driver);
	return ret;

}
module_init(mtk_soc_offload_gdma_init);

static void __exit mtk_soc_offload_gdma_exit(void)
{
	pr_warn("%s\n", __func__);

	platform_driver_unregister(&mtk_offload_gdma_driver);
}
module_exit(mtk_soc_offload_gdma_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");

#endif
