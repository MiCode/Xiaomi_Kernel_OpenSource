// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"REMOTE-FG: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/soc/qcom/slate_events_bridge_intf.h>
#include <linux/uaccess.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#include "battery-profile-loader.h"
#include "smb5-iio.h"
#include "smblite-remote-bms.h"

static struct smblite_remote_bms *the_bms;

#define DEBUG_BATT_ID_LOW	6000
#define DEBUG_BATT_ID_HIGH	8500
static bool is_debug_batt_id(struct smblite_remote_bms *bms)
{
	if (is_between(DEBUG_BATT_ID_LOW, DEBUG_BATT_ID_HIGH,
					bms->batt_id_ohm))
		return true;

	return false;
}

static int remote_bms_read_prop_from_sdam(struct smblite_remote_bms *bms,
					int offset,
					int *val)
{
	if (!bms->nvmem)
		return -EINVAL;

	return nvmem_device_read(bms->nvmem, offset, 1, val);
}

static int get_bms_param_from_smb5_param(int smb_param, int *offset)
{
	int rc = 0;

	switch (smb_param) {
	case SMB5_QG_CHARGE_FULL:
		*offset = CHARGE_FULL;
		break;
	case SMB5_QG_CHARGE_FULL_DESIGN:
		*offset = CHARGE_FULL_DESIGN;
		break;
	case SMB5_QG_CURRENT_NOW:
		*offset = CURRENT_NOW;
		break;
	case SMB5_QG_CAPACITY:
		*offset = CAPACITY;
		break;
	case SMB5_QG_TIME_TO_FULL_NOW:
		*offset = TIME_TO_FULL;
		break;
	case SMB5_QG_CYCLE_COUNT:
		*offset = CYCLE_COUNT;
		break;
	case SMB5_QG_CHARGE_COUNTER:
		*offset = CHARGE_COUNTER;
		break;
	default:
		pr_err_ratelimited("Unsupported SMB param\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int bms_get_buffered_data(int channel, int *val, int src)
{
	int rc = 0, offset;

	rc = get_bms_param_from_smb5_param(channel, &offset);
	if (rc < 0) {
		pr_err("Unsupported remote bms property %d\n", channel);
		return rc;
	}

	if (src == BMS_SDAM) {
		rc = remote_bms_read_prop_from_sdam(the_bms, offset, val);
		if (rc < 0) {
			pr_err("SDAM read failed for property %d, rc=%d\n",
					channel, rc);
			return rc;
		}
	} else {
		*val = the_bms->rx_params[offset].data;
		rc = 0;
	}

	pr_debug("remote_bms_param:%u src:%d value:%d\n", offset, src, *val);

	return rc;
}

int remote_bms_get_prop(int channel, int *val, int src)
{
	int rc = 0;

	if (!the_bms)
		return -EINVAL;

	switch (channel) {
	case SMB5_QG_CHARGE_FULL:
	case SMB5_QG_CHARGE_FULL_DESIGN:
	case SMB5_QG_CURRENT_NOW:
	case SMB5_QG_TIME_TO_FULL_NOW:
	case SMB5_QG_CYCLE_COUNT:
	case SMB5_QG_CHARGE_COUNTER:
		rc = bms_get_buffered_data(channel, val, src);
		break;
	case SMB5_QG_CAPACITY:
		if (is_debug_batt_id(the_bms)) {
			*val = REMOTE_FG_DEBUG_BATT_SOC;
		} else if (!the_bms->is_seb_up || !the_bms->received_first_data) {
			pr_debug("Waiting for Co-Proc Up: %d / QBG Data : %d\n",
				the_bms->is_seb_up, the_bms->received_first_data);
			return -EAGAIN;
		} else
			rc = bms_get_buffered_data(channel, val, src);
		break;
	case SMB5_QG_VOLTAGE_NOW:
		rc = iio_read_channel_processed(the_bms->batt_volt_chan, val);
		break;
	case SMB5_QG_DEBUG_BATTERY:
		*val = is_debug_batt_id(the_bms);
		break;
	case SMB5_QG_TEMP:
		rc = iio_read_channel_processed(the_bms->batt_temp_chan, val);
		break;
	case SMB5_QG_RESISTANCE_ID:
		*val = the_bms->batt_id_ohm;
		break;
	default:
		pr_err("Invalid channel read for remote-fg\n");
		rc = -EINVAL;
		break;
	}

	if ((rc < 0) && (rc != -EAGAIN))
		pr_err("Failed to read remote bms property %d, rc=%d\n", channel, rc);
	else
		pr_debug("Read Remote bms param:%u value:%d\n", channel, *val);

	return rc;
}

static int remote_bms_get_data(struct smblite_remote_bms *bms)
{
	int rc, buf_ptr = 0;
	char *tx_buf;
	unsigned int tx_buf_size;

	if (!bms->seb_handle) {
		pr_err("Not registered with Slate event bridge, cannot request data\n");
		return -EINVAL;
	}

	mutex_lock(&bms->tx_lock);

	tx_buf_size = SEB_BUF_HEADER_SIZE + SEB_EACH_OPCODE_SIZE;

	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf) {
		mutex_unlock(&bms->tx_lock);
		return -ENOMEM;
	}

	tx_buf[buf_ptr++] = BMS_READ;
	/* One opcode REQUEST_DATA to request data from remote-fg */
	tx_buf[buf_ptr++] = 1;
	tx_buf[buf_ptr++] = REQUEST_DATA;

	rc = seb_send_event_to_slate(bms->seb_handle, GMI_SLATE_EVENT_QBG, tx_buf,
					tx_buf_size);
	if (rc < 0) {
		pr_err("Failed to send REQUEST_DATA event to remote-fg, rc=%d\n",
			rc);
		goto free_tx_buf;
	}

	pr_debug("Send REQUEST_DATA event to remote-fg successful\n");

free_tx_buf:
	kfree(tx_buf);
	mutex_unlock(&bms->tx_lock);

	return rc;
}

static void periodic_fg_work(struct work_struct *work)
{
	struct smblite_remote_bms *bms = container_of(work,
						struct smblite_remote_bms,
						periodic_fg_work.work);
	int rc = 0;

	pr_debug("Read runtime data work expired, requesting remote-fg for data\n");
	rc = remote_bms_get_data(bms);
	if (rc < 0) {
		pr_err("Couldn't request data from remote-fg, rc=%d\n", rc);
		return;
	}

	schedule_delayed_work(&bms->periodic_fg_work,
				msecs_to_jiffies(BMS_READ_INTERVAL_MS));
}

static int remote_bms_send_data(struct smblite_remote_bms *bms)
{
	int rc;
	char *tx_buf;
	unsigned int tx_buf_size, buf_ptr = 0;

	mutex_lock(&bms->tx_lock);
	tx_buf_size = SEB_BUF_HEADER_SIZE + (TX_MAX * SEB_EACH_PARAM_SIZE);

	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf) {
		mutex_unlock(&bms->tx_lock);
		return -ENOMEM;
	}

	tx_buf[buf_ptr++] = BMS_WRITE;
	tx_buf[buf_ptr++] = TX_MAX;

	tx_buf[buf_ptr++] = CHARGE_STATUS;
	*(unsigned int *)(tx_buf + buf_ptr) = (unsigned int)bms->charge_status;
	buf_ptr += 4;

	tx_buf[buf_ptr++] = CHARGE_TYPE;
	*(unsigned int *)(tx_buf + buf_ptr) = (unsigned int)bms->charge_type;
	buf_ptr += 4;

	tx_buf[buf_ptr++] = CHARGER_PRESENT;
	*(unsigned int *)(tx_buf + buf_ptr) = (unsigned int)bms->charger_present;
	buf_ptr += 4;

	rc = seb_send_event_to_slate(bms->seb_handle, GMI_SLATE_EVENT_QBG, tx_buf,
					tx_buf_size);
	if (rc < 0) {
		pr_err("Failed to send event to remote-fg, rc=%d\n", rc);
		goto free_tx_buf;
	}

	pr_debug("Send event to remote-fg successful, charge_status:%d charger_present:%d chg_type:%d\n",
			bms->charge_status, bms->charger_present, bms->charge_type);

free_tx_buf:
	kfree(tx_buf);
	mutex_unlock(&bms->tx_lock);

	return rc;
}

static int remote_bms_handle_recharge(struct smblite_remote_bms *bms)
{
	int rc = 0;
	union power_supply_propval prop = {0, };
	int force_recharge = bms->rx_params[RECHARGE_TRIGGER].data;
	int recharge_fv = bms->rx_params[RECHARGE_FV].data;
	int recharge_iterm = bms->rx_params[RECHARGE_ITERM].data;

	if (force_recharge != bms->force_recharge) {
		bms->force_recharge = force_recharge;
		if (!force_recharge)
			return 0;

		pr_debug("Recharge configuration requested with FV:%duV Iterm:%dmA\n",
						recharge_fv, recharge_iterm);

		if ((recharge_fv >= 0) && (recharge_fv != bms->recharge_float_voltage)) {
			prop.intval = bms->recharge_float_voltage = recharge_fv;
			rc = power_supply_set_property(bms->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
			if (rc < 0) {
				pr_err("Failed to set recharge voltage_max property on batt_psy, rc=%d\n",
					rc);
				return rc;
			}
		}

		/* Configure charger iterm only if digital termination is used */
		if (bms->default_iterm_ma && (bms->default_iterm_ma != -EINVAL) &&
			(recharge_iterm != bms->recharge_iterm)) {
			prop.intval = bms->recharge_iterm = recharge_iterm;
			/* smblite charger expects termination current with -ve sign */
			prop.intval = (-1 * prop.intval);
			rc = power_supply_set_property(bms->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
			if (rc < 0) {
				pr_err("Failed to set recharge charge_term_current property on batt_psy, rc=%d\n",
					rc);
				return rc;
			}
		}

		rc = bms->iio_write(bms->dev, PSY_IIO_FORCE_RECHARGE, bms->force_recharge);
		if (rc < 0)
			pr_err("Failed to set force recharge, rc=%d\n", rc);

		pr_debug("Recharge configuration completed with FV:%duV Iterm:%dmA\n",
					bms->recharge_float_voltage, bms->recharge_iterm);
	}

	return rc;
}

static void rx_data_work(struct work_struct *work)
{
	struct smblite_remote_bms *bms = container_of(work,
						struct smblite_remote_bms,
						rx_data_work);
	int rc = -EINVAL, i;
	unsigned int buf_ptr = 0, param;
	unsigned char num_params;
	char *rx_buf;

	vote(bms->awake_votable, REMOTE_FG_VOTER, true, 0);

	mutex_lock(&bms->rx_lock);

	num_params = *(unsigned char *)(bms->rx_buf + 1);

	pr_debug("Processing %d parameteres\n", num_params);

	rx_buf = (char *)(bms->rx_buf + SEB_BUF_HEADER_SIZE);

	if (num_params > SEB_MAX_RX_PARAMS)
		num_params = SEB_MAX_RX_PARAMS;

	for (i = 0, buf_ptr = 0; i < num_params; i++) {
		param = rx_buf[buf_ptr++];

		switch (param) {
		case CAPACITY:
		case CURRENT_NOW:
		case VOLTAGE_OCV:
		case CYCLE_COUNT:
		case CHARGE_COUNTER:
		case CHARGE_FULL:
		case CHARGE_FULL_DESIGN:
		case TIME_TO_FULL:
		case TIME_TO_EMPTY:
		case SOH:
		case RECHARGE_TRIGGER:
		case RECHARGE_FV:
		case RECHARGE_ITERM:
		case REQUEST_CHG_DATA:
			bms->rx_params[param].data =
					*(unsigned int *)(rx_buf + buf_ptr);
			buf_ptr += 4;
			pr_debug("param:%u data:%u\n", param, bms->rx_params[param].data);
			break;
		default:
			pr_debug("Unsupported remote-fg parameter %d with value %d\n",
					param, *(unsigned int *)(rx_buf + buf_ptr));
			buf_ptr += 4;
			break;
		}
	}

	if (!bms->received_first_data) {
		pr_debug("First SOC reported from Co-Proc : %d\n",
				bms->rx_params[CAPACITY].data);
		bms->received_first_data = true;
	}

	rc = remote_bms_handle_recharge(bms);
	if (rc < 0) {
		pr_err("Failed to handle recharge, rc=%d\n",
			rc);
		goto out;
	}

	if (bms->rx_params[REQUEST_CHG_DATA].data) {
		pr_debug("Charger data requested by remote-fg\n");

		rc = remote_bms_send_data(bms);
		if (rc < 0) {
			pr_err("Failed to send data to remote-fg, rc=%d\n",
				rc);
			goto out;
		}
	}

	if (!rc)
		pr_debug("Finished processing rx buf from remote-fg\n");

	/* Notify Clients so that they pick-up latest data from QBG */
	if (bms->batt_psy)
		power_supply_changed(bms->batt_psy);

out:
	mutex_unlock(&bms->rx_lock);
	vote(bms->awake_votable, REMOTE_FG_VOTER, false, 0);
}

static int seb_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct smblite_remote_bms *bms = container_of(nb,
			struct smblite_remote_bms, seb_nb);
	int rc, buf_size;
	u8 command, num_params;

	if (is_debug_batt_id(bms))
		return 0;

	if (event == GLINK_CHANNEL_STATE_UP) {
		bms->is_seb_up = true;

		rc = remote_bms_get_data(bms);
		if (rc < 0) {
			pr_err("Couldn't request runtime data from QBG, rc=%d\n");
			return rc;
		}
		pr_debug("Slate-UP, requested first data from QBG\n");

		/* send the first charger status */
		if (!work_pending(&bms->psy_status_change_work))
			schedule_work(&bms->psy_status_change_work);

		schedule_delayed_work(&bms->periodic_fg_work,
				msecs_to_jiffies(BMS_READ_INTERVAL_MS));
		return 0;
	} else if (event != GMI_SLATE_EVENT_QBG) {
		pr_debug("SEB event is not for GMI_SLATE_EVENT_QBG\n");
		return 0;
	}

	/* Always process latest data from remote-fg */
	cancel_work_sync(&bms->rx_data_work);

	if (!data) {
		pr_err("Invalid buffer received from remote-fg\n");
		return -EINVAL;
	}

	command = *(u8 *)data;
	if (command != BMS_READ) {
		pr_err("Packet with invalid command %d received, cannot process\n", command);
		return -EINVAL;
	}

	num_params = *(u8 *)(data + 1);
	if (!num_params) {
		pr_err("No params received from remote-fg\n");
		return -EINVAL;
	}

	pr_debug("Remote-fg data received: event:%d command:%d num_params:%d\n",
			event, command, num_params);

	buf_size = SEB_BUF_HEADER_SIZE + (num_params * SEB_EACH_PARAM_SIZE);
	if (buf_size > SEB_RX_BUF_SIZE) {
		pr_err("Received more parameters from remote-fg, processing only till %d bytes\n",
			SEB_RX_BUF_SIZE);
	}

	memcpy(bms->rx_buf, data, SEB_RX_BUF_SIZE);

	/* Process QBG data */
	schedule_work(&bms->rx_data_work);

	/* Cancel and re-schedule periodic work to read QBG data */
	cancel_delayed_work(&bms->periodic_fg_work);
	schedule_delayed_work(&bms->periodic_fg_work,
				msecs_to_jiffies(BMS_READ_INTERVAL_MS));

	return 0;
}

static int remote_bms_set_battery_params(struct smblite_remote_bms *bms)
{
	int rc;
	union power_supply_propval prop = {0, };

	prop.intval = bms->float_volt_uv;
	rc = power_supply_set_property(bms->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set voltage_max property on batt_psy, rc=%d\n",
			rc);
		return rc;
	}

	prop.intval = bms->fastchg_curr_ma * 1000;
	rc = power_supply_set_property(bms->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &prop);
	if (rc < 0) {
		pr_err("Failed to set constant_charge_current_max property on batt_psy, rc=%d\n",
			rc);
		return rc;
	}

	if (bms->default_iterm_ma != -EINVAL) {
		prop.intval = bms->default_iterm_ma;
		rc = power_supply_set_property(bms->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
		if (rc < 0) {
			pr_err("Failed to set charge_current_max property on batt_psy, rc=%d\n",
				rc);
			return rc;
		}
	}

	bms->recharge_float_voltage = bms->recharge_iterm = 0;

	pr_debug("Set iterm:%dmA max voltage:%duV and max_current:%dmA to charger\n",
			bms->default_iterm_ma, bms->float_volt_uv, bms->fastchg_curr_ma);

	return rc;
}

static bool is_batt_available(struct smblite_remote_bms *bms)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!bms->batt_psy) {
		bms->batt_psy = power_supply_get_by_name("battery");
		if (!bms->batt_psy)
			return false;

		/*
		 * Read termination current from charger,
		 * Use it to configure iterm after recharge upon USB removal.
		 */
		rc = power_supply_get_property(bms->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
		if (!rc)
			bms->default_iterm_ma = prop.intval;

		rc = remote_bms_set_battery_params(bms);
		return (rc < 0) ? false : true;
	}

	return true;
}

static bool is_usb_available(struct smblite_remote_bms *bms)
{
	if (!bms->usb_psy)
		bms->usb_psy = power_supply_get_by_name("usb");

	if (!bms->usb_psy)
		return false;

	return true;
}

static void psy_status_change_work(struct work_struct *work)
{
	struct smblite_remote_bms *bms = container_of(work,
						struct smblite_remote_bms,
						psy_status_change_work);
	int rc;
	int charge_status, charge_type, charger_present;
	union power_supply_propval prop = {0, };

	vote(bms->awake_votable, REMOTE_FG_VOTER, true, 0);

	if (!bms->seb_handle) {
		pr_err("Not registered with Slate event bridge, exiting\n");
		goto out;
	}

	if (!is_batt_available(bms))
		goto out;

	if (!is_usb_available(bms))
		goto out;

	rc = power_supply_get_property(bms->batt_psy, POWER_SUPPLY_PROP_STATUS,
					&prop);
	if (rc < 0) {
		pr_err("Failed to get battery charge status, rc=%d\n", rc);
		goto out;
	}
	charge_status = prop.intval;

	rc = power_supply_get_property(bms->usb_psy, POWER_SUPPLY_PROP_PRESENT,
					&prop);
	if (rc < 0) {
		pr_err("Failed to get USB present, rc=%d\n", rc);
		goto out;
	}
	charger_present = prop.intval;

	rc = power_supply_get_property(bms->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0) {
		pr_err("Failed to get charge type, rc=%d\n", rc);
		goto out;
	}
	charge_type = prop.intval;

	/*
	 * Set FCC and iterm back to default value on charger removal
	 * as they could have been updated during recharge.
	 */
	if (bms->charger_present && !charger_present) {
		rc = remote_bms_set_battery_params(bms);
		if (rc < 0) {
			pr_err("Failed to set battery FCC and FV on charger removal\n");
			goto out;
		}
	}

	if ((charge_status != bms->charge_status) ||
	    (charger_present != bms->charger_present) ||
	    (charge_type != bms->charge_type)) {
		bms->charge_status = charge_status;
		bms->charger_present = charger_present;
		bms->charge_type = charge_type;

		pr_debug("Charger update: charger_status:%d charger_present:%d charger_type:%d\n",
				bms->charge_status, bms->charger_present, bms->charge_type);

		rc = remote_bms_send_data(bms);
		if (rc < 0)
			pr_err("Failed to send data to remote-fg, rc=%d\n", rc);
	}

out:
	vote(bms->awake_votable, REMOTE_FG_VOTER, false, 0);
}

static int bms_psy_notifier_cb(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct power_supply *psy = data;
	struct smblite_remote_bms *bms = container_of(nb,
						struct smblite_remote_bms,
							psy_nb);

	if (is_debug_batt_id(bms))
		return NOTIFY_OK;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (work_pending(&bms->psy_status_change_work))
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0)) {
		schedule_work(&bms->psy_status_change_work);
	}

	return NOTIFY_OK;
}

