/* SPDX-License-Identifier: GPL-2.0 */

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-18 File created.
 */

#ifndef __FSM_PUBLIC_H__
#define __FSM_PUBLIC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fsm-dev.h"
#include "fsm_reg_bf.h"

/*
 * module: fsm_regmap
 */
#if defined(CONFIG_FSM_REGMAP)
#include <linux/regmap.h>
int fsm_regmap_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_regmap_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *pval, uint32_t len);
int fsm_regmap_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval);
int fsm_regmap_bulkread(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *pval, uint32_t len);
int fsm_regmap_update_bits(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t mask, uint8_t val);
struct regmap *fsm_regmap_i2c_init(struct i2c_client *i2c);
int fsm_regmap_i2c_deinit(struct regmap *map);
#else
#define fsm_regmap_i2c_deinit(...)
#endif
/*
 * module: fsm_i2c
 */
#if defined(CONFIG_FSM_I2C)
#include <linux/i2c.h>
bool fsm_set_pdev(struct device *dev);
struct device *fsm_get_pdev(void);
void *fsm_devm_kstrdup(struct device *dev, void *buf, size_t size);
int fsm_i2c_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pVal);
int fsm_i2c_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_i2c_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg,
				uint8_t *data, int len);
int fsm_i2c_init(void);
void fsm_i2c_exit(void);
#endif

void fsm_mutex_lock(void);
void fsm_mutex_unlock(void);

/*
 * module: fsm_misc
 */
#if defined(CONFIG_FSM_MISC)
int fsm_misc_init(void);
void fsm_misc_deinit(void);
#else
#define fsm_misc_init(...)
#define fsm_misc_deinit(...)
#endif

/*
 * module: fsm_proc
 */
#if defined(CONFIG_FSM_PROC)
int fsm_proc_init(void);
void fsm_proc_deinit(void);
#else
#define fsm_proc_init(...)
#define fsm_proc_deinit(...)
#endif

/*
 * module: fsm_sysfs
 */
#if defined(CONFIG_FSM_SYSFS)
int fsm_sysfs_init(struct device *dev);
void fsm_sysfs_deinit(struct device *dev);
#else
#define fsm_sysfs_init(...) (-1)
#define fsm_sysfs_deinit(...)
#endif

/*
 * module: fsm_firmware
 */
#if defined(CONFIG_FSM_FIRMWARE)
int fsm_firmware_init(char *fw_name);
int fsm_firmware_init_sync(char *fw_name);
void fsm_firmware_deinit(void);
#elif defined(FSM_HAL_SUPPORT)
int fsm_firmware_init_sync(char *fw_name);
void fsm_firmware_deinit(void);
#else
#define fsm_firmware_init(...) (-1)
#define fsm_firmware_init_sync(...) (-1)
#define fsm_firmware_deinit(...)
#endif

/*
 * module: fsm_codec
 */
#if defined(CONFIG_FSM_CODEC)
int fsm_codec_register(struct device *dev, int id);
void fsm_codec_unregister(struct device *dev);
#else
#define fsm_codec_register(...) (-1)
#define fsm_codec_unregister(...)
#endif

/*
 * module: fsm_core
 */
fsm_config_t *fsm_get_config(void);
struct preset_file *fsm_get_presets(void);
void fsm_set_presets(struct preset_file *file);
void fsm_get_version(fsm_version_t *version);
void fsm_delay_ms(uint32_t delay_ms);
void *fsm_alloc_mem(int size);
void fsm_free_mem(void *buf);
uint16_t set_bf_val(uint16_t *pval, const uint16_t bf, const uint16_t bf_val);
uint16_t get_bf_val(const uint16_t bf, const uint16_t val);
int fsm_set_bf(fsm_dev_t *fsm_dev, const uint16_t bf, const uint16_t val);
int fsm_get_bf(fsm_dev_t *fsm_dev, const uint16_t bf, uint16_t *pval);
int fsm_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval);
int fsm_reg_update_bits(fsm_dev_t *fsm_dev, reg_unit_t *reg);
int fsm_reg_update(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_reg_multiread(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval);
int fsm_burst_write(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *data, int len);
struct fsm_dev *fsm_get_fsm_dev(uint8_t addr);
int fsm_read_vbat(fsm_dev_t *fsm_dev, uint16_t *vbat);
int fsm_get_srate_bits(fsm_dev_t *fsm_dev, uint32_t srate);
int fsm_access_key(fsm_dev_t *fsm_dev, int access);
int fsm_reg_dump(fsm_dev_t *fsm_dev);
int fsm_init_dev_list(fsm_dev_t *fsm_dev);
void *fsm_get_list_by_idx(fsm_dev_t *fsm_dev, int idx);
void *fsm_get_data_list(fsm_dev_t *fsm_dev, int type);
int fsm_get_spk_info(fsm_dev_t *fsm_dev, uint16_t info_type);
int zero_bit_counter(uint8_t byte);
int get_otp_counter(uint16_t byte);
int get_otp_max_count(fsm_dev_t *fsm_dev);
void convert_data_to_bytes(uint32_t val, uint8_t *buf);
int fsm_parse_preset(const void *data, uint32_t size);
int fsm_swap_channel(fsm_dev_t *fsm_dev, int next_angle);
int fsm_set_monitor(fsm_dev_t *fsm_dev);
int fsm_wait_stable(fsm_dev_t *fsm_dev, int type);
int fsm_parse_otp(fsm_dev_t *fsm_dev, uint16_t value, int *re25, int *count);
int fsm_dev_recover(fsm_dev_t *fsm_dev);

/*
 * module: fsm_hal
 */
#if defined(FSM_HAL_SUPPORT)
int fsm_hal_open(void);
int fsm_hal_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *val);
int fsm_hal_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_hal_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *val, uint32_t len);
void fsm_hal_close(void);
#else
#define fsm_hal_open(...) (0)
#define fsm_hal_close(...)
#endif

#ifdef CONFIG_FSM_FS1601S
void fs1601s_ops(fsm_dev_t *fsm_dev);
#else
#define fs1601s_ops(...)
#endif

#ifdef CONFIG_FSM_FS1603
void fs1603_ops(fsm_dev_t *fsm_dev);
#else
#define fs1603_ops(...)
#endif

#ifdef CONFIG_FSM_FS1801
void fs1801_ops(fsm_dev_t *fsm_dev);
#else
#define fs1801_ops(...)
#endif

#ifdef CONFIG_FSM_FS1860
void fs1860_ops(fsm_dev_t *fsm_dev);
#else
#define fs1860_ops(...)
#endif

void fsm_set_fw_name(char *name);
int fsm_probe(fsm_dev_t *fsm_dev, int addr);
void fsm_remove(fsm_dev_t *fsm_dev);
int fsm_dev_count(void);
void fsm_set_i2s_clocks(uint32_t rate, uint32_t bclk);
void fsm_set_scene(int scene);
void fsm_set_volume(int volume);
void fsm_set_cfg_flag(int pos, int mark);
void fsm_init(void);
void fsm_speaker_onn(void);
void fsm_speaker_off(void);
void fsm_stereo_flip(int next_angle);
void fsm_batv_monitor(void);
void fsm_re25_test(bool force);
void fsm_f0_test(void);
int fsm_test_result(struct fsm_cal_result *result);
void fsm_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
