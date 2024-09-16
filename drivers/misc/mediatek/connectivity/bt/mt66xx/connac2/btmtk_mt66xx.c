/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kthread.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include "btmtk_chip_if.h"
#include "btmtk_mt66xx_reg.h"
#include "btmtk_define.h"
#include "btmtk_main.h"

#include "conninfra.h"
#include "connsys_debug_utility.h"


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define PATCH_FILE_NUM			2
#define WMT_CMD_HDR_LEN			(4)

#define EMI_BGFSYS_MCU_START    	0
#define EMI_BGFSYS_MCU_SIZE     	0x100000 /* 1024*1024 */
#define EMI_BT_START            	(EMI_BGFSYS_MCU_START + EMI_BGFSYS_MCU_SIZE)
#define EMI_BT_SIZE             	0x129400 /* 1189*1024 */
#define EMI_DATETIME_LEN		16 // 14 bytes(ex: "20190821054545") + end of string
#define EMI_COREDUMP_MCU_DATE_OFFSET	0xE0
#define EMI_COREDUMP_BT_DATE_OFFSET	0xF0

#define POS_POLLING_RTY_LMT		100
#define IDLE_LOOP_RTY_LMT		100
#define CAL_READY_BIT_PATTERN		0x5AD02EA5
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
struct fw_patch_emi_hdr {
	uint8_t date_time[EMI_DATETIME_LEN];
	uint8_t plat[4];
	uint16_t hw_ver;
	uint16_t sw_ver;
	uint8_t emi_addr[4];
	uint32_t subsys_id;
	uint8_t reserved[14];
	uint16_t crc;
};

#if (CUSTOMER_FW_UPDATE == 1)
struct fwp_info {
	bool result;
	uint8_t status;
	uint8_t datetime[PATCH_FILE_NUM][EMI_DATETIME_LEN];
	uint8_t update_time[EMI_DATETIME_LEN];
};
#else
struct fwp_info{
	bool result;
	uint8_t datetime[PATCH_FILE_NUM][EMI_DATETIME_LEN];
};
#endif

enum FWP_LOAD_FROM {
	FWP_DIR_DEFAULT,
	FWP_DIR_UPDATE,
};

