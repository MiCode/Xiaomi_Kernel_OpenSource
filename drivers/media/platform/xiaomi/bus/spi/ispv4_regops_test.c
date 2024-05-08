// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#define pr_fmt(fmt) "ispv4 regops test: " fmt

#define DEBUG

#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <ispv4_regops.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

extern struct dentry * volatile ispv4_debugfs;
static struct delayed_work mod_init_work;

static u32 debug_reg_addr;
static struct dentry *regops_debugfs;

#define ST_DEBUG(x)                                                            \
	static u32 x = 0;                                                      \
	static const char *x##_name = #x

ST_DEBUG(st_read_err);
ST_DEBUG(st_write_err);
ST_DEBUG(st_success);
ST_DEBUG(st_read_total);
ST_DEBUG(st_write_total);
ST_DEBUG(st_mismatch);
ST_DEBUG(st_base);
ST_DEBUG(st_length);

static bool stress_log = false;

#define CREATE_ST_FILE(p, x) debugfs_create_u32(x##_name, 0666, p, &x)

int reg_read_show(struct seq_file *m, void *unused)
{
	u32 val = 0;
	int ret = 0;
	ret = ispv4_regops_read(debug_reg_addr, &val);
	if (ret == 0) {
		seq_printf(m, "0x%08llx = 0x%08llx\n", debug_reg_addr, val);
	} else {
		seq_printf(m, "Read 0x%08llx error %d\n", debug_reg_addr, ret);
	}
	return ret;
}
DEFINE_SHOW_ATTRIBUTE(reg_read);

static ssize_t reg_write(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	u32 val = 0;
	int ret = 0;

	(void)kstrtouint_from_user(user_buf, count, 16, &val);
	pr_info("will write val = 0x%lx\n", val);
	ret = ispv4_regops_write(debug_reg_addr, val);
	if (ret != 0) {
		pr_err("debugfs write 0x%llx failed %d\n", debug_reg_addr, ret);
	}

	return ret == 0 ? count : ret;
}

static struct file_operations reg_write_fops = {
	.open = simple_open,
	.write = reg_write,
};

