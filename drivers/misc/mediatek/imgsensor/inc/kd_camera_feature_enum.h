#ifndef _KD_CAMERA_FEATURE_ENUM_H_
#define _KD_CAMERA_FEATURE_ENUM_H_


/* Scene Mode */
FID_TO_TYPE_ENUM(
        FID_SCENE_MODE,
        FTYPE_ENUM(
            SCENE_MODE_BEGIN = 0,
            SCENE_MODE_OFF = SCENE_MODE_BEGIN,  /* Disable scene mode equal Auto mode */
            SCENE_MODE_NORMAL,  /* Normal mode */
            SCENE_MODE_ACTION,  /* Action mode */
            SCENE_MODE_PORTRAIT,    /* Portrait mode */
            SCENE_MODE_LANDSCAPE,   /* Landscape */
            SCENE_MODE_NIGHTSCENE,  /* Night Scene */
            SCENE_MODE_NIGHTPORTRAIT,   /* Night Portrait */
            SCENE_MODE_THEATRE, /* Theatre mode */
            SCENE_MODE_BEACH,   /* Beach mode */
            SCENE_MODE_SNOW,    /* Snow mode */
            SCENE_MODE_SUNSET,  /* Sunset mode */
            SCENE_MODE_STEADYPHOTO, /* Steady photo mode */
            SCENE_MODE_FIREWORKS,   /* Fireworks mode */
            SCENE_MODE_SPORTS,  /* Sports mode */
            SCENE_MODE_PARTY,   /* Party mode */
            SCENE_MODE_CANDLELIGHT, /* Candle light mode */
            /* (Unsupported legacy symbol; don't use this symbol if possible) */
            SCENE_MODE_ISO_ANTI_SHAKE,  /* ISO Anti Shake mode */
            /* (Unsupported legacy symbol; don't use this symbol if possible) */
            SCENE_MODE_BRACKET_AE,  /* Bracket AE */
            SCENE_MODE_HDR,
            SCENE_MODE_FACE_PRIORITY,
            SCENE_MODE_BARCODE,
            SCENE_MODE_NUM
        )
    )SCENE_MODE_T;


/* Effect */
FID_TO_TYPE_ENUM(
    FID_COLOR_EFFECT, 
    FTYPE_ENUM(
            MEFFECT_BEGIN = 0, 
            MEFFECT_OFF = MEFFECT_BEGIN, 
            MEFFECT_MONO, 
            MEFFECT_SEPIA, 
            MEFFECT_NEGATIVE, 
            MEFFECT_SOLARIZE, 
            MEFFECT_POSTERIZE, 
            MEFFECT_AQUA, 
            MEFFECT_BLACKBOARD, 
            MEFFFECT_BLACKBOARD = MEFFECT_BLACKBOARD,   /* legacy symbol (don't use if possible) */
            MEFFECT_WHITEBOARD, 
            MEFFECT_SEPIAGREEN, 
            MEFFECT_SEPIABLUE, 
            MEFFECT_NASHVILLE,
            MEFFECT_HEFE, 
            MEFFECT_VALENCIA, 
            MEFFECT_XPROII, 
            MEFFECT_LOFI, 
            MEFFECT_SIERRA, 
            MEFFECT_KELVIN, 
            MEFFECT_WALDEN, 
            MEFFECT_F1977,
            MEFFECT_NUM
        )
    )MCOLOR_EFFECT;


/* AWB Mode */
FID_TO_TYPE_ENUM(
    FID_AWB_MODE,
    FTYPE_ENUM(
            AWB_MODE_BEGIN = 0,
            AWB_MODE_OFF = AWB_MODE_BEGIN,
            AWB_MODE_AUTO,
            AWB_MODE_DAYLIGHT,
            AWB_MODE_CLOUDY_DAYLIGHT,
            AWB_MODE_SHADE,
            AWB_MODE_TWILIGHT,
            AWB_MODE_FLUORESCENT,
            AWB_MODE_WARM_FLUORESCENT, 
            AWB_MODE_INCANDESCENT, 
            AWB_MODE_TUNGSTEN,
            /* (Unsupported legacy symbol; don't use this symbol if possible) */
            AWB_MODE_GRAYWORLD, /* Gray world mode for CCT use */
            AWB_MODE_NUM
        )
    )AWB_MODE_T;


