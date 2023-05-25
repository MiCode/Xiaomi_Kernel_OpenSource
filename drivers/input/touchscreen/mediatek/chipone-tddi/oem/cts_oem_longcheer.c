#define LOG_TAG         "Oem"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_oem.h"
#include "cts_test.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

/* Following options override device tree settings */
#define OEM_OF_DEF_PROPVAL_TEST_RESET_PIN   true
#define OEM_OF_DEF_PROPVAL_TEST_INT_PIN     false
#define OEM_OF_DEF_PROPVAL_TEST_RAWDATA     true
#define OEM_OF_DEF_PROPVAL_TEST_NOISE       true
#define OEM_OF_DEF_PROPVAL_TEST_OPEN        true
#define OEM_OF_DEF_PROPVAL_TEST_SHORT       true
#define OEM_OF_DEF_PROPVAL_TEST_COMP_CAP    true

/* Default settings if device tree NOT exist */
#define OEM_OF_DEF_PROPVAL_RAWDATA_FRAMES   1
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN      700
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX      1500
#define OEM_OF_DEF_PROPVAL_NOISE_FRAMES     16
#define OEM_OF_DEF_PROPVAL_NOISE_MAX        50
#define OEM_OF_DEF_PROPVAL_OPEN_MIN         200
#define OEM_OF_DEF_PROPVAL_SHORT_MIN        200
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MIN     20
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MAX     110

#define OEM_OF_PROPNAME_PREFIX                 "chipone," //modify

#define OEM_OF_PROPNAME_TEST_RESET_PIN      OEM_OF_PROPNAME_PREFIX"test-reset-pin"
#define OEM_OF_PROPNAME_TEST_INT_PIN        OEM_OF_PROPNAME_PREFIX"test-int-pin"
#define OEM_OF_PROPNAME_TEST_RAWDATA        OEM_OF_PROPNAME_PREFIX"test-rawdata"
#define OEM_OF_PROPNAME_RAWDATA_FRAMES      OEM_OF_PROPNAME_PREFIX"test-rawdata-frames"
#define OEM_OF_PROPNAME_RAWDATA_MIN         OEM_OF_PROPNAME_PREFIX"rawdata-min"
#define OEM_OF_PROPNAME_RAWDATA_MAX         OEM_OF_PROPNAME_PREFIX"rawdata-max"
#define OEM_OF_PROPNAME_TEST_NOISE          OEM_OF_PROPNAME_PREFIX"test-noise"
#define OEM_OF_PROPNAME_NOISE_FRAMES        OEM_OF_PROPNAME_PREFIX"test-noise-frames"
#define OEM_OF_PROPNAME_NOISE_MAX           OEM_OF_PROPNAME_PREFIX"noise-max"
#define OEM_OF_PROPNAME_TEST_OPEN           OEM_OF_PROPNAME_PREFIX"test-open"
#define OEM_OF_PROPNAME_OPEN_MIN            OEM_OF_PROPNAME_PREFIX"open-min"
#define OEM_OF_PROPNAME_TEST_SHORT          OEM_OF_PROPNAME_PREFIX"test-short"
#define OEM_OF_PROPNAME_SHORT_MIN           OEM_OF_PROPNAME_PREFIX"short-min"
#define OEM_OF_PROPNAME_TEST_COMP_CAP       OEM_OF_PROPNAME_PREFIX"test-compensate-cap"
#define OEM_OF_PROPNAME_COMP_CAP_MIN        OEM_OF_PROPNAME_PREFIX"compensate-cap-min"
#define OEM_OF_PROPNAME_COMP_CAP_MAX        OEM_OF_PROPNAME_PREFIX"compensate-cap-max"

#define OEM_SELFTEST_PROC_FILENAME          "chipone_selftest"


#define OEM_TEST_DATA_DIR                   "/sdcard"
#define OEM_RAWDATA_TEST_DATA_FILEPATH      OEM_TEST_DATA_DIR"/FWMutualTest.csv"
#define OEM_NOISE_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/NoiseTest.csv"
#define OEM_OPEN_TEST_DATA_FILEPATH         OEM_TEST_DATA_DIR"/OpenTest.csv"
#define OEM_SHORT_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/ShortTest.csv"
#define OEM_COMP_CAP_TEST_DATA_FILEPATH     OEM_TEST_DATA_DIR"/FWCCTest.csv"

