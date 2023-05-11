/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/extcon.h>
#include <linux/mfd/max77729-private.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec/maxim/max77729-muic.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>
#include <linux/usb/typec/maxim/max77729_alternate.h>


extern struct max77729_usbc_platform_data *g_usbc_data;

static void set_pd_active(struct max77729_usbc_platform_data *usbc_data, int pd_active)
{
	//struct max77729_pd_data *pd_data = usbc_data->pd_data;
	int rc = 0;
	union power_supply_propval val = {0,};

	pr_info("%s : set_pd_active %d\n", __func__, pd_active);

	usbc_data->pd_active = pd_active;
	if (!usbc_data->usb_psy)
		usbc_data->usb_psy = power_supply_get_by_name("usb");

	if (usbc_data->usb_psy) {
		val.intval = pd_active;
		rc = power_supply_set_property(usbc_data->usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		if (rc < 0)
			pr_err("Couldn't read USB Present status, rc=%d\n", rc);

          //power_supply_changed(usbc_data->usb_psy);
	}
}


static void max77729_switch_path(struct max77729_muic_data *muic_data,
	u8 reg_val)
{
	struct max77729_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;

	pr_info("%s value(0x%x)\n", __func__, reg_val);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x06;
	write_data.write_length = 1;
	write_data.write_data[0] = reg_val;
	write_data.read_length = 0;

	max77729_usbc_opcode_write(usbc_pdata, &write_data);
}

int com_to_usb_ap(struct max77729_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s\n", __func__);

	reg_val = COM_USB;

	/* write command - switch */
	max77729_switch_path(muic_data, reg_val);

	return ret;
}

static void max77729_process_pd(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;

	if (pd_data->cc_sbu_short) {
		pd_data->pd_noti.sink_status.available_pdo_num = 1;
		pd_data->pd_noti.sink_status.power_list[1].max_current =
			pd_data->pd_noti.sink_status.power_list[1].max_current > 1800 ?
			1800 : pd_data->pd_noti.sink_status.power_list[1].max_current;
		pd_data->pd_noti.sink_status.has_apdo = false;
	}

	/* set_pd_active(usbc_data, pd_data->pd_noti.sink_status.has_apdo ? 2 : 1); */
	pr_info("%s : current_pdo_num(%d), available_pdo_num(%d), has_apdo(%d)\n", __func__,
		pd_data->pd_noti.sink_status.current_pdo_num, pd_data->pd_noti.sink_status.available_pdo_num, pd_data->pd_noti.sink_status.has_apdo);
}

void max77729_select_pdo(int num)
{
	struct max77729_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;
	u8 temp;

	init_usbc_cmd_data(&value);
	pr_info("%s : NUM(%d)\n", __func__, num);

	temp = num;

	pd_data->pd_noti.sink_status.selected_pdo_num = num;

	if (pd_data->pd_noti.event != PDIC_NOTIFY_EVENT_PD_SINK)
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	/* if (pd_data->pd_noti.sink_status.current_pdo_num == pd_data->pd_noti.sink_status.selected_pdo_num) { */
		/* max77729_process_pd(g_usbc_data); */
	/* } else { */
		g_usbc_data->pn_flag = false;
		value.opcode = OPCODE_SRCCAP_REQUEST;
		value.write_data[0] = temp;
		value.write_length = 1;
		value.read_length = 1;
		max77729_usbc_opcode_write(g_usbc_data, &value);
	/* } */

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) NUM(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length, num);
}

void max77729_response_pdo_request(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];

	pr_info("%s: %s (0x%02X)\n", __func__, result ? "Error," : "Sent,", result);

	switch (result) {
		case 0x00:
			pr_info("%s: Sent PDO Request Message to Port Partner(0x%02X)\n", __func__, result);
			break;
		case 0xFE:
			pr_info("%s: Error, SinkTxNg(0x%02X)\n", __func__, result);
			break;
		case 0xFF:
			pr_info("%s: Error, Not in SNK Ready State(0x%02X)\n", __func__, result);
			break;
		default:
			break;
	}

	/* retry if the state of sink is not stable yet */
	if (result == 0xFE || result == 0xFF) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		queue_delayed_work(usbc_data->pd_data->wqueue, &usbc_data->pd_data->retry_work, 0);
	}
}

void max77729_set_enable_pps(bool enable, int ppsVol, int ppsCur)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SET_PPS;
	if (enable) {
		value.write_data[0] = 0x1; //PPS_ON On
		value.write_data[1] = (ppsVol / 20) & 0xFF; //Default Output Voltage (Low), 20mV
		value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; //Default Output Voltage (High), 20mV
		value.write_data[3] = (ppsCur / 50) & 0x7F; //Default Operating Current, 50mA
		value.write_length = 4;
		value.read_length = 1;
		pr_info("%s : PPS_On (Vol:%dmV, Cur:%dmA)\n", __func__, ppsVol, ppsCur);
	} else {
		g_usbc_data->pd_data->bPPS_on = false;
		value.write_data[0] = 0x0; //PPS_ON Off
		value.write_length = 1;
		value.read_length = 1;
		pr_info("%s : PPS_Off\n", __func__);
	}
	max77729_usbc_opcode_write(g_usbc_data, &value);
}

void max77729_response_set_pps(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];

	if (result & 0x01)
		usbc_data->pd_data->bPPS_on = true;
	else
		usbc_data->pd_data->bPPS_on = false;

	pr_info("%s : PPS_%s (0x%02X)\n",
		__func__, usbc_data->pd_data->bPPS_on ? "On" : "Off", result);
}

