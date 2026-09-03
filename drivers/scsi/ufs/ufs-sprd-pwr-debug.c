// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include "ufshcd.h"

struct ufs_pa_layer_attr pwr_now_raw_pre;
struct ufs_pa_layer_attr pwr_now_raw_post;
struct ufs_pa_layer_attr final_params_raw;
struct ufs_pa_layer_attr max_pwr_info_raw;
u32 times_pre_pwr;
u32 times_pre_compare_fail;
u32 times_post_pwr;
u32 times_post_compare_fail;

static void ufshcd_print_pwr_info(struct ufs_hba *hba,
				struct ufs_pa_layer_attr *pwr_mode, char *pre_post, char *str)
{
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW_MODE",
		"INVALID MODE",
		"FASTAUTO_MODE",
		"SLOWAUTO_MODE",
		"INVALID MODE",
	};
	if (pwr_mode->pwr_rx == SLOW_MODE && pwr_mode->pwr_tx == SLOW_MODE) {
		dev_err(hba->dev, "%s:%s [RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate don't care = %d\n",
				pre_post, str,
				pwr_mode->gear_rx, pwr_mode->gear_tx,
				pwr_mode->lane_rx, pwr_mode->lane_tx,
				names[pwr_mode->pwr_rx > 6 ? 0 : pwr_mode->pwr_rx],
				names[pwr_mode->pwr_tx > 6 ? 0 : pwr_mode->pwr_tx],
				pwr_mode->hs_rate);

	} else {
		dev_err(hba->dev, "%s:%s [RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate = %d\n",
				pre_post, str,
				pwr_mode->gear_rx, pwr_mode->gear_tx,
				pwr_mode->lane_rx, pwr_mode->lane_tx,
				names[pwr_mode->pwr_rx > 6 ? 0 : pwr_mode->pwr_rx],
				names[pwr_mode->pwr_tx > 6 ? 0 : pwr_mode->pwr_tx],
				pwr_mode->hs_rate);
	}
}

static int ufs_compare_max_pwr_mode(struct ufs_hba *hba, struct ufs_pa_layer_attr *final_params)
{
	struct ufs_pa_layer_attr  tmp_max_pwr_info_raw;
	struct ufs_pa_layer_attr  *max_pwr_info = &(tmp_max_pwr_info_raw);

	/* Only pwr for FAST_MODE or SLOW_MODE*/
	if (final_params->pwr_rx > SLOW_MODE  || final_params->pwr_tx > SLOW_MODE) {
		if (final_params->pwr_rx == FASTAUTO_MODE  ||
				final_params->pwr_tx == FASTAUTO_MODE) {
			final_params->pwr_rx = FAST_MODE;
			final_params->pwr_tx = FAST_MODE;
		} else {
			final_params->pwr_rx = SLOW_MODE;
			final_params->pwr_tx = SLOW_MODE;
		}
	}

	max_pwr_info->pwr_tx = FAST_MODE;
	max_pwr_info->pwr_rx = FAST_MODE;
	max_pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&max_pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&max_pwr_info->lane_tx);

	if (!max_pwr_info->lane_rx || !max_pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				max_pwr_info->lane_rx,
				max_pwr_info->lane_tx);
		return -EINVAL;
	}

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &max_pwr_info->gear_rx);
	if (!max_pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_rx);
		if (!max_pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
					__func__, max_pwr_info->gear_rx);
			return -EINVAL;
		}
		max_pwr_info->pwr_rx = SLOW_MODE;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&max_pwr_info->gear_tx);
	if (!max_pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_tx);
		if (!max_pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
					__func__, max_pwr_info->gear_tx);
			return -EINVAL;
		}
		max_pwr_info->pwr_tx = SLOW_MODE;
	}
	if (max_pwr_info->pwr_tx == SLOW_MODE &&
			max_pwr_info->pwr_rx == SLOW_MODE)
		max_pwr_info->hs_rate = 0;

	memcpy(&(final_params_raw), final_params, sizeof(struct ufs_pa_layer_attr));
	memcpy(&(max_pwr_info_raw), max_pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (final_params->gear_rx > max_pwr_info->gear_rx  ||
			final_params->gear_tx > max_pwr_info->gear_tx  ||
			final_params->lane_rx > max_pwr_info->lane_rx   ||
			final_params->lane_tx > max_pwr_info->lane_tx   ||
			final_params->pwr_rx < max_pwr_info->pwr_rx     ||
			final_params->pwr_tx < max_pwr_info->pwr_tx     ||
			final_params->hs_rate > max_pwr_info->hs_rate) {
		dev_err(hba->dev, "=== ufs err ! final more than max ===\n");
		return -EINVAL;
	}
	return 0;
}

