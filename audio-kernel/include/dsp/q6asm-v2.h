/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __Q6_ASM_V2_H__
#define __Q6_ASM_V2_H__

#include <ipc/apr.h>
#include <dsp/rtac.h>
#include <dsp/apr_audio-v2.h>
#include <linux/list.h>
#include <linux/msm_ion.h>

#define IN                      0x000
#define OUT                     0x001
#define CH_MODE_MONO            0x001
#define CH_MODE_STEREO          0x002

#define FORMAT_LINEAR_PCM   0x0000
#define FORMAT_DTMF         0x0001
#define FORMAT_ADPCM	    0x0002
#define FORMAT_YADPCM       0x0003
#define FORMAT_MP3          0x0004
#define FORMAT_MPEG4_AAC    0x0005
#define FORMAT_AMRNB	    0x0006
#define FORMAT_AMRWB	    0x0007
#define FORMAT_V13K	    0x0008
#define FORMAT_EVRC	    0x0009
#define FORMAT_EVRCB	    0x000a
#define FORMAT_EVRCWB	    0x000b
#define FORMAT_MIDI	    0x000c
#define FORMAT_SBC	    0x000d
#define FORMAT_WMA_V10PRO   0x000e
#define FORMAT_WMA_V9	    0x000f
#define FORMAT_AMR_WB_PLUS  0x0010
#define FORMAT_MPEG4_MULTI_AAC 0x0011
#define FORMAT_MULTI_CHANNEL_LINEAR_PCM 0x0012
#define FORMAT_AC3          0x0013
#define FORMAT_EAC3         0x0014
#define FORMAT_MP2          0x0015
#define FORMAT_FLAC         0x0016
#define FORMAT_ALAC         0x0017
#define FORMAT_VORBIS       0x0018
#define FORMAT_APE          0x0019
#define FORMAT_G711_ALAW_FS 0x001a
#define FORMAT_G711_MLAW_FS 0x001b
#define FORMAT_DTS          0x001c
#define FORMAT_DSD          0x001d
#define FORMAT_APTX         0x001e
#define FORMAT_GEN_COMPR    0x001f
#define FORMAT_TRUEHD       0x0020
#define FORMAT_IEC61937     0x0021

#define ENCDEC_SBCBITRATE   0x0001
#define ENCDEC_IMMEDIATE_DECODE 0x0002
#define ENCDEC_CFG_BLK          0x0003

#define CMD_PAUSE          0x0001
#define CMD_FLUSH          0x0002
#define CMD_EOS            0x0003
#define CMD_CLOSE          0x0004
#define CMD_OUT_FLUSH      0x0005
#define CMD_SUSPEND        0x0006

/* bit 0:1 represents priority of stream */
#define STREAM_PRIORITY_NORMAL	0x0000
#define STREAM_PRIORITY_LOW	0x0001
#define STREAM_PRIORITY_HIGH	0x0002

/* bit 4 represents META enable of encoded data buffer */
#define BUFFER_META_ENABLE	0x0010

/* bit 5 represents timestamp */
/* bit 5 - 0 -- ASM_DATA_EVENT_READ_DONE will have relative time-stamp*/
/* bit 5 - 1 -- ASM_DATA_EVENT_READ_DONE will have absolute time-stamp*/
#define ABSOLUTE_TIMESTAMP_ENABLE  0x0020

/* Enable Sample_Rate/Channel_Mode notification event from Decoder */
#define SR_CM_NOTIFY_ENABLE	0x0004

#define TUN_WRITE_IO_MODE 0x0008 /* tunnel read write mode */
#define TUN_READ_IO_MODE  0x0004 /* tunnel read write mode */
#define SYNC_IO_MODE	0x0001
#define ASYNC_IO_MODE	0x0002
#define COMPRESSED_IO	0x0040
#define COMPRESSED_STREAM_IO	0x0080
#define NT_MODE        0x0400

#define NO_TIMESTAMP    0xFF00
#define SET_TIMESTAMP   0x0000

#define SOFT_PAUSE_ENABLE	1
#define SOFT_PAUSE_DISABLE	0

#define ASM_ACTIVE_STREAMS_ALLOWED	0x8
/* Control session is used for mapping calibration memory */
#define ASM_CONTROL_SESSION	(ASM_ACTIVE_STREAMS_ALLOWED + 1)

