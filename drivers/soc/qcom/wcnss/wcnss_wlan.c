/* Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/wcnss_wlan.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_qos.h>
#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/ipc_logging.h>
#include <soc/qcom/socinfo.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

#include <soc/qcom/smd.h>

#define DEVICE "wcnss_wlan"
#define CTRL_DEVICE "wcnss_ctrl"
#define VERSION "1.01"
#define WCNSS_PIL_DEVICE "wcnss"

#define WCNSS_PINCTRL_STATE_DEFAULT "wcnss_default"
#define WCNSS_PINCTRL_STATE_SLEEP "wcnss_sleep"
#define WCNSS_PINCTRL_GPIO_STATE_DEFAULT "wcnss_gpio_default"

#define WCNSS_DISABLE_PC_LATENCY	100
#define WCNSS_ENABLE_PC_LATENCY	PM_QOS_DEFAULT_VALUE
#define WCNSS_PM_QOS_TIMEOUT	15000
#define IS_CAL_DATA_PRESENT     0
#define WAIT_FOR_CBC_IND     2
#define WCNSS_DUAL_BAND_CAPABILITY_OFFSET	BIT(8)

/* module params */
#define WCNSS_CONFIG_UNSPECIFIED (-1)
#define UINT32_MAX (0xFFFFFFFFU)

#define SUBSYS_NOTIF_MIN_INDEX	0
#define SUBSYS_NOTIF_MAX_INDEX	9
char *wcnss_subsys_notif_type[] = {
	"SUBSYS_BEFORE_SHUTDOWN",
	"SUBSYS_AFTER_SHUTDOWN",
	"SUBSYS_BEFORE_POWERUP",
	"SUBSYS_AFTER_POWERUP",
	"SUBSYS_RAMDUMP_NOTIFICATION",
	"SUBSYS_POWERUP_FAILURE",
	"SUBSYS_PROXY_VOTE",
	"SUBSYS_PROXY_UNVOTE",
	"SUBSYS_SOC_RESET",
	"SUBSYS_NOTIF_TYPE_COUNT"
};

static int has_48mhz_xo = WCNSS_CONFIG_UNSPECIFIED;
module_param(has_48mhz_xo, int, 0644);
MODULE_PARM_DESC(has_48mhz_xo, "Is an external 48 MHz XO present");

static int has_calibrated_data = WCNSS_CONFIG_UNSPECIFIED;
module_param(has_calibrated_data, int, 0644);
MODULE_PARM_DESC(has_calibrated_data, "whether calibrated data file available");

static int has_autodetect_xo = WCNSS_CONFIG_UNSPECIFIED;
module_param(has_autodetect_xo, int, 0644);
MODULE_PARM_DESC(has_autodetect_xo, "Perform auto detect to configure IRIS XO");

static int do_not_cancel_vote = WCNSS_CONFIG_UNSPECIFIED;
module_param(do_not_cancel_vote, int, 0644);
MODULE_PARM_DESC(do_not_cancel_vote, "Do not cancel votes for wcnss");

static DEFINE_SPINLOCK(reg_spinlock);

#define RIVA_SPARE_OFFSET		0x0b4
#define RIVA_SUSPEND_BIT		BIT(24)

#define CCU_RIVA_INVALID_ADDR_OFFSET		0x100
#define CCU_RIVA_LAST_ADDR0_OFFSET		0x104
#define CCU_RIVA_LAST_ADDR1_OFFSET		0x108
#define CCU_RIVA_LAST_ADDR2_OFFSET		0x10c

#define PRONTO_PMU_SPARE_OFFSET       0x1088
#define PMU_A2XB_CFG_HSPLIT_RESP_LIMIT_OFFSET	0x117C

#define PRONTO_PMU_COM_GDSCR_OFFSET       0x0024
#define PRONTO_PMU_COM_GDSCR_SW_COLLAPSE  BIT(0)
#define PRONTO_PMU_COM_GDSCR_HW_CTRL      BIT(1)

#define PRONTO_PMU_WLAN_BCR_OFFSET         0x0050
#define PRONTO_PMU_WLAN_BCR_BLK_ARES       BIT(0)

#define PRONTO_PMU_WLAN_GDSCR_OFFSET       0x0054
#define PRONTO_PMU_WLAN_GDSCR_SW_COLLAPSE  BIT(0)

#define PRONTO_PMU_WDOG_CTL		0x0068

#define PRONTO_PMU_CBCR_OFFSET        0x0008
#define PRONTO_PMU_CBCR_CLK_EN        BIT(0)

#define PRONTO_PMU_COM_CPU_CBCR_OFFSET     0x0030
#define PRONTO_PMU_COM_AHB_CBCR_OFFSET     0x0034

#define PRONTO_PMU_WLAN_AHB_CBCR_OFFSET    0x0074
#define PRONTO_PMU_WLAN_AHB_CBCR_CLK_EN    BIT(0)
#define PRONTO_PMU_WLAN_AHB_CBCR_CLK_OFF   BIT(31)

#define PRONTO_PMU_CPU_AHB_CMD_RCGR_OFFSET  0x0120
#define PRONTO_PMU_CPU_AHB_CMD_RCGR_ROOT_EN BIT(1)

#define PRONTO_PMU_CFG_OFFSET              0x1004
#define PRONTO_PMU_COM_CSR_OFFSET          0x1040
#define PRONTO_PMU_SOFT_RESET_OFFSET       0x104C

#define PRONTO_QFUSE_DUAL_BAND_OFFSET	   0x0018

#define A2XB_CFG_OFFSET				0x00
#define A2XB_INT_SRC_OFFSET			0x0c
#define A2XB_TSTBUS_CTRL_OFFSET		0x14
#define A2XB_TSTBUS_OFFSET			0x18
#define A2XB_ERR_INFO_OFFSET		0x1c
#define A2XB_FIFO_FILL_OFFSET		0x07
#define A2XB_READ_FIFO_FILL_MASK		0x3F
#define A2XB_CMD_FIFO_FILL_MASK			0x0F
#define A2XB_WRITE_FIFO_FILL_MASK		0x1F
#define A2XB_FIFO_EMPTY			0x2
#define A2XB_FIFO_COUNTER			0xA

#define WCNSS_TSTBUS_CTRL_EN		BIT(0)
#define WCNSS_TSTBUS_CTRL_AXIM		(0x02 << 1)
#define WCNSS_TSTBUS_CTRL_CMDFIFO	(0x03 << 1)
#define WCNSS_TSTBUS_CTRL_WRFIFO	(0x04 << 1)
#define WCNSS_TSTBUS_CTRL_RDFIFO	(0x05 << 1)
#define WCNSS_TSTBUS_CTRL_CTRL		(0x07 << 1)
#define WCNSS_TSTBUS_CTRL_AXIM_CFG0	(0x00 << 8)
#define WCNSS_TSTBUS_CTRL_AXIM_CFG1	(0x01 << 8)
#define WCNSS_TSTBUS_CTRL_CTRL_CFG0	(0x00 << 28)
#define WCNSS_TSTBUS_CTRL_CTRL_CFG1	(0x01 << 28)

#define CCU_PRONTO_INVALID_ADDR_OFFSET		0x08
#define CCU_PRONTO_LAST_ADDR0_OFFSET		0x0c
#define CCU_PRONTO_LAST_ADDR1_OFFSET		0x10
#define CCU_PRONTO_LAST_ADDR2_OFFSET		0x14

#define CCU_PRONTO_AOWBR_ERR_ADDR_OFFSET	0x28
#define CCU_PRONTO_AOWBR_TIMEOUT_REG_OFFSET	0xcc
#define CCU_PRONTO_AOWBR_ERR_TIMEOUT_OFFSET	0xd0
#define CCU_PRONTO_A2AB_ERR_ADDR_OFFSET		0x18

#define PRONTO_SAW2_SPM_STS_OFFSET		0x0c
#define PRONTO_SAW2_SPM_CTL		0x30
#define PRONTO_SAW2_SAW2_VERSION		0xFD0
#define PRONTO_SAW2_MAJOR_VER_OFFSET		0x1C

#define PRONTO_PLL_STATUS_OFFSET		0x1c
#define PRONTO_PLL_MODE_OFFSET			0x1c0

#define MCU_APB2PHY_STATUS_OFFSET		0xec
#define MCU_CBR_CCAHB_ERR_OFFSET		0x380
#define MCU_CBR_CAHB_ERR_OFFSET			0x384
#define MCU_CBR_CCAHB_TIMEOUT_OFFSET		0x388
#define MCU_CBR_CAHB_TIMEOUT_OFFSET		0x38c
#define MCU_DBR_CDAHB_ERR_OFFSET		0x390
#define MCU_DBR_DAHB_ERR_OFFSET			0x394
#define MCU_DBR_CDAHB_TIMEOUT_OFFSET		0x398
#define MCU_DBR_DAHB_TIMEOUT_OFFSET		0x39c
#define MCU_FDBR_CDAHB_ERR_OFFSET		0x3a0
#define MCU_FDBR_FDAHB_ERR_OFFSET		0x3a4
#define MCU_FDBR_CDAHB_TIMEOUT_OFFSET		0x3a8
#define MCU_FDBR_FDAHB_TIMEOUT_OFFSET		0x3ac
#define PRONTO_PMU_CCPU_BOOT_REMAP_OFFSET	0x2004

#define WCNSS_DEF_WLAN_RX_BUFF_COUNT		1024

#define WCNSS_CTRL_CHANNEL			"WCNSS_CTRL"
#define WCNSS_MAX_FRAME_SIZE		(4 * 1024)
#define WCNSS_VERSION_LEN			30
#define WCNSS_MAX_BUILD_VER_LEN		256
#define WCNSS_MAX_CMD_LEN		(128)
#define WCNSS_MIN_CMD_LEN		(3)

/* control messages from userspace */
#define WCNSS_USR_CTRL_MSG_START  0x00000000
#define WCNSS_USR_HAS_CAL_DATA    (WCNSS_USR_CTRL_MSG_START + 2)
#define WCNSS_USR_WLAN_MAC_ADDR   (WCNSS_USR_CTRL_MSG_START + 3)

#define MAC_ADDRESS_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define SHOW_MAC_ADDRESS_STR	"%02x:%02x:%02x:%02x:%02x:%02x\n"
#define WCNSS_USER_MAC_ADDR_LENGTH	18

/* message types */
#define WCNSS_CTRL_MSG_START	0x01000000
#define	WCNSS_VERSION_REQ             (WCNSS_CTRL_MSG_START + 0)
#define	WCNSS_VERSION_RSP             (WCNSS_CTRL_MSG_START + 1)
#define	WCNSS_NVBIN_DNLD_REQ          (WCNSS_CTRL_MSG_START + 2)
#define	WCNSS_NVBIN_DNLD_RSP          (WCNSS_CTRL_MSG_START + 3)
#define	WCNSS_CALDATA_UPLD_REQ        (WCNSS_CTRL_MSG_START + 4)
#define	WCNSS_CALDATA_UPLD_RSP        (WCNSS_CTRL_MSG_START + 5)
#define	WCNSS_CALDATA_DNLD_REQ        (WCNSS_CTRL_MSG_START + 6)
#define	WCNSS_CALDATA_DNLD_RSP        (WCNSS_CTRL_MSG_START + 7)
#define	WCNSS_VBATT_LEVEL_IND         (WCNSS_CTRL_MSG_START + 8)
#define	WCNSS_BUILD_VER_REQ           (WCNSS_CTRL_MSG_START + 9)
#define	WCNSS_BUILD_VER_RSP           (WCNSS_CTRL_MSG_START + 10)
#define	WCNSS_PM_CONFIG_REQ           (WCNSS_CTRL_MSG_START + 11)
#define	WCNSS_CBC_COMPLETE_IND        (WCNSS_CTRL_MSG_START + 12)

/* max 20mhz channel count */
#define WCNSS_MAX_CH_NUM			45
#define WCNSS_MAX_PIL_RETRY			2

#define VALID_VERSION(version) \
	((strcmp(version, "INVALID")) ? 1 : 0)

#define FW_CALDATA_CAPABLE() \
	((penv->fw_major >= 1) && (penv->fw_minor >= 5) ? 1 : 0)

static int wcnss_pinctrl_set_state(bool active);

struct smd_msg_hdr {
	unsigned int msg_type;
	unsigned int msg_len;
};

struct wcnss_version {
	struct smd_msg_hdr hdr;
	unsigned char  major;
	unsigned char  minor;
	unsigned char  version;
	unsigned char  revision;
};

struct wcnss_pmic_dump {
	char reg_name[10];
	u16 reg_addr;
};

static int wcnss_notif_cb(struct notifier_block *this, unsigned long code,
			  void *ss_handle);

static struct notifier_block wnb = {
	.notifier_call = wcnss_notif_cb,
};

#define NVBIN_FILE "wlan/prima/WCNSS_qcom_wlan_nv.bin"

/* On SMD channel 4K of maximum data can be transferred, including message
 * header, so NV fragment size as next multiple of 1Kb is 3Kb.
 */
#define NV_FRAGMENT_SIZE  3072
#define MAX_CALIBRATED_DATA_SIZE  (64 * 1024)
#define LAST_FRAGMENT        BIT(0)
#define MESSAGE_TO_FOLLOW    BIT(1)
#define CAN_RECEIVE_CALDATA  BIT(15)
#define WCNSS_RESP_SUCCESS   1
#define WCNSS_RESP_FAIL      0

/* Macro to find the total number fragments of the NV bin Image */
#define TOTALFRAGMENTS(x) ((((x) % NV_FRAGMENT_SIZE) == 0) ? \
	((x) / NV_FRAGMENT_SIZE) : (((x) / NV_FRAGMENT_SIZE) + 1))

struct nvbin_dnld_req_params {
	/* Fragment sequence number of the NV bin Image. NV Bin Image
	 * might not fit into one message due to size limitation of
	 * the SMD channel FIFO so entire NV blob is chopped into
	 * multiple fragments starting with seqeunce number 0. The
	 * last fragment is indicated by marking is_last_fragment field
	 * to 1. At receiving side, NV blobs would be concatenated
	 * together without any padding bytes in between.
	 */
	unsigned short frag_number;

	/* bit 0: When set to 1 it indicates that no more fragments will
	 * be sent.
	 * bit 1: When set, a new message will be followed by this message
	 * bit 2- bit 14:  Reserved
	 * bit 15: when set, it indicates that the sender is capable of
	 * receiving Calibrated data.
	 */
	unsigned short msg_flags;

	/* NV Image size (number of bytes) */
	unsigned int nvbin_buffer_size;

	/* Following the 'nvbin_buffer_size', there should be
	 * nvbin_buffer_size bytes of NV bin Image i.e.
	 * uint8[nvbin_buffer_size].
	 */
};

struct nvbin_dnld_req_msg {
	/* Note: The length specified in nvbin_dnld_req_msg messages
	 * should be hdr.msg_len = sizeof(nvbin_dnld_req_msg) +
	 * nvbin_buffer_size.
	 */
	struct smd_msg_hdr hdr;
	struct nvbin_dnld_req_params dnld_req_params;
};

struct cal_data_params {
	/* The total size of the calibrated data, including all the
	 * fragments.
	 */
	unsigned int total_size;
	unsigned short frag_number;
	/* bit 0: When set to 1 it indicates that no more fragments will
	 * be sent.
	 * bit 1: When set, a new message will be followed by this message
	 * bit 2- bit 15: Reserved
	 */
	unsigned short msg_flags;
	/* fragment size
	 */
	unsigned int frag_size;
	/* Following the frag_size, frag_size of fragmented
	 * data will be followed.
	 */
};

