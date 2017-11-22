/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <soc/qcom/cmd-db.h>

#define RESOURCE_ID_LEN 8
#define NUM_PRIORITY  2
#define MAX_SLV_ID 8
#define CMD_DB_MAGIC 0x0C0330DBUL
#define SLAVE_ID_MASK 0x7
#define SLAVE_ID_SHIFT 16
#define CMD_DB_STANDALONE_MASK BIT(0)

struct entry_header {
	uint64_t res_id;
	u32 priority[NUM_PRIORITY];
	u32 addr;
	u16 len;
	u16 offset;
};

struct rsc_hdr {
	u16  slv_id;
	u16  header_offset;	/* Entry header offset from data  */
	u16  data_offset;	/* Entry offset for data location */
	u16  cnt;	/* Number of entries for HW type */
	u16  version;	/* MSB is Major and LSB is Minor version
			 * identifies the HW type of Aux Data
			 */
	u16 reserved[3];
};

struct cmd_db_header {
	u32 version;
	u32 magic_num;
	struct rsc_hdr header[MAX_SLV_ID];
	u32 check_sum;
	u32 reserved;
	u8 data[];
};

struct cmd_db_entry {
	const char resource_id[RESOURCE_ID_LEN + 1]; /* Unique id per entry */
	const u32 addr; /* TCS Addr Slave ID + Offset address */
	const u32 priority[NUM_PRIORITY]; /* Bitmask for DRV IDs */
	u32       len;                                 /* Aux data len */
	u16       version;
	u8        data[];
};

/* CMD DB QUERY TYPES */
enum cmd_db_query_type {
	CMD_DB_QUERY_RES_ID = 0,
	CMD_DB_QUERY_ADDRESS,
	CMD_DB_QUERY_INVALID,
	CMD_DB_QUERY_MAX = 0x7ffffff,
};

static void __iomem *start_addr;
static struct cmd_db_header *cmd_db_header;
static int cmd_db_status = -EPROBE_DEFER;

static u64 cmd_db_get_u64_id(const char *id)
{
	uint64_t rsc_id = 0;
	uint8_t *ch  = (uint8_t *)&rsc_id;
	int i;

	for (i = 0; ((i < sizeof(rsc_id)) && id[i]); i++)
		ch[i] = id[i];

	return rsc_id;
}

static int cmd_db_get_header(u64 query, struct entry_header *eh,
		struct rsc_hdr *rh, bool use_addr)
{
	struct rsc_hdr *rsc_hdr;
	int i, j;

	if (!cmd_db_header)
		return -EPROBE_DEFER;

	if (!eh || !rh)
		return -EINVAL;

	rsc_hdr = &cmd_db_header->header[0];

	for (i = 0; i < MAX_SLV_ID ; i++, rsc_hdr++) {
		struct entry_header *ent;

		if (!rsc_hdr->slv_id)
			break;

		ent = (struct entry_header *)(start_addr
				+ sizeof(*cmd_db_header)
				+ rsc_hdr->header_offset);

		for (j = 0; j < rsc_hdr->cnt; j++, ent++) {
			if (use_addr) {
				if (ent->addr == (u32)(query))
					break;
			} else if (ent->res_id == query)
				break;
		}

		if (j < rsc_hdr->cnt) {
			memcpy(eh, ent, sizeof(*ent));
			memcpy(rh, &cmd_db_header->header[i], sizeof(*rh));
			return 0;
		}
	}
	return -ENODEV;
}

static int cmd_db_get_header_by_addr(u32 addr,
		struct entry_header *ent_hdr,
		struct rsc_hdr *rsc_hdr)
{
	return cmd_db_get_header((u64)addr, ent_hdr, rsc_hdr, true);
}

static int cmd_db_get_header_by_rsc_id(const char *resource_id,
		struct entry_header *ent_hdr,
		struct rsc_hdr *rsc_hdr)
{
	u64 rsc_id = cmd_db_get_u64_id(resource_id);

	return cmd_db_get_header(rsc_id, ent_hdr, rsc_hdr, false);
}

u32 cmd_db_get_addr(const char *resource_id)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	ret = cmd_db_get_header_by_rsc_id(resource_id, &ent, &rsc_hdr);

	return ret < 0 ? 0 : ent.addr;
}

bool cmd_db_get_priority(u32 addr, u8 drv_id)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	ret = cmd_db_get_header_by_addr(addr, &ent, &rsc_hdr);

	return ret < 0 ? false : (bool)(ent.priority[0] & (1 << drv_id));

}
int cmd_db_get_aux_data(const char *resource_id, u8 *data, int len)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	if (!data)
		return -EINVAL;

	ret = cmd_db_get_header_by_rsc_id(resource_id, &ent, &rsc_hdr);

	if (ret)
		return ret;

	if (ent.len < len)
		return -EINVAL;

	len = (ent.len < len) ? ent.len : len;

	memcpy_fromio(data,
			start_addr + sizeof(*cmd_db_header)
			+ rsc_hdr.data_offset + ent.offset,
			len);
	return len;
}
EXPORT_SYMBOL(cmd_db_get_aux_data);

