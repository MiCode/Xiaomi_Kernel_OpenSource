/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/completion.h>
#include "teei_id.h"
#include "teei_common.h"
#include "teei_smc_call.h"
#include "nt_smc_call.h"
#include "utdriver_macro.h"
#include "teei_log.h"
#include "notify_queue.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "teei_smc_struct.h"
#include "sched_status.h"
#include "teei_capi.h"
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct semaphore capi_mutex;

void set_sch_nq_cmd(void)
{
	struct message_head msg_head;

	memset((void *)(&msg_head), 0, sizeof(struct message_head));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = STANDARD_CALL_TYPE;
	msg_head.child_type = N_INVOKE_T_NQ_CMD;

	memcpy((void *)message_buff,
		(void *)(&msg_head), sizeof(struct message_head));

	Flush_Dcache_By_Area((unsigned long)message_buff,
			(unsigned long)message_buff + MESSAGE_SIZE);
}

/**
 * @brief
 *
 * @param cmd_addr phys address
 *
 * @return
 */

static u32 teei_smc(u32 cmd, unsigned long cmd_addr, int size, int valid_flag)
{
	unsigned long smc_type = 2;

	add_nq_entry(cmd, cmd_addr, size, valid_flag);
	set_sch_nq_cmd();

	smc_type = teei_secure_call(N_INVOKE_T_NQ, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
	return 0;
}

static struct completion wait_completion;

struct new_api_call_param {
	u32 cmd;
	unsigned long cmd_addr;
	int size;
};

void notify_smc_completed(void)
{
	complete(&wait_completion);
}

int handle_new_capi_call(void *args)
{
	struct new_api_call_param *params = args;

	forward_call_flag = GLSCH_LOW;
	return teei_smc(params->cmd, params->cmd_addr, params->size, NQ_VALID);
}

int teei_forward_call(u32 cmd, unsigned long cmd_addr, int size)
{
	int ret;
	struct new_api_call_param params = {
		.cmd = cmd,
		.cmd_addr = cmd_addr,
		.size = size,
	};

	KATRACE_BEGIN("teei_forward_call");

	lock_system_sleep();

	down(&capi_mutex);

	down(&smc_lock);

	init_completion(&wait_completion);

	ret = add_work_entry(NEW_CAPI_CALL, (unsigned long)&params);
	if (ret) {
		up(&smc_lock);
		up(&capi_mutex);
		unlock_system_sleep();
		KATRACE_END("teei_forward_call");
		return ret;
	}

	wait_for_completion(&wait_completion);

	up(&capi_mutex);

	unlock_system_sleep();

	KATRACE_END("teei_forward_call");

	return 0;
}
EXPORT_SYMBOL(teei_forward_call);

int teei_forward_call_without_lock(u32 cmd, unsigned long cmd_addr, int size)
{
	int ret;
	struct new_api_call_param params = {
		.cmd = cmd,
		.cmd_addr = cmd_addr,
		.size = size,
	};

	lock_system_sleep();

	down(&smc_lock);

	init_completion(&wait_completion);

	ret = add_work_entry(NEW_CAPI_CALL, (unsigned long)&params);
	if (ret) {
		up(&smc_lock);
		up(&capi_mutex);
		unlock_system_sleep();
		return ret;
	}

	wait_for_completion(&wait_completion);

	unlock_system_sleep();

	return 0;
}
EXPORT_SYMBOL(teei_forward_call_without_lock);

/**
 * @brief
 *      call smc
 * @param svc_id  - service identifier
 * @param cmd_id  - command identifier
 * @param context - session context
 * @param enc_id - encoder identifier
 * @param cmd_buf - command buffer
 * @param cmd_len - command buffer length
 * @param resp_buf - response buffer
 * @param resp_len - response buffer length
 * @param meta_data
 * @param ret_resp_len
 *
 * @return
 */
int __teei_smc_call(unsigned long local_smc_cmd,
		u32 teei_cmd_type,
		u32 dev_file_id,
		u32 svc_id,
		u32 cmd_id,
		u32 context,
		u32 enc_id,
		const void *cmd_buf,
		size_t cmd_len,
		void *resp_buf,
		size_t resp_len,
		const void *meta_data,
		const void *info_data,
		size_t info_len,
		int *ret_resp_len,
		int *error_code,
		struct semaphore *psema)
{
	int ret = 50;
	void *smc_cmd_phys = 0;
	struct teei_smc_cmd *smc_cmd = NULL;

	struct teei_shared_mem *temp_shared_mem = NULL;
	struct teei_context *temp_cont = NULL;

	smc_cmd = (struct teei_smc_cmd *)local_smc_cmd;

	if (ret_resp_len)
		*ret_resp_len = 0;

	smc_cmd->teei_cmd_type = teei_cmd_type;
	smc_cmd->dev_file_id = dev_file_id;
	smc_cmd->src_id = svc_id;
	smc_cmd->src_context = task_tgid_vnr(current);

	smc_cmd->id = cmd_id;
	smc_cmd->context = context;
	smc_cmd->enc_id = enc_id;
	smc_cmd->src_context = task_tgid_vnr(current);

	smc_cmd->req_buf_len = cmd_len;
	smc_cmd->resp_buf_len = resp_len;
	smc_cmd->info_buf_len = info_len;
	smc_cmd->ret_resp_buf_len = 0;

	if (psema == NULL)
		return -EINVAL;

	smc_cmd->teei_sema = (u64)psema;

	if (cmd_buf != NULL) {
		smc_cmd->req_buf_phys = virt_to_phys((void *)cmd_buf);

		Flush_Dcache_By_Area((unsigned long)cmd_buf,
					(unsigned long)cmd_buf + cmd_len);

		Flush_Dcache_By_Area((unsigned long)&cmd_buf,
				(unsigned long)&cmd_buf + sizeof(int));

	} else
		smc_cmd->req_buf_phys = 0;

	if (resp_buf) {
		smc_cmd->resp_buf_phys = virt_to_phys((void *)resp_buf);

		Flush_Dcache_By_Area((unsigned long)resp_buf,
				(unsigned long)resp_buf + resp_len);

	} else
		smc_cmd->resp_buf_phys = 0;

	if (meta_data) {
		smc_cmd->meta_data_phys = virt_to_phys(meta_data);

		Flush_Dcache_By_Area((unsigned long)meta_data,
				(unsigned long)meta_data +
				sizeof(struct teei_encode_meta) *
				(TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS));

	} else
		smc_cmd->meta_data_phys = 0;

	if (info_data) {
		smc_cmd->info_buf_phys = virt_to_phys(info_data);

		Flush_Dcache_By_Area((unsigned long)info_data,
					(unsigned long)info_data + info_len);

	} else
		smc_cmd->info_buf_phys = 0;

	smc_cmd_phys = (void *)virt_to_phys((void *)smc_cmd);

	smc_cmd->error_code = 0;

	Flush_Dcache_By_Area((unsigned long)smc_cmd,
			(unsigned long)smc_cmd + sizeof(struct teei_smc_cmd));

	Flush_Dcache_By_Area((unsigned long)&smc_cmd,
			(unsigned long)&smc_cmd + sizeof(int));

	/* down(&smc_lock); */

	list_for_each_entry(temp_cont,
		&teei_contexts_head.context_list, link) {

		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_shared_mem,
				&temp_cont->shared_mem_list, head)

				Flush_Dcache_By_Area(
					(unsigned long)temp_shared_mem->k_addr,
					(unsigned long)temp_shared_mem->k_addr
					+ temp_shared_mem->len);
		}
	}

	forward_call_flag = GLSCH_LOW;
	ret = teei_smc(0, (unsigned long)smc_cmd_phys,
				sizeof(struct teei_smc_cmd), NQ_VALID);

	/* down(psema); */
	return 0;
}

int teei_smc_call(u32 teei_cmd_type,
		u32 dev_file_id,
		u32 svc_id,
		u32 cmd_id,
		u32 context,
		u32 enc_id,
		void *cmd_buf,
		size_t cmd_len,
		void *resp_buf,
		size_t resp_len,
		void *meta_data,
		void *info_data,
		size_t info_len,
		int *ret_resp_len,
		int *error_code,
		struct semaphore *psema)
{
	IMSG_ERROR("[%s][%d] NOT Support! Please use new client api\n",
						__func__, __LINE__);
	return -1;
}