struct cal_data_msg {
	/* The length specified in cal_data_msg should be
	 * hdr.msg_len = sizeof(cal_data_msg) + frag_size
	 */
	struct smd_msg_hdr hdr;
	struct cal_data_params cal_params;
};

struct vbatt_level {
	u32 curr_volt;
	u32 threshold;
};

struct vbatt_message {
	struct smd_msg_hdr hdr;
	struct vbatt_level vbatt;
};

static struct {
	struct platform_device *pdev;
	void		*pil;
	struct resource	*mmio_res;
	struct resource	*tx_irq_res;
	struct resource	*rx_irq_res;
	struct resource	*gpios_5wire;
	const struct dev_pm_ops *pm_ops;
	int		triggered;
	int		smd_channel_ready;
	u32		wlan_rx_buff_count;
	int		is_vsys_adc_channel;
	int		is_a2xb_split_reg;
	smd_channel_t	*smd_ch;
	unsigned char	wcnss_version[WCNSS_VERSION_LEN];
	unsigned char   fw_major;
	unsigned char   fw_minor;
	unsigned int	serial_number;
	int		thermal_mitigation;
	enum wcnss_hw_type	wcnss_hw_type;
	void		(*tm_notify)(struct device *, int);
	struct wcnss_wlan_config wlan_config;
	struct delayed_work wcnss_work;
	struct delayed_work vbatt_work;
	struct work_struct wcnssctrl_version_work;
	struct work_struct wcnss_pm_config_work;
	struct work_struct wcnssctrl_nvbin_dnld_work;
	struct work_struct wcnssctrl_rx_work;
	struct work_struct wcnss_vadc_work;
	struct wakeup_source wcnss_wake_lock;
	void __iomem *msm_wcnss_base;
	void __iomem *riva_ccu_base;
	void __iomem *pronto_a2xb_base;
	void __iomem *pronto_ccpu_base;
	void __iomem *pronto_saw2_base;
	void __iomem *pronto_pll_base;
	void __iomem *pronto_mcu_base;
	void __iomem *pronto_qfuse;
	void __iomem *wlan_tx_status;
	void __iomem *wlan_tx_phy_aborts;
	void __iomem *wlan_brdg_err_source;
	void __iomem *alarms_txctl;
	void __iomem *alarms_tactl;
	void __iomem *fiq_reg;
	int	nv_downloaded;
	int	is_cbc_done;
	unsigned char *fw_cal_data;
	unsigned char *user_cal_data;
	int	fw_cal_rcvd;
	int	fw_cal_exp_frag;
	int	fw_cal_available;
	int	user_cal_read;
	int	user_cal_available;
	u32	user_cal_rcvd;
	u32	user_cal_exp_size;
	int	iris_xo_mode_set;
	int	fw_vbatt_state;
	char	wlan_nv_mac_addr[WLAN_MAC_ADDR_SIZE];
	int	ctrl_device_opened;
	/* dev node lock */
	struct mutex dev_lock;
	/* dev control lock */
	struct mutex ctrl_lock;
	wait_queue_head_t read_wait;
	struct qpnp_adc_tm_btm_param vbat_monitor_params;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct qpnp_vadc_chip *vadc_dev;
	/* battery monitor lock */
	struct mutex vbat_monitor_mutex;
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[WCNSS_MAX_CH_NUM];
	void *wcnss_notif_hdle;
	struct pinctrl *pinctrl;
	struct pinctrl_state *wcnss_5wire_active;
	struct pinctrl_state *wcnss_5wire_suspend;
	struct pinctrl_state *wcnss_gpio_active;
	int gpios[WCNSS_WLAN_MAX_GPIO];
	int use_pinctrl;
	u8 is_shutdown;
	struct pm_qos_request wcnss_pm_qos_request;
	int pc_disabled;
	struct delayed_work wcnss_pm_qos_del_req;
	/* power manager QOS lock */
	struct mutex pm_qos_mutex;
	struct clk *snoc_wcnss;
	unsigned int snoc_wcnss_clock_freq;
	bool is_dual_band_disabled;
	dev_t dev_ctrl, dev_node;
	struct class *node_class;
	struct cdev ctrl_dev, node_dev;
} *penv = NULL;

static void *wcnss_ipc_log;

#define IPC_NUM_LOG_PAGES	12
#define wcnss_ipc_log_string(_x...) do {		\
	if (wcnss_ipc_log)				\
		ipc_log_string(wcnss_ipc_log, _x);	\
	} while (0)

void wcnss_log(enum wcnss_log_type type, const char *_fmt, ...)
{
	struct va_format vaf = {
		.fmt = _fmt,
	};
	va_list args;

	va_start(args, _fmt);
	vaf.va = &args;
	switch (type) {
	case ERR:
		pr_err("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
		break;
	case WARN:
		pr_warn("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
		break;
	case INFO:
		pr_info("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
		break;
	case DBG:
#if defined(CONFIG_DYNAMIC_DEBUG)
		pr_debug("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
#elif defined(DEBUG)
		pr_debug("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
#else
		pr_devel("wcnss: %pV", &vaf);
		wcnss_ipc_log_string("wcnss: %pV", &vaf);
#endif
		break;
	}
	va_end(args);
}
EXPORT_SYMBOL(wcnss_log);

static ssize_t wcnss_wlan_macaddr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int index;
	int mac_addr[WLAN_MAC_ADDR_SIZE];

	if (!penv)
		return -ENODEV;

	if (strlen(buf) != WCNSS_USER_MAC_ADDR_LENGTH) {
		wcnss_log(ERR, "%s: Invalid MAC addr length\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, MAC_ADDRESS_STR, &mac_addr[0], &mac_addr[1],
		   &mac_addr[2], &mac_addr[3], &mac_addr[4],
		   &mac_addr[5]) != WLAN_MAC_ADDR_SIZE) {
		wcnss_log(ERR, "%s: Failed to Copy MAC\n", __func__);
		return -EINVAL;
	}

	for (index = 0; index < WLAN_MAC_ADDR_SIZE; index++) {
		memcpy(&penv->wlan_nv_mac_addr[index],
		       (char *)&mac_addr[index], sizeof(char));
	}

	wcnss_log(INFO, "%s: Write MAC Addr:" MAC_ADDRESS_STR "\n", __func__,
		penv->wlan_nv_mac_addr[0], penv->wlan_nv_mac_addr[1],
		penv->wlan_nv_mac_addr[2], penv->wlan_nv_mac_addr[3],
		penv->wlan_nv_mac_addr[4], penv->wlan_nv_mac_addr[5]);

	return count;
}

static ssize_t wcnss_wlan_macaddr_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, SHOW_MAC_ADDRESS_STR,
		penv->wlan_nv_mac_addr[0], penv->wlan_nv_mac_addr[1],
		penv->wlan_nv_mac_addr[2], penv->wlan_nv_mac_addr[3],
		penv->wlan_nv_mac_addr[4], penv->wlan_nv_mac_addr[5]);
}

static DEVICE_ATTR(wcnss_mac_addr, 0600, wcnss_wlan_macaddr_show,
		   wcnss_wlan_macaddr_store);

static ssize_t wcnss_thermal_mitigation_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", penv->thermal_mitigation);
}

static ssize_t wcnss_thermal_mitigation_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	int value;

	if (!penv)
		return -ENODEV;

	if (kstrtoint(buf, 10, &value) != 1)
		return -EINVAL;
	penv->thermal_mitigation = value;
	if (penv->tm_notify)
		penv->tm_notify(dev, value);
	return count;
}

static DEVICE_ATTR(thermal_mitigation, 0600, wcnss_thermal_mitigation_show,
		   wcnss_thermal_mitigation_store);

static ssize_t wcnss_version_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%s", penv->wcnss_version);
}

static DEVICE_ATTR(wcnss_version, 0400, wcnss_version_show, NULL);

/* wcnss_reset_fiq() is invoked when host drivers fails to
 * communicate with WCNSS over SMD; so logging these registers
 * helps to know WCNSS failure reason
 */
void wcnss_riva_log_debug_regs(void)
{
	void __iomem *ccu_reg;
	u32 reg = 0;

	ccu_reg = penv->riva_ccu_base + CCU_RIVA_INVALID_ADDR_OFFSET;
	reg = readl_relaxed(ccu_reg);
	wcnss_log(INFO, "%s: CCU_CCPU_INVALID_ADDR %08x\n", __func__, reg);

	ccu_reg = penv->riva_ccu_base + CCU_RIVA_LAST_ADDR0_OFFSET;
	reg = readl_relaxed(ccu_reg);
	wcnss_log(INFO, "%s: CCU_CCPU_LAST_ADDR0 %08x\n", __func__, reg);

	ccu_reg = penv->riva_ccu_base + CCU_RIVA_LAST_ADDR1_OFFSET;
	reg = readl_relaxed(ccu_reg);
	wcnss_log(INFO, "%s: CCU_CCPU_LAST_ADDR1 %08x\n", __func__, reg);

	ccu_reg = penv->riva_ccu_base + CCU_RIVA_LAST_ADDR2_OFFSET;
	reg = readl_relaxed(ccu_reg);
	wcnss_log(INFO, "%s: CCU_CCPU_LAST_ADDR2 %08x\n", __func__, reg);
}
EXPORT_SYMBOL(wcnss_riva_log_debug_regs);

void wcnss_pronto_is_a2xb_bus_stall(void *tst_addr, u32 fifo_mask, char *type)
{
	u32 iter = 0, reg = 0;
	u32 axi_fifo_count = 0, axi_fifo_count_last = 0;

	reg = readl_relaxed(tst_addr);
	axi_fifo_count = (reg >> A2XB_FIFO_FILL_OFFSET) & fifo_mask;
	while ((++iter < A2XB_FIFO_COUNTER) && axi_fifo_count) {
		axi_fifo_count_last = axi_fifo_count;
		reg = readl_relaxed(tst_addr);
		axi_fifo_count = (reg >> A2XB_FIFO_FILL_OFFSET) & fifo_mask;
		if (axi_fifo_count < axi_fifo_count_last)
			break;
	}

	if (iter == A2XB_FIFO_COUNTER)
		wcnss_log(ERR,
		"%s data FIFO testbus possibly stalled reg%08x\n", type, reg);
	else
		wcnss_log(ERR,
		"%s data FIFO tstbus not stalled reg%08x\n", type, reg);
}

int wcnss_get_dual_band_capability_info(struct platform_device *pdev)
{
	u32 reg = 0;
	struct resource *res;

	res = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "pronto_qfuse");
	if (!res)
		return -EINVAL;

	penv->pronto_qfuse = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(penv->pronto_qfuse))
		return -ENOMEM;

	reg = readl_relaxed(penv->pronto_qfuse +
			PRONTO_QFUSE_DUAL_BAND_OFFSET);
	if (reg & WCNSS_DUAL_BAND_CAPABILITY_OFFSET)
		penv->is_dual_band_disabled = true;
	else
		penv->is_dual_band_disabled = false;

	return 0;
}

/* Log pronto debug registers during SSR Timeout CB */
void wcnss_pronto_log_debug_regs(void)
{
	void __iomem *reg_addr, *tst_addr, *tst_ctrl_addr;
	u32 reg = 0, reg2 = 0, reg3 = 0, reg4 = 0;

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_SPARE_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_SPARE %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_COM_CPU_CBCR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_COM_CPU_CBCR %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_COM_AHB_CBCR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_COM_AHB_CBCR %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_CFG_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_CFG %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_COM_CSR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_COM_CSR %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_SOFT_RESET_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_SOFT_RESET %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_WDOG_CTL;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_WDOG_CTL %08x\n", reg);

	reg_addr = penv->pronto_saw2_base + PRONTO_SAW2_SPM_STS_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_SAW2_SPM_STS %08x\n", reg);

	reg_addr = penv->pronto_saw2_base + PRONTO_SAW2_SPM_CTL;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_SAW2_SPM_CTL %08x\n", reg);

	if (penv->is_a2xb_split_reg) {
		reg_addr = penv->msm_wcnss_base +
			   PMU_A2XB_CFG_HSPLIT_RESP_LIMIT_OFFSET;
		reg = readl_relaxed(reg_addr);
		wcnss_log(ERR, "PMU_A2XB_CFG_HSPLIT_RESP_LIMIT %08x\n", reg);
	}

	reg_addr = penv->pronto_saw2_base + PRONTO_SAW2_SAW2_VERSION;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_SAW2_SAW2_VERSION %08x\n", reg);
	reg >>= PRONTO_SAW2_MAJOR_VER_OFFSET;

	reg_addr = penv->msm_wcnss_base  + PRONTO_PMU_CCPU_BOOT_REMAP_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_CCPU_BOOT_REMAP %08x\n", reg);

	reg_addr = penv->pronto_pll_base + PRONTO_PLL_STATUS_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PLL_STATUS %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_CPU_AHB_CMD_RCGR_OFFSET;
	reg4 = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PMU_CPU_CMD_RCGR %08x\n", reg4);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_COM_GDSCR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PRONTO_PMU_COM_GDSCR %08x\n", reg);
	reg >>= 31;

	if (!reg) {
		wcnss_log(ERR,
		"Cannot log, Pronto common SS is power collapsed\n");
		return;
	}
	reg &= ~(PRONTO_PMU_COM_GDSCR_SW_COLLAPSE
			| PRONTO_PMU_COM_GDSCR_HW_CTRL);
	writel_relaxed(reg, reg_addr);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_CBCR_OFFSET;
	reg = readl_relaxed(reg_addr);
	reg |= PRONTO_PMU_CBCR_CLK_EN;
	writel_relaxed(reg, reg_addr);

	reg_addr = penv->pronto_a2xb_base + A2XB_CFG_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "A2XB_CFG_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_a2xb_base + A2XB_INT_SRC_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "A2XB_INT_SRC_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_a2xb_base + A2XB_ERR_INFO_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "A2XB_ERR_INFO_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_INVALID_ADDR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_CCPU_INVALID_ADDR %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_LAST_ADDR0_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_CCPU_LAST_ADDR0 %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_LAST_ADDR1_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_CCPU_LAST_ADDR1 %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_LAST_ADDR2_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_CCPU_LAST_ADDR2 %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_AOWBR_ERR_ADDR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_PRONTO_AOWBR_ERR_ADDR_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_AOWBR_TIMEOUT_REG_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_PRONTO_AOWBR_TIMEOUT_REG_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_AOWBR_ERR_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_PRONTO_AOWBR_ERR_TIMEOUT_OFFSET %08x\n", reg);

	reg_addr = penv->pronto_ccpu_base + CCU_PRONTO_A2AB_ERR_ADDR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "CCU_PRONTO_A2AB_ERR_ADDR_OFFSET %08x\n", reg);

	tst_addr = penv->pronto_a2xb_base + A2XB_TSTBUS_OFFSET;
	tst_ctrl_addr = penv->pronto_a2xb_base + A2XB_TSTBUS_CTRL_OFFSET;

	/*  read data FIFO */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_RDFIFO;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	if (!(reg & A2XB_FIFO_EMPTY)) {
		wcnss_pronto_is_a2xb_bus_stall(tst_addr,
					       A2XB_READ_FIFO_FILL_MASK,
					       "Read");
	} else {
		wcnss_log(ERR, "Read data FIFO testbus %08x\n", reg);
	}
	/*  command FIFO */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_CMDFIFO;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	if (!(reg & A2XB_FIFO_EMPTY)) {
		wcnss_pronto_is_a2xb_bus_stall(tst_addr,
					       A2XB_CMD_FIFO_FILL_MASK, "Cmd");
	} else {
		wcnss_log(ERR, "Command FIFO testbus %08x\n", reg);
	}

	/*  write data FIFO */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_WRFIFO;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	if (!(reg & A2XB_FIFO_EMPTY)) {
		wcnss_pronto_is_a2xb_bus_stall(tst_addr,
					       A2XB_WRITE_FIFO_FILL_MASK,
					       "Write");
	} else {
		wcnss_log(ERR, "Write data FIFO testbus %08x\n", reg);
	}

	/*   AXIM SEL CFG0 */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_AXIM |
				WCNSS_TSTBUS_CTRL_AXIM_CFG0;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	wcnss_log(ERR, "AXIM SEL CFG0 testbus %08x\n", reg);

	/*   AXIM SEL CFG1 */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_AXIM |
				WCNSS_TSTBUS_CTRL_AXIM_CFG1;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	wcnss_log(ERR, "AXIM SEL CFG1 testbus %08x\n", reg);

	/*   CTRL SEL CFG0 */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_CTRL |
		WCNSS_TSTBUS_CTRL_CTRL_CFG0;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	wcnss_log(ERR, "CTRL SEL CFG0 testbus %08x\n", reg);

	/*   CTRL SEL CFG1 */
	reg = 0;
	reg = reg | WCNSS_TSTBUS_CTRL_EN | WCNSS_TSTBUS_CTRL_CTRL |
		WCNSS_TSTBUS_CTRL_CTRL_CFG1;
	writel_relaxed(reg, tst_ctrl_addr);
	reg = readl_relaxed(tst_addr);
	wcnss_log(ERR, "CTRL SEL CFG1 testbus %08x\n", reg);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_WLAN_BCR_OFFSET;
	reg = readl_relaxed(reg_addr);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_WLAN_GDSCR_OFFSET;
	reg2 = readl_relaxed(reg_addr);

	reg_addr = penv->msm_wcnss_base + PRONTO_PMU_WLAN_AHB_CBCR_OFFSET;
	reg3 = readl_relaxed(reg_addr);
	wcnss_log(ERR, "PMU_WLAN_AHB_CBCR %08x\n", reg3);

	msleep(50);

	if ((reg & PRONTO_PMU_WLAN_BCR_BLK_ARES) ||
	    (reg2 & PRONTO_PMU_WLAN_GDSCR_SW_COLLAPSE) ||
	    (!(reg4 & PRONTO_PMU_CPU_AHB_CMD_RCGR_ROOT_EN)) ||
	    (reg3 & PRONTO_PMU_WLAN_AHB_CBCR_CLK_OFF) ||
	    (!(reg3 & PRONTO_PMU_WLAN_AHB_CBCR_CLK_EN))) {
		wcnss_log(ERR, "Cannot log, wlan domain is power collapsed\n");
		return;
	}

	reg = readl_relaxed(penv->wlan_tx_phy_aborts);
	wcnss_log(ERR, "WLAN_TX_PHY_ABORTS %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_APB2PHY_STATUS_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_APB2PHY_STATUS %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_CBR_CCAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_CBR_CCAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_CBR_CAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_CBR_CAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_CBR_CCAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_CBR_CCAHB_TIMEOUT %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_CBR_CAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_CBR_CAHB_TIMEOUT %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_DBR_CDAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_DBR_CDAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_DBR_DAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_DBR_DAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_DBR_CDAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_DBR_CDAHB_TIMEOUT %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_DBR_DAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_DBR_DAHB_TIMEOUT %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_FDBR_CDAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_FDBR_CDAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_FDBR_FDAHB_ERR_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_FDBR_FDAHB_ERR %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_FDBR_CDAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_FDBR_CDAHB_TIMEOUT %08x\n", reg);

	reg_addr = penv->pronto_mcu_base + MCU_FDBR_FDAHB_TIMEOUT_OFFSET;
	reg = readl_relaxed(reg_addr);
	wcnss_log(ERR, "MCU_FDBR_FDAHB_TIMEOUT %08x\n", reg);

	reg = readl_relaxed(penv->wlan_brdg_err_source);
	wcnss_log(ERR, "WLAN_BRDG_ERR_SOURCE %08x\n", reg);

	reg = readl_relaxed(penv->wlan_tx_status);
	wcnss_log(ERR, "WLAN_TXP_STATUS %08x\n", reg);

	reg = readl_relaxed(penv->alarms_txctl);
	wcnss_log(ERR, "ALARMS_TXCTL %08x\n", reg);

	reg = readl_relaxed(penv->alarms_tactl);
	wcnss_log(ERR, "ALARMS_TACTL %08x\n", reg);
}
EXPORT_SYMBOL(wcnss_pronto_log_debug_regs);

