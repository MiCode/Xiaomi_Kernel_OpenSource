/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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

#include "sbuf.h"
#include "sipc_priv.h"
#include "sprd_actions_queue.h"

/* actions */
enum {
	SBUF_READ_DATA_ACTION = 0,
	SBUF_CAN_WRITE_ACTION,
};

struct sbuf_bridge;

/* sbuf callback  struct*/
struct sbuf_cb {
	struct sbuf_bridge *sb;
};

/* sbuf bridge action struct */
struct sb_action {
	struct sbuf_mgr		*sbuf_rx;
	struct sbuf_mgr		*sbuf_tx;
	struct sbuf_bridge	*sb;

	void	*actions;
	char	*buf;
	u32	buf_size;
	char	name[20];
};

/* sbuf_bridge struct */
struct sbuf_bridge {
	u8	dst_a;
	u8	dst_b;
	u8	ch_a;
	u8	ch_b;

	u32	ch_mark;
	u32	priority;
	bool	ready;

	struct sbuf_mgr		*sbuf_a;
	struct sbuf_mgr		*sbuf_b;
	struct sbuf_cb		sbuf_a_cb;
	struct sbuf_cb		sbuf_b_cb;
	struct sb_action	a_to_b_action;
	struct sb_action	b_to_a_action;

	struct device *dev;
	struct work_struct register_work;
};

static void sb_sbuf_event_callback(int event, u32 bufid, void *data)
{
	struct sbuf_cb *scb = (struct sbuf_cb *)data;
	struct sbuf_bridge *sb  = scb->sb;
	struct sbuf_mgr *sbuf;

	switch (event) {
	case SBUF_NOTIFY_READY:
		sbuf = (scb == &sb->sbuf_a_cb) ? sb->sbuf_a : sb->sbuf_b;
		dev_info(sb->dev, "sbuf-%d-%d: SBUF_NOTIFY_READY",
			 sbuf->dst, sbuf->channel);

		if (sb->sbuf_a->state == SBUF_STATE_READY &&
			sb->sbuf_b->state == SBUF_STATE_READY) {
			sb->ready = true;
			dev_info(sb->dev, "sb is ready.");
			/* a last ready notify b_to_a, b last notify a_to_b. */
			if (scb == &sb->sbuf_a_cb)
				sprd_add_action_ex(sb->b_to_a_action.actions,
						   SBUF_READ_DATA_ACTION,
						   (int)bufid);
			else
				sprd_add_action_ex(sb->a_to_b_action.actions,
						   SBUF_READ_DATA_ACTION,
						   (int)bufid);
		}
		break;

	case SBUF_NOTIFY_READ:
		if (!sb->ready)
			break;

		/* a  can read notify a_to_b, b can write notify b_to_a. */
		if (scb == &sb->sbuf_a_cb)
			sprd_add_action_ex(sb->a_to_b_action.actions,
					   SBUF_READ_DATA_ACTION, (int)bufid);
		else
			sprd_add_action_ex(sb->b_to_a_action.actions,
					   SBUF_READ_DATA_ACTION, (int)bufid);
		break;

	case SBUF_NOTIFY_WRITE:
		if (!sb->ready)
			break;

		/* a  can write notify b_to_a, b can write notify a_to_b. */
		if (scb == &sb->sbuf_a_cb)
			sprd_add_action_ex(sb->b_to_a_action.actions,
					   SBUF_CAN_WRITE_ACTION, (int)bufid);
		else
			sprd_add_action_ex(sb->a_to_b_action.actions,
					   SBUF_CAN_WRITE_ACTION, (int)bufid);
		break;

	default:
		break;
	}
}

