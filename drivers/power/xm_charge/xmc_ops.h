#include <linux/kernel.h>
#include <linux/device.h>

#define ADAPTER_CAP_MAX_NR 10

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

enum xmc_bc12_type {
	XMC_BC12_TYPE_NONE,
	XMC_BC12_TYPE_SDP,
	XMC_BC12_TYPE_DCP,
	XMC_BC12_TYPE_CDP,
	XMC_BC12_TYPE_OCP,
	XMC_BC12_TYPE_FLOAT,
};

enum xmc_qc_type {
	XMC_QC_TYPE_NONE,
	XMC_QC_TYPE_HVCHG,
	XMC_QC_TYPE_HVDCP,
	XMC_QC_TYPE_HVDCP_2,
	XMC_QC_TYPE_HVDCP_3,
	XMC_QC_TYPE_HVDCP_3_18W,
	XMC_QC_TYPE_HVDCP_3_27W,
	XMC_QC_TYPE_HVDCP_3P5,
};

enum xmc_pd_type {
	XMC_PD_TYPE_NONE,
	XMC_PD_TYPE_PD2,
	XMC_PD_TYPE_PD3,
	XMC_PD_TYPE_PPS,
};

enum xmc_pdo_type {
	XMC_PDO_APDO_START,
	XMC_PDO_APDO_END,
	XMC_PDO_PD,
	XMC_PDO_APDO,
	XMC_PDO_UNKNOWN,
};

enum xmc_cp_div_mode {
	XMC_CP_1T1,
	XMC_CP_2T1,
	XMC_CP_4T1,
};

struct xmc_pd_cap {
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

struct xmc_device {
	const char *name;
	const struct xmc_ops *ops;
	void *data;
	struct list_head list;
};

struct xmc_ops {
	/* For BUCK and BOOST and CP */
	int (*charge_enable)(struct xmc_device *dev, bool enable);
	int (*get_charge_enable)(struct xmc_device *dev, bool *enable);
	int (*powerpath_enable)(struct xmc_device *dev, bool enable);
	int (*get_powerpath_enable)(struct xmc_device *dev, bool *enable);
	int (*set_div_mode)(struct xmc_device *dev, enum xmc_cp_div_mode mode);
	int (*get_div_mode)(struct xmc_device *dev, enum xmc_cp_div_mode *mode);
	int (*set_fcc)(struct xmc_device *dev, int value);
	int (*set_icl)(struct xmc_device *dev, int value);
	int (*set_fv)(struct xmc_device *dev, int value);
	int (*set_iterm)(struct xmc_device *dev, int value);
	int (*set_vinmin)(struct xmc_device *dev, int value);
	int (*get_vbus)(struct xmc_device *dev, int *value);
	int (*get_ibus)(struct xmc_device *dev, int *value);
	int (*get_ts)(struct xmc_device *dev, int *value);
	int (*adc_enable)(struct xmc_device *dev, bool enable);
	int (*device_init)(struct xmc_device *dev, enum xmc_cp_div_mode mode);

	/* For PD */
	int (*request_vdm_cmd)(struct xmc_device *dev, enum uvdm_state cmd, unsigned char *data, unsigned int data_len);
	int (*set_cap)(struct xmc_device *dev, enum xmc_pdo_type type, int mV, int mA);
	int (*get_cap)(struct xmc_device *dev, enum xmc_pdo_type type, struct xmc_pd_cap *cap);
	int (*get_pd_id)(struct xmc_device *dev);
};

/*  common ops */
struct xmc_device *xmc_device_register(const char *name, const struct xmc_ops *ops, void *data);
void xmc_device_unregister(struct xmc_device *xmc_dev);
struct xmc_device *xmc_ops_find_device(const char *name);
void *xmc_ops_get_data(const struct xmc_device *xmc_dev);

/* For BUCK and BOOST and CP */
int xmc_ops_charge_enable(struct xmc_device *xmc_dev, bool enable);
int xmc_ops_get_charge_enable(struct xmc_device *xmc_dev, bool *enable);
int xmc_ops_powerpath_enable(struct xmc_device *xmc_dev, bool enable);
int xmc_ops_get_powerpath_enable(struct xmc_device *xmc_dev, bool *enable);
int xmc_ops_set_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode);
int xmc_ops_get_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode *mode);
int xmc_ops_set_fcc(struct xmc_device *xmc_dev, int value);
int xmc_ops_set_icl(struct xmc_device *xmc_dev, int value);
int xmc_ops_set_fv(struct xmc_device *xmc_dev, int value);
int xmc_ops_set_iterm(struct xmc_device *xmc_dev, int value);
int xmc_ops_set_vinmin(struct xmc_device *xmc_dev, int value);
int xmc_ops_get_vbus(struct xmc_device *dev, int *value);
int xmc_ops_get_ibus(struct xmc_device *dev, int *value);
int xmc_ops_get_ts(struct xmc_device *dev, int *value);
int xmc_ops_adc_enable(struct xmc_device *xmc_dev, bool enable);
int xmc_ops_device_init(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode);

/* For PD */
int xmc_ops_request_vdm_cmd(struct xmc_device *xmc_dev, enum uvdm_state cmd, unsigned char *data, unsigned int data_len);
int xmc_ops_set_cap(struct xmc_device *xmc_dev, enum xmc_pdo_type type, int mV, int mA);
int xmc_ops_get_cap(struct xmc_device *xmc_dev, enum xmc_pdo_type type, struct xmc_pd_cap *cap);
int xmc_ops_get_pd_id(struct xmc_device *xmc_dev);