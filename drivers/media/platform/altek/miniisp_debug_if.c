/*
 * File: miniisp_debug_if.c
 * Description: mini ISP debug daemon interface
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2017/04/20; PhenixChen; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/******Include File******/
#include "include/miniisp.h"
#include "include/miniisp_debug_if.h"
#include "include/miniisp_customer_define.h"

#include <linux/uaccess.h> /* copy_*_user() */
#include <linux/fs.h>	   /* chrdev, struct file_operations */
#include <linux/cdev.h>	   /* cdev_add()/cdev_del() */
#include <linux/mm.h>
#include <linux/delay.h> /*msleep()*/

#if EN_605_IOCTRL_INTF
#include "include/miniisp_ctrl_intf.h"
#include <miniISP/miniISP_ioctl.h>
#endif

/******Private Constant Definition******/
#define MINI_ISP_LOG_TAG	"[miniisp_debug_if]"
#define MAX_RW_SIZE		(64 * 1024)

/******Private Function Prototype******/
static int miniisp_dev_open(struct inode*, struct file*);
static int miniisp_dev_release(struct inode *inode, struct file *filp);
static long miniisp_dev_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg);
static int miniisp_dev_map(struct file *filp, struct vm_area_struct *vma);
static u32 miniisp_debugif_memory_read(u32 start_addr, u32 len);
static u32 miniisp_debugif_RegBulk_read(u32 start_addr, u32 end_reg_addr);
static long miniisp_debugif_get_mem(u32 len);
static long miniisp_debugif_munmap_done(void);
static long handle_RegCmd(unsigned int cmd, unsigned long arg);
static long handle_MemCmd(unsigned int cmd, unsigned long arg);
static long handle_BulkCmd(unsigned int cmd, unsigned long arg);

/******Public Function Prototype******/
struct device *miniisp_chdev_create(struct class *mini_isp_class);

/******Private Global Variable******/
static int miniisp_dev_major;
static int miniisp_dev_minor;
static struct cdev *miniisp_dev_cdev;
static unsigned long *allocated_memmory_align;
static unsigned long *allocated_memmory;
static unsigned long allocated_size;
static u8 During_Refresh_Module;

struct dbg_drvdata {
	/* u32 val; */
	u32 reg_tbl[4]; /* for Test */
	/* unsigned char mem[16]; */
	rwlock_t lock;
};

static const struct file_operations miniisp_dev_fops = {
	.owner = THIS_MODULE,
	.open = miniisp_dev_open,
	.release = miniisp_dev_release,
	.mmap =  miniisp_dev_map,
	/* .read = dev_read, */
	/* .write = dev_write, */

	.unlocked_ioctl = miniisp_dev_ioctl,
#if EN_605_IOCTRL_INTF
	.compat_ioctl = miniisp_dev_ioctl,
#endif
};

static long miniisp_debugif_munmap_done(void)
{
	unsigned long virt_addr;

	misp_err("%s:called", __func__);
	if (allocated_memmory_align) {
		for (virt_addr = (unsigned long)allocated_memmory_align;
			virt_addr < (unsigned long)allocated_memmory_align
				+ allocated_size; virt_addr += PAGE_SIZE)
			ClearPageReserved(virt_to_page(virt_addr));
		kfree(allocated_memmory);
		allocated_memmory = NULL;
		allocated_memmory_align = NULL;
		allocated_size = 0;
	}
	return 0;
}

