#define LOG_TAG         "Tool"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "cts_test.h"

#ifdef CONFIG_CTS_LEGACY_TOOL

#pragma pack(1)
/** Tool command structure */
struct cts_tool_cmd {
    u8 cmd;
    u8 flag;
    u8 circle;
    u8 times;
    u8 retry;
    u32 data_len;
    u8 addr_len;
    u8 addr[2];
    u8 data[PAGE_SIZE];

};
#pragma pack()

#define CTS_TOOL_CMD_HEADER_LENGTH            (12)

enum cts_tool_cmd_code {
    CTS_TOOL_CMD_GET_PANEL_PARAM = 0,
    CTS_TOOL_CMD_GET_DOWNLOAD_STATUS = 2,
    CTS_TOOL_CMD_GET_RAW_DATA = 4,
    CTS_TOOL_CMD_GET_DIFF_DATA = 6,
    CTS_TOOL_CMD_READ_HOSTCOMM = 12,
    CTS_TOOL_CMD_READ_ADC_STATUS = 14,
    CTS_TOOL_CMD_READ_GESTURE_INFO = 16,
    CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE = 18,
    CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE = 20,
    CTS_TOOL_CMD_READ_ICTYPE = 22,
    CTS_TOOL_CMD_I2C_DIRECT_READ = 24,
    CTS_TOOL_CMD_GET_DRIVER_INFO = 26,

    CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM = 1,
    CTS_TOOL_CMD_DOWNLOAD_FIRMWARE_WITH_FILENAME = 3,
    CTS_TOOL_CMD_DOWNLOAD_FIRMWARE = 5,
    CTS_TOOL_CMD_WRITE_HOSTCOMM = 11,
    CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE = 15,
    CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE = 17,
    CTS_TOOL_CMD_I2C_DIRECT_WRITE = 19,

};

struct cts_test_ioctl_data {
    __u32 ntests;
    struct cts_test_param __user *tests;
};

#define CTS_IOCTL_RDWR_REG_FLAG_RD          (0x0001)
// TODO: Flags can specify DDI level 1/2/3, read/write flag

struct cts_rdwr_reg {
    __u32 addr;
    __u32 flags;
    __u8  __user *data;
    __u32 len;
    __u32 delay_ms;
};

#define CTS_IOCTL_RDWR_REG_TYPE_FW          (1)
#define CTS_IOCTL_RDWR_REG_TYPE_HW          (2)
#define CTS_IOCTL_RDWR_REG_TYPE_DDI         (3)

#define CTS_RDWR_REG_IOCTL_MAX_REGS         (128)

struct cts_rdwr_reg_ioctl_data {
    __u8  reg_type;
    __u32 nregs;
    struct cts_rdwr_reg __user *regs;
};

#define CTS_IOCTL_UPGRADE_FW_FLAG_TO_FLASH  (0x00000001)
#define CTS_IOCTL_UPGRADE_FW_FLAG_BUILTIN   (0x00000002)
#define CTS_IOCTL_UPGRADE_FW_FLAG_FILE      (0x00000004)
#define CTS_IOCTL_UPGRADE_FW_FLAG_FW_DATA   (0x00000008)

struct cts_upgrade_fw_ioctl_data {
    __u32 flags;
    __u32 builtin_fw_index;
    const char __user *filepath;
    const __u8 __user *fw_data;
    __u32 fw_data_size;
    __u32 split_size;
};

#define CTS_TOOL_IOCTL_GET_DRIVER_VERSION   _IOR('C', 0x00, unsigned int *)
#define CTS_TOOL_IOCTL_GET_DEVICE_TYPE      _IOR('C', 0x01, unsigned int *)
#define CTS_TOOL_IOCTL_GET_FW_VERSION       _IOR('C', 0x02, unsigned short *)
#define CTS_TOOL_IOCTL_GET_RESOLUTION       _IOR('C', 0x03, unsigned int *) /* X in LSW, Y in MSW */
#define CTS_TOOL_IOCTL_GET_ROW_COL          _IOR('C', 0x04, unsigned int *) /* row in LSW, col in MSW */

#define CTS_TOOL_IOCTL_TEST                 _IOWR('C', 0x10, struct cts_test_ioctl_data *)
#define CTS_TOOL_IOCTL_RDWR_REG             _IOWR('C', 0x20, struct cts_rdwr_reg_ioctl_data *)
#define CTS_TOOL_IOCTL_UPGRADE_FW           _IOWR('C', 0x21, struct cts_upgrade_fw_ioctl_data *)

#define CTS_DRIVER_VERSION_CODE \
    ((CFG_CTS_DRIVER_MAJOR_VERSION << 16) | \
     (CFG_CTS_DRIVER_MINOR_VERSION << 8) | \
     (CFG_CTS_DRIVER_PATCH_VERSION << 0))

static struct cts_tool_cmd cts_tool_cmd;
#ifdef CFG_CTS_FIRMWARE_IN_FS
static char cts_tool_firmware_filepath[PATH_MAX];
#endif /* CFG_CTS_FIRMWARE_IN_FS */
#ifdef CONFIG_CTS_I2C_HOST
/* If CFG_CTS_MAX_I2C_XFER_SIZE < 58(PC tool length), this is neccessary */
static u32 cts_tool_direct_access_addr = 0;
#endif /* CONFIG_CTS_I2C_HOST */

static int cts_tool_open(struct inode *inode, struct file *file)
{
    file->private_data = PDE_DATA(inode);
    return 0;
}

