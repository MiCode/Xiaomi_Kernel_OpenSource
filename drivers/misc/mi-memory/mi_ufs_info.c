
#include <soc/qcom/socinfo.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_common.h>
#include <asm/unaligned.h>
#include "mi_memory_sysfs.h"
#include "mem_interface.h"
#include "../../scsi/ufs/ufs-qcom.h"
#include "../../scsi/mi_ufs/mi-ufshcd.h"

#define SCSI_LUN 		0

static DEFINE_MUTEX(health_info_mutex);
static struct scsi_device *sdev = NULL;

enum field_width {
	BYTE	= 1,
	WORD	= 2,
	DWORD   = 4,
};

struct desc_field_offset {
	char *name;
	int offset;
	enum field_width width_byte;
};

static void look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return;
	sdev = scsi_device_lookup(shost, 0, 0, lun);

	pr_info("[mi_ufs_info] scsi device proc name is %s\n", sdev->host->hostt->proc_name);

	scsi_device_put(sdev);

	scsi_host_put(shost);
}

struct ufs_hba *get_ufs_hba_data(void)
{
	if(!sdev)
		look_up_scsi_device(SCSI_LUN);

	return shost_priv(sdev->host);
}

static struct scsi_device *get_ufs_sdev_data(void)
{
	if(!sdev)
		look_up_scsi_device(SCSI_LUN);

	return sdev;
}

u16 get_ufs_id(void)
{
	u16 ufs_id = 0;

	struct ufs_hba *hba = get_ufs_hba_data();

	ufs_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_MANF_ID, &ufs_id, 2);

	return ufs_id;
}

static int ufshcd_read_desc(struct ufs_hba *hba, enum desc_idn desc_id, int desc_index, void *buf, u32 size)
{
	int ret = 0;

	pm_runtime_get_sync(hba->dev);
	ret = mi_ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
	pm_runtime_put_sync(hba->dev);

	return ret;
}

static ssize_t dump_health_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 value = 0;
	int count = 0, i = 0;
	u8 buff_len = 0;
	u8 *desc_buf = NULL;

	struct ufs_hba *hba = get_ufs_hba_data();

	struct desc_field_offset health_desc_field_name[] = {
		{"bLength", 			HEALTH_DESC_PARAM_LEN, 				BYTE},
		{"bDescriptorType", 	HEALTH_DESC_PARAM_TYPE, 			BYTE},
		{"bPreEOLInfo", 		HEALTH_DESC_PARAM_EOL_INFO, 		BYTE},
		{"bDeviceLifeTimeEstA", HEALTH_DESC_PARAM_LIFE_TIME_EST_A, 	BYTE},
		{"bDeviceLifeTimeEstB", HEALTH_DESC_PARAM_LIFE_TIME_EST_B, 	BYTE},
	};

	struct desc_field_offset *tmp = NULL;

	ufs_read_desc_param(hba, QUERY_DESC_IDN_HEALTH, 0, HEALTH_DESC_PARAM_LEN, &buff_len, BYTE);

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		count += snprintf((buf + count), PAGE_SIZE, "get health info fail\n");
		return count;
	}

	ufshcd_read_desc(hba, QUERY_DESC_IDN_HEALTH, 0, desc_buf, buff_len);

	for (i = 0; i < ARRAY_SIZE(health_desc_field_name); ++i) {
		u8 *ptr = NULL;

		tmp = &health_desc_field_name[i];

		ptr = desc_buf + tmp->offset;

		switch (tmp->width_byte) {
			case BYTE:
				value = (u16)(*ptr);
				break;
			case WORD:
				value = *(u16 *)(ptr);
				break;
			default:
				value = (u16)(*ptr);
				break;
		}

		count += snprintf((buf + count), PAGE_SIZE, "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
			tmp->offset, tmp->name, value);
	}

	kfree(desc_buf);

	return count;

}

static DEVICE_ATTR_RO(dump_health_desc);

