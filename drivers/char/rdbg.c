/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <soc/qcom/smsm.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#define SMP2P_NUM_PROCS				8
#define MAX_RETRIES				20

#define SM_VERSION				1
#define SM_BLOCKSIZE				128

#define SMQ_MAGIC_INIT				0xFF00FF00
#define SMQ_MAGIC_PRODUCER			(SMQ_MAGIC_INIT | 0x1)
#define SMQ_MAGIC_CONSUMER			(SMQ_MAGIC_INIT | 0x2)

enum SMQ_STATUS {
	SMQ_SUCCESS    =  0,
	SMQ_ENOMEMORY  = -1,
	SMQ_EBADPARM   = -2,
	SMQ_UNDERFLOW  = -3,
	SMQ_OVERFLOW   = -4
};

enum smq_type {
	PRODUCER = 1,
	CONSUMER = 2,
	INVALID  = 3
};

struct smq_block_map {
	uint32_t index_read;
	uint32_t num_blocks;
	uint8_t *map;
};

struct smq_node {
	uint16_t index_block;
	uint16_t num_blocks;
} __attribute__ ((__packed__));

struct smq_hdr {
	uint8_t producer_version;
	uint8_t consumer_version;
} __attribute__ ((__packed__));

struct smq_out_state {
	uint32_t init;
	uint32_t index_check_queue_for_reset;
	uint32_t index_sent_write;
	uint32_t index_free_read;
} __attribute__ ((__packed__));

struct smq_out {
	struct smq_out_state s;
	struct smq_node sent[1];
};

struct smq_in_state {
	uint32_t init;
	uint32_t index_check_queue_for_reset_ack;
	uint32_t index_sent_read;
	uint32_t index_free_write;
} __attribute__ ((__packed__));

struct smq_in {
	struct smq_in_state s;
	struct smq_node free[1];
};

struct smq {
	struct smq_hdr *hdr;
	struct smq_out *out;
	struct smq_in *in;
	uint8_t *blocks;
	uint32_t num_blocks;
	struct mutex *lock;
	uint32_t initialized;
	struct smq_block_map block_map;
	enum smq_type type;
};

struct gpio_info {
	int gpio_base_id;
	int irq_base_id;
};

struct rdbg_data {
	struct device *device;
	struct completion work;
	struct gpio_info in;
	struct gpio_info out;
	bool   device_initialized;
	int    gpio_out_offset;
	bool   device_opened;
	void   *smem_addr;
	size_t smem_size;
	struct smq    producer_smrb;
	struct smq    consumer_smrb;
	struct mutex  write_mutex;
};

struct rdbg_device {
	struct cdev cdev;
	struct class *class;
	dev_t dev_no;
	int num_devices;
	struct rdbg_data *rdbg_data;
};

static struct rdbg_device g_rdbg_instance = {
	{ {0} },
	NULL,
	0,
	SMP2P_NUM_PROCS,
	NULL
};

struct processor_specific_info {
	char *name;
	unsigned int smem_buffer_addr;
	size_t smem_buffer_size;
};

static struct processor_specific_info proc_info[SMP2P_NUM_PROCS] = {
		{0},	/*APPS*/
		{"rdbg_modem", 0, 0},	/*MODEM*/
		{"rdbg_adsp", SMEM_LC_DEBUGGER, 16*1024},	/*ADSP*/
		{0},	/*SMP2P_RESERVED_PROC_1*/
		{"rdbg_wcnss", 0, 0},		/*WCNSS*/
		{0},	/*SMP2P_RESERVED_PROC_2*/
		{0},	/*SMP2P_POWER_PROC*/
		{0}		/*SMP2P_REMOTE_MOCK_PROC*/
};

static int smq_blockmap_get(struct smq_block_map *block_map,
	uint32_t *block_index, uint32_t n)
{
	uint32_t start;
	uint32_t mark = 0;
	uint32_t found = 0;
	uint32_t i = 0;

	start = block_map->index_read;

	if (n == 1) {
		do {
			if (!block_map->map[block_map->index_read]) {
				*block_index = block_map->index_read;
				block_map->map[block_map->index_read] = 1;
				block_map->index_read++;
				block_map->index_read %= block_map->num_blocks;
				return SMQ_SUCCESS;
			}
			block_map->index_read++;
		} while (start != (block_map->index_read %=
			block_map->num_blocks));
	} else {
		mark = block_map->num_blocks;

		do {
			if (!block_map->map[block_map->index_read]) {
				if (mark > block_map->index_read) {
					mark = block_map->index_read;
					start = block_map->index_read;
					found = 0;
				}

				found++;
				if (found == n) {
					*block_index = mark;
					for (i = 0; i < n; i++)
						block_map->map[mark + i] =
							(uint8_t)(n - i);
					block_map->index_read += block_map->map
						[block_map->index_read] - 1;
					return SMQ_SUCCESS;
				}
			} else {
				found = 0;
				block_map->index_read += block_map->map
					[block_map->index_read] - 1;
				mark = block_map->num_blocks;
			}
			block_map->index_read++;
		} while (start != (block_map->index_read %=
			block_map->num_blocks));
	}

	return SMQ_ENOMEMORY;
}