static long miniisp_debugif_get_mem(u32 len)
{
	long retval = 0;
	unsigned long virt_addr;
	unsigned long mask = ~(PAGE_SIZE - 1);

	/* page-aligned memory size */
	allocated_size = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	misp_err("%s:memory length = %u, page-aligned memory size = %lu",
		__func__, len, allocated_size);

	/* allocate page-aligned memory */
	allocated_memmory = kzalloc(allocated_size + PAGE_SIZE - 1, GFP_KERNEL);
	if (!allocated_memmory) {
		retval = -ENOMEM;
		misp_err("%s:kzalloc failed", __func__);
		goto done;
	}
	/* printf指標的位址通常使用%x，但它的寬度是 32-bit，
	無法在 64-bit 環境下顯示正確結果，需改用%p */
	misp_err("%s:allocated_memmory before align = %p",
		__func__, allocated_memmory);
	allocated_memmory_align = (unsigned long *)
		(((unsigned long)allocated_memmory + PAGE_SIZE - 1) & mask);
	misp_err("%s:allocated_memmory after align = %p",
		__func__, allocated_memmory_align);
	misp_err("%s:kzalloc succeed, total allocated_size = %lu",
		__func__, allocated_size + PAGE_SIZE - 1);

	/* reserve all pages to make them remapable */
	for (virt_addr = (unsigned long)allocated_memmory_align;
		virt_addr <	((unsigned long)allocated_memmory_align
			+ allocated_size);
						virt_addr += PAGE_SIZE)
		SetPageReserved(virt_to_page(virt_addr));
done:
	return retval;
}


static int miniisp_dev_map(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long page, pos;
	unsigned long start = (unsigned long)vma->vm_start;
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

	misp_err("%s:called", __func__);

	if (size > (unsigned long)allocated_size) {
		misp_err("%s:ERROR1 mapsz = %lu, allocated_size = %lu",
			__func__, size, (unsigned long)allocated_size);
		return -EINVAL;
	}
	pos = (unsigned long)allocated_memmory_align;
	page = virt_to_phys((void *)pos);

	if (remap_pfn_range(vma,
		start, page >> PAGE_SHIFT, size, PAGE_SHARED)) {
		misp_err("%s:ERROR2", __func__);
		return -EAGAIN;
	}
	return 0;
}

static u32 miniisp_debugif_RegBulk_read(u32 start_reg_addr, u32 end_reg_addr)
{
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;
	u32 count;
	u32 rx_dummy_len;
	u8 *send_buffer;
	u8 *recv_buffer;
	u8 *io_buffer = NULL;
	u32 io_size;
	u8 *dump_memory = NULL;
	u8 *keep_dump_memory = NULL;
	u32 ouput_size;
	u8 ctrlbyte = CTRL_BYTE_REGRD;

	/* prepare for mmap >>>*/
	long retval = 0;
	u8 *mmap_addr;

	/*how many registers(4 bytes) do you want to read?*/
	count = ((end_reg_addr - start_reg_addr) / 4) + 1;
	ouput_size = (count + 2) * 4; /* read 4 bytes register value */

	retval = miniisp_debugif_get_mem(ouput_size);
	if (retval)
		goto mini_isp_register_read_end;
	mmap_addr = (u8 *)allocated_memmory_align;
	/* prepare for mmap <<<*/

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);


	rx_dummy_len = mini_isp_get_rx_dummy_byte(ctrlbyte);

#if EN_SPIE_REG_DUMMY_BYTE
	mini_isp_set_spie_dummy_byte(rx_dummy_len-1); /* 0 base */
