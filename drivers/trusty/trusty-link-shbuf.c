/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

#include "trusty-link-shbuf.h"

#define MAX_LINK_SHBUF_DEV 4
#define ENABLE_SHBUF_RAMCONSOLE (1)

#if ENABLE_SHBUF_RAMCONSOLE
#define RAMCONSOLE_SIZE   (64ul * 1024ul) // 64KB
#define RAMCONSOLE_OFFSET (sizeof(uint64_t)) // 64B
#endif

static struct link_shbuf_data *link_shbuf_dev[MAX_LINK_SHBUF_DEV];
static u32 link_shbuf_dev_num;
struct link_shbuf_data *gpriv;

u32 trusty_virtio_devid = 0, trusty_virtio_vqid = 0;

/* Sentinel values for indicating missing communication channels */
#define NO_OUTGOING_IRQ  (0)
#define NO_INCOMING_IRQ  (-1)

static bool link_shbuf_data_invariant(const struct link_shbuf_data *priv)
{
	if (!priv)
		return false;

	if (!priv->base || (uintptr_t)priv->base % PAGE_SIZE != 0)
		return false;

	if (resource_size(&priv->buffer) == 0)
		return false;

	return true;
}

struct link_shbuf_data *trusty_get_link_shbuf_device(u32 lsdidx)
{
	if (lsdidx >= MAX_LINK_SHBUF_DEV)
		return NULL;

	return link_shbuf_dev[lsdidx];
}

s32 trusty_link_shbuf_recv(u32 lsdidx, ulong *out)
{
	ulong payload;
	int ret;
	struct link_shbuf_data *dev = trusty_get_link_shbuf_device(lsdidx);

	if (!dev) {
		dev_err(dev->dev, "lsdidx %u invalid (should be < %u)\n",
				lsdidx, link_shbuf_dev_num);
		return -EINVAL;
	}

	do {
		ret = wait_event_interruptible_timeout(dev->virq_wq,
				dev->virq_pending, msecs_to_jiffies(30000));
		if (ret < 0) {
			dev_err(dev->dev, "wait failed ret %d virq_pending %u\n",
					ret, dev->virq_pending);
			return ret;
		} else if (ret == 0) {
			dev_err(dev->dev, "wait timeout ret %d virq_pending %u\n",
					ret, dev->virq_pending);
			return -ETIMEDOUT;
		}
	} while (!xchg(&dev->virq_pending, 0));

	/* Ensure all PEs see the latest payload. */
	smp_rmb();
	payload = atomic64_xchg(&dev->virq_payload, 0);

	if (out)
		*out = payload;

	dev_dbg(dev->dev, "%s: virq %d hwirq %d payload 0x%lx\n",
			__func__, dev->virq, dev->hwirq, payload);

	return 0;
}

s32 trusty_link_shbuf_send(u32 lsdidx, ulong payload)
{
	okl4_error_t err;
	struct link_shbuf_data *dev = trusty_get_link_shbuf_device(lsdidx);

	if (!dev) {
		dev_err(dev->dev, "lsdidx %u invalid (should be < %u)\n",
				lsdidx, link_shbuf_dev_num);
		return -EINVAL;
	}

	err = _okl4_sys_vinterrupt_raise(dev->virqline, payload);
	if (err != OKL4_OK) {
		dev_err(dev->dev, "failed to raise virq %u err %u payload 0x%lx\n",
				dev->virqline, err, payload);
		return  -EIO;
	}

	dev_dbg(dev->dev, "%s: virqline %d payload 0x%lx\n",
			__func__, dev->virqline, payload);

	return 0;
}

static int shbuf_virq_show(struct seq_file *m, void *v)
{
	ulong out;

	trusty_link_shbuf_recv(0,  &out);
	seq_printf(m, "%lx\n", out);
	return 0;
}

ssize_t shbuf_virq_write(struct file *filp, const char __user *ubuf,
		size_t size, loff_t *off)
{
	char kbuf[32];
	ulong payload;

	if (!size)
		return 0;
	if (!access_ok(VERIFY_READ, ubuf, size))
		return -EFAULT;

	if (size > sizeof(kbuf) - 1)
		size = sizeof(kbuf) - 1;

	if (copy_from_user(kbuf, ubuf, size))
		return -EFAULT;

	kbuf[size] = '\0';

	if (kstrtoul(kbuf, 0, &payload) == 0) {
		dev_info(gpriv->dev, "shbuf_send payload 0x%lx\n", payload);
		trusty_link_shbuf_send(0, payload);
	}

	return size;
}