/* AE Flicker */
FID_TO_TYPE_ENUM(
    FID_AE_FLICKER,
    FTYPE_ENUM(
            AE_FLICKER_MODE_BEGIN = 0,
            AE_FLICKER_MODE_60HZ = AE_FLICKER_MODE_BEGIN,
            AE_FLICKER_MODE_50HZ,
            AE_FLICKER_MODE_AUTO, 
            AE_FLICKER_MODE_OFF, 
            AE_FLICKER_MODE_TOTAL_NUM
        )
    )AE_FLICKER_MODE_T;


/* AE EV */
FID_TO_TYPE_ENUM(
    FID_AE_EV, 
    FTYPE_ENUM(
            AE_EV_COMP_BEGIN = 0,
            AE_EV_COMP_00 = AE_EV_COMP_BEGIN,  /* Disable EV compenate */
            AE_EV_COMP_03,   /* EV compensate 0.3 */
            AE_EV_COMP_05,   /* EV compensate 0.5 */
            AE_EV_COMP_07,   /* EV compensate 0.7 */
            AE_EV_COMP_10,   /* EV compensate 1.0 */
            AE_EV_COMP_13,   /* EV compensate 1.3 */
            AE_EV_COMP_15,   /* EV compensate 1.5 */
            AE_EV_COMP_17,   /* EV compensate 1.7 */
            AE_EV_COMP_20,   /* EV compensate 2.0 */
            AE_EV_COMP_23,   /* EV compensate 2.3 */
            AE_EV_COMP_25,   /* EV compensate 2.5 */
            AE_EV_COMP_27,   /* EV compensate 2.7 */
            AE_EV_COMP_30,   /* EV compensate 3.0 */
            AE_EV_COMP_n03,  /* EV compensate -0.3 */
            AE_EV_COMP_n05,  /* EV compensate -0.5 */
            AE_EV_COMP_n07,  /* EV compensate -0.7 */
            AE_EV_COMP_n10,  /* EV compensate -1.0 */
            AE_EV_COMP_n13,  /* EV compensate -1.3 */
            AE_EV_COMP_n15,  /* EV compensate -1.5 */
            AE_EV_COMP_n17,  /* EV compensate -1.7 */
            AE_EV_COMP_n20,  /* EV compensate -2.0 */
            AE_EV_COMP_n23,  /* EV compensate -2.3 */
            AE_EV_COMP_n25,  /* EV compensate -2.5 */
            AE_EV_COMP_n27,  /* EV compensate -2.7 */
            AE_EV_COMP_n30,  /* EV compensate -3.0 */
            AE_EV_COMP_TOTAL_NUM
        )
    )AE_EVCOMP_T;


/* AE Mode */
FID_TO_TYPE_ENUM(
    FID_AE_SCENE_MODE,
    FTYPE_ENUM(
            AE_MODE_BEGIN = 0, 
            AE_MODE_OFF = AE_MODE_BEGIN,  /* disable AE */
            AE_MODE_AUTO,    /* auto mode   full auto ,EV ISO LCE .. is inactive */
            AE_MODE_PROGRAM, /* AE program mode , allow set EV ISO LCE .... */
            AE_MODE_TV,  /* AE TV mode */
            AE_MODE_AV,  /* AE AV mode */
            AE_MODE_SV,  /* AE SV mode */
            AE_MODE_VIDEO,   /* Video mode AE */
            AE_MODE_NIGHT,   /* Night Scene mode */
            AE_MODE_ACTION,  /* AE Action mode */
            AE_MODE_BEACH,   /* AE beach mode */
            AE_MODE_CANDLELIGHT, /* AE Candlelight mode */
            AE_MODE_FIREWORKS,   /* AE firework mode */
            AE_MODE_LANDSCAPE,   /* AE landscape mode */
            AE_MODE_PORTRAIT,    /* AE portrait mode */
            AE_MODE_NIGHT_PORTRAIT,  /* AE night portrait mode */
            AE_MODE_PARTY,   /* AE party mode */
            AE_MODE_SNOW,    /* AE snow mode */
            AE_MODE_SPORTS,  /* AE sport mode */
            AE_MODE_STEADYPHOTO, /* AE steadyphoto mode */
            AE_MODE_SUNSET,  /* AE sunset mode */
            AE_MODE_THEATRE, /* AE theatre mode */
            AE_MODE_ISO_ANTI_SHAKE,  /* AE ISO anti shake mode */
            AE_MODE_BRACKET_AE,
            AE_MODE_AUTO_PANORAMA,
            AE_MODE_HDR,
            AE_MODE_TOTAL_NUM
        )
    )AE_MODE_T;


