/*
 * max77729-irq.c - Interrupt controller support for MAX77729
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
 *
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/max77729.h>
#include <linux/mfd/max77729-private.h>

static const u8 max77729_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[SYS_INT] = MAX77729_PMIC_REG_SYSTEM_INT_MASK,
	[CHG_INT] = MAX77729_CHG_REG_INT_MASK,
	[FUEL_INT] = MAX77729_REG_INVALID,
	[USBC_INT] = MAX77729_USBC_REG_UIC_INT_M,
	[CC_INT] = MAX77729_USBC_REG_CC_INT_M,
	[PD_INT] = MAX77729_USBC_REG_PD_INT_M,
	[VDM_INT] = MAX77729_USBC_REG_VDM_INT_M,
	[VIR_INT] = MAX77729_REG_INVALID,
};

static struct i2c_client *get_i2c(struct max77729_dev *max77729,
				enum max77729_irq_source src)
{
	switch (src) {
	case SYS_INT:
		return max77729->i2c;
	case FUEL_INT:
		return max77729->fuelgauge;
	case CHG_INT:
		return max77729->charger;
	case USBC_INT:
	case CC_INT:
	case PD_INT:
	case VDM_INT:
	case VIR_INT:
		return max77729->muic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct max77729_irq_data {
	int mask;
	enum max77729_irq_source group;
};

static const struct max77729_irq_data max77729_irqs[] = {
	[MAX77729_SYSTEM_IRQ_BSTEN_INT] = { .group = SYS_INT, .mask = 1 << 3 },
	[MAX77729_SYSTEM_IRQ_SYSUVLO_INT] = { .group = SYS_INT, .mask = 1 << 4 },
	[MAX77729_SYSTEM_IRQ_SYSOVLO_INT] = { .group = SYS_INT, .mask = 1 << 5 },
	[MAX77729_SYSTEM_IRQ_TSHDN_INT] = { .group = SYS_INT, .mask = 1 << 6 },
	[MAX77729_SYSTEM_IRQ_TM_INT] = { .group = SYS_INT, .mask = 1 << 7 },

	[MAX77729_CHG_IRQ_BYP_I] = { .group = CHG_INT, .mask = 1 << 0 },
	[MAX77729_CHG_IRQ_BATP_I] = { .group = CHG_INT, .mask = 1 << 2 },
	[MAX77729_CHG_IRQ_BAT_I] = { .group = CHG_INT, .mask = 1 << 3 },
	[MAX77729_CHG_IRQ_CHG_I] = { .group = CHG_INT, .mask = 1 << 4 },
	[MAX77729_CHG_IRQ_WCIN_I] = { .group = CHG_INT, .mask = 1 << 5 },
	[MAX77729_CHG_IRQ_CHGIN_I] = { .group = CHG_INT, .mask = 1 << 6 },
	[MAX77729_CHG_IRQ_AICL_I] = { .group = CHG_INT, .mask = 1 << 7 },

	[MAX77729_FG_IRQ_ALERT] = { .group = FUEL_INT, .mask = 1 << 1 },

	[MAX77729_USBC_IRQ_APC_INT] = { .group = USBC_INT, .mask = 1 << 7 },
	[MAX77729_USBC_IRQ_SYSM_INT] = { .group = USBC_INT, .mask = 1 << 6 },
	[MAX77729_USBC_IRQ_VBUS_INT] = { .group = USBC_INT, .mask = 1 << 5 },
	[MAX77729_USBC_IRQ_VBADC_INT] = { .group = USBC_INT, .mask = 1 << 4 },
	[MAX77729_USBC_IRQ_DCD_INT] = { .group = USBC_INT, .mask = 1 << 3 },
	[MAX77729_USBC_IRQ_FAKVB_INT] = { .group = USBC_INT, .mask = 1 << 2 },
	[MAX77729_USBC_IRQ_CHGT_INT] = { .group = USBC_INT, .mask = 1 << 1 },

	[MAX77729_CC_IRQ_VCONNCOP_INT] = { .group = CC_INT, .mask = 1 << 7 },
	[MAX77729_CC_IRQ_VSAFE0V_INT] = { .group = CC_INT, .mask = 1 << 6 },
	[MAX77729_CC_IRQ_DETABRT_INT] = { .group = CC_INT, .mask = 1 << 5 },
	[MAX77729_CC_IRQ_CCPINSTAT_INT] = { .group = CC_INT, .mask = 1 << 3 },
	[MAX77729_CC_IRQ_CCISTAT_INT] = { .group = CC_INT, .mask = 1 << 2 },
	[MAX77729_CC_IRQ_CCVCNSTAT_INT] = { .group = CC_INT, .mask = 1 << 1 },
	[MAX77729_CC_IRQ_CCSTAT_INT] = { .group = CC_INT, .mask = 1 << 0 },

	[MAX77729_PD_IRQ_PDMSG_INT] = { .group = PD_INT, .mask = 1 << 7 },
	[MAX77729_PD_IRQ_PS_RDY_INT] = { .group = PD_INT, .mask = 1 << 6 },
	[MAX77729_PD_IRQ_DATAROLE_INT] = { .group = PD_INT, .mask = 1 << 5 },
	[MAX77729_IRQ_VDM_ATTENTION_INT] = { .group = PD_INT, .mask = 1 << 4 },
	[MAX77729_IRQ_VDM_DP_CONFIGURE_INT] = { .group = PD_INT, .mask = 1 << 3 },
	[MAX77729_IRQ_VDM_DP_STATUS_UPDATE_INT] = { .group = PD_INT, .mask = 1 << 2 },

	[MAX77729_IRQ_VDM_DISCOVER_ID_INT] = { .group = VDM_INT, .mask = 1 << 0 },
	[MAX77729_IRQ_VDM_DISCOVER_SVIDS_INT] = { .group = VDM_INT, .mask = 1 << 1 },
	[MAX77729_IRQ_VDM_DISCOVER_MODES_INT] = { .group = VDM_INT, .mask = 1 << 2 },
	[MAX77729_IRQ_VDM_ENTER_MODE_INT] = { .group = VDM_INT, .mask = 1 << 3 },

	[MAX77729_VIR_IRQ_ALTERROR_INT] = { .group = VIR_INT, .mask = 1 << 0 },
};

static void max77729_irq_lock(struct irq_data *data)
{
	struct max77729_dev *max77729 = irq_get_chip_data(data->irq);

	mutex_lock(&max77729->irqlock);
}

static void max77729_irq_sync_unlock(struct irq_data *data)
{
	struct max77729_dev *max77729 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MAX77729_IRQ_GROUP_NR; i++) {
		u8 mask_reg = max77729_mask_reg[i];
		struct i2c_client *i2c = get_i2c(max77729, i);

		if (mask_reg == MAX77729_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		max77729->irq_masks_cache[i] = max77729->irq_masks_cur[i];

		max77729_write_reg(i2c, max77729_mask_reg[i],
				max77729->irq_masks_cur[i]);
	}

	mutex_unlock(&max77729->irqlock);
}

static const inline struct max77729_irq_data *
irq_to_max77729_irq(struct max77729_dev *max77729, int irq)
{
	return &max77729_irqs[irq - max77729->irq_base];
}

static void max77729_irq_mask(struct irq_data *data)
{
	struct max77729_dev *max77729 = irq_get_chip_data(data->irq);
	const struct max77729_irq_data *irq_data =
	    irq_to_max77729_irq(max77729, data->irq);

	if (irq_data->group >= MAX77729_IRQ_GROUP_NR)
		return;

	max77729->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void max77729_irq_unmask(struct irq_data *data)
{
	struct max77729_dev *max77729 = irq_get_chip_data(data->irq);
	const struct max77729_irq_data *irq_data =
	    irq_to_max77729_irq(max77729, data->irq);

	if (irq_data->group >= MAX77729_IRQ_GROUP_NR)
		return;

	max77729->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static void max77729_irq_disable(struct irq_data *data)
{
	max77729_irq_mask(data);
}

static struct irq_chip max77729_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= max77729_irq_lock,
	.irq_bus_sync_unlock	= max77729_irq_sync_unlock,
	.irq_mask		= max77729_irq_mask,
	.irq_unmask		= max77729_irq_unmask,
	.irq_disable            = max77729_irq_disable,
};

#define VB_LOW 0

static irqreturn_t max77729_irq_thread(int irq, void *data)
{
	struct max77729_dev *max77729 = data;
	u8 irq_reg[MAX77729_IRQ_GROUP_NR] = {0};
	u8 irq_src;
	int i, ret;
	u8 irq_vdm_mask = 0x0;
	u8 dump_reg[10] = {0, };
	/* u8 pmic_rev = max77729->pmic_rev; */
	u8 reg_data;
	u8 cc_status0 = 0;
	u8 cc_status1 = 0;
	u8 bc_status0 = 0;
	u8 ccstat = 0;
	u8 vbvolt = 0;
	u8 pre_ccstati = 0;
	u8 ic_alt_mode = 0;

	/* pr_info("%s: irq gpio pre-state(0x%02x)\n", __func__, */
				/* gpio_get_value(max77729->irq_gpio)); */

