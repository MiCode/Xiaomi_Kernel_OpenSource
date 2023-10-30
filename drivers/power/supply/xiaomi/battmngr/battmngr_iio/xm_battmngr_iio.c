
#include <linux/battmngr/xm_battmngr_iio.h>

struct xm_battmngr_iio *g_battmngr_iio;
EXPORT_SYMBOL(g_battmngr_iio);

int xm_get_iio_channel(struct xm_battmngr_iio *battmngr_iio, const char *propname,
					struct iio_channel **chan)
{
	int rc = 0;

	rc = of_property_match_string(battmngr_iio->dev->of_node,
					"io-channel-names", propname);
	if (rc < 0)
		return 0;

	*chan = devm_iio_channel_get(battmngr_iio->dev, propname);
	if (IS_ERR(*chan)) {
		rc = PTR_ERR(*chan);
		if (rc != -EPROBE_DEFER)
			pr_err("%s channel unavailable, %d\n",
							propname, rc);
		*chan = NULL;
	}

	return rc;
}
EXPORT_SYMBOL(xm_get_iio_channel);

bool is_cp_master_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(battmngr_iio->iio_chan_list_cp[chan]))
		return false;

	if (!battmngr_iio->iio_chan_list_cp[chan]) {
		battmngr_iio->iio_chan_list_cp[chan] = devm_iio_channel_get(battmngr_iio->dev,
					cp_iio_chan[chan]);
		if (IS_ERR(battmngr_iio->iio_chan_list_cp[chan])) {
			rc = PTR_ERR(battmngr_iio->iio_chan_list_cp[chan]);
			if (rc == -EPROBE_DEFER)
				battmngr_iio->iio_chan_list_cp[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_iio_chan[chan], rc);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(is_cp_master_chan_valid);

bool is_cp_slave_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(battmngr_iio->iio_chan_list_cp_sec[chan]))
		return false;

	if (!battmngr_iio->iio_chan_list_cp_sec[chan]) {
		battmngr_iio->iio_chan_list_cp_sec[chan] = devm_iio_channel_get(battmngr_iio->dev,
					cp_sec_iio_chan[chan]);
		if (IS_ERR(battmngr_iio->iio_chan_list_cp_sec[chan])) {
			rc = PTR_ERR(battmngr_iio->iio_chan_list_cp_sec[chan]);
			if (rc == -EPROBE_DEFER)
				battmngr_iio->iio_chan_list_cp_sec[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_sec_iio_chan[chan], rc);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(is_cp_slave_chan_valid);

bool is_main_chg_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum main_chg_iio_channels chan)
{
	int rc;

	if (IS_ERR(battmngr_iio->iio_chan_list_main_chg[chan]))
		return false;

	if (!battmngr_iio->iio_chan_list_main_chg[chan]) {
		battmngr_iio->iio_chan_list_main_chg[chan] = devm_iio_channel_get(battmngr_iio->dev,
					main_chg_iio_chan[chan]);
		if (IS_ERR(battmngr_iio->iio_chan_list_main_chg[chan])) {
			rc = PTR_ERR(battmngr_iio->iio_chan_list_main_chg[chan]);
			if (rc == -EPROBE_DEFER)
				battmngr_iio->iio_chan_list_main_chg[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_chg_iio_chan[chan], rc);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(is_main_chg_chan_valid);

bool is_batt_fg_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum batt_fg_iio_channels chan)
{
	int rc;

	if (IS_ERR(battmngr_iio->iio_chan_list_batt_fg[chan]))
		return false;

	if (!battmngr_iio->iio_chan_list_batt_fg[chan]) {
		battmngr_iio->iio_chan_list_batt_fg[chan] = devm_iio_channel_get(battmngr_iio->dev,
					batt_fg_iio_chan[chan]);
		if (IS_ERR(battmngr_iio->iio_chan_list_batt_fg[chan])) {
			rc = PTR_ERR(battmngr_iio->iio_chan_list_batt_fg[chan]);
			if (rc == -EPROBE_DEFER)
				battmngr_iio->iio_chan_list_batt_fg[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				batt_fg_iio_chan[chan], rc);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(is_batt_fg_chan_valid);

bool is_pd_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum batt_fg_iio_channels chan)
{
	int rc;

	if (IS_ERR(battmngr_iio->iio_chan_list_pd[chan]))
		return false;

	if (!battmngr_iio->iio_chan_list_pd[chan]) {
		battmngr_iio->iio_chan_list_pd[chan] = devm_iio_channel_get(battmngr_iio->dev,
					pd_iio_chan[chan]);
		if (IS_ERR(battmngr_iio->iio_chan_list_pd[chan])) {
			rc = PTR_ERR(battmngr_iio->iio_chan_list_pd[chan]);
			if (rc == -EPROBE_DEFER)
				battmngr_iio->iio_chan_list_pd[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				pd_iio_chan[chan], rc);
			return false;
		}
	}

	return true;
}
EXPORT_SYMBOL(is_pd_chan_valid);

int xm_battmngr_read_iio_prop(struct xm_battmngr_iio *battmngr_iio,
		enum iio_type type, int iio_chan, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(!battmngr_iio)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!is_cp_master_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_cp[iio_chan];
		break;
	case CP_SLAVE:
		if (!is_cp_slave_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_cp_sec[iio_chan];
		break;
	case MAIN_CHG:
		if (!is_main_chg_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_main_chg[iio_chan];
		break;
	case BATT_FG:
		if (!is_batt_fg_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_batt_fg[iio_chan];
		break;
	case PD_PHY:
		if (!is_pd_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_pd[iio_chan];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(xm_battmngr_read_iio_prop);

int xm_battmngr_write_iio_prop(struct xm_battmngr_iio *battmngr_iio,
		enum iio_type type, int iio_chan, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(!battmngr_iio)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!is_cp_master_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_cp[iio_chan];
		break;
	case CP_SLAVE:
		if (!is_cp_slave_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_cp_sec[iio_chan];
		break;
	case MAIN_CHG:
		if (!is_main_chg_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_main_chg[iio_chan];
		break;
	case BATT_FG:
		if (!is_batt_fg_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_batt_fg[iio_chan];
		break;
	case PD_PHY:
		if (!is_pd_chan_valid(battmngr_iio, iio_chan))
			return -ENODEV;
		iio_chan_list = battmngr_iio->iio_chan_list_pd[iio_chan];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(xm_battmngr_write_iio_prop);

int xm_battmngr_iio_init(struct xm_battmngr_iio *battmngr_iio)
{
	int rc = 0;

	pr_err("xm_battmngr_iio_init start\n");

	battmngr_iio->iio_chan_list_cp = devm_kcalloc(battmngr_iio->dev,
		ARRAY_SIZE(cp_iio_chan), sizeof(*battmngr_iio->iio_chan_list_cp), GFP_KERNEL);
	if (!battmngr_iio->iio_chan_list_cp)
		return -ENOMEM;

	battmngr_iio->iio_chan_list_cp_sec = devm_kcalloc(battmngr_iio->dev,
		ARRAY_SIZE(cp_sec_iio_chan), sizeof(*battmngr_iio->iio_chan_list_cp_sec), GFP_KERNEL);
	if (!battmngr_iio->iio_chan_list_cp_sec)
		return -ENOMEM;

	battmngr_iio->iio_chan_list_main_chg = devm_kcalloc(battmngr_iio->dev,
		ARRAY_SIZE(main_chg_iio_chan), sizeof(*battmngr_iio->iio_chan_list_main_chg), GFP_KERNEL);
	if (!battmngr_iio->iio_chan_list_main_chg)
		return -ENOMEM;

	battmngr_iio->iio_chan_list_batt_fg = devm_kcalloc(battmngr_iio->dev,
		ARRAY_SIZE(batt_fg_iio_chan), sizeof(*battmngr_iio->iio_chan_list_batt_fg), GFP_KERNEL);
	if (!battmngr_iio->iio_chan_list_batt_fg)
		return -ENOMEM;

	battmngr_iio->iio_chan_list_pd = devm_kcalloc(battmngr_iio->dev,
		ARRAY_SIZE(pd_iio_chan), sizeof(*battmngr_iio->iio_chan_list_pd), GFP_KERNEL);
	if (!battmngr_iio->iio_chan_list_pd)
		return -ENOMEM;

	return rc;
}
EXPORT_SYMBOL(xm_battmngr_iio_init);

MODULE_DESCRIPTION("Xiaomi Battery Manager iio");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("getian@xiaomi.com");

