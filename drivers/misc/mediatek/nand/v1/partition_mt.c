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

/******************************************************************************
* partition_mt.c - MT6516 NAND partition management Driver
 *
* Copyright 2009-2010 MediaTek Co., Ltd.
 *
* DESCRIPTION:
*	This file provid the other drivers partition relative functions
 *
* modification history
* ----------------------------------------
* v1.0, 28 Feb 2011, mtk80134 written
* ----------------------------------------
******************************************************************************/
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>

#include <mach/mt_clkmgr.h>
#include "pmt.h"

#define PMT 1
#ifdef PMT

#if (defined(MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT))
static bool MLC_DEVICE = TRUE;
#else
static bool MLC_DEVICE = FALSE;
#endif

unsigned long long  partition_type_array[PART_MAX_COUNT];

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif

#ifndef NULL
  #define NULL  (0)
#endif

pt_resident new_part[PART_MAX_COUNT];
pt_resident lastest_part[PART_MAX_COUNT];
unsigned char part_name[PART_MAX_COUNT][MAX_PARTITION_NAME_LEN];
struct excel_info PartInfo[PART_MAX_COUNT];
struct mtd_partition g_pasStatic_Partition[PART_MAX_COUNT];
int part_num;
/* struct excel_info PartInfo[PART_MAX_COUNT]; */
#define MTD_SECFG_STR "seccnfg"
#define MTD_BOOTIMG_STR "boot"
#define MTD_ANDROID_STR "system"
#define MTD_SECRO_STR "secstatic"
#define MTD_USRDATA_STR "userdata"

int block_size;
int page_size;
pt_info pi;
u8 sig_buf[PT_SIG_SIZE];

DM_PARTITION_INFO_PACKET pmtctl;
struct mtd_partition g_exist_Partition[PART_MAX_COUNT];

/* #define LPAGE 2048 */
/* char page_buf[LPAGE+64]; */
/* char page_readbuf[LPAGE]; */
char *page_buf;
char *page_readbuf;

#define  PMT_MAGIC	'p'
#define PMT_READ		_IOW(PMT_MAGIC, 1, int)
#define PMT_WRITE		_IOW(PMT_MAGIC, 2, int)
#define PMT_VERSION	_IOW(PMT_MAGIC, 3, int)

#define PMT_POOL_SIZE	(2)

bool init_pmt_done = FALSE;

void get_part_tab_from_complier(void)
{
	pr_info("get_pt_from_complier\n");

	memcpy(&g_exist_Partition, &g_pasStatic_Partition, sizeof(struct mtd_partition) * PART_MAX_COUNT);

}

u64 part_get_startaddress(u64 byte_address, u32 *idx)
{
	int index = 0;

	if (TRUE == init_pmt_done) {
		while (index < part_num) {
			if (g_exist_Partition[index].offset > byte_address) {
				*idx = index-1;
				return g_exist_Partition[index-1].offset;
			}
			index++;
		}
	}

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if (byte_address > ((u64)gn_devinfo.totalsize << 10))
		MSG(INIT, "[NAND] Warning!!! address(0x%llx) beyonds the chip size(0x%llx)!!!\n"
			, byte_address, ((u64)gn_devinfo.totalsize << 10));

#endif
	*idx = part_num-1;
	return byte_address;
}

bool raw_partition(u32 index)
{
	if ((partition_type_array[index] == REGION_LOW_PAGE)
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	|| (partition_type_array[index] == REGION_SLC_MODE))
#else
	)
#endif
		return TRUE;
	return FALSE;
}

int find_empty_page_from_top(u64 start_addr, struct mtd_info *mtd)
{
	int page_offset;	/* , i; */
	u64 current_add;
#if (defined(MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT))
	int i;
	bool pptbl = TRUE;
#endif
	struct mtd_oob_ops ops_pt;
	struct erase_info ei;

	ei.mtd = mtd;
	ei.len = mtd->erasesize;
	ei.time = 1000;
	ei.retries = 2;
	ei.callback = NULL;

	ops_pt.datbuf = (uint8_t *) page_buf;
	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;
	memset(page_buf, 0xFF, page_size + mtd->oobsize);
	memset(page_readbuf, 0xFF, page_size);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& (gn_devinfo.tlcControl.normaltlc)) {
		pptbl = FALSE;
	}