static void sb_register_work_fn(struct work_struct *work)
{
	struct sbuf_bridge *sb = container_of(work, struct sbuf_bridge,
						     register_work);
	if (!sb->sbuf_a)
		sb->sbuf_a = sbuf_register_notifier_ex(sb->dst_a, sb->ch_a,
						       sb->ch_mark,
						       sb_sbuf_event_callback,
						       &sb->sbuf_a_cb);

	if (!sb->sbuf_b)
		sb->sbuf_b = sbuf_register_notifier_ex(sb->dst_b, sb->ch_b,
						       sb->ch_mark,
						       sb_sbuf_event_callback,
						       &sb->sbuf_b_cb);

	/* until all sbuf register ok */
	if (!sb->sbuf_a || !sb->sbuf_b) {
		msleep(20);
		schedule_work(&sb->register_work);
	} else {
		dev_info(sb->dev, "2 buf register ok\n");
		sb->a_to_b_action.sbuf_rx = sb->sbuf_a;
		sb->a_to_b_action.sbuf_tx = sb->sbuf_b;

		sb->b_to_a_action.sbuf_rx = sb->sbuf_b;
		sb->b_to_a_action.sbuf_tx = sb->sbuf_a;
	}
}

/* return the max number of can write date. */
static int sb_sring_can_write(struct sbuf_ring *r_rx,
			      struct sbuf_ring *r_tx,
			      u8 d_rx, u8 d_tx)
{
	struct sbuf_ring_header_op *op_rx, *op_tx;
	u32 rd, wt;
	int ret;

	/* calcurate read data size. */
	op_rx = &r_rx->header_op;
	mutex_lock(&r_rx->rxlock);
	ret = sipc_smem_request_resource(r_rx->rx_pms, d_rx, -1);
	if (ret < 0) {
		mutex_unlock(&r_rx->rxlock);
		return ret;
	}

	rd = (*(op_rx->rx_wt_p) - *(op_rx->rx_rd_p));
	sipc_smem_release_resource(r_rx->rx_pms, d_rx);
	mutex_unlock(&r_rx->rxlock);

	if (rd == 0)
		return 0;

	/* calcurate can write data size. */
	op_tx = &r_tx->header_op;
	mutex_lock(&r_tx->txlock);
	ret = sipc_smem_request_resource(r_tx->tx_pms, d_tx, -1);
	if (ret < 0) {
		mutex_unlock(&r_tx->txlock);
		return ret;
	}
	wt = (op_tx->tx_size - (*(op_tx->tx_wt_p) - *(op_tx->tx_rd_p)));
	sipc_smem_release_resource(r_tx->tx_pms, d_tx);
	mutex_unlock(&r_tx->txlock);

	/* return the min size. */
	return min(rd, wt);
}

/* read from sbuf_rx and write to sbuf_tx. */
static void sb_data_move(struct sb_action *sba, u32 bufid)
{
	u32 has_data;
	int rd, wt, size;
	struct sbuf_ring *r_rx, *r_tx;
	struct sbuf_mgr *sbuf_rx = sba->sbuf_rx;
	struct sbuf_mgr *sbuf_tx = sba->sbuf_tx;
	struct sbuf_bridge *sb = sba->sb;

	if (!sb->ready) {
		dev_err(sb->dev, "%s is not ready.", sba->name);
		return;
	}

	r_rx = sbuf_rx->rings + bufid;
	r_tx = sbuf_tx->rings + bufid;

	/* only alloc once, will free it in remove function. */
	if (!sba->buf) {
		sba->buf_size = r_rx->header_op.rx_size;
		sba->buf = devm_kzalloc(sb->dev, sba->buf_size, GFP_KERNEL);
		if (!sba->buf)
			return;
		dev_info(sb->dev, "%s rx_size is 0x%x",
			 sba->name, sba->buf_size);
	}

	do {
		/* init has data to 0. */
		has_data = 0;
		rd = sb_sring_can_write(r_rx, r_tx, sbuf_rx->dst, sbuf_tx->dst);
		dev_dbg(sb->dev, "%s: ring[%d] can write =0x%x",
			sba->name, bufid, rd);
		if (rd <= 0)
			continue;

		size = (int)sba->buf_size;
		rd = min(rd, size);

		rd = sbuf_read(sbuf_rx->dst, sbuf_rx->channel,
			       bufid, sba->buf, rd, 0);
		if (rd <= 0)
			break;

		has_data = 1;
		wt = sbuf_write(sbuf_tx->dst, sbuf_tx->channel,
				bufid, sba->buf, rd, 0);

		if (rd != wt)
			dev_warn(sb->dev, "%s, bufid=%d, rd=%d, wr=%d\n",
				 sba->name, bufid, rd, wt);
	} while (has_data);
}

