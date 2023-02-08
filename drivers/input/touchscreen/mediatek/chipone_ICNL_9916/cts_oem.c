#define LOG_TAG         "Oem"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_oem.h"
#include "cts_test.h"
#include "cts_firmware.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#define CTS_PROC_ANDROID_TOUCH_DIR          "android_touch"
#ifdef CONFIG_CTS_TP_INFO
#define CTS_PROC_TP_INFO					"tp_info"
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08  start */
#ifdef CONFIG_CTS_TP_LOCKDOWN_INFO
#define CTS_PROC_TP_LOCKDOWN_INFO			"tp_lockdown_info"
#endif
char ts_lockdowninfo[17] = {0};
/*C3T code for HQ-229304 by zenghui at 2022/10/08  end */
#ifdef CONFIG_CTS_TP_DATA_DUMP
#define CTS_PROC_TP_DATA_DUMP				"tp_data_dump"
#endif
#ifdef CONFIG_CTS_TP_SELFTEST
#define CTS_PROC_TP_SELFTEST				"tp_selftest"
#define TP_SELFTEST_SPI						"spi"
#define TP_SELFTEST_OPEN					"open"
#define TP_SELFTEST_SHORT					"short"
int tp_selftest_result;
#endif

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
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN		1200
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX		2800
#define OEM_OF_DEF_PROPVAL_NOISE_FRAMES     50
#define OEM_OF_DEF_PROPVAL_NOISE_MAX        80
/*C3T code for HQ-254102 by jishen at 2022/10/09  start */
#define OEM_OF_DEF_PROPVAL_OPEN_MIN			1500
/*C3T code for HQ-254102 by jishen at 2022/10/09  end */

/*C3T code for HQ-254102 by jishen at 2022/10/09  start */
#define OEM_OF_DEF_PROPVAL_SHORT_MIN		1800
/*C3T code for HQ-254102 by jishen at 2022/10/09  end */
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MIN     1
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MAX     126

#define OEM_OF_PROPNAME_PREFIX                 "chipone," //modify

#define OEM_OF_PROPNAME_TEST_RESET_PIN      OEM_OF_PROPNAME_PREFIX"test-reset-pin"
#define OEM_OF_PROPNAME_TEST_INT_PIN        OEM_OF_PROPNAME_PREFIX"test-int-pin"
#define OEM_OF_PROPNAME_TEST_RAWDATA        OEM_OF_PROPNAME_PREFIX"test-rawdata"
#define OEM_OF_PROPNAME_RAWDATA_FRAMES      OEM_OF_PROPNAME_PREFIX"test-rawdata-frames"
#define OEM_OF_PROPNAME_RAWDATA_MIN			OEM_OF_PROPNAME_PREFIX"rawdata-min"
#define OEM_OF_PROPNAME_RAWDATA_MAX			OEM_OF_PROPNAME_PREFIX"rawdata-max"
#define OEM_OF_PROPNAME_TEST_NOISE          OEM_OF_PROPNAME_PREFIX"test-noise"
#define OEM_OF_PROPNAME_NOISE_FRAMES        OEM_OF_PROPNAME_PREFIX"test-noise-frames"
#define OEM_OF_PROPNAME_NOISE_MAX           OEM_OF_PROPNAME_PREFIX"noise-max"
#define OEM_OF_PROPNAME_TEST_OPEN           OEM_OF_PROPNAME_PREFIX"test-open"
#define OEM_OF_PROPNAME_OPEN_MIN			OEM_OF_PROPNAME_PREFIX"open-min"
#define OEM_OF_PROPNAME_TEST_SHORT          OEM_OF_PROPNAME_PREFIX"test-short"
#define OEM_OF_PROPNAME_SHORT_MIN			OEM_OF_PROPNAME_PREFIX"short-min"
#define OEM_OF_PROPNAME_TEST_COMP_CAP       OEM_OF_PROPNAME_PREFIX"test-compensate-cap"
#define OEM_OF_PROPNAME_COMP_CAP_MIN        OEM_OF_PROPNAME_PREFIX"compensate-cap-min"
#define OEM_OF_PROPNAME_COMP_CAP_MAX        OEM_OF_PROPNAME_PREFIX"compensate-cap-max"

