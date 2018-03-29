/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* pr_debug() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... filp_open */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>	/*proc */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/aio.h>
#include <asm/uaccess.h>	/*set_fs get_fs mm_segment_t */
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/unistd.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mtd/mtd.h>
#include <generated/autoconf.h>
#include <linux/sched.h>	/* show_stack(current,NULL) */
#include <env.h>

#include "dumchar.h"		/* local definitions */
#ifdef CONFIG_MTK_EMMC_SUPPORT
#include "pmt.h"
#else
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#include "partition_define_tlc.h"
#else
#include "partition_define_mlc.h"
#endif
#endif
#include <linux/mmc/host.h>
/* #include <linux/mmc/sd_misc.h> */
/* #include "../mmc-host/mt_sd.h" */
#include <linux/genhd.h>
/* #include "partition_dumchar.h" */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/syscalls.h>
struct device;

static struct class *dumchar_class;
static struct device *dumchar_device[PART_MAX_COUNT];
static struct dumchar_dev *dumchar_devices;	/* store all dum char info,  allocated in dumchar_init */
static struct proc_dir_entry *dumchar_proc_entry;


static unsigned int major;

int IsEmmc(void)
{
#ifdef CONFIG_MTK_EMMC_SUPPORT
	return 1;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(IsEmmc);

#ifdef CONFIG_MTK_EMMC_SUPPORT
static int init_sd_cmd(struct msdc_ioctl *cmd, loff_t addr,
		       u32 __user *buffer, int host_num, int iswrite,
		       u32 totalsize, int transtype, Region part)
{
	if (!cmd) {
		pr_debug("DumChar ERROR:no space for msdc_ioctl\n");
		return -EINVAL;
	}
	if (addr < 0) {
		pr_debug("DumChar ERROR:Wrong Address %llx for emmc!\n", addr);
		return -EINVAL;
	}

	if (totalsize > MAX_SD_BUFFER) {
		pr_debug("DumChar ERROR:too mucn bytes for msdc\n");
		return -EINVAL;
	}
	memset(cmd, 0, sizeof(struct msdc_ioctl));
	if (addr % 512 == 0)
		cmd->address = addr / 512;
	else {
		pr_debug("DumChar ERROR: Wrong Address\n");
		return -EINVAL;
	}
	/* cmd->address =0x100000; */
	cmd->buffer = buffer;
	cmd->clock_freq = 0;
	cmd->host_num = host_num;
	cmd->iswrite = iswrite;
	cmd->result = -1;
	cmd->trans_type = transtype;
	cmd->total_size = totalsize;
	/* cmd->region = part; */
	cmd->partition = part;

	/*      cmdtype:MSDC_SINGLE_READ_WRITE  while MAX_SD_BUFFER >totalsize >512 byte;
	   MSDC_MULTIPLE_READ_WRITE   while totalsize <=512 byte;
	 */
	if (totalsize <= 512)
		cmd->opcode = MSDC_SINGLE_READ_WRITE;
	else
		cmd->opcode = MSDC_MULTIPLE_READ_WRITE;
/*
	pr_debug("*****************************\nDumCharDebug:in init_sd_cmd:\n");
	pr_debug("cmd->opcode=%d MSDC_SINGLE_READ_WRITE =(2) MSDC_MULTIPLE_READ_WRITE =(3)\n",cmd->opcode);
	pr_debug("cmd->host_num=%d supose=1\n",cmd->host_num);
	pr_debug("cmd->iswrite=%d write=1 read=0\n",cmd->iswrite);
	pr_debug("cmd->trans_type=%d\n",cmd->trans_type);
	pr_debug("cmd->total_size=%d\n",cmd->total_size);
	pr_debug("cmd->address=%d\n",cmd->address);
	pr_debug("cmd->buffer=%p\n",cmd->buffer);
	pr_debug("cmd->cmd_driving=%d\n",cmd->cmd_driving);
	pr_debug("cmd->dat_driving=%d\n",cmd->dat_driving);
	pr_debug("cmd->clock_freq=%d\n",cmd->clock_freq);
	pr_debug("cmd->result=%d\n",cmd->result);
	pr_debug("***************************\n");
*/
	return 0;

}
#endif
#ifdef CONFIG_MTK_EMMC_SUPPORT

int eMMC_rw_x(loff_t addr, u32 *buffer, int host_num, int iswrite, u32 totalsize, int transtype,
	      Region part)
{
	struct msdc_ioctl cmd;
	int result = 0;

	if (addr < 0) {
		pr_debug("DumChar ERROR:Wrong Address %llx for emmc!\n", addr);
		return -EINVAL;
	}

	memset(&cmd, 0, sizeof(struct msdc_ioctl));

	if (addr % 512 == 0)
		cmd.address = addr / 512;
	else {
		pr_debug("DumChar ERROR: Wrong Address\n");
		return -EINVAL;
	}
	/* cmd->address =0x100000; */
	cmd.buffer = buffer;
	cmd.clock_freq = 0;
	cmd.host_num = host_num;
	cmd.iswrite = iswrite;
	cmd.result = -1;
	cmd.trans_type = transtype;
	cmd.total_size = totalsize;
/* cmd.region = part; */
	cmd.partition = part;

	cmd.opcode = MSDC_CARD_DUNM_FUNC;

	result = simple_sd_ioctl_rw(&cmd);

	return result;
}
EXPORT_SYMBOL(eMMC_rw_x);

#if defined(CONFIG_MTK_EMMC_SUPPORT)
#include <linux/syscalls.h>

void emmc_create_sys_symlink(struct mmc_card *card)
{
	int i = 0;
	struct disk_part_iter piter;
	struct hd_struct *part;
	struct gendisk *disk;
	char link_target[256];
	char link_name[256];
	struct storage_info s_info = { 0 };

	/* emmc always in slot0 */
	if (!mmc_card_mmc(card)) {
		pr_debug("mtk-msdc %s: NOT emmc card,\n", __func__);
		return;
	}
	if (msdc_get_info(EMMC_CARD_BOOT, DISK_INFO, &s_info))
		disk = s_info.disk;
	else
		BUG();

	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter))) {
		for (i = 0; i < PART_NUM; i++) {
			if (PartInfo[i].partition_idx != 0
			    && PartInfo[i].partition_idx == part->partno) {
				sprintf(link_target, "/dev/block/%sp%d", disk->disk_name,
					part->partno);
				sprintf(link_name, "/emmc@%s", PartInfo[i].name);
				pr_debug("%s: target=%s, name=%s\n", __func__,
				       link_target, link_name);
				sys_symlink(link_target, link_name);
				break;
			}
		}
	}
	disk_part_iter_exit(&piter);

}

