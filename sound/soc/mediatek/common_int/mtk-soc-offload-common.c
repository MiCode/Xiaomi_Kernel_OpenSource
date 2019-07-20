/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
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
 ******************************************************************************
 */

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"

#include "mtk-auddrv-offloadcommon.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-digital-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/compat.h>
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include "adsp_feature_define.h"
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#include <audio_dma_buf_control.h>
#include <audio_ipi_client_playback.h>
#include <audio_task_manager.h>
#endif

/*
 * Variable Definition
 *
 */

#define USE_PERIODS_MAX 8192
#define OFFLOAD_SIZE_BYTES (USE_PERIODS_MAX << 9)       /* 4M */
#define FILL_BUFFERING (USE_PERIODS_MAX << 3)		/* 64K */
#define RESERVE_DRAMPLAYBACKSIZE (USE_PERIODS_MAX << 2) /* 32 K*/

static struct afe_offload_service_t afe_offload_service = {
	.write_blocked = false,
	.enable = false,
	.drain = false,
	.ipiwait = false,
	.needdata = false,
	.ipiresult = true,
	.volume = 0x10000,
};

static struct afe_offload_param_t afe_offload_block = {
	.state = OFFLOAD_STATE_INIT,
	.samplerate = 0,
	.channels = 0,
	.period_size = 0,
	.hw_buffer_size = 0,
	.hw_buffer_area = NULL,
	.hw_buffer_addr = 0,
	.data_buffer_size = 0,
	.transferred = 0,
	.copied_total = 0,
	.write_blocked_idx = 0,
	.wakelock = false,
	.drain_state = AUDIO_DRAIN_NONE,
};

static struct snd_compr_stream *offload_stream;

static struct device *offload_dev;

static struct afe_mem_control_t *mem_control;
static unsigned int playback_dram_state;
static bool prepare_done;
static bool irq7_user;
static bool offload_playback_pause;
static bool offload_playback_resume;
#define use_wake_lock
#ifdef use_wake_lock
static DEFINE_SPINLOCK(offload_lock);
struct wakeup_source Offload_suspend_lock;
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
static struct audio_resv_dram_t *resv_dram;
#endif
/*
 * Function  Declaration
 */
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
static void offloadservice_ipicmd_send(uint8_t data_type, uint8_t ack_type,
				       unsigned short msg_id,
				       unsigned int param1, unsigned int param2,
				       char *payload);
static void offloadservice_ipicmd_received(struct ipi_msg_t *ipi_msg);
static void offloadservice_task_unloaded_handling(void);
#endif
static int offloadservice_copydatatoram(void __user *buf, size_t count);
#ifdef use_wake_lock
static void mtk_compr_offload_int_wakelock(bool enable);
#endif

/*
 * Function Implementation
 *
 */

static void offloadservice_setwriteblocked(bool flag)
{
	afe_offload_service.write_blocked = flag;
}

static void offloadservice_releasewriteblocked(void)
{
	offload_stream->runtime->state = SNDRV_PCM_STATE_RUNNING;
	wake_up(&offload_stream->runtime->sleep);
}

static int offloadservice_setvolume(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	afe_offload_service.volume =
		(unsigned int)ucontrol->value.integer.value[0];
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				   MP3_VOLUME, afe_offload_service.volume,
				   afe_offload_service.volume, NULL);
#endif
	return 0;
}

static int offloadservice_getvolume(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = afe_offload_service.volume;
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_dl3offload_controls[] = {
	SOC_SINGLE_EXT("offload digital volume", SND_SOC_NOPM, 0, 0x1000000, 0,
		       offloadservice_getvolume, offloadservice_setvolume),
};

/*
 * DL3 init
 *
 */

static int mtk_offload_dl3_prepare(void)
{
	bool mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;

	if (prepare_done == false) {
		if (afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_S32_LE ||
		    afe_offload_block.pcmformat == SNDRV_PCM_FORMAT_U32_LE) {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL3,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL3, AFE_WLEN_16_BIT);
			/* fix o3_o4 32bit high solution out */
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		}
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,
			      afe_offload_block.samplerate);
		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(afe_offload_block.samplerate, false,
				     mI2SWLen);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
		EnableAfe(true);
		prepare_done = true;
	}
	return 0;
}

