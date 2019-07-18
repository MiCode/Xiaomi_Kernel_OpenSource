/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/


#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/compat.h>
#include <linux/acpi.h>


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
/*  includes the file structure, that is, file open read close */
#include <linux/fs.h>

/* include the character device, makes cdev avilable */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/* includes copy_user vice versa */
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>

#include <linux/pm_wakeup.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/kobject.h>

#include "../asoc/msm-pcm-routing-v2.h"
#include <dsp/q6audio-v2.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6adm-v2.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/msm-cirrus-playback.h>

#undef CONFIG_OF

#undef pr_info
#undef pr_err
#undef pr_debug
#define pr_debug(fmt, args...) printk(KERN_INFO "[CSPL] " pr_fmt(fmt), ##args)
#define pr_info(fmt, args...) printk(KERN_INFO "[CSPL] " pr_fmt(fmt), ##args)
#define pr_err(fmt, args...) printk(KERN_ERR "[CSPL] " pr_fmt(fmt), ##args)

#define CRUS_TX_CONFIG "crus_sp_tx%d.bin"
#define CRUS_RX_CONFIG "crus_sp_rx%d.bin"

#define CIRRUS_RX_GET_IODATA 0x00A1AF09
#define CIRRUS_TX_GET_IODATA 0x00A1BF09

#define CIRRUS_NONE 0
#define CIRRUS_COPP 1
#define CIRRUS_AFE 2

static struct crus_sp_ioctl_header crus_sp_hdr;

static struct crus_control_t this_ctrl = {
	.fb_port_index = 0,
	.fb_port = AFE_PORT_ID_PRIMARY_MI2S_TX,
	.ff_port = AFE_PORT_ID_PRIMARY_MI2S_RX,
	.usecase_dt_count = MAX_TUNING_CONFIGS,
	.ch_sw_duration = 30,
	.ch_sw = 0,
	.vol_atten = 0,
	.prot_en = false,
	.q6afe_rev = 2, // V3 as default
	.location = CIRRUS_NONE,
};


static void *crus_gen_afe_get_header(int length, int port, int module,
				     int param)
{
	struct afe_custom_crus_get_config_t *config = NULL;
	int size = sizeof(struct afe_custom_crus_get_config_t);
	int index = afe_get_port_index(port);
	uint16_t payload_size = sizeof(struct afe_port_param_data_v2) +
				length;

	/* Allocate memory for the message */
	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return NULL;

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config->hdr.pkt_size = size;
	config->hdr.src_svc = APR_SVC_AFE;
	config->hdr.src_domain = APR_DOMAIN_APPS;
	config->hdr.src_port = 0;
	config->hdr.dest_svc = APR_SVC_AFE;
	config->hdr.dest_domain = APR_DOMAIN_ADSP;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_GET_PARAM_V2;

	/* Set param section */
	config->param.port_id = (uint16_t) port;
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;
	config->param.module_id = (uint32_t) module;
	config->param.param_id = (uint32_t) param;
	/* max data size of the param_ID/module_ID combination */
	config->param.payload_size = payload_size;

	/* Set data section */
	config->data.module_id = (uint32_t) module;
	config->data.param_id = (uint32_t) param;
	config->data.reserved = 0; /* Must be set to 0 */
	/* actual size of the data for the module_ID/param_ID pair */
	config->data.param_size = length;

	return (void *)config;
}


static void *crus_gen_afe_set_header(int length, int port, int module,
				     int param)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	int size = sizeof(struct afe_custom_crus_set_config_t) + length;
	int index = afe_get_port_index(port);
	uint16_t payload_size = sizeof(struct afe_port_param_data_v2) +
				length;

	/* Allocate memory for the message */
	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return NULL;

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	config->hdr.pkt_size = size;
	config->hdr.src_svc = APR_SVC_AFE;
	config->hdr.src_domain = APR_DOMAIN_APPS;
	config->hdr.src_port = 0;
	config->hdr.dest_svc = APR_SVC_AFE;
	config->hdr.dest_domain = APR_DOMAIN_ADSP;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;

	/* Set param section */
	config->param.port_id = (uint16_t) port;
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;
	/* max data size of the param_ID/module_ID combination */
	config->param.payload_size = payload_size;

	/* Set data section */
	config->data.module_id = (uint32_t) module;
	config->data.param_id = (uint32_t) param;
	config->data.reserved = 0; /* Must be set to 0 */
	/* actual size of the data for the module_ID/param_ID pair */
	config->data.param_size = length;

	return (void *)config;
}

