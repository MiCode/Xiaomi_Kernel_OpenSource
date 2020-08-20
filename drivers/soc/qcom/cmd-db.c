// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved. */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/types.h>

#include <soc/qcom/cmd-db.h>

#define NUM_PRIORITY		2
#define MAX_SLV_ID		8
#define SLAVE_ID_MASK		0x7
#define SLAVE_ID_SHIFT		16
#define CMD_DB_STANDALONE_MASK BIT(0)

/**
 * struct entry_header: header for each entry in cmddb
 *
 * @id: resource's identifier
 * @priority: unused
 * @addr: the address of the resource
 * @len: length of the data
 * @offset: offset from :@data_offset, start of the data
 */
struct entry_header {
	u8 id[8];
	__le32 priority[NUM_PRIORITY];
	__le32 addr;
	__le16 len;
	__le16 offset;
};

/**
 * struct rsc_hdr: resource header information
 *
 * @slv_id: id for the resource
 * @header_offset: entry's header at offset from the end of the cmd_db_header
 * @data_offset: entry's data at offset from the end of the cmd_db_header
 * @cnt: number of entries for HW type
 * @version: MSB is major, LSB is minor
 * @reserved: reserved for future use.
 */
struct rsc_hdr {
	__le16 slv_id;
	__le16 header_offset;
	__le16 data_offset;
	__le16 cnt;
	__le16 version;
	__le16 reserved[3];
};

/**
 * struct cmd_db_header: The DB header information
 *
 * @version: The cmd db version
 * @magic: constant expected in the database
 * @header: array of resources
 * @checksum: checksum for the header. Unused.
 * @reserved: reserved memory
 * @data: driver specific data
 */
struct cmd_db_header {
	__le32 version;
	u8 magic[4];
	struct rsc_hdr header[MAX_SLV_ID];
	__le32 checksum;
	__le32 reserved;
	u8 data[];
};

/**
 * DOC: Description of the Command DB database.
 *
 * At the start of the command DB memory is the cmd_db_header structure.
 * The cmd_db_header holds the version, checksum, magic key as well as an
 * array for header for each slave (depicted by the rsc_header). Each h/w
 * based accelerator is a 'slave' (shared resource) and has slave id indicating
 * the type of accelerator. The rsc_header is the header for such individual
 * slaves of a given type. The entries for each of these slaves begin at the
 * rsc_hdr.header_offset. In addition each slave could have auxiliary data
 * that may be needed by the driver. The data for the slave starts at the
 * entry_header.offset to the location pointed to by the rsc_hdr.data_offset.
 *
 * Drivers have a stringified key to a slave/resource. They can query the slave
 * information and get the slave id and the auxiliary data and the length of the
 * data. Using this information, they can format the request to be sent to the
 * h/w accelerator and request a resource state.
 */

static const u8 CMD_DB_MAGIC[] = { 0xdb, 0x30, 0x03, 0x0c };
static struct dentry *debugfs;

static bool cmd_db_magic_matches(const struct cmd_db_header *header)
{
	const u8 *magic = header->magic;

	return memcmp(magic, CMD_DB_MAGIC, ARRAY_SIZE(CMD_DB_MAGIC)) == 0;
}

static struct cmd_db_header *cmd_db_header;

static inline const void *rsc_to_entry_header(const struct rsc_hdr *hdr)
{
	u16 offset = le16_to_cpu(hdr->header_offset);

	return cmd_db_header->data + offset;
}

static inline void *
rsc_offset(const struct rsc_hdr *hdr, const struct entry_header *ent)
{
	u16 offset = le16_to_cpu(hdr->data_offset);
	u16 loffset = le16_to_cpu(ent->offset);

	return cmd_db_header->data + offset + loffset;
}

/**
 * cmd_db_ready - Indicates if command DB is available
 *
 * Return: 0 on success, errno otherwise
 */
