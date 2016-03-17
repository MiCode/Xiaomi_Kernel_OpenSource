/*
* Copyright (C) 2012 Invensense, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#ifndef _INV_MPU_IIO_H_
#define _INV_MPU_IIO_H_

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/iio/imu/mpu.h>

#include "inv_sh_timesync.h"

#define REG_WHO_AM_I					0x00

#define REG_MOD_EN					0x02
#define BIT_I2C_IF_DIS					0x01
#define BIT_SERIF_FIFO_EN				0x02
#define BIT_DMP_EN					0x04
#define BIT_MCU_EN					0x08
#define BIT_I2C_MST_EN					0x10
#define BIT_SPI_MST_EN					0x20
#define BIT_TIMERS_EN					0x40

#define REG_MOD_RST					0x03

#define REG_PWR_MGMT_1					0x06
#define BIT_SOFT_RESET					0x80

#define REG_B0_SCRATCH_INT_EN				0x0F

#define REG_B0_SCRATCH_INT0_STATUS			0x19
#define REG_B0_SCRATCH_INT1_STATUS			0x20

#define REG_MEM_ADDR_SEL_0				0x70

#define REG_MEM_R_W                                     0x74
#define REG_FIFO_INDEX                                  0x75
#define REG_FIFO_COUNTH                                 0x76
#define REG_FIFO_R_W                                    0x78

#define REG_FLASH_ERASE					0x7C
#define BIT_FLASH_ERASE_MASS_EN				0x80

#define REG_IDLE_STATUS                                 0x7D
#define BIT_FLASH_IDLE                                             0x01
#define BIT_FLASH_LOAD_DONE                                        0x02

#define REG_MOD_CTRL2					0x8A
#define BIT_FIFO_EMPTY_IND_DIS				0x08

#define REG_FIFO_3_SIZE                                 0xD4
#define REG_FIFO_2_SIZE                                 0xD5
#define REG_FIFO_1_SIZE                                 0xD6
#define REG_FIFO_0_SIZE                                 0xD7

#define REG_FIFO_3_PKT_SIZE                             0xE4
#define REG_FIFO_2_PKT_SIZE                             0xE5
#define REG_FIFO_1_PKT_SIZE                             0xE6
#define REG_FIFO_0_PKT_SIZE                             0xE7
#define REG_PKT_SIZE_OVERRIDE                           0xE8

#define REG_FIFO_CFG                                    0xE9
#define REG_FIFO_RST                                    0xEA
#define REG_FIFO_MODE                                   0xEB

#define REG_FLASH_CFG                                   0xFC
#define BIT_FLASH_IFM_DIS                                          0x02
#define BIT_FLASH_CACHE_BYPASS                                     0x10

#define REG_BANK_SEL                                    0xFF

/* bank 1 */
#define REG_PRGRM_STRT_ADDR_DRDY_0                      0
#define REG_PRGRM_STRT_ADDR_TIMER_0                     3
#define REG_PRGRM_STRT_ADDR_DEMAND_0                    6

#define REG_B1_SCRATCH_INT				0x26
#define BIT_SCRATCH_INT_0				0x01
#define BIT_SCRATCH_INT_1				0x02
#define BIT_SCRATCH_INT_2				0x04
#define BIT_SCRATCH_INT_3				0x08
#define BIT_SCRATCH_INT_4				0x10
#define BIT_SCRATCH_INT_5				0x20
#define BIT_SCRATCH_INT_6				0x40
#define BIT_SCRATCH_INT_7				0x80

#define IIO_BUFFER_SIZE                                 1
#define DMP_IMAGE_SIZE                                  32652
#define BANK0_BASE_ADDR                                 0x50000000
#define FLASH_START_ADDR                                0
#define SRAM_START_ADDR                                 0x20000000
#define MPU_MEM_BANK_SIZE                               16
#define MAX_FLASH_PAGE_ADDRESS                          0x1F

#define GARNET_DMA_CH_0_START_ADDR 0x40002000
#define GARNET_DMA_CHANNEL_ADDRESS_OFFSET 0x20

#define GARNET_DMA_INTERRUPT_REGISTER 0x40002100

#define GARNET_DMA_SOURCE_ADDRESS_OFFSET                0x00
#define GARNET_DMA_DEST_ADDRESS_OFFSET                  0x04
#define GARNET_DMA_TRANSFER_COUNT_OFFSET                0x0C

#define GARNET_DMA_CONTROL_REGISTER_BYTE_0_OFFSET 0x08
#define GARNET_DMA_CONTROL_REGISTER_BYTE_0_WORD_SIZE_BITS 0x02
#define GARNET_DMA_CONTROL_REGISTER_BYTE_1_OFFSET 0x09
#define GARNET_DMA_CONTROL_REGISTER_BYTE_1_MAX_BURST_BITS 0x00
#define GARNET_DMA_CONTROL_REGISTER_BYTE_2_OFFSET 0x0A
#define GARNET_DMA_CONTROL_REGISTER_BYTE_2_START_BIT 0x02
#define GARNET_DMA_CONTROL_REGISTER_BYTE_2_TYPE_BITS 0x00
#define GARNET_DMA_CONTROL_REGISTER_BYTE_2_CHG_BIT 0x04
#define GARNET_DMA_CONTROL_REGISTER_BYTE_2_STRT_BIT 0x02

