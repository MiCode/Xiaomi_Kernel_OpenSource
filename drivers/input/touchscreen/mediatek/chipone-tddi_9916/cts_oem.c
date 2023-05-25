#define LOG_TAG         "Oem"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_oem.h"
#include "cts_test.h"
#include "cts_firmware.h"
#include "cts_tcs.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#ifdef CONFIG_CTS_TP_WORK_IRQ
#define CTS_TP_WORK_PROC_FILE           	 	"tp_work"

#define LC_TP_WORK_ENABLE_IRQ_CMD         "1"
#define LC_TP_WORK_DISABLE_IRQ_CMD        "0"
bool cts_irq_status;
#endif

#ifdef CONFIG_CTS_GESTURE_CALLBACK
bool cts_gesture_mode = 0;//1--enable;0--disable
#endif

#ifdef CONFIG_CTS_TP_DATA_DUMP
#define CTS_PROC_TP_DATA_DUMP "tp_data_dump"
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
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN		1290
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX		2833
#define OEM_OF_DEF_PROPVAL_NOISE_FRAMES     16
#define OEM_OF_DEF_PROPVAL_NOISE_MAX        100
#define OEM_OF_DEF_PROPVAL_OPEN_MIN			1433
#define OEM_OF_DEF_PROPVAL_SHORT_MIN		800
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MIN     1
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MAX     126
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN_HOLITECH      1290
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX_HOLITECH      2833
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN_CTC     1283
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX_CTC     2846
#define OEM_OF_DEF_PROPVAL_OPEN_MIN_HOLITECH   3674
#define OEM_OF_DEF_PROPVAL_OPEN_MIN_CTC  3465
#define OEM_OF_DEF_PROPVAL_SHORT_MIN_HOLITECH  500
#define OEM_OF_DEF_PROPVAL_SHORT_MIN_CTC 800
#endif

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
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
#define OEM_OF_PROPNAME_RAWDATA_MIN_HOLITECH   OEM_OF_PROPNAME_PREFIX"rawdata-min-holitech"
#define OEM_OF_PROPNAME_RAWDATA_MAX_HOLITECH   OEM_OF_PROPNAME_PREFIX"rawdata-max-holitech"
#define OEM_OF_PROPNAME_RAWDATA_MIN_CTC  OEM_OF_PROPNAME_PREFIX"rawdata-min-ctc"
#define OEM_OF_PROPNAME_RAWDATA_MAX_CTC  OEM_OF_PROPNAME_PREFIX"rawdata-max-ctc"
#define OEM_OF_PROPNAME_OPEN_MIN_HOLITECH      OEM_OF_PROPNAME_PREFIX"open-min-holitech"
#define OEM_OF_PROPNAME_OPEN_MIN_CTC     OEM_OF_PROPNAME_PREFIX"open-min-ctc"
#define OEM_OF_PROPNAME_SHORT_MIN_HOLITECH     OEM_OF_PROPNAME_PREFIX"short-min-holitech"
#define OEM_OF_PROPNAME_SHORT_MIN_CTC    OEM_OF_PROPNAME_PREFIX"short-min-ctc"
#endif

#define OEM_SELFTEST_PROC_FILENAME          "tp_selftest"

#define OEM_TEST_DATA_DIR                   "/sdcard"
#define OEM_RAWDATA_TEST_DATA_FILEPATH      OEM_TEST_DATA_DIR"/FWMutualTest.csv"
#define OEM_NOISE_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/NoiseTest.csv"
#define OEM_OPEN_TEST_DATA_FILEPATH         OEM_TEST_DATA_DIR"/OpenTest.csv"
#define OEM_SHORT_TEST_DATA_FILEPATH        OEM_TEST_DATA_DIR"/ShortTest.csv"
#define OEM_COMP_CAP_TEST_DATA_FILEPATH     OEM_TEST_DATA_DIR"/FWCCTest.csv"

struct cts_oem_data {
    struct proc_dir_entry *selftest_proc_entry;
    struct proc_dir_entry *proc_file_tp_hardware_test_entry;

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

