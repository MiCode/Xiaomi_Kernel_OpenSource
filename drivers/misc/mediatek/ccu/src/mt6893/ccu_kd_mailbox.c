/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/io.h>

#include "ccu_cmn.h"
#include "ccu_reg.h"
#include "ccu_kd_mailbox.h"
#include "ccu_mailbox_extif.h"

static struct ccu_mailbox_t *_ccu_mailbox;
static struct ccu_mailbox_t *_apmcu_mailbox;

static enum mb_result _mailbox_write_to_buffer(struct ccu_msg *task);

/*****************************************************************************
 * Public functions
 *****************************************************************************/
enum mb_result mailbox_init(struct ccu_mailbox_t *apmcu_mb_addr,
	struct ccu_mailbox_t *ccu_mb_addr)
{
	_ccu_mailbox = ccu_mb_addr;
	_apmcu_mailbox = apmcu_mb_addr;

	LOG_DBG("ccu_mailbox addr: %p\n", _ccu_mailbox);
	LOG_DBG("apmcu_mailbox addr: %p\n", _apmcu_mailbox);
	LOG_DBG("memclr _ccu_mailbox: %d\n",
		ccu_memclr(_ccu_mailbox, sizeof(struct ccu_mailbox_t)));
	LOG_DBG("memclr _apmcu_mailbox: %d\n",
		ccu_memclr(_apmcu_mailbox, sizeof(struct ccu_mailbox_t)));

	/* memory barrier to check mailbox value wrote into DRAM*/
	/* instead of keep in CPU write buffer*/
	mb();

	return MAILBOX_OK;
}


enum mb_result mailbox_send_cmd(struct ccu_msg *task)
{
	/*Fill slot*/
	enum mb_result result = _mailbox_write_to_buffer(task);

	if (result != MAILBOX_OK)
		return result;

	LOG_DBG("sending cmd: f(%d), r(%d), cmd(%d), in(%x), out(%x)\n",
			_ccu_mailbox->front,
			_ccu_mailbox->rear,
			_ccu_mailbox->queue[_ccu_mailbox->rear].msg_id,
			_ccu_mailbox->queue[_ccu_mailbox->rear].in_data_ptr,
			_ccu_mailbox->queue[_ccu_mailbox->rear].out_data_ptr);

	/*Send interrupt*/
	/* MCU write this field to trigger ccu interrupt pulse */
	ccu_write_reg_bit(ccu_base, EXT2CCU_INT_CCU,
		EXT2CCU_INT_CCU, 1);

	return MAILBOX_OK;
}


enum mb_result mailbox_receive_cmd(struct ccu_msg *task)
{
	enum mb_result result;
	 MUINT32 rear;
	 MUINT32 front;

	MUINT32 nextReadSlot;

	rear = _apmcu_mailbox->rear;
	front = _apmcu_mailbox->front;

	/*Check if queue is empty*/
	/*empty when front=rear*/
	if (rear != front) {
		/*modulus add: rear+1 = rear+1 % CCU_MAILBOX_QUEUE_SIZE*/
		nextReadSlot =
		(_apmcu_mailbox->front + 1) & (CCU_MAILBOX_QUEUE_SIZE - 1);
		ccu_memcpy(task, &(_apmcu_mailbox->queue[nextReadSlot]),
			sizeof(struct ccu_msg));
		_apmcu_mailbox->front = nextReadSlot;

		LOG_DBG(
		"received cmd: f(%d), r(%d), cmd(%d), in(%x), out(%x)\n",
		_apmcu_mailbox->front,
		_apmcu_mailbox->rear,
		_apmcu_mailbox->queue[nextReadSlot].msg_id,
		_apmcu_mailbox->queue[nextReadSlot].in_data_ptr,
		_apmcu_mailbox->queue[nextReadSlot].out_data_ptr);

		result = MAILBOX_OK;
	} else {
		LOG_DBG("apmcu mailbox is empty\n");
		result = MAILBOX_QUEUE_EMPTY;
	}

	return result;
}


/******************************************************************************
 * Private functions
 *****************************************************************************/
static int ccu_msg_copy(struct ccu_msg *dest, struct ccu_msg *src)
{
		/*LOG_DBG("src->msg_id: %d\n", src->msg_id);*/
		dest->msg_id = src->msg_id;
		/*LOG_DBG("dest->msg_id: %d\n", dest->msg_id);*/

		/*LOG_DBG("src->in_data_ptr: %d\n", src->in_data_ptr);*/
		dest->in_data_ptr = src->in_data_ptr;
		/*LOG_DBG("dest->in_data_ptr: %d\n", dest->in_data_ptr);*/

		/*LOG_DBG("src->out_data_ptr: %d\n", src->out_data_ptr);*/
		dest->out_data_ptr = src->out_data_ptr;
		/*LOG_DBG("dest->out_data_ptr: %d\n", dest->out_data_ptr);*/

		dest->tg_info = src->tg_info;
		/*LOG_DBG_MUST("dest->tg_info: %d\n", dest->tg_info);*/
		return 0;
}

static enum mb_result _mailbox_write_to_buffer(struct ccu_msg *task)
{
	enum mb_result result;
	MUINT32 nextWriteSlot =
	(_ccu_mailbox->rear + 1) & (CCU_MAILBOX_QUEUE_SIZE - 1);

	/*Check if queue is full*/
	/*full when front=rear+1 */
	/*(modulus add: rear+1 = rear+1 % CCU_MAILBOX_QUEUE_SIZE)*/
	if (nextWriteSlot == _ccu_mailbox->front) {
		LOG_DBG("ccu mailbox queue full !!\n");
		result = MAILBOX_QUEUE_FULL;
	} else {
		LOG_DBG("copy cmd to mailbox slot: %d\n", nextWriteSlot);
		LOG_DBG("target mailbox slot address: %p\n",

		&(_ccu_mailbox->queue[nextWriteSlot]));

		LOG_DBG("incming cmd: cmd(%d), in(%x), out(%x)\n",
				task->msg_id,
				task->in_data_ptr,
				task->out_data_ptr
				);

		LOG_DBG("writing target: slot(%d), cmd(%d), in(%x), out(%x)\n",
				nextWriteSlot,
				_ccu_mailbox->queue[nextWriteSlot].msg_id,
				_ccu_mailbox->queue[nextWriteSlot].in_data_ptr,
				_ccu_mailbox->queue[nextWriteSlot].out_data_ptr
				);

		ccu_msg_copy(&(_ccu_mailbox->queue[nextWriteSlot]), task);
		_ccu_mailbox->rear = nextWriteSlot;

		LOG_DBG("wrote target: slot(%d), cmd(%d), in(%x), out(%x)\n",
			_ccu_mailbox->rear,
			_ccu_mailbox->queue[_ccu_mailbox->rear].msg_id,
			_ccu_mailbox->queue[_ccu_mailbox->rear].in_data_ptr,
			_ccu_mailbox->queue[_ccu_mailbox->rear].out_data_ptr
			);

		/* memory barrier to check mailbox value wrote into DRAM*/
		/* instead of keep in CPU write buffer*/
		mb();

		result = MAILBOX_OK;
	}

	return result;
}
