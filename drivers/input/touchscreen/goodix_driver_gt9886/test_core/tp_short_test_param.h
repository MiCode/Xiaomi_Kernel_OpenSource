#ifndef TP_SHORT_TEST_PARAM_H
#define TP_SHORT_TEST_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "test_item_def.h"

/*//short test threshold parameter*/
typedef struct SHORT_THRESHOLD_PARAM {
	u16 drv_gnd_vdd_resistor_threshold;
	u16 sen_gnd_vdd_resistor_threshold;
	u16 sen_sen_resistor_threshold;
	u16 drv_drv_resistor_threshold;
	u16 drv_sen_resistor_threshold;

	u16 gt_short_threshold;
	u16 adc_read_delay;
	u16 diffcode_short_threshold;
	u16 tx_tx_factor;
	u16 tx_rx_factor;
	u16 rx_rx_factor;

} ST_SHORT_THRESHOLD_PARAM, *PST_SHORT_THRESHOLD_PARAM;

/*//short bin*/
typedef struct SHORT_BIN_PARAM {
	u32 update_bin_len;
	union {
		u32 bin_addr;
		u8 *bin_ptr;
	} update_bin_addr;

} ST_SHORT_BIN_PARAM, *PST_SHORT_BIN_PARAM;

/*//short option*/
typedef struct SHORT_OPTION {
	u16 len;
	union {
		u32 opt_u32;
		u8 *opt_ptr;
	} opt;
} ST_SHORT_OPTION, *PST_SHORT_OPTION;

typedef struct SHORT_PARAM {
	ST_SHORT_THRESHOLD_PARAM short_threshold;
	ST_SHORT_BIN_PARAM short_bin;
	ST_SHORT_OPTION short_opt;
} ST_SHORT_PARAM, *PST_SHORT_PARAM;

#ifdef __cplusplus
}
#endif
#endif
