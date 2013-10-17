/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_BIF_CONSUMER_H_
#define _LINUX_BIF_CONSUMER_H_

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

#define BIF_DEVICE_ID_BYTE_LENGTH	8
#define BIF_UNIQUE_ID_BYTE_LENGTH	10
#define BIF_UNIQUE_ID_BIT_LENGTH	80

#define BIF_PRIMARY_SLAVE_DEV_ADR	0x01

/**
 * enum bif_transaction - BIF master bus transaction types
 * %BIF_TRANS_WD:	Write data
 * %BIF_TRANS_ERA:	Extended register address
 * %BIF_TRANS_WRA:	Write register address
 * %BIF_TRANS_RRA:	Read register address
 * %BIF_TRANS_BC:	Bus command
 * %BIF_TRANS_EDA:	Extended device address
 * %BIF_TRANS_SDA:	Slave device address
 *
 * These values correspond to BIF word bits: BCF, bit 9, bit 8.
 * BCF_n bit is inserted automatically.
 */
enum bif_transaction {
	BIF_TRANS_WD	= 0x00,
	BIF_TRANS_ERA	= 0x01,
	BIF_TRANS_WRA	= 0x02,
	BIF_TRANS_RRA	= 0x03,
	BIF_TRANS_BC	= 0x04,
	BIF_TRANS_EDA	= 0x05,
	BIF_TRANS_SDA	= 0x06,
};

/* BIF slave response components */
#define BIF_SLAVE_RD_ACK		0x200
#define BIF_SLAVE_RD_EOT		0x100
#define BIF_SLAVE_RD_DATA		0x0FF
#define BIF_SLAVE_RD_ERR		0x0FF
#define BIF_SLAVE_TACK_ACK		0x200
#define BIF_SLAVE_TACK_WCNT		0x0FF
#define BIF_SLAVE_TACK_ERR		0x0FF

/**
 * enum bif_bus_command - MIPI defined bus commands to use in BC transaction
 * %BIF_CMD_BRES:	Bus reset of all slaves
 * %BIF_CMD_PDWN:	Put all slaves into power down mode
 * %BIF_CMD_STBY:	Put all slaves into standby mode
 * %BIF_CMD_EINT:	Enable interrupts for all slaves
 * %BIF_CMD_ISTS:	Poll interrupt status for all slaves.  Expects BQ
 *			response if any slave has a pending interrupt.
 * %BIF_CMD_RBL:	Specify the burst read length for the next read
 *			transaction.  Bits 3 to 0 should also be ORed on in
 *			order to specify the number of bytes to read.
 * %BIF_CMD_RBE:	Specify the extended burst read length for the next read
 *			transaction.  Bits 3 to 0 should also be ORed on in
 *			order to specify the number of bytes to read.  The burst
 *			read length for RBEy and RBLx = 16 * y + x.
 * %BIF_CMD_DASM:	Device activation stick mode.  This keeps a slave
 *			selected if it would otherwise become unselected by the
 *			next transaction.
 * %BIF_CMD_DISS:	UID search start
 * %BIF_CMD_DILC:	UID length check.  Expects BQ response if all 80 UID
 *			bits for a given slave have been entered.
 * %BIF_CMD_DIE0:	UID search enter 0
 * %BIF_CMD_DIE1:	UID search enter 1
 * %BIF_CMD_DIP0:	UID search probe 0
 * %BIF_CMD_DIP1:	UID search probe 1
 * %BIF_CMD_DRES:	Device reset of selected slaves
 * %BIF_CMD_TQ:		Transaction query; expects TACK response
 * %BIF_CMD_AIO:	Address increment off for the next transaction
 *
 * These values correspond to BIF word bits 7 to 0.
 */
