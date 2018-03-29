/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _KD_IMGSENSOR_H
#define _KD_IMGSENSOR_H

#include <linux/ioctl.h>
/* #define CONFIG_COMPAT */
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#ifndef ASSERT
#define ASSERT(expr)        BUG_ON(!(expr))
#endif

#define IMGSENSORMAGIC 'i'
/* IOCTRL(inode * ,file * ,cmd ,arg ) */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */

/*******************************************************************************
*
********************************************************************************/
#define YUV_INFO(_id, name, getCalData)\
	{ \
		_id, name, \
NSFeature :  : YUVSensorInfo < _id >  :  : createInstance(name, #name), \
		(NSFeature :  : SensorInfoBase*(*)()) \
NSFeature :  : YUVSensorInfo < _id >  :  : getInstance, \
NSFeature :  : YUVSensorInfo < _id >  :  : getDefaultData, \
		getCalData, \
NSFeature :  : YUVSensorInfo < _id >  :  : getNullFlickerPara \
	}
#define RAW_INFO(_id, name, getCalData)\
	{ \
		_id, name, \
NSFeature :  : RAWSensorInfo < _id >  :  : createInstance(name, #name), \
		(NSFeature :  : SensorInfoBase*(*)()) \
NSFeature :  : RAWSensorInfo < _id >  :  : getInstance, \
NSFeature :  : RAWSensorInfo < _id >  :  : getDefaultData, \
		getCalData, \
NSFeature :  : RAWSensorInfo < _id >  :  : getFlickerPara \
	}
/*******************************************************************************
*
********************************************************************************/

/* sensorOpen */
#define KDIMGSENSORIOC_T_OPEN                       _IO(IMGSENSORMAGIC, 0)
/* sensorGetInfo */
#define KDIMGSENSORIOC_X_GETINFO                    _IOWR(IMGSENSORMAGIC, 5, ACDK_SENSOR_GETINFO_STRUCT)
/* sensorGetResolution */
#define KDIMGSENSORIOC_X_GETRESOLUTION              _IOWR(IMGSENSORMAGIC, 10, ACDK_SENSOR_RESOLUTION_INFO_STRUCT)
/* For kernel 64-bit */
#define KDIMGSENSORIOC_X_GETRESOLUTION2             _IOWR(IMGSENSORMAGIC, 10, ACDK_SENSOR_PRESOLUTION_STRUCT)
/* sensorFeatureControl */
#define KDIMGSENSORIOC_X_FEATURECONCTROL            _IOWR(IMGSENSORMAGIC, 15, ACDK_SENSOR_FEATURECONTROL_STRUCT)
/* sensorControl */
#define KDIMGSENSORIOC_X_CONTROL                    _IOWR(IMGSENSORMAGIC, 20, ACDK_SENSOR_CONTROL_STRUCT)
/* sensorClose */
#define KDIMGSENSORIOC_T_CLOSE                      _IO(IMGSENSORMAGIC, 25)
/* sensorSearch */
#define KDIMGSENSORIOC_T_CHECK_IS_ALIVE             _IO(IMGSENSORMAGIC, 30)
/* set sensor driver */
#define KDIMGSENSORIOC_X_SET_DRIVER                 _IOWR(IMGSENSORMAGIC, 35, SENSOR_DRIVER_INDEX_STRUCT)
/* get socket postion */
#define KDIMGSENSORIOC_X_GET_SOCKET_POS             _IOWR(IMGSENSORMAGIC, 40, u32)
/* set I2C bus */
#define KDIMGSENSORIOC_X_SET_I2CBUS                 _IOWR(IMGSENSORMAGIC, 45, u32)
/* set I2C bus */
#define KDIMGSENSORIOC_X_RELEASE_I2C_TRIGGER_LOCK   _IO(IMGSENSORMAGIC, 50)
/* Set Shutter Gain Wait Done */
#define KDIMGSENSORIOC_X_SET_SHUTTER_GAIN_WAIT_DONE _IOWR(IMGSENSORMAGIC, 55, u32)
/* set mclk */
#define KDIMGSENSORIOC_X_SET_MCLK_PLL               _IOWR(IMGSENSORMAGIC, 60, ACDK_SENSOR_MCLK_STRUCT)
#define KDIMGSENSORIOC_X_GETINFO2                   _IOWR(IMGSENSORMAGIC, 65, IMAGESENSOR_GETINFO_STRUCT)
/* set open/close sensor index */
#define KDIMGSENSORIOC_X_SET_CURRENT_SENSOR         _IOWR(IMGSENSORMAGIC, 70, u32)
/* set GPIO */
#define KDIMGSENSORIOC_X_SET_GPIO                   _IOWR(IMGSENSORMAGIC, 75, IMGSENSOR_GPIO_STRUCT)
/* Get ISP CLK */
#define KDIMGSENSORIOC_X_GET_ISP_CLK                _IOWR(IMGSENSORMAGIC, 80, u32)
/* Get CSI CLK */
#define KDIMGSENSORIOC_X_GET_CSI_CLK                _IOWR(IMGSENSORMAGIC, 85, u32)

#ifdef CONFIG_COMPAT
#define COMPAT_KDIMGSENSORIOC_X_GETINFO            _IOWR(IMGSENSORMAGIC, 5, COMPAT_ACDK_SENSOR_GETINFO_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL    _IOWR(IMGSENSORMAGIC, 15, COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_CONTROL            _IOWR(IMGSENSORMAGIC, 20, COMPAT_ACDK_SENSOR_CONTROL_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_GETINFO2           _IOWR(IMGSENSORMAGIC, 65, COMPAT_IMAGESENSOR_GETINFO_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_GETRESOLUTION2     _IOWR(IMGSENSORMAGIC, 10, COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT)
#endif

/*******************************************************************************
*
********************************************************************************/
/* SENSOR CHIP VERSION */
/*IMX*/
#define IMX362_SENSOR_ID                        0x0362
#define IMX338_SENSOR_ID                        0x0338
#define IMX318_SENSOR_ID                        0x0318
#define IMX377_SENSOR_ID                        0x0377
#define IMX278_SENSOR_ID                        0x0278
#define IMX258_SENSOR_ID                        0x0258
#define IMX258_MONO_SENSOR_ID                   0x0259
#define IMX230_SENSOR_ID                        0x0230
#define IMX220_SENSOR_ID                        0x0220
#define IMX219_SENSOR_ID                        0x0219
#define IMX214_SENSOR_ID                        0x0214
#define IMX214_MONO_SENSOR_ID                   0x0215
#define IMX179_SENSOR_ID                        0x0179
#define IMX178_SENSOR_ID                        0x0178
#define IMX135_SENSOR_ID                        0x0135
#define IMX132MIPI_SENSOR_ID                    0x0132
#define IMX119_SENSOR_ID                        0x0119
#define IMX105_SENSOR_ID                        0x0105
#define IMX091_SENSOR_ID                        0x0091
#define IMX073_SENSOR_ID                        0x0046
#define IMX058_SENSOR_ID                        0x0058
/*OV*/
#define OV23850_SENSOR_ID                       0x023850
#define OV16880_SENSOR_ID                       0x016880
#define OV16825MIPI_SENSOR_ID                   0x016820
#define OV13850_SENSOR_ID                       0xD850
#define OV12830_SENSOR_ID                       0xC830
#define OV9760MIPI_SENSOR_ID                    0x9760
#define OV9740MIPI_SENSOR_ID                    0x9740
#define OV9726_SENSOR_ID                        0x9726
#define OV9726MIPI_SENSOR_ID                    0x9726
#define OV8865_SENSOR_ID                        0x8865
#define OV8858_SENSOR_ID                        0x8858
#define OV8858S_SENSOR_ID                      (0x8858+1)
#define OV8830_SENSOR_ID                        0x8830
#define OV8825_SENSOR_ID                        0x8825
#define OV7675_SENSOR_ID                        0x7673
#define OV5693_SENSOR_ID                        0x5690
#define OV5670MIPI_SENSOR_ID                    0x5670
#define OV5670MIPI_SENSOR_ID_2                  (0x5670+010000)
#define OV5671MIPI_SENSOR_ID                    0x5671
#define OV5650_SENSOR_ID                        0x5651
#define OV5650MIPI_SENSOR_ID                    0x5651
#define OV5648MIPI_SENSOR_ID                    0x5648
#define OV5647_SENSOR_ID                        0x5647
#define OV5647MIPI_SENSOR_ID                    0x5647
#define OV5645MIPI_SENSOR_ID                    0x5645
#define OV5642_SENSOR_ID                        0x5642
#define OV4688MIPI_SENSOR_ID                    0x4688
#define OV3640_SENSOR_ID                        0x364C
#define OV2724MIPI_SENSOR_ID                    0x2724
#define OV2722MIPI_SENSOR_ID                    0x2722
#define OV2680MIPI_SENSOR_ID                    0x2680
#define OV2680_SENSOR_ID                        0x2680
#define OV2659_SENSOR_ID                        0x2656
#define OV2655_SENSOR_ID                        0x2656
#define OV2650_SENSOR_ID                        0x2652
#define OV2650_SENSOR_ID_1                      0x2651
#define OV2650_SENSOR_ID_2                      0x2652
#define OV2650_SENSOR_ID_3                      0x2655
/*S5K*/
#define S5K2L7_SENSOR_ID                        0x20C7
#define S5K3L8_SENSOR_ID                        0x30C8
#define S5K2X8_SENSOR_ID                        0x2188
#define S5K2P8_SENSOR_ID                        0x2108
#define S5K3P3SX_SENSOR_ID                      0x3103
#define S5K3M2_SENSOR_ID                        0x30D2
#define S5K3L8_SENSOR_ID                        0x30C8
#define S5K3AAEA_SENSOR_ID                      0x07AC
#define S5K3BAFB_SENSOR_ID                      0x7070
#define S5K3H7Y_SENSOR_ID                       0x3087
#define S5K3H2YX_SENSOR_ID                      0x382b
#define S5KA3DFX_SENSOR_ID                      0x00AB
#define S5K3E2FX_SENSOR_ID                      0x3E2F
#define S5K4B2FX_SENSOR_ID                      0x5080
#define S5K4E1GA_SENSOR_ID                      0x4E10
#define S5K4ECGX_SENSOR_ID                      0x4EC0
#define S5K53BEX_SENSOR_ID                      0x45A8
#define S5K53BEB_SENSOR_ID                      0x87A8
#define S5K5BAFX_SENSOR_ID                      0x05BA
#define S5K5E8YX_SENSOR_ID                      0x5e80
#define S5K5E2YA_SENSOR_ID                      0x5e20
#define S5K4H5YX_2LANE_SENSOR_ID                0x485B
#define S5K4H5YC_SENSOR_ID                      0x485B
#define S5K83AFX_SENSOR_ID                      0x01C4
#define S5K5CAGX_SENSOR_ID                      0x05ca
#define S5K8AAYX_MIPI_SENSOR_ID                 0x08aa
#define S5K8AAYX_SENSOR_ID                      0x08aa
#define S5K5E8YX_SENSOR_ID                      0x5e80
/*HI*/
#define HI841_SENSOR_ID                         0x0841
#define HI707_SENSOR_ID                         0x00b8
#define HI704_SENSOR_ID                         0x0096
#define HI551_SENSOR_ID                         0x0551
#define HI553_SENSOR_ID                         0x0553
#define HI545MIPI_SENSOR_ID                     0x0545
#define HI544MIPI_SENSOR_ID                     0x0544
#define HI542_SENSOR_ID                         0x00B1
#define HI542MIPI_SENSOR_ID                     0x00B1
#define HI253_SENSOR_ID                         0x0092
#define HI251_SENSOR_ID                         0x0084
#define HI191MIPI_SENSOR_ID                     0x0191
#define HIVICF_SENSOR_ID                        0x0081
/*MT*/
#define MT9D011_SENSOR_ID                       0x1511
#define MT9D111_SENSOR_ID                       0x1511
#define MT9D112_SENSOR_ID                       0x1580
#define MT9M011_SENSOR_ID                       0x1433
#define MT9M111_SENSOR_ID                       0x143A
#define MT9M112_SENSOR_ID                       0x148C
#define MT9M113_SENSOR_ID                       0x2480
#define MT9P012_SENSOR_ID                       0x2800
#define MT9P012_SENSOR_ID_REV7                  0x2801
#define MT9T012_SENSOR_ID                       0x1600
#define MT9T013_SENSOR_ID                       0x2600
#define MT9T113_SENSOR_ID                       0x4680
#define MT9V112_SENSOR_ID                       0x1229
#define MT9DX11_SENSOR_ID                       0x1519
#define MT9D113_SENSOR_ID                       0x2580
#define MT9D115_SENSOR_ID                       0x2580
#define MT9D115MIPI_SENSOR_ID                   0x2580
#define MT9V113_SENSOR_ID                       0x2280
#define MT9V114_SENSOR_ID                       0x2283
#define MT9V115_SENSOR_ID                       0x2284
#define MT9P015_SENSOR_ID                       0x2803
#define MT9P017_SENSOR_ID                       0x4800
#define MT9P017MIPI_SENSOR_ID                   0x4800
#define MT9T113MIPI_SENSOR_ID                   0x4680
/*GC*/
#define GC2355_SENSOR_ID                        0x2355
#define GC2235_SENSOR_ID                        0x2235
#define GC2035_SENSOR_ID                        0x2035
#define GC2145_SENSOR_ID                        0x2145
#define GC0330_SENSOR_ID                        0xC1
#define GC0329_SENSOR_ID                        0xC0
#define GC0310_SENSOR_ID                        0xa310
#define GC0313MIPI_YUV_SENSOR_ID                0xD0
#define GC0312_SENSOR_ID                        0xb310
/*SP*/
#define SP0A19_YUV_SENSOR_ID                    0xA6
#define SP2518_YUV_SENSOR_ID                    0x53
/*A*/
#define A5141MIPI_SENSOR_ID                     0x4800
#define A5142MIPI_SENSOR_ID                     0x4800
/*HM*/
#define HM3451_SENSOR_ID                        0x345
#define HM2051MIPI_SENSOR_ID                    0x2051
/*AR*/
#define AR0833_SENSOR_ID                        0x4B03
/*SIV*/
#define SID020A_SENSOR_ID                       0x12B4
#define SIV100B_SENSOR_ID                       0x0C11
#define SIV100A_SENSOR_ID                       0x0C10
#define SIV120A_SENSOR_ID                       0x1210
#define SIV120B_SENSOR_ID                       0x0012
#define SIV121D_SENSOR_ID                       0xDE
#define SIM101B_SENSOR_ID                       0x09A0
#define SIM120C_SENSOR_ID                       0x0012
#define SID130B_SENSOR_ID                       0x001b
#define SIC110A_SENSOR_ID                       0x000D
#define SIV120B_SENSOR_ID                       0x0012
/*PAS (PixArt Image)*/
#define PAS105_SENSOR_ID                        0x0065
#define PAS302_SENSOR_ID                        0x0064
#define PAS5101_SENSOR_ID                       0x0067
#define PAS6180_SENSOR_ID                       0x6179
/*Panasoic*/
#define MN34152_SENSOR_ID                       0x01
/*Toshiba*/
#define T4KA7_SENSOR_ID                         0x2c30
/*Others*/
#define SHARP3D_SENSOR_ID                       0x003d
#define T8EV5_SENSOR_ID                         0x1011

/* CAMERA DRIVER NAME */
#define CAMERA_HW_DEVNAME                       "kd_camera_hw"
/* SENSOR DEVICE DRIVER NAME */
/*IMX*/
#define SENSOR_DRVNAME_IMX362_MIPI_RAW          "imx362mipiraw"
#define SENSOR_DRVNAME_IMX338_MIPI_RAW          "imx338mipiraw"
#define SENSOR_DRVNAME_IMX318_MIPI_RAW          "imx318mipiraw"
#define SENSOR_DRVNAME_IMX377_MIPI_RAW          "imx377mipiraw"
#define SENSOR_DRVNAME_IMX278_MIPI_RAW          "imx278mipiraw"
#define SENSOR_DRVNAME_IMX258_MIPI_RAW          "imx258mipiraw"
#define SENSOR_DRVNAME_IMX258_MIPI_MONO         "imx258mipimono"
#define SENSOR_DRVNAME_IMX230_MIPI_RAW          "imx230mipiraw"
#define SENSOR_DRVNAME_IMX220_MIPI_RAW          "imx220mipiraw"
#define SENSOR_DRVNAME_IMX219_MIPI_RAW          "imx219mipiraw"
#define SENSOR_DRVNAME_IMX214_MIPI_MONO         "imx214mipimono"
#define SENSOR_DRVNAME_IMX214_MIPI_RAW          "imx214mipiraw"
#define SENSOR_DRVNAME_IMX179_MIPI_RAW          "imx179mipiraw"
#define SENSOR_DRVNAME_IMX178_MIPI_RAW          "imx178mipiraw"
#define SENSOR_DRVNAME_IMX135_MIPI_RAW          "imx135mipiraw"
#define SENSOR_DRVNAME_IMX132_MIPI_RAW          "imx132mipiraw"
#define SENSOR_DRVNAME_IMX119_MIPI_RAW          "imx119mipiraw"
#define SENSOR_DRVNAME_IMX105_MIPI_RAW          "imx105mipiraw"
#define SENSOR_DRVNAME_IMX091_MIPI_RAW          "imx091mipiraw"
#define SENSOR_DRVNAME_IMX073_MIPI_RAW          "imx073mipiraw"
/*OV*/
#define SENSOR_DRVNAME_OV23850_MIPI_RAW         "ov23850mipiraw"
#define SENSOR_DRVNAME_OV16880_MIPI_RAW         "ov16880mipiraw"
#define SENSOR_DRVNAME_OV16825_MIPI_RAW         "ov16825mipiraw"
#define SENSOR_DRVNAME_OV13850_MIPI_RAW         "ov13850mipiraw"
#define SENSOR_DRVNAME_OV12830_MIPI_RAW         "ov12830mipiraw"
#define SENSOR_DRVNAME_OV9760_MIPI_RAW          "ov9760mipiraw"
#define SENSOR_DRVNAME_OV9740_MIPI_YUV          "ov9740mipiyuv"
#define SENSOR_DRVNAME_0V9726_RAW               "ov9726raw"
#define SENSOR_DRVNAME_OV9726_MIPI_RAW          "ov9726mipiraw"
#define SENSOR_DRVNAME_OV8865_MIPI_RAW          "ov8865mipiraw"
#define SENSOR_DRVNAME_OV8858_MIPI_RAW          "ov8858mipiraw"
#define SENSOR_DRVNAME_OV8858S_MIPI_RAW         "ov8858smipiraw"
#define SENSOR_DRVNAME_OV8830_RAW               "ov8830"
#define SENSOR_DRVNAME_OV8825_MIPI_RAW          "ov8825mipiraw"
#define SENSOR_DRVNAME_OV7675_YUV               "ov7675yuv"
#define SENSOR_DRVNAME_OV5693_MIPI_RAW          "ov5693mipi"
#define SENSOR_DRVNAME_OV5670_MIPI_RAW          "ov5670mipiraw"
#define SENSOR_DRVNAME_OV5670_MIPI_RAW_2        "ov5670mipiraw2"
#define SENSOR_DRVNAME_OV5671_MIPI_RAW          "ov5671mipiraw"
#define SENSOR_DRVNAME_OV5647MIPI_RAW           "ov5647mipiraw"
#define SENSOR_DRVNAME_OV5645_MIPI_YUV          "ov5645_mipi_yuv"
#define SENSOR_DRVNAME_OV5650MIPI_RAW           "ov5650mipiraw"
#define SENSOR_DRVNAME_OV5650_RAW               "ov5650raw"
#define SENSOR_DRVNAME_OV5648_MIPI_RAW          "ov5648mipi"
#define SENSOR_DRVNAME_OV5647_RAW               "ov5647"
#define SENSOR_DRVNAME_OV5642_RAW               "ov5642raw"
#define SENSOR_DRVNAME_OV5642_MIPI_YUV          "ov5642mipiyuv"
#define SENSOR_DRVNAME_OV5642_MIPI_RGB          "ov5642mipirgb"
#define SENSOR_DRVNAME_OV5642_MIPI_JPG          "ov5642mipijpg"
#define SENSOR_DRVNAME_OV5642_YUV               "ov5642yuv"
#define SENSOR_DRVNAME_OV5642_YUV_SWI2C         "ov5642yuvswi2c"
#define SENSOR_DRVNAME_OV4688_MIPI_RAW          "ov4688mipiraw"
#define SENSOR_DRVNAME_OV3640_RAW               "ov3640"
#define SENSOR_DRVNAME_OV3640_YUV               "ov3640yuv"
#define SENSOR_DRVNAME_OV2724_MIPI_RAW          "ov2724mipiraw"
#define SENSOR_DRVNAME_OV2722_MIPI_RAW          "ov2722mipiraw"
#define SENSOR_DRVNAME_OV2680_MIPI_RAW          "ov2680mipiraw"
#define SENSOR_DRVNAME_OV2659_YUV               "ov2659yuv"
#define SENSOR_DRVNAME_OV2655_YUV               "ov2655yuv"
#define SENSOR_DRVNAME_OV2650_RAW               "ov265x"
/*S5K*/
#define SENSOR_DRVNAME_S5K2L7_MIPI_RAW          "s5k2l7mipiraw"
#define SENSOR_DRVNAME_S5K3L8_MIPI_RAW          "samsung_s5k3l8"
#define SENSOR_DRVNAME_S5K2X8_MIPI_RAW          "s5k2x8mipiraw"
#define SENSOR_DRVNAME_S5K2P8_MIPI_RAW          "s5k2p8mipiraw"
#define SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW      "s5k3p3sxmipiraw"
#define SENSOR_DRVNAME_S5K3M2_MIPI_RAW          "s5k3m2mipiraw"
#define SENSOR_DRVNAME_S5K3H2YX_MIPI_RAW        "s5k3h2yxmipiraw"
#define SENSOR_DRVNAME_S5K3H7Y_MIPI_RAW         "s5k3h7ymipiraw"
#define SENSOR_DRVNAME_S5K4H5YC_MIPI_RAW        "s5k4h5ycmipiraw"
#define SENSOR_DRVNAME_S5K4E1GA_MIPI_RAW        "s5k4e1gamipiraw"
#define SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV        "s5k4ecgxmipiyuv"
#define SENSOR_DRVNAME_S5K5CAGX_YUV             "s5k5cagxyuv"
#define SENSOR_DRVNAME_S5K4H5YX_2LANE_MIPI_RAW  "s5k4h5yx2lanemipiraw"
#define SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW        "s5k5e2yamipiraw"
#define SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV        "s5k8aayxmipiyuv"
#define SENSOR_DRVNAME_S5K8AAYX_YUV             "s5k8aayxyuv"
#define SENSOR_DRVNAME_S5K5E8YX_MIPI_RAW        "samsung_s5k5e8yx"
/*HI*/
#define SENSOR_DRVNAME_HI841_MIPI_RAW           "hi841mipiraw"
#define SENSOR_DRVNAME_HI707_YUV                "hi707yuv"
#define SENSOR_DRVNAME_HI704_YUV                "hi704yuv"
#define SENSOR_DRVNAME_HI551_MIPI_RAW           "hi551mipiraw"
#define SENSOR_DRVNAME_HI553_MIPI_RAW           "hi553mipiraw"
#define SENSOR_DRVNAME_HI545_MIPI_RAW           "hi545mipiraw"
#define SENSOR_DRVNAME_HI542_RAW                "hi542raw"
#define SENSOR_DRVNAME_HI542MIPI_RAW            "hi542mipiraw"
#define SENSOR_DRVNAME_HI544_MIPI_RAW           "hi544mipiraw"
#define SENSOR_DRVNAME_HI253_YUV                "hi253yuv"
#define SENSOR_DRVNAME_HI191_MIPI_RAW           "hi191mipiraw"
/*MT*/
#define SENSOR_DRVNAME_MT9P012_RAW              "mt9p012"
#define SENSOR_DRVNAME_MT9P015_RAW              "mt9p015"
#define SENSOR_DRVNAME_MT9P017_RAW              "mt9p017"
#define SENSOR_DRVNAME_MT9P017_MIPI_RAW         "mt9p017mipi"
#define SENSOR_DRVNAME_MT9D115_MIPI_RAW         "mt9d115mipiraw"
#define SENSOR_DRVNAME_MT9V114_YUV              "mt9v114"
#define SENSOR_DRVNAME_MT9V115_YUV              "mt9v115yuv"
#define SENSOR_DRVNAME_MT9T113_YUV              "mt9t113yuv"
#define SENSOR_DRVNAME_MT9V113_YUV              "mt9v113yuv"
#define SENSOR_DRVNAME_MT9T113_MIPI_YUV         "mt9t113mipiyuv"
/*GC*/
#define SENSOR_DRVNAME_GC2035_YUV               "gc2035_yuv"
#define SENSOR_DRVNAME_GC2235_RAW               "gc2235_raw"
#define SENSOR_DRVNAME_GC2355_MIPI_RAW          "gc2355mipiraw"
#define SENSOR_DRVNAME_GC0330_YUV               "gc0330_yuv"
#define SENSOR_DRVNAME_GC0329_YUV               "gc0329yuv"
#define SENSOR_DRVNAME_GC2145_MIPI_YUV          "gc2145mipiyuv"
#define SENSOR_DRVNAME_GC0310_MIPI_YUV          "gc0310mipiyuv"
#define SENSOR_DRVNAME_GC0310_YUV               "gc0310yuv"
#define SENSOR_DRVNAME_GC0312_YUV               "gc0312yuv"
#define SENSOR_DRVNAME_GC0313MIPI_YUV           "gc0313mipiyuv"
/*SP*/
#define SENSOR_DRVNAME_SP0A19_YUV               "sp0a19yuv"
#define SENSOR_DRVNAME_SP2518_YUV               "sp2518yuv"
/*A*/
#define SENSOR_DRVNAME_A5141_MIPI_RAW           "a5141mipiraw"
#define SENSOR_DRVNAME_A5142_MIPI_RAW           "a5142mipiraw"
/*HM*/
#define SENSOR_DRVNAME_HM3451_RAW               "hm3451raw"
#define SENSOR_DRVNAME_HM2051_MIPI_RAW          "hm2051mipiraw"
/*AR*/
#define SENSOR_DRVNAME_AR0833_MIPI_RAW          "ar0833mipiraw"
/*SIV*/
#define SENSOR_DRVNAME_SIV121D_YUV              "siv121dyuv"
#define SENSOR_DRVNAME_SIV120B_YUV              "siv120byuv"
/*PAS (PixArt Image)*/
#define SENSOR_DRVNAME_PAS6180_SERIAL_YUV       "pas6180serialyuv"
/*Panasoic*/
#define SENSOR_DRVNAME_MN34152_MIPI_RAW         "mn34152mipiraw"
/*Toshiba*/
#define SENSOR_DRVNAME_T4KA7_MIPI_RAW           "t4ka7mipiraw"
/*Others*/
#define SENSOR_DRVNAME_SHARP3D_MIPI_YUV         "sharp3dmipiyuv"
#define SENSOR_DRVNAME_T8EV5_YUV                "t8ev5_yuv"
/*Test*/
#define SENSOR_DRVNAME_IMX135_MIPI_RAW_5MP      "imx135mipiraw5mp"

/*******************************************************************************
*
********************************************************************************/
void KD_IMGSENSOR_PROFILE_INIT(void);
void KD_IMGSENSOR_PROFILE(char *tag);
void KD_IMGSENSOR_PROFILE_INIT_I2C(void);
void KD_IMGSENSOR_PROFILE_I2C(char *tag, int trans_num);

#define mDELAY(ms)     mdelay(ms)
#define uDELAY(us)       udelay(us)
#endif              /* _KD_IMGSENSOR_H */
