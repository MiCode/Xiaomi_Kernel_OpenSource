#ifndef OIS_CORE_H
#define OIS_CORE_H
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>

typedef enum {
    EnableOIS,          // driver ON
    DisenableOIS,       // driver OFF (unimplemented: driver software handle it)
    Movie,              // Movie mode
    Still,              // Still mode
    Zoom,               // Zoom mode
    CenteringOn,        // OIS OFF (lens centering)
    CenteringOff,       //            (unimplemented: driver software handle it)
    Pantilt,            //            (unimplemented)
    Scene,              //            (unimplemented)
    SceneFilterOn,      //            (unimplemented)
    SceneFilterOff,     //            (unimplemented)
    SceneRangeOn,       //            (unimplemented)
    SceneRangeOff,      //            (unimplemented)
    ManualMovieLens,    // scene manual mode (unimplemented)
    TestMode,           // scene test mode
    ModeEnd             // enum end
} OIS_MODE;

enum ois_i2c_type {
	OIS_I2C_TYPE_INVALID,
	OIS_I2C_TYPE_BYTE,
	OIS_I2C_TYPE_WORD,
	OIS_I2C_TYPE_MAX,
};

enum op_type {
	OP_WRITE,
	OP_READ,
	OP_POLL,
	OP_MAX,
};

struct ois_gyro_offset {
  int OISGyroOffsetX;
  int OISGyroOffsetY;
  int OISGyroOffsetZ;
};

struct ois_drift {
  int OISDriftX;
  int OISDriftY;
};

#define OIS_DATA_NUMBER 64
struct OisInfo {
	int32_t is_ois_supported;
	int32_t data_mode;  /* ON/OFF */
	int32_t samples;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
};

struct ois_setting {
	u16 addr;
	u16 data;
	u32 delay_us;
	enum op_type type;
};

struct ois_driver_info {
	const char *ois_name;
	u32 prog_addr;
	u32 coeff_addr;
	u32 mem_addr;
	u32 cali_addr;
	u32 cali_offset;
	u32 cali_size;
	u32 eeprom_addr;
	enum ois_i2c_type addr_type;
	enum ois_i2c_type data_type;
	struct i2c_client *client;
	struct task_struct *task_thread;
	struct mutex mutex;
	wait_queue_head_t wq;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
	int64_t inputIdx;
	int64_t outputIdx;
};

int ois_i2c_rd_p8(struct i2c_client *i2c_client, u16 addr, u16 reg, u8 *p_vals, u32 n_vals);
int ois_i2c_rd_u8(struct i2c_client *i2c_client, u16 addr, u16 reg, u8 *val);
int ois_i2c_rd_u16(struct i2c_client *i2c_client, u16 addr, u16 reg, u16 *val);
int ois_i2c_rd_u32(struct i2c_client *i2c_client, u16 addr, u16 reg, u32 *val);
int ois_i2c_wr_u8(struct i2c_client *i2c_client, 	u16 addr, u16 reg, u8 val);
int ois_i2c_wr_u16(struct i2c_client *i2c_client, u16 addr, u16 reg, u16 val);
int ois_i2c_poll_u8(struct i2c_client *i2c_client, u16 addr, u16 reg, u8 val);
int ois_i2c_poll_u16(struct i2c_client *i2c_client, u16 addr, u16 reg, u16 val);

int ois_i2c_wr_regs_u8_burst(struct i2c_client *i2c_client, u16 *list, u32 len);
int ois_i2c_wr_regs_u16_burst(struct i2c_client *i2c_client, u16 *list, u32 len);

u32 default_ois_fw_download(struct ois_driver_info *info);
u8 read_eeprom_u8(struct i2c_client *client, u16 eeprom_addr, u16 addr);
int update_ois_cali(struct ois_driver_info *info, struct ois_gyro_offset *gyro_offset);

int ois_write_setting(struct ois_driver_info *info, struct ois_setting *setting, int len);

void reset_ois_data(struct ois_driver_info *info);
bool is_ois_data_empty(struct ois_driver_info *info);
bool is_ois_data_full(struct ois_driver_info *info);
void set_ois_data(struct ois_driver_info *info, u64 timestamps, int x_shifts, int y_shifts);
int get_ois_data(struct ois_driver_info *info, u64 *timestamps, int *x_shifts, int *y_shifts);

#endif //OIS_CORE_H
