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
 *   mt_soc_offloadv2.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio offloadv2 playback
 *
 * Author:
 * -------
 * HY Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

/*
=============================================================================================
------------------------------------------------------------------------------------------------------------
||                    E X T E R N A L   R E F E R E N C E
------------------------------------------------------------------------------------------------------------
=============================================================================================
*/
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "mt_soc_pcm_platform.h"
#include "AudDrv_OffloadCommon.h"
#include <linux/compat.h>
#include <linux/wakelock.h>
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
#include <audio_messenger_ipi.h>
#include <audio_ipi_client_playback.h>
#include <audio_dma_buf_control.h>
#endif



/*****************************************************************************
 * Variable Definition
****************************************************************************/
static int8   OffloadService_name[]         = "offloadserivce_driver_device";

#define USE_PERIODS_MAX        8192
#define OFFLOAD_SIZE_BYTES         (USE_PERIODS_MAX << 9) /* 4M */
#define FILL_BUFFERING             (USE_PERIODS_MAX << 3) /* 64K */
#define RESERVE_DRAMPLAYBACKSIZE   (USE_PERIODS_MAX << 2) /* 32 K*/
#if 0
static struct snd_pcm_hardware mtk_pcm_dl3_hardware = {
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
	.buffer_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.period_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max =    SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size =        0,
};
#endif


static struct AFE_OFFLOAD_SERVICE_T afe_offload_service = {
	.write_blocked   = false,
	.enable          = false,
	.drain           = false,
	.support         = true,
	.ipiwait         = false,
	.ipiresult       = true,
	.setDrain        = NULL,
	.volume          = 0x10000,
};

static struct AFE_OFFLOAD_T afe_offload_block = {
	.state             = OFFLOAD_STATE_INIT,
	.samplerate        = 0,
	.channels          = 0,
	.period_size       = 0,
	.hw_buffer_size    = 0,
	.hw_buffer_area    = NULL,
	.hw_buffer_addr    = 0,
	.data_buffer_size  = 0,
	.transferred       = 0,
	.copied_total      = 0,
	.wakelock          = false,
	.drain_state       = AUDIO_DRAIN_NONE,
};

static struct device offloaddev = {
	.init_name = "offloaddmadev",
	.coherent_dma_mask = ~0,             /* dma_alloc_coherent(): allow any address */
	.dma_mask = &offloaddev.coherent_dma_mask,  /* other APIs: use the same mask as coherent */
};

static uint32 Data_Wait_Queue_flag;
DECLARE_WAIT_QUEUE_HEAD(Data_Wait_Queue);

static AFE_MEM_CONTROL_T *pMemControl;
static unsigned int mPlaybackDramState;
static bool mPrepareDone;
static struct snd_dma_buffer *Dl3_Playback_dma_buf;
OFFLOAD_WRITE_T *params = NULL;
static bool irq7_user;
#define use_wake_lock
#ifdef use_wake_lock
static DEFINE_SPINLOCK(offload_lock);
struct wake_lock Offload_suspend_lock;
#endif
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static audio_resv_dram_t *p_resv_dram;
#endif
/*****************************************************************************
* Function  Declaration
****************************************************************************/
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
static void OffloadService_IPICmd_Send(audio_ipi_msg_data_t data_type,
					audio_ipi_msg_ack_t ack_type, uint16_t msg_id, uint32_t param1,
					uint32_t param2, char *payload);
static void OffloadService_IPICmd_Received(ipi_msg_t *ipi_msg);
#endif
#ifdef use_wake_lock
static void mtk_compr_offload_int_wakelock(bool enable);
#endif
/*
============================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D   W R I T E   B L O C K   F U N C T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================
*/
void OffloadService_ProcessWriteblocked(int flag)
{
	if (flag == 1) {
		pr_debug("offload drain wait\n");
		Data_Wait_Queue_flag = 0;
		wait_event_interruptible(Data_Wait_Queue, Data_Wait_Queue_flag);
		/* pr_debug("offload drain write restart\n"); */
	} else if (afe_offload_service.write_blocked) {
		pr_debug("offload write wait\n");
		Data_Wait_Queue_flag = 0;
		wait_event_interruptible(Data_Wait_Queue, Data_Wait_Queue_flag);
		/* pr_debug("offload write restart\n"); */
	}
}

int OffloadService_GetWriteblocked(void)
{
	return afe_offload_service.write_blocked;
}

void OffloadService_SetWriteblocked(bool flag)
{
	afe_offload_service.write_blocked = flag;
}

void OffloadService_ReleaseWriteblocked(void)
{
	if (Data_Wait_Queue_flag == 0) {
		Data_Wait_Queue_flag = 1;
		wake_up_interruptible(&Data_Wait_Queue);
	}
}


/*
============================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D   C O N T R O L   F U N C T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================
*/
int OffloadService_SetVolume(unsigned long arg)
{
	int retval = 0;

	afe_offload_service.volume = (unsigned int)arg;
	/* pr_debug("%s volume = %d\n", __func__,afe_offload_service.volume); */
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				MP3_VOLUME, afe_offload_service.volume, afe_offload_service.volume, NULL);
#endif
	return retval;
}

