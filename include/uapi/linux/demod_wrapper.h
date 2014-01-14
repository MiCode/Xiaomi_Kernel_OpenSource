#ifndef _DEMOD_WRAPPER_H_
#define _DEMOD_WRAPPER_H_

#include <linux/ioctl.h>

/**
 * enum demod_wrapper_path_type -
 * represents all the possible path types in demod wrapper.
 * @demod_wrapper_forza_atv: a path for analog tv that passes through
 * forza demod. This path type is for all analog standards supported by forza.
 * @demod_wrapper_dtv_s: a path for digital tv that passes through
 * digital demod. This path type is for satellite standards supported by
 * this demod.
 * @demod_wrapper_dtv_t_c: a path for digital tv that passes through
 * digital demod. This path type is for terrestrial and cable standards
 * supported by this demod.
 * @demod_wrapper_ext_atv: a path for analog tv. The demod in this case is
 * external and located outside the demod wrapper.
 * @demod_wrapper_num_of_paths: number of path types.
 */
enum demod_wrapper_path_type {
	DEMOD_WRAPPER_FORZA_ATV,
	DEMOD_WRAPPER_DTV_S,
	DEMOD_WRAPPER_DTV_T_C,
	DEMOD_WRAPPER_EXT_ATV,
	DEMOD_WRAPPER_NUM_OF_PATHS
};

/**
 * enum demod_wrapper_power_mode -
 * represents all the possible power modes of demod wrapper.
 * @demod_wrapper_full_preformance: demod wrapper will operate in full
 * performance mode.
 * @demod_wrapper_num_of_power_modes: number of power modes.
 */
enum demod_wrapper_power_mode {
	DEMOD_WRAPPER_FULL_PREFORMANCE,
	DEMOD_WRAPPER_NUM_OF_POWER_MODES
};

/**
 * enum demod_wrapper_baud_rate_mode -
 * represents all the baud rate modes that are supported in dtv satellite paths.
 * @demod_wrapper_narrow: narrow baud rate mode.
 * @demod_wrapper_medium: medium baud rate mode.
 * @demod_wrapper_wide: wide baud rate mode.
 * @demod_wrapper_num_of_baud_rate_modes: number of baud_rate modes.
 */
enum demod_wrapper_baud_rate_mode {
	DEMOD_WRAPPER_NARROW,
	DEMOD_WRAPPER_MEDIUM,
	DEMOD_WRAPPER_WIDE,
	DEMOD_WRAPPER_NUM_OF_BAUD_RATE_MODES
};

/**
 * enum demod_wrapper_pdm_num -
 * represents the two pdms that exists in demod wrapper: pdm0 and pdm1.
 * @demod_wrapper_pdm0:
 * @demod_wrapper_pdm1:
 */
enum demod_wrapper_pdm_num {
	DEMOD_WRAPPER_PDM0,
	DEMOD_WRAPPER_PDM1
};

/**
 * enum demod_wrapper_ts_bridge -
 * represents the output types of the ts bridge.
 * @demod_wrapper_ts_serial: output of ts bridge is serial.
 * @demod_wrapper_ts_parallel: output of ts bridge is parallel.
 */
enum demod_wrapper_ts_bridge {
	DEMOD_WRAPPER_TS_SERIAL,
	DEMOD_WRAPPER_TS_PARALLEL
};

/**
 * struct demod_wrapper_release_path_args -
 * arguments to be passed to DEMOD_WRAPPER_RELEASE_PATH ioctl.
 * @type: the type of the path to be released.
 */
struct demod_wrapper_release_path_args {
	enum demod_wrapper_path_type type;
};

/**
 * struct demod_wrapper_set_path_args -
 * arguments to be passed to DEMOD_WRAPPER_SET_PATH ioctl.
 * @type: the type of the path to set.
 * @pdm: the number of the pdm that this path should work with.
 * @power: the power mode that this path should work in.
 * Note: if we want to set external_atv path there is no pdm
 * involved so passing any value as pdm will be fine
 */
struct demod_wrapper_set_path_args {
	enum demod_wrapper_path_type type;
	enum demod_wrapper_pdm_num pdm;
	enum demod_wrapper_power_mode power;
};

/**
 * struct demod_wrapper_set_path_dtv_sat_args -
 * arguments that should be passed to DEMOD_WRAPPER_SET_PATH_DTV_SAT
 * ioctl.
 * @pdm: the number of the pdm that dtv satellite path should work with.
 * @power: the power mode that dtv satellite path should work in.
 * @br_mode: baud rate parameter that is a specific paramter for dtv satellite,
 * and indicates what is the baud rate of the satellite signal.
 */
