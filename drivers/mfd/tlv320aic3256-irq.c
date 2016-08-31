/*
 * tlv320aic325x-irq.c  --  Interrupt controller support for
 *			 TI TLV320AIC3xxx family
 *
 * Author:      Mukund Navada <navada@ti.com>
 *              Mehar Bajwa <mehar.bajwa@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>

#include <linux/mfd/tlv320aic325x-core.h>
#include <linux/mfd/tlv320aic325x-registers.h>

#include <linux/delay.h>

struct aic325x_irq_data {
	int mask;
	int status;
};

static struct aic325x_irq_data aic325x_irqs[] = {
	{
	 .mask = AIC3XXX_HEADSET_IN_M,
	 .status = AIC3XXX_HEADSET_PLUG_UNPLUG_INT,
	 },
	{
	 .mask = AIC3XXX_BUTTON_PRESS_M,
	 .status = AIC3XXX_BUTTON_PRESS_INT,
	 },
	{
	 .mask = AIC3XXX_DAC_DRC_THRES_M,
	 .status = AIC3XXX_LEFT_DRC_THRES_INT | AIC3XXX_RIGHT_DRC_THRES_INT,
	 },
	{
	 .mask = AIC3XXX_AGC_NOISE_M,
	 .status = AIC3XXX_LEFT_AGC_NOISE_INT | AIC3XXX_RIGHT_AGC_NOISE_INT,
	 },
	{
	 .mask = AIC3XXX_OVER_CURRENT_M,
	 .status = AIC3XXX_LEFT_OUTPUT_DRIVER_OVERCURRENT_INT
	 | AIC3XXX_RIGHT_OUTPUT_DRIVER_OVERCURRENT_INT,
	 },
	{
	 .mask = AIC3XXX_OVERFLOW_M,
	 .status =
	 AIC3XXX_LEFT_DAC_OVERFLOW_INT | AIC3XXX_RIGHT_DAC_OVERFLOW_INT |
	 AIC3XXX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT |
	 AIC3XXX_LEFT_ADC_OVERFLOW_INT | AIC3XXX_RIGHT_ADC_OVERFLOW_INT |
	 AIC3XXX_MINIDSP_D_BARREL_SHIFT_OVERFLOW_INT,
	 },
	{
	 .mask = AIC3XXX_SPK_OVERCURRENT_M,
	 .status = AIC3XXX_SPK_OVER_CURRENT_INT,
	 },

};

static void aic325x_irq_lock(struct irq_data *data)
{
	struct aic325x *aic325x = irq_data_get_irq_chip_data(data);

	mutex_lock(&aic325x->irq_lock);
}

static void aic325x_irq_sync_unlock(struct irq_data *data)
{
	struct aic325x *aic325x = irq_data_get_irq_chip_data(data);

	/* write back to hardware any change in irq mask */
	if (aic325x->irq_masks_cur != aic325x->irq_masks_cache) {
		aic325x->irq_masks_cache = aic325x->irq_masks_cur;
		aic325x_reg_write(aic325x, AIC3XXX_INT1_CNTL,
				  aic325x->irq_masks_cur);
	}

	mutex_unlock(&aic325x->irq_lock);
}


static void aic325x_irq_enable(struct irq_data *data)
{
	struct aic325x *aic325x = irq_data_get_irq_chip_data(data);
	struct aic325x_irq_data *irq_data = &aic325x_irqs[data->hwirq];
	aic325x->irq_masks_cur |= irq_data->mask;
}

static void aic325x_irq_disable(struct irq_data *data)
{
	struct aic325x *aic325x = irq_data_get_irq_chip_data(data);
	struct aic325x_irq_data *irq_data = &aic325x_irqs[data->hwirq];

	aic325x->irq_masks_cur &= ~irq_data->mask;
}

static struct irq_chip aic325x_irq_chip = {
	.name = "tlv320aic325x",
	.irq_bus_lock = aic325x_irq_lock,
	.irq_bus_sync_unlock = aic325x_irq_sync_unlock,
	.irq_enable = aic325x_irq_enable,
	.irq_disable = aic325x_irq_disable
};

