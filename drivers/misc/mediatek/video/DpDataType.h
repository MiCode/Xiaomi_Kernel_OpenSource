/* /============================================================================ */
/* Copy of alps/mediatek/hardware/dpframework/inc/DpDataType.h */
/* to decouple kernel code with android space code(GLP) */
/* NOTICE: */
/* MUST BE CONSISTENT WITH dpframework */
/* /======================= */
#ifndef __DP_DATA_TYPE_H__
#define __DP_DATA_TYPE_H__

#ifndef MAX
#define MAX(x, y)   ((x) >= (y)) ? (x) : (y)
#endif				/* MAX */

#ifndef MIN
#define MIN(x, y)   ((x) <= (y)) ? (x) : (y)
#endif				/* MIN */

typedef int32_t DpEngineType;

typedef enum DP_STATUS_ENUM {
	DP_STATUS_ABORTED_BY_USER = 4,
	DP_STATUS_ALL_TEST_DONE = 3,
	DP_STATUS_ALL_TPIPE_DONE = 2,
	DP_STATUS_BUFFER_DONE = 1,
	DP_STATUS_RETURN_SUCCESS = 0,
	DP_STATUS_INVALID_PARAX = -1,
	DP_STATUS_INVALID_PORT = -2,
	DP_STATUS_INVALID_PATH = -3,
	DP_STATUS_INVALID_FILE = -4,
	DP_STATUS_INVALID_CHANNEL = -5,
	DP_STATUS_INVALID_BUFFER = -6,
	DP_STATUS_INVALID_STATE = -7,
	DP_STATUS_INVALID_ENGINE = -8,
	DP_STATUS_INVALID_FORMAT = -9,
	DP_STATUS_INVALID_X_INPUT = -10,
	DP_STATUS_INVALID_Y_INPUT = -11,
	DP_STATUS_INVALID_X_OUTPUT = -12,
	DP_STATUS_INVALID_Y_OUTPUT = -13,
	DP_STATUS_INVALID_X_ALIGN = -14,
	DP_STATUS_INVALID_Y_ALIGN = -15,
	DP_STATUS_INVALID_WIDTH = -16,
	DP_STATUS_INVALID_HEIGHT = -17,
	DP_STATUS_INVALID_CROP = -18,
	DP_STATUS_INVALID_ANGLE = -19,
	DP_STATUS_INVALID_EVENT = -20,
	DP_STATUS_INVALID_OPCODE = -21,
	DP_STATUS_CAN_NOT_MERGE = -22,
	DP_STATUS_OUT_OF_MEMORY = -23,
	DP_STATUS_BUFFER_FULL = -24,
	DP_STATUS_BUFFER_EMPTY = -25,
	DP_STATUS_OPERATION_FAILED = -26,
	DP_STATUS_OVER_MAX_BRANCH = -27,
	DP_STATUS_OVER_MAX_ENGINE = -28,
	DP_STATUS_OVER_MAX_BACKUP = -29,
	DP_STATUS_SCHEDULE_ERROR = -30,
	DP_STATUS_OVER_MAX_WIDTH = -31,
	DP_STATUS_OVER_MAX_HEIGHT = -32,
	DP_STATUS_LEFT_EDGE_ERROR = -33,
	DP_STATUS_RIGHT_EDGE_ERROR = -34,
	DP_STATUS_TOP_EDGE_ERROR = -35,
	DP_STATUS_BOTTOM_EDGE_ERROR = -36,
	DP_STATUS_X_LESS_THAN_LAST = -37,
	DP_STATUS_Y_LESS_THAN_LAST = -38,
	DP_STATUS_UNWANTED_X_CAL = -39,
	DP_STATUS_LOSS_OVER_WIDTH = -40,
	DP_STATUS_LOSS_OVER_HEIGHT = -41,
	DP_STATUS_X_ALIGN_ERROR = -42,
	DP_STATUS_Y_ALIGN_ERROR = -43,
	DP_STATUS_X_OUT_OVERLAP = -44,
	DP_STATUS_Y_OUT_OVERLAP = -45,
	DP_STATUS_BACK_LE_FORWARD = -46,
	DP_STATUS_UNKNOWN_ERROR = -47,
} DP_STATUS_ENUM;