enum bif_bus_command {
	BIF_CMD_BRES	= 0x00,
	BIF_CMD_PDWN	= 0x02,
	BIF_CMD_STBY	= 0x03,
	BIF_CMD_EINT	= 0x10,
	BIF_CMD_ISTS	= 0x11,
	BIF_CMD_RBL	= 0x20,
	BIF_CMD_RBE	= 0x30,
	BIF_CMD_DASM	= 0x40,
	BIF_CMD_DISS	= 0x80,
	BIF_CMD_DILC	= 0x81,
	BIF_CMD_DIE0	= 0x84,
	BIF_CMD_DIE1	= 0x85,
	BIF_CMD_DIP0	= 0x86,
	BIF_CMD_DIP1	= 0x87,
	BIF_CMD_DRES	= 0xC0,
	BIF_CMD_TQ	= 0xC2,
	BIF_CMD_AIO	= 0xC4,
};

/**
 * struct bif_ddb_l1_data - MIPI defined L1 DDB data structure
 * @revision:		DDB version; should be 0x10 for DDB v1.0
 * @level:		DDB level support; should be 0x03 for DDB L1 and L2
 * @device_class:	MIPI device class; should be 0x0800
 * @manufacturer_id:	Manufacturer ID number allocated by MIPI
 * @product_id:		Manufacturer specified product ID number
 * @length:		Size of L2 function directory in bytes
 */
struct bif_ddb_l1_data {
	u8	revision;
	u8	level;
	u16	device_class;
	u16	manufacturer_id;
	u16	product_id;
	u16	length;
};

/**
 * struct bif_ddb_l2_data - MIPI defined L2 DDB function data structure
 * @function_type:	Defines the type of the function.  The type may be
 *			either MIPI or manufacturer defined.
 * @function_version:	Defines the version of the function.  The version may
 *			be either MIPI or manufacturer defined.
 * @function_pointer:	Address in BIF slave memory where the register map for
 *			the function begins.
 */
struct bif_ddb_l2_data {
	u8	function_type;
	u8	function_version;
	u16	function_pointer;
};

/**
 * enum bif_mipi_function_type - MIPI defined DDB L2 function types
 * %BIF_FUNC_PROTOCOL:		Protocol function which provides access to core
 *				BIF communication features.
 * %BIF_FUNC_SLAVE_CONTROL:	Slave control function which provides control
 *				for BIF slave interrupts and tasks.
 * %BIF_FUNC_TEMPERATURE:	Temperature sensor function which provides a
 *				means to accurately read the battery temperature
 *				in a single-shot or periodic fashion.
 * %BIF_FUNC_NVM:		Non-volatile memory function which provides a
 *				means to store data onto a BIF slave that is
 *				non-volatile.  Secondary slave objects are also
 *				found through the NVM function.
 * %BIF_FUNC_AUTHENTICATION:	Authentication function which provides a means
 *				to authenticate batteries.  This function does
 *				not have a MIPI defined implimentation.  Instead
 *				all aspects of the authentication function are
 *				left to the discretion of the manufacturer.
 */
enum bif_mipi_function_type {
	BIF_FUNC_PROTOCOL	= 0x01,
	BIF_FUNC_SLAVE_CONTROL	= 0x02,
	BIF_FUNC_TEMPERATURE	= 0x03,
	BIF_FUNC_NVM		= 0x04,
	BIF_FUNC_AUTHENTICATION	= 0x05,
};

#define BIF_DDB_L1_BASE_ADDR	0x0000
#define BIF_DDB_L2_BASE_ADDR	0x000A

