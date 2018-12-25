/*
 * Copyright (c) 2015 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

/* GPT Signature should be 0x5452415020494645 */
#define GPT_SIGNATURE_1            0x54524150
#define GPT_SIGNATURE_2            0x20494645

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

#define GET_LWORD_FROM_BYTE(x)     (*(unsigned int *)(x))
#define GET_LLWORD_FROM_BYTE(x)    (*(unsigned long long *)(x))
#define GET_LONG(x)                (*(uint32_t *)(x))
#define PUT_LONG(x, y)             (*((uint32_t *)(x)) = y)
#define PUT_LONG_LONG(x, y)        (*((unsigned long long *)(x)) = y)
#define ROUNDUP(x, y)              ((((x)+(y)-1)/(y))*(y))

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

struct gpt_header {
	uint64_t first_usable_lba;
	uint64_t backup_header_lba;
	uint32_t partition_entry_size;
	uint32_t header_size;
	uint32_t max_partition_count;
};

static int validate_mbr_partition(struct mtd_info *master, const struct mbr_part *part)
{
	u32 lba_size;

	if (mtd_type_is_nand(master))
		lba_size = master->writesize;
	else
		lba_size = 512;

	/* check for invalid types */
	if (part->type == 0)
		return -1;
	/* check for invalid status */
	if (part->status != 0x80 && part->status != 0x00)
		return -1;

	/* make sure the range fits within the device */
	if (part->lba_start >= master->size / lba_size)
		return -1;
	if ((part->lba_start + part->lba_length) > master->size / lba_size)
		return -1;

	return 0;
}

/*
 * Parse the gpt header and get the required header fields
 * Return 0 on valid signature
 */
static int partition_parse_gpt_header(unsigned char *buffer, struct gpt_header *header)
{
	/* Check GPT Signature */
	if (((uint32_t *) buffer)[0] != GPT_SIGNATURE_2 ||
	    ((uint32_t *) buffer)[1] != GPT_SIGNATURE_1)
		return 1;

	header->header_size = GET_LWORD_FROM_BYTE(&buffer[HEADER_SIZE_OFFSET]);
	header->backup_header_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[BACKUP_HEADER_OFFSET]);
	header->first_usable_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[FIRST_USABLE_LBA_OFFSET]);
	header->max_partition_count =
	    GET_LWORD_FROM_BYTE(&buffer[PARTITION_COUNT_OFFSET]);
	header->partition_entry_size =
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