#if defined(CONFIG_QCOM_IFPMIC_SUSPEND)
	max77729->doing_irq = 1;

	ret = wait_event_timeout(max77729->suspend_wait,
						!max77729->suspended,
						msecs_to_jiffies(200));
	if (!ret) {
		pr_info("%s suspend_wait timeout\n", __func__);
		max77729->doing_irq = 0;
		return IRQ_NONE;
	}
#endif

	ret = max77729_read_reg(max77729->i2c,
					MAX77729_PMIC_REG_INTSRC, &irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
			max77729->doing_irq = 0;
		return IRQ_NONE;
	}

	/* pr_info("%s:%s: irq[%d] %d/%d/%d irq_src=0x%02x pmic_rev=0x%02x\n", MFD_DEV_NAME, __func__, */
		/* irq, max77729->irq, max77729->irq_base, max77729->irq_gpio, irq_src, pmic_rev); */

	if (irq_src & MAX77729_IRQSRC_CHG) {
	/* CHG_INT */
		ret = max77729_read_reg(max77729->charger, MAX77729_CHG_REG_INT,
				&irq_reg[CHG_INT]);

	   	if(max77729->enable_nested_irq){
			irq_reg[USBC_INT] |= max77729->usbc_irq;
			max77729->enable_nested_irq = 0x0;
			max77729->usbc_irq = 0x0;
		}
		/* pr_debug("%s: charger interrupt(0x%02x)\n", */
				/* __func__, irq_reg[CHG_INT]); */
		/* mask chgin to prevent chgin infinite interrupt
		 * chgin is unmasked chgin isr
		 */
		if (irq_reg[CHG_INT] &
				max77729_irqs[MAX77729_CHG_IRQ_CHGIN_I].mask) {
			max77729_read_reg(max77729->charger,
				MAX77729_CHG_REG_INT_MASK, &reg_data);
			reg_data |= (1 << 6);
			max77729_write_reg(max77729->charger,
				MAX77729_CHG_REG_INT_MASK, reg_data);
		}
	}


	if (irq_src & MAX77729_IRQSRC_FG) {
		/* pr_err("[%s] fuelgauge interrupt\n", __func__); */
		/* pr_debug("[%s]IRQ_BASE(%d), NESTED_IRQ(%d)\n", */
			/* __func__, max77729->irq_base, max77729->irq_base + MAX77729_FG_IRQ_ALERT); */
		handle_nested_irq(max77729->irq_base + MAX77729_FG_IRQ_ALERT);
		goto done;
	}

	if (irq_src & MAX77729_IRQSRC_TOP) {
		/* SYS_INT */
        ret = max77729_read_reg(max77729->i2c, MAX77729_PMIC_REG_SYSTEM_INT,
                &irq_reg[SYS_INT]);
		pr_info("%s: topsys interrupt(0x%02x)\n", __func__, irq_reg[SYS_INT]);
	}

	if ((irq_src & MAX77729_IRQSRC_USBC) && max77729->cc_booting_complete) {
		/* USBC INT */
        ret = max77729_bulk_read(max77729->muic, MAX77729_USBC_REG_UIC_INT,
                4, &irq_reg[USBC_INT]);
        ret = max77729_read_reg(max77729->muic, MAX77729_USBC_REG_VDM_INT_M,
                &irq_vdm_mask);
        if (irq_reg[USBC_INT] & BIT_VBUSDetI) {
            ret = max77729_read_reg(max77729->muic, REG_BC_STATUS, &bc_status0);
            ret = max77729_read_reg(max77729->muic, REG_CC_STATUS0, &cc_status0);
            vbvolt = (bc_status0 & BIT_VBUSDet) >> FFS(BIT_VBUSDet);
            ccstat = (cc_status0 & BIT_CCStat) >> FFS(BIT_CCStat);
            if (cc_No_Connection == ccstat && vbvolt == VB_LOW) {
                pre_ccstati = irq_reg[CC_INT];
                irq_reg[CC_INT] |= 0x1;
                pr_info("[MAX77729] set the cc_stat int [work-around] :%x, %x\n",
                    pre_ccstati,irq_reg[CC_INT]);
            }
        }
		ret = max77729_bulk_read(max77729->muic, MAX77729_USBC_REG_USBC_STATUS1,
				8, dump_reg);
		pr_err("[MAX77729] irq_reg, complete [%x], %x, %x, %x, %x, %x\n", max77729->cc_booting_complete,
			irq_reg[USBC_INT], irq_reg[CC_INT], irq_reg[PD_INT], irq_reg[VDM_INT], irq_vdm_mask);
		pr_err("[MAX77729] dump_reg, %x, %x, %x, %x, %x, %x, %x, %x\n", dump_reg[0], dump_reg[1],
			dump_reg[2], dump_reg[3], dump_reg[4], dump_reg[5], dump_reg[6], dump_reg[7]);
	}

	if (max77729->cc_booting_complete) {
		max77729_read_reg(max77729->muic, REG_CC_STATUS1, &cc_status1);
		ic_alt_mode = (cc_status1 & BIT_Altmode) >> FFS(BIT_Altmode);
		if (!ic_alt_mode && max77729->set_altmode_en)
			irq_reg[VIR_INT] |= (1 << 0);
		/* pr_info("%s ic_alt_mode=%d\n", __func__, ic_alt_mode); */

		if (irq_reg[PD_INT] & BIT_PDMsg) {
			if (dump_reg[6] == Sink_PD_PSRdy_received
					|| dump_reg[6] == SRC_CAP_RECEIVED) {
				if (max77729->check_pdmsg)
					max77729->check_pdmsg(max77729->usbc_data, dump_reg[6]);
			}
		}
	}

	/* Apply masking */
	for (i = 0; i < MAX77729_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~max77729->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < MAX77729_IRQ_NR; i++) {
		if (irq_reg[max77729_irqs[i].group] & max77729_irqs[i].mask)
			handle_nested_irq(max77729->irq_base + i);
	}