enum FWP_CHECK_STATUS {
	FWP_CHECK_SUCCESS,
	FWP_NO_PATCH,
	FWP_CRC_ERROR,
	FWP_ERROR,
	FWP_OLDER_DATETIME,
	FWP_FUNCTION_DISABLE,
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
static const uint8_t WMT_OVER_HCI_CMD_HDR[] = { 0x01, 0x6F, 0xFC, 0x00 };

#if (CUSTOMER_FW_UPDATE == 1)
static bool g_fwp_update_enable = FALSE;
static uint8_t *g_fwp_names[PATCH_FILE_NUM][2] = {
	{BIN_NAME_MCU, BIN_NAME_MCU_U},
	{BIN_NAME_BT, BIN_NAME_BT_U},
};
#else
static uint8_t *g_fwp_names[PATCH_FILE_NUM][1] = {
	{BIN_NAME_MCU},
	{BIN_NAME_BT},
};
#endif
static struct fwp_info g_fwp_info;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

extern struct btmtk_dev *g_bdev;
extern struct bt_dbg_st g_bt_dbg_st;
struct bt_base_addr bt_reg;
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#if (CUSTOMER_FW_UPDATE == 1)
static uint16_t fwp_checksume16(uint8_t *pData, uint64_t len) {
    int32_t sum = 0;

    while (len > 1) {
        sum += *((uint16_t*)pData);
        pData = pData + 2;

        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        len -= 2;
    }

    if (len) {
        sum += *((uint8_t*)pData);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

static enum FWP_CHECK_STATUS fwp_check_patch (
		uint8_t *patch_name,
		uint8_t *patch_datetime)
{
	uint8_t *p_buf = NULL;
	struct fw_patch_emi_hdr *p_patch_hdr = NULL;
	uint32_t patch_size;
	uint8_t header_szie = sizeof(struct fw_patch_emi_hdr);
	uint16_t crc;
	int32_t ret = FWP_CHECK_SUCCESS;

	/*  Read Firmware patch content */
	if(btmtk_load_code_from_bin(&p_buf, patch_name, NULL, &patch_size, 2) == -1) {
		ret = FWP_NO_PATCH;
		goto done;
	} else if(patch_size < header_szie) {
		BTMTK_ERR("%s: patch size %u error", __func__, patch_size);
		ret = FWP_ERROR;
		goto done;
	}

	/* Check patch header information */
	p_patch_hdr = (struct fw_patch_emi_hdr *)p_buf;
	strncpy(patch_datetime, p_patch_hdr->date_time, EMI_DATETIME_LEN);
	patch_datetime[EMI_DATETIME_LEN - 2] = '\0'; // 14 bytes actually

	/* Caculate crc from body patch */
	crc = fwp_checksume16(p_buf + header_szie, patch_size - header_szie);
	if(crc != p_patch_hdr->crc) {
		ret = FWP_CRC_ERROR;
	}
done:
	BTMTK_INFO("%s: patch_name[%s], ret[%d]", __func__, patch_name, ret);
	if (p_buf)
		vfree(p_buf);
	return ret;
}

static enum FWP_LOAD_FROM fwp_preload_patch(struct fwp_info *info)
{
	uint8_t i = 0;
	bool result = FWP_DIR_UPDATE;
	uint8_t status = FWP_CHECK_SUCCESS;
	uint8_t time_d[EMI_DATETIME_LEN], time_u[EMI_DATETIME_LEN];

	if(g_fwp_update_enable) {
		for(i = 0; i < PATCH_FILE_NUM; i++) {
			fwp_check_patch(g_fwp_names[i][FWP_DIR_DEFAULT], time_d);
			status = fwp_check_patch(g_fwp_names[i][FWP_DIR_UPDATE], time_u);
			if(status != FWP_CHECK_SUCCESS) {
				// if there is any error on update patch
				result = FWP_DIR_DEFAULT;
				goto done;
			} else {
				BTMTK_INFO("%s: %s, datetime[%s]", __func__, g_fwp_names[i][FWP_DIR_DEFAULT], time_d);
				BTMTK_INFO("%s: %s: datetime[%s]", __func__, g_fwp_names[i][FWP_DIR_UPDATE], time_u);
				if(strcmp(time_u, time_d) < 0) {
					// if any update patch datetime is older
					status = FWP_OLDER_DATETIME;
					result = FWP_DIR_DEFAULT;
					goto done;
				}
			}
		}
	} else {
		result = FWP_DIR_DEFAULT;
		status = FWP_FUNCTION_DISABLE;
	}
done:
	info->result = result;
	info->status = status;
	return result;
}

static void fwp_update_info(struct fwp_info *info) {
	struct timeval time;
	struct rtc_time tm;
	unsigned long local_time;
	int i;

	do_gettimeofday(&time);
	local_time = (uint32_t)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time_to_tm(local_time, &tm);
	if (snprintf(info->update_time, EMI_DATETIME_LEN, "%04d%02d%02d%02d%02d%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec) < 0) {
		BTMTK_INFO("%s: snprintf error", __func__);
		return;
	}
	info->update_time[EMI_DATETIME_LEN - 2] = '\0'; // 14 bytes actually

	BTMTK_INFO("%s: result=%s, status=%d, datetime=%s, update_time=%s", __func__,
		info->result == FWP_DIR_DEFAULT ? "FWP_DIR_DEFAULT" : "FWP_DIR_UPDATE",
		info->status, info->datetime[0], info->update_time);
}

void fwp_if_set_update_enable(int par)
{
	/* 0: disable, 1: enable*/
	g_fwp_update_enable = (par == 0) ? FALSE : TRUE;
	BTMTK_INFO("%s: set fw patch update function = %s", __func__, (par == 0) ? "DISABLE" : "ENABLE");
}

int fwp_if_get_update_info(char *buf, int max_len)
{
	// mapping to FWP_CHECK_STATUS
	char* status_info[] ={
		"none", "no new patch", "crc error", "error", "older version", "function disable"
	};

	/* return update information */
	if (snprintf(buf, max_len - 1, "result=%s\nstatus=%s\nversion=%s\nupdate_time=%s",
			g_fwp_info.result == FWP_DIR_DEFAULT ? "FAIL" : "PASS",
			status_info[g_fwp_info.status],
			g_fwp_info.datetime[0],
			g_fwp_info.update_time) < 0) {
		BTMTK_INFO("%s: snprintf error", __func__);
		return 0;
	}
	buf[strlen(buf)] = '\0';
	BTMTK_INFO("%s: %s, %d", __func__, buf, strlen(buf));
	return strlen(buf) + 1;
}
#endif

int fwp_if_get_datetime(char *buf, int max_len)
{
	#define VER_STR_LEN 60
	int i = 0, ret_len = 0;
	bool fwp_from = g_fwp_info.result;
	char *tmp = buf;

	if (PATCH_FILE_NUM * VER_STR_LEN > max_len)
		return 0;
	/* write datetime information of each patch */
	for (i = 0; i < PATCH_FILE_NUM; i++) {
		if (snprintf(tmp, VER_STR_LEN, "%s: %s\n", g_fwp_names[i][fwp_from], g_fwp_info.datetime[i]) < 0) {
			BTMTK_INFO("%s: snprintf error", __func__);
			return 0;
		}
		ret_len += strlen(tmp);
		tmp = tmp + strlen(tmp);
	}
	buf[ret_len] = '\0';
	BTMTK_INFO("%s: %s, %d", __func__, buf, strlen(buf));
	return ret_len + 1;
}

int fwp_if_get_bt_patch_path(char *buf, int max_len)
{
	#undef VER_STR_LEN
	#define VER_STR_LEN	100
	bool fwp_from = g_fwp_info.result;
	int ret = 0;

	if (VER_STR_LEN > max_len)
		return 0;
	if (fwp_from == FWP_DIR_DEFAULT) {
		ret = snprintf(buf, VER_STR_LEN, "/vendor/firmware/%s", g_fwp_names[1][fwp_from]);
	} else {
#if (CUSTOMER_FW_UPDATE == 1)
		ret = snprintf(buf, VER_STR_LEN, "/data/vendor/firmware/update/%s", g_fwp_names[1][fwp_from]);
#else
		ret = snprintf(buf, VER_STR_LEN, "");
#endif
	}

	if (ret < 0) {
		BTMTK_INFO("%s: snprintf error", __func__);
		return 0;
	}
	BTMTK_INFO("%s: %s, %d", __func__, buf, strlen(buf));
	return strlen(buf) + 1;
}

/* bgfsys_ccif_on
 *
 *    MD coex indication, BT driver should set relative control register to info
 *    MD that BT is on
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 * Todo:
 *    Use pre-mapped memory, don't know why it doesn't work
 */
void bgfsys_ccif_on(void)
{
	uint8_t *ccif_base = ioremap_nocache(0x10003300, 0x100);

	if (ccif_base == NULL) {
		BTMTK_ERR("%s: remapping ccif_base fail", __func__);
		return;
	}

	/* CONSYS_BGF_PWR_ON, 0x10003318[0] = 1'b1 */
	SET_BIT(ccif_base + 0x18, BIT(0));
	/* CONSYS_BGF_READY, 0x10003318[1] = 1'b0 */
	CLR_BIT(ccif_base + 0x18, BIT(1));
	iounmap(ccif_base);
}

/* bgfsys_ccif_off
 *
 *    MD coex indication, BT driver should set relative control register to info
 *    MD that BT is off
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 * Todo:
 *    Use pre-mapped memory, don't know why it doesn't work
 */
void bgfsys_ccif_off(void)
{
	uint8_t *ccif_base = NULL, *bgf2md_base = NULL;

	ccif_base = ioremap_nocache(0x10003300, 0x100);
	if (ccif_base == NULL) {
		BTMTK_ERR("%s: remapping ccif_base fail", __func__);
		return;
	}

	/* CONSYS_BGF_PWR_ON, 0x10003318[0] = 1'b0 */
	CLR_BIT(ccif_base + 0x18, BIT(0));
	/* CONSYS_BGF_READY, 0x10003318[1] = 1'b0 */
	CLR_BIT(ccif_base + 0x18, BIT(1));
	iounmap(ccif_base);


	bgf2md_base = ioremap_nocache(0x1025C000, 0x100);
	if (bgf2md_base == NULL) {
		BTMTK_ERR("%s: remapping bgf2md_base fail", __func__);
		return;
	}

	/* set PCCIF5 RX ACK, 0x1025C014[0:7] = 8'b1 */
	REG_WRITEL(bgf2md_base + 0x14, 0xFF);
	iounmap(bgf2md_base);
}

/* bgfsys_cal_data_backup
 *
 *    Backup BT calibration data
 *
 * Arguments:
 *    [IN] start_addr   - offset to the SYSRAM
 *    [IN] cal_data     - data buffer to backup calibration data
 *    [IN] data_len     - data length
 *
 * Return Value:
 *    N/A
 *
 */
static void bgfsys_cal_data_backup(
	uint32_t start_addr,
	uint8_t *cal_data,
	uint16_t data_len)
{
	uint32_t start_offset;

	/*
	 * The calibration data start address and ready bit address returned in WMT RF cal event
	 * should be 0x7C05XXXX from F/W's point of view (locate at ConnInfra sysram region),
	 * treat the low 3 bytes as offset to our remapped virtual base.
	 */
	start_offset = start_addr & 0x0000FFFF;
	if (start_offset > 0x1000) {
		BTMTK_ERR("Error sysram offset address start=[0x%08x]",
								start_addr);
		return;
	}

	if (!conninfra_reg_readable()) {
		int32_t ret = conninfra_is_bus_hang();
		if (ret > 0)
			BTMTK_ERR("%s: conninfra bus is hang, needs reset", __func__);
		else
			BTMTK_ERR("%s: conninfra not readable, but not bus hang ret = %d", __func__, ret);
		return;
	}

	memcpy_fromio(cal_data, (const volatile void *)(CON_REG_INFRA_SYS_ADDR + start_offset), data_len);
}

/* bgfsys_cal_data_restore
 *
 *    Restore BT calibration data to SYSRAM, after restore success,
 *    driver needs to write a special pattern '0x5AD02EA5' to ready address to
 *    info FW not to do calibration this time
 *
 * Arguments:
 *    [IN] start_addr   - offset to the SYSRAM
 *    [IN] ready_addr   - readya_address to write 0x5AD02EA5
 *    [IN] cal_data     - data buffer of calibration data
 *    [IN] data_len     - data length
 *
 * Return Value:
 *    N/A
 *
 */
static void bgfsys_cal_data_restore(uint32_t start_addr,
					 	uint32_t ready_addr,
						uint8_t *cal_data,
						uint16_t data_len)
{
	uint32_t start_offset, ready_offset;
	uint32_t ready_status = 0;

	start_offset = start_addr & 0x0000FFFF;
	ready_offset = ready_addr & 0x0000FFFF;

	if (start_offset > 0x1000 || ready_offset > 0x1000) {
		BTMTK_ERR("Error sysram offset address start=[0x%08x], ready=[0x%08x]",
							start_addr, ready_addr);
		return;
	}

	if (!conninfra_reg_readable()) {
		int32_t ret = conninfra_is_bus_hang();
		if (ret > 0)
			BTMTK_ERR("%s: conninfra bus is hang, needs reset", __func__);
		else
			BTMTK_ERR("%s: conninfra not readable, but not bus hang ret = %d", __func__, ret);
		return;
	}

	memcpy_toio((volatile void *)(CON_REG_INFRA_SYS_ADDR + start_offset), cal_data, data_len);
	/* Firmware will not do calibration again when BT func on */
	REG_WRITEL(CON_REG_INFRA_SYS_ADDR + ready_offset, CAL_READY_BIT_PATTERN);
	ready_status = REG_READL(CON_REG_INFRA_SYS_ADDR + ready_offset);
	BTMTK_DBG("Ready pattern after restore cal=[0x%08x]", ready_status);
}

/* bgfsys_check_conninfra_ready
 *
 *    wakeup conninfra and check its ready status
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
int32_t bgfsys_check_conninfra_ready(void)
{
	int32_t retry = POS_POLLING_RTY_LMT;
	uint32_t value = 0;

	/* wake up conn_infra off */
	SET_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));

	/* check ap2conn slpprot_rdy */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = CONN_INFRA_ON2OFF_SLP_PROT_ACK &
			REG_READL(CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL);
		BTMTK_DBG("ap2conn slpprot_rdy = 0x%08x", value);
		usleep_range(1000, 1100);
		retry--;
	} while (value != 0 && retry > 0);