static int shbuf_virq_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, shbuf_virq_show, NULL);
}

static const struct file_operations shbuf_virq_fops = {
	.open = shbuf_virq_open,
	.read = seq_read,
	.write = shbuf_virq_write,
	.release = single_release,
};

static int shbuf_buffer_show(struct seq_file *m, void *v)
{
	struct link_shbuf_data *priv = m->private;
#if ENABLE_SHBUF_RAMCONSOLE
	char *ramconsole_beg = NULL;
	char *ramconsole_cur = NULL;
	char *ramconsole_end = NULL;

	ramconsole_beg = priv->base;
	ramconsole_cur = priv->base;
	ramconsole_end = priv->base + RAMCONSOLE_SIZE;

	for (; ramconsole_cur < ramconsole_end;
			ramconsole_cur += sizeof(uint64_t)) {
		seq_printf(m, "%08lx: %02x %02x %02x %02x %02x %02x %02x %02x\n",
				ramconsole_cur - ramconsole_beg,
				readb_relaxed(ramconsole_cur + 0),
				readb_relaxed(ramconsole_cur + 1),
				readb_relaxed(ramconsole_cur + 2),
				readb_relaxed(ramconsole_cur + 3),
				readb_relaxed(ramconsole_cur + 4),
				readb_relaxed(ramconsole_cur + 5),
				readb_relaxed(ramconsole_cur + 6),
				readb_relaxed(ramconsole_cur + 7));
	}
#endif

	seq_printf(m, "%llx\n", readq_relaxed(priv->base));
	return 0;
}

ssize_t shbuf_buffer_write(struct file *filp, const char __user *ubuf,
		size_t size, loff_t *off)
{
	ulong offset;
	void *ptr;
	char kbuf[32];
	struct link_shbuf_data *priv =
		((struct seq_file *)filp->private_data)->private;
	const uint64_t pattern = 0x11223344deadbeefULL;

	if (!size)
		return 0;
	if (!access_ok(VERIFY_READ, ubuf, size))
		return -EFAULT;

	if (size > sizeof(kbuf) - 1)
		size = sizeof(kbuf) - 1;

	if (copy_from_user(kbuf, ubuf, size))
		return -EFAULT;

	kbuf[size] = '\0';

	if (kstrtoul(kbuf, 0, &offset) == 0) {
		if (offset > (RAMCONSOLE_SIZE - sizeof(uint64_t)))
			offset = RAMCONSOLE_SIZE - sizeof(uint64_t);
		ptr = (void *)((uintptr_t)priv->base + offset);
		writeq_relaxed(pattern, ptr);
		dev_info(gpriv->dev, "addr %p w:%llx r:%llx\n",
				ptr, pattern, readq_relaxed(ptr));
	}

	return size;
}

static int shbuf_buffer_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, shbuf_buffer_show, inode->i_private);
}

static const struct file_operations shbuf_buffer_fops = {
	.open = shbuf_buffer_open,
	.read = seq_read,
	.write = shbuf_buffer_write,
	.release = single_release,
};

s32 trusty_link_shbuf_smc(u32 lsdidx, ulong r0, ulong r1, ulong r2, ulong r3)
{
	unsigned long ret = 0;
	struct link_shbuf_data *dev = trusty_get_link_shbuf_device(lsdidx);

	trusty_link_shbuf_send(lsdidx, r0);
	trusty_link_shbuf_recv(lsdidx, NULL);
	trusty_link_shbuf_send(lsdidx, r1);
	trusty_link_shbuf_recv(lsdidx, NULL);
	trusty_link_shbuf_send(lsdidx, r2);
	trusty_link_shbuf_recv(lsdidx, NULL);
	trusty_link_shbuf_send(lsdidx, r3);
	trusty_link_shbuf_recv(lsdidx, &ret);

	dev_dbg(dev->dev, "%s: 0x%lx 0x%lx 0x%lx 0x%lx done (0x%lx)\n",
			__func__, r0, r1, r2, r3, ret);

	return ret;
}

