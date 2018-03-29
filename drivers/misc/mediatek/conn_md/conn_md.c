/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include "conn_md.h"
#include "conn_md_dbg.h"

/*global data structure defination*/

CONN_MD_STRUCT g_conn_md;

static int _conn_md_del_msg_by_uid(uint32 u_id);
static int conn_md_dmp_msg(P_CONN_MD_QUEUE p_msg_list, uint32 src_id, uint32 dst_id);

int conn_md_add_user(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops)
{
	P_CONN_MD_USER p_user = NULL;
	struct list_head *pos = NULL;
	P_CONN_MD_STRUCT p_conn_md = &g_conn_md;
	P_CONN_MD_USER_LIST p_user_list = &p_conn_md->user_list;

	/*lock user_list_lock */
	mutex_lock(&p_user_list->lock);

	/*check if earlier uid reged or not */
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, CONN_MD_USER, entry);
		if (p_user->u_id == u_id) {
			/*if yes */
			/*print warning information */
			CONN_MD_WARN_FUNC("uid (0x%08x) is already registered, updating with newer one\n", u_id);
			break;
		}
		p_user = NULL;
	}

	if (NULL == p_user) {
		/*memory allocation for user information */
		p_user = kmalloc(sizeof(CONN_MD_USER), GFP_ATOMIC);
		INIT_LIST_HEAD(&p_user->entry);
		list_add_tail(&p_user->entry, &p_user_list->list);
		p_user->u_id = u_id;
		p_user->state = USER_REGED;
	}

	/*anyway, write user info to target uid */
	memcpy(&p_user->ops, p_ops, sizeof(CONN_MD_BRIDGE_OPS));

	p_user->state = USER_ENABLED;
	/*unlock user_list lock */
	mutex_unlock(&p_user_list->lock);

	CONN_MD_WARN_FUNC("uid (0x%08x) is added to user list successfully\n", p_user->u_id);

	return 0;
}

int conn_md_del_user(uint32 u_id)
{
	int i_ret = -1;
	P_CONN_MD_USER p_user = NULL;
	struct list_head *pos = NULL;
	P_CONN_MD_STRUCT p_conn_md = &g_conn_md;
	P_CONN_MD_USER_LIST p_user_list = &p_conn_md->user_list;

	/*lock user_list_lock */
	mutex_lock(&p_user_list->lock);

	/*check if earlier uid reged or not */
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, CONN_MD_USER, entry);
		if (p_user->u_id == u_id) {
			/*if yes */
			/*print information */
			CONN_MD_INFO_FUNC("uid (0x%08x) is registered, delete it\n", u_id);
			break;
		}
		p_user = NULL;
	}

	if (NULL == p_user) {
		i_ret = CONN_MD_ERR_INVALID_PARAM;
		/*print warning information */
		CONN_MD_WARN_FUNC("uid (0x%08x) not found in user list..\n", u_id);
	} else {
		/*delete user info from user info list of target uid */
		list_del(pos);
		kfree(p_user);
		p_user_list->counter--;
		CONN_MD_INFO_FUNC("uid (0x%08x) is deleted\n", u_id);
		i_ret = 0;
	}

	/*unlock user_list lock */
	mutex_unlock(&p_user_list->lock);

	/*search all message enquued in p_msg_list and delete them before delete user */
	_conn_md_del_msg_by_uid(u_id);

	return i_ret;
}

