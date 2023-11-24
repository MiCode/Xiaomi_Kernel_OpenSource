
#include <linux/device.h>
#include <linux/slab.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_common.h>
#include <asm/unaligned.h>
#include "memory_debugfs.h"
//#include "ufs-mediatek.h"
#include "ufs_info.h"


struct ufs_info_t *g_ufs_info = NULL;
struct ufs_info_t ufs_info ;
static void look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return;
	g_ufs_info->sdev = scsi_device_lookup(shost, 0, 0, lun);

	pr_info("[mi_ufs_info] scsi device proc name is %s\n", g_ufs_info->sdev->host->hostt->proc_name);

	scsi_device_put(g_ufs_info->sdev);

	scsi_host_put(shost);
}

struct ufs_hba *get_ufs_hba_data(void)
{
	if(!g_ufs_info->sdev){
		look_up_scsi_device(SCSI_LUN);
		if(!g_ufs_info->sdev)
		return NULL;
	}

	if(g_ufs_info->sdev->host)
		return shost_priv(g_ufs_info->sdev->host);
	else
		return NULL;
}
EXPORT_SYMBOL(get_ufs_hba_data);

static struct scsi_device *get_ufs_sdev_data(void)
{
	if(!g_ufs_info->sdev)
		look_up_scsi_device(SCSI_LUN);

	return g_ufs_info->sdev;
}


static inline char ufshcd_remove_non_printable(u8 ch)
{
	return (ch >= 0x20 && ch <= 0x7e) ? ch : ' ';
}

static int ufs_read_string_desc(struct ufs_hba *hba, u8 desc_index, u8 **buf, bool ascii)
{
	struct uc_string_id *uc_str;
	u8 *str;
	int ret;

	if (!buf)
		return -EINVAL;

	uc_str = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!uc_str)
		return -ENOMEM;

	ret = ufshcd_read_desc_param_sel(hba, QUERY_DESC_IDN_STRING, desc_index, 0, 0,
				     (u8 *)uc_str, QUERY_DESC_MAX_SIZE);
	if (ret < 0) {
		dev_err(hba->dev, "Reading String Desc failed after %d retries. err = %d\n",
			QUERY_REQ_RETRIES, ret);
		str = NULL;
		goto out;
	}

	if (uc_str->len <= QUERY_DESC_HDR_SIZE) {
		dev_dbg(hba->dev, "String Desc is of zero length\n");
		str = NULL;
		ret = 0;
		goto out;
	}

	if (ascii) {
		ssize_t ascii_len;
		int i;
		/* remove header and divide by 2 to move from UTF16 to UTF8 */
		ascii_len = (uc_str->len - QUERY_DESC_HDR_SIZE) / 2 + 1;
		str = kzalloc(ascii_len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}

		/*
		 * the descriptor contains string in UTF16 format
		 * we need to convert to utf-8 so it can be displayed
		 */
		ret = utf16s_to_utf8s(uc_str->uc,
				      uc_str->len - QUERY_DESC_HDR_SIZE,
				      UTF16_BIG_ENDIAN, str, ascii_len);

		/* replace non-printable or non-ASCII characters with spaces */
		for (i = 0; i < ret; i++)
			str[i] = ufshcd_remove_non_printable(str[i]);

		str[ret++] = '\0';

	} else {
		str = kmemdup(uc_str, uc_str->len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}
		ret = uc_str->len;
	}
out:
	*buf = str;
	kfree(uc_str);
	return ret;
}

int ufs_get_string_desc(struct ufs_hba *hba, void* buf, int size, enum device_desc_param pname, bool ascii_std)
{
	u8 index;
	int ret = 0;

	int desc_len = QUERY_DESC_MAX_SIZE;
	u8 *desc_buf;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_ATOMIC);
	if (!desc_buf)
		return -ENOMEM;
	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_descriptor_sel_retry(hba,
		UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_DEVICE,
		0, 0, desc_buf, &desc_len);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}
	index = desc_buf[pname];
	kfree(desc_buf);
	desc_buf = NULL;
	ret = ufs_read_string_desc(hba, index, &desc_buf, ascii_std);
	if (ret < 0)
		goto out;
	memcpy(buf, desc_buf, size);
