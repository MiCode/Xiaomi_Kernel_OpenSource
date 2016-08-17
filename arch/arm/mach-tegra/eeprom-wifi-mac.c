#include <linux/i2c/at24.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>

#define EEPROM_LEN 32

static char wifi_mac[] = "ff:ff:ff:ff:ff:ff";

static ssize_t show_addr_sysfs(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", wifi_mac);
}

DEVICE_ATTR(addr, 0444, show_addr_sysfs, NULL);

static struct attribute *wifi_mac_addr_attributes[] = {
	&dev_attr_addr.attr,
	NULL
};

static const struct attribute_group wifi_mac_addr_group = {
	.attrs = wifi_mac_addr_attributes,
};

int create_sys_fs(void)
{
	int ret = -1;
	static struct kobject *reg_kobj;
	reg_kobj = kobject_create_and_add("wifi_mac_addr", kernel_kobj);
	if (!reg_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(reg_kobj, &wifi_mac_addr_group);
	if (ret < 0) {
		kobject_put(reg_kobj);
		return ret;
	}
	return ret;
}

void get_mac_addr(struct memory_accessor *mem_acc, void *context)
{
	char *mac_addr;
	int ret = 0;
	off_t offset = (off_t)context;
	mac_addr = kzalloc(sizeof(char)*EEPROM_LEN, GFP_ATOMIC);
	if (!mac_addr) {
		pr_err("no memory to allocate");
		return;
	}

	/* Read MAC addr from EEPROM */
	ret = mem_acc->read(mem_acc, mac_addr, offset, EEPROM_LEN);
	if (ret == EEPROM_LEN) {
		pr_err("Read MAC addr from EEPROM: %pM\n", (mac_addr+19));
		sprintf(wifi_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		*(mac_addr+19), *(mac_addr+20), *(mac_addr+21),
		*(mac_addr+22), *(mac_addr+23), *(mac_addr+24));
	} else
		pr_err("Error reading MAC addr from EEPROM\n");

	create_sys_fs();
}
