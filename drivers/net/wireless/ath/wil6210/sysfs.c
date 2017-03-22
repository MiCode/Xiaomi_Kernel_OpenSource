/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/device.h>
#include <linux/sysfs.h>

#include "wil6210.h"
#include "wmi.h"

static ssize_t
wil_ftm_txrx_offset_sysfs_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
	struct {
		struct wmi_cmd_hdr wmi;
		struct wmi_tof_get_tx_rx_offset_event evt;
	} __packed reply;
	int rc;
	ssize_t len;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -EOPNOTSUPP;

	memset(&reply, 0, sizeof(reply));
	rc = wmi_call(wil, WMI_TOF_GET_TX_RX_OFFSET_CMDID, NULL, 0,
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
wil_ftm_txrx_offset_sysfs_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct wil6210_priv *wil = dev_get_drvdata(dev);
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
	rc = wmi_call(wil, WMI_TOF_SET_TX_RX_OFFSET_CMDID, &cmd, sizeof(cmd),
		      WMI_TOF_SET_TX_RX_OFFSET_EVENTID,
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

static DEVICE_ATTR(ftm_txrx_offset, 0644,
		   wil_ftm_txrx_offset_sysfs_show,
		   wil_ftm_txrx_offset_sysfs_store);

static ssize_t
wil_tt_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
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
wil_tt_sysfs_store(struct device *dev, struct device_attribute *attr,
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

static DEVICE_ATTR(thermal_throttling, 0644,
		   wil_tt_sysfs_show, wil_tt_sysfs_store);

static struct attribute *wil6210_sysfs_entries[] = {
	&dev_attr_ftm_txrx_offset.attr,
	&dev_attr_thermal_throttling.attr,
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
