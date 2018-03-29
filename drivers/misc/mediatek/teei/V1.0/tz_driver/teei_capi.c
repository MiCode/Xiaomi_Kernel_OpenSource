#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "teei_smc_struct.h"
#include "teei_capi.h"
#include "teei_client.h"
#include "teei_id.h"
#include "teei_debug.h"
#include "teei_common.h"
#ifdef CONFIG_ARM64
#include <linux/compat.h>
#else
static inline void __user *compat_ptr(unsigned int * uptr)
{
	        return (void __user *)(unsigned long)uptr;
}
#endif

int teei_client_close_session_for_service(void *private_data, struct teei_session *temp_ses);

struct teei_context *teei_create_context(int dev_count)
{
	struct teei_context *cont = NULL;
	cont = kmalloc(sizeof(struct teei_context), GFP_KERNEL);

	if (cont == NULL)
		return NULL;

	cont->cont_id = dev_count;
	cont->sess_cnt = 0;
	memset(cont->tee_name, 0, TEE_NAME_SIZE);
	INIT_LIST_HEAD(&(cont->link));
	INIT_LIST_HEAD(&(cont->sess_link));
	INIT_LIST_HEAD(&(cont->shared_mem_list));

	list_add(&(cont->link), &(teei_contexts_head.context_list));
	teei_contexts_head.dev_file_cnt++;
	return cont;
}

struct teei_session *teei_create_session(struct teei_context *cont)
{
	struct teei_session *sess = NULL;
	sess = kmalloc(sizeof(struct teei_session), GFP_KERNEL);

	if (sess == NULL)
		return NULL;

	sess->sess_id = (unsigned long)sess;
	sess->parent_cont = cont;
	INIT_LIST_HEAD(&(sess->link));
	INIT_LIST_HEAD(&(sess->encode_list));
	INIT_LIST_HEAD(&(sess->shared_mem_list));

	list_add(&(sess->link), &(cont->sess_link));
	cont->sess_cnt = cont->sess_cnt + 1;

	return sess;
}


int teei_client_context_init(void *private_data, void *argp)
{
	int retVal = 0;
	struct teei_context *temp_cont = NULL;
	unsigned long dev_file_id = (unsigned long)private_data;
	struct ctx_data ctx;
	int dev_found = 0;
	int error_code = 0;
	int *resp_flag = tz_malloc_shared_mem(4, GFP_KERNEL);
	char *name = tz_malloc_shared_mem(sizeof(ctx.name), GFP_KERNEL);

	if (resp_flag == NULL) {
		pr_err("[%s][%d] ========== resp_flag is NULL ============\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (name == NULL) {
		pr_err("[%s][%d] ========== name is NULL ============\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(&ctx, argp, sizeof(ctx))) {
		pr_err("[%s][%d] copy from user failed.\n ", __func__, __LINE__);
		return -EFAULT;
	}

	memcpy(name, ctx.name, sizeof(ctx.name));
#ifdef UT_DEBUG
	pr_debug("[%s][%d] context name = %s.\n ", __func__, __LINE__, name);
#endif
	Flush_Dcache_By_Area((unsigned long)name, (unsigned long)name + sizeof(ctx.name));


	down_write(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			dev_found = 1;
			break;
		}
	}
	up_write(&(teei_contexts_head.teei_contexts_sem));

	if (dev_found) {
		strncpy(temp_cont->tee_name, ctx.name, min(sizeof(temp_cont->tee_name) - 1, strlen(ctx.name)));
		retVal = teei_smc_call(TEEI_CMD_TYPE_INITILIZE_CONTEXT, dev_file_id,
					0, 0, 0, 0, name, 255, resp_flag, 4, NULL,
					NULL, 0, NULL, &error_code, &(temp_cont->cont_lock));
	}

	ctx.ctx_ret = !(*resp_flag);

	tz_free_shared_mem(resp_flag, 4);
	tz_free_shared_mem(name, sizeof(ctx.name));

	if (copy_to_user(argp, &ctx, sizeof(ctx))) {
		pr_err("[%s][%d]copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	return retVal;

}


int teei_client_context_close(void *private_data, void *argp)
{
	int retVal = 0;
	struct teei_context *temp_cont = NULL;
	unsigned long dev_file_id = (unsigned long)private_data;
	struct ctx_data ctx;
	int dev_found = 0;
	int *resp_flag = (int *) tz_malloc_shared_mem(4, GFP_KERNEL);
	int error_code = 0;

	if (resp_flag == NULL) {
		pr_err("[%s][%d] ========== resp_flag is NULL ============\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(&ctx, argp, sizeof(ctx))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	down_write(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			dev_found = 1;
			break;
		}
	}

	up_write(&(teei_contexts_head.teei_contexts_sem));

	if (dev_found) {
		strncpy(temp_cont->tee_name, ctx.name, min(sizeof(temp_cont->tee_name) - 1, strlen(ctx.name)));
		retVal = teei_smc_call(TEEI_CMD_TYPE_FINALIZE_CONTEXT, dev_file_id,
					0, 0, 0, 0,
					NULL, 0, resp_flag, 4, NULL, NULL,
					0, NULL, &error_code, &(temp_cont->cont_lock));
	}

	ctx.ctx_ret = *resp_flag;
	tz_free_shared_mem(resp_flag, 4);

	if (copy_to_user(argp, &ctx, sizeof(ctx))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	return retVal;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_session_init(void *private_data, void *argp)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *ses_new = NULL;
	struct user_ses_init ses_init;
	int ctx_found = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	pr_debug("inside session init\n");

	if (copy_from_user(&ses_init, argp, sizeof(ses_init))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return  -EFAULT;
	}

	down_read(&(teei_contexts_head.teei_contexts_sem));
	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			ctx_found = 1;
			break;
		}
	}
	up_read(&(teei_contexts_head.teei_contexts_sem));

	if (!ctx_found) {
		pr_err("[%s][%d] can't find context.\n", __func__, __LINE__);
		return -EINVAL;
	}

	ses_new = (struct teei_session *)tz_malloc(sizeof(struct teei_session), GFP_KERNEL);

	if (ses_new == NULL) {
		pr_err("[%s][%d] tz_malloc failed.\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ses_init.session_id = (unsigned long)ses_new;
	ses_new->sess_id = (unsigned long)ses_new;
	ses_new->parent_cont = temp_cont;

	INIT_LIST_HEAD(&ses_new->link);
	INIT_LIST_HEAD(&ses_new->encode_list);
	INIT_LIST_HEAD(&ses_new->shared_mem_list);
	list_add_tail(&ses_new->link, &temp_cont->sess_link);

	if (copy_to_user(argp, &ses_init, sizeof(ses_init))) {
		list_del(&ses_new->link);
		kfree(ses_new);
		return -EFAULT;
	}

	return 0;
}

/**
 * @brief			Open one session
 *
 * @param argp
 *
 * @return		0: success
 *				EFAULT: copy data from user space OR copy data back to the user space error
 *				EINVLA: can not find the context OR session structure.
 */
int teei_client_session_open(void *private_data, void *argp)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *ses_new = NULL;
	struct teei_encode *enc_temp = NULL;

	struct ser_ses_id *ses_open = (struct ser_ses_id *)tz_malloc_shared_mem(sizeof(struct ser_ses_id), GFP_KERNEL);
	int ctx_found = 0;
	int sess_found = 0;
	int enc_found = 0;
	int retVal = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	if (ses_open == NULL) {
		pr_err("[%s][%d] ========== ses_open is NULL ============\n", __func__, __LINE__);
		return -EFAULT;
	}

	/* Get the paraments about this session from user space. */
	if (copy_from_user(ses_open, argp, sizeof(struct ser_ses_id))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		tz_free_shared_mem(ses_open, sizeof(struct ser_ses_id));
		return -EFAULT;
	}

	/* Search the teei_context structure */
	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			ctx_found = 1;
			break;
		}
	}

	if (ctx_found == 0) {
		pr_err("[%s][%d] can't find context!\n", __func__, __LINE__);
		tz_free_shared_mem(ses_open, sizeof(struct ser_ses_id));
		return -EINVAL;
	}

	/* Search the teei_session structure */
	list_for_each_entry(ses_new, &temp_cont->sess_link, link) {
		if (ses_new->sess_id == ses_open->session_id) {
			sess_found = 1;
			break;
		}
	}

	if (sess_found == 0) {
		pr_err("[%s][%d] can't find session!\n", __func__, __LINE__);
		tz_free_shared_mem(ses_open, sizeof(struct ser_ses_id));
		return -EINVAL;
	}

	/* Search the teei_encode structure */
	list_for_each_entry(enc_temp, &ses_new->encode_list, head) {
		if (enc_temp->encode_id == ses_open->paramtype) {
			enc_found = 1;
			break;
		}
	}

	/* Invoke the smc_call */
	if (enc_found) {
		/* This session had been encoded. */
		retVal = teei_smc_call(TEEI_CMD_TYPE_OPEN_SESSION,
					dev_file_id,
					0,
					0,
					0,
					0,
					enc_temp->ker_req_data_addr,
					enc_temp->enc_req_offset,
					enc_temp->ker_res_data_addr,
					enc_temp->enc_res_offset,
					enc_temp->meta,
					ses_open,
					sizeof(struct ser_ses_id),
					NULL,
					NULL,
					&(temp_cont->cont_lock));

	} else {
		/* This session didn't have been encoded */
		retVal = teei_smc_call(TEEI_CMD_TYPE_OPEN_SESSION,
					dev_file_id,
					0,
					0,
					0,
					0,
					NULL,
					0,
					NULL,
					0,
					NULL,
					ses_open,
					sizeof(struct ser_ses_id),
					NULL,
					NULL,
					&(temp_cont->cont_lock));
	}

	if (retVal != SMC_SUCCESS) {
		pr_err("[%s][%d] open session smc error!\n", __func__, __LINE__);
		goto clean_hdr_buf;
	}

	if (ses_open->session_id == -1)
		pr_err("[%s][%d] invalid session id!\n", __func__, __LINE__);

	/* Copy the result back to the user space */
	ses_new->sess_id = ses_open->session_id;

	if (copy_to_user(argp, ses_open, sizeof(struct ser_ses_id))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		retVal = -EFAULT;
		goto clean_hdr_buf;
	}

	tz_free_shared_mem(ses_open, sizeof(struct ser_ses_id));
	return 0;

clean_hdr_buf:
	list_del(&ses_new->link);
	tz_free_shared_mem(ses_open, sizeof(struct ser_ses_id));
	kfree(ses_new);
	return retVal;
}



/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_session_close(void *private_data, void *argp)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	int retVal = -EFAULT;
	unsigned long dev_file_id = (unsigned long)private_data;

	struct ser_ses_id ses_close;

	if (copy_from_user(&ses_close, argp, sizeof(ses_close))) {
		pr_err("[%s][%d] copy from user failed.\n ", __func__, __LINE__);
		return -EFAULT;
	}

	down_read(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
				if (temp_ses->sess_id == ses_close.session_id) {
					teei_client_close_session_for_service(private_data, temp_ses);
					retVal = 0;
					goto copy_to_user;
				}
			}
		}
	}