done:
	max77729->doing_irq = 0;
	if(max77729->cc_booting_complete){
		max77729->is_usbc_queue = !(max77729->check_usbc_opcode_queue());
		/* pr_info("%s doing_irq = %d, is_usbc_queue=%d\n", __func__, */
				/* max77729->doing_irq, max77729->is_usbc_queue); */
		wake_up_interruptible(&max77729->queue_empty_wait_q);
	}
	return IRQ_HANDLED;
}

int max77729_irq_init(struct max77729_dev *max77729)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!max77729->irq_gpio) {
		dev_warn(max77729->dev, "No interrupt specified.\n");
		max77729->irq_base = 0;
		return 0;
	}

	if (!max77729->irq_base) {
		dev_err(max77729->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&max77729->irqlock);

	max77729->irq = gpio_to_irq(max77729->irq_gpio);
	/* pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__, */
			/* max77729->irq, max77729->irq_gpio); */

	ret = gpio_request(max77729->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(max77729->dev, "%s: failed requesting gpio %d\n",
			__func__, max77729->irq_gpio);
		return ret;
	}
	gpio_direction_input(max77729->irq_gpio);
	gpio_free(max77729->irq_gpio);


	//disable_irq(max77729->irq);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77729_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;
		/* MUIC IRQ  0:MASK 1:NOT MASK => NOT USE */
		/* Other IRQ 1:MASK 0:NOT MASK */
		max77729->irq_masks_cur[i] = 0xff;
		max77729->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(max77729, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (max77729_mask_reg[i] == MAX77729_REG_INVALID)
			continue;
		max77729_write_reg(i2c, max77729_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77729_IRQ_NR; i++) {
		cur_irq = i + max77729->irq_base;
		irq_set_chip_data(cur_irq, max77729);
		irq_set_chip_and_handler(cur_irq, &max77729_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#if 0 //Brandon it should be modifed based on Xiaomi platform
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
#else
        irq_set_noprobe(cur_irq);
#endif
	}

	/* Unmask max77729 interrupt */
	ret = max77729_read_reg(max77729->i2c, MAX77729_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read muic reg\n", MFD_DEV_NAME, __func__);
		return ret;
	}
	i2c_data |= 0xF;	/* mask muic interrupt */

	max77729_write_reg(max77729->i2c, MAX77729_PMIC_REG_INTSRC_MASK,
			   i2c_data);

 	max77729_write_word(max77729->fuelgauge, 0x1d, 0x2350);     //disable alert fg for some abnormal shutdown, reboot

	ret = request_threaded_irq(max77729->irq, NULL, max77729_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "max77729-irq", max77729);
	if (ret) {
		dev_err(max77729->dev, "Failed to request IRQ %d: %d\n",
			max77729->irq, ret);
		return ret;
	}


	/* Unmask max77729 interrupt */
	ret = max77729_read_reg(max77729->i2c, MAX77729_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read muic reg\n", MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(MAX77729_IRQSRC_CHG);	/* Unmask charger interrupt */
//	i2c_data &= ~(MAX77729_IRQSRC_FG);      /* Unmask fg interrupt */
	i2c_data |= MAX77729_IRQSRC_USBC;	/* mask usbc interrupt */
	/* i2c_data |= MAX77729_IRQSRC_CHG;	[> mask usbc interrupt <] */

	max77729_write_reg(max77729->i2c, MAX77729_PMIC_REG_INTSRC_MASK,
			   i2c_data);

	/* pr_info("%s:%s max77729_PMIC_REG_INTSRC_MASK=0x%02x\n", */
			/* MFD_DEV_NAME, __func__, i2c_data); */

	return 0;
}

void max77729_irq_exit(struct max77729_dev *max77729)
{
	if (max77729->irq)
		free_irq(max77729->irq, max77729);
}
