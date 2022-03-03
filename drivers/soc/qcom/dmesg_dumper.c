// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.*/

#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_dbl.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/kmsg_dump.h>

struct qcom_dmesg_dumper {
	struct device *dev;
	struct kmsg_dumper dump;
	struct kmsg_dump_iter iter;
	struct resource res;
	void *base;
	size_t size;
	u32 label, peer_name, memparcel;
	bool primary_vm;
	struct notifier_block rm_nb;
};

static void qcom_ddump_to_shm(struct kmsg_dumper *dumper,
			  enum kmsg_dump_reason reason)
{
	struct qcom_dmesg_dumper *qdd = container_of(dumper,
					struct qcom_dmesg_dumper, dump);
	size_t len;

	dev_warn(qdd->dev, "reason = %d\n", reason);
	kmsg_dump_rewind(&qdd->iter);
	memset(qdd->base, 0, qdd->size);
	kmsg_dump_get_buffer(&qdd->iter, true, qdd->base, qdd->size, &len);
	dev_warn(qdd->dev, "size of dmesg logbuf logged = %lld\n", len);
}

static struct device_node *qcom_ddump_svm_of_parse(struct qcom_dmesg_dumper *qdd)
{
	const char *compat = "qcom,ddump-gunyah-gen";
	struct device_node *np = NULL;
	struct device_node *shm_np;
	u32 label;
	int ret;

	while ((np = of_find_compatible_node(np, NULL, compat))) {
		ret = of_property_read_u32(np, "qcom,label", &label);
		if (ret) {
			of_node_put(np);
			continue;
		}
		if (label == qdd->label)
			break;

		of_node_put(np);
	}
	if (!np)
		return NULL;

	shm_np = of_parse_phandle(np, "memory-region", 0);
	of_node_put(np);

	return shm_np;
}

static int qcom_ddump_map_memory(struct qcom_dmesg_dumper *qdd)
{
	struct device *dev = qdd->dev;
	struct device_node *np;
	int ret;

	np = of_parse_phandle(dev->of_node, "shared-buffer", 0);
	if (!np) {
		/*
		 * "shared-buffer" is only specified for primary VM.
		 * Parse "memory-region" for the hypervisor-generated node for
		 * secondary VM.
		 */
		np = qcom_ddump_svm_of_parse(qdd);
		if (!np) {
			dev_err(dev, "Unable to parse shared mem node\n");
			return -EINVAL;
		}
	}

	ret = of_address_to_resource(np, 0, &qdd->res);
	of_node_put(np);
	if (ret) {
		dev_err(dev, "of_address_to_resource failed!\n");
		return -EINVAL;
	}
	qdd->size = resource_size(&qdd->res);

	return 0;
}