#ifdef CONFIG_WCNSS_REGISTER_DUMP_ON_BITE

static int wcnss_gpio_set_state(bool is_enable)
{
	struct pinctrl_state *pin_state;
	int ret;
	int i;

	if (!is_enable) {
		for (i = 0; i < WCNSS_WLAN_MAX_GPIO; i++) {
			if (gpio_is_valid(penv->gpios[i]))
				gpio_free(penv->gpios[i]);
		}

		return 0;
	}

	pin_state = penv->wcnss_gpio_active;
	if (!IS_ERR_OR_NULL(pin_state)) {
		ret = pinctrl_select_state(penv->pinctrl, pin_state);
		if (ret < 0) {
			wcnss_log(ERR, "%s: can not set gpio pins err: %d\n",
			       __func__, ret);
			goto pinctrl_set_err;
		}

	} else {
		wcnss_log(ERR, "%s: invalid gpio pinstate err: %lu\n",
		       __func__, PTR_ERR(pin_state));
		goto pinctrl_set_err;
	}

	for (i = WCNSS_WLAN_DATA2; i <= WCNSS_WLAN_DATA0; i++) {
		ret = gpio_request_one(penv->gpios[i],
				       GPIOF_DIR_IN, NULL);
		if (ret) {
			wcnss_log(ERR, "%s: request failed for gpio:%d\n",
			       __func__, penv->gpios[i]);
			i--;
			goto gpio_req_err;
		}
	}

	for (i = WCNSS_WLAN_SET; i <= WCNSS_WLAN_CLK; i++) {
		ret = gpio_request_one(penv->gpios[i],
				       GPIOF_OUT_INIT_LOW, NULL);
		if (ret) {
			wcnss_log(ERR, "%s: request failed for gpio:%d\n",
			       __func__, penv->gpios[i]);
			i--;
			goto gpio_req_err;
		}
	}

	return 0;

gpio_req_err:
	for (; i >= WCNSS_WLAN_DATA2; --i)
		gpio_free(penv->gpios[i]);

pinctrl_set_err:
	return -EINVAL;
}

static u32 wcnss_rf_read_reg(u32 rf_reg_addr)
{
	int count = 0;
	u32 rf_cmd_and_addr = 0;
	u32 rf_data_received = 0;
	u32 rf_bit = 0;

	if (wcnss_gpio_set_state(true))
		return 0;

	/* Reset the signal if it is already being used. */
	gpio_set_value(penv->gpios[WCNSS_WLAN_SET], 0);
	gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 0);

	/* We start with cmd_set high penv->gpio_base + WCNSS_WLAN_SET = 1. */
	gpio_set_value(penv->gpios[WCNSS_WLAN_SET], 1);

	gpio_direction_output(penv->gpios[WCNSS_WLAN_DATA0], 1);
	gpio_direction_output(penv->gpios[WCNSS_WLAN_DATA1], 1);
	gpio_direction_output(penv->gpios[WCNSS_WLAN_DATA2], 1);

	gpio_set_value(penv->gpios[WCNSS_WLAN_DATA0], 0);
	gpio_set_value(penv->gpios[WCNSS_WLAN_DATA1], 0);
	gpio_set_value(penv->gpios[WCNSS_WLAN_DATA2], 0);

	/* Prepare command and RF register address that need to sent out. */
	rf_cmd_and_addr  = (((WLAN_RF_READ_REG_CMD) |
		(rf_reg_addr << WLAN_RF_REG_ADDR_START_OFFSET)) &
		WLAN_RF_READ_CMD_MASK);
	/* Send 15 bit RF register address */
	for (count = 0; count < WLAN_RF_PREPARE_CMD_DATA; count++) {
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 0);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(penv->gpios[WCNSS_WLAN_DATA0],
			       rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(penv->gpios[WCNSS_WLAN_DATA1], rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(penv->gpios[WCNSS_WLAN_DATA2], rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		/* Send the data out penv->gpio_base + WCNSS_WLAN_CLK = 1 */
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 1);
	}

	/* Pull down the clock signal */
	gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 0);

	/* Configure data pins to input IO pins */
	gpio_direction_input(penv->gpios[WCNSS_WLAN_DATA0]);
	gpio_direction_input(penv->gpios[WCNSS_WLAN_DATA1]);
	gpio_direction_input(penv->gpios[WCNSS_WLAN_DATA2]);

	for (count = 0; count < WLAN_RF_CLK_WAIT_CYCLE; count++) {
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 1);
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 0);
	}

	rf_bit = 0;
	/* Read 16 bit RF register value */
	for (count = 0; count < WLAN_RF_READ_DATA; count++) {
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 1);
		gpio_set_value(penv->gpios[WCNSS_WLAN_CLK], 0);

		rf_bit = gpio_get_value(penv->gpios[WCNSS_WLAN_DATA0]);
		rf_data_received |= (rf_bit << (count * WLAN_RF_DATA_LEN
					+ WLAN_RF_DATA0_SHIFT));

		if (count != 5) {
			rf_bit = gpio_get_value(penv->gpios[WCNSS_WLAN_DATA1]);
			rf_data_received |= (rf_bit << (count * WLAN_RF_DATA_LEN
						+ WLAN_RF_DATA1_SHIFT));

			rf_bit = gpio_get_value(penv->gpios[WCNSS_WLAN_DATA2]);
			rf_data_received |= (rf_bit << (count * WLAN_RF_DATA_LEN
						+ WLAN_RF_DATA2_SHIFT));
		}
	}

	gpio_set_value(penv->gpios[WCNSS_WLAN_SET], 0);
	wcnss_gpio_set_state(false);
	wcnss_pinctrl_set_state(true);

	return rf_data_received;
}

static void wcnss_log_iris_regs(void)
{
	int i;
	u32 reg_val;
	u32 regs_array[] = {
		0x04, 0x05, 0x11, 0x1e, 0x40, 0x48,
		0x49, 0x4b, 0x00, 0x01, 0x4d};

	wcnss_log(INFO, "%s: IRIS Registers [address] : value\n", __func__);

	for (i = 0; i < ARRAY_SIZE(regs_array); i++) {
		reg_val = wcnss_rf_read_reg(regs_array[i]);

		wcnss_log(INFO, "IRIS Reg Addr: [0x%08x] : Reg val: 0x%08x\n",
			  regs_array[i], reg_val);
	}
}

int wcnss_get_mux_control(void)
{
	void __iomem *pmu_conf_reg;
	u32 reg = 0;

	if (!penv)
		return 0;

	pmu_conf_reg = penv->msm_wcnss_base + PRONTO_PMU_OFFSET;
	reg = readl_relaxed(pmu_conf_reg);
	reg |= WCNSS_PMU_CFG_GC_BUS_MUX_SEL_TOP;
	writel_relaxed(reg, pmu_conf_reg);
	return 1;
}

void wcnss_log_debug_regs_on_bite(void)
{
	struct platform_device *pdev = wcnss_get_platform_device();
	struct clk *measure;
	struct clk *wcnss_debug_mux;
	unsigned long clk_rate;

	if (wcnss_hardware_type() != WCNSS_PRONTO_HW)
		return;

	measure = clk_get(&pdev->dev, "measure");
	wcnss_debug_mux = clk_get(&pdev->dev, "wcnss_debug");

	if (!IS_ERR(measure) && !IS_ERR(wcnss_debug_mux)) {
		if (clk_set_parent(measure, wcnss_debug_mux)) {
			wcnss_log(ERR, "Setting measure clk parent failed\n");
			return;
		}

		if (clk_prepare_enable(measure)) {
			wcnss_log(ERR, "measure clk enable failed\n");
			return;
		}

		clk_rate = clk_get_rate(measure);
		wcnss_log(DBG, "clock frequency is: %luHz\n", clk_rate);

		if (clk_rate) {
			wcnss_pronto_log_debug_regs();
			if (wcnss_get_mux_control())
				wcnss_log_iris_regs();
		} else {
			wcnss_log(ERR, "clock frequency is zero, cannot");
			wcnss_log(ERR, " access PMU or other registers\n");
			wcnss_log_iris_regs();
		}

		clk_disable_unprepare(measure);
	}
}
#endif

/* interface to reset wcnss by sending the reset interrupt */
void wcnss_reset_fiq(bool clk_chk_en)
{
	if (wcnss_hardware_type() == WCNSS_PRONTO_HW) {
		if (clk_chk_en) {
			wcnss_log_debug_regs_on_bite();
		} else {
			wcnss_pronto_log_debug_regs();
			if (wcnss_get_mux_control())
				wcnss_log_iris_regs();
		}
		if (!wcnss_device_is_shutdown()) {
			/* Insert memory barrier before writing fiq register */
			wmb();
			__raw_writel(1 << 16, penv->fiq_reg);
		} else {
			wcnss_log(INFO,
			"%s: Block FIQ during power up sequence\n", __func__);
		}
	} else {
		wcnss_riva_log_debug_regs();
	}
}
EXPORT_SYMBOL(wcnss_reset_fiq);

static int wcnss_create_sysfs(struct device *dev)
{
	int ret;

	if (!dev)
		return -ENODEV;

	ret = device_create_file(dev, &dev_attr_thermal_mitigation);
	if (ret)
		return ret;

	ret = device_create_file(dev, &dev_attr_wcnss_version);
	if (ret)
		goto remove_thermal;

	ret = device_create_file(dev, &dev_attr_wcnss_mac_addr);
	if (ret)
		goto remove_version;

	return 0;

remove_version:
	device_remove_file(dev, &dev_attr_wcnss_version);
remove_thermal:
	device_remove_file(dev, &dev_attr_thermal_mitigation);

	return ret;
}

static void wcnss_remove_sysfs(struct device *dev)
{
	if (dev) {
		device_remove_file(dev, &dev_attr_thermal_mitigation);
		device_remove_file(dev, &dev_attr_wcnss_version);
		device_remove_file(dev, &dev_attr_wcnss_mac_addr);
	}
}

