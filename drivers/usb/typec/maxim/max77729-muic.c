/*
 * max77729-muic.c
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

 #define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

/* MUIC header file */
#include <linux/mfd/max77729.h>
#include <linux/mfd/max77729-private.h>
#include <linux/usb/typec/maxim/max77729.h>
#include <linux/usb/typec/maxim/max77729-muic.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>

extern unsigned int lpcharge;

extern int com_to_usb_ap(struct max77729_muic_data *muic_data);
struct max77729_muic_data *g_muic_data;

void max77729_bc12_get_vadc(u8 vbadc)
{
	switch (vbadc) {
	case 0:
		pr_info(" VBUS < 3.5V");
		break;
	case 1:
		pr_info(" 3.5V <=  VBUS < 4.5V");
		break;
	case 2:
		pr_info(" 4.5V <=  VBUS < 5.5V ");
		break;
	case 3:
		pr_info(" 5.5V <=  VBUS < 6.5V");
		break;
	case 4:
		pr_info(" 6.5V <=  VBUS < 7.5V");
		break;
	case 5:
		pr_info(" 7.5V <= VBUS < 8.5V");
		break;
	case 6:
		pr_info(" 8.5V <= VBUS < 9.5V");
		break;
	case 7:
		pr_info(" 9.5V <= VBUS < 10.5V");
		break;
	case 8:
		pr_info(" 10.5V <= VBUS < 11.5V");
		break;
	case 9:
		pr_info(" 11.5V <= VBUS < 12.5V");
		break;
	case 10:
		pr_info(" 12.5V <= VBUS < 13.5V");
		break;
	case 11:
		pr_info(" 13.5V <= VBUS < 14.5V");
		break;
	case 12:
		pr_info(" 14.5V <= VBUS < 15.5V");
		break;
	default:
		pr_info(" Reserved ");
		break;

	};
}


int max77729_bc12_set_charger(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_muic_data *muic_data = usbc_data->muic_data;
	union power_supply_propval value;
	enum power_supply_type	real_charger_type;

 	pr_err(" BIT_ChgTyp = %02Xh, BIT_PrChgTyp = %02Xh",
			muic_data->chg_type, muic_data->pr_chg_type);

	switch(muic_data->chg_type){
		case CHGTYP_NOTHING:
			muic_data->dcdtmo = 0;
			value.intval = SEC_BATTERY_CABLE_NONE;
			real_charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		case CHGTYP_CDP_T:
			value.intval = SEC_BATTERY_CABLE_USB_CDP;
			real_charger_type = POWER_SUPPLY_TYPE_USB_CDP;
			com_to_usb_ap(usbc_data->muic_data);
			break;
		case CHGTYP_USB_SDP:
			value.intval = SEC_BATTERY_CABLE_USB;
			real_charger_type = POWER_SUPPLY_TYPE_USB;
			com_to_usb_ap(usbc_data->muic_data);
			break;
		case CHGTYP_DCP:
			value.intval = SEC_BATTERY_CABLE_TA;
			real_charger_type = POWER_SUPPLY_TYPE_USB_DCP;
			break;

	}
	if (usbc_data->typec_power_role == TYPEC_SOURCE) {
		value.intval = SEC_BATTERY_CABLE_OTG;
	}

	switch(value.intval) {
		case SEC_BATTERY_CABLE_TA:
		case SEC_BATTERY_CABLE_USB:
		case SEC_BATTERY_CABLE_USB_CDP:
			psy_do_property("bbc", set, POWER_SUPPLY_PROP_STATUS, value);
			break;
	}
    /* maxim pmic */
    /* psy_do_property("bbc", set, POWER_SUPPLY_PROP_ONLINE, value); */
    /* psy_do_property("bms", set, POWER_SUPPLY_PROP_ONLINE, value); */
	/* value.intval = real_charger_type > 0 ? 1 : 0; */
    /* psy_do_property("usb", set, POWER_SUPPLY_PROP_ONLINE, value); */
	/* value.intval = real_charger_type; */
    /* psy_do_property("usb", set, POWER_SUPPLY_PROP_REAL_TYPE, value); */

	return 0;
}