int OffloadService_GetVolume(void)
{
	return afe_offload_service.volume;
}

void OffloadService_SetEnable(bool enable)
{
	afe_offload_service.enable = enable;
	pr_debug("%s enable:0x%x\n", __func__, enable);
}

unsigned char OffloadService_GetEnable(void)
{
	pr_debug("%s enable:0x%x\n", __func__, afe_offload_service.enable);
	return afe_offload_service.enable;
}

void OffloadService_SetDrainCbk(void (*setDrain)(bool enable, int draintype))
{
	afe_offload_service.setDrain = setDrain;
	/* pr_debug("%s callback:%p\n", __func__, setDrain); */
}



void OffloadService_SetDrain(bool enable, int draintype)
{
	afe_offload_service.drain = enable;
	afe_offload_service.setDrain(enable, draintype);
}

/*****************************************************************************
* DL3 init
****************************************************************************/

static int mtk_offload_dl3_prepare(void)
{
	bool mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;

	if (mPrepareDone == false) {
		if (afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_S32_LE ||
		    afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_U32_LE) {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL3,
						AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_AFE_IO_Block_I2S1_DAC);
			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL3, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_AFE_IO_Block_I2S1_DAC);
			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  afe_offload_block.samplerate);
		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
			SetI2SDacOut(afe_offload_block.samplerate, false, mI2SWLen);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_offload_dl3_start(void)
{
	pr_debug("%s\n", __func__);
	/* here start digital part*/
	if (!mPrepareDone)
		mtk_offload_dl3_prepare();
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_AFE_IO_Block_MEM_DL3, Soc_Aud_AFE_IO_Block_I2S1_DAC);
	/*set IRQ info, only to Cm4*/
	if (!irq7_user) {
		irq_add_user(&irq7_user,
			Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
			afe_offload_block.samplerate,
			afe_offload_block.period_size);
		irq7_user = true;
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3,  afe_offload_block.samplerate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL3, afe_offload_block.channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, true);
	EnableAfe(true);
	return 0;
}

static int mtk_offload_dl3_stop(void)
{
	pr_debug("%s\n", __func__);
	if (irq7_user) {
		irq_remove_user(&irq7_user, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE);
		irq7_user = false;
	}
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, false);
	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			Soc_Aud_AFE_IO_Block_MEM_DL3, Soc_Aud_AFE_IO_Block_I2S1_DAC);
	return 0;
}

static int mtk_offload_dl3_close(void)
{
	pr_debug("%s\n", __func__);
	if (mPrepareDone == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);
		EnableAfe(false);
		mPrepareDone = false;
	}
	if (mPlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mPlaybackDramState = false;
	} else
		freeAudioSram((void *)&afe_offload_block);
	return 0;
}