static void wcnss_pm_qos_add_request(void)
{
	wcnss_log(INFO, "%s: add request\n", __func__);
	pm_qos_add_request(&penv->wcnss_pm_qos_request, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
}

static void wcnss_pm_qos_remove_request(void)
{
	wcnss_log(INFO, "%s: remove request\n", __func__);
	pm_qos_remove_request(&penv->wcnss_pm_qos_request);
}

void wcnss_pm_qos_update_request(int val)
{
	wcnss_log(INFO, "%s: update request %d\n", __func__, val);
	pm_qos_update_request(&penv->wcnss_pm_qos_request, val);
}

void wcnss_disable_pc_remove_req(void)
{
	mutex_lock(&penv->pm_qos_mutex);
	if (penv->pc_disabled) {
		penv->pc_disabled = 0;
		wcnss_pm_qos_update_request(WCNSS_ENABLE_PC_LATENCY);
		wcnss_pm_qos_remove_request();
		wcnss_allow_suspend();
	}
	mutex_unlock(&penv->pm_qos_mutex);
}

void wcnss_disable_pc_add_req(void)
{
	mutex_lock(&penv->pm_qos_mutex);
	if (!penv->pc_disabled) {
		wcnss_pm_qos_add_request();
		wcnss_prevent_suspend();
		wcnss_pm_qos_update_request(WCNSS_DISABLE_PC_LATENCY);
		penv->pc_disabled = 1;
	}
	mutex_unlock(&penv->pm_qos_mutex);
}

static void wcnss_smd_notify_event(void *data, unsigned int event)
{
	int len = 0;

	if (penv != data) {
		wcnss_log(ERR, "invalid env pointer in smd callback\n");
		return;
	}
	switch (event) {
	case SMD_EVENT_DATA:
		len = smd_read_avail(penv->smd_ch);
		if (len < 0) {
			wcnss_log(ERR, "failed to read from smd %d\n", len);
			return;
		}
		schedule_work(&penv->wcnssctrl_rx_work);
		break;

	case SMD_EVENT_OPEN:
		wcnss_log(DBG, "opening WCNSS SMD channel :%s",
			 WCNSS_CTRL_CHANNEL);
		schedule_work(&penv->wcnssctrl_version_work);
		schedule_work(&penv->wcnss_pm_config_work);
		cancel_delayed_work(&penv->wcnss_pm_qos_del_req);
		schedule_delayed_work(&penv->wcnss_pm_qos_del_req, 0);
		if (penv->wlan_config.is_pronto_vadc && (penv->vadc_dev))
			schedule_work(&penv->wcnss_vadc_work);
		break;

	case SMD_EVENT_CLOSE:
		wcnss_log(DBG, "closing WCNSS SMD channel :%s",
			 WCNSS_CTRL_CHANNEL);
		penv->nv_downloaded = 0;
		penv->is_cbc_done = 0;
		break;

	default:
		break;
	}
}

static int
wcnss_pinctrl_set_state(bool active)
{
	struct pinctrl_state *pin_state;
	int ret;

	wcnss_log(DBG, "%s: Set GPIO state : %d\n", __func__, active);

	pin_state = active ? penv->wcnss_5wire_active
			: penv->wcnss_5wire_suspend;

	if (!IS_ERR_OR_NULL(pin_state)) {
		ret = pinctrl_select_state(penv->pinctrl, pin_state);
		if (ret < 0) {
			wcnss_log(ERR, "%s: can not set %s pins\n", __func__,
			       active ? WCNSS_PINCTRL_STATE_DEFAULT
			       : WCNSS_PINCTRL_STATE_SLEEP);
			return ret;
		}
	} else {
		wcnss_log(ERR, "%s: invalid '%s' pinstate\n", __func__,
		       active ? WCNSS_PINCTRL_STATE_DEFAULT
		       : WCNSS_PINCTRL_STATE_SLEEP);
		return PTR_ERR(pin_state);
	}

	return 0;
}

static int
wcnss_pinctrl_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i;

	/* Get pinctrl if target uses pinctrl */
	penv->pinctrl = devm_pinctrl_get(&pdev->dev);

	if (IS_ERR_OR_NULL(penv->pinctrl)) {
		wcnss_log(ERR, "%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(penv->pinctrl);
	}

	penv->wcnss_5wire_active
		= pinctrl_lookup_state(penv->pinctrl,
			WCNSS_PINCTRL_STATE_DEFAULT);

	if (IS_ERR_OR_NULL(penv->wcnss_5wire_active)) {
		wcnss_log(ERR, "%s: can not get default pinstate\n", __func__);
		return PTR_ERR(penv->wcnss_5wire_active);
	}

	penv->wcnss_5wire_suspend
		= pinctrl_lookup_state(penv->pinctrl,
			WCNSS_PINCTRL_STATE_SLEEP);

	if (IS_ERR_OR_NULL(penv->wcnss_5wire_suspend)) {
		wcnss_log(WARN, "%s: can not get sleep pinstate\n", __func__);
		return PTR_ERR(penv->wcnss_5wire_suspend);
	}

	penv->wcnss_gpio_active = pinctrl_lookup_state(penv->pinctrl,
					WCNSS_PINCTRL_GPIO_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(penv->wcnss_gpio_active))
		wcnss_log(WARN, "%s: can not get gpio default pinstate\n",
			  __func__);

	for (i = 0; i < WCNSS_WLAN_MAX_GPIO; i++) {
		penv->gpios[i] = of_get_gpio(node, i);
		if (penv->gpios[i] < 0)
			wcnss_log(WARN, "%s: Fail to get 5wire gpio: %d\n",
				__func__, i);
	}

	return 0;
}

static int
wcnss_pronto_gpios_config(struct platform_device *pdev, bool enable)
{
	int rc = 0;
	int i, j;
	int WCNSS_WLAN_NUM_GPIOS = 5;

	/* Use Pinctrl to configure 5 wire GPIOs */
	rc = wcnss_pinctrl_init(pdev);
	if (rc) {
		wcnss_log(ERR, "%s: failed to get pin resources\n", __func__);
		penv->pinctrl = NULL;
		goto gpio_probe;
	} else {
		rc = wcnss_pinctrl_set_state(true);
		if (rc)
			wcnss_log(ERR, "%s: failed to set pin state\n",
			       __func__);
		penv->use_pinctrl = true;
		return rc;
	}

gpio_probe:
	for (i = 0; i < WCNSS_WLAN_NUM_GPIOS; i++) {
		int gpio = of_get_gpio(pdev->dev.of_node, i);

		if (enable) {
			rc = gpio_request(gpio, "wcnss_wlan");
			if (rc) {
				wcnss_log(ERR, "WCNSS gpio_request %d err %d\n",
				       gpio, rc);
				goto fail;
			}
		} else {
			gpio_free(gpio);
		}
	}
	return rc;

fail:
	for (j = WCNSS_WLAN_NUM_GPIOS - 1; j >= 0; j--) {
		int gpio = of_get_gpio(pdev->dev.of_node, i);

		gpio_free(gpio);
	}
	return rc;
}

static int
wcnss_gpios_config(struct resource *gpios_5wire, bool enable)
{
	int i, j;
	int rc = 0;

	for (i = gpios_5wire->start; i <= gpios_5wire->end; i++) {
		if (enable) {
			rc = gpio_request(i, gpios_5wire->name);
			if (rc) {
				wcnss_log(ERR,
					  "gpio_request %d err %d\n", i, rc);
				goto fail;
			}
		} else {
			gpio_free(i);
		}
	}

	return rc;

fail:
	for (j = i - 1; j >= gpios_5wire->start; j--)
		gpio_free(j);
	return rc;
}

static int
wcnss_wlan_ctrl_probe(struct platform_device *pdev)
{
	if (!penv || !penv->triggered)
		return -ENODEV;

	penv->smd_channel_ready = 1;

	wcnss_log(INFO, "%s: SMD ctrl channel up\n", __func__);
	return 0;
}

static int
wcnss_wlan_ctrl_remove(struct platform_device *pdev)
{
	if (penv)
		penv->smd_channel_ready = 0;

	wcnss_log(INFO, "%s: SMD ctrl channel down\n", __func__);

	return 0;
}

static struct platform_driver wcnss_wlan_ctrl_driver = {
	.driver = {
		.name	= "WLAN_CTRL",
		.owner	= THIS_MODULE,
	},
	.probe	= wcnss_wlan_ctrl_probe,
	.remove	= wcnss_wlan_ctrl_remove,
};

static int
wcnss_ctrl_remove(struct platform_device *pdev)
{
	if (penv && penv->smd_ch)
		smd_close(penv->smd_ch);

	return 0;
}

static int
wcnss_ctrl_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (!penv || !penv->triggered)
		return -ENODEV;

	ret = smd_named_open_on_edge(WCNSS_CTRL_CHANNEL, SMD_APPS_WCNSS,
				     &penv->smd_ch, penv,
				     wcnss_smd_notify_event);
	if (ret < 0) {
		wcnss_log(ERR, "cannot open the smd command channel %s: %d\n",
		       WCNSS_CTRL_CHANNEL, ret);
		return -ENODEV;
	}
	smd_disable_read_intr(penv->smd_ch);

	return 0;
}

/* platform device for WCNSS_CTRL SMD channel */
static struct platform_driver wcnss_ctrl_driver = {
	.driver = {
		.name	= "WCNSS_CTRL",
		.owner	= THIS_MODULE,
	},
	.probe	= wcnss_ctrl_probe,
	.remove	= wcnss_ctrl_remove,
};

struct device *wcnss_wlan_get_device(void)
{
	if (penv && penv->pdev && penv->smd_channel_ready)
		return &penv->pdev->dev;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_device);

void wcnss_get_monotonic_boottime(struct timespec *ts)
{
	get_monotonic_boottime(ts);
}
EXPORT_SYMBOL(wcnss_get_monotonic_boottime);

struct platform_device *wcnss_get_platform_device(void)
{
	if (penv && penv->pdev)
		return penv->pdev;
	return NULL;
}
EXPORT_SYMBOL(wcnss_get_platform_device);

struct wcnss_wlan_config *wcnss_get_wlan_config(void)
{
	if (penv && penv->pdev)
		return &penv->wlan_config;
	return NULL;
}
EXPORT_SYMBOL(wcnss_get_wlan_config);

int wcnss_is_hw_pronto_ver3(void)
{
	if (penv && penv->pdev) {
		if (penv->wlan_config.is_pronto_v3)
			return penv->wlan_config.is_pronto_v3;
	}
	return 0;
}
EXPORT_SYMBOL(wcnss_is_hw_pronto_ver3);

int wcnss_device_ready(void)
{
	if (penv && penv->pdev && penv->nv_downloaded &&
	    !wcnss_device_is_shutdown())
		return 1;
	return 0;
}
EXPORT_SYMBOL(wcnss_device_ready);

bool wcnss_cbc_complete(void)
{
	if (penv && penv->pdev && penv->is_cbc_done &&
	    !wcnss_device_is_shutdown())
		return true;
	return false;
}
EXPORT_SYMBOL(wcnss_cbc_complete);

int wcnss_device_is_shutdown(void)
{
	if (penv && penv->is_shutdown)
		return 1;
	return 0;
}
EXPORT_SYMBOL(wcnss_device_is_shutdown);

struct resource *wcnss_wlan_get_memory_map(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) && penv->smd_channel_ready)
		return penv->mmio_res;
	return NULL;
}
EXPORT_SYMBOL(wcnss_wlan_get_memory_map);

int wcnss_wlan_get_dxe_tx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->tx_irq_res && penv->smd_channel_ready)
		return penv->tx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_tx_irq);

int wcnss_wlan_get_dxe_rx_irq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->rx_irq_res && penv->smd_channel_ready)
		return penv->rx_irq_res->start;
	return WCNSS_WLAN_IRQ_INVALID;
}
EXPORT_SYMBOL(wcnss_wlan_get_dxe_rx_irq);

void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops)
{
	if (penv && dev && (dev == &penv->pdev->dev) && pm_ops)
		penv->pm_ops = pm_ops;
}
EXPORT_SYMBOL(wcnss_wlan_register_pm_ops);

void wcnss_wlan_unregister_pm_ops(struct device *dev,
				  const struct dev_pm_ops *pm_ops)
{
	if (penv && dev && (dev == &penv->pdev->dev) && pm_ops) {
		if (!penv->pm_ops) {
			wcnss_log(ERR, "%s: pm_ops is already unregistered.\n",
			       __func__);
			return;
		}

		if (pm_ops->suspend != penv->pm_ops->suspend ||
		    pm_ops->resume != penv->pm_ops->resume)
			wcnss_log(ERR,
			"PM APIs dont match with registered APIs\n");
		penv->pm_ops = NULL;
	}
}
EXPORT_SYMBOL(wcnss_wlan_unregister_pm_ops);

void wcnss_register_thermal_mitigation(struct device *dev,
				       void (*tm_notify)(struct device *, int))
{
	if (penv && dev && tm_notify)
		penv->tm_notify = tm_notify;
}
EXPORT_SYMBOL(wcnss_register_thermal_mitigation);

void wcnss_unregister_thermal_mitigation(
				void (*tm_notify)(struct device *, int))
{
	if (penv && tm_notify) {
		if (tm_notify != penv->tm_notify)
			wcnss_log(ERR, "tm_notify doesn't match registered\n");
		penv->tm_notify = NULL;
	}
}
EXPORT_SYMBOL(wcnss_unregister_thermal_mitigation);

unsigned int wcnss_get_serial_number(void)
{
	if (penv) {
		penv->serial_number = socinfo_get_serial_number();
		wcnss_log(INFO, "%s: Device serial number: %u\n",
			__func__, penv->serial_number);
		return penv->serial_number;
	}

	return 0;
}
EXPORT_SYMBOL(wcnss_get_serial_number);

int wcnss_get_wlan_mac_address(char mac_addr[WLAN_MAC_ADDR_SIZE])
{
	if (!penv)
		return -ENODEV;

	memcpy(mac_addr, penv->wlan_nv_mac_addr, WLAN_MAC_ADDR_SIZE);
	wcnss_log(DBG, "%s: Get MAC Addr:" MAC_ADDRESS_STR "\n", __func__,
		 penv->wlan_nv_mac_addr[0], penv->wlan_nv_mac_addr[1],
		 penv->wlan_nv_mac_addr[2], penv->wlan_nv_mac_addr[3],
		 penv->wlan_nv_mac_addr[4], penv->wlan_nv_mac_addr[5]);
	return 0;
}
EXPORT_SYMBOL(wcnss_get_wlan_mac_address);

static int enable_wcnss_suspend_notify;

static int enable_wcnss_suspend_notify_set(const char *val,
					   const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	if (enable_wcnss_suspend_notify)
		wcnss_log(DBG, "Suspend notification activated for wcnss\n");

	return 0;
}
module_param_call(enable_wcnss_suspend_notify, enable_wcnss_suspend_notify_set,
		  param_get_int, &enable_wcnss_suspend_notify, 0644);

int wcnss_xo_auto_detect_enabled(void)
{
	return (has_autodetect_xo == 1 ? 1 : 0);
}

void wcnss_set_iris_xo_mode(int iris_xo_mode_set)
{
	penv->iris_xo_mode_set = iris_xo_mode_set;
}
EXPORT_SYMBOL(wcnss_set_iris_xo_mode);

int wcnss_wlan_iris_xo_mode(void)
{
	if (penv && penv->pdev && penv->smd_channel_ready)
		return penv->iris_xo_mode_set;
	return -ENODEV;
}
EXPORT_SYMBOL(wcnss_wlan_iris_xo_mode);

int wcnss_wlan_dual_band_disabled(void)
{
	if (penv && penv->pdev)
		return penv->is_dual_band_disabled;

	return -EINVAL;
}
EXPORT_SYMBOL(wcnss_wlan_dual_band_disabled);

void wcnss_suspend_notify(void)
{
	void __iomem *pmu_spare_reg;
	u32 reg = 0;
	unsigned long flags;

	if (!enable_wcnss_suspend_notify)
		return;

	if (wcnss_hardware_type() == WCNSS_PRONTO_HW)
		return;

	/* For Riva */
	pmu_spare_reg = penv->msm_wcnss_base + RIVA_SPARE_OFFSET;
	spin_lock_irqsave(&reg_spinlock, flags);
	reg = readl_relaxed(pmu_spare_reg);
	reg |= RIVA_SUSPEND_BIT;
	writel_relaxed(reg, pmu_spare_reg);
	spin_unlock_irqrestore(&reg_spinlock, flags);
}
EXPORT_SYMBOL(wcnss_suspend_notify);

void wcnss_resume_notify(void)
{
	void __iomem *pmu_spare_reg;
	u32 reg = 0;
	unsigned long flags;

	if (!enable_wcnss_suspend_notify)
		return;

	if (wcnss_hardware_type() == WCNSS_PRONTO_HW)
		return;

	/* For Riva */
	pmu_spare_reg = penv->msm_wcnss_base + RIVA_SPARE_OFFSET;

	spin_lock_irqsave(&reg_spinlock, flags);
	reg = readl_relaxed(pmu_spare_reg);
	reg &= ~RIVA_SUSPEND_BIT;
	writel_relaxed(reg, pmu_spare_reg);
	spin_unlock_irqrestore(&reg_spinlock, flags);
}
EXPORT_SYMBOL(wcnss_resume_notify);