#endif

	/* read 4 bytes register value at a time */
	io_size = EMODE_TXCMD_LEN + rx_dummy_len + 4;

	io_buffer = kzalloc(io_size, GFP_KERNEL);
	if (!io_buffer) {
		retval = -ENOMEM;
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_memory_fail;
	}

	dump_memory = kzalloc(ouput_size, GFP_KERNEL);
	if (!dump_memory) {
		retval = -ENOMEM;
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_memory_fail;
	}
	keep_dump_memory = dump_memory;

	send_buffer = io_buffer;
	recv_buffer = io_buffer + EMODE_TXCMD_LEN;

	memcpy(dump_memory, &start_reg_addr, 4);
	dump_memory = dump_memory + 4;
	memcpy(dump_memory, &count, 4);
	dump_memory = dump_memory + 4;

	while (start_reg_addr <= end_reg_addr) {

		memset(io_buffer, 0, io_size);
		memcpy(send_buffer, &ctrlbyte, 1);
		memcpy(send_buffer + 1, &start_reg_addr, 4);

		retval = devdata->intf_fn->read((void *)devdata,
					send_buffer, EMODE_TXCMD_LEN,
					recv_buffer, rx_dummy_len + 4);

		if (retval)
			goto mini_isp_register_read_get_fail;

		if (dev_global_variable->intf_status & INTF_SPI_READY) {
			/* check if send len + recv len > 32. if yes then
		       rx_dummy_len need + EMODE_TXCMD_LEN */
			if (EMODE_TXCMD_LEN + rx_dummy_len + 4 > 32)
				retval = mini_isp_check_rx_dummy(
				&recv_buffer, rx_dummy_len+EMODE_TXCMD_LEN);
			else
				retval = mini_isp_check_rx_dummy(
					&recv_buffer, rx_dummy_len);

			if (retval)
				goto mini_isp_register_read_get_fail;
		}

		memcpy(dump_memory, recv_buffer, 4);
		start_reg_addr = start_reg_addr + 4;
		dump_memory = dump_memory + 4;
	}
	memcpy(mmap_addr, keep_dump_memory, ouput_size);
	kfree(io_buffer);
	kfree(keep_dump_memory);
	goto mini_isp_register_read_end;

mini_isp_register_read_get_fail:
	misp_err("%s read failed.", __func__);
allocate_memory_fail:
	misp_err("%s Allocate memory failed.", __func__);
	kfree(io_buffer);
	kfree(keep_dump_memory);
	miniisp_debugif_munmap_done();
mini_isp_register_read_end:
	return retval;
}

static u32 miniisp_debugif_memory_read(u32 start_addr, u32 len)
{
	/* prepare for mmap >>>*/
	long retval = 0;
	u8 *mmap_addr;

	retval = miniisp_debugif_get_mem(len);
	if (retval) {
		misp_err("%s alloc fail", __func__);
		goto mini_isp_memory_read_fail;
	}
	mmap_addr = (u8 *)allocated_memmory_align;
	/* prepare for mmap <<<*/

	retval = mini_isp_memory_read(start_addr, mmap_addr, len);
	if (retval) {
		misp_err("%s read fail", __func__);
		miniisp_debugif_munmap_done();
		goto mini_isp_memory_read_fail;
	}
	misp_err("%s dump_finish", __func__);

mini_isp_memory_read_fail:
	return retval;
}

static long handle_PrepareCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	u8 state; /* 1:start, 0:done */

	if (copy_from_user(&state, (int __user *)arg, sizeof(state))) {
		retval = -EFAULT;
		goto done;
	}
	switch (cmd) {
	case IOCTL_REFRESH_MODULE:
		if (state == 1) {
			During_Refresh_Module = 1;
			mini_isp_a_to_e();
		} else if (state == 0) {
			mini_isp_e_to_a();
			During_Refresh_Module = 0;
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
done:
	return retval;
}

static long handle_InfoCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct irp_and_depth_information data;

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
		retval = -EFAULT;
		goto done;
	}

	mini_isp_a_to_e();

	switch (cmd) {
	case IOCTL_IRP_DEPTH_INFO:
		misp_err("%s:IOCTL_IRP_DEPTH_INFO", __func__);
		retval = mini_isp_utility_get_irp_and_depth_information(&data);
		if (retval) {
			misp_err("%s:miniisp driver spi error, retval = %u",
				__func__, (u32)retval);
			retval = -EIO;
			break;
		}
		if (copy_to_user((int __user *)arg, &data, sizeof(data)))
			retval = -EFAULT;
		break;
	default:
		retval = -ENOTTY;
		break;
	}

	mini_isp_e_to_a();
done:
	return retval;
}

