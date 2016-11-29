#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/efi.h>

#define DEFAULT_DUMP_STATUS false

static struct kobject *dumpmode_kobj;

static efi_char16_t dumpmode_efivar_name[] = {
	'd', 'u', 'm', 'p', '_', 'm', 'o', 'd', 'e'
};

#define DUMPMODE_GUID EFI_GUID(0x0effd3ce, 0x59f0, 0x11e5, 0x8a, 0xc4, 0x08, 0x9e, 0x01, 0xc8, 0x3a, 0xa2)

static bool get_efi_dump_enabled(void)
{
	bool enabled = DEFAULT_DUMP_STATUS;
	efi_status_t status;
	u32 efi_attr;
	efi_char16_t data = 0;
	unsigned long data_len = sizeof(data);

	status = efi.get_variable(dumpmode_efivar_name,
			&DUMPMODE_GUID,
			&efi_attr,
			&data_len,
			&data);
	if (status != EFI_SUCCESS)
		printk(KERN_ERR "efi get error, status: %d\n", status);
	else if (data == 1)
		enabled = true;
	else
		enabled = false;
	return enabled;
}

static bool set_efi_dump_enabled(bool enabled)
{
	bool ret;
	efi_status_t status;
	efi_char16_t val;
	u32 efi_attr;

	if (enabled)
		val = 1;
	else
		val = 0;

	efi_attr = EFI_VARIABLE_NON_VOLATILE |
			EFI_VARIABLE_BOOTSERVICE_ACCESS |
			EFI_VARIABLE_RUNTIME_ACCESS;
	status = efi.set_variable(dumpmode_efivar_name,
			&DUMPMODE_GUID,
			efi_attr,
			sizeof(val),
			&val);
	if (status != EFI_SUCCESS) {
		printk(KERN_ERR "set efi error, status: %d\n", status);
		ret = false;
	} else
		ret = true;

	return ret;
}

static ssize_t dumpmode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t ret;
	bool status = get_efi_dump_enabled();

	if (status == true)
		ret = snprintf(buf, 3, "1\n");
	else
		ret = snprintf(buf, 3, "0\n");

	return ret;
}

static ssize_t dumpmode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	bool enabled;

	ret = strtobool(buf, &enabled);
	if (ret != 0) {
		printk(KERN_ERR "Unknown value for dumpmode: %s\n", buf);
		return 0 - EINVAL;
	}
	if (set_efi_dump_enabled(enabled) == true)
		ret = count;
	else
		ret = 0 - EINVAL;

	return ret;
}

static struct kobj_attribute dumpmode_attr = __ATTR_RW(dumpmode);

static struct attribute *dumpmode_attrs[] = {
	&dumpmode_attr.attr,
};


ATTRIBUTE_GROUPS(dumpmode);


static int __init efi_restart_init(void)
{
	int ret = ENOMEM;
	dumpmode_kobj = kobject_create_and_add("dumpmode", NULL);
	if (dumpmode_kobj == NULL) {
		printk(KERN_ERR "efi_restart_mode: kobject_create_and_add fail\n");
		goto fail;
	}

	ret = sysfs_create_group(dumpmode_kobj, &dumpmode_group);
	if (ret != 0) {
		printk(KERN_ERR "efi_restart_mode: sysfs_create_group fail %d\n", ret);
		goto sys_fail;
	}
	if (set_efi_dump_enabled(DEFAULT_DUMP_STATUS) == true)
		printk(KERN_ERR "efi_restart_mode: set default %d ok\n", DEFAULT_DUMP_STATUS);
	else
		printk(KERN_ERR "efi_restart_mode: set default %d fail\n", DEFAULT_DUMP_STATUS);

	return ret;

sys_fail:
	printk(KERN_ERR "efi_restart_mode: delete kobj\n");
	kobject_del(dumpmode_kobj);

fail:
	return ret;
}


static void __exit efi_restart_exit(void)
{
	printk(KERN_ERR "efi_restart_mode: delete kobj\n");
	kobject_del(dumpmode_kobj);
}

module_init(efi_restart_init);
module_exit(efi_restart_exit);