struct cts_oem_data {
    struct proc_dir_entry *selftest_proc_entry;

    bool test_config_from_dt_has_parsed;

    /* Test configuration from device tree */
    bool test_reset_pin;
    int  reset_pin_test_result;

    bool test_int_pin;
    int  int_pin_test_result;

    bool test_rawdata;
    u32  rawdata_test_frames;
    int  rawdata_test_result;
    u16 *rawdata_test_data;
    int  rawdata_test_data_buff_size;
    int  rawdata_test_data_wr_size;
    int  rawdata_min;
    int  rawdata_max;

    bool test_noise;
    u32  noise_test_frames;
    int  noise_test_result;
    u16 *noise_test_data;
    int  noise_test_data_buff_size;
    int  noise_test_data_wr_size;
    int  noise_max;

    bool test_open;
    int  open_test_result;
    u16 *open_test_data;
    int  open_test_data_buff_size;
    int  open_test_data_wr_size;
    int  open_min;

    bool test_short;
    int  short_test_result;
    u16 *short_test_data;
    int  short_test_data_buff_size;
    int  short_test_data_wr_size;
    int  short_min;

    bool test_comp_cap;
    int  comp_cap_test_result;
    u8  *comp_cap_test_data;
    int  comp_cap_test_data_buff_size;
    int  comp_cap_test_data_wr_size;
    int  comp_cap_min;
    int  comp_cap_max;

    struct chipone_ts_data *cts_data;

};

#define ALLOC_TEST_DATA_MEM(type, size) \
    do { \
        if (oem_data->test_##type) { \
            if (oem_data->type##_test_data == NULL) { \
                cts_info(" - Alloc " #type " test data mem size %d", size); \
                oem_data->type##_test_data = kmalloc(size, GFP_KERNEL); \
                if (oem_data->type##_test_data == NULL) { \
                    cts_err("Alloc " #type " test data mem failed"); \
                    return -ENOMEM; \
                } \
                oem_data->type##_test_data_buff_size = size; \
            } \
            memset(oem_data->type##_test_data, 0, size); \
        } \
    } while (0)

/* NOTE: Any test data mem alloc failed will NOT clean other mem */
static int alloc_sleftest_data_mem(struct cts_oem_data *oem_data, int nodes)
{
    cts_info("Alloc selftest data");

    ALLOC_TEST_DATA_MEM(rawdata,
        nodes * 2 * oem_data->rawdata_test_frames);
    ALLOC_TEST_DATA_MEM(noise,
        nodes * 2 * oem_data->noise_test_frames);
    ALLOC_TEST_DATA_MEM(open, nodes * 2);
    ALLOC_TEST_DATA_MEM(short, nodes * 2 * 7);
    ALLOC_TEST_DATA_MEM(comp_cap, nodes);

    return 0;
}
#undef ALLOC_TEST_DATA_MEM

#define FREE_TEST_DATA_MEM(type) \
    do { \
        if (oem_data->type##_test_data) { \
            cts_info("- Free " #type " test data mem"); \
            kfree(oem_data->type##_test_data); \
            oem_data->type##_test_data = NULL; \
            oem_data->type##_test_data_buff_size = 0; \
        } \
    } while(0)

static void free_selftest_data_mem(struct cts_oem_data *oem_data)
{
    cts_info("Free selftest data");

    FREE_TEST_DATA_MEM(rawdata);
    FREE_TEST_DATA_MEM(noise);
    FREE_TEST_DATA_MEM(open);
    FREE_TEST_DATA_MEM(short);
    FREE_TEST_DATA_MEM(comp_cap);
}
#undef FREE_TEST_DATA_MEM

static int parse_selftest_dt(struct cts_oem_data *oem_data,
    struct device_node *np)
{
    int ret;

    cts_info("Parse selftest dt");

    oem_data->test_reset_pin = OEM_OF_DEF_PROPVAL_TEST_RESET_PIN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RESET_PIN);

    oem_data->test_int_pin = OEM_OF_DEF_PROPVAL_TEST_INT_PIN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_INT_PIN);

    oem_data->test_rawdata = OEM_OF_DEF_PROPVAL_TEST_RAWDATA ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RAWDATA);
    if (oem_data->test_rawdata) {
        oem_data->rawdata_test_frames = OEM_OF_DEF_PROPVAL_RAWDATA_FRAMES;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_FRAMES,
            &oem_data->rawdata_test_frames);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_FRAMES"' failed %d", ret);
        }

        oem_data->rawdata_min = OEM_OF_DEF_PROPVAL_RAWDATA_MIN;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MIN,
            &oem_data->rawdata_min);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MIN"' failed %d", ret);
        }

        oem_data->rawdata_max = OEM_OF_DEF_PROPVAL_RAWDATA_MAX;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MAX,
            (u32 *)&oem_data->rawdata_max);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MAX"' failed %d", ret);
        }
        }

    oem_data->test_noise = OEM_OF_DEF_PROPVAL_TEST_NOISE ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_NOISE);
    if (oem_data->test_noise) {
        oem_data->noise_test_frames = OEM_OF_DEF_PROPVAL_NOISE_FRAMES;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_NOISE_FRAMES,
            &oem_data->noise_test_frames);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_NOISE_FRAMES"' failed %d", ret);
        }

        oem_data->noise_max = OEM_OF_DEF_PROPVAL_NOISE_MAX;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_NOISE_MAX,
            (u32 *)&oem_data->noise_max);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_NOISE_MAX"' failed %d", ret);
        }
    }

    oem_data->test_open = OEM_OF_DEF_PROPVAL_TEST_OPEN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_OPEN);
    if (oem_data->test_open) {
        oem_data->open_min = OEM_OF_DEF_PROPVAL_OPEN_MIN;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_OPEN_MIN,
            (u32 *)&oem_data->open_min);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_OPEN_MIN"' failed %d", ret);
        }
    }

    oem_data->test_short = OEM_OF_DEF_PROPVAL_TEST_SHORT ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_SHORT);
    if (oem_data->test_short) {
        oem_data->short_min = OEM_OF_DEF_PROPVAL_SHORT_MIN;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_SHORT_MIN,
            (u32 *)&oem_data->short_min);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_SHORT_MIN"' failed %d", ret);
        }
    }

    oem_data->test_comp_cap = OEM_OF_DEF_PROPVAL_TEST_COMP_CAP ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_COMP_CAP);
    if (oem_data->test_comp_cap) {
        oem_data->comp_cap_min = OEM_OF_DEF_PROPVAL_COMP_CAP_MIN;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_COMP_CAP_MIN,
            (u32 *)&oem_data->comp_cap_min);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_COMP_CAP_MIN"' failed %d", ret);
        }

        oem_data->comp_cap_max = OEM_OF_DEF_PROPVAL_COMP_CAP_MAX;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_COMP_CAP_MAX,
            (u32 *)&oem_data->comp_cap_max);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_COMP_CAP_MAX"' failed %d", ret);
        }
    }

    oem_data->test_config_from_dt_has_parsed = true;

    return 0;
}