#define OEM_SELFTEST_PROC_FILENAME          "self_test"

#define OEM_TEST_DATA_DIR                   "/sdcard"
#define OEM_RAWDATA_TEST_DATA_FILEPATH      OEM_TEST_DATA_DIR"/FWMutualTest.csv"
#define OEM_NOISE_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/NoiseTest.csv"
#define OEM_OPEN_TEST_DATA_FILEPATH         OEM_TEST_DATA_DIR"/OpenTest.csv"
#define OEM_SHORT_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/ShortTest.csv"
#define OEM_COMP_CAP_TEST_DATA_FILEPATH     OEM_TEST_DATA_DIR"/FWCCTest.csv"

struct cts_oem_data {
	struct proc_dir_entry *cts_proc_android_touch_dir;
	
    struct proc_dir_entry *self_test_proc_entry;

    struct proc_dir_entry *tp_info_proc_entry;
	
	struct proc_dir_entry *tp_lockdown_info_proc_entry;

    struct proc_dir_entry *tp_data_dump_proc_entry;

    struct proc_dir_entry *tp_selftest_proc_entry;

    bool test_config_from_dt_has_parsed;
	bool tp_selftest_from_dt_has_parsed;

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
static int alloc_selftest_data_mem(struct cts_oem_data *oem_data, int nodes)
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

static int alloc_tp_selftest_data_mem(struct cts_oem_data *oem_data, int nodes)
{
	cts_info("Alloc tp selftest data");

	ALLOC_TEST_DATA_MEM(open, nodes * 2);
	ALLOC_TEST_DATA_MEM(short, nodes * 2 * 10);

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
    oem_data->test_int_pin = OEM_OF_DEF_PROPVAL_TEST_INT_PIN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_INT_PIN);

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

    return 0;
}

static void print_selftest_config(const struct cts_oem_data *oem_data)
{
    cts_info("Seltest configuration:");

	/** reset pin **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_RESET_PIN,
		oem_data->test_reset_pin ? 'Y' : 'N');

	/** int pin **/
    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_INT_PIN,
		oem_data->test_int_pin ? 'Y' : 'N');

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
            cts_err("Test int pin failed %d,%d", oem_data->int_pin_test_result);
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
	            cts_err("Write to file '%s' failed %d",
	                filepath, ret);
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

#if 0
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
#endif

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

#if 0
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
#endif