static int wcnss_wlan_suspend(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->suspend)
		return penv->pm_ops->suspend(dev);
	return 0;
}

static int wcnss_wlan_suspend_noirq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->suspend_noirq)
		return penv->pm_ops->suspend_noirq(dev);
	return 0;
}

static int wcnss_wlan_resume(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->resume)
		return penv->pm_ops->resume(dev);
	return 0;
}

static int wcnss_wlan_resume_noirq(struct device *dev)
{
	if (penv && dev && (dev == &penv->pdev->dev) &&
	    penv->smd_channel_ready &&
	    penv->pm_ops && penv->pm_ops->resume_noirq)
		return penv->pm_ops->resume_noirq(dev);
	return 0;
}

void wcnss_prevent_suspend(void)
{
	if (penv)
		__pm_stay_awake(&penv->wcnss_wake_lock);
}
EXPORT_SYMBOL(wcnss_prevent_suspend);

void wcnss_allow_suspend(void)
{
	if (penv)
		__pm_relax(&penv->wcnss_wake_lock);
}
EXPORT_SYMBOL(wcnss_allow_suspend);

int wcnss_hardware_type(void)
{
	if (penv)
		return penv->wcnss_hw_type;
	else
		return -ENODEV;
}
EXPORT_SYMBOL(wcnss_hardware_type);

int fw_cal_data_available(void)
{
	if (penv)
		return penv->fw_cal_available;
	else
		return -ENODEV;
}

u32 wcnss_get_wlan_rx_buff_count(void)
{
	if (penv)
		return penv->wlan_rx_buff_count;
	else
		return WCNSS_DEF_WLAN_RX_BUFF_COUNT;
}
EXPORT_SYMBOL(wcnss_get_wlan_rx_buff_count);

int wcnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	if (penv && unsafe_ch_list &&
	    (ch_count <= WCNSS_MAX_CH_NUM)) {
		memcpy((char *)penv->unsafe_ch_list,
		       (char *)unsafe_ch_list, ch_count * sizeof(u16));
		penv->unsafe_ch_count = ch_count;
		return 0;
	} else {
		return -ENODEV;
	}
}
EXPORT_SYMBOL(wcnss_set_wlan_unsafe_channel);

int wcnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 buffer_size,
				  u16 *ch_count)
{
	if (penv) {
		if (buffer_size < penv->unsafe_ch_count * sizeof(u16))
			return -ENODEV;
		memcpy((char *)unsafe_ch_list,
		       (char *)penv->unsafe_ch_list,
		       penv->unsafe_ch_count * sizeof(u16));
		*ch_count = penv->unsafe_ch_count;
		return 0;
	} else {
		return -ENODEV;
	}
}
EXPORT_SYMBOL(wcnss_get_wlan_unsafe_channel);

static int wcnss_smd_tx(void *data, int len)
{
	int ret = 0;

	ret = smd_write_avail(penv->smd_ch);
	if (ret < len) {
		wcnss_log(ERR, "no space available for smd frame\n");
		return -ENOSPC;
	}
	ret = smd_write(penv->smd_ch, data, len);
	if (ret < len) {
		wcnss_log(ERR, "failed to write Command %d", len);
		ret = -ENODEV;
	}
	return ret;
}

static int wcnss_get_battery_volt(int *result_uv)
{
	int rc = -1;
	struct qpnp_vadc_result adc_result;

	if (!penv->vadc_dev) {
		wcnss_log(ERR, "not setting up vadc\n");
		return rc;
	}

	rc = qpnp_vadc_read(penv->vadc_dev, VBAT_SNS, &adc_result);
	if (rc) {
		wcnss_log(ERR, "error reading adc channel = %d, rc = %d\n",
		       VBAT_SNS, rc);
		return rc;
	}

	wcnss_log(INFO, "Battery mvolts phy=%lld meas=0x%llx\n",
		  adc_result.physical, adc_result.measurement);
	*result_uv = (int)adc_result.physical;

	return 0;
}

static void wcnss_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	int rc = 0;

	mutex_lock(&penv->vbat_monitor_mutex);
	cancel_delayed_work_sync(&penv->vbatt_work);

	if (state == ADC_TM_LOW_STATE) {
		wcnss_log(DBG, "low voltage notification triggered\n");
		penv->vbat_monitor_params.state_request =
			ADC_TM_HIGH_THR_ENABLE;
		penv->vbat_monitor_params.high_thr = WCNSS_VBATT_THRESHOLD +
		WCNSS_VBATT_GUARD;
		penv->vbat_monitor_params.low_thr = 0;
	} else if (state == ADC_TM_HIGH_STATE) {
		penv->vbat_monitor_params.state_request =
			ADC_TM_LOW_THR_ENABLE;
		penv->vbat_monitor_params.low_thr = WCNSS_VBATT_THRESHOLD -
		WCNSS_VBATT_GUARD;
		penv->vbat_monitor_params.high_thr = 0;
		wcnss_log(DBG, "high voltage notification triggered\n");
	} else {
		wcnss_log(DBG, "unknown voltage notification state: %d\n",
			 state);
		mutex_unlock(&penv->vbat_monitor_mutex);
		return;
	}
	wcnss_log(DBG, "set low thr to %d and high to %d\n",
		 penv->vbat_monitor_params.low_thr,
		 penv->vbat_monitor_params.high_thr);

	rc = qpnp_adc_tm_channel_measure(penv->adc_tm_dev,
					 &penv->vbat_monitor_params);

	if (rc)
		wcnss_log(ERR, "%s: tm setup failed: %d\n", __func__, rc);
	else
		schedule_delayed_work(&penv->vbatt_work,
				      msecs_to_jiffies(2000));

	mutex_unlock(&penv->vbat_monitor_mutex);
}

static int wcnss_setup_vbat_monitoring(void)
{
	int rc = -1;

	if (!penv->adc_tm_dev) {
		wcnss_log(ERR, "not setting up vbatt\n");
		return rc;
	}
	penv->vbat_monitor_params.low_thr = WCNSS_VBATT_THRESHOLD;
	penv->vbat_monitor_params.high_thr = WCNSS_VBATT_THRESHOLD;
	penv->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;

	if (penv->is_vsys_adc_channel)
		penv->vbat_monitor_params.channel = VSYS;
	else
		penv->vbat_monitor_params.channel = VBAT_SNS;

	penv->vbat_monitor_params.btm_ctx = (void *)penv;
	penv->vbat_monitor_params.timer_interval = ADC_MEAS1_INTERVAL_1S;
	penv->vbat_monitor_params.threshold_notification = &wcnss_notify_vbat;
	wcnss_log(DBG, "set low thr to %d and high to %d\n",
		 penv->vbat_monitor_params.low_thr,
		 penv->vbat_monitor_params.high_thr);

	rc = qpnp_adc_tm_channel_measure(penv->adc_tm_dev,
					 &penv->vbat_monitor_params);
	if (rc)
		wcnss_log(ERR, "%s: tm setup failed: %d\n", __func__, rc);

	return rc;
}

static void wcnss_send_vbatt_indication(struct work_struct *work)
{
	struct vbatt_message vbatt_msg;
	int ret = 0;

	vbatt_msg.hdr.msg_type = WCNSS_VBATT_LEVEL_IND;
	vbatt_msg.hdr.msg_len = sizeof(struct vbatt_message);
	vbatt_msg.vbatt.threshold = WCNSS_VBATT_THRESHOLD;

	mutex_lock(&penv->vbat_monitor_mutex);
	vbatt_msg.vbatt.curr_volt = penv->wlan_config.vbatt;
	mutex_unlock(&penv->vbat_monitor_mutex);
	wcnss_log(DBG, "send curr_volt: %d to FW\n",
		 vbatt_msg.vbatt.curr_volt);

	ret = wcnss_smd_tx(&vbatt_msg, vbatt_msg.hdr.msg_len);
	if (ret < 0)
		wcnss_log(ERR, "smd tx failed\n");
}

static void wcnss_update_vbatt(struct work_struct *work)
{
	struct vbatt_message vbatt_msg;
	int ret = 0;

	vbatt_msg.hdr.msg_type = WCNSS_VBATT_LEVEL_IND;
	vbatt_msg.hdr.msg_len = sizeof(struct vbatt_message);
	vbatt_msg.vbatt.threshold = WCNSS_VBATT_THRESHOLD;

	mutex_lock(&penv->vbat_monitor_mutex);
	if (penv->vbat_monitor_params.low_thr &&
	    (penv->fw_vbatt_state == WCNSS_VBATT_LOW ||
	     penv->fw_vbatt_state == WCNSS_CONFIG_UNSPECIFIED)) {
		vbatt_msg.vbatt.curr_volt = WCNSS_VBATT_HIGH;
		penv->fw_vbatt_state = WCNSS_VBATT_HIGH;
		wcnss_log(DBG, "send HIGH BATT to FW\n");
	} else if (!penv->vbat_monitor_params.low_thr &&
		   (penv->fw_vbatt_state == WCNSS_VBATT_HIGH ||
		    penv->fw_vbatt_state == WCNSS_CONFIG_UNSPECIFIED)){
		vbatt_msg.vbatt.curr_volt = WCNSS_VBATT_LOW;
		penv->fw_vbatt_state = WCNSS_VBATT_LOW;
		wcnss_log(DBG, "send LOW BATT to FW\n");
	} else {
		mutex_unlock(&penv->vbat_monitor_mutex);
		return;
	}
	mutex_unlock(&penv->vbat_monitor_mutex);
	ret = wcnss_smd_tx(&vbatt_msg, vbatt_msg.hdr.msg_len);
	if (ret < 0)
		wcnss_log(ERR, "smd tx failed\n");
}

static unsigned char wcnss_fw_status(void)
{
	int len = 0;
	int rc = 0;

	unsigned char fw_status = 0xFF;

	len = smd_read_avail(penv->smd_ch);
	if (len < 1) {
		wcnss_log(ERR, "%s: invalid firmware status", __func__);
		return fw_status;
	}

	rc = smd_read(penv->smd_ch, &fw_status, 1);
	if (rc < 0) {
		wcnss_log(ERR, "%s: incomplete data read from smd\n", __func__);
		return fw_status;
	}
	return fw_status;
}

static void wcnss_send_cal_rsp(unsigned char fw_status)
{
	struct smd_msg_hdr *rsphdr;
	unsigned char *msg = NULL;
	int rc;

	msg = kmalloc((sizeof(*rsphdr) + 1), GFP_KERNEL);
	if (!msg)
		return;

	rsphdr = (struct smd_msg_hdr *)msg;
	rsphdr->msg_type = WCNSS_CALDATA_UPLD_RSP;
	rsphdr->msg_len = sizeof(struct smd_msg_hdr) + 1;
	memcpy(msg + sizeof(struct smd_msg_hdr), &fw_status, 1);

	rc = wcnss_smd_tx(msg, rsphdr->msg_len);
	if (rc < 0)
		wcnss_log(ERR, "smd tx failed\n");

	kfree(msg);
}

/* Collect calibrated data from WCNSS */
void extract_cal_data(int len)
{
	int rc;
	struct cal_data_params calhdr;
	unsigned char fw_status = WCNSS_RESP_FAIL;

	if (len < sizeof(struct cal_data_params)) {
		wcnss_log(ERR, "incomplete cal header length\n");
		return;
	}

	mutex_lock(&penv->dev_lock);
	rc = smd_read(penv->smd_ch, (unsigned char *)&calhdr,
		      sizeof(struct cal_data_params));
	if (rc < sizeof(struct cal_data_params)) {
		wcnss_log(ERR, "incomplete cal header read from smd\n");
		mutex_unlock(&penv->dev_lock);
		return;
	}

	if (penv->fw_cal_exp_frag != calhdr.frag_number) {
		wcnss_log(ERR, "Invalid frgament");
		goto unlock_exit;
	}

	if (calhdr.frag_size > WCNSS_MAX_FRAME_SIZE) {
		wcnss_log(ERR, "Invalid fragment size");
		goto unlock_exit;
	}

	if (penv->fw_cal_available) {
		/* ignore cal upload from SSR */
		smd_read(penv->smd_ch, NULL, calhdr.frag_size);
		penv->fw_cal_exp_frag++;
		if (calhdr.msg_flags & LAST_FRAGMENT) {
			penv->fw_cal_exp_frag = 0;
			goto unlock_exit;
		}
		mutex_unlock(&penv->dev_lock);
		return;
	}

	if (calhdr.frag_number == 0) {
		if (calhdr.total_size > MAX_CALIBRATED_DATA_SIZE) {
			wcnss_log(ERR, "Invalid cal data size %d",
			       calhdr.total_size);
			goto unlock_exit;
		}
		kfree(penv->fw_cal_data);
		penv->fw_cal_rcvd = 0;
		penv->fw_cal_data = kmalloc(calhdr.total_size,
				GFP_KERNEL);
		if (!penv->fw_cal_data) {
			smd_read(penv->smd_ch, NULL, calhdr.frag_size);
			goto unlock_exit;
		}
	}

	if (penv->fw_cal_rcvd + calhdr.frag_size >
			MAX_CALIBRATED_DATA_SIZE) {
		wcnss_log(ERR, "calibrated data size is more than expected %d",
		       penv->fw_cal_rcvd + calhdr.frag_size);
		penv->fw_cal_exp_frag = 0;
		penv->fw_cal_rcvd = 0;
		smd_read(penv->smd_ch, NULL, calhdr.frag_size);
		goto unlock_exit;
	}

	rc = smd_read(penv->smd_ch, penv->fw_cal_data + penv->fw_cal_rcvd,
		      calhdr.frag_size);
	if (rc < calhdr.frag_size)
		goto unlock_exit;

	penv->fw_cal_exp_frag++;
	penv->fw_cal_rcvd += calhdr.frag_size;

	if (calhdr.msg_flags & LAST_FRAGMENT) {
		penv->fw_cal_exp_frag = 0;
		penv->fw_cal_available = true;
		wcnss_log(INFO, "cal data collection completed\n");
	}
	mutex_unlock(&penv->dev_lock);
	wake_up(&penv->read_wait);

	if (penv->fw_cal_available) {
		fw_status = WCNSS_RESP_SUCCESS;
		wcnss_send_cal_rsp(fw_status);
	}
	return;

unlock_exit:
	mutex_unlock(&penv->dev_lock);
	wcnss_send_cal_rsp(fw_status);
}