static irqreturn_t aic325x_irq_thread(int irq, void *data)
{
	struct aic325x *aic325x = data;
	u8 status[4];

	/* Reading sticky bit registers acknowledges
		the interrupt to the device */
	aic325x_bulk_read(aic325x, AIC3XXX_INT_STICKY_FLAG1, 4, status);

	/* report  */
	if (status[2] & aic325x_irqs[AIC3XXX_IRQ_HEADSET_DETECT].status)
		handle_nested_irq(aic325x->irq_base);
	if (status[2] & aic325x_irqs[AIC3XXX_IRQ_BUTTON_PRESS].status)
		handle_nested_irq(aic325x->irq_base + 1);
	if (status[2] & aic325x_irqs[AIC3XXX_IRQ_DAC_DRC].status)
		handle_nested_irq(aic325x->irq_base + 2);
	if (status[3] & aic325x_irqs[AIC3XXX_IRQ_AGC_NOISE].status)
		handle_nested_irq(aic325x->irq_base + 3);
	if (status[2] & aic325x_irqs[AIC3XXX_IRQ_OVER_CURRENT].status)
		handle_nested_irq(aic325x->irq_base + 4);
	if (status[0] & aic325x_irqs[AIC3XXX_IRQ_OVERFLOW_EVENT].status)
		handle_nested_irq(aic325x->irq_base + 5);
	if (status[3] & aic325x_irqs[AIC3XXX_IRQ_SPEAKER_OVER_TEMP].status)
		handle_nested_irq(aic325x->irq_base + 6);

	return IRQ_HANDLED;
}

static int aic325x_irq_map(struct irq_domain *h, unsigned int virq,
				irq_hw_number_t hw)
{
	struct aic325x *aic325x = h->host_data;

	irq_set_chip_data(virq, aic325x);
	irq_set_chip_and_handler(virq, &aic325x_irq_chip, handle_edge_irq);
	irq_set_nested_thread(virq, 1);

	/* ARM needs us to explicitly flag the IRQ as valid
	 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
	set_irq_flags(virq, IRQF_VALID);
#else
	irq_set_noprobe(virq);
#endif

	return 0;
}

static const struct irq_domain_ops aic325x_domain_ops = {
	.map    = aic325x_irq_map,
	.xlate  = irq_domain_xlate_twocell,
};

int aic325x_irq_init(struct aic325x *aic325x)
{
	int ret;

	mutex_init(&aic325x->irq_lock);

	/* mask the individual interrupt sources */
	aic325x->irq_masks_cur = 0x0;
	aic325x->irq_masks_cache = 0x0;
	aic325x_reg_write(aic325x, AIC3XXX_INT1_CNTL, 0x0);

	if (!aic325x->irq) {
		dev_warn(aic325x->dev, "no interrupt specified\n");
		aic325x->irq_base = 0;
		return 0;
	}
	if (aic325x->irq_base) {
		aic325x->domain = irq_domain_add_legacy(aic325x->dev->of_node,
					ARRAY_SIZE(aic325x_irqs),
					aic325x->irq_base, 0,
					&aic325x_domain_ops, aic325x);
	} else {
		aic325x->domain = irq_domain_add_linear(aic325x->dev->of_node,
					ARRAY_SIZE(aic325x_irqs),
					&aic325x_domain_ops, aic325x);
		/* initiallizing irq_base from irq_domain*/
	}
	if (!aic325x->domain) {
		dev_err(aic325x->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	aic325x->irq_base = irq_create_mapping(aic325x->domain, 0);

	ret = request_threaded_irq(aic325x->irq, NULL, aic325x_irq_thread,
				   IRQF_ONESHOT,
				   "tlv320aic325x", aic325x);
	if (ret < 0) {
		dev_err(aic325x->dev, "failed to request IRQ %d: %d\n",
			aic325x->irq, ret);
		return ret;
	}
	irq_set_irq_type(aic325x->irq, IRQF_TRIGGER_RISING);

	return 0;
}
EXPORT_SYMBOL(aic325x_irq_init);

void aic325x_irq_exit(struct aic325x *aic325x)
{
	if (aic325x->irq)
		free_irq(aic325x->irq, aic325x);
}
EXPORT_SYMBOL(aic325x_irq_exit);
MODULE_AUTHOR("Mukund navada <navada@ti.com>");
MODULE_AUTHOR("Mehar Bajwa <mehar.bajwa@ti.com>");
MODULE_DESCRIPTION("Interrupt controller support for TI TLV320AIC3XXX family");
MODULE_LICENSE("GPL");