static int crus_afe_get_param(int port, int module,
				int param, void *data, int length)
{
	struct afe_custom_crus_get_config_t *config = NULL;
	int index = afe_get_port_index(port);
	int ret = 0, count = 0;

	pr_info("CRUS_SP: (get_param) module = 0x%08x, port = 0x%08x, param = 0x%08x\n",
		module, port, param);

	config = (struct afe_custom_crus_get_config_t *)
		 crus_gen_afe_get_header(length, port, module, param);
	if (config == NULL) {
		pr_err("CRUS_SP: Memory allocation failed\n");
		return -ENOMEM;
	}

	mutex_lock(&this_ctrl.param_lock);
	atomic_set(&this_ctrl.callback_wait, 0);

	this_ctrl.user_buffer = kzalloc(config->param.payload_size + 16,
				     GFP_KERNEL);

	ret = afe_apr_send_pkt_crus(config, index, 0);
	if (ret)
		pr_err("CRUS_SP: (get_param) failed with code %d\n", ret);

	/* Wait for afe callback to populate data */
	while (!atomic_read(&this_ctrl.callback_wait)) {
		usleep_range(800, 1200);
		if (count++ >= 1000) {
			pr_err("CRUS_SP: AFE callback timeout\n");
			atomic_set(&this_ctrl.callback_wait, 1);
			ret = -EINVAL;
			goto exit;
		}
	}

	/* Copy from dynamic buffer to return buffer */
	memcpy((u8 *)data, &this_ctrl.user_buffer[4], length);

exit:
	kfree(this_ctrl.user_buffer);
	mutex_unlock(&this_ctrl.param_lock);
	kfree(config);
	return ret;
}



static int crus_afe_set_param(int port, int module,
				int param, void *data, int length)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	int index = afe_get_port_index(port);
	int ret = 0;

	pr_info("CRUS_SP: (set_param) module = 0x%08x, port = 0x%08x, param = 0x%08x\n",
		module, port, param);

	config = crus_gen_afe_set_header(length, port, module, param);
	if (config == NULL) {
		pr_err("CRUS_SP: Memory allocation failed\n");
		return -ENOMEM;
	}

	memcpy((u8 *)config + sizeof(struct afe_custom_crus_set_config_t),
	       (u8 *) data, length);

	ret = afe_apr_send_pkt_crus(config, index, 1);
	if (ret)
		pr_err("CRUS_SP: (set_param) failed with code %d\n", ret);

	kfree(config);
	return ret;
}

static int crus_get_param(int port, int module, int param,
				void *data, int length)
{
	switch(this_ctrl.location) {
	case CIRRUS_AFE:
		crus_afe_get_param(port, module, param, data, length);
		break;
	case CIRRUS_COPP:
		crus_adm_get_params(port, 0, module, param, data, length, 0);
		break;
	default:
		break;
	}
	return 0;
}
static int crus_set_param(int port, int module, int param,
				void* data, int length)
{
	switch(this_ctrl.location) {
	case CIRRUS_AFE:
		crus_afe_set_param(port, module, param, data, length);
		break;
	case CIRRUS_COPP:
		crus_adm_set_params(port, 0, module, param, data, length);
		break;
	default:
		break;
	}
	return 0;
}
static int crus_afe_send_config(const char *data, int32_t length,
				int32_t port, int32_t module)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	struct crus_external_config_t *payload = NULL;
	int size = sizeof(struct crus_external_config_t);
	int chars_to_send, mem_size, sent = 0, ret = 0;
	int index = afe_get_port_index(port);
	uint32_t param = 0;

	pr_info("CRUS_SP: (send_config) module = 0x%08x, port = 0x%08x\n",
		module, port);

	/* Destination settings for message */
	if (port == this_ctrl.ff_port)
		param = CRUS_PARAM_RX_SET_EXT_CONFIG;
	else if (port == this_ctrl.fb_port)
		param = CRUS_PARAM_TX_SET_EXT_CONFIG;
	else {
		pr_err("CRUS_SP: Received invalid port parameter %d\n", port);
		return -EINVAL;
	}

	if (length > APR_CHUNK_SIZE)
		mem_size = APR_CHUNK_SIZE;
	else
		mem_size = length;

	config = crus_gen_afe_set_header(size, port, module, param);
	if (config == NULL) {
		pr_err("CRUS_SP: Memory allocation failed\n");
		return -ENOMEM;
	}

	payload = (struct crus_external_config_t *)((u8 *)config +
			sizeof(struct afe_custom_crus_set_config_t));
	payload->total_size = (uint32_t)length;
	payload->reserved = 0;
	payload->config = PAYLOAD_FOLLOWS_CONFIG;
	    /* ^ This tells the algorithm to expect array */
	    /*   immediately following the header */

	/* Send config string in chunks of APR_CHUNK_SIZE bytes */
	while (sent < length) {
		chars_to_send = length - sent;
		if (chars_to_send > APR_CHUNK_SIZE) {
			chars_to_send = APR_CHUNK_SIZE;
			payload->done = 0;
		} else {
			payload->done = 1;
		}

		/* Configure per message parameter settings */
		memcpy(payload->data, data + sent, chars_to_send);
		payload->chunk_size = chars_to_send;

		/* Send the actual message */
		ret = afe_apr_send_pkt_crus(config, index, 1);

		if (ret) {
			pr_err("CRUS_SP: (send_config) failure code %d\n", ret);
			break;
		}

		sent += chars_to_send;
	}

	kfree(config);
	return ret;
}