int conn_md_dmp_msg(P_CONN_MD_QUEUE p_msg_list, uint32 src_id, uint32 dst_id)
{
#define MAX_LENGTH_PER_PACKAGE 8

	struct list_head *pos = NULL;
	P_CONN_MD_MSG p_msg = NULL;
	int i = 0;
	int counter = 0;

	if (NULL == p_msg_list) {
		CONN_MD_ERR_FUNC("invalid parameter, p_msg_list:0x%08x\n", p_msg_list);
		return CONN_MD_ERR_INVALID_PARAM;
	}

	mutex_lock(&p_msg_list->lock);

	list_for_each(pos, &p_msg_list->list) {
		p_msg = container_of(pos, CONN_MD_MSG, entry);
		if (((0 == src_id) || (src_id == p_msg->ilm.src_mod_id)) &&
		    ((0 == dst_id) || (dst_id == p_msg->ilm.dest_mod_id))) {
			counter++;
			CONN_MD_INFO_FUNC
			    ("p_msg:0x%08x, src_id:0x%08x, dest_id:0x%08x, msg_len:%d\n", p_msg,
			     p_msg->ilm.src_mod_id, p_msg->ilm.dest_mod_id, p_msg->local_para.msg_len);
			for (i = 0; (i < p_msg->local_para.msg_len) && (i < MAX_LENGTH_PER_PACKAGE); i++) {
				CONN_MD_INFO_FUNC("%02x ", p_msg->local_para.data[i]);
				if (7 == (i % 8))
					CONN_MD_INFO_FUNC("\n");
			}
			CONN_MD_INFO_FUNC("\n");
		}
	}

	mutex_unlock(&p_msg_list->lock);

	CONN_MD_INFO_FUNC("%d messages found in message list\n", counter);
	return 0;
}

int _conn_md_del_msg_by_uid(uint32 u_id)
{
	/*only delete messaged enqueued in queue list, do not care message in active queue list */
	P_CONN_MD_STRUCT p_conn_md = &g_conn_md;
	P_CONN_MD_QUEUE p_msg_list = &p_conn_md->msg_queue;
	struct list_head *pos = NULL;
	P_CONN_MD_MSG p_msg = NULL;
	int flag = 1;

	CONN_MD_TRC_FUNC();
	mutex_lock(&p_msg_list->lock);
	while (flag) {
		/*set flat to 0 before search message */
		flag = 0;

		list_for_each(pos, &p_msg_list->list) {
			p_msg = container_of(pos, CONN_MD_MSG, entry);
			if ((p_msg->ilm.dest_mod_id == u_id) || (p_msg->ilm.src_mod_id == u_id)) {
				flag = 1;
				CONN_MD_DBG_FUNC
				    ("message for uid(0x%08x) found in queue list, dest_id(0x%08x), src_id(0x%08x)\n",
				     u_id, p_msg->ilm.dest_mod_id, p_msg->ilm.src_mod_id);
				break;
			}
		}

		if (1 == flag) {
			p_msg_list->counter--;
			list_del(pos);
			kfree(p_msg);
			CONN_MD_DBG_FUNC("dequeued in queue list, counter:%d\n", p_msg_list->counter);
		}

	}
	mutex_unlock(&p_msg_list->lock);
	CONN_MD_TRC_FUNC();
	return 0;
}

int conn_md_send_msg(ipc_ilm_t *ilm)
{

	P_CONN_MD_STRUCT p_conn_md = &g_conn_md;
	P_CONN_MD_QUEUE p_msg_list = &p_conn_md->msg_queue;
	uint32 msg_str_len = 0;
	local_para_struct *p_local_para = NULL;
	P_CONN_MD_MSG p_new_msg = NULL;
	uint32 msg_info_len = ilm->local_para_ptr->msg_len;

	CONN_MD_DBG_FUNC("ilm:0x%08x, msg_len:%d\n", ilm, ilm->local_para_ptr->msg_len);

	/*malloc message structure for this msg */
	msg_str_len = sizeof(CONN_MD_MSG) + msg_info_len;
	p_new_msg = kmalloc(msg_str_len, GFP_ATOMIC);

	if (NULL != p_new_msg) {
		CONN_MD_DBG_FUNC("p_new_msg:0x%08x\n", p_new_msg);
		/*copy message from ilm */
		memcpy(p_new_msg, ilm, sizeof(ipc_ilm_t));

		p_local_para = &p_new_msg->local_para;
		p_new_msg->ilm.local_para_ptr = p_local_para;
		/*copy local_para_ptr structure */
		memcpy(p_local_para, ilm->local_para_ptr, sizeof(local_para_struct));
		/*copy data from local_para_ptr structure */
		memcpy(p_local_para->data, ilm->local_para_ptr->data, msg_info_len);

		CONN_MD_DBG_FUNC("p_local_para:0x%08x, msg_len:%d\n", p_local_para, p_local_para->msg_len);

		INIT_LIST_HEAD(&p_new_msg->entry);

		/*lock tx queue lock */
		mutex_lock(&p_msg_list->lock);
		/*enqueue tx message */
		list_add_tail(&p_new_msg->entry, &p_msg_list->list);

		p_msg_list->counter++;

		/*unlock queue lock */
		mutex_unlock(&p_msg_list->lock);

		CONN_MD_DBG_FUNC
		    ("enqueue new message to msg queue list succeed, enqueued msg counter:%d\n", p_msg_list->counter);

		conn_md_dmp_in(ilm, MSG_ENQUEUE, p_conn_md->p_msg_dmp_sys);

		CONN_MD_DBG_FUNC("begin to wake up conn-md-thread\n");

		/*wakeup conn-md thread to handle tx message */
		complete(&p_conn_md->tx_comp);

		CONN_MD_DBG_FUNC("wake up conn-md-thread done\n");
	} else {
		CONN_MD_ERR_FUNC("kmalloc for new message structure failed\n");
	}

	return 0;
}

