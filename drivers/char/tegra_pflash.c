/*
 * drivers/char/tegra_pflash.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mtd/gen_probe.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/map.h>
#include <linux/tegra_snor.h>
#include <linux/platform_data/tegra_nor.h>
#include <linux/mutex.h>
#include <linux/tegra_pflash.h>

/* Please extend above function to support diffrent bank widths */

static inline map_word convert_map_word(const unsigned long data)
{
	map_word r;

	r.x[0] = data;
	return r;
}

static inline int cfi2_chip_ready(unsigned int index, unsigned long sectAddr)
{

	struct map_info *map;
	map_word first_read, second_read;

	map = get_map_info(index);
	first_read = pflash_nor_read(map, sectAddr);
	second_read = pflash_nor_read(map, sectAddr);

	pr_debug("Consecutive reads @ physical address" \
			"0x%lx gave 0x%lx and 0x%lx\n",
			(flash_data.chipsize*index)+sectAddr,
			first_read.x[0],
			second_read.x[0]);

	if (map_word_equal(map, first_read, second_read))
		return 1;
	else
		return 0;
}

static inline int cfi2_chip_good(unsigned int index,  unsigned long sectAddr,
							unsigned long expected)
{
	struct map_info *map;
	map_word first_read, second_read;

	map = get_map_info(index);
	mb();
	first_read = pflash_nor_read(map, sectAddr);
	second_read = pflash_nor_read(map, sectAddr);

	pr_debug("Consecutive reads @ physical address " \
			"0x%lx gave 0x%lx and 0x%lx\n",
	(flash_data.chipsize*index)+sectAddr, first_read.x[0],
							second_read.x[0]);

	pr_debug("Expected value : 0x%lx\n", expected);

	if (((map_word_equal(map, first_read, second_read))) &&
						(second_read.x[0] == expected))
		return 1;

	cfi_udelay(100);
	return 0;
}

static void cfi2_write_buffer(unsigned int index, unsigned long sectAddr,
								void *buffer)
{
	int i;
	struct map_info *map;
	map_word datum;
	unsigned long *data = (unsigned long *)buffer;

	map = get_map_info(index);
	pflash_nor_write(map, convert_map_word(0x00AA00AA),
					0x555 * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00550055),
					0x2AA * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00250025), sectAddr);
	pflash_nor_write(map, convert_map_word(0x00FF00FF), sectAddr);

	/* move length -words of data to the chip */
	for (i = 0; i < AMD_WRITE_BUFFER_SIZE; i++) {
		datum = map_word_load(map, data);
		pflash_nor_write(map, datum, sectAddr + (i*4));
		data++;
	}

	/* Command to start flushing buffer to sectors */
	pflash_nor_write(map, convert_map_word(0x00290029), sectAddr);
}


static void cfi2_erase_block(unsigned int index, unsigned long sectAddr)
{

	struct map_info *map;

	map = get_map_info(index);
	pflash_nor_write(map, convert_map_word(0x00AA00AA),
						0x555 * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00550055),
						0x2AA * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00800080),
						0x555 * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00AA00AA),
						0x555 * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00550055),
						0x2AA * map->bankwidth);
	pflash_nor_write(map, convert_map_word(0x00300030), sectAddr);

}

static char *write_buffer[MAX_CHIPS];

