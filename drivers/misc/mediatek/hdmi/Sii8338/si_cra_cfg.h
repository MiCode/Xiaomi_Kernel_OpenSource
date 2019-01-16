#ifndef __SI_CRA_CFG_H__
#define __SI_CRA_CFG_H__
typedef enum _deviceAddrTypes_t {
	DEV_I2C_0,
	DEV_I2C_1,
	DEV_I2C_2,
	DEV_I2C_3,
	DEV_I2C_OFFSET,
	DEV_I2C_1_OFFSET,
	DEV_I2C_2_OFFSET,
	DEV_I2C_3_OFFSET,
	DEV_DDC_0 = 0,
	DEV_DDC_1,
	DEV_PARALLEL,
} deviceAddrTypes_t;
enum {
	DEV_PAGE_TPI_0 = (0x72),
	DEV_PAGE_TX_L0_0 = (0x72),
	DEV_PAGE_TPI_1 = (0x76),
	DEV_PAGE_TX_L0_1 = (0x76),
	DEV_PAGE_TX_L1_0 = (0x7A),
	DEV_PAGE_TX_L1_1 = (0x7E),
	DEV_PAGE_TX_2_0 = (0x92),
	DEV_PAGE_TX_2_1 = (0x96),
	DEV_PAGE_TX_3_0 = (0x9A),
	DEV_PAGE_TX_3_1 = (0x9E),
	DEV_PAGE_CBUS_0 = (0xC8),
	DEV_PAGE_CBUS_1 = (0xCC),
	DEV_PAGE_DDC_EDID = (0xA0),
	DEV_PAGE_DDC_SEGM = (0x60),
};
enum {
	TX_PAGE_TPI = 0x0000,	/* 0x72 */
	TX_PAGE_L0 = 0x0100,	/* 0x72 */
	TX_PAGE_L1 = 0x0200,	/* 0x7A */
	TX_PAGE_2 = 0x0300,	/* 0x92 */
	TX_PAGE_3 = 0x0400,	/* 0x9A */
	TX_PAGE_CBUS = 0x0500,
	TX_PAGE_DDC_EDID = 0x0600,	/* 0xA0 */
	TX_PAGE_DDC_SEGM = 0x0700,	/* 0x60 */
};
#define SII_CRA_MAX_DEVICE_INSTANCES    2
#define SII_CRA_DEVICE_PAGE_COUNT       8
typedef struct pageConfig {
	deviceAddrTypes_t busType;
	prefuint_t address;
} pageConfig_t;
#endif