#endif

#if (defined(MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT))
	for (page_offset = 0, i = 0;
		page_offset < (block_size / page_size);
		pptbl ? (page_offset = functArray[gn_devinfo.feature_set.ptbl_idx](i++)) : page_offset++) {
		current_add = start_addr + (page_offset * page_size);
		if (mtd->_read_oob(mtd, (loff_t) current_add, &ops_pt) != 0) {
			pr_info("find_emp read failed %llx\n", current_add);
			continue;
		} else {
			if (memcmp(page_readbuf, page_buf, page_size)
			|| memcmp(page_buf + page_size, page_readbuf, 32)) {
				continue;
			} else {
				pr_info("find_emp  at %x\n", page_offset);
				break;
			}

		}
	}
#else
	for (page_offset = 0; page_offset < (block_size / page_size); page_offset++) {
		current_add = start_addr + (page_offset * page_size);
		if (mtd->_read_oob(mtd, (loff_t) current_add, &ops_pt) != 0) {
			pr_info("find_emp read failed %llx\n", current_add);
			continue;
		} else {
			if (memcmp(page_readbuf, page_buf, page_size)
			|| memcmp(page_buf + page_size, page_readbuf, 32)) {
				continue;
			} else {
				pr_info("find_emp  at %x\n", page_offset);
				break;
			}

		}
	}
#endif

	pr_info("find_emp find empty at %x\n", page_offset);

	if (page_offset != 0x40) {
		pr_info("find_emp at  %x\n", page_offset);
		return page_offset;
	}
	pr_info("find_emp no empty\n");
	ei.addr =  start_addr;
	if (mtd->_erase(mtd, &ei) != 0) {
		pr_info("find_emp erase mirror failed %llx\n", start_addr);
		pi.mirror_pt_has_space = 0;
		return 0xFFFF;
	}
	return 0;
}

bool find_mirror_pt_from_bottom(u64 *start_addr, struct mtd_info *mtd)
{
	int mpt_locate, i;
	u64 mpt_start_addr;
	u64 current_start_addr = 0;
	u8 pmt_spare[4];
	struct mtd_oob_ops ops_pt;

	mpt_start_addr = ((mtd->size) + block_size);
	memset(page_buf, 0xFF, page_size + mtd->oobsize);

	ops_pt.datbuf = (uint8_t *) page_buf;
	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;
	pr_info("find_mirror find begain at %llx\n", mpt_start_addr);

	for (mpt_locate = ((block_size / page_size) - 1),
		i = ((block_size / page_size) - 1); mpt_locate >= 0; mpt_locate--) {
		memset(pmt_spare, 0xFF, PT_SIG_SIZE);

		current_start_addr = mpt_start_addr + mpt_locate * page_size;
		if (mtd->_read_oob(mtd, (loff_t) current_start_addr, &ops_pt) != 0)
			pr_info("find_mirror read  failed %llx %x\n", current_start_addr, mpt_locate);

		memcpy(pmt_spare, &page_buf[page_size], PT_SIG_SIZE);

		if (is_valid_mpt(page_buf)) {
			slc_ratio = *((u32 *)page_buf + 1);
			sys_slc_ratio = (slc_ratio >> 16)&0xFF;
			usr_slc_ratio = (slc_ratio)&0xFF;
			pr_warn("[k] slc ratio %d\n", slc_ratio);
			pi.sequencenumber = page_buf[PT_SIG_SIZE + page_size];
			pr_info("find_mirror find valid pt at %llx sq %x\n", current_start_addr, pi.sequencenumber);
			break;
		}
	}
	if (mpt_locate == -1) {
		pr_info("no valid mirror page\n");
		pi.sequencenumber = 0;
		return FALSE;
	}
	*start_addr = current_start_addr;
	return TRUE;
}

int load_exist_part_tab(u8 *buf)
{
	u64 pt_start_addr;
	u64 pt_cur_addr;
	int pt_locate, i;
	int reval = DM_ERR_OK;
	u64 mirror_address;
	struct mtd_oob_ops ops_pt;
	struct mtd_info *mtd;

	mtd = &host->mtd;


#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
	&& (gn_devinfo.tlcControl.normaltlc))
		block_size = gn_devinfo.blocksize * 1024 / 3;
	else