#define ACT_QUEUE_DBG 0

static int conn_md_thread(void *p_data)
{
	P_CONN_MD_STRUCT p_conn_md = (P_CONN_MD_STRUCT) p_data;
	P_CONN_MD_QUEUE p_act_queue = &p_conn_md->act_queue;
	P_CONN_MD_QUEUE p_msg_queue = &p_conn_md->msg_queue;
	struct list_head *pos = NULL;
	P_CONN_MD_MSG p_msg = NULL;
	P_CONN_MD_USER p_user = NULL;
	struct list_head *p_user_pos = NULL;
	P_CONN_MD_USER_LIST p_user_list = &p_conn_md->user_list;
	ipc_ilm_t *p_cur_ilm = NULL;

	while (1) {
		wait_for_completion_interruptible(&p_conn_md->tx_comp);

		if (kthread_should_stop()) {
			CONN_MD_WARN_FUNC("conn-md-thread stoping ...\n");
			break;
		}

		/*check if p_conn_md->msg_queue is empty or not */
		mutex_lock(&p_msg_queue->lock);
		if (!list_empty(&p_msg_queue->list)) {
			/*if not empty, remove all list structure to list of p_conn_md->act_queue */
			mutex_lock(&p_act_queue->lock);
			if (!list_empty(&p_act_queue->list)) {
				/*warning message, this should never happen!!! */
				CONN_MD_ERR_FUNC
				    ("p_act_queue list is not empty, this should never happen!!!---*?*---!!!\n");
			}

			/*ignore case of p_act_queue is not empty */
			list_replace_init(&p_msg_queue->list, &p_act_queue->list);

			mutex_unlock(&p_act_queue->lock);

		} else {
			/*warning message */
			CONN_MD_DBG_FUNC("no msg queued in msg queue...\n");
		}
		mutex_unlock(&p_msg_queue->lock);

		mutex_lock(&p_act_queue->lock);
		/*dequeue from p_act_queue */
		list_for_each(pos, &p_act_queue->list) {
			p_msg = container_of(pos, CONN_MD_MSG, entry);
			p_cur_ilm = &p_msg->ilm;
			if (NULL == p_cur_ilm) {
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);

				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n", p_act_queue->counter);
#endif
				continue;
			}

			conn_md_dmp_in(p_cur_ilm, MSG_DEQUEUE, p_conn_md->p_msg_dmp_sys);

			mutex_lock(&p_user_list->lock);

			/*check if src module is enabled or not */
			list_for_each(p_user_pos, &p_user_list->list) {
				p_user = container_of(p_user_pos, CONN_MD_USER, entry);
				if (p_user->u_id == p_cur_ilm->src_mod_id && p_user->state == USER_ENABLED) {
					/*src module id is enabled already */
					CONN_MD_DBG_FUNC("source user id (0x%08x) found\n", p_cur_ilm->src_mod_id);
					break;
				}
				p_user = NULL;
			}

			if (NULL == p_user) {
				mutex_unlock(&p_user_list->lock);
				CONN_MD_WARN_FUNC
				    ("source user id (0x%08x) is not registered or not enabled yet, drop ilm msg\n",
				     p_cur_ilm->src_mod_id);
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n", p_act_queue->counter);
#endif

				continue;
			}

			/*check if destination module is enabled or not */
			list_for_each(p_user_pos, &p_user_list->list) {
				p_user = container_of(p_user_pos, CONN_MD_USER, entry);
				if (p_user->u_id == p_cur_ilm->dest_mod_id && p_user->state == USER_ENABLED) {
					CONN_MD_DBG_FUNC("target user id (0x%08x) found\n", p_cur_ilm->dest_mod_id);
					/*src module id is enabled already */
					break;
				}
				p_user = NULL;
			}

			if (NULL == p_user) {
				mutex_unlock(&p_user_list->lock);

				CONN_MD_WARN_FUNC
				    ("target user id (0x%08x) is not registered or enabled yet, drop ilm msg\n",
				     p_cur_ilm->dest_mod_id);
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n", p_act_queue->counter);
#endif
				continue;
			}
			CONN_MD_DBG_FUNC("p_cur_ilm:0x%08x, local_para_ptr:0x%08x, msg_len:%d\n",
					 p_cur_ilm, &p_cur_ilm->local_para_ptr, p_cur_ilm->local_para_ptr->msg_len);
			CONN_MD_DBG_FUNC("sending message to user id (0x%08x)\n", p_cur_ilm->dest_mod_id);
			/*send package to dest module by call corresponding rx callback function */
			(*(p_user->ops.rx_cb)) (p_cur_ilm);
			CONN_MD_DBG_FUNC("message sent to user id (0x%08x) done\n", p_cur_ilm->dest_mod_id);
			mutex_unlock(&p_user_list->lock);

#if (ACT_QUEUE_DBG == 1)
			/*free message structure */
			list_del(pos);
			kfree(p_msg);
			CONN_MD_DBG_FUNC("message structure freed\n");

			p_act_queue->counter++;
			CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n", p_act_queue->counter);
#endif
		}
		p_msg = NULL;

		while (!list_empty(&p_act_queue->list)) {
			list_for_each(pos, &p_act_queue->list) {
				p_msg = container_of(pos, CONN_MD_MSG, entry);
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_msg = NULL;
				CONN_MD_DBG_FUNC("message structure freed\n");

				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n", p_act_queue->counter);
				break;
			}

		}

		mutex_unlock(&p_act_queue->lock);

	}
	CONN_MD_WARN_FUNC("conn-md-thread stopped.\n");
	return 0;
}