static int crus_afe_send_delta(const char *data, uint32_t length)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	struct crus_delta_config_t *payload = NULL;
	int size = sizeof(struct crus_delta_config_t);
	int port = this_ctrl.ff_port;
	int param = CRUS_PARAM_RX_SET_DELTA_CONFIG;
	int module = CIRRUS_SP;
	int chars_to_send, mem_size, sent = 0, ret = 0;
	int index = afe_get_port_index(port);

	pr_info("CRUS_SP: (send_delta) module = 0x%08x, port = 0x%08x\n",
		module, port);

	if (length > APR_CHUNK_SIZE)
		mem_size = APR_CHUNK_SIZE;
	else
		mem_size = length;

	config = crus_gen_afe_set_header(size, port, module, param);
	if (config == NULL) {
		pr_err("CRUS_SP: Memory allocation failed\n");
		return -ENOMEM;
	}

	payload = (struct crus_delta_config_t *)((u8 *)config +
			sizeof(struct afe_custom_crus_set_config_t));
	payload->total_size = length;
	payload->index = 0;
	payload->reserved = 0;
	payload->config = PAYLOAD_FOLLOWS_CONFIG;
	    /* ^ This tells the algorithm to expect array */
	    /*   immediately following the header */

	/* Send config string in chunks of APR_CHUNK_SIZE bytes */
	while (sent < length) {
		chars_to_send = length - sent;
		if (chars_to_send > APR_CHUNK_SIZE) {
			chars_to_send = APR_CHUNK_SIZE;
			payload->done = 0;
		} else
			payload->done = 1;

		/* Configure per message parameter settings */
		memcpy(payload->data, data + sent, chars_to_send);
		payload->chunk_size = chars_to_send;

		/* Send the actual message */
		ret = afe_apr_send_pkt_crus(config, index, 1);

		if (ret) {
			pr_err("CRUS_SP: (send_delta) failure code %d\n", ret);
			break;
		}

		sent += chars_to_send;
	}

	kfree(config);
	return ret;
}

extern int crus_afe_callback(void* payload, int size)
{
	uint32_t* payload32 = payload;

	pr_debug("%s: module=0x%x size = %d\n",
			__func__, (int)payload32[1], size);

	switch(payload32[1]) {
	case CIRRUS_SP:
		memcpy(this_ctrl.user_buffer, payload32, size);
		atomic_set(&this_ctrl.callback_wait, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int msm_routing_cirrus_location_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int location = ucontrol->value.integer.value[0];

	pr_info("%s: %d\n", __func__, location);

	switch(location) {
		case CIRRUS_AFE:
		case CIRRUS_COPP:
		case CIRRUS_NONE:
			this_ctrl.location = location;
			break;
		default:
			break;
	}

	return 0;
}

static int msm_routing_cirrus_location_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: this_ctrl.location= %d", __func__, this_ctrl.location);
	ucontrol->value.integer.value[0] = this_ctrl.location;
	return 0;
}



static int msm_routing_cirrus_fbport_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	this_ctrl.fb_port_index = ucontrol->value.integer.value[0];
	pr_info("%s: %d\n", __func__, this_ctrl.fb_port);

	switch (this_ctrl.fb_port_index) {
		case 0:
			this_ctrl.fb_port = AFE_PORT_ID_PRIMARY_MI2S_TX;
			this_ctrl.ff_port = AFE_PORT_ID_PRIMARY_MI2S_RX;
			break;
		case 1:
			this_ctrl.fb_port = AFE_PORT_ID_SECONDARY_MI2S_TX;
			this_ctrl.ff_port = AFE_PORT_ID_SECONDARY_MI2S_RX;
			break;
		case 2:
			this_ctrl.fb_port = AFE_PORT_ID_TERTIARY_MI2S_TX;
			this_ctrl.ff_port = AFE_PORT_ID_TERTIARY_MI2S_RX;
			break;
		case 3:
			this_ctrl.fb_port = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			this_ctrl.ff_port = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			break;
		default:
			/* Default port to QUATERNARY */
			this_ctrl.fb_port = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			this_ctrl.ff_port = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			break;
	}
	return 0;
}

int msm_routing_cirrus_fbport_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: this_ctrl.fb_port = %d", __func__, this_ctrl.fb_port);
	ucontrol->value.integer.value[0] = this_ctrl.fb_port_index;
	return 0;
}

static int msm_routing_crus_sp_enable_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];


	this_ctrl.enable = crus_set;

    return 0;
}

static int msm_routing_crus_sp_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: %d\n", __func__, this_ctrl.enable);
	ucontrol->value.integer.value[0] = this_ctrl.enable;

	return 0;
}
static int msm_routing_crus_sp_prot_enable_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_err("CRUS_SP: Cirrus SP Protection Enable is set via DT\n");
	return 0;
}

static int msm_routing_crus_sp_prot_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = this_ctrl.prot_en ? 1 : 0;
	return 0;
}

