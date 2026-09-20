/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#ifndef _TMS_NFC_H_
#define _TMS_NFC_H_

/*********** PART0: Head files ***********/
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include "../tms_common.h"
/*********** PART1: Define Area ***********/
#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE               "Nfc"
#endif
#define NFC_VERSION               TMS_VERSION ".010201"
#define MAX_CHIP_NAME_SIZE        (32)
#define NCI_HDR_LEN               (3)
#define NCI_PAYLOAD_LEN_BYTE      (2)
#define MAX_NCI_PAYLOAD_LEN       (255)
#define MAX_NCI_BUFFER_SIZE       (NCI_HDR_LEN + MAX_NCI_PAYLOAD_LEN)
#define T1_HDR_LEN                (3)
#define T1_PAYLOAD_LEN_BYTE       (2)
#define T1_LRC_LEN                (1)
#define MAX_T1_PAYLOAD_LEN        (255)
#define MAX_T1_BUFFER_SIZE        (T1_HDR_LEN + MAX_T1_PAYLOAD_LEN + T1_LRC_LEN)
#define WAKEUP_SRC_TIMEOUT        (2000)
#define RETRY_TIMES               (3)
#define HEAD_PAYLOAD_BYTE         (2)
/*********** PART2: Struct Area ***********/
enum nfc_ioctl_request_ese_table {
    REQUEST_ESE_POWER_ON = 0, /* eSE POWER ON */
    REQUEST_ESE_POWER_OFF,    /* eSE POWER OFF */
    REQUEST_ESE_POWER_STATE,  /* eSE GET POWER STATE */
};

// NFC LDO configuration
struct ldo {
    bool             enabled;
    uint32_t         voltage_range[2];  /* min and max voltage in uV */
    uint32_t         load_current;      /* load current in uA */
    struct regulator *reg;
};

struct nfc_info {
    bool                   irq_enable;
    bool                   ven_enable;       /* store VEN state */
    volatile bool          release_read;
    bool                   irq_wake_up;
    unsigned int           open_dev_count;
    struct i2c_client      *client;
    struct device          *i2c_dev;         /* Used for i2c->dev */
    struct dev_register    dev;
    struct hw_resource     hw_res;
    struct clk             *clk;
    struct clk             *clk_parent;
    struct clk             *clk_enable;
    struct ldo             ldo;
    struct tms_info        *tms;             /* tms common data */
    struct mutex           read_mutex;
    struct mutex           write_mutex;
    struct mutex           open_dev_mutex;
    spinlock_t             irq_enable_slock;
    wait_queue_head_t      read_wq;

};

/*********** PART3: Function or variables for other files ***********/
struct nfc_info *nfc_data_alloc(struct device *dev, struct nfc_info *nfc);
void nfc_data_free(struct device *dev, struct nfc_info *nfc);
int nfc_common_info_init(struct nfc_info *nfc);
int nfc_ioctl_set_ese_state(struct nfc_info *nfc, unsigned long arg);
int nfc_ioctl_get_ese_state(struct nfc_info *nfc, unsigned long arg);
void nfc_gpio_release(struct nfc_info *nfc);
struct nfc_info *nfc_get_data(struct inode *inode);
void nfc_disable_irq(struct nfc_info *nfc);
void nfc_enable_irq(struct nfc_info *nfc);
void nfc_power_control(struct nfc_info *nfc, bool state);
void nfc_fw_download_control(struct nfc_info *nfc, bool state);
void nfc_hard_reset(struct tms_info *tms);
void nfc_read_flush(struct nfc_info *nfc);
int nfc_irq_register(struct nfc_info *nfc);
bool nfc_hw_check(struct i2c_client *client, struct tms_info *tms);
bool support_download_gpio(void);
int nfc_enable_rf_clk(struct nfc_info *nfc);
void nfc_disable_rf_clk(struct nfc_info *nfc);
int nfc_regulator_get(struct device *i2c_dev, struct ldo *ldo);
void nfc_regulator_put(struct ldo *ldo);
int nfc_ldo_vote(struct ldo *ldo);
void nfc_ldo_unvote(struct ldo *ldo);
bool nfc_write(struct i2c_client *client, const uint8_t *cmd,
               size_t len, const char *msg);
bool nfc_read(struct i2c_client *client, unsigned int irq_gpio,
              uint8_t *data, size_t len, const char *msg);
int nfc_create_sysfs_interfaces(struct device *dev);
void nfc_remove_sysfs_interfaces(struct device *dev);
void nfc_jump_fw(struct i2c_client *client, unsigned int irq_gpio);
bool get_board_id_from_dtb(char boardidinfo[]);
bool support_download_gpio(void);
#endif /* _TMS_NFC_H_ */