static int mtk_offload_dl3_start(void)
{
	pr_debug("%s\n", __func__);
	/* here start digital part*/
	if (!prepare_done)
		mtk_offload_dl3_prepare();
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL3,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	/*set IRQ info, only to Cm4*/
	if (!irq7_user) {
		irq_add_user(&irq7_user, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
			     afe_offload_block.samplerate,
			     afe_offload_block.period_size);
		irq7_user = true;
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3,
		      afe_offload_block.samplerate);
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
			  Soc_Aud_AFE_IO_Block_MEM_DL3,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	return 0;
}

static int mtk_offload_dl3_close(void)
{
	if (prepare_done == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);
		EnableAfe(false);
		prepare_done = false;
	}
	if (playback_dram_state == true) {
		AudDrv_Emi_Clk_Off();
		playback_dram_state = false;
	} else
		freeAudioSram((void *)&afe_offload_block);
	return 0;
}

static void SetDL3Buffer(void)
{
	struct afe_block_t *pblock = &mem_control->rBlock;

	pblock->pucPhysBufAddr = (unsigned int)afe_offload_block.hw_buffer_addr;
	pblock->pucVirtBufAddr = afe_offload_block.hw_buffer_area;
	pblock->u4BufferSize = afe_offload_block.hw_buffer_size;
	pblock->u4SampleNumMask = 0x001f; /* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	Afe_Set_Reg(AFE_DL3_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL3_END,
		    pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);
	memset_io(pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}

/*
 *                 O F F L O A D V 1   D R I V E R   O P E R A T I O N S
 */
#ifdef use_wake_lock
static void mtk_compr_offload_int_wakelock(bool enable)
{
	spin_lock(&offload_lock);
	if (enable ^ afe_offload_block.wakelock) {
		if (enable)
			aud_wake_lock(&Offload_suspend_lock);
		else
			aud_wake_unlock(&Offload_suspend_lock);
		afe_offload_block.wakelock = enable;
	}
	spin_unlock(&offload_lock);
}
#endif

static int mtk_compr_offload_draindone(void)
{
	if (afe_offload_block.drain_state == AUDIO_DRAIN_ALL) {
		/* gapless mode clear vars */
		afe_offload_block.transferred = 0;
		afe_offload_block.copied_total = 0;
		afe_offload_block.buf.readIdx = 0;
		afe_offload_block.buf.writeIdx = 0;
		afe_offload_block.write_blocked_idx = 0;
		afe_offload_block.drain_state = AUDIO_DRAIN_NONE;
		afe_offload_block.state = OFFLOAD_STATE_PREPARE;
		/* for gapless */
		offloadservice_setwriteblocked(false);
		offloadservice_releasewriteblocked();
	}
	return 0;
}

int mtk_compr_offload_copy(struct snd_compr_stream *stream, char __user *buf,
			   size_t count)
{
	int ret = 0;

#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(true);
#endif
	ret = offloadservice_copydatatoram(buf, count);
	return ret;
}

static int mtk_compr_offload_drain(struct snd_compr_stream *stream)
{
	if (afe_offload_block.transferred > (8 * USE_PERIODS_MAX)) {
		int silence_length = 0;
		unsigned int Drain_idx = 0;

		afe_offload_block.state = OFFLOAD_STATE_DRAIN;
		afe_offload_block.drain_state = AUDIO_DRAIN_EARLY_NOTIFY;

		if (afe_offload_block.buf.readIdx >
		    afe_offload_block.buf.writeIdx)
			silence_length = afe_offload_block.buf.readIdx -
					 afe_offload_block.buf.writeIdx;
		else
			silence_length = afe_offload_block.buf.bufferSize -
					 afe_offload_block.buf.writeIdx;
		if (silence_length > (USE_PERIODS_MAX >> 1))
			silence_length = (USE_PERIODS_MAX >> 1);
		memset_io(afe_offload_block.buf.pucVirtBufAddr +
				  afe_offload_block.buf.writeIdx,
			  0, silence_length);
		Drain_idx = afe_offload_block.buf.writeIdx + silence_length;
		afe_offload_service.needdata = false;
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		offloadservice_ipicmd_send(
			AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK, MP3_DRAIN,
			Drain_idx, afe_offload_block.drain_state, NULL);
#endif
#ifdef use_wake_lock
		mtk_compr_offload_int_wakelock(false);
#endif
	} else {
		afe_offload_block.drain_state = AUDIO_DRAIN_ALL;
		mtk_compr_offload_draindone();
		pr_debug("%s params alloc failed\n", __func__);
	}
	return 1; /* make compress driver drain failed */
}

static int mtk_compr_offload_open(struct snd_compr_stream *stream)
{
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	scp_reserve_mblock_t MP3DRAM;

	memset(&MP3DRAM, 0, sizeof(MP3DRAM));
	MP3DRAM.num = MP3_MEM_ID;
	resv_dram = get_reserved_dram();
	MP3DRAM.start_phys = scp_get_reserve_mem_phys(MP3DRAM.num);
	MP3DRAM.start_virt = scp_get_reserve_mem_virt(MP3DRAM.num);
	MP3DRAM.size = scp_get_reserve_mem_size(MP3DRAM.num) -
		       RESERVE_DRAMPLAYBACKSIZE;
	afe_offload_block.buf.pucPhysBufAddr = (unsigned int)MP3DRAM.start_phys;
	afe_offload_block.buf.pucVirtBufAddr =
		(unsigned char *)MP3DRAM.start_virt;
	afe_offload_block.buf.bufferSize = (unsigned int)MP3DRAM.size;
#else
	afe_offload_block.buf.pucVirtBufAddr = dma_alloc_coherent(
		&offload_dev, (OFFLOAD_SIZE_BYTES),
		&afe_offload_block.buf.pucPhysBufAddr, GFP_KERNEL);
	if (afe_offload_block.buf.pucVirtBufAddr != NULL)
		afe_offload_block.buf.bufferSize =
			(unsigned int)OFFLOAD_SIZE_BYTES;
	else
		return -1;
#endif
	playback_dram_state = false;
	memset_io((void *)afe_offload_block.buf.pucVirtBufAddr, 0,
		  afe_offload_block.buf.bufferSize);
	afe_offload_block.hw_buffer_size = RESERVE_DRAMPLAYBACKSIZE;
	AudDrv_Clk_On();
	mem_control = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL3);
	/* 3. Init Var & callback */
	afe_offload_block.buf.writeIdx = 0;
	afe_offload_block.buf.readIdx = 0;
/* register received ipi function */
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(true);
#endif

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				   MP3_INIT, 1, 0, NULL);
	scp_register_feature(MP3_FEATURE_ID);