static ssize_t stress_test_write(struct file *file, const char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	u32 mode = 0;
	int i, match = 0;
	int ret1, ret2;

#define WRITE_BYTES_MAX 65536u

	st_read_err = 0;
	st_write_err = 0;
	st_success = 0;
	st_read_total = 0;
	st_write_total = 0;
	st_mismatch = 0;
	(void)kstrtouint_from_user(user_buf, count, 10, &mode);
	pr_info("[stress test]: speed = %d\n", ispv4_regops_get_speed());

	st_length = min(st_length, WRITE_BYTES_MAX);
	pr_info("[stress test]: mode = %d times(32bit)= %d\n", mode,
		st_length / 4);

	if (mode == 0) {
		u32 write_val = st_base;
		u32 read_val;
		int inc;
		for (i = 0; i < st_length; i += 4) {
			st_read_total++;
			st_write_total++;
			ret1 = ispv4_regops_write(debug_reg_addr + i,
						  write_val);
			if (stress_log)
				pr_info("stlog write 0x%08x=%08x ret=%d",
					debug_reg_addr + i, write_val, ret1);
			if (ret1 != 0) {
				st_write_err++;
				continue;
			}
			ret2 = ispv4_regops_read(debug_reg_addr + i, &read_val);
			if (stress_log)
				pr_info("stlog read 0x%08x=%08x should %08x, ret=%d",
					debug_reg_addr + i, read_val, write_val,
					ret2);
			if (ret2 != 0) {
				st_read_err++;
				continue;
			}

			inc = write_val == read_val ? 1 : 0;
			match += inc;
			st_mismatch += 1 - inc;
			st_success += inc;

			write_val++;
		}
	} else if (mode == 1) {
		u32 write_val = st_base;
		u32 read_val;
		int inc;
		for (i = 0; i < st_length; i += 4) {
			st_write_total++;
			ret1 = ispv4_regops_write(debug_reg_addr + i,
						  write_val + i);
			if (stress_log)
				pr_info("stlog write 0x%08x=%08x ret=%d",
					debug_reg_addr + i, write_val + i,
					ret1);
			if (ret1 != 0)
				st_write_err++;
		}

		for (i = 0; i < st_length; i += 4) {
			st_read_total++;
			ret2 = ispv4_regops_read(debug_reg_addr + i, &read_val);
			if (stress_log)
				pr_info("stlog read 0x%08x=%08x should %08x, ret=%d",
					debug_reg_addr + i, read_val,
					write_val + i, ret2);
			if (ret2 != 0)
				st_read_err++;
			inc = (write_val + i) == read_val ? 1 : 0;
			match += inc;
			st_mismatch += 1 - inc;
			st_success += inc;
		}
	} else if (mode == 2) {
		u32 wv = st_base;
		u32 rv1, rv2;
		int inc;
		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_write(debug_reg_addr + i, wv);
			ispv4_regops_read(debug_reg_addr + i, &rv1);
			ispv4_regops_read(debug_reg_addr + i, &rv2);
			inc = (wv == rv1) && (rv1 == rv2) ? 1 : 0;
			match += inc;
			wv++;
		}
	} else if (mode == 3) {
		u32 write_val = st_base;
		u32 read_val;
		int inc;
		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_write(debug_reg_addr + i, write_val + i);
		}

		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_read(debug_reg_addr + i, &read_val);
		}

		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_read(debug_reg_addr + i, &read_val);
			inc = (write_val + i) == read_val ? 1 : 0;
			match += inc;
		}
	} else if (mode == 4) {
		static u32 num[WRITE_BYTES_MAX / 4] = { 0 };
		u32 read_val;
		int idx, inc;
		for (idx = 0; idx < st_length / 4; idx++) {
			num[idx] = st_base + idx;
		}
		ispv4_regops_burst_write(debug_reg_addr, (u8 *)num, st_length);
		for (idx = 0; idx < st_length; idx += 4) {
			ispv4_regops_read(debug_reg_addr + idx, &read_val);
			inc = (st_base + idx / 4) == read_val ? 1 : 0;
			match += inc;
		}
	} else if (mode == 5) {
		static u32 num[WRITE_BYTES_MAX / 4] = { 0 };
		u32 read_val;
		int idx, inc;
		u64 time;
		for (idx = 0; idx < st_length / 4; idx++) {
			num[idx] = st_base + idx;
		}
		time = ktime_get_real_ns();
		ispv4_regops_long_burst_write(debug_reg_addr, (u8 *)num,
					      st_length);
		time = ktime_get_real_ns() - time;
		for (idx = 0; idx < st_length; idx += 4) {
			ispv4_regops_read(debug_reg_addr + idx, &read_val);
			inc = (st_base + idx / 4) == read_val ? 1 : 0;
			match += inc;
		}
		pr_info("Write %d bytes by b256 take %ld us\n", st_length,
			time / 1000);
	} else if (mode == 6) {
		u32 write_val = st_base;
		u32 read_val;
		int inc;
		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_write(debug_reg_addr + i, write_val + i);
		}

		for (i = 0; i < st_length; i += 4) {
			ispv4_regops_dread(debug_reg_addr + i, &read_val);
			inc = (write_val + i) == read_val ? 1 : 0;
			match += inc;
		}
	} else if (mode == 7) {
		const int addr = 0xC;
		int i;
		pr_cont("Dump inner reg: ");
		for (i = 0; i < 4; i++) {
			int iaddr = addr + i * 4;
			int val;
			ispv4_regops_inner_read(iaddr, &val);
			pr_cont("0x%x=0x%x ", iaddr, val);
		}
		pr_cont("\n");
	} else if (mode == 8) {
		const int addr = 0x0;
		int i;
		for (i = 0; i < 1000; i++) {
			int val;
			int ret;
			ret = ispv4_regops_write(addr, i);
			if (ret != 0)
				break;
			ret = ispv4_regops_read(addr, &val);
			if (ret != 0)
				break;
			if (val != i)
				break;
		}
	}

	pr_info("[stress result]: match %d/%d", match, st_length / 4);
	return count;
}

