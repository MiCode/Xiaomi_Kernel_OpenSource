/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cpu.h>
#include "notify_queue.h"
#include "teei_id.h"
#include "teei_log.h"
#include "utdriver_macro.h"
#include "teei_common.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include <switch_queue.h>
#include <teei_secure_api.h>
#include <nt_smc_call.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct teei_queue_stat {
	struct mutex nq_stat_mutex;
	unsigned long long get;
	struct mutex nt_t_mutex;
	struct mutex nt_t_bdrv_mutex;
};

static struct teei_queue_stat g_nq_stat;

static unsigned long nt_t_buffer;
static unsigned long t_nt_buffer;
static unsigned long nt_t_bdrv_buffer;

/***********************************************************************
 *
 * create_notify_queue:
 *   Create the two way notify queues between T_OS and NT_OS.
 *
 * argument:
 *   size    the notify queue size.
 *
 * return value:
 *   EINVAL  invalid argument
 *   ENOMEM  no enough memory
 *   EAGAIN  The command ID in the response is NOT accordant to the request.
 *
 ***********************************************************************/
struct teei_queue_param {
	unsigned long long phys_addr;
	unsigned long long size;
};

void secondary_init_cmdbuf(void *info)
{
	struct teei_queue_param *cd = (struct teei_queue_param *)info;
	unsigned long smc_type = 2;

	smc_type = teei_secure_call(N_INIT_T_FC_BUF,
				cd->phys_addr, cd->size, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

}


static unsigned long create_notify_queue(unsigned long size)
{
	struct teei_queue_param nq_param;
	unsigned long buff_addr = 0;
	long retVal = 0;

	/* Create the double NQ buffer. */
#ifdef UT_DMA_ZONE
	buff_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA,
					get_order(ROUND_UP(size, SZ_4K)));
#else
	buff_addr = (unsigned long) __get_free_pages(GFP_KERNEL,
					get_order(ROUND_UP(size, SZ_4K)));
#endif
	if ((unsigned char *)buff_addr == NULL) {
		IMSG_ERROR("[%s][%d]: Alloc queue buffer failed.\n",
					__func__, __LINE__);
		retVal =  -ENOMEM;
		goto return_fn;
	}

	nq_param.phys_addr = virt_to_phys((void *)buff_addr);
	nq_param.size = size;


	/* Call the smc_fast_call */
	retVal = add_work_entry(INIT_CMD_CALL, (unsigned long)&nq_param);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the add_work_entry!\n",
				__func__, __LINE__);
		goto Destroy_buffer;

	}

	down(&(boot_sema));

	return buff_addr;

Destroy_buffer:
	free_pages(buff_addr, get_order(ROUND_UP(size, SZ_4K)));
return_fn:
	return 0;
}

static void NQ_init(unsigned long NQ_buff)
{
	memset((char *)NQ_buff, 0, NQ_BUFF_SIZE);
}

static int init_nq_head(unsigned long buffer_addr, unsigned int type)
{
	struct NQ_head *temp_head = NULL;

	temp_head = (struct NQ_head *)buffer_addr;

	memset(temp_head, 0, NQ_BLOCK_SIZE);
	temp_head->nq_type = type;
	temp_head->max_count = BLOCK_MAX_COUNT;
	temp_head->put_index = 0;

	return 0;
}

int add_nq_entry(unsigned long long cmd_ID, unsigned long long sub_cmd_ID,
			unsigned long long block_p, unsigned long long p0,
			unsigned long long p1, unsigned long long p2)
{
	static unsigned long long check_index;
	struct NQ_head *temp_head = NULL;
	struct NQ_entry *temp_entry = NULL;

	mutex_lock(&(g_nq_stat.nt_t_mutex));

	temp_head = (struct NQ_head *)nt_t_buffer;
	temp_entry = (struct NQ_entry *)(nt_t_buffer + NQ_BLOCK_SIZE
				+ temp_head->put_index * NQ_BLOCK_SIZE);

	temp_entry->cmd_ID = cmd_ID;
	temp_entry->sub_cmd_ID = sub_cmd_ID;
	temp_entry->block_p = block_p;
	temp_entry->param[0] = p0;
	temp_entry->param[1] = p1;
	temp_entry->param[2] = p2;
	temp_entry->param[3] = block_p;
	temp_entry->param[4] = check_index;

	check_index = (check_index + 1) % 10000;

	/* Call the smp_mb() to make sure setting entry before setting head */
	smp_mb();

	temp_head->put_index = (temp_head->put_index + 1)
					% temp_head->max_count;

	/* Call the rmb() to make sure setting entry before setting head */
	rmb();

	teei_secure_call(N_ADD_TRIGGER_IRQ_COUNT, 0, 0, 0);

	mutex_unlock(&(g_nq_stat.nt_t_mutex));

	return 0;
}