static void wcnss_process_smd_msg(int len)
{
	int rc = 0;
	unsigned char buf[sizeof(struct wcnss_version)];
	unsigned char build[WCNSS_MAX_BUILD_VER_LEN + 1];
	struct smd_msg_hdr *phdr;
	struct smd_msg_hdr smd_msg;
	struct wcnss_version *pversion;
	int hw_type;
	unsigned char fw_status = 0;

	rc = smd_read(penv->smd_ch, buf, sizeof(struct smd_msg_hdr));
	if (rc < sizeof(struct smd_msg_hdr)) {
		wcnss_log(ERR, "incomplete header read from smd\n");
		return;
	}
	len -= sizeof(struct smd_msg_hdr);

	phdr = (struct smd_msg_hdr *)buf;

	switch (phdr->msg_type) {
	case WCNSS_VERSION_RSP:
		if (len != sizeof(struct wcnss_version)
				- sizeof(struct smd_msg_hdr)) {
			wcnss_log(ERR, "invalid version data from wcnss %d\n",
			       len);
			return;
		}
		rc = smd_read(penv->smd_ch, buf + sizeof(struct smd_msg_hdr),
			      len);
		if (rc < len) {
			wcnss_log(ERR, "incomplete data read from smd\n");
			return;
		}
		pversion = (struct wcnss_version *)buf;
		penv->fw_major = pversion->major;
		penv->fw_minor = pversion->minor;
		snprintf(penv->wcnss_version, WCNSS_VERSION_LEN,
			 "%02x%02x%02x%02x", pversion->major, pversion->minor,
			 pversion->version, pversion->revision);
		wcnss_log(INFO, "version %s\n", penv->wcnss_version);
		/* schedule work to download nvbin to ccpu */
		hw_type = wcnss_hardware_type();
		switch (hw_type) {
		case WCNSS_RIVA_HW:
			/* supported only if riva major >= 1 and minor >= 4 */
			if ((pversion->major >= 1) && (pversion->minor >= 4)) {
				wcnss_log(INFO,
					  "schedule download work for riva\n");
				schedule_work(&penv->wcnssctrl_nvbin_dnld_work);
			}
			break;

		case WCNSS_PRONTO_HW:
			smd_msg.msg_type = WCNSS_BUILD_VER_REQ;
			smd_msg.msg_len = sizeof(smd_msg);
			rc = wcnss_smd_tx(&smd_msg, smd_msg.msg_len);
			if (rc < 0)
				wcnss_log(ERR, "smd tx failed: %s\n", __func__);

			/* supported only if pronto major >= 1 and minor >= 4 */
			if ((pversion->major >= 1) && (pversion->minor >= 4)) {
				wcnss_log(INFO,
					  "schedule dnld work for pronto\n");
				schedule_work(&penv->wcnssctrl_nvbin_dnld_work);
			}
			break;

		default:
			wcnss_log(INFO,
			"unknown hw type (%d) will not schedule dnld work\n",
			hw_type);
			break;
		}
		break;

	case WCNSS_BUILD_VER_RSP:
		if (len > WCNSS_MAX_BUILD_VER_LEN) {
			wcnss_log(ERR,
			"invalid build version data from wcnss %d\n", len);
			return;
		}
		rc = smd_read(penv->smd_ch, build, len);
		if (rc < len) {
			wcnss_log(ERR, "incomplete data read from smd\n");
			return;
		}
		build[len] = 0;
		wcnss_log(INFO, "build version %s\n", build);
		break;

	case WCNSS_NVBIN_DNLD_RSP:
		penv->nv_downloaded = true;
		fw_status = wcnss_fw_status();
		wcnss_log(DBG, "received WCNSS_NVBIN_DNLD_RSP from ccpu %u\n",
			 fw_status);
		if (fw_status != WAIT_FOR_CBC_IND)
			penv->is_cbc_done = 1;
		wcnss_setup_vbat_monitoring();
		break;

	case WCNSS_CALDATA_DNLD_RSP:
		penv->nv_downloaded = true;
		fw_status = wcnss_fw_status();
		wcnss_log(DBG, "received WCNSS_CALDATA_DNLD_RSP from ccpu %u\n",
			 fw_status);
		break;
	case WCNSS_CBC_COMPLETE_IND:
		penv->is_cbc_done = 1;
		wcnss_log(DBG, "received WCNSS_CBC_COMPLETE_IND from FW\n");
		break;

	case WCNSS_CALDATA_UPLD_REQ:
		extract_cal_data(len);
		break;

	default:
		wcnss_log(ERR, "invalid message type %d\n", phdr->msg_type);
	}
}

static void wcnssctrl_rx_handler(struct work_struct *worker)
{
	int len;

	while (1) {
		len = smd_read_avail(penv->smd_ch);
		if (len == 0) {
			pr_debug("wcnss: No more data to be read\n");
			return;
		}

		if (len > WCNSS_MAX_FRAME_SIZE) {
			pr_err("wcnss: frame larger than the allowed size\n");
			smd_read(penv->smd_ch, NULL, len);
			return;
		}

		if (len < sizeof(struct smd_msg_hdr)) {
			pr_err("wcnss: incomplete header available len = %d\n",
			       len);
			return;
	}

		wcnss_process_smd_msg(len);
	}
}

static void wcnss_send_version_req(struct work_struct *worker)
{
	struct smd_msg_hdr smd_msg;
	int ret = 0;

	smd_msg.msg_type = WCNSS_VERSION_REQ;
	smd_msg.msg_len = sizeof(smd_msg);
	ret = wcnss_smd_tx(&smd_msg, smd_msg.msg_len);
	if (ret < 0)
		wcnss_log(ERR, "smd tx failed\n");
}

static void wcnss_send_pm_config(struct work_struct *worker)
{
	struct smd_msg_hdr *hdr;
	unsigned char *msg = NULL;
	int rc, prop_len;
	u32 *payload;

	if (!of_find_property(penv->pdev->dev.of_node,
			      "qcom,wcnss-pm", &prop_len))
		return;

	msg = kmalloc((sizeof(struct smd_msg_hdr) + prop_len), GFP_KERNEL);
	if (!msg)
		return;

	payload = (u32 *)(msg + sizeof(struct smd_msg_hdr));

	prop_len /= sizeof(int);

	rc = of_property_read_u32_array(penv->pdev->dev.of_node,
					"qcom,wcnss-pm", payload, prop_len);
	if (rc < 0) {
		wcnss_log(ERR, "property read failed\n");
		kfree(msg);
		return;
	}

	wcnss_log(DBG, "%s:size=%d: <%d, %d, %d, %d, %d %d>\n", __func__,
		 prop_len, *payload, *(payload + 1), *(payload + 2),
		 *(payload + 3), *(payload + 4), *(payload + 5));

	hdr = (struct smd_msg_hdr *)msg;
	hdr->msg_type = WCNSS_PM_CONFIG_REQ;
	hdr->msg_len = sizeof(struct smd_msg_hdr) + (prop_len * sizeof(int));

	rc = wcnss_smd_tx(msg, hdr->msg_len);
	if (rc < 0)
		wcnss_log(ERR, "smd tx failed\n");

	kfree(msg);
}

static void wcnss_pm_qos_enable_pc(struct work_struct *worker)
{
	wcnss_disable_pc_remove_req();
}

static DECLARE_RWSEM(wcnss_pm_sem);

static void wcnss_nvbin_dnld(void)
{
	int ret = 0;
	struct nvbin_dnld_req_msg *dnld_req_msg;
	unsigned short total_fragments = 0;
	unsigned short count = 0;
	unsigned short retry_count = 0;
	unsigned short cur_frag_size = 0;
	unsigned char *outbuffer = NULL;
	const void *nv_blob_addr = NULL;
	unsigned int nv_blob_size = 0;
	const struct firmware *nv = NULL;
	struct device *dev = &penv->pdev->dev;

	down_read(&wcnss_pm_sem);

	ret = request_firmware(&nv, NVBIN_FILE, dev);

	if (ret || !nv || !nv->data || !nv->size) {
		wcnss_log(ERR,
			  "%s: request_firmware failed for %s (ret = %d)\n",
		       __func__, NVBIN_FILE, ret);
		goto out;
	}

	/* First 4 bytes in nv blob is validity bitmap.
	 * We cannot validate nv, so skip those 4 bytes.
	 */
	nv_blob_addr = nv->data + 4;
	nv_blob_size = nv->size - 4;

	total_fragments = TOTALFRAGMENTS(nv_blob_size);

	wcnss_log(INFO, "NV bin size: %d, total_fragments: %d\n",
		nv_blob_size, total_fragments);

	/* get buffer for nv bin dnld req message */
	outbuffer = kmalloc((sizeof(struct nvbin_dnld_req_msg) +
			     NV_FRAGMENT_SIZE), GFP_KERNEL);
	if (!outbuffer)
		goto err_free_nv;

	dnld_req_msg = (struct nvbin_dnld_req_msg *)outbuffer;

	dnld_req_msg->hdr.msg_type = WCNSS_NVBIN_DNLD_REQ;
	dnld_req_msg->dnld_req_params.msg_flags = 0;

	for (count = 0; count < total_fragments; count++) {
		dnld_req_msg->dnld_req_params.frag_number = count;

		if (count == (total_fragments - 1)) {
			/* last fragment, take care of boundary condition */
			cur_frag_size = nv_blob_size % NV_FRAGMENT_SIZE;
			if (!cur_frag_size)
				cur_frag_size = NV_FRAGMENT_SIZE;

			dnld_req_msg->dnld_req_params.msg_flags |=
				LAST_FRAGMENT;
			dnld_req_msg->dnld_req_params.msg_flags |=
				CAN_RECEIVE_CALDATA;
		} else {
			cur_frag_size = NV_FRAGMENT_SIZE;
			dnld_req_msg->dnld_req_params.msg_flags &=
				~LAST_FRAGMENT;
		}

		dnld_req_msg->dnld_req_params.nvbin_buffer_size =
			cur_frag_size;

		dnld_req_msg->hdr.msg_len =
			sizeof(struct nvbin_dnld_req_msg) + cur_frag_size;

		/* copy NV fragment */
		memcpy((outbuffer + sizeof(struct nvbin_dnld_req_msg)),
		       (nv_blob_addr + count * NV_FRAGMENT_SIZE),
		       cur_frag_size);

		ret = wcnss_smd_tx(outbuffer, dnld_req_msg->hdr.msg_len);

		retry_count = 0;
		while ((ret == -ENOSPC) && (retry_count <= 3)) {
			wcnss_log(DBG, "%s: smd tx failed, ENOSPC\n",
				 __func__);
			wcnss_log(DBG, "fragment %d, len: %d,", count,
				  dnld_req_msg->hdr.msg_len);
			wcnss_log(DBG, "TotFragments: %d, retry_count: %d\n",
				  total_fragments, retry_count);

			/* wait and try again */
			msleep(20);
			retry_count++;
			ret = wcnss_smd_tx(outbuffer,
					   dnld_req_msg->hdr.msg_len);
		}

		if (ret < 0) {
			wcnss_log(ERR, "%s: smd tx failed\n", __func__);
			wcnss_log(ERR, "fragment %d, len: %d,", count,
				  dnld_req_msg->hdr.msg_len);
			wcnss_log(ERR, "TotFragments: %d, retry_count: %d\n",
				  total_fragments, retry_count);
			goto err_dnld;
		}
	}

err_dnld:
	/* free buffer */
	kfree(outbuffer);

err_free_nv:
	/* release firmware */
	release_firmware(nv);

out:
	up_read(&wcnss_pm_sem);
}

static void wcnss_caldata_dnld(const void *cal_data,
			       unsigned int cal_data_size, bool msg_to_follow)
{
	int ret = 0;
	struct cal_data_msg *cal_msg;
	unsigned short total_fragments = 0;
	unsigned short count = 0;
	unsigned short retry_count = 0;
	unsigned short cur_frag_size = 0;
	unsigned char *outbuffer = NULL;

	total_fragments = TOTALFRAGMENTS(cal_data_size);

	outbuffer = kmalloc((sizeof(struct cal_data_msg) +
			     NV_FRAGMENT_SIZE), GFP_KERNEL);
	if (!outbuffer)
		return;

	cal_msg = (struct cal_data_msg *)outbuffer;

	cal_msg->hdr.msg_type = WCNSS_CALDATA_DNLD_REQ;
	cal_msg->cal_params.msg_flags = 0;

	for (count = 0; count < total_fragments; count++) {
		cal_msg->cal_params.frag_number = count;

		if (count == (total_fragments - 1)) {
			cur_frag_size = cal_data_size % NV_FRAGMENT_SIZE;
			if (!cur_frag_size)
				cur_frag_size = NV_FRAGMENT_SIZE;

			cal_msg->cal_params.msg_flags
			    |= LAST_FRAGMENT;
			if (msg_to_follow)
				cal_msg->cal_params.msg_flags |=
					MESSAGE_TO_FOLLOW;
		} else {
			cur_frag_size = NV_FRAGMENT_SIZE;
			cal_msg->cal_params.msg_flags &=
				~LAST_FRAGMENT;
		}

		cal_msg->cal_params.total_size = cal_data_size;
		cal_msg->cal_params.frag_size =
			cur_frag_size;

		cal_msg->hdr.msg_len =
			sizeof(struct cal_data_msg) + cur_frag_size;

		memcpy((outbuffer + sizeof(struct cal_data_msg)),
		       (cal_data + count * NV_FRAGMENT_SIZE),
		       cur_frag_size);

		ret = wcnss_smd_tx(outbuffer, cal_msg->hdr.msg_len);

		retry_count = 0;
		while ((ret == -ENOSPC) && (retry_count <= 3)) {
			wcnss_log(DBG, "%s: smd tx failed, ENOSPC\n",
				 __func__);
			wcnss_log(DBG, "fragment: %d, len: %d",
				  count, cal_msg->hdr.msg_len);
			wcnss_log(DBG, " TotFragments: %d, retry_count: %d\n",
				  total_fragments, retry_count);

			/* wait and try again */
			msleep(20);
			retry_count++;
			ret = wcnss_smd_tx(outbuffer,
					   cal_msg->hdr.msg_len);
		}

		if (ret < 0) {
			wcnss_log(ERR, "%s: smd tx failed: fragment %d, len:%d",
				  count, cal_msg->hdr.msg_len, __func__);
			wcnss_log(ERR, " TotFragments: %d, retry_count: %d\n",
				  total_fragments, retry_count);
			goto err_dnld;
		}
	}

err_dnld:
	/* free buffer */
	kfree(outbuffer);
}

static void wcnss_nvbin_dnld_main(struct work_struct *worker)
{
	int retry = 0;

	if (!FW_CALDATA_CAPABLE())
		goto nv_download;

	if (!penv->fw_cal_available && IS_CAL_DATA_PRESENT
		!= has_calibrated_data && !penv->user_cal_available) {
		while (!penv->user_cal_available && retry++ < 5)
			msleep(500);
	}
	if (penv->fw_cal_available) {
		wcnss_log(INFO, "cal download, using fw cal");
		wcnss_caldata_dnld(penv->fw_cal_data, penv->fw_cal_rcvd, true);

	} else if (penv->user_cal_available) {
		wcnss_log(INFO, "cal download, using user cal");
		wcnss_caldata_dnld(penv->user_cal_data,
				   penv->user_cal_rcvd, true);
	}

nv_download:
	wcnss_log(INFO, "NV download");
	wcnss_nvbin_dnld();
}

static int wcnss_pm_notify(struct notifier_block *b,
			   unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&wcnss_pm_sem);
		break;

	case PM_POST_SUSPEND:
		up_write(&wcnss_pm_sem);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block wcnss_pm_notifier = {
	.notifier_call = wcnss_pm_notify,
};

static int wcnss_ctrl_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	if (!penv || penv->ctrl_device_opened)
		return -EFAULT;

	penv->ctrl_device_opened = 1;

	return rc;
}

void process_usr_ctrl_cmd(u8 *buf, size_t len)
{
	u16 cmd = buf[0] << 8 | buf[1];

	switch (cmd) {
	case WCNSS_USR_HAS_CAL_DATA:
		if (buf[2] > 1)
			wcnss_log(ERR, "%s: Invalid data for cal %d\n",
				  __func__, buf[2]);
		has_calibrated_data = buf[2];
		break;
	case WCNSS_USR_WLAN_MAC_ADDR:
		memcpy(&penv->wlan_nv_mac_addr,  &buf[2],
		       sizeof(penv->wlan_nv_mac_addr));
		wcnss_log(DBG, "%s: MAC Addr:" MAC_ADDRESS_STR "\n", __func__,
			 penv->wlan_nv_mac_addr[0], penv->wlan_nv_mac_addr[1],
			 penv->wlan_nv_mac_addr[2], penv->wlan_nv_mac_addr[3],
			 penv->wlan_nv_mac_addr[4], penv->wlan_nv_mac_addr[5]);
		break;
	default:
		wcnss_log(ERR, "%s: Invalid command %d\n", __func__, cmd);
		break;
	}
}

