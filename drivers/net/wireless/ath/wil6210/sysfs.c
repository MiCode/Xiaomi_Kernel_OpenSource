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

static ssize_t
thermal_throttling_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len;
	struct wmi_tt_data tt_data;
	int i, rc;

	rc = wmi_get_tt_cfg(wil, &tt_data);
	if (rc)
		return rc;

	len = snprintf(buf, PAGE_SIZE, "    high      max       critical\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "bb: ");
	if (tt_data.bb_enabled)
		for (i = 0; i < WMI_NUM_OF_TT_ZONES; ++i)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%03d-%03d   ",
					tt_data.bb_zones[i].temperature_high,
					tt_data.bb_zones[i].temperature_low);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "* disabled *");
	len += snprintf(buf + len, PAGE_SIZE - len, "\nrf: ");
	if (tt_data.rf_enabled)
		for (i = 0; i < WMI_NUM_OF_TT_ZONES; ++i)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%03d-%03d   ",
					tt_data.rf_zones[i].temperature_high,
					tt_data.rf_zones[i].temperature_low);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "* disabled *");
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t
thermal_throttling_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int i, rc = -EINVAL;
	char *token, *dupbuf, *tmp;
	struct wmi_tt_data tt_data = {
		.bb_enabled = 0,
		.rf_enabled = 0,
	};

	tmp = kmemdup(buf, count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;
	tmp[count] = '\0';
	dupbuf = tmp;

	/* Format for writing is 12 unsigned bytes separated by spaces:
	 * <bb_z1_h> <bb_z1_l> <bb_z2_h> <bb_z2_l> <bb_z3_h> <bb_z3_l> \
	 * <rf_z1_h> <rf_z1_l> <rf_z2_h> <rf_z2_l> <rf_z3_h> <rf_z3_l>
	 * To disable thermal throttling for bb or for rf, use 0 for all
	 * its six set points.
	 */

	/* bb */
	for (i = 0; i < WMI_NUM_OF_TT_ZONES; ++i) {
		token = strsep(&dupbuf, " ");
		if (!token)
			goto out;
		if (kstrtou8(token, 0, &tt_data.bb_zones[i].temperature_high))
			goto out;
		token = strsep(&dupbuf, " ");
		if (!token)
			goto out;
		if (kstrtou8(token, 0, &tt_data.bb_zones[i].temperature_low))
			goto out;

		if (tt_data.bb_zones[i].temperature_high > 0 ||
		    tt_data.bb_zones[i].temperature_low > 0)
			tt_data.bb_enabled = 1;
	}
	/* rf */
	for (i = 0; i < WMI_NUM_OF_TT_ZONES; ++i) {
		token = strsep(&dupbuf, " ");
		if (!token)
			goto out;
		if (kstrtou8(token, 0, &tt_data.rf_zones[i].temperature_high))
			goto out;
		token = strsep(&dupbuf, " ");
		if (!token)
			goto out;
		if (kstrtou8(token, 0, &tt_data.rf_zones[i].temperature_low))
			goto out;

		if (tt_data.rf_zones[i].temperature_high > 0 ||
		    tt_data.rf_zones[i].temperature_low > 0)
			tt_data.rf_enabled = 1;
	}

	rc = wmi_set_tt_cfg(wil, &tt_data);
	if (rc)
		goto out;

	rc = count;
out:
	kfree(tmp);
	return rc;
}

static DEVICE_ATTR_RW(thermal_throttling);

static ssize_t
fst_link_loss_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(wil->sta); i++)
		if (wil->sta[i].status == wil_sta_connected)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"[%d] %pM %s\n", i, wil->sta[i].addr,
					wil->sta[i].fst_link_loss ?
					"On" : "Off");

	return len;
}

static ssize_t
fst_link_loss_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	u8 addr[ETH_ALEN];
	char *token, *dupbuf, *tmp;
	int rc = -EINVAL;
	bool fst_link_loss;

	tmp = kmemdup(buf, count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	tmp[count] = '\0';
	dupbuf = tmp;

	token = strsep(&dupbuf, " ");
	if (!token)
		goto out;

	/* mac address */
	if (sscanf(token, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		   &addr[0], &addr[1], &addr[2],
		   &addr[3], &addr[4], &addr[5]) != 6)
		goto out;

	/* On/Off */
	if (strtobool(dupbuf, &fst_link_loss))
		goto out;

	wil_dbg_misc(wil, "set [%pM] with %d\n", addr, fst_link_loss);

	rc = wmi_link_maintain_cfg_write(wil, addr, fst_link_loss);
	if (!rc)
		rc = count;

out:
	kfree(tmp);
	return rc;
}

static DEVICE_ATTR_RW(fst_link_loss);

static ssize_t
snr_thresh_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (wil->snr_thresh.enabled)
		len = snprintf(buf, PAGE_SIZE, "omni=%d, direct=%d\n",
			       wil->snr_thresh.omni, wil->snr_thresh.direct);

	return len;
}

static ssize_t
snr_thresh_store(struct device *dev,
		 struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int rc;
	short omni, direct;

	/* to disable snr threshold, set both omni and direct to 0 */
	if (sscanf(buf, "%hd %hd", &omni, &direct) != 2)
		return -EINVAL;

	rc = wmi_set_snr_thresh(wil, omni, direct);
	if (!rc)
		rc = count;

	return rc;
}

static DEVICE_ATTR_RW(snr_thresh);

static struct attribute *wil6210_sysfs_entries[] = {
	&dev_attr_ftm_txrx_offset.attr,
	&dev_attr_thermal_throttling.attr,
	&dev_attr_fst_link_loss.attr,
	&dev_attr_snr_thresh.attr,
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

	kobject_uevent(&dev->kobj, KOBJ_CHANGE);

	return 0;
}

void wil6210_sysfs_remove(struct wil6210_priv *wil)
{
	struct device *dev = wil_to_dev(wil);

	sysfs_remove_group(&dev->kobj, &wil6210_attribute_group);
	kobject_uevent(&dev->kobj, KOBJ_CHANGE);
}
