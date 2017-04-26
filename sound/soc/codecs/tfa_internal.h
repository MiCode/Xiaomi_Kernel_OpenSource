/*
	internal functions for TFA layer (not shared with SRV and HAL layer!)
*/

#ifndef __TFA_INTERNAL_H__
#define __TFA_INTERNAL_H__

#include "tfa_dsp_fw.h"
#include "tfa_service.h"
#include "tfa_config.h"

#if __GNUC__ >= 4
  #define TFA_INTERNAL __attribute__ ((visibility ("hidden")))
#else
  #define TFA_INTERNAL
#endif


#define TFA98XX_GENERIC_SLAVE_ADDRESS 0x1C

enum featureSupport {
	supportNotSet, /* the default is not set yet, so = 0 */
	supportNo,
	supportYes
};

typedef enum featureSupport featureSupport_t;

/*
 * tfa98xx control structure gathers data related to a single control
 * (a 'control' can be related to an interface file)
 * Some operations can be flagged as deferrable, meaning that they can be
 *   scheduled for later execution. This can be used for operations that
 *   require the i2s clock to run, and if it is not available while applying
 *   the control.
 * The Some operations can as well be cache-able (supposedly they are the same
 *   operations as the deferrable). Cache-able means that the status or
 *   register value may not be accesable while accessing the control. Caching
 *   allows to get the last programmed value.
 *
 * Fields:
 * deferrable:
 *   true: means the action or register accces can be run later (for example
 *   when an active i2s clock will be available).
 *   false: meams the operation can be applied immediately
 * triggered: true if the deferred operation was triggered and is scheduled
 *   to run later
 * wr_value: the value to write in the deferred action (if applicable)
 * rd_value: the cached value to report on a cached read (if applicable)
 * rd_valid: true if the rd_value was initialized (and can be reported)
 */

struct tfa98xx_control {
	bool deferrable;
	bool triggered;
	int wr_value;
	int rd_value;
	bool rd_valid;
};

struct tfa98xx_controls {
	struct tfa98xx_control otc;
	struct tfa98xx_control mtpex;
	struct tfa98xx_control calib;
};

struct tfa_device_ops {
	enum Tfa98xx_Error (*tfa_init)(Tfa98xx_handle_t dev_idx);
	enum Tfa98xx_Error (*tfa_dsp_reset)(Tfa98xx_handle_t dev_idx, int state);
	enum Tfa98xx_Error (*tfa_dsp_system_stable)(Tfa98xx_handle_t handle, int *ready);
	enum Tfa98xx_Error (*tfa_dsp_write_tables)(Tfa98xx_handle_t dev_idx, int sample_rate);
	struct tfa98xx_controls controls;
};

struct Tfa98xx_handle_private {
	int in_use;
	int buffer_size;
	unsigned char slave_address;
	unsigned short rev;
	unsigned char tfa_family; /* tfa1/tfa2 */
	enum featureSupport supportDrc;
	enum featureSupport supportFramework;
	enum featureSupport support_saam;
	int sw_feature_bits[2]; /* cached feature bits data */
	int hw_feature_bits; /* cached feature bits data */
	int profile;	/* cached active profile */
	int vstep[2]; /* cached active vsteps */
	unsigned char spkr_count;
	unsigned char spkr_select;
	unsigned char support_tcoef;
	enum Tfa98xx_DAI daimap;
	int mohm[3]; /* > speaker calibration values in milli ohms -1 is error */
	struct tfa_device_ops dev_ops;
	uint16_t interrupt_enable[3];
	uint16_t interrupt_status[3];
};

/* tfa_core.c */
extern TFA_INTERNAL struct Tfa98xx_handle_private handles_local[];
TFA_INTERNAL int tfa98xx_handle_is_open(Tfa98xx_handle_t h);
TFA_INTERNAL enum Tfa98xx_Error tfa98xx_check_rpc_status(Tfa98xx_handle_t handle, int *pRpcStatus);
TFA_INTERNAL enum Tfa98xx_Error tfa98xx_wait_result(Tfa98xx_handle_t handle, int waitRetryCount);
TFA_INTERNAL void tfa98xx_apply_deferred_calibration(Tfa98xx_handle_t handle);
TFA_INTERNAL void tfa98xx_deferred_calibration_status(Tfa98xx_handle_t handle, int calibrateDone);
TFA_INTERNAL int print_calibration(Tfa98xx_handle_t handle, char *str, size_t size);

#endif /* __TFA_INTERNAL_H__ */
