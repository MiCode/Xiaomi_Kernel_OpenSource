/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/cs.h>
#include <linux/cs-stm.h>
#include <asm/unaligned.h>

#include "cs-priv.h"

#define stm_writel(stm, val, off)	\
			__raw_writel((val), stm.base + off)
#define stm_readl(stm, val, off)	\
			__raw_readl(stm.base + off)

#define NR_STM_CHANNEL		(32)
#define BYTES_PER_CHANNEL	(256)

enum {
	STM_PKT_TYPE_DATA	= 0x98,
	STM_PKT_TYPE_FLAG	= 0xE8,
	STM_PKT_TYPE_TRIG	= 0xF8,
};

enum {
	STM_OPTION_MARKED	= 0x10,
};

#define STM_TRACE_BUF_SIZE	(1024)

#define OST_START_TOKEN		(0x30)
#define OST_VERSION		(0x1)

#define stm_channel_addr(ch)						\
				(stm.chs.base + (ch * BYTES_PER_CHANNEL))
#define stm_channel_off(type, opts)	(type & ~opts)

#define STM_LOCK()							\
do {									\
	mb();								\
	stm_writel(stm, 0x0, CS_LAR);					\
} while (0)
#define STM_UNLOCK()							\
do {									\
	stm_writel(stm, CS_UNLOCK_MAGIC, CS_LAR);			\
	mb();								\
} while (0)

#define STMSPER		(0xE00)
#define STMSPTER	(0xE20)
#define STMTCSR		(0xE80)
#define STMSYNCR	(0xE90)

#ifdef CONFIG_MSM_QDSS_STM_DEFAULT_ENABLE
static int stm_boot_enable = 1;
#else
static int stm_boot_enable;
#endif

module_param_named(
	stm_boot_enable, stm_boot_enable, int, S_IRUGO
);

static int stm_boot_nr_channel;

module_param_named(
	stm_boot_nr_channel, stm_boot_nr_channel, int, S_IRUGO
);

struct channel_space {
	void __iomem		*base;
	unsigned long		*bitmap;
};

struct stm_ctx {
	void __iomem		*base;
	bool			enabled;
	struct qdss_source	*src;
	struct device		*dev;
	struct kobject		*kobj;
	struct clk		*clk;
	uint32_t		entity;
	struct channel_space	chs;
};

static struct stm_ctx stm = {
	.entity		= OST_ENTITY_ALL,
};


static void __stm_enable(void)
{
	STM_UNLOCK();

	stm_writel(stm, 0x80, STMSYNCR);
	stm_writel(stm, 0xFFFFFFFF, STMSPTER);
	stm_writel(stm, 0xFFFFFFFF, STMSPER);
	stm_writel(stm, 0x30003, STMTCSR);

	STM_LOCK();
}

static int stm_enable(void)
{
	int ret;

	if (stm.enabled) {
		dev_err(stm.dev, "STM tracing already enabled\n");
		ret = -EINVAL;
		goto err;
	}

	ret = clk_prepare_enable(stm.clk);
	if (ret)
		goto err_clk;

	ret = qdss_enable(stm.src);
	if (ret)
		goto err_qdss;

	__stm_enable();

	stm.enabled = true;

	dev_info(stm.dev, "STM tracing enabled\n");
	return 0;

err_qdss:
	clk_disable_unprepare(stm.clk);
err_clk:
err:
	return ret;
}

static void __stm_disable(void)
{
	STM_UNLOCK();

	stm_writel(stm, 0x30000, STMTCSR);
	stm_writel(stm, 0x0, STMSPER);
	stm_writel(stm, 0x0, STMSPTER);

	STM_LOCK();
}

static int stm_disable(void)
{
	int ret;

	if (!stm.enabled) {
		dev_err(stm.dev, "STM tracing already disabled\n");
		ret = -EINVAL;
		goto err;
	}

	__stm_disable();

	stm.enabled = false;

	qdss_disable(stm.src);

	clk_disable_unprepare(stm.clk);

	dev_info(stm.dev, "STM tracing disabled\n");
	return 0;

err:
	return ret;
}

static uint32_t stm_channel_alloc(uint32_t off)
{
	uint32_t ch;

	do {
		ch = find_next_zero_bit(stm.chs.bitmap,	NR_STM_CHANNEL, off);
	} while ((ch < NR_STM_CHANNEL) && test_and_set_bit(ch, stm.chs.bitmap));

	return ch;
}

static void stm_channel_free(uint32_t ch)
{
	clear_bit(ch, stm.chs.bitmap);
}

