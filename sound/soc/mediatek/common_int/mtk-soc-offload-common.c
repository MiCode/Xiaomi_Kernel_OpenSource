// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
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
 *****************************************************/
#include "mtk-auddrv-offloadcommon.h"
#include <linux/compat.h>

/* wake lock relate*/
#include <linux/device.h>
#include <linux/pm_wakeup.h>

#define aud_wake_lock_init(ws, name) wakeup_source_init(ws, name)
#define aud_wake_lock_destroy(ws) wakeup_source_trash(ws)
#define aud_wake_lock(ws) __pm_stay_awake(ws)
#define aud_wake_unlock(ws) __pm_relax(ws)

#if defined(CONFIG_SND_SOC_MTK_AUDIODSP)
#include "mtk-dsp-common_define.h"
#endif
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include "adsp_feature_define.h"
#endif

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#include <audio_task_manager.h>
#include <audio_ipi_client_playback.h>
#include <audio_ipi_dma.h>
#endif

#define DEBUG_VERBOSE

/**************************************************
  * Variable Definition
  **************************************************/

#define USE_PERIODS_MAX        8192
#define OFFLOAD_SIZE_BYTES         (USE_PERIODS_MAX << 9) /* 4M */
#define FILL_BUFFERING             (USE_PERIODS_MAX << 3) /* 64K */
#define RESERVE_DRAMPLAYBACKSIZE   (USE_PERIODS_MAX << 2) /* 32 K*/
#define ID AUDIO_TASK_OFFLOAD_ID

enum {
	TASK_SCENE_OFFLOAD_MP3,
	TASK_SCENE_OFFLOAD_AAC
};

typedef uint8_t task_offload_scene_t;

static task_offload_scene_t OFFLOAD_TYPE = TASK_SCENE_OFFLOAD_MP3;

static struct afe_offload_service_t afe_offload_service = {
	.write_blocked   = false,
	.enable          = false,
	.drain           = false,
	.ipiwait         = false,
	.needdata        = false,
	.ipiresult       = true,
	.decode_error    = false,
	.volume          = 0x10000,
	.scene           = TASK_SCENE_PLAYBACK_MP3,
};

static struct afe_offload_param_t afe_offload_block = {
	.state             = OFFLOAD_STATE_INIT,
	.samplerate        = 0,
	.transferred       = 0,
	.copied_total      = 0,
	.write_blocked_idx = 0,
	.wakelock          = false,
	.drain_state       = AUDIO_DRAIN_NONE,
};

static struct afe_offload_codec_t afe_offload_codec_info = {
	.codec_samplerate = 0,
	.codec_bitrate = 0,
};

static struct snd_compr_stream *offload_stream;

static struct device *offload_dev;


static bool offload_playback_pause;
static bool offload_playback_resume;
#define use_wake_lock
static unsigned long ringbuf_writebk;
static unsigned long long ringbufbridge_writebk;

#ifdef use_wake_lock
static DEFINE_SPINLOCK(offload_lock);
struct wakeup_source Offload_suspend_lock;
#endif
#ifdef CONFIG_SND_SOC_MTK_AUDIO_DSP
struct mtk_base_dsp *dsp;

#endif
/*
 * Function  Declaration
 */
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
static void offloadservice_ipicmd_received(struct ipi_msg_t *ipi_msg);
static void offloadservice_task_unloaded_handling(void);
static bool OffloadService_IPICmd_Wait(unsigned int id);
#endif
static int offloadservice_copydatatoram(void __user *buf, size_t count);
#ifdef use_wake_lock
static void mtk_compr_offload_int_wakelock(bool enable);
#endif

/*
 * Function Implementation
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
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_BYPASS_ACK,
			 OFFLOAD_VOLUME, afe_offload_service.volume,
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

static int offloadservice_set_pcmdump(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	afe_offload_service.pcmdump
		= (unsigned int)ucontrol->value.integer.value[0];

	if (afe_offload_service.pcmdump > 0) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		playback_open_dump_file();
#endif
	}
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
			 OFFLOAD_PCMDUMP_ON,
			 0, afe_offload_service.pcmdump,
			 0);
#endif
	pr_debug("%s, PCMDUMP = %d\n", __func__, afe_offload_service.pcmdump);
	afe_offload_service.ipiwait = true;
	/* dsp dump closed */
	if (afe_offload_service.pcmdump == 0) {
		OffloadService_IPICmd_Wait(OFFLOAD_PCMDUMP_OK);
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		playback_close_dump_file();
#endif
	}

	return 0;
}

