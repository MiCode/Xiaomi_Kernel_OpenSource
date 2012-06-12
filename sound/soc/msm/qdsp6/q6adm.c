/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <mach/qdsp6v2/audio_acdb.h>
#include <mach/qdsp6v2/rtac.h>

#include <sound/apr_audio.h>
#include <sound/q6afe.h>

#define TIMEOUT_MS 1000
#define AUDIO_RX 0x0
#define AUDIO_TX 0x1

#define ASM_MAX_SESSION 0x8 /* To do: define in a header */
#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF

struct adm_ctl {
	void *apr;
	atomic_t copp_id[AFE_MAX_PORTS];
	atomic_t copp_cnt[AFE_MAX_PORTS];
	atomic_t copp_stat[AFE_MAX_PORTS];
	wait_queue_head_t wait;
	int  ec_ref_rx;
};

static struct acdb_cal_block mem_addr_audproc[MAX_AUDPROC_TYPES];
static struct acdb_cal_block mem_addr_audvol[MAX_AUDPROC_TYPES];

static struct adm_ctl			this_adm;


int srs_trumedia_open(int port_id, int srs_tech_id, void *srs_params)
{
	struct asm_pp_params_command *open = NULL;
	int ret = 0, sz = 0;
	int index;

	pr_debug("SRS - %s", __func__);
	switch (srs_tech_id) {
	case SRS_ID_GLOBAL: {
		struct srs_trumedia_params_GLOBAL *glb_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_GLOBAL);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_GLOBAL) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS;
		open->params.param_size =
				sizeof(struct srs_trumedia_params_GLOBAL);
		glb_params = (struct srs_trumedia_params_GLOBAL *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(glb_params, srs_params,
			sizeof(struct srs_trumedia_params_GLOBAL));
		pr_debug("SRS - %s: Global params - 1 = %x, 2 = %x, 3 = %x,"
				" 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x\n",
				__func__, (int)glb_params->v1,
				(int)glb_params->v2, (int)glb_params->v3,
				(int)glb_params->v4, (int)glb_params->v5,
				(int)glb_params->v6, (int)glb_params->v7,
				(int)glb_params->v8);
		break;
	}
	case SRS_ID_WOWHD: {
		struct srs_trumedia_params_WOWHD *whd_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_WOWHD);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_WOWHD) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS_WOWHD;
		open->params.param_size =
				sizeof(struct srs_trumedia_params_WOWHD);
		whd_params = (struct srs_trumedia_params_WOWHD *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(whd_params, srs_params,
				sizeof(struct srs_trumedia_params_WOWHD));
		pr_debug("SRS - %s: WOWHD params - 1 = %x, 2 = %x, 3 = %x,"
			" 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x, 9 = %x,"
			" 10 = %x, 11 = %x\n", __func__, (int)whd_params->v1,
			(int)whd_params->v2, (int)whd_params->v3,
			(int)whd_params->v4, (int)whd_params->v5,
			(int)whd_params->v6, (int)whd_params->v7,
			(int)whd_params->v8, (int)whd_params->v9,
			(int)whd_params->v10, (int)whd_params->v11);
		break;
	}
	case SRS_ID_CSHP: {
		struct srs_trumedia_params_CSHP *chp_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_CSHP);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_CSHP) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS_CSHP;
		open->params.param_size =
					sizeof(struct srs_trumedia_params_CSHP);
		chp_params = (struct srs_trumedia_params_CSHP *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(chp_params, srs_params,
				sizeof(struct srs_trumedia_params_CSHP));
		pr_debug("SRS - %s: CSHP params - 1 = %x, 2 = %x, 3 = %x,"
				" 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x,"
				" 9 = %x\n", __func__, (int)chp_params->v1,
				(int)chp_params->v2, (int)chp_params->v3,
				(int)chp_params->v4, (int)chp_params->v5,
				(int)chp_params->v6, (int)chp_params->v7,
				(int)chp_params->v8, (int)chp_params->v9);
		break;
	}
	case SRS_ID_HPF: {
		struct srs_trumedia_params_HPF *hpf_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_HPF);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_HPF) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS_HPF;
		open->params.param_size =
					sizeof(struct srs_trumedia_params_HPF);
		hpf_params = (struct srs_trumedia_params_HPF *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(hpf_params, srs_params,
			sizeof(struct srs_trumedia_params_HPF));
		pr_debug("SRS - %s: HPF params - 1 = %x\n", __func__,
				(int)hpf_params->v1);
		break;
	}
	case SRS_ID_PEQ: {
		struct srs_trumedia_params_PEQ *peq_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_PEQ);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_PEQ) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS_PEQ;
		open->params.param_size =
					sizeof(struct srs_trumedia_params_PEQ);
		peq_params = (struct srs_trumedia_params_PEQ *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(peq_params, srs_params,
				sizeof(struct srs_trumedia_params_PEQ));
		pr_debug("SRS - %s: PEQ params - 1 = %x 2 = %x, 3 = %x,"
			" 4 = %x\n", __func__, (int)peq_params->v1,
			(int)peq_params->v2, (int)peq_params->v3,
			(int)peq_params->v4);
		break;
	}
	case SRS_ID_HL: {
		struct srs_trumedia_params_HL *hl_params = NULL;
		sz = sizeof(struct asm_pp_params_command) +
			sizeof(struct srs_trumedia_params_HL);
		open = kzalloc(sz, GFP_KERNEL);
		open->payload_size = sizeof(struct srs_trumedia_params_HL) +
					sizeof(struct asm_pp_param_data_hdr);
		open->params.param_id = SRS_TRUMEDIA_PARAMS_HL;
		open->params.param_size = sizeof(struct srs_trumedia_params_HL);
		hl_params = (struct srs_trumedia_params_HL *)((u8 *)open +
				sizeof(struct asm_pp_params_command));
		memcpy(hl_params, srs_params,
				sizeof(struct srs_trumedia_params_HL));
		pr_debug("SRS - %s: HL params - 1 = %x, 2 = %x, 3 = %x, 4 = %x,"
				" 5 = %x, 6 = %x, 7 = %x\n", __func__,
				(int)hl_params->v1, (int)hl_params->v2,
				(int)hl_params->v3, (int)hl_params->v4,
				(int)hl_params->v5, (int)hl_params->v6,
				(int)hl_params->v7);
		break;
	}
	default:
		goto fail_cmd;
	}

	open->payload = NULL;
	open->params.module_id = SRS_TRUMEDIA_MODULE_ID;
	open->params.reserved = 0;
	open->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	open->hdr.pkt_size = sz;
	open->hdr.src_svc = APR_SVC_ADM;
	open->hdr.src_domain = APR_DOMAIN_APPS;
	open->hdr.src_port = port_id;
	open->hdr.dest_svc = APR_SVC_ADM;
	open->hdr.dest_domain = APR_DOMAIN_ADSP;
	index = afe_get_port_index(port_id);
	open->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	open->hdr.token = port_id;
	open->hdr.opcode = ADM_CMD_SET_PARAMS;
	pr_debug("SRS - %s: Command was sent now check Q6 - port id = %d,"
			" size %d, module id %x, param id %x.\n", __func__,
			open->hdr.dest_port, open->payload_size,
			open->params.module_id, open->params.param_id);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)open);
	if (ret < 0) {
		pr_err("SRS - %s: ADM enable for port %d failed\n", __func__,
			port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait, 1,
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("SRS - %s: ADM open failed for port %d\n", __func__,
			port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	kfree(open);
	return ret;
}

static int32_t adm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;
	int i, index;
	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("adm_callback: Reset event is received: %d %d apr[%p]\n",
				data->reset_event, data->reset_proc,
				this_adm.apr);
		if (this_adm.apr) {
			apr_reset(this_adm.apr);
			for (i = 0; i < AFE_MAX_PORTS; i++) {
				atomic_set(&this_adm.copp_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_cnt[i], 0);
				atomic_set(&this_adm.copp_stat[i], 0);
			}
			this_adm.apr = NULL;
		}
		return 0;
	}

	pr_debug("%s: code = 0x%x %x %x size = %d\n", __func__,
			data->opcode, payload[0], payload[1],
					data->payload_size);

	if (data->payload_size) {
		index = afe_get_port_index(data->token);
		pr_debug("%s: Port ID %d, index %d\n", __func__,
			data->token, index);
		if (index < 0 || index >= AFE_MAX_PORTS) {
			pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, data->token);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("APR_BASIC_RSP_RESULT id %x\n", payload[0]);
			switch (payload[0]) {
			case ADM_CMD_SET_PARAMS:
				if (rtac_make_adm_callback(payload,
						data->payload_size))
					break;
			case ADM_CMD_COPP_CLOSE:
			case ADM_CMD_MEMORY_MAP:
			case ADM_CMD_MEMORY_UNMAP:
			case ADM_CMD_MEMORY_MAP_REGIONS:
			case ADM_CMD_MEMORY_UNMAP_REGIONS:
			case ADM_CMD_MATRIX_MAP_ROUTINGS:
			case ADM_CMD_CONNECT_AFE_PORT:
			case ADM_CMD_DISCONNECT_AFE_PORT:
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait);
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_COPP_OPEN:
		case ADM_CMDRSP_MULTI_CHANNEL_COPP_OPEN: {
			struct adm_copp_open_respond *open = data->payload;
			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait);
				break;
			}
			atomic_set(&this_adm.copp_id[index], open->copp_id);
			atomic_set(&this_adm.copp_stat[index], 1);
			pr_debug("%s: coppid rxed=%d\n", __func__,
							open->copp_id);
			wake_up(&this_adm.wait);
			}
			break;
		case ADM_CMDRSP_GET_PARAMS:
			pr_debug("%s: ADM_CMDRSP_GET_PARAMS\n", __func__);
			rtac_make_adm_callback(payload,
				data->payload_size);
			break;
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
							data->opcode);
			break;
		}
	}
	return 0;
}

