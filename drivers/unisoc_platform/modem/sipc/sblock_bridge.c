/*
 * Copyright (C) 2021 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "sblock.h"
#include "sipc_priv.h"
#include "sprd_actions_queue.h"

/* actions */
enum {
	SBLOCK_READ_DATA_ACTION = 0,
	SBLOCK_CAN_WRITE_ACTION
};

struct sblock_bridge;

/* sblock callback  struct*/
struct sblock_cb {
	struct sblock_bridge *sb;
};

/* sblock bridge action struct */
struct sblock_action {
	struct sblock_mgr	*sblock_rx;
	struct sblock_mgr	*sblock_tx;
	struct sblock_bridge	*sb;
	bool	rx_bigger;/* is rx bigger */
	u32	tx_blks_cnt;/* rx bigger, 1 rx blcks need tx blcks cnt. */
	u32	rx_blks_cnt;/* tx bigger, 1 tx blcks can read rx blcks cnt. */

	void	*actions;
	char	name[20];
};

/* sblock_bridge struct */
struct sblock_bridge {
	u8	dst_a;
	u8	dst_b;
	u8	ch_a;
	u8	ch_b;

	u32	priority;
	bool	ready;

	struct sblock_mgr	*sblock_a;
	struct sblock_mgr	*sblock_b;
	struct sblock_cb	sblock_a_cb;
	struct sblock_cb	sblock_b_cb;
	struct sblock_action	a_to_b_action;
	struct sblock_action	b_to_a_action;

	struct device *dev;
	struct work_struct register_work;
};

static void sb_sblock_event_callback(int event, void *data)
{
	struct sblock_cb *scb = (struct sblock_cb *)data;
	struct sblock_bridge *sb  = scb->sb;
	struct sblock_mgr *sblock;

	switch (event) {
	case SBLOCK_NOTIFY_OPEN:
		sblock = (scb == &sb->sblock_a_cb) ?
			sb->sblock_a : sb->sblock_b;
		dev_info(sb->dev, "sblock-%d-%d: SBLOCK_NOTIFY_READY",
			 sblock->dst, sblock->channel);

		if (sb->sblock_a->state == SBLOCK_STATE_READY &&
		    sb->sblock_b->state == SBLOCK_STATE_READY) {
			sb->ready = true;
			dev_info(sb->dev, "sblock bridge is ready.");

			/* a last ready notify b_to_a, b last notify a_to_b. */
			if (scb == &sb->sblock_a_cb)
				sprd_add_action_ex(sb->b_to_a_action.actions,
						   SBLOCK_READ_DATA_ACTION,
						   0);
			else
				sprd_add_action_ex(sb->a_to_b_action.actions,
						   SBLOCK_READ_DATA_ACTION,
						   0);
		}
		break;

	case SBLOCK_NOTIFY_RECV:
		if (!sb->ready)
			break;

		/* a  can read notify a_to_b, b can write notify b_to_a. */
		if (scb == &sb->sblock_a_cb)
			sprd_add_action_ex(sb->a_to_b_action.actions,
					   SBLOCK_READ_DATA_ACTION, 0);
		else
			sprd_add_action_ex(sb->b_to_a_action.actions,
					   SBLOCK_READ_DATA_ACTION, 0);
		break;

	case SBLOCK_NOTIFY_GET:
		if (!sb->ready)
			break;

		/* a  can write notify b_to_a, b can write notify a_to_b. */
		if (scb == &sb->sblock_a_cb)
			sprd_add_action_ex(sb->b_to_a_action.actions,
					   SBLOCK_CAN_WRITE_ACTION, 0);
		else
			sprd_add_action_ex(sb->a_to_b_action.actions,
					   SBLOCK_CAN_WRITE_ACTION, 0);
		break;

	default:
		break;
	}
}

static void sb_sblock_action_calculate(struct sblock_action *action)
{
	u32 rxblksz = action->sblock_rx->rxblksz;
	u32 txblksz = action->sblock_tx->txblksz;

	action->rx_bigger = rxblksz > txblksz;
	if (action->rx_bigger)
		action->tx_blks_cnt = rxblksz / txblksz +
			((rxblksz % txblksz) ? 1 : 0);
	else
		action->rx_blks_cnt = txblksz / rxblksz;

	pr_debug("%s: rx_bigger=%d. tx_blks_cnt=%d, rx_blks_cnt=%d",
		 action->name, action->rx_bigger,
		 action->rx_blks_cnt, action->tx_blks_cnt);
}

