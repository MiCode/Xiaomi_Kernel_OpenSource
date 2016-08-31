/*
 * include/linux/tegra_pflash.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 */

#ifndef _TEGRA_PFLASH_H_
#define _TEGRA_PFLASH_H_

#define MAX_CHIPS 16
struct pflash_data {
	char *buffer[MAX_CHIPS];
	unsigned long chip_ofs[MAX_CHIPS];
	unsigned int chip_numbyte[MAX_CHIPS];
	char start_prog[MAX_CHIPS];
	char chip_erase[MAX_CHIPS];
	unsigned long chip_erase_size[MAX_CHIPS];
};
struct nv_flash_data {
	unsigned long erasesize;
	unsigned long chipsize;
	int cmd_set_type;
	unsigned long long flash_total_size;
	int max_num_chips;
};
#define ERASESIZE 0x80000
#define COMMAND_SET_TYPE 0x02

/* IOCTL numbers */
#define PFLASH_MAGIC  0x85
#define PFLASH_CFI2 _IOW(PFLASH_MAGIC, 2, struct pflash_data)
#define PFLASH_GET_MAXCHIPS _IO(PFLASH_MAGIC, 3)
#define PFLASH_CHIP_INFO _IOR(PFLASH_MAGIC, 4, struct nv_flash_data)

#define CHIP_BUFFER_SIZE 1048576
#define AMD_WRITE_BUFFER_SIZE 256
#define FLASH_ERASED_VALUE 0xffffffff

static int pflash_major;
static struct cdev pflash_cdev;
static struct class *pflash_class;

#define PFLASH_DEVICE "pflash"
#define PFLASH_DEVICE_NO 1
#define PFLASH_MAJOR 0 /* Get dynamically */
#define PFLASH_MINOR 0

/* extern struct pflash_device pflash_device_info; */
static unsigned int flash_num_chips;
struct nv_flash_data flash_data;

#define pflash_nor_write(map, write_value, sectaddr) \
({ tegra_nor_write(map, write_value, sectaddr); mb(); })

#define pflash_nor_read(map, sectaddr) \
		({ map_word __readval;  \
		__readval = tegra_nor_read(map, sectaddr); \
		mb(); __readval; })


#endif