#endif
	offload_stream = stream;
#if defined(OFFLOAD_DEBUG_LO)
	pr_debug("%s()\n", __func__);
#endif
	return 0;
}

static int mtk_afe_dl3offload_probe(struct snd_soc_platform *platform)
{
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	audio_task_register_callback(TASK_SCENE_PLAYBACK_MP3,
				     offloadservice_ipicmd_received,
				     offloadservice_task_unloaded_handling);
#endif
	snd_soc_add_platform_controls(
		platform, Audio_snd_dl3offload_controls,
		ARRAY_SIZE(Audio_snd_dl3offload_controls));

	return 0;
}

static int mtk_compr_offload_free(struct snd_compr_stream *stream)
{
	pr_debug("%s()\n", __func__);
	mtk_offload_dl3_close();
	offloadservice_setwriteblocked(false);
	afe_offload_block.state = OFFLOAD_STATE_INIT;
	/* memset_io((void *)afe_offload_block.hw_buffer_area, 0,
	 * afe_offload_block.hw_buffer_size);
	 */
	SetOffloadEnableFlag(false);
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	scp_deregister_feature(MP3_FEATURE_ID);
#endif
	return 0;
}

static int mtk_compr_offload_set_params(struct snd_compr_stream *stream,
					struct snd_compr_params *params)
{
	struct snd_codec codec;

