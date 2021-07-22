// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/capability.h>

#include <linux/rpmb.h>

#include "rpmb-cdev.h"

static dev_t rpmb_devt;
#define RPMB_MAX_DEVS  MINORMASK

#define RPMB_DEV_OPEN    0  /** single open bit (position) */
/* from MMC_IOC_MAX_CMDS */
#define RPMB_MAX_FRAMES 255

/**
 * rpmb_open - the open function
 *
 * @inode: pointer to inode structure
 * @fp: pointer to file structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_open(struct inode *inode, struct file *fp)
{
	struct rpmb_dev *rdev;

	rdev = container_of(inode->i_cdev, struct rpmb_dev, cdev);
	if (!rdev)
		return -ENODEV;

	/* the rpmb is single open! */
	if (test_and_set_bit(RPMB_DEV_OPEN, &rdev->status))
		return -EBUSY;

	mutex_lock(&rdev->lock);

	fp->private_data = rdev;

	mutex_unlock(&rdev->lock);

	return nonseekable_open(inode, fp);
}

/**
 * rpmb_open - the open function
 *
 * @inode: pointer to inode structure
 * @fp: pointer to file structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_release(struct inode *inode, struct file *fp)
{
	struct rpmb_dev *rdev = fp->private_data;

	clear_bit(RPMB_DEV_OPEN, &rdev->status);

	return 0;
}

/**
 * rpmb_cmd_copy_from_user -  copy rpmb command from the user space
 *
 * @cmd:  internal cmd structure
 * @ucmd: user space cmd structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_cmd_copy_from_user(struct rpmb_cmd *cmd,
				   struct rpmb_ioc_cmd __user *ucmd)
{
	size_t sz;
	struct rpmb_frame *frames;
	u64 frames_ptr;

	if (get_user(cmd->flags, &ucmd->flags))
		return -EFAULT;

	if (get_user(cmd->nframes, &ucmd->nframes))
		return -EFAULT;

	if (cmd->nframes > RPMB_MAX_FRAMES)
		return -EOVERFLOW;

	if (!cmd->nframes)
		return -EINVAL;

	/* some archs have issues with 64bit get_user */
	if (copy_from_user(&frames_ptr, &ucmd->frames_ptr, sizeof(frames_ptr)))
		return -EFAULT;

	sz = cmd->nframes * sizeof(struct rpmb_frame);
	frames = memdup_user((const void __user *)frames_ptr, sz);
	if (IS_ERR(frames))
		return PTR_ERR(frames);

	cmd->frames = frames;
	return 0;
}

/**
 * rpmb_cmd_copy_to_user -  copy rpmb command to the user space
 *
 * @ucmd: user space cmd structure
 * @cmd:  internal cmd structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_cmd_copy_to_user(struct rpmb_ioc_cmd __user *ucmd,
				 struct rpmb_cmd *cmd)
{
	size_t sz;
	u64 frames_ptr;

	sz = cmd->nframes * sizeof(struct rpmb_frame);

	if (copy_from_user(&frames_ptr, &ucmd->frames_ptr, sizeof(frames_ptr)))
		return -EFAULT;

	/* some archs have issues with 64bit get_user */
	if (copy_to_user((void __user *)frames_ptr, cmd->frames, sz))
		return -EFAULT;

	return 0;
}

/**
 * rpmb_ioctl_seq_cmd: issue rpmb sequence
 *
 * @rdev: rpmb device
 * @ptr:  rpmb cmd sequence
 *
 * RPMB_IOC_SEQ_CMD handler
 *
 * Return: 0 on success, <0 on error
 */
static long rpmb_ioctl_seq_cmd(struct rpmb_dev *rdev,
			       struct rpmb_ioc_seq_cmd __user *ptr)
{
	__u64 ncmds;
	struct rpmb_cmd *cmds;
	struct rpmb_ioc_cmd __user *ucmds;

	int i;
	int ret;

	/* The caller must have CAP_SYS_RAWIO, like mmc ioctl */
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	/* some archs have issues with 64bit get_user */
	if (copy_from_user(&ncmds, &ptr->num_of_cmds, sizeof(ncmds)))
		return -EFAULT;

	if (ncmds > 3) {
		dev_notice(&rdev->dev, "supporting up to 3 packets (%llu)\n",
			ncmds);
		return -EINVAL;
	}

	cmds = kcalloc(ncmds, sizeof(*cmds), GFP_KERNEL);
	if (!cmds)
		return -ENOMEM;

