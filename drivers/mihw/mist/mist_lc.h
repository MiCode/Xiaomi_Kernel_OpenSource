// MIUI ADD: Performance_TurboSched
#ifndef MIST_LC_H
#define MIST_LC_H

#include <linux/module.h>
#include <linux/mi_soc.h>
#include "mist_dflt.h"
#include "mist_gflt.h"
#include <linux/cdev.h>
#include <linux/device.h>

u64 gpu_load_calibration(void);
u64 gpubw_load_calibration(void);

// ********************* 常量定义 *********************
#define MIST_DRIVER_NAME     "mist_driver"    // 驱动名称
#define MIST_DEVICE_NAME     "mist_device"    // 设备名称
#define MIST_CLASS_NAME      "mist_class"     // 设备类名称
#define MIST_MAJOR_DEV       0                // 动态分配主设备号
#define MIST_MAX_DEVICES     4                // 最大支持设备数
#define MIST_DATA_BUF_SIZE   256              // 数据缓冲区大小
#define MIST_DEFAULT_TIMEOUT 5000             // 默认超时时间(ms)

// 设备状态标志位
#define MIST_FLAG_ACTIVE     0x01             // 设备激活状态
#define MIST_FLAG_ERROR      0x02             // 错误状态

// 调试级别
enum mist_debug_levels {
    MIST_DBG_OFF = 0,       // 关闭调试
    MIST_DBG_ERR,           // 错误信息
    MIST_DBG_WARN,          // 警告信息
    MIST_DBG_INFO,          // 一般信息
    MIST_DBG_VERBOSE        // 详细调试
};

// 调试级别
enum mist_boost_types {
    MIST_DEFAULT = 0,
    MIST_PRE_BOOST,
    MIST_GC_BOOST,
    MIST_FB_BOOST,
    MIST_GPU_BOOST,
    MIST_GPU_OTHER
};

enum mist_GPU_load_levels {
    MIST_GPU_DEFAULT = 0,
    mist_GPU_load_levels_1,
    mist_GPU_load_levels_2,
    mist_GPU_load_levels_3,
    mist_GPU_load_levels_4,
    mist_GPU_load_levels_5,
    mist_GPU_load_levels_6,
    mist_GPU_load_levels_7,
    mist_GPU_load_levels_8
};

enum mist_preboost_levels {
    MIST_PRE_DEFAULT = 0,
    mist_preboost_levels_1,
    mist_preboost_levels_2,
    mist_preboost_levels_3,
    mist_preboost_levels_4,
    mist_preboost_levels_5,
    mist_preboost_levels_6,
    mist_preboost_levels_7,
    mist_preboost_levels_8
};
// ********************* 宏定义 *********************
// 条件调试打印宏
#define mist_print(level, fmt, ...) \
    do { \
        if (level <= mist_debug_level) { \
            printk(KERN_##level "MIST: %s: " fmt, __func__, ##__VA_ARGS__); \
        } \
    } while (0)

// 简化版调试宏
#define mist_err(fmt, ...)   mist_print(ERR, fmt, ##__VA_ARGS__)
#define mist_warn(fmt, ...)  mist_print(WARNING, fmt, ##__VA_ARGS__)
#define mist_info(fmt, ...)  mist_print(INFO, fmt, ##__VA_ARGS__)
#define mist_dbg(fmt, ...)   mist_print(DEBUG, fmt, ##__VA_ARGS__)

// 设备状态操作宏
#define mist_set_flag(dev, flag)   set_bit(flag, &(dev)->status_flags)
#define mist_clear_flag(dev, flag) clear_bit(flag, &(dev)->status_flags)
#define mist_test_flag(dev, flag)  test_bit(flag, &(dev)->status_flags)

// ********************* 数据结构 *********************
// 设备状态结构
struct mist_status {
    atomic_t usage_count;        // 使用计数
    unsigned long status_flags;  // 状态标志位
    u32 last_error;              // 最后错误代码
    u64 bytes_processed;         // 已处理字节数
};

// 设备核心结构
struct mist_device {
    struct cdev mist_cdev;            // 字符设备结构
    struct device *sysfs_dev;    // sysfs设备
    char data_buf[MIST_DATA_BUF_SIZE]; // 数据缓冲区
    struct mist_status status;   // 设备状态
    wait_queue_head_t wait_queue;// 等待队列
    struct mutex io_mutex;       // I/O互斥锁
};

// ********************* 模块参数声明 *********************
extern int mist_debug_level;
extern char *mist_target_ip;
extern int mist_timeout_ms;
int current_boost_level;
int current_GPU_load_level;
int current_preboost_level;
int current_fb_boost_level;
int current_gpu_boost_level;
int current_gpu_other_boost_level;
// 参数权限定义
#define PARAM_RO 0444
#define PARAM_RW 0644

int mist_register_device(void);
void mist_unregister_device(void);
#endif
// END Performance_TurboSched