int conn_md_dmp_msg_queued(uint32 src_id, uint32 dst_id)
{
	return conn_md_dmp_msg(&g_conn_md.msg_queue, src_id, dst_id);
}

int conn_md_dmp_msg_active(uint32 src_id, uint32 dst_id)
{
	return conn_md_dmp_msg(&g_conn_md.act_queue, src_id, dst_id);
}

int conn_md_dmp_msg_logged(uint32 src_id, uint32 dst_id)
{
	return conn_md_dmp_out(g_conn_md.p_msg_dmp_sys, src_id, dst_id);
}

static int conn_md_init(void)
{
	int i_ret = -1;
	P_CONN_MD_QUEUE p_queue = NULL;
	P_CONN_MD_USER_LIST p_user_list = NULL;

	CONN_MD_TRC_FUNC();

	/*init message queue structure */
	p_queue = &g_conn_md.msg_queue;
	INIT_LIST_HEAD(&(p_queue->list));
	mutex_init(&(p_queue->lock));
	p_queue->counter = 0;
	CONN_MD_INFO_FUNC("init message queue list succeed\n");

	/*init active queue structure */
	p_queue = &g_conn_md.act_queue;
	INIT_LIST_HEAD(&(p_queue->list));
	mutex_init(&(p_queue->lock));
	p_queue->counter = 0;
	CONN_MD_INFO_FUNC("init active queue list succeed\n");

	/*init user_list structure */
	p_user_list = &g_conn_md.user_list;
	INIT_LIST_HEAD(&(p_user_list->list));
	mutex_init(&(p_user_list->lock));
	p_user_list->counter = 0;
	CONN_MD_INFO_FUNC("init user information list succeed\n");

	g_conn_md.p_msg_dmp_sys = conn_md_dmp_init();
	if (NULL == g_conn_md.p_msg_dmp_sys)
		CONN_MD_WARN_FUNC("conn_md_dmp_init failed\n");
	else
		CONN_MD_INFO_FUNC("conn_md_dmp_init succeed\n");
	/*init proc interface */
	conn_md_dbg_init();

	/*create conn-md thread */

	init_completion(&g_conn_md.tx_comp);
	g_conn_md.p_task = kthread_create(conn_md_thread, &g_conn_md, "conn-md-thread");
	if (NULL == g_conn_md.p_task) {
		CONN_MD_ERR_FUNC("create conn_md_thread fail\n");
		i_ret = -ENOMEM;
		conn_md_dmp_deinit(g_conn_md.p_msg_dmp_sys);
		goto conn_md_err;
	}
	CONN_MD_INFO_FUNC("create conn_md_thread succeed, wakeup it\n");
	/*wakeup conn_md_thread */
	wake_up_process(g_conn_md.p_task);

conn_md_err:

	CONN_MD_TRC_FUNC();
	return i_ret;
}

