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
#define pr_fmt(fmt) "sprd-spool: " fmt

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sipc.h>

#include "spool.h"

struct spool_device;

struct spool_device {
	struct spool_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
	struct platform_device	*plt_dev;

	struct device		*sys_dev;	/* Device object in sysfs */
};

struct spool_sblock {
	u8			dst;
	u8			channel;
	/* support for log loop */
	u8			sblock_loop;
	bool			is_hold;
	struct sblock	hold;
};

static struct class	*spool_class;

static int spool_open(struct inode *inode, struct file *filp)
{
	struct spool_device *spool;
	struct spool_sblock *sblock;
	int ret;

	spool = container_of(inode->i_cdev, struct spool_device, cdev);
	ret = sblock_query(spool->init->dst, spool->init->channel);
	if (ret)
		return ret;
	sblock = kmalloc(sizeof(struct spool_sblock), GFP_KERNEL);
	if (!sblock)
		return -ENOMEM;
	filp->private_data = sblock;

	sblock->dst = spool->init->dst;
	sblock->channel = spool->init->channel;
	sblock->sblock_loop = spool->init->sblock_loop;
	sblock->is_hold = 0;

	return 0;
}

static int spool_release(struct inode *inode, struct file *filp)
{
	struct spool_sblock *sblock = filp->private_data;

	if (sblock->is_hold) {
		if (sblock->sblock_loop ? sblock_release_loop(sblock->dst,
		    sblock->channel, &sblock->hold) : sblock_release(sblock->dst,
		    sblock->channel, &sblock->hold))
			pr_debug("failed to release block!\n");
	}
	kfree(sblock);

	return 0;
}

static ssize_t spool_read(struct file *filp,
			  char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int rdsize = 0;
	struct sblock blk = {0};

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	if (sblock->is_hold) {
		if (count < sblock->hold.length - *ppos) {
			rdsize = count;
		} else {
			rdsize = sblock->hold.length - *ppos;
			sblock->is_hold = 0;
		}
		blk = sblock->hold;
	} else {
		*ppos = 0;
		ret = sblock->sblock_loop ? sblock_receive_loop(sblock->dst,
			sblock->channel, &blk) : sblock_receive(sblock->dst,
			sblock->channel, &blk, timeout);
		if (ret < 0) {
			pr_debug("failed to receive block!\n");
			return ret;
		}
		if (blk.length <= count)
			rdsize = blk.length;
		else {
			rdsize = count;
			sblock->is_hold = 1;
			sblock->hold = blk;
		}
	}

	if (unalign_copy_to_user(buf, blk.addr + *ppos, rdsize)) {
		pr_err("failed to copy to user!\n");
		sblock->is_hold = 0;
		*ppos = 0;
		ret = -EFAULT;
	} else {
		ret = rdsize;
		*ppos += rdsize;
	}

	if (sblock->is_hold == 0) {
		if (sblock->sblock_loop ? sblock_release_loop(sblock->dst,
		    sblock->channel, &blk) : sblock_release(sblock->dst,
		    sblock->channel, &blk))
			pr_err("failed to release block!\n");
	}

	return ret;
}

static ssize_t spool_write(struct file *filp,
			   const char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int wrsize = 0;
	int pos = 0;
	struct sblock blk = {0};
	size_t len = count;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	do {
		ret = sblock_get(sblock->dst, sblock->channel, &blk, timeout);
		if (ret < 0) {
			pr_info("failed to get block!\n");
			return ret;
		}

		wrsize = (blk.length > len ? len : blk.length);
		if (unalign_copy_from_user(blk.addr, buf + pos, wrsize)) {
			pr_info("failed to copy from user!\n");
			ret = -EFAULT;
		} else {
			blk.length = wrsize;
			len -= wrsize;
			pos += wrsize;
		}

		if (sblock_send(sblock->dst, sblock->channel, &blk))
			pr_debug("failed to send block!");
	} while (len > 0 && ret == 0);

	return count - len;
}

