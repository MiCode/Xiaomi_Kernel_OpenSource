
#ifndef XM_PD_ADAPTER_H
#define XM_PD_ADAPTER_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

#include <linux/usb/tcpc/tcpm.h>
#include <linux/usb/tcpc/tcpci_config.h>
#include <linux/battmngr/battmngr_notifier.h>

#define ADAPTER_CAP_MAX_NR 10

extern struct rt1711_chip *g_tcpc_rt1711h;

struct adapter_power_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[ADAPTER_CAP_MAX_NR];
	int max_mv[ADAPTER_CAP_MAX_NR];
	int min_mv[ADAPTER_CAP_MAX_NR];
	int ma[ADAPTER_CAP_MAX_NR];
	int maxwatt[ADAPTER_CAP_MAX_NR];
	int minwatt[ADAPTER_CAP_MAX_NR];
	uint8_t type[ADAPTER_CAP_MAX_NR];
	int info[ADAPTER_CAP_MAX_NR];
};

struct adapter_properties {
	const char *alias_name;
};

enum uvdm_state {
	USBPD_UVDM_DISCONNECT,
	USBPD_UVDM_CHARGER_VERSION,
	USBPD_UVDM_CHARGER_VOLTAGE,
	USBPD_UVDM_CHARGER_TEMP,
	USBPD_UVDM_SESSION_SEED,
	USBPD_UVDM_AUTHENTICATION,
	USBPD_UVDM_VERIFIED,
	USBPD_UVDM_REMOVE_COMPENSATION,
	USBPD_UVDM_REVERSE_AUTHEN,
	USBPD_UVDM_CONNECT,
	USBPD_UVDM_NAN_ACK,
};

enum adapter_cap_type {
	XM_PD,
	XM_PD_APDO,
	XM_PD_APDO_REGAIN,
	XM_CAP_TYPE_UNKNOWN,
};

#define USB_PD_MI_SVID			0x2717
#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1
#define USBPD_VDM_REQUEST		0x1

#define VDM_HDR(svid, cmd0, cmd1) \
	   (((svid) << 16) | (0 << 15) | ((cmd0) << 8) \
	   | (cmd1))
#define UVDM_HDR_CMD(hdr)	((hdr) & 0xFF)

struct usbpd_vdm_data {
	int ta_version;
	int ta_temp;
	int ta_voltage;
	bool reauth;
	unsigned long s_secert[USBPD_UVDM_SS_LEN];
	unsigned long digest[USBPD_UVDM_SS_LEN];
};

struct adapter_device {
	struct adapter_properties props;
	const struct adapter_ops *ops;
	struct mutex ops_lock;
	struct device dev;
	struct srcu_notifier_head evt_nh;
	void	*driver_data;
	uint32_t adapter_svid;
	uint32_t adapter_id;
	uint32_t adapter_fw_ver;
	uint32_t adapter_hw_ver;
	struct	 usbpd_vdm_data   vdm_data;
	int  uvdm_state;
	bool verify_process;
	bool verifed;
	uint8_t role;
	uint8_t current_state;
	uint32_t received_pdos[7];
};

struct adapter_ops {
	int (*suspend)(struct adapter_device *dev, pm_message_t state);
	int (*resume)(struct adapter_device *dev);
	int (*get_cap)(struct adapter_device *dev, enum adapter_cap_type type,
		struct adapter_power_cap *cap);
	int (*get_svid)(struct adapter_device *dev);
	int (*request_vdm_cmd)(struct adapter_device *dev, enum uvdm_state cmd,
		unsigned char *data, unsigned int data_len);
	int (*get_power_role)(struct adapter_device *dev);
	int (*get_current_state)(struct adapter_device *dev);
	int (*get_pdos)(struct adapter_device *dev);
	int (*set_pd_verify_process)(struct adapter_device *dev, int verify_in_process);
};

struct xm_pd_adapter_info {
	struct device *dev;
	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_device *adapter_dev;
	struct task_struct *adapter_task;
	const char *adapter_dev_name;
	bool enable_kpoc_shdn;
	struct tcpm_svid_list *adapter_svid_list;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;

	int pd_active;
	int pd_cur_max;
	int pd_vol_min;
	int pd_vol_max;
	int pd_in_hard_reset;
	int typec_cc_orientation;
	int typec_mode;
	int pd_usb_suspend_supported;
	int pd_apdo_volt_max;
	int pd_apdo_curr_max;
	int pd_usb_real_type;
	int typec_accessory_mode;
};

static inline void *adapter_dev_get_drvdata(
	const struct adapter_device *adapter_dev)
{
	return adapter_dev->driver_data;
}

static inline void adapter_dev_set_drvdata(
	struct adapter_device *adapter_dev, void *data)
{
	adapter_dev->driver_data = data;
}

int adapter_check_usb_psy(struct xm_pd_adapter_info *info);
int adapter_check_battery_psy(struct xm_pd_adapter_info *info);

#endif /*XM_PD_ADAPTER_H*/

