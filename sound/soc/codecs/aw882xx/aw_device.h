#ifndef __AWINIC_DEVICE_FILE_H__
#define __AWINIC_DEVICE_FILE_H__
#include "aw_data_type.h"
#include "aw_calib.h"
#include "aw_monitor.h"


#define AW_VOLUME_STEP_DB	(6 * 2)
#define AW_REG_NONE		(0xFF)
#define AW_NAME_MAX		(50)

enum {
	AW_1000_US = 1000,
	AW_2000_US = 2000,
	AW_3000_US = 3000,
	AW_4000_US = 4000,
	AW_5000_US = 5000,
	AW_10000_US = 10000,
};

struct aw_device;

enum {
	AW_DEV_TYPE_OK = 0,
	AW_DEV_TYPE_NONE = 1,
};

enum {
	AW_DEV_CH_PRI_L = 0,
	AW_DEV_CH_PRI_R = 1,
	AW_DEV_CH_SEC_L = 2,
	AW_DEV_CH_SEC_R = 3,
	AW_DEV_CH_MAX,
};

enum AW_DEV_INIT {
	AW_DEV_INIT_ST = 0,
	AW_DEV_INIT_OK = 1,
	AW_DEV_INIT_NG = 2,
};

enum AW_DEV_STATUS {
	AW_DEV_PW_OFF = 0,
	AW_DEV_PW_ON,
};

enum AW_DEV_FW_STATUS {
	AW_DEV_FW_FAILED = 0,
	AW_DEV_FW_OK,
};

struct aw_device_ops {
	int (*aw_i2c_write)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int reg_data);
	int (*aw_i2c_read)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int *reg_data);
	int (*aw_i2c_write_bits)(struct aw_device *aw_dev, unsigned char reg_addr, unsigned int mask, unsigned int reg_data);
	int (*aw_set_volume)(struct aw_device *aw_dev, unsigned int value);
	int (*aw_get_volume)(struct aw_device *aw_dev, unsigned int *value);
	unsigned int (*aw_reg_val_to_db)(unsigned int value);
	void (*aw_i2s_enable)(struct aw_device *aw_dev, bool flag);
	bool (*aw_check_wr_access)(int reg);
	bool (*aw_check_rd_access)(int reg);
	int (*aw_get_reg_num)(void);
	int (*aw_get_version)(char *buf, int size);
	int (*aw_get_dev_num)(void);
	void (*aw_set_algo)(struct aw_device *aw_dev);
	unsigned int (*aw_get_irq_type)(struct aw_device *aw_dev, unsigned int value);
};

struct aw_int_desc {
	unsigned int mask_reg;			/*interrupt mask reg*/
	unsigned int st_reg;			/*interrupt status reg*/
	unsigned int mask_default;		/*default mask close all*/
	unsigned int int_mask;			/*set mask*/
};

struct aw_frcpwm_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int frcpwm_val;
};

struct aw_soft_rst {
	int reg;
	int reg_value;
};

struct aw_pwd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_amppd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};


struct aw_vcalb_desc {
	unsigned int icalk_reg;
	unsigned int icalk_reg_mask;
	unsigned int icalk_shift;
	unsigned int icalkl_reg;
	unsigned int icalkl_reg_mask;
	unsigned int icalkl_shift;
	unsigned int icalk_sign_mask;
	unsigned int icalk_neg_mask;
	int icalk_value_factor;

	unsigned int vcalk_reg;
	unsigned int vcalk_reg_mask;
	unsigned int vcalk_shift;
	unsigned int vcalkl_reg;
	unsigned int vcalkl_reg_mask;
	unsigned int vcalkl_shift;
	unsigned int vcalk_sign_mask;
	unsigned int vcalk_neg_mask;
	int vcalk_value_factor;

	unsigned int vcalb_reg;
	int cabl_base_value;
	int vcal_factor;
};

struct aw_mute_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_sysst_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int st_check;
	unsigned int pll_check;
};

struct aw_profctrl_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int spk_mode;
	unsigned int cfg_prof_mode;
};

struct aw_bstctrl_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int frc_bst;
	unsigned int tsp_type;
	unsigned int cfg_bst_type;
};

struct aw_cco_mux_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int divided_val;
	unsigned int bypass_val;
};

struct aw_volume_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;
	int init_volume;
	int mute_volume;
};

