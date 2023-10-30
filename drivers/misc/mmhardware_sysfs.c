#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/mmhardware_sysfs.h>

/* show */
static ssize_t mm_register_show(struct kobject *dev,
	struct kobj_attribute *a, char *buf)
{
	struct mm_info *mi = container_of(a, struct mm_info, k_attr);
	switch (mi->mm_id) {
		case MM_HW_ADSP:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_CODEC:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_1:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_2:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_3:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_4:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_5:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_6:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_7:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_PA_8:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_HAPTIC_1:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_HAPTIC_2:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		case MM_HW_AS:
			if (!mi->on_register) {
				pr_info("%s: 0x%x is not registered\n", __func__, mi->mm_id);
			}
			return sprintf(buf, "%d\n", mi->on_register);
			break;
		default:
			break;
	}
	return 0;
}
/* store */
static ssize_t mm_register_store(struct kobject *dev,
	struct kobj_attribute *a, const char *buf, size_t count)
{
	return count;
}
MM_INFO(MM_HW_ADSP, adsp);
MM_INFO(MM_HW_CODEC, codec);
MM_INFO(MM_HW_PA_1, pa1);
MM_INFO(MM_HW_PA_2, pa2);
MM_INFO(MM_HW_PA_3, pa3);
MM_INFO(MM_HW_PA_4, pa4);
MM_INFO(MM_HW_PA_5, pa5);
MM_INFO(MM_HW_PA_6, pa6);
MM_INFO(MM_HW_PA_7, pa7);
MM_INFO(MM_HW_PA_8, pa8);
MM_INFO(MM_HW_HAPTIC_1, haptic1);
MM_INFO(MM_HW_HAPTIC_2, haptic2);
MM_INFO(MM_HW_AS, audioswitch);

static struct attribute *mm_attrs[] = {
	&adsp_info.k_attr.attr,
	&codec_info.k_attr.attr,
	&pa1_info.k_attr.attr,
	&pa2_info.k_attr.attr,
	&pa3_info.k_attr.attr,
	&pa4_info.k_attr.attr,
	&pa5_info.k_attr.attr,
	&pa6_info.k_attr.attr,
	&pa7_info.k_attr.attr,
	&pa8_info.k_attr.attr,
	&haptic1_info.k_attr.attr,
	&haptic2_info.k_attr.attr,
	&audioswitch_info.k_attr.attr,
	NULL,  /* need to NULL terminate the list of attributes */
};
static struct attribute_group mm_attr_group = {
	.attrs = mm_attrs,
};
static struct kobject *mm_sysfs_kobj;
int mmhardware_initialize_sysfs(void)
{
	int err;
	/* create mm_hardware under /sys */
	mm_sysfs_kobj = kobject_create_and_add(MM_HARDWARE_SYSFS_ROOT_FOLDER, kernel_kobj->parent);
	if (!mm_sysfs_kobj) {
		pr_err("failed to create kobj");
		return -ENOMEM;
	}
	err = sysfs_create_group(mm_sysfs_kobj, &mm_attr_group);
	if (err) {
		pr_err("failed to create sysfs group");
		kobject_put(mm_sysfs_kobj);
		return -ENOMEM;
	}
	return 0;
}
void mmhardware_cleanup_sysfs(void)
{
	kobject_put(mm_sysfs_kobj);
}
int register_kobj_under_mmsysfs(enum hardware_id mm_id, const char *name)
{
	int ret = 0;
	int iter = 0;
	int find_id = 0;
	struct mm_info *mi = NULL;
	struct kobj_attribute *mi_attr = NULL;
	struct attribute *a = mm_attrs[iter];
	if (name == NULL) {
		pr_err("%s: device_name is empty\n", __func__);
		ret = -2;
		goto err;
	}
	while (a) {
		mi_attr = container_of(a, struct kobj_attribute, attr);
		mi = container_of(mi_attr, struct mm_info, k_attr);
		iter++;
		a = mm_attrs[iter];
		if (mm_id == mi->mm_id) {
			find_id = 1;
			if (mi->on_register) {
				pr_info("%s: device(id:%d name:%s) has already registered\n", __func__, mi->mm_id, name);
				ret = -4;
				goto err;
			}
			mi->on_register = 1;
			goto err;
		} else continue;
	}
	if (find_id == 0) {
		pr_err("%s: Can't find correct hardware_id: 0x%x\n", __func__, mm_id);
		ret = -3;
	}
err:
	return ret;
}
EXPORT_SYMBOL(register_kobj_under_mmsysfs);

subsys_initcall(mmhardware_initialize_sysfs);
module_exit(mmhardware_cleanup_sysfs);
MODULE_AUTHOR("xiaomi mm");
MODULE_DESCRIPTION("Multimedia hardware detection");
MODULE_LICENSE("GPL");