/**
 * enum bif_slave_error_code - MIPI defined BIF slave error codes
 * %BIF_ERR_NONE:		No error occurred
 * %BIF_ERR_GENERAL:		An unenumerated error occurred
 * %BIF_ERR_PARITY:		A Hamming-15 parity check failed for a word
 *				sent on the bus
 * %BIF_ERR_INVERSION:		More than 8 bits in a word were 1
 * %BIF_ERR_BAD_LENGTH:		Word had more or less than 17 bits
 * %BIF_ERR_TIMING:		Bit timing was violated in a word
 * %BIF_ERR_UNKNOWN_CMD:	Bus command was unknown to the slave
 * %BIF_ERR_CMD_SEQ:		Commands with ordering dependency were not
 *				sent in the right order
 * %BIF_ERR_BUS_COLLISION:	BCL was already low at the beginning of a new
 *				transaction
 * %BIF_ERR_SLAVE_BUSY:		Slave is busy and cannot respond
 * %BIF_ERR_FATAL:		Slave is in an unrecoverable error state and
 *				must be reset
 *
 * These values are present in the ERR portion of an RD or TACK slave response
 * word.  These values can also be found in the ERR_CODE register of the
 * protocol function.
 */
enum bif_slave_error_code {
	BIF_ERR_NONE		= 0x00,
	BIF_ERR_GENERAL		= 0x10,
	BIF_ERR_PARITY		= 0x11,
	BIF_ERR_INVERSION	= 0x12,
	BIF_ERR_BAD_LENGTH	= 0x13,
	BIF_ERR_TIMING		= 0x14,
	BIF_ERR_UNKNOWN_CMD	= 0x15,
	BIF_ERR_CMD_SEQ		= 0x16,
	BIF_ERR_BUS_COLLISION	= 0x1F,
	BIF_ERR_SLAVE_BUSY	= 0x20,
	BIF_ERR_FATAL		= 0x7F,
};

/**
 * struct bif_protocol_function - constant data present in protocol function
 * @l2_entry:		Pointer to protocol function L2 DDB data struct
 * @protocol_pointer:	BIF slave address where protocol registers begin
 * @device_id_pointer:	BIF slave address where device ID begins
 * @device_id:		The 8-byte unique device ID in MSB to LSB order
 */
struct bif_protocol_function {
	struct bif_ddb_l2_data *l2_entry;
	u16	protocol_pointer;
	u16	device_id_pointer;
	u8	device_id[BIF_DEVICE_ID_BYTE_LENGTH]; /* Unique ID */
};

#define PROTOCOL_FUNC_DEV_ADR_ADDR(protocol_pointer)	((protocol_pointer) + 0)
#define PROTOCOL_FUNC_ERR_CODE_ADDR(protocol_pointer)	((protocol_pointer) + 2)
#define PROTOCOL_FUNC_ERR_CNT_ADDR(protocol_pointer)	((protocol_pointer) + 3)
#define PROTOCOL_FUNC_WORD_CNT_ADDR(protocol_pointer)	((protocol_pointer) + 4)

/**
 * struct bif_slave_control_function - constant data present in slave control
 *			function as well internal software state parameters
 * @l2_entry:		Pointer to slave control function L2 DDB data struct
 * @slave_ctrl_pointer:	BIF slave address where slave control registers begin
 * @task_count:		Number of tasks supported by the slave
 * @irq_notifier_list:	List of notifiers for consumers drivers that wish to be
 *			notified when any given interrupt triggers.  This list
 *			is dynamically allocated with length task_count.
 */
struct bif_slave_control_function {
	struct bif_ddb_l2_data		*l2_entry;
	u16				slave_ctrl_pointer;
	unsigned int			task_count;
	struct blocking_notifier_head	*irq_notifier_list;
};

#define SLAVE_CTRL_TASKS_PER_SET	8

/**
 * bif_slave_control_task_is_valid() - returns true if the specified task
 *		is supported by the slave or false if it isn't
 * @func:	Pointer to slave's slave control function structure
 * @task:	Slave task number to check
 */
static inline bool
bif_slave_control_task_is_valid(struct bif_slave_control_function *func,
				unsigned int task)
{
	return func ? task < func->task_count : false;
}

#define SLAVE_CTRL_FUNC_IRQ_EN_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 0)

