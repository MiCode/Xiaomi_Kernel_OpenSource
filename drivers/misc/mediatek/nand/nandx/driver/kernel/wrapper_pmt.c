/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "nandx_pmt.h"
#include "mntl_ops.h"
#include "nandx_ops.h"
#include "wrapper_pmt.h"

#define PMT_MAGIC		'p'
#define PMT_READ		_IOW(PMT_MAGIC, 1, int)
#define PMT_WRITE		_IOW(PMT_MAGIC, 2, int)
#define PMT_VERSION		_IOW(PMT_MAGIC, 3, int)
#define PMT_UPDATE		_IOW(PMT_MAGIC, 4, int)

#define MTD_SECFG_STR		"seccnfg"
#define MTD_BOOTIMG_STR		"boot"
#define MTD_ANDROID_STR		"system"
#define MTD_SECRO_STR		"secstatic"
#define MTD_USRDATA_STR		"userdata"

static struct mtd_partition *mtdpart;
static unsigned char *part_name;
static struct mtd_erase_region_info *mtderase;

static int nandx_pmt_open(struct inode *inode, struct file *filp)
{
	pr_debug("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__,
		 MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static int nandx_pmt_release(struct inode *inode, struct file *filp)
{
	pr_debug("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__,
		 MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static int nandx_pmt_write(void __user *uarg)
{
	struct pmt_handler *handler;
	struct pt_resident *pt;
	struct DM_PARTITION_INFO_PACKET *packet;
	u32 i;
	int ret = 0, pt_changed = 0;

	handler = nandx_get_pmt_handler();
	pt = handler->pmt;

	packet = (struct DM_PARTITION_INFO_PACKET *)
	    mem_alloc(1, sizeof(struct DM_PARTITION_INFO_PACKET));
	if (packet == NULL)
		return -ENOMEM;

	if (copy_from_user(packet, uarg,
			   sizeof(struct DM_PARTITION_INFO_PACKET))) {
		ret = -EFAULT;
		goto freepacket;
	}

	for (i = 0; i < PART_MAX_COUNT; i++) {
		if (strcmp(pt->name, packet->part_info[i].part_name))
			pt_changed = 1;
		memcpy(pt->name, packet->part_info[i].part_name,
		       MAX_PARTITION_NAME_LEN);
		if (pt->offset != packet->part_info[i].start_addr)
			pt_changed = 1;
		pt->offset = packet->part_info[i].start_addr;
		if (pt->size != packet->part_info[i].part_len)
			pt_changed = 1;
		pt->size = packet->part_info[i].part_len;
		pt->mask_flags = 0;
		pr_debug("%s: new_pt %s size %llx\n",
			 __func__, pt->name, pt->size);
		if (!strcmp(pt->name, "BMTPOOL"))
			break;
		pt++;
	}

	if (pt_changed)
		ret = nandx_pmt_update();
freepacket:
	mem_free(packet);

	return ret;
}

static long nandx_pmt_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
	struct pmt_handler *handler;
	long ret = 0;
	ulong version = PT_SIG;
	void __user *uarg = (void __user *)arg;

	pr_debug("%s: cmd %d\n", __func__, cmd);

	handler = nandx_get_pmt_handler();

	switch (cmd) {
	case PMT_READ:
		if (copy_to_user(uarg, handler->pmt,
				 sizeof(struct pt_resident) * PART_MAX_COUNT))
			ret = -EFAULT;
		break;
	case PMT_WRITE:
		ret = nandx_pmt_write(uarg);
		break;
	case PMT_VERSION:
		if (copy_to_user(uarg, &version, PT_SIG_SIZE))
			ret = -EFAULT;
		break;
	case PMT_UPDATE:
		if (copy_from_user(handler->pmt, uarg,
				   sizeof(struct pt_resident) *
				   PART_MAX_COUNT))
			ret = -EFAULT;
		if (ret == 0)
			ret = nandx_pmt_update();
		break;
	default:
		pr_err("%s: type %d invalid\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations nandx_pmt_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = nandx_pmt_ioctl,
	.open = nandx_pmt_open,
	.release = nandx_pmt_release,
};

static struct miscdevice nandx_pmt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pmt",
	.fops = &nandx_pmt_fops,
};

int get_part_num_nand(void)
{
	struct pmt_handler *handler = nandx_get_pmt_handler();

	return handler->part_num;
}
EXPORT_SYMBOL(get_part_num_nand);

int nandx_pmt_register(struct mtd_info *mtd)
{
	struct pmt_handler *handler;
	struct nandx_chip_info *info;
	struct mtd_partition *part;
	unsigned char *name;
	struct pt_resident *pt;
	u32 i, j, bmt_block;
	u64 offset, size;
	int ret;

	if (mtd == NULL)
		return -EINVAL;

	handler = nandx_get_pmt_handler();
	info = handler->info;
	pt = handler->pmt;
	if (pt == NULL)
		return -ENODEV;

	/* assume bmt is always at the last of NAND */
	bmt_block = nandx_calculate_bmt_num(info);

	part_name = mem_alloc(handler->part_num, MAX_PARTITION_NAME_LEN);
	if (part_name == NULL)
		return -ENOMEM;
	name = part_name;

	mtdpart = mem_alloc(handler->part_num, sizeof(struct mtd_partition));
	if (mtdpart == NULL)
		return -ENOMEM;
	part = mtdpart;

	mtd->eraseregions = mem_alloc(handler->part_num,
				      sizeof(struct mtd_erase_region_info));
	if (mtd->eraseregions == NULL)
		return -ENOMEM;
	mtderase = mtd->eraseregions;

	mtd->numeraseregions = 0;

	for (i = 0; i < handler->part_num; i++) {
		if (i == (handler->part_num - 1)) {
			offset = (u64)info->block_size *
			    (info->block_num - bmt_block);
			size = (u64)bmt_block * info->block_size;
		} else {
			offset = pt->offset;
			size = pt->size;
		}

		/* change partition name to lowercase */
		for (j = 0; j < MAX_PARTITION_NAME_LEN; j++) {
			if (pt->name[j] == 0)
				break;
			name[j] = tolower(pt->name[j]);
		}

#ifdef CONFIG_DUM_CHAR_V2
		/* setup PartInfo */
		PartInfo[i].name = name;
		PartInfo[i].type = NAND;
		PartInfo[i].start_address = offset;
		PartInfo[i].size = size;
#endif

		/* setup eraseregions */
		mtd->eraseregions[i].offset = offset;
		mtd->eraseregions[i].erasesize = mtd->erasesize;
		if (nandx_pmt_is_raw_partition(pt))
			mtd->eraseregions[i].erasesize /= info->wl_page_num;
		mtd->numeraseregions++;
		/* setup mtd partitions */
		part->name = name;
		if (!strcmp(pt->name, "SECCFG"))
			part->name = MTD_SECFG_STR;
		if (!strcmp(pt->name, "BOOTIMG"))
			part->name = MTD_BOOTIMG_STR;
		if (!strcmp(pt->name, "SEC_RO"))
			part->name = MTD_SECRO_STR;
		if (!strcmp(pt->name, "ANDROID"))
			part->name = MTD_ANDROID_STR;
		if (!strcmp(pt->name, "USRDATA"))
			part->name = MTD_USRDATA_STR;
		part->size = size;
		part->offset = offset;
		part->mask_flags = 0;

		pt++;
		part++;
		name += MAX_PARTITION_NAME_LEN;
	}

	/* mtd device register */
	ret = mtd_device_register(mtd, mtdpart, handler->part_num);
	if (ret)
		goto mem_free;

	/* pmt misc device register */
	ret = misc_register(&nandx_pmt_dev);
	if (ret)
		goto mem_free;

	return 0;

mem_free:
	mem_free(part_name);
	mem_free(mtdpart);
	mem_free(mtderase);

	return ret;
}

void nandx_pmt_unregister(void)
{
	mem_free(part_name);
	mem_free(mtdpart);
	mem_free(mtderase);
}

int get_data_partition_info(struct nand_ftl_partition_info *info,
			    struct mtk_nand_chip_info *cinfo)
{
	struct pmt_handler *handler;
	struct nandx_chip_info *dev;
	struct mtd_partition *part = mtdpart;
	u32 i;

	handler = nandx_get_pmt_handler();
	dev = handler->info;
	cinfo->block_type_bitmap = handler->block_bitmap;

	for (i = 0; i < handler->part_num; i++) {
		if (!strcmp(part->name, FTL_PARTITION_NAME)) {
			info->total_block = (u32)div_down(part->size,
							  dev->block_size);

			info->start_block = (u32)div_down(part->offset,
							  dev->block_size);

			info->slc_ratio = 8 /*handler->usr_slc_ratio */;

			return 0;
		}
		part++;
	}

	return -1;
}

static void update_block_bitmap(struct mtk_nand_chip_info *info, int num,
				u32 *blk)
{
	struct pmt_handler *handler;
	int i, byte, bit;

	handler = nandx_get_pmt_handler();

	for (i = 0; i < num; i++) {
		byte = *blk / 32;
		bit = *blk % 32;
		info->block_type_bitmap[byte] |= (1 << bit);
		blk++;
	}
}

int mntl_update_part_tab(struct mtk_nand_chip_info *info, int num, u32 *blk)
{
	int ret;

	if (num <= 0 || blk == NULL)
		return -EINVAL;

	update_block_bitmap(info, num, blk);

	ret = nandx_pmt_update();

	return ret;
}
