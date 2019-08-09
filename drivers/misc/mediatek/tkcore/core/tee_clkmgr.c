/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
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
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/string.h>

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/tee_clkmgr.h>
#include <linux/tee_client_api.h>

#include "tee_clkmgr_priv.h"

struct clkmgr_handle {
	uint32_t token;
	void *e, *d;
	const void *p0, *p1, *p2;
	size_t argnum;
	struct list_head le;
};

/* sync with tee-os */
enum tee_clkmgr_type {
	TEE_CLKMGR_TYPE_SPI = 0,
	TEE_CLKMGR_TYPE_I2C,
	TEE_CLKMGR_TYPE_I2C_DMA
};

static const char * const clkid[] = {
	[TEE_CLKMGR_TYPE_SPI] = "spi",
	[TEE_CLKMGR_TYPE_I2C] = "i2c",
	[TEE_CLKMGR_TYPE_I2C_DMA] = "i2c-dma",
};

static LIST_HEAD(clk_list);
static DEFINE_SPINLOCK(clk_list_lock);

/* called inside list_lock */
static struct clkmgr_handle *get_clkmgr_handle(uint32_t token)
{
	struct clkmgr_handle *h;

	list_for_each_entry(h, &clk_list, le) {
		if (h->token == token)
			return h;
	}

	return NULL;
}

int tee_clkmgr_handle(uint32_t token, uint32_t op)
{
	struct clkmgr_handle *ph, h;
	void *fn;

	spin_lock(&clk_list_lock);

	ph = get_clkmgr_handle(token | TEE_CLKMGR_TOKEN_NOT_LEGACY);
	if (ph == NULL) {
		pr_err("invalid token %u\n", token);
		spin_unlock(&clk_list_lock);
		return TEEC_ERROR_ITEM_NOT_FOUND;
	}

	memcpy(&h, ph, sizeof(h));

	spin_unlock(&clk_list_lock);

	fn = (op & TEE_CLKMGR_OP_ENABLE) ? h.e : h.d;

	if (h.argnum == 0) {
		((void (*)(void)) fn) ();
	} else if (h.argnum == 1) {
		((void (*)(const void *)) fn) (h.p0);
	} else if (h.argnum == 2) {
		((void (*)(const void *, const void *)) fn) (h.p0, h.p1);
	} else if (h.argnum == 3) {
		((void (*) (const void *, const void *, const void *)) fn)
			(h.p0, h.p1, h.p2);
	} else {
		pr_err("unsupported token %u argnum %zu\n",
			h.token, h.argnum);
		return TEEC_ERROR_NOT_SUPPORTED;
	}

	return 0;
}
EXPORT_SYMBOL(tee_clkmgr_handle);

int tee_clkmgr_register(const char *clkname, int id, void *e, void *d,
	void *p0, void *p1, void *p2, size_t argnum)
{
	size_t n;

	struct clkmgr_handle *h, *w;

	if (argnum > 3) {
		pr_err("does not support argnum %zu\n", argnum);
		return -EINVAL;
	}

	for (n = 0; n < ARRAY_SIZE(clkid); n++) {
		if (clkid[n] && strcmp(clkname, clkid[n]) == 0)
			break;
	}

	if (n == ARRAY_SIZE(clkid)) {
		pr_err("invalid clkname %s\n", clkname);
		return -EINVAL;
	}

	if ((id << TEE_CLKMGR_TOKEN_ID_SHIFT) &
		(TEE_CLKMGR_TOKEN_TYPE_MASK << TEE_CLKMGR_TOKEN_TYPE_SHIFT)) {
		pr_err("%s-%d: invalid id\n", clkname, id);
		return -EINVAL;
	}

	h = kmalloc(sizeof(struct clkmgr_handle), GFP_KERNEL);
	if (h == NULL)
		return -ENOMEM;

	h->token = TEE_CLKMGR_TOKEN((uint32_t) n, (uint32_t) id);
	h->e = e;
	h->d = d;
	h->p0 = p0;
	h->p1 = p1;
	h->p2 = p2;
	h->argnum = argnum;

	spin_lock(&clk_list_lock);

	/* check for duplication */
	list_for_each_entry(w, &clk_list, le) {
		if (w->token == h->token) {
			pr_err("clk 0x%x already registerred\n",
				h->token);
			spin_unlock(&clk_list_lock);
			return -EINVAL;
		}
	}

	list_add(&(h->le), &clk_list);
	spin_unlock(&clk_list_lock);

	return 0;
}
EXPORT_SYMBOL(tee_clkmgr_register);

int tee_clkmgr_init(void)
{
	return 0;
}

void tee_clkmgr_exit(void)
{
	struct clkmgr_handle *h, *n;

	spin_lock(&clk_list_lock);

	list_for_each_entry_safe(h, n, &clk_list, le) {
		list_del(&(h->le));
		kfree(h);
	}

	spin_unlock(&clk_list_lock);
}