#endif				/* CONFIG_MTK_EMMC_SUPPORT */





#endif
#ifdef CONFIG_MTK_EMMC_SUPPORT
static ssize_t sd_single_read(struct file *filp, char __user *buf, size_t count, loff_t addr,
			      Region part)
{
	struct file_obj *fo = filp->private_data;
	struct msdc_ioctl cmd;
	ssize_t result = 0;

	if (init_sd_cmd(&cmd, addr, (u32 *) buf, 0, 0, count, 0, part)) {
		pr_debug("DumChar:init sd_cmd fail\n");
		return -EINVAL;
	}

	if (fo->act_filp->f_op->unlocked_ioctl)
		result = fo->act_filp->f_op->unlocked_ioctl(fo->act_filp, 1, (unsigned long)&cmd);
	else if (fo->act_filp->f_op->compat_ioctl)
		result = fo->act_filp->f_op->compat_ioctl(fo->act_filp, 1, (unsigned long)&cmd);

	if (result == 0)
		result = count;

	return result;
}

static ssize_t sd_read(struct file *filp, char __user *buf, size_t count, loff_t *pos,
		       Region part)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	size_t total_retlen = 0;
	int retlen = 0;
	int len;
	loff_t addr = *pos;

	if (*pos - dev->start_address + count > dev->size)
		count = dev->size - (*pos - dev->start_address);

	if (!count)
		return 0;
	if (addr % ALIE_LEN != 0 || (addr + count) % ALIE_LEN != 0) {
		loff_t startaddr = addr;
		loff_t endaddr = addr + count;
		loff_t startaddr2, endaddr2;
		loff_t buflen;
		char *pbuf;
		char *pbuf2;
		mm_segment_t curr_fs;

		if (addr % ALIE_LEN != 0)
			startaddr = (addr / ALIE_LEN) * ALIE_LEN;

		if ((addr + count) % ALIE_LEN != 0)
			endaddr = ((addr + count) / ALIE_LEN + 1) * ALIE_LEN;

		buflen = endaddr - startaddr;
		startaddr2 = startaddr;
		endaddr2 = endaddr;

		pbuf = kmalloc(buflen, GFP_KERNEL);
		if (!pbuf)
			return -ENOMEM;

		memset(pbuf, 0, buflen);
		pbuf2 = pbuf;

		curr_fs = get_fs();
		set_fs(KERNEL_DS);

		while (buflen > 0) {
			if (buflen > MAX_SD_BUFFER)
				len = MAX_SD_BUFFER;
			else
				len = buflen;

			retlen = sd_single_read(filp, pbuf2, len, startaddr2, part);

			if (retlen > 0) {
				startaddr2 += retlen;
				total_retlen += retlen;
				buflen -= retlen;
				pbuf2 += retlen;
				pr_debug("while retlen > 0 total_retlen=%d\n", (int)total_retlen);
			} else
				break;
		}
		set_fs(curr_fs);


#if defined(PrintBuff)
		int iter = 0;

		pr_debug
		    ("******************************\nGet %d bytes from %d to %d in %s in kernel\n",
		     (int)total_retlen, (int)startaddr, (int)endaddr, dev->dumname);
		for (iter = 0; iter < total_retlen; iter++) {
			if (iter % 16 == 0)
				pr_debug("\n");
			pr_debug(" %02x", pbuf[iter]);

		}
		pr_debug("\n********************************************************************\n");
#endif

		if (total_retlen == (endaddr - startaddr)) {
			int n = copy_to_user(buf, pbuf + (addr - startaddr), count);

			if (n != 0)
				pr_debug("read fail in DumChar_sd_read\n");

			total_retlen = count - n;
		} else
			pr_debug("read fail DumChar_sd_read!\n");

#if defined(PrintBuff)
		pr_debug("******************************\nGet %ld bytes from %d in %s in user:\n",
		       count, (int)*pos, dev->dumname);
		for (iter = 0; iter < count; iter++) {
			if (iter % 16 == 0)
				pr_debug("\n");
			pr_debug(" %02x", buf[iter]);

		}
		pr_debug("\n********************************************************************\n");
#endif

		kfree(pbuf);
	} else {
		while (count > 0) {
			if (count > MAX_SD_BUFFER)
				len = MAX_SD_BUFFER;
			else
				len = count;
			retlen = sd_single_read(filp, buf, len, addr, part);
			if (retlen > 0) {
				addr += retlen;
				total_retlen += retlen;
				count -= retlen;
				buf += retlen;
			} else
				break;
		}

	}
	*pos += total_retlen;
	return total_retlen;
}				/* mtd_read */

