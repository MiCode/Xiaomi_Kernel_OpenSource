/*
* Copyright (C) 2016 MediaTek Inc.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "ufs.h"
#include <linux/nls.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "unipro.h"
#include "ufs-mtk.h"

#ifdef CONFIG_MTK_UFS_HW_CRYPTO
/* #include <mach/mt_secure_api.h> */
#endif

/* Query request retries */
#define QUERY_REQ_RETRIES 10

static struct ufs_dev_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
	UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
	UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
	UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
	UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_INCORRECT_PWM_BURST_CLOSURE_EXTENSION),

	END_FIX
};

ufs_cmd_str_table ufs_mtk_cmd_str_tbl[] = {
	{"TEST_UNIT_READY"       , 0x00},
	{"REQUEST_SENSE"         , 0x03},
	{"FORMAT_UNIT"           , 0x04},
	{"READ_BLOCK_LIMITS"     , 0x05},
	{"INQUIRY"               , 0x12},
	{"RECOVER_BUFFERED_DATA" , 0x14},
	{"MODE_SENSE"            , 0x1a},
	{"START_STOP"            , 0x1b},
	{"SEND_DIAGNOSTIC"       , 0x1d},
	{"READ_FORMAT_CAPACITIES", 0x23},
	{"READ_CAPACITY"         , 0x25},
	{"READ_10"               , 0x28},
	{"WRITE_10"              , 0x2a},
	{"PRE_FETCH"             , 0x34},
	{"SYNCHRONIZE_CACHE"     , 0x35},
	{"WRITE_BUFFER"          , 0x3b},
	{"READ_BUFFER"           , 0x3c},
	{"UNMAP"                 , 0x42},
	{"MODE_SELECT_10"        , 0x55},
	{"MODE_SENSE_10"         , 0x5a},
	{"REPORT_LUNS"           , 0xa0},
	{"READ_CAPACITY_16"      , 0x9e},
	{"SECURITY_PROTOCOL_IN"  , 0xa2},
	{"MAINTENANCE_IN"        , 0xa3},
	{"MAINTENANCE_OUT"       , 0xa4},
	{"SECURITY_PROTOCOL_OUT" , 0xb5},
	{"UNKNOWN",               0xFF}
};

bool ufs_mtk_host_deep_stall_enable = 0;
bool ufs_mtk_host_scramble_enable = 0;
bool ufs_mtk_tr_cn_used = 1;
void __iomem *ufs_mtk_mmio_base_infracfg_ao = NULL;
void __iomem *ufs_mtk_mmio_base_pericfg = NULL;

enum ufs_dbg_lvl_t ufs_mtk_dbg_lvl = T_UFS_DBG_LVL_1;

#if defined(CONFIG_MTK_UFS_DEBUG)

static inline void dumpMemory4(const char *str, const void *data, size_t size)
{
	print_hex_dump(KERN_DEBUG, str, DUMP_PREFIX_OFFSET, 16, 4, data, size,
		       false);
}

static inline void print_prd(struct ufshcd_lrb *lrbp)
{
	dumpMemory4("PRDT : ", lrbp->ucd_prdt_ptr,
		    sizeof(lrbp->ucd_prdt_ptr[0]) *
		    le16_to_cpu(lrbp->utr_descriptor_ptr->prd_table_length));
}

static void print_query_function(const void *upiu)
{
	const unsigned char *req_upiu = upiu;
	u8 opcode = req_upiu[12];
	u8 idn = req_upiu[13];
	u8 index = req_upiu[14];
	char *opcode_name;

	switch (opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		opcode_name = "read descriptor";
		break;
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		opcode_name = "write descriptor";
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		opcode_name = "read attribute";
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		opcode_name = "write attribute";
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		opcode_name = "read flag";
		break;
	case UPIU_QUERY_OPCODE_SET_FLAG:
		opcode_name = "set flag";
		break;
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
		opcode_name = "clear flag";
		break;
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		opcode_name = "toggle flag";
		break;
	default:
		opcode_name = "unknown opcde";
		break;
	}
	pr_err("%s: IDN %02x INDEX %02x\n", opcode_name, idn, index);
}

