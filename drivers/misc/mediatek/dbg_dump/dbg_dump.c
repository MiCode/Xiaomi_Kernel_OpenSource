#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/sync_write.h>
#include <mach/dbg_dump.h>
#include <linux/kallsyms.h>

int dbg_reg_dump_probe(struct platform_device *pdev);

int __weak reg_dump_platform(char *buf) { return 1; }

int mt_reg_dump(char *buf)
{
    if(reg_dump_platform(buf) == 0)
        return 0;
    else
        return -1;
}

static struct platform_driver dbg_reg_dump_driver = {
	.probe = dbg_reg_dump_probe,
	.driver = {
		   .name = "dbg_reg_dump",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
};

static ssize_t last_pc_dump_show(struct device_driver *driver, char *buf)
{
	int ret = mt_reg_dump(buf);
	if (ret == -1)
		pr_err("Dump error in %s, %d\n", __func__, __LINE__);

	return strlen(buf);
}

static ssize_t last_pc_dump_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(last_pc_dump, 0664, last_pc_dump_show, last_pc_dump_store);

int dbg_reg_dump_probe(struct platform_device *pdev)
{
	int ret;

	ret = driver_create_file(&dbg_reg_dump_driver.driver, &driver_attr_last_pc_dump);
	if (ret) {
		pr_err("Fail to create mt_reg_dump_drv sysfs files");
	}

	return 0;
}

/**
 * driver initialization entry point
 */
static int __init dbg_reg_dump_init(void)
{
	int err;

	err = platform_driver_register(&dbg_reg_dump_driver);
	if (err) {
		return err;
	}

	return 0;
}

/**
 * driver exit point
 */
static void __exit dbg_reg_dump_exit(void)
{
}
module_init(dbg_reg_dump_init);
module_exit(dbg_reg_dump_exit);