#define ASM_SHIFT_GAPLESS_MODE_FLAG	31
#define ASM_SHIFT_LAST_BUFFER_FLAG	30

#define ASM_LITTLE_ENDIAN 0
#define ASM_BIG_ENDIAN 1

/* PCM_MEDIA_FORMAT_Version */
enum {
	PCM_MEDIA_FORMAT_V2 = 0,
	PCM_MEDIA_FORMAT_V3,
	PCM_MEDIA_FORMAT_V4,
};

/* PCM format modes in DSP */
enum {
	DEFAULT_QF = 0,
	Q15 = 15,
	Q23 = 23,
	Q31 = 31,
};

/* payload structure bytes */
#define READDONE_IDX_STATUS 0
#define READDONE_IDX_BUFADD_LSW 1
#define READDONE_IDX_BUFADD_MSW 2
#define READDONE_IDX_MEMMAP_HDL 3
#define READDONE_IDX_SIZE 4
#define READDONE_IDX_OFFSET 5
#define READDONE_IDX_LSW_TS 6
#define READDONE_IDX_MSW_TS 7
#define READDONE_IDX_FLAGS 8
#define READDONE_IDX_NUMFRAMES 9
#define READDONE_IDX_SEQ_ID 10

#define SOFT_PAUSE_PERIOD       30   /* ramp up/down for 30ms    */
#define SOFT_PAUSE_STEP         0 /* Step value 0ms or 0us */
enum {
	SOFT_PAUSE_CURVE_LINEAR = 0,
	SOFT_PAUSE_CURVE_EXP,
	SOFT_PAUSE_CURVE_LOG,
};

#define SOFT_VOLUME_PERIOD       30   /* ramp up/down for 30ms    */
#define SOFT_VOLUME_STEP         0 /* Step value 0ms or 0us */
enum {
	SOFT_VOLUME_CURVE_LINEAR = 0,
	SOFT_VOLUME_CURVE_EXP,
	SOFT_VOLUME_CURVE_LOG,
};

#define SOFT_VOLUME_INSTANCE_1	1
#define SOFT_VOLUME_INSTANCE_2	2

typedef void (*app_cb)(uint32_t opcode, uint32_t token,
			uint32_t *payload, void *priv);

struct audio_buffer {
	dma_addr_t phys;
	void       *data;
	uint32_t   used;
	uint32_t   size;/* size of buffer */
	uint32_t   actual_size; /* actual number of bytes read by DSP */
	struct      ion_handle *handle;
	struct      ion_client *client;
};

struct audio_aio_write_param {
	phys_addr_t   paddr;
	uint32_t      len;
	uint32_t      uid;
	uint32_t      lsw_ts;
	uint32_t      msw_ts;
	uint32_t      flags;
	uint32_t      metadata_len;
	uint32_t      last_buffer;
};

struct audio_aio_read_param {
	phys_addr_t   paddr;
	uint32_t      len;
	uint32_t      uid;
	uint32_t      flags;/*meta data flags*/
};

struct audio_port_data {
	struct audio_buffer *buf;
	uint32_t	    max_buf_cnt;
	uint32_t	    dsp_buf;
	uint32_t	    cpu_buf;
	struct list_head    mem_map_handle;
	uint32_t	    tmp_hdl;
	/* read or write locks */
	struct mutex	    lock;
	spinlock_t	    dsp_lock;
};

struct shared_io_config {
	uint32_t format;
	uint16_t bits_per_sample;
	uint32_t rate;
	uint32_t channels;
	uint16_t sample_word_size;
	uint32_t bufsz;
	uint32_t bufcnt;
};

struct audio_client {
	int                    session;
	app_cb		       cb;
	atomic_t	       cmd_state;
	atomic_t	       cmd_state_pp;
	/* Relative or absolute TS */
	atomic_t	       time_flag;
	atomic_t	       nowait_cmd_cnt;
	atomic_t               mem_state;
	void		       *priv;
	uint32_t               io_mode;
	uint64_t	       time_stamp;
	struct apr_svc         *apr;
	struct apr_svc         *mmap_apr;
	struct apr_svc         *apr2;
	struct mutex	       cmd_lock;
	/* idx:1 out port, 0: in port*/
	struct audio_port_data port[2];
	wait_queue_head_t      cmd_wait;
	wait_queue_head_t      time_wait;
	wait_queue_head_t      mem_wait;
	int                    perf_mode;
	int					   stream_id;
	struct device *dev;
	int		       topology;
	int		       app_type;
	/* audio cache operations fptr*/
	int (*fptr_cache_ops)(struct audio_buffer *abuff, int cache_op);
	atomic_t               unmap_cb_success;
	atomic_t               reset;
	/* holds latest DSP pipeline delay */
	uint32_t               path_delay;
	/* shared io */
	struct audio_buffer shared_pos_buf;
	struct shared_io_config config;
};