/* AE Metering */
FID_TO_TYPE_ENUM(
    FID_AE_METERING,
    FTYPE_ENUM(
            AE_METERING_BEGIN = 0, 
            AE_METERING_MODE_CENTER_WEIGHT = AE_METERING_BEGIN, /* CENTER WEIGHTED MODE */
            AE_METERING_MODE_SOPT, /* SPOT MODE */
            AE_METERING_MODE_AVERAGE,  /* AVERAGE MODE */
            NUM_OF_AE_METER
        )
    )AE_METERING_T;


/* AE ISO */
FID_TO_TYPE_ENUM(
    FID_AE_ISO,
    FTYPE_ENUM(
            AE_ISO_BEGIN = 0,
            AE_ISO_AUTO = AE_ISO_BEGIN,
            AE_ISO_100,
            AE_ISO_200, 
            AE_ISO_400, 
            AE_ISO_800, 
            AE_ISO_1600, 
            NUM_OF_AE_ISO
        )
    )AE_ISO_T;


/* AE Strobe / Flashlight */
FID_TO_TYPE_ENUM(
    FID_AE_STROBE,
    FTYPE_ENUM(
            FLASHLIGHT_BEGIN = 0,
            FLASHLIGHT_AUTO = FLASHLIGHT_BEGIN,
            FLASHLIGHT_FORCE_ON,
            FLASHLIGHT_FORCE_OFF,
            FLASHLIGHT_REDEYE,
            FLASHLIGHT_TORCH,
            NUM_OF_FLASHLIGHT
        )
    )AE_STROBE_T;

/* AF Mode */
FID_TO_TYPE_ENUM(
    FID_AF_MODE,
    FTYPE_ENUM(
            AF_MODE_BEGIN = 0,
            AF_MODE_AFS = AF_MODE_BEGIN,    /* (AF-Single Shot Mode) */
            AF_MODE_AFC,   /* AF-Continuous Mode */
            AF_MODE_AFC_VIDEO, /* AF-Continuous Mode (Video) */
            AF_MODE_MACRO, /* AF Macro Mode */
            AF_MODE_INFINITY,  /* Focus is set at infinity. */
            AF_MODE_MF,    /* Manual Focus Mode */
            AF_MODE_FULLSCAN,  /* AF Full Scan Mode */
            NUM_OF_AF_MODE
        )
    )AF_MODE_T;


/* AF Metering */
FID_TO_TYPE_ENUM(
    FID_AF_METERING,
    FTYPE_ENUM(
            AF_METERING_BEGIN = 0, 
            AF_METER_SPOT = AF_METERING_BEGIN,  /* Spot Window */
            AF_METER_MATRIX,   /* Matrix Window */
            AF_METER_MOVESPOT, 
            NUM_OF_AF_METER
        )
    )_AF_METERING_T;

/* ISP Edge */
FID_TO_TYPE_ENUM(
    FID_ISP_EDGE,
    FTYPE_ENUM(
            ISP_EDGE_BEGIN = 0,
            ISP_EDGE_LOW = ISP_EDGE_BEGIN,
            ISP_EDGE_MIDDLE, 
            ISP_EDGE_HIGH, 
            NUM_OF_ISP_EDGE
        )
    )ISP_EDGE_T;


/* ISP Hue */
FID_TO_TYPE_ENUM(
    FID_ISP_HUE,
    FTYPE_ENUM(
            ISP_HUE_BEGIN = 0,
            ISP_HUE_LOW = ISP_HUE_BEGIN,
            ISP_HUE_MIDDLE, 
            ISP_HUE_HIGH, 
            NUM_OF_ISP_HUE
        )
    )ISP_HUE_T;


/* ISP Saturation */
FID_TO_TYPE_ENUM(
    FID_ISP_SAT,
    FTYPE_ENUM(
            ISP_SAT_BEGIN = 0,
            ISP_SAT_LOW = ISP_SAT_BEGIN,
            ISP_SAT_MIDDLE, 
            ISP_SAT_HIGH, 
            NUM_OF_ISP_SAT
        )
    )ISP_SAT_T;