#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_OFFSET 0x0B
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_INT_BIT 0x01
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_TC_BIT 0x02
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_SINC_BIT 0x04
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_SDEC_BIT 0x08
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_DINC_BIT 0x20
#define GARNET_DMA_CONTROL_REGISTER_BYTE_3_DDEC_BIT 0x40

#define GARNET_PRGRM_STRT_ADDR_DRDY_0_B1 0x00
#define GARNET_PRGRM_STRT_ADDR_TIMER_0_B1 0x03
#define GARNET_PRGRM_STRT_ADDR_DEMAND_0_B1 0x06

#define SRAM_SHARED_MEMROY_START_ADDR   (SRAM_START_ADDR + 0xE000)

#define INV_FIFO_SIZE_VAL(mult, factor)		(((mult) << 5) | (factor))
#define INV_COMPUTE_FIFO_SIZE(mult, factor, packet)			\
		((1 << ((mult) & 0x7)) * (((factor) & 0x1F) + 1) * ((packet) + 1))

/*
 * FIFO 0: commands
 * packet: 1 byte
 * size: 128 bytes
 */
#define INV_FIFO_CMD_ID				0x01
#define INV_FIFO_CMD_INDEX			0
#define INV_FIFO_CMD_REG_PACKET			REG_FIFO_0_PKT_SIZE
#define INV_FIFO_CMD_REG_SIZE			REG_FIFO_0_SIZE
#define INV_FIFO_CMD_PACKET			0
#define INV_FIFO_CMD_MULT			0x2
#define INV_FIFO_CMD_FACTOR			0x1F
#define INV_FIFO_CMD_SIZE_VAL						\
	INV_FIFO_SIZE_VAL(INV_FIFO_CMD_MULT,				\
			  INV_FIFO_CMD_FACTOR)
#define INV_FIFO_CMD_SIZE						\
		INV_COMPUTE_FIFO_SIZE(INV_FIFO_CMD_MULT,		\
				      INV_FIFO_CMD_FACTOR,		\
				      INV_FIFO_CMD_PACKET)

/*
 * FIFO 1: normal sensors data
 * FIFO 2: wake-up sensors data
 * packet: 1 byte
 * size: 4096 bytes
 */
#define INV_FIFO_DATA_NB			2
#define INV_FIFO_DATA_NORMAL_ID			0x02
#define INV_FIFO_DATA_NORMAL_INDEX		1
#define INV_FIFO_DATA_NORMAL_REG_PACKET		REG_FIFO_1_PKT_SIZE
#define INV_FIFO_DATA_NORMAL_REG_SIZE		REG_FIFO_1_SIZE
#define INV_FIFO_DATA_WAKEUP_ID			0x04
#define INV_FIFO_DATA_WAKEUP_INDEX		2
#define INV_FIFO_DATA_WAKEUP_REG_PACKET		REG_FIFO_2_PKT_SIZE
#define INV_FIFO_DATA_WAKEUP_REG_SIZE		REG_FIFO_2_SIZE
#define INV_FIFO_DATA_PACKET			0
#define INV_FIFO_DATA_MULT			0x7
#define INV_FIFO_DATA_FACTOR			0x1F
#define INV_FIFO_DATA_SIZE_VAL						\
		INV_FIFO_SIZE_VAL(INV_FIFO_DATA_MULT,			\
				  INV_FIFO_DATA_FACTOR)
#define INV_FIFO_DATA_SIZE						\
		INV_COMPUTE_FIFO_SIZE(INV_FIFO_DATA_MULT,		\
				      INV_FIFO_DATA_FACTOR,		\
				      INV_FIFO_DATA_PACKET)

#define INV_FIFO_IDS			(INV_FIFO_CMD_ID |		\
					 INV_FIFO_DATA_WAKEUP_ID |	\
					 INV_FIFO_DATA_NORMAL_ID)

/* device enum */
enum inv_devices {
	ICM30628,
	SENSORHUB_V4,
	INV_NUM_PARTS
};

/**
 *  struct inv_hw_s - Other important hardware information.
 *  @num_reg:	Number of registers on device.
 *  @name:      name of the chip
 */
struct inv_hw_s {
	u8 num_reg;
	u8 *name;
};

struct inv_fifo {
	uint8_t index;
	uint8_t *buffer;
	size_t size;
	size_t count;
};