#define SLAVE_CTRL_FUNC_IRQ_STATUS_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 1)
#define SLAVE_CTRL_FUNC_IRQ_CLEAR_ADDR(slave_ctrl_pointer, task) \
	SLAVE_CTRL_FUNC_IRQ_STATUS_ADDR(slave_ctrl_pointer, task)

#define SLAVE_CTRL_FUNC_TASK_TRIGGER_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 2)
#define SLAVE_CTRL_FUNC_TASK_BUSY_ADDR(slave_ctrl_pointer, task) \
	SLAVE_CTRL_FUNC_TASK_TRIGGER_ADDR(slave_ctrl_pointer, task)

#define SLAVE_CTRL_FUNC_TASK_AUTO_TRIGGER_ADDR(slave_ctrl_pointer, task) \
	((slave_ctrl_pointer) + 4 * ((task) / SLAVE_CTRL_TASKS_PER_SET) + 3)

/**
 * struct bif_temperature_function - constant data present in temperature
 *				sensor function
 * @temperatuer_pointer:	BIF slave address where temperature sensor
 *				control registers begin
 * @slave_control_channel:	Slave control channel associated with the
 *				temperature sensor function.  This channel is
 *				also the task number.
 * @accuracy_pointer:		BIF slave address where temperature accuracy
 *				registers begin
 */
struct bif_temperature_function {
	u16	temperature_pointer;
	u8	slave_control_channel;
	u16	accuracy_pointer;
};

/**
 * enum bif_mipi_object_type - MIPI defined BIF object types
 * %BIF_OBJ_END_OF_LIST:	Indicates that the end of the object list in
 *				NVM has been reached
 * %BIF_OBJ_SEC_SLAVE:		Specifies the UIDs of secondary slaves found
 *				inside of the battery pack
 * %BIF_OBJ_BATT_PARAM:		Specifies some variety of battery parameter.
 *				There is no MIPI defined format for this object
 *				type so parsing is manufacturer specific.
 */
enum bif_mipi_object_type {
	BIF_OBJ_END_OF_LIST	= 0x00,
	BIF_OBJ_SEC_SLAVE	= 0x01,
	BIF_OBJ_BATT_PARAM	= 0x02,
};

/**
 * struct bif_object - contains all header and data information for a slave
 *			data object
 * @type:		Object type
 * @version:		Object version
 * @manufacturer_id:	Manufacturer ID number allocated by MIPI
 * @length:		Length of the entire object including header and CRC
 * @data:		Raw byte data found in the object
 * @crc:		CRC of the object calculated using CRC-CCITT
 * @list:		Linked-list connection parameter
 * @addr:		BIF slave address correspond to the start of the object
 *
 * manufacturer_id == 0x0000 if MIPI type and version.
 */
struct bif_object {
	u8			type;
	u8			version;
	u16			manufacturer_id;
	u16			length;
	u8			*data;
	u16			crc;
	struct list_head	list;
	u16			addr;
};

/**
 * struct bif_nvm_function - constant data present in non-volatile memory
 *				function as well internal software state
 *				parameters
 * @nvm_pointer:		BIF slave address where NVM registers begin
 * @slave_control_channel:	Slave control channel associated with the
 *				NVM function.  This channel is also the task
 *				number.
 * @write_buffer_size:		Size in bytes of the NVM write buffer.  0x00
 *				is used to denote a 256 byte buffer.
 * @nvm_base_address:		BIF slave address where NVM begins
 * @nvm_size:			NVM size in bytes
 * @object_count:		Number of BIF objects read from NVM
 * @object_list:		List of BIF objects read from NVM
 */
struct bif_nvm_function {
	u16			nvm_pointer;
	u8			slave_control_channel;
	u8			write_buffer_size;
	u16			nvm_base_address;
	u16			nvm_size;
	int			object_count;
	struct list_head	object_list;
};

/**
 * struct bif_ctrl - Opaque handle for a BIF controller to be used in bus
 *			oriented BIF function calls.
 */
struct bif_ctrl;

