
/*
 *  controls_v2_dpcm.c - Intel MID Platform driver DPCM ALSA controls for Mrfld
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Omair Mohammed Abdullah <omair.m.abdullah@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <asm/platform_sst_audio.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "../platform_ipc_v2.h"
#include "../sst_platform.h"
#include "../sst_platform_pvt.h"
#include "controls_v2.h"
#include "sst_widgets.h"

static inline void sst_fill_byte_control(char *param,
					 u8 ipc_msg, u8 block,
					 u8 task_id, u8 pipe_id,
					 u16 len, void *cmd_data)
{

	struct snd_sst_bytes_v2 *byte_data = (struct snd_sst_bytes_v2 *)param;
	byte_data->type = SST_CMD_BYTES_SET;
	byte_data->ipc_msg = ipc_msg;
	byte_data->block = block;
	byte_data->task_id = task_id;
	byte_data->pipe_id = pipe_id;

	if (len > SST_MAX_BIN_BYTES - sizeof(*byte_data)) {
		pr_err("%s: command length too big (%u)", __func__, len);
		len = SST_MAX_BIN_BYTES - sizeof(*byte_data);
		WARN_ON(1); /* this happens only if code is wrong */
	}
	byte_data->len = len;
	memcpy(byte_data->bytes, cmd_data, len);
	print_hex_dump_bytes("writing to lpe: ", DUMP_PREFIX_OFFSET,
			     byte_data, len + sizeof(*byte_data));
}

static int sst_fill_and_send_cmd_unlocked(struct sst_data *sst,
				 u8 ipc_msg, u8 block, u8 task_id, u8 pipe_id,
				 void *cmd_data, u16 len)
{
	sst_fill_byte_control(sst->byte_stream, ipc_msg, block, task_id, pipe_id,
			      len, cmd_data);
	return sst_dsp->ops->set_generic_params(SST_SET_BYTE_STREAM,
						sst->byte_stream);
}

/**
 * sst_fill_and_send_cmd - generate the IPC message and send it to the FW
 * @ipc_msg:	type of IPC (CMD, SET_PARAMS, GET_PARAMS)
 * @cmd_data:	the IPC payload
 */
static int sst_fill_and_send_cmd(struct sst_data *sst,
				 u8 ipc_msg, u8 block, u8 task_id, u8 pipe_id,
				 void *cmd_data, u16 len)
{
	int ret;

	mutex_lock(&sst->lock);
	ret = sst_fill_and_send_cmd_unlocked(sst, ipc_msg, block, task_id, pipe_id,
					     cmd_data, len);
	mutex_unlock(&sst->lock);

	return ret;
}

static int sst_probe_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct sst_probe_value *v = (void *)kcontrol->private_value;

	ucontrol->value.enumerated.item[0] = v->val;
	return 0;
}

static int sst_probe_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct sst_probe_value *v = (void *)kcontrol->private_value;
	const struct soc_enum *e = v->p_enum;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;
	v->val = ucontrol->value.enumerated.item[0];
	return 0;
}

int sst_probe_enum_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct sst_probe_value *v = (void *)kcontrol->private_value;
	const struct soc_enum *e = v->p_enum;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->items;

	if (uinfo->value.enumerated.item > e->items - 1)
		uinfo->value.enumerated.item = e->items - 1;
	strcpy(uinfo->value.enumerated.name,
		e->texts[uinfo->value.enumerated.item]);
	return 0;
}

/*
 * slot map value is a bitfield where each bit represents a FW channel
 *
 *			3 2 1 0		# 0 = codec0, 1 = codec1
 *			RLRLRLRL	# 3, 4 = reserved
 *
 * e.g. slot 0 rx map =	00001100b -> data from slot 0 goes into codec_in1 L,R
 */
static u8 sst_ssp_slot_map[SST_MAX_TDM_SLOTS] = {
	0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, /* default rx map */
};

/*
 * channel map value is a bitfield where each bit represents a slot
 *
 *			  76543210	# 0 = slot 0, 1 = slot 1
 *
 * e.g. codec1_0 tx map = 00000101b -> data from codec_out1_0 goes into slot 0, 2
 */
static u8 sst_ssp_channel_map[SST_MAX_TDM_SLOTS] = {
	0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, /* default tx map */
};

static void sst_send_slot_map(struct sst_data *sst)
{
	struct sst_param_sba_ssp_slot_map cmd;

	pr_debug("Enter: %s\n", __func__);

	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);
	cmd.header.command_id = SBA_SET_SSP_SLOT_MAP;
	cmd.header.length = sizeof(struct sst_param_sba_ssp_slot_map)
				- sizeof(struct sst_dsp_header);

	cmd.param_id = SBA_SET_SSP_SLOT_MAP;
	cmd.param_len = sizeof(cmd.rx_slot_map) + sizeof(cmd.tx_slot_map) + sizeof(cmd.ssp_index);
	cmd.ssp_index = SSP_CODEC;

	memcpy(cmd.rx_slot_map, &sst_ssp_slot_map[0], sizeof(cmd.rx_slot_map));
	memcpy(cmd.tx_slot_map, &sst_ssp_channel_map[0], sizeof(cmd.tx_slot_map));

	sst_fill_and_send_cmd(sst, SST_IPC_IA_SET_PARAMS, SST_FLAG_BLOCKED,
			      SST_TASK_SBA, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);
}

int sst_slot_enum_info(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_info *uinfo)
{
	struct sst_enum *e = (struct sst_enum *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
		e->texts[uinfo->value.enumerated.item]);
	return 0;
}

/**
 * sst_slot_get - get the status of the interleaver/deinterleaver control
 *
 * Searches the map where the control status is stored, and gets the
 * channel/slot which is currently set for this enumerated control. Since it is
 * an enumerated control, there is only one possible value.
 */
static int sst_slot_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct sst_enum *e = (void *)kcontrol->private_value;
	unsigned int ctl_no = e->reg;
	unsigned int is_tx = e->tx;
	unsigned int val, mux;
	u8 *map = is_tx ? sst_ssp_channel_map : sst_ssp_slot_map;

	val = 1 << ctl_no;
	/* search which slot/channel has this bit set - there should be only one */
	for (mux = e->max; mux > 0;  mux--)
		if (map[mux - 1] & val)
			break;

	ucontrol->value.enumerated.item[0] = mux;
	pr_debug("%s: %s - %s map = %#x\n", __func__, is_tx ? "tx channel" : "rx slot",
		 e->texts[mux], mux ? map[mux - 1] : -1);
	return 0;
}

/**
 * sst_slot_put - set the status of interleaver/deinterleaver control
 *
 * (de)interleaver controls are defined in opposite sense to be user-friendly
 *
 * Instead of the enum value being the value written to the register, it is the
 * register address; and the kcontrol number (register num) is the value written
 * to the register. This is so that there can be only one value for each
 * slot/channel since there is only one control for each slot/channel.
 *
 * This means that whenever an enum is set, we need to clear the bit
 * for that kcontrol_no for all the interleaver OR deinterleaver registers
 */
static int sst_slot_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct sst_enum *e = (void *)kcontrol->private_value;
	int i;
	unsigned int ctl_no = e->reg;
	unsigned int is_tx = e->tx;
	unsigned int slot_channel_no;
	unsigned int val, mux;

	u8 *map = is_tx ? sst_ssp_channel_map : sst_ssp_slot_map;

	val = 1 << ctl_no;
	mux = ucontrol->value.enumerated.item[0];
	if (mux > e->max - 1)
		return -EINVAL;

	/* first clear all registers of this bit */
	for (i = 0; i < e->max; i++)
		map[i] &= ~val;

	if (mux == 0) /* kctl set to 'none' */
		return 0;

	/* offset by one to take "None" into account */
	slot_channel_no = mux - 1;
	map[slot_channel_no] |= val;

	pr_debug("%s: %s %s map = %#x\n", __func__, is_tx ? "tx channel" : "rx slot",
		 e->texts[mux], map[slot_channel_no]);

	if (e->w && e->w->power)
		sst_send_slot_map(sst);
	return 0;
}

/* assumes a boolean mux */
static inline bool get_mux_state(struct sst_data *sst, unsigned int reg, unsigned int shift)
{
	return sst_reg_read(sst, reg, shift, 1) == 1;
}

int sst_vtsv_event_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	pr_debug("in %s\n", __func__);
	/* First element contains size */
	memcpy(ucontrol->value.bytes.data, sst->vtsv_result.data, sst->vtsv_result.data[0]);
	return 0;
}

static int sst_mux_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct sst_data *sst = snd_soc_platform_get_drvdata(widget->platform);
	struct soc_enum *e = (void *)kcontrol->private_value;
	unsigned int max = e->items - 1;

	ucontrol->value.enumerated.item[0] = sst_reg_read(sst, e->reg, e->shift_l, max);
	return 0;
}

static int sst_mux_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct sst_data *sst = snd_soc_platform_get_drvdata(widget->platform);
	struct soc_enum *e = (void *)kcontrol->private_value;
	struct snd_soc_dapm_update update;
	unsigned int max = e->items - 1;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int mux, val;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;

	mux = ucontrol->value.enumerated.item[0];
	val = sst_reg_write(sst, e->reg, e->shift_l, max, mux);

	pr_debug("%s: reg[%d] = %#x\n", __func__, e->reg, val);

	dapm_kcontrol_set_value(kcontrol, val);
	update.kcontrol = kcontrol;
	update.reg = e->reg;
	update.mask = mask;
	update.val = val;

	snd_soc_dapm_mux_update_power(widget->dapm, kcontrol, mux, e, &update);
	return 0;
}

static int sst_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *e = (void *)kcontrol->private_value;
	unsigned int max = e->items - 1;

	ucontrol->value.enumerated.item[0] = sst_reg_read(sst, e->reg, e->shift_l, max);
	return 0;
}

static int sst_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct soc_enum *e = (void *)kcontrol->private_value;
	unsigned int max = e->items - 1;
	unsigned int val;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;

	val = sst_reg_write(sst, e->reg, e->shift_l, max, ucontrol->value.enumerated.item[0]);
	pr_debug("%s: reg[%d] - %#x\n", __func__, e->reg, val);
	return 0;
}

static void sst_send_algo_cmd(struct sst_data *sst,
			      struct sst_algo_control *bc)
{
	int len;
	struct sst_cmd_set_params *cmd;

	if (bc->params == NULL)
		return;

	/* bc->max includes sizeof algos + length field */
	len = sizeof(cmd->dst) + sizeof(cmd->command_id) + bc->max;

	cmd = kzalloc(len, GFP_KERNEL);
	if (cmd == NULL) {
		pr_err("Failed to send cmd, kzalloc failed\n");
		return;
	}

	SST_FILL_DESTINATION(2, cmd->dst, bc->pipe_id, bc->module_id);
	cmd->command_id = bc->cmd_id;
	memcpy(cmd->params, bc->params, bc->max);

