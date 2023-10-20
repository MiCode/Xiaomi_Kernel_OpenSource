#ifndef __AW882XX_DSP_H__
#define __AW882XX_DSP_H__

#define AW_MTK_PLATFORM_WITH_DSP

/*factor form 12bit(4096) to 1000*/
#define AW_DSP_RE_TO_SHOW_RE(re)	(((re) * (1000)) >> (12))
#define AW_SHOW_RE_TO_DSP_RE(re)	(((re) << 12) / (1000))

#define AW_DSP_SLEEP_TIME	(10)

#define AW_TX_DEFAULT_TOPO_ID		(0x1000FF00)
#define AW_RX_DEFAULT_TOPO_ID		(0x1000FF01)
#define AW_TX_DEFAULT_PORT_ID		(0x1007)
#define AW_RX_DEFAULT_PORT_ID		(0x1006)

enum aw_dsp_msg_type {
	AW_DSP_MSG_TYPE_DATA = 0,
	AW_DSP_MSG_TYPE_CMD = 1,
};

enum {
	AW_SPIN_0 = 0,
	AW_SPIN_90,
	AW_SPIN_180,
	AW_SPIN_270,
	AW_SPIN_MAX,
};


enum {
	AW_AUDIO_MIX_DSIABLE = 0,
	AW_AUDIO_MIX_ENABLE,
};

#define AW_DSP_MSG_HDR_VER (1)
typedef struct aw_msg_hdr aw_dsp_msg_t;

int aw882xx_dsp_write_msg(struct aw_device *aw_dev, uint32_t msg_id, char *data_ptr, unsigned int data_size);
int aw882xx_dsp_read_msg(struct aw_device *aw_dev, uint32_t msg_id, char *data_ptr, unsigned int data_size);
int aw882xx_dsp_write_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_read_cali_cfg(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_noise_en(struct aw_device *aw_dev, bool is_noise);
int aw882xx_dsp_write_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_read_vmax(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_write_params(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_write_cali_re(struct aw_device *aw_dev, int32_t cali_re);
int aw882xx_dsp_read_cali_re(struct aw_device *aw_dev, int32_t *cali_re);
int aw882xx_dsp_read_r0(struct aw_device *aw_dev, int32_t *r0);
int aw882xx_dsp_read_st(struct aw_device *aw_dev, int32_t *r0, int32_t *te);
int aw882xx_dsp_read_te(struct aw_device *aw_dev, int32_t *te);
int aw882xx_dsp_get_dc_status(struct aw_device *aw_dev);
int aw882xx_dsp_hmute_en(struct aw_device *aw_dev, bool is_hmute);
int aw882xx_dsp_cali_en(struct aw_device *aw_dev, int32_t cali_msg_data);
int aw882xx_dsp_read_f0(struct aw_device *aw_dev, int32_t *f0);
int aw882xx_dsp_read_f0_q(struct aw_device *aw_dev, int32_t *f0, int32_t *q);
int aw882xx_dsp_read_cali_data(struct aw_device *aw_dev, char *data, unsigned int data_len);
int aw882xx_dsp_set_afe_module_en(int type, int enable);
int aw882xx_dsp_get_afe_module_en(int type, int *status);
int aw882xx_dsp_set_copp_module_en(bool enable);
int aw882xx_dsp_write_spin(int spin_mode);
int aw882xx_dsp_read_spin(int *spin_mode);
int aw882xx_get_algo_version(struct aw_device *aw_dev, char *algo_ver_buf);
void aw882xx_device_parse_topo_id_dt(struct aw_device *aw_dev);
void aw882xx_device_parse_port_id_dt(struct aw_device *aw_dev);
int aw882xx_dsp_set_mixer_en(struct aw_device *aw_dev, uint32_t mixer_en);

#endif