static void print_selftest_config(const struct cts_oem_data *oem_data)
{
    cts_info("Seltest configuration:");

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RESET_PIN,
        oem_data->test_reset_pin ? 'Y' : 'N');

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_INT_PIN,
        oem_data->test_int_pin ? 'Y' : 'N');

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RAWDATA,
        oem_data->test_rawdata ? 'Y' : 'N');
    if (oem_data->test_rawdata) {
        cts_info(" - %-32s = %u", OEM_OF_PROPNAME_RAWDATA_FRAMES,
            oem_data->rawdata_test_frames);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MIN,
            oem_data->rawdata_min);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MAX,
            oem_data->rawdata_max);
    }

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_NOISE,
        oem_data->test_noise ? 'Y' : 'N');
    if (oem_data->test_noise) {
        cts_info(" - %-32s = %u", OEM_OF_PROPNAME_NOISE_FRAMES ,
            oem_data->noise_test_frames);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_NOISE_MAX,
            oem_data->noise_max);
    }

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_OPEN,
        oem_data->test_open ? 'Y' : 'N');
    if (oem_data->test_open) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN,
            oem_data->open_min);
    }

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_SHORT,
        oem_data->test_short ? 'Y' : 'N');
    if (oem_data->test_short) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN,
            oem_data->short_min);
    }

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_COMP_CAP,
        oem_data->test_comp_cap ? 'Y' : 'N');
    if (oem_data->test_comp_cap) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_COMP_CAP_MIN,
            oem_data->comp_cap_min);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_COMP_CAP_MAX,
            oem_data->comp_cap_max);
    }
}

