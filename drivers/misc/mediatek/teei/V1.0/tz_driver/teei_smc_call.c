#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/io.h>

#include "teei_id.h"
#include "teei_common.h"
#include "teei_smc_call.h"
#include "teei_debug.h"
#include "nt_smc_call.h"
#include "utdriver_macro.h"

#define CAPI_CALL       0x01

extern int add_work_entry(int work_type, unsigned long buff);

void set_sch_nq_cmd(void)
{
	struct message_head msg_head;

	memset(&msg_head, 0, sizeof(struct message_head));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = STANDARD_CALL_TYPE;
	msg_head.child_type = N_INVOKE_T_NQ;

	memcpy(message_buff, &msg_head, sizeof(struct message_head));
	Flush_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);

	return;

}

/**
 * @brief
 *
 * @param cmd_addr phys address
 *
 * @return
 */

static u32 teei_smc(u32 cmd_addr, int size, int valid_flag)
{
	int retVal = 0;

	add_nq_entry(cmd_addr, size, valid_flag);
	set_sch_nq_cmd();
	Flush_Dcache_By_Area((unsigned long)t_nt_buffer, (unsigned long)t_nt_buffer + 0x1000);

	n_invoke_t_nq(0, 0, 0);
	return 0;
}


#if 0
/**
 * @brief
 *
 * @param teei_smc wrapper to handle the multi core case
 *
 * @return
 */
static u32 teei_smc(u32 cmd_addr, int size, int valid_flag)
{
#if 0
	int cpu_id = smp_processor_id();
	/* int cpu_id = raw_smp_processor_id(); */

	if (cpu_id != 0) {
		/* with mb */
		mb();
		pr_debug("[%s][%d]\n", __func__, __LINE__);
		return post_teei_smc(0, cmd_addr, size, valid_flag); /* post it to primary */
	} else {
		pr_debug("[%s][%d]\n", __func__, __LINE__);
		return _teei_smc(cmd_addr, size, valid_flag); /* called directly on primary core */
	}

#else
	return _teei_smc(cmd_addr, size, valid_flag);
	/* return post_teei_smc(0, cmd_addr, size, valid_flag); */
#endif
}

#endif

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

#if 0
	smc_cmd = (struct teei_smc_cmd *)tz_malloc_shared_mem(sizeof(struct teei_smc_cmd), GFP_KERNEL);

	if (!smc_cmd) {
		pr_err("tz_malloc failed for smc command");
		ret = -ENOMEM;
		goto out;
	}

#else
	smc_cmd = (struct teei_smc_cmd *)local_smc_cmd;
#endif

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

	if (NULL == psema)
		return -EINVAL;
	else
		smc_cmd->teei_sema = psema;

	if (cmd_buf != NULL) {
		smc_cmd->req_buf_phys = virt_to_phys((void *)cmd_buf);
		Flush_Dcache_By_Area((unsigned long)cmd_buf, (unsigned long)cmd_buf + cmd_len);
		Flush_Dcache_By_Area((unsigned long)&cmd_buf, (unsigned long)&cmd_buf + sizeof(int));
	} else
		smc_cmd->req_buf_phys = 0;

	if (resp_buf) {
		smc_cmd->resp_buf_phys = virt_to_phys((void *)resp_buf);
		Flush_Dcache_By_Area((unsigned long)resp_buf, (unsigned long)resp_buf + resp_len);
	} else
		smc_cmd->resp_buf_phys = 0;

	if (meta_data) {
		smc_cmd->meta_data_phys = virt_to_phys(meta_data);
		Flush_Dcache_By_Area((unsigned long)meta_data, (unsigned long)meta_data +
					sizeof(struct teei_encode_meta) * (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS));
	} else
		smc_cmd->meta_data_phys = 0;

	if (info_data) {
		smc_cmd->info_buf_phys = virt_to_phys(info_data);
		Flush_Dcache_By_Area((unsigned long)info_data, (unsigned long)info_data + info_len);
	} else
		smc_cmd->info_buf_phys = 0;

	smc_cmd_phys = virt_to_phys((void *)smc_cmd);

	smc_cmd->error_code = 0;

	Flush_Dcache_By_Area((unsigned long)smc_cmd, (unsigned long)smc_cmd + sizeof(struct teei_smc_cmd));
	Flush_Dcache_By_Area((unsigned long)&smc_cmd, (unsigned long)&smc_cmd + sizeof(int));

	/* down(&smc_lock); */

	list_for_each_entry(temp_cont,
			&teei_contexts_head.context_list,
			link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_shared_mem,
					&temp_cont->shared_mem_list,
					head) {
				Flush_Dcache_By_Area((unsigned long)temp_shared_mem->k_addr, (unsigned long)temp_shared_mem->k_addr + temp_shared_mem->len);
			}
		}
	}

	forward_call_flag = GLSCH_LOW;
	ret = teei_smc(smc_cmd_phys, sizeof(struct teei_smc_cmd), NQ_VALID);

	/* down(psema); */

	return 0;
}

