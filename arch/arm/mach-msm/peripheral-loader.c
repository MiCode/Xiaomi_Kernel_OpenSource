/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/elf.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>

#include <asm/uaccess.h>
#include <asm/setup.h>
#include <mach/peripheral-loader.h>

#include "peripheral-loader.h"

#define pil_err(desc, fmt, ...)						\
	dev_err(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)
#define pil_info(desc, fmt, ...)					\
	dev_info(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)

/**
 * proxy_timeout - Override for proxy vote timeouts
 * -1: Use driver-specified timeout
 *  0: Hold proxy votes until shutdown
 * >0: Specify a custom timeout in ms
 */
static int proxy_timeout_ms = -1;
module_param(proxy_timeout_ms, int, S_IRUGO | S_IWUSR);

enum pil_state {
	PIL_OFFLINE,
	PIL_ONLINE,
};

static const char *pil_states[] = {
	[PIL_OFFLINE] = "OFFLINE",
	[PIL_ONLINE] = "ONLINE",
};

struct pil_device {
	struct pil_desc *desc;
	int count;
	enum pil_state state;
	struct mutex lock;
	struct device dev;
	struct module *owner;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
};

/**
 * struct pil_priv - Private state for a pil_desc
 * @proxy: work item used to run the proxy unvoting routine
 * @wlock: wakelock to prevent suspend during pil_boot
 * @wname: name of @wlock
 * @desc: pointer to pil_desc this is private data for
 *
 * This struct contains data for a pil_desc that should not be exposed outside
 * of this file. This structure points to the descriptor and the descriptor
 * points to this structure so that PIL drivers can't access the private
 * data of a descriptor but this file can access both.
 */
struct pil_priv {
	struct delayed_work proxy;
	struct wake_lock wlock;
	char wname[32];
	struct pil_desc *desc;
};

#define to_pil_device(d) container_of(d, struct pil_device, dev)

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", to_pil_device(dev)->desc->name);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	enum pil_state state = to_pil_device(dev)->state;
	return snprintf(buf, PAGE_SIZE, "%s\n", pil_states[state]);
}

static struct device_attribute pil_attrs[] = {
	__ATTR_RO(name),
	__ATTR_RO(state),
	{ },
};

struct bus_type pil_bus_type = {
	.name		= "pil",
	.dev_attrs	= pil_attrs,
};

static int __find_peripheral(struct device *dev, void *data)
{
	struct pil_device *pdev = to_pil_device(dev);
	return !strncmp(pdev->desc->name, data, INT_MAX);
}

static struct pil_device *find_peripheral(const char *str)
{
	struct device *dev;

	if (!str)
		return NULL;

	dev = bus_find_device(&pil_bus_type, NULL, (void *)str,
			__find_peripheral);
	return dev ? to_pil_device(dev) : NULL;
}

static void pil_proxy_work(struct work_struct *work)
{
	struct delayed_work *delayed = to_delayed_work(work);
	struct pil_priv *priv = container_of(delayed, struct pil_priv, proxy);
	struct pil_desc *desc = priv->desc;

	desc->ops->proxy_unvote(desc);
	wake_unlock(&priv->wlock);
	module_put(desc->owner);
}

static int pil_proxy_vote(struct pil_desc *desc)
{
	int ret = 0;
	struct pil_priv *priv = desc->priv;

	if (desc->ops->proxy_vote) {
		wake_lock(&priv->wlock);
		ret = desc->ops->proxy_vote(desc);
		if (ret)
			wake_unlock(&priv->wlock);
	}
	return ret;
}

static void pil_proxy_unvote(struct pil_desc *desc, unsigned long timeout)
{
	struct pil_priv *priv = desc->priv;

	if (proxy_timeout_ms >= 0)
		timeout = proxy_timeout_ms;

	if (timeout && desc->ops->proxy_unvote) {
		if (WARN_ON(!try_module_get(desc->owner)))
			return;
		schedule_delayed_work(&priv->proxy, msecs_to_jiffies(timeout));
	}
}