static int qcom_ddump_share_mem(struct qcom_dmesg_dumper *qdd, gh_vmid_t self,
			   gh_vmid_t peer)
{
	u32 src_vmlist[1] = {self};
	int src_perms[2] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret;

	ret = hyp_assign_phys(qdd->res.start, resource_size(&qdd->res),
			      src_vmlist, 1,
			      dst_vmlist, dst_perms, 2);
	if (ret) {
		dev_err(qdd->dev, "hyp_assign_phys addr=%x size=%u failed: %d\n",
		       qdd->res.start, qdd->size, ret);
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
	sgl->sgl_entries[0].ipa_base = qdd->res.start;
	sgl->sgl_entries[0].size = resource_size(&qdd->res);

	ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, qdd->label,
			      acl, sgl, NULL, &qdd->memparcel);
	if (ret) {
		dev_err(qdd->dev, "Gunyah mem share addr=%x size=%u failed: %d\n",
		       qdd->res.start, qdd->size, ret);
		/* Attempt to give resource back to HLOS */
		hyp_assign_phys(qdd->res.start, resource_size(&qdd->res),
				dst_vmlist, 2,
				src_vmlist, src_perms, 1);
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

static void qcom_ddump_unshare_mem(struct qcom_dmesg_dumper *qdd, gh_vmid_t self,
			      gh_vmid_t peer)
{
	int dst_perms[2] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int src_vmlist[2] = {self, peer};
	u32 dst_vmlist[1] = {self};
	int ret;

	ret = gh_rm_mem_reclaim(qdd->memparcel, 0);
	if (ret)
		dev_err(qdd->dev, "Gunyah mem reclaim failed: %d\n", ret);

	hyp_assign_phys(qdd->res.start, resource_size(&qdd->res),
			src_vmlist, 2, dst_vmlist, dst_perms, 1);
}

static int qcom_ddump_rm_cb(struct notifier_block *nb, unsigned long cmd,
			     void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload;
	struct qcom_dmesg_dumper *qdd;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;

	qdd = container_of(nb, struct qcom_dmesg_dumper, rm_nb);

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	vm_status_payload = data;
	if (vm_status_payload->vm_status != GH_RM_VM_STATUS_READY &&
	    vm_status_payload->vm_status != GH_RM_VM_STATUS_RESET)
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(qdd->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (vm_status_payload->vm_status == GH_RM_VM_STATUS_READY) {
		if (qcom_ddump_share_mem(qdd, self_vmid, peer_vmid)) {
			dev_err(qdd->dev, "Failed to share memory\n");
			return NOTIFY_DONE;
		}
	}

	if (vm_status_payload->vm_status == GH_RM_VM_STATUS_RESET)
		qcom_ddump_unshare_mem(qdd, self_vmid, peer_vmid);

	return NOTIFY_DONE;
}

static int qcom_ddump_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct qcom_dmesg_dumper *qdd;
	struct device *dev;
	int ret;
	struct resource *res;

	qdd = devm_kzalloc(&pdev->dev, sizeof(*qdd), GFP_KERNEL);
	if (!qdd)
		return -ENOMEM;

	qdd->dev = &pdev->dev;
	platform_set_drvdata(pdev, qdd);

	dev = qdd->dev;
	ret = of_property_read_u32(node, "gunyah-label", &qdd->label);
	if (ret) {
		dev_err(dev, "Failed to read label %d\n", ret);
		return ret;
	}

	qdd->primary_vm = of_property_read_bool(node, "qcom,primary-vm");

	ret = qcom_ddump_map_memory(qdd);
	if (ret)
		return ret;

	if (qdd->primary_vm) {
		ret = of_property_read_u32(node, "peer-name", &qdd->peer_name);
		if (ret)
			qdd->peer_name = GH_SELF_VM;

		qdd->rm_nb.notifier_call = qcom_ddump_rm_cb;
		qdd->rm_nb.priority = INT_MAX;
		gh_rm_register_notifier(&qdd->rm_nb);
	} else {
		res = devm_request_mem_region(dev, qdd->res.start, qdd->size, dev_name(dev));
		if (!res) {
			dev_err(dev, "request mem region fail\n");
			return -ENXIO;
		}

		qdd->base = devm_ioremap_wc(dev, qdd->res.start, qdd->size);
		if (!qdd->base) {
			dev_err(dev, "ioremap fail\n");
			return -ENOMEM;
		}

		kmsg_dump_rewind(&qdd->iter);
		qdd->dump.dump = qcom_ddump_to_shm;
		ret = kmsg_dump_register(&qdd->dump);
		if (ret)
			return ret;
	}

	return 0;
}

static int qcom_ddump_remove(struct platform_device *pdev)
{
	int ret;
	struct qcom_dmesg_dumper *qdd = platform_get_drvdata(pdev);

	ret = kmsg_dump_unregister(&qdd->dump);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id ddump_match_table[] = {
	{ .compatible = "qcom,dmesg-dump" },
	{}
};

static struct platform_driver ddump_driver = {
	.driver = {
		.name = "qcom_dmesg_dumper",
		.of_match_table = ddump_match_table,
	 },
	.probe = qcom_ddump_probe,
	.remove = qcom_ddump_remove,
};

static int __init qcom_ddump_init(void)
{
	return platform_driver_register(&ddump_driver);
}

#ifdef CONFIG_ARCH_QTI_VM
arch_initcall(qcom_ddump_init);
#else
module_init(qcom_ddump_init);
#endif

static __exit void qcom_ddump_exit(void)
{
	platform_driver_unregister(&ddump_driver);
}
module_exit(qcom_ddump_exit);

MODULE_DESCRIPTION("QTI Virtual Machine dmesg log buffer dumper");
MODULE_LICENSE("GPL v2");
