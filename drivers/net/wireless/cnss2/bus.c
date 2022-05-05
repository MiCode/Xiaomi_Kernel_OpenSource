// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved. */

#include "bus.h"
#include "debug.h"
#include "pci.h"

enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev)
{
	if (!dev)
		return CNSS_BUS_NONE;

	if (!dev->bus)
		return CNSS_BUS_NONE;

	if (memcmp(dev->bus->name, "pci", 3) == 0)
		return CNSS_BUS_PCI;
	else
		return CNSS_BUS_NONE;
}

enum cnss_dev_bus_type cnss_get_bus_type(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct device *dev;
	enum cnss_dev_bus_type bus_type = CNSS_BUS_NONE;

	if (plat_priv->is_converged_dt) {
		dev = &plat_priv->plat_dev->dev;
		ret = of_property_read_u32(dev->of_node, "qcom,bus-type",
					   &bus_type);
		if (!ret && bus_type <= CNSS_BUS_USB)
			cnss_pr_dbg("Got bus type[%u] from dt\n", bus_type);
		else
			cnss_pr_err("No bus type for converged dt\n");

		return bus_type;
	}

	/* Get bus type according to device id if it's not converged DT */
	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCN7605_DEVICE_ID:
	case QCA6490_DEVICE_ID:
	case WCN7850_DEVICE_ID:
		bus_type = CNSS_BUS_PCI;
		break;
	default:
		cnss_pr_err("Unknown device: 0x%lx\n", plat_priv->device_id);
		break;
	}

	return bus_type;
}

void *cnss_bus_dev_to_bus_priv(struct device *dev)
{
	if (!dev)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		return cnss_get_pci_priv(to_pci_dev(dev));
	default:
		return NULL;
	}
}

struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev)
{
	void *bus_priv;

	if (!dev)
		return cnss_get_plat_priv(NULL);

	bus_priv = cnss_bus_dev_to_bus_priv(dev);
	if (!bus_priv)
		return NULL;

	switch (cnss_get_dev_bus_type(dev)) {
	case CNSS_BUS_PCI:
		return cnss_pci_priv_to_plat_priv(bus_priv);
	default:
		return NULL;
	}
}

int cnss_bus_init(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_init(plat_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

void cnss_bus_deinit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		cnss_pci_deinit(plat_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

void cnss_bus_add_fw_prefix_name(struct cnss_plat_data *plat_priv,
				 char *prefix_name, char *name)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_add_fw_prefix_name(plat_priv->bus_priv,
						   prefix_name, name);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

int cnss_bus_load_m3(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_load_m3(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_alloc_fw_mem(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_alloc_qdss_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_alloc_qdss_mem(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

void cnss_bus_free_qdss_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		cnss_pci_free_qdss_mem(plat_priv->bus_priv);
		return;
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_get_wake_msi(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_force_fw_assert_hdlr(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_qmi_send_get(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_qmi_send_get(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_qmi_send_put(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_qmi_send_put(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

void cnss_bus_fw_boot_timeout_hdlr(struct timer_list *t)
{
	struct cnss_plat_data *plat_priv =
		from_timer(plat_priv, t, fw_boot_timer);

	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_fw_boot_timeout_hdlr(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv, bool in_panic)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_collect_dump_info(plat_priv->bus_priv,
						  in_panic);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

void cnss_bus_device_crashed(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_device_crashed(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

int cnss_bus_call_driver_probe(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_call_driver_probe(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_call_driver_remove(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_call_driver_remove(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_dev_powerup(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_dev_powerup(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_dev_shutdown(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_dev_shutdown(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_dev_crash_shutdown(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_dev_crash_shutdown(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_dev_ramdump(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_dev_ramdump(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_register_driver_hdlr(struct cnss_plat_data *plat_priv, void *data)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_register_driver_hdlr(plat_priv->bus_priv, data);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_unregister_driver_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_unregister_driver_hdlr(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_call_driver_modem_status(struct cnss_plat_data *plat_priv,
				      int modem_current_status)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_call_driver_modem_status(plat_priv->bus_priv,
							 modem_current_status);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_update_status(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_update_status(plat_priv->bus_priv, status);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_update_uevent(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status, void *data)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_call_driver_uevent(plat_priv->bus_priv,
						   status, data);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_is_device_down(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pcie_is_device_down(plat_priv->bus_priv);
	default:
		cnss_pr_dbg("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return 0;
	}
}

int cnss_bus_check_link_status(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_check_link_status(plat_priv->bus_priv);
	default:
		cnss_pr_dbg("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return 0;
	}
}

int cnss_bus_recover_link_down(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_recover_link_down(plat_priv->bus_priv);
	default:
		cnss_pr_dbg("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_debug_reg_read(struct cnss_plat_data *plat_priv, u32 offset,
			    u32 *val)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_debug_reg_read(plat_priv->bus_priv, offset,
					       val);
	default:
		cnss_pr_dbg("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return 0;
	}
}

int cnss_bus_debug_reg_write(struct cnss_plat_data *plat_priv, u32 offset,
			     u32 val)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_debug_reg_write(plat_priv->bus_priv, offset,
						val);
	default:
		cnss_pr_dbg("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return 0;
	}
}

int cnss_bus_get_iova(struct cnss_plat_data *plat_priv, u64 *addr, u64 *size)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_get_iova(plat_priv->bus_priv, addr, size);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_get_iova_ipa(struct cnss_plat_data *plat_priv, u64 *addr,
			  u64 *size)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_get_iova_ipa(plat_priv->bus_priv, addr, size);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_get_msi_assignment(struct cnss_plat_data *plat_priv,
				char *msi_name,
				int *num_vectors,
				u32 *user_base_data,
				u32 *base_vector)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_get_user_msi_assignment(plat_priv->bus_priv,
						    msi_name,
						    num_vectors,
						    user_base_data,
						    base_vector);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

bool cnss_bus_is_smmu_s1_enabled(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return false;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_is_smmu_s1_enabled(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return false;
	}
}

int cnss_bus_dsp_link_control(struct cnss_plat_data *plat_priv,
			      bool link_enable)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_dsp_link_control(plat_priv->bus_priv,
						 link_enable);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}