typedef enum DP_MEMORY_ENUM {
	DP_MEMORY_VA,
	DP_MEMORY_ION,
	DP_MEMORY_PHY,
	DP_MEMORY_MVA
} DP_MEMORY_ENUM;

typedef enum DP_PROFILE_ENUM {
	DP_PROFILE_BT601,
	DP_PROFILE_BT709,
	DP_PROFILE_JPEG
} DP_PROFILE_ENUM;


/* FMT GROUP , 0-RGB , 1-YUV , 2-Bayer raw , 3-compressed format */
#define DP_COLORFMT_PACK(VIDEO, PLANE, COPLANE, HFACTOR, VFACTOR, BITS, GROUP , SWAP_ENABLE, UNIQUEID)  \
    ((VIDEO         << 27) |                                                             \
     (PLANE         << 24) |                                                             \
     (COPLANE       << 22) |                                                             \
     (HFACTOR       << 20) |                                                             \
     (VFACTOR       << 18) |                                                             \
     (BITS          << 8)  |                                                             \
     (GROUP         << 6)  |                                                             \
     (SWAP_ENABLE   << 5)  |                                                             \
     (UNIQUEID      << 0))

#define DP_COLOR_GET_UFP_ENABLE(color)          ((0x20000000 & color) >> 29)
#define DP_COLOR_GET_INTERLACED_MODE(color)     ((0x10000000 & color) >> 28)
#define DP_COLOR_GET_BLOCK_MODE(color)          ((0x18000000 & color) >> 27)
#define DP_COLOR_GET_PLANE_COUNT(color)         ((0x07000000 & color) >> 24)
#define DP_COLOR_IS_UV_COPLANE(color)           ((0x00C00000 & color) >> 22)
#define DP_COLOR_GET_H_SUBSAMPLE(color)         ((0x00300000 & color) >> 20)
#define DP_COLOR_GET_V_SUBSAMPLE(color)         ((0x000C0000 & color) >> 18)
#define DP_COLOR_BITS_PER_PIXEL(color)          ((0x0003FF00 & color) >>  8)
#define DP_COLOR_GET_COLOR_GROUP(color)         ((0x000000C0 & color) >>  6)
#define DP_COLOR_GET_SWAP_ENABLE(color)         ((0x00000020 & color) >>  5)
#define DP_COLOR_GET_UNIQUE_ID(color)           ((0x0000001F & color) >>  0)
#define DP_COLOR_GET_HW_FORMAT(color)           ((0x0000001F & color) >>  0)