copy_to_user:
	up_read(&(teei_contexts_head.teei_contexts_sem));

	if (copy_to_user(argp, &ses_close, sizeof(ses_close))) {
		pr_err("[%s][%d] copy from user failed.\n", __func__, __LINE__);
		return -EFAULT;
	}

	return retVal;
}



/**
 * @brief		search the teei_encode and teei_session structures
 *			if there is no structure, create one.
 * @param		private_data Context ID
 * @param		enc
 * @param		penc_context
 * @param		psession
 *
 * @return		EINVAL: Invalid parament
 *			ENOMEM: No enough memory
 *			0: success
 */
int teei_client_prepare_encode(void *private_data,
				struct teei_client_encode_cmd *enc,
				struct teei_encode **penc_context,
				struct teei_session **psession)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	struct teei_encode *enc_context = NULL;
	int session_found = 0;
	int enc_found = 0;
	int retVal = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	/* search the context session with private_data */
	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
				if (temp_ses->sess_id == enc->session_id) {
					session_found = 1;
					break;
				}
			}
		}

		if (session_found == 1)
			break;
	}

	if (!session_found) {
		pr_err("[%s][%d] session (ID: %x) not found!\n", __func__, __LINE__, enc->session_id);
		return -EINVAL;
	}

	/*
	 * check if the enc struct had been inited.
	 */

	if (enc->encode_id != -1) {
		list_for_each_entry(enc_context, &temp_ses->encode_list, head) {
			if (enc_context->encode_id == enc->encode_id) {
				enc_found = 1;
				break;
			}
		}
	}

	/* create one command parament block */
	if (!enc_found) {
		enc_context = (struct teei_encode *)tz_malloc(sizeof(struct teei_encode), GFP_KERNEL);

		if (enc_context == NULL) {
			pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
			return -ENOMEM;
		}

		enc_context->meta = tz_malloc_shared_mem(sizeof(struct teei_encode_meta) *
					(TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS),
					GFP_KERNEL);

		if (enc_context->meta == NULL) {
			pr_err("[%s][%d] enc_context->meta is NULL!\n", __func__, __LINE__);
			kfree(enc_context);
			return -ENOMEM;
		}

		memset(enc_context->meta, 0, sizeof(struct teei_encode_meta) *
		       (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS));

		enc_context->encode_id = (unsigned long)enc_context;
		enc_context->ker_req_data_addr = NULL;
		enc_context->ker_res_data_addr = NULL;
		enc_context->enc_req_offset = 0;
		enc_context->enc_res_offset = 0;
		enc_context->enc_req_pos = 0;
		enc_context->enc_res_pos = TEEI_MAX_REQ_PARAMS;
		enc_context->dec_res_pos = TEEI_MAX_REQ_PARAMS;
		enc_context->dec_offset = 0;
		list_add_tail(&enc_context->head, &temp_ses->encode_list);
		enc->encode_id = enc_context->encode_id;
	}

	/* return the enc_context & temp_ses */
	*penc_context = enc_context;
	*psession = temp_ses;

	return retVal;
}