static void SetDL3Buffer(void)
{
	AFE_BLOCK_T *pblock = &pMemControl->rBlock;

	pblock->pucPhysBufAddr =  (kal_uint32)afe_offload_block.hw_buffer_addr;
	pblock->pucVirtBufAddr =  afe_offload_block.hw_buffer_area;
	pblock->u4BufferSize =    afe_offload_block.hw_buffer_size;
	pblock->u4SampleNumMask = 0x001f;  /* 32 byte align */
	pblock->u4WriteIdx     = 0;
	pblock->u4DMAReadIdx    = 0;
	pblock->u4DataRemained  = 0;
	pblock->u4fsyncflag     = false;
	pblock->uResetFlag      = true;
	/* pr_debug("%s u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n", __func__,
	pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr); */
	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL3_BASE , pblock->pucPhysBufAddr , 0xffffffff);
	Afe_Set_Reg(AFE_DL3_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D V 1   D R I V E R   O P E R A T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
#ifdef use_wake_lock
static void mtk_compr_offload_int_wakelock(bool enable)
{
	spin_lock(&offload_lock);
	if (enable ^ afe_offload_block.wakelock) {
		if (enable)
			wake_lock(&Offload_suspend_lock);
		else
			wake_unlock(&Offload_suspend_lock);
		afe_offload_block.wakelock = enable;
	}
	spin_unlock(&offload_lock);
}
#endif
static void mtk_compr_offload_draindone(void)
{
	if (afe_offload_block.drain_state == AUDIO_DRAIN_ALL) {
		/* gapless mode clear vars */
		afe_offload_block.transferred       = 0;
		afe_offload_block.copied_total      = 0;
		afe_offload_block.buf.u4ReadIdx     = 0;
		afe_offload_block.buf.u4WriteIdx    = 0;
		afe_offload_block.drain_state       = AUDIO_DRAIN_NONE;
		afe_offload_block.state = OFFLOAD_STATE_PREPARE;
		/* for gapless */
		OffloadService_SetWriteblocked(false);
		OffloadService_SetDrain(false, afe_offload_block.drain_state);
		OffloadService_ReleaseWriteblocked();
	}
}

int mtk_compr_offload_copy(unsigned long arg)/* (OFFLOAD_WRITE_T __user *arg) */
{
	int retval = 0;

	if (arg == 0) {
		if (afe_offload_block.state == OFFLOAD_STATE_DRAIN) {
			if (afe_offload_block.transferred > (8 * USE_PERIODS_MAX)) {
				int silence_length = 0;
				unsigned int Drain_idx = 0;

				if (afe_offload_block.buf.u4ReadIdx > afe_offload_block.buf.u4WriteIdx)
					silence_length = afe_offload_block.buf.u4ReadIdx -
					afe_offload_block.buf.u4WriteIdx;
				else
					silence_length = afe_offload_block.buf.u4BufferSize -
					afe_offload_block.buf.u4WriteIdx;
				if (silence_length > (USE_PERIODS_MAX >> 1))
					silence_length = (USE_PERIODS_MAX >> 1);
				memset_io(afe_offload_block.buf.pucVirtBufAddr +
					afe_offload_block.buf.u4WriteIdx,
					0, silence_length);
				Drain_idx = afe_offload_block.buf.u4WriteIdx + silence_length;
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
				OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
							MP3_DRAIN, Drain_idx,
							afe_offload_block.drain_state, NULL);
#endif
#ifdef use_wake_lock
				mtk_compr_offload_int_wakelock(false);
#endif
			} else {
				afe_offload_block.drain_state = AUDIO_DRAIN_ALL;
				mtk_compr_offload_draindone();
				pr_debug("%s params alloc failed\n", __func__);
			}
			return retval;
		}
	}
	if (!params) {
		retval = -1;
		pr_debug("%s params alloc failed\n", __func__);
	} else {
		retval = copy_from_user((void *)params, (void __user *)arg, sizeof(*params));
		if (retval > 0) {
			retval = -1;
			pr_debug("%s failed!! retval = %d\n", __func__, retval);
		} else {
#ifdef use_wake_lock
			mtk_compr_offload_int_wakelock(true);
#endif
			switch (afe_offload_block.state) {
			case OFFLOAD_STATE_INIT:
			case OFFLOAD_STATE_IDLE:
				retval = OffloadService_CopyDatatoRAM(params->tmpBuffer, params->bytes);
				break;
			case OFFLOAD_STATE_PREPARE:
				retval = OffloadService_CopyDatatoRAM(params->tmpBuffer, params->bytes);
				break;
			case OFFLOAD_STATE_RUNNING:
				retval = OffloadService_CopyDatatoRAM(params->tmpBuffer, params->bytes);
				break;
			default:
				break;
			}
		}
	}
	return retval;
}

static void mtk_compr_offload_drain(bool enable, int draintype)
{
	pr_debug("%s, enable = %d type = %d\n", __func__, enable, draintype);
	if (enable) {
		afe_offload_block.drain_state = (AUDIO_DRAIN_TYPE_T)draintype;
		afe_offload_block.state       = OFFLOAD_STATE_DRAIN;
		mtk_compr_offload_copy(0);
	}
}


static int mtk_compr_offload_open(void)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	scp_reserve_mblock_t MP3DRAM;

	memset(&MP3DRAM, 0, sizeof(MP3DRAM));
	MP3DRAM.num = MP3_MEM_ID;
	p_resv_dram = get_reserved_dram();
	MP3DRAM.start_phys = get_reserve_mem_phys(MP3DRAM.num);
	MP3DRAM.start_virt = get_reserve_mem_virt(MP3DRAM.num);
	MP3DRAM.size = get_reserve_mem_size(MP3DRAM.num) - RESERVE_DRAMPLAYBACKSIZE;
	afe_offload_block.buf.pucPhysBufAddr = (kal_uint32)MP3DRAM.start_phys;
	afe_offload_block.buf.pucVirtBufAddr = (kal_uint8 *) MP3DRAM.start_virt;
	afe_offload_block.buf.u4BufferSize = (kal_uint32)MP3DRAM.size;
#else
	afe_offload_block.buf.pucVirtBufAddr = dma_alloc_coherent(&offloaddev,
		(OFFLOAD_SIZE_BYTES), &afe_offload_block.buf.pucPhysBufAddr,
		GFP_KERNEL);
	if (afe_offload_block.buf.pucVirtBufAddr != NULL)
		afe_offload_block.buf.u4BufferSize = (kal_uint32)OFFLOAD_SIZE_BYTES;
	else
		return -1;