#endif
	{
		block_size = mtd->erasesize;
	}
	page_size = mtd->writesize;
	pt_start_addr = (mtd->size);
	pr_info("load_exist_part_tab %llx\n", pt_start_addr);
	ops_pt.datbuf = (uint8_t *) page_buf;
	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = (page_buf + page_size);
	ops_pt.ooboffs = 0;

	pr_info("ops_pt.len %lx\n", (unsigned long)ops_pt.len);
	if (mtd->_read_oob == NULL)
		pr_info("should not happpen\n");

	for (pt_locate = 0, i = 0; pt_locate < (block_size / page_size); pt_locate++) {
		pt_cur_addr = pt_start_addr + pt_locate * page_size;

		pr_warn("load_pt read pt 0x%llx\n", pt_cur_addr);

		if (mtd->_read_oob(mtd, (loff_t) pt_cur_addr, &ops_pt) != 0)
			pr_info("load_pt read pt failded: %llx\n", (u64) pt_cur_addr);

		if (is_valid_pt(page_buf)) {
			slc_ratio = *((u32 *)page_buf + 1);
			sys_slc_ratio = (slc_ratio >> 16)&0xFF;
			usr_slc_ratio = (slc_ratio)&0xFF;
			pr_warn("[k] slc ratio sys_slc_ratio %d usr_slc_ratio %d\n"
				, sys_slc_ratio, usr_slc_ratio);
			pi.sequencenumber = page_buf[PT_SIG_SIZE + page_size];
			pr_info("load_pt find valid pt at %llx sq %x\n", pt_start_addr, pi.sequencenumber);
			break;
		}
	}
	if (pt_locate == (block_size / page_size)) {
		pr_info("load_pt find pt failed\n");
		pi.pt_has_space = 0;

		if (!find_mirror_pt_from_bottom(&mirror_address, mtd)) {
			pr_info("First time download\n");
			reval = ERR_NO_EXIST;
			return reval;
		}
		mtd->_read_oob(mtd, (loff_t) mirror_address, &ops_pt);
	}
	memcpy(&lastest_part, &page_buf[PT_SIG_SIZE], sizeof(lastest_part));

	return reval;
}

static int pmt_open(struct inode *inode, struct file *filp)
{
	pr_info("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__, MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static int pmt_release(struct inode *inode, struct file *filp)
{
	pr_info("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__, MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	return 0;
}

static long pmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	ulong version = PT_SIG;
	void __user *uarg = (void __user *)arg;

	pr_info("PMT IOCTL: Enter\n");

	if (false == g_bInitDone) {
		pr_info("ERROR: NAND Flash Not initialized !!\n");
		ret = -EFAULT;
		goto exit;
	}

	switch (cmd) {
	case PMT_READ:
		pr_info("PMT IOCTL: PMT_READ\n");
	ret = read_pmt(uarg);
		break;
	case PMT_WRITE:
		pr_info("PMT IOCTL: PMT_WRITE\n");
		if (copy_from_user(&pmtctl, uarg, sizeof(DM_PARTITION_INFO_PACKET))) {
			ret = -EFAULT;
			goto exit;
		}
		new_part_tab((u8 *) &pmtctl, (struct mtd_info *)&host->mtd);
		update_part_tab((struct mtd_info *)&host->mtd);

		break;
	case PMT_VERSION:
			if (copy_to_user((void __user *)arg, &version, PT_SIG_SIZE))
				ret = -EFAULT;
			else
				ret = 0;
		break;
	default:
		ret = -EINVAL;
	}
exit:
	return ret;
}
static int read_pmt(void __user *arg)
{
	pr_warn("read_pmt\n");

	if (copy_to_user(arg, &lastest_part, sizeof(pt_resident)*PART_MAX_COUNT))
		return -EFAULT;
	return 0;
}

static const struct file_operations pmt_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pmt_ioctl,
	.open = pmt_open,
	.release = pmt_release,
};

static struct miscdevice pmt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pmt",
	.fops = &pmt_fops,
};

static int lowercase(int c)
{
	if ((c >= 'A') && (c <= 'Z'))
		c += 'a' - 'A';
	return c;
}

