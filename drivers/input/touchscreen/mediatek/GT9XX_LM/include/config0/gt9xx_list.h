#ifndef GT9XX_LIST_H
#define GT9XX_LIST_H

typedef unsigned char u8;
#define CFG_GROUP_LEN(p_cfg_grp) (ARRAY_SIZE(p_cfg_grp) / sizeof(p_cfg_grp[0]))

#include "GT9110P_4020_V1.h"
#include "GT9110P_4020_V2.h"
#include "GT9110P_DZ.h"
#include "GT9110P_SX.h"
#include "gt9xx_mid.h"
struct mid_cfg_data project_cfg_data[] = {
	{"GT9110P_4020_V1", &GT9110P_4020_V1_CFG},
	{"GT9110P_4020_V2", &GT9110P_4020_V2_CFG},
	{"GT9110P_DZ", &GT9110P_DZ_CFG},
	{"GT9110P_SX", &GT9110P_SX_CFG},
};

#endif