static int regops_speed_get(void *data, u64 *val)
{
	(void)data;
	*val = ispv4_regops_get_speed();
	return 0;
}

static int regops_speed_set(void *data, u64 val)
{
	ispv4_regops_set_speed(val);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(regops_speed_fops, regops_speed_get, regops_speed_set,
			 "%llu\n");

static struct file_operations stress_test_fops = {
	.open = simple_open,
	.write = stress_test_write,
};

static void xm_regops_test_init_work(struct work_struct *w)
{
	struct dentry *debugfs_file;

	(void)w;
	pr_info("Into %s\n", __FUNCTION__);

	if (ispv4_debugfs == NULL) {
		pr_info("%s waiting ispv4_debugfs\n", __FUNCTION__);
		mod_delayed_work(system_wq, &mod_init_work,
				 msecs_to_jiffies(3000));
		return;
	}

	pr_info("%s find ispv4_debugfs\n", __FUNCTION__);

	regops_debugfs = debugfs_create_dir("ispv4_regops", ispv4_debugfs);
	if (!IS_ERR_OR_NULL(regops_debugfs)) {
		debugfs_create_u32("addr", 0666, regops_debugfs,
				   &debug_reg_addr);
		debugfs_file = debugfs_create_file(
			"write", 0222, regops_debugfs, NULL, &reg_write_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("write debugfs init failed %d\n",
				PTR_ERR(debugfs_file));
		}
		debugfs_file =
			debugfs_create_file("stress_test", 0222, regops_debugfs,
					    NULL, &stress_test_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("stress test debugfs init failed %d\n",
				PTR_ERR(debugfs_file));
		}
		pr_info("stress test: 0 wr; 1 c wr; 2 wrr; 3 c wrr\n");
		debugfs_file = debugfs_create_file("read", 0444, regops_debugfs,
						   NULL, &reg_read_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("read debugfs init failed %d\n",
				PTR_ERR(debugfs_file));
		}
		debugfs_file =
			debugfs_create_file("speed", 0666, regops_debugfs, NULL,
					    &regops_speed_fops);
		if (IS_ERR_OR_NULL(debugfs_file)) {
			pr_info("speed debugfs init failed %d\n",
				PTR_ERR(debugfs_file));
		}

		CREATE_ST_FILE(regops_debugfs, st_read_err);
		CREATE_ST_FILE(regops_debugfs, st_write_err);
		CREATE_ST_FILE(regops_debugfs, st_read_total);
		CREATE_ST_FILE(regops_debugfs, st_write_total);
		CREATE_ST_FILE(regops_debugfs, st_success);
		CREATE_ST_FILE(regops_debugfs, st_mismatch);
		CREATE_ST_FILE(regops_debugfs, st_base);
		CREATE_ST_FILE(regops_debugfs, st_length);
		debugfs_create_bool("log", 0666, regops_debugfs, &stress_log);
	}

	pr_info("reg ops test probe finish!\n");
	return;
}

static int __init xm_regops_test_init(void)
{
	pr_info("Into %s\n", __FUNCTION__);
	INIT_DELAYED_WORK(&mod_init_work, xm_regops_test_init_work);
	schedule_delayed_work(&mod_init_work, 0);
	return 0;
}
module_init(xm_regops_test_init);

static void __exit xm_regops_test_exit(void)
{
	if (!IS_ERR_OR_NULL(regops_debugfs)) {
		debugfs_remove(regops_debugfs);
	}
}
module_exit(xm_regops_test_exit);

MODULE_AUTHOR("Chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 regops driver test");
MODULE_LICENSE("GPL v2");