static void conn_md_exit(void)
{

	P_CONN_MD_STRUCT p_conn_md = &g_conn_md;
	P_CONN_MD_QUEUE p_queue = NULL;
	P_CONN_MD_USER_LIST p_user_list = &p_conn_md->user_list;
	struct list_head *pos = NULL;
	P_CONN_MD_USER p_user = NULL;
	P_CONN_MD_MSG p_msg = NULL;

	CONN_MD_TRC_FUNC();

	/*terminate conn-md thread */
	if (NULL != p_conn_md->p_task) {
		CONN_MD_INFO_FUNC("signaling conn-md-thread to stop ...\n");
		kthread_stop(p_conn_md->p_task);
	}

	/*delete user_list structure if user list is not empty */
	mutex_lock(&p_user_list->lock);
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, CONN_MD_USER, entry);
		list_del(pos);
		kfree(p_user);
	}
	p_user_list->counter = 0;
	mutex_unlock(&p_user_list->lock);
	mutex_destroy(&p_user_list->lock);

	/*delete queue structure if message queue list is empty */
	p_queue = &p_conn_md->msg_queue;
	mutex_lock(&p_queue->lock);
	list_for_each(pos, &p_queue->list) {
		p_msg = container_of(pos, CONN_MD_MSG, entry);
		list_del(pos);
		kfree(p_msg);
	}
	p_queue->counter = 0;
	mutex_unlock(&p_queue->lock);
	mutex_destroy(&p_queue->lock);

	/*delete queue structure if active queue list is empty */
	p_queue = &p_conn_md->act_queue;
	mutex_lock(&p_queue->lock);
	list_for_each(pos, &p_queue->list) {
		p_msg = container_of(pos, CONN_MD_MSG, entry);
		list_del(pos);
		kfree(p_msg);
	}
	p_queue->counter = 0;
	mutex_unlock(&p_queue->lock);
	mutex_destroy(&p_queue->lock);

	if (NULL != p_conn_md->p_msg_dmp_sys)
		conn_md_dmp_deinit(p_conn_md->p_msg_dmp_sys);

	CONN_MD_TRC_FUNC();
}

/*---------------------------------------------------------------------------*/

/*
module_init(conn_md_init);
module_exit(conn_md_exit);
*/
subsys_initcall(conn_md_init);
module_exit(conn_md_exit);
/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("MBJ/WCN/SE/SS1/Chaozhong.Liang");
MODULE_DESCRIPTION("MTK CONN-MD Bridge Driver$1.0$");
MODULE_LICENSE("GPL");

/*---------------------------------------------------------------------------*/