static int msm_routing_crus_sp_usecase_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct crus_rx_run_case_ctrl_t case_ctrl;
	struct crus_rx_temperature_t rx_temp;
	const int crus_set = ucontrol->value.integer.value[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	uint32_t max_index = e->items;
	int buffer[CRUS_MAX_BUFFER_SIZE / 4];

	this_ctrl.usecase = crus_set;
	pr_info("%s: %d\n", __func__, this_ctrl.usecase);
	if (crus_set >= max_index) {
		pr_err("CRUS_SP: Config index out of bounds (%d)\n", crus_set);
		return -EINVAL;
	}

		crus_get_param(this_ctrl.ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
						buffer, CRUS_MAX_BUFFER_SIZE);

	memcpy(&rx_temp, buffer, sizeof(rx_temp));


	case_ctrl.status_l = 1;
	case_ctrl.status_r = 1;
	case_ctrl.z_l = rx_temp.z_l;
	case_ctrl.z_r = rx_temp.z_r;
	case_ctrl.checksum_l = rx_temp.z_l + 1;
	case_ctrl.checksum_r = rx_temp.z_r + 1;
	case_ctrl.atemp = rx_temp.amb_temp_l;
	case_ctrl.value = this_ctrl.usecase;

	if (this_ctrl.prot_en){
			crus_set_param(this_ctrl.fb_port, CIRRUS_SP,
					CRUS_PARAM_TX_SET_USECASE,
					(void *)&this_ctrl.usecase,
					sizeof(this_ctrl.usecase));
	}


	crus_set_param(this_ctrl.ff_port, CIRRUS_SP,
				CRUS_PARAM_RX_SET_USECASE,
				(void *)&case_ctrl, sizeof(case_ctrl));

	return 0;
}

static int msm_routing_crus_sp_usecase_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: %d\n", __func__, this_ctrl.usecase);
	ucontrol->value.integer.value[0] = this_ctrl.usecase;

    return 0;
}

static int msm_routing_crus_load_config_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	const int crus_set = ucontrol->value.integer.value[0];
	char config[CONFIG_FILE_SIZE];
	const struct firmware *firmware;

	pr_debug("%s:  %d\n", __func__, crus_set);
	this_ctrl.conf_sel = crus_set;

	switch (crus_set) {
	case 0: /* "None" */
		break;
	case 1: /* Load RX Config */
		snprintf(config, CONFIG_FILE_SIZE, CRUS_RX_CONFIG,
			 this_ctrl.usecase);

		if (request_firmware(&firmware, config, plat->dev) != 0) {
			pr_err("CRUS_SP: Request firmware failed\n");
			return -EINVAL;
		}

		pr_info("CRUS_SP: Sending RX config...\n");

		crus_afe_send_config(firmware->data, firmware->size,
				     this_ctrl.ff_port, CIRRUS_SP);
		release_firmware(firmware);
		break;
	case 2: /* Load TX Config */
		if (this_ctrl.prot_en == false)
			return -EINVAL;

		snprintf(config, CONFIG_FILE_SIZE, CRUS_TX_CONFIG,
			 this_ctrl.usecase);

		if (request_firmware(&firmware, config, plat->dev) != 0) {
			pr_err("CRUS_SP: Request firmware failed\n");
			return -EINVAL;
		}

		pr_info("CRUS_SP: Sending TX config...\n");

		crus_afe_send_config(firmware->data, firmware->size,
				     this_ctrl.fb_port, CIRRUS_SP);
		release_firmware(firmware);
		break;
	default:
		return -EINVAL;
	}

	this_ctrl.conf_sel = 0;

    return 0;
}

static int msm_routing_crus_load_config_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: %d\n", __func__, this_ctrl.conf_sel);
	ucontrol->value.integer.value[0] = this_ctrl.conf_sel;
    return 0;
}

static int msm_routing_crus_delta_config_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct crus_single_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];
	const struct firmware *firmware;

	switch (crus_set) {
	case 0:
		break;
	case 1: /* Load delta config over AFE */
		this_ctrl.delta_sel = crus_set;

		if (request_firmware(&firmware, "crus_sp_delta_config.bin",
				     plat->dev) != 0) {
			pr_err("CRUS_SP: Request firmware failed\n");
			this_ctrl.delta_sel = 0;
			return -EINVAL;
		}

		pr_info("CRUS_SP: Sending delta config...\n");

		crus_afe_send_delta(firmware->data, firmware->size);
		release_firmware(firmware);
		break;
	case 2: /* Run delta transition */
		this_ctrl.delta_sel = crus_set;
		data.value = 0;
		crus_set_param(this_ctrl.ff_port, CIRRUS_SP,
				CRUS_PARAM_RX_RUN_DELTA_CONFIG,
				&data, sizeof(struct crus_single_data_t));
		break;
	default:
		return -EINVAL;
	}

	this_ctrl.delta_sel = 0;
	return 0;
}

static int msm_routing_crus_delta_config_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = this_ctrl.delta_sel;
	return 0;
}

static int msm_routing_crus_vol_attn_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];
	struct crus_dual_data_t data;

	mutex_lock(&this_ctrl.sp_lock);

	this_ctrl.vol_atten = crus_set;

	switch (crus_set) {
		case 0: /* 0dB */
			data.data1 = VOL_ATTN_MAX;
			data.data2 = VOL_ATTN_MAX;
			break;
		case 1: /* -18dB */
			data.data1 = VOL_ATTN_18DB;
			data.data2 = VOL_ATTN_18DB;
			break;
		case 2: /* -24dB */
			data.data1 = VOL_ATTN_24DB;
			data.data2 = VOL_ATTN_24DB;
			break;
		default:
			return -EINVAL;
	}

	crus_set_param(this_ctrl.ff_port, CIRRUS_SP,
						CRUS_PARAM_RX_SET_ATTENUATION,
						&data, sizeof(struct crus_dual_data_t));

	mutex_unlock(&this_ctrl.sp_lock);

	return 0;
}

