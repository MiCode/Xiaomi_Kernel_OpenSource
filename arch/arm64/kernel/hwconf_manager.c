/*
 * hwconf_manager.c
 *
 * Copyright (C) 2016 Xiaomi Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/crypto.h>
#include <linux/cJSON.h>
#include <asm/setup.h>
#include <asm/hwconf_manager.h>

struct hw_info_manager {
	cJSON *hw_config;
	cJSON *hw_monitor;
	struct crypto_cipher *tfm;
	struct kobject *hwconf_kobj;
	struct dentry *hwconf_check;
	int hw_mon_inited;
};

struct hw_info_manager *info_manager;
static RAW_NOTIFIER_HEAD(hw_mon_notifier_list);
static DEFINE_MUTEX(hw_mon_notifier_lock);

#define INIT_KEY "0123456789"
char crypto_key[32] = INIT_KEY;

#define __HWINFO_DECRYPT_DEBUG__ 0

#define hwconf_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

int add_hw_component_info(char *component_name, char *key, char *value)
{
	cJSON *component;

	if (!info_manager->hw_config) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_config,
					component_name);
	if (!component)
		return -EINVAL;

	if (cJSON_HasObjectItem(component, key)) {
		pr_err("%s is added in %s already\n", key, component_name);
		return -EINVAL;
	}

	cJSON_AddStringToObject(component, key, value);
	pr_debug("%s: %s\n", __func__, cJSON_Print(component));

	return 0;
}
EXPORT_SYMBOL(add_hw_component_info);

int register_hw_component_info(char *component_name)
{
	cJSON *component;

	if (!info_manager->hw_config) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_config,
					component_name);
	if (component) {
		pr_err("%s is registered already\n", component_name);
		return -EINVAL;
	}

	component = cJSON_CreateObject();
	cJSON_AddItemToObject(info_manager->hw_config,
			      component_name, component);
	pr_debug("%s: %s\n", __func__, cJSON_Print(component));

	return 0;
}
EXPORT_SYMBOL(register_hw_component_info);

int unregister_hw_component_info(char *component_name)
{
	cJSON *component;

	if (!info_manager->hw_config) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_config,
					component_name);
	if (!component)
		return -EINVAL;

	cJSON_DetachItemFromObject(info_manager->hw_config,
				   component_name);

	return 0;
}
EXPORT_SYMBOL(unregister_hw_component_info);

int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
	cJSON *component;

	if (!info_manager->hw_monitor) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_monitor,
					component_name);
	if (!component) {
		pr_err("No component %s\n", component_name);
		return -EINVAL;
	}

	if (!cJSON_HasObjectItem(component, mon_key)) {
		pr_err("No key %s\n", mon_key);
		return -EINVAL;
	}

	cJSON_DeleteItemFromObject(component, mon_key);
	cJSON_AddStringToObject(component, mon_key, mon_value);

	pr_debug("%s: %s\n", __func__, cJSON_Print(info_manager->hw_monitor));

	return 0;
}
EXPORT_SYMBOL(update_hw_monitor_info);

int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value)
{
	cJSON *component;

	if (!info_manager->hw_monitor) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_monitor,
					component_name);
	if (!component)
		return -EINVAL;

	if (cJSON_HasObjectItem(component, mon_key)) {
		pr_err("%s is added in %s already\n", mon_key, component_name);
		return -EINVAL;
	}

	cJSON_AddStringToObject(component, mon_key, mon_value);
	pr_debug("%s: %s\n", __func__, cJSON_Print(component));

	return 0;
}
EXPORT_SYMBOL(add_hw_monitor_info);

int register_hw_monitor_info(char *component_name)
{
	cJSON *component;

	if (!info_manager->hw_monitor) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_monitor,
					component_name);
	if (component) {
		pr_err("%s is registered already\n", component_name);
		return -EINVAL;
	}

	component = cJSON_CreateObject();
	cJSON_AddItemToObject(info_manager->hw_monitor,
			      component_name, component);
	pr_debug("%s: %s\n", __func__, cJSON_Print(component));

	return 0;
}
EXPORT_SYMBOL(register_hw_monitor_info);

int unregister_hw_monitor_info(char *component_name)
{
	cJSON *component;

	if (!info_manager->hw_monitor) {
		pr_err("hwconfig_manager is still not ready.\n");
		return -EINVAL;
	}

	component = cJSON_GetObjectItem(info_manager->hw_monitor,
					component_name);
	if (!component)
		return -EINVAL;

	cJSON_DetachItemFromObject(info_manager->hw_monitor,
				   component_name);

	return 0;
}
EXPORT_SYMBOL(unregister_hw_monitor_info);

static ssize_t hw_info_store(struct kobject *kobj,
			     struct kobj_attribute *attr, const char *buf,
			     size_t count)
{
	memcpy(crypto_key, buf, sizeof(crypto_key));

	pr_debug("%s crypto_key=%s\n", __func__, crypto_key);

	return count;
}

#if __HWINFO_DECRYPT_DEBUG__
static int hw_info_decrypt_test(char *buf, size_t len)
{
	char *dest;
	unsigned int blocksize;
	int i;
	int ret;

	info_manager->tfm = crypto_alloc_cipher("aes",
			CRYPTO_ALG_TYPE_BLKCIPHER, CRYPTO_ALG_ASYNC);
	if (IS_ERR(info_manager->tfm)) {
		pr_err("Failed to load transform for aes mode!\n");
		ret = -EINVAL;
		goto out;
	}

	ret = crypto_cipher_setkey(info_manager->tfm, crypto_key,
				   sizeof(crypto_key));
	if (ret) {
		pr_err("Failed to setkey\n");
		ret = -EINVAL;
		goto free_cipher;
	}

	blocksize = crypto_cipher_blocksize(info_manager->tfm);
	if (!blocksize) {
		ret = -EINVAL;
		goto free_cipher;
	}

	dest = kzalloc(len, GFP_KERNEL);
	if (!dest) {
		ret = -ENOMEM;
		goto free_cipher;
	}

	for (i = 0; i < len; i += blocksize)
		crypto_cipher_decrypt_one(info_manager->tfm,
					  &dest[i], &buf[i]);

	pr_debug("%s: %s\n", __func__, dest);

	kfree(dest);
free_cipher:
	crypto_free_cipher(info_manager->tfm);
out:
	return ret;
}
#endif

/*
 * Decrypt example using Python:
 *
 * $ adb shell 'echo "01234567890123456789012345678901" > /sys/hwconf/hw_info'
 * $ adb pull /sys/hwconf/hw_info
 * 0 KB/s (48 bytes in 0.046s)
 * $ ./decrypt.py
 * {
 *	"display":	{
 *			"LCD":	"LGD FHD SW43101 VIDEO OLED PANEL"
 *	}
 * }
 *
 * $ cat decrypt.py
 * #!/usr/bin/python
 *
 * from Crypto.Cipher import AES
 *
 * fs = open('./hw_info', 'r+')
 * key = b'01234567890123456789012345678901'
 * dec = AES.new(key, AES.MODE_ECB)
 * ciphertext = fs.read()
 * print dec.decrypt(ciphertext)
 * fs.close()
 *
 */
