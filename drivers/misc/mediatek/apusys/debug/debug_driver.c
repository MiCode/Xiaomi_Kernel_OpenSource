// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/sysinfo.h>

#include "debug_driver.h"

#define APUDBG_DEV_NAME		"apu_debug"

char *dbglog_buf;
u8 g_debug_log_lv;

struct dentry *apusys_debug_root;
struct dentry *apusys_debug_user;
struct dentry *apusys_debug_devinfo;
struct dentry *apusys_debug_devattr;


u32 g_debug_log_level;
static int buf_head, line_count;
static long long char_count; //record char count of history
static int _cPos;

static int add_timestamp_string(char *buf, int bufsize)
{
	u64 ts;
	unsigned long rem_nsec;
	unsigned int len = 0;

	ts = local_clock();
	rem_nsec = do_div(ts, 1000000000);
	len = snprintf(buf, bufsize, "{%5lu.%06lu}",
		       (unsigned long)ts, rem_nsec / 1000);

	if (len < 0)
		pr_notice("len = %d error\n", len);

#ifdef DEBUG_DRIVER
	pr_notice("get_timestamp_string, len = %d\n", len);
#endif
	return len;
}

static int apusys_debug_dump(struct seq_file *s, void *unused)
{
	DBG_LOG_CON(s, "%d\n", g_debug_log_lv);
	return 0;
}

static int apusys_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_dump, inode->i_private);
}


static ssize_t show_debuglv(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos)
{
	int i = 0, p_line = 1, total_count = 0, save_head = buf_head;
	char *data = (dbglog_buf + buf_head);
	char buf[512];
	unsigned int len = 0;

	len += scnprintf(buf + len, sizeof(buf) - len,
			"g_debug_log_lv = %d:\n", g_debug_log_lv);

	pr_notice("buf_head = %d,char_count = %lld dbglog_buf = %s\n",
		buf_head, char_count, (dbglog_buf + buf_head));


	if (line_count > 1) {
		if (buf_head == 0) {
			while (p_line < line_count - 1) {
				while (*(data+i) != '\0')
					i++;

				i++;
				p_line++;
				pr_notice("dbglog[%d,%d] = %s\n", p_line, i,
					(dbglog_buf + buf_head + i));
			}
		} else {
			while (total_count < APU_LOG_SIZE) {
				if (buf_head + i > APU_LOG_SIZE) {
					pr_notice("reset data\n");
					data = dbglog_buf;
					buf_head = 0;
					i = 0;
					p_line++;
					pr_notice("!!dbglog[%d,%d] = %s\n",
						p_line, i,
						(dbglog_buf	+
						buf_head + i));
				}
				while (*(data+i) != '\0') {
					i++;
					total_count++;
				}
				i++;
				p_line++;
				if (p_line >= line_count) {
					pr_notice("strang line number too big!!!\n");
					break;
				}

				if (buf_head == 0 && i >= save_head) {
					pr_notice("print too much [%d,%d] !!!\n",
						i, total_count);
					break;
				}
				pr_notice("dbglog[%d,%d,%d] = %s\n",
					p_line, i, total_count,
					(dbglog_buf + buf_head + i));
			}
		}
	}
	pr_notice("finished print debug log\n");
	/*Reset all buffer count*/

	len += scnprintf(buf + len, sizeof(buf) - len,
			"print line number = %d done\n", p_line);

	buf_head = 0;
	line_count = 0;
	_cPos = 0;

	return simple_read_from_buffer(buffer, count, ppos, buf, len);

}

static ssize_t set_debuglv(struct file *flip,
						   const char __user *buffer,
						   size_t count, loff_t *f_pos)
{
	char *tmp, *cursor;
	int ret;
	unsigned int input = 0;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		DBG_LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		kfree(tmp);
		return count;
	}

	tmp[count] = '\0';
	cursor = tmp;
	ret = kstrtouint(cursor, 10, &input);

	pr_notice("set debug lv = %d\n", input);

	g_debug_log_lv = input;

	if (input >= 5) {
		pr_notice("release dbg resource\n");
		if (dbglog_buf != NULL) {
			kfree(dbglog_buf);
			dbglog_buf = NULL;
			buf_head = 0;
			line_count = 0;
			_cPos = 0;
		}
	}
	kfree(tmp);

	return count;
}

static const struct file_operations apusys_debug_fops = {
	.open = apusys_debug_open,
	.read = show_debuglv,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = set_debuglv,
};

