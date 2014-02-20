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
#ifndef __Q6LSM_H__
#define __Q6LSM_H__

#include <linux/list.h>
#include <linux/msm_ion.h>
#include <sound/apr_audio-v2.h>
#include <sound/lsm_params.h>
#include <mach/qdsp6v2/apr.h>

typedef void (*lsm_app_cb)(uint32_t opcode, uint32_t token,
		       uint32_t *payload, void *priv);

struct lsm_sound_model {
	dma_addr_t      phys;
	void		*data;
	uint32_t	size; /* size of buffer */
	uint32_t	actual_size; /* actual number of bytes read by DSP */
	struct ion_handle *handle;
	struct ion_client *client;
	uint32_t	mem_map_handle;
};

struct lsm_client {
	int		session;
	lsm_app_cb	cb;
	atomic_t	cmd_state;
	void		*priv;
	struct apr_svc  *apr;
	struct apr_svc  *mmap_apr;
	struct mutex    cmd_lock;
	struct lsm_sound_model sound_model;
	wait_queue_head_t cmd_wait;
	uint16_t	mode;
	uint16_t	connect_to_port;
	uint16_t	user_sensitivity;
	uint16_t	kw_sensitivity;
	bool		started;
	dma_addr_t	lsm_cal_phy_addr;
	uint32_t	lsm_cal_size;
};

struct lsm_stream_cmd_open_tx {
	struct apr_hdr  hdr;
	uint16_t	app_id;
	uint16_t	reserved;
	uint32_t	sampling_rate;
} __packed;

struct lsm_param_payload_common {
	uint32_t	module_id;
	uint32_t	param_id;
	uint16_t	param_size;
	uint16_t	reserved;
} __packed;

struct lsm_param_op_mode {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint16_t	mode;
	uint16_t	reserved;
} __packed;

struct lsm_param_connect_to_port {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	/* AFE port id that receives voice wake up data */
	uint16_t	port_id;
	uint16_t	reserved;
} __packed;

struct lsm_param_kw_detect_sensitivity {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	/* scale factor to change the keyword confidence thresholds */
	uint16_t	keyword_sensitivity;
	uint16_t	reserved;
} __packed;

struct lsm_param_user_detect_sensitivity {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	/* scale factor to change the user confidence thresholds */
	uint16_t	user_sensitivity;
	uint16_t	reserved;
} __packed;

struct lsm_params_payload {
	struct lsm_param_connect_to_port connect_to_port;
	struct lsm_param_op_mode	op_mode;
	struct lsm_param_kw_detect_sensitivity kwds;
	struct lsm_param_user_detect_sensitivity uds;
} __packed;

struct lsm_cmd_set_params {
	struct apr_hdr  hdr;
	uint32_t	data_payload_size;
	uint32_t	data_payload_addr_lsw;
	uint32_t	data_payload_addr_msw;
	uint32_t	mem_map_handle;
	struct lsm_params_payload payload;
} __packed;

struct lsm_cmd_reg_snd_model {
	struct apr_hdr	hdr;
	uint32_t	model_size;
	uint32_t	model_addr_lsw;
	uint32_t	model_addr_msw;
	uint32_t	mem_map_handle;
} __packed;

struct lsm_client *q6lsm_client_alloc(lsm_app_cb cb, void *priv);
void q6lsm_client_free(struct lsm_client *client);
int q6lsm_open(struct lsm_client *client);
int q6lsm_start(struct lsm_client *client, bool wait);
int q6lsm_stop(struct lsm_client *client, bool wait);
int q6lsm_snd_model_buf_alloc(struct lsm_client *client, size_t len);
int q6lsm_snd_model_buf_free(struct lsm_client *client);
int q6lsm_close(struct lsm_client *client);
int q6lsm_register_sound_model(struct lsm_client *client,
			       enum lsm_detection_mode mode, u16 minkeyword,
			       u16 minuser, bool detectfailure);
int q6lsm_deregister_sound_model(struct lsm_client *client);

#endif /* __Q6LSM_H__ */