static int offloadservice_get_pcmdump(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = afe_offload_service.pcmdump;
	return 0;
}

static int offloadservice_setformat(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	OFFLOAD_TYPE = (task_offload_scene_t)ucontrol->value.integer.value[0];
	pr_debug("%s OFFLOAD_TYPE = %d\n", __func__, OFFLOAD_TYPE);
	return 0;
}

static int offloadservice_getformat(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	if (afe_offload_service.decode_error == true) {
		pr_info("offloadgetformat decode_error\n");
		ucontrol->value.integer.value[0] = 1;
	}
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_dloffload_controls[] = {
	SOC_SINGLE_EXT("offload digital volume", SND_SOC_NOPM, 0, 0x1000000, 0,
	offloadservice_getvolume, offloadservice_setvolume),
	SOC_SINGLE_EXT("offload set dump", SND_SOC_NOPM, 0, 0xF, 0,
	offloadservice_get_pcmdump, offloadservice_set_pcmdump),
	SOC_SINGLE_EXT("offload set format", SND_SOC_NOPM, 0,
	TASK_SCENE_PLAYBACK_MP3, 0,
	offloadservice_getformat, offloadservice_setformat),
};


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
		afe_offload_block.transferred       = 0;
		afe_offload_block.copied_total      = 0;
		afe_offload_block.write_blocked_idx = 0;
		afe_offload_block.drain_state       = AUDIO_DRAIN_NONE;
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
	if (afe_offload_service.decode_error == true)
		ret = -1;
	return ret;
}

static int mtk_compr_offload_drain(struct snd_compr_stream *stream)
{
	struct RingBuf *ringbuf = &(dsp->dsp_mem[ID].ring_buf);
	struct ringbuf_bridge *buf_bridge =
		&(dsp->dsp_mem[ID].adsp_buf.aud_buffer.buf_bridge);

	int silence_length = 0;

	afe_offload_block.state = OFFLOAD_STATE_DRAIN;
	afe_offload_block.drain_state = AUDIO_DRAIN_EARLY_NOTIFY;

	if (ringbuf->pRead >= ringbuf->pWrite)
		silence_length = ringbuf->pRead - ringbuf->pWrite;
	else
		silence_length = ringbuf->pBufEnd - ringbuf->pWrite;
	if (silence_length > (USE_PERIODS_MAX))
		silence_length = USE_PERIODS_MAX;
	memset(ringbuf->pWrite, 0, silence_length);
	RingBuf_update_writeptr(ringbuf, silence_length);
	RingBuf_Bridge_update_writeptr(buf_bridge, silence_length);
	ringbuf_writebk = (unsigned long)ringbuf->pWrite;
	ringbufbridge_writebk = buf_bridge->pWrite;
	afe_offload_service.needdata = false;

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	pr_info("%s, OFFLOAD_DRAIN", __func__);
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK,
			 OFFLOAD_DRAIN,
			 sizeof(buf_bridge->pWrite),
			 0,
			 (void *)&buf_bridge->pWrite);
#endif
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif

	return 1;  /* make compress driver drain failed */
}



static int mtk_compr_offload_open(struct snd_compr_stream *stream)
{
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(true);
#endif

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	adsp_register_feature(OFFLOAD_FEATURE_ID);
#else
	scp_register_feature(MP3_FEATURE_ID);
#endif

	mtk_scp_ipi_send(TASK_SCENE_PLAYBACK_MP3,
			 AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK,
			 AUDIO_DSP_TASK_OPEN,
			 0,
			 0,
			 NULL);
#endif
	offload_stream = stream;
	pr_debug("%s OFFLOAD_TYPE = %d\n", __func__, OFFLOAD_TYPE);

	return 0;
}