static void sb_sblock_register_work_fn(struct work_struct *work)
{
	struct sblock_bridge *sb = container_of(work, struct sblock_bridge,
						register_work);

	if (!sb->sblock_a)
		sb->sblock_a =
			sblock_register_notifier_ex(sb->dst_a, sb->ch_a,
						    sb_sblock_event_callback,
						    &sb->sblock_a_cb);

	if (!sb->sblock_b)
		sb->sblock_b =
			sblock_register_notifier_ex(sb->dst_b, sb->ch_b,
						    sb_sblock_event_callback,
						    &sb->sblock_b_cb);

	/* until all sblock register ok */
	if (!sb->sblock_a || !sb->sblock_b) {
		msleep(200);
		schedule_work(&sb->register_work);
	} else {
		dev_info(sb->dev, "2 sblock register ok\n");
		sb->a_to_b_action.sblock_rx = sb->sblock_a;
		sb->a_to_b_action.sblock_tx = sb->sblock_b;
		sb_sblock_action_calculate(&sb->a_to_b_action);

		sb->b_to_a_action.sblock_rx = sb->sblock_b;
		sb->b_to_a_action.sblock_tx = sb->sblock_a;
		sb_sblock_action_calculate(&sb->a_to_b_action);
	}
}

/* read from sblock_rx and write to sblock_tx. */
static void sb_block_data_move(struct sblock_action *sba)
{
	u32 size, left, cpy_szie, cnt;
	struct sblock_mgr *sblock_rx = sba->sblock_rx;
	struct sblock_mgr *sblock_tx = sba->sblock_tx;
	struct sblock_bridge *sb = sba->sb;
	struct sblock blk_rx;
	struct sblock blk_tx;
	int ret;

	if (!sb->ready) {
		dev_err(sb->dev, "%s is not ready.\n", sba->name);
		return;
	}

	/* all rx read data send to tx. */
	while (sblock_receive(sblock_rx->dst,
			     sblock_rx->channel, &blk_rx, 0) == 0) {
		if (sba->rx_bigger) {
			/* one rx may need some tx blks */
			size = 0;
			left = blk_rx.length;
			do {
				ret = sblock_get(sblock_tx->dst,
						 sblock_tx->channel,
						 &blk_tx, -1);
				if (ret) {
					dev_err(sb->dev,
						"sblock-%d-%d get failed, ret = %d!\n",
						sblock_tx->dst,
						sblock_tx->channel, ret);
					break;
				}
				cpy_szie = min(left, blk_tx.length);
				memcpy(blk_tx.addr, blk_rx.addr + size,
				       cpy_szie);
				ret = sblock_send(sblock_tx->dst,
						  sblock_tx->channel, &blk_tx);
				if (ret) {
					dev_err(sb->dev,
						"sblock-%d-%d send failed, ret = %d!\n",
						sblock_tx->dst,
						sblock_tx->channel, ret);
				}

				size += cpy_szie;
				left -= cpy_szie;
			} while (left > 0);
		} else {
			/* one tx may can send some rx blks */
			ret = sblock_get(sblock_tx->dst, sblock_tx->channel,
					 &blk_tx, -1);
			if (ret) {
				dev_err(sb->dev,
					"sblock-%d-%d get failed, ret = %d!\n",
					sblock_tx->dst,
					sblock_tx->channel, ret);
				break;
			}
			size = 0;
			cnt = 0;
			do {
				memcpy(blk_tx.addr + size, blk_rx.addr,
				       blk_rx.length);
				cnt++;
				size += blk_rx.length;
				if (cnt == sba->rx_blks_cnt)
					break;
			} while (sblock_receive(sblock_rx->dst,
						sblock_rx->channel,
						&blk_rx, 0) == 0);

			blk_tx.length = size;
			ret = sblock_send(sblock_tx->dst,
					  sblock_tx->channel, &blk_tx);
			if (ret) {
				dev_err(sb->dev,
					"sblock-%d-%d send failed, ret = %d!\n",
					sblock_tx->dst,
					sblock_tx->channel, ret);
			}
		}
	}
}