static int remote_bms_get_adc_props(struct smblite_remote_bms *bms)
{
	int rc = 0;

	bms->batt_id_chan = iio_channel_get(bms->dev, "batt-id");
	if (IS_ERR(bms->batt_id_chan)) {
		rc = PTR_ERR(bms->batt_id_chan);
		if (rc != -EPROBE_DEFER)
			pr_err("batt-id channel unavailable, rc=%d\n", rc);
		bms->batt_id_chan = NULL;
		return rc;
	}

	rc = iio_read_channel_processed(bms->batt_id_chan, &bms->batt_id_ohm);
	if (rc < 0) {
		pr_err("Couldn't read battery ID channel rc=%d\n", rc);
		return rc;
	}

	bms->batt_temp_chan = iio_channel_get(bms->dev, "batt-temp");
	if (IS_ERR(bms->batt_temp_chan)) {
		rc = PTR_ERR(bms->batt_temp_chan);
		if (rc != -EPROBE_DEFER)
			pr_err("batt-temp channel unavailable, rc=%d\n", rc);
		bms->batt_temp_chan = NULL;
		return rc;
	}

	bms->batt_volt_chan = iio_channel_get(bms->dev, "batt-volt");
	if (IS_ERR(bms->batt_volt_chan)) {
		rc = PTR_ERR(bms->batt_volt_chan);
		if (rc != -EPROBE_DEFER)
			pr_err("batt-volt channel unavailable, rc=%d\n", rc);
		bms->batt_volt_chan = NULL;
		return rc;
	}

	return rc;
}