static int save_selftest_data_to_file(struct cts_oem_data *oem_data)
{
    int rows, cols, hw_cols;
    int ret;

    cts_info("Save selftest data to file");

    rows	= oem_data->cts_data->cts_dev.fwdata.rows;
    cols	= oem_data->cts_data->cts_dev.fwdata.cols;
	hw_cols	= oem_data->cts_data->cts_dev.hwdata->num_col;

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
			oem_data->noise_test_frames, rows, cols, hw_cols);
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
	char buf[8] = {0};

    cts_info("Show seq selftest");

    if (cts_data == NULL) {
        cts_err("Selftest seq file private data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;

    seq_printf(m, "FW Version %04x!\n\n", cts_data->cts_dev.fwdata.version);

/*
 * 0-rawdata, 1-noise, 2-open, 3-short, 4-comp cap
 */

    if (oem_data->test_rawdata) {
        if (oem_data->rawdata_test_result == 0)
			buf[0] = 'P';
        else if (oem_data->rawdata_test_result > 0)
        	buf[0] = 'F';
        else
			buf[0] = 'F';
    }
    if (oem_data->test_noise) {
        if (oem_data->noise_test_result == 0)
			buf[1] = 'P';
        else if (oem_data->noise_test_result > 0)
			buf[1] = 'F';
        else
			buf[1] = 'F';
    }
    if (oem_data->test_open) {
        if (oem_data->open_test_result == 0)
			buf[2] = 'P';
        else if (oem_data->open_test_result > 0)
			buf[2] = 'F';
        else
			buf[2] = 'F';
    }
    if (oem_data->test_short) {
        if (oem_data->short_test_result == 0)
			buf[3] = 'P';
        else if (oem_data->short_test_result > 0)
			buf[3] = 'F';
        else
			buf[3] = 'F';
    }
    if (oem_data->test_comp_cap) {
        if (oem_data->comp_cap_test_result == 0)
			buf[4] = 'P';
        else if (oem_data->comp_cap_test_result > 0)
			buf[4] = 'F';
        else
			buf[4] = 'F';
    }

	seq_printf(m, " 0%c-1%c-2%c-3%c-4%c\n\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

    return 0;
}

const struct seq_operations selftest_seq_ops = {
    .start  = selftest_seq_start,
    .next   = selftest_seq_next,
    .stop   = selftest_seq_stop,
    .show   = selftest_seq_show,
};

static int32_t self_test_proc_open(struct inode *inode, struct file *file)
{
    struct chipone_ts_data *cts_data = PDE_DATA(inode);
    struct cts_oem_data *oem_data = NULL;
    int ret;

    if (cts_data == NULL) {
        cts_err("Open self_test proc with cts_data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;
    if (oem_data == NULL) {
        cts_err("Open self_test proc with oem_data = NULL");
        return -EFAULT;
    }

    cts_info("Open '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"/"OEM_SELFTEST_PROC_FILENAME"'");

    if (!oem_data->test_config_from_dt_has_parsed) {
        parse_selftest_dt(oem_data, cts_data->pdata->spi_client->dev.of_node);
	    oem_data->test_config_from_dt_has_parsed = true;
    }

    print_selftest_config(oem_data);

    ret = alloc_selftest_data_mem(oem_data,
        cts_data->cts_dev.hwdata->num_row * cts_data->cts_dev.hwdata->num_col);
    if (ret) {
        cts_err("Alloc test data mem failed");
        return ret;
    }

    do_selftest(oem_data);

    ret = save_selftest_data_to_file(oem_data);
    if (ret) {
        cts_err("Save self_test data to file failed %d", ret);
    }

    ret = seq_open(file, &selftest_seq_ops);
    if (ret) {
        cts_err("Open self_test seq file failed %d", ret);
        return ret;
    }

    ((struct seq_file *)file->private_data)->private = cts_data;

    return 0;
}

static const struct file_operations self_test_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = self_test_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

#ifdef CONFIG_CTS_TP_INFO
static void *tp_info_seq_start(struct seq_file *m, loff_t *pos)
{
    return *pos < 1 ? (void *)1 : NULL;
}

static void *tp_info_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

static void tp_info_seq_stop(struct seq_file *m, void *v)
{
    return;
}

static int tp_info_seq_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)m->private;

	/*C3T code for HQ-244496 by jishen at 2022/10/01  start */
	seq_printf(m, "[Vendor]TianMa, [TP-IC]:ICNL9916, [FW]0x%04x\n\n", cts_data->cts_dev.fwdata.version);
	/*C3T code for HQ-244496 by jishen at 2022/10/01  end */

    return 0;
}

const struct seq_operations tp_info_seq_ops = {
    .start  = tp_info_seq_start,
    .next   = tp_info_seq_next,
    .stop   = tp_info_seq_stop,
    .show   = tp_info_seq_show,
};

static int32_t tp_info_proc_open(struct inode *inode, struct file *file)
{
    struct chipone_ts_data *cts_data = PDE_DATA(inode);
    struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;

	ret = cts_stop_device(cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
		goto err_start_device;
	}

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_fw_ver(cts_dev, &cts_dev->fwdata.version);
	if (ret < 0) {
		cts_err("get_fw_ver failed");
		goto err_unlock;
	}

    ret = seq_open(file, &tp_info_seq_ops);
	if (ret)
		cts_err("Open tp_info seq file failed %d", ret);

	((struct seq_file *)file->private_data)->private = cts_data;
	
err_unlock:
	cts_unlock_device(cts_dev);
err_start_device:
	ret = cts_start_device(cts_dev);
	if (ret)
		cts_err("Start device failed %d", ret);

	return ret;
}

static const struct file_operations tp_info_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = tp_info_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};
#endif

/*C3T code for HQ-229304 by zenghui at 2022/10/08 start*/
#ifdef CONFIG_CTS_TP_LOCKDOWN_INFO
static int __init is_lockdown_info_detect(char *str)
{
  	strlcpy(ts_lockdowninfo, str, sizeof(ts_lockdowninfo));
  	return 0;
}
__setup("tp_lockdown_info=", is_lockdown_info_detect);

static void *tp_lockdown_info_seq_start(struct seq_file *m, loff_t *pos)
{
    return *pos < 1 ? (void *)1 : NULL;
}

static void *tp_lockdown_info_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

static void tp_lockdown_info_seq_stop(struct seq_file *m, void *v)
{
    return;
}

static int tp_lockdown_info_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", ts_lockdowninfo);
    return 0;
}

const struct seq_operations tp_lockdown_info_seq_ops = {
    .start  = tp_lockdown_info_seq_start,
    .next   = tp_lockdown_info_seq_next,
    .stop   = tp_lockdown_info_seq_stop,
    .show   = tp_lockdown_info_seq_show,
};

static int32_t tp_lockdown_info_proc_open(struct inode *inode, struct file *file)
{
    struct chipone_ts_data *cts_data = PDE_DATA(inode);
    struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;

	ret = cts_stop_device(cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
		goto err_start_device;
	}

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_fw_ver(cts_dev, &cts_dev->fwdata.version);
	if (ret < 0) {
		cts_err("get_fw_ver failed");
		goto err_unlock;
	}

    ret = seq_open(file, &tp_lockdown_info_seq_ops);
	if (ret)
		cts_err("Open tp_lockdown_info seq file failed %d", ret);

	((struct seq_file *)file->private_data)->private = cts_data;
	
err_unlock:
	cts_unlock_device(cts_dev);
err_start_device:
	ret = cts_start_device(cts_dev);
	if (ret)
		cts_err("Start device failed %d", ret);

	return ret;
}

static const struct file_operations tp_lockdown_info_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = tp_lockdown_info_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08 end*/
#ifdef CONFIG_CTS_TP_DATA_DUMP
extern int cts_test_polling_rawdata(struct cts_device *cts_dev,
        u8 *buf, size_t size);
static int tp_data_dump_proc_show(struct seq_file *m, void *v)
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

    cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
	//get rawdata end

	//get diffdata start
    ret = cts_dev->ops->top_get_real_diff(cts_dev, (u8 *)diffdata, DATA_BUFFER_SIZE(cts_dev));
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

static int tp_data_dump_proc_open(struct inode* inode, struct file* file)
{
	return single_open(file, tp_data_dump_proc_show,  PDE_DATA(inode));
}

static const struct file_operations tp_data_dump_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = tp_data_dump_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};
#endif

