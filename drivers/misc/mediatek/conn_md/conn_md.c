// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "conn_md.h"
#include "conn_md_dbg.h"
#include <linux/sched/clock.h>

/*global data structure defination*/
struct conn_md_struct g_conn_md;
static struct conn_md_log_msg_info g_log_msg_info;

static int _conn_md_del_msg_by_uid(uint32 u_id);
static int conn_md_dmp_msg(struct conn_md_queue *p_msg_list, uint32 src_id,
		uint32 dst_id);

static unsigned long conn_md_log_msg_interval_ms(
	struct conn_md_time_struct *p_begin,
	struct conn_md_time_struct *p_end)
{
	long long diff_sec;
	long diff_msec;

	diff_sec = p_end->sec - p_begin->sec;
	diff_msec = (p_end->msec - p_begin->msec);
	if (diff_msec < 0) {
		diff_msec += 1000;
		diff_sec -= 1;
	}

	return (diff_sec * 1000 + diff_msec);
}

static void conn_md_get_local_time(struct conn_md_time_struct *time)
{
	time->sec = local_clock();
	time->msec = do_div(time->sec, 1000000000) / 1000000;
}

static void conn_md_log_add_msg(struct conn_md_time_struct *cur_time)
{
	char buf[CONN_MD_MSG_TIME_LENGTH];
	int msg_buf_size, remain_size;

	if (g_log_msg_info.msg_total == 0)
		g_log_msg_info.msg_begin_time = *cur_time;

	snprintf(buf, CONN_MD_MSG_TIME_LENGTH, " %llu.%03lu",
		 cur_time->sec, cur_time->msec);

	msg_buf_size = strlen(buf);
	remain_size = CONN_MD_BUF_SIZE - strlen(g_log_msg_info.msg_buf) - 1;
	if (remain_size >= msg_buf_size) {
		strncat(g_log_msg_info.msg_buf, buf, msg_buf_size);
		g_log_msg_info.msg_total++;
	} else {
		CONN_MD_ERR_FUNC("buff full, %s (remain %d), cant add %s (%d)",
				 g_log_msg_info.msg_buf, remain_size, buf,
				 msg_buf_size);
	}
}

#define CONN_MD_LOG_MSG_HEAD_NAME "send message to Modem,"
#define CONN_MD_LOG_MSG_MAX_INTERVAL_MS 1000
static void conn_md_log_print_msg(struct conn_md_time_struct *cur_time)
{
	unsigned long interval;

	if (g_log_msg_info.msg_total > 0) {
		interval = conn_md_log_msg_interval_ms(
			&g_log_msg_info.msg_begin_time, cur_time);
		if ((g_log_msg_info.msg_total == CONN_MD_MSG_MAX_NUM ||
		    interval > CONN_MD_LOG_MSG_MAX_INTERVAL_MS)) {
			CONN_MD_INFO_FUNC("%s%s\n", CONN_MD_LOG_MSG_HEAD_NAME,
					  g_log_msg_info.msg_buf);
			g_log_msg_info.msg_buf[0] = '\0';
			g_log_msg_info.msg_total = 0;
		}
	}
}

static void conn_md_log_msg_time(uint32 dest_id)
{
	struct conn_md_time_struct cur_time;

	conn_md_get_local_time(&cur_time);

	if (dest_id == MD_MOD_EL1)
		conn_md_log_add_msg(&cur_time);

	conn_md_log_print_msg(&cur_time);
}