	codec = params->codec;
	if (AllocateAudioSram(
		    (dma_addr_t *)&afe_offload_block.hw_buffer_addr,
		    (unsigned char **)&afe_offload_block.hw_buffer_area,
		    afe_offload_block.hw_buffer_size, &afe_offload_block,
		    codec.format, false) == 0)
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL3, false,
			    (dma_addr_t)afe_offload_block.hw_buffer_addr);
	else {
		afe_offload_block.hw_buffer_size = Dl3_MAX_BUFFER_SIZE;
		afe_offload_block.hw_buffer_area =
			(afe_offload_block.buf.pucVirtBufAddr +
			 afe_offload_block.buf.bufferSize);
		afe_offload_block.hw_buffer_addr =
			(afe_offload_block.buf.pucPhysBufAddr +
			 afe_offload_block.buf.bufferSize);
		playback_dram_state = true;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL3, true,
			    (dma_addr_t)afe_offload_block.hw_buffer_addr);
		AudDrv_Emi_Clk_On();
	}
	SetDL3Buffer();
	afe_offload_block.samplerate = codec.sample_rate;
	afe_offload_block.period_size = codec.reserved[0];
	afe_offload_block.channels = codec.ch_out;
	afe_offload_block.data_buffer_size = codec.reserved[1];
	afe_offload_block.pcmformat = codec.format;
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				   MP3_SETPRAM, 0, 0, NULL);
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				   MP3_SETMEM, 0, 0, NULL);
#endif
	return 0;
}

static int mtk_compr_offload_get_params(struct snd_compr_stream *stream,
					struct snd_codec *params)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_compr_offload_get_caps(struct snd_compr_stream *stream,
				      struct snd_compr_caps *caps)
{
	pr_debug("%s\n", __func__);
	caps->num_codecs = 2;
	caps->codecs[0] = SND_AUDIOCODEC_PCM;
	caps->codecs[1] = SND_AUDIOCODEC_MP3;
	caps->min_fragment_size = 8192;
	caps->max_fragment_size = 0x7FFFFFFF;
	caps->min_fragments = 2;
	caps->max_fragments = 1875;
	return 0;
}

static int mtk_compr_offload_get_codec_caps(struct snd_compr_stream *stream,
					    struct snd_compr_codec_caps *codec)
{
	return 0;
}

static int mtk_compr_offload_set_metadata(struct snd_compr_stream *stream,
					  struct snd_compr_metadata *metadata)
{
	return 0;
}

static int mtk_compr_offload_get_metadata(struct snd_compr_stream *stream,
					  struct snd_compr_metadata *metadata)
{
	return 0;
}