static ssize_t hw_info_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	int ret;
	char *src = cJSON_Print(info_manager->hw_config);
	int i;
	unsigned int blocksize;
	char *padding;
	int blocks = 0;

	if (!strncmp(crypto_key, INIT_KEY, sizeof(crypto_key))) {
		pr_err("crypto_key == INIT_KEY\n");
		return 0;
	}

	/* Allocate transform for AES CRYPTO_ALG_TYPE_BLKCIPHER */
	info_manager->tfm = crypto_alloc_cipher("aes",
			CRYPTO_ALG_TYPE_BLKCIPHER, CRYPTO_ALG_ASYNC);
	if (IS_ERR(info_manager->tfm)) {
		pr_err("Failed to load transform for aes mode!\n");
		return 0;
	}

	pr_debug("%s crypto_key=%s\n", __func__, crypto_key);
	ret = crypto_cipher_setkey(info_manager->tfm, crypto_key,
				   sizeof(crypto_key));
	if (ret) {
		pr_err("Failed to setkey\n");
		crypto_free_cipher(info_manager->tfm);
		return 0;
	}

	blocksize = crypto_cipher_blocksize(info_manager->tfm);
	if (!blocksize)
		return 0;

	padding = kmalloc(blocksize + 1, GFP_KERNEL);
	if (!padding)
		return -ENOMEM;

	/* start encrypt */
	for (i = 0; i < strlen(src); i += blocksize) {
		memset(padding, 0, blocksize + 1);
		strlcpy(padding, &src[i], blocksize + 1);
		crypto_cipher_encrypt_one(info_manager->tfm,
					  &buf[i], padding);
		blocks++;
	}

	kfree(padding);
	crypto_free_cipher(info_manager->tfm);

#if __HWINFO_DECRYPT_DEBUG__
	hw_info_decrypt_test(buf, blocks * blocksize);
#endif

	return blocks * blocksize;
}