static int msm_routing_crus_vol_attn_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Volume Attenuation Get function call\n");
	ucontrol->value.integer.value[0] = this_ctrl.vol_atten;

	return 0;
}


static int msm_routing_crus_chan_swap_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct crus_dual_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];

	switch (crus_set) {
	case 0: /* L/R */
		data.data1 = 1;
		break;
	case 1: /* R/L */
		data.data1 = 2;
		break;
	default:
		return -EINVAL;
	}

	data.data2 = this_ctrl.ch_sw_duration;

	mutex_lock(&this_ctrl.sp_lock);

	this_ctrl.ch_sw = crus_set;

	crus_set_param(this_ctrl.ff_port, CIRRUS_SP,
				CRUS_PARAM_RX_CHANNEL_SWAP,
				&data, sizeof(struct crus_dual_data_t));

	mutex_unlock(&this_ctrl.sp_lock);

	return 0;
}

static int msm_routing_crus_chan_swap_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct crus_single_data_t data;
	pr_debug("%s: %d\n", __func__, this_ctrl.ch_sw);
	crus_get_param(this_ctrl.ff_port, CIRRUS_SP,
			   CRUS_PARAM_RX_GET_CHANNEL_SWAP,
				&data, sizeof(struct crus_single_data_t));

	ucontrol->value.integer.value[0] = this_ctrl.ch_sw;

	return 0;
}

static int msm_routing_crus_chan_swap_dur_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int crus_set = ucontrol->value.integer.value[0];

	pr_debug("%s: %d\n", __func__, crus_set);

	if ((crus_set < 0) || (crus_set > MAX_CHAN_SWAP_SAMPLES)) {
		pr_err("CRUS_SP: Value out of range (%d)\n", crus_set);
		return -EINVAL;
	}

	if (crus_set < MIN_CHAN_SWAP_SAMPLES) {
		pr_info("CRUS_SP: Received %d, round up to min value %d\n",
			crus_set, MIN_CHAN_SWAP_SAMPLES);
		crus_set = MIN_CHAN_SWAP_SAMPLES;
	}

	this_ctrl.ch_sw_duration = crus_set;

	return 0;
}

static int msm_routing_crus_chan_swap_dur_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: %d\n", __func__, this_ctrl.ch_sw_duration);

	ucontrol->value.integer.value[0] = this_ctrl.ch_sw_duration;

	return 0;
}
static int msm_routing_crus_fail_det(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int msm_routing_crus_fail_det_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const char * const cirrus_location_text[] = {"NONE", "COPP", "AFE"};
static const char * const cirrus_fb_port_text[] = {"PRI_MI2S_RX",
						   "SEC_MI2S_RX",
						   "TERT_MI2S_RX",
						   "QUAT_MI2S_RX",
						   "QUAT_TDM_RX_0",
						   "SLIMBUS_0_RX"};

static const char * const crus_en_text[] = {"Config SP Disable",
					    "Config SP Enable"};

static const char * const crus_prot_en_text[] = {"Disable", "Enable"};

static const char * const crus_conf_load_text[] = {"Idle", "Load RX",
						   "Load TX"};

static const char * const crus_conf_load_no_prot_text[] = {"Idle", "Load"};

static const char * const crus_delta_text[] = {"Idle", "Load", "Run"};

static const char * const crus_chan_swap_text[] = {"LR", "RL"};
static const char * crus_sp_usecase_dt_text[MAX_TUNING_CONFIGS] = {"Music", "Voice", "Headset", "Handsfree"};

static const char * const crus_vol_attn_text[] = {"0dB", "-18dB", "-24dB"};


static const struct soc_enum cirrus_location_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, cirrus_location_text),
};
static const struct soc_enum cirrus_fb_controls_enum[] = {
	SOC_ENUM_SINGLE_EXT(6, cirrus_fb_port_text),
};

static const struct soc_enum crus_en_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_en_text),
};

static const struct soc_enum crus_prot_en_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_prot_en_text),
};

static struct soc_enum crus_sp_usecase_enum[] = {
	SOC_ENUM_SINGLE_EXT(MAX_TUNING_CONFIGS, crus_sp_usecase_dt_text),
};

static const struct soc_enum crus_conf_load_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_conf_load_text),
};

static const struct soc_enum crus_conf_load_no_prot_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_conf_load_no_prot_text),
};

static const struct soc_enum crus_delta_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_delta_text),
};

static const struct soc_enum crus_chan_swap_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_chan_swap_text),
};