	if (retry == 0)
		return -1;

	if (conninfra_reg_readable()) {
		/* check conn_infra off ID */
		value = REG_READL(CONN_INFRA_CFG_VERSION);
		BTMTK_DBG("connifra cfg version = 0x%08x", value);
		if (value != CONN_INFRA_CFG_ID)
			return -1;
	} else  {
		BTMTK_ERR("Conninfra is not readable");
		return -1;
	}

	return 0;
}

static void bgfsys_dump_uart_pta_pready_status(void)
{
	uint8_t *base = NULL;

	if (!conninfra_reg_readable()) {
		BTMTK_ERR("%s: conninfra bus is not readable, discard", __func__);
		return;
	}

	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0x128, 0x00020002);
	BTMTK_INFO("0x18060128 = [0x%08x]",
		REG_READL(CON_REG_SPM_BASE_ADDR + 0x128));

	BTMTK_INFO("0x18001A00 = [0x%08x]",
		REG_READL(CON_REG_INFRA_CFG_ADDR + 0xA00));

	base = ioremap_nocache(0x1800C00C, 4);
	if (base == NULL) {
		BTMTK_ERR("%s: remapping 0x18001A00 fail", __func__);
		return;
	}
	BTMTK_INFO("0x1800C00C = [0x%08x]", *base);
	iounmap(base);
}