static ssize_t cts_tool_read(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
    struct chipone_ts_data *cts_data;
    struct cts_tool_cmd *cmd;
    struct cts_device *cts_dev;
    int ret = 0;

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("Read with private_data = NULL");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_GET_PANEL_PARAM:
        cts_info("Get panel param len: %u", cmd->data_len);
        ret = cts_get_panel_param(cts_dev, cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Get panel param len: %u failed %d", cmd->data_len, ret);
        }
        break;

     case CTS_TOOL_CMD_GET_DOWNLOAD_STATUS:
        cmd->data[0] = 100;
        cts_info("Get update status = %hhu", cmd->data[0]);
        break;

    case CTS_TOOL_CMD_GET_RAW_DATA:
    case CTS_TOOL_CMD_GET_DIFF_DATA:
        cts_dbg("Get %s data row: %u col: %u len: %u",
            cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA ? "raw" : "diff",
            cmd->addr[1], cmd->addr[0], cmd->data_len);

        ret = cts_enable_get_rawdata(cts_dev);
        if (ret) {
            cts_err("Enable read raw/diff data failed %d", ret);
            break;
        }
        mdelay(1);

        if (cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA) {
            ret = cts_get_rawdata(cts_dev, cmd->data);
        } else if (cmd->cmd == CTS_TOOL_CMD_GET_DIFF_DATA) {
            ret = cts_get_diffdata(cts_dev, cmd->data);
        }
        if(ret) {
            cts_err("Get %s data failed %d",
                cmd->cmd == CTS_TOOL_CMD_GET_RAW_DATA ? "raw" : "diff", ret);
            break;
        }

        ret = cts_disable_get_rawdata(cts_dev);
        if (ret) {
            cts_err("Disable read raw/diff data failed %d", ret);
            break;
        }

        break;

    case CTS_TOOL_CMD_READ_HOSTCOMM:
        ret = cts_fw_reg_readb(cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data);
        if (ret) {
            cts_err("Read firmware reg addr 0x%04x failed %d",
                get_unaligned_le16(cmd->addr), ret);
        } else {
            cts_dbg("Read firmware reg addr 0x%04x, val=0x%02x",
                get_unaligned_le16(cmd->addr), cmd->data[0]);
        }
        break;

#ifdef CFG_CTS_GESTURE
    case CTS_TOOL_CMD_READ_GESTURE_INFO:
        ret = cts_get_gesture_info(cts_dev, cmd->data, true);
        if (ret) {
            cts_err("Get gesture info failed %d", ret);
        }
        break;
#endif /* CFG_CTS_GESTURE */

    case CTS_TOOL_CMD_READ_HOSTCOMM_MULTIBYTE:
        cmd->data_len = min((size_t)cmd->data_len, sizeof(cmd->data));
        ret = cts_fw_reg_readsb(cts_dev, get_unaligned_le16(cmd->addr),
                cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Read firmware reg addr 0x%04x len %u failed %d",
                get_unaligned_le16(cmd->addr), cmd->data_len, ret);
        } else {
            cts_dbg("Read firmware reg addr 0x%04x len %u",
                get_unaligned_le16(cmd->addr), cmd->data_len);
        }
        break;

    case CTS_TOOL_CMD_READ_PROGRAM_MODE_MULTIBYTE:
        cts_dbg("Read under program mode addr 0x%06x len %u",
            (cmd->flag << 16) | get_unaligned_le16(cmd->addr),
            cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            break;
        }

        ret = cts_sram_readsb(&cts_data->cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Read under program mode I2C xfer failed %d", ret);
            //break;
        }

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Enter normal mode failed %d", ret);
            break;
        }
        break;

    case CTS_TOOL_CMD_READ_ICTYPE:
        cts_info("Get IC type");
        if (cts_dev->hwdata) {
            switch(cts_dev->hwdata->hwid) {
            case CTS_DEV_HWID_ICNL9911: cmd->data[0] = 0x91; break;
            case CTS_DEV_HWID_ICNL9911S: cmd->data[0] = 0x91; break;
            case CTS_DEV_HWID_ICNL9911C: cmd->data[0] = 0x91; break;
            default:  cmd->data[0] = 0x00; break;
            }
        } else {
            cmd->data[0] = 0x10;
        }
        break;

#ifdef CONFIG_CTS_I2C_HOST
    case CTS_TOOL_CMD_I2C_DIRECT_READ:
        {
            u32 addr_width;
            char *wr_buff = NULL;
            u8 addr_buff[4];
            size_t left_size, max_xfer_size;
            u8 *data;

            if (cmd->addr[0] != CTS_DEV_PROGRAM_MODE_I2CADDR) {
                cmd->addr[0] = CTS_DEV_NORMAL_MODE_I2CADDR;
                addr_width = 2;
            } else {
                addr_width = cts_dev->hwdata->program_addr_width;
            }

            cts_dbg("Direct read from i2c_addr 0x%02x addr 0x%06x size %u",
                cmd->addr[0], cts_tool_direct_access_addr, cmd->data_len);

            left_size = cmd->data_len;
            max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
            data = cmd->data;
            while (left_size) {
                size_t xfer_size = min(left_size, max_xfer_size);
                ret = cts_plat_i2c_read(cts_data->pdata, cmd->addr[0],
                        wr_buff, addr_width, data, xfer_size, 1, 0);
                if (ret) {
                    cts_err("Direct read i2c_addr 0x%02x addr 0x%06x len %zu failed %d",
                        cmd->addr[0], cts_tool_direct_access_addr, xfer_size, ret);
                    break;
                }

                left_size -= xfer_size;
                if (left_size) {
                    data += xfer_size;
                    cts_tool_direct_access_addr += xfer_size;
                    if (addr_width == 2) {
                        put_unaligned_be16(cts_tool_direct_access_addr, addr_buff);
                    } else if (addr_width == 3) {
                        put_unaligned_be24(cts_tool_direct_access_addr, addr_buff);
                    }
                    wr_buff = addr_buff;
                }
            }
        }
        break;
#endif
    case CTS_TOOL_CMD_GET_DRIVER_INFO:
        break;

    default:
        cts_warn("Read unknown command %u", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    if (ret == 0) {
        if(cmd->cmd == CTS_TOOL_CMD_I2C_DIRECT_READ) {
            ret = copy_to_user(buffer + CTS_TOOL_CMD_HEADER_LENGTH,
                                cmd->data, cmd->data_len);
        } else {
            ret = copy_to_user(buffer, cmd->data, cmd->data_len);
        }
        if (ret) {
            cts_err("Copy data to user buffer failed %d", ret);
            return 0;
        }

        return cmd->data_len;
    }

    return 0;
}