static ssize_t dump_string_desc_serial_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 ser_number[128] = { 0 };
	int i = 0, count = 0;

	struct ufs_hba *hba = get_ufs_hba_data();

	ufs_get_string_desc(hba, &ser_number, sizeof(ser_number), DEVICE_DESC_PARAM_SN, SD_RAW);

	count += snprintf((buf + count), PAGE_SIZE, "serial:");

	for (i = 2; i <  ser_number[QUERY_DESC_LENGTH_OFFSET]; i += 2) {
		count += snprintf((buf + count), PAGE_SIZE, "%02x%02x", ser_number[i], ser_number[i+1]);
	}

	count += snprintf((buf + count), PAGE_SIZE, "\n");

	return count;
}

static DEVICE_ATTR_RO(dump_string_desc_serial);

static ssize_t dump_device_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0, count = 0;
	u32 value = 0;
	u8 buff_len = 0;
	u8 *desc_buf = NULL;

	struct ufs_hba *hba = get_ufs_hba_data();

	struct desc_field_offset device_desc_field_name[] = {
		{"bLength", 			DEVICE_DESC_PARAM_LEN, 		BYTE},
		{"bDescriptorType", 	DEVICE_DESC_PARAM_TYPE, 	BYTE},
		{"bDevice",				DEVICE_DESC_PARAM_DEVICE_TYPE, BYTE},
		{"bDeviceClass",		DEVICE_DESC_PARAM_DEVICE_CLASS, BYTE},
		{"bDeviceSubClass",		DEVICE_DESC_PARAM_DEVICE_SUB_CLASS, BYTE},
		{"bProtocol",			DEVICE_DESC_PARAM_PRTCL, BYTE},
		{"bNumberLU",			DEVICE_DESC_PARAM_NUM_LU, BYTE},
		{"bNumberWLU",			DEVICE_DESC_PARAM_NUM_WLU, BYTE},
		{"bBootEnable",			DEVICE_DESC_PARAM_BOOT_ENBL, BYTE},
		{"bDescrAccessEn",		DEVICE_DESC_PARAM_DESC_ACCSS_ENBL, BYTE},
		{"bInitPowerMode",		DEVICE_DESC_PARAM_INIT_PWR_MODE, BYTE},
		{"bHighPriorityLUN",	DEVICE_DESC_PARAM_HIGH_PR_LUN, BYTE},
		{"bSecureRemovalType",	DEVICE_DESC_PARAM_SEC_RMV_TYPE, BYTE},
		{"bSecurityLU",			DEVICE_DESC_PARAM_SEC_LU, BYTE},
		{"Reserved",			DEVICE_DESC_PARAM_BKOP_TERM_LT, BYTE},
		{"bInitActiveICCLevel",	DEVICE_DESC_PARAM_ACTVE_ICC_LVL, BYTE},
		{"wSpecVersion",		DEVICE_DESC_PARAM_SPEC_VER, WORD},
		{"wManufactureDate",	DEVICE_DESC_PARAM_MANF_DATE, WORD},
		{"iManufactureName",	DEVICE_DESC_PARAM_MANF_NAME, BYTE},
		{"iProductName",		DEVICE_DESC_PARAM_PRDCT_NAME, BYTE},
		{"iSerialNumber",		DEVICE_DESC_PARAM_SN, BYTE},
		{"iOemID",				DEVICE_DESC_PARAM_OEM_ID, BYTE},
		{"wManufactureID",		DEVICE_DESC_PARAM_MANF_ID, WORD},
		{"bUD0BaseOffset",		DEVICE_DESC_PARAM_UD_OFFSET, BYTE},
		{"bUDConfigPLength",	DEVICE_DESC_PARAM_UD_LEN, BYTE},
		{"bDeviceRTTCap",		DEVICE_DESC_PARAM_RTT_CAP, BYTE},
		{"wPeriodicRTCUpdate",	DEVICE_DESC_PARAM_FRQ_RTC, WORD},
		{"bUFSFeaturesSupport", DEVICE_DESC_PARAM_UFS_FEAT, BYTE},
		{"bFFUTimeout", 		DEVICE_DESC_PARAM_FFU_TMT, BYTE},
		{"bQueueDepth", 		DEVICE_DESC_PARAM_Q_DPTH, BYTE},
		{"wDeviceVersion", 		DEVICE_DESC_PARAM_DEV_VER, WORD},
		{"bNumSecureWpArea", 	DEVICE_DESC_PARAM_NUM_SEC_WPA, BYTE},
		{"dPSAMaxDataSize", 	DEVICE_DESC_PARAM_PSA_MAX_DATA, DWORD},
		{"bPSAStateTimeout", 	DEVICE_DESC_PARAM_PSA_TMT, BYTE},
		{"iProductRevisionLevel", DEVICE_DESC_PARAM_PRDCT_REV, BYTE},
	};

	struct desc_field_offset *tmp = NULL;

	ufs_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_LEN, &buff_len, BYTE);

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		count += snprintf((buf + count), PAGE_SIZE, "get desc info fail\n");
		return count;
	}

	ufshcd_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf, buff_len);

	for (i = 0; i < ARRAY_SIZE(device_desc_field_name); ++i) {
		u8 *ptr = NULL;

		tmp = &device_desc_field_name[i];

		ptr = desc_buf + tmp->offset;

		switch (tmp->width_byte) {
			case BYTE:
				value = (u32)(*ptr);
				break;
			case WORD:
				value = (u32)get_unaligned_be16(ptr);
				break;
			case DWORD:
				value = (u32)get_unaligned_be32(ptr);
				break;
			default:
				value = (u32)(*ptr);
				break;
		}

		count += snprintf((buf + count), PAGE_SIZE, "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
					tmp->offset, tmp->name, value);
	}

	kfree(desc_buf);

	return count;
}

