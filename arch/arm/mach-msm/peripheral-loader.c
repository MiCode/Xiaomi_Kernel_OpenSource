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
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>

#include <asm/uaccess.h>
#include <asm/setup.h>

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
	return register_pm_notifier(&pil_pm_notifier);
}
subsys_initcall(msm_pil_init);

static void __exit msm_pil_exit(void)
{
	unregister_pm_notifier(&pil_pm_notifier);
}
module_exit(msm_pil_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Load peripheral images and bring peripherals out of reset");
