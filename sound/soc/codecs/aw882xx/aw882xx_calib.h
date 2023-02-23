#ifndef __AW882XX_CALIBRATION_H__
#define __AW882XX_CALIBRATION_H__

#define AW_CALI_STORE_EXAMPLE
#define AW_CALI_READ_TIMES (3)
#define AW_ERRO_CALI_VALUE (0)
#define AW_CALI_RE_DEFAULT_TIMER	(3000)

#define AW_CALI_RE_MAX     (15000)
#define AW_CALI_RE_MIN     (4000)

#define AW_CALI_CFG_NUM (3)
#define AW_CALI_DATA_NUM (6)
#define AW_PARAMS_NUM (600)
#define AW_KILO_PARAMS_NUM (1000)

#define AW_CALI_RE_DEFAULT_MAX		(50000)
#define AW_CALI_RE_DEFAULT_MIN		(1000)

#define AW_DEV_RE_RANGE	(RE_RANGE_NUM * AW_DEV_CH_MAX)

struct aw_device;

enum afe_module_type {
	AW_RX_MODULE = 0,
	AW_TX_MODULE = 1,
};

struct cali_cfg {
	int32_t data[AW_CALI_CFG_NUM];
};

struct cali_data {
	int32_t data[AW_CALI_DATA_NUM];
};

struct params_data {
	int32_t data[AW_PARAMS_NUM];
};

struct ptr_params_data {
	int len;
	int32_t *data;
};

struct f0_q_data {
	int32_t data[4];
};

enum {
	AW_IOCTL_MSG_IOCTL = 0,
	AW_IOCTL_MSG_RD_DSP,
	AW_IOCTL_MSG_WR_DSP
};

enum {
	CALI_CHECK_DISABLE = 0,
	CALI_CHECK_ENABLE = 1,
};

enum {
	CALI_RESULT_NONE = 0,
	CALI_RESULT_NORMAL = 1,
	CALI_RESULT_ERROR = -1,
};

enum {
	RE_MIN_FLAG = 0,
	RE_MAX_FLAG = 1,
	RE_RANGE_NUM = 2,
};

enum {
	CALI_DATA_RE = 0,
	CALI_DATA_F0,
	CALI_DATA_F0_Q,
};

struct re_data {
	uint32_t re_range[2];
};

#define AW_IOCTL_MSG_VERSION (0)
typedef struct {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t data_len;
	char *data_buf;
	int32_t reseriver[2];
} aw_ioctl_msg_t;

#define AW_IOCTL_MAGIC				'a'
#define AW_IOCTL_SET_CALI_CFG			_IOWR(AW_IOCTL_MAGIC, 1, struct cali_cfg)
#define AW_IOCTL_GET_CALI_CFG			_IOWR(AW_IOCTL_MAGIC, 2, struct cali_cfg)
#define AW_IOCTL_GET_CALI_DATA			_IOWR(AW_IOCTL_MAGIC, 3, struct cali_data)
#define AW_IOCTL_SET_NOISE			_IOWR(AW_IOCTL_MAGIC, 4, int32_t)
#define AW_IOCTL_GET_F0				_IOWR(AW_IOCTL_MAGIC, 5, int32_t)
#define AW_IOCTL_SET_CALI_RE			_IOWR(AW_IOCTL_MAGIC, 6, int32_t)
#define AW_IOCTL_GET_CALI_RE			_IOWR(AW_IOCTL_MAGIC, 7, int32_t)
#define AW_IOCTL_SET_VMAX			_IOWR(AW_IOCTL_MAGIC, 8, int32_t)
#define AW_IOCTL_GET_VMAX			_IOWR(AW_IOCTL_MAGIC, 9, int32_t)
#define AW_IOCTL_SET_PARAM			_IOWR(AW_IOCTL_MAGIC, 10, struct params_data)
#define AW_IOCTL_ENABLE_CALI			_IOWR(AW_IOCTL_MAGIC, 11, int8_t)
#define AW_IOCTL_SET_PTR_PARAM_NUM		_IOWR(AW_IOCTL_MAGIC, 12, struct ptr_params_data)
#define AW_IOCTL_GET_F0_Q			_IOWR(AW_IOCTL_MAGIC, 13, struct f0_q_data)
#define AW_IOCTL_SET_DSP_HMUTE			_IOWR(AW_IOCTL_MAGIC, 14, int32_t)
#define AW_IOCTL_SET_CALI_CFG_FLAG		_IOWR(AW_IOCTL_MAGIC, 15, int32_t)
#define AW_IOCTL_MSG				_IOWR(AW_IOCTL_MAGIC, 16, aw_ioctl_msg_t)
#define AW_IOCTL_GET_RE_RANGE			_IOWR(AW_IOCTL_MAGIC, 17, struct re_data)

enum{
	AW_CALI_MODE_NONE = 0,
	AW_CALI_MODE_ALL,
	AW_CALI_MODE_MAX,
};

enum {
	AW_CALI_CMD_RE = 0,
	AW_CALI_CMD_F0,
	AW_CALI_CMD_RE_F0,
	AW_CALI_CMD_F0_Q,
	AW_CALI_CMD_RE_F0_Q,
};

enum {
	CALI_OPS_HMUTE = 0X0001,
	CALI_OPS_NOISE = 0X0002,
};

enum {
	CALI_TYPE_RE = 0,
	CALI_TYPE_F0,
};

enum {
	CALI_STR_NONE = 0,
	CALI_STR_CALI_RE_F0,
	CALI_STR_CALI_RE,
	CALI_STR_CALI_F0,
	CALI_STR_SET_RE,
	CALI_STR_SHOW_RE,		/*show cali_re*/
	CALI_STR_SHOW_R0,		/*show real r0*/
	CALI_STR_SHOW_CALI_F0,		/*GET DEV CALI_F0*/
	CALI_STR_SHOW_F0,		/*SHOW REAL F0*/
	CALI_STR_SHOW_TE,
	CALI_STR_SHOW_ST,
	CALI_STR_DEV_SEL,		/*switch device*/
	CALI_STR_VER,
	CALI_STR_DEV_NUM,
	CALI_STR_CALI_F0_Q,
	CALI_STR_SHOW_F0_Q,
	CALI_STR_SHOW_RE_RANGE,
	CALI_STR_MAX,
};

struct aw_cali_desc {
	unsigned char status;
	unsigned char mode;		/*0:NONE 1:ATTR 2:CLASS 3:MISC */
	int32_t cali_re;		/*set cali_re*/
	int32_t cali_f0;		/*store cali_f0*/
	int32_t cali_q;			/*store cali q*/
	int8_t cali_result;
	uint8_t cali_check_st;
};

int aw882xx_cali_init(struct aw_cali_desc *cali_desc);
void aw882xx_cali_deinit(struct aw_cali_desc *cali_desc);
int aw882xx_cali_svc_get_cali_status(void);
int aw882xx_cali_read_re_from_nvram(int32_t *cali_re, int32_t ch_index);


#endif
