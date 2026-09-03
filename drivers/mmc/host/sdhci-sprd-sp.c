// SPDX-License-Identifier: GPL-2.0
//
// Customization requirements for special eMMC
//
// Secure Digital Host Controller
//
// Copyright (C) 2023 Spreadtrum, Inc.
// Author: Qianning Huang <qianning.huang@unisoc.com>

#include "sdhci.h"

/* eMMC */
#define MANFID_SAMSUNG  0x15
#define MANFID_YMTC     0x9b
#define MANFID_HYNIX    0x90

/* T-Card */
#define MANFID_SANDISK  0X3

#define PNM_SIZE        8 /* Product name */

struct mmc_mfrs_special {
	/* Material manufacturer information */
	unsigned int manfid;
	char prod_name[PNM_SIZE];
	/* Special flag identified in different scenes */
	char *flag;
	/* Print tips if chosen a special material */
	char *tips;
	/* Perform special ops if match a special eMMC */
	void (*special_ops)(struct mmc_card *card);
};

static void mmc_disable_cmdq(struct mmc_card *card)
{
	card->ext_csd.cmdq_support = false;
}

static void mmc_mask_excp(struct mmc_card *card)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(card->host));

	sprd_host->mask_excp = true;
}

static void mmc_fixup_ds_type4(struct mmc_card *card)
{
	card->host->fixed_drv_type = 0x4;
}

static void mmc_set_min_timeout4(struct mmc_card *card)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(card->host));

	sprd_host->min_data_timeout = 0x4;
}

/* Special material collection */
static const struct mmc_mfrs_special mmc_special_coll[] = {
	/* Manfid,        Name,     Flag,       Tips,          Special ops */
	/* eMMC */
	{MANFID_SAMSUNG, "GX6BAB", "cmdq",     "disable cmdq", mmc_disable_cmdq},
	{MANFID_SAMSUNG, "GX6BMB", "cmdq",     "disable cmdq", mmc_disable_cmdq},
	{MANFID_SAMSUNG, "QE63BB", "cmdq",     "disable cmdq", mmc_disable_cmdq},
	{MANFID_SAMSUNG, "QE63MB", "cmdq",     "disable cmdq", mmc_disable_cmdq},
	{MANFID_HYNIX,   "HBG4a2", "cmdq",     "disable cmdq", mmc_disable_cmdq},
	{MANFID_YMTC,    "Y0S064", "excp",     "mask exception bit in R1/R1b", mmc_mask_excp},
	{MANFID_YMTC,    "Y0S128", "excp",     "mask exception bit in R1/R1b", mmc_mask_excp},
	{MANFID_YMTC,    "Y0S064", "ds",       "fix drvier strength Type4", mmc_fixup_ds_type4},
	/* T-card */
	{MANFID_SANDISK, "SU02G",  "min_timeout", "set data timeout min to 4", mmc_set_min_timeout4}
};

/* Return index of the special material in collection */
static int mmc_cmp_mfrs_info(unsigned int manfid, char prod_name[PNM_SIZE],
				const char * const flag)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mmc_special_coll); i++) {
		if ((mmc_special_coll[i].manfid == manfid) &&
			(!strcmp(mmc_special_coll[i].prod_name, prod_name)) &&
			(!strcmp(mmc_special_coll[i].flag, flag)))
			return i;
	}

	return -EINVAL;
}

static bool mmc_special_flag_match(struct mmc_card *card, const char * const flag)
{
	char *tips;
	int index = 0;

	if (card == NULL || flag == NULL) {
		pr_err("%s: NULL parameter is invalid\n", __func__);
		return false;
	}

	index = mmc_cmp_mfrs_info(card->cid.manfid, card->cid.prod_name, flag);
	if (index == -EINVAL)
		return false;

	tips = mmc_special_coll[index].tips;
	if (mmc_special_coll[index].special_ops == NULL)
		goto out;

	mmc_special_coll[index].special_ops(card);

out:
	pr_info("%s: special mmc need %s.\n", mmc_hostname(card->host), tips);
	return true;
}

static const char * const mmc_special_flag_coll[] = {
	"cmdq",
	"excp",
	"ds",
	"min_timeout"
};

void mmc_special_ops_needed(struct mmc_card *card)
{
	int i = 0;
	int cnt = 0;

	if (card == NULL) {
		pr_err("%s: NULL parameter is invalid\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(mmc_special_flag_coll); i++) {
		if (mmc_special_flag_match(card, mmc_special_flag_coll[i]))
			cnt++;
	}

	if (cnt)
		pr_debug("%s: perform %d special ops\n", mmc_hostname(card->host), cnt);
}