void q6asm_audio_client_free(struct audio_client *ac);

struct audio_client *q6asm_audio_client_alloc(app_cb cb, void *priv);

struct audio_client *q6asm_get_audio_client(int session_id);

int q6asm_audio_client_buf_alloc(unsigned int dir/* 1:Out,0:In */,
				struct audio_client *ac,
				unsigned int bufsz,
				uint32_t bufcnt);
int q6asm_audio_client_buf_alloc_contiguous(unsigned int dir
				/* 1:Out,0:In */,
				struct audio_client *ac,
				unsigned int bufsz,
				unsigned int bufcnt);

int q6asm_audio_client_buf_free_contiguous(unsigned int dir,
			struct audio_client *ac);

int q6asm_open_read(struct audio_client *ac, uint32_t format
		/*, uint16_t bits_per_sample*/);

int q6asm_open_read_v2(struct audio_client *ac, uint32_t format,
			uint16_t bits_per_sample);

int q6asm_open_read_v3(struct audio_client *ac, uint32_t format,
		       uint16_t bits_per_sample);

int q6asm_open_read_v4(struct audio_client *ac, uint32_t format,
		       uint16_t bits_per_sample, bool ts_mode);

int q6asm_open_write(struct audio_client *ac, uint32_t format
		/*, uint16_t bits_per_sample*/);

int q6asm_open_write_v2(struct audio_client *ac, uint32_t format,
			uint16_t bits_per_sample);

int q6asm_open_shared_io(struct audio_client *ac,
			 struct shared_io_config *c, int dir);

int q6asm_open_write_v3(struct audio_client *ac, uint32_t format,
			uint16_t bits_per_sample);

int q6asm_open_write_v4(struct audio_client *ac, uint32_t format,
			uint16_t bits_per_sample);

int q6asm_stream_open_write_v2(struct audio_client *ac, uint32_t format,
			       uint16_t bits_per_sample, int32_t stream_id,
			       bool is_gapless_mode);

int q6asm_stream_open_write_v3(struct audio_client *ac, uint32_t format,
			       uint16_t bits_per_sample, int32_t stream_id,
			       bool is_gapless_mode);

int q6asm_stream_open_write_v4(struct audio_client *ac, uint32_t format,
			       uint16_t bits_per_sample, int32_t stream_id,
			       bool is_gapless_mode);

int q6asm_open_write_compressed(struct audio_client *ac, uint32_t format,
				uint32_t passthrough_flag);

int q6asm_open_read_write(struct audio_client *ac,
			uint32_t rd_format,
			uint32_t wr_format);

int q6asm_open_read_write_v2(struct audio_client *ac, uint32_t rd_format,
			     uint32_t wr_format, bool is_meta_data_mode,
			     uint32_t bits_per_sample, bool overwrite_topology,
			     int topology);

int q6asm_open_loopback_v2(struct audio_client *ac,
			   uint16_t bits_per_sample);

int q6asm_open_transcode_loopback(struct audio_client *ac,
			   uint16_t bits_per_sample, uint32_t source_format,
			   uint32_t sink_format);

int q6asm_write(struct audio_client *ac, uint32_t len, uint32_t msw_ts,
				uint32_t lsw_ts, uint32_t flags);
int q6asm_write_nolock(struct audio_client *ac, uint32_t len, uint32_t msw_ts,
				uint32_t lsw_ts, uint32_t flags);

int q6asm_async_write(struct audio_client *ac,
					  struct audio_aio_write_param *param);

int q6asm_async_read(struct audio_client *ac,
					  struct audio_aio_read_param *param);

int q6asm_read(struct audio_client *ac);
int q6asm_read_v2(struct audio_client *ac, uint32_t len);
int q6asm_read_nolock(struct audio_client *ac);

int q6asm_memory_map(struct audio_client *ac, phys_addr_t buf_add,
			int dir, uint32_t bufsz, uint32_t bufcnt);

