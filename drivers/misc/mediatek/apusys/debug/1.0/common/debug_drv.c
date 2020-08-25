// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/string.h>


#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#endif

#include "debug_plat_api.h"
#include "apusys_core.h"
#include "debug_drv.h"
#include "apusys_debug_api.h"

#define FORCE_REG_DUMP_ENABLE (1)


struct debug_plat_drv debug_drv;

#define STRMAX 128
struct dump_data {
	char module_name[STRMAX];
	char *reg_all_mem;
	u32 *gals_reg;
};


struct dump_data data;


int debug_log_level;
bool apusys_dump_force;
bool apusys_dump_skip_gals;
static void *apu_top;
static struct mutex dbg_lock;
static struct mutex dump_lock;

struct dentry *apusys_dump_root;

struct apusys_core_info *dbg_core_info;


void apusys_reg_dump_skip_gals(int onoff)
{
	LOG_DEBUG("+\n");

	if (onoff)
		apusys_dump_skip_gals = true;
	else
		apusys_dump_skip_gals = false;

	LOG_DEBUG("-\n");
}

void apusys_reg_dump(char *module_name, bool dump_vpu)
{
	LOG_DEBUG("+\n");

	mutex_lock(&dump_lock);

	if (data.reg_all_mem == NULL) {

		data.reg_all_mem = vzalloc(debug_drv.apusys_reg_size);
		if (data.reg_all_mem == NULL)
			goto out;

		data.gals_reg = vzalloc(debug_drv.total_dbg_mux_count);
		if (data.gals_reg == NULL)
			goto out;

		if (module_name != NULL)
			strncpy(data.module_name, module_name, STRMAX);

	} else {
		LOG_INFO("dump is in process, skip this dump!\n");
		goto out;

	}

	debug_drv.reg_dump(apu_top, dump_vpu, data.reg_all_mem,
				apusys_dump_skip_gals, data.gals_reg);

out:
	mutex_unlock(&dump_lock);

	LOG_DEBUG("-\n");
}

int dump_show(struct seq_file *sfile, void *v)
{
	LOG_DEBUG("+\n");

	mutex_lock(&dbg_lock);

	if (apusys_dump_force)
		apusys_reg_dump("force_dump", true);

	mutex_lock(&dump_lock);

	if (data.reg_all_mem == NULL)
		goto out;

	debug_drv.dump_show(sfile, v, data.reg_all_mem,
				data.gals_reg, data.module_name);


	vfree(data.reg_all_mem);
	data.reg_all_mem = NULL;

	vfree(data.gals_reg);
	data.gals_reg = NULL;

out:
	mutex_unlock(&dump_lock);
	mutex_unlock(&dbg_lock);

	LOG_DEBUG("-\n");

	return 0;

}

static int dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_show, NULL);
}

static const struct file_operations apu_dump_debug_fops = {
	.owner = THIS_MODULE,
	.open = dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int debug_probe(struct platform_device *pdev)
{
	int ret = 0;

	debug_log_level = 0;

	LOG_DEBUG("+\n");

	debug_drv = *(struct debug_plat_drv *)of_device_get_match_data(&pdev->dev);

	mutex_init(&dbg_lock);
	mutex_init(&dump_lock);

	apusys_dump_root = debugfs_create_dir("dump", dbg_core_info->dbg_root);
	ret = IS_ERR_OR_NULL(apusys_dump_root);
	if (ret) {
		LOG_ERR("failed to create debugfs dir\n");
		return -1;
	}

	debugfs_create_file("apusys_reg_all", 0444,
			apusys_dump_root, NULL, &apu_dump_debug_fops);
#if FORCE_REG_DUMP_ENABLE
	debugfs_create_bool("force_dump", 0644,
			apusys_dump_root, &apusys_dump_force);
#endif
	apu_top = ioremap(debug_drv.apusys_base,
					debug_drv.apusys_reg_size);
	if (apu_top == NULL) {
		LOG_ERR("could not allocate iomem base(0x%x) size(0x%x)\n",
			debug_drv.apusys_base, debug_drv.apusys_base);
		return -EIO;
	}

	data.reg_all_mem = NULL;
	data.gals_reg = NULL;
	memset(data.module_name, 0, STRMAX);

	apusys_dump_force = false;
	apusys_dump_skip_gals = false;

	LOG_DEBUG("-\n");

	return ret;
}

static int debug_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int debug_resume(struct platform_device *pdev)
{
	return 0;
}

static int debug_remove(struct platform_device *pdev)
{
	LOG_DEBUG("+\n");

	debugfs_remove_recursive(apusys_dump_root);
	iounmap(apu_top);

	LOG_DEBUG("-\n");

	return 0;
}

static struct platform_driver debug_driver = {
	.probe		= debug_probe,
	.remove		= debug_remove,
	.suspend	= debug_suspend,
	.resume		= debug_resume,
	.driver		= {
		.name   = APUSYS_DEBUG_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

int debug_init(struct apusys_core_info *info)
{
	LOG_DEBUG("debug driver init start\n");

	dbg_core_info = info;

	memset(&debug_drv, 0, sizeof(struct debug_plat_drv));

	debug_driver.driver.of_match_table = debug_plat_get_device();

	if (platform_driver_register(&debug_driver)) {
		LOG_ERR("failed to register %s driver", APUSYS_DEBUG_DEV_NAME);
		return -ENODEV;
	}

	return 0;
}

void debug_exit(void)
{
	LOG_DEBUG("+\n");

	platform_driver_unregister(&debug_driver);

	LOG_DEBUG("-\n");
}