static void secondary_teei_smc_call(void *info)
{
	struct smc_call_struct *cd = (struct smc_call_struct *)info;

	/* with a rmb() */
	rmb();

	cd->retVal = __teei_smc_call(cd->local_cmd,
				cd->teei_cmd_type,
				cd->dev_file_id,
				cd->svc_id,
				cd->cmd_id,
				cd->context,
				cd->enc_id,
				cd->cmd_buf,
				cd->cmd_len,
				cd->resp_buf,
				cd->resp_len,
				cd->meta_data,
				cd->info_data,
				cd->info_len,
				cd->ret_resp_len,
				cd->error_code,
				cd->psema);

	/* with a wmb() */
	wmb();
}



int teei_smc_call(u32 teei_cmd_type,
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
	int cpu_id = 0;
	int retVal = 0;

	struct teei_smc_cmd *local_smc_cmd = (struct teei_smc_cmd *)tz_malloc_shared_mem(sizeof(struct teei_smc_cmd), GFP_KERNEL);

	if (local_smc_cmd == NULL) {
		pr_err("[%s][%d] tz_malloc_shared_mem failed!\n", __func__, __LINE__);
		return -1;
	}

	smc_call_entry.local_cmd = local_smc_cmd;
	smc_call_entry.teei_cmd_type = teei_cmd_type;
	smc_call_entry.dev_file_id = dev_file_id;
	smc_call_entry.svc_id = svc_id;
	smc_call_entry.cmd_id = cmd_id;
	smc_call_entry.context = context;
	smc_call_entry.enc_id = enc_id;
	smc_call_entry.cmd_buf = cmd_buf;
	smc_call_entry.cmd_len = cmd_len;
	smc_call_entry.resp_buf = resp_buf;
	smc_call_entry.resp_len = resp_len;
	smc_call_entry.meta_data = meta_data;
	smc_call_entry.info_data = info_data;
	smc_call_entry.info_len = info_len;
	smc_call_entry.ret_resp_len = ret_resp_len;
	smc_call_entry.error_code = error_code;
	smc_call_entry.psema = psema;

	down(&smc_lock);

	if (teei_config_flag == 1)
		complete(&global_down_lock);

	/* with a wmb() */
	wmb();

#if 0
	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_teei_smc_call, (void *)(&smc_call_entry), 1);
	put_online_cpus();
#else
	Flush_Dcache_By_Area((unsigned long)&smc_call_entry, (unsigned long)&smc_call_entry + sizeof(smc_call_entry));
	retVal = add_work_entry(CAPI_CALL, (unsigned long)&smc_call_entry);

	if (retVal != 0) {
		tz_free_shared_mem(local_smc_cmd, sizeof(struct teei_smc_cmd));
		return retVal;
	}

#endif

	down(psema);

	Invalidate_Dcache_By_Area((unsigned long)local_smc_cmd, (unsigned long)local_smc_cmd + sizeof(struct teei_smc_cmd));
	Invalidate_Dcache_By_Area((unsigned long)&smc_call_entry, (unsigned long)&smc_call_entry + sizeof(smc_call_entry));

	if (cmd_buf)
		Invalidate_Dcache_By_Area((unsigned long)cmd_buf, (unsigned long)cmd_buf + cmd_len);

	if (resp_buf)
		Invalidate_Dcache_By_Area((unsigned long)resp_buf, (unsigned long)resp_buf + resp_len);

	if (meta_data)
		Invalidate_Dcache_By_Area((unsigned long)meta_data, (unsigned long)meta_data +
					sizeof(struct teei_encode_meta) * (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS));

	if (info_data)
		Invalidate_Dcache_By_Area((unsigned long)info_data, (unsigned long)info_data + info_len);

	/* with a rmb() */
	rmb();

	if (ret_resp_len)
		*ret_resp_len = local_smc_cmd->ret_resp_buf_len;

	tz_free_shared_mem(local_smc_cmd, sizeof(struct teei_smc_cmd));

	return smc_call_entry.retVal;
}