static long parallel_flash_writer_ioctl(struct file *filep,
				unsigned int ioctlno, unsigned long addr)
{
	unsigned int chip_no, blk_no;
	struct pflash_data pdata;
	unsigned long buf_no, timeout, loc, status;

	switch (ioctlno) {
	case PFLASH_CFI2:
		if (copy_from_user(&pdata, (void *)addr,
					sizeof(struct pflash_data)))
			return -EFAULT;

		/* Reject the buffer if not multiple of 1MB */
		for (chip_no = 0; chip_no < flash_num_chips; chip_no++) {
			if ((pdata.chip_numbyte[chip_no] & (SZ_1M - 1)) != 0) {
				printk(KERN_ERR"Buffer is not 1MB aligned." \
					"Cannot write buffer to flash.\n");
				return -EIO;
			}
		}
		/* Get user space buffers */
		for (chip_no = 0; chip_no < flash_num_chips; chip_no++) {

			if (!pdata.chip_erase[chip_no])
				continue;

			if (copy_from_user(write_buffer[chip_no],
						(void *)pdata.buffer[chip_no],
						CHIP_BUFFER_SIZE))
					return -EFAULT;
		}

		/* Send erase command to all relevent sectors */
		for (blk_no = 0; blk_no < CHIP_BUFFER_SIZE;
					blk_no += flash_data.erasesize) {
			/* Give erase command to all chips */
			for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {
				pr_debug("Giving erase commands to all chips.\n");
				/* Give erase only if the sector is required */
				if (!pdata.chip_erase[chip_no])
					continue;

				cfi2_erase_block(chip_no,
						blk_no +
						pdata.chip_ofs[chip_no]);

				pdata.start_prog[chip_no] = 0;
			}

			pr_debug("Erase command sent," \
				"checking whether chip is ready\n");

			/* 875ms is document as per datasheet */
			mdelay(1500);

			/* Check the status of erase
			 *  Wait for chip to be ready */

			for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {
				if (!pdata.chip_erase[chip_no])
					continue;

				/* timeout is 4 seconds */
				timeout = jiffies + 4 * HZ;
				do {
					loc = blk_no + pdata.chip_ofs[chip_no];
					status = cfi2_chip_ready(chip_no,
								loc);
				} while (!status && time_after(jiffies,
								timeout));

				if (!status)
					return -EIO;
			}

			/* check erase is successful */
			for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {
				if (!pdata.chip_erase[chip_no])
					continue;
				/* timeout is 10 seconds */
				timeout = jiffies + 10 * HZ;

				do {

					loc = blk_no + pdata.chip_ofs[chip_no];
					status = cfi2_chip_good(chip_no,
							loc,
							FLASH_ERASED_VALUE);

				} while (!status && time_after(jiffies,
								timeout));
				if (!status)
					return -EIO;

				/* Declare chip OK to be programmed */
				pdata.start_prog[chip_no] = 1;
			}

			pr_debug("Erase over. Giving program commands\n");
			/* For writing to one sector to flash,
				program all chips erase/buff_size times */
			for (buf_no = 0; buf_no < flash_data.erasesize;
					buf_no += AMD_WRITE_BUFFER_SIZE *
						sizeof(unsigned long)) {
				/* Send program buffer command to all chips */
				for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {

					if (!pdata.start_prog[chip_no])
						continue;

					loc = blk_no + buf_no;
					cfi2_write_buffer(chip_no,
						pdata.chip_ofs[chip_no] + loc,
						write_buffer[chip_no] + loc);
					}
				/* Giving delay of more than 340us for
							program to complete */
				cfi_udelay(1500);

				pr_debug("Program commands sent." \
					"Checking whether chip is ready\n");

				/* Check status of chip */
				for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {
					if (!pdata.start_prog[chip_no])
						continue;

					/* Timeout set to 4 second */
					timeout = jiffies + 4 * HZ;
					do {
						loc = pdata.chip_ofs[chip_no] +
							blk_no + buf_no;
						status = cfi2_chip_ready(
								chip_no, loc);

					} while (!status && time_after(jiffies,
								timeout));

					if (!status)
						return -EIO;
				}


				/* Verify atleast first word is written */
				for (chip_no = 0; chip_no < flash_num_chips;
								chip_no++) {

					if (!pdata.start_prog[chip_no])
						continue;

					/* Timeout set to 7 second */
					timeout = jiffies + 7 * HZ;
					do {
						unsigned long data;
						loc = blk_no + buf_no;
						data = *(unsigned long *)
							((unsigned long)
							write_buffer[chip_no] +
							loc);

						status = cfi2_chip_good(
							chip_no,
							pdata.chip_ofs[chip_no]
							+ loc,
							data);
					} while (!status && time_after(jiffies,
								timeout));
					if (!status)
						return -EIO;
					mb();
				}
				mb();
			}
			pr_debug("Programming complete." \
					"Going for next sector\n");
		}
		break;

	case PFLASH_GET_MAXCHIPS:
		return flash_num_chips;

	case PFLASH_CHIP_INFO:
		if (copy_to_user((void *)addr,
					(void *)&flash_data,
					sizeof(struct nv_flash_data)))
			return -EFAULT;
	}
	return 0;
}