static void smq_blockmap_put(struct smq_block_map *block_map, uint32_t i)
{
	uint32_t num_blocks = block_map->map[i];

	while (num_blocks--) {
		block_map->map[i] = 0;
		i++;
	}
}

static int smq_blockmap_reset(struct smq_block_map *block_map)
{
	if (!block_map->map)
		return SMQ_ENOMEMORY;
	memset(block_map->map, 0 , block_map->num_blocks + 1);
	block_map->index_read = 0;

	return SMQ_SUCCESS;
}

static int smq_blockmap_ctor(struct smq_block_map *block_map,
	uint32_t num_blocks)
{
	if (num_blocks <= 1)
		return SMQ_ENOMEMORY;

	block_map->map = kcalloc(num_blocks, sizeof(uint8_t), GFP_KERNEL);
	if (!block_map->map)
		return SMQ_ENOMEMORY;

	block_map->num_blocks = num_blocks - 1;
	smq_blockmap_reset(block_map);

	return SMQ_SUCCESS;
}

static void smq_blockmap_dtor(struct smq_block_map *block_map)
{
	kfree(block_map->map);
	block_map->map = NULL;
}

static int smq_free(struct smq *smq, void *data)
{
	struct smq_node node;
	uint32_t index_block;
	int err = SMQ_SUCCESS;

	if (smq->lock)
		mutex_lock(smq->lock);

	if ((SM_VERSION != smq->hdr->producer_version) &&
		(SMQ_MAGIC_PRODUCER != smq->out->s.init)) {
		err = SMQ_UNDERFLOW;
		goto bail;
	}

	index_block = ((uint8_t *)data - smq->blocks) / SM_BLOCKSIZE;
	if (index_block >= smq->num_blocks) {
		err = SMQ_EBADPARM;
		goto bail;
	}

	node.index_block = (uint16_t)index_block;
	node.num_blocks = 0;
	*((struct smq_node *)(smq->in->free + smq->in->
		s.index_free_write)) = node;

	smq->in->s.index_free_write = (smq->in->s.index_free_write + 1)
		% smq->num_blocks;

bail:
	if (smq->lock)
		mutex_unlock(smq->lock);
	return err;
}

static int smq_receive(struct smq *smq, void **pp, int *pnsize, int *pbmore)
{
	struct smq_node *node;
	int err = SMQ_SUCCESS;
	int more = 0;

	if ((SM_VERSION != smq->hdr->producer_version) &&
		(SMQ_MAGIC_PRODUCER != smq->out->s.init))
		return SMQ_UNDERFLOW;

	if (smq->in->s.index_sent_read == smq->out->s.index_sent_write) {
		err = SMQ_UNDERFLOW;
		goto bail;
	}

	node = (struct smq_node *)(smq->out->sent + smq->in->s.index_sent_read);
	if (node->index_block >= smq->num_blocks) {
		err = SMQ_EBADPARM;
		goto bail;
	}

	smq->in->s.index_sent_read = (smq->in->s.index_sent_read + 1)
		% smq->num_blocks;

	*pp = smq->blocks + (node->index_block * SM_BLOCKSIZE);
	*pnsize = SM_BLOCKSIZE * node->num_blocks;
	rmb();
	if (smq->in->s.index_sent_read != smq->out->s.index_sent_write)
		more = 1;

bail:
	*pbmore = more;
	return err;
}