void max77729_response_apdo_request(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data)
{
	u8 result = data[1];
	u8 status[5];
	u8 vbvolt;

	pr_info("%s: %s (0x%02X)\n", __func__, result ? "Error," : "Sent,", result);

	switch (result) {
	case 0x00:
		pr_info("%s: Sent APDO Request Message to Port Partner(0x%02X)\n", __func__, result);
		break;
	case 0x01:
		pr_info("%s: Error, Invalid APDO position(0x%02X)\n", __func__, result);
		break;
	case 0x02:
		pr_info("%s: Error, Invalid Output Voltage(0x%02X)\n", __func__, result);
		break;
	case 0x03:
		pr_info("%s: Error, Invalid Operating Current(0x%02X)\n", __func__, result);
		break;
	case 0x04:
		pr_info("%s: Error, PPS Function Off(0x%02X)\n", __func__, result);
		break;
	case 0x05:
		pr_info("%s: Error, Not in SNK Ready State(0x%02X)\n", __func__, result);
		break;
	case 0x06:
		pr_info("%s: Error, PD2.0 Contract(0x%02X)\n", __func__, result);
		break;
	case 0x07:
		pr_info("%s: Error, SinkTxNg(0x%02X)\n", __func__, result);
		break;
	default:
		break;
	}

	max77729_bulk_read(usbc_data->muic, MAX77729_USBC_REG_USBC_STATUS1, 5, status);
	vbvolt = (status[2] & BIT_VBUSDet) >> BC_STATUS_VBUSDET_SHIFT;
	if (vbvolt != 0x01)
		pr_info("%s: Error, VBUS isn't above 5V(0x%02X)\n", __func__, vbvolt);

	/* retry if the state of sink is not stable yet */
	if ((result == 0x05 || result == 0x07) && vbvolt == 0x1) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		queue_delayed_work(usbc_data->pd_data->wqueue, &usbc_data->pd_data->retry_work, 0);
	}
}

int max77729_select_pps(int num, int ppsVol, int ppsCur)
{
	struct max77729_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;

	if(!g_usbc_data->pd_active){
		pr_info("%s: pd charger remove.\n", __func__);
		return -EINVAL;
	}

	/* [dchg] TODO: check more below option */
	if (num > pd_data->pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		return -EINVAL;
	}

	if (!pd_data->pd_noti.sink_status.power_list[num].apdo) {
		pr_info("%s: request pdo num(%d) is not apdo.\n", __func__, num);
		return -EINVAL;
	} else
		pd_data->pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_data->pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_data->pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_data->pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_data->pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_data->pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_data->pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_data->pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_data->pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_data->pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_data->pd_noti.sink_status.pps_voltage = ppsVol;
	pd_data->pd_noti.sink_status.pps_current = ppsCur;

	pr_err(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_data->pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	if (!pd_data->bPPS_on)
		max77729_set_enable_pps(true, 5000, 1000); /* request as default 5V when enable first */

	init_usbc_cmd_data(&value);

	g_usbc_data->pn_flag = false;
	value.opcode = OPCODE_APDO_SRCCAP_REQUEST;
	value.write_data[0] = (num & 0xFF); /* APDO Position */
	value.write_data[1] = (ppsVol / 20) & 0xFF; /* Output Voltage(Low) */
	value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; /* Output Voltage(High) */
	value.write_data[3] = (ppsCur / 50) & 0x7F; /* Operating Current */
	value.write_length = 4;
	value.read_length = 1; /* Result */
	max77729_usbc_opcode_write(g_usbc_data, &value);

/* [dchg] TODO: add return value */
	return 0;
}

void max77729_pd_retry_work(struct work_struct *work)
{
	struct max77729_pd_data *pd_data = g_usbc_data->pd_data;
	usbc_cmd_data value;
	u8 num;

	if (pd_data->pd_noti.event == PDIC_NOTIFY_EVENT_DETACH)
		return;

	init_usbc_cmd_data(&value);
	num = pd_data->pd_noti.sink_status.selected_pdo_num;
	pr_info("%s : latest selected_pdo_num(%d)\n", __func__, num);
	g_usbc_data->pn_flag = false;

	if (pd_data->pd_noti.sink_status.power_list[num].apdo) {
		value.opcode = OPCODE_APDO_SRCCAP_REQUEST;
		value.write_data[0] = (num & 0xFF); /* APDO Position */
		value.write_data[1] = (pd_data->pd_noti.sink_status.pps_voltage / 20) & 0xFF; /* Output Voltage(Low) */
		value.write_data[2] = ((pd_data->pd_noti.sink_status.pps_voltage / 20) >> 8) & 0xFF; /* Output Voltage(High) */
		value.write_data[3] = (pd_data->pd_noti.sink_status.pps_current / 50) & 0x7F; /* Operating Current */
		value.write_length = 4;
		value.read_length = 1; /* Result */
		max77729_usbc_opcode_write(g_usbc_data, &value);
	} else {
		value.opcode = OPCODE_SRCCAP_REQUEST;
		value.write_data[0] = num;
		value.write_length = 1;
		value.read_length = 1;
		max77729_usbc_opcode_write(g_usbc_data, &value);
	}

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) NUM(%d)\n",
			__func__, value.opcode, value.write_length, value.read_length, num);
}

void max77729_usbc_icurr(u8 curr)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CHGIN_ILIM_W;
	value.write_data[0] = curr;
	value.write_length = 1;
	value.read_length = 0;
	max77729_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) USBC_ILIM(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, curr);

}
EXPORT_SYMBOL(max77729_usbc_icurr);


