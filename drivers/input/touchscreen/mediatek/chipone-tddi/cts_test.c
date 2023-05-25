#define LOG_TAG         "Test"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_test.h"

const char *cts_test_item_str(int test_item)
{
#define case_test_item(item) \
    case CTS_TEST_ ## item: return #item "-TEST"

    switch (test_item) {
        case_test_item(RESET_PIN);
        case_test_item(INT_PIN);
        case_test_item(RAWDATA);
        case_test_item(NOISE);
        case_test_item(OPEN);
        case_test_item(SHORT);
        case_test_item(COMPENSATE_CAP);

        default: return "INVALID";
    }
#undef case_test_item
}

#define CTS_FIRMWARE_WORK_MODE_NORMAL   (0x00)
#define CTS_FIRMWARE_WORK_MODE_FACTORY  (0x01)
#define CTS_FIRMWARE_WORK_MODE_CONFIG   (0x02)
#define CTS_FIRMWARE_WORK_MODE_TEST     (0x03)

#define CTS_TEST_SHORT                  (0x01)
#define CTS_TEST_OPEN                   (0x02)

#define CTS_SHORT_TEST_UNDEFINED        (0x00)
#define CTS_SHORT_TEST_BETWEEN_COLS     (0x01)
#define CTS_SHORT_TEST_BETWEEN_ROWS     (0x02)
#define CTS_SHORT_TEST_BETWEEN_GND      (0x03)

#define TEST_RESULT_BUFFER_SIZE(cts_dev) \
    (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

#define RAWDATA_BUFFER_SIZE(cts_dev) \
        (cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

static int disable_fw_monitor_mode(struct cts_device *cts_dev)
{
    int ret;
    u8 value;

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_FLAG_BITS, &value);
    if (ret) {
        return ret;
    }

    if (value & BIT(0)) {
        return cts_fw_reg_writeb(cts_dev,
            CTS_DEVICE_FW_REG_FLAG_BITS, value & (~BIT(0)));
    }

    return 0;
}

static int disable_fw_auto_compensate(struct cts_device *cts_dev)
{
    return cts_fw_reg_writeb(cts_dev,
        CTS_DEVICE_FW_REG_AUTO_CALIB_COMP_CAP_ENABLE, 0);
}

static int set_fw_work_mode(struct cts_device *cts_dev, u8 mode)
{
    int ret, retries;
    u8  pwr_mode;

    cts_info("Set firmware work mode to %u", mode);

    ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_WORK_MODE, mode);
    if (ret) {
        cts_err("Write firmware work mode register failed %d", ret);
        return ret;
    }

    ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_POWER_MODE,
        &pwr_mode);
    if (ret) {
        cts_err("Read firmware power mode register failed %d", ret);
        return ret;
    }

    if (pwr_mode == 1) {
        ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
        if (ret) {
            cts_err("Send cmd QUIT_GESTURE_MONITOR failed %d", ret);
            return ret;
        }

        msleep(50);
    }

    retries = 0;
    do {
        u8 sys_busy, curr_mode;

        msleep(10);

        ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_SYS_BUSY,
            &sys_busy);
        if (ret) {
            cts_err("Read firmware system busy register failed %d", ret);
            //return ret;
            continue;
        }
        if (sys_busy)
            continue;

        ret = cts_fw_reg_readb(cts_dev, CTS_DEVICE_FW_REG_GET_WORK_MODE,
            &curr_mode);
        if (ret) {
            cts_err("Read firmware current work mode failed %d", ret);
            //return ret;
            continue;
        }

        if (curr_mode == mode /*|| curr_mode == 0xFF*/) {
            break;
        }
    } while (retries++ < 1000);

    return (retries >= 1000 ? -ETIMEDOUT : 0);
}

static int set_display_state(struct cts_device *cts_dev, bool active)
{
    int ret;
    u8  access_flag;

    cts_info("Set display state to %s", active ? "ACTIVE" : "SLEEP");

    ret = cts_hw_reg_readb(cts_dev, 0x3002C, &access_flag);
    if (ret) {
        cts_err("Read display access flag failed %d", ret);
        return ret;
    }

    ret = cts_hw_reg_writeb(cts_dev, 0x3002C, access_flag | 0x01);
    if (ret) {
        cts_err("Write display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    if (active) {
        ret = cts_hw_reg_writeb(cts_dev, 0x3C044, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD11 fail");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb(cts_dev, 0x3C0A4, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD29 fail");
            return ret;
        }

        msleep(100);
    } else {
        ret = cts_hw_reg_writeb(cts_dev, 0x3C0A0, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD28 fail");
            return ret;
        }

        msleep(100);

        ret = cts_hw_reg_writeb(cts_dev, 0x3C040, 0x55);
        if (ret) {
            cts_err("Write DCS-CMD10 fail");
            return ret;
        }

        msleep(100);
    }

    ret = cts_hw_reg_writeb(cts_dev, 0x3002C, access_flag);
    if (ret) {
        cts_err("Restore display access flag %02x failed %d", access_flag, ret);
        return ret;
    }

    return 0;
}