void construct_mtd_partition(struct mtd_info *mtd)
{
	int i, j;

	for (i = 0; i < PART_MAX_COUNT; i++) {
		if (!strcmp(lastest_part[i-1].name, "BMTPOOL"))
			break;
		for (j = 0; j < MAX_PARTITION_NAME_LEN; j++) {
			if (lastest_part[i].name[j] == 0)
				break;
			part_name[i][j] = lowercase(lastest_part[i].name[j]);
		}
		PartInfo[i].name = part_name[i];
		g_exist_Partition[i].name = part_name[i];
		if (!strcmp(lastest_part[i].name, "SECCFG"))
			g_exist_Partition[i].name = MTD_SECFG_STR;

		if (!strcmp(lastest_part[i].name, "BOOTIMG"))
			g_exist_Partition[i].name = MTD_BOOTIMG_STR;

		if (!strcmp(lastest_part[i].name, "SEC_RO"))
			g_exist_Partition[i].name = MTD_SECRO_STR;

		if (!strcmp(lastest_part[i].name, "ANDROID"))
			g_exist_Partition[i].name = MTD_ANDROID_STR;

		if (!strcmp(lastest_part[i].name, "USRDATA"))
			g_exist_Partition[i].name = MTD_USRDATA_STR;

		g_exist_Partition[i].size = (uint64_t) lastest_part[i].size;
		g_exist_Partition[i].offset = (uint64_t) lastest_part[i].offset;

		g_exist_Partition[i].mask_flags = lastest_part[i].mask_flags;

		PartInfo[i].type = NAND;
		PartInfo[i].start_address = lastest_part[i].offset;
		PartInfo[i].size = lastest_part[i].size;
		partition_type_array[i] = lastest_part[i].part_id;

		if (!strcmp(lastest_part[i].name, "BMTPOOL")) {
			g_exist_Partition[i].offset = mtd->size
				+ PMT_POOL_SIZE * (gn_devinfo.blocksize * 1024);
			partition_type_array[i] = REGION_LOW_PAGE;
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC) {
				if (gn_devinfo.tlcControl.normaltlc)
					partition_type_array[i] = REGION_SLC_MODE;
				else
					partition_type_array[i] = REGION_TLC_MODE;
			}
#endif
		}
#if 0
		pr_warn("partition %s %s offset %llx size %llx %llx\n",
		lastest_part[i].name, PartInfo[i].name, g_exist_Partition[i].offset,
		g_exist_Partition[i].size, partition_type_array[i]);
#endif
#if 1
		if (MLC_DEVICE == TRUE) {
			mtd->eraseregions[i].offset = lastest_part[i].offset;
			mtd->eraseregions[i].erasesize = mtd->erasesize;

			if (partition_type_array[i] == REGION_LOW_PAGE)
				mtd->eraseregions[i].erasesize = mtd->erasesize/2;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
			if (partition_type_array[i] == REGION_SLC_MODE)
				mtd->eraseregions[i].erasesize = mtd->erasesize/3;

#endif

			mtd->numeraseregions++;
		}
#endif
	}
	part_num = i;
	g_exist_Partition[i-1].size = MTDPART_SIZ_FULL;
}

void part_init_pmt(struct mtd_info *mtd, u8 *buf)
{
	int retval = 0;
	int i = 0;
	int err = 0;

	pr_info("part_init_pmt  %s\n", __TIME__);
	page_buf = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	page_readbuf = kzalloc(mtd->writesize, GFP_KERNEL);

	memset(&pi, 0xFF, sizeof(pi));
	memset(&lastest_part, 0, PART_MAX_COUNT * sizeof(pt_resident));
	retval = load_exist_part_tab(buf);

	if (retval == ERR_NO_EXIST) {
		pr_info("%s no pt\n", __func__);
		get_part_tab_from_complier();
		if (MLC_DEVICE == TRUE)
			mtd->numeraseregions = 0;
		for (i = 0; i < part_num; i++) {
#if 1
			if (MLC_DEVICE == TRUE) {
				mtd->eraseregions[i].offset = lastest_part[i].offset;
				mtd->eraseregions[i].erasesize = mtd->erasesize;
				if (partition_type_array[i] == REGION_LOW_PAGE)
					mtd->eraseregions[i].erasesize = mtd->erasesize/2;

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
				if (partition_type_array[i] == REGION_SLC_MODE)
					mtd->eraseregions[i].erasesize = mtd->erasesize/3;

#endif

				mtd->numeraseregions++;
			}
#endif
		}
	} else {
		pr_info("Find pt or mpt\n");
		if (MLC_DEVICE == TRUE)
			mtd->numeraseregions = 0;
		construct_mtd_partition(mtd);
	}
	init_pmt_done = TRUE;

	pr_info(": register NAND PMT device ...\n");
#ifndef MTK_EMMC_SUPPORT

	err = misc_register(&pmt_dev);
	if (unlikely(err))
		pr_info("PMT failed to register device!\n");

#endif
}

