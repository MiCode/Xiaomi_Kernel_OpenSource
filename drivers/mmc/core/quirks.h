/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  This file contains work-arounds for many known SD/MMC
 *  and SDIO hardware bugs.
 *
 *  Copyright (c) 2011 Andrei Warkentin <andreiw@motorola.com>
 *  Copyright (c) 2011 Pierre Tardy <tardyp@gmail.com>
 *  Inspired from pci fixup code:
 *  Copyright (c) 1999 Martin Mares <mj@ucw.cz>
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_ids.h>

#include "card.h"

static const struct mmc_fixup mmc_blk_fixups[] = {
#define INAND_CMD38_ARG_EXT_CSD  113
#define INAND_CMD38_ARG_ERASE    0x00
#define INAND_CMD38_ARG_TRIM     0x01
#define INAND_CMD38_ARG_SECERASE 0x80
#define INAND_CMD38_ARG_SECTRIM1 0x81
#define INAND_CMD38_ARG_SECTRIM2 0x88
	/* CMD38 argument is passed through EXT_CSD[113] */
	MMC_FIXUP("SEM02G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM04G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM08G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM16G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),
	MMC_FIXUP("SEM32G", CID_MANFID_SANDISK, 0x100, add_quirk,
		  MMC_QUIRK_INAND_CMD38),

	/*
	 * Some MMC cards experience performance degradation with CMD23
	 * instead of CMD12-bounded multiblock transfers. For now we'll
	 * black list what's bad...
	 * - Certain Toshiba cards.
	 *
	 * N.B. This doesn't affect SD cards.
	 */
	MMC_FIXUP("SDMB-32", CID_MANFID_SANDISK, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("SDM032", CID_MANFID_SANDISK, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC08G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC16G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("MMC32G", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_TOSHIBA, CID_OEMID_ANY,
		  add_quirk_mmc, MMC_QUIRK_CMDQ_EMPTY_BEFORE_DCMD),

	/*
	 * Some SD cards lockup while using CMD23 multiblock transfers.
	 */
	MMC_FIXUP("AF SD", CID_MANFID_ATP, CID_OEMID_ANY, add_quirk_sd,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("APUSD", CID_MANFID_APACER, 0x5048, add_quirk_sd,
		  MMC_QUIRK_BLK_NO_CMD23),

	/*
	 * Some SD cards lockup while using CMD23 multiblock transfers.
	 */
	MMC_FIXUP("AF SD", CID_MANFID_ATP, CID_OEMID_ANY, add_quirk_sd,
		  MMC_QUIRK_BLK_NO_CMD23),
	MMC_FIXUP("APUSD", CID_MANFID_APACER, 0x5048, add_quirk_sd,
		  MMC_QUIRK_BLK_NO_CMD23),

	/*
	 * Some MMC cards need longer data read timeout than indicated in CSD.
	 */
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_MICRON, 0x200, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),
	MMC_FIXUP("008GE0", CID_MANFID_TOSHIBA, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * Some Samsung MMC cards need longer data read timeout than
	 * indicated in CSD.
	 */
	MMC_FIXUP("Q7XSAB", CID_MANFID_SAMSUNG, 0x100, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * Hynix eMMC cards need longer data read timeout than
	 * indicated in CSD.
	 */
	MMC_FIXUP(CID_NAME_ANY, CID_MANFID_HYNIX, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_LONG_READ_TIME),

	/*
	 * On these Samsung MoviNAND parts, performing secure erase or
	 * secure trim can result in unrecoverable corruption due to a
	 * firmware bug.
	 */
	MMC_FIXUP("M8G2FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MAG4FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MBG8FA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("MCGAFA", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VAL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("KYL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),
	MMC_FIXUP("VZL00M", CID_MANFID_SAMSUNG, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_SEC_ERASE_TRIM_BROKEN),

	/*
	 *  On Some Kingston eMMCs, performing trim can result in
	 *  unrecoverable data conrruption occasionally due to a firmware bug.
	 */
	MMC_FIXUP("V10008", CID_MANFID_KINGSTON, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_TRIM_BROKEN),
	MMC_FIXUP("V10016", CID_MANFID_KINGSTON, CID_OEMID_ANY, add_quirk_mmc,
		  MMC_QUIRK_TRIM_BROKEN),

	/* Some INAND MCP devices advertise incorrect timeout values */
	MMC_FIXUP("SEM04G", 0x45, CID_OEMID_ANY, add_quirk_mmc,
		MMC_QUIRK_INAND_DATA_TIMEOUT),

	END_FIXUP
};

static const struct mmc_fixup mmc_ext_csd_fixups[] = {
	/*
	 * Certain Hynix eMMC 4.41 cards might get broken when HPI feature
	 * is used so disable the HPI feature for such buggy cards.
	 */
	MMC_FIXUP_EXT_CSD_REV(CID_NAME_ANY, CID_MANFID_HYNIX,
			      0x014a, add_quirk, MMC_QUIRK_BROKEN_HPI, 5),
	/*
	 * Certain Micron (Numonyx) eMMC 4.5 cards might get broken when HPI
	 * feature is used so disable the HPI feature for such buggy cards.
	 */
	MMC_FIXUP_EXT_CSD_REV(CID_NAME_ANY, CID_MANFID_NUMONYX,
			      0x014e, add_quirk, MMC_QUIRK_BROKEN_HPI, 6),

	END_FIXUP
};

static const struct mmc_fixup sdio_fixup_methods[] = {
	SDIO_FIXUP(SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271,
		   add_quirk, MMC_QUIRK_NONSTD_FUNC_IF),

	SDIO_FIXUP(SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271,
		   add_quirk, MMC_QUIRK_DISABLE_CD),

	SDIO_FIXUP(SDIO_VENDOR_ID_STE, SDIO_DEVICE_ID_STE_CW1200,
		   add_quirk, MMC_QUIRK_BROKEN_BYTE_MODE_512),

	SDIO_FIXUP(SDIO_VENDOR_ID_MARVELL, SDIO_DEVICE_ID_MARVELL_8797_F0,
		   add_quirk, MMC_QUIRK_BROKEN_IRQ_POLLING),

	END_FIXUP
};

#ifndef SDIO_VENDOR_ID_TI
#define SDIO_VENDOR_ID_TI		0x0097
#endif

#ifndef SDIO_DEVICE_ID_TI_WL1271
#define SDIO_DEVICE_ID_TI_WL1271	0x4076
#endif

#ifndef SDIO_VENDOR_ID_STE
#define SDIO_VENDOR_ID_STE		0x0020
#endif

#ifndef SDIO_DEVICE_ID_STE_CW1200
#define SDIO_DEVICE_ID_STE_CW1200	0x2280
#endif

#ifndef SDIO_DEVICE_ID_MARVELL_8797_F0
#define SDIO_DEVICE_ID_MARVELL_8797_F0	0x9128
#endif

#ifndef SDIO_VENDOR_ID_MSM
#define SDIO_VENDOR_ID_MSM		0x0070
#endif

#ifndef SDIO_DEVICE_ID_MSM_WCN1314
#define SDIO_DEVICE_ID_MSM_WCN1314	0x2881
#endif

#ifndef SDIO_VENDOR_ID_MSM_QCA
#define SDIO_VENDOR_ID_MSM_QCA		0x271
#endif

#ifndef SDIO_DEVICE_ID_MSM_QCA_AR6003_1
#define SDIO_DEVICE_ID_MSM_QCA_AR6003_1	0x300
#endif

#ifndef SDIO_DEVICE_ID_MSM_QCA_AR6003_2
#define SDIO_DEVICE_ID_MSM_QCA_AR6003_2	0x301
#endif

#ifndef SDIO_DEVICE_ID_MSM_QCA_AR6004_1
#define SDIO_DEVICE_ID_MSM_QCA_AR6004_1	0x400
#endif

#ifndef SDIO_DEVICE_ID_MSM_QCA_AR6004_2
#define SDIO_DEVICE_ID_MSM_QCA_AR6004_2	0x401
#endif

#ifndef SDIO_VENDOR_ID_QCA6574
#define SDIO_VENDOR_ID_QCA6574		0x271
#endif

#ifndef SDIO_DEVICE_ID_QCA6574
#define SDIO_DEVICE_ID_QCA6574		0x50a
#endif

#ifndef SDIO_VENDOR_ID_QCA9377
#define SDIO_VENDOR_ID_QCA9377		0x271
#endif

#ifndef SDIO_DEVICE_ID_QCA9377
#define SDIO_DEVICE_ID_QCA9377		0x701
#endif

/*
 * This hook just adds a quirk for all sdio devices
 */
static void add_quirk_for_sdio_devices(struct mmc_card *card, int data)
{
	if (mmc_card_sdio(card))
		card->quirks |= data;
}

static const struct mmc_fixup mmc_fixup_methods[] = {
	/* by default sdio devices are considered CLK_GATING broken */
	/* good cards will be whitelisted as they are tested */
	SDIO_FIXUP(SDIO_ANY_ID, SDIO_ANY_ID,
		   add_quirk_for_sdio_devices,
		   MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_MSM, SDIO_DEVICE_ID_MSM_WCN1314,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_MSM_QCA, SDIO_DEVICE_ID_MSM_QCA_AR6003_1,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_MSM_QCA, SDIO_DEVICE_ID_MSM_QCA_AR6003_2,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_MSM_QCA, SDIO_DEVICE_ID_MSM_QCA_AR6004_1,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_MSM_QCA, SDIO_DEVICE_ID_MSM_QCA_AR6004_2,
		   remove_quirk, MMC_QUIRK_BROKEN_CLK_GATING),

	SDIO_FIXUP(SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271,
		   add_quirk, MMC_QUIRK_NONSTD_FUNC_IF),

	SDIO_FIXUP(SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271,
		   add_quirk, MMC_QUIRK_DISABLE_CD),

	SDIO_FIXUP(SDIO_VENDOR_ID_STE, SDIO_DEVICE_ID_STE_CW1200,
		   add_quirk, MMC_QUIRK_BROKEN_BYTE_MODE_512),

	SDIO_FIXUP(SDIO_VENDOR_ID_MARVELL, SDIO_DEVICE_ID_MARVELL_8797_F0,
		   add_quirk, MMC_QUIRK_BROKEN_IRQ_POLLING),

	SDIO_FIXUP(SDIO_VENDOR_ID_QCA6574, SDIO_DEVICE_ID_QCA6574,
		   add_quirk, MMC_QUIRK_QCA6574_SETTINGS),

	SDIO_FIXUP(SDIO_VENDOR_ID_QCA9377, SDIO_DEVICE_ID_QCA9377,
		add_quirk, MMC_QUIRK_QCA9377_SETTINGS),
	END_FIXUP
};

static inline void mmc_fixup_device(struct mmc_card *card,
				    const struct mmc_fixup *table)
{
	const struct mmc_fixup *f;
	u64 rev = cid_rev_card(card);

	/* Non-core specific workarounds. */
	if (!table)
		table = mmc_fixup_methods;

	for (f = table; f->vendor_fixup; f++) {
		if ((f->manfid == CID_MANFID_ANY ||
		     f->manfid == card->cid.manfid) &&
		    (f->oemid == CID_OEMID_ANY ||
		     f->oemid == card->cid.oemid) &&
		    (f->name == CID_NAME_ANY ||
		     !strncmp(f->name, card->cid.prod_name,
			      sizeof(card->cid.prod_name))) &&
		    (f->ext_csd_rev == EXT_CSD_REV_ANY ||
		     f->ext_csd_rev == card->ext_csd.rev) &&
		    (f->cis_vendor == card->cis.vendor ||
		     f->cis_vendor == (u16) SDIO_ANY_ID) &&
		    (f->cis_device == card->cis.device ||
		     f->cis_device == (u16) SDIO_ANY_ID) &&
		    (f->ext_csd_rev == EXT_CSD_REV_ANY ||
		     f->ext_csd_rev == card->ext_csd.rev) &&
		    rev >= f->rev_start && rev <= f->rev_end) {
			dev_dbg(&card->dev, "calling %pf\n", f->vendor_fixup);
			f->vendor_fixup(card, f->data);
		}
	}
}