out:
	pm_runtime_put_sync(hba->dev);
	kfree(desc_buf);
	return ret;
}

int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id, u8 desc_index, u8 param_offset, void* buf, u8 param_size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (param_size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param_sel(hba, desc_id, desc_index, 0,
				param_offset, desc_buf, param_size);
	pm_runtime_put_sync(hba->dev);

	if (ret)
		return -EINVAL;
	switch (param_size) {
	case 1:
		*(u8*)buf = *desc_buf;
		break;
	case 2:
		*(u16*)buf = get_unaligned_be16(desc_buf);
		break;
	case 4:
		*(u32*)buf =  get_unaligned_be32(desc_buf);
		break;
	case 8:
		*(u64*)buf= get_unaligned_be64(desc_buf);
		break;
	default:
		*(u8*)buf = *desc_buf;
		break;
	}

	return ret;
}

static int _scsi_execute(struct scsi_device *sdev, const unsigned char *cmd,
		 int data_direction, void *buffer, unsigned bufflen,
		 unsigned char *sense, struct scsi_sense_hdr *sshdr,
		 int timeout, int retries, u64 flags, req_flags_t rq_flags,
		 int *resid)
{
	struct request *req;
	struct scsi_request *rq;
	int ret = DRIVER_ERROR << 24;

	req = blk_get_request(sdev->request_queue,
			data_direction == DMA_TO_DEVICE ?
			REQ_OP_SCSI_OUT : REQ_OP_SCSI_IN,
			rq_flags & RQF_PM ? BLK_MQ_REQ_PM : 0);
	if (IS_ERR(req))
		return ret;
	rq = scsi_req(req);

	if (bufflen &&	blk_rq_map_kern(sdev->request_queue, req,
					buffer, bufflen, GFP_NOIO))
		goto out;

	rq->cmd_len = COMMAND_SIZE(cmd[0]);

	if (cmd[0] == 0xC0 || cmd[0] == 0xD0)
		rq->cmd_len = 16;

	memcpy(rq->cmd, cmd, rq->cmd_len);
	rq->retries = retries;
	req->timeout = timeout;
	req->cmd_flags |= flags;
	req->rq_flags |= rq_flags | RQF_QUIET;

	/*
	 * head injection *required* here otherwise quiesce won't work
	 */
	blk_execute_rq(req->q, NULL, req, 1);

	/*
	 * Some devices (USB mass-storage in particular) may transfer
	 * garbage data together with a residue indicating that the data
	 * is invalid.  Prevent the garbage from being misinterpreted
	 * and prevent security leaks by zeroing out the excess data.
	 */
	if (unlikely(rq->resid_len > 0 && rq->resid_len <= bufflen))
		memset(buffer + (bufflen - rq->resid_len), 0, rq->resid_len);

	if (resid)
		*resid = rq->resid_len;
	if (sense && rq->sense_len)
		memcpy(sense, rq->sense, SCSI_SENSE_BUFFERSIZE);
	if (sshdr)
		scsi_normalize_sense(rq->sense, rq->sense_len, sshdr);
	ret = rq->result;
 out:
	blk_put_request(req);

	return ret;
}

int scsi_exec_req(struct scsi_device *sdev,
	const unsigned char *cmd, int data_direction, void *buffer,
	unsigned bufflen, struct scsi_sense_hdr *sshdr, int timeout,
	int retries, int *resid)
{
	return _scsi_execute(sdev, cmd, data_direction, buffer,
		bufflen, NULL, sshdr, timeout, retries,  0, RQF_PM, resid);
}


struct ufs_info_t *init_ufs_info(void)
{
	u64 raw_device_capacity = 0;
	g_ufs_info = &ufs_info;

	g_ufs_info->hba = get_ufs_hba_data();
	if (!g_ufs_info->hba) {
		pr_err("%s:get hba struct fail\n",__func__);
		goto err;
	}