static ssize_t cts_tool_write(struct file *file,
        const char __user * buffer, size_t count, loff_t * ppos)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    struct cts_tool_cmd *cmd;
    int ret = 0;

    if (count < CTS_TOOL_CMD_HEADER_LENGTH || count > PAGE_SIZE) {
        cts_err("Write len %zu invalid", count);
        return -EFAULT;
    }

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("Write with private_data = NULL");
        return -EIO;
    }

    cmd = &cts_tool_cmd;
    ret = copy_from_user(cmd, buffer, CTS_TOOL_CMD_HEADER_LENGTH);
    if (ret) {
        cts_err("Copy command header from user buffer failed %d", ret);
        return -EIO;
    } else {
        ret = CTS_TOOL_CMD_HEADER_LENGTH;
    }

    if (cmd->data_len > PAGE_SIZE) {
        cts_err("Write with invalid count %d", cmd->data_len);
        return -EIO;
    }

    if(cmd->cmd & BIT(0)) {
        if(cmd->data_len) {
            ret = copy_from_user(cmd->data,
                    buffer + CTS_TOOL_CMD_HEADER_LENGTH, cmd->data_len);
            if (ret) {
                cts_err("Copy command payload from user buffer len %u failed %d",
                    cmd->data_len, ret);
                return -EIO;
            }
        }
    } else {
        /* Prepare for read command */
        cts_dbg("Write read command(%d) header, prepare read size: %d",
            cmd->cmd, cmd->data_len);
        return CTS_TOOL_CMD_HEADER_LENGTH + cmd->data_len;
    }

    cts_dev = &cts_data->cts_dev;
    cts_lock_device(cts_dev);

    switch (cmd->cmd) {
    case CTS_TOOL_CMD_UPDATE_PANEL_PARAM_IN_SRAM:
        cts_info("Write panel param len %u data\n", cmd->data_len);
        ret = cts_fw_reg_writesb(cts_dev, CTS_DEVICE_FW_REG_PANEL_PARAM,
                cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Write panel param failed %d", ret);
            break;
        }

        ret = cts_send_command(cts_dev, CTS_CMD_RESET);
        if (ret) {

        }

        ret = cts_set_work_mode(cts_dev, 1);
        if (ret) {

        }

        mdelay(100);

        ret = cts_set_work_mode(cts_dev, 0);
        if (ret) {

        }
        mdelay(100);

        break;

#ifdef CFG_CTS_FIRMWARE_IN_FS
    case CTS_TOOL_CMD_DOWNLOAD_FIRMWARE_WITH_FILENAME:
        cts_info("Write firmware path: '%.*s'",
            cmd->data_len, cmd->data);

        memcpy(cts_tool_firmware_filepath, cmd->data, cmd->data_len);
        cts_tool_firmware_filepath[cmd->data_len] = '\0';
        break;

    case CTS_TOOL_CMD_DOWNLOAD_FIRMWARE:
        cts_info("Start download firmware path: '%s'",
            cts_tool_firmware_filepath);

        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            break;
        }

        // TODO:  Use async mode such as thread, otherwise, host can not get update status.
        ret = cts_update_firmware_from_file(cts_dev, cts_tool_firmware_filepath, true);
        if (ret) {
            cts_err("Updata firmware failed %d", ret);
            //break;
        }

        ret = cts_start_device(cts_dev);
        if (ret) {
            cts_err("Start device failed %d", ret);
            break;
        }
        break;
#endif /* CFG_CTS_FIRMWARE_IN_FS */

    case CTS_TOOL_CMD_WRITE_HOSTCOMM:
        cts_dbg("Write firmware reg addr: 0x%04x val=0x%02x",
            get_unaligned_le16(cmd->addr), cmd->data[0]);

        ret = cts_fw_reg_writeb(cts_dev,
                get_unaligned_le16(cmd->addr), cmd->data[0]);
        if (ret) {
            cts_err("Write firmware reg addr: 0x%04x val=0x%02x failed %d",
                get_unaligned_le16(cmd->addr), cmd->data[0], ret);
        }
        break;

    case CTS_TOOL_CMD_WRITE_HOSTCOMM_MULTIBYTE:
        cts_dbg("Write firmare reg addr: 0x%04x len %u",
            get_unaligned_le16(cmd->addr), cmd->data_len);
        ret = cts_fw_reg_writesb(cts_dev, get_unaligned_le16(cmd->addr),
                cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Write firmare reg addr: 0x%04x len %u failed %d",
                get_unaligned_le16(cmd->addr), cmd->data_len, ret);
        }
        break;

    case CTS_TOOL_CMD_WRITE_PROGRAM_MODE_MULTIBYTE:
        cts_dbg("Write to addr 0x%06x size %u under program mode",
            (cmd->flag << 16) | (cmd->addr[1] << 8) | cmd->addr[0],
            cmd->data_len);
        ret = cts_enter_program_mode(cts_dev);
        if (ret) {
            cts_err("Enter program mode failed %d", ret);
            break;
        }

        ret = cts_sram_writesb(cts_dev,
                (cmd->flag << 16) | (cmd->addr[1] << 8) | cmd->addr[0],
                cmd->data, cmd->data_len);
        if (ret) {
            cts_err("Write program mode multibyte failed %d", ret);
            //break;
        }

        ret = cts_enter_normal_mode(cts_dev);
        if (ret) {
            cts_err("Enter normal mode failed %d", ret);
            break;
        }

        break;

