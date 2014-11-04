#ifndef __INPUT_FORMATTER_GLOBAL_H_INCLUDED__
#define __INPUT_FORMATTER_GLOBAL_H_INCLUDED__

#define IS_INPUT_FORMATTER_VERSION1

#include "stdint.h"

#include "if_defs.h"

typedef struct input_formatter_cfg_s	input_formatter_cfg_t;

/* Hardware registers */
#define HIVE_IF_RESET_ADDRESS                   0x000
#define HIVE_IF_START_LINE_ADDRESS              0x004
#define HIVE_IF_START_COLUMN_ADDRESS            0x008
#define HIVE_IF_CROPPED_HEIGHT_ADDRESS          0x00C
#define HIVE_IF_CROPPED_WIDTH_ADDRESS           0x010
#define HIVE_IF_VERTICAL_DECIMATION_ADDRESS     0x014
#define HIVE_IF_HORIZONTAL_DECIMATION_ADDRESS   0x018
#define HIVE_IF_H_DEINTERLEAVING_ADDRESS        0x01C
#define HIVE_IF_LEFTPADDING_WIDTH_ADDRESS       0x020
#define HIVE_IF_END_OF_LINE_OFFSET_ADDRESS      0x024
#define HIVE_IF_VMEM_START_ADDRESS_ADDRESS      0x028
#define HIVE_IF_VMEM_END_ADDRESS_ADDRESS        0x02C
#define HIVE_IF_VMEM_INCREMENT_ADDRESS          0x030
#define HIVE_IF_YUV_420_FORMAT_ADDRESS          0x034
#define HIVE_IF_VSYNCK_ACTIVE_LOW_ADDRESS       0x038
#define HIVE_IF_HSYNCK_ACTIVE_LOW_ADDRESS       0x03C
#define HIVE_IF_ALLOW_FIFO_OVERFLOW_ADDRESS     0x040
#define HIVE_IF_BLOCK_FIFO_NO_REQ_ADDRESS       0x044
#define HIVE_IF_V_DEINTERLEAVING_ADDRESS        0x048
/* Registers only for simulation */
#define HIVE_IF_CRUN_MODE_ADDRESS               0x04C
#define HIVE_IF_DUMP_OUTPUT_ADDRESS             0x050

/* Follow the DMA syntax, "cmd" last */
#define IF_PACK(val, cmd)             ((val & 0x0fff) | (cmd /*& 0xf000*/))

/*
 * This data structure is shared between host and SP
 */
struct input_formatter_cfg_s {
	uint32_t	start_line;
	uint32_t	start_column;
	uint32_t	left_padding;
	uint32_t	cropped_height;
	uint32_t	cropped_width;
	uint32_t	deinterleaving;
	uint32_t	buf_vecs;
	uint32_t	buf_start_index;
	uint32_t	buf_increment;
	uint32_t	buf_eol_offset;
	uint32_t	is_yuv420_format;
	uint32_t	block_no_reqs;
};

#endif /* __INPUT_FORMATTER_GLOBAL_H_INCLUDED__ */
