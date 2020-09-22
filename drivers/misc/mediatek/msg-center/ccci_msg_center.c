// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "ccci_msg_center.h"
#include "ccci_cmd_center.h"

#define TAG "msg-cen"

#define CMD_THREAD_NAME_LEN 50

enum cci_msg_obj_type {
	MSG_OBJ_HAS_LOCK,
	MSG_OBJ_NOT_LOCK,
	MSG_OBJ_CMD_THREAD,

};

struct ccci_msg_obj {
	int id;
	enum cci_msg_obj_type type;

};

static struct ccci_msg_obj ccci_msg_objs[] = {
	{CCCI_USB_MSG_ID, MSG_OBJ_HAS_LOCK},
//	{CCCI_FSM_MSG_ID, MSG_OBJ_CMD_THREAD},
//	{CCCI_PORT_MSG_ID, MSG_OBJ_NOT_LOCK},

};

enum {
	SEND_TO_ALL,
	SEND_TO_ONE,
	SEND_TO_FIRST,
	SEND_TO_BIT,
};

struct command_data {
	int msg_id;
	unsigned int cmd_id;
	void *cmd_data;
	int send_flag;
	struct command_data *next;

};

struct command_head {
	spinlock_t cmd_lock;
	wait_queue_head_t cmd_wq;
	struct task_struct *pthread;

	int cmd_count;
	struct command_data *first;
	struct command_data *last;
	int if_exit;

};

struct register_head {
	int           msg_id;
	unsigned int  sub_id;
	void         *my_data;

	int (*callback)(
		int msg_id,
		unsigned int  sub_id,
		void *msg_data,
		void *my_data);

	struct list_head list;

};

struct message_head {
	int           msg_id;
	unsigned int  sub_id;
	void         *msg_data;
	int           free_msg_data;

};

struct array_head {
	struct list_head hlist;
	spinlock_t *ptr_lock;

	struct command_head *pcmd_head;

};

static struct array_head msg_reg_array[CCCI_MAX_MSG_ID];



static inline struct message_head *alloc_msg_head(
		int msg_id,
		unsigned int sub_id,
		void *msg_data,
		int free_msg_data)
{
	struct message_head *msg_head = NULL;

	msg_head = kzalloc(sizeof(struct message_head), GFP_KERNEL);
	if (msg_head == NULL)
		return msg_head;

	msg_head->msg_id   = msg_id;
	msg_head->sub_id   = sub_id;
	msg_head->msg_data = msg_data;
	msg_head->free_msg_data = free_msg_data;

	return msg_head;
}

static inline struct register_head *alloc_reg_head(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		int (*callback)(
			int msg_id,
			unsigned int  sub_id,
			void *msg_data,
			void *my_data))
{
	struct register_head *new_reg = NULL;

	new_reg = kzalloc(sizeof(struct register_head), GFP_KERNEL);
	if (new_reg) {
		new_reg->msg_id   = msg_id;
		new_reg->sub_id   = sub_id;
		new_reg->my_data  = my_data;
		new_reg->callback = callback;

		INIT_LIST_HEAD(&new_reg->list);

	}

	return new_reg;
}

static inline int msg_id_check(
		int msg_id)
{
	if (msg_id < 0 || msg_id >= CCCI_MAX_MSG_ID) {
		pr_notice("[%s][%s] error: invalid msg id: %d\n",
			TAG, __func__, msg_id);

		return -CCCI_ERR_INVALID_MSG_ID;
	}

	return 0;
}