	sst_fill_and_send_cmd(sst, SST_IPC_IA_SET_PARAMS, SST_FLAG_BLOCKED,
			      bc->task_id, 0, cmd, len);
	kfree(cmd);
}

/**
 * sst_find_and_send_pipe_algo - send all the algo parameters for a pipe
 *
 * The algos which are in each pipeline are sent to the firmware one by one
 */
static void sst_find_and_send_pipe_algo(struct sst_data *sst,
					const char *pipe, struct sst_ids *ids)
{
	struct sst_algo_control *bc;
	struct module *algo = NULL;

	pr_debug("Enter: %s, widget=%s\n", __func__, pipe);

	list_for_each_entry(algo, &ids->algo_list, node) {
		bc = (void *)algo->kctl->private_value;

		pr_debug("Found algo control name=%s pipe=%s\n", algo->kctl->id.name, pipe);
		sst_send_algo_cmd(sst, bc);
	}
}

int sst_algo_bytes_ctl_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct sst_algo_control *bc = (void *)kcontrol->private_value;
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = bc->max;

	/* allocate space to cache the algo parameters in the driver */
	if (bc->params == NULL) {
		bc->params = devm_kzalloc(platform->dev, bc->max, GFP_KERNEL);
		if (bc->params == NULL) {
			pr_err("kzalloc failed\n");
			return -ENOMEM;
		}
	}
	return 0;
}

static int sst_algo_control_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct sst_algo_control *bc = (void *)kcontrol->private_value;

	switch (bc->type) {
	case SST_ALGO_PARAMS:
		if (bc->params)
			memcpy(ucontrol->value.bytes.data, bc->params, bc->max);
		break;
	case SST_ALGO_BYPASS:
		ucontrol->value.integer.value[0] = bc->bypass ? 1 : 0;
		pr_debug("%s: bypass  %d\n", __func__, bc->bypass);
		break;
	default:
		pr_err("Invalid Input- algo type:%d\n", bc->type);
		return -EINVAL;

	}
	return 0;
}

static int sst_algo_control_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct sst_algo_control *bc = (void *)kcontrol->private_value;

	pr_debug("in %s control_name=%s\n", __func__, kcontrol->id.name);
	switch (bc->type) {
	case SST_ALGO_PARAMS:
		if (bc->params)
			memcpy(bc->params, ucontrol->value.bytes.data, bc->max);
		break;
	case SST_ALGO_BYPASS:
		bc->bypass = !!ucontrol->value.integer.value[0];
		break;
	default:
		pr_err("Invalid Input- algo type:%ld\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}
	/*if pipe is enabled, need to send the algo params from here */
	if (bc->w && bc->w->power)
		sst_send_algo_cmd(sst, bc);

	return 0;
}

static int sst_gain_ctl_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct sst_gain_mixer_control *mc = (void *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = mc->stereo ? 2 : 1;
	uinfo->value.integer.min = mc->min;
	uinfo->value.integer.max = mc->max;
	return 0;
}

/**
 * sst_send_gain_cmd - send the gain algorithm IPC to the FW
 * @gv:		the stored value of gain (also contains rampduration)
 * @mute:	flag that indicates whether this was called from the
 *		digital_mute callback or directly. If called from the
 *		digital_mute callback, module will be muted/unmuted based on this
 *		flag. The flag is always 0 if called directly.
 *
 * The user-set gain value is sent only if the user-controllable 'mute' control
 * is OFF (indicated by gv->mute). Otherwise, the mute value (MIN value) is
 * sent.
 */
static void sst_send_gain_cmd(struct sst_data *sst, struct sst_gain_value *gv,
			      u16 task_id, u16 loc_id, u16 module_id, int mute)
{
	struct sst_cmd_set_gain_dual cmd;
	pr_debug("%s\n", __func__);

	cmd.header.command_id = MMX_SET_GAIN;
	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);
	cmd.gain_cell_num = 1;

	if (mute || gv->mute) {
		cmd.cell_gains[0].cell_gain_left = SST_GAIN_MIN_VALUE;
		cmd.cell_gains[0].cell_gain_right = SST_GAIN_MIN_VALUE;
	} else {
		cmd.cell_gains[0].cell_gain_left = gv->l_gain;
		cmd.cell_gains[0].cell_gain_right = gv->r_gain;
	}
	SST_FILL_DESTINATION(2, cmd.cell_gains[0].dest,
			     loc_id, module_id);
	cmd.cell_gains[0].gain_time_constant = gv->ramp_duration;

	cmd.header.length = sizeof(struct sst_cmd_set_gain_dual)
				- sizeof(struct sst_dsp_header);

	sst_fill_and_send_cmd(sst, SST_IPC_IA_SET_PARAMS, SST_FLAG_BLOCKED,
			      task_id, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);
}

static int sst_gain_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct sst_gain_mixer_control *mc = (void *)kcontrol->private_value;
	struct sst_gain_value *gv = mc->gain_val;

	switch (mc->type) {
	case SST_GAIN_TLV:
		ucontrol->value.integer.value[0] = gv->l_gain;
		ucontrol->value.integer.value[1] = gv->r_gain;
		break;
	case SST_GAIN_MUTE:
		ucontrol->value.integer.value[0] = gv->mute ? 1 : 0;
		break;
	case SST_GAIN_RAMP_DURATION:
		ucontrol->value.integer.value[0] = gv->ramp_duration;
		break;
	default:
		pr_err("Invalid Input- gain type:%d\n", mc->type);
		return -EINVAL;
	};
	return 0;
}

static int sst_gain_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct sst_gain_mixer_control *mc = (void *)kcontrol->private_value;
	struct sst_gain_value *gv = mc->gain_val;

	switch (mc->type) {
	case SST_GAIN_TLV:
		gv->l_gain = ucontrol->value.integer.value[0];
		gv->r_gain = ucontrol->value.integer.value[1];
		pr_debug("%s: %s: Volume %d, %d\n", __func__, mc->pname, gv->l_gain, gv->r_gain);
		break;
	case SST_GAIN_MUTE:
		gv->mute = !!ucontrol->value.integer.value[0];
		pr_debug("%s: %s: Mute %d\n", __func__, mc->pname, gv->mute);
		break;
	case SST_GAIN_RAMP_DURATION:
		gv->ramp_duration = ucontrol->value.integer.value[0];
		pr_debug("%s: %s: RampDuration %d\n", __func__, mc->pname, gv->ramp_duration);
		break;
	default:
		pr_err("Invalid Input- gain type:%d\n", mc->type);
		return -EINVAL;
	};

	if (mc->w && mc->w->power)
		sst_send_gain_cmd(sst, gv, mc->task_id,
				mc->pipe_id | mc->instance_id, mc->module_id, 0);
	return 0;
}

static void sst_set_pipe_gain(struct sst_ids *ids, struct sst_data *sst, int mute);

static void sst_send_pipe_module_params(struct snd_soc_dapm_widget *w)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;

	sst_find_and_send_pipe_algo(sst, w->name, ids);
	sst_set_pipe_gain(ids, sst, 0);
}

static int sst_generic_modules_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *k, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		sst_send_pipe_module_params(w);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(sst_gain_tlv_common, SST_GAIN_MIN_VALUE * 10, 10, 0);

/* Look up table to convert MIXER SW bit regs to SWM inputs */
static const uint swm_mixer_input_ids[SST_SWM_INPUT_COUNT] = {
	[SST_IP_MODEM]		= SST_SWM_IN_MODEM,
	[SST_IP_BT]		= SST_SWM_IN_BT,
	[SST_IP_CODEC0]		= SST_SWM_IN_CODEC0,
	[SST_IP_CODEC1]		= SST_SWM_IN_CODEC1,
	[SST_IP_LOOP0]		= SST_SWM_IN_SPROT_LOOP,
	[SST_IP_LOOP1]		= SST_SWM_IN_MEDIA_LOOP1,
	[SST_IP_LOOP2]		= SST_SWM_IN_MEDIA_LOOP2,
	[SST_IP_SIDETONE]	= SST_SWM_IN_SIDETONE,
	[SST_IP_TXSPEECH]	= SST_SWM_IN_TXSPEECH,
	[SST_IP_SPEECH]		= SST_SWM_IN_SPEECH,
	[SST_IP_TONE]		= SST_SWM_IN_TONE,
	[SST_IP_VOIP]		= SST_SWM_IN_VOIP,
	[SST_IP_PCM0]		= SST_SWM_IN_PCM0,
	[SST_IP_PCM1]		= SST_SWM_IN_PCM1,
	[SST_IP_LOW_PCM0]	= SST_SWM_IN_LOW_PCM0,
	[SST_IP_FM]		= SST_SWM_IN_FM,
	[SST_IP_MEDIA0]		= SST_SWM_IN_MEDIA0,
	[SST_IP_MEDIA1]		= SST_SWM_IN_MEDIA1,
	[SST_IP_MEDIA2]		= SST_SWM_IN_MEDIA2,
	[SST_IP_MEDIA3]		= SST_SWM_IN_MEDIA3,
};

/**
 * fill_swm_input - fill in the SWM input ids given the register
 *
 * The register value is a bit-field inicated which mixer inputs are ON. Use the
 * lookup table to get the input-id and fill it in the structure.
 */
static int fill_swm_input(struct swm_input_ids *swm_input, unsigned int reg)
{
	uint i, is_set, nb_inputs = 0;
	u16 input_loc_id;

	pr_debug("%s: reg: %#x\n", __func__, reg);
	for (i = 0; i < SST_SWM_INPUT_COUNT; i++) {
		is_set = reg & BIT(i);
		if (!is_set)
			continue;

		input_loc_id = swm_mixer_input_ids[i];
		SST_FILL_DESTINATION(2, swm_input->input_id,
				     input_loc_id, SST_DEFAULT_MODULE_ID);
		nb_inputs++;
		swm_input++;
		pr_debug("input id: %#x, nb_inputs: %d\n", input_loc_id, nb_inputs);

		if (nb_inputs == SST_CMD_SWM_MAX_INPUTS) {
			pr_warn("%s: SET_SWM cmd max inputs reached", __func__);
			break;
		}
	}
	return nb_inputs;
}

static void sst_set_pipe_gain(struct sst_ids *ids, struct sst_data *sst, int mute)
{
	struct sst_gain_mixer_control *mc;
	struct sst_gain_value *gv;
	struct module *gain = NULL;

	list_for_each_entry(gain, &ids->gain_list, node) {
		struct snd_kcontrol *kctl = gain->kctl;

		pr_debug("control name=%s\n", kctl->id.name);
		mc = (void *)kctl->private_value;
		gv = mc->gain_val;

		sst_send_gain_cmd(sst, gv, mc->task_id,
				mc->pipe_id | mc->instance_id, mc->module_id, mute);
	}
}

