#define LOG_TAG         "Oem"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_oem.h"
#include "cts_test.h"
#include "cts_tcs.h"
#include "cts_firmware.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#ifdef CONFIG_CTS_TP_WORK_IRQ
#define CTS_TP_WORK_PROC_FILE			"tp_work"
#ifdef CONFIG_CTS_TP_DATA_DUMP
#define CTS_PROC_TP_DATA_DUMP "tp_data_dump"
#endif
#define LC_TP_WORK_ENABLE_IRQ_CMD		"1"
#define LC_TP_WORK_DISABLE_IRQ_CMD		"0"
bool cts_irq_status;
#endif
/* Following options override device tree settings */
#define OEM_OF_DEF_PROPVAL_TEST_RESET_PIN   true
//#define OEM_OF_DEF_PROPVAL_TEST_INT_PIN     true
#define OEM_OF_DEF_PROPVAL_TEST_RAWDATA     true
#define OEM_OF_DEF_PROPVAL_TEST_NOISE       true
#define OEM_OF_DEF_PROPVAL_TEST_OPEN        true
#define OEM_OF_DEF_PROPVAL_TEST_SHORT       true
#define OEM_OF_DEF_PROPVAL_TEST_COMP_CAP    true

/* Default settings if device tree NOT exist */
#define OEM_OF_DEF_PROPVAL_RAWDATA_FRAMES   1
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN      1200
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX      2800
#define OEM_OF_DEF_PROPVAL_NOISE_FRAMES     50
#define OEM_OF_DEF_PROPVAL_NOISE_MAX        80
#define OEM_OF_DEF_PROPVAL_OPEN_MIN         1980
#define OEM_OF_DEF_PROPVAL_SHORT_MIN        500
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MIN     1
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MAX     126

#define OEM_OF_PROPNAME_PREFIX              "chipone," //modify

#define OEM_OF_PROPNAME_TEST_RESET_PIN      OEM_OF_PROPNAME_PREFIX"test-reset-pin"
//#define OEM_OF_PROPNAME_TEST_INT_PIN        OEM_OF_PROPNAME_PREFIX"test-int-pin"
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

#define OEM_SELFTEST_PROC_FILENAME          "cts_selftest"
#define OEM_LIMIT_PROC_FILENAME             "cts_limit"
#define OEM_FACTORY_TEST_PROC_FILENAME      "cts_factory_test"
#define OEM_RAWDATA_PROC_FILENAME           "cts_rawdata"
#define OEM_MANUAL_PROC_FILENAME            "cts_manual"
#define OEM_DIFFDATA_PROC_FILENAME          "cts_diffdata"

#define OEM_TEST_DATA_DIR                   "/data/vendor/touchpad/"
#define OEM_RAWDATA_TEST_DATA_FILEPATH      OEM_TEST_DATA_DIR"/FWMutualTest.csv"
#define OEM_NOISE_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/NoiseTest.csv"
#define OEM_OPEN_TEST_DATA_FILEPATH         OEM_TEST_DATA_DIR"/OpenTest.csv"
#define OEM_SHORT_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/ShortTest.csv"
#define OEM_COMP_CAP_TEST_DATA_FILEPATH     OEM_TEST_DATA_DIR"/FWCCTest.csv"


#pragma pack(1)
struct cts_limit {
    u8 header[7];
    u8 version;
    struct {
        unsigned int_item : 1;
        unsigned reset_item : 1;
        unsigned normal_rawdata_item : 1;
        unsigned normal_noise_item : 1;
        unsigned open_item : 1;
        unsigned short_item : 1;
        unsigned comp_cap_item : 1;
        unsigned gstr_rawdata_item : 1;
        unsigned gstrlp_rawdata_item : 1;
        unsigned gstr_noise_item : 1;
        unsigned gstrlp_noise_item : 1;
        unsigned type : 5;
    };
    u16 normal_rawdata_frames;
    u16 normal_rawdata_min;
    u16 normal_rawdata_max;
    u16 normal_noise_frames;
    u16 normal_noise_max;
    u16 normal_open_min;
    u16 normal_short_min;
    u16 normal_compcap_min;
    u16 normal_compcap_max;
    u16 gstr_rawdata_frames;
    u16 gstr_rawdata_min;
    u16 gstr_rawdata_max;
    u16 gstrlp_rawdata_frames;
    u16 gstrlp_rawdata_min;
    u16 gstrlp_rawdata_max;
    u16 gstr_noise_frames;
    u16 gstr_noise_max;
    u16 gstrlp_noise_frames;
    u16 gstrlp_noise_max;
};
#pragma pack()