int new_part_tab(u8 *buf, struct mtd_info *mtd)
{
	DM_PARTITION_INFO_PACKET *dm_part = (DM_PARTITION_INFO_PACKET *) buf;
	int part_num, change_index, i = 0;
	int retval;
	int pageoffset;
	u64 start_addr = (u64)((mtd->size) + block_size);
	u64 current_addr = 0;
	u64 temp_value;
	struct mtd_oob_ops ops_pt;

	pi.pt_changed = 0;
	pi.tool_or_sd_update = 2;

	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;
#if 1
	for (part_num = 0; part_num < PART_MAX_COUNT; part_num++) {
		memcpy(new_part[part_num].name, dm_part->part_info[part_num].part_name, MAX_PARTITION_NAME_LEN);
		new_part[part_num].offset = dm_part->part_info[part_num].start_addr;
		new_part[part_num].size = dm_part->part_info[part_num].part_len;
		new_part[part_num].mask_flags = 0;
		pr_info("new_pt %s size %llx\n", new_part[part_num].name, new_part[part_num].size);
		if (dm_part->part_info[part_num].part_len == 0) {
			pr_info("new_pt last %x\n", part_num);
			break;
		}
	}
#endif
	for (change_index = 0; change_index <= part_num; change_index++) {
		if ((new_part[change_index].size != lastest_part[change_index].size)
		|| (new_part[change_index].offset != lastest_part[change_index].offset)) {
			pr_info("new_pt %x size changed from %llx to %llx\n",
			change_index, lastest_part[change_index].size, new_part[change_index].size);
			pi.pt_changed = 1;
			break;
		}
	}

	if (pi.pt_changed == 1) {
		for (i = change_index; i <= part_num; i++) {

			if (dm_part->part_info[i].dl_selected == 0 && dm_part->part_info[i].part_visibility == 1) {
				pr_info("Full download is need %x\n", i);
				retval = DM_ERR_NO_VALID_TABLE;
				return retval;
			}
		}

		pageoffset = find_empty_page_from_top(start_addr, mtd);
		temp_value = (u64)slc_ratio;
		temp_value = temp_value << 32;
		temp_value = MPT_SIG | temp_value;
		memset(page_buf, 0xFF, page_size + 64);
		*(u64 *)sig_buf = temp_value;
		memcpy(page_buf, &sig_buf, PT_SIG_SIZE);
		memcpy(&page_buf[PT_SIG_SIZE], &new_part[0], sizeof(new_part));
		memcpy(&page_buf[page_size], &sig_buf, PT_SIG_SIZE);
		pi.sequencenumber += 1;
		memcpy(&page_buf[page_size + PT_SIG_SIZE], &pi, PT_SIG_SIZE);

		if (pageoffset != 0xFFFF) {
			if ((pageoffset % 2) != 0) {
				pr_info("new_pt mirror block may destroy last time%x\n", pageoffset);
				pageoffset += 1;
			}
			for (i = 0; i < 2; i++) {
				current_addr = start_addr + (pageoffset + i) * page_size;
				ops_pt.datbuf = (uint8_t *) page_buf;
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_info("new_pt write m first page failed %llx\n", current_addr);
				} else {
					pr_info("new_pt write mirror at %llx\n", current_addr);
					ops_pt.datbuf = (uint8_t *) page_readbuf;
					if ((mtd->_read_oob(mtd, (loff_t) current_addr, &ops_pt) != 0)
						|| memcmp(page_buf, page_readbuf, page_size)) {
						pr_info("new_pt read or verify first mirror page failed %llx\n",
							current_addr);
						ops_pt.datbuf = (uint8_t *) page_buf;
						memset(page_buf, 0, PT_SIG_SIZE);
					} else {
						pr_info("new_pt write mirror ok %x\n", i);
						pi.mirror_pt_dl = 1;
					}
				}
			}
		}
	} else {
		pr_info("new_part_tab no pt change %x\n", i);
	}

	retval = DM_ERR_OK;
	return retval;
}