static long handle_RegCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	u32 val = 0xFFFFFFFF;
	struct ioctl_regRW_cmd data;

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
		retval = -EFAULT;
		goto done;
	}

	if (!During_Refresh_Module)
		mini_isp_a_to_e();

	switch (cmd) {
	case IOCTL_REGREAD:
		misp_err("%s:IOCTL_REGREAD RegAddr = %#04x",
			__func__, data.RegAddr);
		retval = mini_isp_register_read(data.RegAddr, &val);
		if (val == 0xFFFFFFFF) {
			if (retval < 0)
				misp_err("%s:miniisp driver sync_error, retval = %u",
					__func__, (u32)retval);
			else
				misp_err("%s:miniisp driver can't get register.",
					__func__);
			retval = -EIO;
			break;
		}
		data.RegVal = val;
		if (copy_to_user((int __user *)arg, &data, sizeof(data)))
			retval = -EFAULT;
		break;
	case IOCTL_REGWRITE:
		misp_err("%s:IOCTL_REGWRITE RegAddr = %#04x; val = %#04x",
			 __func__, data.RegAddr, data.RegVal);
		mini_isp_register_write(data.RegAddr, data.RegVal);
		break;
	default:
		retval = -ENOTTY;
		break;
	}

	if (!During_Refresh_Module)
		mini_isp_e_to_a();

done:
	return retval;
}


static long handle_MemCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct ioctl_memRW_cmd data;

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
		retval = -EFAULT;
		goto done;
	}

	if (data.len > MAX_RW_SIZE) {
		misp_err("%s:Invalid length, MemAddr = %#04x len = %u",
			__func__, data.MemAddr, data.len);
		retval = -EFAULT;
		goto done;
	}

	if (!During_Refresh_Module)
		mini_isp_a_to_e();

	switch (cmd) {
	case IOCTL_MEMREAD:
		misp_err("%s:IOCTL_MEMREAD MemAddr = %#04x len = %u",
			__func__, data.MemAddr, data.len);
		retval = miniisp_debugif_memory_read(data.MemAddr, data.len);
		break;
	case IOCTL_MUNMAP_DONE:
		misp_err("%s:IOCTL_MMAPDONE", __func__);
		retval = miniisp_debugif_munmap_done();
		break;
	case IOCTL_MEMGET:
		misp_err("%s:IOCTL_MEMGET len = %u", __func__, data.len);
		retval = miniisp_debugif_get_mem(data.len);
		break;
	case IOCTL_MEMWRITE:
		misp_err("%s:IOCTL_MEMWRITE MemAddr = %#04x len = %u",
			__func__, data.MemAddr, data.len);
		mini_isp_memory_write(data.MemAddr,
			(u8 *)allocated_memmory_align, data.len);
		break;
	default:
		retval = -ENOTTY;
		break;
	}

	if (!During_Refresh_Module)
		mini_isp_e_to_a();

done:
	return retval;
}

static long handle_BulkCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct ioctl_regBulkRW_cmd data;

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
		retval = -EFAULT;
		goto done;
	}

	if ((data.StartAddr > data.EndAddr) ||
		((data.EndAddr - data.StartAddr) > MAX_RW_SIZE)) {
		misp_err("%s:Invalid address, StartAddr = %#04x EndAddr = %#04x",
			__func__, data.StartAddr, data.EndAddr);
		retval = -EFAULT;
		goto done;
	}

	if (!During_Refresh_Module)
		mini_isp_a_to_e();

	switch (cmd) {
	case IOCTL_REGBULKREAD:
		misp_err("%s:IOCTL_REGBULKREAD StartAddr = %#04x EndAddr = %#04x",
			__func__, data.StartAddr, data.EndAddr);
		retval = miniisp_debugif_RegBulk_read(
			data.StartAddr, data.EndAddr);
		misp_err("%s:IOCTL_REGBULKREAD end", __func__);
		break;
	default:
		retval = -ENOTTY;
		break;
	}
	if (!During_Refresh_Module)
		mini_isp_e_to_a();

done:
	return retval;
}