static int wait_test_complete(struct cts_device *cts_dev, int skip_frames)
{
    int ret, i, j;

    cts_info("Wait test complete skip %d frames", skip_frames);

    for (i = 0; i < (skip_frames + 1); i++) {
        u8 ready;

        for (j = 0; j < 1000; j++) {
            mdelay(1);

            ready = 0;
            ret = cts_get_data_ready_flag(cts_dev, &ready);
            if (ret) {
                cts_err("Get data ready flag failed %d", ret);
                return ret;
            }

            if (ready) {
                break;
            }
        }

        if (ready == 0) {
            cts_err("Wait test complete timeout");
            return -ETIMEDOUT;
        }
        if (i < skip_frames) {
            ret = cts_clr_data_ready_flag(cts_dev);
            if (ret) {
                cts_err("Clr data ready flag failed %d", ret);
                return ret;
            }
        }
    }

    return 0;
}

static int get_test_result(struct cts_device *cts_dev, u16 *result)
{
    int ret;

    ret = cts_fw_reg_readsb(cts_dev, CTS_DEVICE_FW_REG_RAW_DATA, result,
            TEST_RESULT_BUFFER_SIZE(cts_dev));
    if (ret) {
        cts_err("Get test result data failed %d", ret);
        return ret;
    }

    ret = cts_clr_data_ready_flag(cts_dev);
    if (ret) {
        cts_err("Clear data ready flag failed %d", ret);
        return ret;
    }

    return 0;
}

static int set_fw_test_type(struct cts_device *cts_dev, u8 type)
{
    int ret, retries = 0;
    u8  sys_busy;
    u8  type_readback;

    cts_info("Set test type %d", type);

    ret = cts_fw_reg_writeb(cts_dev, 0x34, type);
    if (ret) {
        cts_err("Write test type register to failed %d", ret);
        return ret;
    }

    do {
        msleep(1);

        ret = cts_fw_reg_readb(cts_dev, 0x01, &sys_busy);
        if (ret) {
            cts_err("Read system busy register failed %d", ret);
            return ret;
        }
    } while (sys_busy && retries++ < 1000);

    if (retries >= 1000) {
        cts_err("Wait system ready timeout");
        return -ETIMEDOUT;
    }

    ret = cts_fw_reg_readb(cts_dev, 0x34, &type_readback);
    if (ret) {
        cts_err("Read test type register failed %d", ret);
        return ret;
    }

    if (type != type_readback) {
        cts_err("Set test type %u != readback %u", type, type_readback);
        return -EFAULT;
    }

    return 0;
}

static bool set_short_test_type(struct cts_device *cts_dev, u8 type)
{
    static struct fw_short_test_param {
        u8  type;
        u32 col_pattern[2];
        u32 row_pattern[2];
    } param = {
        .type = CTS_SHORT_TEST_BETWEEN_COLS,
        .col_pattern = {0, 0},
        .row_pattern = {0, 0}
    };
    int i=0, ret=0;

    cts_info("Set short test type to %u", type);

    param.type = type;
    for (i = 0; i < 5; i++) {
        u8 type_readback;

        ret = cts_fw_reg_writesb(cts_dev, 0x5000, &param, sizeof(param));
        if (ret) {
            cts_err("Set short test type to %u failed %d", type, ret);
            continue;
        }
        ret = cts_fw_reg_readb(cts_dev, 0x5000, &type_readback);
        if (ret) {
            cts_err("Get short test type failed %d", ret);
            continue;
        }
        if (type == type_readback) {
            return 0;
        } else {
            cts_err("Set test type %u != readback %u", type, type_readback);
            continue;
        }
    }

    return ret;
}

int cts_write_file(struct file *filp, const void *data, size_t size)
{
    loff_t  pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
}