static void do_selftest(struct cts_oem_data *oem_data)
{
    struct cts_test_param test_param;

    cts_info("Do selftest");

    if (oem_data->test_reset_pin) {
        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_RESET_PIN;
        oem_data->reset_pin_test_result = 
            cts_test_reset_pin(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->reset_pin_test_result) {
            cts_err("Test reset pin failed %d",
                oem_data->reset_pin_test_result);
        }
    }

    if (oem_data->test_int_pin) {
        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_INT_PIN;
        oem_data->int_pin_test_result =
            cts_test_int_pin(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->int_pin_test_result) {
            cts_err("Test int pin failed %d,%d",
                oem_data->int_pin_test_result);
        }
    }

    if (oem_data->test_rawdata) {
        struct cts_rawdata_test_priv_param priv_param = {0};

        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_RAWDATA;
        test_param.flags =
            CTS_TEST_FLAG_VALIDATE_DATA |
            CTS_TEST_FLAG_VALIDATE_MIN |
            CTS_TEST_FLAG_VALIDATE_MAX |
            CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE;
        test_param.min = &oem_data->rawdata_min;
        test_param.max = &oem_data->rawdata_max;
        test_param.test_data_buf = oem_data->rawdata_test_data;
        test_param.test_data_buf_size = oem_data->rawdata_test_data_buff_size;
        test_param.test_data_wr_size = &oem_data->rawdata_test_data_wr_size;

        priv_param.frames = oem_data->rawdata_test_frames;
        test_param.priv_param = &priv_param;
        test_param.priv_param_size = sizeof(priv_param);

        oem_data->rawdata_test_result =
            cts_test_rawdata(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->rawdata_test_result) {
            cts_err("Test rawdata failed %d",
                oem_data->rawdata_test_result);
        }
    }

    if (oem_data->test_noise) {
        struct cts_noise_test_priv_param priv_param;

        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_NOISE;
        test_param.flags =
            CTS_TEST_FLAG_VALIDATE_DATA |
            CTS_TEST_FLAG_VALIDATE_MAX |
            CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE;
        test_param.max = &oem_data->noise_max;
        test_param.test_data_buf = oem_data->noise_test_data;
        test_param.test_data_buf_size = oem_data->noise_test_data_buff_size;
        test_param.test_data_wr_size = &oem_data->noise_test_data_wr_size;

        priv_param.frames = oem_data->noise_test_frames;
        test_param.priv_param = &priv_param;
        test_param.priv_param_size = sizeof(priv_param);

        oem_data->noise_test_result =
            cts_test_noise(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->noise_test_result) {
            cts_err("Test noise failed %d",
                oem_data->noise_test_result);
        }
    }

    if (oem_data->test_open) {
        memset(&test_param, 0, sizeof(test_param));

        test_param.test_item = CTS_TEST_OPEN;
        test_param.flags =
            CTS_TEST_FLAG_VALIDATE_DATA |
            CTS_TEST_FLAG_VALIDATE_MIN |
            CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE;
        test_param.min = &oem_data->open_min;
        test_param.test_data_buf = oem_data->open_test_data;
        test_param.test_data_buf_size = oem_data->open_test_data_buff_size;
        test_param.test_data_wr_size = &oem_data->open_test_data_wr_size;

        oem_data->open_test_result =
            cts_test_open(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->open_test_result) {
            cts_err("Test open failed %d", oem_data->open_test_result);
        }
    }

    if (oem_data->test_short) {
        memset(&test_param, 0, sizeof(test_param));

        test_param.test_item = CTS_TEST_SHORT;
        test_param.flags =
            CTS_TEST_FLAG_VALIDATE_DATA |
            CTS_TEST_FLAG_VALIDATE_MIN |
            CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE;
        test_param.min = &oem_data->short_min;
        test_param.test_data_buf = oem_data->short_test_data;
        test_param.test_data_buf_size = oem_data->short_test_data_buff_size;
        test_param.test_data_wr_size = &oem_data->short_test_data_wr_size;

        oem_data->short_test_result =
            cts_test_short(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->short_test_result) {
            cts_err("Test short failed %d", oem_data->short_test_result);
        }
    }

    if (oem_data->test_comp_cap) {
        memset(&test_param, 0, sizeof(test_param));

        test_param.test_item = CTS_TEST_COMPENSATE_CAP;
        test_param.flags =
            CTS_TEST_FLAG_VALIDATE_DATA |
            CTS_TEST_FLAG_VALIDATE_MIN |
            CTS_TEST_FLAG_VALIDATE_MAX |
            CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
            CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE;
        test_param.min = &oem_data->comp_cap_min;
        test_param.max = &oem_data->comp_cap_max;
        test_param.test_result = &oem_data->comp_cap_test_result;
        test_param.test_data_buf = oem_data->comp_cap_test_data;
        test_param.test_data_buf_size = oem_data->comp_cap_test_data_buff_size;
        test_param.test_data_wr_size = &oem_data->comp_cap_test_data_wr_size;

        oem_data->comp_cap_test_result =
            cts_test_compensate_cap(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->comp_cap_test_result) {
            cts_err("Test compensate cap failed %d", oem_data->comp_cap_test_result);
        }
    }
}