#endif
	irq7_user = false;
	mPlaybackDramState = false;
	memset_io((void *)afe_offload_block.buf.pucVirtBufAddr, 0,
		  afe_offload_block.buf.u4BufferSize);
	afe_offload_block.hw_buffer_size = GetPLaybackSramFullSize();
	if (params == NULL)
		params = kmalloc(sizeof(*params), GFP_KERNEL);
	/* allocate dram */
	AudDrv_Clk_On();
	AudDrv_Allocate_mem_Buffer(&offloaddev, Soc_Aud_Digital_Block_MEM_DL3,
				   Dl3_MAX_BUFFER_SIZE);
	Dl3_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL3);
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL3);
	/* 3. Init Var & callback */
	afe_offload_block.buf.u4WriteIdx = 0;
	afe_offload_block.buf.u4ReadIdx = 0;
	OffloadService_SetDrainCbk(mtk_compr_offload_drain);
	OffloadService_SetDrain(false, afe_offload_block.drain_state);
	/* register received ipi function */
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	audio_reg_recv_message(TASK_SCENE_PLAYBACK_MP3, OffloadService_IPICmd_Received);
#endif
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(true);
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK, MP3_INIT,
				1, 0, NULL);
#endif
	return 0;
}


static void mtk_compr_offload_free(void)
{
	pr_debug("%s\n", __func__);
	mtk_offload_dl3_close();
	OffloadService_SetWriteblocked(false);
	afe_offload_block.state = OFFLOAD_STATE_INIT;
	/* memset_io((void *)afe_offload_block.hw_buffer_area, 0, afe_offload_block.hw_buffer_size); */
	SetOffloadEnableFlag(false);
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
}

static int mtk_compr_offload_set_params(unsigned long arg)
{
	int retval = 0;
	struct snd_compr_params *params;
	struct snd_codec codec;

	pr_debug("+ %s\n", __func__);

	if (AllocateAudioSram((dma_addr_t *)&afe_offload_block.hw_buffer_addr,
		(unsigned char **)&afe_offload_block.hw_buffer_area,
		afe_offload_block.hw_buffer_size, &afe_offload_block) == 0) {
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL3, false, (dma_addr_t)afe_offload_block.hw_buffer_addr);
	} else {
		afe_offload_block.hw_buffer_size = Dl3_MAX_BUFFER_SIZE;
		afe_offload_block.hw_buffer_area = (afe_offload_block.buf.pucVirtBufAddr +
						    afe_offload_block.buf.u4BufferSize);
		afe_offload_block.hw_buffer_addr = (afe_offload_block.buf.pucPhysBufAddr +
						    afe_offload_block.buf.u4BufferSize);
		mPlaybackDramState = true;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL3, true, (dma_addr_t)afe_offload_block.hw_buffer_addr);
		AudDrv_Emi_Clk_On();
	}

	SetDL3Buffer();
	params = kmalloc(sizeof(*params), GFP_KERNEL);
	if (!params) {
		retval = -1;
		return retval;
	}
	if (copy_from_user(params, (void __user *)arg, sizeof(*params))) {
		retval = -1;
		pr_debug("%s copy failed!!\n", __func__);
		kfree(params);
		return retval;
	}
	codec = params->codec;
	afe_offload_block.samplerate = codec.sample_rate;
	afe_offload_block.period_size = codec.reserved[0];
	afe_offload_block.channels = codec.ch_out;
	afe_offload_block.data_buffer_size = codec.reserved[1];
	afe_offload_block.pcmformat = codec.format;
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_BYPASS_ACK ,
					MP3_SETPRAM, 0, 0, NULL);
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				MP3_SETMEM, 0, 0, NULL);
#endif
	kfree(params);
	/* pr_debug("%s, rate:%x, period:%x,channel: %x ,hw_buf_size:%x area:%p addr:%x data_buf_size:%x\n",__func__,
	afe_offload_block.samplerate, afe_offload_block.period_size,afe_offload_block.channels,
	afe_offload_block.hw_buffer_size, afe_offload_block.hw_buffer_area, afe_offload_block.hw_buffer_addr,
	afe_offload_block.data_buffer_size); */
	return retval;
}

#ifdef LIANG_COMPRESS

static int mtk_compr_offload_get_params(struct snd_compr_stream *stream,
						struct snd_codec *params)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_compr_offload_get_caps(struct snd_compr_stream *stream,
					struct snd_compr_caps *caps)
{
#if 0
	caps->num_codecs        = 2;
	caps->codecs[0]         = SND_AUDIOCODEC_PCM;
	caps->codecs[1]         = SND_AUDIOCODEC_MP3;
	caps->min_fragment_size = 8192;
	caps->max_fragment_size = 0x7FFFFFFF;
	caps->min_fragments     = 2;
	caps->max_fragments     = 1875;
	pr_debug("%s\n", __func__);
#endif
	return 0;
}

static int mtk_compr_offload_get_codec_caps(struct snd_compr_stream *stream,
						struct snd_compr_codec_caps *codec)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_compr_offload_set_metadata(struct snd_compr_stream *stream,
						struct snd_compr_metadata *metadata)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_compr_offload_get_metadata(struct snd_compr_stream *stream,
						struct snd_compr_metadata *metadata)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_compr_offload_mmap(struct snd_compr_stream *stream,
					struct vm_area_struct *vma)
{
	pr_debug("%s\n", __func__);
	return 0;
}
#endif