/* bgfsys_power_on
 *
 *    BGF MCU power on sequence
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bgfsys_power_on(void)
{
	uint32_t value;
	int32_t retry = POS_POLLING_RTY_LMT;
	uint32_t delay_ms = 5;
	uint32_t mcu_idle, mcu_pc;
	uint32_t remap_addr;

	remap_addr = REG_READL(CONN_REMAP_ADDR);
	BTMTK_INFO("remap addr = 0x%08X", remap_addr);

	bgfsys_dump_uart_pta_pready_status();

	/* reset n10 cpu core */
	CLR_BIT(CONN_INFRA_RGU_BGFSYS_CPU_SW_RST, BGF_CPU_SW_RST_B);

	/* bus clock ctrl */
	conninfra_bus_clock_ctrl(CONNDRV_TYPE_BT, CONNINFRA_BUS_CLOCK_WPLL | CONNINFRA_BUS_CLOCK_BPLL, 1);

	/* enable bt function en */
	SET_BIT(CONN_INFRA_CFG_BT_PWRCTLCR0, BT_FUNC_EN_B);

	if (!conninfra_reg_readable()) {
		if (conninfra_is_bus_hang() > 0) {
			BTMTK_ERR("%s: check conninfra status fail after set CONN_INFRA_CFG_BT_PWRCTLCR0!", __func__);
			goto error;
		}
	}

	/* polling bgfsys top on power ack bits until they are asserted */
	do {
		value = BGF_ON_PWR_ACK_B &
			REG_READL(CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_ACK_ST);
		BTMTK_INFO("bgfsys on power ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != BGF_ON_PWR_ACK_B && retry > 0);

	if (0 == retry)
		goto error;

	/* polling bgfsys top off power ack bits until they are asserted */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = BGF_OFF_PWR_ACK_B &
			REG_READL(CONN_INFRA_RGU_BGFSYS_OFF_TOP_PWR_ACK_ST);
		BTMTK_INFO("bgfsys off top power ack_b = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != BGF_OFF_PWR_ACK_B && retry > 0);

	if (0 == retry)
		goto error;

	retry = POS_POLLING_RTY_LMT;
	do {
		value = BGF_OFF_PWR_ACK_S &
			REG_READL(CONN_INFRA_RGU_BGFSYS_OFF_TOP_PWR_ACK_ST);
		BTMTK_INFO("bgfsys off top power ack_s = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != BGF_OFF_PWR_ACK_S && retry > 0);

	if (0 == retry)
		goto error;

	/* disable conn2bt slp_prot rx en */
	CLR_BIT(CONN_INFRA_CONN2BT_GALS_SLP_CTL, CONN2BT_SLP_PROT_RX_EN_B);

	/* polling conn2bt slp_prot rx ack until it is cleared */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = CONN2BT_SLP_PROT_RX_ACK_B &
			REG_READL(CONN_INFRA_CONN2BT_GALS_SLP_CTL);
		BTMTK_INFO("conn2bt slp_prot rx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != 0 && retry > 0);

	if (0 == retry)
		goto error;

	/* disable conn2bt slp_prot tx en */
	CLR_BIT(CONN_INFRA_CONN2BT_GALS_SLP_CTL, CONN2BT_SLP_PROT_TX_EN_B);
	/* polling conn2bt slp_prot tx ack until it is cleared */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = CONN2BT_SLP_PROT_TX_ACK_B &
			REG_READL(CONN_INFRA_CONN2BT_GALS_SLP_CTL);
		BTMTK_INFO("conn2bt slp_prot tx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != 0 && retry > 0);

	if (0 == retry)
		goto error;

	/* disable bt2conn slp_prot rx en */
	CLR_BIT(CONN_INFRA_BT2CONN_GALS_SLP_CTL, BT2CONN_SLP_PROT_RX_EN_B);
	/* polling bt2conn slp_prot rx ack until it is cleared */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = BT2CONN_SLP_PROT_RX_ACK_B &
			REG_READL(CONN_INFRA_BT2CONN_GALS_SLP_CTL);
		BTMTK_INFO("bt2conn slp_prot rx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != 0 && retry > 0);

	if (0 == retry)
		goto error;

	/* disable bt2conn slp_prot tx en */
	CLR_BIT(CONN_INFRA_BT2CONN_GALS_SLP_CTL, BT2CONN_SLP_PROT_TX_EN_B);
	/* polling bt2conn slp_prot tx ack until it is cleared */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = BT2CONN_SLP_PROT_TX_ACK_B &
			REG_READL(CONN_INFRA_BT2CONN_GALS_SLP_CTL);
		BTMTK_INFO("bt2conn slp_prot tx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != 0 && retry > 0);

	if (0 == retry)
		goto error;

	usleep_range(400, 440);

	/* read bgfsys hw_version */
	retry = 10;
	do {
		value = REG_READL(BGF_HW_VERSION);
		BTMTK_INFO("bgfsys hw version id = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value != BGF_HW_VER_ID && retry > 0);

	if (0 == retry)
		goto error;

	/* read bgfsys fw_version */
	value = REG_READL(BGF_FW_VERSION);
	BTMTK_INFO("bgfsys fw version id = 0x%08x", value);
	if (value != BGF_FW_VER_ID)
		goto error;

	/* read bgfsys hw_code */
	value = REG_READL(BGF_HW_CODE);
	BTMTK_INFO("bgfsys hw code = 0x%08x", value);
	if (value != BGF_HW_CODE_ID)
		goto error;

	/* read and check bgfsys version id */
	value = REG_READL(BGF_IP_VERSION);
	BTMTK_INFO("bgfsys version id = 0x%08x", value);
	if (value != BGF_IP_VER_ID)
		goto error;

	/* clear con_cr_ahb_auto_dis */
	CLR_BIT(BGF_MCCR, BGF_CON_CR_AHB_AUTO_DIS);

	/* set con_cr_ahb_stop */
	REG_WRITEL(BGF_MCCR_SET, BGF_CON_CR_AHB_STOP);

	/* reset bfgsys semaphore */
	CLR_BIT(CONN_INFRA_RGU_BGFSYS_SW_RST_B, BGF_SW_RST_B);

	/* release n10 cpu core */
	SET_BIT(CONN_INFRA_RGU_BGFSYS_CPU_SW_RST, BGF_CPU_SW_RST_B);

	/* trun off BPLL & WPLL (use common API) */
	conninfra_bus_clock_ctrl(CONNDRV_TYPE_BT, CONNINFRA_BUS_CLOCK_WPLL | CONNINFRA_BUS_CLOCK_BPLL, 0);

	/*
	 * enable conn_infra bus hang detect function &
	 * bus timeout value(use common API)
	 */
	conninfra_config_setup();

	bgfsys_dump_uart_pta_pready_status();

	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));

	/*
	 * polling BGFSYS MCU sw_dbg_ctl cr to wait it becomes 0x1D1E,
	 * which indicates that the power-on part of ROM is completed.
	 */
	retry = IDLE_LOOP_RTY_LMT;
	do {
		if (conninfra_reg_readable()) {
			mcu_pc = REG_READL(CONN_MCU_PC);
			mcu_idle = REG_READL(BGF_MCU_CFG_SW_DBG_CTL);
			BTMTK_INFO("MCU sw_dbg_ctl = 0x%08x", mcu_idle);
			BTMTK_INFO("MCU pc = 0x%08x", mcu_pc);
			if (0x1D1E == mcu_idle)
				break;
		} else {
			bgfsys_power_on_dump_cr();
			return -1;
		}

		msleep(delay_ms);
		retry--;
	} while (retry > 0);

	if (retry == 0) {
		bt_dump_cif_own_cr();
		return -1;
	}

	return 0;

error:
	/* turn off clock ctrl */
	conninfra_bus_clock_ctrl(CONNDRV_TYPE_BT, CONNINFRA_BUS_CLOCK_WPLL | CONNINFRA_BUS_CLOCK_BPLL, 0);

	bgfsys_power_on_dump_cr();

	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));

	return -1;

}

/* bgfsys_power_off
 *
 *    BGF MCU power off sequence
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bgfsys_power_off(void)
{
	uint32_t value = 0;
	int32_t retry = POS_POLLING_RTY_LMT;
	int32_t ret = 0;

	ret = bgfsys_check_conninfra_ready();
	if (ret)
		return ret;

	bgfsys_dump_uart_pta_pready_status();

	/* enable bt2conn slp_prot tx en */
	SET_BIT(CONN_INFRA_BT2CONN_GALS_SLP_CTL, BT2CONN_SLP_PROT_TX_EN_B);
	/* polling bt2conn slp_prot tx ack until it is asserted */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = BT2CONN_SLP_PROT_TX_ACK_B &
			REG_READL(CONN_INFRA_BT2CONN_GALS_SLP_CTL);
		BTMTK_DBG("bt2conn slp_prot tx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry--;
	} while (value == 0 && retry > 0);

	if (retry == 0)
		ret = -1;

	/* enable bt2conn slp_prot rx en */
	SET_BIT(CONN_INFRA_BT2CONN_GALS_SLP_CTL, BT2CONN_SLP_PROT_RX_EN_B);
	/* polling bt2conn slp_prot rx ack until it is asserted */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = BT2CONN_SLP_PROT_RX_ACK_B &
			REG_READL(CONN_INFRA_BT2CONN_GALS_SLP_CTL);
		BTMTK_DBG("bt2conn slp_prot rx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry --;
	} while (value == 0 && retry > 0);

	if (retry == 0)
		ret = -2;

	/* enable conn2bt slp_prot tx en */
	SET_BIT(CONN_INFRA_CONN2BT_GALS_SLP_CTL, CONN2BT_SLP_PROT_TX_EN_B);
	/* polling conn2bt slp_prot tx ack until it is asserted */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = CONN2BT_SLP_PROT_TX_ACK_B &
			REG_READL(CONN_INFRA_CONN2BT_GALS_SLP_CTL);
		BTMTK_DBG("conn2bt slp_prot tx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry --;
	} while (value == 0 && retry > 0);

	if (retry == 0)
		ret = -2;

	/* enable conn2bt slp_prot rx en */
	SET_BIT(CONN_INFRA_CONN2BT_GALS_SLP_CTL, CONN2BT_SLP_PROT_RX_EN_B);
	/* polling conn2bt slp_prot rx ack until it is asserted */
	retry = POS_POLLING_RTY_LMT;
	do {
		value = CONN2BT_SLP_PROT_RX_ACK_B &
			REG_READL(CONN_INFRA_CONN2BT_GALS_SLP_CTL);
		BTMTK_DBG("conn2bt slp_prot rx ack = 0x%08x", value);
		usleep_range(500, 550);
		retry --;
	} while (value == 0 && retry > 0);

	if (retry == 0)
		ret = -1;

	if (ret == -2)
		conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "Power off fail");

	/* trun off BPLL & WPLL (use common API) */
	conninfra_bus_clock_ctrl(CONNDRV_TYPE_BT, CONNINFRA_BUS_CLOCK_WPLL | CONNINFRA_BUS_CLOCK_BPLL, 0);

	/* disable bt function en */
	CLR_BIT(CONN_INFRA_CFG_BT_PWRCTLCR0, BT_FUNC_EN_B);

	/* reset n10 cpu core */
	CLR_BIT(CONN_INFRA_RGU_BGFSYS_CPU_SW_RST, BGF_CPU_SW_RST_B);

	/* Disable A-die top_ck_en (use common API) */
	conninfra_adie_top_ck_en_off(CONNSYS_ADIE_CTL_FW_BT);

	/* reset bfgsys semaphore */
	CLR_BIT(CONN_INFRA_RGU_BGFSYS_SW_RST_B, BGF_SW_RST_B);

	/* clear bt_emi_req */
	SET_BIT(CONN_INFRA_CFG_EMI_CTL_BT_EMI_REQ_BT, BT_EMI_CTRL_BIT);
	CLR_BIT(CONN_INFRA_CFG_EMI_CTL_BT_EMI_REQ_BT, BT_EMI_CTRL_BIT);
	CLR_BIT(CONN_INFRA_CFG_EMI_CTL_BT_EMI_REQ_BT, BT_EMI_CTRL_BIT1);

	if (ret)
		bgfsys_power_on_dump_cr();

	bgfsys_dump_uart_pta_pready_status();

	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));

	return ret;
}

/* __download_patch_to_emi
 *
 *    Download(copy) FW to EMI
 *
 * Arguments:
 *    [IN] patch_name   - FW bin filename
 *    [IN] emi_start    - offset to EMI address for BT / MCU
 *    [IN] emi_size     - EMI region
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t __download_patch_to_emi(
		uint8_t *patch_name,
		uint32_t emi_start,
		uint32_t emi_size,
#if SUPPORT_COREDUMP
		phys_addr_t fwdate_offset,
#endif
		uint8_t *datetime)
{
	int32_t ret = 0;
	struct fw_patch_emi_hdr *p_patch_hdr = NULL;
	uint8_t *p_buf = NULL, *p_img = NULL;
	uint32_t patch_size;
	uint16_t hw_ver = 0;
	uint16_t sw_ver = 0;
	uint32_t subsys_id = 0;
	uint32_t patch_emi_offset = 0;
	phys_addr_t emi_ap_phy_base;
	uint8_t *remap_addr;

	BTMTK_INFO("%s: load binary [%s]", __func__, patch_name);
	/*  Read Firmware patch content */
	btmtk_load_code_from_bin(&p_img, patch_name, NULL, &patch_size, 10);
	if (p_img == NULL || patch_size < sizeof(struct fw_patch_emi_hdr)) {
		BTMTK_ERR("patch size %u error", patch_size);
		ret = -EINVAL;
		goto done;
	}

	/* Patch binary format:
	 * |<-EMI header: 48 Bytes->|<-BT/MCU header: 48 Bytes ->||<-patch body: X Bytes ----->|
	 */
	/* Check patch header information */
	p_buf = p_img;
	p_patch_hdr = (struct fw_patch_emi_hdr *)p_buf;


	hw_ver = p_patch_hdr->hw_ver;
	sw_ver = p_patch_hdr->sw_ver;
	subsys_id = p_patch_hdr->subsys_id;
	strncpy(datetime, p_patch_hdr->date_time, sizeof(p_patch_hdr->date_time));
	datetime[sizeof(p_patch_hdr->date_time) - 2] = '\0'; // 14 bytes actually
	BTMTK_INFO(
		"[Patch]BTime=%s,HVer=0x%04x,SVer=0x%04x,Plat=%c%c%c%c,Addr=0x%02x%02x%02x%02x,Type=%x",
		datetime,
		((hw_ver & 0x00ff) << 8) | ((hw_ver & 0xff00) >> 8),
		((sw_ver & 0x00ff) << 8) | ((sw_ver & 0xff00) >> 8),
		p_patch_hdr->plat[0], p_patch_hdr->plat[1],
		p_patch_hdr->plat[2], p_patch_hdr->plat[3],
		p_patch_hdr->emi_addr[3], p_patch_hdr->emi_addr[2],
		p_patch_hdr->emi_addr[1], p_patch_hdr->emi_addr[0],
		subsys_id);

	/* Remove EMI header:
	 * |<-BT/MCU header: 48 Bytes ->||<-patch body: X Bytes ----->|
	 */
	patch_size -= sizeof(struct fw_patch_emi_hdr);
	p_buf += sizeof(struct fw_patch_emi_hdr);

	/*
	 * The EMI entry address given in patch header should be 0xFXXXXXXX
	 * from F/W's point of view, treat the middle 2 bytes as offset,
	 * the actual phy base should be dynamically allocated and provided
	 * by conninfra driver.
	 *
	 */
	patch_emi_offset = (p_patch_hdr->emi_addr[2] << 16) |
		(p_patch_hdr->emi_addr[1] << 8);

	conninfra_get_phy_addr((uint32_t*)&emi_ap_phy_base, NULL);
	emi_ap_phy_base &= 0xFFFFFFFF;

	if ((patch_emi_offset >= emi_start) &&
	    (patch_emi_offset + patch_size < emi_start + emi_size)) {
		remap_addr = ioremap_nocache(emi_ap_phy_base + patch_emi_offset, patch_size);
		if (remap_addr) {
			memcpy_toio(remap_addr, p_buf, patch_size);
			iounmap(remap_addr);
		} else {
			BTMTK_ERR("ioremap_nocache fail!");
			ret = -EFAULT;
		}
	} else {
		BTMTK_ERR("emi_start =0x%x size=0x%x", emi_start, emi_size);
		BTMTK_ERR("Patch overflow on EMI, offset=0x%x size=0x%x",
			      patch_emi_offset, patch_size);
		ret = -EINVAL;
	}

#if SUPPORT_COREDUMP
	remap_addr = ioremap_nocache(emi_ap_phy_base + fwdate_offset, sizeof(p_patch_hdr->date_time));
	if (remap_addr) {
		memcpy_toio(remap_addr, p_patch_hdr->date_time, sizeof(p_patch_hdr->date_time));
		iounmap(remap_addr);
	} else
		BTMTK_ERR("ioremap_nocache coredump data field fail");
#endif

done:
	if (p_img)
		vfree(p_img);
	return ret;
}

/* bgfsys_mcu_rom_patch_dl
 *
 *    Download BGF MCU rom patch
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bgfsys_mcu_rom_patch_dl(enum FWP_LOAD_FROM fwp_from, struct fwp_info *info)
{
	int32_t ret = 0;
#if SUPPORT_COREDUMP
	phys_addr_t fwdate_offset = connsys_coredump_get_start_offset(CONN_DEBUG_TYPE_BT) +
				    EMI_COREDUMP_MCU_DATE_OFFSET;
#endif

	ret = bgfsys_check_conninfra_ready();
	if (ret)
		return ret;

	return __download_patch_to_emi(g_fwp_names[0][fwp_from],
				       EMI_BGFSYS_MCU_START,
				       EMI_BGFSYS_MCU_SIZE,
#if SUPPORT_COREDUMP
				       fwdate_offset,
#endif
				       info->datetime[0]);
}

/* bgfsys_mcu_rom_patch_dl
 *
 *    Download BT ram patch
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bgfsys_bt_ram_code_dl(enum FWP_LOAD_FROM fwp_from, struct fwp_info *info)
{
#if SUPPORT_COREDUMP
	phys_addr_t fwdate_offset = connsys_coredump_get_start_offset(CONN_DEBUG_TYPE_BT) +
				    EMI_COREDUMP_BT_DATE_OFFSET;
#endif

	return __download_patch_to_emi(g_fwp_names[1][fwp_from],
				       EMI_BT_START,
				       EMI_BT_SIZE,
#if SUPPORT_COREDUMP
				       fwdate_offset,
#endif
				       info->datetime[1]);
}

/* bt_hw_and_mcu_on
 *
 *    BT HW / MCU / HAL poweron/init flow
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bt_hw_and_mcu_on(void)
{
	int ret = -1;
	enum FWP_LOAD_FROM fwp_from = FWP_DIR_DEFAULT;

	memset(&g_fwp_info, '\0', sizeof(struct fwp_info));
#if (CUSTOMER_FW_UPDATE == 1)
	fwp_from = fwp_preload_patch(&g_fwp_info);
#else
	g_fwp_info.result = fwp_from;
#endif

	/*
	 * Firmware patch download (ROM patch, RAM code...)
	 * start MCU to enter idle loop after patch ready
	 */
	ret = bgfsys_mcu_rom_patch_dl(fwp_from, &g_fwp_info);
	if (ret)
		goto power_on_error;

	ret = bgfsys_bt_ram_code_dl(fwp_from, &g_fwp_info);
	if (ret)
		goto power_on_error;

#if (CUSTOMER_FW_UPDATE == 1)
	fwp_update_info(&g_fwp_info);
#endif

	/* BGFSYS hardware power on */
	if (bgfsys_power_on()) {
		BTMTK_ERR("BGFSYS power on failed!");
		ret = -EIO;
		goto power_on_error;
	}

	/* Register all needed IRQs by MCU */
	ret = bt_request_irq(BGF2AP_BTIF_WAKEUP_IRQ);
	if (ret)
		goto request_irq_error;

	bt_disable_irq(BGF2AP_BTIF_WAKEUP_IRQ);

	ret = bt_request_irq(BGF2AP_SW_IRQ);
	if (ret)
		goto request_irq_error2;

	bt_disable_irq(BGF2AP_SW_IRQ);

	btmtk_reset_init();

	if (btmtk_btif_open()) {
		ret = -EIO;
		goto bus_operate_error;
	}
	return 0;


bus_operate_error:
	bt_free_irq(BGF2AP_SW_IRQ);

request_irq_error2:
	bt_free_irq(BGF2AP_BTIF_WAKEUP_IRQ);

request_irq_error:
power_on_error:
	bgfsys_power_off();
	return ret;
}