static int smq_alloc_send(struct smq *smq, const uint8_t *pcb, int nsize)
{
	void *pv = 0;
	int num_blocks;
	uint32_t index_block = 0;
	int err = SMQ_SUCCESS;
	struct smq_node *node = NULL;

	mutex_lock(smq->lock);

	if ((SMQ_MAGIC_CONSUMER == smq->in->s.init) &&
	 (SM_VERSION == smq->hdr->consumer_version)) {
		if (smq->out->s.index_check_queue_for_reset ==
			smq->in->s.index_check_queue_for_reset_ack) {
			while (smq->out->s.index_free_read !=
				smq->in->s.index_free_write) {
				node = (struct smq_node *)(
					smq->in->free +
					smq->out->s.index_free_read);
				if (node->index_block >= smq->num_blocks) {
					err = SMQ_EBADPARM;
					goto bail;
				}

				smq->out->s.index_free_read =
					(smq->out->s.index_free_read + 1)
						% smq->num_blocks;

				smq_blockmap_put(&smq->block_map,
					node->index_block);
				rmb();
			}
		}
	}

	num_blocks = ALIGN(nsize, SM_BLOCKSIZE)/SM_BLOCKSIZE;
	err = smq_blockmap_get(&smq->block_map, &index_block, num_blocks);
	if (SMQ_SUCCESS != err)
		goto bail;

	pv = smq->blocks + (SM_BLOCKSIZE * index_block);

	err = copy_from_user((void *)pv, (void *)pcb, nsize);
	if (0 != err)
		goto bail;

	((struct smq_node *)(smq->out->sent +
		smq->out->s.index_sent_write))->index_block
			= (uint16_t)index_block;
	((struct smq_node *)(smq->out->sent +
		smq->out->s.index_sent_write))->num_blocks
			= (uint16_t)num_blocks;

	smq->out->s.index_sent_write = (smq->out->s.index_sent_write + 1)
		% smq->num_blocks;

bail:
	if (SMQ_SUCCESS != err) {
		if (pv)
			smq_blockmap_put(&smq->block_map, index_block);
	}
	mutex_unlock(smq->lock);
	return err;
}

static int smq_reset_producer_queue_internal(struct smq *smq,
	uint32_t reset_num)
{
	int retval = 0;
	uint32_t i;

	if (PRODUCER != smq->type)
		goto bail;

	mutex_lock(smq->lock);
	if (smq->out->s.index_check_queue_for_reset != reset_num) {
		smq->out->s.index_check_queue_for_reset = reset_num;
		for (i = 0; i < smq->num_blocks; i++)
			(smq->out->sent + i)->index_block = 0xFFFF;

		smq_blockmap_reset(&smq->block_map);
		smq->out->s.index_sent_write = 0;
		smq->out->s.index_free_read = 0;
		retval = 1;
	}
	mutex_unlock(smq->lock);

bail:
	return retval;
}

static int smq_check_queue_reset(struct smq *p_cons, struct smq *p_prod)
{
	int retval = 0;
	uint32_t reset_num, i;

	if ((CONSUMER != p_cons->type) ||
		(SMQ_MAGIC_PRODUCER != p_cons->out->s.init) ||
		(SM_VERSION != p_cons->hdr->producer_version))
		goto bail;

	reset_num = p_cons->out->s.index_check_queue_for_reset;
	if (p_cons->in->s.index_check_queue_for_reset_ack != reset_num) {
		p_cons->in->s.index_check_queue_for_reset_ack = reset_num;
		for (i = 0; i < p_cons->num_blocks; i++)
			(p_cons->in->free + i)->index_block = 0xFFFF;

		p_cons->in->s.index_sent_read = 0;
		p_cons->in->s.index_free_write = 0;

		retval = smq_reset_producer_queue_internal(p_prod, reset_num);
	}

bail:
	return retval;
}

static int check_subsystem_debug_enabled(void *base_addr, int size)
{
	int num_blocks;
	uint8_t *pb_orig;
	uint8_t *pb;
	struct smq smq;
	int err = 0;

	pb = pb_orig = (uint8_t *)base_addr;
	pb += sizeof(struct smq_hdr);
	pb = PTR_ALIGN(pb, 8);
	size -= pb - (uint8_t *)pb_orig;
	num_blocks = (int)((size - sizeof(struct smq_out_state) -
		sizeof(struct smq_in_state))/(SM_BLOCKSIZE +
		sizeof(struct smq_node) * 2));
	if (0 >= num_blocks) {
		err = SMQ_EBADPARM;
		goto bail;
	}

	pb += num_blocks * SM_BLOCKSIZE;
	smq.out = (struct smq_out *)pb;
	pb += sizeof(struct smq_out_state) + (num_blocks *
		sizeof(struct smq_node));
	smq.in = (struct smq_in *)pb;

	if (SMQ_MAGIC_CONSUMER != smq.in->s.init) {
		pr_err("%s, smq in consumer not initialized", __func__);
		err = -ECOMM;
	}

bail:
	return err;
}

static void smq_dtor(struct smq *smq)
{
	if (SMQ_MAGIC_INIT == smq->initialized) {
		switch (smq->type) {
		case PRODUCER:
			smq->out->s.init = 0;
			smq_blockmap_dtor(&smq->block_map);
			break;
		case CONSUMER:
			smq->in->s.init = 0;
			break;
		default:
		case INVALID:
			break;
		}

		smq->initialized = 0;
	}
}

