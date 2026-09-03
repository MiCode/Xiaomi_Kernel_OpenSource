// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : wcn_bus.c
 * Abstract : This file is a implementation for wcn sdio hal function
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <misc/wcn_bus.h>

#include "bus_common.h"
#include "sprd_wcn.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "WCN BUS: " fmt

struct buffer_pool_t {
	unsigned int size;
	unsigned int free;
	unsigned int payload;
	void *head;
	char *mem;
	spinlock_t lock;
};

struct chn_info_t {
	struct mchn_ops_t *ops;
	struct mutex callback_lock;
	struct buffer_pool_t pool;
};

static struct sprdwcn_bus_ops *wcn_bus_ops;

static struct chn_info_t g_sipc_chn_info[SIPC_CHN_MAX_NUM];
static struct chn_info_t g_chn_info[CHN_MAX_NUM];

static bool mdbg_white_list_str_check(char *buf, ssize_t len)
{
	int i = 0;
	char * const str_in_assert[] = {
		"at+sleep_switch",
	};

	for (i = 0; i < ARRAY_SIZE(str_in_assert); i++) {
		if (!strncasecmp(buf, str_in_assert[i], strlen(str_in_assert[i]))) {
			pr_info("%s (%s) white list\n", __func__, buf);
			return true;
		}
	}

	return false;
}

bool wcn_push_list_condition_check(
		struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	size_t rsvlen = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (sprdwcn_bus_get_carddump_status() || wcn_is_assert()) {
		if (num != 1) {
			pr_err("%s mbuf(%d) does not allow sending(WCN ASSERT)\n",
				__func__, num);
			return false;
		}

		if (g_match_config && g_match_config->unisoc_wcn_sipc)
			rsvlen = 0;
		else if (g_match_config && g_match_config->unisoc_wcn_sdio)
			rsvlen = SDIOHAL_PUB_HEAD_RSV;
		else
			rsvlen = PUB_HEAD_RSV;

		return mdbg_white_list_str_check(head->buf + rsvlen, head->len);
	}

	return true;
}

int wlan_status = 1;

int sprd_wlan_power_status_sync(int option, int value)
{
	if (option == 1)
		wlan_status = value;
	return wlan_status;
}
EXPORT_SYMBOL_GPL(sprd_wlan_power_status_sync);

static struct chn_info_t *chn_info(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_sipc)
		return g_sipc_chn_info;
	else
		return g_chn_info;
}

static int buf_list_check(struct buffer_pool_t *pool, struct mbuf_t *head,
			  struct mbuf_t *tail, int num)
{
	int i;
	struct mbuf_t *mbuf;

	if (num == 0)
		return 0;
	if (head == NULL)
		return 0;

	for (i = 0, mbuf = head; i < num; i++) {
		if ((i == (num - 1)) && (mbuf != tail)) {
			pr_err("%s(0x%lx, 0x%lx, %d), err 1\n", __func__,
			       (unsigned long)virt_to_phys(head),
			       (unsigned long)virt_to_phys(tail), num);
			WARN_ON_ONCE(1);
		}
		WARN_ON_ONCE(!mbuf);
		WARN_ON_ONCE((char *)mbuf < pool->mem ||
			(char *)mbuf > pool->mem + ((sizeof(struct mbuf_t)
			+ pool->payload) * pool->size));
		if (mbuf != NULL)
			mbuf = mbuf->next;
		else
			return 0;
	}

	if (tail->next != NULL) {
		pr_err("%s(0x%lx, 0x%lx, %d), err 2\n", __func__,
		       (unsigned long)virt_to_phys(head),
		       (unsigned long)virt_to_phys(tail), num);
		WARN_ON_ONCE(1);
	}

	return 0;
}

static int buf_pool_check(struct buffer_pool_t *pool)
{
	int i;
	struct mbuf_t *mbuf;

	if (pool->head == NULL)
		return 0;

	for (i = 0, mbuf = pool->head; i < (int)pool->free; i++) {
		WARN_ON_ONCE(!mbuf);
		WARN_ON_ONCE((char *)mbuf < pool->mem ||
			(char *)mbuf > pool->mem + ((sizeof(struct mbuf_t)
			+ pool->payload) * pool->size));
		if (mbuf != NULL)
			mbuf = mbuf->next;
		else
			return 0;
	}

	if (mbuf != NULL) {
		pr_err("%s(0x%p) err\n", __func__, pool);
		WARN_ON_ONCE(1);
	}

	return 0;
}

/* mbuf init and list, current payload is zero */
static int buf_pool_init(struct buffer_pool_t *pool, int size, int payload)
{
	int i;
	struct mbuf_t *mbuf, *next;

	pool->size = size;
	pool->payload = payload;
	spin_lock_init(&(pool->lock));
	pool->mem = kzalloc((sizeof(struct mbuf_t) + payload) * size,
			    GFP_KERNEL);
	if (!pool->mem)
		return -ENOMEM;

	pr_debug("mbuf_pool->mem:0x%lx\n",
		 (unsigned long)virt_to_phys(pool->mem));
	pool->head = (struct mbuf_t *) (pool->mem);
	for (i = 0, mbuf = (struct mbuf_t *)(pool->head);
	     i < (size - 1); i++) {
		mbuf->seq = i;
		pr_debug("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
			 (unsigned long)mbuf,
			 (unsigned long)virt_to_phys(mbuf));
		next = (struct mbuf_t *)((char *)mbuf +
			sizeof(struct mbuf_t) + payload);
		mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
		mbuf->len = payload;
		mbuf->next = next;
		mbuf = next;
	}
	pr_debug("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
		 (unsigned long)mbuf,
		 (unsigned long)virt_to_phys(mbuf));
	mbuf->seq = i;
	mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
	mbuf->len = payload;
	mbuf->next = NULL;
	pool->free = size;

	return 0;
}

