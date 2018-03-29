/*
 * Definitions for akm09912 compass chip.
 */
#ifndef AKM09912_H
#define AKM09912_H

#include <linux/ioctl.h>

#define AKM09912_I2C_NAME "akm09912"

#define AKM09912_I2C_ADDRESS	0x18
#define AKM09912_BUFSIZE		0x50

#define SENSOR_DATA_SIZE		9	/* Rx buffer size, i.e from ST1 to ST2 */
#define RWBUF_SIZE				16	/* Read/Write buffer size. */
#define CALIBRATION_DATA_SIZE	26

#define AK09912_REG_WIA1			0x00
#define AK09912_REG_WIA2			0x01
#define AK09912_REG_INFO1			0x02
#define AK09912_REG_INFO2			0x03
#define AK09912_REG_ST1				0x10
#define AK09912_REG_HXL				0x11
#define AK09912_REG_HXH				0x12
#define AK09912_REG_HYL				0x13
#define AK09912_REG_HYH				0x14
#define AK09912_REG_HZL				0x15
#define AK09912_REG_HZH				0x16
#define AK09912_REG_TMPS			0x17
#define AK09912_REG_ST2				0x18
#define AK09912_REG_CNTL1			0x30
#define AK09912_REG_CNTL2			0x31
#define AK09912_REG_CNTL3			0x32

#define AK09912_FUSE_ASAX			0x60
#define AK09912_FUSE_ASAY			0x61
#define AK09912_FUSE_ASAZ			0x62

#define AK09912_MODE_SNG_MEASURE	0x01
#define AK09912_MODE_SELF_TEST		0x10
#define AK09912_MODE_FUSE_ACCESS	0x1F
#define AK09912_MODE_POWERDOWN		0x00
#define AK09912_RESET_DATA			0x01

#define AK09912_REGS_SIZE		13
#define AK09912_WIA1_VALUE		0x48
#define AK09912_WIA2_VALUE		0x05

/* Device specific constant values for AK8963*/
#define AK8963_REG_WIA		0x00
#define AK8963_REG_INFO		0x01
#define AK8963_REG_ST1		0x02
#define AK8963_REG_HXL		0x03
#define AK8963_REG_HXH		0x04
#define AK8963_REG_HYL		0x05
#define AK8963_REG_HYH		0x06
#define AK8963_REG_HZL		0x07
#define AK8963_REG_HZH		0x08
#define AK8963_REG_ST2		0x09
#define AK8963_REG_CNTL1	0x0A
#define AK8963_REG_CNTL2	0x0B
#define AK8963_REG_ASTC		0x0C
#define AK8963_REG_TS1		0x0D
#define AK8963_REG_TS2		0x0E
#define AK8963_REG_I2CDIS	0x0F

#define AK8963_FUSE_ASAX	0x10
#define AK8963_FUSE_ASAY	0x11
#define AK8963_FUSE_ASAZ	0x12

#define AK8963_MODE_SNG_MEASURE		0x01
#define AK8963_MODE_SELF_TEST		0x08
#define AK8963_MODE_FUSE_ACCESS		0x0F
#define AK8963_MODE_POWERDOWN		0x00
#define AK8963_RESET_DATA			0x01

#define AK8963_REGS_SIZE		13
#define AK8963_WIA_VALUE		0x48

/* To avoid device dependency, convert to general name */
#define AKM_I2C_NAME			"akm09912"
#define AKM_MISCDEV_NAME		"akm09912_dev"
#define AKM_SYSCLS_NAME			"compass"
#define AKM_SYSDEV_NAME			"akm09912"
#define AKM_REG_MODE			AK09912_REG_CNTL2
#define AKM_REG_RESET			AK09912_REG_CNTL3
#define AKM_REG_STATUS			AK09912_REG_ST1
#define AKM_MEASURE_TIME_US		10000
#define AKM_DRDY_IS_HIGH(x)		((x) & 0x01)
#define AKM_SENSOR_INFO_SIZE	2
#define AKM_SENSOR_CONF_SIZE	3
#define AKM_SENSOR_DATA_SIZE	9

#define AKM_YPR_DATA_SIZE		26
#define AKM_RWBUF_SIZE			16
#define AKM_REGS_SIZE			AK09912_REGS_SIZE
#define AKM_REGS_1ST_ADDR		AK09912_REG_WIA1
#define AKM_FUSE_1ST_ADDR		AK09912_FUSE_ASAX