#endif
ssize_t dumchar_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	ssize_t result = 0;
#ifdef CONFIG_MTK_EMMC_SUPPORT
	loff_t pos = 0;
#endif
	if (fo->act_filp == (struct file *)0xffffffff) {
		pr_debug
		    ("[%s] Forbid to access %s with dumchar(/dev/%s), the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
		pr_debug("[dumchar_read] show_stack*************************************\n");
		show_stack(NULL, NULL);
		return -EINVAL;
	}

	if (dev->type != NAND && dev->type != EMMC) {
		pr_debug("DumChar:Wrong Dummy device Type %d ,it should be MTD or SDCARD!\n",
		       dev->type);
		return -EINVAL;
	}

	if (dev->type == EMMC) {
#ifdef CONFIG_MTK_EMMC_SUPPORT
		pos = *f_pos + dev->start_address;
		switch (dev->region) {
		case USER:
			result = vfs_read(fo->act_filp, buf, count, &pos);
			break;
		case BOOT_1:
			result = sd_read(filp, buf, count, &pos, BOOT_1);
			break;
		default:
			pr_debug("DumChar: Wrong EMMC Region\n");
			return -EINVAL;
		}
		fo->act_filp->f_pos = pos - dev->start_address;
		*f_pos = pos - dev->start_address;
#endif
	} else {
		result = vfs_read(fo->act_filp, buf, count, f_pos);
		fo->act_filp->f_pos = *f_pos;
	}
	return result;
}

#ifdef CONFIG_MTK_EMMC_SUPPORT
static ssize_t sd_single_write(struct file *filp, const char __user *buf, size_t count,
			       loff_t addr, Region part)
{
	struct file_obj *fo = filp->private_data;
	struct msdc_ioctl cmd;
	int result = 0;

	/*mt6573_sd0 host0 Todo: Only support host 0 */
	if (init_sd_cmd(&cmd, addr, (u32 *) buf, 0, 1, count, 0, part)) {
		pr_debug("DumChar:init sd_cmd fail\n");
		return -EINVAL;
	}

	if (fo->act_filp->f_op->unlocked_ioctl)
		result = fo->act_filp->f_op->unlocked_ioctl(fo->act_filp, 1, (unsigned long)&cmd);
	else if (fo->act_filp->f_op->compat_ioctl) {
		result = fo->act_filp->f_op->compat_ioctl(fo->act_filp, 1, (unsigned long)&cmd);


	if (result >= 0)
		result = count;
	else
		result = 0;

	return result;
}

static ssize_t sd_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos,
			Region part)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	size_t total_retlen = 0;
	int retlen = 0;
	int len;
	loff_t addr = *pos;	/* +dev->start_address; */

	if (*pos + count > dev->size)
		count = dev->size - *pos;
	if (!count)
		return 0;

	while (count > 0) {

		if (count > MAX_SD_BUFFER)
			len = MAX_SD_BUFFER;
		else
			len = count;
		retlen = sd_single_write(filp, buf, len, addr, part);
		if (retlen > 0) {
			addr += retlen;
			total_retlen += retlen;
			count -= retlen;
			buf += retlen;
		} else {
			return total_retlen;
		}
	}
	*pos += total_retlen;
	return total_retlen;
}
#endif

ssize_t dumchar_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	ssize_t result = 0;
#ifdef CONFIG_MTK_EMMC_SUPPORT
	loff_t pos = 0;
#endif
#if defined(CONFIG_MTK_COMBO_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	struct mtd_info *mtd;
	unsigned int writesize = -1;
#endif

	if (fo->act_filp == (struct file *)0xffffffff) {
		pr_debug
		    ("[%s] Forbidded to access %s with dumchar(/dev/%s), the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
		pr_debug("[dumchar_write] show_stack*************************************\n");
		show_stack(NULL, NULL);
		return -EINVAL;
	}

#if defined(CONFIG_MTK_COMBO_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	mtd_for_each_device(mtd) {
		if (dev->mtd_index == mtd->index) {
			writesize = mtd->writesize;
			break;
		}
	}
	if (writesize < 0) {
		pr_debug("[dumchar_write] Get writesize fail\n");
		show_stack(NULL, NULL);
		return -EINVAL;
	}
	if ((__u32) *f_pos % (writesize) != 0) {
		pr_debug("[dumchar_write] Address Not alignment, write_size=%d byte, address =%lld\n",
		       writesize, *f_pos);
		show_stack(NULL, NULL);
		return -EINVAL;
	}
#else
	if (*f_pos % (WRITE_SIZE_Byte) != 0) {
		pr_debug("[dumchar_write] Address Not alignment, write_size=%d byte, address =%lld\n",
		       WRITE_SIZE_Byte, *f_pos);
		show_stack(NULL, NULL);
		return -EINVAL;
	}
#endif

	if (dev->type != NAND && dev->type != EMMC) {
		pr_debug("DumChar:Wrong Dummy device Type %d ,it should be MTD or SDCARD!\n",
		       dev->type);
		return -EINVAL;
	}

	if (dev->type == EMMC) {
#ifdef CONFIG_MTK_EMMC_SUPPORT
		pos = *f_pos + dev->start_address;
		switch (dev->region) {
		case USER:
			result = vfs_write(fo->act_filp, buf, count, &pos);
			break;
		case BOOT_1:
			result = sd_write(filp, buf, count, &pos, BOOT_1);
			break;
		default:
			pr_debug("DumChar: Wrong EMMC Region\n");
			return -EINVAL;
		}
		fo->act_filp->f_pos = pos - dev->start_address;
		*f_pos = pos - dev->start_address;
#endif
	} else {
		result = vfs_write(fo->act_filp, buf, count, f_pos);
		fo->act_filp->f_pos = *f_pos;
	}
	return result;
}