static inline int handle_msg_data(
		int msg_id,
		unsigned int sub_id,
		void *msg_data,
		int send_flag)
{
	int ret = 0, match;
	int count = 0;
	struct register_head *reg;
	unsigned long flags = 0;

	ret = msg_id_check(msg_id);
	if (ret)
		return ret;

	if (msg_reg_array[msg_id].ptr_lock)
		spin_lock_irqsave(msg_reg_array[msg_id].ptr_lock, flags);

	if (!list_empty(&msg_reg_array[msg_id].hlist)) {
		list_for_each_entry(reg, &msg_reg_array[msg_id].hlist, list) {

			if (send_flag == SEND_TO_FIRST) {
				ret = reg->callback(msg_id, sub_id, msg_data,
								reg->my_data);
				count++;
				break;
			}

			match = 0;
			if (reg->sub_id == CCCI_NONE_SUB_ID)
				match = 1;

			else if (send_flag == SEND_TO_BIT) {
				if ((reg->sub_id & sub_id) == reg->sub_id)
					match = 1;

			} else if (reg->sub_id == sub_id)
				match = 1;

			if (match) {
				ret = reg->callback(msg_id, sub_id, msg_data,
								reg->my_data);
				count++;

				if (send_flag == SEND_TO_ONE)
					break;
			}
		}
	}

	if (msg_reg_array[msg_id].ptr_lock)
		spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

	if (count)
		return ret;

	else {
		pr_notice("[%s][%s] error: no user recv: (%d,0x%08X)\n",
			TAG, __func__, msg_id, sub_id);
		return -CCCI_ERR_NO_USER_RECV;

	}
}

static inline int del_reg_from_array(
		int msg_id,
		unsigned int sub_id,
		int (*callback)(
			int msg_id,
			unsigned int  sub_id,
			void *msg_data,
			void *my_data))
{
	struct register_head *reg;
	unsigned long flags = 0;
	int ret;

	ret = msg_id_check(msg_id);
	if (ret)
		return ret;

	if (msg_reg_array[msg_id].ptr_lock)
		spin_lock_irqsave(msg_reg_array[msg_id].ptr_lock, flags);

	list_for_each_entry(reg, &msg_reg_array[msg_id].hlist, list) {
		if (reg->sub_id == sub_id &&
			reg->callback == callback) {

			list_del(&reg->list);
			kfree(reg);

			if (msg_reg_array[msg_id].ptr_lock)
				spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

			return 0;
		}
	}

	if (msg_reg_array[msg_id].ptr_lock)
		spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

	pr_notice("[%s][%s] error: no call back func: (%d,0x%08X)\n",
			TAG, __func__, msg_id, sub_id);

	return -CCCI_ERR_NO_CALLBACK_FUN;
}

static inline int alloc_spin_lock(struct array_head *head)
{
	head->ptr_lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);

	if (!head->ptr_lock)
		goto err_nomem;

	spin_lock_init(head->ptr_lock);

	return 0;

err_nomem:
	pr_notice("[%s][%s] error: kmalloc failed.\n",
		TAG, __func__);

	return -CCCI_ERR_NO_MEMORY;
}

static inline struct command_data *pop_next_cmd(struct command_head *pcmd_head)
{
	struct command_data *pcmd = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&pcmd_head->cmd_lock, flags);

	if (pcmd_head->cmd_count > 0) {
		pcmd = pcmd_head->first;
		pcmd_head->first = pcmd->next;

		if (pcmd_head->cmd_count == 1)
			pcmd_head->last = NULL;

		pcmd_head->cmd_count--;
	}

	spin_unlock_irqrestore(&pcmd_head->cmd_lock, flags);

	return pcmd;
}

static inline int cmd_handle_thread_func(void *arg)
{
	struct array_head *head = (struct array_head *)arg;
	struct command_data *cmd = NULL;
	struct command_head *pcmd_head = head->pcmd_head;
	int ret;

	pr_notice("[%s][%s] thread start.\n",
		TAG, __func__);

	while (1) {
		ret = wait_event_interruptible(
				pcmd_head->cmd_wq,
				pcmd_head->cmd_count);

		if (ret == -ERESTARTSYS)
			continue;

		while ((cmd = pop_next_cmd(pcmd_head)) != NULL) {
			ret = handle_msg_data(cmd->msg_id, cmd->cmd_id,
					 cmd->cmd_data, cmd->send_flag);

			if (ret && (ret != -CCCI_ERR_NO_USER_RECV &&
				ret != -CCCI_ERR_INVALID_MSG_ID))
				pr_notice("[%s][%s] warning: return %d; (%d,0x%08X)\n",
					TAG, __func__, ret,
					cmd->msg_id, cmd->cmd_id);

			kfree(cmd);
		}

		if (kthread_should_stop())
			break;
	}

	pr_notice("[%s][%s] thread exit.\n",
		TAG, __func__);

	// free remain cmd obj
	while ((cmd = pop_next_cmd(pcmd_head)) != NULL) {
		pr_info("[%s][%s] free remain cmd(%d,0x%08X)\n",
			TAG, __func__, cmd->msg_id, cmd->cmd_id);
		kfree(cmd);
	}

	return 0;
}