/* ISP Brightness */
FID_TO_TYPE_ENUM(
    FID_ISP_BRIGHT,
    FTYPE_ENUM(
            ISP_BRIGHT_BEGIN = 0,
            ISP_BRIGHT_LOW = ISP_BRIGHT_BEGIN,
            ISP_BRIGHT_MIDDLE, 
            ISP_BRIGHT_HIGH, 
            NUM_OF_ISP_BRIGHT
        )
    )ISP_BRIGHT_T;


/* ISP Contrast */
FID_TO_TYPE_ENUM(
    FID_ISP_CONTRAST,
    FTYPE_ENUM(
            ISP_CONTRAST_BEGIN = 0,
            ISP_CONTRAST_LOW = ISP_CONTRAST_BEGIN,
            ISP_CONTRAST_MIDDLE, 
            ISP_CONTRAST_HIGH, 
            NUM_OF_ISP_CONTRAST
        )
    )ISP_CONTRAST_T;

/* Capture Mode */
FID_TO_TYPE_ENUM(
    FID_CAPTURE_MODE, 
    FTYPE_ENUM(
            CAPTURE_MODE_BEGIN = 0, 
            CAPTURE_MODE_NORMAL = CAPTURE_MODE_BEGIN, 
            CAPTURE_MODE_BURST_SHOT, 
            CAPTURE_MODE_SMILE_SHOT, 
            CAPTURE_MODE_PANORAMA, 
            CAPTURE_MODE_AUTORAMA, 
            CAPTURE_MODE_BEST_SHOT, 
            CAPTURE_MODE_EV_BRACKET, 
            CAPTURE_MODE_MAV, 
            CAPTURE_MODE_HDR, 
            CAPTURE_MODE_ASD, 
            CAPTURE_MODE_ZSD,    /* Zero-shutter-delay Shot */
            CAPTURE_MODE_PANO_3D,
            CAPTURE_MODE_SINGLE_3D,
            CAPTURE_MODE_FACE_BEAUTY,
            CAPTURE_MODE_CONTINUOUS_SHOT,
            CAPTURE_MODE_MULTI_MOTION,
            NUM_OF_CAPTURE_MODE
        )
    )CAPTURE_MODE_T;


/* Capture Size */
FID_TO_TYPE_ENUM(
    FID_CAP_SIZE,
    FTYPE_ENUM(
            CAPTURE_SIZE_BEGIN = 0,
            CAPTURE_SIZE_320_240 = CAPTURE_SIZE_BEGIN,
            CAPTURE_SIZE_640_480,
            CAPTURE_SIZE_1024_768,
            CAPTURE_SIZE_1280_960,
            CAPTURE_SIZE_1600_1200,
            CAPTURE_SIZE_2048_1536,
            CAPTURE_SIZE_2560_1920,
            CAPTURE_SIZE_3264_2448, 
            CAPTURE_SIZE_4000_3000, 
            NUM_OF_CAPTURE_SIZE
        )
    )CAPTURE_SIZE_T;


/* Preview Size */
FID_TO_TYPE_ENUM(
    FID_PREVIEW_SIZE,
    FTYPE_ENUM(
            PREVIEW_SIZE_BEGIN = 0,
            PREVIEW_SIZE_160_120 = PREVIEW_SIZE_BEGIN,
            PREVIEW_SIZE_176_144,
            PREVIEW_SIZE_320_240,
            PREVIEW_SIZE_352_288,
            PREVIEW_SIZE_480_320,
            PREVIEW_SIZE_480_368,
            PREVIEW_SIZE_640_480,
            PREVIEW_SIZE_720_480,
            PREVIEW_SIZE_800_480,
            PREVIEW_SIZE_864_480,
            PREVIEW_SIZE_800_600,
            PREVIEW_SIZE_1280_720, 
            PREVIEW_SIZE_1920_1080, 
            NUM_OF_PREVIEW_SIZE
        )
    )PREVIEW_SIZE_T;

/* Video Preview Size */
FID_TO_TYPE_ENUM(
    FID_VIDEO_PREVIEW_SIZE,
    FTYPE_ENUM(
            VIDEO_PREVIEW_SIZE_BEGIN = 0,
            VIDEO_PREVIEW_SIZE_640_480 = VIDEO_PREVIEW_SIZE_BEGIN,
            VIDEO_PREVIEW_SIZE_800_600, 
            NUM_OF_VIDEO_PREVIEW_SIZE
        )
    )VIDEO_PREVIEW_SIZE_T;


