#ifndef __INPUT_SYSTEM_GLOBAL_H_INCLUDED__
#define __INPUT_SYSTEM_GLOBAL_H_INCLUDED__

#define IS_INPUT_SYSTEM_VERSION_1

#include <stdint.h>
#include <css_receiver_ahb_defs.h>

#define _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX \
	_HRT_CSS_RECEIVER_AHB_IRQ_STATUS_REG_IDX
#define _HRT_CSS_RECEIVER_IRQ_OVERRUN_BIT \
	_HRT_CSS_RECEIVER_AHB_IRQ_OVERRUN_BIT

typedef enum {
	MIPI_0LANE_CFG = 0,
	MIPI_1LANE_CFG = 1,
	MIPI_4LANE_CFG = 4
} mipi_lane_cfg_t;

typedef enum {
	INPUT_SYSTEM_SOURCE_SENSOR = 0,
	INPUT_SYSTEM_SOURCE_FIFO,
	INPUT_SYSTEM_SOURCE_TPG,
	INPUT_SYSTEM_SOURCE_PRBS,
	INPUT_SYSTEM_SOURCE_MEMORY,
	N_INPUT_SYSTEM_SOURCE,
} input_system_source_t;

typedef enum {
	INPUT_SYSTEM_SINK_MEMORY = 0,
	INPUT_SYSTEM_SINK_ISP,
	INPUT_SYSTEM_SINK_SP,
	N_INPUT_SYSTEM_SINK,
} input_system_sink_t;

typedef struct input_system_cfg_s	input_system_cfg_t;
typedef struct sync_generator_cfg_s	sync_generator_cfg_t;
typedef struct tpg_cfg_s			tpg_cfg_t;
typedef struct prbs_cfg_s			prbs_cfg_t;

/* MW: uint16_t should be sufficient */
struct input_system_cfg_s {
	uint32_t	no_side_band;
	uint32_t	fmt_type;
	uint32_t	ch_id;
	uint32_t	input_mode;
};

struct sync_generator_cfg_s {
	uint32_t	width;
	uint32_t	height;
	uint32_t	hblank_cycles;
	uint32_t	vblank_cycles;
};

/* MW: tpg & prbs are exclusive */
struct tpg_cfg_s {
	uint32_t	x_mask;
	uint32_t	y_mask;
	uint32_t	x_delta;
	uint32_t	y_delta;
	uint32_t	xy_mask;
};

struct prbs_cfg_s {
	uint32_t	seed;
};

/*
static const hrt_address MIPI_PORT_OFFSET[N_MIPI_PORT_ID] = {
	0x00000100UL,
	0x00000200UL};
*/

#endif /* __INPUT_SYSTEM_GLOBAL_H_INCLUDED__ */