struct file *cts_test_data_filp = NULL;
int cts_start_dump_test_data_to_file(const char *filepath, bool append_to_file)
{
#define START_BANNER \
        ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    cts_info("Start dump test data to file '%s'", filepath);

    cts_test_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_test_data_filp)) {
        int ret = PTR_ERR(cts_test_data_filp);
        cts_test_data_filp = NULL;
        cts_err("Open file '%s' for test data failed %d",
            cts_test_data_filp, ret);
        return ret;
    }

    cts_write_file(cts_test_data_filp, START_BANNER, strlen(START_BANNER));

    return 0;
#undef START_BANNER
}

void cts_stop_dump_test_data_to_file(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
    int r;

    cts_info("Stop dump test data to file");

    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp,
            END_BANNER, strlen(END_BANNER));
        r = filp_close(cts_test_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
        cts_test_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }
#undef END_BANNER
}

static void cts_dump_tsdata(struct cts_device *cts_dev,
        const char *desc, const u16 *data, bool to_console)
{
#define SPLIT_LINE_STR \
    "---------------------------------------------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[128];
    int count = 0;

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            u16 val = data[r * cts_dev->fwdata.cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < cts_dev->fwdata.cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count +=
                scnprintf(line_buf + count, sizeof(line_buf) - count,
                    DATA_FORMAT_STR,
                    data[r * cts_dev->fwdata.cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

static bool is_invalid_node(u32 *invalid_nodes, u32 num_invalid_nodes,
    u16 row, u16 col)
{
    int i;

    for (i = 0; i < num_invalid_nodes; i++) {
        if (MAKE_INVALID_NODE(row,col)== invalid_nodes[i]) {
            return true;
        }
    }

    return false;
}

static int validate_tsdata(struct cts_device *cts_dev,
    const char *desc, u16 *data,
    u32 *invalid_nodes, u32 num_invalid_nodes,
    bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    int failed_cnt = 0;

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r,c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u",
                    failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static int validate_comp_cap(struct cts_device *cts_dev,
    const char *desc, u8 *cap,
    u32 *invalid_nodes, u32 num_invalid_nodes,
    bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    int failed_cnt = 0;

    cts_info("Validate %s data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r,c)) {
                continue;
            }

            if ((min != NULL && cap[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && cap[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u",
                    failed_cnt, r, c, cap[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static int wait_fw_to_normal_work(struct cts_device *cts_dev)
{
    int i = 0;
    int ret;

    cts_info ("Wait fw to normal work");

    do {
        u8 work_mode;

        ret = cts_fw_reg_readb(cts_dev,
            CTS_DEVICE_FW_REG_GET_WORK_MODE, &work_mode);
        if (ret) {
            cts_err("Get fw curr work mode failed %d", work_mode);
            continue;
        } else {
            if (work_mode == CTS_FIRMWARE_WORK_MODE_NORMAL) {
                return 0;
            }
        }

        mdelay (10);
    } while (++i < 100);

    return ret ? ret : -ETIMEDOUT;
}

static int prepare_test(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Prepare test");

    cts_plat_reset_device(cts_dev->pdata);

    ret = cts_set_dev_esd_protection(cts_dev, false);
    if (ret) {
        cts_err("Disable firmware ESD protection failed %d", ret);
        return ret;
    }

    ret = disable_fw_monitor_mode(cts_dev);
    if (ret) {
        cts_err("Disable firmware monitor mode failed %d", ret);
        return ret;
    }

    ret = disable_fw_auto_compensate(cts_dev);
    if (ret) {
        cts_err("Disable firmware auto compensate failed %d", ret);
        return ret;
    }

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_CONFIG);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        return ret;
    }

    cts_dev->rtdata.testing = true;

    return 0;
}

static void post_test(struct cts_device *cts_dev)
{
    int ret;

    cts_info("Post test");

    cts_plat_reset_device(cts_dev->pdata);

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_NORMAL);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_NORMAL failed %d", ret);
    }

    ret = wait_fw_to_normal_work(cts_dev);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        //return ret;
    }

    cts_dev->rtdata.testing = false;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_short(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_if_failed = false;
    bool dump_test_date_to_user = false;
    bool dump_test_date_to_console = false;
    bool dump_test_date_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  loopcnt;
    int  ret;
    u16 *test_result = NULL;
    bool recovery_display_state = false;
    u8   need_display_on;
    u8   feature_ver;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Short test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_date_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_date_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_date_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_if_failed =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Short test, flags: 0x%08x,"
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    if (dump_test_date_to_user) {
        test_result = (u16 *)param->test_data_buf;
    } else {
        test_result = (u16 *)kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate test result buffer failed");
            return -ENOMEM;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);

    ret = prepare_test(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto err_free_test_result;
    }

    cts_info("Test short to GND");

    ret = cts_sram_readb(cts_dev, 0xE8, &feature_ver);
    if (ret) {
        cts_err("Read firmware feature version failed %d", ret);
        goto err_free_test_result;
    }
    cts_info("Feature version: %u", feature_ver);

    if (feature_ver > 0) {
        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_UNDEFINED);
        if (ret) {
            cts_err("Set short test type to UNDEFINED failed %d", ret);
            goto err_free_test_result;
        }

        ret = set_fw_test_type(cts_dev, CTS_TEST_SHORT);
        if (ret) {
            cts_err("Set test type to SHORT failed %d", ret);
            goto err_free_test_result;
        }

        ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
        if (ret) {
            cts_err("Set firmware work mode to WORK_MODE_TEST failed %d",
                ret);
            goto err_free_test_result;
        }

        if (feature_ver <= 3) {
            u8 val;

            cts_info("Patch short test issue");

            ret = cts_hw_reg_readb(cts_dev, 0x350E2, &val);
            if (ret) {
                cts_err("Read 0x350E2 failed %d", ret);
                return ret;
            }
            if ((val & (BIT(2) | BIT(5))) != 0) {
                ret = cts_hw_reg_writeb(cts_dev, 0x350E2, val & 0xDB);
                if (ret) {
                    cts_err("Write 0x350E2 failed %d", ret);
                    return ret;
                }
            }
        }

        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_GND);
        if (ret) {
            cts_err("Set short test type to SHORT_TO_GND failed %d", ret);
            goto err_free_test_result;
        }

        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            cts_err("Wait test complete failed %d", ret);
            goto err_free_test_result;
        }
    } else {
        ret = cts_send_command(cts_dev, CTS_CMD_RECOVERY_TX_VOL);
        if (ret) {
            cts_err("Send command RECOVERY_TX_VOL failed %d", ret);
            goto err_free_test_result;
        }

        ret = wait_test_complete(cts_dev, 2);
        if (ret) {
            cts_err("Wait test complete failed %d", ret);
            goto err_free_test_result;
        }

        // TODO: In factory mode
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto err_free_test_result;
    }

    if (dump_test_date_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_date_to_console || dump_test_date_to_file) {
        cts_dump_tsdata(cts_dev, "GND-short", test_result,
            dump_test_date_to_console);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "GND-short",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
        if (ret) {
            cts_err("Short to GND test failed %d", ret);
            if (stop_if_failed) {
                goto err_free_test_result;
            }
        }
    }

    if (dump_test_date_to_user) {
        test_result += num_nodes;
    }

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON, &need_display_on);
    if (ret) {
        cts_err("Read need display on register failed %d", ret);
        goto err_free_test_result;
    }

    if (need_display_on == 0) {
        ret = set_display_state(cts_dev, false);
        if (ret) {
            cts_err("Set display state to SLEEP failed %d", ret);
            goto err_free_test_result;
        }
        recovery_display_state = true;
    }

    /*
     * Short between colums
     */
    cts_info("Test short between columns");

#if 0
    ret = set_fw_test_type(cts_dev, CTS_TEST_SHORT);
    if (ret) {
        cts_err("Set test type to SHORT failed %d", ret);
        return ret;
    }
#endif

    ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_COLS failed %d", ret);
        goto err_recovery_display_state;
    }

#if 0
    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_TEST failed %d",
            ret);
        return ret;
    }
#endif

    if (need_display_on == 0) {
        cts_info("Skip first frame data");

        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            cts_err("Wait test complete failed %d", ret);
            goto err_recovery_display_state;
        }

        ret = get_test_result(cts_dev, test_result);
        if (ret) {
            cts_err("Read skip test result failed %d", ret);
            goto err_recovery_display_state;
        }

        ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
        if (ret) {
            cts_err("Set short test type to BETWEEN_COLS failed %d",
                ret);
            goto err_recovery_display_state;
        }
    }

    ret = wait_test_complete(cts_dev, 0);
    if (ret) {
        cts_err("Wait test complete failed %d", ret);
        goto err_recovery_display_state;
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto err_recovery_display_state;
    }

    if (dump_test_date_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_date_to_console || dump_test_date_to_file) {
        cts_dump_tsdata(cts_dev, "Col-short", test_result,
            dump_test_date_to_console);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Col-short",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
        if (ret) {
            cts_err("Short between columns test failed %d", ret);
            if (stop_if_failed) {
                goto err_recovery_display_state;
            }
        }
    }

    if (dump_test_date_to_user) {
        test_result += num_nodes;
    }

    /*
     * Short between colums
     */
    cts_info("Test short between rows");

    ret = set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_ROWS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_ROWS failed %d", ret);
        goto err_recovery_display_state;
    }

    loopcnt = cts_dev->hwdata->num_row;
    while (loopcnt > 1) {
        ret = wait_test_complete(cts_dev, 0);
        if (ret) {
            cts_err("Wait test complete failed %d", ret);
            goto err_recovery_display_state;
        }

        ret = get_test_result(cts_dev, test_result);
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto err_recovery_display_state;
        }

        if (dump_test_date_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_date_to_console || dump_test_date_to_file) {
            cts_dump_tsdata(cts_dev, "Row-short", test_result,
                dump_test_date_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Row-short",
                test_result, param->invalid_nodes, param->num_invalid_node,
                validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between columns test failed %d", ret);
                if (stop_if_failed) {
                    goto err_recovery_display_state;
                }
            }
        }

        if (dump_test_date_to_user) {
            test_result += num_nodes;
        }

        loopcnt += loopcnt % 2;
        loopcnt = loopcnt >> 1;
    }