int add_bdrv_nq_entry(unsigned long long cmd_ID, unsigned long long sub_cmd_ID,
			unsigned long long block_p, unsigned long long p0,
			unsigned long long p1, unsigned long long p2)
{
	static unsigned long long check_index;
	struct NQ_head *temp_head = NULL;
	struct NQ_entry *temp_entry = NULL;

	mutex_lock(&(g_nq_stat.nt_t_mutex));

	temp_head = (struct NQ_head *)nt_t_bdrv_buffer;
	temp_entry = (struct NQ_entry *)(nt_t_bdrv_buffer + NQ_BLOCK_SIZE
				+ temp_head->put_index * NQ_BLOCK_SIZE);

	temp_entry->cmd_ID = cmd_ID;
	temp_entry->sub_cmd_ID = sub_cmd_ID;
	temp_entry->block_p = block_p;
	temp_entry->param[0] = p0;
	temp_entry->param[1] = p1;
	temp_entry->param[2] = p2;
	temp_entry->param[3] = block_p;
	temp_entry->param[4] = check_index;

	check_index = (check_index + 1) % 10000;

	/* Call the smp_mb() to make sure setting entry before setting head */
	smp_mb();

	temp_head->put_index = (temp_head->put_index + 1)
					% temp_head->max_count;

	/* Call the rmb() to make sure setting entry before setting head */
	rmb();

	teei_secure_call(N_ADD_TRIGGER_IRQ_COUNT, 0, 0, 0);

	mutex_unlock(&(g_nq_stat.nt_t_mutex));

	return 0;
}

int show_t_nt_queue(void)
{
	int next_index = 0;
	struct NQ_entry *entry = NULL;
	struct NQ_head *temp_head = NULL;
	int i = 0;
	int max_cnt = 0;
	int put = 0;

	next_index = g_nq_stat.get;
	IMSG_PRINTK("---------- g_nq_stat.get = %d --------\n", next_index);

	temp_head = (struct NQ_head *)t_nt_buffer;
	max_cnt = temp_head->max_count;
	IMSG_PRINTK("---------- t_nt_buffer max_cnt = %d --------\n", max_cnt);
	put = temp_head->put_index;
	IMSG_PRINTK("---------- t_nt_buffer put = %d --------\n", put);

	for (i = 0; i < max_cnt; i++) {
		entry = (struct NQ_entry *)(t_nt_buffer +
				NQ_BLOCK_SIZE + i * NQ_BLOCK_SIZE);
		IMSG_PRINTK("t_nt_buff[%d].cmd_ID = %llu\n",
						i, entry->cmd_ID);
		IMSG_PRINTK("t_nt_buff[%d].sub_cmd_ID = %llu\n",
						i, entry->sub_cmd_ID);
		IMSG_PRINTK("t_nt_buff[%d].block_p = %llx\n",
						i, entry->block_p);
		IMSG_PRINTK("t_nt_buff[%d].param[0] = %llx\n",
						i, entry->param[0]);
		IMSG_PRINTK("t_nt_buff[%d].param[1] = %llx\n",
						i, entry->param[1]);
		IMSG_PRINTK("t_nt_buff[%d].param[2] = %llx\n",
						i, entry->param[2]);
		IMSG_PRINTK("t_nt_buff[%d].param[3] = %llx\n",
						i, entry->param[3]);
		IMSG_PRINTK("t_nt_buff[%d].param[4] = %llx\n",
						i, entry->param[4]);
	}

	IMSG_PRINTK("--------------------------------------------------\n");

	temp_head = (struct NQ_head *)nt_t_buffer;
	max_cnt = temp_head->max_count;
	IMSG_PRINTK("--------- nt_t_buffer max_cnt = %d --------\n", max_cnt);
	put = temp_head->put_index;
	IMSG_PRINTK("--------- nt_t_buffer put = %d ------------\n", put);

	for (i = 0; i < max_cnt; i++) {
		entry = (struct NQ_entry *)(nt_t_buffer +
					NQ_BLOCK_SIZE + i * NQ_BLOCK_SIZE);
		IMSG_PRINTK("nt_t_buff[%d].cmd_ID = %llu\n",
						i, entry->cmd_ID);
		IMSG_PRINTK("nt_t_buff[%d].sub_cmd_ID = %llu\n",
						i, entry->sub_cmd_ID);
		IMSG_PRINTK("nt_t_buff[%d].block_p = %llx\n",
						i, entry->block_p);
		IMSG_PRINTK("nt_t_buff[%d].param[0] = %llx\n",
						i, entry->param[0]);
		IMSG_PRINTK("nt_t_buff[%d].param[1] = %llx\n",
						i, entry->param[1]);
		IMSG_PRINTK("nt_t_buff[%d].param[2] = %llx\n",
						i, entry->param[2]);
		IMSG_PRINTK("nt_t_buff[%d].param[3] = %llx\n",
						i, entry->param[3]);
		IMSG_PRINTK("nt_t_buff[%d].param[4] = %llx\n",
						i, entry->param[4]);
	}

	return 0;
}