	ufs_get_string_desc(g_ufs_info->hba, g_ufs_info->ufs_name, (sizeof(g_ufs_info->ufs_name) - 1), DEVICE_DESC_PARAM_PRDCT_NAME, SD_ASCII_STD);
	ufs_get_string_desc(g_ufs_info->hba, g_ufs_info->ufs_fwver, (sizeof(g_ufs_info->ufs_fwver) - 1), DEVICE_DESC_PARAM_PRDCT_REV, SD_ASCII_STD);
	ufs_read_desc_param(g_ufs_info->hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_MANF_ID, &g_ufs_info->ufs_id, 2);
	ufs_read_desc_param(g_ufs_info->hba, QUERY_DESC_IDN_GEOMETRY, 0, GEOMETRY_DESC_PARAM_DEV_CAP, &raw_device_capacity, 8);

	raw_device_capacity = (raw_device_capacity * 512) / 1024 / 1024 / 1024;
	if (raw_device_capacity > 512 && raw_device_capacity <= 1024) {
		g_ufs_info->ufs_size = 1024;
	} else if (raw_device_capacity > 256) {
		g_ufs_info->ufs_size = 512;
	} else if (raw_device_capacity > 128) {
		g_ufs_info->ufs_size = 256;
	} else if (raw_device_capacity > 64) {
		g_ufs_info->ufs_size = 128;
	} else if (raw_device_capacity > 32) {
		g_ufs_info->ufs_size = 64;
	} else if (raw_device_capacity > 16) {
		g_ufs_info->ufs_size = 32;
	} else if (raw_device_capacity > 8) {
		g_ufs_info->ufs_size = 8;
	} else {
		g_ufs_info->ufs_size = 0;
		pr_info("mv unkonwn ufs size %d\n", raw_device_capacity);
	}
	return g_ufs_info;
err:
	g_ufs_info = NULL;
	return NULL;
}
EXPORT_SYMBOL(init_ufs_info);

u16 get_ufs_id(void) {
	if(!g_ufs_info)
		return -1;

	return g_ufs_info->ufs_id;
}
EXPORT_SYMBOL(get_ufs_id);

static int ufshcd_read_desc(struct ufs_hba *hba, enum desc_idn desc_id, int desc_index, void *buf, u32 size)
{
	int ret = 0;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param_sel(hba, desc_id, desc_index, 0, 0, buf, size);
	pm_runtime_put_sync(hba->dev);

	return ret;
}

