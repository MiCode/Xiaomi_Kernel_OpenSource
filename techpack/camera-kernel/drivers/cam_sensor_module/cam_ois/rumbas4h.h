#ifndef _CAM_RUMNAS4H_H_
#define _CAM_RUMNAS4H_H_

#define APP_RUMBAS4HFW_SIZE    (28*1024)
#define RUMBAS4H_REG_APP_VER    0x00FC
#define RUMBAS4H_REG_OIS_STS    0x0001
#define STATE_IDLE              0x01
#define RUMBAS4H_REG_FWUP_CTRL  0x000C
#define RUMBAS4H_REG_DATA_BUF   0x100
#define RUMBAS4H_REG_FWUP_ERR   0x0006
#define RUMBAS4H_SUCCESS_STATE  0x0000
#define RUMBAS4H_REG_FWUP_CHKSUM 0x0008
#define RUMBAS4H_REG_CHECK_SAVE  0x0036
#define RUMBAS4H_REG_SW_RESET1   0x000D
#define RUMBAS4H_REG_SW_RESET2   0x000E

#define NEEDRETRY 0
#define OUTRETRYTIME 1
#define UPDATESUCCESS 2

int32_t i2c_write_data_burst(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t length,uint8_t *data, uint32_t delay);

int32_t i2c_read_data_burst(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t length, uint8_t *data);

int32_t load_fw_buff_burst(
	struct cam_ois_ctrl_t *o_ctrl,
	char* firmware_name,
	uint8_t *read_data,
	uint32_t read_length);

#endif