void max77729_set_fw_noautoibus(int enable)
{
	usbc_cmd_data value;
	u8 op_data = 0x00;

	switch (enable) {
	case MAX77729_AUTOIBUS_FW_AT_OFF:
		op_data = 0x03; /* usbc fw off & auto off(manual on) */
		break;
	case MAX77729_AUTOIBUS_FW_OFF:
		op_data = 0x02; /* usbc fw off & auto on(manual off) */
		break;
	case MAX77729_AUTOIBUS_AT_OFF:
		op_data = 0x01; /* usbc fw on & auto off(manual on) */
		break;
	case MAX77729_AUTOIBUS_ON:
	default:
		op_data = 0x00; /* usbc fw on & auto on(manual off) */
		break;
	}

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_FW_AUTOIBUS;
	value.write_data[0] = op_data;
	value.write_length = 1;
	value.read_length = 0;
	max77729_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) AUTOIBUS(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, op_data);
}
EXPORT_SYMBOL(max77729_set_fw_noautoibus);


static void max77729_set_snkcap(struct max77729_usbc_platform_data *usbc_data)
{
	struct device_node *np = NULL;
	u8 *snkcap_data;
	int len = 0, ret = 0;
	usbc_cmd_data value;
	int i;
	char *str = NULL;

	np = of_find_compatible_node(NULL, NULL, "maxim,max77729");
	if (!np) {
		pr_info("%s : np is NULL\n", __func__);
		return;
	}

	if (!of_get_property(np, "max77729,snkcap_data", &len)) {
		pr_info("%s : max77729,snkcap_data is Empty !!\n", __func__);
		return;
	}

	len = len / sizeof(u8);
	snkcap_data = kzalloc(sizeof(*snkcap_data) * len, GFP_KERNEL);
	if (!snkcap_data) {
		pr_err("%s : Failed to allocate memory (snkcap_data)\n", __func__);
		return;
	}

	ret = of_property_read_u8_array(np, "max77729,snkcap_data",
		snkcap_data, len);
	if (ret) {
		pr_info("%s : max77729,snkcap_data is Empty\n", __func__);
		goto err_free_snkcap_data;
	}

	init_usbc_cmd_data(&value);

	if (len)
		memcpy(value.write_data, snkcap_data, len);

	str = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (str) {
		for (i = 0; i < len; i++)
			sprintf(str + strlen(str), "0x%02x ", value.write_data[i]);
		pr_info("%s: SNK_CAP : %s\n", __func__, str);
	}

	value.opcode = OPCODE_SET_SNKCAP;
	value.write_length = len;
	value.read_length = 0;
	max77729_usbc_opcode_write(usbc_data, &value);

	kfree(str);
err_free_snkcap_data:
	kfree(snkcap_data);
}

void max77729_vbus_turn_on_ctrl(struct max77729_usbc_platform_data *usbc_data, bool enable, bool swaped)
{
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0;
	int count = 5;

	pr_err("%s : enable=%d\n", __func__, enable);

	while (count) {
		psy_otg = power_supply_get_by_name("otg");
		if (psy_otg) {
			val.intval = enable;
			ret = psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
			if (ret == -ENODEV) {
				pr_err("%s: fail to set power_suppy ONLINE property %d) retry (%d)\n",__func__, ret, count);
				count--;
			} else {
				if (ret) {
					pr_err("%s: fail to set power_suppy ONLINE property(%d) \n",__func__, ret);
				} else {
					pr_info("otg accessory power = %d\n", on);
				}
				break;
			}
		} else {
			pr_err("%s: fail to get psy battery\n", __func__);
			count--;
			msleep(200);
		}
	}
}

void max77729_pdo_list(struct max77729_usbc_platform_data *usbc_data, unsigned char *data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	u8 temp = 0x00;
	int i;
	bool do_power_nego = false;
	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	temp = (data[1] >> 5);

	if (temp > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, temp, MAX_PDO_NUM);
		temp = MAX_PDO_NUM;
	}

	pd_data->pd_noti.sink_status.available_pdo_num = temp;
	pr_info("%s : Temp[0x%02x] Data[0x%02x] available_pdo_num[%d]\n",
		__func__, temp, data[1], pd_data->pd_noti.sink_status.available_pdo_num);

	for (i = 0; i < temp; i++) {
		u32 pdo_temp;
		int max_current = 0, max_voltage = 0;

		pdo_temp = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		pr_info("%s : PDO[%d] = 0x%x\n", __func__, i, pdo_temp);

		max_current = (0x3FF & pdo_temp);
		max_voltage = (0x3FF & (pdo_temp >> 10));

		if (!(do_power_nego) &&
			(pd_data->pd_noti.sink_status.power_list[i + 1].max_current != max_current * UNIT_FOR_CURRENT ||
			pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage != max_voltage * UNIT_FOR_VOLTAGE))
			do_power_nego = true;

		pd_data->pd_noti.sink_status.power_list[i + 1].max_current = max_current * UNIT_FOR_CURRENT;
		pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage = max_voltage * UNIT_FOR_VOLTAGE;

		pr_info("%s : PDO_Num[%d] MAX_CURR(%d) MAX_VOLT(%d), AVAILABLE_PDO_Num(%d)\n", __func__,
				i, pd_data->pd_noti.sink_status.power_list[i + 1].max_current,
				pd_data->pd_noti.sink_status.power_list[i + 1].max_voltage,
				pd_data->pd_noti.sink_status.available_pdo_num);
	}

	if (usbc_data->pd_data->pdo_list && do_power_nego) {
		pr_info("%s : PDO list is changed, so power negotiation is need\n",
			__func__);
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	}

	if (pd_data->pd_noti.sink_status.current_pdo_num != pd_data->pd_noti.sink_status.selected_pdo_num) {
		if (pd_data->pd_noti.sink_status.selected_pdo_num == 0)
			pr_info("%s : PDO is not selected yet by default\n", __func__);
	}

	usbc_data->pd_data->pdo_list = true;
	max77729_process_pd(usbc_data);
	/* if (do_power_nego) */
		/* set_pd_active(usbc_data, pd_data->pd_noti.sink_status.has_apdo ? 2 : 1); */
}