/**
 * struct bif_slave - Opaque handle for a BIF slave to be used in slave oriented
 *			BIF function calls.
 */
struct bif_slave;

/**
 * enum bif_bus_state - indicates the current or desired state of the BIF bus
 * %BIF_BUS_STATE_MASTER_DISABLED:	BIF host hardware is disabled
 * %BIF_BUS_STATE_POWER_DOWN:		BIF bus is in power down state and
 *					BCL is not being pulled high
 * %BIF_BUS_STATE_STANDBY:		BIF slaves are in standby state in which
 *					less power is drawn
 * %BIF_BUS_STATE_ACTIVE:		BIF slaves are ready for immediate
 *					communications
 * %BIF_BUS_STATE_INTERRUPT:		BIF bus is active, but no communication
 *					is possible.  Instead, either one of the
 *					slaves or the master must transition to
 *					active state by pulling BCL low for 1
 *					tau bif period.
 */
enum bif_bus_state {
	BIF_BUS_STATE_MASTER_DISABLED,
	BIF_BUS_STATE_POWER_DOWN,
	BIF_BUS_STATE_STANDBY,
	BIF_BUS_STATE_ACTIVE,
	BIF_BUS_STATE_INTERRUPT,
};

/**
 * enum bif_bus_event - events that the BIF framework may send to BIF consumers
 * %BIF_BUS_EVENT_BATTERY_INSERTED:	Indicates that a battery was just
 *					inserted physically or that the BIF
 *					host controller for the battery just
 *					probed and a battery was already
 *					present.
 * %BIF_BUS_EVENT_BATTERY_REMOVED:	Indicates that a battery was just
 *					removed and thus its slaves are no
 *					longer accessible.
 */
enum bif_bus_event {
	BIF_BUS_EVENT_BATTERY_INSERTED,
	BIF_BUS_EVENT_BATTERY_REMOVED,
};

/* Mask values to be ORed together for use in bif_match_criteria.match_mask. */
#define BIF_MATCH_MANUFACTURER_ID	BIT(0)
#define BIF_MATCH_PRODUCT_ID		BIT(1)
#define BIF_MATCH_FUNCTION_TYPE		BIT(2)
#define BIF_MATCH_FUNCTION_VERSION	BIT(3)
#define BIF_MATCH_IGNORE_PRESENCE	BIT(4)

/**
 * struct bif_match_criteria - specifies the matching criteria that a BIF
 *			consumer uses to find an appropriate BIF slave
 * @match_mask:		Mask value specifying which parameters to match upon.
 *			This value should be some ORed combination of
 *			BIF_MATCH_* specified above.
 * @manufacturer_id:	Manufacturer ID number allocated by MIPI
 * @product_id:		Manufacturer specified product ID number
 * @function_type:	Defines the type of the function.  The type may be
 *			either MIPI or manufacturer defined.
 * @function_version:	Defines the version of the function.  The version may
 *			be either MIPI or manufacturer defined.
 * @ignore_presence:	If true, then slaves that are currently not present
 *			will be successfully matched against.  By default, only
 *			present slaves can be matched.
 */
struct bif_match_criteria {
	u32	match_mask;
	u16	manufacturer_id;
	u16	product_id;
	u8	function_type;
	u8	function_version;
	bool	ignore_presence;
};

/**
 * bif_battery_rid_ranges - MIPI-BIF defined Rid battery pack resistance ranges
 * %BIF_BATT_RID_SPECIAL1_MIN:	Minimum Rid for special case 1
 * %BIF_BATT_RID_SPECIAL1_MAX:	Maximum Rid for special case 1
 * %BIF_BATT_RID_SPECIAL2_MIN:	Minimum Rid for special case 2
 * %BIF_BATT_RID_SPECIAL2_MAX:	Maximum Rid for special case 2
 * %BIF_BATT_RID_SPECIAL3_MIN:	Minimum Rid for special case 3
 * %BIF_BATT_RID_SPECIAL3_MAX:	Maximum Rid for special case 3
 * %BIF_BATT_RID_LOW_COST_MIN:	Minimum Rid for a low cost battery pack
 * %BIF_BATT_RID_LOW_COST_MAX:	Maximum Rid for a low cost battery pack
 * %BIF_BATT_RID_SMART_MIN:	Minimum Rid for a smart battery pack
 * %BIF_BATT_RID_SMART_MAX:	Maximum Rid for a smart battery pack
 */