struct cts_oem_data {
    struct proc_dir_entry *selftest_proc_entry;
    struct proc_dir_entry *limit_entry;
    struct proc_dir_entry *factory_test_entry;
    struct proc_dir_entry *manual_proc_entry;
    struct proc_dir_entry *diffdata_proc_entry;
    struct proc_dir_entry *rawdata_proc_entry;

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

struct proc_dir_entry *cts_tp_work_proc;

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
        nodes * 2 * (oem_data->noise_test_frames + 3));
    ALLOC_TEST_DATA_MEM(open, nodes * 2);
    ALLOC_TEST_DATA_MEM(short, nodes * 2 * 10);
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

    /** reset pin **/
    oem_data->test_reset_pin = OEM_OF_DEF_PROPVAL_TEST_RESET_PIN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RESET_PIN);

    /** int pin **/
    //oem_data->test_int_pin = OEM_OF_DEF_PROPVAL_TEST_INT_PIN ||
      //  of_property_read_bool(np, OEM_OF_PROPNAME_TEST_INT_PIN);

    /** rawdata **/
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
            (u32 *)&oem_data->rawdata_min);
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

    /** nosie **/
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

    /** open **/
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

    /** short **/
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

    /** comp cap **/
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

    /** reset pin **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RESET_PIN,
        oem_data->test_reset_pin ? 'Y' : 'N');

    /** int pin **/
    //cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_INT_PIN,
     //   oem_data->test_int_pin ? 'Y' : 'N');

    /** rawdata **/
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

    /** noise **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_NOISE,
        oem_data->test_noise ? 'Y' : 'N');
    if (oem_data->test_noise) {
        cts_info(" - %-32s = %u", OEM_OF_PROPNAME_NOISE_FRAMES ,
            oem_data->noise_test_frames);
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_NOISE_MAX,
            oem_data->noise_max);
    }

    /** open **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_OPEN,
        oem_data->test_open ? 'Y' : 'N');
    if (oem_data->test_open) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN,
            oem_data->open_min);
    }

    /** short **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_SHORT,
        oem_data->test_short ? 'Y' : 'N');
    if (oem_data->test_short) {
        cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN,
            oem_data->short_min);
    }

    /** comp cap **/
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

    /** reset pin test **/
    if (oem_data->test_reset_pin) {
        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_RESET_PIN;
        oem_data->reset_pin_test_result =
            cts_test_reset_pin(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->reset_pin_test_result) {
            cts_err("Test reset pin failed %d", oem_data->reset_pin_test_result);
        }
    }

    /** int pin test **/
    if (oem_data->test_int_pin) {
        memset(&test_param, 0, sizeof(test_param));
        test_param.test_item = CTS_TEST_INT_PIN;
        oem_data->int_pin_test_result =
            cts_test_int_pin(&oem_data->cts_data->cts_dev, &test_param);
        if (oem_data->int_pin_test_result) {
            cts_err("Test int pin failed %d", oem_data->int_pin_test_result);
        }
    }

    /** rawdata test **/
    if (oem_data->test_rawdata) {
        struct cts_rawdata_test_priv_param priv_param = {0};
        memset(&test_param, 0, sizeof(test_param));
        oem_data->rawdata_test_data_wr_size = 0;

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
            cts_err("Test rawdata failed %d", oem_data->rawdata_test_result);
        }
    }

    /** noise test **/
    if (oem_data->test_noise) {
        struct cts_noise_test_priv_param priv_param = {0};
        memset(&test_param, 0, sizeof(test_param));
        oem_data->noise_test_data_wr_size = 0;

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
            cts_err("Test noise failed %d", oem_data->noise_test_result);
        }
    }

    /** open test **/
    if (oem_data->test_open) {
        memset(&test_param, 0, sizeof(test_param));
        oem_data->open_test_data_wr_size = 0;
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

    /** short test **/
    if (oem_data->test_short) {
        memset(&test_param, 0, sizeof(test_param));
        oem_data->short_test_data_wr_size = 0;
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

    /** comp cap test **/
    if (oem_data->test_comp_cap) {
        memset(&test_param, 0, sizeof(test_param));
        oem_data->comp_cap_test_data_wr_size = 0;
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

static int dump_tsdata_to_csv_file(const char *filepath,
    int flags, const u16 *data, int frames, int rows, int cols, int hw_cols)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    loff_t pos = 0;
    int i, r, ret;
    struct file *file;

    cts_info("Dump tsdata to csv file: '%s' flags: 0x%x data: %p frames: %d row: %d col: %d hw_col: %d",
        filepath, flags, data, frames, rows, cols, hw_cols);

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
                cts_err("Write to file '%s' failed %d", filepath, ret);
                goto close_file;
            }
            data += hw_cols;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        ret = kernel_write(file, "\n", 1, &pos);
#else
        ret = kernel_write(file, "\n", 1, pos);
        pos ++;
#endif
        if (ret != 1) {
            cts_err("Write newline to file '%s' failed %d", filepath, ret);
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
#endif
}

static void dump_tsdata_to_seq_file(struct seq_file *m,
        const u16 *data, int rows, int cols, int hw_cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_tsdata_row_to_buffer(linebuf, sizeof(linebuf),
            data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += hw_cols;
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

static int dump_comp_cap_to_csv_file(const char *filepath,
        int flags, const u8 *cap, int rows, int cols, int hw_cols)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
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
            cts_err("Write to file '%s' failed %d", filepath, ret);
            goto close_file;
        }

        cap += hw_cols;
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, ret);
        }
    }

    return ret;
#endif
}

static void dump_comp_cap_to_seq_file(struct seq_file *m,
    const u8 *data, int rows, int cols, int hw_cols)
{
    int r;

    for (r = 0; r < rows; r++) {
        char linebuf[256];
        int len;

        len = dump_comp_cap_row_to_buffer(linebuf, sizeof(linebuf),
            data, cols, NULL, "\n", ',');
        seq_puts(m, linebuf);

        data += hw_cols;
    }
}

