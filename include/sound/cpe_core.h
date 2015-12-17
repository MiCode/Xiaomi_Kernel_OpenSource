/*
 * Copyright (c) 2013-2015, Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>
#include <sound/lsm_params.h>

enum {
	CMD_INIT_STATE = 0,
	CMD_SENT,
	CMD_RESP_RCVD,
};

enum wcd_cpe_event {
	WCD_CPE_PRE_ENABLE = 1,
	WCD_CPE_POST_ENABLE,
	WCD_CPE_PRE_DISABLE,
	WCD_CPE_POST_DISABLE,
};

struct wcd_cpe_afe_port_cfg {
	u8 port_id;
	u16 bit_width;
	u16 num_channels;
	u32 sample_rate;
};

struct lsm_out_fmt_cfg {
	u8 format;
	u8 pack_mode;
	u8 data_path_events;
	u8 transfer_mode;
};

struct cpe_lsm_session {
	/* sound model related */
	void *snd_model_data;
	u8 *conf_levels;
	void *cmi_reg_handle;

	/* Clients private data */
	void *priv_d;

	void (*event_cb) (void *priv_data,
			  u8 detect_status,
			  u8 size, u8 *payload);

	struct completion cmd_comp;
	struct wcd_cpe_afe_port_cfg afe_port_cfg;
	struct wcd_cpe_afe_port_cfg afe_out_port_cfg;
	struct mutex lsm_lock;

	u32 snd_model_size;
	u32 lsm_mem_handle;
	u16 cmd_err_code;
	u8 id;
	u8 num_confidence_levels;
	u16 afe_out_port_id;
	struct task_struct *lsm_lab_thread;
	bool started;

	u32 lab_enable;
	struct lsm_out_fmt_cfg out_fmt_cfg;

	bool is_topology_used;
};

struct wcd_cpe_afe_ops {
	int (*afe_set_params) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg,
			       bool afe_mad_ctl);

	int (*afe_port_start) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_stop) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_suspend) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_resume) (void *core_handle,
			       struct wcd_cpe_afe_port_cfg *cfg);

	int (*afe_port_cmd_cfg)(void *core_handle,
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

	int (*lsm_get_afe_out_port_id)(void *core_handle,
			       struct cpe_lsm_session *session);

	int (*lsm_start) (void *core_handle,
			  struct cpe_lsm_session *);

	int (*lsm_stop) (void *core_handle,
			 struct cpe_lsm_session *);

	int (*lsm_lab_control)(void *core_handle,
			       struct cpe_lsm_session *session,
			       bool enable);

	int (*lab_ch_setup)(void *core_handle,
				   struct cpe_lsm_session *session,
				   enum wcd_cpe_event event);

	int (*lsm_set_data) (void *core_handle,
			struct cpe_lsm_session *session,
			enum lsm_detection_mode detect_mode,
			bool detect_failure);
	int (*lsm_set_fmt_cfg)(void *core_handle,
			struct cpe_lsm_session *session);

	int (*lsm_set_port)(void *core_handle,
			struct cpe_lsm_session *session);
	int (*lsm_set_one_param)(void *core_handle,
			struct cpe_lsm_session *session,
			struct lsm_params_info *p_info,
			void *data, enum LSM_PARAM_TYPE param_type);
	void (*lsm_get_snd_model_offset)
		(void *core_handle, struct cpe_lsm_session *,
		 size_t *offset);
};

int wcd_cpe_get_lsm_ops(struct wcd_cpe_lsm_ops *);
int wcd_cpe_get_afe_ops(struct wcd_cpe_afe_ops *);
void *wcd_cpe_get_core_handle(struct snd_soc_codec *);
#endif