static int ufs_get_sprd_pwr_now(struct ufs_hba *hba, enum ufs_notify_change_status status)
{
	int ret = 0, pwr = 0;
	struct ufs_pa_layer_attr  *pwr_now;

	if (status == PRE_CHANGE)
		pwr_now = &pwr_now_raw_pre;
	else
		pwr_now = &pwr_now_raw_post;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_RXGEAR),
			&(pwr_now->gear_rx));
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TXGEAR),
			&(pwr_now->gear_tx));
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			&(pwr_now->lane_rx));
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			&(pwr_now->lane_tx));
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HSSERIES),
			&(pwr_now->hs_rate));
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			&pwr);
	if (ret)
		goto out;
	pwr_now->pwr_rx = (pwr >> 4) & 0xf;
	pwr_now->pwr_tx = (pwr >> 0) & 0xf;

	ufshcd_print_pwr_info(hba, pwr_now, "ufs_get_sprd_pwr_now",
			(status == PRE_CHANGE)?"pre_now":"post_now");
	return 0;
out:
	return -1;
}

static int ufs_print_sprd_pwr_now(struct ufs_hba *hba, char *pre_post)
{
	ufshcd_print_pwr_info(hba, &(pwr_now_raw_pre), pre_post, "pre_now");
	ufshcd_print_pwr_info(hba, &(pwr_now_raw_post), pre_post, "post_now");
	ufshcd_print_pwr_info(hba, &(final_params_raw), pre_post, " final ");
	ufshcd_print_pwr_info(hba, &(max_pwr_info_raw), pre_post, " max ");

	return 0;
}

static int ufs_sprd_pwr_post_compare(struct ufs_hba *hba)
{

	if ((pwr_now_raw_post).pwr_rx == SLOW_MODE &&
			(pwr_now_raw_post).pwr_tx == SLOW_MODE)
		(pwr_now_raw_post).hs_rate = 0;

	if ((pwr_now_raw_post).gear_rx == (final_params_raw).gear_rx &&
			(pwr_now_raw_post).gear_tx == (final_params_raw).gear_tx &&
			(pwr_now_raw_post).lane_rx == (final_params_raw).lane_rx &&
			(pwr_now_raw_post).lane_tx == (final_params_raw).lane_tx &&
			(pwr_now_raw_post).pwr_rx == (final_params_raw).pwr_rx &&
			(pwr_now_raw_post).pwr_tx == (final_params_raw).pwr_tx &&
			(pwr_now_raw_post).hs_rate == (final_params_raw).hs_rate) {
		return 0;
	}
	return -1;
}

void ufs_sprd_pwr_change_compare(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *final_params,
		int *err)
{
	ufs_get_sprd_pwr_now(hba, status);

	switch (status) {
	case PRE_CHANGE:
		times_pre_pwr++;
		if (ufs_compare_max_pwr_mode(hba, final_params)) {
			times_pre_compare_fail++;
			ufs_print_sprd_pwr_now(hba, "ufs pwr pre err");
#if defined(CONFIG_SPRD_DEBUG)
			panic("pre_compare_fail");
#endif
		}
		break;
	case POST_CHANGE:
		times_post_pwr++;
		/*after call ufs_sprd_pwr_now, if already configured to the requested pwr_mode */
		if (ufs_sprd_pwr_post_compare(hba)) {
			times_post_compare_fail++;
			ufs_print_sprd_pwr_now(hba, "ufs pwr post err");
#if defined(CONFIG_SPRD_DEBUG)
			panic("post_compare_fail");
#endif
		}
		break;
	default:
		*err = -EINVAL;
		break;
	}
}
EXPORT_SYMBOL_GPL(ufs_sprd_pwr_change_compare);