static long miniisp_dev_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;

	misp_err("%s: IOCTL cmd:0x%08x", __func__, cmd);
	/*
	if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
		retval = -EFAULT;
		goto done;
	}
	*/
	switch (_IOC_TYPE(cmd)) {
	case IOC_REGCMD_MAGIC:
		retval = handle_RegCmd(cmd, arg);
		break;
	case IOC_MEMCMD_MAGIC:
		retval = handle_MemCmd(cmd, arg);
		break;
	case IOC_BULKCMD_MAGIC:
		retval = handle_BulkCmd(cmd, arg);
		break;
	case IOC_INFOCMD_MAGIC:
		retval = handle_InfoCmd(cmd, arg);
		break;
	case IOC_PREPARECMD_MAGIC:
		retval = handle_PrepareCmd(cmd, arg);
		break;
#if EN_605_IOCTRL_INTF
	case IOC_ISP_CTRL_FLOW_MAGIC:
		retval = handle_ControlFlowCmd(cmd, arg);
		break;
#endif
	default:
		retval = -ENOTTY;
		break;
	}
	return retval;
}

static int miniisp_dev_open(struct inode *inode, struct file *filp)
{
	struct dbg_drvdata *p;

	misp_err("%s:major %d minor %d (pid %d)", __func__,
		imajor(inode), iminor(inode), current->pid);
	inode->i_private = inode;

	p = kmalloc(sizeof(struct dbg_drvdata), GFP_KERNEL);
	if (p == NULL) {
		/* misp_err("%s:No memory", __func__); */
		return -ENOMEM;
	}

	memset(p, 0, sizeof(*p));

	rwlock_init(&p->lock);
	filp->private_data = p;

	misp_err("%s:inode->i_private = %p, filp->private_data = %p",
		__func__, inode->i_private, filp->private_data);
	return 0;
}

static int miniisp_dev_release(struct inode *inode, struct file *filp)
{
	misp_err("%s:major %d, minor %d, pid %d",
		__func__, imajor(inode), iminor(inode), current->pid);
	misp_err("%s:inode->i_private = %p, filp->private_data = %p",
		__func__, inode->i_private, filp->private_data);
	kfree(filp->private_data);
	return 0;
}

struct device *miniisp_chdev_create(struct class *mini_isp_class)
{
	dev_t miniisp_dev_nr;  /* device number */
	int ret = 0;
	struct device *mini_isp_dev;

	/* alloc major number */
	if (alloc_chrdev_region(
		&miniisp_dev_nr, 0, 1, "mini_isp_device") != 0) {
		misp_err("%s:Allocate major number failed", __func__);
		mini_isp_dev = device_create(
			mini_isp_class, NULL, 0, NULL, "mini_isp_device");
	} else {
		misp_err("%s:Allocate major number succeed", __func__);
		mini_isp_dev = device_create(mini_isp_class, NULL,
			miniisp_dev_nr, NULL, "mini_isp_device");

		miniisp_dev_major = MAJOR(miniisp_dev_nr);
		miniisp_dev_minor = MINOR(miniisp_dev_nr);
		misp_err("%s:register chrdev(%d,%d)", __func__,
			miniisp_dev_major, miniisp_dev_minor);
		/* alloc struct cdev */
		/* kmalloc(sizeof(struct cdev), GFP_KERNEL);
		//alloc struct cdev */
		miniisp_dev_cdev = cdev_alloc();
		if (miniisp_dev_cdev == NULL) {
			misp_err("%s:kmalloc dev_cdev failed", __func__);
			return mini_isp_dev;
		} else {
			misp_err("%s:kmalloc dev_cdev succeed", __func__);
		}
		/* register handler to struct cdev */
		/* cdev_init(miniisp_dev_cdev, &miniisp_dev_fops); */
		miniisp_dev_cdev->ops = &miniisp_dev_fops;
		miniisp_dev_cdev->owner = THIS_MODULE;
		/* register driver (struct cdev with file handler) to kernel */
		ret = cdev_add(miniisp_dev_cdev,
			MKDEV(miniisp_dev_major, miniisp_dev_minor), 1);
		if (ret < 0) {
			misp_err("%s:Add character device failed", __func__);

			kfree(miniisp_dev_cdev);
			miniisp_dev_cdev = NULL;
		} else {
			misp_err("%s:Add character device succeed", __func__);
		}
	}
	return mini_isp_dev;
}