void max77729_current_pdo(struct max77729_usbc_platform_data *usbc_data, unsigned char *data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	u8 sel_pdo_pos = 0x00, num_of_pdo = 0x00;
	int i, available_pdo_num = 0;
	bool do_power_nego = false;
	U_SEC_PDO_OBJECT pdo_obj;
	POWER_LIST* pPower_list;
	POWER_LIST prev_power_list;

	if (!pd_data->pd_noti.sink_status.available_pdo_num)
		do_power_nego = true;

	sel_pdo_pos = ((data[1] >> 3) & 0x07);
	pd_data->pd_noti.sink_status.current_pdo_num = sel_pdo_pos;

	num_of_pdo = (data[1] & 0x07);
	if (num_of_pdo > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, num_of_pdo, MAX_PDO_NUM);
		num_of_pdo = MAX_PDO_NUM;
	}

	pd_data->pd_noti.sink_status.has_apdo = false;

	for (i = 0; i < num_of_pdo; ++i) {
		pPower_list = &pd_data->pd_noti.sink_status.power_list[available_pdo_num + 1];

		pdo_obj.data = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		if (!do_power_nego)
			prev_power_list = pd_data->pd_noti.sink_status.power_list[available_pdo_num + 1];

		switch (pdo_obj.BITS_supply.type) {
		case PDO_TYPE_FIXED:
			pPower_list->apdo = false;
			pPower_list->max_voltage = pdo_obj.BITS_pdo_fixed.voltage * UNIT_FOR_VOLTAGE;
			pPower_list->min_voltage = 0;
			pPower_list->max_current = pdo_obj.BITS_pdo_fixed.max_current * UNIT_FOR_CURRENT;
			pPower_list->comm_capable = pdo_obj.BITS_pdo_fixed.usb_communications_capable;
			pPower_list->suspend = pdo_obj.BITS_pdo_fixed.usb_suspend_supported;
			if (pPower_list->max_voltage > AVAILABLE_VOLTAGE)
				pPower_list->accept = false;
			else
				pPower_list->accept = true;
			available_pdo_num++;
 			break;
		case PDO_TYPE_APDO:
			pd_data->pd_noti.sink_status.has_apdo = true;
			available_pdo_num++;
			pPower_list->apdo = true;
			pPower_list->max_voltage = pdo_obj.BITS_pdo_programmable.max_voltage * UNIT_FOR_APDO_VOLTAGE;
			pPower_list->min_voltage = pdo_obj.BITS_pdo_programmable.min_voltage * UNIT_FOR_APDO_VOLTAGE;
			pPower_list->max_current = pdo_obj.BITS_pdo_programmable.max_current * UNIT_FOR_APDO_CURRENT;
			pPower_list->accept = true;
			break;
		case PDO_TYPE_BATTERY:
		case PDO_TYPE_VARIABLE:
		default:
			break;
		}

		if (!(do_power_nego) &&
			(pPower_list->max_current != prev_power_list.max_current ||
			pPower_list->max_voltage != prev_power_list.max_voltage ||
			pPower_list->min_voltage != prev_power_list.min_voltage))
			do_power_nego = true;
	}


	if (!do_power_nego && (pd_data->pd_noti.sink_status.available_pdo_num != available_pdo_num))
		do_power_nego = true;

	pd_data->pd_noti.sink_status.available_pdo_num = available_pdo_num;
	pr_info("%s : current_pdo_num(%d), available_pdo_num(%d/%d), comm(%d), suspend(%d)\n", __func__,
		pd_data->pd_noti.sink_status.current_pdo_num,
		pd_data->pd_noti.sink_status.available_pdo_num, num_of_pdo,
		pd_data->pd_noti.sink_status.power_list[sel_pdo_pos].comm_capable,
		pd_data->pd_noti.sink_status.power_list[sel_pdo_pos].suspend);

	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	if (usbc_data->pd_data->pdo_list && do_power_nego) {
		pr_info("%s : PDO list is changed, so power negotiation is need\n", __func__);
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	}

	if (pd_data->pd_noti.sink_status.current_pdo_num != pd_data->pd_noti.sink_status.selected_pdo_num) {
		if (pd_data->pd_noti.sink_status.selected_pdo_num == 0)
			pr_info("%s : PDO is not selected yet by default\n", __func__);
	}

	if (do_power_nego || pd_data->pd_noti.sink_status.selected_pdo_num == 0) {
		for (i = 0; i < num_of_pdo; ++i) {
			pdo_obj.data = (data[2 + (i * 4)]
				| (data[3 + (i * 4)] << 8)
				| (data[4 + (i * 4)] << 16)
				| (data[5 + (i * 4)] << 24));
			pr_info("%s : PDO[%d] = 0x%08X\n", __func__, i + 1, pdo_obj.data);
			usbc_data->received_pdos[i] = pdo_obj.data;
		}

		for (i = 0; i < pd_data->pd_noti.sink_status.available_pdo_num; ++i) {
			pPower_list = &pd_data->pd_noti.sink_status.power_list[i + 1];

			pr_info("%s : PDO[%d,%s,%s] max_vol(%dmV),min_vol(%dmV),max_cur(%dmA)\n",
				__func__, i + 1,
				pPower_list->apdo ? "APDO" : "FIXED", pPower_list->accept ? "O" : "X",
				pPower_list->max_voltage, pPower_list->min_voltage, pPower_list->max_current);
		}
	}

	usbc_data->pd_data->pdo_list = true;
	max77729_process_pd(usbc_data);

	 if (do_power_nego)
		set_pd_active(usbc_data, pd_data->pd_noti.sink_status.has_apdo ? 2 : 1);
}