static DEVICE_ATTR_RO(dump_device_desc);

static ssize_t show_hba_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t count = 0;

	struct ufs_hba *hba = get_ufs_hba_data();
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	count += snprintf((buf + count), PAGE_SIZE, "hba->outstanding_tasks = 0x%x\n", (u32)hba->outstanding_tasks);
	count += snprintf((buf + count), PAGE_SIZE, "hba->outstanding_reqs = 0x%x\n", (u32)hba->outstanding_reqs);

	count += snprintf((buf + count), PAGE_SIZE, "hba->capabilities = 0x%x\n", hba->capabilities);
	count += snprintf((buf + count), PAGE_SIZE, "hba->nutrs = %d\n", hba->nutrs);
	count += snprintf((buf + count), PAGE_SIZE, "hba->nutmrs = %d\n", hba->nutmrs);
	count += snprintf((buf + count), PAGE_SIZE, "hba->ufs_version = 0x%x\n", hba->ufs_version);
	count += snprintf((buf + count), PAGE_SIZE, "hba->irq = 0x%x\n", hba->irq);
	count += snprintf((buf + count), PAGE_SIZE, "hba->auto_bkops_enabled = %d\n", hba->auto_bkops_enabled);

	count += snprintf((buf + count), PAGE_SIZE, "hba->ufshcd_state = 0x%x\n", hba->ufshcd_state);
	count += snprintf((buf + count), PAGE_SIZE, "hba->clk_gating.state = 0x%x\n", hba->clk_gating.state);
	count += snprintf((buf + count), PAGE_SIZE, "hba->eh_flags = 0x%x\n", hba->eh_flags);
	count += snprintf((buf + count), PAGE_SIZE, "hba->intr_mask = 0x%x\n", hba->intr_mask);
	count += snprintf((buf + count), PAGE_SIZE, "hba->ee_ctrl_mask = 0x%x\n", hba->ee_ctrl_mask);

	/* HBA Errors */
	count += snprintf((buf + count), PAGE_SIZE, "hba->errors = 0x%x\n", hba->errors);
	count += snprintf((buf + count), PAGE_SIZE, "hba->uic_error = 0x%x\n", hba->uic_error);
	count += snprintf((buf + count), PAGE_SIZE, "hba->saved_err = 0x%x\n", hba->saved_err);
	count += snprintf((buf + count), PAGE_SIZE, "hba->saved_uic_err = 0x%x\n", hba->saved_uic_err);
	count += snprintf((buf + count), PAGE_SIZE, "hibern8_exit_cnt = %d\n", hba->ufs_stats.hibern8_exit_cnt);
	
	/* uic specific errors */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_pa_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_PA_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_dl_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_DL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_nl_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_NL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_tl_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_TL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_dme_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_DME_ERR].cnt);
	
	/* fatal errors */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_auto_hibern8_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_AUTO_HIBERN8_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_fatal_error_cnt= 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_FATAL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_link_startup_fail_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_LINK_STARTUP_FAIL].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_resume_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_RESUME_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_suspend_error_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_SUSPEND_ERR].cnt);
	
	/* abnormal events */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_device_reset_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_DEV_RESET].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_host_reset_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_HOST_RESET].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_abort_cnt = 0x%x\n",
			hba->ufs_stats.event[UFS_EVT_ABORT].cnt);

	/* PA Errors */
	count += snprintf((buf + count), PAGE_SIZE, "pa_err_cnt_total = %d\n", host->ufs_stats.pa_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_0_err_cnt = %d\n", host->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_0]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_1_err_cnt = %d\n", host->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_1]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_2_err_cnt = %d\n", host->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_2]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_3_err_cnt = %d\n", host->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_3]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_line_reset_err_cnt = %d\n", host->ufs_stats.pa_err_cnt[UFS_EC_PA_LINE_RESET]);

	/* DL Errors */
	count += snprintf((buf + count), PAGE_SIZE, "dl_err_cnt_total = %d\n",
		host->ufs_stats.dl_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE, "dl_nac_received_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_RECEIVED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_tcx_replay_timer_expired_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_afcx_request_timer_expired_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_fcx_protection_timer_expired_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_crc_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_CRC_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dll_rx_buffer_overflow_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_RX_BUFFER_OVERFLOW]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_max_frame_length_exceeded_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_wrong_sequence_number_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_WRONG_SEQUENCE_NUMBER]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_afc_frame_syntax_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_nac_frame_syntax_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_eof_syntax_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_EOF_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_frame_syntax_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_bad_ctrl_symbol_type_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_pa_init_err_cnt = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_INIT_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_pa_error_ind_received = %d\n",
		host->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_ERROR_IND_RECEIVED]);

	/* DME Errors */
	count += snprintf((buf + count), PAGE_SIZE, "dme_err_cnt = %d\n", host->ufs_stats.dme_err_cnt);

	return count;
}