int conn_md_add_user(uint32 u_id, struct conn_md_bridge_ops *p_ops)
{
	struct conn_md_user *p_user = NULL;
	struct list_head *pos = NULL;
	struct conn_md_struct *p_conn_md = &g_conn_md;
	struct conn_md_user_list *p_user_list = &p_conn_md->user_list;

	/*lock user_list_lock */
	mutex_lock(&p_user_list->lock);

	/*check if earlier uid reged or not */
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, struct conn_md_user, entry);
		if (p_user->u_id == u_id) {
			/*if yes */
			/*print warning information */
			CONN_MD_WARN_FUNC("uid (0x%08x) registered, updating\n",
					u_id);
			break;
		}
		p_user = NULL;
	}

	if (p_user == NULL) {
		/*memory allocation for user information */
		p_user = kmalloc(sizeof(struct conn_md_user), GFP_ATOMIC);
		if (p_user == NULL) {
			CONN_MD_ERR_FUNC("kmalloc failed\n");
			mutex_unlock(&p_user_list->lock);
			return CONN_MD_ERR_OTHERS;
		}
		INIT_LIST_HEAD(&p_user->entry);
		list_add_tail(&p_user->entry, &p_user_list->list);
		p_user->u_id = u_id;
		p_user->state = USER_REGED;
	}

	/*anyway, write user info to target uid */
	memcpy(&p_user->ops, p_ops, sizeof(struct conn_md_bridge_ops));

	p_user->state = USER_ENABLED;

	CONN_MD_WARN_FUNC("uid (0x%08x) is added to user list successfully\n",
			p_user->u_id);
	/*unlock user_list lock */
	mutex_unlock(&p_user_list->lock);

	return 0;
}

int conn_md_del_user(uint32 u_id)
{
	int i_ret = -1;
	struct conn_md_user *p_user = NULL;
	struct list_head *pos = NULL;
	struct conn_md_struct *p_conn_md = &g_conn_md;
	struct conn_md_user_list *p_user_list = &p_conn_md->user_list;

	/*lock user_list_lock */
	mutex_lock(&p_user_list->lock);

	/*check if earlier uid reged or not */
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, struct conn_md_user, entry);
		if (p_user->u_id == u_id) {
			/*if yes */
			/*print information */
			CONN_MD_INFO_FUNC("uid(0x%08x) registered, delete it\n",
					u_id);
			break;
		}
		p_user = NULL;
	}

	if (p_user == NULL) {
		i_ret = CONN_MD_ERR_INVALID_PARAM;
		/*print warning information */
		CONN_MD_WARN_FUNC("uid (0x%08x) not found in user list..\n",
				u_id);
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

	/*search all message enquued in p_msg_list */
	/* and delete them before delete user */
	_conn_md_del_msg_by_uid(u_id);

	return i_ret;
}

