// SPDX-License-Identifier: ISC
/* Copyright (c) 2016,2018-2019 The Linux Foundation. All rights reserved. */

#include <linux/device.h>
#include <linux/sysfs.h>

#include "wil6210.h"
#include "wmi.h"

static ssize_t
ftm_txrx_offset_show(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	struct wil6210_vif *vif = ndev_to_vif(wil->main_ndev);
	struct {
		struct wmi_cmd_hdr wmi;
		struct wmi_tof_get_tx_rx_offset_event evt;
	} __packed reply;
	int rc;
	ssize_t len;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -EOPNOTSUPP;

	memset(&reply, 0, sizeof(reply));
	rc = wmi_call(wil, WMI_TOF_GET_TX_RX_OFFSET_CMDID, vif->mid, NULL, 0,
		      WMI_TOF_GET_TX_RX_OFFSET_EVENTID,
		      &reply, sizeof(reply), 100);
	if (rc < 0)
		return rc;
	if (reply.evt.status) {
		wil_err(wil, "get_tof_tx_rx_offset failed, error %d\n",
			reply.evt.status);
		return -EIO;
	}
	len = snprintf(buf, PAGE_SIZE, "%u %u\n",
		       le32_to_cpu(reply.evt.tx_offset),
		       le32_to_cpu(reply.evt.rx_offset));
	return len;
}

static ssize_t
ftm_txrx_offset_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	struct wil6210_vif *vif = ndev_to_vif(wil->main_ndev);
	struct wmi_tof_set_tx_rx_offset_cmd cmd;
	struct {
		struct wmi_cmd_hdr wmi;
		struct wmi_tof_set_tx_rx_offset_event evt;
	} __packed reply;
	unsigned int tx_offset, rx_offset;
	int rc;

	if (sscanf(buf, "%u %u", &tx_offset, &rx_offset) != 2)
		return -EINVAL;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -EOPNOTSUPP;

	memset(&cmd, 0, sizeof(cmd));
	cmd.tx_offset = cpu_to_le32(tx_offset);
	cmd.rx_offset = cpu_to_le32(rx_offset);
	memset(&reply, 0, sizeof(reply));
	rc = wmi_call(wil, WMI_TOF_SET_TX_RX_OFFSET_CMDID, vif->mid,
		      &cmd, sizeof(cmd), WMI_TOF_SET_TX_RX_OFFSET_EVENTID,
		      &reply, sizeof(reply), 100);
	if (rc < 0)
		return rc;
	if (reply.evt.status) {
		wil_err(wil, "set_tof_tx_rx_offset failed, error %d\n",
			reply.evt.status);
		return -EIO;
	}
	return count;
}

static DEVICE_ATTR_RW(ftm_txrx_offset);

static struct attribute *wil6210_sysfs_entries[] = {
	&dev_attr_ftm_txrx_offset.attr,
	NULL
};

static struct attribute_group wil6210_attribute_group = {
	.name = "wil6210",
	.attrs = wil6210_sysfs_entries,
};

int wil6210_sysfs_init(struct wil6210_priv *wil)
{
	struct device *dev = wil_to_dev(wil);
	int err;

	err = sysfs_create_group(&dev->kobj, &wil6210_attribute_group);
	if (err) {
		wil_err(wil, "failed to create sysfs group: %d\n", err);
		return err;
	}

	return 0;
}

void wil6210_sysfs_remove(struct wil6210_priv *wil)
{
	struct device *dev = wil_to_dev(wil);

	sysfs_remove_group(&dev->kobj, &wil6210_attribute_group);
}