static DEVICE_ATTR_RO(show_hba);

/**
get toshiba hr inquiry
 */
static int scsi_hr_inquiry(struct scsi_device *sdev, char *hr_inq, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!hr_inq)
		return -EINVAL;

	cmd[0] = INQUIRY;
	cmd[1] = 0x69;		/* EVPD */
	cmd[2] = 0xC0;
	cmd[3] = len >> 8;
	cmd[4] = len & 0xff;
	cmd[5] = 0;		/* Control byte */

	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, hr_inq,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get hr_inquiry result error 0x%x\n", result);
		return -EIO;
	}

	/* Sanity check that we got the page back that we asked for */
	if (hr_inq[1] != 0xC0)
		pr_err("ufs: hr_inruiry data error\n");

	return 0;
}

/**
get sandisk device report
 */
static int scsi_sdr(struct scsi_device *sdev, char *sdr, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!sdr)
		return -EINVAL;

	cmd[0] = READ_BUFFER;
	cmd[1] = 0x01;		/* mode vendor specific*/
	cmd[2] = 0x01;		/* buffer ID*/

	cmd[3] = 0x7D;
	cmd[4] = 0x9C;
	cmd[5] = 0x69;

	cmd[6] = 0x00;
	cmd[7] = 0x02;
	cmd[8] = 0x00;

	cmd[9] = 0;		/* Control byte */

	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, sdr,
				  len, NULL, 30 * HZ, 3, NULL);

	if (result) {
		pr_err("ufs: get sdr result error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}

/**
get micron hr
 */
static int scsi_mhr(struct scsi_device *sdev, char *hr, int len)
{
	int result;
	unsigned char write_buffer[16] = {0x3B, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x00};
	unsigned char read_buffer[16] = {0x3C, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};

	char VU[0x2c] = {0};

	VU[0] = 0xE7;
	VU[1] = 0x40;
	VU[3] = 0x10;
	VU[4] = 0x01;

	if (!hr)
		return -EINVAL;

	result = scsi_exec_req(sdev, write_buffer, DMA_TO_DEVICE, VU,
				  0x2c, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr write buffer  error 0x%x\n", result);
		return -EIO;
	}
	result = scsi_exec_req(sdev, read_buffer, DMA_FROM_DEVICE, hr,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr read buffer  error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}

/**
get samsung osv
 */
static int scsi_osv(struct scsi_device *sdev, char *osv, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!osv)
		return -EINVAL;

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x40;

	cmd[4] = 0x01;
	cmd[5] = 0x0c;

	cmd[15] = 0x1c;

	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, osv,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get osv result error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}

/**
get skhynix hr
 */

uint8_t  *hynix_v6_uMCP_list[] = {
	"H9QT1GGBN6X129",
};
#define PRODUCT_ID_SIZE	16
#define VENDOR_ID_SIZE	8

static int scsi_sk_hr(struct scsi_device *sdev, char *buff, int len)
{
	int result;
	unsigned char cmd[16] = {0};
	struct ufs_hba *hba = NULL;
	char *parse_inquiry = NULL;
	uint8_t product_id[PRODUCT_ID_SIZE + 1] = {0};

	if (!buff)
		return -EINVAL;

	if (sdev->host->hostt->proc_name == NULL) {
		pr_err("%s %d proc_name is NULL\n", __func__, __LINE__);
		return -1;
	}
	if (strncmp(sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
		pr_err("%s %d proc_name is not UFSHCD\n", __func__, __LINE__);
		return -1;
	}
	hba = shost_priv(sdev->host);
	parse_inquiry = hba->sdev_ufs_device->inquiry + 8;  /*be careful about the inquiry formate*/
	parse_inquiry += VENDOR_ID_SIZE;
	memcpy(product_id, parse_inquiry, PRODUCT_ID_SIZE);
	pr_err("%s %d proc_name: %s. len: 0x%x\n", __func__, __LINE__, product_id, strlen(product_id));

	cmd[0] = 0xD0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x03;
	cmd[2] = 0x58;

	cmd[11] = 0x52;

	if (!strncmp(product_id, hynix_v6_uMCP_list[0], strlen(hynix_v6_uMCP_list[0])))
		cmd[11] = 0x2c; /*for V7 hynix Voyager*/
	else
		cmd[11] = 0x52; /*for V6 hynix Voyager*/
	pr_err("%s %d cmd[11]:0x%x\n", __func__, __LINE__, cmd[11]);

	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, buff,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get skhynix result error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}

static int scsi_ss_set_pwd(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x03;

	cmd[2] = 'g';
	cmd[3] = 'h';
	cmd[4] = 'r';
	cmd[5] = 0;

	result = scsi_exec_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_set_pwd error 0x%x\n", result);
	}
	return result;
}


static int scsi_ss_enter_vendor_mode(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};


	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0;


	cmd[2] = 0x5C;
	cmd[3] = 0x38;
	cmd[4] = 0x23;
	cmd[5] = 0xAE;

	cmd[6] = 'g';
	cmd[7] = 'h';
	cmd[8] = 'r';
	cmd[9] = 0;

	result = scsi_exec_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_enter_vendor_mode error 0x%x\n", result);
	}
	return result;
}