static ssize_t hw_mon_store(struct kobject *kobj,
			    struct kobj_attribute *attr, const char *buf,
			    size_t count)
{
	int on = 99;
	char component_name[32] = { 0 };
	int len;

	len = sscanf(buf, "%s %d", component_name, &on);

	pr_debug("%s component_name=%s, on=%d\n", __func__, component_name, on);

	if (on == 0)
		unregister_hw_monitor_info(component_name);

	return count;
}

static ssize_t hw_mon_show(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	mutex_lock(&hw_mon_notifier_lock);
	raw_notifier_call_chain(&hw_mon_notifier_list,
					0, NULL);
	mutex_unlock(&hw_mon_notifier_lock);
	return snprintf(buf, PAGE_SIZE, "%s\n",
			cJSON_Print(info_manager->hw_monitor));
}

hwconf_attr(hw_info);
hwconf_attr(hw_mon);

static struct attribute *g[] = {
	&hw_info_attr.attr,
	&hw_mon_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int hwconf_debugfs_get(void *data, u64 *val)
{
	pr_debug("hw_config:\n%s\n", cJSON_Print(info_manager->hw_config));
	pr_debug("hw_monitor:\n%s\n", cJSON_Print(info_manager->hw_monitor));
	*val = 0;
	return 0;
}

static int hwconf_debugfs_set(void *data, u64 val)
{
	if (val == 1) {
		register_hw_component_info("debugfs_hwconf");
		add_hw_component_info("debugfs_hwconf", "key1", "value1");

		register_hw_monitor_info("debugfs_hwmon");
		add_hw_monitor_info("debugfs_hwmon", "key2", "value2");
	} else if (val == 2) {
		update_hw_monitor_info("debugfs_hwmon", "key2", "value3");
	} else if (val == 3) {
		unregister_hw_component_info("debugfs_hwconf");
		unregister_hw_monitor_info("debugfs_hwmon");
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hwconf_check_fops, hwconf_debugfs_get,
	hwconf_debugfs_set, "%llu\n");

int hw_monitor_notifier_register(struct notifier_block *nb)
{
	int ret;

	if (!nb || !info_manager->hw_mon_inited)
		return -EINVAL;
	mutex_lock(&hw_mon_notifier_lock);
	ret = raw_notifier_chain_register(&hw_mon_notifier_list, nb);
	if (info_manager->hw_mon_inited)
		nb->notifier_call(nb, 0, NULL);
	mutex_unlock(&hw_mon_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(hw_monitor_notifier_register);

int hw_monitor_notifier_unregister(struct notifier_block *nb)
{
	int ret;

	if (!nb || !info_manager->hw_mon_inited)
		return -EINVAL;
	mutex_lock(&hw_mon_notifier_lock);
	ret = raw_notifier_chain_unregister(&hw_mon_notifier_list,
						nb);
	mutex_unlock(&hw_mon_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(hw_monitor_notifier_unregister);

static int __init hwconf_init(void)
{
	int ret = -ENOMEM;

	info_manager = kmalloc(sizeof(struct hw_info_manager), GFP_KERNEL);
	if (!info_manager)
		return ret;
	memset(info_manager, 0, sizeof(struct hw_info_manager));
	info_manager->hw_config = cJSON_CreateObject();
	info_manager->hw_monitor = cJSON_CreateObject();

	info_manager->hwconf_kobj = kobject_create_and_add("hwconf", NULL);
	if (!info_manager->hwconf_kobj) {
		pr_err("hwconf_init: subsystem_register failed\n");
		goto fail;
	}

	ret = sysfs_create_group(info_manager->hwconf_kobj, &attr_group);
	if (ret) {
		pr_err("hwconf_init: subsystem_register failed\n");
		goto sys_fail;
	}

	info_manager->hwconf_check = debugfs_create_file("hwconf_check",
			0644, NULL, NULL, &hwconf_check_fops);

	info_manager->hw_mon_inited = 1;

	return ret;

sys_fail:
	kobject_del(info_manager->hwconf_kobj);
fail:
	cJSON_Delete(info_manager->hw_config);
	cJSON_Delete(info_manager->hw_monitor);
	kfree(info_manager);

	return ret;
}

static void __exit hwconf_exit(void)
{
	cJSON_Delete(info_manager->hw_config);
	cJSON_Delete(info_manager->hw_monitor);

	if (info_manager->hwconf_kobj) {
		sysfs_remove_group(info_manager->hwconf_kobj, &attr_group);
		kobject_del(info_manager->hwconf_kobj);
	}
	debugfs_remove(info_manager->hwconf_check);
	kfree(info_manager);
}

core_initcall(hwconf_init);
module_exit(hwconf_exit);