static int mtk_afe_dloffload_component_probe(struct snd_soc_component *component)
{
	snd_soc_add_component_controls(component, Audio_snd_dloffload_controls,
				      ARRAY_SIZE(Audio_snd_dloffload_controls));
	return 0;
}

static int mtk_compr_offload_free(struct snd_compr_stream *stream)
{
	pr_debug("%s()\n", __func__);
	offloadservice_setwriteblocked(false);
	afe_offload_block.state = OFFLOAD_STATE_INIT;
	//SetOffloadEnableFlag(false);
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	adsp_deregister_feature(OFFLOAD_FEATURE_ID);
#else
	scp_deregister_feature(MP3_FEATURE_ID);
#endif

#endif
	return 0;
}

static int mtk_compr_offload_set_params(struct snd_compr_stream *stream,
					struct snd_compr_params *params)
{
	struct snd_codec codec;
	struct audio_hw_buffer *audio_hwbuf;
	void *ipi_audio_buf; /* dsp <-> audio data struct*/
	int ret = 0;

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	audio_task_register_callback(
			 TASK_SCENE_PLAYBACK_MP3,
			 offloadservice_ipicmd_received,
			 offloadservice_task_unloaded_handling);
#endif

	codec = params->codec;
	afe_offload_block.samplerate = codec.sample_rate; //check this

	//set shared Dram meme
	dsp = (struct mtk_base_dsp *)get_dsp_base();
	audio_hwbuf = &dsp->dsp_mem[ID].adsp_buf;

	/* gen pool related */
	dsp->dsp_mem[ID].gen_pool_buffer = mtk_get_adsp_dram_gen_pool(
							   ID);
	if (dsp->dsp_mem[ID].gen_pool_buffer != NULL) {
		pr_debug("gen_pool_avail = %zu poolsize = %zu\n",
			gen_pool_avail(
				dsp->dsp_mem[ID].gen_pool_buffer),
			gen_pool_size(
				dsp->dsp_mem[ID].gen_pool_buffer));

		/* allocate ring buffer wioth share memory*/
		ret = mtk_adsp_genpool_allocate_sharemem_ring(
			      &dsp->dsp_mem[ID],
			      gen_pool_avail(dsp->
			      dsp_mem[ID].gen_pool_buffer),
			      ID);

		if (ret < 0) {
			pr_warn("%s err\n", __func__);
			return -1;
		}

		pr_debug("gen_pool_avail = %zu poolsize = %zu\n",
			 gen_pool_avail(
			 dsp->dsp_mem[ID].gen_pool_buffer),
			 gen_pool_size(
			 dsp->dsp_mem[ID].gen_pool_buffer));
	}
	dump_audio_dsp_dram(&dsp->dsp_mem[ID].dsp_ring_share_buf);
	// set_audiobuffer_attribute
	//set codec info
	afe_offload_codec_info.codec_samplerate = codec.sample_rate;
	afe_offload_codec_info.codec_bitrate = codec.bit_rate;
	audio_hwbuf->aud_buffer.buffer_attr.channel = codec.ch_out;
	audio_hwbuf->aud_buffer.buffer_attr.format = codec.format;
	audio_hwbuf->aud_buffer.buffer_attr.rate = codec.reserved[2];
	//
	ret = set_audiobuffer_hw(&dsp->dsp_mem[ID].adsp_buf,
				 BUFFER_TYPE_SHARE_MEM);
	if (ret < 0)
		goto ERROR;
	ret = set_audiobuffer_memorytype(&dsp->dsp_mem[ID].adsp_buf,
					 MEMORY_AUDIO_DRAM);
	if (ret < 0)
		goto ERROR;