static int stm_send(void *addr, const void *data, uint32_t size)
{
	uint64_t prepad = 0;
	uint64_t postpad = 0;
	char *pad;
	uint8_t off, endoff;
	uint32_t len = size;

	/* only 64bit writes are supported, we rely on the compiler to
	 * generate STRD instruction for the casted 64bit assignments
	 */

	off = (unsigned long)data & 0x7;

	if (off) {
		endoff = 8 - off;
		pad = (char *)&prepad;
		pad += off;

		while (endoff && size) {
			*pad++ = *(char *)data++;
			endoff--;
			size--;
		}
		*(volatile uint64_t __force *)addr = prepad;
	}

	/* now we are 64bit aligned */
	while (size >= 8) {
		*(volatile uint64_t __force *)addr = *(uint64_t *)data;
		data += 8;
		size -= 8;
	}

	if (size) {
		pad = (char *)&postpad;

		while (size) {
			*pad++ = *(char *)data++;
			size--;
		}
		*(volatile uint64_t __force *)addr = postpad;
	}

	return roundup(len + off, 8);
}

static int stm_trace_ost_header(unsigned long ch_addr, uint32_t options,
				uint8_t entity_id, uint8_t proto_id,
				const void *payload_data, uint32_t payload_size)
{
	void *addr;
	uint8_t prepad_size;
	uint64_t header;
	char *hdr;

	hdr = (char *)&header;

	hdr[0] = OST_START_TOKEN;
	hdr[1] = OST_VERSION;
	hdr[2] = entity_id;
	hdr[3] = proto_id;
	prepad_size = (unsigned long)payload_data & 0x7;
	*(uint32_t *)(hdr + 4) = (prepad_size << 24) | payload_size;

	/* for 64bit writes, header is expected to be of the D32M, D32M */
	options |= STM_OPTION_MARKED;
	options &= ~STM_OPTION_TIMESTAMPED;
	addr =  (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_DATA, options));

	return stm_send(addr, &header, sizeof(header));
}

static int stm_trace_data(unsigned long ch_addr, uint32_t options,
			  const void *data, uint32_t size)
{
	void *addr;

	options &= ~STM_OPTION_TIMESTAMPED;
	addr = (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_DATA, options));

	return stm_send(addr, data, size);
}

static int stm_trace_ost_tail(unsigned long ch_addr, uint32_t options)
{
	void *addr;
	uint64_t tail = 0x0;

	addr = (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_FLAG, options));

	return stm_send(addr, &tail, sizeof(tail));
}

static inline int __stm_trace(uint32_t options, uint8_t entity_id,
			      uint8_t proto_id, const void *data, uint32_t size)
{
	int len = 0;
	uint32_t ch;
	unsigned long ch_addr;

	/* allocate channel and get the channel address */
	ch = stm_channel_alloc(0);
	ch_addr = (unsigned long)stm_channel_addr(ch);

	/* send the ost header */
	len += stm_trace_ost_header(ch_addr, options, entity_id, proto_id, data,
				    size);

	/* send the payload data */
	len += stm_trace_data(ch_addr, options, data, size);

	/* send the ost tail */
	len += stm_trace_ost_tail(ch_addr, options);

	/* we are done, free the channel */
	stm_channel_free(ch);

	return len;
}

/**
 * stm_trace - trace the binary or string data through STM
 * @options: tracing options - guaranteed, timestamped, etc
 * @entity_id: entity representing the trace data
 * @proto_id: protocol id to distinguish between different binary formats
 * @data: pointer to binary or string data buffer
 * @size: size of data to send
 *
 * Packetizes the data as the payload to an OST packet and sends it over STM
 *
 * CONTEXT:
 * Can be called from any context.
 *
 * RETURNS:
 * number of bytes transfered over STM
 */
int stm_trace(uint32_t options, uint8_t entity_id, uint8_t proto_id,
	      const void *data, uint32_t size)
{
	/* we don't support sizes more than 24bits (0 to 23) */
	if (!(stm.enabled && (stm.entity & entity_id) &&
	      (size < 0x1000000)))
		return 0;

	return __stm_trace(options, entity_id, proto_id, data, size);
}
EXPORT_SYMBOL(stm_trace);

static ssize_t stm_write(struct file *file, const char __user *data,
			 size_t size, loff_t *ppos)
{
	char *buf;

	if (!stm.enabled)
		return -EINVAL;

	if (!(stm.entity & OST_ENTITY_DEV_NODE))
		return size;

	if (size > STM_TRACE_BUF_SIZE)
		size = STM_TRACE_BUF_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, data, size)) {
		kfree(buf);
		dev_dbg(stm.dev, "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	__stm_trace(STM_OPTION_TIMESTAMPED, OST_ENTITY_DEV_NODE, 0, buf, size);

	kfree(buf);

	return size;
}

static const struct file_operations stm_fops = {
	.owner		= THIS_MODULE,
	.write		= stm_write,
	.llseek		= no_llseek,
};

static struct miscdevice stm_misc = {
	.name		= "msm_stm",
	.minor		= MISC_DYNAMIC_MINOR,
	.fops		= &stm_fops,
};

#define STM_ATTR(__name)						\
static struct kobj_attribute __name##_attr =				\
	__ATTR(__name, S_IRUGO | S_IWUSR, __name##_show, __name##_store)

static ssize_t enabled_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	int ret = 0;
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = stm_enable();
	else
		ret = stm_disable();

	if (ret)
		return ret;
	return n;
}
static ssize_t enabled_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = stm.enabled;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
STM_ATTR(enabled);

static ssize_t entity_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	stm.entity = val;
	return n;
}
static ssize_t entity_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = stm.entity;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
STM_ATTR(entity);

