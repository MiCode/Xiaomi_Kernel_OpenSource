/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/uaccess.h>

#define DEBUG_MAX_RW_BUF 4096

/*
 * Preprocessor Definitions and Constants
 */
#define TZBSP_CPU_COUNT 0x02
/*
 * Number of VMID Tables
 */
#define TZBSP_DIAG_NUM_OF_VMID 16
/*
 * VMID Description length
 */
#define TZBSP_DIAG_VMID_DESC_LEN 7
/*
 * Number of Interrupts
 */
#define TZBSP_DIAG_INT_NUM  32
/*
 * Length of descriptive name associated with Interrupt
 */
#define TZBSP_MAX_INT_DESC 16
/*
 * VMID Table
 */
struct tzdbg_vmid_t {
	uint8_t vmid; /* Virtual Machine Identifier */
	uint8_t desc[TZBSP_DIAG_VMID_DESC_LEN];	/* ASCII Text */
};
/*
 * Boot Info Table
 */
struct tzdbg_boot_info_t {
	uint32_t entry_cnt;	/* Warmboot entry CPU Counter */
	uint32_t exit_cnt;	/* Warmboot exit CPU Counter */
	uint32_t warm_jmp_addr;	/* Last Warmboot Jump Address */
	uint32_t spare;	/* Reserved for future use. */
};
/*
 * Reset Info Table
 */
struct tzdbg_reset_info_t {
	uint32_t reset_type;	/* Reset Reason */
	uint32_t reset_cnt;	/* Number of resets occured/CPU */
};
/*
 * Interrupt Info Table
 */
struct tzdbg_int_t {
	/*
	 * Type of Interrupt/exception
	 */
	uint16_t int_info;
	/*
	 * Availability of the slot
	 */
	uint8_t avail;
	/*
	 * Reserved for future use
	 */
	uint8_t spare;
	/*
	 * Interrupt # for IRQ and FIQ
	 */
	uint32_t int_num;
	/*
	 * ASCII text describing type of interrupt e.g:
	 * Secure Timer, EBI XPU. This string is always null terminated,
	 * supporting at most TZBSP_MAX_INT_DESC characters.
	 * Any additional characters are truncated.
	 */
	uint8_t int_desc[TZBSP_MAX_INT_DESC];
	uint64_t int_count[TZBSP_CPU_COUNT]; /* # of times seen per CPU */
};
/*
 * Diagnostic Table
 */
struct tzdbg_t {
	uint32_t magic_num;
	uint32_t version;
	/*
	 * Number of CPU's
	 */
	uint32_t cpu_count;
	/*
	 * Offset of VMID Table
	 */
	uint32_t vmid_info_off;
	/*
	 * Offset of Boot Table
	 */
	uint32_t boot_info_off;
	/*
	 * Offset of Reset info Table
	 */
	uint32_t reset_info_off;
	/*
	 * Offset of Interrupt info Table
	 */
	uint32_t int_info_off;
	/*
	 * Ring Buffer Offset
	 */
	uint32_t ring_off;
	/*
	 * Ring Buffer Length
	 */
	uint32_t ring_len;
	/*
	 * VMID to EE Mapping
	 */
	struct tzdbg_vmid_t vmid_info[TZBSP_DIAG_NUM_OF_VMID];
	/*
	 * Boot Info
	 */
	struct tzdbg_boot_info_t  boot_info[TZBSP_CPU_COUNT];
	/*
	 * Reset Info
	 */
	struct tzdbg_reset_info_t reset_info[TZBSP_CPU_COUNT];
	uint32_t num_interrupts;
	struct tzdbg_int_t  int_info[TZBSP_DIAG_INT_NUM];
	/*
	 * We need at least 2K for the ring buffer
	 */
	uint8_t *ring_buffer;	/* TZ Ring Buffer */
};

/*
 * Enumeration order for VMID's
 */
enum tzdbg_stats_type {
	TZDBG_BOOT = 0,
	TZDBG_RESET,
	TZDBG_INTERRUPT,
	TZDBG_VMID,
	TZDBG_GENERAL,
	TZDBG_LOG,
	TZDBG_STATS_MAX,
};

struct tzdbg_stat {
	char *name;
	char *data;
};

struct tzdbg {
	void __iomem *virt_iobase;
	struct tzdbg_t *diag_buf;
	char *disp_buf;
	int debug_tz[TZDBG_STATS_MAX];
	struct tzdbg_stat stat[TZDBG_STATS_MAX];
};

static struct tzdbg tzdbg = {

	.stat[TZDBG_BOOT].name = "boot",
	.stat[TZDBG_RESET].name = "reset",
	.stat[TZDBG_INTERRUPT].name = "interrupt",
	.stat[TZDBG_VMID].name = "vmid",
	.stat[TZDBG_GENERAL].name = "general",
	.stat[TZDBG_LOG].name = "log",
};


/*
 * Debugfs data structure and functions
 */