int update_part_tab(struct mtd_info *mtd)
{
	int retval = 0;
	int retry_w;
	int retry_r;
	u64 start_addr = (u64)(mtd->size);
	u64 current_addr = 0;
	u64 temp_value;
	struct erase_info ei;
	struct mtd_oob_ops ops_pt;

	memset(page_buf, 0xFF, page_size + 64);

	ei.mtd = mtd;
	ei.len =  mtd->erasesize;
	ei.time = 1000;
	ei.retries = 2;
	ei.callback = NULL;

	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;

	if ((pi.pt_changed == 1 || pi.pt_has_space == 0) && pi.tool_or_sd_update == 2) {
		pr_info("update_pt pt changes\n");

		ei.addr = start_addr;
		if (mtd->_erase(mtd, &ei) != 0) {
			pr_info("update_pt erase failed %llx\n", start_addr);
			if (pi.mirror_pt_dl == 0)
				retval = DM_ERR_NO_SPACE_FOUND;
			return retval;
		}

		for (retry_r = 0; retry_r < RETRY_TIMES; retry_r++) {
			for (retry_w = 0; retry_w < RETRY_TIMES; retry_w++) {
				current_addr = start_addr + (retry_w + retry_r * RETRY_TIMES) * page_size;
				temp_value = (u64)slc_ratio;
				temp_value = temp_value << 32;
				temp_value = temp_value | PT_SIG;
				*(u64 *)sig_buf = temp_value;
				memcpy(page_buf, &sig_buf, PT_SIG_SIZE);
				memcpy(&page_buf[PT_SIG_SIZE], &new_part[0], sizeof(new_part));
				memcpy(&page_buf[page_size], &sig_buf, PT_SIG_SIZE);
				memcpy(&page_buf[page_size + PT_SIG_SIZE], &pi, PT_SIG_SIZE);

				ops_pt.datbuf = (uint8_t *) page_buf;
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_info("update_pt write failed %x\n", retry_w);
					memset(page_buf, 0, PT_SIG_SIZE);
					if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
						pr_info("write error mark failed\n");
						continue;
					}
				} else {
					pr_info("write pt success %llx %x\n", current_addr, retry_w);
					break;
				}
			}
			if (retry_w == RETRY_TIMES) {
				pr_info("update_pt retry w failed\n");
				if (pi.mirror_pt_dl == 0) {
					retval = DM_ERR_NO_SPACE_FOUND;
					return retval;
				} else {
					return DM_ERR_OK;
				}
			}
			current_addr = (start_addr + (((retry_w) + retry_r * RETRY_TIMES) * page_size));
			ops_pt.datbuf = (uint8_t *) page_readbuf;
			if ((mtd->_read_oob(mtd, (loff_t) current_addr, &ops_pt) != 0)
			|| memcmp(page_buf, page_readbuf, page_size)) {
				pr_info("v or r failed %x\n", retry_r);
				memset(page_buf, 0, PT_SIG_SIZE);
				ops_pt.datbuf = (uint8_t *) page_buf;
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_info("read error mark failed\n");
					continue;
				}

			} else {
				pr_info("update_pt r&v ok %llx\n", current_addr);
				break;
			}
		}
	} else {
		pr_info("update_pt no change\n");
	}
	return DM_ERR_OK;
}

int get_part_num_nand(void)
{
	return part_num;
}
EXPORT_SYMBOL(get_part_num_nand);