static const struct soc_enum crus_vol_attn_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_vol_attn_text),
};
static const struct snd_kcontrol_new crus_protect_controls[] = {
	SOC_ENUM_EXT("Cirrus SP FBPort", cirrus_fb_controls_enum[0],
	msm_routing_cirrus_fbport_get, msm_routing_cirrus_fbport_put),
	SOC_ENUM_EXT("Cirrus SP", crus_en_enum[0],
	msm_routing_crus_sp_enable_get, msm_routing_crus_sp_enable_put),
	SOC_ENUM_EXT("Cirrus SP Protection", crus_prot_en_enum[0],
	msm_routing_crus_sp_prot_enable_get, msm_routing_crus_sp_prot_enable_put),
	SOC_ENUM_EXT("Cirrus SP Usecase", crus_sp_usecase_enum[0],
	msm_routing_crus_sp_usecase_get, msm_routing_crus_sp_usecase_put),
	SOC_ENUM_EXT("Cirrus SP Load Config", crus_conf_load_enum[0],
	msm_routing_crus_load_config_get, msm_routing_crus_load_config_put),
	SOC_ENUM_EXT("Cirrus SP Delta Config", crus_delta_enum[0],
	msm_routing_crus_delta_config_get, msm_routing_crus_delta_config_put),
	SOC_ENUM_EXT("Cirrus SP Channel Swap", crus_chan_swap_enum[0],
	msm_routing_crus_chan_swap_get, msm_routing_crus_chan_swap_put),
	SOC_SINGLE_EXT("Cirrus SP Channel Swap Duration", SND_SOC_NOPM, 0,
	MAX_CHAN_SWAP_SAMPLES, 0, msm_routing_crus_chan_swap_dur_get,
	msm_routing_crus_chan_swap_dur_put),
	SOC_SINGLE_BOOL_EXT("Cirrus SP Failure Detection", 0,
	msm_routing_crus_fail_det_get, msm_routing_crus_fail_det),
	SOC_ENUM_EXT("Cirrus SP Location", cirrus_location_enum[0],
	msm_routing_cirrus_location_get, msm_routing_cirrus_location_put),
};
static const struct snd_kcontrol_new crus_no_protect_controls[] = {
	SOC_ENUM_EXT("Cirrus SP FBPort", cirrus_fb_controls_enum[0],
	msm_routing_cirrus_fbport_get, msm_routing_cirrus_fbport_put),
	SOC_ENUM_EXT("Cirrus SP", crus_en_enum[0],
	msm_routing_crus_sp_enable_get, msm_routing_crus_sp_enable_put),
	SOC_ENUM_EXT("Cirrus SP Protection", crus_prot_en_enum[0],
	msm_routing_crus_sp_prot_enable_get, msm_routing_crus_sp_prot_enable_put),
	SOC_ENUM_EXT("Cirrus SP Usecase", crus_sp_usecase_enum[0],
	msm_routing_crus_sp_usecase_get, msm_routing_crus_sp_usecase_put),
	SOC_ENUM_EXT("Cirrus SP Load Config", crus_conf_load_no_prot_enum[0],
	msm_routing_crus_load_config_get, msm_routing_crus_load_config_put),
	SOC_ENUM_EXT("Cirrus SP Delta Config", crus_delta_enum[0],
	msm_routing_crus_delta_config_get, msm_routing_crus_delta_config_put),
	SOC_ENUM_EXT("Cirrus SP Channel Swap", crus_chan_swap_enum[0],
	msm_routing_crus_chan_swap_get, msm_routing_crus_chan_swap_put),
	SOC_SINGLE_EXT("Cirrus SP Channel Swap Duration", SND_SOC_NOPM, 0,
	MAX_CHAN_SWAP_SAMPLES, 0, msm_routing_crus_chan_swap_dur_get,
	msm_routing_crus_chan_swap_dur_put),
	SOC_ENUM_EXT("Cirrus SP Volume Attenuation", crus_vol_attn_enum[0],
	msm_routing_crus_vol_attn_get, msm_routing_crus_vol_attn_put),
	SOC_ENUM_EXT("Cirrus SP Location", cirrus_location_enum[0],
	msm_routing_cirrus_location_get, msm_routing_cirrus_location_put),
};

void msm_crus_pb_add_controls(struct snd_soc_platform *platform)
{
	if (this_ctrl.usecase_dt_count == 0)
		pr_info("CRUS_SP: Usecase config not specified\n");

	crus_sp_usecase_enum[0].items = this_ctrl.usecase_dt_count;
	crus_sp_usecase_enum[0].texts = crus_sp_usecase_dt_text;

	if (this_ctrl.prot_en)
		snd_soc_add_platform_controls(platform, crus_protect_controls,
					ARRAY_SIZE(crus_protect_controls));
	else
		snd_soc_add_platform_controls(platform,
					      crus_no_protect_controls,
					ARRAY_SIZE(crus_no_protect_controls));
}

EXPORT_SYMBOL(msm_crus_pb_add_controls);