void ufs_mtk_print_request(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	u8 req = be32_to_cpu(ucd_req_ptr->header.dword_0) >> 24;

	if (ufs_mtk_dbg_lvl >= T_UFS_DBG_LVL_4) {
		dumpMemory4("UTRD : ", req_desc, sizeof(*req_desc));
		dumpMemory4("CMD UPIU : ", ucd_req_ptr, sizeof(*ucd_req_ptr));
	}

	switch (req) {
	case UPIU_TRANSACTION_COMMAND:
		if (ufs_mtk_dbg_lvl >= T_UFS_DBG_LVL_3) {
			__scsi_print_command(ucd_req_ptr->sc.cdb);
			print_prd(lrbp);
		}
		break;
	case UPIU_TRANSACTION_QUERY_REQ:
		if (ufs_mtk_dbg_lvl >= T_UFS_DBG_LVL_2)
			print_query_function(ucd_req_ptr);
		break;
	}
}

void ufs_mtk_print_response(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_rsp *ucd_rsp_ptr = lrbp->ucd_rsp_ptr;
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	u8 req = be32_to_cpu(lrbp->ucd_req_ptr->header.dword_0) >> 24;

	if (ufs_mtk_dbg_lvl >= T_UFS_DBG_LVL_4) {
		pr_err("response\n");
		dumpMemory4("UTRD : ", req_desc, sizeof(*req_desc));
		dumpMemory4("RSP UPIU: ", ucd_rsp_ptr, sizeof(*ucd_rsp_ptr));

		if (req == UPIU_TRANSACTION_COMMAND)
			print_prd(lrbp);
	}
}

#else

void ufs_mtk_print_request(struct ufshcd_lrb *lrbp)
{
}

void ufs_mtk_print_response(struct ufshcd_lrb *lrbp)
{
}

#endif


int ufs_mtk_query_desc(struct ufs_hba *hba, enum query_opcode opcode, enum desc_idn idn, u8 index, void *desc, int len)
{
	return ufshcd_query_descriptor(hba, opcode, idn, index, 0, desc, &len);
}

int ufs_mtk_send_uic_command(struct ufs_hba *hba, u32 cmd, u32 arg1, u32 arg2, u32 *arg3, u8 *err_code)
{
	int result;
	struct uic_command uic_cmd = {
		.command = cmd,
		.argument1 = arg1,
		.argument2 = arg2,
		.argument3 = *arg3,
	};

	result = ufshcd_send_uic_cmd(hba, &uic_cmd);

	if (err_code)
		*err_code = uic_cmd.argument2 & MASK_UIC_COMMAND_RESULT;

	if (result) {
		dev_err(hba->dev, "UIC command error: %#x\n", result);
		return -EIO;
	}
	if (cmd == UIC_CMD_DME_GET || cmd == UIC_CMD_DME_PEER_GET)
		*arg3 = uic_cmd.argument3;

	return 0;
}

int ufs_mtk_run_batch_uic_cmd(struct ufs_hba *hba, struct uic_command *cmds, int ncmds)
{
	int i;
	int err = 0;

	for (i = 0; i < ncmds; i++) {

		err = ufshcd_send_uic_cmd(hba, &cmds[i]);

		if (err) {
			dev_err(hba->dev, "ufs_mtk_run_batch_uic_cmd fail, cmd: %x, arg1: %x\n",
				cmds->command, cmds->argument1);
			/* return err; */
		}
	}

	return err;
}

int ufs_mtk_get_cmd_str_idx(char cmd)
{
	int i;

	for (i = 0; ufs_mtk_cmd_str_tbl[i].cmd != 0xFF; i++) {
		if (ufs_mtk_cmd_str_tbl[i].cmd == cmd)
			return i;
	}

	return i;
}

int ufs_mtk_enable_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp;

	if (enable) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);
		tmp = tmp | (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_DEBUGCLOCKENABLE), tmp);
	}

	return 0;
}