/**
 * @brief		Send a command to TEE
 *
 * @param argp
 *
 * @return		EINVAL: Invalid parament
 *                      EFAULT: copy data from user space OR copy data back to the user space error
 *                      0: success
 */
int teei_client_send_cmd(void *private_data, void *argp)
{
	int retVal = 0;
	struct teei_client_encode_cmd enc;
	unsigned long dev_file_id = 0;
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	struct teei_encode *enc_temp = NULL;
	int ctx_found = 0;
	int sess_found = 0;
	int enc_found = 0;

	dev_file_id = (unsigned long)private_data;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	down_read(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			ctx_found = 1;
			break;
		}

	}

	up_read(&(teei_contexts_head.teei_contexts_sem));

	if (ctx_found == 0) {
		pr_err("[%s][%d] can't find context data!\n", __func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
		if (temp_ses->sess_id == enc.session_id) {
			sess_found = 1;
			break;
		}
	}

	if (sess_found == 0) {
		pr_err("[%s][%d] can't find session data!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (enc.encode_id != -1) {
		list_for_each_entry(enc_temp, &temp_ses->encode_list, head) {
			if (enc_temp->encode_id == enc.encode_id) {
				enc_found = 1;
				break;
			}
		}
	} else {
		retVal = teei_client_prepare_encode(private_data, &enc, &enc_temp, &temp_ses);

		if (retVal == 0)
			enc_found = 1;
	}

	if (enc_found == 0) {
		pr_err("[%s][%d] can't find encode data!\n", __func__, __LINE__);
		return -EINVAL;
	}

	retVal = teei_smc_call(TEEI_CMD_TYPE_INVOKE_COMMAND,
				dev_file_id,
				0,
				enc.cmd_id,
				enc.session_id,
				enc.encode_id,
				enc_temp->ker_req_data_addr,
				enc_temp->enc_req_offset,
				enc_temp->ker_res_data_addr,
				enc_temp->enc_res_offset,
				enc_temp->meta,
				NULL,
				0,
				&enc.return_value,
				&enc.return_origin,
				&(temp_cont->cont_lock));

	if (retVal != SMC_SUCCESS)
		pr_err("[%s][%d] send cmd secure call failed!\n", __func__, __LINE__);

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("[%s][%d] copy to user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	return retVal;
}

/**
 * @brief		Delete the operation with the encode_id
 *
 * @param argp
 *
 * @return		EINVAL: Invalid parament
 *                      EFAULT: copy data from user space OR copy data back to the user space error
 *                      0: success
 */
int teei_client_operation_release(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	struct teei_encode *enc_context = NULL;
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	int ctx_found = 0;
	int session_found = 0;
	int enc_found = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			ctx_found = 1;
			break;
		}
	}

	if (ctx_found == 0) {
		pr_err("[%s][%d] ctx_found failed!\n", __func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
		if (temp_ses->sess_id == enc.session_id) {
			session_found = 1;
			break;
		}
	}

	if (session_found == 0) {
		pr_err("[%s][%d] session_found failed!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (enc.encode_id != -1) {
		list_for_each_entry(enc_context, &temp_ses->encode_list, head) {
			if (enc_context->encode_id == enc.encode_id) {
				enc_found = 1;
				break;
			}
		}
	}

	if (enc_found == 0) {
		pr_err("[%s][%d] enc_found failed!\n", __func__, __LINE__);
		return -EINVAL;
	} else {
		if (enc_context->ker_req_data_addr)
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);

		if (enc_context->ker_res_data_addr)
			tz_free_shared_mem(enc_context->ker_res_data_addr, TEEI_1K_SIZE);

		list_del(&enc_context->head);
		/* kfree(enc_context->meta); */
		tz_free_shared_mem(enc_context->meta, sizeof(struct teei_encode_meta) *
					(TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS));
		kfree(enc_context);
	}

	return 0;
}


/**
 * @brief
 *
 * @param argp
 *
 * @return
 */

int teei_client_encode_uint32(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	int retVal = 0;
	struct teei_session *session = NULL;
	struct teei_encode *enc_context = NULL;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal != 0) {
		pr_err("[%s][%d]  failed!\n", __func__, __LINE__);
		return retVal;
	}

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (enc_context->ker_req_data_addr == NULL) {
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (enc_context->ker_req_data_addr == NULL) {
				pr_err("[%s][%d] enc_context->ker_req_data_addr is NULL!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_u32;
			}
		}

		if ((enc_context->enc_req_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			u64 addr = enc.data;
			void __user *pt = compat_ptr((unsigned int *)addr);
			u32 value = 0;

			if (copy_from_user(&value, pt, 4)) {
				retVal = -EINVAL;
				goto ret_encode_u32;
			}

			*(u32 *)((char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset) = value;
			enc_context->enc_req_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].type = TEEI_ENC_UINT32;
			enc_context->meta[enc_context->enc_req_pos].len = sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].value_flag = enc.value_flag;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;
			enc_context->enc_req_pos++;
		} else {
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_u32;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (!enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] enc_context->ker_res_data_addr is NULL\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_u32;
			}
		}

		if ((enc_context->enc_res_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos < (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			if (enc.data != NULL) {
				u64 addr = enc.data;
				void __user *pt = compat_ptr((unsigned int *)addr);
				/*
				u32 value = 0;
				copy_from_user(&value, pt, 4);
				pr_debug("value %x\n", value);
				 */
				/* chengxin modify, the usr addr is a 64 bit type, should not asign to 32 */
				/* enc_context->meta[enc_context->enc_res_pos].usr_addr = pt; */
				enc_context->meta[enc_context->enc_res_pos].usr_addr = (unsigned int)enc.data;
			} else {
				enc_context->meta[enc_context->enc_res_pos].usr_addr = 0;
			}

			enc_context->enc_res_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].type = TEEI_ENC_UINT32;
			enc_context->meta[enc_context->enc_res_pos].len = sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].value_flag = enc.value_flag;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;
			enc_context->enc_res_pos++;
		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_res_data_addr, TEEI_1K_SIZE);
			retVal =  -ENOMEM;
			goto ret_encode_u32;
		}
	}


