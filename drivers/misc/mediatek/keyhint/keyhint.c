/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/key.h>
#include <linux/module.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <mt-plat/keyhint.h>

/* #define KH_DEBUG */

#ifdef KH_DEBUG
#define kh_info(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#else
#define kh_info(fmt, ...)
#endif

#define kh_err(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)

int kh_register(struct kh_dev *dev, unsigned int key_bits,
		unsigned int key_slot)
{
	int size;
	int ret = 0;

	if (dev->kh) {
		kh_info("already registered, dev 0x%p\n", dev);
		return -EPERM;
	}

	if (key_bits % (sizeof(unsigned int) * BITS_PER_LONG)) {
		kh_info("key_bits %u shall be multiple of %u\n",
			key_bits, BITS_PER_LONG);
	}

	size = (key_bits / BITS_PER_BYTE) * key_slot;

	kh_info("key_bits=%u, key_slot=%u, size=%u bytes\n",
		key_bits, key_slot, size);

	dev->kh = kzalloc(size, GFP_KERNEL);

	if (!dev->kh)
		goto nomem_kh;

	size = key_slot * sizeof(unsigned long long);
	dev->kh_last_access = kzalloc(size, GFP_KERNEL);

	if (!dev->kh_last_access)
		goto nomem_last_access;

	size = key_slot * sizeof(unsigned short);
	dev->kh_slot_usage_cnt = kzalloc(size, GFP_KERNEL);

	if (!dev->kh_slot_usage_cnt)
		goto nomem_slot_usage_cnt;

	dev->kh_slot_total_cnt = key_slot;
	dev->kh_unit_per_key = (key_bits / BITS_PER_BYTE) /
		sizeof(unsigned long);
	dev->kh_slot_active_cnt = 0;

	kh_info("kh=%p, kh_last_access=%p, kh_slot_usage_cnt=%p\n",
		dev->kh, dev->kh_last_access, dev->kh_slot_usage_cnt);

	goto exit;

nomem_slot_usage_cnt:
	kfree(dev->kh_last_access);
nomem_last_access:
	kfree(dev->kh);
nomem_kh:
	ret = -ENOMEM;
exit:
	kh_info("register ret=%d\n", ret);
	return ret;
}

static int kh_get_free_slot(struct kh_dev *dev)
{
	int i, min_slot;
	unsigned long long min_time = LLONG_MAX;

	if (dev->kh_slot_active_cnt < dev->kh_slot_total_cnt) {
		dev->kh_slot_active_cnt++;

		kh_info("new, slot=%d\n", (dev->kh_slot_active_cnt - 1));

		return (dev->kh_slot_active_cnt - 1);
	}

	min_slot = dev->kh_slot_active_cnt;

	for (i = 0; i < dev->kh_slot_active_cnt; i++) {
		if ((dev->kh_slot_usage_cnt[i] == 0) &&
			dev->kh_last_access[i] < min_time) {
			min_time = dev->kh_last_access[i];
			min_slot = i;
		}
	}

	if (min_slot == dev->kh_slot_active_cnt) {
		kh_err("no available slot!\n");
		return -ENOMEM;
	}

	kh_info("vic, slot=%d, mint=%lu\n", min_slot, min_time);

	return min_slot;
}

int kh_release_hint(struct kh_dev *dev, int slot)
{
	if (unlikely(!dev->kh))
		return -ENODEV;

	if (unlikely(!dev->kh_slot_usage_cnt[slot])) {
		kh_err("unbalanced get and release! slot=%d\n", slot);

		/* shall we bug on here? */
		return -1;
	}

	dev->kh_slot_usage_cnt[slot]--;

	kh_info("rel, %d, %d\n", slot,
		dev->kh_slot_usage_cnt[slot]);

	return 0;
}

int kh_get_hint(struct kh_dev *dev, const char *key, int *need_update)
{
	int i, j, matched, matched_slot;
	unsigned long *ptr_kh, *ptr_key;

	if (unlikely(!dev->kh || !need_update)) {
		kh_info("get, err, key=0x%lx\n", *(unsigned long *)key);
		return -ENODEV;
	}

	/* round 1: simple match */

	matched = 0;
	matched_slot = 0;
	ptr_kh = (unsigned long *)dev->kh;
	ptr_key = (unsigned long *)key;

	for (i = 0; i < dev->kh_slot_active_cnt; i++) {

		if (*ptr_kh == *ptr_key) {
			matched_slot = i;
			matched++;
		}

		ptr_kh += dev->kh_unit_per_key;
	}

	if (matched == 1) {

		/* fully match rest part to ensure 100% matched */

		ptr_kh = (unsigned long *)dev->kh;
		ptr_kh += (dev->kh_unit_per_key * matched_slot);

		for (i = 0; i < dev->kh_unit_per_key - 1; i++) {

			ptr_kh++;
			ptr_key++;

			if (*ptr_kh != *ptr_key) {

				matched = 0;
				break;
			}
		}

		if (matched) {
			*need_update = 0;
			dev->kh_last_access[matched_slot] =
				sched_clock();

			dev->kh_slot_usage_cnt[matched_slot]++;

			kh_info("get, 1, %d, key=0x%lx, %d\n",
				matched_slot, *(unsigned long *)key,
				dev->kh_slot_usage_cnt[matched_slot]);


			return matched_slot;
		}
	}

	/* round 2: full match if simple match finds multiple targets */

	if (matched) {

		matched = 0;

		for (i = 0; i < dev->kh_slot_active_cnt; i++) {

			ptr_kh = (unsigned long *)dev->kh;
			ptr_kh += (i * dev->kh_unit_per_key);
			ptr_key = (unsigned long *)key;

			for (j = 0; j < dev->kh_unit_per_key; j++) {
				if (*ptr_kh++ != *ptr_key++)
					break;
			}

			if (j == dev->kh_unit_per_key) {
				*need_update = 0;
				dev->kh_last_access[i] =
					sched_clock();

				dev->kh_slot_usage_cnt[i]++;

				kh_info("get, 2, %d, key=0x%lx %d\n",
					i, *(unsigned long *)key,
					dev->kh_slot_usage_cnt[i]);


				return i;
			}
		}
	}

	/* nothing matched, add new hint */

	j = kh_get_free_slot(dev);

	if (j < 0)
		return j;

	ptr_kh = (unsigned long *)dev->kh;
	ptr_kh += (j * dev->kh_unit_per_key);
	ptr_key = (unsigned long *)key;

	for (i = 0; i < dev->kh_unit_per_key; i++)
		*ptr_kh++ = *ptr_key++;

	dev->kh_last_access[j] = sched_clock();
	dev->kh_slot_usage_cnt[j]++;

	*need_update = 1;

	kh_info("get, n, %d, key=0x%lx, %d\n", j,
		*(unsigned long *)key,
		dev->kh_slot_usage_cnt[j]);

	return j;
}

static int kh_reset(struct kh_dev *dev)
{
	if (unlikely(!dev->kh))
		return -ENODEV;

	dev->kh_slot_active_cnt = 0;

	memset(dev->kh_slot_usage_cnt, 0,
		sizeof(unsigned short) *
		dev->kh_slot_total_cnt);

	kh_info("rst, dev=0x%p\n", dev);

	return 0;
}

int kh_suspend(struct kh_dev *dev)
{
	int i;

	if (unlikely(!dev->kh))
		return -ENODEV;

	/* shall have zero key reference before suspend */
	for (i = 0; i < dev->kh_slot_active_cnt; i++)
		WARN_ON(dev->kh_slot_usage_cnt[i]);

	return kh_reset(dev);
}

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Key Hint");