#ifdef MTK_AUDIO_TUNNELING_SUPPORT
void OffloadService_IPICmd_Received(ipi_msg_t *ipi_msg)
{
	switch (ipi_msg->msg_id) {
	case MP3_NEEDDATA:
		afe_offload_block.buf.u4ReadIdx = ipi_msg->param1;
		OffloadService_SetWriteblocked(false);
		OffloadService_SetDrain(false, afe_offload_block.drain_state);
		OffloadService_ReleaseWriteblocked();
		break;
	case MP3_PCMCONSUMED:
		afe_offload_block.copied_total = ipi_msg->param1;
		afe_offload_service.ipiwait = false;
		afe_offload_service.ipiresult = true;
		break;
	case MP3_DRAINDONE:
		afe_offload_block.drain_state = AUDIO_DRAIN_ALL;
		mtk_compr_offload_draindone();
		afe_offload_service.ipiwait = false;
		afe_offload_service.ipiresult = true;
		break;
	case MP3_PCMDUMP_OK:
		afe_offload_service.ipiwait = false;
		playback_dump_message(ipi_msg);
		break;
	}
}

static void OffloadService_IPICmd_Send(audio_ipi_msg_data_t data_type,
						audio_ipi_msg_ack_t ack_type, uint16_t msg_id, uint32_t param1,
						uint32_t param2, char *payload)
{
	unsigned int test_buf[8];

	memset(test_buf, 0, sizeof(test_buf));
	if (data_type == AUDIO_IPI_PAYLOAD) {
		switch (msg_id) {
		case MP3_SETPRAM:
			test_buf[0] = afe_offload_block.channels;
			test_buf[1] = afe_offload_block.samplerate;
			test_buf[2] = afe_offload_block.pcmformat;
			param1 = sizeof(unsigned int) * 3;
			break;
		case MP3_SETMEM:
			test_buf[0] = afe_offload_block.buf.pucPhysBufAddr; /* dram addr */
			test_buf[1] = afe_offload_block.buf.u4BufferSize;
			test_buf[2] = (unsigned int)afe_offload_block.hw_buffer_addr; /* playback buffer */
			test_buf[3] = afe_offload_block.hw_buffer_size; /* playback size */
			test_buf[4] = mPlaybackDramState;
			param1 = sizeof(unsigned int) * 5;
			break;
		}
	}
	if (data_type != AUDIO_IPI_DMA)
		payload = (char *)&test_buf;
	audio_send_ipi_msg(TASK_SCENE_PLAYBACK_MP3, data_type, ack_type, msg_id, param1,
			param2, payload);
}
#endif

static bool OffloadService_IPICmd_Wait(IPI_MSG_ID id)
{
	int timeout = 0;

	while (afe_offload_service.ipiwait) {
		msleep(MP3_WAITCHECK_INTERVAL_MS);
		if (timeout++ >= MP3_IPIMSG_TIMEOUT) {
			/* pr_debug("Error: IPI MSG timeout:id_%x\n", id); */
			afe_offload_service.ipiwait = false;
			return false;
		}
	}
	/* pr_debug("IPI MSG -: time:%x, id:%x\n", timeout, id); */
	/* if (!afe_offload_service.ipiresult)
		return false; */
	return true;
}

