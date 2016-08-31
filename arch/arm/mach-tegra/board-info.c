/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * arch/arm/mach-tegra/board-info.c
 * Parse kernel commandline (skuinfo, prodinfo and board_id info parsed here)
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include "board.h"
#include <mach/board_id.h>

/* skuinfo is 18 character long xxx-xxxxx-xxxx-xxx
 *so buffer size is set to 18+1 */
#define SKUINFO_BUF_SIZE 19
/* skuver is 2 character long XX so buffer size is set to 2+1 */
#define SKUVER_BUF_SIZE 3

/* prodinfo is 18 character long xxx-xxxxx-xxxx-xxx
 * so buffer size is set to 18+1 */
#define PRODINFO_BUF_SIZE 19
/* prodver is 2 character long XX so buffer size is set to 2+1 */
#define PRODVER_BUF_SIZE 3

/* board_id is 23 character long xxx-xxxxx-xxxx-xxx-XY.ZZ
 * so buffer size is set to 24+1 */
#define BOARD_ID_BUF_SIZE 25

extern unsigned int system_serial_low;
extern unsigned int system_serial_high;

static unsigned int board_serial_low;
static unsigned int board_serial_high;
static char prodinfo_buffer[PRODINFO_BUF_SIZE];
static char prodver_buffer[PRODVER_BUF_SIZE];
static char skuinfo_buffer[SKUINFO_BUF_SIZE];
static char skuver_buffer[SKUVER_BUF_SIZE];

static int board_id_iterator __initdata;
static struct tegra_board_info board_info_array[TEGRA_MAX_BOARDS];

static int __init board_id_setup(char *line)
{
	char board_id[BOARD_ID_BUF_SIZE] = {0};
	char *traverser = board_id;
	int delimiter_index;

	if (board_id_iterator >= TEGRA_MAX_BOARDS) {
		pr_warn("Number of Board Id's more than Max Boards defined\n");
		return 0;
	}

	memset(&board_info_array[board_id_iterator], 0,
		sizeof(board_info_array[board_id_iterator]));
	strncpy(board_id, line, BOARD_ID_BUF_SIZE);
	board_id[BOARD_ID_BUF_SIZE - 1] = '\0';

	board_info_array[board_id_iterator].valid = 1;
	delimiter_index = strcspn(traverser, "-");
	memcpy(board_info_array[board_id_iterator].bom,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(board_info_array[board_id_iterator].project,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(board_info_array[board_id_iterator].sku,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(board_info_array[board_id_iterator].revision,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-\0");
	memcpy(board_info_array[board_id_iterator].bom_version,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	board_id_iterator++;
	return 1;
}

__setup("board_id=", board_id_setup);


static int __init sku_info_setup(char *line)
{
	memcpy(skuinfo_buffer, line, SKUINFO_BUF_SIZE);
	return 1;
}

__setup("nvsku=", sku_info_setup);

static int __init sku_ver_setup(char *line)
{
	memcpy(skuver_buffer, line, SKUVER_BUF_SIZE);
	return 1;
}

__setup("SkuVer=", sku_ver_setup);

static int __init prod_info_setup(char *line)
{
	memcpy(prodinfo_buffer, line, PRODINFO_BUF_SIZE);
	return 1;
}

__setup("ProdInfo=", prod_info_setup);

static int __init prod_ver_setup(char *line)
{
	memcpy(prodver_buffer, line, PRODVER_BUF_SIZE);
	return 1;
}

__setup("ProdVer=", prod_ver_setup);

static int read_skuinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuinfo_buffer);
	return 0;
}

static int read_skuinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuinfo_proc_show, NULL);
}

static const struct file_operations read_skuinfo_proc_fops = {
	.open = read_skuinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_skuver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuver_buffer);
	return 0;
}

static int read_skuver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuver_proc_show, NULL);
}

static const struct file_operations read_skuver_proc_fops = {
	.open = read_skuver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_serialinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%013llu\n",
		   (((unsigned long long int)board_serial_high) << 32) |
		   board_serial_low);
	return 0;
}

static int read_serialinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_serialinfo_proc_show, NULL);
}

static const struct file_operations read_serialinfo_proc_fops = {
	.open = read_serialinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_prodinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodinfo_buffer);
	return 0;
}

static int read_prodinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodinfo_proc_show, NULL);
}

static const struct file_operations read_prodinfo_proc_fops = {
	.open = read_prodinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_prodver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodver_buffer);
	return 0;
}

static int read_prodver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodver_proc_show, NULL);
}

static const struct file_operations read_prodver_proc_fops = {
	.open = read_prodver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_init_board_info(void)
{
	board_serial_low = system_serial_low;
	board_serial_high = system_serial_high;

	if (!proc_create("board_serial", 0, NULL, &read_serialinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for board_serial!\n");
		return -1;
	}

	if (!proc_create("skuinfo", 0, NULL, &read_skuinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuinfo!\n");
		return -1;
	}

	if (!proc_create("skuver", 0, NULL, &read_skuver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuver!\n");
		return -1;
	}

	if (!proc_create("prodinfo", 0, NULL, &read_prodinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodinfo!\n");
		return -1;
	}

	if (!proc_create("prodver", 0, NULL, &read_prodver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodver!\n");
		return -1;
	}

	return 0;
}

bool tegra_is_board(const char *bom, const char *project,
		const char *sku, const char *revision,
		const char *bom_version) {
	int i = 0;
	bool b = true;

	while (i < TEGRA_MAX_BOARDS && board_info_array[i].valid != 0) {

		b = (!bom || !strncmp(board_info_array[i].bom,
				bom, MAX_BUFFER)) &&
			(!project || !strncmp(board_info_array[i].project,
				project, MAX_BUFFER)) &&
			(!sku || !strncmp(board_info_array[i].sku,
				sku, MAX_BUFFER)) &&
			(!revision || !strncmp(board_info_array[i].revision,
				revision, MAX_BUFFER)) &&
			(!bom_version ||
			 !strncmp(board_info_array[i].bom_version,
				bom_version, MAX_BUFFER));

		if (b)
			return true;
		i++;
	}
	return false;
}
EXPORT_SYMBOL(tegra_is_board);