struct aw_voltage_desc {
	unsigned int reg;
	unsigned int vbat_range;
	unsigned int int_bit;
};

struct aw_temperature_desc {
	unsigned int reg;
	unsigned int sign_mask;
	unsigned int neg_mask;
};

struct aw_ipeak_desc {
	unsigned int reg;
	unsigned int mask;
};

struct aw_container {
	int len;
	uint8_t data[];
};

struct aw_device {
	int index;
	int status;
	int bstcfg_enable;
	int frcpwm_en;
	unsigned int mute_st;
	unsigned int amppd_st;

	unsigned char cur_prof;  /*current profile index*/
	unsigned char set_prof;  /*set profile index*/
	unsigned int channel;    /*pa channel select*/
	unsigned int vol_step;
	unsigned int re_max;
	unsigned int re_min;

	struct device *dev;
	struct i2c_client *i2c;
	char acf_name[AW_NAME_MAX];
	char monitor_name[AW_NAME_MAX];
	void *private_data;

	struct aw_int_desc int_desc;
	struct aw_pwd_desc pwd_desc;
	struct aw_amppd_desc amppd_desc;
	struct aw_mute_desc mute_desc;
	struct aw_vcalb_desc vcalb_desc;
	struct aw_sysst_desc sysst_desc;
	struct aw_profctrl_desc profctrl_desc;
	struct aw_bstctrl_desc bstctrl_desc;
	struct aw_cco_mux_desc cco_mux_desc;
	struct aw_voltage_desc voltage_desc;
	struct aw_temperature_desc temp_desc;
	struct aw_ipeak_desc ipeak_desc;
	struct aw_volume_desc volume_desc;
	struct aw_prof_info prof_info;
	struct aw_cali_desc cali_desc;
	struct aw_monitor_desc monitor_desc;
	struct aw_soft_rst soft_rst;
	struct aw_frcpwm_desc frcpwm_desc;
	struct aw_device_ops ops;
	struct list_head list_node;
};


int aw_dev_load_acf_check(struct aw_container *aw_cfg);
void aw_dev_deinit(struct aw_device *aw_dev);
int aw_device_init(struct aw_device *aw_dev, struct aw_container *aw_cfg);
int aw_device_start(struct aw_device *aw_dev);
int aw_device_stop(struct aw_device *aw_dev);
int aw_dev_reg_update(struct aw_device *aw_dev, bool force);
int aw_device_irq_reinit(struct aw_device *aw_dev);

/*profile*/
int aw_dev_prof_update(struct aw_device *aw_dev, bool force);
int aw_dev_get_profile_count(struct aw_device *aw_dev);
int aw_dev_get_profile_name(struct aw_device *aw_dev, char *name, int index);
int aw_dev_check_profile_index(struct aw_device *aw_dev, int index);
int aw_dev_get_profile_index(struct aw_device *aw_dev);
int aw_dev_set_profile_index(struct aw_device *aw_dev, int index);

/*re*/
int aw_dev_get_cali_re(struct aw_device *aw_dev, int32_t *cali_re);
int aw_dev_init_cali_re(struct aw_device *aw_dev);
int aw_dev_dc_status(struct aw_device *aw_dev);

/*interrupt*/
int aw_dev_status(struct aw_device *aw_dev);
int aw_dev_get_int_status(struct aw_device *aw_dev, uint16_t *int_status);
void aw_dev_clear_int_status(struct aw_device *aw_dev);
int aw_dev_set_intmask(struct aw_device *aw_dev, bool flag);

/*fade int / out*/
void aw_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step);
int aw_dev_get_fade_vol_step(struct aw_device *aw_dev);
void aw_dev_get_fade_time(unsigned int *time, bool fade_in);
void aw_dev_set_fade_time(unsigned int time, bool fade_in);

/*dsp kcontrol*/
int aw_dev_set_afe_module_en(int type, int enable);
int aw_dev_get_afe_module_en(int type, int *status);
int aw_dev_set_copp_module_en(bool enable);
int aw_dev_set_spin(int spin_mode);
int aw_dev_get_spin(int *spin_mode);

int aw_device_probe(struct aw_device *aw_dev);
int aw_device_remove(struct aw_device *aw_dev);
int aw_dev_get_list_head(struct list_head **head);

#endif