    oem_data->test_reset_pin = OEM_OF_DEF_PROPVAL_TEST_RESET_PIN ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RESET_PIN);

    oem_data->test_int_pin = OEM_OF_DEF_PROPVAL_TEST_INT_PIN;

    oem_data->test_rawdata = OEM_OF_DEF_PROPVAL_TEST_RAWDATA ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_RAWDATA);
    if (oem_data->test_rawdata) {
        oem_data->rawdata_test_frames = OEM_OF_DEF_PROPVAL_RAWDATA_FRAMES;
        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_FRAMES,
            &oem_data->rawdata_test_frames);
        if (ret) {
            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_FRAMES"' failed %d", ret);
        }
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
	        oem_data->rawdata_min = OEM_OF_DEF_PROPVAL_RAWDATA_MIN_HOLITECH;
	        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MIN_HOLITECH,
	            &oem_data->rawdata_min);
	        if (ret) {
	            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MIN_HOLITECH"' failed %d", ret);
	        }

	        oem_data->rawdata_max = OEM_OF_DEF_PROPVAL_RAWDATA_MAX_HOLITECH;
	        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MAX_HOLITECH,
	            (u32 *)&oem_data->rawdata_max);
	        if (ret) {
	            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MAX_HOLITECH"' failed %d", ret);
	        }
		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
	        oem_data->rawdata_min = OEM_OF_DEF_PROPVAL_RAWDATA_MIN_CTC;
	        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MIN_CTC,
	            &oem_data->rawdata_min);
	        if (ret) {
	            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MIN_CTC"' failed %d", ret);
	        }

	        oem_data->rawdata_max = OEM_OF_DEF_PROPVAL_RAWDATA_MAX_CTC;
	        ret = of_property_read_u32(np, OEM_OF_PROPNAME_RAWDATA_MAX_CTC,
	            (u32 *)&oem_data->rawdata_max);
	        if (ret) {
	            cts_warn("Parse '"OEM_OF_PROPNAME_RAWDATA_MAX_CTC"' failed %d", ret);
	        }
		}
#else
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
#endif
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
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
			oem_data->open_min = OEM_OF_DEF_PROPVAL_OPEN_MIN_HOLITECH;
			ret = of_property_read_u32(np, OEM_OF_PROPNAME_OPEN_MIN_HOLITECH,
				&oem_data->open_min);
			if (ret) {
				cts_warn("Parse '"OEM_OF_PROPNAME_OPEN_MIN_HOLITECH"' failed %d", ret);
			}
		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
			oem_data->open_min = OEM_OF_DEF_PROPVAL_OPEN_MIN_CTC;
			ret = of_property_read_u32(np, OEM_OF_PROPNAME_OPEN_MIN_CTC,
				&oem_data->open_min);
			if (ret) {
				cts_warn("Parse '"OEM_OF_PROPNAME_OPEN_MIN_CTC"' failed %d", ret);
			}
		}
#else
		oem_data->open_min = OEM_OF_DEF_PROPVAL_OPEN_MIN;
		ret = of_property_read_u32(np, OEM_OF_PROPNAME_OPEN_MIN,
			&oem_data->open_min);
		if (ret) {
			cts_warn("Parse '"OEM_OF_PROPNAME_OPEN_MIN"' failed %d", ret);
		}
#endif
    }

    oem_data->test_short = OEM_OF_DEF_PROPVAL_TEST_SHORT ||
        of_property_read_bool(np, OEM_OF_PROPNAME_TEST_SHORT);
    if (oem_data->test_short) {
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
			oem_data->short_min = OEM_OF_DEF_PROPVAL_SHORT_MIN_HOLITECH;
			ret = of_property_read_u32(np, OEM_OF_PROPNAME_SHORT_MIN_HOLITECH,
				&oem_data->short_min);
			if (ret) {
				cts_warn("Parse '"OEM_OF_PROPNAME_SHORT_MIN_HOLITECH"' failed %d", ret);
			}
		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
			oem_data->short_min = OEM_OF_DEF_PROPVAL_SHORT_MIN_CTC;
			ret = of_property_read_u32(np, OEM_OF_PROPNAME_SHORT_MIN_CTC,
				&oem_data->short_min);
			if (ret) {
				cts_warn("Parse '"OEM_OF_PROPNAME_SHORT_MIN_CTC"' failed %d", ret);
			}
		}