#ifdef CONFIG_CTS_TP_SELFTEST
static int tp_selftest_proc_open(struct inode *inode, struct file *file)
{
    file->private_data = PDE_DATA(inode);
    return 0;
}

static ssize_t tp_selftest_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	ssize_t cnt=0;
	char *page = NULL;

	if (*ppos)
		return 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;
	
	cnt = sprintf(page, "%s", ((tp_selftest_result == 2) ? "2\n" : 
		(tp_selftest_result == 1) ? "1\n" : "0\n"));
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	
	cts_info("tp_selftest_result: %s", page);

	kfree(page);
	return cnt;

}

static ssize_t tp_selftest_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct chipone_ts_data *cts_data = NULL;
    struct cts_oem_data *oem_data = NULL;
    struct cts_test_param test_param;
	char tmp_data[64] = {0};

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("tp selftest proc with cts_data = NULL");
        return -EIO;
    }

    oem_data = cts_data->oem_data;
    if (oem_data == NULL) {
        cts_err("tp selftest proc with oem_data = NULL");
        return -EFAULT;
    }
	
	if (!oem_data->tp_selftest_from_dt_has_parsed) {
		parse_selftest_dt(oem_data, cts_data->pdata->spi_client->dev.of_node);
		oem_data->tp_selftest_from_dt_has_parsed = true;
	}

    alloc_tp_selftest_data_mem(oem_data,
        cts_data->cts_dev.hwdata->num_row * cts_data->cts_dev.hwdata->num_col);

	if (copy_from_user(tmp_data, buf, size)) {
		cts_err("copy_from_user() fail.\n");
		return -EFAULT;
	}

	cts_info("read data from user is %s", tmp_data);

	if((strncmp(tmp_data, TP_SELFTEST_SPI, strlen(TP_SELFTEST_SPI)) == 0)) {
		if (!cts_plat_is_normal_mode(cts_data->pdata))
			tp_selftest_result = 1;
		else
			tp_selftest_result = 2;
	} else if ((strncmp(tmp_data, TP_SELFTEST_OPEN, strlen(TP_SELFTEST_OPEN)) == 0)) {
		memset(&test_param, 0, sizeof(test_param));
		oem_data->open_test_data_wr_size = 0;
		test_param.test_item = CTS_TEST_OPEN;
		test_param.flags =
			CTS_TEST_FLAG_VALIDATE_DATA |
			CTS_TEST_FLAG_VALIDATE_MIN |
			CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
			CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE;
		test_param.min = &oem_data->open_min;
		test_param.test_data_buf = oem_data->open_test_data;
		test_param.test_data_buf_size = oem_data->open_test_data_buff_size;
		test_param.test_data_wr_size = &oem_data->open_test_data_wr_size;
		
		oem_data->open_test_result =
			cts_test_open(&oem_data->cts_data->cts_dev, &test_param);
		if (oem_data->open_test_result > 0)
			tp_selftest_result = 1;
		else if (oem_data->open_test_result < 0)
			tp_selftest_result = 0;
		else
			tp_selftest_result = 2;
	} else if ((strncmp(tmp_data, TP_SELFTEST_SHORT, strlen(TP_SELFTEST_SHORT)) == 0)) {
		memset(&test_param, 0, sizeof(test_param));
		oem_data->short_test_data_wr_size = 0;
		test_param.test_item = CTS_TEST_SHORT;
		test_param.flags =
		    CTS_TEST_FLAG_VALIDATE_DATA |
		    CTS_TEST_FLAG_VALIDATE_MIN |
		    CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
		    CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE;
		test_param.min = &oem_data->short_min;
		test_param.test_data_buf = oem_data->short_test_data;
		test_param.test_data_buf_size = oem_data->short_test_data_buff_size;
		test_param.test_data_wr_size = &oem_data->short_test_data_wr_size;

		oem_data->short_test_result =
		    cts_test_short(&oem_data->cts_data->cts_dev, &test_param);
		if (oem_data->short_test_result > 0)
			tp_selftest_result = 1;
		else if (oem_data->short_test_result < 0)
			tp_selftest_result = 0;
		else
			tp_selftest_result = 2;
	} else {
		cts_err("invalid arg, need echo spi/open/short > /proc/tp_selftest!");
		return -EINVAL;
	}

	return size;
}

