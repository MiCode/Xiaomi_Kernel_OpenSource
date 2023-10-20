
#ifndef _GPU_PLAID_H
#define _GPU_PLAID_H
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mempolicy.h>
#include <linux/export.h>
#include <linux/rtc.h>
#define MAX_TRANSFER_SIZE (200)
enum BUFFER_STATUS
{
    ISFREED = 0,
    HASWROTE
};
struct xiaomi_gpu_plaid
{
    struct miscdevice misc_dev;
    struct device *dev;
    struct class *class;
    struct attribute_group attrs;
    char transfer_data[MAX_TRANSFER_SIZE];
    int transfer_size;
    struct mutex transfer_mutex;
    enum BUFFER_STATUS buffer_status;
};
//do not name any process to 0
/*
check here to add a new process to access this note
step 1: add enum number to GAME_NAME
step 2: add process name to game_type_array
*/
enum GAME_NAME
{
    SGAME = 1,
    PUBGMHD = 2,
    YS = 3,
};
struct game_type
{
    char *name;
    enum GAME_NAME type;
};
static struct game_type game_type_array[] = {
    {"com.tencent.tmgp.sgame", SGAME},
    {"com.tencent.tmgp.pubgmhd", PUBGMHD},
    {"com.miHoYo.ys.mi", YS},
};
#endif