int cmd_db_ready(void)
{
	if (cmd_db_header == NULL)
		return -EPROBE_DEFER;
	else if (!cmd_db_magic_matches(cmd_db_header))
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(cmd_db_ready);

static int cmd_db_get_header(const char *id, const struct entry_header **eh,
			     const struct rsc_hdr **rh)
{
	const struct rsc_hdr *rsc_hdr;
	const struct entry_header *ent;
	int ret, i, j;
	u8 query[8];

	ret = cmd_db_ready();
	if (ret)
		return ret;

	/* Pad out query string to same length as in DB */
	strncpy(query, id, sizeof(query));

	for (i = 0; i < MAX_SLV_ID; i++) {
		rsc_hdr = &cmd_db_header->header[i];
		if (!rsc_hdr->slv_id)
			break;

		ent = rsc_to_entry_header(rsc_hdr);
		for (j = 0; j < le16_to_cpu(rsc_hdr->cnt); j++, ent++) {
			if (memcmp(ent->id, query, sizeof(ent->id)) == 0) {
				if (eh)
					*eh = ent;
				if (rh)
					*rh = rsc_hdr;
				return 0;
			}
		}
	}

	return -ENODEV;
}

/**
 * cmd_db_read_addr() - Query command db for resource id address.
 *
 * @id: resource id to query for address
 *
 * Return: resource address on success, 0 on error
 *
 * This is used to retrieve resource address based on resource
 * id.
 */
u32 cmd_db_read_addr(const char *id)
{
	int ret;
	const struct entry_header *ent;

	ret = cmd_db_get_header(id, &ent, NULL);

	return ret < 0 ? 0 : le32_to_cpu(ent->addr);
}
EXPORT_SYMBOL(cmd_db_read_addr);

/**
 * cmd_db_read_aux_data() - Query command db for aux data.
 *
 *  @id: Resource to retrieve AUX Data on
 *  @len: size of data buffer returned
 *
 *  Return: pointer to data on success, error pointer otherwise
 */
const void *cmd_db_read_aux_data(const char *id, size_t *len)
{
	int ret;
	const struct entry_header *ent;
	const struct rsc_hdr *rsc_hdr;

	ret = cmd_db_get_header(id, &ent, &rsc_hdr);
	if (ret)
		return ERR_PTR(ret);

	if (len)
		*len = le16_to_cpu(ent->len);

	return rsc_offset(rsc_hdr, ent);
}
EXPORT_SYMBOL(cmd_db_read_aux_data);

/**
 * cmd_db_read_slave_id - Get the slave ID for a given resource address
 *
 * @id: Resource id to query the DB for version
 *
 * Return: cmd_db_hw_type enum on success, CMD_DB_HW_INVALID on error
 */
enum cmd_db_hw_type cmd_db_read_slave_id(const char *id)
{
	int ret;
	const struct entry_header *ent;
	u32 addr;

	ret = cmd_db_get_header(id, &ent, NULL);
	if (ret < 0)
		return CMD_DB_HW_INVALID;

	addr = le32_to_cpu(ent->addr);
	return (addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
}
EXPORT_SYMBOL(cmd_db_read_slave_id);

bool cmd_db_is_standalone(void)
{
	int ret = cmd_db_ready();
	u32 standalone = le32_to_cpu(cmd_db_header->reserved) &
			 CMD_DB_STANDALONE_MASK;

	return !ret && standalone;
}
EXPORT_SYMBOL(cmd_db_is_standalone);

static void *cmd_db_start(struct seq_file *m, loff_t *pos)
{
	int slv_idx, ent_idx, cnt;
	struct entry_header *ent;
	int total = 0;

	for (slv_idx = 0; slv_idx < MAX_SLV_ID; slv_idx++) {
		cnt = le16_to_cpu(cmd_db_header->header[slv_idx].cnt);
		if (!cnt)
			continue;
		ent_idx = *pos - total;
		if (ent_idx < cnt)
			break;
		total += cnt;
	}

	if (slv_idx == MAX_SLV_ID)
		return NULL;

	ent = (void *)cmd_db_header->data +
	      le16_to_cpu(cmd_db_header->header[slv_idx].header_offset);

	return &ent[ent_idx];
}

static void cmd_db_stop(struct seq_file *m, void *v)
{
}

static void *cmd_db_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return cmd_db_start(m, pos);
}

static int cmd_db_seq_show(struct seq_file *m, void *v)
{
	struct entry_header *eh = v;
	char buf[9] = {0};
	int eh_addr, eh_len, eh_offset;

	if (!eh)
		return 0;

	memcpy(buf, &eh->id, sizeof(eh->id));
	eh_addr = le32_to_cpu(eh->addr);
	eh_len = le16_to_cpu(eh->len);
	eh_offset = le16_to_cpu(eh->offset);
	seq_printf(m, "Address: 0x%05x, id: %s", eh_addr, buf);

	if (eh_len) {
		int slv_id = (eh_addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
		u8 aux[32] = {0};
		int len, offset;
		int k;

		len = min_t(u32, eh_len, sizeof(aux));
		for (k = 0; k < MAX_SLV_ID; k++) {
			struct rsc_hdr *hdr  = &cmd_db_header->header[k];

			if (le16_to_cpu(hdr->slv_id) == slv_id)
				break;
		}

		if (k == MAX_SLV_ID)
			return -EINVAL;

		offset = le16_to_cpu(cmd_db_header->header[k].data_offset);
		memcpy(aux, cmd_db_header->data + offset + eh_offset, len);

		seq_puts(m, ", aux data: ");

		for (k = 0; k < len; k++)
			seq_printf(m, "%02x ", aux[k]);

	}
	seq_puts(m, "\n");

	return 0;
}

static const struct seq_operations cmd_db_seq_ops = {
	.start = cmd_db_start,
	.stop = cmd_db_stop,
	.next = cmd_db_next,
	.show = cmd_db_seq_show,
};

static int cmd_db_file_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &cmd_db_seq_ops);
}

