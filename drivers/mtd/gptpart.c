// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author:	Guochun Mao	<guochun.mao@mediatek.com>
 *		Xiaolei Li	<xiaolei.li@mediatek.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/div64.h>

/* GPT Signature should be 0x5452415020494645 */
#define GPT_SIGNATURE_1            0x54524150U
#define GPT_SIGNATURE_2            0x20494645U

/* GPT Offsets */
#define HEADER_SIZE_OFFSET         12
#define HEADER_CRC_OFFSET          16
#define PRIMARY_HEADER_OFFSET      24
#define BACKUP_HEADER_OFFSET       32
#define FIRST_USABLE_LBA_OFFSET    40
#define LAST_USABLE_LBA_OFFSET     48
#define PARTITION_ENTRIES_OFFSET   72
#define PARTITION_COUNT_OFFSET     80
#define PENTRY_SIZE_OFFSET         84
#define PARTITION_CRC_OFFSET       88

#define ENTRY_SIZE                 0x80

#define UNIQUE_GUID_OFFSET         16
#define FIRST_LBA_OFFSET           32
#define LAST_LBA_OFFSET            40
#define ATTRIBUTE_FLAG_OFFSET      48
#define PARTITION_NAME_OFFSET      56

#define MAX_GPT_NAME_SIZE          72
#define PARTITION_TYPE_GUID_SIZE   16
#define UNIQUE_PARTITION_GUID_SIZE 16
#define NUM_PARTITIONS             128

#define GET_LWORD_FROM_BYTE(ptr)	(*(uint32_t *)((unsigned long)(ptr)))
#define GET_LLWORD_FROM_BYTE(ptr)	(*(uint64_t *)((unsigned long)(ptr)))

struct chs {
	uint8_t c;
	uint8_t h;
	uint8_t s;
};

struct mbr_part {
	uint8_t status;
	struct chs start;
	uint8_t type;
	struct chs end;
	uint32_t lba_start;
	uint32_t lba_length;
};

struct gpt_info {
	uint64_t first_usable_lba;
	uint64_t backup_header_lba;
	uint32_t partition_entry_size;
	uint32_t header_size;
	uint32_t max_partition_count;
};

static int validate_mbr_partition(struct mtd_info *master,
				  const struct mbr_part *part)
{
	u32 lba_size, lba_number;
	uint64_t temp;

	if (mtd_type_is_nand(master) != 0)
		lba_size = master->writesize;
	else
		lba_size = 512;

	/* check for invalid types */
	if ((int8_t)part->type == 0)
		return -1;
	/* check for invalid status */
	if (part->status != (uint8_t)0x80 &&
		part->status != (uint8_t)0x00)
		return -1;

	/* make sure the range fits within the device */
	temp = (uint64_t)master->size;
	do_div(temp, lba_size);
	lba_number = temp;
	if (part->lba_start >= lba_number)
		return -1;
	if ((part->lba_start + part->lba_length) > lba_number)
		return -1;

	return 0;
}

/*
 * Parse the gpt header and get the required header fields
 * Return 0 on valid signature
 */
static int partition_parse_gpt_header(unsigned char *buffer,
				      struct gpt_info *info)
{
	/* Check GPT Signature */
	if (GET_LWORD_FROM_BYTE(&buffer[0]) != GPT_SIGNATURE_2 ||
	    GET_LWORD_FROM_BYTE(&buffer[4]) != GPT_SIGNATURE_1)
		return 1;

	info->header_size = GET_LWORD_FROM_BYTE(&buffer[HEADER_SIZE_OFFSET]);
	info->backup_header_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[BACKUP_HEADER_OFFSET]);
	info->first_usable_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[FIRST_USABLE_LBA_OFFSET]);
	info->max_partition_count =
	    GET_LWORD_FROM_BYTE(&buffer[PARTITION_COUNT_OFFSET]);
	info->partition_entry_size =
	    GET_LWORD_FROM_BYTE(&buffer[PENTRY_SIZE_OFFSET]);

	return 0;
}

static void gpt_add_part(struct mtd_partition *part, char *name,
				 u64 offset, uint32_t mask_flags, uint64_t size)
{
	part->name = name;
	part->offset = offset;
	part->mask_flags = mask_flags;
	part->size = size;
}