int OffloadService_CopyDatatoRAM(void __user *buf, size_t count)
{
	size_t copy1, copy2;
	int free_space = 0;
	unsigned int u4BufferSize = afe_offload_block.buf.u4BufferSize;
	unsigned int u4WriteIdx = afe_offload_block.buf.u4WriteIdx;
	unsigned int u4ReadIdx = afe_offload_block.buf.u4ReadIdx;
	/* pr_debug("%s, count = %lu, transferred:%lu, writeIdx:%lu, length:%lu\n",
	__func__, (unsigned long)count,
	(unsigned long)afe_offload_block.transferred,
	(unsigned long)afe_offload_block.buf.u4WriteIdx,
	(unsigned long)afe_offload_block.buf.u4BufferSize); */
	if (count % 64 != 0)
		count = USE_PERIODS_MAX;
	Auddrv_Dl3_Spinlock_lock();
	if (u4WriteIdx >= u4ReadIdx)
		free_space = (u4BufferSize - u4WriteIdx) + u4ReadIdx;
	else
		free_space = u4ReadIdx - u4WriteIdx;
	free_space = Align64ByteSize(free_space);
	if (count < free_space) {
		if (count > (u4BufferSize - u4WriteIdx)) {
			copy1 = Align64ByteSize(u4BufferSize - u4WriteIdx);
			copy2 = Align64ByteSize(count - copy1);
			if (copy_from_user(afe_offload_block.buf.pucVirtBufAddr + u4WriteIdx, buf, copy1))
				goto Error;
			if (copy2 > 0)
				if (copy_from_user(afe_offload_block.buf.pucVirtBufAddr, buf + copy1, copy2))
					goto Error;
			u4WriteIdx = copy2;
		} else {
			count = Align64ByteSize(count);
			if (copy_from_user(afe_offload_block.buf.pucVirtBufAddr + u4WriteIdx, buf, count))
				goto Error;
			u4WriteIdx += count; /* update write index */
		}
		afe_offload_block.transferred += count;
	}
	u4WriteIdx %= u4BufferSize;
	afe_offload_block.buf.u4BufferSize = u4BufferSize;
	afe_offload_block.buf.u4WriteIdx = u4WriteIdx;
	afe_offload_block.buf.u4ReadIdx = u4ReadIdx;
	Auddrv_Dl3_Spinlock_unlock();
	if (u4WriteIdx >= u4ReadIdx)
		free_space = (u4BufferSize - u4WriteIdx) + u4ReadIdx;
	else
		free_space = u4ReadIdx - u4WriteIdx;
	if (count >= free_space) {
		OffloadService_SetWriteblocked(true);
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
		OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				MP3_SETWRITEBLOCK, u4WriteIdx, 0, NULL);
#endif
		pr_debug("%s buffer full , WIdx=%d\n", __func__, u4WriteIdx);
#ifdef use_wake_lock
		mtk_compr_offload_int_wakelock(false);
#endif
	}
	if (afe_offload_block.state != OFFLOAD_STATE_RUNNING) {
		if ((afe_offload_block.transferred > 8 * USE_PERIODS_MAX) ||
			(afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
			afe_offload_block.state == OFFLOAD_STATE_DRAIN)) {
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
			OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK, MP3_RUN,
			afe_offload_block.buf.u4BufferSize, 0, NULL);
#endif
			pr_debug("%s,MSG_DECODER_START\n", __func__);
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
		}
	}
	return 0;
Error:
	pr_debug("%s copy failed\n", __func__);
	return -1;
}
static int mtk_compr_offload_pointer(void __user *arg)
{
	int ret = 0;
	struct OFFLOAD_TIMESTAMP_T timestamp;

	if (!afe_offload_service.ipiwait) {
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
		OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
					   MP3_TSTAMP, 0, 0, NULL);
#endif

		afe_offload_service.ipiwait = true;
	}
	if (afe_offload_block.state == OFFLOAD_STATE_INIT ||
		afe_offload_block.state == OFFLOAD_STATE_IDLE ||
		afe_offload_block.state == OFFLOAD_STATE_PREPARE) {
		timestamp.sampling_rate  = 0;
		timestamp.pcm_io_frames = 0;
		return 0;
	}
	if (afe_offload_block.state == OFFLOAD_STATE_RUNNING && afe_offload_service.write_blocked)
		OffloadService_IPICmd_Wait(MP3_PCMCONSUMED);
	timestamp.sampling_rate = afe_offload_block.samplerate;
	timestamp.pcm_io_frames = afe_offload_block.copied_total >> 2; /* DSP return 16bit data */

	/* pr_debug("%s pcm_io_frames = %d\n", __func__,timestamp.pcm_io_frames); */
	if (copy_to_user((struct OFFLOAD_TIMESTAMP_T __user *)arg, &timestamp, sizeof(timestamp))) {
		pr_debug("%s copy to user fail\n", __func__);
		return -1;
	}
	return ret;
}

