#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/mt_boot.h>

//extern u32 PTP_INIT_01_API(void);

static ssize_t track_vsram_show(struct device_driver *driver, char *buf)
{
    return 0;
}

static ssize_t track_vsram_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(track_vsram, 0644, track_vsram_show, track_vsram_store);

#if 0
static ssize_t ptp_od_show(struct device_driver *driver, char *buf)
{
    volatile u32 *clc_temp_p;
    u32 val = 0 ;
    clc_temp_p = (volatile u32 *)PTP_INIT_01_API();

    /* only need bit 31 ~ bit 16 of the read data*/
    val = clc_temp_p[0];
    val >>= 16;
    val &= 0x0000FFFF;

    return snprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static ssize_t ptp_od_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(ptp_od, 0644, ptp_od_show, ptp_od_store);

#endif
static ssize_t chip_id_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "0x%x\n", get_chip_code());
}

static ssize_t chip_id_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(chip_id, 0644, chip_id_show, chip_id_store);

struct mssv_driver
{
    struct device_driver driver;
    const struct platform_device_id *id_table;
};

static struct mssv_driver mssv_driver =
{
    .driver =
    {
        .name = "mssv",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};

int mssv_init(void)
{
    int ret;

    ret = driver_register(&mssv_driver.driver);
    if (ret) {
        printk("Fail to register mssv_driver\n");
        return ret;
    }

    ret = driver_create_file(&mssv_driver.driver, &driver_attr_track_vsram);
    if (ret) {
        printk("Fail to create mssv_driver sysfs files\n");
        return ret;
    }
#if 0
    ret = driver_create_file(&mssv_driver.driver, &driver_attr_ptp_od);
    if (ret) {
        printk("Fail to create mssv_driver sysfs files\n");
        return ret;
    }
#endif
    ret = driver_create_file(&mssv_driver.driver, &driver_attr_chip_id);
    if (ret) {
        printk("Fail to create mssv_driver sysfs files\n");
        return ret;
    }

    return 0;
}

arch_initcall(mssv_init);