static int send_adm_cal_block(int port_id, struct acdb_cal_block *aud_cal)
{
	s32				result = 0;
	struct adm_set_params_command	adm_params;
	int index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %d\n",
				__func__, index, port_id);
		return 0;
	}

	pr_debug("%s: Port id %d, index %d\n", __func__, port_id, index);

	if (!aud_cal || aud_cal->cal_size == 0) {
		pr_debug("%s: No ADM cal to send for port_id = %d!\n",
			__func__, port_id);
		result = -EINVAL;
		goto done;
	}

	adm_params.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_params));
	adm_params.hdr.src_svc = APR_SVC_ADM;
	adm_params.hdr.src_domain = APR_DOMAIN_APPS;
	adm_params.hdr.src_port = port_id;
	adm_params.hdr.dest_svc = APR_SVC_ADM;
	adm_params.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params.hdr.token = port_id;
	adm_params.hdr.opcode = ADM_CMD_SET_PARAMS;
	adm_params.payload = aud_cal->cal_paddr;
	adm_params.payload_size = aud_cal->cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending SET_PARAMS payload = 0x%x, size = %d\n",
		__func__, adm_params.payload, adm_params.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_params);
	if (result < 0) {
		pr_err("%s: Set params failed port = %d payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait,
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out port = %d, payload = 0x%x\n",
			__func__, port_id, aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}

	result = 0;
done:
	return result;
}