int conn_md_dmp_msg(struct conn_md_queue *p_msg_list, uint32 src_id,
		uint32 dst_id)
{
#define MAX_LENGTH_PER_PACKAGE 8

	struct list_head *pos = NULL;
	struct conn_md_msg *p_msg = NULL;
	int i = 0;
	int counter = 0;

	if (p_msg_list == NULL) {
		CONN_MD_ERR_FUNC("invalid parameter, p_msg_list is NULL\n");
		return CONN_MD_ERR_INVALID_PARAM;
	}

	mutex_lock(&p_msg_list->lock);

	list_for_each(pos, &p_msg_list->list) {
		p_msg = container_of(pos, struct conn_md_msg, entry);
		if (((src_id == 0) || (src_id == p_msg->ilm.src_mod_id)) &&
		    ((dst_id == 0) || (dst_id == p_msg->ilm.dest_mod_id))) {
			counter++;
			CONN_MD_INFO_FUNC
			("p_msg:%p, src:0x%08x, dest:0x%08x, msg_len:%d\n",
					p_msg, p_msg->ilm.src_mod_id,
					p_msg->ilm.dest_mod_id,
					p_msg->local_para.msg_len);
			for (i = 0; (i < p_msg->local_para.msg_len) &&
					(i < MAX_LENGTH_PER_PACKAGE); i++) {
				CONN_MD_INFO_FUNC("%02x ",
						p_msg->local_para.data[i]);
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
	struct conn_md_struct *p_conn_md = &g_conn_md;
	struct conn_md_queue *p_msg_list = &p_conn_md->msg_queue;
	struct list_head *pos = NULL;
	struct conn_md_msg *p_msg = NULL;
	int flag = 1;

	CONN_MD_TRC_FUNC();
	mutex_lock(&p_msg_list->lock);
	while (flag) {
		/*set flat to 0 before search message */
		flag = 0;

		list_for_each(pos, &p_msg_list->list) {
			p_msg = container_of(pos, struct conn_md_msg, entry);
			if ((p_msg->ilm.dest_mod_id == u_id) ||
					(p_msg->ilm.src_mod_id == u_id)) {
				flag = 1;
				CONN_MD_DBG_FUNC
				("uid(0x%08x)found,dest(0x%08x),src(0x%08x)\n",
						u_id, p_msg->ilm.dest_mod_id,
						p_msg->ilm.src_mod_id);
				break;
			}
		}

		if (flag == 1) {
			p_msg_list->counter--;
			list_del(pos);
			kfree(p_msg);
			CONN_MD_DBG_FUNC("dequeued in queue list_counter:%d\n",
					p_msg_list->counter);
		}

	}
	mutex_unlock(&p_msg_list->lock);
	CONN_MD_TRC_FUNC();
	return 0;
}

int conn_md_send_msg(struct ipc_ilm *ilm)
{

	struct conn_md_struct *p_conn_md = &g_conn_md;
	struct conn_md_queue *p_msg_list = &p_conn_md->msg_queue;
	uint32 msg_str_len = 0;
	struct local_para *p_local_para = NULL;
	struct conn_md_msg *p_new_msg = NULL;
	uint32 msg_info_len = ilm->local_para_ptr->msg_len;

	CONN_MD_DBG_FUNC("ilm:%p, msg_len:%d\n", ilm,
			ilm->local_para_ptr->msg_len);

	/*malloc message structure for this msg */
	msg_str_len = sizeof(struct conn_md_msg) + msg_info_len;
	p_new_msg = kmalloc(msg_str_len, GFP_ATOMIC);

	if (p_new_msg != NULL) {
		CONN_MD_DBG_FUNC("p_new_msg:%p\n", p_new_msg);
		/*copy message from ilm */
		memcpy(p_new_msg, ilm, sizeof(struct ipc_ilm));

		p_local_para = &p_new_msg->local_para;
		p_new_msg->ilm.local_para_ptr = p_local_para;
		/*copy local_para_ptr structure */
		memcpy(p_local_para, ilm->local_para_ptr,
				sizeof(struct local_para));
		/*copy data from local_para_ptr structure */
		memcpy(p_local_para->data, ilm->local_para_ptr->data,
				msg_info_len - sizeof(struct local_para));

		CONN_MD_DBG_FUNC("p_local_para:%p, msg_len:%d\n",
				 p_local_para, p_local_para->msg_len);

		INIT_LIST_HEAD(&p_new_msg->entry);

		/*lock tx queue lock */
		mutex_lock(&p_msg_list->lock);
		/*enqueue tx message */
		list_add_tail(&p_new_msg->entry, &p_msg_list->list);

		p_msg_list->counter++;

		/*unlock queue lock */
		mutex_unlock(&p_msg_list->lock);

		CONN_MD_DBG_FUNC
		    ("enqueue new message ist succeed, msg counter:%d\n",
			p_msg_list->counter);

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
	struct conn_md_struct *p_conn_md = (struct conn_md_struct *) p_data;
	struct conn_md_queue *p_act_queue = &p_conn_md->act_queue;
	struct conn_md_queue *p_msg_queue = &p_conn_md->msg_queue;
	struct list_head *pos = NULL;
	struct conn_md_msg *p_msg = NULL;
	struct conn_md_user *p_user = NULL;
	struct list_head *p_user_pos = NULL;
	struct conn_md_user_list *p_user_list = &p_conn_md->user_list;
	struct ipc_ilm *p_cur_ilm = NULL;

	while (1) {
		wait_for_completion_interruptible(&p_conn_md->tx_comp);

		if (kthread_should_stop()) {
			CONN_MD_WARN_FUNC("conn-md-thread stopping ...\n");
			break;
		}

		/*check if p_conn_md->msg_queue is empty or not */
		mutex_lock(&p_msg_queue->lock);
		if (!list_empty(&p_msg_queue->list)) {
			mutex_lock(&p_act_queue->lock);
			if (!list_empty(&p_act_queue->list)) {
				/* warning message, this should never happen */
				CONN_MD_ERR_FUNC
				    ("this should never happen!!!--*-!!!\n");
			}

			/*ignore case of p_act_queue is not empty */
			list_replace_init(&p_msg_queue->list,
					&p_act_queue->list);

			mutex_unlock(&p_act_queue->lock);

		} else {
			/*warning message */
			CONN_MD_DBG_FUNC("no msg queued in msg queue...\n");
		}
		mutex_unlock(&p_msg_queue->lock);

		mutex_lock(&p_act_queue->lock);
		/*dequeue from p_act_queue */
		list_for_each(pos, &p_act_queue->list) {
			p_msg = container_of(pos, struct conn_md_msg, entry);
			p_cur_ilm = &p_msg->ilm;
			if (p_cur_ilm == NULL) {
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);

				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeue act_q counter:%d\n",
						p_act_queue->counter);
#endif
				continue;
			}

			conn_md_dmp_in(p_cur_ilm, MSG_DEQUEUE,
					p_conn_md->p_msg_dmp_sys);

			mutex_lock(&p_user_list->lock);

			/*check if src module is enabled or not */
			list_for_each(p_user_pos, &p_user_list->list) {
				p_user = container_of(p_user_pos,
						struct conn_md_user, entry);
				if (p_user->u_id == p_cur_ilm->src_mod_id &&
						p_user->state == USER_ENABLED) {
					/*src module id is enabled already */
					CONN_MD_DBG_FUNC
					("source user id (0x%08x) found\n",
							p_cur_ilm->src_mod_id);
					break;
				}
				p_user = NULL;
			}

			if (p_user == NULL) {
				mutex_unlock(&p_user_list->lock);
				CONN_MD_WARN_FUNC
				    ("user (0x%08x) is ready, drop ilm msg\n",
				     p_cur_ilm->src_mod_id);
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeued act_q counter:%d\n",
						p_act_queue->counter);
#endif

				continue;
			}

			/*check if destination module is enabled or not */
			list_for_each(p_user_pos, &p_user_list->list) {
				p_user = container_of(p_user_pos,
						struct conn_md_user, entry);
				if (p_user->u_id == p_cur_ilm->dest_mod_id &&
						p_user->state == USER_ENABLED) {
					CONN_MD_DBG_FUNC
					("target user id (0x%08x) found\n",
							p_cur_ilm->dest_mod_id);
					/*src module id is enabled already */
					break;
				}
				p_user = NULL;
			}

			if (p_user == NULL) {
				mutex_unlock(&p_user_list->lock);

				CONN_MD_WARN_FUNC
				    ("user (0x%08x) not ready, drop ilm msg\n",
				     p_cur_ilm->dest_mod_id);
#if (ACT_QUEUE_DBG == 1)
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeue act_que counter:%d\n",
						p_act_queue->counter);
#endif
				continue;
			}
			CONN_MD_DBG_FUNC(
			"p_cur_ilm:%p, local_para_ptr:%p, msg_len:%d\n",
					 p_cur_ilm, &p_cur_ilm->local_para_ptr,
					 p_cur_ilm->local_para_ptr->msg_len);
			CONN_MD_DBG_FUNC("send message to user id(0x%08x)\n",
					p_cur_ilm->dest_mod_id);
			conn_md_log_msg_time(p_cur_ilm->dest_mod_id);
			(*(p_user->ops.rx_cb)) (p_cur_ilm);
			CONN_MD_DBG_FUNC("message sent user id(0x%08x)done\n",
					p_cur_ilm->dest_mod_id);
			mutex_unlock(&p_user_list->lock);

#if (ACT_QUEUE_DBG == 1)
			/*free message structure */
			list_del(pos);
			kfree(p_msg);
			CONN_MD_DBG_FUNC("message structure freed\n");

			p_act_queue->counter++;
			CONN_MD_DBG_FUNC("dequeued in act queue counter:%d\n",
					p_act_queue->counter);
#endif
		}
		p_msg = NULL;

		while (!list_empty(&p_act_queue->list)) {
			list_for_each(pos, &p_act_queue->list) {
				p_msg = container_of(pos,
						struct conn_md_msg, entry);
				/*free message structure */
				list_del(pos);
				kfree(p_msg);
				p_msg = NULL;
				CONN_MD_DBG_FUNC("message structure freed\n");

				p_act_queue->counter++;
				CONN_MD_DBG_FUNC("dequeue act_que counter:%d\n",
						p_act_queue->counter);
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

static int __init conn_md_init(void)
{
	int i_ret = 0;
	struct conn_md_queue *p_queue = NULL;
	struct conn_md_user_list *p_user_list = NULL;

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
	if (g_conn_md.p_msg_dmp_sys == NULL)
		CONN_MD_WARN_FUNC("conn_md_dmp_init failed\n");
	else
		CONN_MD_INFO_FUNC("conn_md_dmp_init succeed\n");
	/*init proc interface */
	conn_md_dbg_init();

	/*create conn-md thread */

	init_completion(&g_conn_md.tx_comp);
	g_conn_md.p_task = kthread_create(conn_md_thread, &g_conn_md,
					 "conn-md-thread");
	if (g_conn_md.p_task == NULL) {
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

	struct conn_md_struct *p_conn_md = &g_conn_md;
	struct conn_md_queue *p_queue = NULL;
	struct conn_md_user_list *p_user_list = &p_conn_md->user_list;
	struct list_head *pos = NULL;
	struct conn_md_user *p_user = NULL;
	struct conn_md_msg *p_msg = NULL;

	CONN_MD_TRC_FUNC();

	/*terminate conn-md thread */
	if (p_conn_md->p_task != NULL) {
		CONN_MD_INFO_FUNC("signaling conn-md-thread to stop ...\n");
		kthread_stop(p_conn_md->p_task);
	}

	/*delete user_list structure if user list is not empty */
	mutex_lock(&p_user_list->lock);
	list_for_each(pos, &p_user_list->list) {
		p_user = container_of(pos, struct conn_md_user, entry);
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
		p_msg = container_of(pos, struct conn_md_msg, entry);
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
		p_msg = container_of(pos, struct conn_md_msg, entry);
		list_del(pos);
		kfree(p_msg);
	}
	p_queue->counter = 0;
	mutex_unlock(&p_queue->lock);
	mutex_destroy(&p_queue->lock);

	if (p_conn_md->p_msg_dmp_sys != NULL)
		conn_md_dmp_deinit(p_conn_md->p_msg_dmp_sys);

	CONN_MD_TRC_FUNC();
}

/*---------------------------------------------------------------------------*/

/*
 * module_init(conn_md_init);
 * module_exit(conn_md_exit);
 */
subsys_initcall(conn_md_init);
module_exit(conn_md_exit);
/*---------------------------------------------------------------------------*/

MODULE_AUTHOR("MBJ/WCN/SE/SS1/Chaozhong.Liang");
MODULE_DESCRIPTION("MTK CONN-MD Bridge Driver$1.0$");
MODULE_LICENSE("GPL");

/*---------------------------------------------------------------------------*/