static unsigned int spool_poll(struct file *filp, poll_table *wait)
{
	struct spool_sblock *sblock = filp->private_data;

	return sblock_poll_wait(sblock->dst, sblock->channel, filp, wait);
}

static long spool_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations spool_fops = {
	.open		= spool_open,
	.release	= spool_release,
	.read		= spool_read,
	.write		= spool_write,
	.poll		= spool_poll,
	.unlocked_ioctl	= spool_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int spool_parse_dt(struct spool_init_data **init, struct device *dev,
			  struct device_node *np)
{
	struct device_node *parent_np;
	struct spool_init_data *pdata = NULL;
	int ret;
	u32 data;

	pdata = devm_kzalloc(dev, sizeof(struct spool_init_data), GFP_KERNEL);
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

	ret = of_property_read_u32(np, "sprd,preconfigured", (u32 *)&data);
	if (!ret)
		pdata->pre_cfg = (int)data;

	ret = of_property_read_u32(np, "sprd,smem", (u32 *)&data);
	if (!ret)
		pdata->smem = (u8)data;
	else
		pdata->smem = 0;

	ret = of_property_read_u32(np, "sprd,sblock_loop", (u32 *)&data);
	if (!ret)
		pdata->sblock_loop = (u8)data;
	else
		pdata->sblock_loop = 0;

	ret = of_property_read_u32(np, "sprd,tx-blksize",
				   (u32 *)&pdata->txblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,tx-blknum",
				   (u32 *)&pdata->txblocknum);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blksize",
				   (u32 *)&pdata->rxblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blknum",
				   (u32 *)&pdata->rxblocknum);
	if (ret)
		goto error;

	if (!of_property_read_u32(np, "sprd,nodev", (u32 *)&data))
		pdata->nodev = (u8)data;

	*init = pdata;
	dev_dbg(dev, "label  =%s\n", pdata->name);
	dev_dbg(dev, "dst    =%d\n", pdata->dst);
	dev_dbg(dev, "channel=%d\n", pdata->channel);

	return ret;
error:
	devm_kfree(dev, pdata);
	*init = NULL;
	return ret;
}

static ssize_t base_addr_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct spool_device *spool = (struct spool_device *)
		dev_get_drvdata(dev);
	struct spool_init_data *init = spool->init;
	uint32_t addr;
	int ret;

	ret = sblock_get_smem_cp_addr(init->dst, init->channel,
				      &addr);
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%u %u 0x%08X %d %u %u %u %u\n",
			(unsigned int)init->dst,
			(unsigned int)init->channel,
			addr,
			init->pre_cfg,
			(unsigned int)init->txblocknum,
			(unsigned int)init->txblocksize,
			(unsigned int)init->rxblocknum,
			(unsigned int)init->rxblocksize);
}

static DEVICE_ATTR(base_addr, 0440,
		    base_addr_show, NULL);

static int create_spool(struct platform_device *pdev,
			struct spool_init_data *init,
			struct spool_device **out)
{
	int rval;
	struct spool_device *spool;
	dev_t dev_no;
	char sp_name[16];
	struct device *dev = &pdev->dev;

	snprintf(sp_name, sizeof(sp_name), "spool-%u-%u",
		 (unsigned int)init->dst,
		 (unsigned int)init->channel);
	rval = alloc_chrdev_region(&dev_no, 0, 1, sp_name);
	if (rval)
		return rval;

	if (init->pre_cfg)
		rval = sblock_pcfg_create(init->dst,
					  init->channel,
					  init->smem,
					  init->txblocknum,
					  init->txblocksize,
					  init->rxblocknum,
					  init->rxblocksize);
	else
		rval = sblock_create_ex(init->dst,
					init->channel,
					init->smem,
					init->txblocknum,
					init->txblocksize,
					init->rxblocknum,
					init->rxblocksize,
					NULL, NULL);
	if (rval) {
		dev_info(dev, "Failed to create sblock: %d\n", rval);
		goto free_devno;
	}