#else
		oem_data->short_min = OEM_OF_DEF_PROPVAL_SHORT_MIN;
		ret = of_property_read_u32(np, OEM_OF_PROPNAME_SHORT_MIN,
			&oem_data->short_min);
		if (ret) {
			cts_warn("Parse '"OEM_OF_PROPNAME_SHORT_MIN"' failed %d", ret);
		}
#endif
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

#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MIN_HOLITECH,
				oem_data->rawdata_min);
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MAX_HOLITECH,
				oem_data->rawdata_max);

		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MIN_CTC,
				oem_data->rawdata_min);
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MAX_CTC,
				oem_data->rawdata_max);
		}
#else
		cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MIN,
			oem_data->rawdata_min);
		cts_info(" - %-32s = %d", OEM_OF_PROPNAME_RAWDATA_MAX,
			oem_data->rawdata_max);
#endif
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
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN_HOLITECH,
				oem_data->open_min);
		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN_CTC,
				oem_data->open_min);
		}
#else
		cts_info(" - %-32s = %d", OEM_OF_PROPNAME_OPEN_MIN,
			oem_data->open_min);
#endif
    }

    cts_info(" - %-32s = %c", OEM_OF_PROPNAME_TEST_SHORT,
        oem_data->test_short ? 'Y' : 'N');
    if (oem_data->test_short) {	
#ifdef CONFIG_CTS_CHECK_TOUCH_VENDOR
		if(cts_vendor_info_val == CTS_VENDOR_HOLITECH){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN_HOLITECH,
				oem_data->short_min);
		}else if(cts_vendor_info_val == CTS_VENDOR_CTC){
			cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN_CTC,
				oem_data->short_min);
		}
#else
		cts_info(" - %-32s = %d", OEM_OF_PROPNAME_SHORT_MIN,
			oem_data->short_min);
#endif
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
            cts_err("Test rawdata failed %d",
                oem_data->rawdata_test_result);
        }
    }

    if (oem_data->test_noise) {
        struct cts_noise_test_priv_param priv_param;

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
            cts_err("Test noise failed %d",
                oem_data->noise_test_result);
        }
    }

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

