// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"gh_panic_notifier: " fmt

#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/gunyah/gh_dbl.h>
#include <linux/gunyah/gh_panic_notifier.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>
#include <soc/qcom/secure_buffer.h>

#define GH_RECOVERY_VM_MAX 2
#define GH_PANIC_DBL_MASK				0x1

struct gh_panic_notifier_dev {
	struct device *dev;
	struct resource res;
	void *base;
	u64 size;
	u32 label, peer_name, memparcel;
	bool primary_vm;
	void *tx_dbl;
	void *rx_dbl;
	struct wakeup_source *ws;
	struct notifier_block vm_nb;
	struct notifier_block gh_panic_blk;
};

const static struct {
	enum gh_vm_names val;
	const char *str;
} recovery_name_to_vm_name[] = {
	{GH_TRUSTED_VM, "trustedvm_recovery"},
	{GH_OEM_VM, "oemvm_recovery"},
};

struct recovery_vm {
	bool recovery;
	struct device_attribute recovery_attr;
};

static struct recovery_vm *recovery_vms;
SRCU_NOTIFIER_HEAD_STATIC(gh_panic_notifier);

static inline struct recovery_vm *get_recovery_vm_from_vmid(gh_vmid_t vmid)
{
	struct recovery_vm *vm = NULL;
	enum gh_vm_names vm_name;
	int i, ret;

	ret = gh_rm_get_vm_name(vmid, &vm_name);
	if (ret) {
		pr_err("Failed to get VM name for VMID%d ret=%d\n", vmid, ret);
		return vm;
	}

	for (i = 0; i < ARRAY_SIZE(recovery_name_to_vm_name); ++i) {
		if (recovery_name_to_vm_name[i].val == vm_name) {
			vm = &recovery_vms[i];
			break;
		}
	}

	return vm;
}

static inline struct recovery_vm *get_recovery_vm_from_name(const char *str)
{
	struct recovery_vm *vm = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(recovery_name_to_vm_name); ++i) {
		if (!strcmp(str, recovery_name_to_vm_name[i].str)) {
			vm = &recovery_vms[i];
			break;
		}
	}

	return vm;
}

int gh_panic_notifier_register(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&gh_panic_notifier, nb);
}
EXPORT_SYMBOL_GPL(gh_panic_notifier_register);

int gh_panic_notifier_unregister(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&gh_panic_notifier, nb);
}
EXPORT_SYMBOL_GPL(gh_panic_notifier_unregister);

static inline int gh_panic_notifier_kick(void *tx_dbl)
{
	gh_dbl_flags_t dbl_mask = GH_PANIC_DBL_MASK;
	int ret;

	ret = gh_dbl_send(tx_dbl, &dbl_mask, GH_DBL_NONBLOCK);
	if (ret)
		printk_deferred("failed to raise virq to the sender %d\n", ret);

	return ret;
}

static void gh_panic_notify_receiver(int irq, void *data)
{
	struct gh_panic_notifier_dev *gpnd = data;
	gh_dbl_flags_t dbl_mask = GH_PANIC_DBL_MASK;
	bool *handle_done;

	handle_done = gpnd->base;
	gh_dbl_read_and_clean(gpnd->rx_dbl, &dbl_mask, GH_DBL_NONBLOCK);

	/* avoid system enter suspend */
	__pm_stay_awake(gpnd->ws);
	srcu_notifier_call_chain(&gh_panic_notifier, 0, NULL);
	*handle_done = true;
}