static const struct file_operations tp_selftest_proc_fops = {
	.owner = THIS_MODULE,
	.open  = tp_selftest_proc_open,
	.read = tp_selftest_proc_read,
	.write = tp_selftest_proc_write,
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

	cts_info(" - Create '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"'");
	oem_data->cts_proc_android_touch_dir = 
		proc_mkdir(CTS_PROC_ANDROID_TOUCH_DIR, NULL);

	cts_info(" - Create '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"/"OEM_SELFTEST_PROC_FILENAME"'");
    oem_data->self_test_proc_entry =
        proc_create_data(OEM_SELFTEST_PROC_FILENAME,
            S_IRUGO, oem_data->cts_proc_android_touch_dir, &self_test_proc_fops, cts_data);
    if (oem_data->self_test_proc_entry == NULL) {
        cts_err("Create '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"/"OEM_SELFTEST_PROC_FILENAME"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }

#ifdef CONFIG_CTS_TP_INFO
	cts_info(" - Create '/proc/"CTS_PROC_TP_INFO"'");
    oem_data->tp_info_proc_entry =
        proc_create_data(CTS_PROC_TP_INFO,
            S_IRUGO, NULL, &tp_info_proc_fops, cts_data);
    if (oem_data->tp_info_proc_entry == NULL) {
        cts_err("Create '/proc/"CTS_PROC_TP_INFO"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08  start */
#ifdef CONFIG_CTS_TP_LOCKDOWN_INFO
	cts_info(" - Create '/proc/"CTS_PROC_TP_LOCKDOWN_INFO"'");
    oem_data->tp_lockdown_info_proc_entry =
        proc_create_data(CTS_PROC_TP_LOCKDOWN_INFO,
            S_IRUGO, NULL, &tp_lockdown_info_proc_fops, cts_data);
    if (oem_data->tp_lockdown_info_proc_entry == NULL) {
        cts_err("Create '/proc/"CTS_PROC_TP_LOCKDOWN_INFO"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08  end */
#ifdef CONFIG_CTS_TP_DATA_DUMP
	cts_info(" - Create '/proc/"CTS_PROC_TP_DATA_DUMP"'");
    oem_data->tp_data_dump_proc_entry =
        proc_create_data(CTS_PROC_TP_DATA_DUMP,
            S_IRUGO, NULL, &tp_data_dump_proc_fops, cts_data);
    if (oem_data->tp_data_dump_proc_entry == NULL) {
        cts_err("Create '/proc/"CTS_PROC_TP_DATA_DUMP"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }
#endif

#ifdef CONFIG_CTS_TP_SELFTEST
	cts_info(" - Create '/proc/"CTS_PROC_TP_SELFTEST"'");
    oem_data->tp_selftest_proc_entry =
        proc_create_data(CTS_PROC_TP_SELFTEST,
            S_IRUGO, NULL, &tp_selftest_proc_fops, cts_data);
    if (oem_data->tp_selftest_proc_entry == NULL) {
        cts_err("Create '/proc/"CTS_PROC_TP_SELFTEST"' failed");
        ret = -EFAULT;
        goto free_oem_data;
    }
#endif

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

    if (oem_data->self_test_proc_entry) {
        cts_info("Remove '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"/"OEM_SELFTEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, oem_data->cts_proc_android_touch_dir);
    }

	if (oem_data->cts_proc_android_touch_dir) {
		cts_info("Remove '/proc/"CTS_PROC_ANDROID_TOUCH_DIR"'");
		remove_proc_entry(CTS_PROC_ANDROID_TOUCH_DIR, NULL);
	}

#ifdef CONFIG_CTS_TP_INFO
	if (oem_data->tp_info_proc_entry) {
		cts_info("Remove '/proc/"CTS_PROC_TP_INFO"'");
		remove_proc_entry(CTS_PROC_TP_INFO, NULL);
	}
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08  start */
#ifdef CONFIG_CTS_TP_LOCKDOWN_INFO
	if (oem_data->tp_lockdown_info_proc_entry) {
		cts_info("Remove '/proc/"CTS_PROC_TP_LOCKDOWN_INFO"'");
		remove_proc_entry(CTS_PROC_TP_LOCKDOWN_INFO, NULL);
	}
#endif
/*C3T code for HQ-229304 by zenghui at 2022/10/08  end */
#ifdef CONFIG_CTS_TP_DATA_DUMP
	if (oem_data->tp_data_dump_proc_entry) {
		cts_info("Remove '/proc/"CTS_PROC_TP_DATA_DUMP"'");
		remove_proc_entry(CTS_PROC_TP_DATA_DUMP, NULL);
	}
#endif
#ifdef CONFIG_CTS_TP_SELFTEST
	if (oem_data->tp_selftest_proc_entry) {
		cts_info("Remove '/proc/"CTS_PROC_TP_SELFTEST"'");
		remove_proc_entry(CTS_PROC_TP_SELFTEST, NULL);
	}
#endif
    free_selftest_data_mem(oem_data);

    kfree(cts_data->oem_data);
    cts_data->oem_data = NULL;

    return 0;
}

