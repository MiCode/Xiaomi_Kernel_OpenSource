/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "star.h"
#include "star_procfs.h"

static struct star_procfs star_proc;

static bool str_cmp_seq(char **buf, const char *substr)
{
	size_t len = strlen(substr);

	if (!strncmp(*buf, substr, len)) {
		*buf += len + 1;
		return true;
	} else {
		return false;
	}
}

static struct net_device *star_get_net_device(void)
{
	if (!star_proc.ndev)
		star_proc.ndev = dev_get_by_name(&init_net, "eth0");

	return star_proc.ndev;
}

static void star_put_net_device(void)
{
	if (!star_proc.ndev)
		return;

	dev_put(star_proc.ndev);
}

static ssize_t proc_phy_reg_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	STAR_PR_INFO("read phy register useage:\n");
	STAR_PR_INFO("\t echo rp reg_addr > phy_reg\n");

	STAR_PR_INFO("write phy register useage:\n");
	STAR_PR_INFO("\t echo wp reg_addr value > phy_reg\n");

	return 0;
}

static ssize_t proc_reg_write(struct file *file,
			      const char __user *buffer,
			      size_t count, loff_t *pos)
{
	char *buf, *tmp;
	u16 phy_val;
	u32 i, mac_val, len = 0, address = 0, value = 0;
	struct net_device *dev;
	star_private *star_prv;
	star_dev *star_dev;

	tmp = kmalloc(count + 1, GFP_KERNEL);
	buf = tmp;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);
	star_dev = &star_prv->star_dev;

	if (str_cmp_seq(&buf, "rp")) {
		if (!kstrtou32(buf, 0, &address)) {
			STAR_PR_INFO("address(0x%x):0x%x\n",
				     address,
				     star_mdc_mdio_read(star_dev,
							star_prv->phy_addr,
							address));
		} else {
			STAR_PR_INFO("kstrtou32 rp(%s) error\n", buf);
		}
	} else if (str_cmp_seq(&buf, "wp")) {
		if (sscanf(buf, "%x %x", &address, &value) == 2) {
			phy_val = star_mdc_mdio_read(star_dev,
						     star_prv->phy_addr,
						     address);
			star_mdc_mdio_write(star_dev, star_prv->phy_addr,
					    address, (u16)value);
			STAR_PR_INFO("0x%x: 0x%x --> 0x%x!\n",
				     address, phy_val,
				     star_mdc_mdio_read(star_dev,
							star_prv->phy_addr,
							address));
		} else {
			STAR_PR_INFO("sscanf wp(%s) error\n", buf);
		}
	} else if (str_cmp_seq(&buf, "rr")) {
		if (sscanf(buf, "%x %x", &address, &len) == 2) {
			for (i = 0; i < len / 4; i++) {
				STAR_PR_INFO(
					 "%p:\t%08x\t%08x\t%08x\t%08x\t\n",
					 star_dev->base + address + i * 16,
					 star_get_reg(star_dev->base
						    + address + i * 16),
					 star_get_reg(star_dev->base + address
						    + i * 16 + 4),
					 star_get_reg(star_dev->base + address
						    + i * 16 + 8),
					 star_get_reg(star_dev->base + address
						    + i * 16 + 12));
			}
		} else {
			STAR_PR_INFO("sscanf rr(%s) error\n", buf);
		}
	} else if (str_cmp_seq(&buf, "wr")) {
		if (sscanf(buf, "%x %x", &address, &value) == 2) {
			mac_val = star_get_reg(star_dev->base + address);
			star_set_reg(star_dev->base + address, value);
			STAR_PR_INFO("%p: %08x --> %08x!\n",
				     star_dev->base + address,
				     mac_val,
				     star_get_reg(star_dev->base
						  + address));
		} else {
			STAR_PR_INFO("sscanf wr(%s) error\n", buf);
		}
	} else {
		STAR_PR_INFO("wrong arg:%s\n", buf);
	}

	kfree(tmp);
	return count;
}

static const struct file_operations star_phy_reg_ops = {
	.read = proc_phy_reg_read,
	.write = proc_reg_write,
};

static ssize_t proc_mac_reg_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	STAR_PR_INFO("read MAC register useage:\n");
	STAR_PR_INFO("\t echo rr reg_addr len > macreg\n");

	STAR_PR_INFO("write MAC register useage:\n");
	STAR_PR_INFO("\t echo wr reg_addr value > macreg\n");

	return 0;
}

static const struct file_operations star_mac_reg_ops = {
	.read = proc_mac_reg_read,
	.write = proc_reg_write,
};

static int get_wol_status(struct seq_file *seq, void *v)
{
	struct net_device *dev;
	star_private *star_prv;

	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);

	seq_printf(seq, "Wake On Lan (WOL) type is (%d)\n", star_prv->wol);
	STAR_PR_INFO("'echo 0 > /proc/driver/star/wol'->WOL_NONE\n");
	STAR_PR_INFO("'echo 1 > /proc/driver/star/wol'->MAC_WOL\n");
	STAR_PR_INFO("'echo 2 > /proc/driver/star/wol'->PHY_WOL\n");

	return 0;
}

static ssize_t wol_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *data)
{
	struct net_device *dev;
	star_private *star_prv;
	char *buf;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);
	star_prv->wol = buf[0] - '0';
	STAR_PR_INFO("Wake On Lan (WOL) type is (%d)\n", star_prv->wol);
	kfree(buf);

	return count;
}

static int wol_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_wol_status, NULL);
}

static const struct file_operations star_wol_ops = {
	.owner = THIS_MODULE,
	.open = wol_open,
	.read = seq_read,
	.write = wol_write,
};

