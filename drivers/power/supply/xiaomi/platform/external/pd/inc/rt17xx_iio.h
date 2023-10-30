
#ifndef RT17XX_IIO_H
#define RT17XX_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct rt17xx_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define RT17XX_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define RT17XX_CHAN_ENERGY(_name, _num)			\
	RT17XX_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct rt17xx_iio_channels rt17xx_iio_psy_channels[] = {
	RT17XX_CHAN_ENERGY("rt_pd_active", PSY_IIO_RT_PD_ACTIVE)
	RT17XX_CHAN_ENERGY("rt_pd_current_max", PSY_IIO_RT_PD_CURRENT_MAX)
	RT17XX_CHAN_ENERGY("rt_pd_voltage_min", PSY_IIO_RT_PD_VOLTAGE_MIN)
	RT17XX_CHAN_ENERGY("rt_pd_voltage_max", PSY_IIO_RT_PD_VOLTAGE_MAX)
	RT17XX_CHAN_ENERGY("rt_pd_in_hard_reset", PSY_IIO_RT_PD_IN_HARD_RESET)
	RT17XX_CHAN_ENERGY("rt_typec_cc_orientation", PSY_IIO_RT_TYPEC_CC_ORIENTATION)
	RT17XX_CHAN_ENERGY("rt_typec_mode", PSY_IIO_RT_TYPEC_MODE)
	RT17XX_CHAN_ENERGY("rt_pd_usb_suspend_supported", PSY_IIO_RT_PD_USB_SUSPEND_SUPPORTED)
	RT17XX_CHAN_ENERGY("rt_pd_apdo_volt_max", PSY_IIO_RT_PD_APDO_VOLT_MAX)
	RT17XX_CHAN_ENERGY("rt_pd_apdo_curr_max", PSY_IIO_RT_PD_APDO_CURR_MAX)
	RT17XX_CHAN_ENERGY("rt_pd_usb_real_type", PSY_IIO_RT_PD_USB_REAL_TYPE)
	RT17XX_CHAN_ENERGY("rt_typec_accessory_mode", PSY_IIO_RT_TYPEC_ACCESSORY_MODE)
	RT17XX_CHAN_ENERGY("rt_typec_adapter_id", PSY_IIO_RT_TYPEC_ADAPTER_ID)
};

int rt17xx_init_iio_psy(struct xm_pd_adapter_info *info);

#endif /*RT17XX_IIO_H*/