POWER_LIST* usbpd_fetch_pdo(void)
{
	struct max77729_pd_data *pd_data = g_usbc_data->pd_data;
	POWER_LIST* powerlist =NULL;

	if(g_usbc_data->pd_active){
		powerlist = (void*)&pd_data->pd_noti.sink_status.power_list[1];
	}

	return powerlist;
}
EXPORT_SYMBOL(usbpd_fetch_pdo);


int usbpd_select_pdo_maxim(int pdo, int mv, int ma)
{
	return max77729_select_pps(pdo + 1, mv, ma);
}
EXPORT_SYMBOL(usbpd_select_pdo_maxim);

void max77729_detach_pd(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;

	pr_info("%s : Detach PD CHARGER\n", __func__);

	if (pd_data->pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		cancel_delayed_work(&usbc_data->pd_data->retry_work);
		if (pd_data->pd_noti.sink_status.available_pdo_num)
			memset(pd_data->pd_noti.sink_status.power_list, 0, (sizeof(POWER_LIST) * (MAX_PDO_NUM + 1)));
		pd_data->pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.available_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		pd_data->pd_noti.sink_status.pps_voltage = 0;
		pd_data->pd_noti.sink_status.pps_current = 0;
 		pd_data->pd_noti.sink_status.has_apdo = false;
		max77729_set_enable_pps(false, 0, 0);
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		set_pd_active(usbc_data, 0);
		usbc_data->uvdm_state = 0;
	}
}

static void max77729_notify_prswap(struct max77729_usbc_platform_data *usbc_data, u8 pd_msg)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;

	pr_info("%s : PR SWAP pd_msg [%x]\n", __func__, pd_msg);

	switch(pd_msg) {
	case PRSWAP_SNKTOSWAP:
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.available_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		break;
	default:
		break;
	}
}

void max77729_check_pdo(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CURRENT_SRCCAP;
	value.write_length = 0x0;
	value.read_length = 31;
	max77729_usbc_opcode_write(usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

void max77729_notify_rp_current_level(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	unsigned int rp_currentlvl;
	union power_supply_propval val = {0,};

	switch (usbc_data->cc_data->ccistat) {
	case CCI_500mA:
		rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;
		val.intval = 500;
		break;
	case CCI_1_5A:
		rp_currentlvl = RP_CURRENT_LEVEL2;
		val.intval = 2000;
		break;
	case CCI_3_0A:
		rp_currentlvl = RP_CURRENT_LEVEL3;
		val.intval = 3000;
		break;
	default:
		rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		val.intval = 500;
		break;
	}

	if (usbc_data->plug_attach_done && !usbc_data->pd_data->psrdy_received &&
		usbc_data->cc_data->current_pr == SNK &&
		usbc_data->pd_state == max77729_State_PE_SNK_Wait_for_Capabilities &&
		rp_currentlvl != pd_data->rp_currentlvl &&
		rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT) {
		pd_data->rp_currentlvl = rp_currentlvl;
		/* psy_do_property("bbc", set, POWER_SUPPLY_PROP_CURRENT_MAX, val); */
		/* psy_do_property("bbc", set, POWER_SUPPLY_PROP_CURRENT_NOW, val); */

		pr_debug("%s : rp_currentlvl(%d)\n", __func__, pd_data->rp_currentlvl);
	}
	/* val.intval = SEC_BAT_CHG_MODE_CHARGING; */
	/* psy_do_property("bbc", set, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val); */
	/* psy_do_property("bms", set, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val); */

}

static int max77729_get_chg_info(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	if (usbc_data->pd_data->sent_chg_info)
		return 0;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SEND_GET_REQUEST;
	value.write_data[0] = OPCODE_GET_SRC_CAP_EXT;
	value.write_data[1] = 0; /*  */
	value.write_data[2] = 0; /*  */
	value.write_length = 3;
	value.read_length = 1; /* Result */
	max77729_usbc_opcode_write(g_usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);

	usbc_data->pd_data->sent_chg_info = true;
	return 0;
}

static void clear_chg_info(struct max77729_usbc_platform_data *usbc_data)
{
	SEC_PD_SINK_STATUS *snk_sts = &usbc_data->pd_data->pd_noti.sink_status;

	usbc_data->pd_data->sent_chg_info = false;
	snk_sts->pid = 0;
	snk_sts->vid = 0;
	snk_sts->xid = 0;
}

static void max77729_pd_check_pdmsg(struct max77729_usbc_platform_data *usbc_data, u8 pd_msg)
{
#if 0 //Brandon it should apply based on target platform
	struct power_supply *psy_charger;
	union power_supply_propval val;
#endif
	usbc_cmd_data value;
	/*int dr_swap, pr_swap, vcon_swap = 0; u8 value[2], rc = 0;*/
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	VDM_MSG_IRQ_State.DATA = 0x0;
	init_usbc_cmd_data(&value);
	msg_maxim(" pd_msg [%x]", pd_msg);

	switch (pd_msg) {
	case Nothing_happened:
		clear_chg_info(usbc_data);
		break;
	case Sink_PD_PSRdy_received:
		max77729_get_chg_info(usbc_data);
		/* currently, do nothing
		 * calling max77729_check_pdo() has been moved to max77729_psrdy_irq()
		 * for specific PD charger issue
		 */
		break;
	case Sink_PD_Error_Recovery:
		break;
	case Sink_PD_SenderResponseTimer_Timeout:
		msg_maxim("Sink_PD_SenderResponseTimer_Timeout received.");
	/*	queue_work(usbc_data->op_send_queue, &usbc_data->op_send_work); */
		break;
	case Source_PD_PSRdy_Sent:
		break;
	case Source_PD_Error_Recovery:
		break;
	case Source_PD_SenderResponseTimer_Timeout:
		max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
		schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(800));
		break;
	case PD_DR_Swap_Request_Received:
		msg_maxim("DR_SWAP received.");
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 1);
#endif
		/* currently, do nothing
		 * calling max77729_check_pdo() has been moved to max77729_psrdy_irq()
		 * for specific PD charger issue
		 */
		break;
	case PD_PR_Swap_Request_Received:
		msg_maxim("PR_SWAP received.");
		break;
	case PD_VCONN_Swap_Request_Received:
		msg_maxim("VCONN_SWAP received.");
		break;
	case Received_PD_Message_in_illegal_state:
		break;
	case Samsung_Accessory_is_attached:
		break;
	case VDM_Attention_message_Received:
		break;
	case Sink_PD_Disabled:
#if 0
		/* to do */
		/* AFC HV */
		value[0] = 0x20;
		rc = max77729_ccpd_write_command(chip, value, 1);
		if (rc > 0)
			pr_err("failed to send command\n");
#endif
		break;
	case Source_PD_Disabled:
		break;
	case Prswap_Snktosrc_Sent:
		usbc_data->pd_pr_swap = cc_SOURCE;
		break;
	case Prswap_Srctosnk_Sent:
		usbc_data->pd_pr_swap = cc_SINK;
		break;
	case HARDRESET_RECEIVED:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		}
		break;
	case HARDRESET_SENT:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		}

		break;
	case Get_Vbus_turn_on:
		break;
	case Get_Vbus_turn_off:
		max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
		break;
	case PRSWAP_SRCTOSWAP:
		max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
		msg_maxim("PRSWAP_SRCTOSWAP : [%x]", pd_msg);
		break;
	case PRSWAP_SWAPTOSNK:
		max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
		msg_maxim("PRSWAP_SWAPTOSNK : [%x]", pd_msg);
		break;
	case PRSWAP_SNKTOSWAP:
		msg_maxim("PRSWAP_SNKTOSWAP : [%x]", pd_msg);
		max77729_notify_prswap(usbc_data, PRSWAP_SNKTOSWAP);
