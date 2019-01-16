#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/eint_drv.h>

static struct mt_eint_driver mt_eint_drv = {
	.driver = {
		   .driver = {
			      .name = "eint",
			      .bus = &platform_bus_type,
			      .owner = THIS_MODULE,
			      },
		   },
};

static unsigned int cur_eint_num;

struct mt_eint_driver *get_mt_eint_drv(void)
{
	return &mt_eint_drv;
}

static ssize_t cur_eint_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cur_eint_num);
}

static ssize_t cur_eint_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);

	if (mt_eint_drv.eint_max_channel) {
		if (num >= mt_eint_drv.eint_max_channel()) {
			pr_err("Invalid EINT number %d from user space", num);
		} else {
			cur_eint_num = num;
		}
	} else {
		pr_err("mt_eint_drv.eint_max_channel is NULL");
	}

	return count;
}

DRIVER_ATTR(current_eint, 0664, cur_eint_show, cur_eint_store);

static ssize_t cur_eint_sens_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.get_sens) {
		return snprintf(buf, PAGE_SIZE, "%d\n", mt_eint_drv.get_sens(cur_eint_num));
	} else {
		pr_err("mt_eint_drv.get_sens is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t cur_eint_sens_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int sens;

	sens = simple_strtoul(p, &p, 10);
	if (sens != 1 && sens != 0) {
		pr_err("Invalid value: %d", sens);
		return count;
	}

	if (mt_eint_drv.set_sens && mt_eint_drv.is_disable
	    && mt_eint_drv.disable && mt_eint_drv.enable) {
		/*
		 * No lock is used here.
		 * It is assumed that other drivers won't use the same EINT at the same time.
		 * The user should note this assumption.
		 */
		if (mt_eint_drv.is_disable(cur_eint_num)) {
			mt_eint_drv.set_sens(cur_eint_num, sens);
		} else {
			mt_eint_drv.disable(cur_eint_num);
			mt_eint_drv.set_sens(cur_eint_num, sens);
			mt_eint_drv.enable(cur_eint_num);
		}
	} else {
		pr_err("Fail to set EINT sensitivity");
	}

	return count;
}

DRIVER_ATTR(current_eint_sens, 0644, cur_eint_sens_show, cur_eint_sens_store);

static ssize_t cur_eint_pol_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.get_polarity) {
		return snprintf(buf, PAGE_SIZE, "%d\n", mt_eint_drv.get_polarity(cur_eint_num));
	} else {
		pr_err("mt_eint_drv.get_polarity is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t cur_eint_pol_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int pol;

	pol = simple_strtoul(p, &p, 10);
	if (pol != 1 && pol != 0) {
		pr_err("Invalid value: %d\n", pol);
		return count;
	}

	if (mt_eint_drv.set_polarity && mt_eint_drv.is_disable
	    && mt_eint_drv.disable && mt_eint_drv.enable) {
		/*
		 * No lock is used here.
		 * It is assumed that other drivers won't use the same EINT at the same time.
		 * The user should note this assumption.
		 */
		if (mt_eint_drv.is_disable(cur_eint_num)) {
			mt_eint_drv.set_polarity(cur_eint_num, pol);
		} else {
			mt_eint_drv.disable(cur_eint_num);
			mt_eint_drv.set_polarity(cur_eint_num, pol);
			mt_eint_drv.enable(cur_eint_num);
		}
	} else {
		pr_err("Fail to set EINT polarity");
	}

	return count;
}

DRIVER_ATTR(current_eint_pol, 0644, cur_eint_pol_show, cur_eint_pol_store);

static ssize_t cur_eint_deb_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.get_debounce_cnt) {
		return snprintf(buf, PAGE_SIZE, "%d\n", mt_eint_drv.get_debounce_cnt(cur_eint_num));
	} else {
		pr_err("mt_eint_drv.get_debounce_cnt is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t cur_eint_deb_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int cnt;

	cnt = simple_strtoul(p, &p, 10);

	if (mt_eint_drv.set_debounce_cnt && mt_eint_drv.is_disable
	    && mt_eint_drv.disable && mt_eint_drv.enable) {
		/*
		 * No lock is used here.
		 * It is assumed that other drivers won't use the same EINT at the same time.
		 * The user should note this assumption.
		 */
		if (mt_eint_drv.is_disable(cur_eint_num)) {
			mt_eint_drv.set_debounce_cnt(cur_eint_num, cnt);
		} else {
			mt_eint_drv.disable(cur_eint_num);
			mt_eint_drv.set_debounce_cnt(cur_eint_num, cnt);
			mt_eint_drv.enable(cur_eint_num);
		}
	} else {
		pr_err("Fail to set EINT debounce time");
	}

	return count;
}

DRIVER_ATTR(current_eint_deb, 0644, cur_eint_deb_show, cur_eint_deb_store);

static ssize_t cur_eint_deb_en_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.is_debounce_en) {
		return snprintf(buf, PAGE_SIZE, "%d\n", mt_eint_drv.is_debounce_en(cur_eint_num));
	} else {
		pr_err("mt_eint_drv.is_debounce_en is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t cur_eint_deb_en_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int enable;

	enable = simple_strtoul(p, &p, 10);

	if (enable != 1 && enable != 0) {
		pr_err("Invalid value: %d", enable);
		return count;
	}

	if (mt_eint_drv.enable_debounce && mt_eint_drv.disable_debounce
	    && mt_eint_drv.is_disable && mt_eint_drv.disable && mt_eint_drv.enable) {
		/*
		 * No lock is used here.
		 * It is assumed that other drivers won't use the same EINT at the same time.
		 * The user should note this assumption.
		 */
		if (mt_eint_drv.is_disable(cur_eint_num)) {
			if (enable) {
				mt_eint_drv.enable_debounce(cur_eint_num);
			} else {
				mt_eint_drv.disable_debounce(cur_eint_num);
			}
		} else {
			mt_eint_drv.disable(cur_eint_num);
			if (enable) {
				mt_eint_drv.enable_debounce(cur_eint_num);
			} else {
				mt_eint_drv.disable_debounce(cur_eint_num);
			}
			mt_eint_drv.enable(cur_eint_num);
		}
	} else {
		pr_err("Fail to set EINT debounce");
	}

	return count;
}

DRIVER_ATTR(current_eint_deb_en, 0644, cur_eint_deb_en_show, cur_eint_deb_en_store);

#if defined(EINT_DRV_TEST)
static ssize_t current_eint_enable_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.is_disable) {
		return snprintf(buf, PAGE_SIZE, "%s",
				mt_eint_drv.is_disable(cur_eint_num) ? "disable" : "enable");
	} else {
		pr_err("mt_eint_drv.is_disable is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t current_eint_enable_store(struct device_driver *driver,
					 const char *buf, size_t count)
{
	if (mt_eint_drv.enable) {
		mt_eint_drv.enable(cur_eint_num);
		if (mt_eint_drv.is_disable(cur_eint_num)) {
			pr_crit("EINT %d should be enabled but it is still disabled", cur_eint_num);
		}
	} else {
		pr_err("mt_eint_drv.enable is NULL");
	}

	return count;
}

DRIVER_ATTR(current_eint_enable, 0644, current_eint_enable_show, current_eint_enable_store);

static ssize_t current_eint_disable_show(struct device_driver *driver, char *buf)
{
	if (mt_eint_drv.is_disable) {
		return snprintf(buf, PAGE_SIZE, "%s",
				mt_eint_drv.is_disable(cur_eint_num) ? "disable" : "enable");
	} else {
		pr_err("mt_eint_drv.is_disable is NULL");
		return snprintf(buf, PAGE_SIZE, "ERROR");
	}
}

static ssize_t current_eint_disable_store(struct device_driver *driver,
					  const char *buf, size_t count)
{
	if (mt_eint_drv.disable) {
		mt_eint_drv.disable(cur_eint_num);
	} else {
		pr_err("mt_eint_drv.enable is NULL");
	}

	return count;
}

DRIVER_ATTR(current_eint_disable, 0644, current_eint_disable_show, current_eint_disable_store);
#endif

static int __init eint_drv_init(void)
{
	int ret;

	ret = driver_register(&mt_eint_drv.driver.driver);
	if (ret) {
		pr_err("Fail to register mt_eint_drv");
	}

	ret = driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint);
	ret |= driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_sens);
	ret |= driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_pol);
	ret |= driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_deb);
	ret |= driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_deb_en);
	if (ret) {
		pr_err("Fail to create mt_eint_drv sysfs files");
	}
#if defined(EINT_DRV_TEST)
	ret = driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_enable);
	ret |= driver_create_file(&mt_eint_drv.driver.driver, &driver_attr_current_eint_disable);
	if (ret) {
		pr_err("Fail to create EINT sysfs files");
	}
#endif

	return 0;
}
arch_initcall(eint_drv_init);