static long dumchar_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	long result = 0;
#ifdef CONFIG_MTK_EMMC_SUPPORT
	long i;
	struct mtd_info_user info;
	struct erase_info_user erase_info;
	void __user *argp = (void __user *)arg;
	u_long size;
	mm_segment_t curr_fs;
	char *ebuf;
	loff_t addr;
	__u32 count;
#endif
	if (fo->act_filp == (struct file *)0xffffffff) {
		pr_debug
		    ("[%s] Forbidded to access %s with dumchar(/dev/%s),the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
		pr_debug("[dumchar_loctl] show_stack*************************************\n");
		show_stack(NULL, NULL);
		return -EINVAL;
	}


	if (dev->type != NAND && dev->type != EMMC) {
		pr_debug("DumChar:Wrong Dummy device Type %d ,it should be MTD or SDCARD!\n",
		       dev->type);
		return -EINVAL;
	}

	if (dev->type == NAND) {
		if (fo->act_filp->f_op->unlocked_ioctl)
			result = fo->act_filp->f_op->unlocked_ioctl(fo->act_filp, cmd, arg);
		else if (fo->act_filp->f_op->compat_ioctl)
			result = fo->act_filp->f_op->compat_ioctl(fo->act_filp, cmd, arg);

	} else {
#if 0
		if (!strcmp(dev->dumname, "pmt")) {
#ifdef CONFIG_MTK_EMMC_SUPPORT
			switch (cmd) {
			case PMT_READ:
				pr_debug("PMT IOCTL: PMT_READ\n");
				result = read_pmt(argp);
				break;
			case PMT_WRITE:
				pr_debug("PMT IOCTL: PMT_WRITE\n");
				result = write_pmt(argp);
				break;
			default:
				result = -EINVAL;
			}
			return result;

#endif

		} else {
#endif
#ifdef CONFIG_MTK_EMMC_SUPPORT
			size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
			if (cmd & IOC_IN) {
				if (!access_ok(VERIFY_READ, argp, size))
					return -EFAULT;
			}
			if (cmd & IOC_OUT) {
				if (!access_ok(VERIFY_WRITE, argp, size))
					return -EFAULT;
			}

			switch (cmd) {
			case MEMGETREGIONCOUNT:
				break;
			case MEMGETREGIONINFO:
				break;
			case MEMGETINFO:
				info.type = MTD_NANDFLASH;
				info.flags = MTD_WRITEABLE;
				info.size = dev->size;
				info.erasesize = 128 * 1024;
				info.writesize = 512;
				info.oobsize = 0;
				info.padding = 0;
				if (copy_to_user(argp, &info, sizeof(struct mtd_info_user)))
					return -EFAULT;
				break;
			case MEMERASE:
				if (copy_from_user
				    (&erase_info, argp, sizeof(struct erase_info_user)))
					return -EFAULT;
				addr = (loff_t) erase_info.start + dev->start_address;
				count = erase_info.length;
				ebuf = kmalloc(MAX_SD_BUFFER, GFP_KERNEL);
				if (!ebuf) {
					pr_debug("DumChar: malloc ebuf buffer fail\n");
					return -ENOMEM;
				}
				memset(ebuf, 0xFF, MAX_SD_BUFFER);

				curr_fs = get_fs();
				set_fs(KERNEL_DS);

				switch (dev->region) {
				case USER:
					for (i = 0; i < (count + MAX_SD_BUFFER - 1) / MAX_SD_BUFFER;
					     i++) {
						result =
						    vfs_write(fo->act_filp, ebuf, MAX_SD_BUFFER,
							      &addr);
					}
					break;
				case BOOT_1:
					for (i = 0; i < (count + MAX_SD_BUFFER - 1) / MAX_SD_BUFFER;
					     i++) {
						result =
						    sd_write(filp, ebuf, MAX_SD_BUFFER, &addr,
							     BOOT_1);
					}
					break;
				default:
					pr_debug("DumChar: Wrong EMMC Region\n");
					return -EINVAL;
				}

				set_fs(curr_fs);
				kfree(ebuf);
				break;
			case MEMERASE64:
				break;
			case MEMWRITEOOB:
				break;
			case MEMREADOOB:
				break;
			case MEMWRITEOOB64:
				break;
			case MEMREADOOB64:
				break;
			case MEMLOCK:
				break;
			case MEMUNLOCK:
				break;
			case MEMGETOOBSEL:
				break;
			case MEMGETBADBLOCK:
				break;
			case MEMSETBADBLOCK:
				break;
			case ECCGETLAYOUT:
				break;
			case ECCGETSTATS:
				break;
			case MTDFILEMODE:
				break;
			case MTD_FILE_MODE_NORMAL:
				break;
			default:
				result = -EINVAL;
			}
#endif
#if 0
		}
#endif
	}
	return result;
}