static int create_cmd_head(int msg_id)
{
	int ret = 0;
	unsigned long flags = 0;
	struct array_head *head = &msg_reg_array[msg_id];
	struct command_head *pcmd_head = NULL;

	if (head->ptr_lock)
		spin_lock_irqsave(head->ptr_lock, flags);

	if (!head->pcmd_head) {
		pr_notice("[%s][%s] create pthread; msg_id: %d\n",
			TAG, __func__, msg_id);

		pcmd_head = kzalloc(sizeof(struct command_head), GFP_KERNEL);
		if (!pcmd_head) {
			ret = -CCCI_ERR_NO_MEMORY;
			goto create_fail;
		}

		pcmd_head->cmd_count = 0;
		pcmd_head->first = NULL;
		pcmd_head->last = NULL;
		pcmd_head->if_exit = 0;

		spin_lock_init(&pcmd_head->cmd_lock);
		init_waitqueue_head(&pcmd_head->cmd_wq);

		pcmd_head->pthread = kthread_run(
				cmd_handle_thread_func,
				head,
				"cmd_%d_handle", msg_id);

		head->pcmd_head = pcmd_head;
		if (IS_ERR(pcmd_head->pthread)) {
			ret = PTR_ERR_OR_ZERO(pcmd_head->pthread);
			goto create_fail;
		}
	}
	goto is_pass;

create_fail:
	pr_notice("[%s][%s] error: no memory; msg_id: %d; %d\n",
		TAG, __func__, msg_id, ret);

is_pass:
	if (head->ptr_lock)
		spin_unlock_irqrestore(head->ptr_lock, flags);

	return ret;
}

static int __init msg_center_init(void)
{
	int i = 0;
	int ret = 0;
	struct ccci_msg_obj *ptr_msg;
	struct array_head *ptr_head;

	pr_info("[%s][%s]\n", TAG, __func__);

	for (i = 0; i < ARRAY_SIZE(ccci_msg_objs); i++) {
		ptr_msg = &ccci_msg_objs[i];

		if (ptr_msg->id < 0 || ptr_msg->id >= CCCI_MAX_MSG_ID) {
			pr_info("[%s][%s] error: id is invalid: %d\n",
				TAG, __func__, ptr_msg->id);

			return -CCCI_ERR_INVALID_MSG_ID;
		}

		ptr_head = &msg_reg_array[ptr_msg->id];

		INIT_LIST_HEAD(&ptr_head->hlist);
		ptr_head->pcmd_head = NULL;

		if (ptr_msg->type == MSG_OBJ_HAS_LOCK ||
			ptr_msg->type == MSG_OBJ_CMD_THREAD) {
			ret = alloc_spin_lock(ptr_head);
			if (ret)
				return ret;

			if (ptr_msg->type == MSG_OBJ_CMD_THREAD) {
				ret = create_cmd_head(ptr_msg->id);
				if (ret)
					return ret;
			}

		} else if (ptr_msg->type == MSG_OBJ_NOT_LOCK)
			ptr_head->ptr_lock = NULL;

		else {
			pr_info("[%s][%s] error: msg type is invalid: %d,%d\n",
				TAG, __func__, ptr_msg->id, ptr_msg->type);

			return -CCCI_ERR_INVALID_MSG_TYPE;
		}
	}

	return 0;
}