/**
 * ufs_mtk_advertise_quirks - advertise the known mtk UFS controller quirks
 * @hba: host controller instance
 *
 * mtk UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void ufs_mtk_advertise_hci_quirks(struct ufs_hba *hba)
{
}

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 */
int ufs_mtk_init(struct ufs_hba *hba)
{
	struct device_node *node_infracfg_ao;
	struct device_node *node_pericfg;
	int err;

	ufs_mtk_advertise_hci_quirks(hba);

	/* get ufs_mtk_mmio_base_infracfg_ao */

	ufs_mtk_mmio_base_infracfg_ao = NULL;
	node_infracfg_ao = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");

	if (node_infracfg_ao) {
		ufs_mtk_mmio_base_infracfg_ao = of_iomap(node_infracfg_ao, 0);

	    if (IS_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_infracfg_ao);
			dev_err(hba->dev, "error: ufs_mtk_mmio_base_infracfg_ao init fail\n");
			ufs_mtk_mmio_base_infracfg_ao = NULL;
		}
	} else
	    dev_err(hba->dev, "error: node_infracfg_ao init fail\n");

	/* get ufs_mtk_mmio_base_pericfg */

	ufs_mtk_mmio_base_pericfg = NULL;
	node_pericfg = of_find_compatible_node(NULL, NULL, "mediatek,pericfg");

	if (node_pericfg) {
		ufs_mtk_mmio_base_pericfg = of_iomap(node_pericfg, 0);

		if (IS_ERR(*(void **)&ufs_mtk_mmio_base_pericfg)) {
			err = PTR_ERR(*(void **)&ufs_mtk_mmio_base_pericfg);
			dev_err(hba->dev, "error: mmio_base_pericfg init fail\n");
			ufs_mtk_mmio_base_pericfg = NULL;
		}
	} else
	    dev_err(hba->dev, "error: node_pericfg init fail\n");

	if (ufs_mtk_mmio_base_infracfg_ao)
		dev_err(hba->dev, "ufs_mtk_mmio_base_infracfg_ao: %p\n", ufs_mtk_mmio_base_infracfg_ao);

	if (ufs_mtk_mmio_base_pericfg)
		dev_err(hba->dev, "ufs_mtk_mmio_base_pericfg: %p\n", ufs_mtk_mmio_base_pericfg);

	return 0;

}

int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
			      bool stage, struct ufs_pa_layer_attr *desired,
			      struct ufs_pa_layer_attr *final)
{
	struct ufs_descriptor desc;
	int err;

	if (PRE_CHANGE == stage) {  /* before power mode change */

		/* get manu ID for vendor specific configuration in the future */
		if (0 == hba->manu_id) {

			/* read device descriptor */
			desc.descriptor_idn = 0;
			desc.index = 0;

			err = ufs_mtk_query_desc(hba, UPIU_QUERY_FUNC_STANDARD_READ_REQUEST,
						      desc.descriptor_idn, desc.index,
						      desc.descriptor, sizeof(desc.descriptor));
			if (err)
				return err;

			/* get wManufacturerID */
			hba->manu_id = desc.descriptor[0x18] << 8 | desc.descriptor[0x19];
			dev_err(hba->dev, "wManufacturerID: 0x%x\n", hba->manu_id);
		}

		/* all devices use HS-G2Bx1L as the maximum gear */

		final->gear_rx = 2;
		final->gear_tx = 2;
		final->lane_rx = 1;
		final->lane_tx = 1;
		final->hs_rate = PA_HS_MODE_B;
		final->pwr_rx = FASTAUTO_MODE;
		final->pwr_tx = FASTAUTO_MODE;
	}

	return 0;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret = 0;
	u32 tmp;

	ufs_mtk_bootrom_deputy(hba);

	/* TODO: check deep stall policy */
	ufshcd_dme_get(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), &tmp);

	if (ufs_mtk_host_deep_stall_enable)   /* enable deep stall */
		tmp |= (1 << 6);
	else
		tmp &= ~(1 << 6);   /* disable deep stall */

	ufshcd_dme_set(hba, UIC_ARG_MIB(VENDOR_SAVEPOWERCONTROL), tmp);

	if (ufs_mtk_host_scramble_enable)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 1);
	else
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 0);

	if (0 != ret)
		ret = 1;

	return ret;
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	int ret = 0;
	u32 arg = 0;

	/* disable device LCC */
	ret = ufs_mtk_send_uic_command(hba, UIC_CMD_DME_SET, UIC_ARG_MIB(PA_LOCALTXLCCENABLE), 0, &arg, NULL);

	if (ret) {
		dev_err(hba->dev, "dme_setting_after_link fail\n");
		ret = 0;	/* skip error */
	}