loff_t dumchar_llseek(struct file *filp, loff_t off, int whence)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = dumchar_devices + fo->index;
	loff_t newpos;

	if (fo->act_filp == (struct file *)0xffffffff) {
		pr_debug
		    ("[%s] Forbidded to access %s with dumchar(/dev/%s), the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
		pr_debug("[dumchar_llseek] show_stack*************************************\n");
		show_stack(NULL, NULL);
		return -EINVAL;
	}

	if (dev->type != NAND && dev->type != EMMC) {
		pr_debug("DumChar:Wrong Dummy device Type %d ,it should be MTD or SDCARD!\n",
		       dev->type);
		return -EINVAL;
	}

	if (fo->act_filp->f_op->llseek) {
		newpos = fo->act_filp->f_op->llseek(fo->act_filp, off, whence);
	} else {
		switch (whence) {
		case SEEK_SET:
			newpos = off;
			break;
		case SEEK_CUR:
			newpos = fo->act_filp->f_pos + off;
			break;
		case SEEK_END:
			newpos = dev->size + off;
			break;
		default:
			return -EINVAL;
		}
	}

	if (newpos >= 0 && newpos <= dev->size) {
		fo->act_filp->f_pos = newpos;
		filp->f_pos = newpos;
		return fo->act_filp->f_pos;
	}

	return -EINVAL;
}