#if 0 //Brandon it should apply based on target platform
		/* CHGINSEL disable */
		psy_charger = power_supply_get_by_name("max77729-charger");
		if (psy_charger) {
			val.intval = 0;
			psy_do_property("max77729-charger", set, POWER_SUPPLY_EXT_PROP_CHGINSEL, val);
		} else {
			pr_err("%s: Fail to get psy charger\n", __func__);
		}
#endif
		break;
	case PRSWAP_SWAPTOSRC:
		max77729_vbus_turn_on_ctrl(usbc_data, ON, false);
		msg_maxim("PRSWAP_SNKTOSRC : [%x]", pd_msg);
		break;
	case SRC_CAP_RECEIVED:
		msg_maxim("src cap flag : [%x]", pd_msg);
		usbc_data->src_cap_flag = 1;
		break;
	case Status_Received:
		value.opcode = OPCODE_READ_MESSAGE;
		value.write_data[0] = 0x02;
		value.write_length = 1;
		value.read_length = 32;
		max77729_usbc_opcode_write(usbc_data, &value);
		msg_maxim("@TA_ALERT: Status Receviced : [%x]", pd_msg);
		break;
	case Alert_Message:
		value.opcode = OPCODE_READ_MESSAGE;
		value.write_data[0] = 0x01;
		value.write_length = 1;
		value.read_length = 32;
		max77729_usbc_opcode_write(usbc_data, &value);
		msg_maxim("@TA_ALERT: Alert Message : [%x]", pd_msg);
		break;
	default:
		break;
	}
}

static irqreturn_t max77729_pdmsg_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	u8 pdmsg = 0;

	max77729_read_reg(usbc_data->muic, REG_PD_STATUS0, &pd_data->pd_status0);
	pdmsg = pd_data->pd_status0;
	msg_maxim("IRQ(%d)_IN pdmsg: %02x", irq, pdmsg);
	max77729_pd_check_pdmsg(usbc_data, pdmsg);
	pd_data->pdsmg = pdmsg;
	msg_maxim("IRQ(%d)_OUT", irq);

	return IRQ_HANDLED;
}


