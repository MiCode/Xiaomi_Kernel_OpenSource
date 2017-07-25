/*
 * Definitions for akm8963 compass chip.
 */
#ifndef AKM8963_H
#define AKM8963_H

#include <linux/ioctl.h>

/* Device specific constant values */
#define AK8963_REG_WIA			0x00
#define AK8963_REG_INFO			0x01
#define AK8963_REG_ST1			0x02
#define AK8963_REG_HXL			0x03
#define AK8963_REG_HXH			0x04
#define AK8963_REG_HYL			0x05
#define AK8963_REG_HYH			0x06
#define AK8963_REG_HZL			0x07
#define AK8963_REG_HZH			0x08
#define AK8963_REG_ST2			0x09
#define AK8963_REG_CNTL1		0x0A
#define AK8963_REG_CNTL2		0x0B
#define AK8963_REG_ASTC			0x0C
#define AK8963_REG_TS1			0x0D
#define AK8963_REG_TS2			0x0E
#define AK8963_REG_I2CDIS		0x0F
#define AK8963_FUSE_ASAX		0x10
#define AK8963_FUSE_ASAY		0x11
#define AK8963_FUSE_ASAZ		0x12

#define AK8963_MODE_POWERDOWN		0x00
#define AK8963_MODE_SNG_MEASURE		0x01
#define AK8963_MODE_CONT1_MEASURE	0x02
#define AK8963_MODE_EXT_TRIG_MEASURE	0x04
#define AK8963_MODE_CONT2_MEASURE	0x06
#define AK8963_MODE_SELF_TEST		0x08
#define AK8963_MODE_FUSE_ACCESS		0x0F

#define AKM8963_BIT_OP_14		0x00
#define AKM8963_BIT_OP_16		0x10

#define AK8963_RESET_DATA		0x01

#define AK8963_REGS_SIZE		13
#define AK8963_WIA_VALUE		0x48

#define AKM8963_ST1_DRDY		0x01
#define AKM8963_ST1_DOR			0x02
#define AKM8963_ST2_HOLF		0x08
#define AKM8963_ST2_BITM		0x10

/* To avoid device dependency, convert to general name */
#define AKM_I2C_NAME			"akm8963"
#define AKM_MISCDEV_NAME		"akm8963_dev"
#define AKM_SYSCLS_NAME			"compass"
#define AKM_SYSDEV_NAME			"akm8963"
#define AKM_REG_MODE			AK8963_REG_CNTL1
#define AKM_REG_RESET			AK8963_REG_CNTL2
#define AKM_REG_STATUS			AK8963_REG_ST1
#define AKM_MEASURE_TIME_US		10000
#define AKM_DRDY_IS_HIGH(x)		((x) & 0x01)
#define AKM_SENSOR_INFO_SIZE		2
#define AKM_SENSOR_CONF_SIZE		3
#define AKM_SENSOR_DATA_SIZE		8

#define AKM_YPR_DATA_SIZE		16
#define AKM_RWBUF_SIZE			16
#define AKM_REGS_SIZE			AK8963_REGS_SIZE
#define AKM_REGS_1ST_ADDR		AK8963_REG_WIA
#define AKM_FUSE_1ST_ADDR		AK8963_FUSE_ASAX

#define AKM_MODE_SNG_MEASURE		AK8963_MODE_SNG_MEASURE
#define AKM_MODE_SELF_TEST		AK8963_MODE_SELF_TEST
#define AKM_MODE_FUSE_ACCESS		AK8963_MODE_FUSE_ACCESS
#define AKM_MODE_POWERDOWN		AK8963_MODE_POWERDOWN
#define AKM_RESET_DATA			AK8963_RESET_DATA

#define ACC_DATA_FLAG			0
#define MAG_DATA_FLAG			1
#define FUSION_DATA_FLAG		2
#define AKM_NUM_SENSORS			3

#define ACC_DATA_READY			(1<<(ACC_DATA_FLAG))
#define MAG_DATA_READY			(1<<(MAG_DATA_FLAG))
#define FUSION_DATA_READY		(1<<(FUSION_DATA_FLAG))

#define AKMIO				0xA1

/* IOCTLs for AKM library */
#define ECS_IOCTL_READ		_IOWR(AKMIO, 0x01, char)
#define ECS_IOCTL_WRITE		_IOW(AKMIO, 0x02, char)
#define ECS_IOCTL_RESET		_IO(AKMIO, 0x03)
#define ECS_IOCTL_SET_MODE	_IOW(AKMIO, 0x10, char)
#define ECS_IOCTL_SET_YPR	_IOW(AKMIO, 0x11, int[AKM_YPR_DATA_SIZE])
#define ECS_IOCTL_GET_INFO \
	_IOR(AKMIO, 0x20, unsigned char[AKM_SENSOR_INFO_SIZE])
#define ECS_IOCTL_GET_CONF \
	_IOR(AKMIO, 0x21, unsigned char[AKM_SENSOR_CONF_SIZE])
#define ECS_IOCTL_GET_DATA \
	_IOR(AKMIO, 0x22, unsigned char[AKM_SENSOR_DATA_SIZE])
#define ECS_IOCTL_GET_OPEN_STATUS	_IOR(AKMIO, 0x23, int)
#define ECS_IOCTL_GET_CLOSE_STATUS	_IOR(AKMIO, 0x24, int)
#define ECS_IOCTL_GET_DELAY		_IOR(AKMIO, 0x25, long long int)
#define ECS_IOCTL_GET_LAYOUT		_IOR(AKMIO, 0x26, char)
#define ECS_IOCTL_GET_ACCEL		_IOR(AKMIO, 0x30, short[3])

struct akm8963_platform_data {
	char layout;
	int	auto_report;
	int gpio_DRDY;
	int gpio_rstn;
	int gpio_int;
	unsigned int int_flags;
	bool use_int;
};

#endif