static int sst_swm_mixer_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	struct sst_cmd_set_swm cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;
	bool set_mixer = false;
	int val = sst->widget[ids->reg];

	pr_debug("%s: widget = %s\n", __func__, w->name);
	pr_debug("%s: reg[%d] = %#x\n", __func__, ids->reg, val);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMD:
		set_mixer = true;
		break;
	case SND_SOC_DAPM_POST_REG:
		if (w->power)
			set_mixer = true;
		break;
	default:
		set_mixer = false;
	}

	if (set_mixer == false)
		return 0;

	if (SND_SOC_DAPM_EVENT_ON(event) ||
	    event == SND_SOC_DAPM_POST_REG)
		cmd.switch_state = SST_SWM_ON;
	else
		cmd.switch_state = SST_SWM_OFF;

	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);
	/* MMX_SET_SWM == SBA_SET_SWM */
	cmd.header.command_id = SBA_SET_SWM;

	SST_FILL_DESTINATION(2, cmd.output_id,
			     ids->location_id, SST_DEFAULT_MODULE_ID);
	cmd.nb_inputs =	fill_swm_input(&cmd.input[0], val);
	cmd.header.length = offsetof(struct sst_cmd_set_swm, input) - sizeof(struct sst_dsp_header)
				+ (cmd.nb_inputs * sizeof(cmd.input[0]));

	sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
			      ids->task_id, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);
	return 0;
}

/* SBA mixers - 16 inputs */
#define SST_SBA_DECLARE_MIX_CONTROLS(kctl_name, mixer_reg)			\
	static const struct snd_kcontrol_new kctl_name[] = {			\
		SOC_SINGLE_EXT("modem_in", mixer_reg, SST_IP_MODEM, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("bt_in", mixer_reg, SST_IP_BT, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("codec_in0", mixer_reg, SST_IP_CODEC0, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("codec_in1", mixer_reg, SST_IP_CODEC1, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("sprot_loop_in", mixer_reg, SST_IP_LOOP0, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("media_loop1_in", mixer_reg, SST_IP_LOOP1, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("media_loop2_in", mixer_reg, SST_IP_LOOP2, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("sidetone_in", mixer_reg, SST_IP_SIDETONE, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("txspeech_in", mixer_reg, SST_IP_TXSPEECH, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("speech_in", mixer_reg, SST_IP_SPEECH, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("tone_in", mixer_reg, SST_IP_TONE, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("voip_in", mixer_reg, SST_IP_VOIP, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("pcm0_in", mixer_reg, SST_IP_PCM0, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("pcm1_in", mixer_reg, SST_IP_PCM1, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("low_pcm0_in", mixer_reg, SST_IP_LOW_PCM0, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("fm_in", mixer_reg, SST_IP_FM, 1, 0,		\
				sst_mix_get, sst_mix_put),			\
	}

#define SST_SBA_MIXER_GRAPH_MAP(mix_name)			\
	{ mix_name, "modem_in",		"modem_in" },		\
	{ mix_name, "bt_in",		"bt_in" },		\
	{ mix_name, "codec_in0",	"codec_in0" },		\
	{ mix_name, "codec_in1",	"codec_in1" },		\
	{ mix_name, "sprot_loop_in",	"sprot_loop_in" },	\
	{ mix_name, "media_loop1_in",	"media_loop1_in" },	\
	{ mix_name, "media_loop2_in",	"media_loop2_in" },	\
	{ mix_name, "sidetone_in",	"sidetone_in" },	\
	{ mix_name, "txspeech_in",	"txspeech_in" },	\
	{ mix_name, "speech_in",	"speech_in" },		\
	{ mix_name, "tone_in",		"tone_in" },		\
	{ mix_name, "voip_in",		"voip_in" },		\
	{ mix_name, "pcm0_in",		"pcm0_in" },		\
	{ mix_name, "pcm1_in",		"pcm1_in" },		\
	{ mix_name, "low_pcm0_in",	"low_pcm0_in" },	\
	{ mix_name, "fm_in",		"fm_in" }

#define SST_MMX_DECLARE_MIX_CONTROLS(kctl_name, mixer_reg)			\
	static const struct snd_kcontrol_new kctl_name[] = {			\
		SOC_SINGLE_EXT("media0_in", mixer_reg, SST_IP_MEDIA0, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("media1_in", mixer_reg, SST_IP_MEDIA1, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("media2_in", mixer_reg, SST_IP_MEDIA2, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
		SOC_SINGLE_EXT("media3_in", mixer_reg, SST_IP_MEDIA3, 1, 0,	\
				sst_mix_get, sst_mix_put),			\
	}

SST_MMX_DECLARE_MIX_CONTROLS(sst_mix_media0_controls, SST_MIX_MEDIA0);
SST_MMX_DECLARE_MIX_CONTROLS(sst_mix_media1_controls, SST_MIX_MEDIA1);

/* 18 SBA mixers */
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_pcm0_controls, SST_MIX_PCM0);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_pcm1_controls, SST_MIX_PCM1);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_pcm2_controls, SST_MIX_PCM2);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_sprot_l0_controls, SST_MIX_LOOP0);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_media_l1_controls, SST_MIX_LOOP1);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_media_l2_controls, SST_MIX_LOOP2);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_voip_controls, SST_MIX_VOIP);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_aware_controls, SST_MIX_AWARE);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_vad_controls, SST_MIX_VAD);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_hf_sns_controls, SST_MIX_HF_SNS);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_hf_controls, SST_MIX_HF);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_speech_controls, SST_MIX_SPEECH);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_rxspeech_controls, SST_MIX_RXSPEECH);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_codec0_controls, SST_MIX_CODEC0);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_codec1_controls, SST_MIX_CODEC1);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_bt_controls, SST_MIX_BT);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_fm_controls, SST_MIX_FM);
SST_SBA_DECLARE_MIX_CONTROLS(sst_mix_modem_controls, SST_MIX_MODEM);

void sst_handle_vb_timer(struct snd_soc_platform *p, bool enable)
{
	struct sst_cmd_generic cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(p);
	static int timer_usage;

	if (enable)
		cmd.header.command_id = SBA_VB_START;
	else
		cmd.header.command_id = SBA_IDLE;
	pr_debug("%s: enable=%u, usage=%d\n", __func__, enable, timer_usage);

	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);
	cmd.header.length = 0;

	if (enable)
		sst_dsp->ops->power(true);

	mutex_lock(&sst->lock);
	if (enable)
		timer_usage++;
	else
		timer_usage--;

	/* Send the command only if this call is the first enable or last
	 * disable
	 */
	if ((enable && (timer_usage == 1)) ||
	    (!enable && (timer_usage == 0))) {

		if (sst_fill_and_send_cmd_unlocked(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				SST_TASK_SBA, 0, &cmd, sizeof(cmd.header) + cmd.header.length) == 0) {

			if (sst_dsp->ops->set_generic_params(SST_SET_MONITOR_LPE,
									(void *)&enable) != 0)
				pr_err("%s: failed to set recovery timer\n", __func__);
		} else
			pr_err("%s: failed to send sst cmd %d\n",
						 __func__, cmd.header.command_id);

	}
	mutex_unlock(&sst->lock);

	if (!enable)
		sst_dsp->ops->power(false);
}

void send_ssp_cmd(struct snd_soc_platform *platform, const char *id, bool enable)
{
	struct sst_cmd_sba_hw_set_ssp cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	unsigned int domain, mux;
	int domain_shift, mux_shift, ssp_no;
	const struct sst_ssp_config *config;

	pr_err("Enter:%s, enable=%d port_name=%s\n", __func__, enable, id);

	if (strcmp(id, "ssp0-port") == 0)
		ssp_no = SST_SSP0;
	else if (strcmp(id, "ssp1-port") == 0)
		ssp_no = SST_SSP1;
	else if (strcmp(id, "ssp2-port") == 0)
		ssp_no = SST_SSP2;
	else
		return;

	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);
	cmd.header.command_id = SBA_HW_SET_SSP;
	cmd.header.length = sizeof(struct sst_cmd_sba_hw_set_ssp)
				- sizeof(struct sst_dsp_header);
	mux_shift = sst->pdata->mux_shift[ssp_no];
	mux = (mux_shift == -1) ? 0 : get_mux_state(sst, SST_MUX_REG, mux_shift);
	domain_shift = sst->pdata->domain_shift[ssp_no][mux];
	domain = (domain_shift == -1) ? 0 : get_mux_state(sst, SST_MUX_REG, domain_shift);

	config = &(sst->pdata->ssp_config)[ssp_no][mux][domain];
	pr_debug("%s: ssp_id: %u, mux: %d, domain: %d\n", __func__,
		 config->ssp_id, mux, domain);

	if (enable)
		cmd.switch_state = SST_SWITCH_ON;
	else
		cmd.switch_state = SST_SWITCH_OFF;

	cmd.selection = config->ssp_id;
	cmd.nb_bits_per_slots = config->bits_per_slot;
	cmd.nb_slots = config->slots;
	cmd.mode = config->ssp_mode | (config->pcm_mode << 1);
	cmd.duplex = config->duplex;
	cmd.active_tx_slot_map = config->active_slot_map;
	cmd.active_rx_slot_map = config->active_slot_map;
	cmd.frame_sync_frequency = config->fs_frequency;
	cmd.frame_sync_polarity = SSP_FS_ACTIVE_HIGH;
	cmd.data_polarity = config->data_polarity;
	cmd.frame_sync_width = config->fs_width;
	cmd.ssp_protocol = config->ssp_protocol;
	cmd.start_delay = config->start_delay;
	cmd.reserved1 = cmd.reserved2 = 0xFF;

	sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				SST_TASK_SBA, 0, &cmd,
				sizeof(cmd.header) + cmd.header.length);
}

static int sst_set_be_modules(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *k, int event)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);

	pr_debug("Enter: %s, widget=%s\n", __func__, w->name);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		sst_send_slot_map(sst);
		sst_send_pipe_module_params(w);
	}
	return 0;
}

/**
 * sst_set_speech_path - send SPEECH_UL/DL enable/disable IPC
 *
 * The SPEECH_PATH IPC enables more than one pipeline (speech uplink, downlink,
 * sidetone etc.). Since the command should be sent only once, use a refcount to
 * send the command only on first enable/last disable.
 */