static void mtk_compr_offload_pcmdump(unsigned long enable)
{

	if (enable > 0) {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
		playback_open_dump_file();
#endif
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
		OffloadService_IPICmd_Send(AUDIO_IPI_DMA, AUDIO_IPI_MSG_BYPASS_ACK,
				   MP3_PCMDUMP_ON, p_resv_dram->size, enable, p_resv_dram->phy_addr);
#endif
	afe_offload_service.ipiwait = true;
	/* dsp dump closed */
	if (!enable) {
		OffloadService_IPICmd_Wait(MP3_PCMDUMP_OK);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
		playback_close_dump_file();
#endif
	}
}

/*****************************************************************************
 * mtk_compr_offload_trigger
****************************************************************************/
#ifdef LIANG_COMPRESS
static int mtk_compr_offload_trigger(struct snd_compr_stream *stream, int cmd)
{
	pr_debug("%s cmd:%x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		return mtk_compr_offload_start(stream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_compr_offload_resume(stream);
	case SNDRV_PCM_TRIGGER_STOP:
		return mtk_compr_offload_stop(stream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_compr_offload_pause(stream);
	case SND_COMPR_TRIGGER_DRAIN:
		return mtk_compr_offload_drain(stream);
	}
	return 0;
}
#endif
/*
=============================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D    TRIGGER   O P E R A T I O N S
------------------------------------------------------------------------------------------------------------
=============================================================================================
*/
static void mtk_compr_offload_start(void)
{
	/* AFE start , HW start */
	/* pr_debug("%s, rate:%x, channels:%x\n", __func__,
		afe_offload_block.samplerate, afe_offload_block.channels); */
	/* here start digital part */
	afe_offload_block.state = OFFLOAD_STATE_PREPARE;
	SetOffloadEnableFlag(true);
	afe_offload_block.drain_state = AUDIO_DRAIN_NONE;
	mtk_offload_dl3_start();
}

static void mtk_compr_offload_resume(void)
{
	pr_debug("%s\n", __func__);
	afe_offload_block.state = OFFLOAD_STATE_RUNNING;
	if (!mPrepareDone)
		mtk_offload_dl3_prepare();
	if (!irq7_user) {
		irq_add_user(&irq7_user,
			Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
			afe_offload_block.samplerate,
			afe_offload_block.period_size);
		irq7_user = true;
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3, afe_offload_block.samplerate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, true);
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK, MP3_RUN,
				afe_offload_block.buf.u4WriteIdx, 0, NULL);
#endif
	SetOffloadEnableFlag(true);
	OffloadService_ReleaseWriteblocked();
}

static void mtk_compr_offload_pause(void)
{
	pr_debug("%s\n", __func__);
	afe_offload_block.state = OFFLOAD_STATE_PAUSED;
	if (irq7_user) {
		irq7_user = false;
		irq_remove_user(&irq7_user, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE);
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3, afe_offload_block.samplerate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, false);
	SetOffloadEnableFlag(false);
	OffloadService_ReleaseWriteblocked();
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				MP3_PAUSE, 0, 0, NULL);
#endif

}
static int mtk_compr_offload_stop(void)
{
	int ret = 0;

	afe_offload_block.state = OFFLOAD_STATE_IDLE;
	pr_debug("%s\n", __func__);
	SetOffloadEnableFlag(false);
	/* stop hw */
	mtk_offload_dl3_stop();
	/* clear vars*/
	afe_offload_block.transferred       = 0;
	afe_offload_block.copied_total      = 0;
	afe_offload_block.buf.u4ReadIdx     = 0;
	afe_offload_block.buf.u4WriteIdx    = 0;
	afe_offload_block.drain_state       = AUDIO_DRAIN_NONE;
	memset_io((void *)afe_offload_block.buf.pucVirtBufAddr, 0,
		afe_offload_block.buf.u4BufferSize);
	OffloadService_SetWriteblocked(false);
	OffloadService_SetDrain(false, afe_offload_block.drain_state);
	OffloadService_ReleaseWriteblocked();
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
	OffloadService_IPICmd_Send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				MP3_CLOSE, 0, 0, NULL);
#endif
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
	return ret;
}


/*
============================================================================================================
||                        P L A T F O R M   D R I V E R   F O R   O F F L O A D   C O M M O N
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
static int OffloadService_open(struct inode *inode, struct file *fp)
{
	/* pr_warn("%s inode:%p, file:%p\n", __func__, inode, fp); */
	return 0;
}

static int OffloadService_release(struct inode *inode, struct file *fp)
{
	pr_warn("%s inode:%p, file:%p\n", __func__, inode, fp);
	return 0;
}

long OffloadService_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	/* pr_debug("OffloadService_ioctl cmd = %u arg = %lu\n", cmd, (unsigned long)arg); */
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OFFLOADSERVICE_WRITEBLOCK):
		OffloadService_ProcessWriteblocked((int)(unsigned long) arg);
		ret = 0;
		/* for power key event */
		if (afe_offload_block.state == OFFLOAD_STATE_DRAIN &&
			afe_offload_block.drain_state != AUDIO_DRAIN_NONE) {
			ret = 1;
		}
		break;
	case _IOC_NR(OFFLOADSERVICE_GETWRITEBLOCK):
		ret = OffloadService_GetWriteblocked();
		break;
	case _IOC_NR(OFFLOADSERVICE_SETGAIN):
		OffloadService_SetVolume(arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_SETDRAIN):
		OffloadService_SetDrain(1, (int)(unsigned long) arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_ACTION): { /* here to allocate DRAM from 4M~1M */
		int action = (int)(unsigned long) arg;

		if (action == 0)
			mtk_compr_offload_open();
		else if (action == 1)
			mtk_compr_offload_start();
		else if (action == 2)
			mtk_compr_offload_pause();
		else if (action == 3)
			mtk_compr_offload_resume();
		else if (action == 4)
			mtk_compr_offload_stop();
		else if (action == 5)
			mtk_compr_offload_free();
	}
	break;
	case _IOC_NR(OFFLOADSERVICE_WRITE):
		ret = mtk_compr_offload_copy(arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_SETPARAM):
		ret = mtk_compr_offload_set_params(arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_GETTIMESTAMP):
		mtk_compr_offload_pointer((void __user *)arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_PCMDUMP):
		mtk_compr_offload_pcmdump(arg);
		break;
	default:
		break;
	}
	return ret;
}

static ssize_t OffloadService_write(struct file *fp, const char __user *data,
				size_t count, loff_t *offset)
{
	return 0;
}