int gpt_parse(struct mtd_info *master,
			     const struct mtd_partition **pparts,
			     struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts;
	uint32_t curr_part = 0;
	int err;
	uint8_t *buf;
	size_t bytes_read = 0;
	uint32_t tmp;
	unsigned int i, j, n, k;
	int gpt_partitions_exist = 0;
	struct mbr_part part[4];
	struct gpt_header gpthdr = {0, 0, 0, 0, 0};
	uint32_t part_entry_cnt;
	uint64_t partition_0;
	u32 lba_size;

	dev_dbg(&master->dev, "GPT: enter gpt parser...\n");

	if (mtd_type_is_nand(master))
		lba_size = master->writesize;
	else
		lba_size = 512;

	buf = kzalloc(lba_size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	err = mtd_read(master, 0, lba_size, &bytes_read, buf);
	if (err < 0)
		goto freebuf;

	for (k = 0; k < lba_size; k += 8) {
		dev_dbg(&master->dev, "+%d: %x %x %x %x  %x %x %x %x\n",
			k, buf[k], buf[k+1], buf[k+2], buf[k+3],
			buf[k+4], buf[k+5], buf[k+6], buf[k+7]);
	}

	/* look for the aa55 tag */
	if (buf[510] != 0x55 || buf[511] != 0xaa) {
		dev_err(&master->dev, "GPT: not find aa55 @ 510,511\n");
		goto freebuf;
	}

	/* see if a partition table makes sense here */
	memcpy(part, buf + 446, sizeof(part));

	/* validate each of the partition entries */
	for (i = 0; i < 4; i++) {
		if (validate_mbr_partition(master, &part[i]) >= 0) {
			/* Type 0xEE indicates end of MBR and GPT partitions exist */
			if (part[i].type == 0xee) {
				gpt_partitions_exist = 1;
				break;
			}
		}
	}

	if (gpt_partitions_exist == 0) {
		dev_err(&master->dev, "GPT: not find GPT\n");
		goto freebuf;
	}

	err = mtd_read(master, lba_size, lba_size, &bytes_read, buf);
	if (err < 0)
		goto freebuf;

	err = partition_parse_gpt_header(buf, &gpthdr);
	if (err) {
		dev_warn(&master->dev, "GPT: Read GPT header fail, try to check the backup gpt.\n");
		err = mtd_read(master, master->size - lba_size, lba_size, &bytes_read, buf);
		if (err < 0) {
			dev_err(&master->dev, "GPT: Could not read backup gpt.\n");
			goto freebuf;
		}

		err = partition_parse_gpt_header(buf, &gpthdr);
		if (err) {
			dev_err(&master->dev, "GPT: Primary and backup signatures invalid.\n");
			goto freebuf;
		}
	}

	parts = kcalloc(gpthdr.max_partition_count, sizeof(struct mtd_partition),
			GFP_KERNEL);
	if (parts == NULL)
		return -ENOMEM;

	part_entry_cnt = lba_size / ENTRY_SIZE;
	partition_0 = GET_LLWORD_FROM_BYTE(&buf[PARTITION_ENTRIES_OFFSET]);

	/* Read GPT Entries */
	for (i = 0; i < (ROUNDUP(gpthdr.max_partition_count, part_entry_cnt)) / part_entry_cnt; i++) {
		err = mtd_read(master, (partition_0 * lba_size) + (i * lba_size),
			lba_size, &bytes_read, (uint8_t *)buf);
		if (err < 0) {
			dev_err(&master->dev, "GPT: read failed reading partition entries.\n");
			goto freeparts;
		}

		for (j = 0; j < part_entry_cnt; j++) {
			unsigned char type_guid[PARTITION_TYPE_GUID_SIZE];
			unsigned char *name = kzalloc(MAX_GPT_NAME_SIZE, GFP_KERNEL);
			unsigned char UTF16_name[MAX_GPT_NAME_SIZE];
			uint64_t first_lba, last_lba, size;

			memcpy(&type_guid,
				&buf[(j * gpthdr.partition_entry_size)],
				PARTITION_TYPE_GUID_SIZE);
			if (type_guid[0] == 0 && type_guid[1] == 0) {
				i = ROUNDUP(gpthdr.max_partition_count, part_entry_cnt);
				break;
			}

			tmp = j * gpthdr.partition_entry_size + FIRST_LBA_OFFSET;
			first_lba = GET_LLWORD_FROM_BYTE(&buf[tmp]);
			tmp = j * gpthdr.partition_entry_size + LAST_LBA_OFFSET;
			last_lba = GET_LLWORD_FROM_BYTE(&buf[tmp]);
			size = last_lba - first_lba + 1;

			memset(&UTF16_name, 0x00, MAX_GPT_NAME_SIZE);
			memcpy(UTF16_name, &buf[(j * gpthdr.partition_entry_size) +
				PARTITION_NAME_OFFSET], MAX_GPT_NAME_SIZE);

			/*
			 * Currently partition names in *.xml are UTF-8 and lowercase
			 * Only supporting english for now so removing 2nd byte of UTF-16
			 */
			for (n = 0; n < MAX_GPT_NAME_SIZE / 2; n++)
				name[n] = UTF16_name[n * 2];

			dev_dbg(&master->dev, "partition(%s) first_lba(%lld), last_lba(%lld), size(%lld)\n",
				name, first_lba, last_lba, size);

			gpt_add_part(&parts[curr_part++], name,
				first_lba * lba_size, 0, (last_lba - first_lba + 1) * lba_size);

			dev_dbg(&master->dev, "gpt there are <%d> parititons.\n", curr_part);
		}
	}

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
	register_mtd_parser(&gpt_parser);
	return 0;
}

static void __exit gptpart_exit(void)
{
	deregister_mtd_parser(&gpt_parser);
}

module_init(gptpart_init);
module_exit(gptpart_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPT partitioning for flash memories");