static int stm_sysfs_init(void)
{
	int ret;

	stm.kobj = kobject_create_and_add("stm", qdss_get_modulekobj());
	if (!stm.kobj) {
		dev_err(stm.dev, "failed to create STM sysfs kobject\n");
		ret = -ENOMEM;
		goto err_create;
	}

	ret = sysfs_create_file(stm.kobj, &enabled_attr.attr);
	if (ret) {
		dev_err(stm.dev, "failed to create STM sysfs enabled attr\n");
		goto err_file;
	}

	if (sysfs_create_file(stm.kobj, &entity_attr.attr))
		dev_err(stm.dev, "failed to create STM sysfs entity attr\n");

	return 0;
err_file:
	kobject_put(stm.kobj);
err_create:
	return ret;
}

static void stm_sysfs_exit(void)
{
	sysfs_remove_file(stm.kobj, &entity_attr.attr);
	sysfs_remove_file(stm.kobj, &enabled_attr.attr);
	kobject_put(stm.kobj);
}

static int stm_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	size_t res_size, bitmap_size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res0;
	}

	stm.base = ioremap_nocache(res->start, resource_size(res));
	if (!stm.base) {
		ret = -EINVAL;
		goto err_ioremap0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		ret = -EINVAL;
		goto err_res1;
	}

	if (stm_boot_nr_channel) {
		res_size = min((resource_size_t)(stm_boot_nr_channel *
				  BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = stm_boot_nr_channel * sizeof(long);
	} else {
		res_size = min((resource_size_t)(NR_STM_CHANNEL *
				 BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = NR_STM_CHANNEL * sizeof(long);
	}

	stm.chs.bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!stm.chs.bitmap) {
		ret = -ENOMEM;
		goto err_bitmap;
	}

	stm.chs.base = ioremap_nocache(res->start, res_size);
	if (!stm.chs.base) {
		ret = -EINVAL;
		goto err_ioremap1;
	}

	stm.dev = &pdev->dev;

	ret = misc_register(&stm_misc);
	if (ret)
		goto err_misc;

	stm.src = qdss_get("msm_stm");
	if (IS_ERR(stm.src)) {
		ret = PTR_ERR(stm.src);
		goto err_qdssget;
	}

	stm.clk = clk_get(stm.dev, "core_clk");
	if (IS_ERR(stm.clk)) {
		ret = PTR_ERR(stm.clk);
		goto err_clk_get;
	}

	ret = clk_set_rate(stm.clk, CS_CLK_RATE_TRACE);
	if (ret)
		goto err_clk_rate;

	ret = stm_sysfs_init();
	if (ret)
		goto err_sysfs;

	if (stm_boot_enable)
		stm_enable();

	dev_info(stm.dev, "STM initialized\n");
	return 0;

err_sysfs:
err_clk_rate:
	clk_put(stm.clk);
err_clk_get:
	qdss_put(stm.src);
err_qdssget:
	misc_deregister(&stm_misc);
err_misc:
	iounmap(stm.chs.base);
err_ioremap1:
	kfree(stm.chs.bitmap);
err_bitmap:
err_res1:
	iounmap(stm.base);
err_ioremap0:
err_res0:
	dev_err(stm.dev, "STM init failed\n");
	return ret;
}

static int stm_remove(struct platform_device *pdev)
{
	if (stm.enabled)
		stm_disable();
	stm_sysfs_exit();
	clk_put(stm.clk);
	qdss_put(stm.src);
	misc_deregister(&stm_misc);
	iounmap(stm.chs.base);
	kfree(stm.chs.bitmap);
	iounmap(stm.base);

	return 0;
}

static struct of_device_id stm_match[] = {
	{.compatible = "qcom,msm-stm"},
	{}
};

static struct platform_driver stm_driver = {
	.probe          = stm_probe,
	.remove         = stm_remove,
	.driver         = {
		.name   = "msm_stm",
		.owner	= THIS_MODULE,
		.of_match_table = stm_match,
	},
};

static int __init stm_init(void)
{
	return platform_driver_register(&stm_driver);
}
module_init(stm_init);

static void __exit stm_exit(void)
{
	platform_driver_unregister(&stm_driver);
}
module_exit(stm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight System Trace Macrocell driver");