static ssize_t show_debugAttr(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;

	len += scnprintf(buf + len, sizeof(buf) - len,
					"dbglog_buf = 0x%p\n", dbglog_buf);

	len += scnprintf(buf + len, sizeof(buf) - len,
					"buf_head = %d\n", buf_head);

	len += scnprintf(buf + len, sizeof(buf) - len,
					"char_count = %lld\n", char_count);


	len += scnprintf(buf + len, sizeof(buf) - len,
					"line_count = %d\n", line_count);

	len += scnprintf(buf + len, sizeof(buf) - len,
					"_cPos = %d:\n", _cPos);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);

}


static const struct file_operations apusys_debug_attr_fops = {
	.open = apusys_debug_open,
	.read = show_debugAttr,
	.llseek = seq_lseek,
	.release = seq_release,
};



void apu_dbg_print(const char *fmt, ...)
{
	int number_count = 0;
	va_list args;

#if defined(CONFIG_MTK_ENG_BUILD)

	if (dbglog_buf == NULL)
		dbglog_buf = kmalloc(APU_LOG_SIZE, GFP_KERNEL);

#endif

	if (dbglog_buf != NULL) {
		va_start(args, fmt);

		if (_cPos + 256 > APU_LOG_SIZE) {
			_cPos = 0;
			buf_head = 1;
			pr_notice("%s reset _cPos", __func__);
		}
		number_count = add_timestamp_string(dbglog_buf + _cPos, 512);
		_cPos += number_count;
		char_count += number_count;
		number_count = vsprintf(dbglog_buf + _cPos, fmt, args);
		if (number_count < 0)
			goto print_end;

		_cPos += (number_count + 1);
		char_count += (number_count + 1);

		line_count++;
		if (buf_head != 0)
			buf_head = _cPos;

#ifdef DEBUG_DRIVER
		pr_notice("edma_print _cPos = %d,line = %d, buf_head = %d\n",
			_cPos, line_count, buf_head);
#endif
print_end:
		va_end(args);
	}
}

static int debug_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_notice("%s in", __func__);

	apusys_debug_root = debugfs_create_dir(APUSYS_DEBUG_DIR, NULL);
	ret = IS_ERR_OR_NULL(apusys_debug_root);
	if (ret) {
		DBG_LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

	/* create device table info */
	apusys_debug_devinfo = debugfs_create_file("log", 0444,
		apusys_debug_root, NULL, &apusys_debug_fops);
	ret = IS_ERR_OR_NULL(apusys_debug_devinfo);
	if (ret) {
		DBG_LOG_ERR("failed to create debug node(devinfo).\n");
		goto out;
	}

	apusys_debug_devattr = debugfs_create_file("attr", 0444,
		apusys_debug_root, NULL, &apusys_debug_attr_fops);

	ret = IS_ERR_OR_NULL(apusys_debug_devattr);
	if (ret) {
		DBG_LOG_ERR("failed to create debug attr node(devinfo).\n");
		goto out;
	}

	pr_notice("debug probe done, dbglog_buf= 0x%p\n", dbglog_buf);
	ret = apusys_dump_init(&pdev->dev);
	if (ret) {
		DBG_LOG_ERR("failed to create debug dump attr node(devinfo).\n");
		goto out;
	}

	return 0;

out:
	pr_notice("debug probe error!!\n");

	return ret;
}

static int debug_remove(struct platform_device *pdev)
{
	apusys_dump_exit(&pdev->dev);
	return 0;
}


static struct platform_driver debug_driver = {
	.probe = debug_probe,
	.remove = debug_remove,
	.driver = {
		   .name = APUDBG_DEV_NAME,
		   .owner = THIS_MODULE,
	}
};

static struct platform_device debug_device = {
	.name = APUDBG_DEV_NAME,
	.id = 0,
};

static int __init debug_INIT(void)
{
	int ret = 0;

	pr_notice("%s debug driver init", __func__);

	dbglog_buf = NULL;
	g_debug_log_lv = 0;
	g_debug_log_level = 0;
	buf_head = 0;
	line_count = 0;
	char_count = 0;
	_cPos = 0;

	ret = platform_driver_register(&debug_driver);
	if (ret != 0)
		pr_notice("failed to register debug driver");


	if (platform_device_register(&debug_device)) {
		DBG_LOG_ERR("failed to register debug_driver device");
		platform_driver_unregister(&debug_driver);
		return -ENODEV;
	}


	return ret;
}

static void __exit debug_EXIT(void)
{
	platform_driver_unregister(&debug_driver);
}

module_init(debug_INIT);
module_exit(debug_EXIT);
MODULE_LICENSE("GPL");