int q6asm_memory_unmap(struct audio_client *ac, phys_addr_t buf_add,
							int dir);

struct audio_buffer *q6asm_shared_io_buf(struct audio_client *ac, int dir);

int q6asm_shared_io_free(struct audio_client *ac, int dir);

int q6asm_get_shared_pos(struct audio_client *ac, uint32_t *si, uint32_t *msw,
			 uint32_t *lsw);

int q6asm_map_rtac_block(struct rtac_cal_block_data *cal_block);

int q6asm_unmap_rtac_block(uint32_t *mem_map_handle);

int q6asm_send_cal(struct audio_client *ac);

int q6asm_run(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts);

int q6asm_run_nowait(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts);

int q6asm_stream_run_nowait(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts, uint32_t stream_id);

int q6asm_reg_tx_overflow(struct audio_client *ac, uint16_t enable);

int q6asm_reg_rx_underflow(struct audio_client *ac, uint16_t enable);

int q6asm_cmd(struct audio_client *ac, int cmd);

int q6asm_stream_cmd(struct audio_client *ac, int cmd, uint32_t stream_id);

int q6asm_cmd_nowait(struct audio_client *ac, int cmd);

int q6asm_stream_cmd_nowait(struct audio_client *ac, int cmd,
			    uint32_t stream_id);

void *q6asm_is_cpu_buf_avail(int dir, struct audio_client *ac,
				uint32_t *size, uint32_t *idx);

int q6asm_cpu_buf_release(int dir, struct audio_client *ac);

void *q6asm_is_cpu_buf_avail_nolock(int dir, struct audio_client *ac,
					uint32_t *size, uint32_t *idx);

int q6asm_is_dsp_buf_avail(int dir, struct audio_client *ac);

/* File format specific configurations to be added below */

int q6asm_enc_cfg_blk_aac(struct audio_client *ac,
			 uint32_t frames_per_buf,
			uint32_t sample_rate, uint32_t channels,
			 uint32_t bit_rate,
			 uint32_t mode, uint32_t format);

int q6asm_enc_cfg_blk_g711(struct audio_client *ac,
			 uint32_t frames_per_buf,
			uint32_t sample_rate);

int q6asm_enc_cfg_blk_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels);

int q6asm_enc_cfg_blk_pcm_v2(struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			uint16_t bits_per_sample,
			bool use_default_chmap, bool use_back_flavor,
			u8 *channel_map);

int q6asm_enc_cfg_blk_pcm_v3(struct audio_client *ac,
			     uint32_t rate, uint32_t channels,
			     uint16_t bits_per_sample, bool use_default_chmap,
			     bool use_back_flavor, u8 *channel_map,
			     uint16_t sample_word_size);

int q6asm_enc_cfg_blk_pcm_v4(struct audio_client *ac,
			     uint32_t rate, uint32_t channels,
			     uint16_t bits_per_sample, bool use_default_chmap,
			     bool use_back_flavor, u8 *channel_map,
			     uint16_t sample_word_size, uint16_t endianness,
			     uint16_t mode);

int q6asm_enc_cfg_blk_pcm_format_support(struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			uint16_t bits_per_sample);

int q6asm_enc_cfg_blk_pcm_format_support_v3(struct audio_client *ac,
					    uint32_t rate, uint32_t channels,
					    uint16_t bits_per_sample,
					    uint16_t sample_word_size);

int q6asm_enc_cfg_blk_pcm_format_support_v4(struct audio_client *ac,
					    uint32_t rate, uint32_t channels,
					    uint16_t bits_per_sample,
					    uint16_t sample_word_size,
					    uint16_t endianness,
					    uint16_t mode);

int q6asm_set_encdec_chan_map(struct audio_client *ac,
		uint32_t num_channels);

int q6asm_enc_cfg_blk_pcm_native(struct audio_client *ac,
			uint32_t rate, uint32_t channels);

int q6asm_enable_sbrps(struct audio_client *ac,
			uint32_t sbr_ps);

int q6asm_cfg_dual_mono_aac(struct audio_client *ac,
			uint16_t sce_left, uint16_t sce_right);

int q6asm_cfg_aac_sel_mix_coef(struct audio_client *ac, uint32_t mix_coeff);