static int parallel_flash_writer_open(struct inode *inp, struct file *filep)
{
	int chip_no;
	struct map_info *map;

	/* Get map info of first bank */
	map = get_map_info(0);

	flash_data.chipsize = map->size;
	flash_data.max_num_chips = get_maps_no();
	flash_data.erasesize = ERASESIZE;
	flash_data.flash_total_size = getflashsize();
	flash_data.cmd_set_type = COMMAND_SET_TYPE;

	/* Fill up bank_list, device_list (GLOBALS) */
	flash_num_chips = get_maps_no();

	for (chip_no = 0; chip_no < flash_num_chips; chip_no++) {
		write_buffer[chip_no] = kmalloc(CHIP_BUFFER_SIZE, GFP_KERNEL);

		if (!write_buffer[chip_no])
			return -ENOMEM;
	}

	return 0;
}

static int parallel_flash_writer_release(struct inode *inp, struct file *filep)
{
	int chip_no;

	for (chip_no = 0; chip_no < flash_num_chips; chip_no++)
		kfree(write_buffer[chip_no]);
	return 0;
}

static const struct file_operations pflash_fops = {
	.owner = THIS_MODULE,
	.open = parallel_flash_writer_open,
	.release = parallel_flash_writer_release,
	.unlocked_ioctl = parallel_flash_writer_ioctl,
};

static void pflash_cleanup(void)
{
	cdev_del(&pflash_cdev);
	device_destroy(pflash_class, MKDEV(pflash_major, PFLASH_MINOR));

	if (pflash_class)
		class_destroy(pflash_class);

	unregister_chrdev_region(MKDEV(pflash_major, PFLASH_MINOR),
							PFLASH_DEVICE_NO);
}

static int __init parallel_flash_writer_init(void)
{
	int result;
	int ret = -ENODEV;
	dev_t pflash_dev ;

	printk(KERN_INFO "Pflash driver.\n");
	result = alloc_chrdev_region(&pflash_dev, 0,
			PFLASH_DEVICE_NO, PFLASH_DEVICE);

	pflash_major = MAJOR(pflash_dev);

	if (result < 0) {
		printk(KERN_ERR "alloc_chrdev_region() failed for pflash\n");
		goto fail_err;
	}

	/* Register a character device. */
	cdev_init(&pflash_cdev, &pflash_fops);
	pflash_cdev.owner = THIS_MODULE;
	pflash_cdev.ops = &pflash_fops;
	result = cdev_add(&pflash_cdev, pflash_dev, PFLASH_DEVICE_NO);

	if (result < 0)
		goto fail_chrdev;

	pflash_class = class_create(THIS_MODULE, PFLASH_DEVICE);

	if (IS_ERR(pflash_class)) {
		pr_err(KERN_ERR "pflash: device class file already in use.\n");
		pflash_cleanup();
		return PTR_ERR(pflash_class);
	}

	device_create(pflash_class, NULL,
				MKDEV(pflash_major, PFLASH_MINOR),
					NULL, "%s", PFLASH_DEVICE);

	return 0;

fail_chrdev:
	unregister_chrdev_region(pflash_dev, PFLASH_DEVICE_NO);

fail_err:
	return ret;
}

static void __exit parallel_flash_writer_exit(void)
{
	pflash_cleanup();
}

module_init(parallel_flash_writer_init);
module_exit(parallel_flash_writer_exit);
MODULE_AUTHOR("Ashutosh Patel <ashutoshp@nvidia.com>");
MODULE_LICENSE("GPL v2");