static int scsi_ss_exit_vendor_mode(struct scsi_device *sdev)
{
	int result;
	unsigned char cmd[16] = {0};

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x01;

	result = scsi_exec_req(sdev, cmd, DMA_NONE, 0,
				  0, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: scsi_ss_enter_vendor_mode error 0x%x\n", result);
	}
	return result;
}


static int scsi_ss_nandinfo(struct scsi_device *sdev, char *osv, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!osv)
		return -EINVAL;

	cmd[0] = 0xc0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x40;

	cmd[4] = 0x01;
	cmd[5] = 0x0A;

	cmd[15] = 0x4C;

	len = 0x4C;
	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, osv,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get osv result error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}

static int scsi_ss_hr(struct scsi_device *sdev, char *osv, int len)
{
	int result = 0;

	result = scsi_ss_enter_vendor_mode(sdev);
	if (result) {
		pr_err("ufs: enter vendor mode fail, program key and try again\n");

		result = scsi_ss_set_pwd(sdev);
		if (result) {
			pr_err("ufs: set pwd fail 0x%x\n", result);
			goto out;
		} else {
			result = scsi_ss_enter_vendor_mode(sdev);
			if (result) {
				pr_err("ufs: enter vendor mode fail 0x%x\n", result);
				goto out;
			}
		}
	}

	result = scsi_ss_nandinfo(sdev, osv, len);
	if (result) {
		pr_err("ufs: ger hr fail fail 0x%x\n", result);
	}

	result = scsi_ss_exit_vendor_mode(sdev);
	if (result) {
		pr_err("ufs: exit vendor mode fail 0x%x\n", result);
	}

out:
	return result;
}