static int mtk_compr_offload_mmap(struct snd_compr_stream *stream,
				  struct vm_area_struct *vma)
{
	return 0;
}

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
static void offloadservice_ipicmd_received(struct ipi_msg_t *ipi_msg)
{
	switch (ipi_msg->msg_id) {
	case MP3_NEEDDATA:
		afe_offload_block.buf.readIdx = ipi_msg->param1;
		offloadservice_setwriteblocked(false);
		offloadservice_releasewriteblocked();
		afe_offload_service.needdata = true;
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
#if defined(OFFLOAD_DEBUG_LO)
	pr_debug("%s msg_id :  %d\n", __func__, ipi_msg->msg_id);
#endif
}

static void offloadservice_task_unloaded_handling(void)
{
#if defined(OFFLOAD_DEBUG_LO)
	pr_debug("%s()\n", __func__);
#endif
}

static void offloadservice_ipicmd_send(uint8_t data_type, uint8_t ack_type,
				       unsigned short msg_id,
				       unsigned int param1, unsigned int param2,
				       char *payload)
{
	struct ipi_msg_t ipi_msg;
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
			test_buf[0] = afe_offload_block.buf
					      .pucPhysBufAddr; /* dram addr */
			test_buf[1] = afe_offload_block.buf.bufferSize;
			test_buf[2] =
				(unsigned int)afe_offload_block.hw_buffer_addr;
			test_buf[3] =
				afe_offload_block
					.hw_buffer_size; /* playback size */
			test_buf[4] = playback_dram_state;
			param1 = sizeof(unsigned int) * 5;
			break;
		}
	}
	if (data_type != AUDIO_IPI_DMA)
		payload = (char *)&test_buf;
	audio_send_ipi_msg(&ipi_msg, TASK_SCENE_PLAYBACK_MP3,
			   AUDIO_IPI_LAYER_KERNEL_TO_SCP, data_type, ack_type,
			   msg_id, param1, param2, payload);
}
#endif

static bool OffloadService_IPICmd_Wait(unsigned int id)
{
	int timeout = 0;

	while (afe_offload_service.ipiwait) {
		msleep(MP3_WAITCHECK_INTERVAL_MS);
		if (timeout++ >= MP3_IPIMSG_TIMEOUT) {
			pr_debug("Error: IPI MSG timeout:id_%x\n", id);
			afe_offload_service.ipiwait = false;
			return false;
		}
	}
	return true;
}

