
#include "inc/xm_pd_adapter.h"
#include "inc/xm_adapter_class.h"
#include "inc/rt17xx_iio.h"

static int rt17xx_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct xm_pd_adapter_info *info = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_RT_PD_ACTIVE:
		if (info->pd_active == val1)
			break;
		info->pd_active = val1;
		if (adapter_check_usb_psy(info) &&
				adapter_check_battery_psy(info)) {
			g_battmngr_noti->pd_msg.msg_type = BATTMNGR_MSG_PD_ACTIVE;
			g_battmngr_noti->pd_msg.pd_active = info->pd_active;
			battmngr_notifier_call_chain(BATTMNGR_EVENT_PD, g_battmngr_noti);
			pr_err("%s pd_active: %d\n", __func__, info->pd_active);
		}
		break;
	case PSY_IIO_RT_PD_CURRENT_MAX:
		info->pd_cur_max = val1;
		g_battmngr_noti->pd_msg.pd_curr_max = info->pd_cur_max;
		pr_err("%s pd_curr_max: %d\n", __func__, info->pd_cur_max);
		break;
	case PSY_IIO_RT_PD_VOLTAGE_MIN:
		info->pd_vol_min = val1;
		break;
	case PSY_IIO_RT_PD_VOLTAGE_MAX:
		info->pd_vol_max = val1;
		break;
	case PSY_IIO_RT_PD_IN_HARD_RESET:
		info->pd_in_hard_reset = val1;
		break;
	case PSY_IIO_RT_TYPEC_CC_ORIENTATION:
		info->typec_cc_orientation = val1;
		break;
	case PSY_IIO_RT_TYPEC_MODE:
		info->typec_mode = val1;
		break;
	case PSY_IIO_RT_PD_USB_SUSPEND_SUPPORTED:
		info->pd_usb_suspend_supported = val1;
		break;
	case PSY_IIO_RT_PD_APDO_VOLT_MAX:
		info->pd_apdo_volt_max = val1;
		break;
	case PSY_IIO_RT_PD_APDO_CURR_MAX:
		info->pd_apdo_curr_max = val1;
		break;
	case PSY_IIO_RT_PD_USB_REAL_TYPE:
		info->pd_usb_real_type = val1;
		break;
	case PSY_IIO_RT_TYPEC_ACCESSORY_MODE:
		if (info->typec_accessory_mode == val1)
			break;
		info->typec_accessory_mode = val1;
		pr_err("%s typec_accessory_mode: %d\n", __func__,
			info->typec_accessory_mode);
		g_battmngr_noti->pd_msg.msg_type = BATTMNGR_MSG_PD_AUDIO;
		g_battmngr_noti->pd_msg.accessory_mode = info->typec_accessory_mode;
		battmngr_notifier_call_chain(BATTMNGR_EVENT_PD, g_battmngr_noti);
		break;
	default:
		pr_debug("Unsupported rt17xx IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int rt17xx_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct xm_pd_adapter_info *info = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_RT_PD_ACTIVE:
		*val1 = info->pd_active;;
		break;
	case PSY_IIO_RT_PD_CURRENT_MAX:
		*val1 = info->pd_cur_max;
		break;
	case PSY_IIO_RT_PD_VOLTAGE_MIN:
		*val1 = info->pd_vol_min;
		break;
	case PSY_IIO_RT_PD_VOLTAGE_MAX:
		*val1 = info->pd_vol_max;;
		break;
	case PSY_IIO_RT_PD_IN_HARD_RESET:
		*val1 = info->pd_in_hard_reset;
		break;
	case PSY_IIO_RT_TYPEC_CC_ORIENTATION:
		*val1 = info->typec_cc_orientation;
		break;
	case PSY_IIO_RT_TYPEC_MODE:
		*val1 = info->typec_mode;
		break;
	case PSY_IIO_RT_PD_USB_SUSPEND_SUPPORTED:
		*val1 = info->pd_usb_suspend_supported;
		break;
	case PSY_IIO_RT_PD_APDO_VOLT_MAX:
		*val1 = info->pd_apdo_volt_max;
		break;
	case PSY_IIO_RT_PD_APDO_CURR_MAX:
		*val1 = info->pd_apdo_curr_max;
		break;
	case PSY_IIO_RT_PD_USB_REAL_TYPE:
		*val1 = info->pd_usb_real_type;
		break;
	case PSY_IIO_RT_TYPEC_ACCESSORY_MODE:
		*val1 = info->typec_accessory_mode;
		break;
	case PSY_IIO_RT_TYPEC_ADAPTER_ID:
		*val1 = info->adapter_dev->adapter_id;
		break;
	default:
		pr_debug("Unsupported rt17xx IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int rt17xx_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct xm_pd_adapter_info *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(rt17xx_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info rt17xx_iio_info = {
	.read_raw	= rt17xx_iio_read_raw,
	.write_raw	= rt17xx_iio_write_raw,
	.of_xlate	= rt17xx_iio_of_xlate,
};

int rt17xx_init_iio_psy(struct xm_pd_adapter_info *info)
{
	struct iio_dev *indio_dev = info->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(rt17xx_iio_psy_channels);
	int rc, i;

	pr_err("rt17xx_init_iio_psy start\n");
	info->iio_chan = devm_kcalloc(info->dev, num_iio_channels,
				sizeof(*info->iio_chan), GFP_KERNEL);
	if (!info->iio_chan)
		return -ENOMEM;

	info->int_iio_chans = devm_kcalloc(info->dev,
				num_iio_channels,
				sizeof(*info->int_iio_chans),
				GFP_KERNEL);
	if (!info->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &rt17xx_iio_info;
	indio_dev->dev.parent = info->dev;
	indio_dev->dev.of_node = info->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = info->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "rt17xx,pd";
	for (i = 0; i < num_iio_channels; i++) {
		info->int_iio_chans[i].indio_dev = indio_dev;
		chan = &info->iio_chan[i];
		info->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = rt17xx_iio_psy_channels[i].channel_num;
		chan->type = rt17xx_iio_psy_channels[i].type;
		chan->datasheet_name =
			rt17xx_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			rt17xx_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			rt17xx_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(info->dev, indio_dev);
	if (rc)
		pr_err("Failed to register rt17xx IIO device, rc=%d\n", rc);

	pr_err("rt17xx IIO device, rc=%d\n", rc);
	return rc;
}