static int save_selftest_data_to_file(struct cts_oem_data *oem_data)
{
    int rows, cols, hw_cols;
    int ret;

    cts_info("Save selftest data to file");

    rows = oem_data->cts_data->cts_dev.fwdata.rows;
    cols = oem_data->cts_data->cts_dev.fwdata.cols;
    hw_cols = oem_data->cts_data->cts_dev.hwdata->num_col;

    if (oem_data->test_rawdata) {
        ret = dump_tsdata_to_csv_file(OEM_RAWDATA_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->rawdata_test_data,
            oem_data->rawdata_test_frames, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump rawdata test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_noise) {
        ret = dump_tsdata_to_csv_file(OEM_NOISE_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->noise_test_data,
            (oem_data->noise_test_frames + 3), rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump noise test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_open) {
        ret = dump_tsdata_to_csv_file(OEM_OPEN_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->open_test_data,
            1, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump open test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_short) {
        ret = dump_tsdata_to_csv_file(OEM_SHORT_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->short_test_data,
            10, rows, cols, hw_cols);
        if (ret < 0) {
            cts_err("Dump short test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_comp_cap) {
        ret = dump_comp_cap_to_csv_file(OEM_COMP_CAP_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC,
            oem_data->comp_cap_test_data, rows, cols, hw_cols);
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
    int i, rows, cols, hw_rows, hw_cols;

    cts_info("Show seq selftest");

    if (cts_data == NULL) {
        cts_err("Selftest seq file private data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    rows    = cts_data->cts_dev.fwdata.rows;
    cols    = cts_data->cts_dev.fwdata.cols;
    hw_rows    = cts_data->cts_dev.hwdata->num_row;
    hw_cols    = cts_data->cts_dev.hwdata->num_col;

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
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->rawdata_test_result);
        }
        for(i = 0; i < oem_data->rawdata_test_frames; i++) {
            dump_tsdata_to_seq_file(m, oem_data->rawdata_test_data +
            i * hw_rows * hw_cols, rows, cols, hw_cols);
            seq_putc(m, '\n');
        }
    }
    if (oem_data->test_noise) {
        seq_printf(m, "Noise Test");
        if (oem_data->noise_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->noise_test_result > 0) {
            seq_printf(m, " FAIL!\n");
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->noise_test_result);
        }
        for(i = 0; i < oem_data->noise_test_frames + 3; i++) {
            dump_tsdata_to_seq_file(m, oem_data->noise_test_data +
                    i * hw_rows * hw_cols, rows, cols, hw_cols);
            seq_putc(m, '\n');
        }
    }
    if (oem_data->test_open) {
        seq_printf(m, "Open Test");
        if (oem_data->open_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->open_test_result > 0) {
            seq_printf(m, " FAIL!\n");
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->open_test_result);
        }
        dump_tsdata_to_seq_file(m, oem_data->open_test_data,
            rows, cols, hw_cols);
    }
    if (oem_data->test_short) {
        seq_printf(m, "Short Test");
        if (oem_data->short_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->short_test_result > 0) {
            seq_printf(m, " FAIL!\n");
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->short_test_result);
        }
        for (i = 0; i < 10; i++) {
            dump_tsdata_to_seq_file(m, oem_data->short_test_data +
                i * hw_rows * hw_cols, rows, cols, hw_cols);
            seq_putc(m, '\n');
        }
    }
    if (oem_data->test_comp_cap) {
        seq_printf(m, "Compensate-Cap Test");
        if (oem_data->comp_cap_test_result == 0) {
            seq_printf(m, " PASS!\n\n");
        } else if (oem_data->comp_cap_test_result > 0) {
            seq_printf(m, " FAIL!\n");
        } else {
            seq_printf(m, " ERROR(%d)!\n\n", oem_data->comp_cap_test_result);
        }
        dump_comp_cap_to_seq_file(m, oem_data->comp_cap_test_data,
            rows, cols, hw_cols);
    }

    return 0;
}

const struct seq_operations selftest_seq_ops = {
    .start  = selftest_seq_start,
    .next   = selftest_seq_next,
    .stop   = selftest_seq_stop,
    .show   = selftest_seq_show,
};

int lct_cts_tp_selftest_callback(unsigned char cmd)
{
	int ret = 0;
	switch (cmd) {
	case TP_SELFTEST_CMD_LONGCHEER_MMI:
		ret = lct_tp_selftest_all();
		break;
	case TP_SELFTEST_CMD_XIAOMI_I2C:
		ret = lct_tp_selftest_all();
		break;
	case TP_SELFTEST_CMD_XIAOMI_OPEN:
		ret = lct_tp_selftest_all();
		break;
	case TP_SELFTEST_CMD_XIAOMI_SHORT:
		ret = lct_tp_selftest_all();
		break;
	default:
		break;
	}
	return ret;

}

EXPORT_SYMBOL(lct_cts_tp_selftest_callback);

int lct_tp_selftest_all(void)
{
    struct chipone_ts_data *cts_data = g_cts_data;
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

    cts_info("Open '/proc/" TP_SELF_TEST_PROC_FILE "'");

    if (!oem_data->test_config_from_dt_has_parsed) {
#ifndef CONFIG_CTS_I2C_HOST
        ret = parse_selftest_dt(oem_data, cts_data->pdata->spi_client->dev.of_node);
#else
        ret = parse_selftest_dt(oem_data, cts_data->device->of_node);
#endif
        if (ret) {
            cts_err("Parse selftest dt failed %d", ret);
            return ret;
        }
    }

    print_selftest_config(oem_data);

    ret = alloc_sleftest_data_mem(oem_data,
        cts_data->cts_dev.hwdata->num_row * cts_data->cts_dev.hwdata->num_col);
    if (ret) {
        cts_err("Alloc test data mem failed");
        return ret;
    }

    do_selftest(oem_data);
    
    if (oem_data->test_reset_pin) {
         ret += ((oem_data->reset_pin_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_int_pin) {
	 ret += ((oem_data->int_pin_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_rawdata) {
	 ret += ((oem_data->rawdata_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_noise) {
	 ret += ((oem_data->noise_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_open) {
	 ret += ((oem_data->open_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_short) {
	 ret += ((oem_data->short_test_result == 0) ? 0 : 1);
	}
	if (oem_data->test_comp_cap) {
	 ret += ((oem_data->comp_cap_test_result == 0) ? 0 : 1);
	}

	cts_info("show selftest result = %d(2--pass,1--fail)\n",ret);
	if (!ret) {
		return 2;//PASS
	} else {
		return 1;//FAIL
	}
}
#ifdef CONFIG_CTS_TP_WORK_IRQ
static int lct_proc_open(struct inode *inode, struct file *file)
{
    file->private_data = PDE_DATA(inode);
    return 0;
}

static ssize_t lct_proc_tp_work_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	ssize_t cnt=0;
	char *page = NULL;

	if (*ppos)
		return 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;
	cnt = sprintf(page, "%s", (cts_irq_status ? "1\n" : "0\n"));
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	cts_info("Touchpad status : %s", page);

	kfree(page);
	return cnt;

}

static ssize_t lct_proc_tp_work_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
        struct chipone_ts_data *cts_data = (struct chipone_ts_data *)file->private_data;
        int ret = 0;
        char tmp_data[64] = {0};

    if (cts_data == NULL) {
        cts_err("Read with cts_data = NULL");
        return -EIO;
    }

	if(copy_from_user(tmp_data, buf, size)) {
		cts_err("copy_from_user() fail.\n");
		return -EFAULT;
	}

	cts_info("read data from user is %s\n",tmp_data);

	if((strncmp(tmp_data, LC_TP_WORK_ENABLE_IRQ_CMD, strlen(LC_TP_WORK_ENABLE_IRQ_CMD)) == 0)){
		ret = cts_plat_enable_irq(cts_data->pdata);
		if (ret) {
        	cts_err("Enable IRQ failed %d", ret);
    	}else{
			cts_info("Enable Touch irq success...\n");
		}

		cts_irq_status = true;
	}else if( (strncmp(tmp_data, LC_TP_WORK_DISABLE_IRQ_CMD, strlen(LC_TP_WORK_DISABLE_IRQ_CMD)) == 0)) {
		ret = cts_plat_disable_irq(cts_data->pdata);
		if (ret) {
        	cts_err("Disable IRQ failed %d", ret);
    	}else{
			cts_info("Disable Touch irq success...\n");
		}

		cts_irq_status = false;
	} else {
		cts_err("input wrong cmd!");
	}

	//cts_info("Set Touch irq status successfully!\n");

	return size;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops cts_tp_work_proc_fops = {
        .proc_open  = lct_proc_open,
        .proc_read = lct_proc_tp_work_read,
        .proc_write = lct_proc_tp_work_write,
};
#else
static const struct file_operations cts_tp_work_proc_fops = {
	.owner = THIS_MODULE,
	.open  = lct_proc_open,
	.read = lct_proc_tp_work_read,
	.write = lct_proc_tp_work_write,
};
#endif
#endif

#ifdef CONFIG_CTS_TP_DATA_DUMP
extern int cts_test_polling_rawdata(struct cts_device *cts_dev,
        u8 *buf, size_t size);
static int cts_tp_data_dump_proc_show(struct seq_file *m, void *v)
{
#define DATA_BUFFER_SIZE(cts_dev) \
		(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)

	struct chipone_ts_data *cts_data = (struct chipone_ts_data *)m->private;
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u16 *rawdata = NULL;
	s16 *diffdata = NULL;
	int ret, r, c = 0;
	bool data_valid_rawdata = true;
	bool data_valid_diffdata = true;

	cts_info("Show seq tp data dump rawdata/diffdata");

	//get rawdata start
	rawdata = (u16 *)kmalloc(DATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata == NULL) {
		return -EFAULT;
	}

    diffdata = (s16 *)kmalloc(DATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
    if (diffdata == NULL) {
        cts_err("Allocate memory for diffdata failed");
        ret = -ENOMEM;
        goto err_free_rawdata;
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto err_free_diffdata;
    }

    cts_lock_device(cts_dev);
    cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);

	ret = cts_test_polling_rawdata(cts_dev, (u8 *)rawdata, DATA_BUFFER_SIZE(cts_dev));
	if (ret < 0) {
		cts_err("Get raw data failed: %d", ret);
		data_valid_rawdata = false;
	}

    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
	//get rawdata end

	//get diffdata start
    ret = cts_tcs_top_get_real_diff(cts_dev, (u8 *)diffdata, DATA_BUFFER_SIZE(cts_dev));
    if (ret < 0) {
        cts_err("Get diffdata failed");
		data_valid_diffdata = false;
    }
	//get diffdata end

    cts_unlock_device(cts_dev);
	{
		int r = cts_start_device(cts_dev);
		if (r)
			cts_err("Start device failed %d", r);
	}

	if (data_valid_rawdata && data_valid_diffdata) {
		seq_printf(m,"\nRAW DATA\n");
		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			for (c = 0; c < cts_dev->fwdata.cols; c++) {
	            seq_printf(m, "%5d", rawdata[r * cts_dev->hwdata->num_col + c]);
	        }
	        seq_printf(m, "\n");
	    }
	    seq_printf(m,"\nDIFF DATA\n");
		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			for (c = 0; c < cts_dev->fwdata.cols; c++) {
	            seq_printf(m, "%5d", diffdata[r * cts_dev->hwdata->num_col + c]);
	        }
	        seq_printf(m, "\n");
	    }
	}

err_free_diffdata:
    kfree(diffdata);
err_free_rawdata:
	kfree(rawdata);

	return ret;

#undef DATA_BUFFER_SIZE
}

static int cts_tp_data_dump_proc_open(struct inode* inode, struct file* file)
{
	return single_open(file, cts_tp_data_dump_proc_show,  PDE_DATA(inode));
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops cts_tp_data_dump_proc_fops = {
        .proc_open  = cts_tp_data_dump_proc_open,
        .proc_read = seq_read,
};
#else
static const struct file_operations cts_tp_data_dump_proc_fops =
{
    .owner = THIS_MODULE,
    .open = cts_tp_data_dump_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif
#endif

#ifdef CONFIG_CTS_TP_GRIP_AREA
int lct_cts_tp_get_screen_angle_callback(void)
{
	u8 data;
    int ret = -EIO;
	struct chipone_ts_data *cts_data = g_cts_data;
	struct cts_device *cts_dev = &cts_data->cts_dev;

	if(cts_dev->rtdata.suspended || cts_dev->rtdata.updating)
	{
		cts_info("tp is suspended or updating, can not to set");
	}

	cts_lock_device(cts_dev);
	ret = cts_tcs_get_panel_direction(cts_dev, &data);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("Get panel direction failed!");
		goto out;
	}

	switch (data) {
		case 0: ret = 0; break;
		case 1: ret = 90; break;
		case 2: ret = 270; break;
		default: break;
	}

	cts_info("edge status = %d angle = %d", data, ret);
	ret = data;

out:
	return ret;
}


int lct_cts_tp_set_screen_angle_callback(int angle)
{
    u8 val;
    int ret = -EIO;
	struct chipone_ts_data *cts_data = g_cts_data;
	struct cts_device *cts_dev = &cts_data->cts_dev;

	cts_info("set angle = %d", angle);

	if(cts_dev->rtdata.suspended || cts_dev->rtdata.updating)
	{
		cts_info("tp is suspended or updating, can not to set");
	}

    if (angle == 90) {
        val = 1;
    } else if (angle == 270) {
        val = 2;
    } else {
        val = 0;//0
    }

	cts_lock_device(cts_dev);
	ret = cts_tcs_set_panel_direction(cts_dev, val);
	cts_unlock_device(cts_dev);
	if(ret){
        cts_err("Set panel direction failed!");
		goto exit;
	}

	cts_info("edge status = %d angle = %d", val, angle);

exit:
    return ret;
}
#endif

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
#ifndef CONFIG_CTS_I2C_HOST
        ret = parse_selftest_dt(oem_data, cts_data->pdata->spi_client->dev.of_node);
#else
        ret = parse_selftest_dt(oem_data, cts_data->device->of_node);
#endif
        if (ret) {
            cts_err("Parse selftest dt failed %d", ret);
            return ret;
        }
    }

    print_selftest_config(oem_data);

    ret = alloc_sleftest_data_mem(oem_data,
        cts_data->cts_dev.hwdata->num_row * cts_data->cts_dev.hwdata->num_col);
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations selftest_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = selftest_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};
#else
static const struct proc_ops selftest_proc_fops = {
    .proc_open    = selftest_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = seq_release,
};
#endif

static int cts_rawdata_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_rawdata(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_rawdata_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_rawdata_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_rawdata_ops = {
    .owner = THIS_MODULE,
    .open = cts_rawdata_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_rawdata_ops = {
    .proc_open = cts_rawdata_open,
    .proc_read = seq_read,
};
#endif

static int cts_diffdata_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_real_diff(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_diffdata_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_diffdata_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_diffdata_ops = {
    .owner = THIS_MODULE,
    .open = cts_diffdata_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_diffdata_ops = {
    .proc_open = cts_diffdata_open,
    .proc_read = seq_read,
};
#endif

static int cts_manual_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev;
    u8 *rawdata;
    s16 *data;
    u8 rows, cols;
    u8 i, j;

    cts_dev = &cts_data->cts_dev;
    rows = cts_dev->hwdata->num_row;
    cols = cts_dev->hwdata->num_col;
    rawdata = kzalloc(rows * cols * 2, GFP_KERNEL);
    if (rawdata == NULL) {
        cts_err("Allocate rawdata failed");
        return -ENOMEM;
    }

    cts_lock_device(cts_dev);
    cts_tcs_top_get_manual_diff(cts_dev, rawdata, rows * cols * 2);
    cts_unlock_device(cts_dev);

    data = (s16 *)rawdata;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            seq_printf(m, "%5d", data[i * cols + j]);
        }
        seq_printf(m, "\n");
    }

    kfree(rawdata);
    return 0;
}
static int cts_manual_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_manual_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_manual_ops = {
    .owner = THIS_MODULE,
    .open = cts_manual_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_manual_ops = {
    .proc_open = cts_manual_open,
    .proc_read = seq_read,
};
#endif

/* Use limit bin */
static int cts_limit_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev = &cts_data->cts_dev;
    const struct firmware *limit_fw = NULL;
    struct cts_limit limit;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int ret;

    ret = request_firmware(&limit_fw, CFG_CTS_FACTORY_LIMIT_FILENAME,
            &cts_dev->pdata->ts_input_dev->dev);
    if (ret) {
        seq_printf(m, "Request %s failed!\n", CFG_CTS_FACTORY_LIMIT_FILENAME);
        return 0;
    }

    ret = memcmp(header, limit_fw->data, sizeof(header));
    if (ret) {
        seq_printf(m, "LIMIT file was not matched\n");
        release_firmware(limit_fw);
        return 0;
    } else {
        cts_info("Matched limit file!");
    }

    memcpy(&limit, limit_fw->data, limit_fw->size);
    release_firmware(limit_fw);

    seq_printf(m, "Limit version: %x, type:%d\n", limit.version, limit.type);
    seq_printf(m, "\n");
    seq_printf(m, "Switch status:\n");
    seq_printf(m, " int_item              : %s\n", limit.int_item ? "Y" : "N");
    seq_printf(m, " reset_item            : %s\n", limit.reset_item ? "Y" : "N");
    seq_printf(m, " normal_rawdata_item   : %s\n", limit.normal_rawdata_item ? "Y" : "N");
    seq_printf(m, " normal_noise_item     : %s\n", limit.normal_noise_item ? "Y" : "N");
    seq_printf(m, " open_item             : %s\n", limit.open_item ? "Y" : "N");
    seq_printf(m, " short_item            : %s\n", limit.short_item ? "Y" : "N");
    seq_printf(m, " comp_cap_item         : %s\n", limit.comp_cap_item ? "Y" : "N");
    seq_printf(m, " gstr_rawdata_item     : %s\n", limit.gstr_rawdata_item ? "Y" : "N");
    seq_printf(m, " gstrlp_rawdata_item   : %s\n", limit.gstrlp_rawdata_item ? "Y" : "N");
    seq_printf(m, " gstr_noise_item       : %s\n", limit.gstr_noise_item ? "Y" : "N");
    seq_printf(m, " gstrlp_noise_item     : %s\n", limit.gstrlp_noise_item ? "Y" : "N");
    seq_printf(m, "\n");
    seq_printf(m, "Threshold:\n");
    seq_printf(m, " normal_rawdata_frames : %d\n", limit.normal_rawdata_frames);
    seq_printf(m, " normal_rawdata_min    : %d\n", limit.normal_rawdata_min);
    seq_printf(m, " normal_rawdata_max    : %d\n", limit.normal_rawdata_max);
    seq_printf(m, " normal_noise_frames   : %d\n", limit.normal_noise_frames);
    seq_printf(m, " normal_noise_max      : %d\n", limit.normal_noise_max);
    seq_printf(m, " normal_open_min       : %d\n", limit.normal_open_min);
    seq_printf(m, " normal_short_min      : %d\n", limit.normal_short_min);
    seq_printf(m, " normal_compcap_min    : %d\n", limit.normal_compcap_min);
    seq_printf(m, " normal_compcap_max    : %d\n", limit.normal_compcap_max);
    seq_printf(m, " gstr_rawdata_frames   : %d\n", limit.gstr_rawdata_frames);
    seq_printf(m, " gstr_rawdata_min      : %d\n", limit.gstr_rawdata_min);
    seq_printf(m, " gstr_rawdata_max      : %d\n", limit.gstr_rawdata_max);
    seq_printf(m, " gstrlp_rawdata_frames : %d\n", limit.gstrlp_rawdata_frames);
    seq_printf(m, " gstrlp_rawdata_min    : %d\n", limit.gstrlp_rawdata_min);
    seq_printf(m, " gstrlp_rawdata_max    : %d\n", limit.gstrlp_rawdata_max);
    seq_printf(m, " gstr_noise_frames     : %d\n", limit.gstr_noise_frames);
    seq_printf(m, " gstr_noise_max        : %d\n", limit.gstr_noise_max);
    seq_printf(m, " gstrlp_noise_frames   : %d\n", limit.gstrlp_noise_frames);
    seq_printf(m, " gstrlp_noise_max      : %d\n", limit.gstrlp_noise_max);

    return 0;
}
static int cts_limit_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_limit_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_limit_ops = {
    .owner = THIS_MODULE,
    .open = cts_limit_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_limit_ops = {
    .proc_open = cts_limit_open,
    .proc_read = seq_read,
};
#endif

static int cts_factory_test_show(struct seq_file *m, void *v)
{
#define TOUCH_DATA_DIRECTORY_PREFIX           "/sdcard/chipone-tddi/test"
#define RAWDATA_TEST_DATA_FILENAME            "rawdata-test-data.txt"
#define NOISE_TEST_DATA_FILENAME              "noise-test-data.txt"
#define OPEN_TEST_DATA_FILENAME               "open-test-data.txt"
#define SHORT_TEST_DATA_FILENAME              "short-test-data.txt"
#define COMP_CAP_TEST_DATA_FILENAME           "comp-cap-test-data.txt"
    struct chipone_ts_data *cts_data = m->private;
    struct cts_device *cts_dev = &cts_data->cts_dev;

    s64 reset_pin_test_elapsed_time = 0;
    struct cts_test_param reset_pin_test_param = {
        .test_item = CTS_TEST_RESET_PIN,
        .flags = 0,
        .elapsed_time_ms = &reset_pin_test_elapsed_time,
    };
    s64 int_pin_test_elapsed_time = 0;
    struct cts_test_param int_pin_test_param = {
        .test_item = CTS_TEST_INT_PIN,
        .flags = 0,
        .elapsed_time_ms = &int_pin_test_elapsed_time,
    };
    struct cts_rawdata_test_priv_param rawdata_test_priv_param = {
        .frames = 16,
        //.work_mode = 0,
    };
    s64 rawdata_test_elapsed_time = 0;
    struct cts_test_param rawdata_test_param = {
        .test_item = CTS_TEST_RAWDATA,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &rawdata_test_elapsed_time,
        .priv_param = &rawdata_test_priv_param,
        .priv_param_size = sizeof(rawdata_test_priv_param),
    };
    struct cts_noise_test_priv_param noise_test_priv_param = {
        .frames = 50,
        //.work_mode = 0,
    };
    s64 noise_test_elapsed_time = 0;
    struct cts_test_param noise_test_param = {
        .test_item = CTS_TEST_NOISE,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &noise_test_elapsed_time,
        .priv_param = &noise_test_priv_param,
        .priv_param_size = sizeof(noise_test_priv_param),
    };
    s64 open_test_elapsed_time = 0;
    struct cts_test_param open_test_param = {
        .test_item = CTS_TEST_OPEN,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &open_test_elapsed_time,
    };
    s64 short_test_elapsed_time = 0;
    struct cts_test_param short_test_param = {
        .test_item = CTS_TEST_SHORT,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &short_test_elapsed_time,
    };
    s64 comp_cap_test_elapsed_time = 0;
    struct cts_test_param comp_cap_test_param = {
        .test_item = CTS_TEST_COMPENSATE_CAP,
        .flags = CTS_TEST_FLAG_VALIDATE_DATA |
                 CTS_TEST_FLAG_VALIDATE_MIN |
                 CTS_TEST_FLAG_VALIDATE_MAX |
                 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
                 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
        .test_data_filepath = NULL,
        .num_invalid_node = 0,
        .invalid_nodes = NULL,
        .elapsed_time_ms = &comp_cap_test_elapsed_time,
    };
    const struct firmware *limit_fw = NULL;
    struct cts_limit limit;
    char header[7] = {'C', 'h', 'i', 'p', 'o', 'n', 'e'};
    int normal_rawdata_frames;
    int normal_rawdata_min;
    int normal_rawdata_max;
    int normal_noise_frames;
    int normal_noise_max;
    int normal_open_min;
    int normal_short_min;
    int normal_compcap_min;
    int normal_compcap_max;
    int int_pin_test_result;
    int reset_pin_test_result;
    int rawdata_test_result;
    int noise_test_result;
    int open_test_result;
    int short_test_result;
    int comp_cap_test_result;
    char touch_data_filepath[256];
    ktime_t start_time, end_time, delta_time;
    u16 fw_version;
    int ret;

    ret = request_firmware(&limit_fw, CFG_CTS_FACTORY_LIMIT_FILENAME,
            &cts_dev->pdata->ts_input_dev->dev);
    if (ret) {
        cts_err("Request %s failed!", CFG_CTS_FACTORY_LIMIT_FILENAME);
        return 0;
    }

    ret = memcmp(header, limit_fw->data, sizeof(header));
    if (ret) {
        cts_err("LIMIT file was not matched");
        release_firmware(limit_fw);
        return 0;
    } else {
        cts_info("Matched limit file!");
    }

    memcpy(&limit, limit_fw->data, limit_fw->size);
    release_firmware(limit_fw);

    cts_info("limit version         : %x", limit.version);
    cts_info("int_item              : %s", limit.int_item ? "Y" : "N");
    cts_info("reset_item            : %s", limit.reset_item ? "Y" : "N");
    cts_info("normal_rawdata_item   : %s", limit.normal_rawdata_item ? "Y" : "N");
    cts_info("normal_noise_item     : %s", limit.normal_noise_item ? "Y" : "N");
    cts_info("open_item             : %s", limit.open_item ? "Y" : "N");
    cts_info("short_item            : %s", limit.short_item ? "Y" : "N");
    cts_info("comp_cap_item         : %s", limit.comp_cap_item ? "Y" : "N");
    cts_info("gstr_rawdata_item     : %s", limit.gstr_rawdata_item ? "Y" : "N");
    cts_info("gstrlp_rawdata_item   : %s", limit.gstrlp_rawdata_item ? "Y" : "N");
    cts_info("gstr_noise_item       : %s", limit.gstr_noise_item ? "Y" : "N");
    cts_info("gstrlp_noise_item     : %s", limit.gstrlp_noise_item ? "Y" : "N");
    cts_info("type                  : %d", limit.type);
    cts_info("----------------------------");
    cts_info("normal_rawdata_frames : %d", limit.normal_rawdata_frames);
    cts_info("normal_rawdata_min    : %d", limit.normal_rawdata_min);
    cts_info("normal_rawdata_max    : %d", limit.normal_rawdata_max);
    cts_info("normal_noise_frames   : %d", limit.normal_noise_frames);
    cts_info("normal_noise_max      : %d", limit.normal_noise_max);
    cts_info("normal_open_min       : %d", limit.normal_open_min);
    cts_info("normal_short_min      : %d", limit.normal_short_min);
    cts_info("normal_compcap_min    : %d", limit.normal_compcap_min);
    cts_info("normal_compcap_max    : %d", limit.normal_compcap_max);
    cts_info("gstr_rawdata_frames   : %d", limit.gstr_rawdata_frames);
    cts_info("gstr_rawdata_min      : %d", limit.gstr_rawdata_min);
    cts_info("gstr_rawdata_max      : %d", limit.gstr_rawdata_max);
    cts_info("gstrlp_rawdata_frames : %d", limit.gstrlp_rawdata_frames);
    cts_info("gstrlp_rawdata_min    : %d", limit.gstrlp_rawdata_min);
    cts_info("gstrlp_rawdata_max    : %d", limit.gstrlp_rawdata_max);
    cts_info("gstr_noise_frames     : %d", limit.gstr_noise_frames);
    cts_info("gstr_noise_max        : %d", limit.gstr_noise_max);
    cts_info("gstrlp_noise_frames   : %d", limit.gstrlp_noise_frames);
    cts_info("gstrlp_noise_max      : %d", limit.gstrlp_noise_max);

    normal_rawdata_frames = limit.normal_rawdata_frames;
    normal_rawdata_min = limit.normal_rawdata_min;
    normal_rawdata_max = limit.normal_rawdata_max;
    normal_noise_frames = limit.normal_noise_frames;
    normal_noise_max = limit.normal_noise_max;
    normal_open_min = limit.normal_open_min;
    normal_short_min = limit.normal_short_min;
    normal_compcap_min = limit.normal_compcap_min;
    normal_compcap_max = limit.normal_compcap_max;

    rawdata_test_priv_param.frames = limit.normal_rawdata_frames;
    noise_test_priv_param.frames = limit.normal_noise_frames;
    rawdata_test_param.min = &normal_rawdata_min;
    rawdata_test_param.max = &normal_rawdata_max;
    noise_test_param.max = &normal_noise_max;
    open_test_param.min = &normal_open_min;
    short_test_param.min = &normal_short_min;
    comp_cap_test_param.min = &normal_compcap_min;
    comp_cap_test_param.max = &normal_compcap_max;

    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"/"RAWDATA_TEST_DATA_FILENAME);
    rawdata_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"/"NOISE_TEST_DATA_FILENAME);
    noise_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"/"OPEN_TEST_DATA_FILENAME);
    open_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"/"SHORT_TEST_DATA_FILENAME);
    short_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);
    snprintf(touch_data_filepath, sizeof(touch_data_filepath),
        TOUCH_DATA_DIRECTORY_PREFIX"/"COMP_CAP_TEST_DATA_FILENAME);
    comp_cap_test_param.test_data_filepath =
        kstrdup(touch_data_filepath, GFP_KERNEL);

    start_time = ktime_get();

    ret = cts_tcs_get_fw_ver(cts_dev, &fw_version);
    if (ret) {
        seq_printf(m, "Factory test get firmware version failed %d\n", ret);
        fw_version = 0;
    }

    if (limit.int_item) {
        int_pin_test_result = cts_test_int_pin(cts_dev, &int_pin_test_param);
    }
    if (limit.reset_item) {
        reset_pin_test_result = cts_test_reset_pin(cts_dev, &reset_pin_test_param);
    }
    if (limit.normal_rawdata_item) {
        rawdata_test_result = cts_test_rawdata(cts_dev, &rawdata_test_param);
    }
    if (limit.normal_noise_item) {
        noise_test_result = cts_test_noise(cts_dev, &noise_test_param);
    }
    if (limit.open_item) {
        open_test_result = cts_test_open(cts_dev, &open_test_param);
    }
    if (limit.short_item) {
        short_test_result = cts_test_short(cts_dev, &short_test_param);
    }
    if (limit.comp_cap_item) {
        comp_cap_test_result =
            cts_test_compensate_cap(cts_dev, &comp_cap_test_param);
    }

    if (rawdata_test_param.test_data_filepath) {
        kfree(rawdata_test_param.test_data_filepath);
    }
    if (noise_test_param.test_data_filepath) {
        kfree(noise_test_param.test_data_filepath);
    }
    if (open_test_param.test_data_filepath) {
        kfree(open_test_param.test_data_filepath);
    }
    if (short_test_param.test_data_filepath) {
        kfree(short_test_param.test_data_filepath);
    }
    if (comp_cap_test_param.test_data_filepath) {
        kfree(comp_cap_test_param.test_data_filepath);
    }

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    seq_printf(m, "Factory test, total ELAPSED TIME: %lldms\n",
        ktime_to_ms(delta_time));

    seq_printf(m, "Firmware Version: 0x%04X\n", fw_version);

    if (limit.int_item) {
        seq_printf(m, "Int-Pin  : %s,\n", int_pin_test_result ? "Failed" : "Pass");
    }
    if (limit.reset_item) {
        seq_printf(m, "Reset-Pin: %s\n", reset_pin_test_result ? "Failed" : "Pass");
    }
    if (limit.normal_rawdata_item) {
        seq_printf(m, "Rawdata  : %s\n", rawdata_test_result ? "Failed" : "Pass");
    }
    if (limit.normal_noise_item) {
        seq_printf(m, "Noise    : %s\n", noise_test_result ? "Failed" : "Pass");
    }
    if (limit.open_item) {
        seq_printf(m, "Open     : %s\n", open_test_result ? "Failed" : "Pass");
    }
    if (limit.short_item) {
        seq_printf(m, "Short    : %s\n", short_test_result ? "Failed" : "Pass");
    }
    if (limit.comp_cap_item) {
        seq_printf(m, "Comp-Cap : %s\n", comp_cap_test_result ? "Failed" : "Pass");
    }

    return 0;

#undef TOUCH_DATA_DIRECTORY_PREFIX
#undef RAWDATA_TEST_DATA_FILENAME
#undef NOISE_TEST_DATA_FILENAME
#undef OPEN_TEST_DATA_FILENAME
#undef SHORT_TEST_DATA_FILENAME
#undef COMP_CAP_TEST_DATA_FILENAME
}
static int cts_factory_test_open(struct inode *inode, struct file *file)
{
    return single_open(file, cts_factory_test_show, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static const struct file_operations cts_factory_test_ops = {
    .owner = THIS_MODULE,
    .open = cts_factory_test_open,
    .read = seq_read,
};
#else
static const struct proc_ops cts_factory_test_ops = {
    .proc_open = cts_factory_test_open,
    .proc_read = seq_read,
};
#endif


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

#ifdef CONFIG_CTS_TP_WORK_IRQ
	cts_data->procfs_tp_work = proc_create_data(CTS_TP_WORK_PROC_FILE,
		0666, NULL, &cts_tp_work_proc_fops, cts_data);
	if (cts_data->procfs_tp_work == NULL) {
		cts_err("creat /proc/%s failed\n",CTS_TP_WORK_PROC_FILE);
		ret = -EFAULT;
		goto free_oem_data;
	} else {
		cts_info("creat /proc/%s success\n", CTS_TP_WORK_PROC_FILE);
	}
#endif

    oem_data->limit_entry =
        proc_create_data(OEM_LIMIT_PROC_FILENAME,
            S_IRUGO, NULL, &cts_limit_ops, cts_data);
    if (oem_data->limit_entry == NULL) {
        cts_err("Create '/proc/"OEM_LIMIT_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }
#ifdef CONFIG_CTS_TP_DATA_DUMP
	cts_data->tp_data_dump_proc = proc_create_data(CTS_PROC_TP_DATA_DUMP,
		0444, NULL, &cts_tp_data_dump_proc_fops, cts_data);
	if (cts_data->tp_data_dump_proc == NULL) {
		cts_err( "ERROR: create /proc/%s failed.", CTS_PROC_TP_DATA_DUMP);
		ret = -EFAULT;
		goto err_create_procfs_fail;
	}
	cts_info("create /proc/%s", CTS_PROC_TP_DATA_DUMP);
#endif

#ifdef CONFIG_CTS_TP_GRIP_AREA
	ret = init_lct_tp_grip_area(lct_cts_tp_set_screen_angle_callback, lct_cts_tp_get_screen_angle_callback);
	if (ret < 0) {
		cts_err("init_lct_tp_grip_area Failed!");
		goto err_init_lct_tp_grip_area_fail;
	} else {
		cts_info("init_lct_tp_grip_area Succeeded!");
	}
#endif

    oem_data->factory_test_entry =
        proc_create_data(OEM_FACTORY_TEST_PROC_FILENAME,
            S_IRUGO, NULL, &cts_factory_test_ops, cts_data);
    if (oem_data->factory_test_entry == NULL) {
        cts_err("Create '/proc/"OEM_FACTORY_TEST_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

    oem_data->rawdata_proc_entry = proc_create_data(OEM_RAWDATA_PROC_FILENAME,
            S_IRUGO, NULL, &cts_rawdata_ops, cts_data);
    if (oem_data->rawdata_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_RAWDATA_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

    oem_data->manual_proc_entry = proc_create_data(OEM_MANUAL_PROC_FILENAME,
            S_IRUGO, NULL, &cts_manual_ops, cts_data);
    if (oem_data->manual_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_MANUAL_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

    oem_data->diffdata_proc_entry = proc_create_data(OEM_DIFFDATA_PROC_FILENAME,
            S_IRUGO, NULL, &cts_diffdata_ops, cts_data);
    if (oem_data->diffdata_proc_entry == NULL) {
        cts_err("Create '/proc/"OEM_DIFFDATA_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

//create longcheer procfs node
	ret = init_lct_tp_info("[Vendor]unkown,[FW]unkown,[IC]unkown\n", NULL);
	if (ret < 0) {
		cts_info("init_lct_tp_info Failed!\n");
		goto err_init_lct_tp_info_failed;
	} else {
		cts_info("init_lct_tp_info Succeeded!\n");
        }
	ret = init_lct_tp_selftest(lct_cts_tp_selftest_callback);
	if (ret < 0) {
		cts_err("init_lct_tp_selftest Failed!");
		goto err_init_lct_tp_selftest_fail;
	} else {
		cts_err("init_lct_tp_selftest Succeeded!");
	}
    ret = init_lct_tp_proximity_switch(lct_cts_tp_proximity_switch_callback);
	if (ret < 0) {
		cts_info("init_lct_tp_proximity_switch Failed!\n");
		goto err_init_lct_tp_proximity_switch_failed;
	} else {
		cts_info("init_lct_tp_proximity_switch Succeeded!\n");
	}

    cts_data->oem_data = oem_data;
    oem_data->cts_data = cts_data;
    return 0;

err_init_lct_tp_proximity_switch_failed:
    uninit_lct_tp_proximity_switch();
err_init_lct_tp_selftest_fail:
    uninit_lct_tp_selftest();
err_init_lct_tp_info_failed:
    uninit_lct_tp_info();

#ifdef CONFIG_CTS_TP_GRIP_AREA
err_init_lct_tp_grip_area_fail:
    uninit_lct_tp_grip_area();
#endif

#ifdef CONFIG_CTS_TP_DATA_DUMP
err_create_procfs_fail:
	cts_err( "ERROR: create /proc/%s failed.", CTS_PROC_TP_DATA_DUMP);
#endif

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
#ifdef CONFIG_CTS_TP_WORK_IRQ
    if (cts_data->procfs_tp_work) {
	cts_info("	Remove '/proc/"CTS_TP_WORK_PROC_FILE"'");
	remove_proc_entry(CTS_TP_WORK_PROC_FILE, NULL);
	}
#endif
#ifdef CONFIG_CTS_TP_DATA_DUMP
	if (cts_data->tp_data_dump_proc) {
		cts_info("	Remove '/proc/"CTS_PROC_TP_DATA_DUMP"'");
		remove_proc_entry(CTS_PROC_TP_DATA_DUMP, NULL);
	}
#endif

#ifdef CONFIG_CTS_TP_GRIP_AREA
	uninit_lct_tp_grip_area();
#endif

    if (oem_data->limit_entry) {
        cts_info("  Remove '/proc/"OEM_LIMIT_PROC_FILENAME"'");
        remove_proc_entry(OEM_LIMIT_PROC_FILENAME, NULL);
    }
    if (oem_data->factory_test_entry) {
        cts_info("  Remove '/proc/"OEM_FACTORY_TEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_FACTORY_TEST_PROC_FILENAME, NULL);
    }

    if (oem_data->rawdata_proc_entry) {
        cts_info("  Remove '/proc/"OEM_RAWDATA_PROC_FILENAME"'");
        remove_proc_entry(OEM_RAWDATA_PROC_FILENAME, NULL);
    }

    if (oem_data->manual_proc_entry) {
        cts_info("  Remove '/proc/"OEM_MANUAL_PROC_FILENAME"'");
        remove_proc_entry(OEM_MANUAL_PROC_FILENAME, NULL);
    }

    if (oem_data->diffdata_proc_entry) {
        cts_info("  Remove '/proc/"OEM_DIFFDATA_PROC_FILENAME"'");
        remove_proc_entry(OEM_DIFFDATA_PROC_FILENAME, NULL);
    }

    uninit_lct_tp_info();
    uninit_lct_tp_selftest();
    uninit_lct_tp_proximity_switch();

    free_selftest_data_mem(oem_data);

    kfree(cts_data->oem_data);
    cts_data->oem_data = NULL;

    return 0;
}