static int offloadservice_copydatatoram(void __user *buf, size_t count)
{
	size_t copy1, copy2;
	int free_space = 0;
	int transferred = 0;
	static unsigned int u4round = 1;
	unsigned int u4BufferSize = afe_offload_block.buf.bufferSize;
	unsigned int u4WriteIdx = afe_offload_block.buf.writeIdx;
	unsigned int u4ReadIdx = afe_offload_block.buf.readIdx;

	if (count % 64 != 0)
		count = USE_PERIODS_MAX;
	Auddrv_Dl3_Spinlock_lock();
	if (u4WriteIdx >= u4ReadIdx)
		free_space = (u4BufferSize - u4WriteIdx) + u4ReadIdx;
	else
		free_space = u4ReadIdx - u4WriteIdx;
	free_space = word_size_align(free_space);
	if (count < free_space) {
		if (count > (u4BufferSize - u4WriteIdx)) {
			copy1 = word_size_align(u4BufferSize - u4WriteIdx);
			copy2 = word_size_align(count - copy1);
			if (copy_from_user(
				    afe_offload_block.buf.pucVirtBufAddr +
					    u4WriteIdx,
				    buf, copy1))
				goto Error;
			if (copy2 > 0)
				if (copy_from_user(afe_offload_block.buf
							   .pucVirtBufAddr,
						   buf + copy1, copy2))
					goto Error;
			u4WriteIdx = copy2;
		} else {
			count = word_size_align(count);
			if (copy_from_user(
				    afe_offload_block.buf.pucVirtBufAddr +
					    u4WriteIdx,
				    buf, count))
				goto Error;
			u4WriteIdx += count; /* update write index */
		}
		afe_offload_block.transferred += count;
	}
	u4WriteIdx %= u4BufferSize;
	afe_offload_block.buf.bufferSize = u4BufferSize;
	afe_offload_block.buf.writeIdx = u4WriteIdx;
	afe_offload_block.buf.readIdx = u4ReadIdx;
	Auddrv_Dl3_Spinlock_unlock();
	if (u4WriteIdx >= u4ReadIdx)
		free_space = (u4BufferSize - u4WriteIdx) + u4ReadIdx;
	else
		free_space = u4ReadIdx - u4WriteIdx;
	if (count >= free_space) {
		offloadservice_setwriteblocked(true);
		afe_offload_block.write_blocked_idx = u4WriteIdx;
		afe_offload_service.needdata = false;
		u4round = 1;
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		offloadservice_ipicmd_send(
			AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
			MP3_SETWRITEBLOCK, afe_offload_block.write_blocked_idx,
			0, NULL);
#endif
#ifdef use_wake_lock
		mtk_compr_offload_int_wakelock(false);
#endif
#if defined(OFFLOAD_DEBUG_LO)
		pr_debug("%s buffer full , WIdx=%d\n", __func__,
				   u4WriteIdx);
#endif
	}
	if (afe_offload_service.needdata) {
		transferred = u4WriteIdx - afe_offload_block.write_blocked_idx;
		if (transferred < 0)
			transferred += afe_offload_block.buf.bufferSize;
		if (transferred >=
		    (128 * USE_PERIODS_MAX) *
			    u4round) { /* notify writeIDX to SCP each 1M*/
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
			offloadservice_ipicmd_send(
				AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				MP3_WRITEIDX, u4WriteIdx, 0, NULL);
#endif
			u4round++;
		}
	}
	if (afe_offload_block.state != OFFLOAD_STATE_RUNNING) {
		if ((afe_offload_block.transferred >= 8 * USE_PERIODS_MAX) ||
		    (afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
		     afe_offload_block.state == OFFLOAD_STATE_DRAIN)) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
			offloadservice_ipicmd_send(
				AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				MP3_RUN, afe_offload_block.buf.bufferSize, 0,
				NULL);
#endif
			pr_debug("%s(),MSG_DECODER_START, TRANSFERRED %lld\n",
				 __func__, afe_offload_block.transferred);
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
		}
	}
	return count;
Error:
	pr_debug("%s copy failed\n", __func__);
	return -1;
}

static int mtk_compr_offload_pointer(struct snd_compr_stream *stream,
				     struct snd_compr_tstamp *tstamp)
{
	int ret = 0;

	if (!afe_offload_service.ipiwait) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		offloadservice_ipicmd_send(AUDIO_IPI_MSG_ONLY,
					   AUDIO_IPI_MSG_BYPASS_ACK, MP3_TSTAMP,
					   0, 0, NULL);
#endif
		afe_offload_service.ipiwait = true;
	}
	if (afe_offload_block.state == OFFLOAD_STATE_INIT ||
	    afe_offload_block.state == OFFLOAD_STATE_IDLE ||
	    afe_offload_block.state == OFFLOAD_STATE_PREPARE) {
		tstamp->copied_total = 0;
		tstamp->sampling_rate = afe_offload_block.samplerate;
		tstamp->pcm_io_frames = 0;
		return 0;
	}

	if (afe_offload_block.state == OFFLOAD_STATE_RUNNING &&
	    afe_offload_service.write_blocked)
		OffloadService_IPICmd_Wait(MP3_PCMCONSUMED);

	if (!afe_offload_service.needdata) {
		tstamp->copied_total =
			afe_offload_block
				.transferred; /* make buffer available */
	} else {
		tstamp->copied_total =
			afe_offload_block
				.copied_total; /* make buffer available */
	}
	if (afe_offload_service.write_blocked ||
	    afe_offload_block.state == OFFLOAD_STATE_DRAIN) /* Dram full */
		tstamp->copied_total =
			afe_offload_block.transferred - (8 * USE_PERIODS_MAX);
	if (offload_playback_pause) {
		tstamp->copied_total =
			afe_offload_block
				.transferred; /* make buffer available */
		offload_playback_pause = false;
	}
	tstamp->sampling_rate = afe_offload_block.samplerate;
	tstamp->pcm_io_frames =
		afe_offload_block.copied_total >> 2; /* DSP return 16bit data */