/*
 * The shared memory is used as a circular ring buffer in each direction.
 * Thus we have a bi-directional shared memory channel between the AP
 * and a subsystem. We call this SMQ. Each memory channel contains a header,
 * data and a control mechanism that is used to synchronize read and write
 * of data between the AP and the remote subsystem.
 *
 * Overall SMQ memory view:
 *
 *    +------------------------------------------------+
 *    | SMEM buffer                                    |
 *    |-----------------------+------------------------|
 *    |Producer: LA           | Producer: Remote       |
 *    |Consumer: Remote       |           subsystem    |
 *    |          subsystem    | Consumer: LA           |
 *    |                       |                        |
 *    |               Producer|                Consumer|
 *    +-----------------------+------------------------+
 *    |                       |
 *    |                       |
 *    |                       +--------------------------------------+
 *    |                                                              |
 *    |                                                              |
 *    v                                                              v
 *    +--------------------------------------------------------------+
 *    |   Header  |       Data      |            Control             |
 *    +-----------+---+---+---+-----+----+--+--+-----+---+--+--+-----+
 *    |           | b | b | b |     | S  |n |n |     | S |n |n |     |
 *    |  Producer | l | l | l |     | M  |o |o |     | M |o |o |     |
 *    |    Ver    | o | o | o |     | Q  |d |d |     | Q |d |d |     |
 *    |-----------| c | c | c | ... |    |e |e | ... |   |e |e | ... |
 *    |           | k | k | k |     | O  |  |  |     | I |  |  |     |
 *    |  Consumer |   |   |   |     | u  |0 |1 |     | n |0 |1 |     |
 *    |    Ver    | 0 | 1 | 2 |     | t  |  |  |     |   |  |  |     |
 *    +-----------+---+---+---+-----+----+--+--+-----+---+--+--+-----+
 *                                       |           |
 *                                       +           |
 *                                                   |
 *                          +------------------------+
 *                          |
 *                          v
 *                        +----+----+----+----+
 *                        | SMQ Nodes         |
 *                        |----|----|----|----|
 *                 Node # |  0 |  1 |  2 | ...|
 *                        |----|----|----|----|
 * Starting Block Index # |  0 |  3 |  8 | ...|
 *                        |----|----|----|----|
 *            # of blocks |  3 |  5 |  1 | ...|
 *                        +----+----+----+----+
 *
 * Header: Contains version numbers for software compatibility to ensure
 * that both producers and consumers on the AP and subsystems know how to
 * read from and write to the queue.
 * Both the producer and consumer versions are 1.
 *     +---------+-------------------+
 *     | Size    | Field             |
 *     +---------+-------------------+
 *     | 1 byte  | Producer Version  |
 *     +---------+-------------------+
 *     | 1 byte  | Consumer Version  |
 *     +---------+-------------------+
 *
 * Data: The data portion contains multiple blocks [0..N] of a fixed size.
 * The block size SM_BLOCKSIZE is fixed to 128 bytes for header version #1.
 * Payload sent from the debug agent app is split (if necessary) and placed
 * in these blocks. The first data block is placed at the next 8 byte aligned
 * address after the header.
 *
 * The number of blocks for a given SMEM allocation is derived as follows:
 *   Number of Blocks = ((Total Size - Alignment - Size of Header
 *		- Size of SMQIn - Size of SMQOut)/(SM_BLOCKSIZE))
 *
 * The producer maintains a private block map of each of these blocks to
 * determine which of these blocks in the queue is available and which are free.
 *
 * Control:
 * The control portion contains a list of nodes [0..N] where N is number
 * of available data blocks. Each node identifies the data
 * block indexes that contain a particular debug message to be transfered,
 * and the number of blocks it took to hold the contents of the message.
 *
 * Each node has the following structure:
 *     +---------+-------------------+
 *     | Size    | Field             |
 *     +---------+-------------------+
 *     | 2 bytes |Staring Block Index|
 *     +---------+-------------------+
 *     | 2 bytes |Number of Blocks   |
 *     +---------+-------------------+
 *
 * The producer and the consumer update different parts of the control channel
 * (SMQOut / SMQIn) respectively. Each of these control data structures contains
 * information about the last node that was written / read, and the actual nodes
 * that were written/read.
 *
 * SMQOut Structure (R/W by producer, R by consumer):
 *     +---------+-------------------+
 *     | Size    | Field             |
 *     +---------+-------------------+
 *     | 4 bytes | Magic Init Number |
 *     +---------+-------------------+
 *     | 4 bytes | Reset             |
 *     +---------+-------------------+
 *     | 4 bytes | Last Sent Index   |
 *     +---------+-------------------+
 *     | 4 bytes | Index Free Read   |
 *     +---------+-------------------+
 *
 * SMQIn Structure (R/W by consumer, R by producer):
 *     +---------+-------------------+
 *     | Size    | Field             |
 *     +---------+-------------------+
 *     | 4 bytes | Magic Init Number |
 *     +---------+-------------------+
 *     | 4 bytes | Reset ACK         |
 *     +---------+-------------------+
 *     | 4 bytes | Last Read Index   |
 *     +---------+-------------------+
 *     | 4 bytes | Index Free Write  |
 *     +---------+-------------------+
 *
 * Magic Init Number:
 * Both SMQ Out and SMQ In initialize this field with a predefined magic
 * number so as to make sure that both the consumer and producer blocks
 * have fully initialized and have valid data in the shared memory control area.
 *	Producer Magic #: 0xFF00FF01
 *	Consumer Magic #: 0xFF00FF02
 */