typedef enum DP_COLOR_ENUM {
	DP_COLOR_FULLG8 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 8, 3, 0, 20),
	DP_COLOR_FULLG10 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 10, 3, 0, 21),
	DP_COLOR_FULLG12 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 12, 3, 0, 22),
	DP_COLOR_FULLG14 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 14, 3, 0, 26),
	DP_COLOR_UFO10 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 10, 3, 0, 27),
	DP_COLOR_BAYER8 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 8, 2, 0, 20),
	DP_COLOR_BAYER10 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 10, 2, 0, 21),
	DP_COLOR_BAYER12 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 12, 2, 0, 22),
	DP_COLOR_RGB565 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 16, 0, 0, 0),
	DP_COLOR_BGR565 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 16, 0, 1, 0),
	DP_COLOR_RGB888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 24, 0, 1, 1),
	DP_COLOR_BGR888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 24, 0, 0, 1),
	DP_COLOR_RGBA8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 1, 2),
	DP_COLOR_BGRA8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 2),
	DP_COLOR_ARGB8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 1, 3),
	DP_COLOR_ABGR8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 3),
	DP_COLOR_I420 = DP_COLORFMT_PACK(0, 3, 0, 1, 1, 8, 1, 0, 8),
	DP_COLOR_YV12 = DP_COLORFMT_PACK(0, 3, 0, 1, 1, 8, 1, 1, 8),
	DP_COLOR_NV12 = DP_COLORFMT_PACK(0, 2, 1, 1, 1, 8, 1, 0, 12),
	DP_COLOR_NV21 = DP_COLORFMT_PACK(0, 2, 1, 1, 1, 8, 1, 1, 12),
	DP_COLOR_I422 = DP_COLORFMT_PACK(0, 3, 0, 1, 0, 8, 1, 0, 9),
	DP_COLOR_YV16 = DP_COLORFMT_PACK(0, 3, 0, 1, 0, 8, 1, 1, 9),
	DP_COLOR_NV16 = DP_COLORFMT_PACK(0, 2, 1, 1, 0, 8, 1, 0, 13),
	DP_COLOR_NV61 = DP_COLORFMT_PACK(0, 2, 1, 1, 0, 8, 1, 1, 13),
	DP_COLOR_YUYV = DP_COLORFMT_PACK(0, 1, 0, 1, 0, 16, 1, 0, 5),
	DP_COLOR_YVYU = DP_COLORFMT_PACK(0, 1, 0, 1, 0, 16, 1, 1, 5),
	DP_COLOR_UYVY = DP_COLORFMT_PACK(0, 1, 0, 1, 0, 16, 1, 0, 4),
	DP_COLOR_VYUY = DP_COLORFMT_PACK(0, 1, 0, 1, 0, 16, 1, 1, 4),
	DP_COLOR_I444 = DP_COLORFMT_PACK(0, 3, 0, 0, 0, 8, 1, 0, 10),
	DP_COLOR_YV24 = DP_COLORFMT_PACK(0, 3, 0, 0, 0, 8, 1, 1, 10),
	DP_COLOR_IYU2 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 24, 1, 0, 25),
	DP_COLOR_NV24 = DP_COLORFMT_PACK(0, 2, 1, 0, 0, 8, 1, 0, 14),
	DP_COLOR_NV42 = DP_COLORFMT_PACK(0, 2, 1, 0, 0, 8, 1, 1, 14),
	DP_COLOR_GREY = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 8, 1, 0, 7),


	/* Mediatek proprietary format */
	DP_COLOR_420_BLKP_UFO = DP_COLORFMT_PACK(5, 2, 1, 1, 1, 256, 1, 0, 12),	/* Frame mode + Block mode */
	DP_COLOR_420_BLKP = DP_COLORFMT_PACK(1, 2, 1, 1, 1, 256, 1, 0, 12),	/* Frame mode + Block mode */
	DP_COLOR_420_BLKI = DP_COLORFMT_PACK(3, 2, 1, 1, 1, 256, 1, 0, 12),	/* Field mode + Block mode */
	DP_COLOR_422_BLKP = DP_COLORFMT_PACK(1, 1, 0, 1, 0, 512, 1, 0, 4),	/* Frame mode */
	DP_COLOR_YUY2 = DP_COLORFMT_PACK(0, 1, 0, 1, 0, 16, 1, 0, 5),	/*  */
	DP_COLOR_PARGB8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 26),
	DP_COLOR_XARGB8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 27),
	DP_COLOR_PABGR8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 28),
	DP_COLOR_XABGR8888 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 32, 0, 0, 29),

	DP_COLOR_YUV444 = DP_COLORFMT_PACK(0, 1, 0, 0, 0, 24, 1, 0, 30),
/* DP_COLOR_YUV422I        = DP_COLORFMT_PACK(1,  0, 1, 0, 16, 1, 41),//Dup to DP_COLOR_YUYV */
/* DP_COLOR_Y800           = DP_COLORFMT_PACK(1,  0, 1, 0, 8, 1, 42),//Dup to DP_COLOR_GREY */
/* DP_COLOR_COMPACT_RAW1   = DP_COLORFMT_PACK(1,  0, 1, 0, 10, 2, 43),//Dup to Bayer10 */
/* DP_COLOR_420_3P_YVU     = DP_COLORFMT_PACK(3,  0, 1, 1,  8, 1, 44),//Dup to DP_COLOR_YV12 */
} DP_COLOR_ENUM;


/* Legacy for 6589 compatible */
typedef DP_COLOR_ENUM DpColorFormat;