	/* send codec info to SCP side */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_BYPASS_ACK,
			 OFFLOAD_CODEC_INFO,
			 afe_offload_codec_info.codec_bitrate,
			 afe_offload_codec_info.codec_samplerate
			 , NULL);

	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf =
		(void *)dsp->dsp_mem[ID].msg_atod_share_buf.va_addr;
	pr_debug("%s offload ipi_audio_buf = %p\n", __func__, ipi_audio_buf);
	memcpy((void *)ipi_audio_buf,
	       (void *)&dsp->dsp_mem[ID].adsp_buf,
	       sizeof(struct audio_hw_buffer));

	dump_audio_hwbuffer(ipi_audio_buf);
	dump_rbuf_s(__func__, &dsp->dsp_mem[ID].ring_buf);

	/* send to task with hw_param information , buffer and pcm attribute */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWPARAM,
			 sizeof(dsp->dsp_mem[ID].msg_atod_share_buf.phy_addr),
			 0,
			 (char *)
			 &dsp->dsp_mem[ID].msg_atod_share_buf.phy_addr);

	pr_debug("%s AUDIO_DSP_TASK_HWPARAM Done\n", __func__);
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
			 AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK,
			 OFFLOAD_SCENE, OFFLOAD_TYPE, OFFLOAD_TYPE, NULL);
	pr_debug("%s OFFLOAD_SCENE Done\n", __func__);
	return ret;

ERROR:
	pr_err("%s err\n", __func__);
	return -1;
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
	caps->num_codecs        = 2;
	caps->codecs[0]         = SND_AUDIOCODEC_PCM;
	caps->codecs[1]         = SND_AUDIOCODEC_MP3;
	caps->min_fragment_size = 8192;
	caps->max_fragment_size = 0x7FFFFFFF;
	caps->min_fragments     = 2;
	caps->max_fragments     = 1875;
	return 0;
}

static int mtk_compr_offload_get_codec_caps(struct snd_compr_stream *stream,
					    struct snd_compr_codec_caps *codec)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static int mtk_compr_offload_set_metadata(struct snd_compr_stream *stream,
					  struct snd_compr_metadata *metadata)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static int mtk_compr_offload_get_metadata(struct snd_compr_stream *stream,
					  struct snd_compr_metadata *metadata)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static int mtk_compr_offload_mmap(struct snd_compr_stream *stream,
				  struct vm_area_struct *vma)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static void mtk_dsp_mp3_dl_handler(struct mtk_base_dsp *dsp,
				   struct ipi_msg_t *ipi_msg, int id)
{
	/* get dsp_mem */
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	struct RingBuf *ringbuf = &(dsp->dsp_mem[ID].ring_buf);
	struct ringbuf_bridge *buf_bridge =
		&(dsp->dsp_mem[ID].adsp_buf.aud_buffer.buf_bridge);
    //unsigned long ringbuf_writebk = (unsigned long)ringbuf->pWrite;


#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp->dsp_mem[id].ring_buf);
	pr_debug("%s msg_id = %u param1 = %u param2 = %u", __func__,
		 ipi_msg->msg_id, ipi_msg->param1, ipi_msg->param2);
#endif
	if (ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
		memcpy((void *)&dsp_mem->adsp_buf,
		       (void *)dsp_mem->msg_dtoa_share_buf.vir_addr,
		       sizeof(struct audio_hw_buffer));
		ringbuf->pWrite = (char *)ringbuf_writebk;
		buf_bridge->pWrite = ringbufbridge_writebk;
		sync_ringbuf_readidx(
			&dsp_mem->ring_buf,
			&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	}
#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp->dsp_mem[id].ring_buf);
#endif
}

static void offloadservice_ipicmd_received(struct ipi_msg_t *ipi_msg)
{
	struct mtk_base_dsp *dsp =
		(struct mtk_base_dsp *)get_ipi_recv_private();
	int id = 0;

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	if (dsp == NULL) {
		pr_warn("%s dsp == NULL\n", __func__);
		return;
	}

	id = get_dspdaiid_by_dspscene(ipi_msg->task_scene);

	if (id < 0)
		return;

	switch (ipi_msg->msg_id) {
	case AUDIO_DSP_TASK_IRQDL:
		mtk_dsp_mp3_dl_handler(dsp, ipi_msg, id);
		offloadservice_setwriteblocked(false);
		offloadservice_releasewriteblocked();
		afe_offload_service.needdata = true;
		break;
	case OFFLOAD_PCMCONSUMED:
		afe_offload_block.copied_total = ipi_msg->param1;
		afe_offload_service.ipiwait = false;
		afe_offload_service.ipiresult = true;
		break;
	case OFFLOAD_DRAINDONE:
		pr_info("%s mtk_compr_offload_draindone\n", __func__);
		afe_offload_block.drain_state = AUDIO_DRAIN_ALL;
		mtk_compr_offload_draindone();
		afe_offload_service.ipiwait = false;
		afe_offload_service.ipiresult = true;
		break;
	case OFFLOAD_PCMDUMP_OK:
		afe_offload_service.ipiwait = false;
		playback_dump_message(ipi_msg);
		break;

	case OFFLOAD_DECODE_ERROR:
		afe_offload_service.decode_error = true;
		pr_info("%s decode_error\n", __func__);
		break;
	default:
		break;
	}
	pr_debug("%s msg_id :  %d\n", __func__, ipi_msg->msg_id);
}


static void offloadservice_task_unloaded_handling(void)
{
	pr_debug("%s()\n", __func__);
}

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
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	int copy_size, availsize, ret = 0;
	static unsigned int u4round = 1;
	int transferred = 0;
	struct RingBuf *ringbuf = &(dsp->dsp_mem[ID].ring_buf);
	struct ringbuf_bridge *buf_bridge =
		&(dsp->dsp_mem[ID].adsp_buf.aud_buffer.buf_bridge);