static int dump_tsdata_row_to_buffer(char *buf, size_t size, const u16 *data,
    int cols, const char *prefix, const char *suffix, char seperator)
{
    int c, count = 0;

    if (prefix) {
        count += scnprintf(buf, size, "%s", prefix);
    }

    for (c = 0; c < cols; c++) {
        count += scnprintf(buf + count, size - count,
            "%4u%c ", data[c], seperator);
    }

    if (suffix) {
        count += scnprintf(buf + count, size - count, "%s", suffix);
    }

    return count;
}

static int dump_tsdata_to_csv_file(const char *filepath, int flags,
    const u16 *data, int frames, int rows, int cols)
{
    struct file *file;
    int i, r, ret = 0;
    loff_t pos = 0;

    cts_info("Dump tsdata to csv file: '%s' "
             "flags: 0x%x data: %p frames: %d row: %d col: %d",
        filepath, flags, data, frames, rows, cols);

    file = filp_open(filepath, flags, 0666);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        return PTR_ERR(file);
    }

    for (i = 0; i < frames; i++) {
	    for (r = 0; r < rows; r++) {
	        char linebuf[256];
	        int len;
	
	        len = dump_tsdata_row_to_buffer(linebuf, sizeof(linebuf),
	            data, cols, NULL, "\n", ',');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	        ret = kernel_write(file, linebuf, len, &pos);
#else
	        ret = kernel_write(file, linebuf, len, pos);
	        pos += len;
#endif
	        if (ret != len) {
	            cts_err("Write to file '%s' failed %d",
	                filepath, ret);
	            goto close_file;
	        }
	
	        data += cols;
	    }
	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	    ret = kernel_write(file, "\n", 1, &pos);
#else
	    ret = kernel_write(file, "\n", 1, pos);
	    pos ++;
#endif
	    if (ret != 1) {
	        cts_err("Write newline to file '%s' failed %d",
	            filepath, ret);
	        goto close_file;
	    }
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, r);
        }
    }

    return ret;
}

static void dump_tsdata_to_seq_file(struct seq_file *m,
    const u16 *data, int rows, int cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_tsdata_row_to_buffer(linebuf, sizeof(linebuf),
            data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += cols;
    }
}

static int dump_comp_cap_row_to_buffer(char *buf, size_t size, const u8 *cap,
    int cols, const char *prefix, const char *suffix, char seperator)
{
    int c, count = 0;

    if (prefix) {
        count += scnprintf(buf, size, "%s", prefix);
    }

    for (c = 0; c < cols; c++) {
        count += scnprintf(buf + count, size - count,
            "%3u%c ", cap[c], seperator);
    }

    if (suffix) {
        count += scnprintf(buf + count, size - count, "%s", suffix);
    }

    return count;
}

static int dump_comp_cap_to_csv_file(const char *filepath, int flags,
    const u8 *cap, int rows, int cols)
{
    struct file *file;
    int r, ret = 0;
    loff_t pos = 0;

    cts_info("Dump compensate cap to csv file: '%s' flags: 0x%x row: %d col: %d",
        filepath, flags, rows, cols);

    file = filp_open(filepath, flags, 0666);
    if (IS_ERR(file)) {
        cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
        return PTR_ERR(file);
    }

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_comp_cap_row_to_buffer(linebuf, sizeof(linebuf),
            cap, cols, NULL, "\n", ',');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        ret = kernel_write(file, linebuf, len, &pos);
#else
        ret = kernel_write(file, linebuf, len, pos);
        pos += len;
#endif
        if (ret != len) {
            cts_err("Write to file '%s' failed %d",
                filepath, ret);
            goto close_file;
        }

        cap += cols;
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, ret);
        }
    }

    return ret;
}