struct demod_wrapper_set_path_dtv_sat_args {
	enum demod_wrapper_pdm_num pdm;
	enum demod_wrapper_power_mode power;
	enum demod_wrapper_baud_rate_mode br_mode;
};

/**
 * struct demod_wrapper_init_ts_bridge -
 * argumants that should be passed to DEMOD_WRAPPER_TS_BRIDGE_INIT ioctl.
 * @out: indicates if the output signal of the ts bridge is serial or parallel.
 */
struct demod_wrapper_init_ts_bridge_args {
	enum demod_wrapper_ts_bridge out;
};

/**
 * struct demod_wrapper_pm_set_params_args -
 * argumants that should be passed to DEMOD_WRAPPER_PM_SET_PARAMS ioctl.
 * @pm_loop_cntr: the value that will be set to BCDEM_REGS_PM_LOOP_CNTR register
 * @pm_params_threshold: the value that will be set to
 * BCDEM_REGS_PM_PARAMS_THRESHOLD
 */
struct demod_wrapper_pm_set_params_args {
	unsigned int pm_loop_cntr;
	unsigned int pm_params_threshold;
};

/**
 * struct demod_wrapper_pm_get_thrshld_cntr_args -
 * argumants that should be passed to DEMOD_WRAPPER_PM_GET_THRSHLD_CNTR ioctl.
 * @pm_thrshld_cntr: the parameter will hold the value gotten from
 * BCDEM_REGS_PM_RO_THRSHLD_CNTR
 */
struct demod_wrapper_pm_get_thr_cntr_args {
	unsigned int pm_thrshld_cntr;
};

/**
 * struct demod_wrapper_pm_get_power_args -
 * argumants that should be passed to DEMOD_WRAPPER_PM_GET_POWER ioctl.
 * @pm_power: the parameter will hold the value gotten from
 * BCDEM_REGS_PM_RO_POWER
 */
struct demod_wrapper_pm_get_power_args {
	unsigned int pm_power;
};

#define DEMOD_WRAPPER_BASIC_CMDS_NUM	1

/**
 * ioctl cmd : DEMOD_WRAPPER_SET_PATH -
 * Performs initialization and configuration of components
 * in demod_wrapper that are needed for the requested path.
 * If there is any path conflict - overrides.
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_SET_PATH _IOW(DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	1, struct demod_wrapper_set_path_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_SET_PATH_DTV_SAT -
 * Performs initialization and configuration of components
 * in demod_wrapper that are needed for dtv sat path.
 * If there is any path conflict - overrides.
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_SET_PATH_DTV_SAT _IOW(\
	DEMOD_WRAPPER_BASIC_CMDS_NUM, 2,\
	struct demod_wrapper_set_path_dtv_sat_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_RELEASE_PATH -
 * Releases the requested path.
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_RELEASE_PATH _IOW(DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	3, struct demod_wrapper_release_path_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_RELEASE_PATH -
 * Sets the out of the ts-bridge according to received
 * argument. Sets the in as parallel.
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_TS_BRIDGE_INIT _IOW(DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	4, struct demod_wrapper_init_ts_bridge_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_RELEASE_PATH -
 * Enables the ts-bridge. If ts-bridge was'nt initialized
 * (with DEMOD_WRAPPER_TS_BRIDGE_INIT) sets default values:
 *      in - parallel
 *      out - parallel
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_TS_BRIDGE_ENABLE _IO(DEMOD_WRAPPER_BASIC_CMDS_NUM, 5)

/**
 * ioctl cmd : DEMOD_WRAPPER_RELEASE_PATH -
 * Disables the ts-bridge.
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_TS_BRIDGE_DISABLE _IO(\
	DEMOD_WRAPPER_BASIC_CMDS_NUM, 6)

/**
 * ioctl cmd : DEMOD_WRAPPER_PM_SET_PARAMS -
 * Sets values for BCDEM_REGS_PM_LOOP_CNTR and BCDEM_REGS_PM_PARAMS_THRESHOLD
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_PM_SET_PARAMS _IOW(DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	7, struct demod_wrapper_pm_set_params_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_PM_GET_THR_CNTR -
 * Gets the value of the BCDEM_REGS_OM_RO_THRSHLD_CNTR
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_PM_GET_THRSHLD_CNTR _IOR(\
	DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	8, struct demod_wrapper_pm_get_thr_cntr_args)

/**
 * ioctl cmd : DEMOD_WRAPPER_PM_GET_POWER -
 * Gets the value of the BCDEM_REGS_RO_POWER
 * Returns 0 in success.
 */
#define DEMOD_WRAPPER_PM_GET_POWER _IOR(DEMOD_WRAPPER_BASIC_CMDS_NUM,\
	9, struct demod_wrapper_pm_get_power_args)

#endif /* _DEMOD_WRAPPER_H_ */