#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
u64 OFFSET(u32 block)
{
	u64 offset;
	u32 idx;
	u64 start_address;
	bool raw_part;
	u64 real_address = (u64)block * (gn_devinfo.blocksize * 1024);
	u32 total_blk_num;
	u32 slc_blk_num;
	u64 temp;

	start_address = part_get_startaddress(real_address, &idx);
	if (raw_partition(idx))
		raw_part = TRUE;
	else
		raw_part = FALSE;

	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& gn_devinfo.tlcControl.normaltlc) {
		if (raw_part) {
			offset = start_address + (real_address - start_address);
			do_div(offset, 3);
		} else {
			temp = (g_exist_Partition[idx + 1].offset - g_exist_Partition[idx].offset);
			do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
			total_blk_num = temp;
			if (!strcmp(lastest_part[idx].name, "ANDROID"))
				slc_blk_num = total_blk_num * sys_slc_ratio / 100;
			else {
				total_blk_num -= 2;
				slc_blk_num = total_blk_num * usr_slc_ratio / 100;
			}
			if (slc_blk_num % 3)
				slc_blk_num += (3 - (slc_blk_num % 3));
			if (block < (system_block_count - PMT_POOL_SIZE - slc_blk_num))
				offset = (((u64)block) * (gn_devinfo.blocksize * 1024));
			else
				offset =
					(system_block_count - PMT_POOL_SIZE - slc_blk_num)
					* (gn_devinfo.blocksize * 1024)
					+ (block - (system_block_count - PMT_POOL_SIZE - slc_blk_num))
					* (gn_devinfo.blocksize * 1024) / 3;
		}
	} else {
		offset = (((u64)block) * (gn_devinfo.blocksize * 1024));
	}

	return offset;
}

void mtk_slc_blk_addr(u64 addr, u32 *blk_num, u32 *page_in_block)
{
	u64 start_address;
	u32 idx;
	u32 total_blk_num;
	u32 slc_blk_num;
	u64 offset;
	u32 block_size = (gn_devinfo.blocksize * 1024);
	u64 temp, temp1;

	start_address = part_get_startaddress(addr, &idx);
	if (raw_partition(idx)) {
		temp = start_address;
		temp1  = addr-start_address;
		do_div(temp, (block_size & 0xFFFFFFFF));
		do_div(temp1, ((block_size / 3) & 0xFFFFFFFF));
		*blk_num = (u32)((u32)temp + (u32)temp1);
		temp1  = addr-start_address;
		do_div(temp1, (gn_devinfo.pagesize & 0xFFFFFFFF));
		*page_in_block = ((u32)temp1 % ((block_size/gn_devinfo.pagesize)/3));
		*page_in_block *= 3;
	} else {
		if ((addr < g_exist_Partition[idx + 1].offset)
		&& (addr >= (g_exist_Partition[idx + 1].offset - PMT_POOL_SIZE * block_size))) {
			temp = addr;
			do_div(temp, (block_size & 0xFFFFFFFF));
			*blk_num = (u32)temp;
			temp1 = addr;
			do_div(temp1, (gn_devinfo.pagesize & 0xFFFFFFFF));
			*page_in_block = ((u32)temp1 % ((block_size/gn_devinfo.pagesize)/3));
			*page_in_block *= 3;
		} else {
			temp = (g_exist_Partition[idx + 1].offset - g_exist_Partition[idx].offset);
			do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
			total_blk_num = temp;
			if (!strcmp(lastest_part[idx].name, "ANDROID"))
				slc_blk_num = total_blk_num * sys_slc_ratio / 100;
			else {
				total_blk_num -= 2;
				slc_blk_num = total_blk_num * usr_slc_ratio / 100;
			}
			if (slc_blk_num % 3)
				slc_blk_num += (3 - (slc_blk_num % 3));
			offset = start_address + (u64)(gn_devinfo.blocksize * 1024) * (total_blk_num - slc_blk_num);

			if (offset <= addr) {
				temp = offset;
				temp1  = addr-offset;
				do_div(temp, (block_size & 0xFFFFFFFF));
				do_div(temp1, ((block_size / 3) & 0xFFFFFFFF));
				*blk_num = (u32)((u32)temp + (u32)temp1);
				temp1  = addr-offset;
				do_div(temp1, (gn_devinfo.pagesize & 0xFFFFFFFF));
				*page_in_block = ((u32)temp1 % ((block_size/gn_devinfo.pagesize)/3));
				*page_in_block *= 3;
			} else {
				pr_warn("[xiaolei] error :this is not slc mode block\n");
				while (1)
					;
			}
		}
	}
}