#define eYUV_420_3P             DP_COLOR_I420
#define eYUV_420_2P_YUYV        DP_COLOR_YUYV
#define eYUV_420_2P_UYVY        DP_COLOR_UYVY
#define eYUV_420_2P_YVYU        DP_COLOR_YVYU
#define eYUV_420_2P_VYUY        DP_COLOR_VYUY
#define eYUV_420_2P_ISP_BLK     DP_COLOR_420_BLKP
#define eYUV_420_2P_VDO_BLK     DP_COLOR_420_BLKI
#define eYUV_422_3P             DP_COLOR_I422
#define eYUV_422_2P             DP_COLOR_NV16
#define eYUV_422_I              DP_COLOR_YUYV
#define eYUV_422_I_BLK          DP_COLOR_422_BLKP
#define eYUV_444_3P             DP_COLOR_I444
#define eYUV_444_2P             DP_COLOR_NV24
#define eYUV_444_1P             DP_COLOR_YUV444
#define eBAYER8                 DP_COLOR_BAYER8
#define eBAYER10                DP_COLOR_BAYER10
#define eBAYER12                DP_COLOR_BAYER12
#define eRGB565                 DP_COLOR_RGB565
#define eBGR565                 DP_COLOR_BGR565
#define eRGB888                 DP_COLOR_RGB888
#define eBGR888                 DP_COLOR_BGR888
#define eARGB8888               DP_COLOR_ARGB8888
#define eABGR8888               DP_COLOR_ABGR8888
#define DP_COLOR_XRGB8888       DP_COLOR_ARGB8888
#define DP_COLOR_XBGR8888       DP_COLOR_ABGR8888
#define eRGBA8888               DP_COLOR_RGBA8888
#define eBGRA8888               DP_COLOR_BGRA8888
#define eXRGB8888               DP_COLOR_XRGB8888
#define eXBGR8888               DP_COLOR_XBGR8888
#define DP_COLOR_RGBX8888       DP_COLOR_RGBA8888
#define DP_COLOR_BGRX8888       DP_COLOR_BGRA8888
#define eRGBX8888               DP_COLOR_RGBX8888
#define eBGRX8888               DP_COLOR_BGRX8888
#define ePARGB8888              DP_COLOR_PARGB8888
#define eXARGB8888              DP_COLOR_XARGB8888
#define ePABGR8888              DP_COLOR_PABGR8888
#define eXABGR8888              DP_COLOR_XABGR8888
#define eGREY                   DP_COLOR_GREY
#define eI420                   DP_COLOR_I420
#define eYV12                   DP_COLOR_YV12
#define eIYU2                   DP_COLOR_IYU2


#define eYV21                   DP_COLOR_I420
#define eNV12_BLK               DP_COLOR_420_BLKP
#define eNV12_BLK_FCM           DP_COLOR_420_BLKI
#define eYUV_420_3P_YVU         DP_COLOR_YV12

#define eNV12_BP                DP_COLOR_420_BLKP
#define eNV12_BI                DP_COLOR_420_BLKI
#define eNV12                   DP_COLOR_NV12
#define eNV21                   DP_COLOR_NV21
#define eI422                   DP_COLOR_I422
#define eYV16                   DP_COLOR_YV16
#define eNV16                   DP_COLOR_NV16
#define eNV61                   DP_COLOR_NV61
#define eUYVY                   DP_COLOR_UYVY
#define eVYUY                   DP_COLOR_VYUY
#define eYUYV                   DP_COLOR_YUYV
#define eYVYU                   DP_COLOR_YVYU
#define eUYVY_BP                DP_COLOR_422_BLKP
#define eI444                   DP_COLOR_I444
#define eNV24                   DP_COLOR_NV24
#define eNV42                   DP_COLOR_NV42
#define eYUY2                   DP_COLOR_YUY2
#define eY800                   DP_COLOR_GREY
/* #define eIYU2 */
#define eMTKYUV                 DP_COLOR_422_BLKP

#define eCompactRaw1            DP_COLOR_BAYER10


enum DpInterlaceFormat {
	eInterlace_None,
	eTop_Field,
	eBottom_Field
};

enum DpSecure {
	DP_SECURE_NONE = 0,
	DP_SECURE = 1,
	DP_SECURE_SHIFT = 8
};

#endif				/* __DP_DATA_TYPE_H__ */