static void sb_bridge_do_actions(u32 action, int param, void *data)
{
	struct sb_action *sba = (struct sb_action *)data;

	switch (action) {
	case SBUF_READ_DATA_ACTION:
		dev_dbg(sba->sb->dev,
			"%s, SBUF_READ_DATA_ACTION, bufid=%d",
			sba->name, param);
		sb_data_move(sba, (u32)param);
		break;

	case SBUF_CAN_WRITE_ACTION:
		dev_dbg(sba->sb->dev,
			"%s, SBUF_READ_DATA_ACTION bufid=%d",
			sba->name, param);
		sb_data_move(sba, (u32)param);
		break;
	default:
		break;
	}
}

static int sbuf_bridge_probe(struct platform_device *pdev)
{
	int ret;
	struct sbuf_bridge *sb;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	u32 data[2];

	/* allcoc struct sbuf_bridge. */
	sb = devm_kzalloc(dev, sizeof(*sb), GFP_KERNEL);
	if (!sb)
		return -ENOMEM;

	sb->dev = dev;

	/* parse dst_a, ch_a. */
	ret = of_property_read_u32_array(np, "sprd,channel_a", data, 2);
	if (ret) {
		dev_err(dev, "channel_a err =%d", ret);
		return ret;
	}
	sb->dst_a = (u8)data[0];
	sb->ch_a  = (u8)data[1];

	/* parse dst_b, ch_b. */
	ret = of_property_read_u32_array(np, "sprd,channel_b", data, 2);
	if (ret) {
		dev_err(dev, "channel_b err =%d", ret);
		return ret;
	}

	sb->dst_b = (u8)data[0];
	sb->ch_b  = (u8)data[1];

	/* parse sub channel mask.*/
	ret = of_property_read_u32(np, "sprd,mask", &data[0]);
	if (ret) {
		dev_err(dev, "mask err =%d", ret);
		return ret;
	}
	/* mask to mark. */
	sb->ch_mark = ~data[0];

	/* parse priority, default 10. */
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

	dev_err(dev, "probed, mark=0x%x, priority=%d\n",
		sb->ch_mark, sb->priority);

	/* init scb and action. */
	sb->sbuf_a_cb.sb = sb;
	sb->sbuf_b_cb.sb = sb;
	sb->a_to_b_action.sb = sb;
	sb->b_to_a_action.sb = sb;

	INIT_WORK(&sb->register_work, sb_register_work_fn);
	schedule_work(&sb->register_work);

	platform_set_drvdata(pdev, sb);

	return 0;
}

static int  sbuf_bridge_remove(struct platform_device *pdev)
{
	struct sbuf_bridge *sb = platform_get_drvdata(pdev);

	if (sb) {
		cancel_work_sync(&sb->register_work);

		if (sb->a_to_b_action.actions) {
			devm_kfree(&pdev->dev, sb->a_to_b_action.buf);
			sprd_destroy_action_queue(sb->a_to_b_action.actions);
		}

		if (sb->b_to_a_action.actions) {
			devm_kfree(&pdev->dev, sb->b_to_a_action.buf);
			sprd_destroy_action_queue(sb->b_to_a_action.actions);
		}

		devm_kfree(&pdev->dev, sb);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static const struct of_device_id sbuf_bridge_match_table[] = {
	{.compatible = "sprd,sbuf_bridge", },
	{ },
};

static struct platform_driver sbuf_bridge_driver = {
	.driver = {
		.name = "sbuf_bridge",
		.of_match_table = sbuf_bridge_match_table,
	},
	.probe = sbuf_bridge_probe,
	.remove = sbuf_bridge_remove,
};

static int __init sbuf_bridge_init(void)
{
	return platform_driver_register(&sbuf_bridge_driver);
}

static void __exit sbuf_bridge_exit(void)
{
	platform_driver_unregister(&sbuf_bridge_driver);
}

module_init(sbuf_bridge_init);
module_exit(sbuf_bridge_exit);

MODULE_AUTHOR("Zhou Wenping");
MODULE_DESCRIPTION("SIPC/sbuf_bridge driver");
MODULE_LICENSE("GPL");