static int smq_ctor(struct smq *smq, void *base_addr, int size,
	enum smq_type type, struct mutex *lock_ptr)
{
	int num_blocks;
	uint8_t *pb_orig;
	uint8_t *pb;
	uint32_t i;
	int err;

	if (SMQ_MAGIC_INIT == smq->initialized) {
		err = SMQ_EBADPARM;
		goto bail;
	}

	if (!base_addr || !size) {
		err = SMQ_EBADPARM;
		goto bail;
	}

	if (type == PRODUCER)
		smq->lock = lock_ptr;

	pb_orig = (uint8_t *)base_addr;
	smq->hdr = (struct smq_hdr *)pb_orig;
	pb = pb_orig;
	pb += sizeof(struct smq_hdr);
	pb = PTR_ALIGN(pb, 8);
	size -= pb - (uint8_t *)pb_orig;
	num_blocks = (int)((size - sizeof(struct smq_out_state) -
		sizeof(struct smq_in_state))/(SM_BLOCKSIZE +
		sizeof(struct smq_node) * 2));
	if (0 >= num_blocks) {
		err = SMQ_ENOMEMORY;
		goto bail;
	}

	smq->blocks = pb;
	smq->num_blocks = num_blocks;
	pb += num_blocks * SM_BLOCKSIZE;
	smq->out = (struct smq_out *)pb;
	pb += sizeof(struct smq_out_state) + (num_blocks *
		sizeof(struct smq_node));
	smq->in = (struct smq_in *)pb;
	smq->type = type;
	if (PRODUCER == type) {
		smq->hdr->producer_version = SM_VERSION;
		for (i = 0; i < smq->num_blocks; i++)
			(smq->out->sent + i)->index_block = 0xFFFF;

		err = smq_blockmap_ctor(&smq->block_map, smq->num_blocks);
		if (SMQ_SUCCESS != err)
			goto bail;

		smq->out->s.index_sent_write = 0;
		smq->out->s.index_free_read = 0;
		if (smq->out->s.init == SMQ_MAGIC_PRODUCER) {
			smq->out->s.index_check_queue_for_reset += 1;
		} else {
			smq->out->s.index_check_queue_for_reset = 1;
			smq->out->s.init = SMQ_MAGIC_PRODUCER;
		}
	} else {
		smq->hdr->consumer_version = SM_VERSION;
		for (i = 0; i < smq->num_blocks; i++)
			(smq->in->free + i)->index_block = 0xFFFF;

		smq->in->s.index_sent_read = 0;
		smq->in->s.index_free_write = 0;
		if (smq->out->s.init == SMQ_MAGIC_PRODUCER) {
			smq->in->s.index_check_queue_for_reset_ack =
				smq->out->s.index_check_queue_for_reset;
		} else {
			smq->in->s.index_check_queue_for_reset_ack = 0;
		}

		smq->in->s.init = SMQ_MAGIC_CONSUMER;
	}
	smq->initialized = SMQ_MAGIC_INIT;
	err = SMQ_SUCCESS;

bail:
	return err;
}

static void send_interrupt_to_subsystem(struct rdbg_data *rdbgdata)
{
	int offset = rdbgdata->gpio_out_offset;
	int val = 1 ^ gpio_get_value(rdbgdata->out.gpio_base_id + offset);
	gpio_set_value(rdbgdata->out.gpio_base_id + offset, val);
	rdbgdata->gpio_out_offset = (offset + 1) % 32;

	dev_dbg(rdbgdata->device, "%s: sent interrupt %d to subsystem",
		__func__, val);
}

static irqreturn_t on_interrupt_from(int irq, void *ptr)
{
	struct rdbg_data *rdbgdata = (struct rdbg_data *) ptr;

	dev_dbg(rdbgdata->device, "%s: Received interrupt %d from subsystem",
		__func__, irq);

	complete(&(rdbgdata->work));
	return IRQ_HANDLED;
}