extern void max77729_send_new_srccap(struct max77729_usbc_platform_data *usbpd_data, int idx);
static irqreturn_t max77729_psrdy_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	u8 psrdy_received = 0;
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	msg_maxim("IN");
	max77729_read_reg(usbc_data->muic, REG_PD_STATUS1, &usbc_data->pd_status1);
	psrdy_received = (usbc_data->pd_status1 & BIT_PD_PSRDY)
			>> FFS(BIT_PD_PSRDY);

	if (psrdy_received && !usbc_data->pd_support
			&& usbc_data->pd_data->cc_status != CC_NO_CONN)
		usbc_data->pd_support = true;

	if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_PR &&
		usbc_data->pd_support) {
		msg_maxim("typec_reverse_completion");
		usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		complete(&usbc_data->typec_reverse_completion);
	}
	msg_maxim("psrdy_received=%d, usbc_data->pd_support=%d, cc_status=%d",
		psrdy_received, usbc_data->pd_support, usbc_data->pd_data->cc_status);

	mode = max77729_get_pd_support(usbc_data);

	if (usbc_data->pd_data->cc_status == CC_SNK && psrdy_received) {
		if (!usbc_data->sink_Ready || !usbc_data->pd_data->pd_noti.sink_status.has_apdo)
			max77729_check_pdo(usbc_data);
		usbc_data->pd_data->psrdy_received = true;
		usbc_data->sink_Ready = true;
		msg_maxim("Sink SNK_Ready");
	}

	if (usbc_data->pd_data->cc_status == CC_SRC && psrdy_received) {
		msg_maxim("Source SRC_Ready");
		if (!usbc_data->source_Ready){
			union power_supply_propval val = {0,};
			psy_do_property("bms", get,
					POWER_SUPPLY_PROP_CAPACITY, val);
			if (val.intval > 5)
				max77729_send_new_srccap(usbc_data, 0);
		}
		usbc_data->source_Ready = true;
	}

	if (psrdy_received && usbc_data->pd_data->cc_status != CC_NO_CONN) {
		usbc_data->pn_flag = true;
	}

	msg_maxim("OUT");
	return IRQ_HANDLED;
}

bool max77729_sec_pps_control(int en)
{
#if 0 //Brandon it should apply based on target platform

	struct max77729_usbc_platform_data *pusbpd = g_usbc_data;

	union power_supply_propval val = {0,};

	msg_maxim(": %d", en);

	val.intval = en; /* 0: stop pps, 1: start pps */
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_DIRECT_SEND_UVDM, val);
	if (!en && !pusbpd->pn_flag) {
		reinit_completion(&pusbpd->psrdy_wait);
		if (!wait_for_completion_timeout(&pusbpd->psrdy_wait, msecs_to_jiffies(1000))) {
			msg_maxim("PSRDY COMPLETION TIMEOUT");
			return false;
		}
	}
#endif
	return true;
}

void stop_usb_host(void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	extcon_set_state_sync(usbc_data->extcon, EXTCON_USB_HOST, 0);
}

void start_usb_host(void *data, bool ss)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	union extcon_property_value val;
	/* int ret = 0; */

	val.intval = (usbc_data->cc_pin_status == CC2_ACTVIE);
	extcon_set_property(usbc_data->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_TYPEC_POLARITY, val);

	val.intval = ss;
	extcon_set_property(usbc_data->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_SS, val);

	extcon_set_state_sync(usbc_data->extcon, EXTCON_USB_HOST, 1);

	/* blocks until USB host is completely started */
	/* ret = extcon_blocking_sync(usbc_data->extcon, EXTCON_USB_HOST, 1); */
	/* if (ret) { */
		/* usbpd_err(&pd->dev, "err(%d) starting host", ret); */
		/* return; */
	/* } */
}

void stop_usb_peripheral(void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	extcon_set_state_sync(usbc_data->extcon, EXTCON_USB, 0);
}

void start_usb_peripheral(void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	union extcon_property_value val;

	val.intval = (usbc_data->cc_pin_status == CC2_ACTVIE);

	extcon_set_property(usbc_data->extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_POLARITY, val);

	val.intval = 1;
	extcon_set_property(usbc_data->extcon, EXTCON_USB, EXTCON_PROP_USB_SS, val);

	val.intval = usbc_data->cc_data->ccistat > CCI_500mA ? 1 : 0;
	extcon_set_property(usbc_data->extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT, val);

	extcon_set_state_sync(usbc_data->extcon, EXTCON_USB, 1);
}