/*
 * Interrupt handler.
 *
 * This function will be called when our link partner uses the ioctl on their
 * shared memory device to send an outgoing interrupt.
 */
static irqreturn_t link_shbuf_irq_handler(int irq, void *data)
{
	u64 payload, old, new;
	struct _okl4_sys_interrupt_get_payload_return _payload;

	/* Retrieve a pointer to our private data. */
	struct link_shbuf_data *priv = data;

	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return IRQ_NONE;

	/*
	 * We should only ever be handling a single interrupt, and only if there
	 * was an incoming interrupt in the configuration.
	 */
	if (WARN_ON(priv->virq < 0 || priv->virq != irq))
		return IRQ_NONE;

	_payload = _okl4_sys_interrupt_get_payload(priv->hwirq);
	payload = (u64)_payload.payload;

	dev_dbg(priv->dev, "irq %d payload %llx\n", irq, payload);

	if (payload == TRUSTY_PROBE_DEFER) {
		trusty_probe_late();
	} else if ((payload >> 32) == SMC_NC_VDEV_KICK_VQ) {
		trusty_virtio_devid = (u32)((payload & 0xFFFF0000) >> 16);
		trusty_virtio_vqid = (u32)(payload & 0x0000FFFF);
		trusty_notifier_call();
	} else {
		/*
		 * At this point, it is possible the pending flag is already
		 * set. It is up to the user to synchronise their transmission
		 * and acknowledgement of interrupts.
		 */

		/* atomic64_or which is not universally available. */
		do {
			old = atomic64_read(&priv->virq_payload);
			new = old | payload;
		} while (atomic64_cmpxchg(
					&priv->virq_payload, old, new) != old);

		priv->virq_pending = true;
		/* Ensure all PEs see the latest payload/pending. */
		smp_wmb();
		wake_up_interruptible(&priv->virq_wq);
	}

	return IRQ_HANDLED;
}

static int link_shbuf_dev_compare(const void *lhs, const void *rhs)
{
	const struct link_shbuf_data *l = lhs;
	const struct link_shbuf_data *r = rhs;

	return l->hwirq - r->hwirq;
}

/*
 * Discover and add a new shared-buffer link.
 *
 * In the following function, we are expecting to parse device tree entries
 * looking like the following:
 *
 *	hypervisor {
 *		...
 *		interrupt-line@1d {
 *				compatible = "okl,microvisor-interrupt-line",
 *				"okl,microvisor-capability";
 *			phandle = <0x7>;
 *			reg = <0x1d>;
 *			label = "foo_virqline";
 *		};
 *	 ;
 *
 *	foo@41003000 {
 *		compatible = "okl,microvisor-link-shbuf",
 *			"okl,microvisor-shared-memory";
 *		phandle = <0xd>;
 *		reg = <0x0 0x41003000 0x2000>;
 *		label = "foo";
 *		okl,rwx = <0x6>;
 *		okl,interrupt-line = <0x7>;
 *		interrupts = <0x0 0x4 0x1>;
 *		interrupt-parent = <0x1>;
 *	};
 */