static ssize_t wcnss_ctrl_write(struct file *fp, const char __user
			*user_buffer, size_t count, loff_t *position)
{
	int rc = 0;
	u8 buf[WCNSS_MAX_CMD_LEN];

	if (!penv || !penv->ctrl_device_opened || WCNSS_MAX_CMD_LEN < count ||
	    count < WCNSS_MIN_CMD_LEN)
		return -EFAULT;

	mutex_lock(&penv->ctrl_lock);
	rc = copy_from_user(buf, user_buffer, count);
	if (rc == 0)
		process_usr_ctrl_cmd(buf, count);

	mutex_unlock(&penv->ctrl_lock);

	return rc;
}

static const struct file_operations wcnss_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = wcnss_ctrl_open,
	.write = wcnss_ctrl_write,
};

static int
wcnss_trigger_config(struct platform_device *pdev)
{
	int ret = 0;
	int rc;
	struct qcom_wcnss_opts *pdata;
	struct resource *res;
	int is_pronto_vadc;
	int is_pronto_v3;
	int pil_retry = 0;
	struct device_node *node = (&pdev->dev)->of_node;
	int has_pronto_hw = of_property_read_bool(node, "qcom,has-pronto-hw");

	is_pronto_vadc = of_property_read_bool(node, "qcom,is-pronto-vadc");
	is_pronto_v3 = of_property_read_bool(node, "qcom,is-pronto-v3");

	penv->is_vsys_adc_channel =
		of_property_read_bool(node, "qcom,has-vsys-adc-channel");
	penv->is_a2xb_split_reg =
		of_property_read_bool(node, "qcom,has-a2xb-split-reg");

	if (of_property_read_u32(node, "qcom,wlan-rx-buff-count",
				 &penv->wlan_rx_buff_count)) {
		penv->wlan_rx_buff_count = WCNSS_DEF_WLAN_RX_BUFF_COUNT;
	}

	rc = wcnss_parse_voltage_regulator(&penv->wlan_config, &pdev->dev);
	if (rc) {
		wcnss_log(ERR, "Failed to parse voltage regulators\n");
		goto fail;
	}

	/* make sure we are only triggered once */
	if (penv->triggered)
		return 0;
	penv->triggered = 1;

	/* initialize the WCNSS device configuration */
	pdata = pdev->dev.platform_data;
	if (has_48mhz_xo == WCNSS_CONFIG_UNSPECIFIED) {
		if (has_pronto_hw) {
			has_48mhz_xo =
			of_property_read_bool(node, "qcom,has-48mhz-xo");
		} else {
			has_48mhz_xo = pdata->has_48mhz_xo;
		}
	}
	penv->wcnss_hw_type = (has_pronto_hw) ? WCNSS_PRONTO_HW : WCNSS_RIVA_HW;
	penv->wlan_config.use_48mhz_xo = has_48mhz_xo;
	penv->wlan_config.is_pronto_vadc = is_pronto_vadc;
	penv->wlan_config.is_pronto_v3 = is_pronto_v3;

	if (has_autodetect_xo == WCNSS_CONFIG_UNSPECIFIED && has_pronto_hw) {
		has_autodetect_xo =
			of_property_read_bool(node, "qcom,has-autodetect-xo");
	}

	penv->thermal_mitigation = 0;
	strlcpy(penv->wcnss_version, "INVALID", WCNSS_VERSION_LEN);

	/* Configure 5 wire GPIOs */
	if (!has_pronto_hw) {
		penv->gpios_5wire = platform_get_resource_byname(pdev,
					IORESOURCE_IO, "wcnss_gpios_5wire");

		/* allocate 5-wire GPIO resources */
		if (!penv->gpios_5wire) {
			wcnss_log(ERR, "insufficient IO resources\n");
			ret = -ENOENT;
			goto fail_gpio_res;
		}
		ret = wcnss_gpios_config(penv->gpios_5wire, true);
	} else {
		ret = wcnss_pronto_gpios_config(pdev, true);
	}

	if (ret) {
		wcnss_log(ERR, "gpios config failed.\n");
		goto fail_gpio_res;
	}

	/* allocate resources */
	penv->mmio_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"wcnss_mmio");
	penv->tx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlantx_irq");
	penv->rx_irq_res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"wcnss_wlanrx_irq");

	if (!(penv->mmio_res && penv->tx_irq_res && penv->rx_irq_res)) {
		wcnss_log(ERR, "insufficient resources\n");
		ret = -ENOENT;
		goto fail_res;
	}
	INIT_WORK(&penv->wcnssctrl_rx_work, wcnssctrl_rx_handler);
	INIT_WORK(&penv->wcnssctrl_version_work, wcnss_send_version_req);
	INIT_WORK(&penv->wcnss_pm_config_work, wcnss_send_pm_config);
	INIT_WORK(&penv->wcnssctrl_nvbin_dnld_work, wcnss_nvbin_dnld_main);
	INIT_DELAYED_WORK(&penv->wcnss_pm_qos_del_req, wcnss_pm_qos_enable_pc);

	wakeup_source_init(&penv->wcnss_wake_lock, "wcnss");
	/* Add pm_qos request to disable power collapse for DDR */
	wcnss_disable_pc_add_req();

	if (wcnss_hardware_type() == WCNSS_PRONTO_HW) {
		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "pronto_phy_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource pronto_phy_base failed\n",
			       __func__);
			goto fail_ioremap;
		}
		penv->msm_wcnss_base =
			devm_ioremap_resource(&pdev->dev, res);
	} else {
		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "riva_phy_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource riva_phy_base failed\n",
			       __func__);
			goto fail_ioremap;
		}
		penv->msm_wcnss_base =
			devm_ioremap_resource(&pdev->dev, res);
	}

	if (!penv->msm_wcnss_base) {
		ret = -ENOMEM;
		wcnss_log(ERR, "%s: ioremap wcnss physical failed\n", __func__);
		goto fail_ioremap;
	}

	penv->wlan_config.msm_wcnss_base = penv->msm_wcnss_base;
	if (wcnss_hardware_type() == WCNSS_RIVA_HW) {
		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "riva_ccu_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource riva_ccu_base failed\n",
			       __func__);
			goto fail_ioremap;
		}
		penv->riva_ccu_base =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->riva_ccu_base) {
			ret = -ENOMEM;
			wcnss_log(ERR, "%s: ioremap riva ccu physical failed\n",
			       __func__);
			goto fail_ioremap;
		}
	} else {
		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "pronto_a2xb_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource pronto_a2xb_base failed\n",
			       __func__);
			goto fail_ioremap;
		}
		penv->pronto_a2xb_base =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->pronto_a2xb_base) {
			ret = -ENOMEM;
			wcnss_log(ERR,
			"%s: ioremap pronto a2xb physical failed\n", __func__);
			goto fail_ioremap;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "pronto_ccpu_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource pronto_ccpu_base failed\n",
			       __func__);
			goto fail_ioremap;
		}
		penv->pronto_ccpu_base =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->pronto_ccpu_base) {
			ret = -ENOMEM;
			wcnss_log(ERR,
			"%s: ioremap pronto ccpu physical failed\n", __func__);
			goto fail_ioremap;
		}

		/* for reset FIQ */
		res = platform_get_resource_byname(penv->pdev,
						   IORESOURCE_MEM, "wcnss_fiq");
		if (!res) {
			wcnss_log(ERR, "insufficient irq mem resources\n");
			ret = -ENOENT;
			goto fail_ioremap;
		}
		penv->fiq_reg = ioremap_nocache(res->start, resource_size(res));
		if (!penv->fiq_reg) {
			wcnss_log(ERR, "%s", __func__,
			"ioremap_nocache() failed fiq_reg addr:%pr\n",
			&res->start);
			ret = -ENOMEM;
			goto fail_ioremap;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "pronto_saw2_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource pronto_saw2_base failed\n",
			       __func__);
			goto fail_ioremap2;
		}
		penv->pronto_saw2_base =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->pronto_saw2_base) {
			wcnss_log(ERR,
			"%s: ioremap wcnss physical(saw2) failed\n", __func__);
			ret = -ENOMEM;
			goto fail_ioremap2;
		}

		penv->pronto_pll_base =
			penv->msm_wcnss_base + PRONTO_PLL_MODE_OFFSET;
		if (!penv->pronto_pll_base) {
			wcnss_log(ERR,
			"%s: ioremap wcnss physical(pll) failed\n", __func__);
			ret = -ENOMEM;
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "wlan_tx_phy_aborts");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR,
			"%s: resource wlan_tx_phy_aborts failed\n", __func__);
			goto fail_ioremap2;
		}
		penv->wlan_tx_phy_aborts =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->wlan_tx_phy_aborts) {
			ret = -ENOMEM;
			wcnss_log(ERR, "%s: ioremap wlan TX PHY failed\n",
				  __func__);
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "wlan_brdg_err_source");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR,
			"%s: get wlan_brdg_err_source res failed\n", __func__);
			goto fail_ioremap2;
		}
		penv->wlan_brdg_err_source =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->wlan_brdg_err_source) {
			ret = -ENOMEM;
			wcnss_log(ERR, "%s: ioremap wlan BRDG ERR failed\n",
				  __func__);
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "wlan_tx_status");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR, "%s: resource wlan_tx_status failed\n",
			       __func__);
			goto fail_ioremap2;
		}
		penv->wlan_tx_status =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->wlan_tx_status) {
			ret = -ENOMEM;
			wcnss_log(ERR, "%s: ioremap wlan TX STATUS failed\n",
				   __func__);
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "alarms_txctl");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR,
			"%s: resource alarms_txctl failed\n", __func__);
			goto fail_ioremap2;
		}
		penv->alarms_txctl =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->alarms_txctl) {
			ret = -ENOMEM;
			wcnss_log(ERR,
			"%s: ioremap alarms TXCTL failed\n", __func__);
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "alarms_tactl");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR,
			"%s: resource alarms_tactl failed\n", __func__);
			goto fail_ioremap2;
		}
		penv->alarms_tactl =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->alarms_tactl) {
			ret = -ENOMEM;
			wcnss_log(ERR,
			"%s: ioremap alarms TACTL failed\n", __func__);
			goto fail_ioremap2;
		}

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "pronto_mcu_base");
		if (!res) {
			ret = -EIO;
			wcnss_log(ERR,
			"%s: resource pronto_mcu_base failed\n", __func__);
			goto fail_ioremap2;
		}
		penv->pronto_mcu_base =
			devm_ioremap_resource(&pdev->dev, res);

		if (!penv->pronto_mcu_base) {
			ret = -ENOMEM;
			wcnss_log(ERR,
			"%s: ioremap pronto mcu physical failed\n", __func__);
			goto fail_ioremap2;
		}

		if (of_property_read_bool(node,
					  "qcom,is-dual-band-disabled")) {
			ret = wcnss_get_dual_band_capability_info(pdev);
			if (ret) {
				wcnss_log(ERR,
				"%s: failed to get dual band info\n", __func__);
				goto fail_ioremap2;
			}
		}
	}

	penv->adc_tm_dev = qpnp_get_adc_tm(&penv->pdev->dev, "wcnss");
	if (IS_ERR(penv->adc_tm_dev)) {
		wcnss_log(ERR, "%s: adc get failed\n", __func__);
		penv->adc_tm_dev = NULL;
	} else {
		INIT_DELAYED_WORK(&penv->vbatt_work, wcnss_update_vbatt);
		penv->fw_vbatt_state = WCNSS_CONFIG_UNSPECIFIED;
	}

	penv->snoc_wcnss = devm_clk_get(&penv->pdev->dev, "snoc_wcnss");
	if (IS_ERR(penv->snoc_wcnss)) {
		wcnss_log(ERR, "%s: couldn't get snoc_wcnss\n", __func__);
		penv->snoc_wcnss = NULL;
	} else {
		if (of_property_read_u32(pdev->dev.of_node,
					 "qcom,snoc-wcnss-clock-freq",
					 &penv->snoc_wcnss_clock_freq)) {
			wcnss_log(DBG,
			"%s: snoc clock frequency is not defined\n", __func__);
			devm_clk_put(&penv->pdev->dev, penv->snoc_wcnss);
			penv->snoc_wcnss = NULL;
		}
	}

	if (penv->wlan_config.is_pronto_vadc) {
		penv->vadc_dev = qpnp_get_vadc(&penv->pdev->dev, "wcnss");

		if (IS_ERR(penv->vadc_dev)) {
			wcnss_log(DBG, "%s: vadc get failed\n", __func__);
			penv->vadc_dev = NULL;
		} else {
			rc = wcnss_get_battery_volt(&penv->wlan_config.vbatt);
			INIT_WORK(&penv->wcnss_vadc_work,
				  wcnss_send_vbatt_indication);

			if (rc < 0)
				wcnss_log(ERR,
				"battery voltage get failed:err=%d\n", rc);
		}
	}

	do {
		/* trigger initialization of the WCNSS */
		penv->pil = subsystem_get(WCNSS_PIL_DEVICE);
		if (IS_ERR(penv->pil)) {
			wcnss_log(ERR, "Peripheral Loader failed on WCNSS.\n");
			ret = PTR_ERR(penv->pil);
			wcnss_disable_pc_add_req();
			wcnss_pronto_log_debug_regs();
		}
	} while (pil_retry++ < WCNSS_MAX_PIL_RETRY && IS_ERR(penv->pil));

	if (IS_ERR(penv->pil)) {
		wcnss_reset_fiq(false);
		if (penv->wcnss_notif_hdle)
			subsys_notif_unregister_notifier(penv->wcnss_notif_hdle,
							 &wnb);
		penv->pil = NULL;
		goto fail_ioremap2;
	}
	/* Remove pm_qos request */
	wcnss_disable_pc_remove_req();

	return 0;

fail_ioremap2:
	if (penv->fiq_reg)
		iounmap(penv->fiq_reg);
fail_ioremap:
	wakeup_source_trash(&penv->wcnss_wake_lock);
fail_res:
	if (!has_pronto_hw)
		wcnss_gpios_config(penv->gpios_5wire, false);
	else if (penv->use_pinctrl)
		wcnss_pinctrl_set_state(false);
	else
		wcnss_pronto_gpios_config(pdev, false);
fail_gpio_res:
	wcnss_disable_pc_remove_req();
fail:
	if (penv->wcnss_notif_hdle)
		subsys_notif_unregister_notifier(penv->wcnss_notif_hdle, &wnb);
	penv = NULL;
	return ret;
}

/* Driver requires to directly vote the snoc clocks
 * To enable and disable snoc clock, it call
 * wcnss_snoc_vote function
 */
void wcnss_snoc_vote(bool clk_chk_en)
{
	int rc;

	if (!penv->snoc_wcnss) {
		wcnss_log(ERR, "%s: couldn't get clk snoc_wcnss\n", __func__);
		return;
	}

	if (clk_chk_en) {
		rc = clk_set_rate(penv->snoc_wcnss,
				  penv->snoc_wcnss_clock_freq);
		if (rc) {
			wcnss_log(ERR,
				  "%s: snoc_wcnss_clk-clk_set_rate failed=%d\n",
				__func__, rc);
			return;
		}

		if (clk_prepare_enable(penv->snoc_wcnss)) {
			wcnss_log(ERR, "%s: snoc_wcnss clk enable failed\n",
				  __func__);
			return;
		}
	} else {
		clk_disable_unprepare(penv->snoc_wcnss);
	}
}
EXPORT_SYMBOL(wcnss_snoc_vote);