int ccci_msg_register(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		int (*callback)(
			int msg_id,
			unsigned int sub_id,
			void *msg_data,
			void *my_data))
{
	int ret;
	struct register_head *new_reg = NULL;
	unsigned long flags = 0;

	ret = msg_id_check(msg_id);
	if (ret)
		return ret;

	new_reg = alloc_reg_head(msg_id, sub_id, my_data, callback);
	if (!new_reg) {
		pr_notice("[%s][%s] error: no memory; id:(%d,0x%08X)\n",
			TAG, __func__, msg_id, sub_id);

		return -CCCI_ERR_NO_MEMORY;
	}

	pr_info("[%s][%s] id: (%d,0x%08X); cb: %p\n",
			TAG, __func__, msg_id, sub_id, callback);

	if (msg_reg_array[msg_id].ptr_lock)
		spin_lock_irqsave(msg_reg_array[msg_id].ptr_lock, flags);

	list_add_tail(&new_reg->list, &msg_reg_array[msg_id].hlist);

	if (msg_reg_array[msg_id].ptr_lock)
		spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

	return 0;
}
EXPORT_SYMBOL(ccci_msg_register);

int	ccci_msg_unregister(
		int msg_id,
		unsigned int sub_id,
		void *callback)
{
	pr_info("[%s][%s] id: (%d,0x%08X); cb: %p\n",
			TAG, __func__, msg_id, sub_id, callback);

	return del_reg_from_array(msg_id, sub_id, callback);
}
EXPORT_SYMBOL(ccci_msg_unregister);

/* think the msg_id is one-to-all */
int ccci_msg_send(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	pr_debug("[%s][%s] id: (%d,0x%08X)\n",
		TAG, __func__, msg_id, sub_id);

	return handle_msg_data(msg_id, sub_id, msg_data, SEND_TO_ALL);
}
EXPORT_SYMBOL(ccci_msg_send);

/* think the msg_id is one-to-bit */
int ccci_msg_send_to_bit(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	pr_debug("[%s][%s] id: (%d,0x%08X)\n",
		TAG, __func__, msg_id, sub_id);

	return handle_msg_data(msg_id, sub_id, msg_data, SEND_TO_BIT);
}
EXPORT_SYMBOL(ccci_msg_send_to_bit);

/* think the msg_id is one-to-first */
int ccci_msg_send_to_first(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	pr_debug("[%s][%s] id: (%d,0x%08X)\n",
		TAG, __func__, msg_id, sub_id);

	return handle_msg_data(msg_id, sub_id, msg_data, SEND_TO_FIRST);
}
EXPORT_SYMBOL(ccci_msg_send_to_first);

/* think the msg_id is one-to-one */
int ccci_msg_send_to_one(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	pr_debug("[%s][%s] id: (%d,0x%08X)\n",
		TAG, __func__, msg_id, sub_id);

	return handle_msg_data(msg_id, sub_id, msg_data, SEND_TO_ONE);
}
EXPORT_SYMBOL(ccci_msg_send_to_one);

static void add_cmd_to_list(
		struct command_head *pcmd_head,
		struct command_data *pcmd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&pcmd_head->cmd_lock, flags);

	if (pcmd_head->cmd_count > 0)
		pcmd_head->last->next = pcmd;
	else
		pcmd_head->first = pcmd;

	pcmd_head->last = pcmd;

	pcmd_head->cmd_count++;

	spin_unlock_irqrestore(&pcmd_head->cmd_lock, flags);
}

static struct command_data *create_cmd_data(int msg_id,
		unsigned int cmd_id, void *cmd_data, int send_flag)
{
	struct command_data *pcmd = NULL;

	pcmd = kzalloc(sizeof(struct command_data), GFP_KERNEL);
	if (!pcmd)
		goto err_nomem;

	pcmd->msg_id    = msg_id;
	pcmd->cmd_id    = cmd_id;
	pcmd->cmd_data  = cmd_data;
	pcmd->send_flag = send_flag;
	pcmd->next      = NULL;

	return pcmd;

err_nomem:
	pr_notice("[%s][%s] error: no memory; (%d,0x%08X)\n",
		TAG, __func__, msg_id, cmd_id);

	return NULL;
}