static int dump_tsdata_to_csv_file(struct cts_oem_data *oem_data, const char *filepath,
	int flags, const u16 *data, int frames, int rows, int cols)
{
    struct file *file;
    int i, r, hw_cols, ret = 0;
    loff_t pos = 0;

	hw_cols = oem_data->cts_data->cts_dev.hwdata->num_col;

    cts_info("Dump tsdata to csv file: '%s' "
             "flags: 0x%x data: %p frames: %d row: %d col: %d hw_cols: %d",
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
/*
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
*/
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

static int dump_comp_cap_to_csv_file(struct cts_oem_data *oem_data, const char *filepath,
	int flags, const u8 *cap, int rows, int cols)
{
    struct file *file;
    int r, hw_cols, ret = 0;
    loff_t pos = 0;

	hw_cols = oem_data->cts_data->cts_dev.hwdata->num_col;

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

        cap += hw_cols;
    }

close_file: {
        int r = filp_close(file, NULL);
        if (r) {
            cts_err("Close file '%s' failed %d", filepath, ret);
        }
    }

    return ret;
}
/*
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
*/
static int save_selftest_data_to_file(struct cts_oem_data *oem_data)
{
    int rows, cols;
    int ret;

    cts_info("Save selftest data to file");

    rows  = oem_data->cts_data->cts_dev.fwdata.rows;
    cols  = oem_data->cts_data->cts_dev.fwdata.cols;

    if (oem_data->test_rawdata) {
        ret = dump_tsdata_to_csv_file(oem_data, OEM_RAWDATA_TEST_DATA_FILEPATH,
        O_RDWR | O_CREAT | O_TRUNC, oem_data->rawdata_test_data,
        oem_data->rawdata_test_frames, rows, cols);
        if (ret < 0) {
            cts_err("Dump rawdata test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_noise) {
        ret = dump_tsdata_to_csv_file(oem_data, OEM_NOISE_TEST_DATA_FILEPATH,
        O_RDWR | O_CREAT | O_TRUNC, oem_data->noise_test_data,
        oem_data->noise_test_frames, rows, cols);
        if (ret < 0) {
            cts_err("Dump noise test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_open) {
        ret = dump_tsdata_to_csv_file(oem_data, OEM_OPEN_TEST_DATA_FILEPATH,
            O_RDWR | O_CREAT | O_TRUNC, oem_data->open_test_data,
            1, rows, cols);
        if (ret < 0) {
            cts_err("Dump open test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_short) {
        ret = dump_tsdata_to_csv_file(oem_data, OEM_SHORT_TEST_DATA_FILEPATH,
        O_RDWR | O_CREAT | O_TRUNC, oem_data->short_test_data,
        7, rows, cols);
        if (ret < 0) {
            cts_err("Dump short test data to file failed");
            return ret;
        }
    }

    if (oem_data->test_comp_cap) {
        ret = dump_comp_cap_to_csv_file(oem_data, OEM_COMP_CAP_TEST_DATA_FILEPATH,
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

#if 0 //use seq_prinf to test info and tsdata
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
#else//use seq_prinf to print test result
static int selftest_seq_show(struct seq_file *m, void *v)
{
    struct chipone_ts_data *cts_data = (struct chipone_ts_data *)m->private;
    struct cts_oem_data *oem_data = NULL;
    int ret = 0;

    cts_info("Show seq selftest result only!!!");

    if (cts_data == NULL) {
        cts_err("Selftest seq file private data = NULL");
        return -EFAULT;
    }

    oem_data = cts_data->oem_data;

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
		seq_printf(m, "2\n");//2---PASS
	} else {
		seq_printf(m, "1\n");//1---FAIL
	}

    return 0;
}

#endif

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
	struct chipone_ts_data *cts_data;
    int ret = 0;
	char tmp_data[64] = {0};

    cts_data = (struct chipone_ts_data *)file->private_data;
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

static const struct file_operations cts_tp_work_proc_fops = {
		.owner = THIS_MODULE,
		.open  = lct_proc_open,
		.read = lct_proc_tp_work_read,
		.write = lct_proc_tp_work_write,
};
#endif

static const struct file_operations selftest_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = selftest_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

#ifdef CONFIG_CTS_GESTURE_CALLBACK
int lct_cts_tp_gesture_callback(bool flag)
{
	cts_info("set gesture mode %s", flag ? "enabled":"disabled");

	set_lct_tp_gesture_status(flag);

	if (flag){
#if defined(CONFIG_TOUCHSCREEN_COMMON)
		tpd_gesture_flag = true;
#endif
		cts_gesture_mode = 1;//enable
	}
	else{
#if defined(CONFIG_TOUCHSCREEN_COMMON)
		tpd_gesture_flag = false;
#endif
		cts_gesture_mode = 0;//disable
	}
	return 0;
}

#define WAKEUP_OFF 4
#define WAKEUP_ON 5
int cts_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	cts_info("Enter. type = %u, code = %u, value = %d\n", type, code, value);
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF)
			lct_cts_tp_gesture_callback(false);
		else if (value == WAKEUP_ON)
			lct_cts_tp_gesture_callback(true);
	}
	cts_info("Exit\n");
	
	return 0;
}

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

static int cts_tp_data_dump_proc_open(struct inode* inode, struct file* file)
{
	return single_open(file, cts_tp_data_dump_proc_show,  PDE_DATA(inode));
}

static const struct file_operations cts_tp_data_dump_proc_fops =
{
    .owner = THIS_MODULE,
    .open = cts_tp_data_dump_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

#endif

#define PROC_TP_HARDWARE_TEST_FILENAME "tp_hardware_test"
#define PROC_TP_HARDWARE_TEST_FILEPATH \
    "/proc/"PROC_TP_HARDWARE_TEST_FILENAME

static int proc_tp_hardware_test_open(struct inode *inode, struct file *file)
{
    file->private_data = PDE_DATA(inode);
    return 0;
}

static int recovery_device(struct cts_device *cts_dev)
{
    const struct cts_firmware *firmware;
    int retries, ret;

    cts_info("Recovery device");

	cts_reset_device(cts_dev);

#ifdef CONFIG_CTS_I2C_HOST
    if (cts_plat_is_i2c_online(cts_dev->pdata, CTS_DEV_NORMAL_MODE_I2CADDR)) {
        //return 0;
    }
#else
    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
        return 0;
    }
#endif

    firmware = cts_request_firmware(cts_dev, cts_dev->hwdata->hwid,
        CTS_DEV_FWID_ANY, 0);
    if (firmware == NULL) {
        cts_err("Request firmware for recovery device failed");
        return -ENOENT;
    }

    retries = 0;
    do {
        ret = cts_update_firmware(cts_dev, firmware, true);
        if (ret) {
            cts_err("Update firmware failed %d retries %d", ret,
                retries);

			cts_reset_device(cts_dev);
        } else {
            break;
        }
    } while (++retries < 3);

    cts_release_firmware(firmware);
    
    return ret;
}

static ssize_t proc_tp_hardware_test_read(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
#define TP_HARDWARE_TEST_LOOP       50

    struct chipone_ts_data *cts_data;
    struct cts_device *cts_dev;
    const char *result_str = "PASS\n";
    ktime_t start_time, test_complete_time, recovery_complete_time;
    int i, ret = 0;

    cts_data = (struct chipone_ts_data *)file->private_data;
    if (cts_data == NULL) {
        cts_err("Read '"PROC_TP_HARDWARE_TEST_FILEPATH
            "' with private_data = NULL");
        return -EIO;
    }

    if (*ppos != 0) {
        return 0;
    }

    cts_err("Read '"PROC_TP_HARDWARE_TEST_FILEPATH"'");

    cts_dev = &cts_data->cts_dev;

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        return ret;
    }

    cts_lock_device(cts_dev);

    start_time = ktime_get();

    ret = cts_enter_program_mode(cts_dev);
    if (ret) {
        cts_err("Device enter program mode failed %d", ret);
        result_str = "FAIL\n";
        goto test_complete;
    }

    for (i = 0; i < TP_HARDWARE_TEST_LOOP; i++) {
        int retry;
        u8 result[3];

        cts_dbg("Do hardware test loop %d", i);

        ret = cts_hw_reg_writeb(cts_dev, 0x3000C, 0x03);
        if (ret) {
            cts_err("Write 0x3000C to 0x03 failed %d", ret);
            result_str = "FAIL\n";
            goto test_complete;
        }

        retry = 0;
        do {
            mdelay(5);

            ret = cts_hw_reg_readsb(cts_dev, 0x3000C, result, sizeof(result));
            if (ret) {
                cts_err("Check test complete by read 0x3000C failed %d", ret);
                result_str = "FAIL\n";
                goto test_complete;
            }

            if (result[0] & BIT(2)) {
                if (result[1] || (result[2] & 0x1F)) {
                    cts_err("Test result failed: %02u %02u %02u",
                        result[0], result[1], result[2]);
                    result_str = "FAIL\n";
                    goto test_complete;
                }
                break;
            }
        }while (++retry < 20);

        if (retry >= 20) {
            cts_err("Wait test complete failed");
            result_str = "FAIL\n";
            goto test_complete;
        }
    }

test_complete:
    test_complete_time = ktime_get();
    cts_info("Hardware test complete: %.*s", 4, result_str);

    /* Recovery firmware */
    ret = recovery_device(cts_dev);
    if (ret) {
        cts_err("Recovery device failed %d", ret);
        result_str = "FAIL\n";
    } else {
        cts_start_device(cts_dev);
    }

    recovery_complete_time = ktime_get();

    cts_unlock_device(cts_dev);

    cts_info("Hardware test complete: %.*s, "
        "test ELAPSED TIME: %lldms, "
        "recovery ELAPSED TIME: %lldms, "
        "total ELAPSED TIME: %lldms",
        4, result_str,
        ktime_ms_delta(test_complete_time, start_time),
        ktime_ms_delta(recovery_complete_time, test_complete_time),
        ktime_ms_delta(recovery_complete_time, start_time));

    ret = copy_to_user(buffer, result_str, strlen(result_str));
    if (ret) {
        cts_err("Copy result to user buffer failed %d", ret);
        return 0;
    }

    *ppos += strlen(result_str);

    return strlen(result_str);
#undef TP_HARDWARE_TEST_LOOP
}


static const struct file_operations proc_tp_hardware_test_fops = {
    .owner = THIS_MODULE,
    .open  = proc_tp_hardware_test_open,
    .read = proc_tp_hardware_test_read,
};

#ifdef CONFIG_CTS_TP_GRIP_AREA
int lct_cts_tp_get_screen_angle_callback(void)
{
	u8 data;
    int ret = -EIO;
	struct chipone_ts_data *cts_data = chipone_ts_data;
	struct cts_device *cts_dev = &cts_data->cts_dev;

	if(cts_dev->rtdata.suspended || cts_dev->rtdata.updating)
	{
		cts_info("tp is suspended or updating, can not to set");
	}

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_panel_direction(cts_dev, &data);
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
	struct chipone_ts_data *cts_data = chipone_ts_data;
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
	ret = cts_dev->ops->set_panel_direction(cts_dev, val);
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

    oem_data->proc_file_tp_hardware_test_entry =
        proc_create_data(PROC_TP_HARDWARE_TEST_FILENAME,
            S_IRUGO, NULL, &proc_tp_hardware_test_fops, cts_data);
    if (oem_data->proc_file_tp_hardware_test_entry == NULL) {
        cts_err("Create '"PROC_TP_HARDWARE_TEST_FILEPATH"' failed");
        goto free_oem_data;
    }

	//[LCT_TP_SELFTEST] tp_selftest_init will be create /proc/tp_selftest before 
	//CHIPONE PROBE when POWER ON 
	remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, NULL);
	oem_data->selftest_proc_entry = NULL;
	cts_info("remove /proc/%s\n", OEM_SELFTEST_PROC_FILENAME);

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

#ifdef CONFIG_CTS_GESTURE_CALLBACK
	cts_data->pdata->ts_input_dev->event = cts_gesture_switch;
    ret = init_lct_tp_gesture(lct_cts_tp_gesture_callback);
    if (ret < 0) {
        cts_err("init_lct_tp_gesture Failed!");
        goto err_init_lct_tp_gesture_fail;
    } else {
        cts_info("init_lct_tp_gesture Succeeded!");
    }
#endif

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
		cts_err("init_lct_tp_work Failed!");
		goto err_init_lct_tp_grip_area_fail;
	} else {
		cts_info("init_lct_tp_work Succeeded!");
	}
#endif

    cts_data->oem_data = oem_data;
	oem_data->cts_data = cts_data;
    return 0;
	
#ifdef CONFIG_CTS_TP_GRIP_AREA
err_init_lct_tp_grip_area_fail:
    uninit_lct_tp_grip_area();
#endif

#ifdef CONFIG_CTS_TP_DATA_DUMP
err_create_procfs_fail:
	cts_err( "ERROR: create /proc/%s failed.", CTS_PROC_TP_DATA_DUMP);
#endif

#ifdef CONFIG_CTS_GESTURE_CALLBACK	
err_init_lct_tp_gesture_fail:
	uninit_lct_tp_gesture();
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

    if (oem_data->proc_file_tp_hardware_test_entry) {
        cts_info("  Remove '"PROC_TP_HARDWARE_TEST_FILEPATH"'");
        remove_proc_entry(PROC_TP_HARDWARE_TEST_FILENAME, NULL);
    }

    if (oem_data->selftest_proc_entry) {
        cts_info("  Remove '/proc/"OEM_SELFTEST_PROC_FILENAME"'");
        remove_proc_entry(OEM_SELFTEST_PROC_FILENAME, NULL);
    }
	
#ifdef CONFIG_CTS_TP_WORK_IRQ
    if (cts_data->procfs_tp_work) {
        cts_info("  Remove '/proc/"CTS_TP_WORK_PROC_FILE"'");
        remove_proc_entry(CTS_TP_WORK_PROC_FILE, NULL);
    }
#endif

#ifdef CONFIG_CTS_GESTURE_CALLBACK	
	uninit_lct_tp_gesture();
#endif

#ifdef CONFIG_CTS_TP_DATA_DUMP
    if (cts_data->tp_data_dump_proc) {
        cts_info("  Remove '/proc/"CTS_PROC_TP_DATA_DUMP"'");
        remove_proc_entry(CTS_PROC_TP_DATA_DUMP, NULL);
    }
#endif

#ifdef CONFIG_CTS_TP_GRIP_AREA
	uninit_lct_tp_grip_area();
#endif

    free_selftest_data_mem(oem_data);

    kfree(cts_data->oem_data);
    cts_data->oem_data = NULL;

    return 0;
}