/* bt_hw_and_mcu_off
 *
 *    BT HW / MCU / HAL poweron/deinit flow
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 */
static void bt_hw_and_mcu_off(void)
{
	BTMTK_INFO("%s", __func__);
	/* Close hardware bus interface */
	btmtk_btif_close();

	/* Free all registered IRQs */
	bt_free_irq(BGF2AP_SW_IRQ);
	bt_free_irq(BGF2AP_BTIF_WAKEUP_IRQ);

	/* BGFSYS hardware power off */
	bgfsys_power_off();
}

/* btmtk_send_wmt_reset_cmd
 *
 *    Send BT reset command (for testing)
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_send_wmt_reset_cmd(struct hci_dev *hdev)
{
	/* Support MT66xx*/
	uint8_t cmd[] =  {0x01, 0x5B, 0xFD, 0x00};

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_SKIP_VENDOR_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

#if 0
static int btmtk_send_gating_disable_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x05, 0x01, 0x03, 0x01, 0x00, 0x04 };
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x06, 0x02, 0x03, 0x02, 0x00, 0x00, 0x04 }; */
	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}

static int btmtk_send_read_conn_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x07 };
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x0A, 0x02, 0x04, 0x06, 0x00, 0x00, 0x07, 0x00, 0x8a, 0x00, 0x00 }; */
	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	BTMTK_INFO("%s done", __func__);
	return 0;
}
#endif