#if defined(OFFLOAD_DEBUG_LO)
	pr_debug("%s() tstamp->copied_total = %u\n", __func__,
			   tstamp->copied_total);
#endif
	return ret;
}

/* TO be implemented with mixer control */
#ifdef OFFLOAD_PCM_DUMP
static void mtk_compr_offload_pcmdump(unsigned long enable)
{
	if (enable > 0) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		playback_open_dump_file();
#endif
	}
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_DMA, AUDIO_IPI_MSG_BYPASS_ACK,
				   MP3_PCMDUMP_ON, resv_dram->size, enable,
				   resv_dram->phy_addr);
#endif
	afe_offload_service.ipiwait = true;
	/* dsp dump closed */
	if (!enable) {
		OffloadService_IPICmd_Wait(MP3_PCMDUMP_OK);
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		playback_close_dump_file();
#endif
	}
}
#endif

/*
 *=======================================================================
 *-----------------------------------------------------------------------
 *||                    O F F L O A D    TRIGGER   O P E R A T I O N S
 *-----------------------------------------------------------------------
 *=======================================================================
 */
static int mtk_compr_offload_start(struct snd_compr_stream *stream)
{
	afe_offload_block.state = OFFLOAD_STATE_PREPARE;
	SetOffloadEnableFlag(true);
	afe_offload_block.drain_state = AUDIO_DRAIN_NONE;
	mtk_offload_dl3_start();
	return 0;
}

static int mtk_compr_offload_resume(struct snd_compr_stream *stream)
{
	if (!prepare_done)
		mtk_offload_dl3_prepare();
	if (!irq7_user) {
		irq_add_user(&irq7_user, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
			     afe_offload_block.samplerate,
			     afe_offload_block.period_size);
		irq7_user = true;
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3,
		      afe_offload_block.samplerate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, true);
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	if ((afe_offload_block.transferred > 8 * USE_PERIODS_MAX) ||
	    (afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
	     ((afe_offload_block.drain_state == AUDIO_DRAIN_EARLY_NOTIFY) ||
	      (afe_offload_block.state == OFFLOAD_STATE_DRAIN)))) {
		offloadservice_ipicmd_send(
			AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK, MP3_RUN,
			afe_offload_block.buf.writeIdx, 0, NULL);
		if (afe_offload_block.drain_state != AUDIO_DRAIN_EARLY_NOTIFY)
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
		pr_debug("%s\n", __func__);
	}
#endif
	SetOffloadEnableFlag(true);
	offloadservice_releasewriteblocked();

	offload_playback_pause = false;
	offload_playback_resume = true;
	return 0;
}

static int mtk_compr_offload_pause(struct snd_compr_stream *stream)
{
	if (irq7_user) {
		irq7_user = false;
		irq_remove_user(&irq7_user, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE);
	}
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL3,
		      afe_offload_block.samplerate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3, false);
	SetOffloadEnableFlag(false);
	offloadservice_releasewriteblocked();
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	if ((afe_offload_block.transferred > 8 * USE_PERIODS_MAX) ||
	    (afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
	     ((afe_offload_block.drain_state == AUDIO_DRAIN_EARLY_NOTIFY) ||
	      (afe_offload_block.state == OFFLOAD_STATE_DRAIN)))) {
		offloadservice_ipicmd_send(AUDIO_IPI_MSG_ONLY,
					   AUDIO_IPI_MSG_BYPASS_ACK, MP3_PAUSE,
					   0, 0, NULL);
		pr_debug("%s > transferred\n", __func__);
	}
#endif
	offload_playback_pause = true;
	offload_playback_resume = false;
	return 0;
}

