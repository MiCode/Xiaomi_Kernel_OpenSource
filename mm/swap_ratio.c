/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mm_types.h>
#include <linux/swapfile.h>
#include <linux/swap.h>

#define SWAP_RATIO_GROUP_START (SWAP_FLAG_PRIO_MASK - 9) /* 32758 */
#define SWAP_RATIO_GROUP_END (SWAP_FLAG_PRIO_MASK) /* 32767 */
#define SWAP_FAST_WRITES (SWAPFILE_CLUSTER * (SWAP_CLUSTER_MAX / 8))
#define SWAP_SLOW_WRITES SWAPFILE_CLUSTER

/*
 * The fast/slow swap write ratio.
 * 100 indicates that all writes should
 * go to fast swap device.
 */
int sysctl_swap_ratio = 100;

/* Enable the swap ratio feature */
int sysctl_swap_ratio_enable;

static bool is_same_group(struct swap_info_struct *a,
		struct swap_info_struct *b)
{
	if (!sysctl_swap_ratio_enable)
		return false;

	if (!is_swap_ratio_group(a->prio))
		return false;

	if (a->prio == b->prio)
		return true;

	return false;
}

/* Caller must hold swap_avail_lock */
static int calculate_write_pending(struct swap_info_struct *si,
			struct swap_info_struct *n)
{
	int ratio = sysctl_swap_ratio;

	if ((ratio < 0) || (ratio > 100))
		return -EINVAL;

	if (WARN_ON(!(si->flags & SWP_FAST)))
		return -ENODEV;

	if ((n->flags & SWP_FAST) || !is_same_group(si, n))
		return -ENODEV;

	si->max_writes = ratio ? SWAP_FAST_WRITES : 0;
	n->max_writes  = ratio ? (SWAP_FAST_WRITES * 100) /
			ratio - SWAP_FAST_WRITES : SWAP_SLOW_WRITES;

	si->write_pending = si->max_writes;
	n->write_pending = n->max_writes;

	return 0;
}

static int swap_ratio_slow(struct swap_info_struct **si)
{
	struct swap_info_struct *n = NULL;
	int ret = 0;

	spin_lock(&(*si)->lock);
	spin_lock(&swap_avail_lock);
	if (&(*si)->avail_list == plist_last(&swap_avail_head)) {
		/* just to make skip work */
		n = *si;
		ret = -ENODEV;
		goto skip;
	}
	n = plist_next_entry(&(*si)->avail_list,
			struct swap_info_struct,
			avail_list);
	if (n == *si) {
		/* No other swap device */
		ret = -ENODEV;
		goto skip;
	}

	spin_unlock(&swap_avail_lock);
	spin_lock(&n->lock);
	spin_lock(&swap_avail_lock);

	if ((*si)->flags & SWP_FAST) {
		if ((*si)->write_pending) {
			(*si)->write_pending--;
			goto exit;
		} else {
			if ((n->flags & SWP_FAST) || !is_same_group(*si, n)) {
				/* Should never happen */
				ret = -ENODEV;
			} else if (n->write_pending) {
				/*
				 * Requeue fast device, since there are pending
				 * writes for slow device.
				 */
				plist_requeue(&(*si)->avail_list,
					&swap_avail_head);
				n->write_pending--;
				spin_unlock(&(*si)->lock);
				*si = n;
				goto skip;
			} else {
				if (calculate_write_pending(*si, n) < 0) {
					ret = -ENODEV;
					goto exit;
				}
				/* Restart from fast device */
				(*si)->write_pending--;
			}
		}
	} else {
		if (!(n->flags & SWP_FAST) || !is_same_group(*si, n)) {
			/* Should never happen */
			ret = -ENODEV;
		} else if (n->write_pending) {
			/*
			 * Pending writes for fast device.
			 * We reach here when slow device is swapped on first,
			 * before fast device.
			 */
			/* requeue slow device to the end */
			plist_requeue(&(*si)->avail_list, &swap_avail_head);
			n->write_pending--;
			spin_unlock(&(*si)->lock);
			*si = n;
			goto skip;
		} else {
			if ((*si)->write_pending) {
				(*si)->write_pending--;
			} else {
				if (calculate_write_pending(n, *si) < 0) {
					ret = -ENODEV;
					goto exit;
				}
				n->write_pending--;
				plist_requeue(&(*si)->avail_list,
					&swap_avail_head);
				spin_unlock(&(*si)->lock);
				*si = n;
				goto skip;
			}
		}
	}
exit:
	spin_unlock(&(*si)->lock);
skip:
	spin_unlock(&swap_avail_lock);
	/* n and si would have got interchanged */
	spin_unlock(&n->lock);
	return ret;
}

bool is_swap_ratio_group(int prio)
{
	return ((prio >= SWAP_RATIO_GROUP_START) &&
		(prio <= SWAP_RATIO_GROUP_END)) ? true : false;
}

void setup_swap_ratio(struct swap_info_struct *p, int prio)
{
	/* Used only if sysctl_swap_ratio_enable is set */
	if (is_swap_ratio_group(prio)) {
		if (p->flags & SWP_FAST)
			p->write_pending = SWAP_FAST_WRITES;
		else
			p->write_pending = SWAP_SLOW_WRITES;
		p->max_writes =  p->write_pending;
	}
}

int swap_ratio(struct swap_info_struct **si)
{
	if (!sysctl_swap_ratio_enable)
		return -ENODEV;

	if (is_swap_ratio_group((*si)->prio))
		return swap_ratio_slow(si);
	else
		return -ENODEV;
}