err_recovery_display_state:
    if (recovery_display_state) {
        int r = set_display_state(cts_dev, true);
        if (r) {
            cts_err("Set display state to ACTIVE failed %d", r);
        }
    }
err_free_test_result:
    if (!dump_test_date_to_user && test_result) {
        kfree(test_result);
    }
    post_test(cts_dev);

    cts_unlock_device(cts_dev);

    cts_info("Short test %s", ret ? "FAILED" : "PASSED");

    cts_start_device(cts_dev);

    return ret;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_open(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_date_to_user = false;
    bool dump_test_date_to_console = false;
    bool dump_test_date_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  ret;
    u16 *test_result = NULL;
    bool recovery_display_state = false;
    u8   need_display_on;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Open test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_date_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_date_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_date_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Open test, flags: 0x%08x,"
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    if (dump_test_date_to_user) {
        test_result = (u16 *)param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate memory for test result faild");
            return -ENOMEM;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);
    ret = prepare_test(cts_dev);
    if (ret) {
        cts_err("Prepare test failed %d", ret);
        goto err_free_test_result;
    }

    ret = cts_fw_reg_readb(cts_dev,
        CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON, &need_display_on);
    if (ret) {
        cts_err("Read need display on register failed %d", ret);
        goto err_free_test_result;
    }

    if (need_display_on == 0) {
        ret = set_display_state(cts_dev, false);
        if (ret) {
            cts_err("Set display state to SLEEP failed %d", ret);
            goto err_free_test_result;
        }
        recovery_display_state = true;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_RECOVERY_TX_VOL);
    if (ret) {
        cts_err("Recovery tx voltage failed %d", ret);
        goto err_recovery_display_state;
    }

    ret = set_fw_test_type(cts_dev, CTS_TEST_OPEN);
    if (ret) {
        cts_err("Set test type to OPEN_TEST failed %d", ret);
        goto err_recovery_display_state;
    }

    ret = set_fw_work_mode(cts_dev, CTS_FIRMWARE_WORK_MODE_TEST);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_TEST failed %d",
            ret);
        goto err_recovery_display_state;
    }

    ret = wait_test_complete(cts_dev, 2);
    if (ret) {
        cts_err("Wait test complete failed %d", ret);
        goto err_recovery_display_state;
    }

    ret = get_test_result(cts_dev, test_result);
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto err_recovery_display_state;
    }

    if (dump_test_date_to_user) {
        *param->test_data_wr_size += tsdata_frame_size;
    }

    if (dump_test_date_to_console || dump_test_date_to_file) {
        cts_dump_tsdata(cts_dev, "Open-circuit", test_result,
            dump_test_date_to_console);
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Open-circuit",
            test_result, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
    }