static ssize_t dump_health_desc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 value = 0;
	int count = 0, i = 0;
	u8 buff_len = 0;
	u8 *desc_buf = NULL;
	int err = 0;


	struct desc_field_offset health_desc_field_name[] = {
		{"bLength", 			HEALTH_DESC_PARAM_LEN, 				BYTE},
		{"bDescriptorType", 	HEALTH_DESC_PARAM_TYPE, 			BYTE},
		{"bPreEOLInfo", 		HEALTH_DESC_PARAM_EOL_INFO, 		BYTE},
		{"bDeviceLifeTimeEstA", HEALTH_DESC_PARAM_LIFE_TIME_EST_A, 	BYTE},
		{"bDeviceLifeTimeEstB", HEALTH_DESC_PARAM_LIFE_TIME_EST_B, 	BYTE},
	};

	struct desc_field_offset *tmp = NULL;
	
	if (!g_ufs_info || !g_ufs_info->hba){
		pr_err("%s:ufs_info is not ready!\n",__func__);
		return -1;
	}
	ufs_read_desc_param(g_ufs_info->hba, QUERY_DESC_IDN_HEALTH, 0, HEALTH_DESC_PARAM_LEN, &buff_len, BYTE);

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		count += snprintf((buf + count), PAGE_SIZE, "get health info fail\n");
		return count;
	}

	err = ufshcd_read_desc(g_ufs_info->hba, QUERY_DESC_IDN_HEALTH, 0, desc_buf, buff_len);
	if (err) {
		count += snprintf((buf + count), PAGE_SIZE, "ufshcd_read_desc fail, err = %d\n", err);
		return count;
	}

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

	if (!g_ufs_info || !g_ufs_info->hba){
		pr_err("%s:ufs_info is not ready!\n",__func__);
		return -1;
	}


	ufs_get_string_desc(g_ufs_info->hba, &ser_number, sizeof(ser_number), DEVICE_DESC_PARAM_SN, SD_RAW);

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
	int err = 0;

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
	if (!g_ufs_info || !g_ufs_info->hba){
		pr_err("%s:ufs_info is not ready!\n",__func__);
		return -1;
	}

	ufs_read_desc_param(g_ufs_info->hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_LEN, &buff_len, BYTE);

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		count += snprintf((buf + count), PAGE_SIZE, "get desc info fail\n");
		return count;
	}

	err = ufshcd_read_desc(g_ufs_info->hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf, buff_len);
	if (err) {
		count += snprintf((buf + count), PAGE_SIZE, "ufshcd_read_desc fail, err = %d\n", err);
		return count;
	}

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
	//struct ufs_mtk_host *host = NULL;

	if (!g_ufs_info || !g_ufs_info->hba){
		pr_err("%s:ufs_info is not ready!\n",__func__);
		return -1;
	}


	count += snprintf((buf + count), PAGE_SIZE, "hba->outstanding_tasks = 0x%x\n", (u32)g_ufs_info->hba->outstanding_tasks);
	count += snprintf((buf + count), PAGE_SIZE, "hba->outstanding_reqs = 0x%x\n", (u32)g_ufs_info->hba->outstanding_reqs);

	count += snprintf((buf + count), PAGE_SIZE, "hba->capabilities = 0x%x\n", g_ufs_info->hba->capabilities);
	count += snprintf((buf + count), PAGE_SIZE, "hba->nutrs = %d\n", g_ufs_info->hba->nutrs);
	count += snprintf((buf + count), PAGE_SIZE, "hba->nutmrs = %d\n", g_ufs_info->hba->nutmrs);
	count += snprintf((buf + count), PAGE_SIZE, "hba->ufs_version = 0x%x\n", g_ufs_info->hba->ufs_version);
	count += snprintf((buf + count), PAGE_SIZE, "hba->irq = 0x%x\n", g_ufs_info->hba->irq);
	count += snprintf((buf + count), PAGE_SIZE, "hba->auto_bkops_enabled = %d\n", g_ufs_info->hba->auto_bkops_enabled);

	count += snprintf((buf + count), PAGE_SIZE, "hba->ufshcd_state = 0x%x\n", g_ufs_info->hba->ufshcd_state);
	count += snprintf((buf + count), PAGE_SIZE, "hba->clk_gating.state = 0x%x\n", g_ufs_info->hba->clk_gating.state);
	count += snprintf((buf + count), PAGE_SIZE, "hba->eh_flags = 0x%x\n", g_ufs_info->hba->eh_flags);
	count += snprintf((buf + count), PAGE_SIZE, "hba->intr_mask = 0x%x\n", g_ufs_info->hba->intr_mask);
	count += snprintf((buf + count), PAGE_SIZE, "hba->ee_ctrl_mask = 0x%x\n", g_ufs_info->hba->ee_ctrl_mask);

	/* HBA Errors */
	count += snprintf((buf + count), PAGE_SIZE, "hba->errors = 0x%x\n", g_ufs_info->hba->errors);
	count += snprintf((buf + count), PAGE_SIZE, "hba->uic_error = 0x%x\n", g_ufs_info->hba->uic_error);
	count += snprintf((buf + count), PAGE_SIZE, "hba->saved_err = 0x%x\n", g_ufs_info->hba->saved_err);
	count += snprintf((buf + count), PAGE_SIZE, "hba->saved_uic_err = 0x%x\n", g_ufs_info->hba->saved_uic_err);
	count += snprintf((buf + count), PAGE_SIZE, "hibern8_exit_cnt = %d\n", g_ufs_info->hba->ufs_stats.hibern8_exit_cnt);

	/* uic specific errors */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_pa_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_PA_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_dl_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_DL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_nl_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_NL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_tl_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_TL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_dme_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_DME_ERR].cnt);

	/* fatal errors */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_auto_hibern8_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_AUTO_HIBERN8_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_fatal_error_cnt= 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_FATAL_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_link_startup_fail_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_LINK_STARTUP_FAIL].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_resume_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_RESUME_ERR].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_suspend_error_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_SUSPEND_ERR].cnt);

	/* abnormal events */
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_device_reset_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_DEV_RESET].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_host_reset_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_HOST_RESET].cnt);
	count += snprintf((buf + count), PAGE_SIZE, "ufs_event_abort_cnt = 0x%x\n",
			g_ufs_info->hba->ufs_stats.event[UFS_EVT_ABORT].cnt);

	/*host = ufshcd_get_variant(g_ufs_info->hba);
	if (!host) {
		count += snprintf((buf + count), PAGE_SIZE, "get host struct fail\n");
		return count;
	}*/

	/* PA Errors */
	count += snprintf((buf + count), PAGE_SIZE, "pa_err_cnt_total = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_0_err_cnt = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt[UFS_EC_PA_LANE_0]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_1_err_cnt = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt[UFS_EC_PA_LANE_1]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_2_err_cnt = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt[UFS_EC_PA_LANE_2]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_lane_3_err_cnt = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt[UFS_EC_PA_LANE_3]);
	count += snprintf((buf + count), PAGE_SIZE, "pa_line_reset_err_cnt = %d\n", g_ufs_info->hba->uic_stats.pa_err_cnt[UFS_EC_PA_LINE_RESET]);

	/* DL Errors */
	count += snprintf((buf + count), PAGE_SIZE, "dl_err_cnt_total = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt_total);
	count += snprintf((buf + count), PAGE_SIZE, "dl_nac_received_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_NAC_RECEIVED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_tcx_replay_timer_expired_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_afcx_request_timer_expired_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_fcx_protection_timer_expired_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_crc_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_CRC_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dll_rx_buffer_overflow_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_RX_BUFFER_OVERFLOW]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_max_frame_length_exceeded_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_wrong_sequence_number_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_WRONG_SEQUENCE_NUMBER]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_afc_frame_syntax_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_nac_frame_syntax_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_eof_syntax_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_EOF_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_frame_syntax_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_FRAME_SYNTAX_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_bad_ctrl_symbol_type_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_pa_init_err_cnt = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_PA_INIT_ERROR]);
	count += snprintf((buf + count), PAGE_SIZE, "dl_pa_error_ind_received = %d\n",
		g_ufs_info->hba->uic_stats.dl_err_cnt[UFS_EC_DL_PA_ERROR_IND_RECEIVED]);

	/* DME Errors */
	count += snprintf((buf + count), PAGE_SIZE, "dme_err_cnt = %d\n", g_ufs_info->hba->uic_stats.dme_err_cnt);

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

	VU[0] = 0xFE;
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
static int scsi_sk_hr(struct scsi_device *sdev, char *buff, int len)
{
	int result;
	unsigned char cmd[16] = {0};

	if (!buff)
		return -EINVAL;

	cmd[0] = 0xD0; /*VENDOR_SPECIFIC_CDB;*/
	cmd[1] = 0x03;
	cmd[2] = 0x58;

#if defined(CONFIG_MEMORY_XAGA_HYNIX_V6)
	cmd[11] = 0x2c;//V6 hr length
#else
	cmd[11] = 0x52;//V7 hr length
#endif

	result = scsi_exec_req(sdev, cmd, DMA_FROM_DEVICE, buff,
				  len, NULL, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: get skhynix result error 0x%x\n", result);
		return -EIO;
	}

	return 0;
}


/**
get ymtc hr
 */
int scsi_ymtc_hr(struct scsi_device *sdev, char *hr, int len)
{
	int result;
	struct scsi_sense_hdr sshdr,sshdrr;
	int len_buff = 4096;
	char *buff = NULL;

	unsigned char write_buffer_1[16] = {0x3B, 0x01, 0x53, 0x4e, 0x44, 0x4b, 0x00, 0x00, 0x00, 0x00};
	unsigned char write_buffer_2[16] = {0x3B, 0x01, 0x00, 0x00, 0x40, 0xc4, 0x00, 0x00, 0x10, 0x00};
	unsigned char  read_buffer[16] = {0x3C, 0x01, 0x00, 0x00, 0x40, 0xc4, 0x00, 0x00,0x10, 0x00};

	if (!hr)
		return -EINVAL;
	buff =  kzalloc(len_buff, GFP_KERNEL);
	buff[0] = 1;

	result = scsi_execute_req(sdev, write_buffer_1, DMA_TO_DEVICE, NULL,
				  0, &sshdr, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr write buffer 1  error 0x%x\n", result);
		pr_err("sense hr write key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
		return -EIO;
	}
	//msleep(50);

	result = scsi_execute_req(sdev, write_buffer_2, DMA_TO_DEVICE, buff,
				  len_buff, &sshdr, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr write buffer 2  error 0x%x\n", result);
		pr_err("sense hr write key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
		return -EIO;
	}
	//msleep(50);
  	memset(buff, 0 ,len_buff);
	result = scsi_execute_req(sdev, read_buffer, DMA_FROM_DEVICE, buff,
				  len_buff, &sshdrr, 30 * HZ, 3, NULL);
	if (result) {
		pr_err("ufs: hr read buffer error 0x%x\n", result);
		pr_err("sense hr read key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
		return -EIO;
	}

	memcpy(hr, buff, len);

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


static int ufs_get_ymtc_hr(struct scsi_device *sdev, char *buf, int len){
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

	ret = scsi_ymtc_hr(sdev, buf, len);
	
	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
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

	scsi_ss_hr(sdev, seg, 0x4c);

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

#if defined(CONFIG_MEMORY_XAGA_HYNIX_V6)
	scsi_sk_hr(sdev, seg, 0x4c);
#else
	scsi_sk_hr(sdev, seg, 0x52);
#endif

err_out:

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
	} else if (!strncmp(sdev->vendor, "YMTC", 4)) {
		err = ufs_get_ymtc_hr(sdev, hr, len);
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
	//struct ufs_hba *hba = g_ufs_info->hba;
	/*struct ufs_mtk_host *host = NULL;
	if (g_ufs_info) {
		//host = ufshcd_get_variant(g_ufs_info->hba);
		if (!host) {
			ret = snprintf(buf, PAGE_SIZE, "get host struct fail\n");
			return ret;
		}
	}*/
	if (!g_ufs_info || !g_ufs_info->hba){
		pr_err("%s:ufs_info is not ready!\n",__func__);
		return -1;
	}
	
	ret = snprintf(buf, PAGE_SIZE, "%d\n", g_ufs_info->hba->err_stats.err_occurred);
	return ret;
}

static DEVICE_ATTR_RO(err_state);

static ssize_t err_reason_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	//struct ufs_hba *hba = g_ufs_info->hba;
	//struct ufs_mtk_host *host = NULL;

	if (g_ufs_info) {
		//host = ufshcd_get_variant(g_ufs_info->hba);
		if (!g_ufs_info->hba) {
			ret = snprintf(buf, PAGE_SIZE, "get hba struct fail\n");
			return ret;
		}

		ret = snprintf(buf, PAGE_SIZE, "%s%s%s%s%s%s%s%s%s%s",
									g_ufs_info->hba->err_stats.err_reason,
									g_ufs_info->hba->err_stats.err_reason+1,
									g_ufs_info->hba->err_stats.err_reason+2,
									g_ufs_info->hba->err_stats.err_reason+3,
									g_ufs_info->hba->err_stats.err_reason+4,
									g_ufs_info->hba->err_stats.err_reason+5,
									g_ufs_info->hba->err_stats.err_reason+6,
									g_ufs_info->hba->err_stats.err_reason+7,
									g_ufs_info->hba->err_stats.err_reason+8,
									g_ufs_info->hba->err_stats.err_reason+9);
	}
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

const struct attribute_group ufs_sysfs_group = {
	.name = "ufshcd0",
	.attrs = ufshcd_sysfs,
};