static irqreturn_t max77729_vbadc_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_muic_data *muic_data = usbc_data->muic_data;
	u8 vbadc = 0;

	/* pr_debug("%s: IRQ(%d)_IN\n", __func__, irq); */
	max77729_read_reg(muic_data->i2c, MAX77729_USBC_REG_USBC_STATUS1,
		&muic_data->usbc_status1);
	vbadc = (muic_data->usbc_status1 & BIT_VBADC) >> FFS(BIT_VBADC);
	max77729_bc12_get_vadc(vbadc);
	muic_data->vbadc = vbadc;
	/* pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq); */

	return IRQ_HANDLED;
}
static irqreturn_t max77729_chgtype_irq(int irq, void *data)
{

	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_muic_data *muic_data = usbc_data->muic_data;

	/* pr_err("%s: IRQ(%d)_IN\n", __func__, irq); */

	max77729_read_reg(muic_data->i2c, REG_BC_STATUS, &muic_data->bc_status);

	muic_data->chg_type = (muic_data->bc_status & BIT_ChgTyp)
		>> FFS(BIT_ChgTyp);

	muic_data->pr_chg_type = (muic_data->bc_status & BIT_PrChgTyp)
		>> FFS(BIT_PrChgTyp);

	max77729_bc12_set_charger(usbc_data);

    if(muic_data->chg_type == CHGTYP_DCP){
        cancel_delayed_work_sync(&(muic_data->qc_work));
        //D- change the LOW after detects DCP type and 1.2sec.
        schedule_delayed_work(&(muic_data->qc_work), msecs_to_jiffies(1200));

    }else{
        cancel_delayed_work_sync(&(muic_data->qc_work));
    }


	/* pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq); */

	return IRQ_HANDLED;
}

static irqreturn_t max77729_dcdtmo_irq(int irq, void *data)
{

	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_muic_data *muic_data = usbc_data->muic_data;
	u8 dcdtmo_flag;

	/* pr_debug("%s: IRQ(%d)_IN\n", __func__, irq); */
	max77729_read_reg(muic_data->i2c, REG_BC_STATUS, &muic_data->bc_status);

	dcdtmo_flag = (muic_data->bc_status & BIT_DCDTmo)
		>> FFS(BIT_DCDTmo);

	if (dcdtmo_flag) {
		muic_data->dcdtmo++;
	} else {
		muic_data->dcdtmo = 0;
	}

	pr_debug("BIT_DCDTmoI occured %d", muic_data->dcdtmo);
	/* pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq); */

	return IRQ_HANDLED;
}

#if 0
static irqreturn_t max77729_vbusdet_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_muic_data *muic_data = usbc_data->muic_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77729_read_reg(muic_data->i2c, REG_BC_STATUS, &muic_data->bc_status);

	if ((muic_data->bc_status & BIT_VBUSDet) == BIT_VBUSDet) {
		pr_info(" VBUS > VVBDET");
		muic_data->vbusdet = 1;
	} else {
		pr_info(" VBUS < VVBDET");
		muic_data->vbusdet = 0;
	}
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}
#endif

static void max77729_muic_print_reg_log(struct work_struct *work)
{
	struct max77729_muic_data *muic_data =
		container_of(work, struct max77729_muic_data, debug_work.work);
	struct i2c_client *i2c = muic_data->i2c;
	struct i2c_client *pmic_i2c = muic_data->usbc_pdata->i2c;
	u8 status[12] = {0, };

	max77729_read_reg(i2c, MAX77729_USBC_REG_USBC_STATUS1, &status[0]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_USBC_STATUS2, &status[1]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_BC_STATUS, &status[2]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_CC_STATUS0, &status[3]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_CC_STATUS1, &status[4]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_PD_STATUS0, &status[5]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_PD_STATUS1, &status[6]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_UIC_INT_M, &status[7]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_CC_INT_M, &status[8]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_PD_INT_M, &status[9]);
	max77729_read_reg(i2c, MAX77729_USBC_REG_VDM_INT_M, &status[10]);
	max77729_read_reg(pmic_i2c, MAX77729_PMIC_REG_INTSRC_MASK, &status[11]);

	/* pr_info("%s USBC1:0x%02x, USBC2:0x%02x, BC:0x%02x, CC0:0x%x, CC1:0x%x, PD0:0x%x, PD1:0x%x\n", */
		/* __func__, status[0], status[1], status[2], status[3], status[4], status[5], status[6]); */
	/* pr_info("%s UIC_INT_M:0x%x, CC_INT_M:0x%x, PD_INT_M:0x%x, VDM_INT_M:0x%x, PMIC_MASK:0x%x, WDT:%d, POR:%d\n", */
		/* __func__, status[7], status[8], status[9], status[10], status[11], */
		/* muic_data->usbc_pdata->watchdog_count, muic_data->usbc_pdata->por_count); */

	schedule_delayed_work(&(muic_data->debug_work),
		msecs_to_jiffies(60000));
}

/* static void max77729_muic_free_irqs(struct max77729_muic_data *muic_data) */
/* { */
	/* pr_info("%s\n", __func__); */

	/* disable_irq(muic_data->irq_chgtyp); */
	/* disable_irq(muic_data->irq_dcdtmo); */
	/* disable_irq(muic_data->irq_vbadc); */
	/* disable_irq(muic_data->irq_vbusdet); */