static void send_adm_cal(int port_id, int path)
{
	int			result = 0;
	s32			acdb_path;
	struct acdb_cal_block	aud_cal;

	pr_debug("%s\n", __func__);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;

	pr_debug("%s: Sending audproc cal\n", __func__);
	get_audproc_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	if (((mem_addr_audproc[acdb_path].cal_paddr != aud_cal.cal_paddr)  &&
		(aud_cal.cal_size > 0)) ||
		(aud_cal.cal_size > mem_addr_audproc[acdb_path].cal_size)) {

		if (mem_addr_audproc[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(
				&mem_addr_audproc[acdb_path].cal_paddr,
				&mem_addr_audproc[acdb_path].cal_size, 1);

		result = adm_memory_map_regions(&aud_cal.cal_paddr, 0,
					&aud_cal.cal_size, 1);
		if (result < 0)
			pr_err("ADM audproc mmap did not work! path = %d, "
				"addr = 0x%x, size = %d\n", acdb_path,
				aud_cal.cal_paddr, aud_cal.cal_size);
		else
			mem_addr_audproc[acdb_path] = aud_cal;
	}

	if (!send_adm_cal_block(port_id, &aud_cal))
		pr_debug("%s: Audproc cal sent for port id: %d, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audproc cal not sent for port id: %d, path %d\n",
			__func__, port_id, acdb_path);

	pr_debug("%s: Sending audvol cal\n", __func__);
	get_audvol_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	if (((mem_addr_audvol[acdb_path].cal_paddr != aud_cal.cal_paddr)  &&
		(aud_cal.cal_size > 0))  ||
		(aud_cal.cal_size > mem_addr_audvol[acdb_path].cal_size)) {
		if (mem_addr_audvol[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(
				&mem_addr_audvol[acdb_path].cal_paddr,
				&mem_addr_audvol[acdb_path].cal_size, 1);

		result = adm_memory_map_regions(&aud_cal.cal_paddr, 0,
					&aud_cal.cal_size, 1);
		if (result < 0)
			pr_err("ADM audvol mmap did not work! path = %d, "
				"addr = 0x%x, size = %d\n", acdb_path,
				aud_cal.cal_paddr, aud_cal.cal_size);
		else
			mem_addr_audvol[acdb_path] = aud_cal;
	}

	if (!send_adm_cal_block(port_id, &aud_cal))
		pr_debug("%s: Audvol cal sent for port id: %d, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audvol cal not sent for port id: %d, path %d\n",
			__func__, port_id, acdb_path);
}

int adm_connect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port	cmd;
	int ret = 0;
	int index;

	pr_debug("%s: port %d session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}
	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = port_id;
	cmd.hdr.token = port_id;
	cmd.hdr.opcode = ADM_CMD_CONNECT_AFE_PORT;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s:ADM enable for port %d failed\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait,
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s ADM connect AFE failed for port %d\n", __func__,
							port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_disconnect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port	cmd;
	int ret = 0;
	int index;

	pr_debug("%s: port %d session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}
	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = port_id;
	cmd.hdr.token = port_id;
	cmd.hdr.opcode = ADM_CMD_DISCONNECT_AFE_PORT;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s:ADM enable for port %d failed\n",
					__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait,
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s ADM connect AFE failed for port %d\n", __func__,
							port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_dec(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode, int topology)
{
	struct adm_copp_open_command	open;
	int ret = 0;
	int index;

	pr_debug("%s: port %d path:%d rate:%d mode:%d\n", __func__,
				port_id, path, rate, channel_mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}


	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp_cnt[index]) == 0) {

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = port_id;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = port_id;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_COPP_OPEN;

		open.mode = path;
		open.endpoint_id1 = port_id;

		if (this_adm.ec_ref_rx == 0) {
			open.endpoint_id2 = 0xFFFF;
		} else if (this_adm.ec_ref_rx && (path != 1)) {
				open.endpoint_id2 = this_adm.ec_ref_rx;
				this_adm.ec_ref_rx = 0;
		}

		pr_debug("%s open.endpoint_id1:%d open.endpoint_id2:%d",
			__func__, open.endpoint_id1, open.endpoint_id2);
		/* convert path to acdb path */
		if (path == ADM_PATH_PLAYBACK)
			open.topology_id = get_adm_rx_topology();
		else {
			open.topology_id = get_adm_tx_topology();
			if ((open.topology_id ==
				VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
			    (open.topology_id ==
				VPM_TX_DM_FLUENCE_COPP_TOPOLOGY))
				rate = 16000;
		}

		if (open.topology_id  == 0)
			open.topology_id = topology;

		open.channel_config = channel_mode & 0x00FF;
		open.rate  = rate;

		pr_debug("%s: channel_config=%d port_id=%d rate=%d"
			"topology_id=0x%X\n", __func__, open.channel_config,\
			open.endpoint_id1, open.rate,\
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %d failed\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %d\n", __func__,
								port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}


int adm_multi_ch_copp_open(int port_id, int path, int rate, int channel_mode,
				int topology)
{
	struct adm_multi_ch_copp_open_command open;
	int ret = 0;
	int index;

	pr_debug("%s: port %d path:%d rate:%d channel :%d\n", __func__,
				port_id, path, rate, channel_mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	if (afe_validate_port(port_id) < 0) {
		pr_err("%s port idi[%d] is invalid\n", __func__, port_id);
		return -ENODEV;
	}

	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID %d, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	/* Create a COPP if port id are not enabled */
	if (atomic_read(&this_adm.copp_cnt[index]) == 0) {

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);

		open.hdr.pkt_size =
			sizeof(struct adm_multi_ch_copp_open_command);
		open.hdr.opcode = ADM_CMD_MULTI_CHANNEL_COPP_OPEN;
		memset(open.dev_channel_mapping, 0, 8);

		if (channel_mode == 1)	{
			open.dev_channel_mapping[0] = PCM_CHANNEL_FC;
		} else if (channel_mode == 2) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
		} else if (channel_mode == 4) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_RB;
			open.dev_channel_mapping[3] = PCM_CHANNEL_LB;
		} else if (channel_mode == 6) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[4] = PCM_CHANNEL_LB;
			open.dev_channel_mapping[5] = PCM_CHANNEL_RB;
		} else if (channel_mode == 8) {
			open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
			open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
			open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
			open.dev_channel_mapping[4] = PCM_CHANNEL_LB;
			open.dev_channel_mapping[5] = PCM_CHANNEL_RB;
			open.dev_channel_mapping[6] = PCM_CHANNEL_FLC;
			open.dev_channel_mapping[7] = PCM_CHANNEL_FRC;
		} else {
			pr_err("%s invalid num_chan %d\n", __func__,
					channel_mode);
			return -EINVAL;
		}


		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = port_id;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = port_id;
		open.hdr.token = port_id;

		open.mode = path;
		open.endpoint_id1 = port_id;

		if (this_adm.ec_ref_rx == 0) {
			open.endpoint_id2 = 0xFFFF;
		} else if (this_adm.ec_ref_rx && (path != 1)) {
				open.endpoint_id2 = this_adm.ec_ref_rx;
				this_adm.ec_ref_rx = 0;
		}

		pr_debug("%s open.endpoint_id1:%d open.endpoint_id2:%d",
			__func__, open.endpoint_id1, open.endpoint_id2);
		/* convert path to acdb path */
		if (path == ADM_PATH_PLAYBACK)
			open.topology_id = get_adm_rx_topology();
		else {
			open.topology_id = get_adm_tx_topology();
			if ((open.topology_id ==
				VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
			    (open.topology_id ==
				VPM_TX_DM_FLUENCE_COPP_TOPOLOGY))
				rate = 16000;
		}

		if (open.topology_id  == 0)
			open.topology_id = topology;

		open.channel_config = channel_mode & 0x00FF;
		open.rate  = rate;

		pr_debug("%s: channel_config=%d port_id=%d rate=%d"
			" topology_id=0x%X\n", __func__, open.channel_config,
			open.endpoint_id1, open.rate,
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s:ADM enable for port %d failed\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s ADM open failed for port %d\n", __func__,
								port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_matrix_map(int session_id, int path, int num_copps,
			unsigned int *port_id, int copp_id)
{
	struct adm_routings_command	route;
	int ret = 0, i = 0;
	/* Assumes port_ids have already been validated during adm_open */
	int index = afe_get_port_index(copp_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, copp_id);
		return 0;
	}

	pr_debug("%s: session 0x%x path:%d num_copps:%d port_id[0]:%d\n",
		 __func__, session_id, path, num_copps, port_id[0]);

	route.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route.hdr.pkt_size = sizeof(route);
	route.hdr.src_svc = 0;
	route.hdr.src_domain = APR_DOMAIN_APPS;
	route.hdr.src_port = copp_id;
	route.hdr.dest_svc = APR_SVC_ADM;
	route.hdr.dest_domain = APR_DOMAIN_ADSP;
	route.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	route.hdr.token = copp_id;
	route.hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS;
	route.num_sessions = 1;
	route.session[0].id = session_id;
	route.session[0].num_copps = num_copps;

	for (i = 0; i < num_copps; i++) {
		int tmp;
		port_id[i] = afe_convert_virtual_to_portid(port_id[i]);

		tmp = afe_get_port_index(port_id[i]);

		pr_debug("%s: port_id[%d]: %d, index: %d\n", __func__, i,
			 port_id[i], tmp);

		if (tmp >= 0 && tmp < AFE_MAX_PORTS)
			route.session[0].copp_id[i] =
					atomic_read(&this_adm.copp_id[tmp]);
	}
	if (num_copps % 2)
		route.session[0].copp_id[i] = 0;

	switch (path) {
	case 0x1:
		route.path = AUDIO_RX;
		break;
	case 0x2:
	case 0x3:
		route.path = AUDIO_TX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&route);
	if (ret < 0) {
		pr_err("%s: ADM routing for port %d failed\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait,
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM cmd Route failed for port %d\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}

	for (i = 0; i < num_copps; i++)
		send_adm_cal(port_id[i], path);

	for (i = 0; i < num_copps; i++)
		rtac_add_adm_device(port_id[i],	atomic_read(&this_adm.copp_id
			[afe_get_port_index(port_id[i])]),
			path, session_id);
	return 0;

fail_cmd:

	return ret;
}

int adm_memory_map_regions(uint32_t *buf_add, uint32_t mempool_id,
				uint32_t *bufsz, uint32_t bufcnt)
{
	struct  adm_cmd_memory_map_regions *mmap_regions = NULL;
	struct  adm_memory_map_regions *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;

	pr_debug("%s\n", __func__);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	cmd_size = sizeof(struct adm_cmd_memory_map_regions)
			+ sizeof(struct adm_memory_map_regions) * bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	mmap_regions = (struct adm_cmd_memory_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
	mmap_regions->hdr.opcode = ADM_CMD_MEMORY_MAP_REGIONS;
	mmap_regions->mempool_id = mempool_id & 0x00ff;
	mmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("%s: map_regions->nregions = %d\n", __func__,
				mmap_regions->nregions);
	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct adm_cmd_memory_map_regions));
	mregions = (struct adm_memory_map_regions *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->phys = buf_add[i];
		mregions->buf_size = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.copp_stat[0], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[0]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return ret;
}

int adm_memory_unmap_regions(uint32_t *buf_add, uint32_t *bufsz,
						uint32_t bufcnt)
{
	struct  adm_cmd_memory_unmap_regions *unmap_regions = NULL;
	struct  adm_memory_unmap_regions *mregions = NULL;
	void    *unmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;

	pr_debug("%s\n", __func__);

	if (this_adm.apr == NULL) {
		pr_err("%s APR handle NULL\n", __func__);
		return -EINVAL;
	}

	cmd_size = sizeof(struct adm_cmd_memory_unmap_regions)
			+ sizeof(struct adm_memory_unmap_regions) * bufcnt;

	unmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!unmap_region_cmd) {
		pr_err("%s: allocate unmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	unmap_regions = (struct adm_cmd_memory_unmap_regions *)
						unmap_region_cmd;
	unmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions->hdr.pkt_size = cmd_size;
	unmap_regions->hdr.src_port = 0;
	unmap_regions->hdr.dest_port = 0;
	unmap_regions->hdr.token = 0;
	unmap_regions->hdr.opcode = ADM_CMD_MEMORY_UNMAP_REGIONS;
	unmap_regions->nregions = bufcnt & 0x00ff;
	unmap_regions->reserved = 0;
	pr_debug("%s: unmap_regions->nregions = %d\n", __func__,
				unmap_regions->nregions);
	payload = ((u8 *) unmap_region_cmd +
			sizeof(struct adm_cmd_memory_unmap_regions));
	mregions = (struct adm_memory_unmap_regions *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->phys = buf_add[i];
		++mregions;
	}
	atomic_set(&this_adm.copp_stat[0], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) unmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait,
			atomic_read(&this_adm.copp_stat[0]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(unmap_region_cmd);
	return ret;
}

int adm_get_copp_id(int port_index)
{
	pr_debug("%s\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	return atomic_read(&this_adm.copp_id[port_index]);
}

void adm_ec_ref_rx_id(int  port_id)
{
	this_adm.ec_ref_rx = port_id;
	pr_debug("%s ec_ref_rx:%d", __func__, this_adm.ec_ref_rx);
}

int adm_close(int port_id)
{
	struct apr_hdr close;

	int ret = 0;
	int index = 0;

	port_id = afe_convert_virtual_to_portid(port_id);

	index = afe_get_port_index(port_id);
	if (afe_validate_port(port_id) < 0)
		return -EINVAL;

	pr_debug("%s port_id=%d index %d\n", __func__, port_id, index);

	if (!(atomic_read(&this_adm.copp_cnt[index]))) {
		pr_err("%s: copp count for port[%d]is 0\n", __func__, port_id);

		goto fail_cmd;
	}
	atomic_dec(&this_adm.copp_cnt[index]);
	if (!(atomic_read(&this_adm.copp_cnt[index]))) {

		close.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		close.pkt_size = sizeof(close);
		close.src_svc = APR_SVC_ADM;
		close.src_domain = APR_DOMAIN_APPS;
		close.src_port = port_id;
		close.dest_svc = APR_SVC_ADM;
		close.dest_domain = APR_DOMAIN_ADSP;
		close.dest_port = atomic_read(&this_adm.copp_id[index]);
		close.token = port_id;
		close.opcode = ADM_CMD_COPP_CLOSE;

		atomic_set(&this_adm.copp_id[index], RESET_COPP_ID);
		atomic_set(&this_adm.copp_stat[index], 0);


		pr_debug("%s:coppid %d portid=%d index=%d coppcnt=%d\n",
				__func__,
				atomic_read(&this_adm.copp_id[index]),
				port_id, index,
				atomic_read(&this_adm.copp_cnt[index]));

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s ADM close failed\n", __func__);
			ret = -EINVAL;
			goto fail_cmd;
		}

		ret = wait_event_timeout(this_adm.wait,
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route failed for port %d\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}

		rtac_remove_adm_device(port_id);
	}

fail_cmd:
	return ret;
}

static int __init adm_init(void)
{
	int i = 0;
	init_waitqueue_head(&this_adm.wait);
	this_adm.apr = NULL;

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		atomic_set(&this_adm.copp_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_cnt[i], 0);
		atomic_set(&this_adm.copp_stat[i], 0);
	}
	return 0;
}

device_initcall(adm_init);
