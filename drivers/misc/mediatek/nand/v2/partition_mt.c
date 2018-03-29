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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>

/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_clkmgr.h> */
/* #include <mach/mtk_nand.h> */
#include "nand_device_define.h"
#include "mtk_nand.h"
/* #include "board-custom.h" */
/* #include <mach/pmt.h> */
#include "pmt.h"
/* #include "partition.h" */
/* #include <mach/board.h> */

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif


#ifdef PMT
unsigned long long partition_type_array[PART_MAX_COUNT];

pt_resident new_part[PART_MAX_COUNT];
pt_resident lastest_part[PART_MAX_COUNT];
unsigned char part_name[PART_MAX_COUNT][MAX_PARTITION_NAME_LEN];
struct mtd_partition g_pasStatic_Partition[PART_MAX_COUNT];
int part_num = 1;
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
/* not support add new partition automatically. */

/* extern struct mtd_partition g_pasStatic_Partition[]; */
/* extern int part_num; */

DM_PARTITION_INFO_PACKET pmtctl;
struct mtd_partition g_exist_Partition[PART_MAX_COUNT];

/* #define LPAGE 2048 */
/* char page_buf[LPAGE+64]; */
/* char page_readbuf[LPAGE]; */
char *page_buf;
char *page_readbuf;

#define  PMT_MAGIC	 'p'
#define PMT_READ		_IOW(PMT_MAGIC, 1, int)
#define PMT_WRITE		_IOW(PMT_MAGIC, 2, int)
#define PMT_VERSION	_IOW(PMT_MAGIC, 3, int)


#if 0
struct pmt_config {
	int (*read_pmt)(char *buf);
	int (*write_pmt)(char *buf);
};
static struct pmt_config g_partition_fuc;
#endif
static bool init_pmt_done = FALSE;
static int read_pmt(void __user *arg);
void get_part_tab_from_complier(void)
{
	/* int index=0; */
	pr_debug("get_pt_from_complier\n");

	memcpy(&g_exist_Partition, &g_pasStatic_Partition,
	       sizeof(struct mtd_partition) * PART_MAX_COUNT);

#if 0
	while (g_pasStatic_Partition[index].size != MTDPART_SIZ_FULL) {	/* the last partition sizefull ==0 */
		memcpy(lastest_part[index].name, g_pasStatic_Partition[index].name,
		       MAX_PARTITION_NAME_LEN);
		lastest_part[index].size = g_pasStatic_Partition[index].size;
		lastest_part[index].offset = g_pasStatic_Partition[index].offset;
		/* this flag in kernel should be fufilled even though in flash is 0. */
		lastest_part[index].mask_flags = g_pasStatic_Partition[index].mask_flags;
		pr_debug("get_ptr  %s %llx\n", lastest_part[index].name,
		       lastest_part[index].offset);
		index++;
	}
	/* get last partition info */
	memcpy(lastest_part[index].name, g_pasStatic_Partition[index].name, MAX_PARTITION_NAME_LEN);
	lastest_part[index].size = g_pasStatic_Partition[index].size;
	lastest_part[index].offset = g_pasStatic_Partition[index].offset;
	/* this flag in kernel should be fufilled even though in flash is 0. */
	lastest_part[index].mask_flags = g_pasStatic_Partition[index].mask_flags;
	pr_debug("get_ptr  %s %llx\n", lastest_part[index].name,
	       lastest_part[index].offset);
#endif
}

u64 part_get_startaddress(u64 byte_address, u32 *idx)
{
	int index = 0;

	if (TRUE == init_pmt_done) {
		while (index < part_num) {
			/* MSG(INIT, "g_exist_Partition[%d].offset %x\n",index, g_exist_Partition[index].offset); */
			if (g_exist_Partition[index].offset > byte_address) {
				/* MSG(INIT, "find g_exist_Partition[%d].offset %x\n",
					index-1, g_exist_Partition[index-1].offset); */
				*idx = index - 1;
				return g_exist_Partition[index - 1].offset;
			}
			index++;
		}
	}
	*idx = part_num - 1;
	return byte_address;
}

bool raw_partition(u32 index)
{
	if (partition_type_array[index] == REGION_LOW_PAGE)
		return TRUE;

	return FALSE;
}

