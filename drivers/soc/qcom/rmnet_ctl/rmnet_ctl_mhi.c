// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * RMNET_CTL mhi handler
 *
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/skbuff.h>
#include <linux/mhi.h>
#include <soc/qcom/rmnet_ctl.h>
#include "rmnet_ctl_client.h"

#define RMNET_CTL_DEFAULT_MRU 256

struct rmnet_ctl_mhi_dev {
	struct mhi_device *mhi_dev;
	struct rmnet_ctl_dev dev;
	u32 mru;
	spinlock_t rx_lock; /* rx lock */
	spinlock_t tx_lock; /* tx lock */
	atomic_t in_reset;
};

static int rmnet_ctl_send_mhi(struct rmnet_ctl_dev *dev, struct sk_buff *skb)
{
	struct rmnet_ctl_mhi_dev *ctl_dev = container_of(
				dev, struct rmnet_ctl_mhi_dev, dev);
	int rc;

	spin_lock_bh(&ctl_dev->tx_lock);

	rc = mhi_queue_transfer(ctl_dev->mhi_dev,
				DMA_TO_DEVICE, skb, skb->len, MHI_EOT);
	if (rc)
		dev->stats.tx_err++;
	else
		dev->stats.tx_pkts++;

	spin_unlock_bh(&ctl_dev->tx_lock);

	return rc;
}

static void rmnet_ctl_alloc_buffers(struct rmnet_ctl_mhi_dev *ctl_dev,
				    gfp_t gfp, void *free_buf)
{
	struct mhi_device *mhi_dev = ctl_dev->mhi_dev;
	void *buf;
	int no_tre, i, rc;

	no_tre = mhi_get_no_free_descriptors(mhi_dev, DMA_FROM_DEVICE);

	if (!no_tre && free_buf) {
		kfree(free_buf);
		return;
	}

	for (i = 0; i < no_tre; i++) {
		if (free_buf) {
			buf = free_buf;
			free_buf = NULL;
		} else {
			buf = kmalloc(ctl_dev->mru, gfp);
		}

		if (!buf)
			return;

		spin_lock_bh(&ctl_dev->rx_lock);
		rc = mhi_queue_transfer(mhi_dev, DMA_FROM_DEVICE,
					buf, ctl_dev->mru, MHI_EOT);
		spin_unlock_bh(&ctl_dev->rx_lock);

		if (rc) {
			kfree(buf);
			return;
		}
	}
}

static void rmnet_ctl_dl_callback(struct mhi_device *mhi_dev,
				  struct mhi_result *mhi_res)
{
	struct rmnet_ctl_mhi_dev *ctl_dev = dev_get_drvdata(&mhi_dev->dev);

	if (mhi_res->transaction_status == -ENOTCONN) {
		kfree(mhi_res->buf_addr);
		return;
	} else if (mhi_res->transaction_status ||
		   !mhi_res->buf_addr || !mhi_res->bytes_xferd) {
		rmnet_ctl_log_err("RXE", mhi_res->transaction_status, NULL, 0);
		ctl_dev->dev.stats.rx_err++;
	} else {
		ctl_dev->dev.stats.rx_pkts++;
		rmnet_ctl_endpoint_post(mhi_res->buf_addr,
					mhi_res->bytes_xferd);
	}

	/* Re-supply receive buffers */
	rmnet_ctl_alloc_buffers(ctl_dev, GFP_ATOMIC, mhi_res->buf_addr);
}

static void rmnet_ctl_ul_callback(struct mhi_device *mhi_dev,
				  struct mhi_result *mhi_res)
{
	struct rmnet_ctl_mhi_dev *ctl_dev = dev_get_drvdata(&mhi_dev->dev);
	struct sk_buff *skb = (struct sk_buff *)mhi_res->buf_addr;

	if (skb) {
		if (mhi_res->transaction_status) {
			rmnet_ctl_log_err("TXE", mhi_res->transaction_status,
					  skb->data, skb->len);
			ctl_dev->dev.stats.tx_err++;
		} else {
			rmnet_ctl_log_debug("TXC", skb->data, skb->len);
			ctl_dev->dev.stats.tx_complete++;
		}
		kfree_skb(skb);
	}
}

static void rmnet_ctl_status_callback(struct mhi_device *mhi_dev,
				      enum MHI_CB mhi_cb)
{
	struct rmnet_ctl_mhi_dev *ctl_dev = dev_get_drvdata(&mhi_dev->dev);

	if (mhi_cb != MHI_CB_FATAL_ERROR)
		return;

	atomic_inc(&ctl_dev->in_reset);
}

static int rmnet_ctl_probe(struct mhi_device *mhi_dev,
			   const struct mhi_device_id *id)
{
	struct rmnet_ctl_mhi_dev *ctl_dev;
	struct device_node *of_node = mhi_dev->dev.of_node;
	int rc;

	ctl_dev = devm_kzalloc(&mhi_dev->dev, sizeof(*ctl_dev), GFP_KERNEL);
	if (!ctl_dev)
		return -ENOMEM;

	ctl_dev->mhi_dev = mhi_dev;
	ctl_dev->dev.xmit = rmnet_ctl_send_mhi;

	spin_lock_init(&ctl_dev->rx_lock);
	spin_lock_init(&ctl_dev->tx_lock);
	atomic_set(&ctl_dev->in_reset, 0);
	dev_set_drvdata(&mhi_dev->dev, ctl_dev);

	rc = of_property_read_u32(of_node, "mhi,mru", &ctl_dev->mru);
	if (rc || !ctl_dev->mru)
		ctl_dev->mru = RMNET_CTL_DEFAULT_MRU;

	rc = mhi_prepare_for_transfer(mhi_dev);
	if (rc) {
		pr_err("%s(): Failed to prep for transfer %d\n", __func__, rc);
		return -EINVAL;
	}

	/* Post receive buffers */
	rmnet_ctl_alloc_buffers(ctl_dev, GFP_KERNEL, NULL);

	rmnet_ctl_endpoint_setdev(&ctl_dev->dev);

	pr_info("rmnet_ctl driver probed\n");

	return 0;
}

static void rmnet_ctl_remove(struct mhi_device *mhi_dev)
{
	rmnet_ctl_endpoint_setdev(NULL);
	synchronize_rcu();
	dev_set_drvdata(&mhi_dev->dev, NULL);

	pr_info("rmnet_ctl driver removed\n");
}

static const struct mhi_device_id rmnet_ctl_mhi_match[] = {
	{ .chan = "RMNET_CTL" },
	{}
};

static struct mhi_driver rmnet_ctl_driver = {
	.probe = rmnet_ctl_probe,
	.remove = rmnet_ctl_remove,
	.dl_xfer_cb = rmnet_ctl_dl_callback,
	.ul_xfer_cb = rmnet_ctl_ul_callback,
	.status_cb = rmnet_ctl_status_callback,
	.id_table = rmnet_ctl_mhi_match,
	.driver = {
		.name = "rmnet_ctl",
		.owner = THIS_MODULE,
	},
};

module_driver(rmnet_ctl_driver,
	      mhi_driver_register, mhi_driver_unregister);

MODULE_DESCRIPTION("RmNet Control Driver");
MODULE_LICENSE("GPL v2");