/* Frame Rate */
FID_TO_TYPE_ENUM(
    FID_FRAME_RATE, 
    FTYPE_ENUM(
            FRAME_RATE_BEGIN = 0, 
            FRAME_RATE_000FPS = FRAME_RATE_BEGIN, /* 0.0fps */
            FRAME_RATE_010FPS, 
            FRAME_RATE_020FPS, 
            FRAME_RATE_030FPS, 
            FRAME_RATE_040FPS, 
            FRAME_RATE_050FPS, 
            FRAME_RATE_060FPS, 
            FRAME_RATE_070FPS, 
            FRAME_RATE_075FPS = FRAME_RATE_070FPS, /* legacy definition. */
            FRAME_RATE_080FPS, 
            FRAME_RATE_090FPS, 
            FRAME_RATE_100FPS,    /* 10  fps */
            FRAME_RATE_110FPS,
            FRAME_RATE_120FPS, 
            FRAME_RATE_125FPS = FRAME_RATE_120FPS,    /* legacy definition. */
            FRAME_RATE_130FPS, 
            FRAME_RATE_140FPS, 
            FRAME_RATE_150FPS, 
            FRAME_RATE_160FPS,
            FRAME_RATE_170FPS,
            FRAME_RATE_180FPS, 
            FRAME_RATE_190FPS, 
            FRAME_RATE_200FPS, /* 20  fps */
            FRAME_RATE_210FPS, 
            FRAME_RATE_220FPS, 
            FRAME_RATE_230FPS, 
            FRAME_RATE_240FPS, 
            FRAME_RATE_250FPS, 
            FRAME_RATE_260FPS, 
            FRAME_RATE_270FPS, 
            FRAME_RATE_280FPS, 
            FRAME_RATE_290FPS,
            FRAME_RATE_300FPS,   /* 30  fps */
            NUM_OF_FRAME_RATE
        )
    )FRAME_RATE_T;

/* Frame Rate Range */
FID_TO_TYPE_ENUM(
    FID_FRAME_RATE_RANGE, 
    FTYPE_ENUM(
            FRAME_RATE_RANGE_BEGIN = 0, 
            FRAME_RATE_RANGE_5_30_FPS = FRAME_RATE_RANGE_BEGIN,   /* 5-30fps */
            NUM_OF_FRAME_RATE_RANGE
        )
    )FRAME_RATE_RANGE_T;


/* Focus Distance Normal */
FID_TO_TYPE_ENUM(
    FID_FOCUS_DIST_NORMAL, 
    FTYPE_ENUM(
            FOCUS_DIST_N_BEGIN = 0, 
            FOCUS_DIST_N_INFINITE = FOCUS_DIST_N_BEGIN,  /* Infinite */
            FOCUS_DIST_N_10CM, 
            NUM_OF_FOCUS_DIST_N
        )
    )FOCUS_DIST_NORMAL_T;


/* Focus Distance Macro */
FID_TO_TYPE_ENUM(
    FID_FOCUS_DIST_MACRO, 
    FTYPE_ENUM(
            FOCUS_DIST_M_BEGIN = 0, 
            FOCUS_DIST_M_INFINITE = FOCUS_DIST_M_BEGIN,   /* Infinite */
            FOCUS_DIST_M_5CM,
            FOCUS_DIST_M_10CM, 
            NUM_OF_FOCUS_DIST_M
        )
    )FOCUS_DIST_MACRO_T;


/* Face Detection */
FID_TO_TYPE_ENUM(
    FID_FD_ON_OFF, 
    FTYPE_ENUM(
            FD_BEGIN = 0, 
            FD_OFF = FD_BEGIN, 
            FD_ON,
            NUM_OF_FD
        )
    )FD_MODE_T;


/* EIS */
FID_TO_TYPE_ENUM(
    FID_EIS, 
    FTYPE_ENUM(
            EIS_BEGIN = 0, 
            EIS_OFF = EIS_BEGIN, 
            EIS_ON, 
            NUM_OF_EIS
        )
    )EIS_T;

/* ZSD */
FID_TO_TYPE_ENUM(
    FID_ZSD, 
    FTYPE_ENUM(
            ZSD_BEGIN = 0, 
            ZSD_OFF = ZSD_BEGIN, 
            ZSD_ON, 
            NUM_OF_ZSD
        )
    )ZSD_T;