ret_encode_u32:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("copy from user failed ");
		retVal = -EFAULT;
	}

	return retVal;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */

int teei_client_encode_uint32_64bit(void *private_data, void *argp)
{

	struct teei_client_encode_cmd enc;
	int retVal = 0;
	struct teei_session *session = NULL;
	struct teei_encode *enc_context = NULL;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d]copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal != 0) {
		pr_err("[%s][%d] failed!\n", __func__, __LINE__);
		return retVal;
	}

	/*
		pr_debug("enc.data= %p\n",(void *)enc.data);
		pr_debug("[%s][%d] enc.encode_id = %x\n", __func__, __LINE__, enc.encode_id);
		pr_debug("[%s][%d] enc_context->encode_id = %x\n", __func__, __LINE__, enc_context->encode_id);
	*/

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (enc_context->ker_req_data_addr == NULL) {
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (enc_context->ker_req_data_addr == NULL) {
				pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_u32;
			}
		}

		if ((enc_context->enc_req_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			u64 addr = enc.data;
			void __user *pt = compat_ptr((unsigned int *)addr);
			u32 value = 0;
			copy_from_user(&value, pt, 4);

			/* chengxin modify if user space is 64 bit, enc.data is a 64 bit addr, do not change to 32 bit */
			*(u32 *)((char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset) = *(u32 *)enc.data;
			/* *(u32 *)((char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset) = value; */

			enc_context->enc_req_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].type = TEEI_ENC_UINT32;
			enc_context->meta[enc_context->enc_req_pos].len = sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].value_flag = enc.value_flag;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;
			enc_context->enc_req_pos++;
		} else {
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_u32;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (!enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] tz_malloc failed\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_u32;
			}
		}

		if ((enc_context->enc_res_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos < (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			if (enc.data != NULL) {
				u64 addr = enc.data;
				void __user *pt = compat_ptr((unsigned int *)addr);
				/*
				u32 value = 0;
				copy_from_user(&value, pt, 4);
				pr_debug("value %x\n", value);
				*/
				/* chengxin modify, the usr addr is a 64 bit type, should not asign to 32 */
				/* enc_context->meta[enc_context->enc_res_pos].usr_addr = pt; */
				enc_context->meta[enc_context->enc_res_pos].usr_addr = enc.data; /* this data should not change to 32bit in 64api system */
			} else {
				enc_context->meta[enc_context->enc_res_pos].usr_addr = 0;
			}

			enc_context->enc_res_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].type = TEEI_ENC_UINT32;
			enc_context->meta[enc_context->enc_res_pos].len = sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].value_flag = enc.value_flag;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;
			enc_context->enc_res_pos++;
		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_res_data_addr, TEEI_1K_SIZE);
			retVal =  -ENOMEM;
			goto ret_encode_u32;
		}
	}


ret_encode_u32:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("copy from user failed\n");
		retVal = -EFAULT;
	}

	return retVal;
}


