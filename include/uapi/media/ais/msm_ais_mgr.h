#ifndef __UAPI_MEDIA_MSM_AIS_MGR_H__
#define __UAPI_MEDIA_MSM_AIS_MGR_H__

#include <media/ais/msm_ais.h>

#define VREGNAME_SIZE 32
#define CLKNAME_SIZE  32

enum cam_ahb_clk_vote {
	/* need to update the voting requests
	 * according to dtsi entries.
	 */
	CAM_AHB_SUSPEND_VOTE = 0x0,
	CAM_AHB_SVS_VOTE = 0x01,
	CAM_AHB_NOMINAL_VOTE = 0x02,
	CAM_AHB_TURBO_VOTE = 0x03,
	CAM_AHB_DYNAMIC_VOTE = 0xFF,
};

enum clk_mgr_cfg_type_t {
	AIS_CLK_ENABLE,
	AIS_CLK_DISABLE,
	AIS_CLK_ENABLE_ALLCLK,
	AIS_CLK_DISABLE_ALLCLK
};

enum ais_mgr_cfg_ext_type_t {
	AIS_DIAG_GET_REGULATOR_INFO_LIST,
	AIS_DIAG_GET_BUS_INFO_STATE,
	AIS_DIAG_GET_CLK_INFO_LIST,
	AIS_DIAG_GET_GPIO_LIST,
	AIS_DIAG_SET_GPIO_LIST,
};

#define AIS_CLK_ENABLE AIS_CLK_ENABLE
#define AIS_CLK_DISABLE AIS_CLK_DISABLE


struct msm_camera_reg_list_cmd {
	void __user *value_list;
	void __user *regaddr_list;
	uint32_t reg_num;
};

struct msm_ais_diag_regulator_info_t {
	int enable;
	char regulatorname[VREGNAME_SIZE];
};

struct msm_ais_diag_regulator_info_list_t {
	struct msm_ais_diag_regulator_info_t *infolist;
	uint32_t regulator_num;
};

struct msm_ais_diag_bus_info_t {
	enum cam_ahb_clk_vote ahb_clk_vote_state;
	uint32_t isp_bus_vector_idx;		/* 0 - init 1- ping 2 - pong */
	uint64_t isp_ab;
	uint64_t isp_ib;
};

struct msm_ais_diag_clk_info_t {
	char  clk_name[CLKNAME_SIZE];
	long  clk_rate;
	uint8_t enable;
};

struct msm_ais_diag_clk_list_t {
	void __user *clk_info;
	uint32_t clk_num;
};

struct msm_ais_diag_gpio_list_t {
	uint32_t __user *gpio_idx_list;
	int32_t __user *gpio_val_list;
	uint32_t gpio_num;
};

struct clk_mgr_cfg_data_ext {
	enum ais_mgr_cfg_ext_type_t cfg_type;
	union {
		struct msm_ais_diag_regulator_info_list_t vreg_infolist;
		struct msm_ais_diag_bus_info_t bus_info;
		struct msm_ais_diag_clk_list_t clk_infolist;
		struct msm_ais_diag_gpio_list_t gpio_list;
	} data;
};

struct clk_mgr_cfg_data {
	enum clk_mgr_cfg_type_t cfg_type;
};

#define VIDIOC_MSM_AIS_CLK_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct clk_mgr_cfg_data)

#define VIDIOC_MSM_AIS_CLK_CFG_EXT \
	_IOWR('V', BASE_VIDIOC_PRIVATE+1, struct clk_mgr_cfg_data_ext)

#endif /* __UAPI_MEDIA_MSM_AIS_MGR_H__ */