int cmd_db_get_aux_data_len(const char *resource_id)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	ret = cmd_db_get_header_by_rsc_id(resource_id, &ent, &rsc_hdr);

	return ret < 0 ? 0 : ent.len;
}
EXPORT_SYMBOL(cmd_db_get_aux_data_len);

u16 cmd_db_get_version(const char *resource_id)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	ret = cmd_db_get_header_by_rsc_id(resource_id, &ent, &rsc_hdr);
	return ret < 0 ? 0 : rsc_hdr.version;
}

int cmd_db_ready(void)
{
	return cmd_db_status;
}

int cmd_db_is_standalone(void)
{
	if (cmd_db_status < 0)
		return cmd_db_status;

	return !!(cmd_db_header->reserved & CMD_DB_STANDALONE_MASK);
}

int cmd_db_get_slave_id(const char *resource_id)
{
	int ret;
	struct entry_header ent;
	struct rsc_hdr rsc_hdr;

	ret = cmd_db_get_header_by_rsc_id(resource_id, &ent, &rsc_hdr);
	return ret < 0 ? 0 : (ent.addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
}

static void *cmd_db_start(struct seq_file *m, loff_t *pos)
{
	struct cmd_db_header *hdr = m->private;
	int slv_idx, ent_idx;
	struct entry_header *ent;
	int total = 0;

	for (slv_idx = 0; slv_idx < MAX_SLV_ID; slv_idx++) {

		if (!hdr->header[slv_idx].cnt)
			continue;
		ent_idx = *pos - total;
		if (ent_idx < hdr->header[slv_idx].cnt)
			break;

		total += hdr->header[slv_idx].cnt;
	}

	if (slv_idx == MAX_SLV_ID)
		return NULL;

	ent = start_addr + hdr->header[slv_idx].header_offset + sizeof(*hdr);
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
	struct cmd_db_header *hdr = m->private;
	char buf[9]  = {0};

	if (!eh)
		return 0;

	memcpy(buf, &eh->res_id, min(sizeof(eh->res_id), sizeof(buf)));

	seq_printf(m, "Address: 0x%05x, id: %s", eh->addr, buf);

	if (eh->len) {
		int slv_id = (eh->addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
		u8 aux[32] = {0};
		int len;
		int k;

		len = min_t(u32, eh->len, sizeof(aux));

		for (k = 0; k < MAX_SLV_ID; k++) {
			if (hdr->header[k].slv_id == slv_id)
				break;
		}

		if (k == MAX_SLV_ID)
			return -EINVAL;

		memcpy_fromio(aux, start_addr + hdr->header[k].data_offset
			+ eh->offset + sizeof(*cmd_db_header), len);

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
	int ret = seq_open(file, &cmd_db_seq_ops);
	struct seq_file *s = (struct seq_file *)(file->private_data);

	s->private = inode->i_private;
	return ret;
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
	struct resource res;
	void __iomem *dict;

	dict = of_iomap(pdev->dev.of_node, 0);
	if (!dict) {
		cmd_db_status = -ENOMEM;
		goto failed;
	}

	/*
	 * Read start address and size of the command DB address from
	 * shared dictionary location
	 */
	res.start = readl_relaxed(dict);
	res.end = res.start + readl_relaxed(dict + 0x4);
	res.flags = IORESOURCE_MEM;
	iounmap(dict);

	start_addr = devm_ioremap_resource(&pdev->dev, &res);

	cmd_db_header = devm_kzalloc(&pdev->dev,
			sizeof(*cmd_db_header), GFP_KERNEL);

	if (!cmd_db_header) {
		cmd_db_status = -ENOMEM;
		goto failed;
	}

	memcpy(cmd_db_header, start_addr, sizeof(*cmd_db_header));

	if (cmd_db_header->magic_num != CMD_DB_MAGIC) {
		pr_err("%s(): Invalid Magic\n", __func__);
		cmd_db_status = -EINVAL;
		goto failed;
	}
	cmd_db_status = 0;
	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	if (!debugfs_create_file("cmd_db", 0444, NULL,
				cmd_db_header, &cmd_db_fops))
		pr_err("Couldn't create debugfs\n");

	if (cmd_db_is_standalone() == 1)
		pr_info("Command DB is initialized in standalone mode.\n");

failed:
	return cmd_db_status;
}

static const struct of_device_id cmd_db_match_table[] = {
	{.compatible = "qcom,cmd-db"},
	{},
};

static struct platform_driver cmd_db_dev_driver = {
	.probe = cmd_db_dev_probe,
	.driver = {
		.name = "cmd-db",
		.owner = THIS_MODULE,
		.of_match_table = cmd_db_match_table,
	},
};

int __init cmd_db_device_init(void)
{
	return platform_driver_register(&cmd_db_dev_driver);
}
arch_initcall(cmd_db_device_init);