/* wlan prop driver cannot invoke cancel_work_sync
 * function directly, so to invoke this function it
 * call wcnss_flush_work function
 */
void wcnss_flush_work(struct work_struct *work)
{
	struct work_struct *cnss_work = work;

	if (cnss_work)
		cancel_work_sync(cnss_work);
}
EXPORT_SYMBOL(wcnss_flush_work);

/* wlan prop driver cannot invoke show_stack
 * function directly, so to invoke this function it
 * call wcnss_dump_stack function
 */
void wcnss_dump_stack(struct task_struct *task)
{
	show_stack(task, NULL);
}
EXPORT_SYMBOL(wcnss_dump_stack);

/* wlan prop driver cannot invoke cancel_delayed_work_sync
 * function directly, so to invoke this function it call
 * wcnss_flush_delayed_work function
 */
void wcnss_flush_delayed_work(struct delayed_work *dwork)
{
	struct delayed_work *cnss_dwork = dwork;

	if (cnss_dwork)
		cancel_delayed_work_sync(cnss_dwork);
}
EXPORT_SYMBOL(wcnss_flush_delayed_work);

/* wlan prop driver cannot invoke INIT_WORK function
 * directly, so to invoke this function call
 * wcnss_init_work function.
 */
void wcnss_init_work(struct work_struct *work, void *callbackptr)
{
	if (work && callbackptr)
		INIT_WORK(work, callbackptr);
}
EXPORT_SYMBOL(wcnss_init_work);

/* wlan prop driver cannot invoke INIT_DELAYED_WORK
 * function directly, so to invoke this function
 * call wcnss_init_delayed_work function.
 */
void wcnss_init_delayed_work(struct delayed_work *dwork, void *callbackptr)
{
	if (dwork && callbackptr)
		INIT_DELAYED_WORK(dwork, callbackptr);
}
EXPORT_SYMBOL(wcnss_init_delayed_work);

static int wcnss_node_open(struct inode *inode, struct file *file)
{
	struct platform_device *pdev;
	int rc = 0;

	if (!penv)
		return -EFAULT;

	if (!penv->triggered) {
		wcnss_log(INFO, DEVICE " triggered by userspace\n");
		pdev = penv->pdev;
		rc = wcnss_trigger_config(pdev);
		if (rc)
			return -EFAULT;
	}

	return rc;
}

static ssize_t wcnss_wlan_read(struct file *fp, char __user
			*buffer, size_t count, loff_t *position)
{
	int rc = 0;

	if (!penv)
		return -EFAULT;

	rc = wait_event_interruptible(penv->read_wait, penv->fw_cal_rcvd
			> penv->user_cal_read || penv->fw_cal_available);

	if (rc < 0)
		return rc;

	mutex_lock(&penv->dev_lock);

	if (penv->fw_cal_available && penv->fw_cal_rcvd
			== penv->user_cal_read) {
		rc = 0;
		goto exit;
	}

	if (count > penv->fw_cal_rcvd - penv->user_cal_read)
		count = penv->fw_cal_rcvd - penv->user_cal_read;

	rc = copy_to_user(buffer, penv->fw_cal_data +
			penv->user_cal_read, count);
	if (rc == 0) {
		penv->user_cal_read += count;
		rc = count;
	}

exit:
	mutex_unlock(&penv->dev_lock);
	return rc;
}

/* first (valid) write to this device should be 4 bytes cal file size */
static ssize_t wcnss_wlan_write(struct file *fp, const char __user
			*user_buffer, size_t count, loff_t *position)
{
	int rc = 0;
	char *cal_data = NULL;

	if (!penv || penv->user_cal_available)
		return -EFAULT;

	if (!penv->user_cal_rcvd && count >= 4 && !penv->user_cal_exp_size) {
		mutex_lock(&penv->dev_lock);
		rc = copy_from_user((void *)&penv->user_cal_exp_size,
				    user_buffer, 4);
		if (!penv->user_cal_exp_size ||
		    penv->user_cal_exp_size > MAX_CALIBRATED_DATA_SIZE) {
			wcnss_log(ERR, DEVICE " invalid size to write %d\n",
				  penv->user_cal_exp_size);
			penv->user_cal_exp_size = 0;
			mutex_unlock(&penv->dev_lock);
			return -EFAULT;
		}
		mutex_unlock(&penv->dev_lock);
		return count;
	} else if (!penv->user_cal_rcvd && count < 4) {
		return -EFAULT;
	}

	mutex_lock(&penv->dev_lock);
	if ((UINT32_MAX - count < penv->user_cal_rcvd) ||
	    (penv->user_cal_exp_size < count + penv->user_cal_rcvd)) {
		wcnss_log(ERR, DEVICE " invalid size to write %zu\n",
			  count + penv->user_cal_rcvd);
		mutex_unlock(&penv->dev_lock);
		return -ENOMEM;
	}

	cal_data = kmalloc(count, GFP_KERNEL);
	if (!cal_data) {
		mutex_unlock(&penv->dev_lock);
		return -ENOMEM;
	}

	rc = copy_from_user(cal_data, user_buffer, count);
	if (!rc) {
		memcpy(penv->user_cal_data + penv->user_cal_rcvd,
		       cal_data, count);
		penv->user_cal_rcvd += count;
		rc += count;
	}

	kfree(cal_data);
	if (penv->user_cal_rcvd == penv->user_cal_exp_size) {
		penv->user_cal_available = true;
		wcnss_log(INFO, "user cal written");
	}
	mutex_unlock(&penv->dev_lock);

	return rc;
}

static int wcnss_node_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int wcnss_notif_cb(struct notifier_block *this, unsigned long code,
			  void *ss_handle)
{
	struct platform_device *pdev = wcnss_get_platform_device();
	struct wcnss_wlan_config *pwlanconfig = wcnss_get_wlan_config();
	struct notif_data *data = (struct notif_data *)ss_handle;
	int ret, xo_mode;

	if (!(code >= SUBSYS_NOTIF_MIN_INDEX) &&
	    (code <= SUBSYS_NOTIF_MAX_INDEX)) {
		wcnss_log(DBG, "%s: Invaild subsystem notification code: %lu\n",
			  __func__, code);
		return NOTIFY_DONE;
	}

	wcnss_log(INFO, "%s: notification event: %lu : %s\n",
		 __func__, code, wcnss_subsys_notif_type[code]);

	if (code == SUBSYS_PROXY_VOTE) {
		if (pdev && pwlanconfig) {
			ret = wcnss_wlan_power(&pdev->dev, pwlanconfig,
					       WCNSS_WLAN_SWITCH_ON, &xo_mode);
			wcnss_set_iris_xo_mode(xo_mode);
			if (ret)
				wcnss_log(ERR, "wcnss_wlan_power failed\n");
		}
	} else if (code == SUBSYS_PROXY_UNVOTE) {
		if (pdev && pwlanconfig) {
			/* Temporary workaround as some pronto images have an
			 * issue of sending an interrupt that it is capable of
			 * voting for it's resources too early.
			 */
			msleep(20);
			wcnss_wlan_power(&pdev->dev, pwlanconfig,
					 WCNSS_WLAN_SWITCH_OFF, NULL);
		}
	} else if ((code == SUBSYS_BEFORE_SHUTDOWN && data && data->crashed) ||
			code == SUBSYS_SOC_RESET) {
		wcnss_disable_pc_add_req();
		schedule_delayed_work(&penv->wcnss_pm_qos_del_req,
				      msecs_to_jiffies(WCNSS_PM_QOS_TIMEOUT));
		penv->is_shutdown = 1;
		wcnss_log_debug_regs_on_bite();
	} else if (code == SUBSYS_POWERUP_FAILURE) {
		if (pdev && pwlanconfig)
			wcnss_wlan_power(&pdev->dev, pwlanconfig,
					 WCNSS_WLAN_SWITCH_OFF, NULL);
		wcnss_pronto_log_debug_regs();
		wcnss_disable_pc_remove_req();
	} else if (code == SUBSYS_BEFORE_SHUTDOWN) {
		wcnss_disable_pc_add_req();
		schedule_delayed_work(&penv->wcnss_pm_qos_del_req,
				      msecs_to_jiffies(WCNSS_PM_QOS_TIMEOUT));
		penv->is_shutdown = 1;
	} else if (code == SUBSYS_AFTER_POWERUP) {
		penv->is_shutdown = 0;
	}

	return NOTIFY_DONE;
}

static const struct file_operations wcnss_node_fops = {
	.owner = THIS_MODULE,
	.open = wcnss_node_open,
	.read = wcnss_wlan_read,
	.write = wcnss_wlan_write,
	.release = wcnss_node_release,
};

static int wcnss_cdev_register(struct platform_device *pdev)
{
	int ret = 0;

	ret = alloc_chrdev_region(&penv->dev_ctrl, 0, 1, CTRL_DEVICE);
	if (ret < 0) {
		wcnss_log(ERR, "CTRL Device Registration failed\n");
		goto alloc_region_ctrl;
	}
	ret = alloc_chrdev_region(&penv->dev_node, 0, 1, DEVICE);
	if (ret < 0) {
		wcnss_log(ERR, "NODE Device Registration failed\n");
		goto alloc_region_node;
	}

	penv->node_class = class_create(THIS_MODULE, "wcnss");
	if (!penv->node_class) {
		wcnss_log(ERR, "NODE Device Class Creation failed\n");
		goto class_create_node;
	}

	if (device_create(penv->node_class, NULL, penv->dev_ctrl, NULL,
			 CTRL_DEVICE) == NULL) {
		wcnss_log(ERR, "CTRL Device Creation failed\n");
		goto device_create_ctrl;
	}

	if (device_create(penv->node_class, NULL, penv->dev_node, NULL,
			  DEVICE) == NULL) {
		wcnss_log(ERR, "NODE Device Creation failed\n");
		goto device_create_node;
	}

	cdev_init(&penv->ctrl_dev, &wcnss_ctrl_fops);
	cdev_init(&penv->node_dev, &wcnss_node_fops);

	if (cdev_add(&penv->ctrl_dev, penv->dev_ctrl, 1) == -1) {
		wcnss_log(ERR, "CTRL Device addition failed\n");
		goto cdev_add_ctrl;
	}
	if (cdev_add(&penv->node_dev, penv->dev_node, 1) == -1) {
		wcnss_log(ERR, "NODE Device addition failed\n");
		goto cdev_add_node;
	}

	return 0;

cdev_add_node:
	cdev_del(&penv->ctrl_dev);
cdev_add_ctrl:
	device_destroy(penv->node_class, penv->dev_node);
device_create_node:
	device_destroy(penv->node_class, penv->dev_ctrl);
device_create_ctrl:
	class_destroy(penv->node_class);
class_create_node:
	unregister_chrdev_region(penv->dev_node, 1);
alloc_region_node:
	unregister_chrdev_region(penv->dev_ctrl, 1);
alloc_region_ctrl:
	return -ENOMEM;
}

static void wcnss_cdev_unregister(struct platform_device *pdev)
{
	wcnss_log(ERR, "Unregistering cdev devices\n");
	cdev_del(&penv->ctrl_dev);
	cdev_del(&penv->node_dev);
	device_destroy(penv->node_class, penv->dev_ctrl);
	device_destroy(penv->node_class, penv->dev_node);
	class_destroy(penv->node_class);
	unregister_chrdev_region(penv->dev_ctrl, 1);
	unregister_chrdev_region(penv->dev_node, 1);
}

static int
wcnss_wlan_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* verify we haven't been called more than once */
	if (penv) {
		wcnss_log(ERR, "cannot handle multiple devices.\n");
		return -ENODEV;
	}

	/* create an environment to track the device */
	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	penv->pdev = pdev;

	penv->user_cal_data =
		devm_kzalloc(&pdev->dev, MAX_CALIBRATED_DATA_SIZE, GFP_KERNEL);
	if (!penv->user_cal_data) {
		wcnss_log(ERR, "Failed to alloc memory for cal data.\n");
		return -ENOMEM;
	}

	/* register sysfs entries */
	ret = wcnss_create_sysfs(&pdev->dev);
	if (ret) {
		penv = NULL;
		return -ENOENT;
	}

	/* register wcnss event notification */
	penv->wcnss_notif_hdle = subsys_notif_register_notifier("wcnss", &wnb);
	if (IS_ERR(penv->wcnss_notif_hdle)) {
		wcnss_log(ERR, "register event notification failed!\n");
		return PTR_ERR(penv->wcnss_notif_hdle);
	}

	mutex_init(&penv->dev_lock);
	mutex_init(&penv->ctrl_lock);
	mutex_init(&penv->vbat_monitor_mutex);
	mutex_init(&penv->pm_qos_mutex);
	init_waitqueue_head(&penv->read_wait);

	penv->user_cal_rcvd = 0;
	penv->user_cal_read = 0;
	penv->user_cal_exp_size = 0;
	penv->user_cal_available = false;

	/* Since we were built into the kernel we'll be called as part
	 * of kernel initialization.  We don't know if userspace
	 * applications are available to service PIL at this time
	 * (they probably are not), so we simply create a device node
	 * here.  When userspace is available it should touch the
	 * device so that we know that WCNSS configuration can take
	 * place
	 */
	wcnss_log(INFO, DEVICE " probed in built-in mode\n");

	return wcnss_cdev_register(pdev);
}

static int
wcnss_wlan_remove(struct platform_device *pdev)
{
	if (penv->wcnss_notif_hdle)
		subsys_notif_unregister_notifier(penv->wcnss_notif_hdle, &wnb);
	wcnss_cdev_unregister(pdev);
	wcnss_remove_sysfs(&pdev->dev);
	penv = NULL;
	return 0;
}

static const struct dev_pm_ops wcnss_wlan_pm_ops = {
	.suspend	= wcnss_wlan_suspend,
	.resume		= wcnss_wlan_resume,
	.suspend_noirq  = wcnss_wlan_suspend_noirq,
	.resume_noirq   = wcnss_wlan_resume_noirq,
};

#ifdef CONFIG_WCNSS_CORE_PRONTO
static const struct of_device_id msm_wcnss_pronto_match[] = {
	{.compatible = "qcom,wcnss_wlan"},
	{}
};
#endif

static struct platform_driver wcnss_wlan_driver = {
	.driver = {
		.name	= DEVICE,
		.owner	= THIS_MODULE,
		.pm	= &wcnss_wlan_pm_ops,
#ifdef CONFIG_WCNSS_CORE_PRONTO
		.of_match_table = msm_wcnss_pronto_match,
#endif
	},
	.probe	= wcnss_wlan_probe,
	.remove	= wcnss_wlan_remove,
};

static int __init wcnss_wlan_init(void)
{

	wcnss_ipc_log = ipc_log_context_create(IPC_NUM_LOG_PAGES, "wcnss", 0);
	if (!wcnss_ipc_log)
		wcnss_log(ERR, "Unable to create log context\n");

	platform_driver_register(&wcnss_wlan_driver);
	platform_driver_register(&wcnss_wlan_ctrl_driver);
	platform_driver_register(&wcnss_ctrl_driver);
	register_pm_notifier(&wcnss_pm_notifier);

	return 0;
}

static void __exit wcnss_wlan_exit(void)
{
	if (penv) {
		if (penv->pil)
			subsystem_put(penv->pil);
		penv = NULL;
	}

	unregister_pm_notifier(&wcnss_pm_notifier);
	platform_driver_unregister(&wcnss_ctrl_driver);
	platform_driver_unregister(&wcnss_wlan_ctrl_driver);
	platform_driver_unregister(&wcnss_wlan_driver);
	ipc_log_context_destroy(wcnss_ipc_log);
	wcnss_ipc_log = NULL;
}

module_init(wcnss_wlan_init);
module_exit(wcnss_wlan_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "Driver");