static int trusty_link_shbuf_probe(struct platform_device *pdev)
{
	int ret, i;
	struct device_node *node;
	struct link_shbuf_data *priv;
	const char *name;
	u32 permissions;
	struct dentry *debugfs_root;

	node = pdev->dev.of_node;

	if (!node)
		return -ENODEV;

	ret = of_property_read_string(node, "label", &name);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read label\n");
		return -ENODEV;
	}

	if (strncmp(name, "mlvm_shm", strlen("mlvm_shm"))) {
		dev_info(&pdev->dev, "label mismatch, expect %s was %s\n",
				"mlvm_shm", name);
		return -ENODEV;
	}

	if (link_shbuf_dev_num >= MAX_LINK_SHBUF_DEV) {
		dev_err(&pdev->dev, "link-shbuf devices > MAX_LINK_SHBUF_DEV\n");
		return -ENOMEM;
	}

	if (!gpriv)
		gpriv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	priv = gpriv;
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	init_waitqueue_head(&priv->virq_wq);
	priv->virq_pending = false;

	/* Retrieve information about the shared memory region. */
	ret = of_address_to_resource(node, 0, &priv->buffer);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get buffer address\n");
		goto err_free_irq;
	}

	/*
	 * We expect the Elfweaver to have validated that we have a non-NULL,
	 * page-aligned region.
	 */
	if (WARN_ON(priv->buffer.start == 0) ||
			WARN_ON(resource_size(&priv->buffer) % PAGE_SIZE != 0))
		goto err_free_irq;

	if (!devm_request_mem_region(&pdev->dev, priv->buffer.start,
				resource_size(&priv->buffer),
				dev_name(&pdev->dev))) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "failed to request_mem_region start %llx size %llx\n",
				priv->buffer.start,
				resource_size(&priv->buffer));
		goto err_free_irq;
	}

	priv->base = devm_memremap(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer), MEMREMAP_WB);
	if (!priv->base) {
		dev_err(&pdev->dev, "failed to memremap, start %llx size %llx\n",
				priv->buffer.start,
				resource_size(&priv->buffer));
		goto err_release_region;
	}

#if ENABLE_SHBUF_RAMCONSOLE
	priv->ramconsole_size = RAMCONSOLE_SIZE;
#endif

	dev_info(&pdev->dev, "shbuf pa %llx size %llx memremap_va 0x%lx rc_sz %zx\n",
			priv->buffer.start, resource_size(&priv->buffer),
			(uintptr_t)priv->base, priv->ramconsole_size);

	/* Read the permissions of the shared memory region. */
	ret = of_property_read_u32(node, "okl,rwx", &permissions);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read shared memory permissions\n");
		goto err_unmap_dev;
	}

	if (permissions & ~(0007)) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "invalid shared memory permissions %u\n",
				permissions);
		goto err_unmap_dev;
	}

	priv->permissions = ((permissions & 0004) ? FMODE_READ : 0) |
		((permissions & 0002) ? FMODE_WRITE : 0) |
		((permissions & 0001) ? FMODE_EXEC : 0);
	if (WARN_ON(priv->permissions == 0)) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "shared memory permissions is 0\n");
		goto err_unmap_dev;
	}

	debugfs_root = debugfs_create_dir(dev_name(&pdev->dev), NULL);
	if (!IS_ERR_OR_NULL(debugfs_root)) {
		debugfs_create_file("shbuf_virq", 0600,
				debugfs_root, NULL, &shbuf_virq_fops);
		debugfs_create_file("shbuf_buffer", 0600,
				debugfs_root, priv, &shbuf_buffer_fops);
	} else {
		dev_err(&pdev->dev, "debugfs_create_dir '%s' failed ret %ld\n",
				dev_name(&pdev->dev), PTR_ERR(debugfs_root));
	}

	dev_set_drvdata(&pdev->dev, priv);

	link_shbuf_dev[link_shbuf_dev_num++] = priv;
	sort(link_shbuf_dev, link_shbuf_dev_num, sizeof(u64),
			&link_shbuf_dev_compare, NULL);

	dev_info(&pdev->dev, "current %d link-shbuf devices registered.\n",
			link_shbuf_dev_num);

	for (i = 0; i < link_shbuf_dev_num; i++) {
		dev_info(&pdev->dev, "  link_shbuf_dev[%d] virq %d hwirq %d virqline %d base 0x%lx remap_pa 0x%llx\n",
				i, link_shbuf_dev[i]->virq,
				link_shbuf_dev[i]->hwirq,
				link_shbuf_dev[i]->virqline,
				(uintptr_t)link_shbuf_dev[i]->base,
				page_to_phys(virt_to_page(link_shbuf_dev[i]->base)));
	}

	return 0;

err_unmap_dev:
	dev_info(&pdev->dev, "%s: devm_memunmap %lx\n",
			__func__, (uintptr_t)priv->base);
	devm_memunmap(&pdev->dev, priv->base);
err_release_region:
	devm_release_mem_region(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer));
err_free_irq:
	if (priv->virq != NO_INCOMING_IRQ)
		devm_free_irq(&pdev->dev, priv->virq, priv);
	devm_kfree(&pdev->dev, priv);
	return ret;
}