static int _disp_tz_general_stats(void)
{
	int len = 0;

	len += snprintf(tzdbg.disp_buf + len, DEBUG_MAX_RW_BUF - 1,
			"   Version        : 0x%x\n"
			"   Magic Number   : 0x%x\n"
			"   Number of CPU  : %d\n",
			tzdbg.diag_buf->version,
			tzdbg.diag_buf->magic_num,
			tzdbg.diag_buf->cpu_count);
	tzdbg.stat[TZDBG_GENERAL].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_vmid_stats(void)
{
	int i, num_vmid;
	int len = 0;
	struct tzdbg_vmid_t *ptr;

	ptr = (struct tzdbg_vmid_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->vmid_info_off);
	num_vmid = ((tzdbg.diag_buf->boot_info_off -
				tzdbg.diag_buf->vmid_info_off)/
					(sizeof(struct tzdbg_vmid_t)));

	for (i = 0; i < num_vmid; i++) {
		if (ptr->vmid < 0xFF) {
			len += snprintf(tzdbg.disp_buf + len,
				(DEBUG_MAX_RW_BUF - 1) - len,
				"   0x%x        %s\n",
				(uint32_t)ptr->vmid, (uint8_t *)ptr->desc);
		}
		if (len > (DEBUG_MAX_RW_BUF - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}

	tzdbg.stat[TZDBG_VMID].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_boot_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_boot_info_t *ptr;

	ptr = (struct tzdbg_boot_info_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->boot_info_off);

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		len += snprintf(tzdbg.disp_buf + len,
				(DEBUG_MAX_RW_BUF - 1) - len,
				"  CPU #: %d\n"
				"     Warmboot jump address     : 0x%x\n"
				"     Warmboot entry CPU counter: 0x%x\n"
				"     Warmboot exit CPU counter : 0x%x\n",
				i, ptr->warm_jmp_addr, ptr->entry_cnt,
				ptr->exit_cnt);

		if (len > (DEBUG_MAX_RW_BUF - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}
	tzdbg.stat[TZDBG_BOOT].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_reset_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_reset_info_t *ptr;

	ptr = (struct tzdbg_reset_info_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->reset_info_off);

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		len += snprintf(tzdbg.disp_buf + len,
				(DEBUG_MAX_RW_BUF - 1) - len,
				"  CPU #: %d\n"
				"     Reset Type (reason)       : 0x%x\n"
				"     Reset counter             : 0x%x\n",
				i, ptr->reset_type, ptr->reset_cnt);

		if (len > (DEBUG_MAX_RW_BUF - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}

		ptr++;
	}
	tzdbg.stat[TZDBG_RESET].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_interrupt_stats(void)
{
	int i, j, int_info_size;
	int len = 0;
	int *num_int;
	unsigned char *ptr;
	struct tzdbg_int_t *tzdbg_ptr;

	num_int = (uint32_t *)((unsigned char *)tzdbg.diag_buf +
			(tzdbg.diag_buf->int_info_off - sizeof(uint32_t)));
	ptr = ((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->int_info_off);
	int_info_size = ((tzdbg.diag_buf->ring_off -
				tzdbg.diag_buf->int_info_off)/(*num_int));

	for (i = 0; i < (*num_int); i++) {
		tzdbg_ptr = (struct tzdbg_int_t *)ptr;
		len += snprintf(tzdbg.disp_buf + len,
				(DEBUG_MAX_RW_BUF - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr->int_num,
				(uint32_t)tzdbg_ptr->int_info,
				(uint8_t *)tzdbg_ptr->int_desc);
		for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
			len += snprintf(tzdbg.disp_buf + len,
				(DEBUG_MAX_RW_BUF - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr->int_count[j]);
		}
		len += snprintf(tzdbg.disp_buf + len, DEBUG_MAX_RW_BUF - 1,
									"\n");

		if (len > (DEBUG_MAX_RW_BUF - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}

		ptr += int_info_size;
	}
	tzdbg.stat[TZDBG_INTERRUPT].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_log_stats(void)
{
	int len = 0;
	unsigned char *ptr;

	ptr = (unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->ring_off;
	len += snprintf(tzdbg.disp_buf, (DEBUG_MAX_RW_BUF - 1) - len,
							"%s\n", ptr);

	tzdbg.stat[TZDBG_LOG].data = tzdbg.disp_buf;
	return len;
}

static ssize_t tzdbgfs_read(struct file *file, char __user *buf,
	size_t count, loff_t *offp)
{
	int len = 0;
	int *tz_id =  file->private_data;

	memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						DEBUG_MAX_RW_BUF);
	switch (*tz_id) {
	case TZDBG_BOOT:
		len = _disp_tz_boot_stats();
		break;
	case TZDBG_RESET:
		len = _disp_tz_reset_stats();
		break;
	case TZDBG_INTERRUPT:
		len = _disp_tz_interrupt_stats();
		break;
	case TZDBG_GENERAL:
		len = _disp_tz_general_stats();
		break;
	case TZDBG_VMID:
		len = _disp_tz_vmid_stats();
		break;
	case TZDBG_LOG:
		len = _disp_tz_log_stats();
		break;
	default:
		break;
	}

	if (len > count)
		len = count;

	return simple_read_from_buffer(buf, len, offp,
				tzdbg.stat[(*tz_id)].data, len);
}

static int tzdbgfs_open(struct inode *inode, struct file *pfile)
{
	pfile->private_data = inode->i_private;
	return 0;
}

const struct file_operations tzdbg_fops = {
	.owner   = THIS_MODULE,
	.read    = tzdbgfs_read,
	.open    = tzdbgfs_open,
};

static int  tzdbgfs_init(struct platform_device *pdev)
{
	int rc = 0;
	int i;
	struct dentry           *dent_dir;
	struct dentry           *dent;

	dent_dir = debugfs_create_dir("tzdbg", NULL);
	if (dent_dir == NULL) {
		dev_err(&pdev->dev, "tzdbg debugfs_create_dir failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < TZDBG_STATS_MAX; i++) {
		tzdbg.debug_tz[i] = i;
		dent = debugfs_create_file(tzdbg.stat[i].name,
				S_IRUGO, dent_dir,
				&tzdbg.debug_tz[i], &tzdbg_fops);
		if (dent == NULL) {
			dev_err(&pdev->dev, "TZ debugfs_create_file failed\n");
			rc = -ENOMEM;
			goto err;
		}
	}
	tzdbg.disp_buf = kzalloc(DEBUG_MAX_RW_BUF, GFP_KERNEL);
	if (tzdbg.disp_buf == NULL) {
		pr_err("%s: Can't Allocate memory for tzdbg.disp_buf\n",
			__func__);

		goto err;
	}
	platform_set_drvdata(pdev, dent_dir);
	return 0;
err:
	debugfs_remove_recursive(dent_dir);

	return rc;
}

static void tzdbgfs_exit(struct platform_device *pdev)
{
	struct dentry           *dent_dir;

	kzfree(tzdbg.disp_buf);
	dent_dir = platform_get_drvdata(pdev);
	debugfs_remove_recursive(dent_dir);
}

/*
 * Driver functions
 */
static int __devinit tz_log_probe(struct platform_device *pdev)
{
	struct resource *resource;
	void __iomem *virt_iobase;
	uint32_t tzdiag_phy_iobase;
	uint32_t *ptr = NULL;

	/*
	 * Get address that stores the physical location of 4KB
	 * diagnostic data
	 */
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev,
				"%s: ERROR Missing MEM resource\n", __func__);
		return -ENXIO;
	};
	/*
	 * Map address that stores the physical location of 4KB
	 * diagnostic data
	 */
	virt_iobase = devm_ioremap_nocache(&pdev->dev, resource->start,
				resource->end - resource->start + 1);
	if (!virt_iobase) {
		dev_err(&pdev->dev,
			"%s: ERROR could not ioremap: start=%p, len=%u\n",
			__func__, (void *) resource->start,
			(resource->end - resource->start + 1));
		return -ENXIO;
	}
	/*
	 * Retrieve the address of 4KB diagnostic data
	 */
	tzdiag_phy_iobase = readl_relaxed(virt_iobase);

	/*
	 * Map the 4KB diagnostic information area
	 */
	tzdbg.virt_iobase = devm_ioremap_nocache(&pdev->dev,
				tzdiag_phy_iobase, DEBUG_MAX_RW_BUF);

	if (!tzdbg.virt_iobase) {
		dev_err(&pdev->dev,
			"%s: ERROR could not ioremap: start=%p, len=%u\n",
			__func__, (void *) tzdiag_phy_iobase, DEBUG_MAX_RW_BUF);
		return -ENXIO;
	}

	ptr = kzalloc(DEBUG_MAX_RW_BUF, GFP_KERNEL);
	if (ptr == NULL) {
		pr_err("%s: Can't Allocate memory: ptr\n",
			__func__);
		return -ENXIO;
	}

	tzdbg.diag_buf = (struct tzdbg_t *)ptr;

	if (tzdbgfs_init(pdev))
		goto err;

	return 0;
err:
	kfree(tzdbg.diag_buf);
	return -ENXIO;
}


static int __devexit tz_log_remove(struct platform_device *pdev)
{
	kzfree(tzdbg.diag_buf);
	tzdbgfs_exit(pdev);

	return 0;
}

static struct platform_driver tz_log_driver = {
	.probe		= tz_log_probe,
	.remove		= __devexit_p(tz_log_remove),
	.driver		= {
		.name = "tz_log",
		.owner = THIS_MODULE,
	},
};

static int __init tz_log_init(void)
{
	return platform_driver_register(&tz_log_driver);
}

static void __exit tz_log_exit(void)
{
	platform_driver_unregister(&tz_log_driver);
}

module_init(tz_log_init);
module_exit(tz_log_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TZ Log driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:tz_log");