static void dump_comp_cap_to_seq_file(struct seq_file *m,
    const u8 *data, int rows, int cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_comp_cap_row_to_buffer(linebuf, sizeof(linebuf),
            data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += cols;
    }
}

static int save_selftest_data_to_file(struct cts_oem_data *oem_data)
{
    int rows, cols;
    int ret;

    cts_info("Save selftest data to file");

    rows  = oem_data->cts_data->cts_dev.fwdata.rows;
    cols  = oem_data->cts_data->cts_dev.fwdata.cols;

    if (oem_data->test_rawdata) {
            ret = dump_tsdata_to_csv_file(OEM_RAWDATA_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->rawdata_test_data,
            oem_data->rawdata_test_frames, rows, cols);
            if (ret < 0) {
                cts_err("Dump rawdata test data to file failed");
                return ret;
            }
        }

    if (oem_data->test_noise) {
            ret = dump_tsdata_to_csv_file(OEM_NOISE_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->noise_test_data,
            oem_data->noise_test_frames, rows, cols);
            if (ret < 0) {
                cts_err("Dump noise test data to file failed");
                return ret;
            }
        }

    if (oem_data->test_open) {
        ret = dump_tsdata_to_csv_file(OEM_OPEN_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->open_test_data,
            1, rows, cols);
        if (ret < 0) {
            cts_err("Dump open test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_short) {
            ret = dump_tsdata_to_csv_file(OEM_SHORT_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->short_test_data,
            7, rows, cols);
            if (ret < 0) {
                cts_err("Dump short test data to file failed");
                return ret;
            }
        }

    if (oem_data->test_comp_cap) {
        ret = dump_comp_cap_to_csv_file(OEM_COMP_CAP_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC,
            oem_data->comp_cap_test_data, rows, cols);
        if (ret < 0) {
            cts_err("Dump compensate cap test data to file failed");
            return ret;
        }
    }

    return 0;
}

static void *selftest_seq_start(struct seq_file *m, loff_t *pos)
{
    return *pos < 1 ? (void *)1 : NULL;
}

static void *selftest_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

static void selftest_seq_stop(struct seq_file *m, void *v)
{
    return;
}

static int selftest_seq_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)m->private;
    struct cts_oem_data *oem_data = NULL;
    int i, rows, cols;

    cts_info("Show seq selftest");

    if (cts_data == NULL) {
        cts_err("Selftest seq file private data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    rows  = cts_data->cts_dev.fwdata.rows;
    cols  = cts_data->cts_dev.fwdata.cols;

    seq_printf(m, "FW Version %04x!\n\n", cts_data->cts_dev.fwdata.version);

    if (oem_data->test_reset_pin) {
        seq_printf(m, "Reset-Pin Test %s!\n\n",
            oem_data->reset_pin_test_result == 0 ? "PASS" : "FAIL");
    }
    if (oem_data->test_int_pin) {
        seq_printf(m, "Int-Pin Test %s!\n\n",
            oem_data->int_pin_test_result == 0 ? "PASS" : "FAIL");
    }
    if (oem_data->test_rawdata) {
        seq_printf(m, "FW Rawdata Test");
        if (oem_data->rawdata_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->rawdata_test_result > 0) {
            seq_printf(m, " FAIL!\n");
            for(i = 0; i < oem_data->rawdata_test_frames; i++) {
                dump_tsdata_to_seq_file(m,
                    oem_data->rawdata_test_data + i * rows * cols, rows, cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->rawdata_test_result);
        }
    }
    if (oem_data->test_noise) {
        seq_printf(m, "Noise Test");
        if (oem_data->noise_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->noise_test_result > 0) {
            seq_printf(m, " FAIL!\n");
            for(i = 0; i < oem_data->noise_test_frames; i++) {
                dump_tsdata_to_seq_file(m,
                    oem_data->noise_test_data + i * rows * cols , rows, cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->noise_test_result);
        }
    }
    if (oem_data->test_open) {
        seq_printf(m, "Open Test");
        if (oem_data->open_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->open_test_result > 0) {
            seq_printf(m, " FAIL!\n");
            dump_tsdata_to_seq_file(m,
                oem_data->open_test_data, rows, cols);
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->open_test_result);
        }
    }
    if (oem_data->test_short) {
        seq_printf(m, "Short Test");
        if (oem_data->short_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->short_test_result > 0) {
            seq_printf(m, " FAIL!\n");
            for (i = 0; i < 7; i++) {
                dump_tsdata_to_seq_file(m,
                    oem_data->short_test_data + i * rows * cols, rows, cols);
                seq_putc(m, '\n');
            }
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->short_test_result);
        }
    }
    if (oem_data->test_comp_cap) {
        seq_printf(m, "Compensate-Cap Test");
        if (oem_data->comp_cap_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->comp_cap_test_result > 0) {
            seq_printf(m, " FAIL!\n");
            dump_comp_cap_to_seq_file(m,
                oem_data->comp_cap_test_data, rows, cols);
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->comp_cap_test_result);
        }
    }

    return 0;
}

const struct seq_operations selftest_seq_ops = {
    .start  = selftest_seq_start,
    .next   = selftest_seq_next,
    .stop   = selftest_seq_stop,
    .show   = selftest_seq_show,
};

static int32_t selftest_proc_open(struct inode *inode, struct file *file)
{
    struct chipone_ts_data *cts_data = PDE_DATA(inode);
    struct cts_oem_data *oem_data = NULL;
    int ret;

    if (cts_data == NULL) {
        cts_err("Open selftest proc with cts_data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    if (oem_data == NULL) {
        cts_err("Open selftest proc with oem_data = NULL");
        return -EFAULT;
    }

    cts_info("Open '/proc/" OEM_SELFTEST_PROC_FILENAME "'");

    if (!oem_data->test_config_from_dt_has_parsed) {
        ret = parse_selftest_dt(oem_data, cts_data->spi_client->dev.of_node);
        if (ret) {
            cts_err("Parse selftest dt failed %d", ret);
            return ret;
        }
    }

    print_selftest_config(oem_data);

    ret = alloc_sleftest_data_mem(oem_data,
        cts_data->cts_dev.fwdata.rows * cts_data->cts_dev.fwdata.cols);
    if (ret) {
        cts_err("Alloc test data mem failed");
        return ret;
    }

    do_selftest(oem_data);

    ret = save_selftest_data_to_file(oem_data);
    if (ret) {
        cts_err("Save selftest data to file failed %d", ret);
    }

    ret = seq_open(file, &selftest_seq_ops);
    if (ret) {
        cts_err("Open selftest seq file failed %d", ret);
        return ret;
    }

    ((struct seq_file *)file->private_data)->private = cts_data;

    return 0;
}

static const struct file_operations selftest_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = selftest_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

int cts_oem_init(struct chipone_ts_data *cts_data)
{
    struct cts_oem_data *oem_data = NULL;
    int ret;

    if (cts_data == NULL) {
        cts_err("Init with cts_data = NULL");
        return -EINVAL;
    }
	
    cts_info("Init");

    cts_data->oem_data = NULL;
	
    oem_data = kzalloc(sizeof(*oem_data), GFP_KERNEL);
    if (oem_data == NULL) {
        cts_err("Alloc oem data failed");
        return -ENOMEM;
    }

    cts_info(" - Create '/proc/"OEM_SELFTEST_PROC_FILENAME"'");
    oem_data->selftest_proc_entry =
        proc_create_data(OEM_SELFTEST_PROC_FILENAME,
            S_IRUGO, NULL, &selftest_proc_fops, cts_data);
    if (oem_data->selftest_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_SELFTEST_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

    cts_data->oem_data = oem_data;
	oem_data->cts_data = cts_data;
    return 0;

free_oem_data:
    kfree(oem_data);
    return ret;
}

int cts_oem_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_oem_data *oem_data = NULL;

    if (cts_data == NULL) {
        cts_err("Deinit with cts_data = NULL");
        return -EINVAL;
    }

    if (cts_data->oem_data == NULL) {
        cts_warn("Deinit with oem_data = NULL");
        return 0;
    }

    cts_info("Deinit");

    oem_data = cts_data->oem_data;

    if (oem_data->selftest_proc_entry) {
        cts_info("  Remove '/proc/"OEM_SELFTEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, NULL);
    }

    free_selftest_data_mem(oem_data);

    kfree(cts_data->oem_data);
    cts_data->oem_data = NULL;

    return 0;
}

