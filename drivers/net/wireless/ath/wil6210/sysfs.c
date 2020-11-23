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

int wil_ftm_offset_set(struct wil6210_priv *wil, const char *buf)
{
	wil->ftm_txrx_offset.enabled = 0;
	if (sscanf(buf, "%u %u", &wil->ftm_txrx_offset.tx_offset,
		   &wil->ftm_txrx_offset.tx_offset) != 2)
		return -EINVAL;

	wil->ftm_txrx_offset.enabled = 1;
	return 0;
}

static ssize_t
ftm_txrx_offset_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int rc;

	rc = wil_ftm_offset_set(wil, buf);
	if (rc < 0)
		return rc;

	rc = wmi_set_tof_tx_rx_offset(wil, wil->ftm_txrx_offset.tx_offset,
				      wil->ftm_txrx_offset.rx_offset);
	if (rc < 0)
		return rc;

	return count;
}

static DEVICE_ATTR_RW(ftm_txrx_offset);

static ssize_t
board_file_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);

	wil_get_board_file(wil, buf, PAGE_SIZE);
	strlcat(buf, "\n", PAGE_SIZE);
	return strlen(buf);
}

int wil_board_file_set(struct wil6210_priv *wil, const char *buf,
		       size_t count)
{
	size_t len;

	mutex_lock(&wil->mutex);

	kfree(wil->board_file);
	wil->board_file = NULL;

	len = count;
	if (buf[count - 1] == '\n')
		len--;
	len = strnlen(buf, len);
	if (len > 0) {
		wil->board_file = kmalloc(len + 1, GFP_KERNEL);
		if (!wil->board_file) {
			mutex_unlock(&wil->mutex);
			return -ENOMEM;
		}
		strlcpy(wil->board_file, buf, len + 1);
	}
	mutex_unlock(&wil->mutex);

	return 0;
}

static ssize_t
board_file_store(struct device *dev,
		 struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int rc;

	rc = wil_board_file_set(wil, buf, count);
	if (rc < 0)
		return rc;

	return count;
}

static DEVICE_ATTR_RW(board_file);

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

int wil_tt_set(struct wil6210_priv *wil, const char *buf,
	       size_t count)
{
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

	wil->tt_data = tt_data;
	wil->tt_data_set = true;
	rc = 0;

out:
	kfree(tmp);
	return rc;
}

static ssize_t
thermal_throttling_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int rc;

	rc = wil_tt_set(wil, buf, count);
	if (rc)
		return rc;

	rc = wmi_set_tt_cfg(wil, &wil->tt_data);
	if (rc)
		return rc;

	return count;
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
fst_config_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	u8 addr[ETH_ALEN];
	int rc;
	u8 enabled, entry_mcs, exit_mcs, slevel;

	/* <ap_bssid> <enabled> <entry_mcs> <exit_mcs> <sensitivity_level> */
	if (sscanf(buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx %hhu %hhu %hhu %hhu",
		   &addr[0], &addr[1], &addr[2],
		   &addr[3], &addr[4], &addr[5],
		   &enabled, &entry_mcs, &exit_mcs, &slevel) != 10)
		return -EINVAL;

	if (entry_mcs > WIL_MCS_MAX || exit_mcs > WIL_MCS_MAX ||
	    entry_mcs < exit_mcs || slevel > WMI_FST_SWITCH_SENSITIVITY_HIGH)
		return -EINVAL;

	wil_dbg_misc(wil,
		     "fst_config %sabled for [%pM] with entry/exit MCS %d/%d, sensitivity %s\n",
		     enabled ? "en" : "dis", addr, entry_mcs, exit_mcs,
		     (slevel == WMI_FST_SWITCH_SENSITIVITY_LOW) ?
			"LOW" : (slevel == WMI_FST_SWITCH_SENSITIVITY_HIGH) ?
					"HIGH" : "MED");

	rc = wmi_set_fst_config(wil, addr, enabled, entry_mcs, exit_mcs,
				slevel);
	if (!rc)
		rc = count;

	return rc;
}

static DEVICE_ATTR_WO(fst_config);

static ssize_t
vr_profile_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len;

	len = snprintf(buf, PAGE_SIZE, "%s\n",
		       wil_get_vr_profile_name(wil->vr_profile));

	return len;
}

static ssize_t
vr_profile_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	u8 profile;
	int rc = 0;

	if (kstrtou8(buf, 0, &profile))
		return -EINVAL;

	if (test_bit(wil_status_fwready, wil->status)) {
		wil_err(wil, "Cannot set VR while interface is up\n");
		return -EIO;
	}

	if (profile == wil->vr_profile) {
		wil_info(wil, "Ignore same VR profile %s\n",
			 wil_get_vr_profile_name(wil->vr_profile));
		return count;
	}

	wil_info(wil, "Sysfs: set VR profile to %s\n",
		 wil_get_vr_profile_name(profile));

	/* Enabling of VR mode is done from wil_reset after FW is ready.
	 * Disabling is done from here.
	 */
	if (profile == WMI_VR_PROFILE_DISABLED) {
		rc = wil_vr_update_profile(wil, profile);
		if (rc)
			return rc;
	}
	wil->vr_profile = profile;

	return count;
}

static DEVICE_ATTR_RW(vr_profile);

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

int wil_snr_thresh_set(struct wil6210_priv *wil, const char *buf)
{
	wil->snr_thresh.enabled = 0;
	/* to disable snr threshold, set both omni and direct to 0 */
	if (sscanf(buf, "%hd %hd", &wil->snr_thresh.omni,
		   &wil->snr_thresh.direct) != 2)
		return -EINVAL;

	if (wil->snr_thresh.omni != 0 || wil->snr_thresh.direct != 0)
		wil->snr_thresh.enabled = 1;

	return 0;
}

static ssize_t max_mcs_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	ssize_t len;

	len = scnprintf(buf, PAGE_SIZE, "%d\n", wil->max_mcs);

	return len;
}

static ssize_t max_mcs_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	u8 max_mcs;

	if (kstrtou8(buf, 0, &max_mcs))
		return -EINVAL;

	if (test_bit(wil_status_fwready, wil->status)) {
		wil_err(wil, "Cannot set MAX MCS while interface is up\n");
		return -EIO;
	}

	if (max_mcs > WIL_MCS_MAX) {
		wil_err(wil, "Ignore invalid MCS %d\n", max_mcs);
		return -EINVAL;
	}

	wil_info(wil, "Sysfs: set max MCS to %d\n", max_mcs);

	wil->max_mcs = max_mcs;

	return count;
}

static DEVICE_ATTR_RW(max_mcs);

static ssize_t
snr_thresh_store(struct device *dev,
		 struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	int rc;

	rc = wil_snr_thresh_set(wil, buf);
	if (rc < 0)
		return rc;

	rc = wmi_set_snr_thresh(wil, wil->snr_thresh.omni,
				wil->snr_thresh.direct);
	if (!rc)
		rc = count;

	return rc;
}

static DEVICE_ATTR_RW(snr_thresh);

static struct attribute *wil6210_sysfs_entries[] = {
	&dev_attr_ftm_txrx_offset.attr,
	&dev_attr_board_file.attr,
	&dev_attr_thermal_throttling.attr,
	&dev_attr_fst_link_loss.attr,
	&dev_attr_snr_thresh.attr,
	&dev_attr_vr_profile.attr,
	&dev_attr_fst_config.attr,
	&dev_attr_max_mcs.attr,
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