#define AKM_MODE_SNG_MEASURE	AK09912_MODE_SNG_MEASURE
#define AKM_MODE_SELF_TEST		AK09912_MODE_SELF_TEST
#define AKM_MODE_FUSE_ACCESS	AK09912_MODE_FUSE_ACCESS
#define AKM_MODE_POWERDOWN		AK09912_MODE_POWERDOWN
#define AKM_RESET_DATA			AK09912_RESET_DATA

#define ACC_DATA_FLAG		0
#define MAG_DATA_FLAG		1
#define FUSION_DATA_FLAG	2
#define AKM_NUM_SENSORS		3

#define ACC_DATA_READY		(1<<(ACC_DATA_FLAG))
#define MAG_DATA_READY		(1<<(MAG_DATA_FLAG))
#define FUSION_DATA_READY	(1<<(FUSION_DATA_FLAG))

#define CONVERT_M			6
#define CONVERT_M_DIV		100
#define CONVERT_O			1
#define CONVERT_O_DIV		64

#define CONVERT_Q16			1
#define CONVERT_Q16_DIV		65536

#define CSPEC_SPI_USE			0
#define DBG_LEVEL0   0x0001
#define DBG_LEVEL1   0x0002
#define DBG_LEVEL2   0x0003
#define DBG_LEVEL3   0x0004
#define DBGFLAG      DBG_LEVEL2