static long crus_sp_shared_ioctl(struct file *f, unsigned int cmd,
				 void __user *arg)
{
	int result = 0, port;
	uint32_t bufsize = 0, size;
	void *io_data = NULL;

	pr_info("%s\n", __func__);

	if (copy_from_user(&size, arg, sizeof(size))) {
		pr_err("CRUS_SP: copy_from_user (size) failed\n");
		result = -EFAULT;
		goto exit;
	}

	/* Copy IOCTL header from usermode */
	if (copy_from_user(&crus_sp_hdr, arg, size)) {
		pr_err("CRUS_SP: copy_from_user (struct) failed\n");
		result = -EFAULT;
		goto exit;
	}

	bufsize = crus_sp_hdr.data_length;
	io_data = kzalloc(bufsize, GFP_KERNEL);

	switch (cmd) {
	case CRUS_SP_IOCTL_GET:
		switch (crus_sp_hdr.module_id) {
		case CRUS_MODULE_ID_TX:
			port = this_ctrl.fb_port;
		break;
		case CRUS_MODULE_ID_RX:
			port = this_ctrl.ff_port;
		break;
		default:
			pr_info("CRUS_SP: Unrecognized port ID (%d)\n",
				crus_sp_hdr.module_id);
			port = this_ctrl.ff_port;
		}

		crus_get_param(port, CIRRUS_SP, crus_sp_hdr.param_id,
						io_data, bufsize);
		result = copy_to_user(crus_sp_hdr.data, io_data, bufsize);
		if (result) {
			pr_err("CRUS_SP: copy_to_user failed (%d)\n", result);
			result = -EFAULT;
		} else
			result = bufsize;
	break;
	case CRUS_SP_IOCTL_SET:
		result = copy_from_user(io_data, (void *)crus_sp_hdr.data,
					bufsize);
		if (result) {
			pr_err("CRUS_SP: copy_from_user failed (%d)\n", result);
			result = -EFAULT;
			goto exit_io;
		}

		switch (crus_sp_hdr.module_id) {
		case CRUS_MODULE_ID_TX:
			port = this_ctrl.fb_port;
		break;
		case CRUS_MODULE_ID_RX:
			port = this_ctrl.ff_port;
		break;
		default:
			pr_info("%s: Unrecognized port ID (%d)\n", __func__,
			       crus_sp_hdr.module_id);
			port = this_ctrl.ff_port;
		}

		crus_set_param(port, CIRRUS_SP,
					crus_sp_hdr.param_id, io_data, bufsize);
	break;

	default:
		pr_err("CRUS_SP: Invalid IOCTL, command = %d\n", cmd);
		result = -EINVAL;
	break;
	}

exit_io:
	kfree(io_data);
exit:
	return result;
}

static long crus_sp_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	pr_info("%s\n", __func__);

	return crus_sp_shared_ioctl(f, cmd, (void __user *)arg);
}

static long crus_sp_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;

	pr_info("%s\n", __func__);

	switch (cmd) {
	case CRUS_SP_IOCTL_GET32:
		cmd64 = CRUS_SP_IOCTL_GET;
		break;
	case CRUS_SP_IOCTL_SET32:
		cmd64 = CRUS_SP_IOCTL_SET;
		break;
	default:
		pr_err("CRUS_SP: Invalid IOCTL, command = %d\n", cmd);
		return -EINVAL;
	}

	return crus_sp_shared_ioctl(f, cmd64, compat_ptr(arg));
}

static int crus_sp_open(struct inode *inode, struct file *f)
{
	pr_debug("%s\n", __func__);

	atomic_inc(&this_ctrl.count_wait);
	return 0;
}

static int crus_sp_release(struct inode *inode, struct file *f)
{
	atomic_dec(&this_ctrl.count_wait);
	return 0;
}

static ssize_t temperature_left_show(struct device *dev,
				     struct device_attribute *a, char *buf)
{
	struct crus_rx_temperature_t rx_temp;
	static const int material = 250;
	static const int scale_factor = 100000;
	int buffer[CRUS_MAX_BUFFER_SIZE / 4];
	int out_cal0;
	int out_cal1;
	int z, r, t;
	int temp0;

	crus_get_param(this_ctrl.ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
				buffer, CRUS_MAX_BUFFER_SIZE);

	memcpy(&rx_temp, buffer, sizeof(rx_temp));

	out_cal0 = rx_temp.hp_status_l;
	out_cal1 = rx_temp.full_status_l;

	z = rx_temp.z_l;

	temp0 = rx_temp.amb_temp_l;

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = rx_temp.temp_l;
	t = (material * scale_factor * (r-z) / z) + (temp0 * scale_factor);

	return snprintf(buf, PAGE_SIZE, "%d.%05dc\n", t / scale_factor,
			t % scale_factor);
}
static DEVICE_ATTR_RO(temperature_left);

static ssize_t temperature_right_show(struct device *dev,
				      struct device_attribute *a, char *buf)
{
	struct crus_rx_temperature_t rx_temp;
	static const int material = 250;
	static const int scale_factor = 100000;
	int buffer[CRUS_MAX_BUFFER_SIZE / 4];
	int out_cal0;
	int out_cal1;
	int z, r, t;
	int temp0;

	crus_get_param(this_ctrl.ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
				buffer, CRUS_MAX_BUFFER_SIZE);

	memcpy(&rx_temp, buffer, sizeof(rx_temp));

	out_cal0 = rx_temp.hp_status_r;
	out_cal1 = rx_temp.full_status_r;

	z = rx_temp.z_r;

	temp0 = rx_temp.amb_temp_r;

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = rx_temp.temp_r;
	t = (material * scale_factor * (r-z) / z) + (temp0 * scale_factor);

	return snprintf(buf, PAGE_SIZE, "%d.%05dc\n", t / scale_factor,
			t % scale_factor);
}
static DEVICE_ATTR_RO(temperature_right);

static ssize_t resistance_left_show(struct device *dev,
				    struct device_attribute *a, char *buf)
{
	struct crus_rx_temperature_t rx_temp;
	static const int scale_factor = 100000000;
	static const int amp_factor = 71498;
	int buffer[CRUS_MAX_BUFFER_SIZE / 4];
	int out_cal0;
	int out_cal1;
	int r;

	crus_get_param(this_ctrl.ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
					buffer, CRUS_MAX_BUFFER_SIZE);