static void sb_bridge_do_actions(u32 action, int param, void *data)
{
	struct sblock_action *sba = (struct sblock_action *)data;

	switch (action) {
	case SBLOCK_READ_DATA_ACTION:
		sb_block_data_move(sba);
		break;

	case SBLOCK_CAN_WRITE_ACTION:
		sb_block_data_move(sba);
		break;
	default:
		break;
	}
}

static int sblock_bridge_probe(struct platform_device *pdev)
{
	int ret;
	struct sblock_bridge *sb;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	u32 data[2];

	/* allcoc struct sblock_bridge. */
	sb = devm_kzalloc(dev, sizeof(*sb), GFP_KERNEL);
	if (!sb)
		return -ENOMEM;

	sb->dev = dev;

	/* parse dst_a, ch_a. */
	ret = of_property_read_u32_array(np, "sprd,channel_a", data, 2);
	if (ret) {
		dev_err(dev, "channel_a err =%d\n", ret);
		return ret;
	}
	sb->dst_a = (u8)data[0];
	sb->ch_a  = (u8)data[1];

	/* parse dst_b, ch_b. */
	ret = of_property_read_u32_array(np, "sprd,channel_b", data, 2);
	if (ret) {
		dev_err(dev, "channel_b err =%d\n", ret);
		return ret;
	}

	sb->dst_b = (u8)data[0];
	sb->ch_b  = (u8)data[1];

	/* parse priority, default 90. */
	ret = of_property_read_u32(np, "sprd,priority", &sb->priority);
	if (ret)
		sb->priority = 90;

	/* create  a_to_b_action. */
	sprintf(sb->a_to_b_action.name, "sb-%d-%d--%d-%d",
		sb->dst_a, sb->ch_a, sb->dst_b, sb->ch_b);
	sb->a_to_b_action.actions =
		sprd_create_action_queue(sb->a_to_b_action.name,
					 sb_bridge_do_actions,
					 &sb->a_to_b_action, sb->priority);

	/* create  b_to_a_action. */
	sprintf(sb->b_to_a_action.name, "sb-%d-%d--%d-%d",
		sb->dst_b, sb->ch_b, sb->dst_a, sb->ch_a);
	sb->b_to_a_action.actions =
		sprd_create_action_queue(sb->b_to_a_action.name,
					 sb_bridge_do_actions,
					 &sb->b_to_a_action, sb->priority);

	dev_info(dev, "probed, priority=%d\n", sb->priority);
	dev_info(dev, "%s, %s\n",
		 sb->a_to_b_action.name,
		 sb->b_to_a_action.name);

	/* init scb and action. */
	sb->sblock_a_cb.sb = sb;
	sb->sblock_b_cb.sb = sb;
	sb->a_to_b_action.sb = sb;
	sb->b_to_a_action.sb = sb;

	INIT_WORK(&sb->register_work, sb_sblock_register_work_fn);
	schedule_work(&sb->register_work);

	platform_set_drvdata(pdev, sb);

	return 0;
}

static int  sblock_bridge_remove(struct platform_device *pdev)
{
	struct sblock_bridge *sb = platform_get_drvdata(pdev);

	if (sb) {
		cancel_work_sync(&sb->register_work);

		if (sb->a_to_b_action.actions)
			sprd_destroy_action_queue(sb->a_to_b_action.actions);

		if (sb->b_to_a_action.actions)
			sprd_destroy_action_queue(sb->b_to_a_action.actions);

		devm_kfree(&pdev->dev, sb);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static const struct of_device_id sblock_bridge_match_table[] = {
	{.compatible = "sprd,sblock_bridge", },
	{ },
};

static struct platform_driver sblock_bridge_driver = {
	.driver = {
		.name = "sblock_bridge",
		.of_match_table = sblock_bridge_match_table,
	},
	.probe = sblock_bridge_probe,
	.remove = sblock_bridge_remove,
};

static int __init sblock_bridge_init(void)
{
	return platform_driver_register(&sblock_bridge_driver);
}

static void __exit sblock_bridge_exit(void)
{
	platform_driver_unregister(&sblock_bridge_driver);
}

module_init(sblock_bridge_init);
module_exit(sblock_bridge_exit);

MODULE_AUTHOR("Zhou Wenping");
MODULE_DESCRIPTION("SIPC/sblock_bridge driver");
MODULE_LICENSE("GPL");