int q6asm_enc_cfg_blk_qcelp(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t reduced_rate_level, uint16_t rate_modulation_cmd);

int q6asm_enc_cfg_blk_evrc(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t rate_modulation_cmd);

int q6asm_enc_cfg_blk_amrnb(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t band_mode, uint16_t dtx_enable);

int q6asm_enc_cfg_blk_amrwb(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t band_mode, uint16_t dtx_enable);

int q6asm_media_format_block_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels);

int q6asm_media_format_block_pcm_format_support(struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			uint16_t bits_per_sample);

int q6asm_media_format_block_pcm_format_support_v2(struct audio_client *ac,
				uint32_t rate, uint32_t channels,
				uint16_t bits_per_sample, int stream_id,
				bool use_default_chmap, char *channel_map);

int q6asm_media_format_block_pcm_format_support_v3(struct audio_client *ac,
						   uint32_t rate,
						   uint32_t channels,
						   uint16_t bits_per_sample,
						   int stream_id,
						   bool use_default_chmap,
						   char *channel_map,
						   uint16_t sample_word_size);

int q6asm_media_format_block_pcm_format_support_v4(struct audio_client *ac,
						   uint32_t rate,
						   uint32_t channels,
						   uint16_t bits_per_sample,
						   int stream_id,
						   bool use_default_chmap,
						   char *channel_map,
						   uint16_t sample_word_size,
						   uint16_t endianness,
						   uint16_t mode);

int q6asm_media_format_block_multi_ch_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			bool use_default_chmap, char *channel_map);

int q6asm_media_format_block_multi_ch_pcm_v2(
			struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			bool use_default_chmap, char *channel_map,
			uint16_t bits_per_sample);
int q6asm_media_format_block_gen_compr(
			struct audio_client *ac,
			uint32_t rate, uint32_t channels,
			bool use_default_chmap, char *channel_map,
			uint16_t bits_per_sample);

int q6asm_media_format_block_iec(
			struct audio_client *ac,
			uint32_t rate, uint32_t channels);

int q6asm_media_format_block_multi_ch_pcm_v3(struct audio_client *ac,
					     uint32_t rate, uint32_t channels,
					     bool use_default_chmap,
					     char *channel_map,
					     uint16_t bits_per_sample,
					     uint16_t sample_word_size);

int q6asm_media_format_block_multi_ch_pcm_v4(struct audio_client *ac,
					     uint32_t rate, uint32_t channels,
					     bool use_default_chmap,
					     char *channel_map,
					     uint16_t bits_per_sample,
					     uint16_t sample_word_size,
					     uint16_t endianness,
					     uint16_t mode);

int q6asm_media_format_block_aac(struct audio_client *ac,
			struct asm_aac_cfg *cfg);

int q6asm_stream_media_format_block_aac(struct audio_client *ac,
			struct asm_aac_cfg *cfg, int stream_id);

int q6asm_media_format_block_multi_aac(struct audio_client *ac,
			struct asm_aac_cfg *cfg);

int q6asm_media_format_block_wma(struct audio_client *ac,
			void *cfg, int stream_id);

int q6asm_media_format_block_wmapro(struct audio_client *ac,
			void *cfg, int stream_id);

int q6asm_media_format_block_amrwbplus(struct audio_client *ac,
			struct asm_amrwbplus_cfg *cfg);

int q6asm_stream_media_format_block_flac(struct audio_client *ac,
			struct asm_flac_cfg *cfg, int stream_id);

int q6asm_media_format_block_alac(struct audio_client *ac,
			struct asm_alac_cfg *cfg, int stream_id);

int q6asm_media_format_block_g711(struct audio_client *ac,
			struct asm_g711_dec_cfg *cfg, int stream_id);

int q6asm_stream_media_format_block_vorbis(struct audio_client *ac,
			struct asm_vorbis_cfg *cfg, int stream_id);

int q6asm_media_format_block_ape(struct audio_client *ac,
			struct asm_ape_cfg *cfg, int stream_id);

int q6asm_media_format_block_dsd(struct audio_client *ac,
			struct asm_dsd_cfg *cfg, int stream_id);

int q6asm_stream_media_format_block_aptx_dec(struct audio_client *ac,
						uint32_t sr, int stream_id);

int q6asm_ds1_set_endp_params(struct audio_client *ac,
				int param_id, int param_value);