static int sst_set_speech_path(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	struct sst_cmd_set_speech_path cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	bool is_wideband;
	static int speech_active;

	pr_debug("%s: widget=%s\n", __func__, w->name);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		speech_active++;
		cmd.switch_state = SST_SWITCH_ON;
	} else {
		speech_active--;
		cmd.switch_state = SST_SWITCH_OFF;
	}

	SST_FILL_DEFAULT_DESTINATION(cmd.header.dst);

	cmd.header.command_id = SBA_VB_SET_SPEECH_PATH;
	cmd.header.length = sizeof(struct sst_cmd_set_speech_path)
				- sizeof(struct sst_dsp_header);
	cmd.config.cfg.s_length = 0;
	cmd.config.cfg.rate = 0;		/* 8 khz */
	cmd.config.cfg.format = 0;

	is_wideband = get_mux_state(sst, SST_MUX_REG, SST_VOICE_MODE_SHIFT);
	if (is_wideband)
		cmd.config.cfg.rate = 1;	/* 16 khz */

	if ((SND_SOC_DAPM_EVENT_ON(event) && (speech_active == 1)) ||
	    (SND_SOC_DAPM_EVENT_OFF(event) && (speech_active == 0)))
		sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				SST_TASK_SBA, 0, &cmd,
				sizeof(cmd.header) + cmd.header.length);

	if (SND_SOC_DAPM_EVENT_ON(event))
		sst_send_pipe_module_params(w);
	return 0;
}

/**
 * sst_set_linked_pipe - send gain/algo for a linked input/output
 *
 * A linked pipe is dependent on the power status of its parent widget since it
 * itself does not have any enabling command.
 */
static int sst_set_linked_pipe(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *k, int event)
{
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;
	pr_debug("%s: widget=%s\n", __func__, w->name);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (ids->parent_w && ids->parent_w->power)
			sst_find_and_send_pipe_algo(sst, w->name, ids);
			sst_set_pipe_gain(ids, sst, 0);
	}
	return 0;
}

static int sst_set_media_path(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *k, int event)
{
	struct sst_cmd_set_media_path cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;

	pr_debug("%s: widget=%s\n", __func__, w->name);
	pr_debug("%s: task=%u, location=%#x\n", __func__,
				ids->task_id, ids->location_id);

	if (SND_SOC_DAPM_EVENT_ON(event))
		cmd.switch_state = SST_PATH_ON;
	else
		cmd.switch_state = SST_PATH_OFF;

	SST_FILL_DESTINATION(2, cmd.header.dst,
			     ids->location_id, SST_DEFAULT_MODULE_ID);

	/* MMX_SET_MEDIA_PATH == SBA_SET_MEDIA_PATH */
	cmd.header.command_id = MMX_SET_MEDIA_PATH;
	cmd.header.length = sizeof(struct sst_cmd_set_media_path)
				- sizeof(struct sst_dsp_header);

	sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
			      ids->task_id, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);

	if (SND_SOC_DAPM_EVENT_ON(event))
		sst_send_pipe_module_params(w);
	return 0;
}

static int sst_set_media_loop(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	struct sst_cmd_sba_set_media_loop_map cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;

	pr_debug("Enter:%s, widget=%s\n", __func__, w->name);
	if (SND_SOC_DAPM_EVENT_ON(event))
		cmd.switch_state = SST_SWITCH_ON;
	else
		cmd.switch_state = SST_SWITCH_OFF;

	SST_FILL_DESTINATION(2, cmd.header.dst,
			     ids->location_id, SST_DEFAULT_MODULE_ID);

	cmd.header.command_id = SBA_SET_MEDIA_LOOP_MAP;
	cmd.header.length = sizeof(struct sst_cmd_sba_set_media_loop_map)
				 - sizeof(struct sst_dsp_header);
	cmd.param.part.cfg.rate = 2; /* 48khz */

	cmd.param.part.cfg.format = ids->format; /* stereo/Mono */
	cmd.param.part.cfg.s_length = 1; /* 24bit left justified*/
	cmd.map = 0; /* Algo sequence: Gain - DRP - FIR - IIR  */

	sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
			      SST_TASK_SBA, 0, &cmd,
			      sizeof(cmd.header) + cmd.header.length);
	if (SND_SOC_DAPM_EVENT_ON(event))
		sst_send_pipe_module_params(w);
	return 0;
}

static int sst_tone_generator_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *k, int event)
{
	struct sst_cmd_tone_stop cmd;
	struct sst_data *sst = snd_soc_platform_get_drvdata(w->platform);
	struct sst_ids *ids = w->priv;

	pr_debug("Enter:%s, widget=%s\n", __func__, w->name);
	/* in case of tone generator, the params are combined with the ON cmd */
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		int len;
		struct module *algo;
		struct sst_algo_control *bc;
		struct sst_cmd_set_params *cmd;

		algo = list_first_entry(&ids->algo_list, struct module, node);
		if (algo == NULL)
			return -EINVAL;
		bc = (void *)algo->kctl->private_value;
		len = sizeof(cmd->dst) + sizeof(cmd->command_id) + bc->max;

		cmd = kzalloc(len, GFP_KERNEL);
		if (cmd == NULL) {
			pr_err("Failed to send cmd, kzalloc failed\n");
			return -ENOMEM;
		}

		SST_FILL_DESTINATION(2, cmd->dst, bc->pipe_id, bc->module_id);
		cmd->command_id = bc->cmd_id;
		memcpy(cmd->params, bc->params, bc->max);

		sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				      bc->task_id, 0, cmd, len);
		kfree(cmd);
		sst_set_pipe_gain(ids, sst, 0);
	} else {
		SST_FILL_DESTINATION(2, cmd.header.dst,
				     SST_PATH_INDEX_RESERVED, SST_MODULE_ID_TONE_GEN);

		cmd.header.command_id = SBA_VB_STOP_TONE;
		cmd.header.length = sizeof(struct sst_cmd_tone_stop)
					 - sizeof(struct sst_dsp_header);
		cmd.switch_state = SST_SWITCH_OFF;
		sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				      SST_TASK_SBA, 0, &cmd,
				      sizeof(cmd.header) + cmd.header.length);
	}
	return 0;
}

static int sst_send_probe_cmd(struct sst_data *sst, u16 probe_pipe_id,
			      int mode, int switch_state,
			      const struct sst_probe_config *probe_cfg)
{
	struct sst_cmd_probe cmd;

	memset(&cmd, 0, sizeof(cmd));

	SST_FILL_DESTINATION(3, cmd.header.dst, SST_DEFAULT_CELL_NBR,
			     probe_pipe_id, SST_DEFAULT_MODULE_ID);
	cmd.header.command_id = SBA_PROBE;
	cmd.header.length = sizeof(struct sst_cmd_probe)
				 - sizeof(struct sst_dsp_header);
	cmd.switch_state = switch_state;

	SST_FILL_DESTINATION(2, cmd.probe_dst,
			     probe_cfg->loc_id, probe_cfg->mod_id);

	cmd.shared_mem = 1;
	cmd.probe_in = 0;
	cmd.probe_out = 0;

	cmd.probe_mode = mode;
	cmd.cfg.s_length = probe_cfg->cfg.s_length;
	cmd.cfg.rate = probe_cfg->cfg.rate;
	cmd.cfg.format = probe_cfg->cfg.format;
	cmd.sm_buf_id = 1;

	return sst_fill_and_send_cmd(sst, SST_IPC_IA_CMD, SST_FLAG_BLOCKED,
				     probe_cfg->task_id, 0, &cmd,
				     sizeof(cmd.header) + cmd.header.length);
}

static const struct snd_kcontrol_new sst_probe_controls[];
static const struct sst_probe_config sst_probes[];

#define SST_MAX_PROBE_STREAMS 8
int sst_dpcm_probe_send(struct snd_soc_platform *platform, u16 probe_pipe_id,
			int substream, int direction, bool on)
{
	int switch_state = on ? SST_SWITCH_ON : SST_SWITCH_OFF;
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	const struct sst_probe_config *probe_cfg;
	struct sst_probe_value *probe_val;
	char *type;
	int offset;
	int mode;

	if (direction == SNDRV_PCM_STREAM_CAPTURE) {
		mode = SST_PROBE_EXTRACTOR;
		offset = 0;
		type = "extractor";
	} else {
		mode = SST_PROBE_INJECTOR;
		offset = SST_MAX_PROBE_STREAMS;
		type = "injector";
	}
	/* get the value of the probe connection kcontrol */
	probe_val = (void *)sst_probe_controls[substream + offset].private_value;
	probe_cfg = &sst_probes[probe_val->val];

	pr_debug("%s: substream=%d, direction=%d\n", __func__, substream, direction);
	pr_debug("%s: %s probe point at %s\n", __func__, type, probe_cfg->name);

	return sst_send_probe_cmd(sst, probe_pipe_id, mode, switch_state, probe_cfg);
}

/**
 * sst_alloc_hostless_stream - send ALLOC for a stream
 *
 * The stream does not send data to IA. The data is consumed by an internal
 * sink.
 */
static int sst_alloc_hostless_stream(const struct sst_pcm_format *pcm_params,
				     int str_id, uint pipe_id, uint task_id)
{
	struct snd_sst_stream_params param;
	struct snd_sst_params str_params = {0};
	struct snd_sst_alloc_params_ext alloc_params = {0};
	int ret_val = 0;

	memset(&param.uc.pcm_params, 0, sizeof(param.uc.pcm_params));
	param.uc.pcm_params.num_chan = pcm_params->channels_max;
	param.uc.pcm_params.pcm_wd_sz = pcm_params->sample_bits;
	param.uc.pcm_params.sfreq = pcm_params->rate_min;
	pr_debug("%s: sfreq= %d, wd_sz = %d\n", __func__,
		 param.uc.pcm_params.sfreq, param.uc.pcm_params.pcm_wd_sz);

	str_params.sparams = param;
	str_params.aparams = alloc_params;
	str_params.codec = SST_CODEC_TYPE_PCM;

	/* fill the pipe_id and stream id to pass to SST driver */
	str_params.stream_type = SST_STREAM_TYPE_MUSIC;
	str_params.stream_id = str_id;
	str_params.device_type = pipe_id;
	str_params.task = task_id;
	str_params.ops = STREAM_OPS_CAPTURE;

	ret_val = sst_dsp->ops->open(&str_params);
	pr_debug("platform prepare: stream open ret_val = 0x%x\n", ret_val);
	if (ret_val <= 0)
		return ret_val;

	return ret_val;
}

static int sst_hostless_stream_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct sst_ids *ids = w->priv;

#define MERR_DPCM_HOSTLESS_STRID 25
	if (SND_SOC_DAPM_EVENT_ON(event))
		/* ALLOC */
		/* FIXME: HACK - FW shouldn't require alloc for aware */
		return sst_alloc_hostless_stream(ids->pcm_fmt,
						 MERR_DPCM_HOSTLESS_STRID,
						 ids->location_id >> SST_PATH_ID_SHIFT,
						 ids->task_id);
	else
		/* FREE */
		return sst_dsp->ops->close(MERR_DPCM_HOSTLESS_STRID);
}