#ifdef CONFIG_CTS_I2C_HOST
    case CTS_TOOL_CMD_I2C_DIRECT_WRITE:
        {
            u32 addr_width;
            size_t left_payload_size;       /* Payload exclude address field */
            size_t max_xfer_size;
            char *payload;

            if (cmd->addr[0] != CTS_DEV_PROGRAM_MODE_I2CADDR) {
                cmd->addr[0] = CTS_DEV_NORMAL_MODE_I2CADDR;
                addr_width = 2;
                cts_tool_direct_access_addr = get_unaligned_be16(cmd->data);
            } else {
                addr_width = cts_dev->hwdata->program_addr_width;
                cts_tool_direct_access_addr = get_unaligned_be24(cmd->data);
            }

            if (cmd->data_len < addr_width) {
                cts_err("Direct write too short %d < address width %d",
                    cmd->data_len, addr_width);
                ret = -EINVAL;
                break;
            }

            cts_dbg("Direct write to i2c_addr 0x%02x addr 0x%06x size %u",
                cmd->addr[0], cts_tool_direct_access_addr, cmd->data_len);

            left_payload_size = cmd->data_len - addr_width;
            max_xfer_size = cts_plat_get_max_i2c_xfer_size(cts_dev->pdata);
            payload = cmd->data + addr_width;
            do {
                size_t xfer_payload_size = min(left_payload_size,
                        max_xfer_size - addr_width);
                size_t xfer_len = xfer_payload_size + addr_width;

                ret = cts_plat_i2c_write(cts_data->pdata, cmd->addr[0],
                        payload - addr_width, xfer_len, 1, 0);
                if (ret) {
                    cts_err("Direct write i2c_addr 0x%02x addr 0x%06x len %zu failed %d",
                        cmd->addr[0], cts_tool_direct_access_addr, xfer_len, ret);
                    break;
                }

                left_payload_size -= xfer_payload_size;
                if (left_payload_size) {
                    payload += xfer_payload_size;
                    cts_tool_direct_access_addr += xfer_payload_size;
                    if (addr_width == 2) {
                        put_unaligned_be16(cts_tool_direct_access_addr, payload - addr_width);
                    } else if (addr_width == 3) {
                        put_unaligned_be24(cts_tool_direct_access_addr, payload - addr_width);
                    }
                }
            } while (left_payload_size);
        }
        break;
#endif
    default:
        cts_warn("Write unknown command %u", cmd->cmd);
        ret = -EINVAL;
        break;
    }

    cts_unlock_device(cts_dev);

    return ret ? 0 : cmd->data_len + CTS_TOOL_CMD_HEADER_LENGTH;
}