enum bif_battery_rid_ranges {
	BIF_BATT_RID_SPECIAL1_MIN	= 0,
	BIF_BATT_RID_SPECIAL1_MAX	= 1,
	BIF_BATT_RID_SPECIAL2_MIN	= 7350,
	BIF_BATT_RID_SPECIAL2_MAX	= 7650,
	BIF_BATT_RID_SPECIAL3_MIN	= 12740,
	BIF_BATT_RID_SPECIAL3_MAX	= 13260,
	BIF_BATT_RID_LOW_COST_MIN	= 19600,
	BIF_BATT_RID_LOW_COST_MAX	= 140000,
	BIF_BATT_RID_SMART_MIN		= 240000,
	BIF_BATT_RID_SMART_MAX		= 450000,
};

#ifdef CONFIG_BIF

int bif_request_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb);
int bif_free_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb);

int bif_trigger_task(struct bif_slave *slave, unsigned int task);
int bif_task_is_busy(struct bif_slave *slave, unsigned int task);

int bif_ctrl_count(void);
struct bif_ctrl *bif_ctrl_get_by_id(unsigned int id);
struct bif_ctrl *bif_ctrl_get(struct device *consumer_dev);
void bif_ctrl_put(struct bif_ctrl *ctrl);

int bif_ctrl_signal_battery_changed(struct bif_ctrl *ctrl);

int bif_slave_match_count(const struct bif_ctrl *ctrl,
			const struct bif_match_criteria *match_criteria);

struct bif_slave *bif_slave_match_get(const struct bif_ctrl *ctrl,
	unsigned int id, const struct bif_match_criteria *match_criteria);

void bif_slave_put(struct bif_slave *slave);

int bif_ctrl_notifier_register(struct bif_ctrl *ctrl,
				struct notifier_block *nb);

int bif_ctrl_notifier_unregister(struct bif_ctrl *ctrl,
				struct notifier_block *nb);

struct bif_ctrl *bif_get_ctrl_handle(struct bif_slave *slave);

int bif_slave_find_function(struct bif_slave *slave, u8 function, u8 *version,
				u16 *function_pointer);

int bif_slave_read(struct bif_slave *slave, u16 addr, u8 *buf, int len);
int bif_slave_write(struct bif_slave *slave, u16 addr, u8 *buf, int len);

int bif_slave_is_present(struct bif_slave *slave);

int bif_slave_is_selected(struct bif_slave *slave);
int bif_slave_select(struct bif_slave *slave);

int bif_ctrl_raw_transaction(struct bif_ctrl *ctrl, int transaction, u8 data);
int bif_ctrl_raw_transaction_read(struct bif_ctrl *ctrl, int transaction,
					u8 data, int *response);
int bif_ctrl_raw_transaction_query(struct bif_ctrl *ctrl, int transaction,
		u8 data, bool *query_response);

void bif_ctrl_bus_lock(struct bif_ctrl *ctrl);
void bif_ctrl_bus_unlock(struct bif_ctrl *ctrl);

u16 bif_crc_ccitt(const u8 *buffer, unsigned int len);

int bif_ctrl_measure_rid(struct bif_ctrl *ctrl);
int bif_ctrl_get_bus_period(struct bif_ctrl *ctrl);
int bif_ctrl_set_bus_period(struct bif_ctrl *ctrl, int period_ns);
int bif_ctrl_get_bus_state(struct bif_ctrl *ctrl);
int bif_ctrl_set_bus_state(struct bif_ctrl *ctrl, enum bif_bus_state state);

