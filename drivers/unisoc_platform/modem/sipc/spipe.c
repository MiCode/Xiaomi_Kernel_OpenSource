// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-spipe: " fmt

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sipc.h>

#include "sipc_priv.h"
#include "spipe.h"

struct spipe_device {
	struct spipe_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
};

struct spipe_sbuf {
	u8			dst;
	u8			channel;
	u32		bufid;
};

static struct class		*spipe_class;

static int spipe_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	struct spipe_device *spipe;
	struct spipe_sbuf *sbuf;

	spipe = container_of(inode->i_cdev, struct spipe_device, cdev);
	if (sbuf_status(spipe->init->dst, spipe->init->channel) != 0) {
		pr_debug("spipe %d-%d not ready to open!\n",
			spipe->init->dst, spipe->init->channel);
		filp->private_data = NULL;
		return -ENODEV;
	}

	sbuf = kmalloc(sizeof(struct spipe_sbuf), GFP_KERNEL);
	if (!sbuf)
		return -ENOMEM;
	filp->private_data = sbuf;

	sbuf->dst = spipe->init->dst;
	sbuf->channel = spipe->init->channel;
	sbuf->bufid = minor - spipe->minor;

	return 0;
}

static int spipe_release(struct inode *inode, struct file *filp)
{
	struct spipe_sbuf *sbuf = filp->private_data;

	kfree(sbuf);

	return 0;
}

static ssize_t spipe_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct spipe_sbuf *sbuf = filp->private_data;
	int timeout = -1;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	return sbuf_read(sbuf->dst, sbuf->channel, sbuf->bufid,
			(void *)buf, count, timeout);
}

static ssize_t spipe_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct spipe_sbuf *sbuf = filp->private_data;
	int timeout = -1;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	return sbuf_write(sbuf->dst, sbuf->channel, sbuf->bufid,
			(void *)buf, count, timeout);
}

static unsigned int spipe_poll(struct file *filp, poll_table *wait)
{
	struct spipe_sbuf *sbuf = filp->private_data;

	return sbuf_poll_wait(sbuf->dst, sbuf->channel, sbuf->bufid,
			filp, wait);
}