static int cts_ioctl_test(struct cts_device *cts_dev,
    u32 ntests, struct cts_test_param *tests)
{
    u32 num_nodes = 0;
    int i, ret = 0;

    cts_info("ioctl test total %u items", ntests);

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;

    for (i = 0; i < ntests; i++) {
        bool validate_data = false;
        bool validate_data_per_node = false;
        bool validate_min = false;
        bool validate_max = false;
        bool skip_invalid_node = false;
        bool stop_test_if_validate_fail = false;
        bool dump_test_date_to_console = false;
        bool dump_test_date_to_user = false;
        bool dump_test_date_to_file = false;
        bool dump_test_date_to_file_append = false;
        bool driver_log_to_user = false;
        bool driver_log_to_file = false;
        bool driver_log_to_file_append = false;
        u32 __user *user_min_threshold = NULL;
        u32 __user *user_max_threshold = NULL;
        u32 __user *user_invalid_nodes = NULL;
        int  __user *user_test_result = NULL;
        void __user *user_test_data = NULL;
        int  __user *user_test_data_wr_size = NULL;
        const char __user *user_test_data_filepath = NULL;
        void __user *user_driver_log_buf = NULL;
        int  __user *user_driver_log_wr_size = NULL;
        const char __user *user_driver_log_filepath = NULL;
        void __user *user_priv_param = NULL;
        int test_result = 0;
        int test_data_wr_size = 0;
        int driver_log_wr_size = 0;

        cts_info("ioctl test item %d: %d(%s) flags: %08x priv param size: %d",
            i, tests[i].test_item, cts_test_item_str(tests[i].test_item),
            tests[i].flags, tests[i].priv_param_size);
        /*
         * Validate arguement
         */
        validate_data =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_DATA);
        validate_data_per_node =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
        validate_min =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_MIN);
        validate_max =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_MAX);
        skip_invalid_node =
            !!(tests[i].flags & CTS_TEST_FLAG_VALIDATE_SKIP_INVALID_NODE);
        stop_test_if_validate_fail =
            !!(tests[i].flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);
        dump_test_date_to_user =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
        dump_test_date_to_file =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
        dump_test_date_to_file_append =
            !!(tests[i].flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND);
        driver_log_to_user =
            !!(tests[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE);
        driver_log_to_file =
            !!(tests[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_FILE);
        driver_log_to_file_append =
            !!(tests[i].flags & CTS_TEST_FLAG_DRIVER_LOG_TO_FILE_APPEND);

        if (tests[i].test_result == NULL) {
            cts_err("Result pointer = NULL");
            return -EFAULT;
        }

        if (validate_data) {
            cts_info("  - Flag: Validate data");

            if (validate_data_per_node) {
                cts_info("  - Flag: Validate data per-node");
            }
            if (validate_min) {
                cts_info("  - Flag: Validate min threshold");
            }
            if (validate_max) {
                cts_info("  - Flag: Validate max threshold");
            }
            if (stop_test_if_validate_fail) {
                cts_info("  - Flag: Stop test if validate fail");
            }

            if (validate_min && tests[i].min == NULL) {
                cts_err("Min threshold pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (validate_max && tests[i].max == NULL) {
                cts_err("Max threshold pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (skip_invalid_node) {
                cts_info("  - Flag: Skip invalid node");

                if (tests[i].num_invalid_node == 0 ||
                    tests[i].num_invalid_node >= num_nodes) {
                    cts_err("Num invalid node %u out of range[0, %u]",
                        tests[i].num_invalid_node, num_nodes);
                    ret = -EINVAL;
                    goto store_result;
                }

                if (tests[i].invalid_nodes == NULL) {
                    cts_err("Invalid nodes pointer = NULL");
                    ret = -EINVAL;
                    goto store_result;
                }
            }
        }

        if (dump_test_date_to_console) {
            cts_info("  - Flag: Dump test data to console");
        }

        if (dump_test_date_to_user) {
            cts_info("  - Flag: Dump test data to user, size: %d",
                tests[i].test_data_buf_size);

            if (tests[i].test_data_buf == NULL) {
                cts_err("Test data pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].test_data_wr_size == NULL) {
                cts_err("Test data write size pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].test_data_buf_size < num_nodes) {
                cts_err("Test data size %d too small < %u",
                    tests[i].test_data_buf_size, num_nodes);
                ret = -EINVAL;
                goto store_result;
            }
        }

        if (dump_test_date_to_file) {
            cts_info("  - Flag: Dump test data to file%s",
                dump_test_date_to_file_append ? "[Append]" : "");

            if (tests[i].test_data_filepath == NULL) {
                cts_err("Test data filepath = NULL");
                ret = -EINVAL;
                goto store_result;
            }
        }

        if (driver_log_to_user) {
            cts_info("  - Flag: Dump driver log to user, size: %d",
                tests[i].driver_log_buf_size);

            if (tests[i].driver_log_buf == NULL) {
                cts_err("Driver log buf pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].driver_log_wr_size == NULL) {
                cts_err("Driver log write size pointer = NULL");
                ret = -EINVAL;
                goto store_result;
            }
            if (tests[i].driver_log_buf_size < 1024) {
                cts_err("Driver log buf size %d too small < 1024",
                    tests[i].test_data_buf_size);
                ret = -EINVAL;
                goto store_result;
            }
        }

        if (driver_log_to_file) {
            cts_info("  - Flag: Dump driver log to file",
                driver_log_to_file_append ? "[Append]" : "");

            if (tests[i].driver_log_filepath == NULL) {
                cts_err("Driver log filepath = NULL");
                ret = -EINVAL;
                goto store_result;
            }
        }

        /*
         * Dump input parameter from user,
         * Aallocate memory for output,
         * Replace __user pointer with kernel pointer.
         */
        user_test_result = (int __user *)tests[i].test_result;
        tests[i].test_result = &test_result;

        if (validate_data) {
            int num_threshold = validate_data_per_node ? num_nodes : 1;
            int threshold_size = num_threshold * sizeof(tests[i].min[0]);

            if (validate_min) {
                user_min_threshold = (int __user *)tests[i].min;
                tests[i].min = memdup_user(user_min_threshold, threshold_size);
                if (IS_ERR(tests[i].min)) {
                    ret = PTR_ERR(tests[i].min);
                    tests[i].min = NULL;
                    cts_err("Memdup min threshold from user failed %d", ret);
                    goto store_result;
                }
            } else {
                tests[i].min = NULL;
            }
            if (validate_max) {
                user_max_threshold = (int __user *)tests[i].max;
                tests[i].max = memdup_user(user_max_threshold, threshold_size);
                if (IS_ERR(tests[i].max)) {
                    ret = PTR_ERR(tests[i].max);
                    tests[i].max = NULL;
                    cts_err("Memdup max threshold from user failed %d", ret);
                    goto store_result;
                }
            } else {
                tests[i].max = NULL;
            }
            if (skip_invalid_node) {
                user_invalid_nodes = (u32 __user *)tests[i].invalid_nodes;
                tests[i].invalid_nodes = memdup_user(user_invalid_nodes,
                    tests[i].num_invalid_node * sizeof(tests[i].invalid_nodes[0]));
                if (IS_ERR(tests[i].invalid_nodes)) {
                    ret = PTR_ERR(tests[i].invalid_nodes);
                    tests[i].invalid_nodes = NULL;
                    cts_err("Memdup invalid node from user failed %d", ret);
                    goto store_result;
                }
            }
        }

        if (dump_test_date_to_user) {
            user_test_data = (void __user *)tests[i].test_data_buf;
            tests[i].test_data_buf = kmalloc(tests[i].test_data_buf_size, GFP_KERNEL);
            if (tests[i].test_data_buf == NULL) {
                ret = -ENOMEM;
                cts_err("Alloc test data mem failed");
                goto store_result;
            }
            user_test_data_wr_size = (int __user *)tests[i].test_data_wr_size;
            tests[i].test_data_wr_size = &test_data_wr_size;
        }

        if (dump_test_date_to_file) {
            user_test_data_filepath = (const char __user *)tests[i].test_data_filepath;
            tests[i].test_data_filepath = strndup_user(user_test_data_filepath, PATH_MAX);
            if (tests[i].test_data_filepath == NULL) {
                cts_err("Strdup test data filepath failed");
                goto store_result;
            }
            cts_start_dump_test_data_to_file(tests[i].test_data_filepath,
                dump_test_date_to_file_append);
        }

        if (driver_log_to_user) {
            user_driver_log_buf = (void __user *)tests[i].driver_log_buf;
            tests[i].driver_log_buf = kmalloc(tests[i].driver_log_buf_size, GFP_KERNEL);
            if (tests[i].driver_log_buf == NULL) {
                ret = -ENOMEM;
                cts_err("Alloc driver log mem failed");
                goto store_result;
            }
            user_driver_log_wr_size = (int __user *)tests[i].driver_log_wr_size;
            tests[i].driver_log_wr_size = &driver_log_wr_size;
        }

        if (driver_log_to_file) {
            user_driver_log_filepath = (const char __user *)tests[i].driver_log_filepath;
            tests[i].driver_log_filepath = strndup_user(user_driver_log_filepath, PATH_MAX);
            if (tests[i].driver_log_filepath == NULL) {
                cts_err("Strdup driver log filepath failed");
                goto store_result;
            }
            cts_info("Log driver log to file '%s'", tests[i].driver_log_filepath);
        }

        if (driver_log_to_file || driver_log_to_user) {
            ret = cts_start_driver_log_redirect(
                tests[i].driver_log_filepath, driver_log_to_file_append,
                tests[i].driver_log_buf, tests[i].driver_log_buf_size,
                tests[i].driver_log_level);
            if (ret) {
                cts_err("Start driver log redirect failed %d", ret);
                goto store_result;
            }
        }

        if (tests[i].priv_param_size && tests[i].priv_param) {
            user_priv_param = (void __user *)tests[i].priv_param;
            tests[i].priv_param = memdup_user(user_priv_param, tests[i].priv_param_size);
            if (IS_ERR(tests[i].priv_param)) {
                ret = PTR_ERR(tests[i].priv_param);
                tests[i].priv_param = NULL;
                cts_err("Memdup priv param from user failed %d", ret);
                goto store_result;
            }
        }

        /*
        * Do test
        */
        switch (tests[i].test_item) {
        case CTS_TEST_RESET_PIN:
            ret = cts_test_reset_pin(cts_dev, &tests[i]);
            break;
        case CTS_TEST_INT_PIN:
            ret = cts_test_int_pin(cts_dev, &tests[i]);
            break;
        case CTS_TEST_RAWDATA:
            ret = cts_test_rawdata(cts_dev, &tests[i]);
            break;
        case CTS_TEST_NOISE:
            ret = cts_test_noise(cts_dev, &tests[i]);
            break;
        case CTS_TEST_OPEN:
            ret = cts_test_open(cts_dev, &tests[i]);
            break;
        case CTS_TEST_SHORT:
            ret = cts_test_short(cts_dev, &tests[i]);
            break;
        case CTS_TEST_COMPENSATE_CAP:
            ret = cts_test_compensate_cap(cts_dev, &tests[i]);
            break;
        default:
            ret = ENOTSUPP;
            cts_err("Un-supported test item");
            break;
        }

        /*
         * Copy result and test data back to userspace.
         */
store_result:
        if (dump_test_date_to_user) {
            if (user_test_data != NULL && test_data_wr_size > 0) {
                cts_info("Copy test data to user, size: %d", test_data_wr_size);
                if (copy_to_user(user_test_data, tests[i].test_data_buf,
                    test_data_wr_size)) {
                    cts_err("Copy test data to user failed");
                    test_data_wr_size = 0;
                    // Skip this error
                }
            }

            if (user_test_data_wr_size != NULL) {
                put_user(test_data_wr_size, user_test_data_wr_size);
            }
        }

        if (driver_log_to_user) {
            driver_log_wr_size = cts_get_driver_log_redirect_size();
            if (user_driver_log_buf != NULL && driver_log_wr_size > 0) {
                cts_info("Copy driver log to user, size: %d", driver_log_wr_size);
                if (copy_to_user(user_driver_log_buf, tests[i].driver_log_buf,
                        driver_log_wr_size)) {
                    cts_err("Copy driver log to user failed");
                    driver_log_wr_size = 0;
                    // Skip this error
                }
            }

            if (user_driver_log_wr_size != NULL) {
                put_user(driver_log_wr_size, user_driver_log_wr_size);
            }
        }

        if (user_test_result != NULL) {
            put_user(ret, user_test_result);
        } else if (tests[i].test_result != NULL){
            put_user(ret, tests[i].test_result);
        }

        /*
         * Free memory
         */
        if (dump_test_date_to_user) {
            if (user_test_data != NULL && tests[i].test_data_buf != NULL) {
                kfree(tests[i].test_data_buf);
            }
        }
        if (validate_data) {
            if (validate_min && user_min_threshold != NULL && tests[i].min != NULL) {
                kfree(tests[i].min);
            }
            if (validate_max && user_max_threshold != NULL && tests[i].max != NULL) {
                kfree(tests[i].max);
            }
            if (skip_invalid_node) {
                if (user_invalid_nodes != NULL && tests[i].invalid_nodes != NULL) {
                    kfree(tests[i].invalid_nodes);
                }
            }
        }

        if (dump_test_date_to_file) {
            cts_stop_dump_test_data_to_file();

            if (user_test_data_filepath != NULL &&
            tests[i].test_data_filepath != NULL) {
                kfree(tests[i].test_data_filepath);
            }
        }

        if (driver_log_to_user) {
            if (user_driver_log_buf != NULL &&
                tests[i].driver_log_buf != NULL) {
                kfree(tests[i].driver_log_buf);
            }
        }

        if (driver_log_to_file) {
            if (user_driver_log_filepath != NULL &&
                tests[i].driver_log_filepath != NULL) {
                kfree(tests[i].driver_log_filepath);
            }
        }

        if (driver_log_to_file || driver_log_to_user) {
            cts_stop_driver_log_redirect();
        }

        if (user_priv_param && tests[i].priv_param) {
            kfree(tests[i].priv_param);
        }

        if (ret && stop_test_if_validate_fail) {
            break;
        }
    }

    kfree(tests);

    return ret;
}

static int cts_ioctl_rdwr_reg(struct cts_device *cts_dev,
    u8 reg_type, u32 nregs, struct cts_rdwr_reg *regs)
{
    int i, ret = 0;
    bool fw_esd_protect = false;

    cts_info("ioctl RDWR_REG type: %u total %u regs", reg_type, nregs);

    cts_lock_device(cts_dev);

    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI) {
        ret = cts_get_dev_esd_protection(cts_dev, &fw_esd_protect);
        if (ret) {
            cts_err("Get fw esd protection failed %d", ret);
            goto unlock_device;
        }

        if (fw_esd_protect) {
            ret = cts_set_dev_esd_protection(cts_dev, false);
            if (ret) {
                cts_err("Set fw esd protection failed %d", ret);
                goto unlock_device;
            }
        }

        ret = cts_dev->hwdata->enable_access_ddi_reg(cts_dev, true);
        if (ret) {
            cts_err("Enable access ddi reg failed %d", ret);
            goto recovery_fw_esd_protect;
        }
    }

    for (i = 0; i < nregs; i++) {
        struct cts_rdwr_reg *reg = regs + i;
        u8 *data = NULL;

        cts_dbg("  reg: %p flags: 0x%x data: %p len: %u delay: %u",
            reg, reg->flags, reg->data, reg->len, reg->delay_ms);

        if (reg->data == NULL || reg->len == 0) {
            cts_err("Rdwr reg(addr: 0x%06x) with data: %p or len: %u",
                reg->addr, reg->data, reg->len);
            ret = -EINVAL;
            goto disable_access_ddi_reg;
        }

        if (reg->flags & CTS_IOCTL_RDWR_REG_FLAG_RD) {
            u8 __user *user_data = reg->data;

            data = kmalloc(reg->len, GFP_KERNEL);
            if (data == NULL) {
                cts_err("Alloc mem for read reg(addr: 0x%06x len: %u) data failed",
                    reg->addr, reg->len);
                ret = -ENOMEM;
                goto disable_access_ddi_reg;
            }
            if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_FW) {
                ret = cts_fw_reg_readsb(cts_dev,
                    regs->addr, data, reg->len);
            } else {
                ret = cts_hw_reg_readsb(cts_dev,
                    regs->addr, data, reg->len);
            }
            if (ret) {
                kfree(data);
                cts_err("Read reg from addr: 0x%06x len: %u failed %d",
                    reg->addr, reg->len, ret);
                goto disable_access_ddi_reg;
            }
            if (copy_to_user(user_data, data, reg->len)) {
                kfree(data);
                cts_err("Copy reg(addr: 0x%06x len: %u) data to user failed",
                    reg->addr, reg->len);
            }
            kfree(data);
        } else {
            data = memdup_user(reg->data, reg->len);
            if (IS_ERR(data)) {
                ret = PTR_ERR(data);
                cts_err("Memdup reg(addr: 0x%06x len: %u) data from user failed",
                    reg->addr, reg->len);
                goto disable_access_ddi_reg;
            }
            if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_FW) {
                ret = cts_fw_reg_writesb(cts_dev,
                    regs->addr, data, reg->len);
            } else {
                ret = cts_hw_reg_writesb(cts_dev,
                    regs->addr, data, reg->len);
            }
            kfree(data);
            if (ret) {
                cts_err("Write reg from addr 0x%06x len %u failed %d",
                    reg->addr, reg->len, ret);
                goto disable_access_ddi_reg;
            }
        }

        if (reg->delay_ms) {
            mdelay(reg->delay_ms);
        }
    }

disable_access_ddi_reg:
    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI) {
        int r = cts_dev->hwdata->enable_access_ddi_reg(cts_dev, false);
        if (r) {
            cts_err("Disable access ddi reg failed %d", r);
        }
    }

recovery_fw_esd_protect:
    if (reg_type == CTS_IOCTL_RDWR_REG_TYPE_DDI && fw_esd_protect) {
        int r = cts_set_dev_esd_protection(cts_dev, true);
        if (r) {
            cts_err("Re-Enable fw esd protection failed %d", r);
        }
    }

unlock_device:
    cts_unlock_device(cts_dev);

    kfree(regs);

    return ret;
}

static int cts_ioctl_upgrade_fw(struct cts_device *cts_dev,
    struct cts_upgrade_fw_ioctl_data *ioctl_data)
{
    bool to_flash;
    int  ret;

    cts_info("ioctl UPGRADE-FW flags: 0x%x "
        "builtin fw index: %u filepath: %p data: %p size: %u",
        ioctl_data->flags,
        ioctl_data->builtin_fw_index, ioctl_data->filepath,
        ioctl_data->fw_data, ioctl_data->fw_data_size);

    to_flash = !!(ioctl_data->flags & CTS_IOCTL_UPGRADE_FW_FLAG_TO_FLASH);

    if (!!(ioctl_data->flags & CTS_IOCTL_UPGRADE_FW_FLAG_BUILTIN)) {
        const struct cts_firmware *firmware;

        firmware = cts_request_driver_builtin_firmware_by_index(
            ioctl_data->builtin_fw_index);
        if (firmware) {
            ret = cts_stop_device(cts_dev);
            if (ret) {
                cts_err("Stop device failed %d", ret);
                return ret;
            }

            cts_lock_device(cts_dev);
            ret = cts_update_firmware(cts_dev, firmware, to_flash);
            cts_unlock_device(cts_dev);

            if (ret) {
                cts_err("Update firmware failed %d", ret);
            }

            cts_start_device(cts_dev);

            return ret;
        } else {
            cts_err("Upgrade fw by builtin NOT found index %u",
                ioctl_data->builtin_fw_index);
            return -EINVAL;
        }
    }

    if (!!(ioctl_data->flags & CTS_IOCTL_UPGRADE_FW_FLAG_FILE)) {
        char *filepath;

        if (ioctl_data->filepath == NULL) {
            cts_err("Upgrade fw by file with filepath = NULL");
            return -EINVAL;
        }

        filepath = strndup_user(ioctl_data->filepath, PATH_MAX);
        if (IS_ERR(filepath)) {
            int ret = PTR_ERR(filepath);
            cts_err("Memdump filepath to kernel failed %d", ret);
            return ret;
        }

        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            return ret;
        }

        cts_lock_device(cts_dev);
        ret = cts_update_firmware_from_file(cts_dev,
            filepath, to_flash);
        cts_unlock_device(cts_dev);

        if (ret) {
            cts_err("Upgrade fw from file '%s' failed %d", filepath);
        }
        kfree(filepath);

        cts_start_device(cts_dev);

        return ret;
    }

    if (!!(ioctl_data->flags & CTS_IOCTL_UPGRADE_FW_FLAG_FW_DATA)) {
        struct cts_firmware firmware;

        if (ioctl_data->fw_data == NULL ||
            ioctl_data->fw_data_size < 0x102 ||
            ioctl_data->fw_data_size > 0x20000) {
            cts_err("Upgrade fw by data with "
                    "data: %p or size: %u invalid",
                ioctl_data->fw_data, ioctl_data->fw_data_size);
            return -EINVAL;
        }

        memset(&firmware, 0, sizeof(firmware));
        firmware.data = memdup_user(ioctl_data->fw_data,
            ioctl_data->fw_data_size);
        if (IS_ERR(firmware.data)) {
            int ret = PTR_ERR(firmware.data);
            cts_err("Memdump fw data to kernel failed %d", ret);
            return ret;
        }
        firmware.size = ioctl_data->fw_data_size;

        ret = cts_stop_device(cts_dev);
        if (ret) {
            cts_err("Stop device failed %d", ret);
            return ret;
        }

        cts_lock_device(cts_dev);
        ret = cts_update_firmware(cts_dev, &firmware, to_flash);
        cts_unlock_device(cts_dev);

        if (ret) {
            cts_err("Upgrade firmware data failed %d", ret);
        }
        kfree(firmware.data);

        cts_start_device(cts_dev);

        return 0;
    } else {
        cts_err("ioctl UPGRADE-FW both filepath and data = NULL");
        return -EINVAL;
    }
}