err_recovery_display_state:
    if (recovery_display_state) {
        int r = set_display_state(cts_dev, true);
        if (r) {
            cts_err("Set display state to ACTIVE failed %d", r);
        }
    }
err_free_test_result:
    if (!dump_test_date_to_user && test_result) {
        kfree(test_result);
    }
    post_test(cts_dev);

    cts_unlock_device(cts_dev);
    cts_info("Open test %s", ret ? "FAILED" : "PASSED");

    cts_start_device(cts_dev);

    return ret;
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_test_reset_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    int ret;
    int val = 0;

    if (cts_dev == NULL || param == NULL) {
        return -EINVAL;
    }

    cts_info("Reset Pin test, flags: 0x%08x, "
               "drive log file: '%s' buf size: %d",
        param->flags,
        param->driver_log_filepath, param->driver_log_buf_size);

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);

    cts_plat_set_reset(cts_dev->pdata, 0);
    mdelay(50);
#ifdef CONFIG_CTS_I2C_HOST
    /* Check whether device is in normal mode */
    if (!cts_plat_is_i2c_online(cts_dev->pdata,
                    CTS_DEV_NORMAL_MODE_I2CADDR)) {
#else
    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
#endif /* CONFIG_CTS_I2C_HOST */
        val++;
    } else {
        cts_err("Device is alive while reset is low");
    }
    cts_plat_set_reset(cts_dev->pdata, 1);
    mdelay(50);

    ret = wait_fw_to_normal_work(cts_dev);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