#else

static inline int bif_request_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb) { return -EPERM; }
static inline int bif_free_irq(struct bif_slave *slave, unsigned int task,
			struct notifier_block *nb) { return -EPERM; }

static inline int bif_trigger_task(struct bif_slave *slave, unsigned int task)
{ return -EPERM; }
static inline int bif_task_is_busy(struct bif_slave *slave, unsigned int task)
{ return -EPERM; }

static inline int bif_ctrl_count(void) { return -EPERM; }
static inline struct bif_ctrl *bif_ctrl_get_by_id(unsigned int id)
{ return ERR_PTR(-EPERM); }
struct bif_ctrl *bif_ctrl_get(struct device *consumer_dev)
{ return ERR_PTR(-EPERM); }
static inline void bif_ctrl_put(struct bif_ctrl *ctrl) { return; }

static inline int bif_ctrl_signal_battery_changed(struct bif_ctrl *ctrl)
{ return -EPERM; }

static inline int bif_slave_match_count(const struct bif_ctrl *ctrl,
			const struct bif_match_criteria *match_criteria)
{ return -EPERM; }

static inline struct bif_slave *bif_slave_match_get(const struct bif_ctrl *ctrl,
	unsigned int id, const struct bif_match_criteria *match_criteria)
{ return ERR_PTR(-EPERM); }

static inline void bif_slave_put(struct bif_slave *slave) { return; }

static inline int bif_ctrl_notifier_register(struct bif_ctrl *ctrl,
				struct notifier_block *nb)
{ return -EPERM; }

static inline int bif_ctrl_notifier_unregister(struct bif_ctrl *ctrl,
				struct notifier_block *nb)
{ return -EPERM; }

static inline struct bif_ctrl *bif_get_ctrl_handle(struct bif_slave *slave)
{ return ERR_PTR(-EPERM); }

static inline int bif_slave_find_function(struct bif_slave *slave, u8 function,
				u8 *version, u16 *function_pointer)
{ return -EPERM; }

static inline int bif_slave_read(struct bif_slave *slave, u16 addr, u8 *buf,
				int len)
{ return -EPERM; }
static inline int bif_slave_write(struct bif_slave *slave, u16 addr, u8 *buf,
				int len)
{ return -EPERM; }

static inline int bif_slave_is_present(struct bif_slave *slave)
{ return -EPERM; }

static inline int bif_slave_is_selected(struct bif_slave *slave)
{ return -EPERM; }
static inline int bif_slave_select(struct bif_slave *slave)
{ return -EPERM; }

static inline int bif_ctrl_raw_transaction(struct bif_ctrl *ctrl,
				int transaction, u8 data)
{ return -EPERM; }
static inline int bif_ctrl_raw_transaction_read(struct bif_ctrl *ctrl,
				int transaction, u8 data, int *response)
{ return -EPERM; }
static inline int bif_ctrl_raw_transaction_query(struct bif_ctrl *ctrl,
				int transaction, u8 data, bool *query_response)
{ return -EPERM; }

static inline void bif_ctrl_bus_lock(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline void bif_ctrl_bus_unlock(struct bif_ctrl *ctrl)
{ return -EPERM; }

static inline u16 bif_crc_ccitt(const u8 *buffer, unsigned int len)
{ return 0; }

static inline int bif_ctrl_measure_rid(struct bif_ctrl *ctrl) { return -EPERM; }
static inline int bif_ctrl_get_bus_period(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline int bif_ctrl_set_bus_period(struct bif_ctrl *ctrl, int period_ns)
{ return -EPERM; }
static inline int bif_ctrl_get_bus_state(struct bif_ctrl *ctrl)
{ return -EPERM; }
static inline int bif_ctrl_set_bus_state(struct bif_ctrl *ctrl,
				enum bif_bus_state state)
{ return -EPERM; }

#endif

#endif