#define IOMAP_SIZE SZ_4M

static int load_segment(const struct elf32_phdr *phdr, unsigned num,
		struct pil_desc *desc)
{
	int ret = 0, count, paddr;
	char fw_name[30];
	const struct firmware *fw = NULL;
	const u8 *data;

	if (memblock_overlaps_memory(phdr->p_paddr, phdr->p_memsz)) {
		pil_err(desc, "kernel memory would be overwritten [%#08lx, %#08lx)\n",
			(unsigned long)phdr->p_paddr,
			(unsigned long)(phdr->p_paddr + phdr->p_memsz));
		return -EPERM;
	}

	if (phdr->p_filesz) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d",
				desc->name, num);
		ret = request_firmware(&fw, fw_name, desc->dev);
		if (ret) {
			pil_err(desc, "Failed to locate blob %s\n", fw_name);
			return ret;
		}

		if (fw->size != phdr->p_filesz) {
			pil_err(desc, "Blob size %u doesn't match %u\n",
					fw->size, phdr->p_filesz);
			ret = -EPERM;
			goto release_fw;
		}
	}

	/* Load the segment into memory */
	count = phdr->p_filesz;
	paddr = phdr->p_paddr;
	data = fw ? fw->data : NULL;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			pil_err(desc, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memcpy(buf, data, size);
		iounmap(buf);

		count -= size;
		paddr += size;
		data += size;
	}

	/* Zero out trailing memory */
	count = phdr->p_memsz - phdr->p_filesz;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			pil_err(desc, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memset(buf, 0, size);
		iounmap(buf);

		count -= size;
		paddr += size;
	}

	if (desc->ops->verify_blob) {
		ret = desc->ops->verify_blob(desc, phdr->p_paddr,
					  phdr->p_memsz);
		if (ret)
			pil_err(desc, "Blob%u failed verification\n", num);
	}

release_fw:
	release_firmware(fw);
	return ret;
}

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type == PT_LOAD) && !segment_is_hash(p->p_flags);
}

/* Synchronize request_firmware() with suspend */
static DECLARE_RWSEM(pil_pm_rwsem);

/**
 * pil_boot() - Load a peripheral image into memory and boot it
 * @desc: descriptor from pil_desc_init()
 *
 * Returns 0 on success or -ERROR on failure.
 */
int pil_boot(struct pil_desc *desc)
{
	int i, ret;
	char fw_name[30];
	struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const struct firmware *fw;
	unsigned long proxy_timeout = desc->proxy_timeout;

	down_read(&pil_pm_rwsem);
	snprintf(fw_name, sizeof(fw_name), "%s.mdt", desc->name);
	ret = request_firmware(&fw, fw_name, desc->dev);
	if (ret) {
		pil_err(desc, "Failed to locate %s\n", fw_name);
		goto out;
	}

	if (fw->size < sizeof(*ehdr)) {
		pil_err(desc, "Not big enough to be an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	ehdr = (struct elf32_hdr *)fw->data;
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		pil_err(desc, "Not an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	if (ehdr->e_phnum == 0) {
		pil_err(desc, "No loadable segments\n");
		ret = -EIO;
		goto release_fw;
	}
	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw->size) {
		pil_err(desc, "Program headers not within mdt\n");
		ret = -EIO;
		goto release_fw;
	}

	ret = desc->ops->init_image(desc, fw->data, fw->size);
	if (ret) {
		pil_err(desc, "Invalid firmware metadata\n");
		goto release_fw;
	}

	phdr = (const struct elf32_phdr *)(fw->data + sizeof(struct elf32_hdr));
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		if (!segment_is_loadable(phdr))
			continue;

		ret = load_segment(phdr, i, desc);
		if (ret) {
			pil_err(desc, "Failed to load segment %d\n", i);
			goto release_fw;
		}
	}

	ret = pil_proxy_vote(desc);
	if (ret) {
		pil_err(desc, "Failed to proxy vote\n");
		goto release_fw;
	}

	ret = desc->ops->auth_and_reset(desc);
	if (ret) {
		pil_err(desc, "Failed to bring out of reset\n");
		proxy_timeout = 0; /* Remove proxy vote immediately on error */
		goto err_boot;
	}
	pil_info(desc, "Brought out of reset\n");
err_boot:
	pil_proxy_unvote(desc, proxy_timeout);
release_fw:
	release_firmware(fw);
out:
	up_read(&pil_pm_rwsem);
	return ret;
}
EXPORT_SYMBOL(pil_boot);