/* btmtk_send_blank_status_cmd
 *
 *    Send blank status to FW
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_send_blank_status_cmd(struct hci_dev *hdev, int32_t blank)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	struct bt_internal_cmd *p_inter_cmd = &bdev->internal_cmd;
	uint8_t cmd[] =  { 0x01, 0x6F, 0xFC, 0x06, 0x01, 0xF0, 0x02, 0x00, 0x03, 0x00 };
	/* uint8_t evt[] = {0x04, 0xE4, 0x06, 0x02, 0xF0, 0x02, 0x00, 0x03, 0x00}; */

	BTMTK_INFO("[InternalCmd] %s", __func__);
	down(&bdev->internal_cmd_sem);
	bdev->event_intercept = TRUE;

	// fb event: 0(screen on, FB_BLANK_UNBLANK) / 4(screen off, FB_BLANK_POWERDOWN)
	// wmt parameter: 0(screen off) / 1(screen on)
	cmd[9] = (blank == 0) ? 1 : 0;
	bdev->event_intercept = TRUE;
	p_inter_cmd->waiting_event = 0xE4;
	p_inter_cmd->pending_cmd_opcode = 0xFC6F;
	p_inter_cmd->wmt_opcode = WMT_OPCODE_0XF0;
	p_inter_cmd->result = WMT_EVT_INVALID;

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	bdev->event_intercept = FALSE;
	up(&bdev->internal_cmd_sem);
	BTMTK_INFO("[InternalCmd] %s done, result = %s", __func__, _internal_evt_result(p_inter_cmd->result));
	return 0;
}

/* _send_wmt_power_cmd
 *
 *    Send BT func on/off command to FW
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *    [IN] is_on    - indicate current action is On (TRUE) or Off (FALSE)
 *
 * Return Value:
 *     0 if success, otherwise -EIO
 *
 */
static int32_t _send_wmt_power_cmd(struct hci_dev *hdev, u_int8_t is_on)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	struct wmt_pkt wmt_cmd;
	uint8_t buffer[HCI_MAX_FRAME_SIZE];
	uint16_t param_len, pkt_len;
	struct bt_internal_cmd *p_inter_cmd = &bdev->internal_cmd;
	int ret;

	BTMTK_INFO("[InternalCmd] %s", __func__);
	down(&bdev->internal_cmd_sem);
	bdev->event_intercept = TRUE;

	wmt_cmd.hdr.dir = WMT_PKT_DIR_HOST_TO_CHIP;

	/* Support Connac 2.0 */
	wmt_cmd.hdr.opcode = WMT_OPCODE_FUNC_CTRL;
	wmt_cmd.params.u.func_ctrl_cmd.subsys = 0;
	wmt_cmd.params.u.func_ctrl_cmd.on = is_on ? 1 : 0;
	param_len = sizeof(wmt_cmd.params.u.func_ctrl_cmd);

	wmt_cmd.hdr.param_len[0] = (uint8_t)(param_len & 0xFF);
	wmt_cmd.hdr.param_len[1] = (uint8_t)((param_len >> 8) & 0xFF);

	pkt_len = HCI_CMD_HDR_LEN + WMT_CMD_HDR_LEN + param_len;
	memcpy(buffer, WMT_OVER_HCI_CMD_HDR, HCI_CMD_HDR_LEN);
	buffer[3] = WMT_CMD_HDR_LEN + param_len;
	memcpy(buffer + HCI_CMD_HDR_LEN, &wmt_cmd, WMT_CMD_HDR_LEN + param_len);

	p_inter_cmd->waiting_event = 0xE4;
	p_inter_cmd->pending_cmd_opcode = 0xFC6F;
	p_inter_cmd->wmt_opcode = WMT_OPCODE_FUNC_CTRL;
	p_inter_cmd->result = WMT_EVT_INVALID;

	btmtk_main_send_cmd(hdev, buffer, pkt_len, BTMTKUART_TX_WAIT_VND_EVT);

	ret = (p_inter_cmd->result == WMT_EVT_SUCCESS) ? 0 : -EIO;
	bdev->event_intercept = FALSE;
	up(&bdev->internal_cmd_sem);
	BTMTK_INFO("[InternalCmd] %s done, result = %s", __func__, _internal_evt_result(p_inter_cmd->result));
	return ret;
}

/* btmtk_send_wmt_power_on_cmd
 *
 *    Send BT func on
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *     0 if success, otherwise -EIO
 *
 */
int32_t btmtk_send_wmt_power_on_cmd(struct hci_dev *hdev)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	int ret = 0;

	ret = _send_wmt_power_cmd(hdev, TRUE);
	if (!ret) {
		bdev->bt_state = FUNC_ON;
		bt_notify_state(bdev);
	} else {
		if (++bdev->rst_count > 3)
			conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT,
					"power on fail more than 3 times");

		bt_dump_cpupcr(10, 5);
		bt_dump_bgfsys_debug_cr();
	}

	return ret;
}

/* btmtk_send_wmt_power_off_cmd
 *
 *    Send BT func off (cmd won't send during reset flow)
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *     0 if success, otherwise -EIO
 *
 */
int32_t btmtk_send_wmt_power_off_cmd(struct hci_dev *hdev)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	int32_t ret = 0;

	/* Do not send CMD to connsys while connsys is resetting */
	if (bdev->bt_state == FUNC_OFF) {
		BTMTK_INFO("already at off state, skip command");
		return 0;
	} else if (bdev->bt_state != RESET_START)
		ret = _send_wmt_power_cmd(hdev, FALSE);

	BTMTK_INFO("%s: Done", __func__);
	return ret;
}

/* btmtk_get_cal_data_ref
 *
 *    Send query calibration data command to FW and wait for response (event)
 *    to get calibration data (for backup calibration data purpose)
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *    [OUT] p_start_addr - start offset to SYSRAM that stores calibration data
 *    [OUT] p_ready_addr - ready address for indicating restore calibration data
 *                         successfully
 *    [OUT] p_data_len   - length of calibration data
 *
 * Return Value:
 *     0 if success, otherwise -EIO
 *
 */