/* Send stream based end params */
int q6asm_ds1_set_stream_endp_params(struct audio_client *ac, int param_id,
				     int param_value, int stream_id);

/* PP specific */
int q6asm_equalizer(struct audio_client *ac, void *eq);

/* Send Volume Command */
int q6asm_set_volume(struct audio_client *ac, int volume);

/* Send Volume Command */
int q6asm_set_volume_v2(struct audio_client *ac, int volume, int instance);

/* DTS Eagle Params */
int q6asm_dts_eagle_set(struct audio_client *ac, int param_id, uint32_t size,
			void *data, struct param_outband *po, int m_id);
int q6asm_dts_eagle_get(struct audio_client *ac, int param_id, uint32_t size,
			void *data, struct param_outband *po, int m_id);

/* Send aptx decoder BT address */
int q6asm_set_aptx_dec_bt_addr(struct audio_client *ac,
				struct aptx_dec_bt_addr_cfg *cfg);

/* Set SoftPause Params */
int q6asm_set_softpause(struct audio_client *ac,
			struct asm_softpause_params *param);

/* Set Softvolume Params */
int q6asm_set_softvolume(struct audio_client *ac,
			struct asm_softvolume_params *param);

/* Set Softvolume Params */
int q6asm_set_softvolume_v2(struct audio_client *ac,
			    struct asm_softvolume_params *param, int instance);

/* Send left-right channel gain */
int q6asm_set_lrgain(struct audio_client *ac, int left_gain, int right_gain);

/* Send multi channel gain */
int q6asm_set_multich_gain(struct audio_client *ac, uint32_t channels,
			   uint32_t *gains, uint8_t *ch_map, bool use_default);

/* Enable Mute/unmute flag */
int q6asm_set_mute(struct audio_client *ac, int muteflag);

int q6asm_get_session_time(struct audio_client *ac, uint64_t *tstamp);

int q6asm_get_session_time_legacy(struct audio_client *ac, uint64_t *tstamp);

int q6asm_send_audio_effects_params(struct audio_client *ac, char *params,
				    uint32_t params_length);

int q6asm_send_stream_cmd(struct audio_client *ac,
			  struct msm_adsp_event_data *data);

int q6asm_send_ion_fd(struct audio_client *ac, int fd);

int q6asm_send_rtic_event_ack(struct audio_client *ac,
			      void *param, uint32_t params_length);

/* Client can set the IO mode to either AIO/SIO mode */
int q6asm_set_io_mode(struct audio_client *ac, uint32_t mode);

/* Get Service ID for APR communication */
int q6asm_get_apr_service_id(int session_id);

/* Common format block without any payload */
int q6asm_media_format_block(struct audio_client *ac, uint32_t format);

/* Send the meta data to remove initial and trailing silence */
int q6asm_send_meta_data(struct audio_client *ac, uint32_t initial_samples,
		uint32_t trailing_samples);

/* Send the stream meta data to remove initial and trailing silence */
int q6asm_stream_send_meta_data(struct audio_client *ac, uint32_t stream_id,
		uint32_t initial_samples, uint32_t trailing_samples);

int q6asm_get_asm_topology(int session_id);
int q6asm_get_asm_app_type(int session_id);

int q6asm_send_mtmx_strtr_window(struct audio_client *ac,
		struct asm_session_mtmx_strtr_param_window_v2_t *window_param,
		uint32_t param_id);

/* Configure DSP render mode */
int q6asm_send_mtmx_strtr_render_mode(struct audio_client *ac,
		uint32_t render_mode);

/* Configure DSP clock recovery mode */
int q6asm_send_mtmx_strtr_clk_rec_mode(struct audio_client *ac,
		uint32_t clk_rec_mode);

/* Enable adjust session clock in DSP */
int q6asm_send_mtmx_strtr_enable_adjust_session_clock(struct audio_client *ac,
		bool enable);

/* Retrieve the current DSP path delay */
int q6asm_get_path_delay(struct audio_client *ac);

/* Helper functions to retrieve data from token */
uint8_t q6asm_get_buf_index_from_token(uint32_t token);
uint8_t q6asm_get_stream_id_from_token(uint32_t token);

/* Adjust session clock in DSP */
int q6asm_adjust_session_clock(struct audio_client *ac,
		uint32_t adjust_time_lsw,
		uint32_t adjust_time_msw);
#endif /* __Q6_ASM_H__ */