static int load_image(struct pil_device *pil)
{
	return pil_boot(pil->desc);
}

static void pil_set_state(struct pil_device *pil, enum pil_state state)
{
	if (pil->state != state) {
		pil->state = state;
		sysfs_notify(&pil->dev.kobj, NULL, "state");
	}
}

/**
 * pil_get() - Load a peripheral into memory and take it out of reset
 * @name: pointer to a string containing the name of the peripheral to load
 *
 * This function returns a pointer if it succeeds. If an error occurs an
 * ERR_PTR is returned.
 *
 * If PIL is not enabled in the kernel, the value %NULL will be returned.
 */
void *pil_get(const char *name)
{
	int ret;
	struct pil_device *pil;
	struct pil_device *pil_d;
	void *retval;

	if (!name)
		return NULL;

	pil = retval = find_peripheral(name);
	if (!pil)
		return ERR_PTR(-ENODEV);
	if (!try_module_get(pil->owner)) {
		put_device(&pil->dev);
		return ERR_PTR(-ENODEV);
	}

	pil_d = pil_get(pil->desc->depends_on);
	if (IS_ERR(pil_d)) {
		retval = pil_d;
		goto err_depends;
	}

	mutex_lock(&pil->lock);
	if (!pil->count) {
		ret = load_image(pil);
		if (ret) {
			retval = ERR_PTR(ret);
			goto err_load;
		}
	}
	pil->count++;
	pil_set_state(pil, PIL_ONLINE);
	mutex_unlock(&pil->lock);
out:
	return retval;
err_load:
	mutex_unlock(&pil->lock);
	pil_put(pil_d);
err_depends:
	put_device(&pil->dev);
	module_put(pil->owner);
	goto out;
}
EXPORT_SYMBOL(pil_get);

/**
 * pil_shutdown() - Shutdown a peripheral
 * @desc: descriptor from pil_desc_init()
 */
void pil_shutdown(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;
	desc->ops->shutdown(desc);
	if (proxy_timeout_ms == 0 && desc->ops->proxy_unvote)
		desc->ops->proxy_unvote(desc);
	else
		flush_delayed_work(&priv->proxy);
}
EXPORT_SYMBOL(pil_shutdown);

static void shutdown_image(struct pil_device *pil)
{
	pil_shutdown(pil->desc);
	pil_set_state(pil, PIL_OFFLINE);
}

/**
 * pil_put() - Inform PIL the peripheral no longer needs to be active
 * @peripheral_handle: pointer from a previous call to pil_get()
 *
 * This doesn't imply that a peripheral is shutdown or in reset since another
 * driver could be using the peripheral.
 */
void pil_put(void *peripheral_handle)
{
	struct pil_device *pil_d, *pil = peripheral_handle;

	if (IS_ERR_OR_NULL(pil))
		return;

	mutex_lock(&pil->lock);
	if (WARN(!pil->count, "%s: %s: Reference count mismatch\n",
			pil->desc->name, __func__))
		goto err_out;
	if (!--pil->count)
		shutdown_image(pil);
	mutex_unlock(&pil->lock);

	pil_d = find_peripheral(pil->desc->depends_on);
	module_put(pil->owner);
	if (pil_d) {
		pil_put(pil_d);
		put_device(&pil_d->dev);
	}
	put_device(&pil->dev);
	return;
err_out:
	mutex_unlock(&pil->lock);
	return;
}
EXPORT_SYMBOL(pil_put);