static int gh_panic_notifier_share_mem(struct gh_panic_notifier_dev *gpnd,
			gh_vmid_t self, gh_vmid_t peer)
{
	struct qcom_scm_vmperm dst_vmlist[] = {{self, PERM_READ | PERM_WRITE},
						{peer, PERM_READ | PERM_WRITE}};
	struct qcom_scm_vmperm src_vmlist[] = {{self,
					   PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 dst_vmid = BIT(dst_vmlist[0].vmid) | BIT(dst_vmlist[1].vmid);
	u64 src_vmid = BIT(src_vmlist[0].vmid);
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret, assign_mem_ret;

	ret = qcom_scm_assign_mem(gpnd->res.start, gpnd->size, &src_vmid,
				dst_vmlist, ARRAY_SIZE(dst_vmlist));
	if (ret) {
		dev_err(gpnd->dev, "qcom_scm_assign_mem addr=%llx size=%llu failed: %d\n",
		       gpnd->res.start, gpnd->size, ret);
		return ret;
	}

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;
	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}
	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = gpnd->res.start;
	sgl->sgl_entries[0].size = resource_size(&gpnd->res);

	ret = ghd_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, gpnd->label,
			      acl, sgl, NULL, &gpnd->memparcel);
	if (ret) {
		dev_err(gpnd->dev, "Gunyah mem share addr=%llx size=%llu failed: %d\n",
		       gpnd->res.start, gpnd->size, ret);
		/* Attempt to give resource back to HLOS */
		assign_mem_ret = qcom_scm_assign_mem(gpnd->res.start, gpnd->size, &dst_vmid,
				src_vmlist, ARRAY_SIZE(src_vmlist));
		if (assign_mem_ret) {
			dev_err(gpnd->dev, "qcom_scm_assign_mem addr=%llx size=%llu failed: %d\n",
				gpnd->res.start, gpnd->size, ret);
		}
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

static void gh_panic_notifier_unshare_mem(struct gh_panic_notifier_dev *gpnd,
			gh_vmid_t self, gh_vmid_t peer)
{
	struct qcom_scm_vmperm dst_vmlist[] = {{self,
					       PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 src_vmid = BIT(self) | BIT(peer);
	int ret;

	ret = ghd_rm_mem_reclaim(gpnd->memparcel, 0);
	if (ret)
		dev_err(gpnd->dev, "Gunyah mem reclaim failed: %d\n", ret);

	ret = qcom_scm_assign_mem(gpnd->res.start, resource_size(&gpnd->res),
			&src_vmid, dst_vmlist, ARRAY_SIZE(dst_vmlist));
	if (ret) {
		dev_err(gpnd->dev, "unshare mem assign call failed with %d\n",
			ret);
	} else {
		dma_free_coherent(gpnd->dev, gpnd->size, gpnd->base,
				phys_to_dma(gpnd->dev, gpnd->res.start));
		gpnd->base = NULL;
	}
}

static int set_irqchip_state(struct irq_desc *desc, unsigned int irq,
			  enum irqchip_irq_state which, bool val)
{
	struct irq_data *data;
	struct irq_chip *chip;
	int ret = -EINVAL;

	if (!desc)
		return ret;

	data = irq_desc_get_irq_data(desc);
	do {
		chip = irq_data_get_irq_chip(data);
		if (WARN_ON_ONCE(!chip))
			return -ENODEV;

		if (chip->irq_set_irqchip_state)
			break;

#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
		data = data->parent_data;
#else
		data = NULL;
#endif
	} while (data);

	if (data)
		ret = chip->irq_set_irqchip_state(data, which, val);

	return ret;
}

static int get_irqchip_state(struct irq_desc *desc,
			  enum irqchip_irq_state which, bool *state)
{
	struct irq_data *data;
	struct irq_chip *chip;
	int ret = -EINVAL;

	if (!desc)
		return ret;

	data = irq_desc_get_irq_data(desc);
	do {
		chip = irq_data_get_irq_chip(data);
		if (WARN_ON_ONCE(!chip))
			return -ENODEV;

		if (chip->irq_get_irqchip_state)
			break;

#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
		data = data->parent_data;
#else
		data = NULL;
#endif
	} while (data);

	if (data)
		ret = chip->irq_get_irqchip_state(data, which, state);

	return ret;
}

static void clear_pending_irq(void)
{
	bool state;
	unsigned int i;
	struct irq_desc *desc;

	for_each_irq_desc(i, desc) {
		struct irq_chip *chip;
		int ret;

		chip = irq_desc_get_chip(desc);
		if (!chip)
			continue;

		ret = get_irqchip_state(desc, IRQCHIP_STATE_PENDING, &state);
		if (!ret && state) {
			/* Clear interrupt pending status */
			ret = set_irqchip_state(desc, i, IRQCHIP_STATE_PENDING, false);
			if (ret && irqd_irq_inprogress(&desc->irq_data) &&
				chip->irq_eoi)
				chip->irq_eoi(&desc->irq_data);

			if (chip->irq_mask)
				chip->irq_mask(&desc->irq_data);

			if (chip->irq_disable && !irqd_irq_disabled(&desc->irq_data))
				chip->irq_disable(&desc->irq_data);
		}
	}
}

static int gh_panic_notifier_notify(struct notifier_block *nb,
			  unsigned long event, void *ptr)
{
	struct gh_panic_notifier_dev *gpnd;
	unsigned int retry_times = 20;
	gh_vmid_t peer_vmid;
	bool *handle_done;
	int ret;

	gpnd = container_of(nb, struct gh_panic_notifier_dev, gh_panic_blk);
	handle_done = gpnd->base;
	if (!handle_done)
		return NOTIFY_DONE;

	ret = ghd_rm_get_vmid(gpnd->peer_name, &peer_vmid);
	if (ret)
		return NOTIFY_DONE;

	ret = gh_panic_notifier_kick(gpnd->tx_dbl);
	if (ret)
		return NOTIFY_DONE;
	/*
	 * When PVM panic, only one cpu can work and disable local irq in PVM.
	 * if there are interrupts pending, they never can be responsed. And call
	 * gh_hcall_vcpu_run will return at once and the vcpu can't be scheduled.
	 * So we should clear the pending interrupts and mask the interrupts before
	 * call gh_hcall_vcpu_run.
	 */
	do {
		clear_pending_irq();
		ret = gh_poll_vcpu_run(peer_vmid);
		if (ret) {
			printk_deferred("Failed poll vcpu run %d\n", ret);
			break;
		}
		retry_times--;
	} while (!(*handle_done) && retry_times > 0);

	if (!(*handle_done))
		printk_deferred("Notify the panic to VM fail\n");

	return NOTIFY_DONE;
}

static int gh_panic_notifier_vm_cb(struct notifier_block *nb, unsigned long cmd,
			     void *data)
{
	struct gh_panic_notifier_dev *gpnd;
	struct recovery_vm *vm;
	dma_addr_t dma_handle;
	gh_vmid_t *notify_vmid;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;
	bool *handle_done;

	gpnd = container_of(nb, struct gh_panic_notifier_dev, vm_nb);
	notify_vmid = data;
	if (ghd_rm_get_vmid(gpnd->peer_name, &peer_vmid))
		return NOTIFY_DONE;

	if (ghd_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;

	switch (cmd) {
	case GH_VM_BEFORE_POWERUP:
		if (peer_vmid == *notify_vmid) {
			gpnd->base = dma_alloc_coherent(gpnd->dev, gpnd->size,
							&dma_handle, GFP_KERNEL);
			if (!gpnd->base)
				return NOTIFY_DONE;

			gpnd->res.start = dma_to_phys(gpnd->dev, dma_handle);
			gpnd->res.end = gpnd->res.start + gpnd->size - 1;
			handle_done = gpnd->base;
			*handle_done = false;
			if (gh_panic_notifier_share_mem(gpnd, self_vmid, peer_vmid)) {
				dev_err(gpnd->dev, "Failed to share memory\n");
				return NOTIFY_DONE;
			}

			gpnd->gh_panic_blk.notifier_call = gh_panic_notifier_notify;
			gpnd->gh_panic_blk.priority = 0;
			atomic_notifier_chain_register(&panic_notifier_list, &gpnd->gh_panic_blk);
		}
		break;
	case GH_VM_EARLY_POWEROFF:
		if (peer_vmid == *notify_vmid) {
			atomic_notifier_chain_unregister(&panic_notifier_list, &gpnd->gh_panic_blk);
			gh_panic_notifier_unshare_mem(gpnd, self_vmid, peer_vmid);
		}
		break;
	case GH_VM_CRASH:
		dev_err(gpnd->dev, "VM: %d Crashed!\n", *notify_vmid);
		vm = get_recovery_vm_from_vmid(*notify_vmid);
		if (!vm) {
			dev_err(gpnd->dev, "Failed to get recovery vm for VM:%d!\n", *notify_vmid);
			return NOTIFY_DONE;
		}

		if (!vm->recovery) {
			if (peer_vmid == *notify_vmid)
				atomic_notifier_chain_unregister(&panic_notifier_list,
									&gpnd->gh_panic_blk);

			panic("Resetting the SoC");
		}
		break;
	}

	return NOTIFY_DONE;
}

static int gh_panic_notifier_svm_mem_map(struct gh_panic_notifier_dev *gpnd)
{
	const char *compat = "qcom,gunyah-panic-gen";
	struct device_node *np = NULL;
	struct device_node *shm_np;
	struct resource *res;
	u32 label;
	int ret;

	while ((np = of_find_compatible_node(np, NULL, compat))) {
		ret = of_property_read_u32(np, "qcom,label", &label);
		if (ret) {
			of_node_put(np);
			continue;
		}
		if (label == gpnd->label)
			break;

		of_node_put(np);
	}
	if (!np) {
		dev_err(gpnd->dev, "can't find the label=%d memory!\n", gpnd->label);
		return -ENODEV;
	}

	shm_np = of_parse_phandle(np, "memory-region", 0);
	of_node_put(np);
	ret = of_address_to_resource(shm_np, 0, &gpnd->res);
	of_node_put(shm_np);
	if (ret) {
		dev_err(gpnd->dev, "of_address_to_resource failed!\n");
		return -EINVAL;
	}

	gpnd->size = resource_size(&gpnd->res);
	res = devm_request_mem_region(gpnd->dev, gpnd->res.start, gpnd->size,
				dev_name(gpnd->dev));
	if (!res) {
		dev_err(gpnd->dev, "request mem region fail\n");
		return -ENXIO;
	}

	gpnd->base = devm_ioremap_wc(gpnd->dev, gpnd->res.start, gpnd->size);
	if (!gpnd->base) {
		dev_err(gpnd->dev, "ioremap fail\n");
		return -ENOMEM;
	}

	return 0;
}

static int gh_panic_notifier_pvm_mem_probe(struct gh_panic_notifier_dev *gpnd)
{
	struct device *dev = gpnd->dev;
	u32 size;
	int ret;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(dev, "%s: dma_set_mask_and_coherent failed\n", __func__);
		return ret;
	}

	ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);
	if (ret) {
		dev_err(dev, "%s: Failed to initialize CMA mem, ret %d\n", __func__, ret);
		return ret;
	}

	ret = of_property_read_u32(gpnd->dev->of_node, "shared-buffer-size", &size);
	if (ret) {
		dev_err(dev, "%s: Failed to get shared memory size, ret %d\n",
				__func__, ret);
		return ret;
	}

	gpnd->size = size;
	return 0;
}

static ssize_t recovery_show(struct device *dev,
			     struct device_attribute *attribute, char *buf)
{
	struct recovery_vm *vm;

	vm = get_recovery_vm_from_name(attribute->attr.name);
	if (!vm)
		return -EINVAL;

	return sysfs_emit(buf, "%s\n",
			vm->recovery ? "enabled" : "disabled");
}

static ssize_t recovery_store(struct device *dev,
			      struct device_attribute *attribute,
			      const char *buf, size_t count)
{
	struct recovery_vm *vm;

	vm = get_recovery_vm_from_name(attribute->attr.name);
	if (!vm)
		return -EINVAL;

	if (sysfs_streq(buf, "enabled"))
		vm->recovery = true;
	else if (sysfs_streq(buf, "disabled"))
		vm->recovery = false;
	else
		return -EINVAL;

	return count;
}

static int init_recovery_vms(struct kobject *kobj)
{
	struct recovery_vm *vm;
	int i, ret;

	for (i = 0; i < GH_RECOVERY_VM_MAX; i++) {
		vm = &recovery_vms[i];
		sysfs_attr_init(&vm->recovery_attr.attr);
		vm->recovery = false;
		vm->recovery_attr.show = recovery_show;
		vm->recovery_attr.store = recovery_store;
		vm->recovery_attr.attr.name = recovery_name_to_vm_name[i].str;
		vm->recovery_attr.attr.mode = 0600;
		sysfs_attr_init(&vm->recovery_attr.attr);
		ret = sysfs_create_file(kobj, &vm->recovery_attr.attr);
		if (ret) {
			pr_err("Create recovery sysfs entry for vm:%d  failed: %d\n",
				recovery_name_to_vm_name[i].val, ret);
			return ret;
		}
	}

	return 0;
}

static int gh_panic_notifier_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct gh_panic_notifier_dev *gpnd;
	enum gh_dbl_label dbl_label;
	struct device *dev;
	int ret;

	gpnd = devm_kzalloc(&pdev->dev, sizeof(*gpnd), GFP_KERNEL);
	if (!gpnd)
		return -ENOMEM;

	gpnd->dev = &pdev->dev;
	platform_set_drvdata(pdev, gpnd);

	dev = gpnd->dev;
	ret = of_property_read_u32(node, "gunyah-label", &gpnd->label);
	if (ret) {
		dev_err(dev, "Failed to read label %d\n", ret);
		return ret;
	}

	dbl_label = gpnd->label;

	gpnd->primary_vm = of_property_read_bool(node, "qcom,primary-vm");
	if (gpnd->primary_vm) {
		ret = gh_panic_notifier_pvm_mem_probe(gpnd);
		if (ret)
			return ret;

		ret = of_property_read_u32(node, "peer-name", &gpnd->peer_name);
		if (ret)
			gpnd->peer_name = GH_SELF_VM;

		gpnd->tx_dbl = gh_dbl_tx_register(dbl_label);
		if (IS_ERR_OR_NULL(gpnd->tx_dbl)) {
			ret = PTR_ERR(gpnd->tx_dbl);
			dev_err(dev, "%s:Failed to get gunyah tx dbl %d\n", __func__, ret);
			return ret;
		}

		recovery_vms = kcalloc(GH_RECOVERY_VM_MAX, sizeof(struct recovery_vm), GFP_KERNEL);
		if (!recovery_vms) {
			ret = -ENOMEM;
			gh_dbl_tx_unregister(gpnd->tx_dbl);
			return ret;
		}

		ret = init_recovery_vms(&pdev->dev.kobj);
		if (ret) {
			gh_dbl_tx_unregister(gpnd->tx_dbl);
			kfree(recovery_vms);
			return ret;
		}

		gpnd->vm_nb.notifier_call = gh_panic_notifier_vm_cb;
		gpnd->vm_nb.priority = INT_MAX;
		gh_register_vm_notifier(&gpnd->vm_nb);
	} else {
		ret = gh_panic_notifier_svm_mem_map(gpnd);
		if (ret)
			return ret;

		gpnd->rx_dbl = gh_dbl_rx_register(dbl_label, gh_panic_notify_receiver,
							gpnd);
		if (IS_ERR_OR_NULL(gpnd->rx_dbl)) {
			ret = PTR_ERR(gpnd->rx_dbl);
			dev_err(dev, "%s:Failed to get gunyah rx dbl %d\n", __func__, ret);
			return ret;
		}

		gpnd->ws = wakeup_source_register(dev, dev_name(dev));
		if (!gpnd->ws) {
			dev_err(dev, "%s:Failed to register wakeup source\n", __func__);
			gh_dbl_rx_unregister(gpnd->rx_dbl);
			return -ENOMEM;
		}
	}

	return 0;
}

static int gh_panic_notifier_remove(struct platform_device *pdev)
{
	struct gh_panic_notifier_dev *gpnd;

	gpnd = platform_get_drvdata(pdev);
	if (gpnd->primary_vm) {
		gh_dbl_tx_unregister(gpnd->tx_dbl);
		gh_unregister_vm_notifier(&gpnd->vm_nb);
		kfree(recovery_vms);
	} else {
		gh_dbl_rx_unregister(gpnd->rx_dbl);
		wakeup_source_unregister(gpnd->ws);
	}

	return 0;
}

static const struct of_device_id gh_panic_notifier_match_table[] = {
	{ .compatible = "qcom,gh-panic-notifier" },
	{}
};

static struct platform_driver gh_panic_notifier_driver = {
	.driver = {
		.name = "gh_panic_notifier",
		.of_match_table = gh_panic_notifier_match_table,
	 },
	.probe = gh_panic_notifier_probe,
	.remove = gh_panic_notifier_remove,
};

static int __init gh_panic_notifier_init(void)
{
	return platform_driver_register(&gh_panic_notifier_driver);
}

#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
arch_initcall(gh_panic_notifier_init);
#else
module_init(gh_panic_notifier_init);
#endif

static __exit void gh_panic_notifier_exit(void)
{
	platform_driver_unregister(&gh_panic_notifier_driver);
}
module_exit(gh_panic_notifier_exit);

MODULE_DESCRIPTION(" Qualcomm Technologies, Inc. Gunyah Panic Notifier Driver");
MODULE_LICENSE("GPL");
