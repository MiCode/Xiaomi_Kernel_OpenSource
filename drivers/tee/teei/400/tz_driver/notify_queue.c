// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
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
#include "irq_register.h"
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

unsigned long long switch_input_index;
unsigned long long switch_output_index;

static unsigned long create_notify_queue(unsigned long size)
{
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
		goto return_fn;
	}

	switch_input_index = ((unsigned long)switch_input_index + 1) % 10000;

	/* Call the smc_fast_call */
	retVal = add_work_entry(SMC_CALL_TYPE, N_INIT_T_FC_BUF,
			virt_to_phys((void *)buff_addr), size, 0);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the add_work_entry!\n",
				__func__, __LINE__);
		goto Destroy_buffer;

	}

	teei_notify_switch_fn();

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

	check_index = ((unsigned long)check_index + 1) % 10000;

	/* Call the smp_mb() to make sure setting entry before setting head */
	smp_mb();

	switch_input_index = ((unsigned long)switch_input_index + 1) % 10000;

	temp_head->put_index = ((unsigned long)temp_head->put_index + 1)
					% (unsigned long)temp_head->max_count;

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

	check_index = ((unsigned long)check_index + 1) % 10000;

	/* Call the smp_mb() to make sure setting entry before setting head */
	smp_mb();

	temp_head->put_index = ((unsigned long)temp_head->put_index + 1)
					% (unsigned long)temp_head->max_count;

	/* Call the rmb() to make sure setting entry before setting head */
	rmb();

	teei_secure_call(N_ADD_TRIGGER_IRQ_COUNT, 0, 0, 0);

	mutex_unlock(&(g_nq_stat.nt_t_mutex));

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

		get = ((unsigned long)get + 1) %
				(unsigned long)temp_head->max_count;

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