void pil_force_shutdown(const char *name)
{
	struct pil_device *pil;

	pil = find_peripheral(name);
	if (!pil) {
		pr_err("%s: Couldn't find %s\n", __func__, name);
		return;
	}

	mutex_lock(&pil->lock);
	if (!WARN(!pil->count, "%s: %s: Reference count mismatch\n",
			pil->desc->name, __func__))
		shutdown_image(pil);
	mutex_unlock(&pil->lock);

	put_device(&pil->dev);
}
EXPORT_SYMBOL(pil_force_shutdown);

int pil_force_boot(const char *name)
{
	int ret = -EINVAL;
	struct pil_device *pil;

	pil = find_peripheral(name);
	if (!pil) {
		pr_err("%s: Couldn't find %s\n", __func__, name);
		return -EINVAL;
	}

	mutex_lock(&pil->lock);
	if (!WARN(!pil->count, "%s: %s: Reference count mismatch\n",
			pil->desc->name, __func__))
		ret = load_image(pil);
	if (!ret)
		pil_set_state(pil, PIL_ONLINE);
	mutex_unlock(&pil->lock);
	put_device(&pil->dev);

	return ret;
}
EXPORT_SYMBOL(pil_force_boot);

#ifdef CONFIG_DEBUG_FS
static int msm_pil_debugfs_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t msm_pil_debugfs_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	int r;
	char buf[40];
	struct pil_device *pil = filp->private_data;

	mutex_lock(&pil->lock);
	r = snprintf(buf, sizeof(buf), "%d\n", pil->count);
	mutex_unlock(&pil->lock);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t msm_pil_debugfs_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct pil_device *pil = filp->private_data;
	char buf[4];

	if (cnt > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	if (!strncmp(buf, "get", 3)) {
		if (IS_ERR(pil_get(pil->desc->name)))
			return -EIO;
	} else if (!strncmp(buf, "put", 3))
		pil_put(pil);
	else
		return -EINVAL;

	return cnt;
}

static const struct file_operations msm_pil_debugfs_fops = {
	.open	= msm_pil_debugfs_open,
	.read	= msm_pil_debugfs_read,
	.write	= msm_pil_debugfs_write,
};

static struct dentry *pil_base_dir;