int find_empty_page_from_top(u64 start_addr, struct mtd_info *mtd)
{
	int page_offset;	/* ,i; */
	u64 current_add;
#if defined(MTK_MLC_NAND_SUPPORT)
	int i;
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
	/* mt6577_nand_erase(start_addr); //for test */
#if defined(MTK_MLC_NAND_SUPPORT)
	for (page_offset = 0, i = 0; page_offset < (block_size / page_size);
	     page_offset = functArray[devinfo.feature_set.ptbl_idx] (i++)) {
#else
	for (page_offset = 0; page_offset < (block_size / page_size); page_offset++) {
#endif
		current_add = start_addr + (page_offset * page_size);
		if (mtd->_read_oob(mtd, (loff_t) current_add, &ops_pt) != 0) {
			pr_notice("find_emp read failed %llx\n", current_add);
			continue;
		} else {
			if (memcmp(page_readbuf, page_buf, page_size)
			    || memcmp(page_buf + page_size, page_readbuf, 32)) {
				continue;
			} else {
				pr_debug("find_emp  at %x\n", page_offset);
				break;
			}

		}
	}
	pr_debug("find_emp find empty at %x\n", page_offset);

	/* i=(0x40); */
	/* pr_debug("test code %x\n",i); */
	/* page_offset = 0x40; */
	if (page_offset != 0x40) {
		pr_debug("find_emp at  %x\n", page_offset);
		return page_offset;
	}
	pr_debug("find_emp no empty\n");
	ei.addr = start_addr;
	if (mtd->_erase(mtd, &ei) != 0) {	/* no good block for used in replace pool */
		pr_notice("find_emp erase mirror failed %llx\n", start_addr);
		pi.mirror_pt_has_space = 0;
		return 0xFFFF;
	} else {
		return 0;	/* the first page is empty */
	}
}

bool find_mirror_pt_from_bottom(u64 *start_addr, struct mtd_info *mtd)
{
	int mpt_locate, i;
	u64 mpt_start_addr;
	u64 current_start_addr = 0;
	u8 pmt_spare[4];
	struct mtd_oob_ops ops_pt;

	mpt_start_addr = ((mtd->size) + block_size);
	/* mpt_start_addr=MPT_LOCATION*block_size-page_size; */
	memset(page_buf, 0xFF, page_size + mtd->oobsize);

	ops_pt.datbuf = (uint8_t *) page_buf;
	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;
	pr_debug("find_mirror find begain at %llx\n", mpt_start_addr);

	for (mpt_locate = ((block_size / page_size) - 1), i = ((block_size / page_size) - 1);
		mpt_locate >= 0; mpt_locate--) {	/* mpt_locate--) */
		memset(pmt_spare, 0xFF, PT_SIG_SIZE);

		current_start_addr = mpt_start_addr + mpt_locate * page_size;
		if (mtd->_read_oob(mtd, (loff_t) current_start_addr, &ops_pt) != 0) {
			pr_notice("find_mirror read  failed %llx %x\n", current_start_addr,
			       mpt_locate);
		}
		memcpy(pmt_spare, &page_buf[page_size], PT_SIG_SIZE);	/* auto do need skip bad block */
		/* need enhance must be the larget sequnce number */
#if 0
		{
			int i;

			for (i = 0; i < 8; i++)
				pr_debug("%x %x\n", page_buf[i], page_buf[2048 + i]);
		}
#endif
		if (is_valid_mpt(page_buf) && is_valid_mpt(pmt_spare)) {
			/* if no pt, pt.has space is 0; */
			pi.sequencenumber = page_buf[PT_SIG_SIZE + page_size];
			pr_debug("find_mirror find valid pt at %llx sq %x\n",
			       current_start_addr, pi.sequencenumber);
			break;
		}
		continue;
	}
	if (mpt_locate == -1) {
		pr_notice("no valid mirror page\n");
		pi.sequencenumber = 0;
		return FALSE;
	}
	*start_addr = current_start_addr;
	return TRUE;
}

/* int load_exist_part_tab(u8 *buf,struct mtd_info *mtd) */
int load_exist_part_tab(u8 *buf)
{
	u64 pt_start_addr;
	u64 pt_cur_addr;
	int pt_locate, i;
	int reval = DM_ERR_OK;
	u64 mirror_address;

	/* u8 pmt_spare[PT_SIG_SIZE]; */
	struct mtd_oob_ops ops_pt;
	struct mtd_info *mtd;

	mtd = &host->mtd;


	block_size = mtd->erasesize;	/* devinfo.blocksize*1024; */
	page_size = mtd->writesize;	/* devinfo.pagesize; */
	/* if(host->hw->nand_sec_shift == 10) //MLC */
	/* block_size = block_size >> 1; */
	pt_start_addr = (mtd->size);
	/* pt_start_addr=PT_LOCATION*block_size; */
	pr_debug("load_exist_part_tab %llx\n", pt_start_addr);
	ops_pt.datbuf = (uint8_t *) page_buf;
	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = (page_buf + page_size);
	ops_pt.ooboffs = 0;

	pr_debug("ops_pt.len %lx\n", (unsigned long)ops_pt.len);
	if (mtd->_read_oob == NULL)
		pr_debug("should not happpen\n");

	for (pt_locate = 0, i = 0; pt_locate < (block_size / page_size); pt_locate++) {
		pt_cur_addr = pt_start_addr + pt_locate * page_size;
		/* memset(pmt_spare,0xFF,PT_SIG_SIZE); */

		/* pr_debug("load_pt read pt %x\n",pt_cur_addr); */

		if (mtd->_read_oob(mtd, (loff_t) pt_cur_addr, &ops_pt) != 0)
			pr_info("load_pt read pt failded: %llx\n", (u64) pt_cur_addr);
#if 0
		{
			int i;

			for (i = 0; i < 8; i++) {
				pr_debug("%x %x\n", *(page_buf + i),
				       *(page_buf + 2048 + i));
			}

		}
#endif
		/* memcpy(pmt_spare,&page_buf[LPAGE] ,PT_SIG_SIZE); //do not need skip bad block flag */
		if (is_valid_pt(page_buf) && is_valid_pt(page_buf + mtd->writesize)) {
			pi.sequencenumber = page_buf[PT_SIG_SIZE + page_size];
			pr_info("load_pt find valid pt at %llx sq %x\n", pt_start_addr,
			       pi.sequencenumber);
			break;
		}
		continue;
	}
	/* for test */
	/* pt_locate=(block_size/page_size); */
	if (pt_locate == (block_size / page_size)) {
		/* first download or download is not compelte after erase or can not download last time */
		pr_info("load_pt find pt failed\n");
		pi.pt_has_space = 0;	/* or before download pt power lost */

		if (!find_mirror_pt_from_bottom(&mirror_address, mtd)) {
			pr_info("First time download\n");
			reval = ERR_NO_EXIST;
			return reval;
		}
		/* used the last valid mirror pt, at lease one is valid. */
		mtd->_read_oob(mtd, (loff_t) mirror_address, &ops_pt);
	}
	memcpy(&lastest_part, &page_buf[PT_SIG_SIZE], sizeof(lastest_part));

	return reval;
}

static int pmt_open(struct inode *inode, struct file *filp)
{
	pr_debug("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__, MAJOR(inode->i_rdev),
	       MINOR(inode->i_rdev));
	/* filp->private_data = (int*); */
	return 0;
}

static int pmt_release(struct inode *inode, struct file *filp)
{
	pr_debug("[%s]:(MAJOR)%d:(MINOR)%d\n", __func__, MAJOR(inode->i_rdev),
	       MINOR(inode->i_rdev));
	return 0;
}

static long pmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;		/* , i=0; */
	ulong version = PT_SIG;

	void __user *uarg = (void __user *)arg;

	pr_debug("PMT IOCTL: Enter\n");


	if (false == g_bInitDone) {
		pr_err("ERROR: NAND Flash Not initialized !!\n");
		ret = -EFAULT;
		goto exit;
	}

	switch (cmd) {
	case PMT_READ:
		pr_debug("PMT IOCTL: PMT_READ\n");
		ret = read_pmt(uarg);
		break;
	case PMT_WRITE:
		pr_debug("PMT IOCTL: PMT_WRITE\n");
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
	pr_debug("read_pmt\n");

	if (copy_to_user(arg, &lastest_part, sizeof(pt_resident) * PART_MAX_COUNT))
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
		/* if((lastest_part[i].size == 0) && (strcmp(lastest_part[i].name,"BMTPOOL"))) */
		/* break; */
		if (!strcmp(lastest_part[i - 1].name, "BMTPOOL"))
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

		g_exist_Partition[i].size = (uint64_t) lastest_part[i].size;	/* mtd partition */
		g_exist_Partition[i].offset = (uint64_t) lastest_part[i].offset;

		g_exist_Partition[i].mask_flags = lastest_part[i].mask_flags;

		/* PartInfo[i].name = part_name[i];  //dumchar */
		PartInfo[i].type = NAND;
		PartInfo[i].start_address = lastest_part[i].offset;
		PartInfo[i].size = lastest_part[i].size;
		partition_type_array[i] = lastest_part[i].part_id;
		pr_debug("partition %s %s size %llx\n", lastest_part[i].name, PartInfo[i].name,
		       g_exist_Partition[i].offset);

#if 1
		if (MLC_DEVICE == TRUE) {
			mtd->eraseregions[i].offset = lastest_part[i].offset;
			mtd->eraseregions[i].erasesize = mtd->erasesize;

			if (partition_type_array[i] == REGION_LOW_PAGE)
				mtd->eraseregions[i].erasesize = mtd->erasesize / 2;

			mtd->numeraseregions++;
		}
#endif
	}
	part_num = i;
	g_exist_Partition[i - 1].size = MTDPART_SIZ_FULL;
}