	spool = devm_kzalloc(&pdev->dev,
			     sizeof(struct spool_device),
			     GFP_KERNEL);
	if (!spool) {
		rval = -ENOMEM;
		goto free_sblock;
	}

	spool->init = init;
	spool->major = MAJOR(dev_no);
	spool->minor = MINOR(dev_no);
	spool->plt_dev = pdev;

	if (!init->nodev) {
		cdev_init(&spool->cdev, &spool_fops);

		rval = cdev_add(&spool->cdev, dev_no, 1);
		if (rval) {
			dev_info(dev, "Failed to add spool cdev\n");
			goto free_spool;
		}
	}

	spool->sys_dev = device_create(spool_class, NULL,
				       dev_no,
				       spool, "%s", init->name);
	device_create_file(&pdev->dev, &dev_attr_base_addr);

	platform_set_drvdata(pdev, spool);

	*out = spool;

	return 0;

free_spool:
	devm_kfree(&pdev->dev, spool);

free_sblock:
	sblock_destroy(init->dst, init->channel);

free_devno:
	unregister_chrdev_region(dev_no, 1);
	return rval;
}

static int destroy_spool(struct spool_device *spool)
{
	dev_t dev_no = MKDEV(spool->major, spool->minor);
	struct spool_init_data *init = spool->init;

	if (spool->sys_dev) {
		device_destroy(spool_class, dev_no);
		spool->sys_dev = NULL;
	}
	if (!init->nodev)
		cdev_del(&spool->cdev);
	sblock_destroy(init->dst, init->channel);
	unregister_chrdev_region(dev_no, 1);
	devm_kfree(&spool->plt_dev->dev, init);
	devm_kfree(&spool->plt_dev->dev, spool);

	return 0;
}

static int spool_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int rval;
	struct spool_init_data *init;
	struct spool_device *spool;
	struct device *dev = &pdev->dev;

	if (!np)
		return -ENODEV;

	rval = spool_parse_dt(&init, &pdev->dev, np);
	if (rval) {
		dev_err(dev, "Failed to parse spool device tree, ret=%d\n",
		       rval);
		return rval;
	}

	dev_info(dev, "name=%s, dst=%u, channel=%u, pre_cfg=%u\n",
		init->name,
		init->dst,
		init->channel,
		init->pre_cfg);

	dev_info(dev, "tx_num=%u, tx_size=%u, rx_num=%u, rx_size=%u\n",
		init->txblocknum,
		init->txblocksize,
		init->rxblocknum,
		init->rxblocksize);

	rval = create_spool(pdev, init, &spool);
	if (rval) {
		dev_err(dev, "Failed to create spool device %u:%u, ret=%d\n",
		       (unsigned int)init->dst,
		       (unsigned int)init->channel, rval);
		devm_kfree(&pdev->dev, init);
	}

	return 0;
}

static int spool_remove(struct platform_device *pdev)
{
	struct spool_device *priv = (struct spool_device *)
		platform_get_drvdata(pdev);

	destroy_spool(priv);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id spool_match_table[] = {
	{ .compatible = "sprd,spool", },
	{ },
};
static struct platform_driver spool_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-spool",
		.of_match_table = spool_match_table,
	},
	.probe = spool_probe,
	.remove = spool_remove,
};

static int __init spool_init(void)
{
	spool_class = class_create(THIS_MODULE, "spool");
	if (IS_ERR(spool_class))
		return PTR_ERR(spool_class);

	return platform_driver_register(&spool_driver);
}

static void __exit spool_exit(void)
{
	class_destroy(spool_class);
	platform_driver_unregister(&spool_driver);
}

module_init(spool_init);
module_exit(spool_exit);

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION("SIPC/SPOOL driver");
MODULE_LICENSE("GPL");