static long spipe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations spipe_fops = {
	.open		= spipe_open,
	.release	= spipe_release,
	.read		= spipe_read,
	.write		= spipe_write,
	.poll		= spipe_poll,
	.unlocked_ioctl	= spipe_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int spipe_parse_dt(struct spipe_init_data **init,
	struct device_node *np, struct device *dev)
{
	struct device_node *parent_np ;
	struct spipe_init_data *pdata = NULL;
	int ret;
	u32 data;

	pdata = devm_kzalloc(dev, sizeof(struct spipe_init_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	/* Get the label id for the node of spipe */
	ret = of_property_read_string(np, "label", &pdata->name);
	if (ret)
		goto error;

	/* Get sipc dst id, spipe dst is share sipc dst on dtsi
	 * Get the parent of the spipe, it is the node of sipc, get reg
	 */
	parent_np = of_get_parent(np);
	if (parent_np) {
		ret = of_property_read_u32(parent_np, "reg", (u32 *)&data);
		if (ret)
			goto error;
		pdata->dst = (u8)data;
	}
	of_node_put(parent_np);

	/* Get the channel id for the node of spipe */
	ret = of_property_read_u32(np, "reg", (u32 *)&data);
	if (ret)
		goto error;
	pdata->channel = (u8)data;

	/* smem, optional, default value is 0. */
	ret = of_property_read_u32(np, "sprd,smem", &data);
	if (!ret)
		pdata->smem = (u16)data;

	/* Get the ringnr for the node of spipe */
	ret = of_property_read_u32(np,  "sprd,ringnr", (u32 *)&pdata->ringnr);
	if (ret)
		goto error;

	/* Get the rxbuf size for the node of spipe */
	ret = of_property_read_u32(np,
				"sprd,size-rxbuf", (u32 *)&pdata->rxbuf_size);
	if (ret)
		goto error;

	/* Get the txbuf size for the node of spipe */
	ret = of_property_read_u32(np, "sprd,size-txbuf",
				   (u32 *)&pdata->txbuf_size);
	if (ret)
		goto error;

	dev_dbg(dev, "label = %s\n", pdata->name);
	dev_dbg(dev, "dst = %d\n", pdata->dst);
	dev_dbg(dev, "channel = %d\n", pdata->channel);
	dev_dbg(dev, "smem = %d\n", pdata->smem);
	dev_dbg(dev, "ringnr = %d\n", pdata->ringnr);
	dev_dbg(dev, "size-rxbuf = %x\n", pdata->rxbuf_size);
	dev_dbg(dev, "size-txbuf = %x\n", pdata->txbuf_size);

	*init = pdata;


	return ret;
error:
	devm_kfree(dev, pdata);
	*init = NULL;
	return ret;
}

static inline void spipe_destroy_pdata(struct spipe_init_data **init,
	struct device *dev)
{
	struct spipe_init_data *pdata = *init;

	devm_kfree(dev, pdata);

	*init = NULL;
}

static int spipe_probe(struct platform_device *pdev)
{
	struct spipe_init_data *init = pdev->dev.platform_data;
	struct spipe_device *spipe;
	dev_t devid;
	int i, rval;
	struct device_node *np;
	struct device *dev = &pdev->dev;

	if (pdev->dev.of_node && !init) {
		np = pdev->dev.of_node;

		rval = spipe_parse_dt(&init, np, &pdev->dev);
		if (rval) {
			dev_err(dev, "Failed to parse spipe device tree, ret=%d\n",
				rval);
			return rval;
		}
		dev_info(dev, "After parse device tree, name=%s, dst=%u, channel=%u, ringnr=%u,  rxbuf_size=0x%x, txbuf_size=0x%x\n",
			init->name,
			init->dst,
			init->channel,
			init->ringnr,
			init->rxbuf_size,
			init->txbuf_size);

		rval = sbuf_create_ex(init->dst, init->channel,
				      init->smem, init->ringnr,
				      init->txbuf_size, init->rxbuf_size);

		if (rval != 0) {
			dev_err(dev, "Failed to create sbuf: %d\n", rval);
			spipe_destroy_pdata(&init, &pdev->dev);
			return rval;
		}

		spipe = devm_kzalloc(&pdev->dev,
				     sizeof(struct spipe_device),
				     GFP_KERNEL);
		if (spipe == NULL) {
			sbuf_destroy(init->dst, init->channel);
			spipe_destroy_pdata(&init, &pdev->dev);
			return -ENOMEM;
		}

		rval = alloc_chrdev_region(&devid, 0, init->ringnr, init->name);
		if (rval != 0) {
			sbuf_destroy(init->dst, init->channel);
			devm_kfree(&pdev->dev, spipe);
			spipe_destroy_pdata(&init, &pdev->dev);
			dev_err(dev, "Failed to alloc spipe chrdev\n");
			return rval;
		}

		cdev_init(&(spipe->cdev), &spipe_fops);
		rval = cdev_add(&(spipe->cdev), devid, init->ringnr);
		if (rval != 0) {
			sbuf_destroy(init->dst, init->channel);
			devm_kfree(&pdev->dev, spipe);
			unregister_chrdev_region(devid, init->ringnr);
			spipe_destroy_pdata(&init, &pdev->dev);
			dev_err(dev, "Failed to add spipe cdev\n");
			return rval;
		}

		spipe->major = MAJOR(devid);
		spipe->minor = MINOR(devid);
		if (init->ringnr > 1) {
			for (i = 0; i < init->ringnr; i++) {
				device_create(spipe_class, NULL,
					MKDEV(spipe->major, spipe->minor + i),
					NULL, "%s%d", init->name, i);
			}
		} else {
			device_create(spipe_class, NULL,
				MKDEV(spipe->major, spipe->minor),
				NULL, "%s", init->name);
		}

		spipe->init = init;

		platform_set_drvdata(pdev, spipe);
	}

	return 0;
}

static int  spipe_remove(struct platform_device *pdev)
{
	struct spipe_device *spipe = platform_get_drvdata(pdev);
	int i;

	if (spipe) {
		for (i = 0; i < spipe->init->ringnr; i++) {
			device_destroy(spipe_class,
					MKDEV(spipe->major, spipe->minor + i));
		}
		cdev_del(&(spipe->cdev));
		unregister_chrdev_region(
			MKDEV(spipe->major, spipe->minor), spipe->init->ringnr);

		sbuf_destroy(spipe->init->dst, spipe->init->channel);

		spipe_destroy_pdata(&spipe->init, &pdev->dev);

		devm_kfree(&pdev->dev, spipe);

		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static const struct of_device_id spipe_match_table[] = {
	{.compatible = "sprd,spipe", },
	{ },
};

static struct platform_driver spipe_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-spipe",
		.of_match_table = spipe_match_table,
	},
	.probe = spipe_probe,
	.remove = spipe_remove,
};

static int __init spipe_init(void)
{
	spipe_class = class_create(THIS_MODULE, "spipe");
	if (IS_ERR(spipe_class))
		return PTR_ERR(spipe_class);

	return platform_driver_register(&spipe_driver);
}

static void __exit spipe_exit(void)
{
	class_destroy(spipe_class);
	platform_driver_unregister(&spipe_driver);
}

module_init(spipe_init);
module_exit(spipe_exit);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SPIPE driver");
MODULE_LICENSE("GPL");
