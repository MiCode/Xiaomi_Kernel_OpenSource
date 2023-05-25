#ifndef CTS_TEST_H
#define CTS_TEST_H

struct cts_device;

#define CTS_TEST_FLAG_RESET_BEFORE_TEST             (1u << 0)
#define CTS_TEST_FLAG_RESET_AFTER_TEST              (1u << 1)
#define CTS_TEST_FLAG_DISPLAY_ON                    (1u << 2)
#define CTS_TEST_FLAG_DISABLE_GAS                   (1u << 3)
#define CTS_TEST_FLAG_DISABLE_LINESHIFT             (1u << 4)

#define CTS_TEST_FLAG_VALIDATE_DATA                 (1u << 8)
#define CTS_TEST_FLAG_VALIDATE_PER_NODE             (1u << 9)
#define CTS_TEST_FLAG_VALIDATE_MIN                  (1u << 10)
#define CTS_TEST_FLAG_VALIDATE_MAX                  (1u << 11)
#define CTS_TEST_FLAG_VALIDATE_SKIP_INVALID_NODE    (1u << 12)
#define CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED  (1u << 13)

#define CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE     (1u << 16)
#define CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE   (1u << 17)
#define CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE        (1u << 18)
#define CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND (1u << 19)
#define CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_CSV    (1u << 20)

#define CTS_TEST_FLAG_DRIVER_LOG_TO_USERSPACE       (1u << 24)
#define CTS_TEST_FLAG_DRIVER_LOG_TO_FILE            (1u << 25)
#define CTS_TEST_FLAG_DRIVER_LOG_TO_FILE_APPEND     (1u << 26)

#define MAKE_INVALID_NODE(r,c)      (((c) << 16) | (r))
#define INVALID_NODE_ROW(node)      ((u16)(node))
#define INVALID_NODE_COL(node)      ((u16)((node) >> 16))

enum cts_test_item {
    CTS_TEST_RESET_PIN = 1,
    CTS_TEST_INT_PIN,
    CTS_TEST_RAWDATA,
    CTS_TEST_NOISE,
    CTS_TEST_OPEN,
    CTS_TEST_SHORT,
    CTS_TEST_COMPENSATE_CAP,
};

struct cts_test_param {
    int test_item;

    __u32 flags;

    __u32  num_invalid_node;
    __u32 *invalid_nodes;
    int *min;
    int *max;

    int     *test_result;

    void    *test_data_buf;
    int test_data_buf_size;
    int *test_data_wr_size;
    const char *test_data_filepath;

    int driver_log_level;
    char *driver_log_buf;
    int driver_log_buf_size;
    int *driver_log_wr_size;
    const char *driver_log_filepath;

    void *priv_param;
    int priv_param_size;
};

struct cts_rawdata_test_priv_param {
    __u32 frames;
    //__u8  work_mode;
};

struct cts_noise_test_priv_param {
    __u32 frames;
    //__u8  work_mode;
};

extern const char *cts_test_item_str(int test_item);
extern int cts_write_file(struct file *filp, const void *data, size_t size);

extern int cts_start_dump_test_data_to_file(const char *filepath,
    bool append_to_file);
extern void cts_stop_dump_test_data_to_file(void);

extern int cts_test_reset_pin(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_int_pin(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_rawdata(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_noise(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_open(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_short(struct cts_device *cts_dev,
    struct cts_test_param *param);
extern int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param);

#endif /* CTS_TEST_H */