	ucmds = (struct rpmb_ioc_cmd __user *)ptr->cmds;
	for (i = 0; i < ncmds; i++) {
		ret = rpmb_cmd_copy_from_user(&cmds[i], &ucmds[i]);
		if (ret)
			goto out;
	}

	ret = rpmb_cmd_seq(rdev, cmds, ncmds);
	if (ret)
		goto out;

	for (i = 0; i < ncmds; i++) {
		ret = rpmb_cmd_copy_to_user(&ucmds[i], &cmds[i]);
		if (ret)
			goto out;
	}
out:
	for (i = 0; i < ncmds; i++)
		kfree(cmds[i].frames);
	kfree(cmds);
	return ret;
}

/**
 * rpmb_ioctl_req_cmd: issue rpmb request command
 *
 * @rdev: rpmb device
 * @ptr: rpmb request command
 *
 * RPMB_IOC_REQ_CMD handler
 *
 * Return: 0 on success; < 0 on error
 */
static long rpmb_ioctl_req_cmd(struct rpmb_dev *rdev,
			       struct rpmb_ioc_req_cmd __user *ptr)
{
	struct rpmb_data rpmbd;
	u64 req_type;
	int ret;

	/* some archs have issues with 64bit get_user */
	if (copy_from_user(&req_type, &ptr->req_type, sizeof(req_type)))
		return -EFAULT;

	if (req_type >= U16_MAX)
		return -EINVAL;

	memset(&rpmbd, 0, sizeof(rpmbd));

	rpmbd.req_type = req_type & 0xFFFF;

	ret = rpmb_cmd_copy_from_user(&rpmbd.icmd, &ptr->icmd);
	if (ret)
		goto out;

	ret = rpmb_cmd_copy_from_user(&rpmbd.ocmd, &ptr->ocmd);
	if (ret)
		goto out;

	ret = rpmb_cmd_req(rdev, &rpmbd);
	if (ret)
		goto out;

	ret = rpmb_cmd_copy_to_user(&ptr->ocmd, &rpmbd.ocmd);

out:
	kfree(rpmbd.icmd.frames);
	kfree(rpmbd.ocmd.frames);
	return ret;
}

/**
 * rpmb_ioctl - rpmb ioctl dispatcher
 *
 * @fp: a file pointer
 * @cmd: ioctl command RPMB_IOC_REQ_CMD or RPMB_IOC_SEQ_CMD
 * @arg: ioctl data: rpmb_ioc_req_cmd or rpmb_ioc_seq_cmd
 *
 * Return: 0 on success; < 0 on error
 */
static long rpmb_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct rpmb_dev *rdev = fp->private_data;
	void __user *ptr = (void __user *)arg;

	switch (cmd) {
	case RPMB_IOC_REQ_CMD:
		return rpmb_ioctl_req_cmd(rdev, ptr);
	case RPMB_IOC_SEQ_CMD:
		return rpmb_ioctl_seq_cmd(rdev, ptr);
	default:
		dev_notice(&rdev->dev, "unsupported ioctl 0x%x.\n", cmd);
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long rpmb_compat_ioctl(struct file *fp, unsigned int cmd,
			      unsigned long arg)
{
	return rpmb_ioctl(fp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /* CONFIG_COMPAT */

static const struct file_operations rpmb_fops = {
	.open           = rpmb_open,
	.release        = rpmb_release,
	.unlocked_ioctl = rpmb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = rpmb_compat_ioctl,
#endif
	.owner          = THIS_MODULE,
	.llseek         = noop_llseek,
};

void rpmb_cdev_prepare(struct rpmb_dev *rdev)
{
	rdev->dev.devt = MKDEV(MAJOR(rpmb_devt), rdev->id);
	rdev->cdev.owner = THIS_MODULE;
	cdev_init(&rdev->cdev, &rpmb_fops);
}

void rpmb_cdev_add(struct rpmb_dev *rdev)
{
	cdev_add(&rdev->cdev, rdev->dev.devt, 1);
}

void rpmb_cdev_del(struct rpmb_dev *rdev)
{
	if (rdev->dev.devt)
		cdev_del(&rdev->cdev);
}

int __init rpmb_cdev_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&rpmb_devt, 0, RPMB_MAX_DEVS, "rpmb");
	if (ret < 0)
		pr_notice("unable to allocate char dev region\n");

	return ret;
}

void __exit rpmb_cdev_exit(void)
{
	if (rpmb_devt)
		unregister_chrdev_region(rpmb_devt, RPMB_MAX_DEVS);
}