static int initialize_smq(struct rdbg_data *rdbgdata)
{
	int err = 0;

	unsigned char *smem_consumer_buffer = rdbgdata->smem_addr;
	smem_consumer_buffer += (rdbgdata->smem_size/2);

	if (smq_ctor(&(rdbgdata->producer_smrb), (void *)(rdbgdata->smem_addr),
		((rdbgdata->smem_size)/2), PRODUCER, &rdbgdata->write_mutex)) {
		dev_err(rdbgdata->device, "%s: smq producer allocation failed",
			__func__);
		err = -ENOMEM;
		goto bail;
	}

	if (smq_ctor(&(rdbgdata->consumer_smrb), (void *)smem_consumer_buffer,
		((rdbgdata->smem_size)/2), CONSUMER, NULL)) {
		dev_err(rdbgdata->device, "%s: smq conmsumer allocation failed",
			__func__);
		err = -ENOMEM;
	}

bail:
	return err;

}

static int rdbg_open(struct inode *inode, struct file *filp)
{
	int device_id = -1;
	struct rdbg_device *device = &g_rdbg_instance;
	struct rdbg_data *rdbgdata = NULL;
	int err = 0;

	if (!inode || !device->rdbg_data) {
		pr_err("Memory not allocated yet");
		err = -ENODEV;
		goto bail;
	}

	device_id = MINOR(inode->i_rdev);
	rdbgdata = &device->rdbg_data[device_id];

	if (rdbgdata->device_opened) {
		dev_err(rdbgdata->device, "%s: Device already opened",
			__func__);
		err = -EEXIST;
		goto bail;
	}

	rdbgdata->smem_size = proc_info[device_id].smem_buffer_size;
	if (!rdbgdata->smem_size) {
		dev_err(rdbgdata->device, "%s: smem not initialized", __func__);
		err = -ENOMEM;
		goto bail;
	}

	rdbgdata->smem_addr = smem_find(proc_info[device_id].smem_buffer_addr,
		rdbgdata->smem_size, 0, SMEM_ANY_HOST_FLAG);
	if (!rdbgdata->smem_addr) {
		dev_err(rdbgdata->device, "%s: Could not allocate smem memory",
			__func__);
		err = -ENOMEM;
		goto bail;
	}
	dev_dbg(rdbgdata->device, "%s: SMEM address=0x%lx smem_size=%d",
		__func__, (unsigned long)rdbgdata->smem_addr,
		(unsigned int)rdbgdata->smem_size);

	if (check_subsystem_debug_enabled(rdbgdata->smem_addr,
		rdbgdata->smem_size/2)) {
		dev_err(rdbgdata->device, "%s: Subsystem %s is not debug enabled",
			__func__, proc_info[device_id].name);
		err = -ECOMM;
		goto bail;
	}

	init_completion(&rdbgdata->work);

	err = request_irq(rdbgdata->in.irq_base_id, on_interrupt_from,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			proc_info[device_id].name,
			(void *)&device->rdbg_data[device_id]);
	if (err) {
		dev_err(rdbgdata->device,
			"%s: Failed to register interrupt.Err=%d,irqid=%d.",
			__func__, err, rdbgdata->in.irq_base_id);
		goto irq_bail;
	}

	err = enable_irq_wake(rdbgdata->in.irq_base_id);
	if (err < 0) {
		dev_dbg(rdbgdata->device, "enable_irq_wake() failed with err=%d",
			err);
		err = 0;
	}

	mutex_init(&rdbgdata->write_mutex);

	err = initialize_smq(rdbgdata);
	if (err) {
		dev_err(rdbgdata->device, "Error initializing smq. Err=%d",
			err);
		goto smq_bail;
	}

	rdbgdata->device_opened = 1;

	filp->private_data = (void *)rdbgdata;

	return 0;

smq_bail:
	smq_dtor(&(rdbgdata->producer_smrb));
	smq_dtor(&(rdbgdata->consumer_smrb));
	mutex_destroy(&rdbgdata->write_mutex);
irq_bail:
	free_irq(rdbgdata->in.irq_base_id, (void *)
		&device->rdbg_data[device_id]);
bail:
	return err;
}