static ssize_t OffloadService_read(struct file *fp,  char __user *data,
				size_t count, loff_t *offset)
{
	return count;
}

static int OffloadService_flush(struct file *flip, fl_owner_t id)
{
	pr_warn("%s\n", __func__);
	mtk_compr_offload_free();
	return 0;
}

static int OffloadService_fasync(int fd, struct file *flip, int mode)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_remap_mmap(struct file *flip,
					struct vm_area_struct *vma)
{
	pr_warn("%s\n", __func__);
	return -1;
}

static int OffloadService_probe(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_remove(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static void OffloadService_shutdown(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
}

static int OffloadService_suspend(struct platform_device *dev,
				pm_message_t state)
{
	pr_warn("%s\n", __func__); /* only one suspend mode */
	return 0;
}

static int OffloadService_resume(struct platform_device *dev) /* wake up */
{
	pr_warn("%s\n", __func__);
	return 0;
}

/*
 * ioctl32 compat
 */
#ifdef CONFIG_COMPAT

static long OffloadService_ioctl_compat(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long ret;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OFFLOADSERVICE_WRITE): {
		OFFLOAD_WRITE_KERNEL_T __user *param32;
		OFFLOAD_WRITE_T __user *param;
		int err;
		compat_uint_t l;
		compat_uptr_t p;

		param32 = compat_ptr(arg);
		param = compat_alloc_user_space(sizeof(*param));
		err = get_user(p, &param32->tmpBuffer);
		err |= put_user(compat_ptr(p), &param->tmpBuffer);
		err |= get_user(l, &param32->bytes);
		err |= put_user(l, &param->bytes);
		ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)param);
	}
	break;
	default:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	}
	return ret;
}

#else
#define OffloadService_ioctl_compat   NULL
#endif
#ifdef LIANG_COMPRESS
static struct snd_compr_ops mtk_offload_compr_ops = {
	.open            = mtk_compr_offload_open,
	.free            = mtk_compr_offload_free,
	.set_params      = mtk_compr_offload_set_params,
	.get_params      = mtk_compr_offload_get_params,
	.set_metadata    = mtk_compr_offload_set_metadata,
	.get_metadata    = mtk_compr_offload_get_metadata,
	.trigger         = mtk_compr_offload_trigger,
	.pointer         = mtk_compr_offload_pointer,
	.copy            = mtk_compr_offload_copy,
	.mmap            = mtk_compr_offload_mmap,
	.ack             = NULL,
	.get_caps        = mtk_compr_offload_get_caps,
	.get_codec_caps  = mtk_compr_offload_get_codec_caps,
};
#endif

static const struct file_operations OffloadService_fops = {
	.owner          = THIS_MODULE,
	.open           = OffloadService_open,
	.release        = OffloadService_release,
	.unlocked_ioctl = OffloadService_ioctl,
	.compat_ioctl  = OffloadService_ioctl_compat,
	.write          = OffloadService_write,
	.read           = OffloadService_read,
	.flush          = OffloadService_flush,
	.fasync         = OffloadService_fasync,
	.mmap           = OffloadService_remap_mmap
};

static struct miscdevice OffloadService_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = OFFLOAD_DEVNAME,
	.fops = &OffloadService_fops,
};

const struct dev_pm_ops OffloadService_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};


static struct platform_driver OffloadService_driver = {
	.probe    = OffloadService_probe,
	.remove   = OffloadService_remove,
	.shutdown = OffloadService_shutdown,
	.suspend  = OffloadService_suspend,
	.resume   = OffloadService_resume,
	.driver   = {
#ifdef CONFIG_PM
		.pm     = &OffloadService_pm_ops,
#endif
		.name = OffloadService_name,
	},
};

static int OffloadService_mod_init(void)
{
	int ret = 0;

	pr_warn("OffloadService_mod_init\n");
	/* Register platform DRIVER */
	ret = platform_driver_register(&OffloadService_driver);
	if (ret) {
		pr_err("OffloadService Fail:%d - Register DRIVER\n", ret);
		return ret;
	}
	/* register MISC device */
	ret = misc_register(&OffloadService_misc_device);
	if (ret) {
		pr_err("OffloadService misc_register Fail:%d\n", ret);
		return ret;
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	audio_ipi_client_playback_init();
#endif
#ifdef use_wake_lock
	wake_lock_init(&Offload_suspend_lock, WAKE_LOCK_SUSPEND, "Offload wakelock");
#endif
	return 0;
}

static void  OffloadService_mod_exit(void)
{
	pr_warn("%s\n", __func__);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	audio_ipi_client_playback_deinit();
#endif

#ifdef use_wake_lock
	wake_lock_destroy(&Offload_suspend_lock);
#endif
}
module_init(OffloadService_mod_init);
module_exit(OffloadService_mod_exit);


/*
============================================================================================
------------------------------------------------------------------------------------------------------------
||                        License
------------------------------------------------------------------------------------------------------------
============================================================================================
*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek OffloadService Driver");