#ifdef CONFIG_CTS_I2C_HOST
    /* Check whether device is in normal mode */
    if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR)) {
#else
    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
#endif /* CONFIG_CTS_I2C_HOST */
        val++;
    } else {
        cts_err("Device is offline while reset is high");
    }
#ifdef CONFIG_CTS_CHARGER_DETECT
    if (cts_is_charger_exist(cts_dev)) {
        int r = cts_set_dev_charger_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev charger attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
    if (cts_dev->fwdata.supp_headphone_cable_reject &&
        cts_is_earjack_exist(cts_dev)) {
        int r = cts_set_dev_earjack_attached(cts_dev, true);
        if (r) {
            cts_err("Set dev earjack attached failed %d", r);
        }
    }
#endif /* CONFIG_CTS_EARJACK_DETECT */


#ifdef CONFIG_CTS_GLOVE
    if (cts_is_glove_enabled(cts_dev)) {
        cts_enter_glove_mode(cts_dev);
    }
#endif

#ifdef CFG_CTS_FW_LOG_REDIRECT
    if (cts_is_fw_log_redirect(cts_dev)) {
        cts_enable_fw_log_redirect(cts_dev);
    }
#endif

    cts_unlock_device(cts_dev);

    ret = cts_start_device(cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
    }

    cts_info("Reset-Pin test %s", val == 2 ? "PASS" : "FAIL");
    if (val == 2) {
        if (!cts_dev->rtdata.program_mode) {
            cts_set_normal_addr(cts_dev);
        }
        return 0;
    }

    return -EFAULT;
}
#endif

int cts_test_int_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    int ret;

    if (cts_dev == NULL || param == NULL) {
        return -EINVAL;
    }

    cts_info("Int Pin test, flags: 0x%08x, "
               "drive log file: '%s' buf size: %d",
        param->flags,
        param->driver_log_filepath, param->driver_log_buf_size);

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);

    ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_HIGH);
    if (ret) {
        cts_err("Send command WRTITE_INT_HIGH failed %d", ret);
        goto unlock_device;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) == 0) {
        cts_err("INT pin state != HIGH");
        ret = -EFAULT;
        goto unlock_device;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_WRTITE_INT_LOW);
    if (ret) {
        cts_err("Send command WRTITE_INT_LOW failed %d", ret);
        goto unlock_device;
    }
    mdelay(10);
    if (cts_plat_get_int_pin(cts_dev->pdata) != 0) {
        cts_err("INT pin state != LOW");
        ret = -EFAULT;
        goto unlock_device;
    }

    ret = cts_send_command(cts_dev, CTS_CMD_RELASE_INT_TEST);
    if (ret) {
        cts_err("Send command RELASE_INT_TEST failed %d", ret);
        ret = 0;    // Ignore this error
    }
    mdelay(10);

unlock_device:
    cts_unlock_device(cts_dev);

    ret = cts_start_device(cts_dev);
    if (ret) {
        cts_err("Start device failed %d", ret);
        ret = 0;    // Ignore this error
    }

    cts_info("Int-Pin test %s", ret == 0 ? "PASS" : "FAIL");
    return ret;
}