static int32_t btmtk_get_cal_data_ref(
	struct hci_dev *hdev,
	uint32_t *p_start_addr,
	uint32_t *p_ready_addr,
	uint16_t *p_data_len
)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	struct wmt_pkt wmt_cmd;
	uint8_t buffer[HCI_MAX_FRAME_SIZE];
	uint16_t param_len, pkt_len;
	struct bt_internal_cmd *p_inter_cmd = &bdev->internal_cmd;
	int ret;

	BTMTK_INFO("[InternalCmd] %s", __func__);
	down(&bdev->internal_cmd_sem);
	bdev->event_intercept = TRUE;

	wmt_cmd.hdr.dir = WMT_PKT_DIR_HOST_TO_CHIP;
	wmt_cmd.hdr.opcode = WMT_OPCODE_RF_CAL;
	wmt_cmd.params.u.rf_cal_cmd.subop = 0x03;
	param_len = sizeof(wmt_cmd.params.u.rf_cal_cmd);


	wmt_cmd.hdr.param_len[0] = (uint8_t)(param_len & 0xFF);
	wmt_cmd.hdr.param_len[1] = (uint8_t)((param_len >> 8) & 0xFF);

	pkt_len = HCI_CMD_HDR_LEN + WMT_CMD_HDR_LEN + param_len;
	memcpy(buffer, WMT_OVER_HCI_CMD_HDR, HCI_CMD_HDR_LEN);
	buffer[3] = WMT_CMD_HDR_LEN + param_len;
	memcpy(buffer + HCI_CMD_HDR_LEN, &wmt_cmd, WMT_CMD_HDR_LEN + param_len);

	/* Save the necessary information to internal_cmd struct */
	p_inter_cmd->waiting_event = 0xE4;
	p_inter_cmd->pending_cmd_opcode = 0xFC6F;
	p_inter_cmd->wmt_opcode = WMT_OPCODE_RF_CAL;
	p_inter_cmd->result = WMT_EVT_INVALID;

	ret = btmtk_main_send_cmd(hdev, buffer, pkt_len, BTMTKUART_TX_WAIT_VND_EVT);

	if (ret <= 0) {
		BTMTK_ERR("Unable to get calibration event in time, start dump and reset!");
		// TODO: FW request dump & reset, need apply to all internal cmdå
		bt_trigger_reset();
		return -1;
	}

	*p_start_addr = (p_inter_cmd->wmt_event_params.u.rf_cal_evt.start_addr[3] << 24) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.start_addr[2] << 16) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.start_addr[1] << 8) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.start_addr[0]);
	*p_ready_addr = (p_inter_cmd->wmt_event_params.u.rf_cal_evt.ready_addr[3] << 24) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.ready_addr[2] << 16) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.ready_addr[1] << 8) |
			(p_inter_cmd->wmt_event_params.u.rf_cal_evt.ready_addr[0]);
	*p_data_len = (p_inter_cmd->wmt_event_params.u.rf_cal_evt.data_len[1] << 8) |
		      (p_inter_cmd->wmt_event_params.u.rf_cal_evt.data_len[0]);

	if (p_inter_cmd->result == WMT_EVT_SUCCESS)
		ret = 0;
	else {
		uint32_t offset = *p_start_addr & 0x0000FFFF;
		uint8_t *data = NULL;

		if(offset > 0x1000)
			BTMTK_ERR("Error calibration offset (%d)", offset);
		else {
			data = (uint8_t *)(CON_REG_INFRA_SYS_ADDR + offset);

			BTMTK_ERR("Calibration fail, dump calibration data");
			if(*p_data_len < BT_CR_DUMP_BUF_SIZE)
				bt_dump_memory8(data, *p_data_len);
			else
				BTMTK_ERR("get wrong calibration length [%d]", *p_data_len);
		}
		ret = -EIO;
	}

	/* Whether succeed or not, no one is waiting, reset the flag here */
	bdev->event_intercept = FALSE;
	up(&bdev->internal_cmd_sem);
	BTMTK_INFO("[InternalCmd] %s done, result = %s", __func__, _internal_evt_result(p_inter_cmd->result));
	return ret;
}

/* btmtk_send_calibration_cmd
 *
 *    Check calibration cache and send query calibration data command if
 *    cache is not available
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_send_calibration_cmd(struct hci_dev *hdev)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	uint32_t cal_data_start_addr = 0;
	uint32_t cal_data_ready_addr = 0;
	uint16_t cal_data_len = 0;
	uint8_t *p_cal_data = NULL;
	int ret = 0;

	if (bdev->cal_data.p_cache_buf) {
		BTMTK_DBG("calibration cache has data, no need to recal");
		return 0;
	}

	/*
	 * In case that we did not have a successful pre-calibration and no
	 * cached data to restore, the first BT func on will trigger calibration,
	 * but it is most likely to be failed due to Wi-Fi is off.
	 *
	 * If BT func on return success and we get here, anyway try to backup
	 * the calibration data, any failure during backup should be ignored.
	 */

	/* Get calibration data reference for backup */
	ret = btmtk_get_cal_data_ref(hdev, &cal_data_start_addr,
				    &cal_data_ready_addr,
				    &cal_data_len);
	if (ret)
		BTMTK_ERR("Get cal data ref failed!");
	else {
		BTMTK_DBG(
			"Get cal data ref: saddr(0x%08x) raddr(0x%08x) len(%d)",
			cal_data_start_addr, cal_data_ready_addr, cal_data_len);

		/* Allocate a cache buffer to backup the calibration data in driver */
		if (cal_data_len) {
			p_cal_data = kmalloc(cal_data_len, GFP_KERNEL);
			if (p_cal_data) {
				bgfsys_cal_data_backup(cal_data_start_addr, p_cal_data, cal_data_len);
				bdev->cal_data.p_cache_buf = p_cal_data;
				bdev->cal_data.cache_len = cal_data_len;
				bdev->cal_data.start_addr = cal_data_start_addr;
				bdev->cal_data.ready_addr = cal_data_ready_addr;
			} else
				BTMTK_ERR("Failed to allocate cache buffer for backup!");
		} else
			BTMTK_ERR(
				"Abnormal cal data length! something error with F/W!");
	}
	return ret;
}


/* btmtk_set_power_on
 *
 *    BT On flow including all steps exclude BT func on commaond
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *    [IN]  for_precal - power on in pre-calibation flow, and don't
 *			 call coninfra_pwr_on if the value is TRUE
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
int32_t btmtk_set_power_on(struct hci_dev *hdev, u_int8_t for_precal)
{
	int ret;
	int sch_ret = -1;
	struct sched_param sch_param;
#if SUPPORT_BT_THREAD
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
#endif

	/*
	 *  1. ConnInfra hardware power on (Must be the first step)
	 *
	 *  We will block in this step if conninfra driver find that the pre-calibration
	 *  has not been performed, conninfra driver should guarantee to trigger
	 *  pre-calibration procedure first:
	 *    - call BT/Wi-Fi registered pwr_on_cb and do_cal_cb
	 *  then return from this API after 2.4G calibration done.
	 */
	if (!for_precal)
	{
		if (conninfra_pwr_on(CONNDRV_TYPE_BT)) {
			BTMTK_ERR("ConnInfra power on failed!");
			ret = -EIO;
			goto conninfra_error;
		}
	}

	BTMTK_DBG("%s: wait halt_sem...", __func__);
	down(&bdev->halt_sem);
	BTMTK_DBG("%s: wait halt_sem finish...", __func__);

	/* record current bt state for restoring orginal state after pre-cal */
	bdev->bt_precal_state = g_bdev->bt_state;

	/* state check before power on */
	if (bdev->bt_state == FUNC_ON) {
		BTMTK_INFO("BT is already on, skip");
		up(&bdev->halt_sem);
		return 0;
	} else if (bdev->bt_state != FUNC_OFF) {
		BTMTK_ERR("BT is not at off state, uanble to on [%d]!",
							bdev->bt_state);
		up(&bdev->halt_sem);
		return -EIO;
	}
	BTMTK_INFO("%s: CONNAC20_CHIPID[%d], bdev[0x%p], bt_state[%d]!",
		__func__, CONNAC20_CHIPID, bdev, bdev->bt_state);

	bdev->bt_state = TURNING_ON;

	/* 2. MD coex ccif on */
	bgfsys_ccif_on();

	/* 3. clear coredump */
#if SUPPORT_COREDUMP
	if (!bdev->coredump_handle)
		BTMTK_ERR("Coredump handle is NULL\n");
	else
		connsys_coredump_clean(bdev->coredump_handle);