#if 0
static int dumchar_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i, len = 0;

	len += sprintf(page + len, "Part_Name\tSize\tStartAddr\tType\tMapTo\n");

	for (i = 0; i < PART_NUM; i++) {
		if (dumchar_devices[i].type == NAND) {
			len += sprintf(page + len, "%-10s   0x%016llx   0x%016llx   1   %s\n",
				       dumchar_devices[i].dumname,
				       dumchar_devices[i].size, dumchar_devices[i].start_address,
				       /*   "NAND", */
				       dumchar_devices[i].actname);
		} else {
			if (PartInfo[i].partition_idx == 0) {
				len +=
				    sprintf(page + len, "%-10s   0x%016llx   0x%016llx   2   %s\n",
					    dumchar_devices[i].dumname, dumchar_devices[i].size,
					    dumchar_devices[i].start_address,
					    /*"EMMC", */
					    dumchar_devices[i].actname);
			} else {
				len +=
				    sprintf(page + len,
					    "%-10s   0x%016llx   0x%016llx   2   /dev/block/mmcblk0p%d\n",
					    dumchar_devices[i].dumname, dumchar_devices[i].size,
					    dumchar_devices[i].start_address,
					    /* "EMMC", */
					    PartInfo[i].partition_idx);
			}
		}

	}

	if (dumchar_devices[i].type == NAND) {
		len +=
		    sprintf(page + len,
			    "Part_Name:Partition name you should open;\nSize:size of partition\nStartAddr:Index (MTD);\nType:Type of partition(MTD=1,EMMC=2);\nMapTo:actual device you operate\n");
	} else {
		len +=
		    sprintf(page + len,
			    "Part_Name:Partition name you should open;\nSize:size of partition\nStartAddr:Start Address of partition;\nType:Type of partition(MTD=1,EMMC=2)\nMapTo:actual device you operate\n");
	}
	*eof = 1;
	return len;
}
#endif
int dumchar_open(struct inode *inode, struct file *filp)
{
	struct dumchar_dev *dev;	/* device information */
	struct file_obj *fo;
	int i, result = -1, found = 0;

	fo = kmalloc(sizeof(struct file_obj), GFP_KERNEL);
	if (!fo)
		return -ENOMEM;

	filp->private_data = fo;

	for (i = 0; i < PART_NUM; i++) {
		if (!strcmp(filp->f_path.dentry->d_name.name, dumchar_devices[i].dumname)) {
			dev = &(dumchar_devices[i]);
			fo->index = i;
			found = 1;
			/* pr_debug("DumChar: find dev %s index=%d\n",dumchar_devices[i].dumname,i); */
			break;
		}
	}

	if (found == 0) {
		pr_debug(" DumChar:ERROR:No Such Dummy Device %s\n ",
		       filp->f_path.dentry->d_name.name);
		return -EINVAL;
	}

	if (!strcmp(dev->dumname, "usrdata") || !strcmp(dev->dumname, "cache")
	    || !strcmp(dev->dumname, "android") || !strcmp(dev->dumname, "fat")) {
		pr_debug
		    ("[%s] Forbidded to access %s with dumchar(/dev/%s), the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
		pr_debug("[dumchar_open] show_stack*************************************\n");
		show_stack(NULL, NULL);
		fo->act_filp = (struct file *)0xffffffff;
		kfree(fo);
		return -EINVAL;

	} else {
		pr_debug("[dumchar_open][%s] will open %s for %s!\n", current->comm, dev->actname,
		       filp->f_path.dentry->d_name.name);
		fo->act_filp = filp_open(dev->actname, filp->f_flags, 0777);
		if (IS_ERR(fo->act_filp)) {
			result = PTR_ERR(fo->act_filp);
			pr_debug
			    (" DumChar: [%s] open %s failed (%s ).  fo->act_filp=%p!, result=%d\n",
			     current->comm, dev->actname, filp->f_path.dentry->d_name.name,
			     fo->act_filp, result);
			pr_debug("[dumchar_open] show_stack*************************************\n");
			show_stack(NULL, NULL);
			/* pr_debug("[dumchar_open] BUG_ON*************************************\n"); */
			/* BUG_ON(1); */
			/* pr_debug("[dumchar_open] ************\n"); */
			goto open_fail2;
		} else {
			if (!(fo->act_filp->f_op)) {
				pr_debug
				    (" DumChar:open %s failed (%s ). has no file operations registered!\n",
				     dev->actname, filp->f_path.dentry->d_name.name);
				result = -EIO;
				goto open_fail1;
			}
		}
	}
	return 0;		/* success */

open_fail1:
	filp_close(fo->act_filp, NULL);
open_fail2:
	fo->act_filp = NULL;
	kfree(fo);
	return result;
}

#ifndef CONFIG_MTK_EMMC_SUPPORT

int mtd_create_symlink(void)
{
	char *link_target;
	char link_name[256];
	/* char ahead_target[256]; */
	/* char ahead_link[256]; */
	int i;

	for (i = 0; i < PART_NUM; i++) {
		if (dumchar_devices[i].actname) {
			memset(link_name, 0x0, sizeof(link_name));
			link_target = dumchar_devices[i].actname;
			sprintf(link_name, "/mtd@%s", dumchar_devices[i].dumname);
			pr_debug("[mtd_create_symlink]: target=%s, name=%s\n", link_target,
			       link_name);
			sys_symlink(link_target, link_name);
		}
#if 0
		if ((!strcmp(dumchar_devices[i].dumname, "usrdata"))
		    || (!strcmp(dumchar_devices[i].dumname, "cache"))
		    || (!strcmp(dumchar_devices[i].dumname, "android"))) {
			memset(ahead_target, 0x0, sizeof(ahead_target));
			memset(ahead_link, 0x0, sizeof(ahead_link));
			sprintf(ahead_target, "/sys/block/mtdblock%d/queue/read_ahead_kb",
				dumchar_devices[i].mtd_index);
			sprintf(ahead_link, "/read_ahead_kb@%s", dumchar_devices[i].dumname);
			sys_symlink(ahead_target, ahead_link);
			pr_debug("[mtd_create_symlink]: target=%s, name=%s\n", ahead_target,
			       ahead_link);
		}
#endif
	}
	return 0;
}

#if 0
int mtd_close_read_ahead(void)
{
	char target[256];
	u32 i, ret, origin, value;
	struct file *filp;

	for (i = 0; i < PART_NUM; i++) {
		if ((!strcmp(dumchar_devices[i].dumname, "usrdata"))
		    || (!strcmp(dumchar_devices[i].dumname, "cache"))
		    || (!strcmp(dumchar_devices[i].dumname, "android"))) {
			origin = 0;
			value = 0;
			memset(target, 0x0, sizeof(target));
			sprintf(target, "/sys/block/mtdblock%d/queue/read_ahead_kb",
				dumchar_devices[i].mtd_index);
			filp = filp_open(target, O_RDWR, 0666);
			if (IS_ERR(filp)) {
				ret = PTR_ERR(filp);
				pr_debug(
				       "[mtd_close_read_ahead]Open %s partition fail! errno=%d\n",
				       target, ret);
				continue;
			}
			ret = filp->f_op->read(filp, &origin, sizeof(origin), &(filp->f_pos));
			if (sizeof(origin) != ret) {
				pr_debug("[mtd_close_read_ahead]read fail!errno=%d\n", ret);
				filp_close(filp, NULL);
				continue;
			}
			ret = filp->f_op->write(filp, &value, sizeof(value), &(filp->f_pos));
			if (sizeof(value) != ret) {
				pr_debug("[mtd_close_read_ahead]write fail!errno=%d\n", ret);
				filp_close(filp, NULL);
				continue;
			}
			filp_close(filp, NULL);
			pr_debug("[mtd_close_read_ahead]:update succeed!%s from %x to %x\n",
			       target, origin, value);
		}

	}
}
#endif
#endif

int dumchar_release(struct inode *inode, struct file *filp)
{
	struct file_obj *fo = filp->private_data;
	struct dumchar_dev *dev = &dumchar_devices[fo->index];

	if (!strcmp(dev->dumname, "usrdata") || !strcmp(dev->dumname, "cache")
	    || !strcmp(dev->dumname, "android") || !strcmp(dev->dumname, "fat")) {
		pr_debug
		    ("[%s] Forbidded to access %s with dumchar(/dev/%s), the %s partition is managed by filesystem!\n",
		     __func__, dev->dumname, dev->dumname, dev->dumname);
	} else {
		filp_close(fo->act_filp, NULL);
	}
	kfree(fo);
	return 0;
}


const struct file_operations dumchar_fops = {
	.owner = THIS_MODULE,
	.llseek = dumchar_llseek,
	.read = dumchar_read,
	.write = dumchar_write,
	.unlocked_ioctl = dumchar_ioctl,
	.open = dumchar_open,
	.release = dumchar_release,
};

int dumchar_probe(struct platform_device *dev)
{
	int result, i, m, l;
	dev_t devno;
	loff_t misc_addr = 0;
#ifdef CONFIG_MTK_MTD_NAND
	struct mtd_info *mtd;
	int mtd_num = 0;
#endif
#ifdef CONFIG_MTK_EMMC_SUPPORT
	struct storage_info s_info = { 0 };
	/* struct msdc_host *host_ctl; */
	pr_debug("[Dumchar_probe]*******************Introduction******************\n");
	pr_debug("[Dumchar_probe]There are 3 address in eMMC Project: Linear, Logical and Physical Address\n");
	pr_debug("[Dumchar_probe]Linear Address: Used in scatter file, uboot, preloader,flash tool etc.\n");
	pr_debug("[Dumchar_probe]MBR linear address is fixed in eMMCComo.mk, that is same for all chips\n");
	pr_debug("[Dumchar_probe]Logical Address: Used in /proc/dumchar_info, mmcblk0 etc. MBR logical address is 0\n");
	pr_debug("[Dumchar_probe]Physical Address: Used in eMMC driver");
	pr_debug("MBR Physical Address = MBR Linear Address - (BOOT1 size + BOOT2 Size + RPMB Size)\n");
	pr_debug("[Dumchar_probe]define  User_Region_Header (BOOT1 size + BOOT2 Size + RPMB Size)\n");
	pr_debug("[Dumchar_probe]*******************Introduction******************\n");

	/* host_ctl = mtk_msdc_host[0]; */


	/* BUG_ON(!host_ctl); */
	/* BUG_ON(!host_ctl->mmc); */
	/* BUG_ON(!host_ctl->mmc->card); */

	init_pmt();

	msdc_get_info(EMMC_CARD_BOOT, EMMC_USER_CAPACITY, &s_info);
	msdc_get_info(EMMC_CARD_BOOT, EMMC_RESERVE, &s_info);
	pr_debug("[Dumchar]MBR address %x\n", MBR_START_ADDRESS_BYTE);
#endif

	result = alloc_chrdev_region(&devno, 0, PART_NUM, "DumChar");
	if (result < 0) {
		pr_debug("DumChar: Get chrdev region fail\n");
		goto fail_alloc_chrdev_region;
	}
	major = MAJOR(devno);

	for (i = 0; i < PART_NUM; i++) {
		dumchar_devices[i].dumname = PartInfo[i].name;
		sema_init(&(dumchar_devices[i].sem), 1);
		devno = MKDEV(major, i);
		cdev_init(&dumchar_devices[i].cdev, &dumchar_fops);
		dumchar_devices[i].cdev.owner = THIS_MODULE;
		dumchar_devices[i].cdev.ops = &dumchar_fops;
		result = cdev_add(&dumchar_devices[i].cdev, devno, 1);
		if (result) {
			pr_debug("DumChar: register char device dumchar fail!\n");
			goto fail_register_chrdev;
		}

		dumchar_devices[i].type = PartInfo[i].type;

		if (dumchar_devices[i].type == EMMC) {
#ifdef CONFIG_MTK_EMMC_SUPPORT

			dumchar_devices[i].region = PartInfo[i].region;
			switch (dumchar_devices[i].region) {
			case BOOT_1:
				sprintf(dumchar_devices[i].actname, MSDC_RAW_DEVICE);
				dumchar_devices[i].start_address = PartInfo[i].start_address;
				break;
			case USER:
				sprintf(dumchar_devices[i].actname, "/dev/block/mmcblk0");
				dumchar_devices[i].start_address =
				    PartInfo[i].start_address - MBR_START_ADDRESS_BYTE;
				break;
			default:
				pr_debug("Error! the emmc region is not supportted!\n");
				goto fail_register_chrdev;
			}

			dumchar_devices[i].size = PartInfo[i].size;

			if (!strcmp(PartInfo[i].name, "fat")) {
				dumchar_devices[i].size =
				    s_info.emmc_user_capacity * 512 -
				    dumchar_devices[i].start_address - s_info.emmc_reserve * 512;
			}
#ifdef CONFIG_MTK_SHARED_SDCARD
			if (!strcmp(PartInfo[i].name, "usrdata")) {
				dumchar_devices[i].size =
				    s_info.emmc_user_capacity * 512 -
				    dumchar_devices[i].start_address - s_info.emmc_reserve * 512;
			}
#endif
			/* if(!strcmp(PartInfo[i].name,"otp")||!strcmp(PartInfo[i].name,"bmtpool")){ */
			/* dumchar_devices[i].size = (PartInfo[i].start_address & 0x0000FFFF)*128*1024; */
			/* } */
			pr_debug("[Dumchar] %s start address=%llx size=%llx\n",
			       dumchar_devices[i].dumname, dumchar_devices[i].start_address,
			       dumchar_devices[i].size);
#endif
		} else {
#ifdef CONFIG_MTK_MTD_NAND
			char *mtdname = dumchar_devices[i].dumname;

			if (!strcmp(dumchar_devices[i].dumname, "seccfg"))
				mtdname = "seccnfg";

			if (!strcmp(dumchar_devices[i].dumname, "bootimg"))
				mtdname = "boot";

			if (!strcmp(dumchar_devices[i].dumname, "sec_ro"))
				mtdname = "secstatic";

			if (!strcmp(dumchar_devices[i].dumname, "android"))
				mtdname = "system";

			if (!strcmp(dumchar_devices[i].dumname, "usrdata"))
				mtdname = "userdata";

			if (!strcmp(dumchar_devices[i].dumname, "misc"))
				mtd_num = i;

			mtd_for_each_device(mtd) {
				if (!strcmp(mtd->name, mtdname)) {
					dumchar_devices[i].mtd_index = mtd->index;
					sprintf(dumchar_devices[i].actname, "/dev/mtd/mtd%d",
						mtd->index);
					dumchar_devices[i].size = mtd->size;
					dumchar_devices[i].start_address =
					    mtd_partition_start_address(mtd);
					break;
				}
			}
#endif
		}

	}

	dumchar_class = class_create(THIS_MODULE, "dumchar");
	for (l = 0; l < PART_NUM; l++) {
		if (!strcmp(dumchar_devices[l].dumname, "otp")) {
			dumchar_device[l] =
			    device_create(dumchar_class, NULL, MKDEV(major, l), NULL, "otp_bak");
		} else {
			dumchar_device[l] =
			    device_create(dumchar_class, NULL, MKDEV(major, l), NULL,
					  dumchar_devices[l].dumname);
		}
		if (!strcmp(dumchar_devices[l].dumname, "misc"))
			misc_addr = dumchar_devices[l].start_address;
		if (IS_ERR(dumchar_device[l])) {
			result = PTR_ERR(dumchar_device[l]);
			pr_debug("DumChar: fail in device_create name = %s  minor = %d\n",
			       dumchar_devices[l].dumname, l);
			goto fail_create_device;
		}
	}

#ifdef CONFIG_MTK_EMMC_SUPPORT
	env_init(misc_addr);
#else
	env_init(misc_addr, mtd_num);
#endif
#ifdef CONFIG_MTK_MTD_NAND
	mtd_create_symlink();
#endif

	return 0;

fail_create_device:
	for (m = 0; m < l; m++)
		device_destroy(dumchar_class, MKDEV(major, m));
	class_destroy(dumchar_class);
fail_register_chrdev:
	for (m = 0; m < i; m++)
		cdev_del(&dumchar_devices[m].cdev);

	unregister_chrdev_region(MKDEV(major, 0), PART_NUM);
fail_alloc_chrdev_region:
	return result;
}

int dumchar_remove(struct platform_device *dev)
{
	int i;

	pr_debug("DumCharDebug: in dumchar_remove\n");
	for (i = 0; i < PART_NUM; i++) {
		device_destroy(dumchar_class, MKDEV(major, i));
		cdev_del(&dumchar_devices[i].cdev);
	}

	class_destroy(dumchar_class);
	unregister_chrdev_region(MKDEV(major, 0), PART_NUM);
	return 0;
}

#ifdef CONFIG_MTK_MTD_NAND
static const struct of_device_id dumchar_of_ids[] = {
	{.compatible = "mediatek,dummy_char",},
	{}
};
#endif
static struct platform_driver dumchar_driver = {
	.probe = dumchar_probe,
	.remove = dumchar_remove,
	.driver = {
		   .name = "dummy_char",
		   .owner = THIS_MODULE,
#ifdef CONFIG_MTK_MTD_NAND
		   .of_match_table = dumchar_of_ids,
#endif
		   },
};

static int dumchar_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "Part_Name\tSize\tStartAddr\tType\tMapTo\n");

	for (i = 0; i < PART_NUM; i++) {
		if (dumchar_devices[i].type == NAND) {
			seq_printf(m, "%-10s   0x%016llx   0x%016llx   1	%s\n",
				   dumchar_devices[i].dumname,
				   dumchar_devices[i].size, dumchar_devices[i].start_address,
				   /*   "NAND", */
				   dumchar_devices[i].actname);
		} else {
			if (PartInfo[i].partition_idx == 0) {
				seq_printf(m, "%-10s   0x%016llx   0x%016llx   2   %s\n",
					   dumchar_devices[i].dumname,
					   dumchar_devices[i].size,
					   dumchar_devices[i].start_address,
					   /*"EMMC", */
					   dumchar_devices[i].actname);
			} else {
				seq_printf(m,
					   "%-10s   0x%016llx   0x%016llx   2   /dev/block/mmcblk0p%d\n",
					   dumchar_devices[i].dumname, dumchar_devices[i].size,
					   dumchar_devices[i].start_address,
					   /* "EMMC", */
					   PartInfo[i].partition_idx);
			}
		}
	}
	seq_printf(m, "%-10s   0x0000000000000000   0x0000000000000000   1\n", "bmtpool");

	if (dumchar_devices[i].type == NAND) {
		seq_printf(m,
			   "Part_Name:Partition name you should open;\nSize:size of partition\nStartAddr:Index (MTD);\nType:Type of partition(MTD=1,EMMC=2);\nMapTo:actual device you operate\n");
	} else {
		seq_printf(m,
			   "Part_Name:Partition name you should open;\nSize:size of partition\nStartAddr:Start Address of partition;\nType:Type of partition(MTD=1,EMMC=2)\nMapTo:actual device you operate\n");
	}

	return 0;
}