static const struct file_operations cmd_db_fops = {
	.owner = THIS_MODULE,
	.open = cmd_db_file_open,
	.read = seq_read,
	.release = seq_release,
	.llseek = no_llseek,
};

static int cmd_db_dev_probe(struct platform_device *pdev)
{
	struct reserved_mem *rmem;
	int ret = 0;

	rmem = of_reserved_mem_lookup(pdev->dev.of_node);
	if (!rmem) {
		dev_err(&pdev->dev, "failed to acquire memory region\n");
		return -EINVAL;
	}

	cmd_db_header = memremap(rmem->base, rmem->size, MEMREMAP_WB);
	if (!cmd_db_header) {
		ret = -ENOMEM;
		cmd_db_header = NULL;
		return ret;
	}

	if (!cmd_db_magic_matches(cmd_db_header)) {
		dev_err(&pdev->dev, "Invalid Command DB Magic\n");
		return -EINVAL;
	}

	if (cmd_db_is_standalone())
		pr_info("Command DB is initialized in standalone mode.\n");

	debugfs = debugfs_create_file("cmd_db", 0444, NULL, NULL, &cmd_db_fops);
	if (!debugfs)
		pr_err("Couldn't create debugfs\n");

	return 0;
}

static const struct of_device_id cmd_db_match_table[] = {
	{ .compatible = "qcom,cmd-db" },
	{ },
};
MODULE_DEVICE_TABLE(of, cmd_db_match_table);

static struct platform_driver cmd_db_dev_driver = {
	.probe  = cmd_db_dev_probe,
	.driver = {
		   .name = "cmd-db",
		   .of_match_table = cmd_db_match_table,
	},
};

static int __init cmd_db_device_init(void)
{
	return platform_driver_register(&cmd_db_dev_driver);
}
arch_initcall(cmd_db_device_init);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Command DB for QCOM SoCs");
MODULE_LICENSE("GPL v2");