bool mtk_block_istlc(u64 addr)
{
	u64 start_address;
	u32 idx;
	u32 total_blk_num;
	u32 slc_blk_num;
	u64 offset;
	u64 temp;

	start_address = part_get_startaddress(addr, &idx);
	if (raw_partition(idx))
		return FALSE;

	if (gn_devinfo.tlcControl.normaltlc) {
		temp = (g_exist_Partition[idx + 1].offset - g_exist_Partition[idx].offset);
		do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
		total_blk_num = temp;
		if (!strcmp(lastest_part[idx].name, "ANDROID"))
			slc_blk_num = total_blk_num * sys_slc_ratio / 100;
		else {
			total_blk_num -= 2;
			slc_blk_num = total_blk_num * usr_slc_ratio / 100;
		}
		if (slc_blk_num % 3)
			slc_blk_num += (3 - (slc_blk_num % 3));

		offset = start_address + (u64)(gn_devinfo.blocksize * 1024) * (total_blk_num - slc_blk_num);

		if (offset <= addr)
			return FALSE;
		return TRUE;
	}
	return TRUE;
}
EXPORT_SYMBOL(mtk_block_istlc);

void mtk_pmt_reset(void)
{
	u32 i;

	if ((gn_devinfo.NAND_FLASH_TYPE == NAND_FLASH_TLC)
		&& gn_devinfo.tlcControl.normaltlc) {
		for (i = 0; i < PART_MAX_COUNT; i++) {
			partition_type_array[i] = REGION_SLC_MODE;
			g_exist_Partition[i].offset = 0;
		}
	}
}

bool mtk_nand_IsBMTPOOL(loff_t logical_address)
{
	loff_t start_address;
	u32 idx;

	start_address = part_get_startaddress(logical_address, &idx);

	if ((!strcmp(part_name[idx], "bmtpool")) || (!init_pmt_done))
		return TRUE;
	else
		return FALSE;
}
#else
void mtk_slc_blk_addr(u64 addr, u32 *blk_num, u32 *page_in_block)
{
	u64 start_address;
	u32 idx;
	u32 total_blk_num;
	u32 slc_blk_num;
	u64 offset;
	u32 block_size = (gn_devinfo.blocksize * 1024);
	u64 temp, temp1;

	start_address = part_get_startaddress(addr, &idx);
	temp = (g_exist_Partition[idx + 1].offset - g_exist_Partition[idx].offset);
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	total_blk_num = temp;
	if (!strcmp(lastest_part[idx].name, "ANDROID"))
		slc_blk_num = total_blk_num * sys_slc_ratio / 100;
	else {
		total_blk_num -= 2;
		slc_blk_num = total_blk_num * usr_slc_ratio / 100;
	}
	if (slc_blk_num % 2)
		slc_blk_num += (2 - (slc_blk_num % 2));
	offset = start_address + (u64)(gn_devinfo.blocksize * 1024) * (total_blk_num - slc_blk_num);
	if (offset <= addr) {
		temp = offset;
		temp1  = addr-offset;
		do_div(temp, (block_size & 0xFFFFFFFF));
		do_div(temp1, ((block_size / 2) & 0xFFFFFFFF));
		*blk_num = (u32)((u32)temp + (u32)temp1);
		temp1  = addr-offset;
		do_div(temp1, (gn_devinfo.pagesize & 0xFFFFFFFF));
		*page_in_block = ((u32)temp1 % ((block_size/gn_devinfo.pagesize)/2));
	} else {
		pr_warn("[xiaolei] error :this is not slc mode block\n");
		while (1)
			;
	}
}

bool mtk_block_istlc(u64 addr)
{
	u64 start_address;
	u32 idx;
	u32 total_blk_num;
	u32 slc_blk_num;
	u64 offset;
	u64 temp;

	start_address = part_get_startaddress(addr, &idx);
	if (raw_partition(idx))
		return FALSE;

	temp = (g_exist_Partition[idx + 1].offset - g_exist_Partition[idx].offset);
	do_div(temp, ((gn_devinfo.blocksize * 1024) & 0xFFFFFFFF));
	total_blk_num = temp;
	if (!strcmp(lastest_part[idx].name, "ANDROID"))
		slc_blk_num = total_blk_num * sys_slc_ratio / 100;
	else {
		total_blk_num -= 2;
		slc_blk_num = total_blk_num * usr_slc_ratio / 100;
	}
	if (slc_blk_num % 2)
		slc_blk_num += (2 - (slc_blk_num % 2));

	offset = start_address + (u64)(gn_devinfo.blocksize * 1024) * (total_blk_num - slc_blk_num);

	if (offset <= addr)
		return FALSE;
	return TRUE;
}
EXPORT_SYMBOL(mtk_block_istlc);

#endif

#endif