static int trusty_link_shbuf_remove(struct platform_device *pdev)
{
	struct link_shbuf_data *priv;

	priv = dev_get_drvdata(&pdev->dev);
	WARN_ON(!link_shbuf_data_invariant(priv));

	/*
	 * None of the following is strictly required, as these are all managed
	 * resources, but we clean it up anyway for clarity.
	 */

	dev_info(&pdev->dev, "%s: devm_memunmap %lx\n",
			__func__, (uintptr_t)priv->base);

	devm_memunmap(&pdev->dev, priv->base);

	devm_release_mem_region(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer));

	if (priv->virq != NO_INCOMING_IRQ)
		devm_free_irq(&pdev->dev, priv->virq, priv);

	devm_kfree(&pdev->dev, priv);

	return 0;
}

static const struct of_device_id trusty_link_shbuf_match[] = {
	{ .compatible = "okl,microvisor-shared-memory", },
	{},
};

static struct platform_driver trusty_link_shbuf_driver = {
	.driver = {
		.name = "trusty-link-shbuf",
		.owner = THIS_MODULE,
		.of_match_table = trusty_link_shbuf_match,
	},
	.probe = trusty_link_shbuf_probe,
	.remove = trusty_link_shbuf_remove,
};

/* Maximum number of minor device numbers */
enum {
	MAX_MINOR = 1 << MINORBITS,
};

static int __init trusty_link_shbuf_init(void)
{
	return platform_driver_register(&trusty_link_shbuf_driver);
}

static void __exit trusty_link_shbuf_exit(void)
{
	platform_driver_unregister(&trusty_link_shbuf_driver);
}

arch_initcall(trusty_link_shbuf_init);
module_exit(trusty_link_shbuf_exit);

static int fifo_vipc_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct link_shbuf_data *priv;
	const char *name;
	struct irq_desc *desc;

	node = pdev->dev.of_node;

	if (!node)
		return -ENODEV;

	ret = of_property_read_string(node, "label", &name);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read label\n");
		return -ENODEV;
	}

	priv = gpriv;
	if (!priv)
		return -ENOMEM;

	ret = of_property_read_u32(node, "reg", &priv->virqline);
	if (ret < 0 || priv->virqline == OKL4_KCAP_INVALID) {
		dev_err(&pdev->dev, "failed to get \"reg\" of interrupt-line\n");
		return -ENODEV;
	}

	/* Retrieve the incoming vIRQ number. Again, this is configurable and we
	 * anticipate that it may not exist.
	 */
	priv->virq = platform_get_irq(pdev, 0);
	if (priv->virq < 0) {
		dev_err(&pdev->dev, "failed to get platform irq\n");
		return -EINVAL;
	}

	desc = irq_to_desc(priv->virq);
	if (!desc) {
		dev_err(&pdev->dev, "failed to get irq_desc\n");
		return -EINVAL;
	}
	priv->hwirq = desc->irq_data.hwirq;

	/* If we have a valid incoming vIRQ, register to handle it. */
	ret = devm_request_irq(&pdev->dev, priv->virq,
			link_shbuf_irq_handler,
			0, dev_name(&pdev->dev), priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request_irq\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "%s: virq %d hwirq %d virqline %d\n",
			__func__, priv->virq, priv->hwirq, priv->virqline);

#if ENABLE_SHBUF_RAMCONSOLE
	WARN_ON(!priv->base);
	/* Notify HEE we're ready for virq communication */
	writeq_relaxed(TRUSTY_PROBE_DEFER, priv->base);
	dev_info(&pdev->dev, "%s: TRUSTY_PROBE_DEFER %llx\n",
			__func__, readq_relaxed(priv->base));
#endif
	return 0;
}

static const struct of_device_id fifo_vipc_match[] = {
	{ .compatible = "qcom,ipcr-fifo-xprt", },
	{},
};

static struct platform_driver fifo_vipc_driver = {
	.driver = {
		.name = "trusty-ipcr-fifo-xprt",
		.owner = THIS_MODULE,
		.of_match_table = fifo_vipc_match,
	},
	.probe = fifo_vipc_probe,
};

static int __init fifo_vipc_init(void)
{
	return platform_driver_register(&fifo_vipc_driver);
}

module_init(fifo_vipc_init);
