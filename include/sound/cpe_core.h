/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
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

#ifndef __CPE_CORE_H__
#define __CPE_CORE_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/msm_ion.h>
#include <linux/dma-mapping.h>
#include <sound/lsm_params.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>

enum {
	CMD_INIT_STATE = 0,
	CMD_SENT,
	CMD_RESP_RCVD,
};

struct wcd_cpe_afe_port_cfg {
	u8 port_id;
	u16 bit_width;
	u16 num_channels;
	u32 sample_rate;
};

enum wcd_cpe_lab_thread {
	MSM_LSM_LAB_THREAD_STOP,
	MSM_LSM_LAB_THREAD_RUNNING,
	MSM_LSM_LAB_THREAD_ERROR,
};

struct wcd_cpe_data_pcm_buf {
	u8 *mem;
	phys_addr_t phys;
};

struct wcd_cpe_lab_hw_params {
	u16 sample_rate;
	u16 sample_size;
	u32 buf_sz;
	u32 period_count;
};

struct wcd_cpe_lsm_lab {
	u32 lab_enable;
	void *slim_handle;
	void *core_handle;
	atomic_t in_count;
	atomic_t abort_read;
	u32 dma_write;
	u32 buf_idx;
	u32 pcm_size;
	enum wcd_cpe_lab_thread thread_status;
	struct cpe_lsm_session *lsm_s;
	struct snd_pcm_substream *substream;
	struct wcd_cpe_lab_hw_params hw_params;
	struct wcd_cpe_data_pcm_buf *pcm_buf;
	wait_queue_head_t period_wait;
	struct completion thread_complete;
};

struct cpe_lsm_session {
	/* sound model related */
	void *snd_model_data;
	u8 *conf_levels;
	void *cmi_reg_handle;
	void *priv_d;

	void (*event_cb) (void *priv_data,
			  u8 detect_status,
			  u8 size, u8 *payload);

	struct completion cmd_comp;
	struct wcd_cpe_afe_port_cfg afe_port_cfg;
	struct mutex lsm_lock;

	u32 snd_model_size;
	u32 lsm_mem_handle;
	u16 cmd_err_code;
	u8 id;
	u8 num_confidence_levels;
	struct task_struct *lsm_lab_thread;
	struct wcd_cpe_lsm_lab lab;
	bool started;
};

struct wcd_cpe_afe_ops {
	int (*afe_set_params) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_start) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_stop) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_suspend) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_resume) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);
};

struct wcd_cpe_lsm_ops {

	struct cpe_lsm_session *(*lsm_alloc_session)
			(void *core_handle, void *lsm_priv_d,
			 void (*event_cb) (void *priv_data,
					   u8 detect_status,
					   u8 size, u8 *payload));

	int (*lsm_dealloc_session)
		(void *core_handle, struct cpe_lsm_session *);

	int (*lsm_open_tx) (void *core_handle,
			    struct cpe_lsm_session *, u16, u16);

	int (*lsm_close_tx) (void *core_handle,
			     struct cpe_lsm_session *);

	int (*lsm_shmem_alloc) (void *core_handle,
				struct cpe_lsm_session *, u32 size);

	int (*lsm_shmem_dealloc) (void *core_handle,
				  struct cpe_lsm_session *);

	int (*lsm_register_snd_model) (void *core_handle,
				       struct cpe_lsm_session *,
				       enum lsm_detection_mode, bool);

	int (*lsm_deregister_snd_model) (void *core_handle,
					 struct cpe_lsm_session *);

	int (*lsm_start) (void *core_handle,
			  struct cpe_lsm_session *);

	int (*lsm_stop) (void *core_handle,
			 struct cpe_lsm_session *);

	int (*lsm_lab_control)(void *core_handle,
			       struct cpe_lsm_session *session,
			       u32 bufsz, u32 bufcnt,
			       bool enable);

	int (*lsm_lab_stop)(void *core_handle, struct cpe_lsm_session *session);

	int (*lsm_lab_data_channel_open)(void *core_handle,
				       struct cpe_lsm_session *session);
	int (*lsm_lab_data_channel_read_status)(void *core_handle,
					struct cpe_lsm_session *session,
					phys_addr_t phys, u32 *len);

	int (*lsm_lab_data_channel_read)(void *core_handle,
				struct cpe_lsm_session *session,
				phys_addr_t phys, u8 *mem,
				u32 read_len);

	int (*lsm_set_data) (void *core_handle,
			struct cpe_lsm_session *session,
			enum lsm_detection_mode detect_mode,
			bool detect_failure);
};

int wcd_cpe_get_lsm_ops(struct wcd_cpe_lsm_ops *);
int wcd_cpe_get_afe_ops(struct wcd_cpe_afe_ops *);
void *wcd_cpe_get_core_handle(struct snd_soc_codec *);
#endif