static int buf_pool_deinit(struct buffer_pool_t *pool)
{
	memset(pool->mem, 0x00,
	       (sizeof(struct mbuf_t) + pool->payload) * pool->size);
	kfree(pool->mem);
	pool->mem = NULL;
	pool->free = 0;

	return 0;
}

/* take mbuf from pool list */
int buf_list_alloc(int chn, struct mbuf_t **head,
		   struct mbuf_t **tail, int *num)
{
	int i;
	struct buffer_pool_t *pool;
	struct mbuf_t *temp_tail;
	struct chn_info_t *chn_inf = chn_info();

	pool = &((chn_inf + chn)->pool);

	if ((*num <= 0) || (pool->free <= 0)) {
		pr_err("[+]%s err, num %d, free %d)\n",
		       __func__, *num, pool->free);
		*num = 0;
		*head = *tail = NULL;
		return -1;
	}

	spin_lock_bh(&(pool->lock));
	buf_pool_check(pool);
	if (*num > (int)pool->free)
		*num = pool->free;

	for (i = 1, temp_tail = pool->head; i < *num; i++)
		temp_tail = temp_tail->next;

	*head = pool->head;
	*tail = temp_tail;
	pool->head = temp_tail->next;
	temp_tail->next = NULL;
	pool->free -= *num;
	buf_list_check(pool, *head, *tail, *num);
	spin_unlock_bh(&(pool->lock));

	return 0;
}

int buf_list_is_empty(int chn)
{
	struct buffer_pool_t *pool;
	struct chn_info_t *chn_inf = chn_info();

	pool = &((chn_inf + chn)->pool);
	return pool->free <= 0;
}

int buf_list_is_full(int chn)
{
	struct buffer_pool_t *pool;
	struct chn_info_t *chn_inf = chn_info();

	pool = &((chn_inf + chn)->pool);
	return pool->free == pool->size;
}

int buf_list_free(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct buffer_pool_t *pool;
	struct chn_info_t *chn_inf = chn_info();

	if ((head == NULL) || (tail == NULL) || (num == 0)) {
		pr_err("%s(%d, 0x%lx, 0x%lx, %d)\n", __func__, chn,
		       (unsigned long)virt_to_phys(head),
		       (unsigned long)virt_to_phys(tail), num);
		return -1;
	}

	pool = &((chn_inf + chn)->pool);
	if (pool->mem == NULL) {
		pr_err("%s channel has been released\n", __func__);
		return -1;
	}
	spin_lock_bh(&(pool->lock));
	buf_list_check(pool, head, tail, num);
	tail->next = pool->head;
	pool->head = head;
	pool->free += num;
	buf_pool_check(pool);
	spin_unlock_bh(&(pool->lock));

	return 0;
}

int bus_chn_init(struct mchn_ops_t *ops, int hif_type)
{
	int ret = 0;
	struct chn_info_t *chn_inf = chn_info();

	pr_info("[+]%s(%d, %d)\n", __func__, ops->channel, ops->hif_type);
	if ((chn_inf + ops->channel)->ops != NULL) {
		pr_err("%s err, hif_type %d\n", __func__, ops->hif_type);
		WARN_ON_ONCE(1);
		return -1;
	}

	mutex_init(&(chn_inf + ops->channel)->callback_lock);
	mutex_lock(&(chn_inf + ops->channel)->callback_lock);
	ops->hif_type = hif_type;
	(chn_inf + ops->channel)->ops = ops;
	if (ops->pool_size > 0)
		ret = buf_pool_init(&((chn_inf + ops->channel)->pool),
				    ops->pool_size, 0);
	mutex_unlock(&(chn_inf + ops->channel)->callback_lock);

	pr_info("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}

int bus_chn_deinit(struct mchn_ops_t *ops)
{
	int ret = 0;
	struct chn_info_t *chn_inf = chn_info();

	pr_info("[+]%s(%d, %d)\n", __func__, ops->channel, ops->hif_type);
	if ((chn_inf + ops->channel)->ops == NULL) {
		pr_err("%s err\n", __func__);
		return -1;
	}

	mutex_lock(&(chn_inf + ops->channel)->callback_lock);
	if (ops->pool_size > 0)
		ret = buf_pool_deinit(&((chn_inf + ops->channel)->pool));
	(chn_inf + ops->channel)->ops = NULL;
	mutex_unlock(&(chn_inf + ops->channel)->callback_lock);
	mutex_destroy(&(chn_inf + ops->channel)->callback_lock);

	pr_info("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}

struct mchn_ops_t *chn_ops(int channel)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_sipc) {
		if (channel >= SIPC_CHN_MAX_NUM || channel < 0)
			return NULL;

		return g_sipc_chn_info[channel].ops;
	}

	if (channel >= CHN_MAX_NUM || channel < 0)
		return NULL;

	return g_chn_info[channel].ops;
}

int module_ops_register(struct sprdwcn_bus_ops *ops)
{
	if (wcn_bus_ops) {
		WARN_ON_ONCE(1);
		return -EBUSY;
	}

	wcn_bus_ops = ops;

	return 0;
}

void module_ops_unregister(void)
{
	wcn_bus_ops = NULL;
}

struct sprdwcn_bus_ops *get_wcn_bus_ops(void)
{
	return wcn_bus_ops;
}
EXPORT_SYMBOL_GPL(get_wcn_bus_ops);