static int push_new_cmd(int msg_id,
		unsigned int cmd_id, void *cmd_data, int send_flag)
{
	struct command_data *pcmd = NULL;

	if (!msg_reg_array[msg_id].pcmd_head) {
		pr_notice("[%s][%s] error: no cmd thread running; (%d,0x%08X)\n",
			TAG, __func__, msg_id, cmd_id);

		return -CCCI_ERR_CMD_HEAD_NULL;
	}

	pcmd = create_cmd_data(msg_id, cmd_id, cmd_data, send_flag);
	if (pcmd == NULL)
		return -CCCI_ERR_NO_MEMORY;

	add_cmd_to_list(msg_reg_array[msg_id].pcmd_head, pcmd);

	return 0;
}

int	ccci_cmd_register(
		int msg_id,
		unsigned int cmd_id,
		void *my_data,
		int (*callback)(
			int msg_id,
			unsigned int cmd_id,
			void *cmd_data,
			void *my_data))
{
	int ret = 0;

	ret = ccci_msg_register(msg_id, cmd_id, my_data, callback);

	return ret;
}
EXPORT_SYMBOL(ccci_cmd_register);

/* think the msg_id is one-to-one */
int ccci_cmd_send_to_one(
		int msg_id,
		unsigned int cmd_id,
		void *cmd_data)
{
	int ret;
	unsigned long flags = 0;

	ret = msg_id_check(msg_id);
	if (ret)
		return ret;

	if (msg_reg_array[msg_id].ptr_lock)
		spin_lock_irqsave(msg_reg_array[msg_id].ptr_lock, flags);

	ret = push_new_cmd(msg_id, cmd_id, cmd_data, SEND_TO_ONE);
	if (!ret)
		wake_up_all(&msg_reg_array[msg_id].pcmd_head->cmd_wq);

	if (msg_reg_array[msg_id].ptr_lock)
		spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

	pr_notice("[%s][%s] ret: %d; (%d,0x%08X)\n",
		TAG, __func__, ret, msg_id, cmd_id);

	return ret;
}
EXPORT_SYMBOL(ccci_cmd_send_to_one);

/* think the msg_id is one-to-all */
int ccci_cmd_send(
		int msg_id,
		unsigned int cmd_id,
		void *cmd_data)
{
	int ret;
	unsigned long flags = 0;

	ret = msg_id_check(msg_id);
	if (ret)
		return ret;

	if (msg_reg_array[msg_id].ptr_lock)
		spin_lock_irqsave(msg_reg_array[msg_id].ptr_lock, flags);

	ret = push_new_cmd(msg_id, cmd_id, cmd_data, SEND_TO_ALL);
	if (!ret)
		wake_up_all(&msg_reg_array[msg_id].pcmd_head->cmd_wq);

	if (msg_reg_array[msg_id].ptr_lock)
		spin_unlock_irqrestore(msg_reg_array[msg_id].ptr_lock, flags);

	pr_notice("[%s][%s] ret: %d; (%d,0x%08X)\n",
				TAG, __func__, ret, msg_id, cmd_id);

	return ret;
}
EXPORT_SYMBOL(ccci_cmd_send);


int	ccci_cmd_unregister(
		int msg_id,
		unsigned int cmd_id,
		void *callback)
{
	int ret;

	pr_info("[%s][%s] id: (%d,0x%08X); cb: %p\n",
			TAG, __func__, msg_id, cmd_id, callback);

	ret = del_reg_from_array(msg_id, cmd_id, callback);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(ccci_cmd_unregister);

subsys_initcall(msg_center_init);

MODULE_AUTHOR("Xin Xu <xin.xu@mediatek.com>");
MODULE_DESCRIPTION("message center driver v1.0");
MODULE_LICENSE("GPL");
