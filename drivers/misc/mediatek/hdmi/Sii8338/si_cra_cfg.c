#include "si_common.h"
#include "si_cra.h"
#include "si_cra_cfg.h"
pageConfig_t g_addrDescriptor[SII_CRA_MAX_DEVICE_INSTANCES][SII_CRA_DEVICE_PAGE_COUNT] = {
	{
	 {DEV_I2C_0, DEV_PAGE_TPI_0},
	 {DEV_I2C_0, DEV_PAGE_TX_L0_0},
	 {DEV_I2C_0, DEV_PAGE_TX_L1_0},
	 {DEV_I2C_0, DEV_PAGE_TX_2_0},
	 {DEV_I2C_0, DEV_PAGE_TX_3_0},
	 {DEV_I2C_0, DEV_PAGE_CBUS_0},
	 {DEV_DDC_0, DEV_PAGE_DDC_EDID},
	 {DEV_DDC_0, DEV_PAGE_DDC_SEGM}
	 },
	{
	 {DEV_I2C_0, DEV_PAGE_TPI_1},
	 {DEV_I2C_0, DEV_PAGE_TX_L0_1},
	 {DEV_I2C_0, DEV_PAGE_TX_L1_1},
	 {DEV_I2C_0, DEV_PAGE_TX_2_1},
	 {DEV_I2C_0, DEV_PAGE_TX_3_1},
	 {DEV_I2C_0, DEV_PAGE_CBUS_1},
	 {DEV_DDC_0, DEV_PAGE_DDC_EDID},
	 {DEV_DDC_0, DEV_PAGE_DDC_SEGM}
	 }
};

SiiReg_t g_siiRegPageBaseRegs[SII_CRA_DEVICE_PAGE_COUNT] = {
	TX_PAGE_L0 | 0xFF,
	TX_PAGE_L0 | 0xFF,
	TX_PAGE_L0 | 0xFC,
	TX_PAGE_L0 | 0xFD,
	TX_PAGE_L0 | 0xFE,
	TX_PAGE_L0 | 0xFF,
	TX_PAGE_L0 | 0xFF,
	TX_PAGE_L0 | 0xFF,
};

SiiReg_t g_siiRegPageBaseReassign[] = {
	0xFFFF
};