	copy_size = count;
	availsize = RingBuf_getFreeSpace(ringbuf);
#ifdef DEBUG_VERBOSE
	pr_debug(
		"%s copy_size = %d availsize = %d\n",
		__func__, copy_size,
		RingBuf_getFreeSpace(ringbuf));

	dump_rbuf_s(__func__, &dsp->dsp_mem[ID].ring_buf);
#endif

	if (availsize >= copy_size) {
		RingBuf_copyFromUserLinear(ringbuf, buf, copy_size);
		RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);
		afe_offload_block.transferred += count;
	} else {
		//Liang: checked below, should not happened
		pr_debug("%s fail copy_size = %d availsize = %d\n", __func__,
			 copy_size, RingBuf_getFreeSpace(ringbuf));
		goto Error;
	}
	//check for next time writable
	if (count >= RingBuf_getFreeSpace(ringbuf)) {
		offloadservice_setwriteblocked(true);
		afe_offload_block.write_blocked_idx =
			buf_bridge->pWrite;
		afe_offload_service.needdata = false;
		u4round = 1;
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
				AUDIO_IPI_PAYLOAD,
				AUDIO_IPI_MSG_BYPASS_ACK,
				OFFLOAD_SETWRITEBLOCK,
				sizeof(afe_offload_block.write_blocked_idx),
				0,
				(void *)&afe_offload_block.write_blocked_idx);
#endif
#ifdef use_wake_lock
		mtk_compr_offload_int_wakelock(false);
#endif
		pr_debug("%s buffer full , WIdx=%lld\n",
			__func__, buf_bridge->pWrite);

	}

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp->dsp_mem[ID].ring_buf);
	Ringbuf_Check(&dsp->dsp_mem[ID].ring_buf);
	Ringbuf_Bridge_Check(
		&dsp->dsp_mem[ID].adsp_buf.aud_buffer.buf_bridge);
#endif

	if (afe_offload_service.needdata) {
		transferred = RingBuf_getDataCount(ringbuf);
		if (transferred >=
		    (32 * USE_PERIODS_MAX) * u4round) {
			/* notify writeIDX to SCP each 256K*/
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
			mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
				AUDIO_IPI_PAYLOAD,
				AUDIO_IPI_MSG_BYPASS_ACK,
				OFFLOAD_WRITEIDX,
				sizeof(buf_bridge->pWrite),
				0,
				(void *)&buf_bridge->pWrite);
#endif
			u4round++;
		}
	}
	if ((afe_offload_block.state != OFFLOAD_STATE_RUNNING) &&
		((afe_offload_block.transferred >= 8 * USE_PERIODS_MAX) ||
			(afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
			afe_offload_block.state == OFFLOAD_STATE_DRAIN))) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		/* send audio_hw_buffer to SCP side, get writeIndx*/
		ipi_audio_buf = (void *)dsp->
		dsp_mem[ID].msg_atod_share_buf.va_addr;
		memcpy((void *)ipi_audio_buf,
			(void *)&dsp->dsp_mem[ID].adsp_buf,
			sizeof(struct audio_hw_buffer));
		ret = mtk_scp_ipi_send(
				get_dspscene_by_dspdaiid(ID),
				AUDIO_IPI_PAYLOAD,
				AUDIO_IPI_MSG_BYPASS_ACK,
				AUDIO_DSP_TASK_DLCOPY,
				sizeof(unsigned int),
				(unsigned int)
				dsp->dsp_mem[ID].msg_atod_share_buf.phy_addr,
				(char *)
				&dsp->dsp_mem[ID].msg_atod_share_buf.phy_addr);