void part_init_pmt(struct mtd_info *mtd, u8 *buf)
{
	/* struct mtd_partition *part; */
	/* u64 lastblk; */
	int retval = 0;
	int i = 0;
	int err = 0;

	pr_debug("part_init_pmt\n");
	page_buf = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	page_readbuf = kzalloc(mtd->writesize, GFP_KERNEL);
#if 0
	part = &g_pasStatic_Partition[0];
	lastblk = part->offset + part->size;
	pr_debug("offset  %llx part->size %llx %s\n", part->offset, part->size, part->name);

	while (1) {
		part++;
		if (part->offset == MTDPART_OFS_APPEND)
			part->offset = lastblk;
		lastblk = part->offset + part->size;
		pr_debug("mt_part_init_pmt %llx\n", part->offset);
		if (part->size == 0) {	/* //the last partition sizefull ==0 */
			break;
		}
	}
#endif

	memset(&pi, 0xFF, sizeof(pi));
	memset(&lastest_part, 0, PART_MAX_COUNT * sizeof(pt_resident));
	retval = load_exist_part_tab(buf);

	if (retval == ERR_NO_EXIST)	{/* first run preloader before dowload */
		/* and valid mirror last download or first download */
		pr_notice("%s no pt\n", __func__);
		get_part_tab_from_complier();	/* get from complier */
		if (MLC_DEVICE == TRUE)
			mtd->numeraseregions = 0;
		for (i = 0; i < part_num; i++) {
#if 1
			if (MLC_DEVICE == TRUE) {
				mtd->eraseregions[i].offset = lastest_part[i].offset;
				mtd->eraseregions[i].erasesize = mtd->erasesize;
				if (partition_type_array[i] == REGION_LOW_PAGE)
					mtd->eraseregions[i].erasesize = mtd->erasesize / 2;

				mtd->numeraseregions++;
			}
#endif
		}
	} else {
		pr_debug("Find pt or mpt\n");
		if (MLC_DEVICE == TRUE)
			mtd->numeraseregions = 0;
#if 0
		memcpy(&g_exist_Partition, &g_pasStatic_Partition,
		       sizeof(struct mtd_partition) * part_num);
		for (i = 0; i < part_num; i++) {
			pr_debug("partition %s size %llx %llx\n", lastest_part[i].name,
			       lastest_part[i].offset, lastest_part[i].size);
			/* still use the name in g_pasSatic_partition */
			g_exist_Partition[i].size = lastest_part[i].size;
			g_exist_Partition[i].offset = lastest_part[i].offset;
#if 1
			if (MLC_DEVICE == TRUE) {
				mtd->eraseregions[i].offset = lastest_part[i].offset;
				mtd->eraseregions[i].erasesize = mtd->erasesize;
#if 0				/* to build pass xiaolei */
				if (partition_type_array[i] == TYPE_RAW)
					mtd->eraseregions[i].erasesize = mtd->erasesize / 2;
#endif
				mtd->numeraseregions++;
			}
#endif
			/* still use the mask flag in g_pasSatic_partition */
			if (i == (part_num - 1))
				g_exist_Partition[i].size = MTDPART_SIZ_FULL;
			pr_debug("partition %s size %llx\n", lastest_part[i].name,
			       g_exist_Partition[i].offset);
		}
#endif
		construct_mtd_partition(mtd);
	}
	init_pmt_done = TRUE;

	pr_debug(": register NAND PMT device ...\n");
#ifndef MTK_EMMC_SUPPORT

	err = misc_register(&pmt_dev);
	if (unlikely(err)) {
		pr_err("PMT failed to register device!\n");
		/* return err; */
	}
#endif
}