static int rdbg_release(struct inode *inode, struct file *filp)
{
	int device_id = -1;
	struct rdbg_device *rdbgdevice = &g_rdbg_instance;
	struct rdbg_data *rdbgdata = NULL;
	int err = 0;

	if (!inode || !rdbgdevice->rdbg_data) {
		pr_err("Memory not allocated yet");
		err = -ENODEV;
		goto bail;
	}

	device_id = MINOR(inode->i_rdev);
	rdbgdata = &rdbgdevice->rdbg_data[device_id];

	if (rdbgdata->device_opened == 1) {
		dev_dbg(rdbgdata->device, "%s: Destroying %s.", __func__,
			proc_info[device_id].name);
		rdbgdata->device_opened = 0;
		complete(&(rdbgdata->work));
		free_irq(rdbgdata->in.irq_base_id, (void *)
			&rdbgdevice->rdbg_data[device_id]);
		if (rdbgdevice->rdbg_data[device_id].producer_smrb.initialized)
			smq_dtor(&(rdbgdevice->rdbg_data[device_id].
				producer_smrb));
		if (rdbgdevice->rdbg_data[device_id].consumer_smrb.initialized)
			smq_dtor(&(rdbgdevice->rdbg_data[device_id].
				consumer_smrb));
		mutex_destroy(&rdbgdata->write_mutex);
	}

	filp->private_data = NULL;

bail:
	return err;
}

static ssize_t rdbg_read(struct file *filp, char __user *buf, size_t size,
	loff_t *offset)
{
	int err = 0;
	struct rdbg_data *rdbgdata = filp->private_data;
	void *p_sent_buffer = NULL;
	int nsize = 0;
	int more = 0;

	if (!rdbgdata) {
		pr_err("Invalid argument");
		err = -EINVAL;
		goto bail;
	}

	dev_dbg(rdbgdata->device, "%s: In receive", __func__);
	err = wait_for_completion_interruptible(&(rdbgdata->work));
	if (err) {
		dev_err(rdbgdata->device, "%s: Error in wait", __func__);
		goto bail;
	}

	smq_check_queue_reset(&(rdbgdata->consumer_smrb),
		&(rdbgdata->producer_smrb));
	if (SMQ_SUCCESS != smq_receive(&(rdbgdata->consumer_smrb),
			&p_sent_buffer, &nsize, &more)) {
		dev_err(rdbgdata->device, "%s: Error in smq_recv(). Err code = %d",
			__func__, err);
		err = -ENODATA;
		goto bail;
	}

	size = ((size < nsize) ? size : nsize);
	err = copy_to_user(buf, p_sent_buffer, size);
	if (err != 0) {
		dev_err(rdbgdata->device, "%s: Error in copy_to_user(). Err code = %d",
			__func__, err);
		err = -ENODATA;
		goto bail;
	}

	smq_free(&(rdbgdata->consumer_smrb), p_sent_buffer);
	err = size;
	dev_dbg(rdbgdata->device, "%s: Read data to buffer with address 0x%lx",
		__func__, (unsigned long) buf);

bail:
	dev_dbg(rdbgdata->device, "%s: Returning from receive", __func__);
	return err;
}

static ssize_t rdbg_write(struct file *filp, const char __user *buf,
	size_t size, loff_t *offset)
{
	int err = 0;
	int num_retries = 0;
	struct rdbg_data *rdbgdata = filp->private_data;

	if (!rdbgdata) {
		pr_err("Invalid argument");
		err = -EINVAL;
		goto bail;
	}

	do {
		err = smq_alloc_send(&(rdbgdata->producer_smrb), buf, size);
		dev_dbg(rdbgdata->device, "%s, smq_alloc_send returned %d.",
			__func__, err);
	} while (err != 0 && num_retries++ < MAX_RETRIES);

	if (err != 0) {
		err = -ECOMM;
		goto bail;
	}

	send_interrupt_to_subsystem(rdbgdata);

	err = size;

bail:
	return err;
}


static const struct file_operations rdbg_fops = {
	.open = rdbg_open,
	.read =  rdbg_read,
	.write =  rdbg_write,
	.release = rdbg_release,
};

static int register_smp2p(char *node_name, struct gpio_info *gpio_info_ptr)
{
	struct device_node *node = NULL;
	int cnt = 0;
	int id = 0;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node) {
		cnt = of_gpio_count(node);
		if (cnt && gpio_info_ptr) {
			id = of_get_gpio(node, 0);
			gpio_info_ptr->gpio_base_id = id;
			gpio_info_ptr->irq_base_id = gpio_to_irq(id);
			return 0;
		}
	}
	return -EINVAL;
}