#ifdef DEBUG_VERBOSE
		pr_debug("%s copy_size = %d availsize = %d\n",
				__func__, copy_size,
				RingBuf_getFreeSpace(ringbuf));
#endif
#endif
			pr_debug("%s(),MSG_DECODER_START, TRANSFERRED %lld\n",
				 __func__,
				 afe_offload_block.transferred);
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
			u4round = 1;
	}
	ringbuf_writebk = (unsigned long)ringbuf->pWrite;
	ringbufbridge_writebk = buf_bridge->pWrite;
	return count;
Error:
	pr_warn("%s copy failed\n", __func__);
	return -1;
}

static int mtk_compr_offload_pointer(struct snd_compr_stream *stream,
				     struct snd_compr_tstamp *tstamp)
{
	int ret = 0;

	if (!afe_offload_service.ipiwait) {
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
				 AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
				 OFFLOAD_TSTAMP, 0, 0, NULL);
#endif
		afe_offload_service.ipiwait = true;
	}
	if (afe_offload_block.state == OFFLOAD_STATE_INIT ||
	    afe_offload_block.state == OFFLOAD_STATE_IDLE ||
	    afe_offload_block.state == OFFLOAD_STATE_PREPARE) {
		tstamp->copied_total  = 0;
		tstamp->sampling_rate = afe_offload_block.samplerate;
		tstamp->pcm_io_frames = 0;
		return 0;
	}

	if (afe_offload_block.state == OFFLOAD_STATE_RUNNING &&
	    afe_offload_service.write_blocked)
		OffloadService_IPICmd_Wait(OFFLOAD_PCMCONSUMED);

	if (!afe_offload_service.needdata) {
		tstamp->copied_total  =
			afe_offload_block.transferred;
	} else {
		tstamp->copied_total  =
			afe_offload_block.copied_total;
	}
	if (afe_offload_service.write_blocked ||
	    afe_offload_block.state == OFFLOAD_STATE_DRAIN)  /* Dram full */
		tstamp->copied_total =
			afe_offload_block.transferred - (8 * USE_PERIODS_MAX);
	if (offload_playback_pause) {
		tstamp->copied_total =
			afe_offload_block.transferred;
		offload_playback_pause = false;
	}
	tstamp->sampling_rate = afe_offload_block.samplerate;
	tstamp->pcm_io_frames = afe_offload_block.copied_total >>
				2;  /* DSP return 16bit data */
	tstamp->pcm_io_frames = tstamp->pcm_io_frames&0Xffffff80;
	return ret;
}


/*
  *=======================================================================
  *-----------------------------------------------------------------------
  *||         O F F L O A D    TRIGGER   O P E R A T I O N S
  *-----------------------------------------------------------------------
  *=======================================================================
  */
static int mtk_compr_offload_start(struct snd_compr_stream *stream)
{
	afe_offload_block.state = OFFLOAD_STATE_PREPARE;
	//SetOffloadEnableFlag(true);
	afe_offload_block.drain_state = AUDIO_DRAIN_NONE;
	return 0;
}