static int __init msm_pil_debugfs_init(void)
{
	pil_base_dir = debugfs_create_dir("pil", NULL);
	if (!pil_base_dir) {
		pil_base_dir = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void __exit msm_pil_debugfs_exit(void)
{
	debugfs_remove_recursive(pil_base_dir);
}

static int msm_pil_debugfs_add(struct pil_device *pil)
{
	if (!pil_base_dir)
		return -ENOMEM;

	pil->dentry = debugfs_create_file(pil->desc->name, S_IRUGO | S_IWUSR,
				pil_base_dir, pil, &msm_pil_debugfs_fops);
	return !pil->dentry ? -ENOMEM : 0;
}

static void msm_pil_debugfs_remove(struct pil_device *pil)
{
	debugfs_remove(pil->dentry);
}
#else
static int __init msm_pil_debugfs_init(void) { return 0; };
static void __exit msm_pil_debugfs_exit(void) { return 0; };
static int msm_pil_debugfs_add(struct pil_device *pil) { return 0; }
static void msm_pil_debugfs_remove(struct pil_device *pil) { }
#endif

static void pil_device_release(struct device *dev)
{
	struct pil_device *pil = to_pil_device(dev);
	mutex_destroy(&pil->lock);
	kfree(pil);
}

/**
 * pil_desc_init() - Initialize a pil descriptor
 * @desc: descriptor to intialize
 *
 * Initialize a pil descriptor for use by other pil functions. This function
 * must be called before calling pil_boot() or pil_shutdown().
 *
 * Returns 0 for success and -ERROR on failure.
 */
int pil_desc_init(struct pil_desc *desc)
{
	struct pil_priv *priv;

	/* Ignore users who don't make any sense */
	WARN(desc->ops->proxy_unvote && !desc->proxy_timeout,
			"A proxy timeout of 0 was specified.\n");
	if (WARN(desc->ops->proxy_unvote && !desc->ops->proxy_vote,
				"Invalid proxy voting. Ignoring\n"))
		((struct pil_reset_ops *)desc->ops)->proxy_unvote = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	desc->priv = priv;
	priv->desc = desc;

	snprintf(priv->wname, sizeof(priv->wname), "pil-%s", desc->name);
	wake_lock_init(&priv->wlock, WAKE_LOCK_SUSPEND, priv->wname);
	INIT_DELAYED_WORK(&priv->proxy, pil_proxy_work);

	return 0;
}
EXPORT_SYMBOL(pil_desc_init);

/**
 * pil_desc_release() - Release a pil descriptor
 * @desc: descriptor to free
 */
void pil_desc_release(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;

	if (priv) {
		flush_delayed_work(&priv->proxy);
		wake_lock_destroy(&priv->wlock);
	}
	desc->priv = NULL;
	kfree(priv);
}
EXPORT_SYMBOL(pil_desc_release);

struct pil_device *msm_pil_register(struct pil_desc *desc)
{
	int err;
	static atomic_t pil_count = ATOMIC_INIT(-1);
	struct pil_device *pil;

	pil = kzalloc(sizeof(*pil), GFP_KERNEL);
	if (!pil)
		return ERR_PTR(-ENOMEM);

	err = pil_desc_init(desc);
	if (err)
		return ERR_PTR(err);

	mutex_init(&pil->lock);
	pil->desc = desc;
	pil->owner = desc->owner;
	pil->dev.parent = desc->dev;
	pil->dev.bus = &pil_bus_type;
	pil->dev.release = pil_device_release;

	dev_set_name(&pil->dev, "pil%d", atomic_inc_return(&pil_count));
	err = device_register(&pil->dev);
	if (err) {
		put_device(&pil->dev);
		mutex_destroy(&pil->lock);
		kfree(pil);
		return ERR_PTR(err);
	}

	err = msm_pil_debugfs_add(pil);
	if (err) {
		device_unregister(&pil->dev);
		return ERR_PTR(err);
	}

	return pil;
}
EXPORT_SYMBOL(msm_pil_register);

void msm_pil_unregister(struct pil_device *pil)
{
	if (IS_ERR_OR_NULL(pil))
		return;

	if (get_device(&pil->dev)) {
		mutex_lock(&pil->lock);
		WARN_ON(pil->count);
		msm_pil_debugfs_remove(pil);
		device_unregister(&pil->dev);
		mutex_unlock(&pil->lock);
		pil_desc_release(pil->desc);
		put_device(&pil->dev);
	}
}
EXPORT_SYMBOL(msm_pil_unregister);

static int pil_pm_notify(struct notifier_block *b, unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&pil_pm_rwsem);
		break;
	case PM_POST_SUSPEND:
		up_write(&pil_pm_rwsem);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block pil_pm_notifier = {
	.notifier_call = pil_pm_notify,
};

static int __init msm_pil_init(void)
{
	int ret = msm_pil_debugfs_init();
	if (ret)
		return ret;
	register_pm_notifier(&pil_pm_notifier);
	return bus_register(&pil_bus_type);
}
subsys_initcall(msm_pil_init);

static void __exit msm_pil_exit(void)
{
	bus_unregister(&pil_bus_type);
	unregister_pm_notifier(&pil_pm_notifier);
	msm_pil_debugfs_exit();
}
module_exit(msm_pil_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Load peripheral images and bring peripherals out of reset");