/**
get hosin hr
 */
int scsi_hosin_hr(struct scsi_device *sdev, char *hr, int len)
{
	int result;
	struct scsi_sense_hdr sshdr, sshdrr;
	int len_buff1 = 0x2c;
	int len_buff2 = 4096;
	char *buff1 = NULL;
	char *buff2 = NULL;

	unsigned char write_buffer[16] = { 0x3B, 0xE1, 0x00, 0x00, 0x00,
					   0x00, 0x00, 0x00, 0x2C, 0x00 };
	unsigned char read_buffer[16] = { 0x3C, 0x01, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x10, 0x00, 0x00 };

	if (!hr)
		return -EINVAL;

	buff1 = kzalloc(len_buff1, GFP_KERNEL);
	buff1[0] = 0xBB;
	buff1[1] = 0x40;
	buff1[2] = 0x10;
	buff1[3] = 0x00;

	result = scsi_execute_req(sdev, write_buffer, DMA_TO_DEVICE, buff1,
				  len_buff1, &sshdr, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr write buffer error 0x%x\n", result);
		pr_err("sense hr write key:0x%x; asc:0x%x; ascq:0x%x\n",
		       (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
		kfree(buff1);
		return -EIO;
	}

	buff2 = kzalloc(len_buff2, GFP_KERNEL);
	result = scsi_execute_req(sdev, read_buffer, DMA_FROM_DEVICE, buff2,
				  len_buff2, &sshdrr, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr read buffer error 0x%x\n", result);
		pr_err("sense hr read key:0x%x; asc:0x%x; ascq:0x%x\n",
		       (int)sshdrr.sense_key, (int)sshdrr.asc,
		       (int)sshdrr.ascq);
		kfree(buff2);
		return -EIO;
	}

	memcpy(hr, buff2, len);

	kfree(buff1);
	kfree(buff2);
	return 0;
}

static int ufs_get_hynix_hr(struct ufs_hba *hba, u8 *buf, u32 size)
{
	size = QUERY_DESC_HEALTH_DEF_SIZE;
	return ufshcd_read_desc(hba, QUERY_DESC_IDN_HEALTH, 0, buf, size);
}

static int ufs_get_wdc_hr(struct scsi_device *sdev, char *buf, int len){
	int ret = 0;
	unsigned long flags = 0;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = scsi_sdr(sdev, buf, len);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

static int ufs_get_toshiba_hr(struct scsi_device *sdev, char *buf, int len){
	int ret = 0;
	unsigned long flags = 0;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = scsi_hr_inquiry(sdev, buf, len);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}


static int ufs_get_micron_hr(struct scsi_device *sdev, char *buf, int len){
	int ret = 0;
	unsigned long flags = 0;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = scsi_mhr(sdev, buf, len);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

static int ufs_get_samsung_hr(struct scsi_device *sdev, char *buf, int len){
	int ret = 0;
	unsigned long flags = 0;
	char *seg = buf + 0x80;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = scsi_osv(sdev, buf, 0x1c);/*0x200 is the same with 0x1c*/
	if(ret)
		goto err_out;

	mutex_lock(&health_info_mutex);
	scsi_ss_hr(sdev, seg, 0x4c);
	mutex_unlock(&health_info_mutex);
err_out:

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

static int ufs_get_skhynix_hr(struct scsi_device *sdev, u8 *buf, u32 size)
{
	int ret = 0;
	unsigned long flags = 0;
	char *seg = buf + 0x80;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = ufs_get_hynix_hr(hba, buf, size);
	if(ret)
		goto err_out;

	scsi_sk_hr(sdev, seg, 0x52);

err_out:

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

static int ufs_get_hosin_hr(struct scsi_device *sdev, char *buf, int len)
{
	int ret = 0;
	unsigned long flags = 0;

	struct ufs_hba *hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_info("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	ret = scsi_hosin_hr(sdev, buf, len);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

static ssize_t hr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int err = 0, i = 0;
	int len = 512; /*0x200*/
	char *hr;
	uint32_t count = 0;
	struct scsi_device *sdev;

	sdev = get_ufs_sdev_data();

	scsi_autopm_get_device(sdev);

	hr =  kzalloc(len, GFP_KERNEL);
	if (!hr) {
		pr_err("kzalloc fail\n");
		return -ENOMEM;
	}

	if (!strncmp(sdev->vendor, "WDC", 3)) {
		err = ufs_get_wdc_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "TOSHIBA", 7)) {
		err = ufs_get_toshiba_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "KIOXIA", 6)) {
		err = ufs_get_toshiba_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "SAMSUNG", 7)) {
		err = ufs_get_samsung_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "MICRON", 6)) {
		err = ufs_get_micron_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "SKhynix", 7)) {
		err = ufs_get_skhynix_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "HG", 2)) {
		err = ufs_get_hosin_hr(sdev, hr, len);
	} else if (!strncmp(sdev->vendor, "CS", 2)) {
		err = ufs_get_hosin_hr(sdev, hr, len);
	} else {
		count += snprintf((buf + count),  PAGE_SIZE, "NOT SUPPORTED %s\n", sdev->vendor);
		goto out;
	}

	if (err) {
		count += snprintf((buf + count),  PAGE_SIZE, "Fail to get hr, err is: %d\n", err);
	} else {
		for (i = 0; i < len; i++)
			count += snprintf((buf + count), PAGE_SIZE, "%02x", hr[i]);
		count += snprintf((buf + count), PAGE_SIZE, "\n");
	}

out:
	kfree(hr);

	scsi_autopm_put_device(sdev);

	return count;
}

static DEVICE_ATTR_RO(hr);

static ssize_t err_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct UFS_DATA *ufs_data_ptr = get_ufs_data();

	if (ufs_data_ptr)
		ret = snprintf(buf, PAGE_SIZE, "%d\n", ufs_data_ptr->ufs_err_state.err_occurred);
	return ret;
}