static int gpt_parse(struct mtd_info *master,
			     const struct mtd_partition **pparts,
			     struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts;
	int curr_part = 0;
	int err;
	u_char *buf, *temp_buf;
	size_t bytes_read = 0;
	int i, j, n, part_entry_cnt, gpt_entries;
	int gpt_partitions_exist = 0;
	struct mbr_part part[4];
	struct gpt_info gptinfo = {0, 0, 0, 0, 0};
	loff_t partition_0;
	u32 lba_size;
	int tmp;

	dev_dbg(&master->dev, "GPT: enter gpt parser...\n");

	if (mtd_type_is_nand(master) != 0)
		lba_size = master->writesize;
	else
		lba_size = 512;

	buf = kzalloc(lba_size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	err = mtd_read(master, 0, lba_size, &bytes_read, buf);
	if (err < 0)
		goto freebuf;

	/* look for the aa55 tag */
	if (buf[510] != (u_char)0x55 || buf[511] != (u_char)0xaa) {
		dev_err(&master->dev, "GPT: not find aa55 @ 510,511\n");
		goto freebuf;
	}

	/* see if a partition table makes sense here */
	temp_buf = (u_char *)&part[0];
	memcpy(temp_buf, &buf[446], sizeof(part));

	/* validate each of the partition entries */
	for (i = 0; i < 4; i++) {
		if (validate_mbr_partition(master, &part[i]) >= 0) {
			/*
			 * Type 0xEE indicates end of MBR
			 * and GPT partitions exist
			 */
			if (part[i].type == (uint8_t)0xee) {
				gpt_partitions_exist = 1;
				break;
			}
		}
	}

	if (gpt_partitions_exist == 0) {
		dev_err(&master->dev, "GPT: not find GPT\n");
		goto freebuf;
	}

	err = mtd_read(master, (loff_t)lba_size, lba_size, &bytes_read, buf);
	if (err < 0)
		goto freebuf;

	err = partition_parse_gpt_header(buf, &gptinfo);
	if (err != 0) {
		dev_warn(&master->dev, "GPT: Read GPT header fail, try to check the backup gpt.\n");
		err = mtd_read(master, (loff_t)master->size - (loff_t)lba_size,
				lba_size, &bytes_read, buf);
		if (err < 0) {
			dev_err(&master->dev, "GPT: Could not read backup gpt.\n");
			goto freebuf;
		}

		err = partition_parse_gpt_header(buf, &gptinfo);
		if (err != 0) {
			dev_err(&master->dev, "GPT: Primary and backup signatures invalid.\n");
			goto freebuf;
		}
	}

	parts = kcalloc(gptinfo.max_partition_count,
			sizeof(struct mtd_partition), GFP_KERNEL);
	if (parts == NULL)
		return -ENOMEM;

	part_entry_cnt = (int)lba_size / ENTRY_SIZE;
	partition_0 =
		(loff_t)GET_LLWORD_FROM_BYTE(&buf[PARTITION_ENTRIES_OFFSET]);

	/* Read GPT Entries */
	gpt_entries = roundup((int)gptinfo.max_partition_count, part_entry_cnt);
	gpt_entries /= part_entry_cnt;
	for (i = 0; i < gpt_entries; i++) {
		err = mtd_read(master,
			((partition_0 + (loff_t)i) * (loff_t)lba_size),
			lba_size, &bytes_read, buf);
		if (err < 0) {
			dev_err(&master->dev, "GPT: read failed reading partition entries.\n");
			goto freeparts;
		}

		for (j = 0; j < part_entry_cnt; j++) {
			int8_t type_guid[PARTITION_TYPE_GUID_SIZE];
			char *name = kzalloc(MAX_GPT_NAME_SIZE,
							GFP_KERNEL);
			char UTF16_name[MAX_GPT_NAME_SIZE];
			uint64_t first_lba, last_lba, size;

			temp_buf = (u_char *)&type_guid[0];
			memcpy(temp_buf,
				&buf[(u32)j * gptinfo.partition_entry_size],
				PARTITION_TYPE_GUID_SIZE);
			if (type_guid[0] == 0 && type_guid[1] == 0) {
				kfree(name);
				goto parsedone;
			}

			tmp = j * (int)gptinfo.partition_entry_size;
			tmp += FIRST_LBA_OFFSET;
			first_lba = GET_LLWORD_FROM_BYTE(&buf[tmp]);
			tmp = j * (int)gptinfo.partition_entry_size;
			tmp += LAST_LBA_OFFSET;
			last_lba = GET_LLWORD_FROM_BYTE(&buf[tmp]);
			size = last_lba - first_lba + 1ULL;

			memset(&UTF16_name[0], 0x00, MAX_GPT_NAME_SIZE);
			temp_buf = (u_char *)&UTF16_name[0];
			memcpy(temp_buf,
				&buf[(u32)j * gptinfo.partition_entry_size +
				(uint32_t)PARTITION_NAME_OFFSET],
				MAX_GPT_NAME_SIZE);

			/*
			 * Currently partition names in *.xml are UTF-8 and
			 * lowercase only supporting english for now so removing
			 * 2nd byte of UTF-16
			 */
			for (n = 0; n < MAX_GPT_NAME_SIZE / 2; n++)
				name[n] = UTF16_name[n * 2];

			dev_dbg(&master->dev, "partition(%s) first_lba(%llu), last_lba(%llu), size(%llu)\n",
				name, first_lba, last_lba, size);

			gpt_add_part(&parts[curr_part++], name,
				first_lba * lba_size, 0,
				(last_lba - first_lba + 1ULL) * lba_size);

			dev_dbg(&master->dev, "gpt there are <%d> parititons.\n",
				curr_part);
		}
	}

parsedone:
	*pparts = parts;
	kfree(buf);
	return curr_part;

freeparts:
	kfree(parts);
freebuf:
	kfree(buf);
	return 0;
};

static struct mtd_part_parser gpt_parser = {
	.owner = THIS_MODULE,
	.parse_fn = gpt_parse,
	.name = "gptpart",
};

static int __init gptpart_init(void)
{
	return register_mtd_parser(&gpt_parser);
}

static void __exit gptpart_exit(void)
{
	deregister_mtd_parser(&gpt_parser);
}

module_init(gptpart_init);
module_exit(gptpart_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPT partitioning for flash memories");