static int dumchar_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dumchar_proc_show, NULL);
}

static const struct file_operations dumchar_proc_ops = {
	.open = dumchar_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct platform_device dummychar_device = {
	.name = "dummy_char",
	.id = 0,
};

static int __init dumchar_init(void)
{
	int result;

	pr_debug("dumchar_int\n");
	dumchar_devices = kcalloc(PART_MAX_COUNT, sizeof(struct dumchar_dev), GFP_KERNEL);
	if (!dumchar_devices) {
		result = -ENOMEM;
		pr_debug("DumChar: malloc dumchar_dev fail\n");
		goto fail_malloc;
	}
	memset(dumchar_devices, 0, PART_MAX_COUNT * sizeof(struct dumchar_dev));

	dumchar_proc_entry =
	    proc_create("dumchar_info", S_IFREG | S_IRUGO, NULL, &dumchar_proc_ops);
	if (dumchar_proc_entry)
		pr_debug("dumchar: register /proc/dumchar_info success\n");
	else {
		pr_debug("dumchar: unable to register /proc/dumchar_info\n");
		result = -ENOMEM;
		goto fail_create_proc;
	}
	result = platform_driver_register(&dumchar_driver);
	if (result) {
		pr_debug("DUMCHAR: Can't register driver\n");
		goto fail_driver_register;
	}
	result = platform_device_register(&dummychar_device);
	pr_debug("[%s]: dummychar_device, retval=%d\n!", __func__, result);
	if (result != 0)
		goto fail_driver_register;

	pr_debug("DumChar: init USIF  Done!\n");

	return 0;
fail_driver_register:
	remove_proc_entry("dumchar_info", NULL);
fail_create_proc:
	kfree(dumchar_devices);
fail_malloc:
	return result;
}


static void __exit dumchar_cleanup(void)
{
	remove_proc_entry("dumchar_info", NULL);
	platform_driver_unregister(&dumchar_driver);
	kfree(dumchar_devices);
}


late_initcall_sync(dumchar_init);
module_exit(dumchar_cleanup);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Dummy Char Device Driver");
MODULE_AUTHOR("Kai Zhu <kai.zhu@mediatek.com>");