void max77729_typec_role(void *data, int datarole)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	struct typec_partner_desc desc;
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;

	if (usbpd_data->partner == NULL) {
		msg_maxim("typec_register_partner, typec_power_role=%d typec_data_role=%d",
				usbpd_data->typec_power_role,usbpd_data->typec_data_role);
		if (datarole == UFP) {
			mode = max77729_get_pd_support(usbpd_data);
			typec_set_pwr_opmode(usbpd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			usbpd_data->typec_data_role = TYPEC_DEVICE;
			typec_set_pwr_role(usbpd_data->port, usbpd_data->typec_power_role);
			typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			usbpd_data->partner = typec_register_partner(usbpd_data->port, &desc);
		} else if (datarole == DFP) {
			mode = max77729_get_pd_support(usbpd_data);
			typec_set_pwr_opmode(usbpd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			usbpd_data->typec_data_role = TYPEC_HOST;
			typec_set_pwr_role(usbpd_data->port, usbpd_data->typec_power_role);
			typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			usbpd_data->partner = typec_register_partner(usbpd_data->port, &desc);
		} else
			msg_maxim("detach case");
	} else {
		msg_maxim("data_role changed, typec_power_role=%d typec_data_role=%d",
				usbpd_data->typec_power_role,usbpd_data->typec_data_role);
		if (datarole == UFP) {
			usbpd_data->typec_data_role = TYPEC_DEVICE;
			typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
		} else if (datarole == DFP) {
			usbpd_data->typec_data_role = TYPEC_HOST;
			typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
		} else
			msg_maxim("detach case");
	}
}

static void max77729_datarole_irq_handler(void *data, int irq)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	u8 ccstat, datarole = 0;

	max77729_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_data->pd_status1);
	datarole = (pd_data->pd_status1 & BIT_PD_DataRole)
			>> FFS(BIT_PD_DataRole);
	/* abnormal data role without setting power role */
	/* if (usbc_data->cc_data->current_pr == 0xFF) { */
		/* msg_maxim("INVALID IRQ IRQ(%d)_OUT", irq); */
		/* return; */
	/* } */

	if (irq == CCIC_IRQ_INIT_DETECT) {
		ccstat = (usbc_data->cc_data->cc_status0 & BIT_CCStat) >> FFS(BIT_CCStat);
		/* if (usbc_data->pd_data->cc_status == CC_SNK) */
		if (ccstat == cc_SINK)
			msg_maxim("initial time : SNK");
		else
			return;
	}

	switch (datarole) {
	case UFP:
			if (pd_data->current_dr != UFP) {
				stop_usb_host(data);
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = UFP;
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);

				/* if (pd_data->current_dr == UFP) { */
					/* if (usbpd_data->is_host == HOST_ON) { */
						/* msg_maxim("pd_state:%02d,	turn off host", */
								/* usbpd_data->pd_state); */
						/* usbpd_data->is_host = HOST_OFF; */
					/* } */
					/* if (usbpd_data->is_client == CLIENT_OFF) { */
						/* usbpd_data->is_client = CLIENT_ON; */
					/* } */
				/* } */

				max77729_typec_role(data, pd_data->current_dr);
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
					complete(&usbc_data->typec_reverse_completion);
				}
			}

			start_usb_peripheral(usbc_data);
			/* com_to_usb_ap(usbc_data->muic_data); */
			msg_maxim(" UFP");
			break;

	case DFP:
			if (pd_data->current_dr != DFP) {
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = DFP;
				stop_usb_peripheral(data);
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);

				max77729_typec_role(data, pd_data->current_dr);
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
					complete(&usbc_data->typec_reverse_completion);
				}


				if (usbc_data->cc_data->current_pr == SNK && !(usbc_data->send_vdm_identity)) {
					max77729_vdm_process_set_identity_req(usbc_data);
					/* msg_maxim("SEND THE IDENTITY REQUEST FROM DFP HANDLER"); */
				}
			}

			com_to_usb_ap(usbc_data->muic_data);
			start_usb_host(usbc_data, true);
			msg_maxim(" DFP");
			break;
	default:
			msg_maxim(" DATAROLE(Never Call this routine)");
			break;
	}
}

static irqreturn_t max77729_datarole_irq(int irq, void *data)
{
	//pr_err("%s: IRQ(%d)_IN\n", __func__, irq);
	max77729_datarole_irq_handler(data, irq);
	//pr_err("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

static void max77729_check_cc_sbu_short(void *data)
{
	u8 cc_status1 = 0;

	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_pd_data *pd_data = usbc_data->pd_data;

	max77729_read_reg(usbc_data->muic, REG_CC_STATUS1, &cc_status1);
	/* 0b01: CC-5V, 0b10: SBU-5V, 0b11: SBU-GND Short */
	cc_status1 = (cc_status1 & BIT_CCSBUSHORT) >> FFS(BIT_CCSBUSHORT);
	if (cc_status1)
		pd_data->cc_sbu_short = true;

	msg_maxim("%s cc_status1 : %x, cc_sbu_short : %d\n", __func__, cc_status1, pd_data->cc_sbu_short);
}

int max77729_pd_init(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_pd_data *pd_data = usbc_data->pd_data;
	int ret = 0;

	msg_maxim(" IN(%d)", pd_data->pd_noti.sink_status.rp_currentlvl);

	/* skip below codes for detecting incomplete connection cable. */
	pd_data->pd_noti.sink_status.available_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.pps_voltage = 0;
	pd_data->pd_noti.sink_status.pps_current = 0;
	pd_data->pd_noti.sink_status.has_apdo = false;
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pdo = max77729_select_pdo;
	pd_data->pd_noti.sink_status.fp_sec_pd_select_pps = max77729_select_pps;

	/* skip below codes for detecting incomplete connection cable. */
	pd_data->pdo_list = false;
	pd_data->psrdy_received = false;


	pd_data->wqueue = create_singlethread_workqueue("max77729_pd");
	if (!pd_data->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_irq;
	}

	INIT_DELAYED_WORK(&pd_data->retry_work, max77729_pd_retry_work);

	pd_data->irq_pdmsg = usbc_data->irq_base + MAX77729_PD_IRQ_PDMSG_INT;
	if (pd_data->irq_pdmsg) {
		ret = request_threaded_irq(pd_data->irq_pdmsg,
			   NULL, max77729_pdmsg_irq,
			   0,
			   "pd-pdmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_psrdy = usbc_data->irq_base + MAX77729_PD_IRQ_PS_RDY_INT;
	if (pd_data->irq_psrdy) {
		ret = request_threaded_irq(pd_data->irq_psrdy,
			   NULL, max77729_psrdy_irq,
			   0,
			   "pd-psrdy-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_datarole = usbc_data->irq_base + MAX77729_PD_IRQ_DATAROLE_INT;
	if (pd_data->irq_datarole) {
		ret = request_threaded_irq(pd_data->irq_datarole,
			   NULL, max77729_datarole_irq,
			   0,
			   "pd-datarole-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	max77729_set_fw_noautoibus(MAX77729_AUTOIBUS_AT_OFF);
	max77729_set_snkcap(usbc_data);
	/* check CC Pin state for cable attach booting scenario */
	max77729_datarole_irq_handler(usbc_data, CCIC_IRQ_INIT_DETECT);
	max77729_check_cc_sbu_short(usbc_data);
	msg_maxim(" OUT(%d)", pd_data->pd_noti.sink_status.rp_currentlvl);
	return 0;

err_irq:
	kfree(pd_data);
	return ret;
}