static int mtk_compr_offload_resume(struct snd_compr_stream *stream)
{
	int ret = 0;

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	if ((afe_offload_block.transferred > 8 * USE_PERIODS_MAX) ||
	    (afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
	     ((afe_offload_block.drain_state == AUDIO_DRAIN_EARLY_NOTIFY) ||
	      (afe_offload_block.state == OFFLOAD_STATE_DRAIN)))) {
		ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
				       AUDIO_IPI_MSG_ONLY,
				       AUDIO_IPI_MSG_NEED_ACK,
				       OFFLOAD_RESUME,
				       1, 0, NULL);

		if (afe_offload_block.drain_state != AUDIO_DRAIN_EARLY_NOTIFY)
			afe_offload_block.state = OFFLOAD_STATE_RUNNING;
	}
#endif
	//SetOffloadEnableFlag(true);
	offloadservice_releasewriteblocked();

	offload_playback_pause = false;
	offload_playback_resume = true;
	return 0;
}

static int mtk_compr_offload_pause(struct snd_compr_stream *stream)
{
	int ret = 0;

	offloadservice_releasewriteblocked();
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	if ((afe_offload_block.transferred > 8 * USE_PERIODS_MAX) ||
	    (afe_offload_block.transferred < 8 * USE_PERIODS_MAX &&
	     ((afe_offload_block.drain_state == AUDIO_DRAIN_EARLY_NOTIFY) ||
	      (afe_offload_block.state == OFFLOAD_STATE_DRAIN)))) {
		ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
					AUDIO_IPI_MSG_ONLY,
					AUDIO_IPI_MSG_NEED_ACK,
					OFFLOAD_PAUSE,
					1, 0, NULL);
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
	//SetOffloadEnableFlag(false);
	/* stop hw */
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
		mtk_scp_ipi_send(get_dspscene_by_dspdaiid(ID),
						 AUDIO_IPI_MSG_ONLY,
						 AUDIO_IPI_MSG_NEED_ACK,
						 AUDIO_DSP_TASK_STOP, 1,
						 0, NULL);
#endif
	afe_offload_block.transferred       = 0;
	afe_offload_block.copied_total      = 0;
	afe_offload_block.write_blocked_idx = 0;
	afe_offload_block.drain_state       = AUDIO_DRAIN_NONE;
	memset(&afe_offload_service, 0, sizeof(afe_offload_service));
	offloadservice_setwriteblocked(false);
	offloadservice_releasewriteblocked();
	clear_audiobuffer_hw(&dsp->dsp_mem[ID].adsp_buf);
	RingBuf_Reset(&dsp->dsp_mem[ID].ring_buf);
#ifdef use_wake_lock
	mtk_compr_offload_int_wakelock(false);
#endif
	return ret;
}

/*****************************************************************************
  * mtk_compr_offload_trigger
  ****************************************************************************/
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


static int mtk_asoc_dloffload_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_dloffload_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

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

static struct snd_soc_component_driver mtk_dloffload_soc_component = {
	.name = AFE_PCM_NAME,
	.compr_ops        = &mtk_offload_compr_ops,
	.pcm_new    = mtk_asoc_dloffload_new,
	.probe      = mtk_afe_dloffload_component_probe,
};

static int mtk_dloffload_probe(struct snd_soc_component *component)
{
	if (component->dev.of_node)
		dev_set_name(&component->dev, "%s", "mt_soc_offload_common");
	component->name = component->dev.kobj.name;

	pr_info("%s: dev name %s\n", __func__, dev_name(&component->dev));

	offload_dev = &component->dev;

	return snd_soc_register_component(&component->dev,
					  &mtk_dloffload_soc_component,
					  NULL,
					  0);
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_offload_common_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_offload_common", },
	{}
};
#endif


static struct platform_driver mtk_offloadplayback_driver = {
	.driver = {
		.name = "mt_soc_offload_common",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_offload_common_of_ids,
#endif
	},
	.probe = mtk_dloffload_probe,
	.remove = mtk_dloffload_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkdloffload_dev;
#endif
static int __init mtk_offloadplayback_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkdloffload_dev =
		platform_device_alloc("mt_soc_offload_common", -1);
	if (!soc_mtkdloffload_dev)
		return -ENOMEM;
	ret = platform_device_add(soc_mtkdloffload_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkdloffload_dev);
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