/**
 *  struct inv_mpu_state - Driver state variables.
 *  @dev:               device structure.
 *  @trig:		iio trigger.
 *  @hw:		Other hardware-specific information.
 *  @chip_type:		chip type.
 *  @fifo_length:	length of data fifo.
 *  @lock:		mutex lock.
 *  @client:		i2c client handle.
 *  @plat_data:		platform data.
 *  @irq:               irq number store.
 *  @timestamp:		last irq timestamp.
 *  @datafifos:		data fifos
 *  @data_enable:	iio device enable state.
 *  @sensors_list:	list of available sensors
 *  @firmware_loaded:   flag indicats firmware loaded.
 *  @firmware:          pointer to the firmware memory allocation.
 *  @bank:              current bank information.
 *  @fifo_index:	current FIFO index.
.*  @wake_lock:         Android wake_lock.
 *  @power_on:          power on function.
 *  @power_off:         power off function.
 */
struct inv_mpu_state {
	struct device *dev;
	struct iio_trigger *trig;
	const struct inv_hw_s *hw;
	enum inv_devices chip_type;
	size_t fifo_length;
	struct mutex lock;
	struct mutex lock_cmd;
	struct mpu_platform_data plat_data;
	int irq;
	ktime_t timestamp;
	struct inv_fifo datafifos[INV_FIFO_DATA_NB];
	atomic_t data_enable;
	struct list_head sensors_list;
	struct inv_sh_timesync timesync;
	bool firmware_loaded;
	u8 *firmware;
	u8 bank;
	u8 fifo_index;
	struct wake_lock wake_lock;
	int (*power_on)(const struct inv_mpu_state *);
	int (*power_off)(const struct inv_mpu_state *);
	int (*write)(const struct inv_mpu_state *, u8 reg,
			u16 len, const u8 *data);
	int (*read)(const struct inv_mpu_state *, u8 reg,
			u16 len, u8 *data);
};

/* IIO attribute address */
enum MPU_IIO_ATTR_ADDR {
	ATTR_CMD = 1,
	ATTR_RESET,
	ATTR_REG_DUMP,
	ATTR_MATRIX_ACCEL,
	ATTR_MATRIX_MAGN,
	ATTR_MATRIX_GYRO,
};

static inline int inv_set_power_on(const struct inv_mpu_state *st)
{
	int ret = 0;

	if (st->power_on)
		ret = st->power_on(st);

	return ret;
}

static inline int inv_set_power_off(const struct inv_mpu_state *st)
{
	int ret = 0;

	if (st->power_off)
		ret = st->power_off(st);

	return ret;
}

static inline int inv_plat_write(const struct inv_mpu_state *st, u8 reg,
					u16 len, const u8 *data)
{
	int ret = -1;

	if (st->write)
		ret = st->write(st, reg, len, data);

	return ret;
}

static inline int inv_plat_read(const struct inv_mpu_state *st, u8 reg,
					u16 len, u8 *data)
{
	int ret = -1;

	if (st->read)
		ret = st->read(st, reg, len, data);

	return ret;
}

static inline int inv_plat_single_write(const struct inv_mpu_state *st,
					u8 reg, u8 data)
{
	return inv_plat_write(st, reg, 1, &data);
}

static inline int inv_plat_single_read(const struct inv_mpu_state *st,
					u8 reg, u8 *data)
{
	return inv_plat_read(st, reg, 1, data);
}

static inline int inv_mem_reg_write(const struct inv_mpu_state *st, u32 mem)
{
	u32 data = cpu_to_be32(mem);

	return inv_plat_write(st, REG_MEM_ADDR_SEL_0, sizeof(data), (u8 *)&data);
}

static inline int mpu_memory_write(const struct inv_mpu_state *st, u32 mem_addr,
					u16 len, const u8 *data)
{
	int ret;

	ret = inv_mem_reg_write(st, mem_addr);
	if (ret)
		return ret;

	return inv_plat_write(st, REG_MEM_R_W, len, data);
}

static inline int mpu_memory_read(const struct inv_mpu_state *st, u32 mem_addr,
					u16 len, u8 *data)
{
	int ret;

	ret = inv_mem_reg_write(st, mem_addr);
	if (ret)
		return ret;

	return inv_plat_read(st, REG_MEM_R_W, len, data);
}

int inv_mpu_configure_ring(struct iio_dev *indio_dev);
int inv_mpu_probe_trigger(struct iio_dev *indio_dev);
void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev);
void inv_mpu_remove_trigger(struct iio_dev *indio_dev);
int inv_set_bank(struct inv_mpu_state *st, u8 bank);
int inv_set_fifo_index(struct inv_mpu_state *st, u8 fifo_index);
int inv_check_chip_type(struct iio_dev *indio_dev, int chip);
int inv_fifo_config(struct inv_mpu_state *st);
int inv_soft_reset(struct inv_mpu_state *st);
int inv_firmware_load(struct inv_mpu_state *st);
int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf);
int inv_create_dmp_sysfs(struct iio_dev *ind);
int inv_send_command_down(struct inv_mpu_state *st, const u8 *data, int len);
void inv_wake_start(const struct inv_mpu_state *st);
void inv_wake_stop(const struct inv_mpu_state *st);
void inv_init_power(struct inv_mpu_state *st);
int inv_proto_set_power(struct inv_mpu_state *st, bool power_on);

#endif  /* #ifndef _INV_MPU_IIO_H_ */