/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_encode_array(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	int retVal = 0;
	struct teei_encode *enc_context = NULL;
	struct teei_session *session = NULL;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal)
		goto return_func;

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (NULL == enc_context->ker_req_data_addr) {
			/* pr_debug("[%s][%d] allocate req data data.\n", __func__, __LINE__); */
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_req_data_addr) {
				pr_err("[%s][%d] enc_context->ker_req_data_addr is NULL!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc.len > 0) && (enc_context->enc_req_offset + enc.len <= TEEI_1K_SIZE) &&
			(enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			if (copy_from_user(
				(char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset,
				(unsigned int)enc.data , enc.len)) {
				pr_err("[%s][%d] copy from user failed.\n", __func__, __LINE__);
				retVal = -EFAULT;
				goto ret_encode_array;
			}

			enc_context->enc_req_offset += enc.len;

			enc_context->meta[enc_context->enc_req_pos].type = TEEI_ENC_ARRAY;
			enc_context->meta[enc_context->enc_req_pos].len = enc.len;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_req_pos++;
		} else {
			/* kfree(enc_context->ker_req_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (NULL == enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (NULL == enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] enc_context->ker_res_data_addr is NULL!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc.len > 0) && (enc_context->enc_res_offset + enc.len <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos <
		     (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			if (enc.data != NULL) {
				enc_context->meta[enc_context->enc_res_pos].usr_addr
				    = (unsigned int)enc.data;
			} else {
				enc_context->meta[enc_context->enc_res_pos].usr_addr = 0;
			}

			enc_context->enc_res_offset += enc.len;
			enc_context->meta[enc_context->enc_res_pos].type = TEEI_ENC_ARRAY;
			enc_context->meta[enc_context->enc_res_pos].len = enc.len;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_res_pos++;
		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	}

ret_encode_array:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

return_func:
	pr_debug("[%s][%d] teei_client_encode_array end!\n", __func__, __LINE__);
	return retVal;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_encode_array_64bit(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	int retVal = 0;
	struct teei_encode *enc_context = NULL;
	struct teei_session *session = NULL;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal)
		goto return_func;

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (NULL == enc_context->ker_req_data_addr) {
			/* pr_debug("[%s][%d] allocate req data data.\n", __func__, __LINE__); */
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_req_data_addr) {
				pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc.len > 0) && (enc_context->enc_req_offset + enc.len <= TEEI_1K_SIZE) &&
		    (enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			if (copy_from_user(
					(char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset,
					enc.data , enc.len)) {
				pr_err("[%s][%d] copy from user failed.\n", __func__, __LINE__);
				retVal = -EFAULT;
				goto ret_encode_array;
			}

			enc_context->enc_req_offset += enc.len;

			enc_context->meta[enc_context->enc_req_pos].type = TEEI_ENC_ARRAY;
			enc_context->meta[enc_context->enc_req_pos].len = enc.len;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_req_pos++;
		} else {
			/* kfree(enc_context->ker_req_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (NULL == enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (NULL == enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc_context->enc_res_offset + enc.len <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos <
		     (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			if (enc.data != NULL) {
				enc_context->meta[enc_context->enc_res_pos].usr_addr
				    = enc.data;
			} else {
				enc_context->meta[enc_context->enc_res_pos].usr_addr = 0;
			}

			enc_context->enc_res_offset += enc.len;
			enc_context->meta[enc_context->enc_res_pos].type = TEEI_ENC_ARRAY;
			enc_context->meta[enc_context->enc_res_pos].len = enc.len;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_res_pos++;
		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	}

ret_encode_array:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

return_func:
	pr_debug("[%s][%d] teei_client_encode_array end!\n", __func__, __LINE__);
	return retVal;
}


/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_encode_mem_ref(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	int retVal = 0;
	int shared_mem_found = 0;
	struct teei_encode *enc_context = NULL;
	struct teei_session *session = NULL;
	struct teei_shared_mem *temp_shared_mem = NULL;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal != 0)
		goto return_func;

	list_for_each_entry(temp_shared_mem, &session->shared_mem_list, s_head) {
		u64 addr = enc.data;

		if (temp_shared_mem && temp_shared_mem->index == (unsigned int)addr) {
			shared_mem_found = 1;
			break;
		}
	}

	if (shared_mem_found == 0) {
		struct teei_context *temp_cont = NULL;
		list_for_each_entry(temp_cont,
					&teei_contexts_head.context_list,
					link) {
			if (temp_cont->cont_id == (unsigned long)private_data) {
				list_for_each_entry(temp_shared_mem,
						&temp_cont->shared_mem_list,
						head) {
					if (temp_shared_mem->index == (u32)enc.data) {
						shared_mem_found = 1;
						break;
					}
				}

				break;
			}
		}
	}

	if (!shared_mem_found) {
		retVal = -EINVAL;
		goto return_func;
	}

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (NULL == enc_context->ker_req_data_addr) {
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (NULL == enc_context->ker_req_data_addr) {
				pr_err("[%s][%d] enc_context->ker_req_data_addr is NULL!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc_context->enc_req_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			*(u32 *)((char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset)
			    = virt_to_phys((char *)temp_shared_mem->k_addr+enc.offset);

			Flush_Dcache_By_Area((unsigned long)(temp_shared_mem->k_addr+enc.offset),
					(unsigned long)temp_shared_mem->k_addr + enc.offset+enc.len);
			enc_context->enc_req_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].usr_addr
			    = (unsigned long)((char *)temp_shared_mem->u_addr + enc.offset);
			enc_context->meta[enc_context->enc_req_pos].type = TEEI_MEM_REF;
			enc_context->meta[enc_context->enc_req_pos].len = enc.len;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_req_pos++;

		} else {
			/* kfree(enc_context->ker_req_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (!enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] enc_context->ker_res_data_addr is NULL!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc_context->enc_res_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos < (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			*(u32 *)((char *)enc_context->ker_res_data_addr +
					enc_context->enc_res_offset)
			    = virt_to_phys((char *)temp_shared_mem->k_addr + enc.offset);

			enc_context->enc_res_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].usr_addr
			    = (unsigned long)((char *)temp_shared_mem->u_addr + enc.offset);
			enc_context->meta[enc_context->enc_res_pos].type
			    =  TEEI_MEM_REF;
			enc_context->meta[enc_context->enc_res_pos].len = enc.len;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_res_pos++;


		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_res_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			goto ret_encode_array;
		}
	}

ret_encode_array:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

return_func:
	return retVal;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_encode_mem_ref_64bit(void *private_data, void *argp)
{
	struct teei_client_encode_cmd enc;
	int retVal = 0;
	int shared_mem_found = 0;
	struct teei_encode *enc_context = NULL;
	struct teei_session *session = NULL;
	struct teei_shared_mem *temp_shared_mem = NULL;
	unsigned int temp_addr;

	if (copy_from_user(&enc, argp, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_encode(private_data, &enc, &enc_context, &session);

	if (retVal != 0)
		goto return_func;

	list_for_each_entry(temp_shared_mem, &session->shared_mem_list, s_head) {
		u64 addr = enc.data;

		if (temp_shared_mem && temp_shared_mem->index == enc.data) {
			shared_mem_found = 1;
			break;
		}
	}

	if (shared_mem_found == 0) {
		struct teei_context *temp_cont = NULL;
		list_for_each_entry(temp_cont,
				&teei_contexts_head.context_list,
				link) {
			if (temp_cont->cont_id == (unsigned long)private_data) {
				list_for_each_entry(temp_shared_mem,
							&temp_cont->shared_mem_list,
							head) {
					if (((temp_shared_mem->index)) == enc.data) {
						shared_mem_found = 1;
						break;
					}
				}

				break;
			}

			if (shared_mem_found == 1)
				break;
		}
	}

	if (!shared_mem_found) {
		pr_err("[%s][%d]fail to find shared_mem!\n", __func__, __LINE__);
		retVal = -EINVAL;
		goto return_func;
	}

	if (enc.param_type == TEEIC_PARAM_IN) {
		if (NULL == enc_context->ker_req_data_addr) {
			enc_context->ker_req_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (NULL == enc_context->ker_req_data_addr) {
				pr_err("[%s][%d]tz_malloc failed!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc_context->enc_req_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_req_pos < TEEI_MAX_REQ_PARAMS)) {
			*(u32 *)((char *)enc_context->ker_req_data_addr + enc_context->enc_req_offset)
			    = virt_to_phys((char *)temp_shared_mem->k_addr+enc.offset);

			Flush_Dcache_By_Area((unsigned long)(temp_shared_mem->k_addr+enc.offset),
					(unsigned long)temp_shared_mem->k_addr+enc.offset+enc.len);
			enc_context->enc_req_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_req_pos].usr_addr
			    = (unsigned long)((char *)temp_shared_mem->u_addr + enc.offset);
			enc_context->meta[enc_context->enc_req_pos].type = TEEI_MEM_REF;
			enc_context->meta[enc_context->enc_req_pos].len = enc.len;
			enc_context->meta[enc_context->enc_req_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_req_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_req_pos++;

		} else {
			/* kfree(enc_context->ker_req_data_addr); */
			tz_free_shared_mem(enc_context->ker_req_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			pr_err("[%s][%d]failed!\n", __func__, __LINE__);
			goto ret_encode_array;
		}
	} else if (enc.param_type == TEEIC_PARAM_OUT) {
		if (!enc_context->ker_res_data_addr) {
			enc_context->ker_res_data_addr = tz_malloc_shared_mem(TEEI_1K_SIZE, GFP_KERNEL);

			if (!enc_context->ker_res_data_addr) {
				pr_err("[%s][%d] tz_malloc failed!\n", __func__, __LINE__);
				retVal = -ENOMEM;
				goto ret_encode_array;
			}
		}

		if ((enc_context->enc_res_offset + sizeof(u32) <= TEEI_1K_SIZE) &&
		    (enc_context->enc_res_pos < (TEEI_MAX_RES_PARAMS + TEEI_MAX_REQ_PARAMS))) {
			*(u32 *)((char *)enc_context->ker_res_data_addr +
				enc_context->enc_res_offset)
			    = virt_to_phys((char *)temp_shared_mem->k_addr + enc.offset);

			enc_context->enc_res_offset += sizeof(u32);
			enc_context->meta[enc_context->enc_res_pos].usr_addr
			    = (unsigned long)((char *)temp_shared_mem->u_addr + enc.offset);
			enc_context->meta[enc_context->enc_res_pos].type
			    =  TEEI_MEM_REF;
			enc_context->meta[enc_context->enc_res_pos].len = enc.len;
			enc_context->meta[enc_context->enc_res_pos].param_pos = enc.param_pos;
			enc_context->meta[enc_context->enc_res_pos].param_pos_type = enc.param_pos_type;

			enc_context->enc_res_pos++;


		} else {
			/* kfree(enc_context->ker_res_data_addr); */
			tz_free_shared_mem(enc_context->ker_res_data_addr, TEEI_1K_SIZE);
			retVal = -ENOMEM;
			pr_err("[%s][%d] failed!\n", __func__, __LINE__);
			goto ret_encode_array;
		}
	}

ret_encode_array:

	if (copy_to_user(argp, &enc, sizeof(enc))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

return_func:
	return retVal;
}


static int print_context(void)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_sess = NULL;
	struct teei_encode *dec_context = NULL;

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		pr_debug("[%s][%d] context id [%lx]\n", __func__, __LINE__, temp_cont->cont_id);
		list_for_each_entry(temp_sess, &temp_cont->sess_link, link) {
			pr_debug("[%s][%d] session id [%x]\n", __func__, __LINE__, temp_sess->sess_id);
			list_for_each_entry(dec_context, &temp_sess->encode_list, head) {
				pr_debug("[%s][%d] encode_id [%x]\n", __func__, __LINE__, dec_context->encode_id);
			}
		}
	}
	return 0;
}


/**
 * @brief
 *
 * @param dec
 * @param pdec_context
 *
 * @return
 */
int teei_client_prepare_decode(void *private_data,
				struct teei_client_encode_cmd *dec,
				struct teei_encode **pdec_context)
{
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	struct teei_encode *dec_context = NULL;
	int session_found = 0;
	int enc_found = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
				if (temp_ses->sess_id == dec->session_id) {
					session_found = 1;
					break;
				}
			}
			break;
		}
	}

	if (0 == session_found) {
		pr_err("[%s][%d] session not found!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (dec->encode_id != -1) {
		list_for_each_entry(dec_context, &temp_ses->encode_list, head) {
			if (dec_context->encode_id == dec->encode_id) {
				enc_found = 1;
				break;
			}
		}
	}

	/* print_context(); */
	if (0 == enc_found) {
		pr_err("[%s][%d] encode[%x] not found!\n", __func__, __LINE__, dec->encode_id);
		return -EINVAL;
	}

	*pdec_context = dec_context;

	return 0;
}