/* } */


void max77729_set_qc(struct max77729_muic_data *muic_data, int voltage)
{
	struct max77729_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;
	u8 dpdndrv;

	switch (voltage) {
	case 5:
		dpdndrv = 0x04;
		break;
	case 9:
		dpdndrv = 0x09;
		break;
	default:
		pr_info("%s:%s invalid value(%d), return\n", MUIC_DEV_NAME,
				__func__, voltage);
		return;
	}

	/* pr_info("%s:%s voltage(%d)\n", MUIC_DEV_NAME, __func__, voltage); */
//	pr_info("%s:%s voltage(%d)\n", MUIC_DEV_NAME, __func__, voltage);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_QC_2_0_SET;
	write_data.write_length = 1;
	write_data.write_data[0] = dpdndrv;
	write_data.read_length = 2;

	max77729_usbc_opcode_write(usbc_pdata, &write_data);
}

static void max77729_qc_work(struct work_struct *work)
{
	struct max77729_muic_data *muic_data =
		container_of(work, struct max77729_muic_data, qc_work.work);
	struct max77729_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;

	/* pr_info("%s\n", __func__); */

    if(muic_data->chg_type == CHGTYP_DCP){
		if((usbc_pdata->pd_data->psrdy_received)
			|| (usbc_pdata->is_hvdcp)
			|| (usbc_pdata->src_cap_flag)){
        	/*pr_info("%s skip the QC command\n", __func__);*/
            return; //skip the QC command because of connects the PD TA.
         }

        //send the QC command in order to set VBUS==9V.
        max77729_set_qc(muic_data, 9);

    }
}


int max77729_bc12_probe(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_platform_data *mfd_pdata = usbc_data->max77729_data;
	struct max77729_muic_data *muic_data;
	int ret = 0;

	/* pr_info("%s\n", __func__); */

	muic_data = devm_kzalloc(usbc_data->dev, sizeof(struct max77729_muic_data), GFP_KERNEL);
	if (!muic_data) {
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}
	mutex_init(&muic_data->muic_mutex);
	/* muic_data->muic_ws = wakeup_source_register(usbc_data->dev, "muic-irq"); */
	muic_data->i2c = usbc_data->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->usbc_pdata = usbc_data;
	usbc_data->muic_data = muic_data;
	g_muic_data = muic_data;


	muic_data->irq_chgtyp = usbc_data->irq_base
		+ MAX77729_USBC_IRQ_CHGT_INT;
	if (muic_data->irq_chgtyp) {
		ret = request_threaded_irq(muic_data->irq_chgtyp,
				NULL, max77729_chgtype_irq,
				0,
				"bc-chgtyp-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	muic_data->irq_dcdtmo = usbc_data->irq_base
		+ MAX77729_USBC_IRQ_DCD_INT;
	if (muic_data->irq_dcdtmo) {
		ret = request_threaded_irq(muic_data->irq_dcdtmo,
				NULL, max77729_dcdtmo_irq,
				0,
				"bc-dcdtmo-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

	muic_data->irq_vbadc = usbc_data->irq_base
		+ MAX77729_USBC_IRQ_VBADC_INT;
	if (muic_data->irq_vbadc) {
		ret = request_threaded_irq(muic_data->irq_vbadc,
				NULL, max77729_vbadc_irq,
				0,
				"bc-vbadc-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
#if 0
	muic_data->irq_vbusdet = usbc_data->irq_base
		+ MAX77729_USBC_IRQ_VBUS_INT;
	if (muic_data->irq_vbusdet) {
		ret = request_threaded_irq(muic_data->irq_vbusdet,
				NULL, max77729_vbusdet_irq,
				0,
				"bc-vbusdet-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}
#endif
	INIT_DELAYED_WORK(&(muic_data->debug_work),
		max77729_muic_print_reg_log);
	INIT_DELAYED_WORK(&(muic_data->qc_work),
		max77729_qc_work);
	/* schedule_delayed_work(&(muic_data->debug_work), */
		/* msecs_to_jiffies(10000)); */


	return 0;
err_irq:

err_return:
	return ret;
}
int max77729_muic_suspend(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_muic_data *muic_data = usbc_data->muic_data;

	/* pr_info("%s\n", __func__); */
	cancel_delayed_work(&(muic_data->debug_work));

	return 0;
}

int max77729_muic_resume(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_muic_data *muic_data = usbc_data->muic_data;

	/* pr_info("%s\n", __func__); */
	schedule_delayed_work(&(muic_data->debug_work), msecs_to_jiffies(1000));

	return 0;
}
