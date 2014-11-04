#ifndef __SYSTEM_GLOBAL_H_INCLUDED__
#define __SYSTEM_GLOBAL_H_INCLUDED__

#include <hive_isp_css_defs.h>	/* HIVE_ISP_DDR_WORD_BITS, ... */
#define HIVE_ISP_DDR_WORD_BYTES  (HIVE_ISP_DDR_WORD_BITS/8)

/*
 * The longest allowed (uninteruptible) bus transfer, does not
 * take stalling into account
 */
#define HIVE_ISP_MAX_BURST_LENGTH	128

/*
 * Create a list of HAS and IS properties that defines the system
 * these definitions should be used for platform specific code
 * control rather than the cell/device specific defines which
 * differ per cell/scope
 *
 * Those properties should be moved to the system API (like the
 * register maps in sh_css_hw.h)
 *
 * The configuration assumes the following
 * - The system is hetereogeneous; Multiple cells and devices classes
 * - The cell and device instances are homogeneous, each device type
 *   belongs to the same class
 * - Device instances supporting a subset of the class capabilities are
 *   allowed
 *
 * We could manage different device classes through the enumerated
 * lists (C) or the use of classes (C++), but that is presently not
 * supported
 */

#include <stdint.h>

#define IS_ISP_2300_SYSTEM
#define IS_ISP_2300_MEDFIELD_SYSTEM

#define HAS_ISP_2300_MEDFIELD
#define HAS_SP_2300
#define HAS_MMU_VERSION_1
#define HAS_DMA_VERSION_1
#define HAS_GDC_VERSION_1
#define HAS_VAMEM_VERSION_1
#define HAS_BAMEM_VERSION_1
#define HAS_NO_HMEM
#define HAS_IRQ_VERSION_1
#define HAS_IRQ_MAP_VERSION_1
#define HAS_INPUT_SYSTEM_VERSION_1
#define HAS_INPUT_FORMATTER_VERSION_1
#define HAS_FIFO_MONITORS_VERSION_1
#define HAS_GP_DEVICE_VERSION_1
#define HAS_GPIO_VERSION_1
#define HAS_TIMED_CTRL_VERSION_1
#define HAS_RX_VERSION_1

/*
 * Semi global. "HRT" is accessible from SP, but the HRT types do not fully apply
 */
#define HRT_VADDRESS_WIDTH	32
//#define HRT_ADDRESS_WIDTH	32		/* Surprise, this is a local property*/
#define HRT_DATA_WIDTH		32

#define SIZEOF_HRT_REG		(HRT_DATA_WIDTH>>3)
#define HIVE_ISP_CTRL_DATA_BYTES (HIVE_ISP_CTRL_DATA_WIDTH/8)

/* The main bus connecting all devices */
#define HRT_BUS_WIDTH		HIVE_ISP_CTRL_DATA_WIDTH
#define HRT_BUS_BYTES		HIVE_ISP_CTRL_DATA_BYTES

typedef uint32_t			hrt_bus_align_t;

/*
 * Enumerate the devices, device access through the API is by ID, through the DLI by address
 * The enumerator terminators are used to size the wiring arrays and as an exception value.
 */
typedef enum {
	DDR0_ID = 0,
	N_DDR_ID
} ddr_ID_t;

typedef enum {
	ISP0_ID = 0,
	N_ISP_ID
} isp_ID_t;

typedef enum {
	SP0_ID = 0,
	N_SP_ID
} sp_ID_t;

typedef enum {
	MMU0_ID = 0,
	N_MMU_ID
} mmu_ID_t;

typedef enum {
	DMA0_ID = 0,
	N_DMA_ID
} dma_ID_t;

typedef enum {
	GDC0_ID = 0,
	N_GDC_ID
} gdc_ID_t;

typedef enum {
	VAMEM0_ID = 0,
	VAMEM1_ID,
	N_VAMEM_ID
} vamem_ID_t;

typedef enum {
	BAMEM0_ID = 0,
	N_BAMEM_ID
} bamem_ID_t;

typedef enum {
	N_HMEM_ID
} hmem_ID_t;

typedef enum {
	IRQ0_ID = 0,
	N_IRQ_ID
} irq_ID_t;

typedef enum {
	FIFO_MONITOR0_ID = 0,
	N_FIFO_MONITOR_ID
} fifo_monitor_ID_t;

typedef enum {
	GP_DEVICE0_ID = 0,
	N_GP_DEVICE_ID
} gp_device_ID_t;

typedef enum {
	GPIO0_ID = 0,
	N_GPIO_ID
} gpio_ID_t;

typedef enum {
	TIMED_CTRL0_ID = 0,
	N_TIMED_CTRL_ID
} timed_ctrl_ID_t;

typedef enum {
	INPUT_FORMATTER0_ID = 0,
	INPUT_FORMATTER1_ID,
	N_INPUT_FORMATTER_ID
} input_formatter_ID_t;

typedef enum {
	INPUT_SYSTEM0_ID = 0,
	N_INPUT_SYSTEM_ID
} input_system_ID_t;

typedef enum {
	RX0_ID = 0,
	N_RX_ID
} rx_ID_t;

typedef enum {
	MIPI_PORT0_ID = 0,
	MIPI_PORT1_ID,
	N_MIPI_PORT_ID
} mipi_port_ID_t;

#define	N_RX_CHANNEL_ID		4

/* Generic port enumeration with an internal port type ID */
typedef enum {
	CSI_PORT0_ID = 0,
	CSI_PORT1_ID,
	TPG_PORT0_ID,
	PRBS_PORT0_ID,
	FIFO_PORT0_ID,
	MEMORY_PORT0_ID,
	N_INPUT_PORT_ID
} input_port_ID_t;

/* 2300 input system has no sub-systems */
typedef enum {
	UNIT0_ID = 0,
	N_SUB_SYSTEM_ID
} sub_system_ID_t;

typedef enum {
	IRQ_EVENT_TYPE0 = 0,
	N_IRQ_EVENT_TYPE
} irq_event_type;

#endif /* __SYSTEM_GLOBAL_H_INCLUDED__ */