static const struct snd_kcontrol_new sst_mix_sw_aware =
	SOC_SINGLE_EXT("switch", SST_MIX_SWITCH, 0, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_mix_sw_vad =
	SOC_SINGLE_EXT("switch", SST_MIX_SWITCH, 0, 1, 0,
		sst_mix_get, sst_mix_put);

static const struct snd_kcontrol_new sst_vad_enroll[] = {
	SOC_SINGLE_BOOL_EXT("SST VTSV Enroll", 0, sst_vtsv_enroll_get,
					sst_vtsv_enroll_set),
};

static const struct snd_kcontrol_new sst_mix_sw_tone_gen =
	SOC_SINGLE_EXT("switch", SST_MIX_SWITCH, 1, 1, 0,
		sst_mix_get, sst_mix_put);
static const struct snd_kcontrol_new sst_vtsv_read[] = {
	SND_SOC_BYTES_EXT("vtsv event", VTSV_MAX_TOTAL_RESULT_ARRAY_SIZE,
		 sst_vtsv_event_get, NULL),

};
static const char * const sst_bt_fm_texts[] = {
	"fm", "bt",
};

static const struct snd_kcontrol_new sst_bt_fm_mux =
	SST_SSP_MUX_CTL("ssp1_out", 0, SST_MUX_REG, SST_BT_FM_MUX_SHIFT, sst_bt_fm_texts,
			sst_mux_get, sst_mux_put);

static const struct sst_pcm_format aware_stream_fmt = {
	.sample_bits = 24,
	.rate_min = 8000,
	.channels_max = 1,
};

static const struct sst_pcm_format vad_stream_fmt = {
	.sample_bits = 16,
	.rate_min = 16000,
	.channels_max = 1,
};

static const struct snd_soc_dapm_widget sst_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("tone"),
	SST_DAPM_OUTPUT("aware", SST_PATH_INDEX_AWARE_OUT, SST_TASK_AWARE, &aware_stream_fmt, sst_hostless_stream_event),
	SST_DAPM_OUTPUT("vad", SST_PATH_INDEX_VAD_OUT, SST_TASK_AWARE, &vad_stream_fmt, sst_hostless_stream_event),
	SST_AIF_IN("modem_in",  sst_set_be_modules),
	SST_AIF_IN("codec_in0", sst_set_be_modules),
	SST_AIF_IN("codec_in1", sst_set_be_modules),
	SST_AIF_IN("bt_fm_in", sst_set_be_modules),
	SST_AIF_OUT("modem_out", sst_set_be_modules),
	SST_AIF_OUT("codec_out0", sst_set_be_modules),
	SST_AIF_OUT("codec_out1", sst_set_be_modules),
	SST_AIF_OUT("bt_fm_out", sst_set_be_modules),

	/* Media Paths */
	/* MediaX IN paths are set via ALLOC, so no SET_MEDIA_PATH command */
	SST_PATH_INPUT("media0_in", SST_TASK_MMX, SST_SWM_IN_MEDIA0, sst_generic_modules_event),
	SST_PATH_INPUT("media1_in", SST_TASK_MMX, SST_SWM_IN_MEDIA1, NULL),
	SST_PATH_INPUT("media2_in", SST_TASK_MMX, SST_SWM_IN_MEDIA2, sst_set_media_path),
	SST_PATH_INPUT("media3_in", SST_TASK_MMX, SST_SWM_IN_MEDIA3, NULL),
	SST_PATH_OUTPUT("media0_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA0, sst_set_media_path),
	SST_PATH_OUTPUT("media1_out", SST_TASK_MMX, SST_SWM_OUT_MEDIA1, sst_set_media_path),

	/* SBA PCM Paths */
	SST_PATH_INPUT("pcm0_in", SST_TASK_SBA, SST_SWM_IN_PCM0, sst_set_media_path),
	SST_PATH_INPUT("pcm1_in", SST_TASK_SBA, SST_SWM_IN_PCM1, sst_set_media_path),
	SST_PATH_OUTPUT("pcm0_out", SST_TASK_SBA, SST_SWM_OUT_PCM0, sst_set_media_path),
	SST_PATH_OUTPUT("pcm1_out", SST_TASK_SBA, SST_SWM_OUT_PCM1, sst_set_media_path),
	SST_PATH_OUTPUT("pcm2_out", SST_TASK_SBA, SST_SWM_OUT_PCM2, sst_set_media_path),
	/* TODO: check if this needs SET_MEDIA_PATH command*/
	SST_PATH_INPUT("low_pcm0_in", SST_TASK_SBA, SST_SWM_IN_LOW_PCM0, NULL),

	SST_PATH_INPUT("voip_in", SST_TASK_SBA, SST_SWM_IN_VOIP, sst_set_media_path),
	SST_PATH_OUTPUT("voip_out", SST_TASK_SBA, SST_SWM_OUT_VOIP, sst_set_media_path),
	SST_PATH_OUTPUT("aware_out", SST_TASK_SBA, SST_SWM_OUT_AWARE, sst_set_media_path),
	SST_PATH_OUTPUT("vad_out", SST_TASK_SBA, SST_SWM_OUT_VAD, sst_set_media_path),

	/* SBA Loops */
	SST_PATH_INPUT("sprot_loop_in", SST_TASK_SBA, SST_SWM_IN_SPROT_LOOP, NULL),
	SST_PATH_INPUT("media_loop1_in", SST_TASK_SBA, SST_SWM_IN_MEDIA_LOOP1, NULL),
	SST_PATH_INPUT("media_loop2_in", SST_TASK_SBA, SST_SWM_IN_MEDIA_LOOP2, NULL),
	SST_PATH_MEDIA_LOOP_OUTPUT("sprot_loop_out", SST_TASK_SBA, SST_SWM_OUT_SPROT_LOOP, SST_FMT_MONO, sst_set_media_loop),
	SST_PATH_MEDIA_LOOP_OUTPUT("media_loop1_out", SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP1, SST_FMT_MONO, sst_set_media_loop),
	SST_PATH_MEDIA_LOOP_OUTPUT("media_loop2_out", SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP2, SST_FMT_STEREO, sst_set_media_loop),

	SST_PATH_INPUT("tone_in", SST_TASK_SBA, SST_SWM_IN_TONE, sst_tone_generator_event),

	SST_PATH_LINKED_INPUT("bt_in", SST_TASK_SBA, SST_SWM_IN_BT, "bt_fm_in", sst_set_linked_pipe),
	SST_PATH_LINKED_INPUT("fm_in", SST_TASK_SBA, SST_SWM_IN_FM, "bt_fm_in", sst_set_linked_pipe),
	SST_PATH_LINKED_OUTPUT("bt_out", SST_TASK_SBA, SST_SWM_OUT_BT, "bt_fm_out", sst_set_linked_pipe),
	SST_PATH_LINKED_OUTPUT("fm_out", SST_TASK_SBA, SST_SWM_OUT_FM, "bt_fm_out", sst_set_linked_pipe),

	/* SBA Voice Paths */
	SST_PATH_LINKED_INPUT("sidetone_in", SST_TASK_SBA, SST_SWM_IN_SIDETONE, "speech_out", sst_set_linked_pipe),
	SST_PATH_INPUT("speech_in", SST_TASK_SBA, SST_SWM_IN_SPEECH, sst_set_speech_path),
	SST_PATH_INPUT("txspeech_in", SST_TASK_SBA, SST_SWM_IN_TXSPEECH, sst_set_speech_path),
	SST_PATH_OUTPUT("hf_sns_out", SST_TASK_SBA, SST_SWM_OUT_HF_SNS, sst_set_speech_path),
	SST_PATH_OUTPUT("hf_out", SST_TASK_SBA, SST_SWM_OUT_HF, sst_set_speech_path),
	SST_PATH_OUTPUT("speech_out", SST_TASK_SBA, SST_SWM_OUT_SPEECH, sst_set_speech_path),
	SST_PATH_OUTPUT("rxspeech_out", SST_TASK_SBA, SST_SWM_OUT_RXSPEECH, sst_set_speech_path),

	/* Media Mixers */
	SST_SWM_MIXER("media0_out mix 0", SST_MIX_MEDIA0, SST_TASK_MMX, SST_SWM_OUT_MEDIA0,
		      sst_mix_media0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media1_out mix 0", SST_MIX_MEDIA1, SST_TASK_MMX, SST_SWM_OUT_MEDIA1,
		      sst_mix_media1_controls, sst_swm_mixer_event),

	/* SBA PCM mixers */
	SST_SWM_MIXER("pcm0_out mix 0", SST_MIX_PCM0, SST_TASK_SBA, SST_SWM_OUT_PCM0,
		      sst_mix_pcm0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("pcm1_out mix 0", SST_MIX_PCM1, SST_TASK_SBA, SST_SWM_OUT_PCM1,
		      sst_mix_pcm1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("pcm2_out mix 0", SST_MIX_PCM2, SST_TASK_SBA, SST_SWM_OUT_PCM2,
		      sst_mix_pcm2_controls, sst_swm_mixer_event),

	/* SBA Loop mixers */
	SST_SWM_MIXER("sprot_loop_out mix 0", SST_MIX_LOOP0, SST_TASK_SBA, SST_SWM_OUT_SPROT_LOOP,
		      sst_mix_sprot_l0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media_loop1_out mix 0", SST_MIX_LOOP1, SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP1,
		      sst_mix_media_l1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("media_loop2_out mix 0", SST_MIX_LOOP2, SST_TASK_SBA, SST_SWM_OUT_MEDIA_LOOP2,
		      sst_mix_media_l2_controls, sst_swm_mixer_event),

	SST_SWM_MIXER("voip_out mix 0", SST_MIX_VOIP, SST_TASK_SBA, SST_SWM_OUT_VOIP,
		      sst_mix_voip_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("aware_out mix 0", SST_MIX_AWARE, SST_TASK_SBA, SST_SWM_OUT_AWARE,
		      sst_mix_aware_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("vad_out mix 0", SST_MIX_VAD, SST_TASK_SBA, SST_SWM_OUT_VAD,
		      sst_mix_vad_controls, sst_swm_mixer_event),

	/* SBA Voice mixers */
	SST_SWM_MIXER("hf_sns_out mix 0", SST_MIX_HF_SNS, SST_TASK_SBA, SST_SWM_OUT_HF_SNS,
		      sst_mix_hf_sns_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("hf_out mix 0", SST_MIX_HF, SST_TASK_SBA, SST_SWM_OUT_HF,
		      sst_mix_hf_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("speech_out mix 0", SST_MIX_SPEECH, SST_TASK_SBA, SST_SWM_OUT_SPEECH,
		      sst_mix_speech_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("rxspeech_out mix 0", SST_MIX_RXSPEECH, SST_TASK_SBA, SST_SWM_OUT_RXSPEECH,
		      sst_mix_rxspeech_controls, sst_swm_mixer_event),

	/* SBA Backend mixers */
	SST_SWM_MIXER("codec_out0 mix 0", SST_MIX_CODEC0, SST_TASK_SBA, SST_SWM_OUT_CODEC0,
		      sst_mix_codec0_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("codec_out1 mix 0", SST_MIX_CODEC1, SST_TASK_SBA, SST_SWM_OUT_CODEC1,
		      sst_mix_codec1_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("bt_out mix 0", SST_MIX_BT, SST_TASK_SBA, SST_SWM_OUT_BT,
		      sst_mix_bt_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("fm_out mix 0", SST_MIX_FM, SST_TASK_SBA, SST_SWM_OUT_FM,
		      sst_mix_fm_controls, sst_swm_mixer_event),
	SST_SWM_MIXER("modem_out mix 0", SST_MIX_MODEM, SST_TASK_SBA, SST_SWM_OUT_MODEM,
		      sst_mix_modem_controls, sst_swm_mixer_event),

	SND_SOC_DAPM_MUX("ssp1_out mux 0", SND_SOC_NOPM, 0, 0, &sst_bt_fm_mux),
	SND_SOC_DAPM_SWITCH("aware_out aware 0", SND_SOC_NOPM, 0, 0, &sst_mix_sw_aware),
	SND_SOC_DAPM_SWITCH("vad_out vad 0", SND_SOC_NOPM, 0, 0, &sst_mix_sw_vad),

	SND_SOC_DAPM_SWITCH("tone_in tone_generator 0", SND_SOC_NOPM, 0, 0, &sst_mix_sw_tone_gen),
};

static const struct snd_soc_dapm_route intercon[] = {
	{"media1_in", NULL, "Headset Playback"},
	{"media2_in", NULL, "pcm0_out"},

	{"media0_out mix 0", "media1_in", "media1_in"},
	{"media0_out mix 0", "media2_in", "media2_in"},
	{"media1_out mix 0", "media1_in", "media1_in"},
	{"media1_out mix 0", "media2_in", "media2_in"},

	{"media0_out", NULL, "media0_out mix 0"},
	{"media1_out", NULL, "media1_out mix 0"},
	{"pcm0_in", NULL, "media0_out"},
	{"pcm1_in", NULL, "media1_out"},

	{"Headset Capture", NULL, "pcm1_out"},
	{"Headset Capture", NULL, "pcm2_out"},
	{"pcm0_out", NULL, "pcm0_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("pcm0_out mix 0"),
	{"pcm1_out", NULL, "pcm1_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("pcm1_out mix 0"),
	{"pcm2_out", NULL, "pcm2_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("pcm2_out mix 0"),

	{"media_loop1_in", NULL, "media_loop1_out"},
	{"media_loop1_out", NULL, "media_loop1_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("media_loop1_out mix 0"),
	{"media_loop2_in", NULL, "media_loop2_out"},
	{"media_loop2_out", NULL, "media_loop2_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("media_loop2_out mix 0"),
	{"sprot_loop_in", NULL, "sprot_loop_out"},
	{"sprot_loop_out", NULL, "sprot_loop_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("sprot_loop_out mix 0"),

	{"voip_in", NULL, "VOIP Playback"},
	{"VOIP Capture", NULL, "voip_out"},
	{"voip_out", NULL, "voip_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("voip_out mix 0"),

	{"aware", NULL, "aware_out"},
	{"aware_out", NULL, "aware_out aware 0"},
	{"aware_out aware 0", "switch", "aware_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("aware_out mix 0"),
	{"vad", NULL, "vad_out"},
	{"vad_out", NULL, "vad_out vad 0"},
	{"vad_out vad 0", "switch", "vad_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("vad_out mix 0"),

	{"codec_out0", NULL, "codec_out0 mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("codec_out0 mix 0"),
	{"codec_out1", NULL, "codec_out1 mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("codec_out1 mix 0"),
	{"modem_out", NULL, "modem_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("modem_out mix 0"),

	{"bt_fm_out", NULL, "ssp1_out mux 0"},
	{"ssp1_out mux 0", "bt", "bt_out"},
	{"ssp1_out mux 0", "fm", "fm_out"},
	{"bt_out", NULL, "bt_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("bt_out mix 0"),
	{"fm_out", NULL, "fm_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("fm_out mix 0"),
	{"bt_in", NULL, "bt_fm_in"},
	{"fm_in", NULL, "bt_fm_in"},

	/* Uplink processing */
	{"txspeech_in", NULL, "hf_sns_out"},
	{"txspeech_in", NULL, "hf_out"},
	{"txspeech_in", NULL, "speech_out"},
	{"sidetone_in", NULL, "speech_out"},

	{"hf_sns_out", NULL, "hf_sns_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("hf_sns_out mix 0"),
	{"hf_out", NULL, "hf_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("hf_out mix 0"),
	{"speech_out", NULL, "speech_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("speech_out mix 0"),

	/* Downlink processing */
	{"speech_in", NULL, "rxspeech_out"},
	{"rxspeech_out", NULL, "rxspeech_out mix 0"},
	SST_SBA_MIXER_GRAPH_MAP("rxspeech_out mix 0"),

	{"tone_in", NULL, "tone_in tone_generator 0"},
	{"tone_in tone_generator 0", "switch", "tone"},

	/* TODO: add sidetone inputs */
	/* TODO: add Low Latency stream support */
};

static const char * const sst_nb_wb_texts[] = {
	"narrowband", "wideband",
};

static const struct snd_kcontrol_new sst_mux_controls[] = {
	SST_SSP_MUX_CTL("domain voice mode", 0, SST_MUX_REG, SST_VOICE_MODE_SHIFT, sst_nb_wb_texts,
			sst_mode_get, sst_mode_put),
	SST_SSP_MUX_CTL("domain bt mode", 0, SST_MUX_REG, SST_BT_MODE_SHIFT, sst_nb_wb_texts,
			sst_mode_get, sst_mode_put),
};

static const char * const slot_names[] = {
	"none",
	"slot 0", "slot 1", "slot 2", "slot 3",
	"slot 4", "slot 5", "slot 6", "slot 7", /* not supported by FW */
};

static const char * const channel_names[] = {
	"none",
	"codec_out0_0", "codec_out0_1", "codec_out1_0", "codec_out1_1",
	"codec_out2_0", "codec_out2_1", "codec_out3_0", "codec_out3_1", /* not supported by FW */
};

#define SST_INTERLEAVER(xpname, slot_name, slotno) \
	SST_SSP_SLOT_CTL(xpname, "interleaver", slot_name, slotno, true, \
			 channel_names, sst_slot_get, sst_slot_put)

#define SST_DEINTERLEAVER(xpname, channel_name, channel_no) \
	SST_SSP_SLOT_CTL(xpname, "deinterleaver", channel_name, channel_no, false, \
			 slot_names, sst_slot_get, sst_slot_put)

static const struct snd_kcontrol_new sst_slot_controls[] = {
	SST_INTERLEAVER("codec_out", "slot 0", 0),
	SST_INTERLEAVER("codec_out", "slot 1", 1),
	SST_INTERLEAVER("codec_out", "slot 2", 2),
	SST_INTERLEAVER("codec_out", "slot 3", 3),
	SST_DEINTERLEAVER("codec_in", "codec_in0_0", 0),
	SST_DEINTERLEAVER("codec_in", "codec_in0_1", 1),
	SST_DEINTERLEAVER("codec_in", "codec_in1_0", 2),
	SST_DEINTERLEAVER("codec_in", "codec_in1_1", 3),
};

#include "probe_point_dpcm.c"

/* initialized based on names in sst_probes array */
static const char *sst_probe_enum_texts[ARRAY_SIZE(sst_probes)];
static const SOC_ENUM_SINGLE_EXT_DECL(sst_probe_enum, sst_probe_enum_texts);

#define SST_PROBE_CTL(name, num)						\
	SST_PROBE_ENUM(SST_PROBE_CTL_NAME(name, num, "connection"),		\
		       sst_probe_enum, sst_probe_get, sst_probe_put)
	/* TODO: implement probe gains
	SOC_SINGLE_EXT_TLV(SST_PROBE_CTL_NAME(name, num, "gains"), xreg, xshift,
		xmax, xinv, xget, xput, sst_gain_tlv_common)
	*/

static const struct snd_kcontrol_new sst_probe_controls[] = {
	SST_PROBE_CTL("probe out", 0),
	SST_PROBE_CTL("probe out", 1),
	SST_PROBE_CTL("probe out", 2),
	SST_PROBE_CTL("probe out", 3),
	SST_PROBE_CTL("probe out", 4),
	SST_PROBE_CTL("probe out", 5),
	SST_PROBE_CTL("probe out", 6),
	SST_PROBE_CTL("probe out", 7),
	SST_PROBE_CTL("probe in", 0),
	SST_PROBE_CTL("probe in", 1),
	SST_PROBE_CTL("probe in", 2),
	SST_PROBE_CTL("probe in", 3),
	SST_PROBE_CTL("probe in", 4),
	SST_PROBE_CTL("probe in", 5),
	SST_PROBE_CTL("probe in", 6),
	SST_PROBE_CTL("probe in", 7),
};

/* Gain helper with min/max set */
#define SST_GAIN(name, path_id, task_id, instance, gain_var)				\
	SST_GAIN_KCONTROLS(name, "gain", SST_GAIN_MIN_VALUE, SST_GAIN_MAX_VALUE,	\
		SST_GAIN_TC_MIN, SST_GAIN_TC_MAX,					\
		sst_gain_get, sst_gain_put,						\
		SST_MODULE_ID_GAIN_CELL, path_id, instance, task_id,			\
		sst_gain_tlv_common, gain_var)

#define SST_VOLUME(name, path_id, task_id, instance, gain_var)				\
	SST_GAIN_KCONTROLS(name, "volume", SST_GAIN_MIN_VALUE, SST_GAIN_MAX_VALUE,	\
		SST_GAIN_TC_MIN, SST_GAIN_TC_MAX,					\
		sst_gain_get, sst_gain_put,						\
		SST_MODULE_ID_VOLUME, path_id, instance, task_id,			\
		sst_gain_tlv_common, gain_var)

#define SST_NUM_GAINS 36
static struct sst_gain_value sst_gains[SST_NUM_GAINS];

static const struct snd_kcontrol_new sst_gain_controls[] = {
	SST_GAIN("media0_in", SST_PATH_INDEX_MEDIA0_IN, SST_TASK_MMX, 0, &sst_gains[0]),
	SST_GAIN("media1_in", SST_PATH_INDEX_MEDIA1_IN, SST_TASK_MMX, 0, &sst_gains[1]),
	SST_GAIN("media2_in", SST_PATH_INDEX_MEDIA2_IN, SST_TASK_MMX, 0, &sst_gains[2]),
	SST_GAIN("media3_in", SST_PATH_INDEX_MEDIA3_IN, SST_TASK_MMX, 0, &sst_gains[3]),

	SST_GAIN("pcm0_in", SST_PATH_INDEX_PCM0_IN, SST_TASK_SBA, 0, &sst_gains[4]),
	SST_GAIN("pcm1_in", SST_PATH_INDEX_PCM1_IN, SST_TASK_SBA, 0, &sst_gains[5]),
	SST_GAIN("low_pcm0_in", SST_PATH_INDEX_LOW_PCM0_IN, SST_TASK_SBA, 0, &sst_gains[6]),
	SST_GAIN("pcm1_out", SST_PATH_INDEX_PCM1_OUT, SST_TASK_SBA, 0, &sst_gains[7]),
	SST_GAIN("pcm2_out", SST_PATH_INDEX_PCM2_OUT, SST_TASK_SBA, 0, &sst_gains[8]),

	SST_GAIN("voip_in", SST_PATH_INDEX_VOIP_IN, SST_TASK_SBA, 0, &sst_gains[9]),
	SST_GAIN("voip_out", SST_PATH_INDEX_VOIP_OUT, SST_TASK_SBA, 0, &sst_gains[10]),
	SST_GAIN("tone_in", SST_PATH_INDEX_TONE_IN, SST_TASK_SBA, 0, &sst_gains[11]),

	SST_GAIN("aware_out", SST_PATH_INDEX_AWARE_OUT, SST_TASK_SBA, 0, &sst_gains[12]),
	SST_GAIN("vad_out", SST_PATH_INDEX_VAD_OUT, SST_TASK_SBA, 0, &sst_gains[13]),

	SST_GAIN("hf_sns_out", SST_PATH_INDEX_HF_SNS_OUT, SST_TASK_SBA, 0, &sst_gains[14]),
	SST_GAIN("hf_out", SST_PATH_INDEX_HF_OUT, SST_TASK_SBA, 0, &sst_gains[15]),
	SST_GAIN("speech_out", SST_PATH_INDEX_SPEECH_OUT, SST_TASK_SBA, 0, &sst_gains[16]),
	SST_GAIN("txspeech_in", SST_PATH_INDEX_TX_SPEECH_IN, SST_TASK_SBA, 0, &sst_gains[17]),
	SST_GAIN("rxspeech_out", SST_PATH_INDEX_RX_SPEECH_OUT, SST_TASK_SBA, 0, &sst_gains[18]),
	SST_GAIN("speech_in", SST_PATH_INDEX_SPEECH_IN, SST_TASK_SBA, 0, &sst_gains[19]),

	SST_GAIN("codec_in0", SST_PATH_INDEX_CODEC_IN0, SST_TASK_SBA, 0, &sst_gains[20]),
	SST_GAIN("codec_in1", SST_PATH_INDEX_CODEC_IN1, SST_TASK_SBA, 0, &sst_gains[21]),
	SST_GAIN("codec_out0", SST_PATH_INDEX_CODEC_OUT0, SST_TASK_SBA, 0, &sst_gains[22]),
	SST_GAIN("codec_out1", SST_PATH_INDEX_CODEC_OUT1, SST_TASK_SBA, 0, &sst_gains[23]),
	SST_GAIN("bt_out", SST_PATH_INDEX_BT_OUT, SST_TASK_SBA, 0, &sst_gains[24]),
	SST_GAIN("fm_out", SST_PATH_INDEX_FM_OUT, SST_TASK_SBA, 0, &sst_gains[25]),
	SST_GAIN("bt_in", SST_PATH_INDEX_BT_IN, SST_TASK_SBA, 0, &sst_gains[26]),
	SST_GAIN("fm_in", SST_PATH_INDEX_FM_IN, SST_TASK_SBA, 0, &sst_gains[27]),
	SST_GAIN("modem_in", SST_PATH_INDEX_MODEM_IN, SST_TASK_SBA, 0, &sst_gains[28]),
	SST_GAIN("modem_out", SST_PATH_INDEX_MODEM_OUT, SST_TASK_SBA, 0, &sst_gains[29]),
	SST_GAIN("media_loop1_out", SST_PATH_INDEX_MEDIA_LOOP1_OUT, SST_TASK_SBA, 0, &sst_gains[30]),
	SST_GAIN("media_loop2_out", SST_PATH_INDEX_MEDIA_LOOP2_OUT, SST_TASK_SBA, 0, &sst_gains[31]),
	SST_GAIN("sprot_loop_out", SST_PATH_INDEX_SPROT_LOOP_OUT, SST_TASK_SBA, 0, &sst_gains[32]),
	SST_VOLUME("media0_in", SST_PATH_INDEX_MEDIA0_IN, SST_TASK_MMX, 0, &sst_gains[33]),
	SST_GAIN("sidetone_in", SST_PATH_INDEX_SIDETONE_IN, SST_TASK_SBA, 0, &sst_gains[34]),
	SST_GAIN("speech_out", SST_PATH_INDEX_SPEECH_OUT, SST_TASK_FBA_UL, 1, &sst_gains[35]),
};

static const struct snd_kcontrol_new sst_algo_controls[] = {
	SST_ALGO_KCONTROL_BYTES("media_loop1_out", "fir", 272, SST_MODULE_ID_FIR_24,
		 SST_PATH_INDEX_MEDIA_LOOP1_OUT, 0, SST_TASK_SBA, SBA_VB_SET_FIR),
	SST_ALGO_KCONTROL_BYTES("media_loop1_out", "iir", 300, SST_MODULE_ID_IIR_24,
		SST_PATH_INDEX_MEDIA_LOOP1_OUT, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("media_loop1_out", "mdrp", 286, SST_MODULE_ID_MDRP,
		SST_PATH_INDEX_MEDIA_LOOP1_OUT, 0, SST_TASK_SBA, SBA_SET_MDRP),
	SST_ALGO_KCONTROL_BYTES("media_loop2_out", "fir", 272, SST_MODULE_ID_FIR_24,
		SST_PATH_INDEX_MEDIA_LOOP2_OUT, 0, SST_TASK_SBA, SBA_VB_SET_FIR),
	SST_ALGO_KCONTROL_BYTES("media_loop2_out", "iir", 300, SST_MODULE_ID_IIR_24,
		SST_PATH_INDEX_MEDIA_LOOP2_OUT, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("media_loop2_out", "mdrp", 286, SST_MODULE_ID_MDRP,
		SST_PATH_INDEX_MEDIA_LOOP2_OUT, 0, SST_TASK_SBA, SBA_SET_MDRP),
	SST_ALGO_KCONTROL_BYTES("aware_out", "fir", 272, SST_MODULE_ID_FIR_24,
		SST_PATH_INDEX_AWARE_OUT, 0, SST_TASK_SBA, SBA_VB_SET_FIR),
	SST_ALGO_KCONTROL_BYTES("aware_out", "iir", 300, SST_MODULE_ID_IIR_24,
		SST_PATH_INDEX_AWARE_OUT, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("aware_out", "aware", 48, SST_MODULE_ID_CONTEXT_ALGO_AWARE,
		SST_PATH_INDEX_AWARE_OUT, 0, SST_TASK_AWARE, AWARE_ENV_CLASS_PARAMS),
	SST_ALGO_KCONTROL_BYTES("vad_out", "fir", 272, SST_MODULE_ID_FIR_24,
		SST_PATH_INDEX_VAD_OUT, 0, SST_TASK_SBA, SBA_VB_SET_FIR),
	SST_ALGO_KCONTROL_BYTES("vad_out", "iir", 300, SST_MODULE_ID_IIR_24,
		SST_PATH_INDEX_VAD_OUT, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("vad_out", "vad", 28, SST_MODULE_ID_ALGO_VTSV,
		SST_PATH_INDEX_VAD_OUT, 0, SST_TASK_AWARE, VAD_ENV_CLASS_PARAMS),
	SST_ALGO_KCONTROL_BYTES("sprot_loop_out", "lpro", 192, SST_MODULE_ID_SPROT,
		SST_PATH_INDEX_SPROT_LOOP_OUT, 0, SST_TASK_SBA, SBA_VB_LPRO),
	SST_ALGO_KCONTROL_BYTES("modem_in", "dcr", 60, SST_MODULE_ID_FILT_DCR,
		SST_PATH_INDEX_MODEM_IN, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("bt_in", "dcr", 60, SST_MODULE_ID_FILT_DCR,
		SST_PATH_INDEX_BT_IN, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("codec_in0", "dcr", 52, SST_MODULE_ID_FILT_DCR,
		SST_PATH_INDEX_CODEC_IN0, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("codec_in1", "dcr", 52, SST_MODULE_ID_FILT_DCR,
		SST_PATH_INDEX_CODEC_IN1, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	SST_ALGO_KCONTROL_BYTES("fm_in", "dcr", 60, SST_MODULE_ID_FILT_DCR,
		SST_PATH_INDEX_FM_IN, 0, SST_TASK_SBA, SBA_VB_SET_IIR),
	/* Uplink */
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "fir_speech", 134, SST_MODULE_ID_FIR_16,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_FIR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "fir_hf_sns", 134, SST_MODULE_ID_FIR_16,
		SST_PATH_INDEX_HF_SNS_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_FIR | (0x0001<<11)),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "iir_speech", 46, SST_MODULE_ID_IIR_16,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_IIR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "iir_hf_sns", 46, SST_MODULE_ID_IIR_16,
		SST_PATH_INDEX_HF_SNS_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_IIR | (0x0001<<11)),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "aec", 642, SST_MODULE_ID_AEC,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_AEC),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "nr", 38, SST_MODULE_ID_NR,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_NR_UL),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "agc", 62, SST_MODULE_ID_AGC,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_AGC),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "compr", 100, SST_MODULE_ID_DRP,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_DUAL_BAND_COMP),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "ser", 44, SST_MODULE_ID_SER,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SER),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "cni", 48, SST_MODULE_ID_CNI_TX,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_TX_CNI),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "ref", 24, SST_MODULE_ID_REF_LINE,
		SST_PATH_INDEX_HF_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_REF_LINE),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "delay", 6, SST_MODULE_ID_EDL,
		SST_PATH_INDEX_HF_OUT, 0, SST_TASK_FBA_UL, FBA_VB_SET_DELAY_LINE),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "bmf", 572, SST_MODULE_ID_BMF,
		SST_PATH_INDEX_HF_SNS_OUT, 0, SST_TASK_FBA_UL, FBA_VB_BMF),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "dnr", 56, SST_MODULE_ID_DNR,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_DNR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "wnr", 64, SST_MODULE_ID_WNR,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_WNR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "tnr", 38, SST_MODULE_ID_TNR,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_TNR_UL),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_out", "ul_module", "nlf", 236, SST_MODULE_ID_NLF,
		SST_PATH_INDEX_SPEECH_OUT, 0, SST_TASK_FBA_UL, FBA_VB_NLF),

	/* Downlink */
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "ana", 52, SST_MODULE_ID_ANA,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_ANA),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "fir", 134, SST_MODULE_ID_FIR_16,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_SET_FIR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "iir", 46, SST_MODULE_ID_IIR_16,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_SET_IIR),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "nr", 38, SST_MODULE_ID_NR,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_NR_DL),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "compr", 100, SST_MODULE_ID_DRP,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_DUAL_BAND_COMP),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "cni", 28, SST_MODULE_ID_CNI,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_RX_CNI),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "bwx", 54, SST_MODULE_ID_BWX,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_BWX),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "gmm", 586, SST_MODULE_ID_BWX,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_GMM),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "glc", 18, SST_MODULE_ID_GLC,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_GLC),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "tnr", 38, SST_MODULE_ID_TNR,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_TNR_DL),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "slv", 34, SST_MODULE_ID_SLV,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_SLV),
	SST_COMBO_ALGO_KCONTROL_BYTES("speech_in", "dl_module", "mdrp", 134, SST_MODULE_ID_MDRP,
		SST_PATH_INDEX_SPEECH_IN, 0, SST_TASK_FBA_DL, FBA_VB_MDRP),

	/* Tone Generator */
	SST_ALGO_KCONTROL_BYTES("tone_in", "tone_generator", 116, SST_MODULE_ID_TONE_GEN,
		SST_PATH_INDEX_RESERVED, 0, SST_TASK_SBA, SBA_VB_START_TONE),

	/* Sidetone */
	SST_ALGO_KCONTROL_BYTES("sidetone_in", "iir", 300, SST_MODULE_ID_IIR_24,
		SST_PATH_INDEX_SIDETONE_IN, 0, SST_TASK_SBA, SBA_VB_SET_IIR),

};

static const struct snd_kcontrol_new sst_debug_controls[] = {
	SND_SOC_BYTES_EXT("sst debug byte control", SST_MAX_BIN_BYTES,
		       sst_byte_control_get, sst_byte_control_set),
};

static inline bool is_sst_dapm_widget(struct snd_soc_dapm_widget *w)
{
	if ((w->id == snd_soc_dapm_pga) ||
	    (w->id == snd_soc_dapm_aif_in) ||
	    (w->id == snd_soc_dapm_aif_out) ||
	    (w->id == snd_soc_dapm_input) ||
	    (w->id == snd_soc_dapm_output) ||
	    (w->id == snd_soc_dapm_mixer))
		return true;
	else
		return false;
}

/**
 * sst_send_pipe_gains - send gains for the front-end DAIs
 *
 * The gains in the pipes connected to the front-ends are muted/unmuted
 * automatically via the digital_mute() DAPM callback. This function sends the
 * gains for the front-end pipes.
 */
int sst_send_pipe_gains(struct snd_soc_dai *dai, int stream, int mute)
{
	struct snd_soc_platform *platform = dai->platform;
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_path *p = NULL;

	pr_debug("%s: enter, dai-name=%s dir=%d\n", __func__, dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("Stream name=%s\n", dai->playback_widget->name);
		w = dai->playback_widget;
		list_for_each_entry(p, &w->sinks, list_source) {
			if (p->connected && !p->connected(w, p->sink))
				continue;

			if (p->connect && p->sink->power && is_sst_dapm_widget(p->sink)) {
				struct sst_ids *ids = p->sink->priv;

				pr_debug("send gains for widget=%s\n", p->sink->name);
				sst_set_pipe_gain(ids, sst, mute);
			}
		}
	} else {
		pr_debug("Stream name=%s\n", dai->capture_widget->name);
		w = dai->capture_widget;
		list_for_each_entry(p, &w->sources, list_sink) {
			if (p->connected && !p->connected(w, p->sink))
				continue;

			if (p->connect &&  p->source->power && is_sst_dapm_widget(p->source)) {
				struct sst_ids *ids = p->source->priv;

				pr_debug("send gain for widget=%s\n", p->source->name);
				sst_set_pipe_gain(ids, sst, mute);
			}
		}
	}
	return 0;
}

/**
 * sst_fill_module_list - populate the list of modules/gains for a pipe
 *
 *
 * Fills the widget pointer in the kcontrol private data, and also fills the
 * kcontrol pointer in the widget private data.
 *
 * Widget pointer is used to send the algo/gain in the .put() handler if the
 * widget is powerd on.
 *
 * Kcontrol pointer is used to send the algo/gain in the widget power ON/OFF
 * event handler. Each widget (pipe) has multiple algos stored in the algo_list.
 */
static int sst_fill_module_list(struct snd_kcontrol *kctl,
	 struct snd_soc_dapm_widget *w, int type)
{
	struct module *module = NULL;
	struct sst_ids *ids = w->priv;

	module = devm_kzalloc(w->platform->dev, sizeof(*module), GFP_KERNEL);
	if (!module) {
		pr_err("kzalloc block failed\n");
		return -ENOMEM;
	}

	if (type == SST_MODULE_GAIN) {
		struct sst_gain_mixer_control *mc = (void *)kctl->private_value;

		mc->w = w;
		module->kctl = kctl;
		list_add_tail(&module->node, &ids->gain_list);
	} else if (type == SST_MODULE_ALGO) {
		struct sst_algo_control *bc = (void *)kctl->private_value;

		bc->w = w;
		module->kctl = kctl;
		list_add_tail(&module->node, &ids->algo_list);
	}

	return 0;
}

/**
 * sst_fill_widget_module_info - fill list of gains/algos for the pipe
 * @widget:	pipe modelled as a DAPM widget
 *
 * Fill the list of gains/algos for the widget by looking at all the card
 * controls and comparing the name of the widget with the first part of control
 * name. First part of control name contains the pipe name (widget name).
 */
static int sst_fill_widget_module_info(struct snd_soc_dapm_widget *w,
	struct snd_soc_platform *platform)
{
	struct snd_kcontrol *kctl;
	int index, ret = 0;
	struct snd_card *card = platform->card->snd_card;
	char *idx;

	down_read(&card->controls_rwsem);

	list_for_each_entry(kctl, &card->controls, list) {
		idx = strstr(kctl->id.name, " ");
		if (idx == NULL)
			continue;
		index  = strlen(kctl->id.name) - strlen(idx);
		if (strstr(kctl->id.name, "volume") &&
		    !strncmp(kctl->id.name, w->name, index))
			ret = sst_fill_module_list(kctl, w, SST_MODULE_GAIN);
		else if (strstr(kctl->id.name, "params") &&
			 !strncmp(kctl->id.name, w->name, index))
			ret = sst_fill_module_list(kctl, w, SST_MODULE_ALGO);
		else if (strstr(kctl->id.name, "mute") &&
			 !strncmp(kctl->id.name, w->name, index)) {
			struct sst_gain_mixer_control *mc = (void *)kctl->private_value;
			mc->w = w;
		} else if (strstr(kctl->id.name, "interleaver") &&
			 !strncmp(kctl->id.name, w->name, index)) {
			struct sst_enum *e = (void *)kctl->private_value;
			e->w = w;
		} else if (strstr(kctl->id.name, "deinterleaver") &&
			 !strncmp(kctl->id.name, w->name, index)) {
			struct sst_enum *e = (void *)kctl->private_value;
			e->w = w;
		}
		if (ret < 0) {
			up_read(&card->controls_rwsem);
			return ret;
		}
	}
	up_read(&card->controls_rwsem);
	return 0;
}

/**
 * sst_fill_linked_widgets - fill the parent pointer for the linked widget
 */
static void sst_fill_linked_widgets(struct snd_soc_platform *platform,
						struct sst_ids *ids)
{
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_context *dapm = &platform->dapm;

	unsigned int len = strlen(ids->parent_wname);
	list_for_each_entry(w, &dapm->card->widgets, list) {
		if (!strncmp(ids->parent_wname, w->name, len)) {
			ids->parent_w = w;
			break;
		}
	}
}

/**
 * sst_map_modules_to_pipe - fill algo/gains list for all pipes
 */
static int sst_map_modules_to_pipe(struct snd_soc_platform *platform)
{
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_context *dapm = &platform->dapm;
	int ret = 0;

	list_for_each_entry(w, &dapm->card->widgets, list) {
		if (w->platform && is_sst_dapm_widget(w) && (w->priv)) {
			struct sst_ids *ids = w->priv;

			pr_debug("widget type=%d name=%s\n", w->id, w->name);
			INIT_LIST_HEAD(&ids->algo_list);
			INIT_LIST_HEAD(&ids->gain_list);
			ret = sst_fill_widget_module_info(w, platform);
			if (ret < 0)
				return ret;
			/* fill linked widgets */
			if (ids->parent_wname !=  NULL)
				sst_fill_linked_widgets(platform, ids);
		}
	}
	return 0;
}

int sst_dsp_init_v2_dpcm(struct snd_soc_platform *platform)
{
	int i, ret = 0;
	struct sst_data *sst = snd_soc_platform_get_drvdata(platform);

	sst->byte_stream = devm_kzalloc(platform->dev,
					SST_MAX_BIN_BYTES, GFP_KERNEL);
	if (!sst->byte_stream) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	sst->widget = devm_kzalloc(platform->dev,
				   SST_NUM_WIDGETS * sizeof(*sst->widget),
				   GFP_KERNEL);
	if (!sst->widget) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	snd_soc_dapm_new_controls(&platform->dapm, sst_dapm_widgets,
			ARRAY_SIZE(sst_dapm_widgets));
	snd_soc_dapm_add_routes(&platform->dapm, intercon,
			ARRAY_SIZE(intercon));
	snd_soc_dapm_new_widgets(platform->dapm.card);

	for (i = 0; i < SST_NUM_GAINS; i++) {
		sst_gains[i].mute = SST_GAIN_MUTE_DEFAULT;
		sst_gains[i].l_gain = SST_GAIN_VOLUME_DEFAULT;
		sst_gains[i].r_gain = SST_GAIN_VOLUME_DEFAULT;
		sst_gains[i].ramp_duration = SST_GAIN_RAMP_DURATION_DEFAULT;
	}

	snd_soc_add_platform_controls(platform, sst_gain_controls,
			ARRAY_SIZE(sst_gain_controls));

	snd_soc_add_platform_controls(platform, sst_algo_controls,
			ARRAY_SIZE(sst_algo_controls));
	snd_soc_add_platform_controls(platform, sst_slot_controls,
			ARRAY_SIZE(sst_slot_controls));
	snd_soc_add_platform_controls(platform, sst_mux_controls,
			ARRAY_SIZE(sst_mux_controls));
	snd_soc_add_platform_controls(platform, sst_debug_controls,
			ARRAY_SIZE(sst_debug_controls));
	snd_soc_add_platform_controls(platform, sst_vad_enroll,
			ARRAY_SIZE(sst_vad_enroll));
	snd_soc_add_platform_controls(platform, sst_vtsv_read,
			ARRAY_SIZE(sst_vtsv_read));

	/* initialize the names of the probe points */
	for (i = 0; i < ARRAY_SIZE(sst_probes); i++)
		sst_probe_enum_texts[i] = sst_probes[i].name;

	snd_soc_add_platform_controls(platform, sst_probe_controls,
			ARRAY_SIZE(sst_probe_controls));

	ret = sst_map_modules_to_pipe(platform);

	return ret;
}