void cts_dump_comp_cap(struct cts_device *cts_dev, u8 *cap, bool to_console)
{
#define SPLIT_LINE_STR \
            "-----------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%3u "
#define DATA_FORMAT_STR     "%4d"

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[128];
    int count;

    max = min = cap[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            u16 val = cap[r * cts_dev->fwdata.cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
              " Compensate Cap MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
              min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "      ");
    for (c = 0; c < cts_dev->fwdata.cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                  COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                  ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            count += scnprintf(line_buf + count,
                      sizeof(line_buf) - count,
                      DATA_FORMAT_STR,
                      cap[r * cts_dev->fwdata.cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }

    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_date_to_user = false;
    bool dump_test_date_to_console = false;
    bool dump_test_date_to_file = false;
    int  num_nodes;
    u8 * cap = NULL;
    int  ret = 0;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Compensate cap test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_date_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_date_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_date_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Compensate cap test, flags: 0x%08x "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d",
        param->flags, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    if (dump_test_date_to_user) {
        cap = (u8 *)param->test_data_buf;
    } else {
        cap = (u8 *)kzalloc(num_nodes, GFP_KERNEL);
        if (cap == NULL) {
            cts_err("Allocate mem for compensate cap failed %d", ret);
            return -ENOMEM;
        }
    }

    cts_lock_device(cts_dev);
    ret = cts_get_compensate_cap(cts_dev, cap);
    cts_unlock_device(cts_dev);
    if (ret) {
        cts_err("Get compensate cap failed %d", ret);
        goto free_cap;
    }

    if (dump_test_date_to_user) {
        *param->test_data_wr_size = num_nodes;
    }

    if (dump_test_date_to_console || dump_test_date_to_file) {
        cts_dump_comp_cap(cts_dev, cap,
            dump_test_date_to_console);
    }

    if (driver_validate_data) {
        ret = validate_comp_cap(cts_dev, "Compensate-Cap",
            cap, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
    }

free_cap:
    if (!dump_test_date_to_user && cap) {
        kfree(cap);
    }

    cts_info("Compensate-Cap test %s", ret == 0 ? "PASS" : "FAIL");

    return ret;
}

int cts_test_rawdata(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_date_to_user = false;
    bool dump_test_date_to_console = false;
    bool dump_test_date_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    u16 *rawdata = NULL;
    int  i;
    int  ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Rawdata test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Rawdata test with too little frame %u",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_date_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_date_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_date_to_file=
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Rawdata test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    if (dump_test_date_to_user) {
        rawdata = (u16 *)param->test_data_buf;
    } else {
        rawdata = (u16 *)kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (rawdata == NULL) {
            cts_err("Allocate memory for rawdata failed");
            return -ENOMEM;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);

		if (rawdata != NULL) { //For memory leak;
      		kfree(rawdata);
   		}
		
        return ret;
    }

    cts_lock_device(cts_dev);

    for (i = 0; i < 5; i++) {
        int r;
        u8 val;
        r = cts_enable_get_rawdata(cts_dev);
        if (r) {
            cts_err("Enable get tsdata failed %d", r);
            continue;
        }
        mdelay(1);
        r = cts_fw_reg_readb(cts_dev, 0x12, &val);
        if (r) {
            cts_err("Read enable get tsdata failed %d", r);
            continue;
        }
        if (val != 0) {
            break;
        }
    }

    if (i >= 5) {
        cts_err("Enable read tsdata failed");
        ret = -EIO;
        goto unlock_device;
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;

        //if (cts_dev.fwdata.monitor_mode) {
            ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
            if (ret) {
                cts_err("Send CMD_QUIT_GESTURE_MONITOR failed %d", ret);
            }
        //}

        for (i = 0; i < 3; i++) {
            ret = cts_get_rawdata(cts_dev, rawdata);
            if (ret) {
                cts_err("Get rawdata failed %d", ret);
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            ret = -EIO;
            break;
        }

        if (dump_test_date_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_date_to_console || dump_test_date_to_file) {
            cts_dump_tsdata(cts_dev, "Rawdata", rawdata,
                dump_test_date_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev,
                "Rawdata", rawdata,
                param->invalid_nodes, param->num_invalid_node,
                validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Rawdata test failed %d", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_date_to_user) {
            rawdata += num_nodes;
        }
    }

    for (i = 0; i < 5; i++) {
        int r = cts_disable_get_rawdata(cts_dev);
        if (r) {
            cts_err("Disable get rawdata failed %d", r);
            continue;
        } else {
            break;
        }
    }

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

    if (!dump_test_date_to_user && rawdata != NULL) {
        kfree(rawdata);
    }

    cts_info("Rawdata test %s", (ret == 0) ? "PASS" : "FAIL");

    return ret;
}

int cts_test_noise(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_date_to_user = false;
    bool dump_test_date_to_console = false;
    bool dump_test_date_to_file = false;
    int  num_nodes;
    int  tsdata_frame_size;
    int  frame;
    u16 *buffer = NULL;
    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *noise = NULL;
    bool first_frame = true;
    bool data_valid = false;
    int  i;
    int  ret;

    if (cts_dev == NULL || param == NULL ||
        param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Noise test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames < 2) {
        cts_err("Noise test with too little frame %u",
            priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * num_nodes;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_date_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_date_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_date_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Noise test, flags: 0x%08x, frames: %d, "
               "num invalid node: %u, "
               "test data file: '%s' buf size: %d, "
               "drive log file: '%s' buf size: %d",
        param->flags, priv_param->frames, param->num_invalid_node,
        param->test_data_filepath, param->test_data_buf_size,
        param->driver_log_filepath, param->driver_log_buf_size);

    if (driver_validate_data || !dump_test_date_to_user) {
        buffer = (u16 *)kmalloc(tsdata_frame_size * 4, GFP_KERNEL);
        if (buffer == NULL) {
            cts_err("Alloc mem for tsdata failed");
            return -ENOMEM;
        }

        if (dump_test_date_to_user) {
            curr_rawdata = (u16 *)param->test_data_buf;
        } else {
            curr_rawdata = buffer;
        }

        if (driver_validate_data) {
            max_rawdata = buffer + 1 * num_nodes;
            min_rawdata = buffer + 2 * num_nodes;
            noise       = buffer + 3 * num_nodes;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
		if (buffer) {
       		kfree(buffer);
    	}
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);

    for (i = 0; i < 5; i++) {
        int r;
        u8 val;
        r = cts_enable_get_rawdata(cts_dev);
        if (r) {
            cts_err("Enable get ts data failed %d", r);
            continue;
        }
        mdelay(1);
        r = cts_fw_reg_readb(cts_dev, 0x12, &val);
        if (r) {
            cts_err("Read enable get ts data failed %d", r);
            continue;
        }
        if (val != 0) {
            break;
        }
    }

    if (i >= 5) {
        cts_err("Enable read tsdata failed");
        ret = -EIO;
        goto unlock_device;
    }

    msleep(50);

    for (frame = 0; frame < priv_param->frames; frame++) {
        ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
        if (ret) {
            cts_err("send quit gesture monitor err");
            // Ignore this error
        }

        for (i = 0; i < 3; i++) {
            int r;
            r = cts_get_rawdata(cts_dev, curr_rawdata);
            if (r) {
                cts_err("Get rawdata failed %d", r);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_date_to_user) {
            *param->test_data_wr_size += tsdata_frame_size;
        }

        if (dump_test_date_to_console || dump_test_date_to_file) {
            cts_dump_tsdata(cts_dev, "Noise-rawdata", curr_rawdata,
                dump_test_date_to_console);
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(max_rawdata, curr_rawdata, tsdata_frame_size);
                memcpy(min_rawdata, curr_rawdata, tsdata_frame_size);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }

        if (dump_test_date_to_user) {
            curr_rawdata += num_nodes;
        }
    }

    data_valid = true;

disable_get_tsdata:
    for (i = 0; i < 5; i++) {
        int r = cts_disable_get_rawdata(cts_dev);
        if (r) {
            cts_err("Disable get rawdata failed %d", r);
            continue;
        } else {
            break;
        }
    }

unlock_device:
    cts_unlock_device(cts_dev);

    {
        int r = cts_start_device(cts_dev);
        if (r) {
            cts_err("Start device failed %d", r);
        }
    }

    if (driver_validate_data && data_valid) {
        for (i = 0; i < num_nodes; i++) {
            noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_date_to_console || dump_test_date_to_file) {
            cts_dump_tsdata(cts_dev, "Noise", noise,
                dump_test_date_to_console);
        }

        ret = validate_tsdata(cts_dev, "Noise test",
            noise, param->invalid_nodes, param->num_invalid_node,
            validate_data_per_node, param->min, param->max);
    }

    if (buffer) {
        kfree(buffer);
    }

    cts_info("Noise test %s",
        (data_valid && (ret == 0)) ? "PASS" : "FAIL");

    return ret;
}