static long cts_tool_ioctl(struct file *file, unsigned int cmd,
        unsigned long arg)
{
    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;

    cts_info("ioctl, cmd=0x%08x, arg=0x%08lx", cmd, arg);

    cts_data = file->private_data;
    if (cts_data == NULL) {
        cts_err("IOCTL with private data = NULL");
        return -EFAULT;
    }

    cts_dev = &cts_data->cts_dev;

    switch (cmd) {
    case CTS_TOOL_IOCTL_GET_DRIVER_VERSION:
        return put_user(CTS_DRIVER_VERSION_CODE,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_DEVICE_TYPE:
        return put_user(cts_dev->hwdata->hwid,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_FW_VERSION:
        return put_user(cts_dev->fwdata.version,
                (unsigned short __user *)arg);
    case CTS_TOOL_IOCTL_GET_RESOLUTION:
        return put_user((cts_dev->fwdata.res_y << 16) + cts_dev->fwdata.res_x,
                (unsigned int __user *)arg);
    case CTS_TOOL_IOCTL_GET_ROW_COL:
        return put_user((cts_dev->fwdata.cols << 16) + cts_dev->fwdata.rows,
                (unsigned int __user *)arg);

    case CTS_TOOL_IOCTL_TEST:{
        struct cts_test_ioctl_data test_arg;
        struct cts_test_param *tests_pa;

        if (copy_from_user(&test_arg,
                (struct cts_test_ioctl_data __user *)arg,
                sizeof(test_arg))) {
            cts_err("Copy ioctl test arg to kernel failed");
            return -EFAULT;
        }

        if (test_arg.ntests > 8) {
            cts_err("ioctl test with too many tests %u",
                test_arg.ntests);
            return -EINVAL;
        }

        tests_pa = memdup_user(test_arg.tests,
            test_arg.ntests * sizeof(struct cts_test_param));
        if (IS_ERR(tests_pa)) {
            int ret = PTR_ERR(tests_pa);
            cts_err("Memdump test param to kernel failed %d", ret);
            return ret;
        }

        return cts_ioctl_test(cts_dev, test_arg.ntests, tests_pa);
    }
    case CTS_TOOL_IOCTL_RDWR_REG:{
        struct cts_rdwr_reg_ioctl_data ioctl_data;
        struct cts_rdwr_reg *regs_pa;

        if (copy_from_user(&ioctl_data,
                (struct cts_rdwr_reg_ioctl_data __user *)arg,
                sizeof(ioctl_data))) {
            cts_err("Copy ioctl rdwr_reg arg to kernel failed");
            return -EFAULT;
        }

        if (ioctl_data.nregs > CTS_RDWR_REG_IOCTL_MAX_REGS) {
            cts_err("ioctl rdwr_reg with too many regs %u",
                ioctl_data.nregs);
            return -EINVAL;
        }

        regs_pa = memdup_user(ioctl_data.regs,
            ioctl_data.nregs * sizeof(struct cts_rdwr_reg));
        if (IS_ERR(regs_pa)) {
            int ret = PTR_ERR(regs_pa);
            cts_err("Memdump cts_rdwr_reg to kernel failed %d", ret);
            return ret;
        }

        return cts_ioctl_rdwr_reg(cts_dev,
            ioctl_data.reg_type, ioctl_data.nregs, regs_pa);
    }
    case CTS_TOOL_IOCTL_UPGRADE_FW:{
        struct cts_upgrade_fw_ioctl_data ioctl_data;

        if (copy_from_user(&ioctl_data,
                (struct cts_upgrade_fw_ioctl_data __user *)arg,
                sizeof(ioctl_data))) {
            cts_err("Copy ioctl UPGRADE-FW arg to kernel failed");
            return -EFAULT;
        }

        return cts_ioctl_upgrade_fw(cts_dev, &ioctl_data);
    }
    default:
        cts_err("Unsupported ioctl cmd=0x%08x, arg=0x%08lx", cmd, arg);
        break;
    }

    return -ENOTSUPP;
}

static struct file_operations cts_tool_fops = {
    .owner = THIS_MODULE,
    .llseek = no_llseek,
    .open  = cts_tool_open,
    .read  = cts_tool_read,
    .write = cts_tool_write,
    .unlocked_ioctl = cts_tool_ioctl,
};

int cts_tool_init(struct chipone_ts_data *cts_data)
{
    cts_info("Init");

    cts_data->procfs_entry = proc_create_data(CFG_CTS_TOOL_PROC_FILENAME,
            0666, NULL, &cts_tool_fops, cts_data);
    if (cts_data->procfs_entry == NULL) {
        cts_err("Create proc entry failed");
        return -EFAULT;
    }

    return 0;
}

void cts_tool_deinit(struct chipone_ts_data *data)
{
    cts_info("Deinit");

    if (data->procfs_entry) {
        remove_proc_entry(CFG_CTS_TOOL_PROC_FILENAME, NULL);
    }
}
#endif /* CONFIG_CTS_LEGACY_TOOL */