#ifdef CONFIG_MTK_UFS_HW_CRYPTO

	/* init crypto */
	/* mt_secure_call(MTK_SIP_KERNEL_UFS_CRYPTO_INIT, 1, 0, 0); */

#endif

	/* enable unipro clock gating feature */
	ufs_mtk_enable_unipro_cg(hba, true);

	return ret;

}

int ufs_mtk_link_startup_notify(struct ufs_hba *hba, bool stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		break;
	default:
		break;
	}

	return ret;
}

/*
 * In early-porting stage, because of no bootrom, something finished by bootrom shall be finished here instead.
 * Returns:
 *  0: Successful.
 *  Non-zero: Failed.
 */
int ufs_mtk_bootrom_deputy(struct ufs_hba *hba)
{
#ifdef CONFIG_MTK_UFS_EARLY_PORTING

	u32 reg;

	if (!ufs_mtk_mmio_base_pericfg)
		return 1;

	reg = readl(ufs_mtk_mmio_base_pericfg + REG_UFS_PERICFG);
	reg = reg | (1 << REG_UFS_PERICFG_LDO_N_BIT);
	writel(reg, ufs_mtk_mmio_base_pericfg + REG_UFS_PERICFG);

	udelay(10);

	reg = readl(ufs_mtk_mmio_base_pericfg + REG_UFS_PERICFG);
	reg = reg | (1 << REG_UFS_PERICFG_RST_N_BIT);
	writel(reg, ufs_mtk_mmio_base_pericfg + REG_UFS_PERICFG);

	return 0;

#else

	return 0;

#endif
}

/* replace non-printable or non-ASCII characters with spaces */
static inline void ufs_mtk_remove_non_printable(char *val)
{
	if (!val)
		return;

	if (*val < 0x20 || *val > 0x7e)
		*val = ' ';
}

/**
 * ufs_mtk_read_string_desc - read string descriptor
 * @hba: pointer to adapter instance
 * @desc_index: descriptor index
 * @buf: pointer to buffer where descriptor would be read
 * @size: size of buf
 * @ascii: if true convert from unicode to ascii characters
 *
 * Return 0 in case of success, non-zero otherwise
 */
static int ufs_mtk_read_string_desc(struct ufs_hba *hba, int desc_index, u8 *buf,
				    u32 size, bool ascii)
{
	int err = 0;

	err = ufshcd_read_desc(hba,
			       QUERY_DESC_IDN_STRING, desc_index, buf, size);

	if (err) {
		dev_err(hba->dev, "%s: reading String Desc failed after %d retries. err = %d\n",
			__func__, QUERY_REQ_RETRIES, err);
		goto out;
	}

	if (ascii) {
		int desc_len;
		int ascii_len;
		int i;
		char *buff_ascii;

		desc_len = buf[0];
		/* remove header and divide by 2 to move from UTF16 to UTF8 */
		ascii_len = (desc_len - QUERY_DESC_HDR_SIZE) / 2 + 1;
		if (size < ascii_len + QUERY_DESC_HDR_SIZE) {
			dev_err(hba->dev, "%s: buffer allocated size is too small\n",
				__func__);
			err = -ENOMEM;
			goto out;
		}

		buff_ascii = kmalloc(ascii_len, GFP_KERNEL);
		if (!buff_ascii) {
			err = -ENOMEM;
			goto out_free_buff;
		}

		/*
		 * the descriptor contains string in UTF16 format
		 * we need to convert to utf-8 so it can be displayed
		 */
		utf16s_to_utf8s((wchar_t *)&buf[QUERY_DESC_HDR_SIZE],
				desc_len - QUERY_DESC_HDR_SIZE,
				UTF16_BIG_ENDIAN, buff_ascii, ascii_len);

		/* replace non-printable or non-ASCII characters with spaces */
		for (i = 0; i < ascii_len; i++)
			ufs_mtk_remove_non_printable(&buff_ascii[i]);

		memset(buf + QUERY_DESC_HDR_SIZE, 0,
		       size - QUERY_DESC_HDR_SIZE);
		memcpy(buf + QUERY_DESC_HDR_SIZE, buff_ascii, ascii_len);
		buf[QUERY_DESC_LENGTH_OFFSET] = ascii_len + QUERY_DESC_HDR_SIZE;
out_free_buff:
		kfree(buff_ascii);
	}
out:
	return err;
}