static int __init rdbg_init(void)
{
	int err = 0;
	struct rdbg_device *rdbgdevice = &g_rdbg_instance;
	int minor = 0;
	int major = 0;
	int minor_nodes_created = 0;

	char *rdbg_compatible_string = "qcom,smp2pgpio_client_rdbg_";
	int max_len = strlen(rdbg_compatible_string) + strlen("xx_out");

	char *node_name = kcalloc(max_len, sizeof(char), GFP_KERNEL);

	if (!node_name) {
		pr_err("Not enough memory");
		err = -ENOMEM;
		goto bail;
	}

	if (rdbgdevice->num_devices < 1 ||
		rdbgdevice->num_devices > SMP2P_NUM_PROCS) {
		pr_err("rgdb: invalid num_devices");
		err = -EDOM;
		goto name_bail;
	}

	rdbgdevice->rdbg_data = kcalloc(rdbgdevice->num_devices,
		sizeof(struct rdbg_data), GFP_KERNEL);
	if (!rdbgdevice->rdbg_data) {
		pr_err("Not enough memory for rdbg devices");
		err = -ENOMEM;
		goto name_bail;
	}

	err = alloc_chrdev_region(&rdbgdevice->dev_no, 0,
		rdbgdevice->num_devices, "rdbgctl");
	if (err) {
		pr_err("Error in alloc_chrdev_region.");
		goto data_bail;
	}
	major = MAJOR(rdbgdevice->dev_no);

	cdev_init(&rdbgdevice->cdev, &rdbg_fops);
	rdbgdevice->cdev.owner = THIS_MODULE;
	err = cdev_add(&rdbgdevice->cdev, MKDEV(major, 0),
		rdbgdevice->num_devices);
	if (err) {
		pr_err("Error in cdev_add");
		goto chrdev_bail;
	}

	rdbgdevice->class = class_create(THIS_MODULE, "rdbg");
	if (IS_ERR(rdbgdevice->class)) {
		err = PTR_ERR(rdbgdevice->class);
		pr_err("Error in class_create");
		goto cdev_bail;
	}

	for (minor = 0; minor < rdbgdevice->num_devices; minor++) {
		if (!proc_info[minor].name)
			continue;

		if (snprintf(node_name, max_len, "%s%d_in",
			rdbg_compatible_string, minor) <= 0) {
			pr_err("Error in snprintf");
			err = -ENOMEM;
			goto device_bail;
		}

		if (register_smp2p(node_name,
			&rdbgdevice->rdbg_data[minor].in)) {
			pr_debug("No incoming device tree entry found for %s",
				proc_info[minor].name);
			continue;
		}

		if (snprintf(node_name, max_len, "%s%d_out",
			rdbg_compatible_string, minor) <= 0) {
			pr_err("Error in snprintf");
			err = -ENOMEM;
			goto device_bail;
		}

		if (register_smp2p(node_name,
			&rdbgdevice->rdbg_data[minor].out)) {
			pr_err("No outgoing device tree entry found for %s",
				proc_info[minor].name);
			err = -EINVAL;
			goto device_bail;
		}

		rdbgdevice->rdbg_data[minor].device = device_create(
			rdbgdevice->class, NULL, MKDEV(major, minor),
			NULL, "%s", proc_info[minor].name);
		if (IS_ERR(rdbgdevice->rdbg_data[minor].device)) {
			err = PTR_ERR(rdbgdevice->rdbg_data[minor].device);
			pr_err("Error in device_create");
			goto device_bail;
		}
		rdbgdevice->rdbg_data[minor].device_initialized = 1;
		minor_nodes_created++;
		dev_dbg(rdbgdevice->rdbg_data[minor].device,
			"%s: created /dev/%s c %d %d'", __func__,
			proc_info[minor].name, major, minor);
	}

	if (!minor_nodes_created) {
		pr_err("No device tree entries found");
		err = -EINVAL;
		goto class_bail;
	}

	goto name_bail;

device_bail:
	for (--minor; minor >= 0; minor--) {
		if (rdbgdevice->rdbg_data[minor].device_initialized)
			device_destroy(rdbgdevice->class,
				MKDEV(MAJOR(rdbgdevice->dev_no), minor));
	}
class_bail:
	class_destroy(rdbgdevice->class);
cdev_bail:
	cdev_del(&rdbgdevice->cdev);
chrdev_bail:
	unregister_chrdev_region(rdbgdevice->dev_no, rdbgdevice->num_devices);
data_bail:
	kfree(rdbgdevice->rdbg_data);
name_bail:
	kfree(node_name);
bail:
	return err;
}

static void __exit rdbg_exit(void)
{
	struct rdbg_device *rdbgdevice = &g_rdbg_instance;
	int minor;

	for (minor = 0; minor < rdbgdevice->num_devices; minor++) {
		if (rdbgdevice->rdbg_data[minor].device_initialized) {
			device_destroy(rdbgdevice->class,
				MKDEV(MAJOR(rdbgdevice->dev_no), minor));
		}
	}
	class_destroy(rdbgdevice->class);
	cdev_del(&rdbgdevice->cdev);
	unregister_chrdev_region(rdbgdevice->dev_no, 1);
	kfree(rdbgdevice->rdbg_data);
}

module_init(rdbg_init);
module_exit(rdbg_exit);

MODULE_DESCRIPTION("rdbg module");
MODULE_LICENSE("GPL v2");