static DEVICE_ATTR_RO(err_state);

static ssize_t err_reason_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct UFS_DATA *ufs_data_ptr = get_ufs_data();

	if (ufs_data_ptr)
		ret = snprintf(buf, PAGE_SIZE, "%s%s%s%s%s%s%s%s%s%s", ufs_data_ptr->ufs_err_state.err_reason,
									ufs_data_ptr->ufs_err_state.err_reason+1,
									ufs_data_ptr->ufs_err_state.err_reason+2,
									ufs_data_ptr->ufs_err_state.err_reason+3,
									ufs_data_ptr->ufs_err_state.err_reason+4,
									ufs_data_ptr->ufs_err_state.err_reason+5,
									ufs_data_ptr->ufs_err_state.err_reason+6,
									ufs_data_ptr->ufs_err_state.err_reason+7,
									ufs_data_ptr->ufs_err_state.err_reason+8,
									ufs_data_ptr->ufs_err_state.err_reason+9);
	return ret;
}

static DEVICE_ATTR_RO(err_reason);

static struct attribute *ufshcd_sysfs[] = {
	&dev_attr_dump_health_desc.attr,
	&dev_attr_dump_string_desc_serial.attr,
	&dev_attr_dump_device_desc.attr,
	&dev_attr_show_hba.attr,
	&dev_attr_hr.attr,
	&dev_attr_err_state.attr,
	&dev_attr_err_reason.attr,
	NULL,
};

const struct attribute_group ufshcd_sysfs_group = {
	.name = "ufshcd0",
	.attrs = ufshcd_sysfs,
};