struct NQ_entry *get_nq_entry(void)
{
	struct NQ_head *temp_head = NULL;
	struct NQ_entry *temp_entry = NULL;
	unsigned long long put = 0;
	unsigned long long get = 0;

	temp_head = (struct NQ_head *)t_nt_buffer;

	put = temp_head->put_index;

	/* mutex_lock(&(g_nq_stat.nq_stat_mutex)); */

	get = g_nq_stat.get;

	if (put != get) {
		temp_entry = (struct NQ_entry *)(t_nt_buffer + NQ_BLOCK_SIZE
				+ get * NQ_BLOCK_SIZE);

		get = (get + 1)	% temp_head->max_count;

		g_nq_stat.get = get;
	}

	/* mutex_unlock(&(g_nq_stat.nq_stat_mutex)); */

	return temp_entry;
}

int create_nq_buffer(void)
{
	unsigned long nq_addr = 0;

	nq_addr = create_notify_queue(NQ_SIZE * 3);

	if (nq_addr == 0) {
		IMSG_ERROR("[%s][%d]:create_notify_queue failed.\n",
						__func__, __LINE__);
		return -ENOMEM;
	}

	nt_t_buffer = nq_addr;
	t_nt_buffer = nq_addr + NQ_SIZE;
	nt_t_bdrv_buffer = nq_addr + NQ_SIZE * 2;

	/* Get the Soter version from notify queue shared memory */
	set_soter_version();

	NQ_init(nt_t_buffer);
	NQ_init(t_nt_buffer);
	NQ_init(nt_t_bdrv_buffer);

	init_nq_head(nt_t_buffer, 0x00);
	init_nq_head(t_nt_buffer, 0x01);
	init_nq_head(nt_t_bdrv_buffer, 0x03);

	memset(&g_nq_stat, 0, sizeof(g_nq_stat));
	mutex_init(&(g_nq_stat.nq_stat_mutex));
	mutex_init(&(g_nq_stat.nt_t_mutex));
	mutex_init(&(g_nq_stat.nt_t_bdrv_mutex));

	return 0;
}

int set_soter_version(void)
{
	unsigned int versionlen = 0;
	char *version = NULL;

	memcpy(&versionlen, (void *)nt_t_buffer, sizeof(unsigned int));
	if (versionlen > 0 && versionlen < 100) {
		version = kmalloc(versionlen + 1, GFP_KERNEL);
		if (version == NULL)
			return -ENOMEM;

		memset(version, 0, versionlen + 1);
		memcpy(version, (void *)(nt_t_buffer + sizeof(unsigned int)),
				versionlen);
	} else
		return -EINVAL;

	IMSG_PRINTK("%s\n", version);
	kfree(version);

	return 0;
}