static int ufs_mtk_get_device_info(struct ufs_hba *hba,
				   struct ufs_device_info *card_data)
{
	int err;
	u8 model_index;
	u8 str_desc_buf[QUERY_DESC_STRING_MAX_SIZE + 1] = {0};
	u8 desc_buf[QUERY_DESC_DEVICE_MAX_SIZE];

	err = ufshcd_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf, QUERY_DESC_DEVICE_MAX_SIZE);

	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			__func__, err);
		goto out;
	}

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	card_data->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

	err = ufs_mtk_read_string_desc(hba, model_index, str_desc_buf,
				       QUERY_DESC_STRING_MAX_SIZE, ASCII_STD);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Product Name. err = %d\n",
			__func__, err);
		goto out;
	}

	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';
	strlcpy(card_data->model, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));

	/* Null terminate the model string */
	card_data->model[MAX_MODEL_LEN] = '\0';

out:
	return err;
}

void ufs_mtk_advertise_fixup_device(struct ufs_hba *hba)
{
	int err;
	struct ufs_dev_fix *f;
	struct ufs_device_info card_data;

	card_data.wmanufacturerid = 0;

	err = ufs_mtk_get_device_info(hba, &card_data);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting device info. err = %d\n",
			__func__, err);
		return;
	}

	for (f = ufs_fixups; f->quirk; f++) {
		if (((f->card.wmanufacturerid == card_data.wmanufacturerid) ||
			(f->card.wmanufacturerid == UFS_ANY_VENDOR)) &&
			(STR_PRFX_EQUAL(f->card.model, card_data.model) ||
			 !strcmp(f->card.model, UFS_ANY_MODEL)))
			hba->dev_quirks |= f->quirk;
	}

#ifdef CONFIG_MTK_UFS_DEBUG
	dev_err(hba->dev, "dev quirks: %#x\n", hba->dev_quirks);
#endif
}

#ifdef CONFIG_MTK_UFS_DEBUG
void ufs_mtk_dump_asc_ascq(struct ufs_hba *hba, u8 asc, u8 ascq)
{
	dev_err(hba->dev, "Sense Data: ASC=%#04x, ASCQ=%#04x\n", asc, ascq);

	if (0x25 == asc) {
		if (0x00 == ascq)
			dev_err(hba->dev, "Logical unit not supported!\n");
	} else if (0x29 == asc) {
		if (0x00 == ascq)
			dev_err(hba->dev, "Power on, reset, or bus device reset occupied\n");
	}
}
#endif

#ifdef CONFIG_MTK_UFS_HW_CRYPTO
void ufs_mtk_crypto_cal_dun(u32 alg_id, u32 lba, u32 *dunl, u32 *dunu)
{
	if (UFS_CRYPTO_ALGO_BITLOCKER_AES_CBC != alg_id) {
		*dunl = lba;
		*dunu = 0;
	} else {                             /* bitlocker dun use byte address */
		*dunl = (lba & 0x7FFFF) << 12;   /* byte address for lower 32 bit */
		*dunu = (lba >> (32-12)) << 12;  /* byte address for higher 32 bit */
	}
}
#endif

/**
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	"mediatek.ufs",  /* name */
	ufs_mtk_init,    /* init */
	NULL,            /* exit */
	NULL,            /* clk_scale_notify */
	NULL,            /* setup_clocks */
	NULL,            /* setup_regulators */
	NULL,            /* hce_enable_notify */
	ufs_mtk_link_startup_notify,  /* link_startup_notify */
	ufs_mtk_pwr_change_notify,    /* pwr_change_notify */
	NULL,            /* suspend */
	NULL,            /* resume */
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	return 0;
}

struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,ufs"},
	{},
};

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	.suspend	= ufshcd_pltfrm_suspend,
	.resume		= ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe	= ufs_mtk_probe,
	.remove	= ufs_mtk_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver	= {
		.name	= "ufshcd",
		.owner	= THIS_MODULE,
		.pm	= &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

module_platform_driver(ufs_mtk_pltform);