static int get_wol_flag_status(struct seq_file *seq, void *v)
{
	struct net_device *dev;
	star_private *star_prv;

	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);
	seq_printf(seq, "Wake On Lan (WOL) flag is (%d)\n", star_prv->wol_flag);

	return 0;
}

static ssize_t wol_flag_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	struct net_device *dev;
	star_private *star_prv;
	star_dev *star_dev;
	char *buf;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);
	star_dev = &star_prv->star_dev;
	star_prv->wol_flag = buf[0] - '0';
	STAR_PR_INFO("Wake On Lan (WOL) flag is (%d)\n", star_prv->wol_flag);
	if (star_prv->wol == MAC_WOL) {
		if (star_prv->wol_flag)
			star_config_wol(star_dev, true);
		else
			star_config_wol(star_dev, false);
	} else if (star_prv->wol == PHY_WOL) {
		if (star_prv->wol_flag) {
			/*set ethernet phy wol setting*/
			if (star_dev->phy_ops->wol_enable)
				star_dev->phy_ops->wol_enable(star_prv->dev);
			enable_irq_wake(star_prv->eint_irq);
			STAR_PR_INFO("set ethernet phy wol setting done.\n");
		} else {
			/*clear ethernet phy wol setting*/
			if (star_dev->phy_ops->wol_disable)
				star_dev->phy_ops->wol_disable(star_prv->dev);
			disable_irq_wake(star_prv->eint_irq);
			STAR_PR_INFO("clear ethernet phy wol setting done.\n");
		}
	}
	kfree(buf);

	return count;
}

static int wol_flag_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_wol_flag_status, NULL);
}

static const struct file_operations star_wol_flag_ops = {
	.owner = THIS_MODULE,
	.open = wol_flag_open,
	.read = seq_read,
	.write = wol_flag_write,
};

static ssize_t proc_dump_net_stat(struct file *file,
				  char __user *buf, size_t count, loff_t *ppos)
{
	struct net_device *dev;
	star_private *star_prv;
	star_dev *star_dev;

	dev = star_get_net_device();
	if (!dev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(dev);
	star_dev = &star_prv->star_dev;
	STAR_PR_DEBUG("\n");
	STAR_PR_DEBUG("rx_packets	=%lu  <total packets received>\n",
		      star_dev->stats.rx_packets);
	STAR_PR_DEBUG("tx_packets	=%lu  <total packets transmitted>\n",
		      star_dev->stats.tx_packets);
	STAR_PR_DEBUG("rx_bytes	=%lu  <total bytes received>\n",
		      star_dev->stats.rx_bytes);
	STAR_PR_DEBUG("tx_bytes	=%lu  <total bytes transmitted>\n",
		      star_dev->stats.tx_bytes);
	STAR_PR_DEBUG("rx_errors;	=%lu  <bad packets received>\n",
		      star_dev->stats.rx_errors);
	STAR_PR_DEBUG("tx_errors;	=%lu  <packet transmit problems>\n",
		      star_dev->stats.tx_errors);
	STAR_PR_DEBUG("rx_crc_errors =%lu  <recved pkt with crc error>\n",
		      star_dev->stats.rx_crc_errors);
	STAR_PR_DEBUG("\n");
	STAR_PR_DEBUG(
		 "Use 'cat /proc/driver/star/stat' to dump net info\n");
	STAR_PR_DEBUG(
		 "Use 'echo clear > /proc/driver/star/stat' to clear info\n");

	return 0;
}

static ssize_t proc_clear_net_stat(struct file *file,
				   const char __user *buffer,
				   size_t count, loff_t *pos)
{
	char *buf;
	struct net_device *ndev;
	star_private *star_prv;
	star_dev *star_dev;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	ndev = star_get_net_device();
	if (!ndev) {
		STAR_PR_ERR("Could not get eth0 device!!!\n");
		return -1;
	}

	star_prv = netdev_priv(ndev);
	star_dev = &star_prv->star_dev;

	if (!strncmp(buf, "clear", count - 1))
		memset(&star_dev->stats, 0, sizeof(struct net_device_stats));
	else
		STAR_PR_INFO("fail to clear stat, buf:%s\n", buf);

	kfree(buf);

	return count;
}

static const struct file_operations star_net_status_ops = {
	.read = proc_dump_net_stat,
	.write = proc_clear_net_stat,
};

static struct star_proc_file star_file_tbl[] = {
	{"phy_reg", &star_phy_reg_ops},
	{"macreg", &star_mac_reg_ops},
	{"wol", &star_wol_ops},
	{"wol_flag", &star_wol_flag_ops},
	{"stat", &star_net_status_ops},
};

int star_init_procfs(void)
{
	int i;

	STAR_PR_INFO("%s entered\n", __func__);
	star_proc.root = proc_mkdir("driver/star", NULL);
	if (!star_proc.root) {
		STAR_PR_INFO("star_proc_dir create failed\n");
		return -1;
	}

	star_proc.entry = kmalloc(ARRAY_SIZE(star_file_tbl) *
				  sizeof(struct star_proc_file), GFP_KERNEL);
	for (i = 0 ; i < ARRAY_SIZE(star_file_tbl); i++) {
		star_proc.entry[i] = proc_create(star_file_tbl[i].name,
						 0755, star_proc.root,
						 star_file_tbl[i].fops);
		if (!star_proc.entry[i]) {
			STAR_PR_ERR(
				 "%s create failed\n", star_file_tbl[i].name);
			return -1;
		}
	}

	return 0;
}

void star_exit_procfs(void)
{
	int i;

	STAR_PR_INFO("%s entered\n", __func__);
	for (i = 0 ; i < ARRAY_SIZE(star_file_tbl); i++)
		remove_proc_entry(star_file_tbl[i].name, star_proc.root);

	kfree(star_proc.entry);
	remove_proc_entry("driver/star", NULL);
	star_put_net_device();
}