static int remote_bms_parse_profile(struct smblite_remote_bms *bms)
{
	struct device_node *profile_node;
	int rc;

	bms->batt_node = of_find_node_by_name(bms->dev->of_node, "qcom,battery-data");
	if (!bms->batt_node) {
		pr_err("Batterydata not available\n");
		return -ENODEV;
	}

	profile_node = of_batterydata_get_best_profile(bms->batt_node,
				bms->batt_id_ohm / 1000, NULL);
	if (IS_ERR_OR_NULL(profile_node)) {
		rc = profile_node ? PTR_ERR(profile_node) : -EINVAL;
		pr_err("Failed to detect valid battery profile %d\n", rc);
		return 0;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&bms->batt_profile_name);
	if (rc < 0) {
		pr_err("Failed to get battery profile name, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
				&bms->float_volt_uv);
	if (rc < 0) {
		pr_err("Failed to read battery float-voltage rc:%d\n", rc);
		bms->float_volt_uv = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
				&bms->fastchg_curr_ma);
	if (rc < 0) {
		pr_err("Failed to read battery fastcharge current rc:%d\n", rc);
		bms->fastchg_curr_ma = -EINVAL;
	}

	pr_debug("batt_id=%d Ohm profile=%s, FV=%d uV FCC=%d mA\n",
		bms->batt_id_ohm, bms->batt_profile_name, bms->float_volt_uv,
		bms->fastchg_curr_ma);

	return rc;
}

int remote_bms_init(struct smblite_remote_bms *bms)
{
	int rc;
	struct seb_notif_info *seb_handle;

	if (!bms->iio_read || !bms->iio_write) {
		pr_err("Invalid iio read/write pointers\n");
		return -EINVAL;
	}

	rc = remote_bms_get_adc_props(bms);
	if (rc < 0) {
		pr_err("Couldn't get adc props, rc=%d\n", rc);
		return rc;
	}

	rc = remote_bms_parse_profile(bms);
	if (rc < 0) {
		pr_err("Couldn't parse battery profile, rc=%d\n", rc);
		return rc;
	}

	bms->awake_votable = find_votable("AWAKE");
	if (bms->awake_votable == NULL) {
		rc = -EINVAL;
		pr_err("Couldn't find AWAKE votable rc=%d\n", rc);
		return rc;
	}

	bms->is_seb_up = false;
	bms->received_first_data = false;
	mutex_init(&bms->data_lock);
	mutex_init(&bms->tx_lock);
	mutex_init(&bms->rx_lock);
	INIT_DELAYED_WORK(&bms->periodic_fg_work, periodic_fg_work);
	INIT_WORK(&bms->psy_status_change_work, psy_status_change_work);
	INIT_WORK(&bms->rx_data_work, rx_data_work);

	bms->seb_nb.notifier_call = seb_notifier_cb;

	seb_handle = seb_register_for_slate_event(GMI_SLATE_EVENT_QBG, &bms->seb_nb);
	if (IS_ERR_OR_NULL(seb_handle)) {
		rc = seb_handle ? PTR_ERR(seb_handle) : -EINVAL;
		pr_err("Failed to register with Slate event bridge, rc=%d\n", rc);
		return rc;
	}

	bms->seb_handle = seb_handle;

	bms->psy_nb.notifier_call = bms_psy_notifier_cb;
	rc = power_supply_reg_notifier(&bms->psy_nb);
	if (rc < 0)
		pr_err("Failed to register psy notifier,rc = %d\n", rc);

	the_bms = bms;

	pr_info("smblite remote-fg registered - battery detected:%s\n",
		is_debug_batt_id(bms) ? "debug_board" : bms->batt_profile_name);

	return rc;
}

int remote_bms_deinit(void)
{
	int rc = 0;

	if (!the_bms)
		return -ENODEV;

	cancel_delayed_work_sync(&the_bms->periodic_fg_work);
	cancel_work_sync(&the_bms->psy_status_change_work);
	cancel_work_sync(&the_bms->rx_data_work);

	rc = seb_unregister_for_slate_event(the_bms->seb_handle, &the_bms->seb_nb);
	if (rc < 0) {
		pr_err("Couldn't unregister with slate event bridge, rc=%d\n", rc);
		return rc;
	}

	mutex_destroy(&the_bms->data_lock);
	mutex_destroy(&the_bms->tx_lock);
	mutex_destroy(&the_bms->rx_lock);

	the_bms = NULL;

	return rc;
}

int remote_bms_resume(void)
{
	if (!the_bms)
		return -ENODEV;

	if (is_debug_batt_id(the_bms))
		return 0;

	schedule_delayed_work(&the_bms->periodic_fg_work, 0);

	return 0;
}

int remote_bms_suspend(void)
{
	if (!the_bms)
		return -ENODEV;

	cancel_delayed_work(&the_bms->periodic_fg_work);
	cancel_work_sync(&the_bms->psy_status_change_work);
	cancel_work_sync(&the_bms->rx_data_work);

	return 0;
}