/* AF LAMP */
FID_TO_TYPE_ENUM(
    FID_AF_LAMP,
    FTYPE_ENUM(
            AF_LAMP_BEGIN = 0,
            AF_LAMP_OFF = AF_LAMP_BEGIN,
            AF_LAMP_ON, 
            AF_LAMP_AUTO, 
            AF_LAMP_FLASH, 
            NUM_OF_AF_LAMP
        )
    )AF_LAMP_T;

/* Stereo 3D Capture Size */
FID_TO_TYPE_ENUM(
    FID_STEREO_3D_CAP_SIZE,
    FTYPE_ENUM(
            STEREO_3D_CAPTURE_SIZE_BEGIN = 0,
            STEREO_3D_CAPTURE_SIZE_2560_720 = STEREO_3D_CAPTURE_SIZE_BEGIN,
            STEREO_3D_CAPTURE_SIZE_3840_1080, 
            NUM_OF_STEREO_3D_CAPTURE_SIZE
        )
    )STEREO_3D_CAPTURE_SIZE_T;

/* Stereo 3D Preview Size */
FID_TO_TYPE_ENUM(
    FID_STEREO_3D_PREVIEW_SIZE,
    FTYPE_ENUM(
            STEREO_3D_PREVIEW_SIZE_BEGIN = 0,
            STEREO_3D_PREVIEW_SIZE_640_360 = STEREO_3D_PREVIEW_SIZE_BEGIN,
            STEREO_3D_PREVIEW_SIZE_854_480,
            STEREO_3D_PREVIEW_SIZE_960_540,
            STEREO_3D_PREVIEW_SIZE_1280_720, 
            NUM_OF_STEREO_3D_PREVIEW_SIZE
        )
    )STEREO_3D_PREVIEW_SIZE_T;

/* Stereo 3D Type */
FID_TO_TYPE_ENUM(
    FID_STEREO_3D_TYPE,
    FTYPE_ENUM(
            STEREO_3D_TYPE_BEGIN = 0,
            STEREO_3D_NOT_SUPPORT = STEREO_3D_TYPE_BEGIN,
            STEREO_3D_FRAME_SEQ,
            STEREO_3D_SIDE_BY_SIDE, 
            STEREO_3D_TOP_BOTTOM,
            NUM_OF_STEREO_3D_TYPE
        )
    )STEREO_3D_TYPE_T;

/* Stereo 3D Mode */
FID_TO_TYPE_ENUM(
    FID_STEREO_3D_MODE,
    FTYPE_ENUM(
            STEREO_3D_MODE_BEGIN = 0,
            STEREO_3D_OFF = STEREO_3D_MODE_BEGIN,
            STEREO_3D_ON, 
            NUM_OF_STEREO_3D_MODE
        )
    )STEREO_3D_MODE_T;

/* Stereo 3D Image Format */
FID_TO_TYPE_ENUM(
    FID_STEREO_3D_IMAGE_FORMAT,
    FTYPE_ENUM(
            STEREO_3D_IMAGE_FORMAT_BEGIN = 0,
            STEREO_3D_JPS = STEREO_3D_IMAGE_FORMAT_BEGIN,
            STEREO_3D_MPO, 
            NUM_OF_STEREO_3D_IMAGE_FORMAT
        )
    )STEREO_3D_IMAGE_FORMAT_T;

/* YUV Auto test */
FID_TO_TYPE_ENUM(
    FID_YUV_AUTOTEST,
    FTYPE_ENUM(
            YUV_AUTOTEST_BEGIN = 0,
            YUV_AUTOTEST_SET_SHADDING = YUV_AUTOTEST_BEGIN,
            YUV_AUTOTEST_SET_GAMMA,
            YUV_AUTOTEST_SET_AE,
            YUV_AUTOTEST_SET_SHUTTER,
            YUV_AUTOTEST_SET_GAIN,
            YUV_AUTOTEST_GET_SHUTTER_RANGE,
            YUV_AUTOTEST_GET_SHADDING,
            YUV_AUTOTEST_GET_GAMMA,
            YUV_AUTOTEST_GET_AE,
            YUV_AUTOTEST_GET_SHUTTER, 
            YUV_AUTOTEST_GET_GAIN, 
            NUM_OF_YUV_AUTOTEST
        )
    )YUV_AUTOTEST_T;

#endif              /* _KD_CAMERA_FEATURE_ENUM_H_ */