static int mtk_compr_offload_stop(struct snd_compr_stream *stream)
{
	int ret = 0;

	afe_offload_block.state = OFFLOAD_STATE_IDLE;
	pr_debug("%s\n", __func__);
	SetOffloadEnableFlag(false);
	/* stop hw */
	mtk_offload_dl3_stop();
	/* clear vars*/
	afe_offload_block.transferred = 0;
	afe_offload_block.copied_total = 0;
	afe_offload_block.buf.readIdx = 0;
	afe_offload_block.buf.writeIdx = 0;
	afe_offload_block.drain_state = AUDIO_DRAIN_NONE;
	memset_io((void *)afe_offload_block.buf.pucVirtBufAddr, 0,
		  afe_offload_block.buf.bufferSize);
	memset(&afe_offload_service, 0, sizeof(afe_offload_service));
	offloadservice_setwriteblocked(false);
	offloadservice_releasewriteblocked();
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	offloadservice_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				   MP3_CLOSE, 0, 0, NULL);
#endif
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
	return ret;
}

/*
 * mtk_compr_offload_trigger
 *
 */
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

static int mtk_asoc_dl3offload_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_dl3offload_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct snd_compr_ops mtk_offload_compr_ops = {
	.open = mtk_compr_offload_open,
	.free = mtk_compr_offload_free,
	.set_params = mtk_compr_offload_set_params,
	.get_params = mtk_compr_offload_get_params,
	.set_metadata = mtk_compr_offload_set_metadata,
	.get_metadata = mtk_compr_offload_get_metadata,
	.trigger = mtk_compr_offload_trigger,
	.pointer = mtk_compr_offload_pointer,
	.copy = mtk_compr_offload_copy,
	.mmap = mtk_compr_offload_mmap,
	.ack = NULL,
	.get_caps = mtk_compr_offload_get_caps,
	.get_codec_caps = mtk_compr_offload_get_codec_caps,
};

static struct snd_soc_platform_driver mtk_dl3offload_soc_platform = {
	.compr_ops = &mtk_offload_compr_ops,
	.pcm_new = mtk_asoc_dl3offload_new,
	.probe = mtk_afe_dl3offload_probe,
};

static int mtk_dl3offload_probe(struct platform_device *dev)
{
	if (dev->dev.of_node)
		dev_set_name(&dev->dev, "%s", MT_SOC_PLAYBACK_OFFLOAD);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&dev->dev));

	offload_dev = &dev->dev;

	return snd_soc_register_platform(&dev->dev,
					 &mtk_dl3offload_soc_platform);
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_offload_common_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_offload_common",
	},
	{} };
#endif

static struct platform_driver mtk_offloadplayback_driver = {
	.driver = {

			.name = MT_SOC_PLAYBACK_OFFLOAD,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_offload_common_of_ids,
#endif
		},
	.probe = mtk_dl3offload_probe,
	.remove = mtk_dl3offload_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkdl3offload_dev;
#endif
static int __init mtk_offloadplayback_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkdl3offload_dev =
		platform_device_alloc(MT_SOC_PLAYBACK_OFFLOAD, -1);
	if (!soc_mtkdl3offload_dev)
		return -ENOMEM;
	ret = platform_device_add(soc_mtkdl3offload_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkdl3offload_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_offloadplayback_driver);

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	audio_ipi_client_playback_init();
#endif
#ifdef use_wake_lock
	aud_wake_lock_init(&Offload_suspend_lock, "Offload wakelock");
#endif

	return ret;
}
module_init(mtk_offloadplayback_soc_platform_init);

static void __exit mtk_offloadplayback_soc_platform_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&mtk_offloadplayback_driver);
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	audio_ipi_client_playback_deinit();
#endif

#ifdef use_wake_lock
	aud_wake_lock_destroy(&Offload_suspend_lock);
#endif
}
module_exit(mtk_offloadplayback_soc_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Offload Driver");