#endif

	/* 4. BGFSYS hardware and MCU bring up, BT RAM code ready */
	ret = bt_hw_and_mcu_on();
	if (ret) {
		BTMTK_ERR("BT hardware and MCU on failed!");
		goto mcu_error;
	}


	/* 5. initialize bdev variables */
	if (bdev->evt_skb)
		kfree_skb(bdev->evt_skb);
	bdev->evt_skb = NULL;

	bdev->event_intercept = FALSE;
	bdev->internal_cmd.waiting_event = 0x00;
	bdev->internal_cmd.pending_cmd_opcode = 0x0000;
	bdev->internal_cmd.wmt_opcode = 0x00;
	bdev->internal_cmd.result = WMT_EVT_INVALID;

	bdev->rx_ind = FALSE;
	bdev->psm.state = PSM_ST_SLEEP;
	bdev->psm.sleep_flag = FALSE;
	bdev->psm.wakeup_flag = FALSE;
	bdev->psm.result = 0;
	bdev->psm.force_on = FALSE;

#if (USE_DEVICE_NODE == 1)
	btmtk_rx_flush();
#endif

#if SUPPORT_BT_THREAD
	/* 6. Create TX thread with PS state machine */
	skb_queue_purge(&bdev->tx_queue);
	init_waitqueue_head(&bdev->tx_waitq);
	bdev->tx_thread = kthread_create(btmtk_tx_thread, bdev, "bt_tx_ps");
	if (IS_ERR(bdev->tx_thread)) {
		BTMTK_DBG("btmtk_tx_thread failed to start!");
		ret = PTR_ERR(bdev->tx_thread);
		goto thread_create_error;
	}

	if(g_bt_dbg_st.rt_thd_enable) {
		sch_param.sched_priority = MAX_RT_PRIO - 20;
		sch_ret = sched_setscheduler(bdev->tx_thread, SCHED_FIFO, &sch_param);
		BTMTK_INFO("sch_ret = %d", sch_ret);
		if (sch_ret != 0)
			BTMTK_INFO("set RT to workqueue failed");
		else
			BTMTK_INFO("set RT to workqueue succeed");
	}
	wake_up_process(bdev->tx_thread);
#endif

	/*
	 * 7. Calibration data restore
	 * If we have a cache of calibration data, restore it to Firmware sysram,
	 * otherwise, BT func on will trigger calibration again.
	 */
	if (bdev->cal_data.p_cache_buf)
		bgfsys_cal_data_restore(bdev->cal_data.start_addr,
					bdev->cal_data.ready_addr,
					bdev->cal_data.p_cache_buf,
					bdev->cal_data.cache_len);

#if SUPPORT_COREDUMP
	if (!bdev->coredump_handle)
		BTMTK_ERR("Coredump handle is NULL\n");
	else
		connsys_coredump_setup_dump_region(bdev->coredump_handle);
#endif

	bt_enable_irq(BGF2AP_SW_IRQ);

	/* 8. init cmd queue and workqueue */
#if (DRIVER_CMD_CHECK == 1)
	cmd_list_initialize();
	cmd_workqueue_init();
	bdev->cmd_timeout_check = FALSE;
#endif
	dump_queue_initialize();

	/* 9. send WMT cmd to set BT on */
	ret = btmtk_send_wmt_power_on_cmd(hdev);
	if (ret) {
		BTMTK_ERR("btmtk_send_wmt_power_on_cmd fail");
		goto wmt_power_on_error;
	}

	/* Set utc_sync & blank_state cmd */
	btmtk_send_utc_sync_cmd();
	btmtk_send_blank_status_cmd(hdev, bdev->blank_state);

	/* clear reset count if power on success */
	bdev->rst_level = RESET_LEVEL_NONE;
	bdev->rst_count = 0;

	up(&bdev->halt_sem);

	return 0;

wmt_power_on_error:
	wake_up_interruptible(&bdev->tx_waitq);
	kthread_stop(bdev->tx_thread);
	bdev->tx_thread = NULL;
#if (DRIVER_CMD_CHECK == 1)
	cmd_workqueue_exit();
	cmd_list_destory();
	bdev->cmd_timeout_check = FALSE;
#endif

thread_create_error:
	bt_hw_and_mcu_off();

mcu_error:
	bgfsys_ccif_off();
	if (!for_precal)
		conninfra_pwr_off(CONNDRV_TYPE_BT);
	up(&bdev->halt_sem);

conninfra_error:
	bdev->bt_state = FUNC_OFF;
	return ret;
}

/* btmtk_set_power_off
 *
 *    BT off flow
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *    [IN]  for_precal - power on in pre-calibation flow, and don't
 *			 call coninfra_pwr_off if the value is TRUE
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_set_power_off(struct hci_dev *hdev, u_int8_t for_precal)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	BTMTK_INFO("%s", __func__);

	down(&bdev->halt_sem);

	if (bdev->bt_state == FUNC_OFF) {
		BTMTK_WARN("Alread in off state, skip");
		up(&bdev->halt_sem);
		return 0;
	/* Do not power off while chip reset thread is doing coredump */
	} else if ((bdev->bt_state == FUNC_ON || bdev->bt_state == TURNING_ON)
		   && bdev->rst_level != RESET_LEVEL_NONE) {
		BTMTK_WARN("BT is coredump, ignore stack power off");
		up(&bdev->halt_sem);
		return 0;
	}

	/* 1. Send WMT cmd to set BT off */
	btmtk_send_wmt_power_off_cmd(hdev);

	/* 2. Stop TX thread */
#if SUPPORT_BT_THREAD
	if (bdev->tx_thread) {
		wake_up_interruptible(&bdev->tx_waitq);
		kthread_stop(bdev->tx_thread);
	}
	bdev->tx_thread = NULL;
	skb_queue_purge(&bdev->tx_queue);
#endif

	/* 3. BGFSYS hardware and MCU shut down */
	bt_hw_and_mcu_off();
	BTMTK_DBG("BT hardware and MCU off!");

	/* 4. MD Coex ccif */
	bgfsys_ccif_off();

	/* 5. close cmd queue and workqueue */
#if (DRIVER_CMD_CHECK == 1)
	cmd_workqueue_exit();
	cmd_list_destory();
	bdev->cmd_timeout_check = FALSE;
#endif

	bdev->bt_state = FUNC_OFF;

	up(&bdev->halt_sem);
	/* 6. ConnInfra hardware power off */
	if (!for_precal)
		conninfra_pwr_off(CONNDRV_TYPE_BT);

	/* Delay sending HW error to stack if it's whole chip reset,
	 * we have to wait conninfra power on then send message or
	 * stack triggers BT on will fail bcz conninfra is not power on
	 */
	if (bdev->rst_level != RESET_LEVEL_0)
		bt_notify_state(bdev);

	BTMTK_INFO("ConnInfra power off!");

	return 0;
}

/* btmtk_set_sleep
 *
 *    Request driver to enter sleep mode (FW own), and restore the state from
 *    btmtk_set_wakeup (this api makes driver stay at wakeup all the time)
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_set_sleep(struct hci_dev *hdev, uint8_t need_wait)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

#if SUPPORT_BT_THREAD
	bdev->psm.sleep_flag = TRUE;
	wake_up_interruptible(&bdev->tx_waitq);

	if (!need_wait)
		return 0;

	if (!wait_for_completion_timeout(&bdev->psm.comp, msecs_to_jiffies(1000))) {
		BTMTK_ERR("[PSM]Timeout for BGFSYS to enter sleep!");
		bdev->psm.sleep_flag = FALSE;
		return -1;
	}

	BTMTK_DBG("[PSM]sleep return %s, sleep(%d), wakeup(%d)",
		      (bdev->psm.result == 0) ? "success" : "failure",
		      bdev->psm.sleep_flag, bdev->psm.wakeup_flag);

	return 0;
#else
	BTMTK_ERR("%s: [PSM] Doesn't support non-thread mode !", __func__);
	return -1;
#endif


}

/* btmtk_set_wakeup
 *
 *    Force BT driver/FW awake all the time
 *
 *
 * Arguments:
 *    [IN]  hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_set_wakeup(struct hci_dev *hdev)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

#if SUPPORT_BT_THREAD
	wake_up_interruptible(&bdev->tx_waitq);
#else
	BTMTK_ERR("%s: [PSM] Doesn't support non-thread mode !", __func__);
	return -1;
#endif

	return 0;
}