#ifndef DBGPRINT
#define DBGPRINT(level, format, ...) \
	((((level) != 0) && ((level) <= DBGFLAG))  \
	? (pr_debug((format), ##__VA_ARGS__)) \
	: (void)0)

#endif

struct akm09912_platform_data {
	char layout;
	char outbit;
	int gpio_DRDY;
	int gpio_RSTN;
};

/*** Limit of factory shipment test *******************************************/
#define TLIMIT_TN_REVISION_09912				""
#define TLIMIT_NO_RST_WIA1_09912				"1-3"
#define TLIMIT_TN_RST_WIA1_09912				"RST_WIA1"
#define TLIMIT_LO_RST_WIA1_09912				0x48
#define TLIMIT_HI_RST_WIA1_09912				0x48
#define TLIMIT_NO_RST_WIA2_09912				"1-4"
#define TLIMIT_TN_RST_WIA2_09912				"RST_WIA2"
#define TLIMIT_LO_RST_WIA2_09912				0x04
#define TLIMIT_HI_RST_WIA2_09912				0x04

#define TLIMIT_NO_WR_CNTL2_09912              "1-7"
#define TLIMIT_TN_WR_CNTL2_09912              "WR_CNTL2"
#define TLIMIT_LO_WR_CNTL2_09912              0x1F
#define TLIMIT_HI_WR_CNTL2_09912              0x1F
#define TLIMIT_NO_ASAX_09912                  "1-8"
#define TLIMIT_TN_ASAX_09912                  "ASAX"
#define TLIMIT_LO_ASAX_09912                  1
#define TLIMIT_HI_ASAX_09912                  254
#define TLIMIT_NO_ASAY_09912                  "1-9"
#define TLIMIT_TN_ASAY_09912                  "ASAY"
#define TLIMIT_LO_ASAY_09912                  1
#define TLIMIT_HI_ASAY_09912                  254
#define TLIMIT_NO_ASAZ_09912                  "1-10"
#define TLIMIT_TN_ASAZ_09912                  "ASAZ"
#define TLIMIT_LO_ASAZ_09912                  1
#define TLIMIT_HI_ASAZ_09912                 254

#define TLIMIT_NO_SNG_ST1_09912				"2-4"
#define TLIMIT_TN_SNG_ST1_09912				"SNG_ST1"
#define TLIMIT_LO_SNG_ST1_09912				1
#define TLIMIT_HI_SNG_ST1_09912				1

#define TLIMIT_NO_SNG_HX_09912				"2-5"
#define TLIMIT_TN_SNG_HX_09912				"SNG_HX"
#define TLIMIT_LO_SNG_HX_09912				-32751
#define TLIMIT_HI_SNG_HX_09912				32751

#define TLIMIT_NO_SNG_HY_09912				"2-7"
#define TLIMIT_TN_SNG_HY_09912				"SNG_HY"
#define TLIMIT_LO_SNG_HY_09912				-32751
#define TLIMIT_HI_SNG_HY_09912				32751

#define TLIMIT_NO_SNG_HZ_09912				"2-9"
#define TLIMIT_TN_SNG_HZ_09912				"SNG_HZ"
#define TLIMIT_LO_SNG_HZ_09912				-32751
#define TLIMIT_HI_SNG_HZ_09912				32751

#define TLIMIT_NO_SNG_TMPS_09912              "2-11"
#define TLIMIT_TN_SNG_TMPS_09912              "SNG_TMPS"
#define TLIMIT_LO_SNG_TMPS_09912              0x28
#define TLIMIT_HI_SNG_TMPS_09912              0xE0

#define TLIMIT_NO_SNG_ST2_09912				"2-12"
#define TLIMIT_TN_SNG_ST2_09912				"SNG_ST2"
#define TLIMIT_LO_SNG_ST2_09912				0
#define TLIMIT_HI_SNG_ST2_09912				0

#define TLIMIT_NO_SLF_ST1_09912				"2-15"
#define TLIMIT_TN_SLF_ST1_09912				"SLF_ST1"
#define TLIMIT_LO_SLF_ST1_09912				1
#define TLIMIT_HI_SLF_ST1_09912				1

#define TLIMIT_NO_SLF_RVHX_09912				"2-16"
#define TLIMIT_TN_SLF_RVHX_09912				"SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX_09912				-200
#define TLIMIT_HI_SLF_RVHX_09912				200

#define TLIMIT_NO_SLF_RVHY_09912				"2-18"
#define TLIMIT_TN_SLF_RVHY_09912				"SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY_09912				-200
#define TLIMIT_HI_SLF_RVHY_09912				200

#define TLIMIT_NO_SLF_RVHZ_09912				"2-20"
#define TLIMIT_TN_SLF_RVHZ_09912				"SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ_09912				-1600
#define TLIMIT_HI_SLF_RVHZ_09912				-400

#define TLIMIT_NO_SLF_ST2_09912				"2-22"
#define TLIMIT_TN_SLF_ST2_09912				"SLF_ST2"
#define TLIMIT_LO_SLF_ST2_09912				0
#define TLIMIT_HI_SLF_ST2_09912				0

/*** Limit of factory shipment test *******************************************/

#define TLIMIT_TN_REVISION				""
#define TLIMIT_NO_RST_WIA				"1-3"
#define TLIMIT_TN_RST_WIA				"RST_WIA"
#define TLIMIT_LO_RST_WIA				0x48
#define TLIMIT_HI_RST_WIA				0x48
#define TLIMIT_NO_RST_INFO				"1-4"
#define TLIMIT_TN_RST_INFO				"RST_INFO"
#define TLIMIT_LO_RST_INFO				0
#define TLIMIT_HI_RST_INFO				255
#define TLIMIT_NO_RST_ST1				"1-5"
#define TLIMIT_TN_RST_ST1				"RST_ST1"
#define TLIMIT_LO_RST_ST1				0
#define TLIMIT_HI_RST_ST1				0
#define TLIMIT_NO_RST_HXL				"1-6"
#define TLIMIT_TN_RST_HXL				"RST_HXL"
#define TLIMIT_LO_RST_HXL				0
#define TLIMIT_HI_RST_HXL				0
#define TLIMIT_NO_RST_HXH				"1-7"
#define TLIMIT_TN_RST_HXH				"RST_HXH"
#define TLIMIT_LO_RST_HXH				0
#define TLIMIT_HI_RST_HXH				0
#define TLIMIT_NO_RST_HYL				"1-8"
#define TLIMIT_TN_RST_HYL				"RST_HYL"
#define TLIMIT_LO_RST_HYL				0
#define TLIMIT_HI_RST_HYL				0
#define TLIMIT_NO_RST_HYH				"1-9"
#define TLIMIT_TN_RST_HYH				"RST_HYH"
#define TLIMIT_LO_RST_HYH				0
#define TLIMIT_HI_RST_HYH				0
#define TLIMIT_NO_RST_HZL				"1-10"
#define TLIMIT_TN_RST_HZL				"RST_HZL"
#define TLIMIT_LO_RST_HZL				0
#define TLIMIT_HI_RST_HZL				0
#define TLIMIT_NO_RST_HZH				"1-11"
#define TLIMIT_TN_RST_HZH				"RST_HZH"
#define TLIMIT_LO_RST_HZH				0
#define TLIMIT_HI_RST_HZH				0
#define TLIMIT_NO_RST_ST2				"1-12"
#define TLIMIT_TN_RST_ST2				"RST_ST2"
#define TLIMIT_LO_RST_ST2				0
#define TLIMIT_HI_RST_ST2				0
#define TLIMIT_NO_RST_CNTL				"1-13"
#define TLIMIT_TN_RST_CNTL				"RST_CNTL"
#define TLIMIT_LO_RST_CNTL				0
#define TLIMIT_HI_RST_CNTL				0
#define TLIMIT_NO_RST_ASTC				"1-14"
#define TLIMIT_TN_RST_ASTC				"RST_ASTC"
#define TLIMIT_LO_RST_ASTC				0
#define TLIMIT_HI_RST_ASTC				0
#define TLIMIT_NO_RST_I2CDIS			"1-15"
#define TLIMIT_TN_RST_I2CDIS			"RST_I2CDIS"
#define TLIMIT_LO_RST_I2CDIS_USEI2C		0
#define TLIMIT_HI_RST_I2CDIS_USEI2C		0
#define TLIMIT_LO_RST_I2CDIS_USESPI		1
#define TLIMIT_HI_RST_I2CDIS_USESPI		1
#define TLIMIT_NO_ASAX					"1-17"
#define TLIMIT_TN_ASAX					"ASAX"
#define TLIMIT_LO_ASAX					1
#define TLIMIT_HI_ASAX					254
#define TLIMIT_NO_ASAY					"1-18"
#define TLIMIT_TN_ASAY					"ASAY"
#define TLIMIT_LO_ASAY					1
#define TLIMIT_HI_ASAY					254
#define TLIMIT_NO_ASAZ					"1-19"
#define TLIMIT_TN_ASAZ					"ASAZ"
#define TLIMIT_LO_ASAZ					1
#define TLIMIT_HI_ASAZ					254
#define TLIMIT_NO_WR_CNTL				"1-20"
#define TLIMIT_TN_WR_CNTL				"WR_CNTL"
#define TLIMIT_LO_WR_CNTL				0x0F
#define TLIMIT_HI_WR_CNTL				0x0F

#define TLIMIT_NO_SNG_ST1				"2-3"
#define TLIMIT_TN_SNG_ST1				"SNG_ST1"
#define TLIMIT_LO_SNG_ST1				1
#define TLIMIT_HI_SNG_ST1				1

#define TLIMIT_NO_SNG_HX				"2-4"
#define TLIMIT_TN_SNG_HX				"SNG_HX"
#define TLIMIT_LO_SNG_HX				-32759
#define TLIMIT_HI_SNG_HX				32759

#define TLIMIT_NO_SNG_HY				"2-6"
#define TLIMIT_TN_SNG_HY				"SNG_HY"
#define TLIMIT_LO_SNG_HY				-32759
#define TLIMIT_HI_SNG_HY				32759

#define TLIMIT_NO_SNG_HZ				"2-8"
#define TLIMIT_TN_SNG_HZ				"SNG_HZ"
#define TLIMIT_LO_SNG_HZ				-32759
#define TLIMIT_HI_SNG_HZ				32759

#define TLIMIT_NO_SNG_ST2				"2-10"
#define TLIMIT_TN_SNG_ST2				"SNG_ST2"
#define TLIMIT_LO_SNG_ST2				0
#define TLIMIT_HI_SNG_ST2				0

#define TLIMIT_NO_SLF_ST1				"2-14"
#define TLIMIT_TN_SLF_ST1				"SLF_ST1"
#define TLIMIT_LO_SLF_ST1				1
#define TLIMIT_HI_SLF_ST1				1

#define TLIMIT_NO_SLF_RVHX				"2-15"
#define TLIMIT_TN_SLF_RVHX				"SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX				-200
#define TLIMIT_HI_SLF_RVHX				200

#define TLIMIT_NO_SLF_RVHY				"2-17"
#define TLIMIT_TN_SLF_RVHY				"SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY				-200
#define TLIMIT_HI_SLF_RVHY				200

#define TLIMIT_NO_SLF_RVHZ				"2-19"
#define TLIMIT_TN_SLF_RVHZ				"SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ				-3200
#define TLIMIT_HI_SLF_RVHZ				-800

#define TLIMIT_NO_SLF_ST2				"2-21"
#define TLIMIT_TN_SLF_ST2				"SLF_ST2"
#define TLIMIT_LO_SLF_ST2				0
#define TLIMIT_HI_SLF_ST2				0

#endif