int new_part_tab(u8 *buf, struct mtd_info *mtd)
{
	DM_PARTITION_INFO_PACKET *dm_part = (DM_PARTITION_INFO_PACKET *) buf;
	int part_num, change_index, i = 0;
	int retval;
	int pageoffset;
	u64 start_addr = (u64) ((mtd->size) + block_size);
	u64 current_addr = 0;
	struct mtd_oob_ops ops_pt;

	pi.pt_changed = 0;
	pi.tool_or_sd_update = 2;	/* tool download is 1. */

	ops_pt.mode = MTD_OPS_AUTO_OOB;
	ops_pt.len = mtd->writesize;
	ops_pt.retlen = 0;
	ops_pt.ooblen = 16;
	ops_pt.oobretlen = 0;
	ops_pt.oobbuf = page_buf + page_size;
	ops_pt.ooboffs = 0;
	/* the first image is ? */
#if 1
	for (part_num = 0; part_num < PART_MAX_COUNT; part_num++) {
		memcpy(new_part[part_num].name, dm_part->part_info[part_num].part_name,
		       MAX_PARTITION_NAME_LEN);
		new_part[part_num].offset = dm_part->part_info[part_num].start_addr;
		new_part[part_num].size = dm_part->part_info[part_num].part_len;
		new_part[part_num].mask_flags = 0;
		/* MSG (INIT, "DM_PARTITION_INFO_PACKET %s size %x %x\n",
				dm_part->part_info[part_num].part_name,
				dm_part->part_info[part_num].part_len,part_num); */
		pr_debug("new_pt %s size %llx\n", new_part[part_num].name,
		       new_part[part_num].size);
		if (dm_part->part_info[part_num].part_len == 0) {
			pr_debug("new_pt last %x\n", part_num);
			break;
		}
	}
#endif
	/* ++++++++++for test */
#if 0
	part_num = 13;
	memcpy(&new_part[0], &lastest_part[0], sizeof(new_part));
	MSG(INIT, "new_part  %x size\n", sizeof(new_part));
	for (i = 0; i < part_num; i++) {
		MSG(INIT, "npt partition %s size\n", new_part[i].name);
		/* MSG (INIT, "npt %x size\n",new_part[i].offset); */
		/* MSG (INIT, "npt %x size\n",lastest_part[i].offset); */
		/* MSG (INIT, "npt %x size\n",new_part[i].size); */
		dm_part->part_info[5].part_visibility = 1;
		dm_part->part_info[5].dl_selected = 1;
		new_part[5].size = lastest_part[5].size + 0x100000;
	}
#endif
	/* ------------for test */
	/* Find the first changed partition, whether is visible */
	for (change_index = 0; change_index <= part_num; change_index++) {
		if ((new_part[change_index].size != lastest_part[change_index].size)
		    || (new_part[change_index].offset != lastest_part[change_index].offset)) {
			pr_debug("new_pt %x size changed from %llx to %llx\n", change_index,
			       lastest_part[change_index].size, new_part[change_index].size);
			pi.pt_changed = 1;
			break;
		}
	}

	if (pi.pt_changed == 1) {
		/* Is valid image update */
		for (i = change_index; i <= part_num; i++) {

			if (dm_part->part_info[i].dl_selected == 0
			    && dm_part->part_info[i].part_visibility == 1) {
				pr_notice("Full download is need %x\n", i);
				retval = DM_ERR_NO_VALID_TABLE;
				return retval;
			}
		}

		pageoffset = find_empty_page_from_top(start_addr, mtd);
		/* download partition used the new partition */
		/* write mirror at the same 2 page */
		memset(page_buf, 0xFF, page_size + 64);
		*(int *)sig_buf = MPT_SIG;
		memcpy(page_buf, &sig_buf, PT_SIG_SIZE);
		memcpy(&page_buf[PT_SIG_SIZE], &new_part[0], sizeof(new_part));
		memcpy(&page_buf[page_size], &sig_buf, PT_SIG_SIZE);
		pi.sequencenumber += 1;
		memcpy(&page_buf[page_size + PT_SIG_SIZE], &pi, PT_SIG_SIZE);

		if (pageoffset != 0xFFFF) {
			if ((pageoffset % 2) != 0) {
				pr_info("new_pt mirror block may destroy last time%x\n",
				       pageoffset);
				pageoffset += 1;
			}
			for (i = 0; i < 2; i++) {
				current_addr = start_addr + (pageoffset + i) * page_size;
				ops_pt.datbuf = (uint8_t *) page_buf;
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_info("new_pt write m first page failed %llx\n",
					       current_addr);
					continue;
				}
				pr_info("new_pt write mirror at %llx\n",
				       current_addr);
				ops_pt.datbuf = (uint8_t *) page_readbuf;
				/* read back verify */
				if ((mtd->_read_oob(mtd, (loff_t) current_addr, &ops_pt) !=
				     0) || memcmp(page_buf, page_readbuf, page_size)) {
					pr_info("new_pt read or verify first mirror page failed %llx\n",
					       current_addr);
					ops_pt.datbuf = (uint8_t *) page_buf;
					memset(page_buf, 0, PT_SIG_SIZE);
					if (mtd->_read_oob(mtd, (loff_t) current_addr, &ops_pt) != 0)
						pr_info("new_pt mark failed %llx\n", current_addr);
				} else {
					pr_info("new_pt write mirror ok %x\n", i);
					/* any one success set this flag? */
					pi.mirror_pt_dl = 1;
				}
			}
		}
	} else {
		pr_debug("new_part_tab no pt change %x\n", i);
	}

	retval = DM_ERR_OK;
	return retval;
}