/**
 * @brief		Decode the uint32 result
 *
 * @param argp
 *
 * @return		EFAULT:	fail to copy data from user space OR to user space.
 *			0:	success
 */
int teei_client_decode_uint32(void *private_data, void *argp)
{
	struct teei_client_encode_cmd dec;
	int retVal = 0;
	struct teei_encode *dec_context = NULL;

	if (copy_from_user(&dec, argp, sizeof(dec))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_decode(private_data, &dec, &dec_context);

	if (retVal != 0) {
		pr_err("[%s][%d] teei_client_prepare_decode failed!\n", __func__, __LINE__);
		goto return_func;
	}

	if ((dec_context->dec_res_pos <= dec_context->enc_res_pos) &&
	    (dec_context->meta[dec_context->dec_res_pos].type == TEEI_ENC_UINT32)) {
		if (dec_context->meta[dec_context->dec_res_pos].usr_addr) {
			dec.data = (void *)((uint64_t)(dec_context->meta[dec_context->dec_res_pos].usr_addr));
		}

		/* *(u32 *)dec.data = *((u32 *)((char *)dec_context->ker_res_data_addr + dec_context->dec_offset)); */
		/* chengxin modify */
		/*
		u32 value = 0;
		u64 addr = dec.data;
		void __user *pt = compat_ptr((unsigned int *)addr);
		value =  *((u32 *)((char *)dec_context->ker_res_data_addr + dec_context->dec_offset));
		copy_to_user(pt, &value, 4);
		*/
		unsigned int value1 = 0;

		if (((u32 *)dec.data) == NULL) {
			pr_err("[%s][%d] error decode dec.data addr11111 is NULL!\n", __func__, __LINE__);
		}

		if (((u32 *)((char *)dec_context->ker_res_data_addr + dec_context->dec_offset) == NULL)) {
			pr_err("[%s][%d] decode data decode addr11111 is NULL!\n", __func__, __LINE__);
		} else {
			value1 = *((u32 *)((char *)dec_context->ker_res_data_addr + dec_context->dec_offset));
			*(unsigned long *)dec.data = value1;
		}

		dec_context->dec_offset += sizeof(u32);
		dec_context->dec_res_pos++;
	}

	if (copy_to_user(argp, &dec, sizeof(dec))) {
		pr_err("[%s][%d] copy to user failed.\n", __func__, __LINE__);
		return -EFAULT;
	}

return_func:
	return retVal;
}


/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_decode_array_space(void *private_data, void *argp)
{
	struct teei_client_encode_cmd dec;
	int retVal = 0;
	struct teei_encode *dec_context = NULL;

	if (copy_from_user(&dec, argp, sizeof(dec))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_decode(private_data, &dec, &dec_context);

	if (retVal != 0)
		goto return_func;

	if ((dec_context->dec_res_pos <= dec_context->enc_res_pos) &&
	    (dec_context->meta[dec_context->dec_res_pos].type
	     == TEEI_ENC_ARRAY)) {
		if (dec_context->meta[dec_context->dec_res_pos].len >=
		    dec_context->meta[dec_context->dec_res_pos].ret_len) {
			if (dec_context->meta[dec_context->dec_res_pos].usr_addr)
				dec.data = (void *)dec_context->meta[dec_context->dec_res_pos].usr_addr;

			if (copy_to_user(dec.data, (char *)dec_context->ker_res_data_addr + dec_context->dec_offset,
					dec_context->meta[dec_context->dec_res_pos].ret_len)) {
				pr_err("[%s][%d] copy from user failed while copying array!\n", __func__, __LINE__);
				retVal = -EFAULT;
				goto return_func;
			}
		} else {

			pr_err("[%s][%d] buffer length is small. Length required %x and supplied length %x,pos %x ",
			       __func__, __LINE__,
			       dec_context->meta[dec_context->dec_res_pos].ret_len,
			       dec_context->meta[dec_context->dec_res_pos].len,
			       dec_context->dec_res_pos);

			retVal = -EFAULT;
			goto return_func;
		}

		dec.len = dec_context->meta[dec_context->dec_res_pos].ret_len;
		dec_context->dec_offset += dec_context->meta[dec_context->dec_res_pos].len;
		dec_context->dec_res_pos++;
	} else if ((dec_context->dec_res_pos <= dec_context->enc_res_pos) &&
			(dec_context->meta[dec_context->dec_res_pos].type == TEEI_MEM_REF)) {
		if (dec_context->meta[dec_context->dec_res_pos].len >=
		    dec_context->meta[dec_context->dec_res_pos].ret_len) {
			dec.data = (void *)dec_context->meta[dec_context->dec_res_pos].usr_addr;
			unsigned long pmem = *(u32 *)((char *)dec_context->ker_res_data_addr + dec_context->dec_offset);
			char *mem = NULL;
			unsigned long addr = (unsigned long)phys_to_virt(pmem);
			mem = (char *)addr;
			Invalidate_Dcache_By_Area((unsigned long)mem,
				(unsigned long)mem + dec_context->meta[dec_context->dec_res_pos].ret_len + 1);
		} else {

			pr_err("[%s][%d] buffer length is small. Length required %x and supplied length %x",
			       __func__, __LINE__,
			       dec_context->meta[dec_context->dec_res_pos].ret_len,
			       dec_context->meta[dec_context->dec_res_pos].len);

			retVal = -EFAULT;
			goto return_func;
		}

		dec.len = dec_context->meta[dec_context->dec_res_pos].ret_len;
		dec_context->dec_offset += sizeof(u32);
		dec_context->dec_res_pos++;
	}

	else {
		pr_err("[%s][%d] invalid data type or decoder at wrong position!\n", __func__, __LINE__);
		retVal = -EINVAL;
		goto return_func;
	}

	if (copy_to_user(argp, &dec, sizeof(dec))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		retVal = -EFAULT;
		goto return_func;
	}

return_func:
	pr_debug("[%s][%d] teei_client_decode_array_space end.\n", __func__, __LINE__);
	return retVal;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_get_decode_type(void *private_data, void *argp)
{
	struct teei_client_encode_cmd dec;
	int retVal = 0;
	struct teei_encode *dec_context = NULL;


	if (copy_from_user(&dec, argp, sizeof(dec))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	retVal = teei_client_prepare_decode(private_data, &dec, &dec_context);

	if (retVal != 0)
		return retVal;

	if (dec_context->dec_res_pos <= dec_context->enc_res_pos)
		dec.data = (unsigned long)dec_context->meta[dec_context->dec_res_pos].type;
	else
		return -EINVAL;

	if (copy_to_user(argp, &dec, sizeof(dec))) {
		pr_err("[%s][%d] copy to user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	return retVal;
}
/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_shared_mem_alloc(void *private_data, void *argp)
{
#if 0
	struct teei_shared_mem *temp_shared_mem = NULL;
	struct teei_session_shared_mem_info mem_info;

	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	int  session_found = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	if (copy_from_user(&mem_info, argp, sizeof(mem_info))) {
		pr_err("[%s][%d] copy from user failed!\n", __func__, __LINE__);
		return -EFAULT;
	}

	/*
	pr_debug("[%s][%d] service id 0x%lx session id 0x%lx user mem addr 0x%p.\n",
			__func__, __LINE__,
			mem_info.service_id,
			mem_info.session_id,
			mem_info.user_mem_addr);
	*/
	list_for_each_entry(temp_cont,
			&teei_contexts_head.context_list,
			link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
				if (temp_ses->sess_id == mem_info.session_id) {
					session_found = 1;
					break;
				}
			}
			break;
		}
	}

	if (session_found == 0) {
		pr_err("[%s][%d] session not found!\n", __func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(temp_shared_mem, &temp_cont->shared_mem_list, head) {
		if (temp_shared_mem->index == (void *)mem_info.user_mem_addr) {
			list_del(&temp_shared_mem->head);
			temp_cont->shared_mem_cnt--;
			list_add_tail(&temp_shared_mem->s_head, &temp_ses->shared_mem_list);
			break;
		}
	}
#endif

	return 0;
}

/**
 * @brief
 *
 * @param argp
 *
 * @return
 */
int teei_client_shared_mem_free(void *private_data, void *argp)
{
	struct teei_shared_mem *temp_shared_mem = NULL;
	struct teei_session_shared_mem_info mem_info;
	struct teei_context *temp_cont = NULL;
	struct teei_session *temp_ses = NULL;
	struct teei_shared_mem *temp_pos = NULL;
	int session_found = 0;
	unsigned long dev_file_id = (unsigned long)private_data;

	if (copy_from_user(&mem_info, argp, sizeof(mem_info))) {
		pr_err("[%s][%d] copy from user failed.\n", __func__, __LINE__);
		return -EFAULT;
	}

	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			pr_debug("found file id\n");
			list_for_each_entry_safe(temp_shared_mem,
						temp_pos,
						&temp_cont->shared_mem_list,
						head) {
				if (temp_shared_mem && temp_shared_mem->u_addr == mem_info.user_mem_addr) {
					list_del(&temp_shared_mem->head);

					if (temp_shared_mem->k_addr)
						free_pages((u64)temp_shared_mem->k_addr,
							get_order(ROUND_UP(temp_shared_mem->len, SZ_4K)));

					kfree(temp_shared_mem);
				}
			}
		}
	}

#if 0
	list_for_each_entry(temp_cont, &teei_contexts_head.context_list, link) {
		if (temp_cont->cont_id == dev_file_id) {
			list_for_each_entry(temp_ses, &temp_cont->sess_link, link) {
				pr_debug("list:session id %x\n", temp_ses->sess_id);

				if (temp_ses->sess_id == mem_info.session_id) {
					session_found = 1;
					break;
				}
			}
			break;
		}
	}

	if (session_found == 0) {
		pr_err("[%s][%d] session not found!\n", __func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(temp_shared_mem, &temp_ses->shared_mem_list, s_head) {
		if (temp_shared_mem && temp_shared_mem->index == (void *)mem_info.user_mem_addr) {
			list_del(&temp_shared_mem->s_head);

			if (temp_shared_mem->k_addr)
				free_pages((unsigned long)temp_shared_mem->k_addr,
						get_order(ROUND_UP(temp_shared_mem->len, SZ_4K)));

			kfree(temp_shared_mem);
			break;
		}
	}
#endif
	return 0;
}

static int teei_client_close_session_for_service_plus(void *private_data, struct teei_session *temp_ses)
{
	struct ser_ses_id *ses_close = (struct ser_ses_id *)tz_malloc_shared_mem(sizeof(struct ser_ses_id), GFP_KERNEL  | GFP_DMA);
	struct teei_context *curr_cont = NULL;
	struct teei_encode *temp_encode = NULL;
	struct teei_encode *enc_context = NULL;
	struct teei_shared_mem *shared_mem = NULL;
	struct teei_shared_mem *temp_shared = NULL;
	unsigned long dev_file_id = (unsigned long)private_data;
	int retVal = 0;
	int *res = (int *)tz_malloc_shared_mem(4, GFP_KERNEL | GFP_DMA);

	if (temp_ses == NULL)
		return -EINVAL;

	if (ses_close == NULL)
		return -ENOMEM;

	if (res == NULL)
		return -ENOMEM;

	ses_close->session_id = temp_ses->sess_id;
	pr_debug("======== ses_close->session_id = %d =========\n", ses_close->session_id);
	curr_cont = temp_ses->parent_cont;


	if (!list_empty(&temp_ses->encode_list)) {

		list_for_each_entry_safe(enc_context,
					temp_encode,
					&temp_ses->encode_list,
					head) {
			if (enc_context) {
				list_del(&enc_context->head);
				kfree(enc_context);
			}
		}
	}

	if (!list_empty(&temp_ses->shared_mem_list)) {

		list_for_each_entry_safe(shared_mem,
					temp_shared,
					&temp_ses->shared_mem_list,
					s_head) {

			list_del(&shared_mem->s_head);

			if (shared_mem->k_addr) {
				free_pages((unsigned long)shared_mem->k_addr,
					get_order(ROUND_UP(shared_mem->len, SZ_4K)));
			}

			kfree(shared_mem);
		}
	}

	list_del(&temp_ses->link);
	curr_cont->sess_cnt = curr_cont->sess_cnt - 1;

	kfree(temp_ses);
	tz_free_shared_mem(res, 4);
	tz_free_shared_mem(ses_close, sizeof(struct ser_ses_id));

	return 0;
}



int teei_client_close_session_for_service(
    void *private_data,
    struct teei_session *temp_ses)
{
	struct ser_ses_id *ses_close = (struct ser_ses_id *)tz_malloc_shared_mem(sizeof(struct ser_ses_id), GFP_KERNEL);
	struct teei_context *curr_cont = NULL;
	struct teei_encode *temp_encode = NULL;
	struct teei_encode *enc_context = NULL;
	struct teei_shared_mem *shared_mem = NULL;
	struct teei_shared_mem *temp_shared = NULL;
	unsigned long dev_file_id = (unsigned long)private_data;
	int retVal = 0;
	int *res = (int *)tz_malloc_shared_mem(4, GFP_KERNEL);
	int error_code = 0;

	if (temp_ses == NULL) {
		pr_err("[%s][%d] ======== temp_ses is NULL =========\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (ses_close == NULL) {
		pr_err("[%s][%d] ======== ses_close is NULL =========\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (res == NULL) {
		pr_err("[%s][%d] ======== res is NULL =========\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ses_close->session_id = temp_ses->sess_id;

	/* pr_err("======== ses_close->session_id = %d =========\n", ses_close->session_id); */

	curr_cont = temp_ses->parent_cont;

	retVal = teei_smc_call(TEEI_CMD_TYPE_CLOSE_SESSION,
				dev_file_id,
				0,
				TEEI_GLOBAL_CMD_ID_CLOSE_SESSION,
				0,
				0,
				ses_close,
				sizeof(struct ser_ses_id),
				res,
				sizeof(int),
				NULL,
				NULL,
				0,
				NULL,
				&error_code,
				&(curr_cont->cont_lock));

	if (!list_empty(&temp_ses->encode_list)) {

		list_for_each_entry_safe(enc_context,
					temp_encode,
					&temp_ses->encode_list,
					head) {
			if (enc_context) {
				list_del(&enc_context->head);
				kfree(enc_context);
			}
		}
	}

	if (!list_empty(&temp_ses->shared_mem_list)) {

		list_for_each_entry_safe(shared_mem,
					temp_shared,
					&temp_ses->shared_mem_list,
					s_head) {

			list_del(&shared_mem->s_head);

			if (shared_mem->k_addr)
				free_pages((unsigned long)shared_mem->k_addr,
						get_order(ROUND_UP(shared_mem->len, SZ_4K)));

			kfree(shared_mem);
		}
	}

	list_del(&temp_ses->link);
	curr_cont->sess_cnt = curr_cont->sess_cnt - 1;

	kfree(temp_ses);
	tz_free_shared_mem(res, 4);
	tz_free_shared_mem(ses_close, sizeof(struct ser_ses_id));

	return 0;
}


/**
 * @brief	close the context with context_ID equal private_data
 *
 * @return	EINVAL: Can not find the context with ID equal private_data
 *		0: success
 */
int teei_client_service_exit(void *private_data)
{
	struct teei_shared_mem *temp_shared_mem = NULL;
	struct teei_shared_mem *temp_pos = NULL;
	struct teei_context *temp_context = NULL;
	struct teei_context *temp_context_pos = NULL;
	struct teei_session *temp_ses = NULL;
	struct teei_session *temp_ses_pos = NULL;
	unsigned long dev_file_id = 0;

	dev_file_id = (unsigned long)(private_data);
	down_write(&(teei_contexts_head.teei_contexts_sem));

	list_for_each_entry_safe(temp_context,
				temp_context_pos,
				&teei_contexts_head.context_list,
				link) {
		if (temp_context->cont_id == dev_file_id) {

			list_for_each_entry_safe(temp_shared_mem,
						temp_pos,
						&temp_context->shared_mem_list,
						head) {
				if (temp_shared_mem) {
					list_del(&(temp_shared_mem->head));

					if (temp_shared_mem->k_addr) {
						free_pages((unsigned long)temp_shared_mem->k_addr,
								get_order(ROUND_UP(temp_shared_mem->len, SZ_4K)));
					}

					kfree(temp_shared_mem);
				}
			}

			if (!list_empty(&temp_context->sess_link)) {
				list_for_each_entry_safe(temp_ses,
							temp_ses_pos,
							&temp_context->sess_link,
							link)
				teei_client_close_session_for_service_plus(private_data, temp_ses);
			}

			list_del(&temp_context->link);
			kfree(temp_context);
			up_write(&(teei_contexts_head.teei_contexts_sem));
			return 0;
		}
	}

	return -EINVAL;
}
