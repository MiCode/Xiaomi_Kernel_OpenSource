#ifndef __UAPI_MEDIA_MSM_AIS_MGR_H__
#define __UAPI_MEDIA_MSM_AIS_MGR_H__

#include <media/ais/msm_ais.h>

enum clk_mgr_cfg_type_t {
	AIS_CLK_ENABLE,
	AIS_CLK_DISABLE,
};

#define AIS_CLK_ENABLE AIS_CLK_ENABLE
#define AIS_CLK_DISABLE AIS_CLK_DISABLE

struct clk_mgr_cfg_data_ext {
	enum clk_mgr_cfg_type_t cfg_type;
};

struct clk_mgr_cfg_data {
	enum clk_mgr_cfg_type_t cfg_type;
};

#define VIDIOC_MSM_AIS_CLK_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct clk_mgr_cfg_data)

#define VIDIOC_MSM_AIS_CLK_CFG_EXT \
	_IOWR('V', BASE_VIDIOC_PRIVATE+1, struct clk_mgr_cfg_data_ext)

#endif /* __UAPI_MEDIA_MSM_AIS_MGR_H__ */