int update_part_tab(struct mtd_info *mtd)
{
	int retval = 0;
	int retry_w;
	int retry_r;
	u64 start_addr = (u64) (mtd->size);	/* PT_LOCATION*block_size; */
	u64 current_addr = 0;
	struct erase_info ei;
	struct mtd_oob_ops ops_pt;

	memset(page_buf, 0xFF, page_size + 64);

	ei.mtd = mtd;
	ei.len = mtd->erasesize;
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
		pr_debug("update_pt pt changes\n");

		ei.addr = start_addr;
		if (mtd->_erase(mtd, &ei) != 0) {	/* no good block for used in replace pool */
			pr_err("update_pt erase failed %llx\n", start_addr);
			if (pi.mirror_pt_dl == 0)
				retval = DM_ERR_NO_SPACE_FOUND;
			return retval;
		}

		for (retry_r = 0; retry_r < RETRY_TIMES; retry_r++) {
			for (retry_w = 0; retry_w < RETRY_TIMES; retry_w++) {
				current_addr = start_addr + (retry_w + retry_r * RETRY_TIMES) * page_size;
				*(int *)sig_buf = PT_SIG;
				memcpy(page_buf, &sig_buf, PT_SIG_SIZE);
				memcpy(&page_buf[PT_SIG_SIZE], &new_part[0], sizeof(new_part));
				memcpy(&page_buf[page_size], &sig_buf, PT_SIG_SIZE);
				memcpy(&page_buf[page_size + PT_SIG_SIZE], &pi, PT_SIG_SIZE);

				ops_pt.datbuf = (uint8_t *) page_buf;
				/* no good block for used in replace pool . still used the original ones */
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_notice("update_pt write failed %x\n", retry_w);
					memset(page_buf, 0, PT_SIG_SIZE);
					if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) !=
					    0) {
						pr_notice("write error mark failed\n");
						/* continue retry */
						continue;
					}
				} else {
					pr_debug("write pt success %llx %x\n",
					       current_addr, retry_w);
					break;	/* retry_w should not count. */
				}
			}
			if (retry_w == RETRY_TIMES) {
				pr_notice("update_pt retry w failed\n");
				if (pi.mirror_pt_dl == 0) {	/* mirror also can not write down */
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

				pr_debug("v or r failed %x\n", retry_r);
				memset(page_buf, 0, PT_SIG_SIZE);
				ops_pt.datbuf = (uint8_t *) page_buf;
				if (mtd->_write_oob(mtd, (loff_t) current_addr, &ops_pt) != 0) {
					pr_notice("read error mark failed\n");
					/* continue retryp */
					continue;
				}

			} else {
				pr_debug("update_pt r&v ok%llx\n", current_addr);
				break;
			}
		}
	} else {
		pr_debug("update_pt no change\n");
	}
	return DM_ERR_OK;
}

int get_part_num_nand(void)
{
	return part_num;
}
EXPORT_SYMBOL(get_part_num_nand);

#endif