	memcpy(&rx_temp, buffer, sizeof(rx_temp));

	out_cal0 = rx_temp.hp_status_l;
	out_cal1 = rx_temp.full_status_l;

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = rx_temp.temp_l * amp_factor;

	return snprintf(buf, PAGE_SIZE, "%d.%08d ohms\n", r / scale_factor,
		       r % scale_factor);
}
static DEVICE_ATTR_RO(resistance_left);

static ssize_t resistance_right_show(struct device *dev,
				     struct device_attribute *a, char *buf)
{
	struct crus_rx_temperature_t rx_temp;
	static const int scale_factor = 100000000;
	static const int amp_factor = 71498;
	int buffer[CRUS_MAX_BUFFER_SIZE / 4];
	int out_cal0;
	int out_cal1;
	int r;

	crus_get_param(this_ctrl.ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
					buffer, CRUS_MAX_BUFFER_SIZE);

	memcpy(&rx_temp, buffer, sizeof(rx_temp));

	out_cal0 = rx_temp.hp_status_r;
	out_cal1 = rx_temp.full_status_r;

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = rx_temp.temp_r * amp_factor;

	return snprintf(buf, PAGE_SIZE, "%d.%08d ohms\n", r / scale_factor,
		       r % scale_factor);
}
static DEVICE_ATTR_RO(resistance_right);

static struct attribute *crus_sp_attrs[] = {
	&dev_attr_temperature_left.attr,
	&dev_attr_temperature_right.attr,
	&dev_attr_resistance_left.attr,
	&dev_attr_resistance_right.attr,
	NULL,
};

static const struct attribute_group crus_sp_group = {
	.attrs  = crus_sp_attrs,
};

static const struct attribute_group *crus_sp_groups[] = {
	&crus_sp_group,
	NULL,
};

#ifdef CONFIG_OF
static int msm_cirrus_playback_probe(struct platform_device *pdev)
{
	int i;

	pr_info("CRUS_SP: Initializing platform device\n");

	this_ctrl.usecase_dt_count = of_property_count_strings(pdev->dev.of_node,
							     "usecase-names");
	if (this_ctrl.usecase_dt_count <= 0) {
		pr_debug("CRUS_SP: Usecase names not found\n");
		this_ctrl.usecase_dt_count = 0;
		return 0;
	}

	if ((this_ctrl.usecase_dt_count > 0) &&
	    (this_ctrl.usecase_dt_count <= MAX_TUNING_CONFIGS))
		of_property_read_string_array(pdev->dev.of_node,
					      "usecase-names",
					      crus_sp_usecase_dt_text,
					      this_ctrl.usecase_dt_count);
	else if (this_ctrl.usecase_dt_count > MAX_TUNING_CONFIGS) {
		pr_err("CRUS_SP: Max of %d usecase configs allowed\n",
			MAX_TUNING_CONFIGS);
		return -EINVAL;
	}

	for (i = 0; i < this_ctrl.usecase_dt_count; i++)
		pr_info("CRUS_SP: Usecase[%d] = %s\n", i,
			 crus_sp_usecase_dt_text[i]);

	this_ctrl.prot_en = of_property_read_bool(pdev->dev.of_node,
						  "protect-en");

	return 0;
}

static const struct of_device_id msm_cirrus_playback_dt_match[] = {
	{.compatible = "cirrus,msm-cirrus-playback"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_cirrus_playback_dt_match);

static struct platform_driver msm_cirrus_playback_driver = {
	.driver = {
		.name = "msm-cirrus-playback",
		.owner = THIS_MODULE,
		.of_match_table = msm_cirrus_playback_dt_match,
	},
	.probe = msm_cirrus_playback_probe,
};
#endif

static const struct file_operations crus_sp_fops = {
	.owner = THIS_MODULE,
	.open = crus_sp_open,
	.release = crus_sp_release,
	.unlocked_ioctl = crus_sp_ioctl,
	.compat_ioctl = crus_sp_compat_ioctl,
};

struct miscdevice crus_sp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_cirrus_playback",
	.fops = &crus_sp_fops,
};

int __init crus_sp_init(void)
{
	pr_info("Initializing misc device\n");
	atomic_set(&this_ctrl.callback_wait, 0);
	atomic_set(&this_ctrl.count_wait, 0);
	mutex_init(&this_ctrl.param_lock);
	mutex_init(&this_ctrl.sp_lock);

	misc_register(&crus_sp_misc);

	if (sysfs_create_groups(&crus_sp_misc.this_device->kobj,
				crus_sp_groups))
		pr_err("%s: Could not create sysfs groups\n", __func__);

#ifdef CONFIG_OF
	platform_driver_register(&msm_cirrus_playback_driver);
#endif
	return 0;
}

void __exit crus_sp_exit(void)
{
	pr_debug("%s\n", __func__);
	mutex_destroy(&this_ctrl.param_lock);

#ifdef CONFIG_OF
	platform_driver_unregister(&msm_cirrus_playback_driver);
#endif
}

MODULE_AUTHOR("Cirrus SP");
MODULE_DESCRIPTION("Providing Interface to Cirrus SP");
//MODULE_LICENSE("GPL");
MODULE_LICENSE("GPL v2");
