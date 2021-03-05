#ifndef __SF_AUTO_H__
#define __SF_AUTO_H__

// auto

/* mtk or not */
#if (SF_PLATFORM_SEL == SF_REE_QUALCOMM) || (SF_PLATFORM_SEL == SF_REE_SPREAD) || \
    (SF_PLATFORM_SEL == SF_TEE_QSEE) || (SF_PLATFORM_SEL == SF_TEE_TRUSTY) || \
    (SF_PLATFORM_SEL == SF_REE_HIKEY9600)
#define SF_MTK_CPU                     0
#else
#define SF_MTK_CPU                     1
#endif

/* tee or ree */
#if (SF_PLATFORM_SEL == SF_REE_QUALCOMM) || (SF_PLATFORM_SEL == SF_REE_SPREAD) || \
    (SF_PLATFORM_SEL == SF_REE_MTK) || (SF_PLATFORM_SEL == SF_REE_HIKEY9600) || \
    (SF_PLATFORM_SEL == SF_REE_MTK_L5_X)
#define SF_REE_PLATFORM             1
#else
#define SF_REE_PLATFORM             0
#endif

/* spi or platform bus driver */
#if (SF_PLATFORM_SEL == SF_TEE_QSEE) || (SF_PLATFORM_SEL == SF_TEE_TRUSTY)
#define SF_SPI_RW_EN                0
#else
#define SF_SPI_RW_EN                1
#endif

#if(SF_COMPATIBLE_SEL == SF_COMPATIBLE_NOF || SF_COMPATIBLE_SEL == SF_COMPATIBLE_NOF_BP_V2_7)
#define MULTI_HAL_COMPATIBLE        0
#else
#define MULTI_HAL_COMPATIBLE        1
#endif

/* check ree, trustkernel, beanpodV2 read chip ID by spi bus or not */
#if ((SF_REE_PLATFORM) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_REE)) || \
    (((SF_PLATFORM_SEL == SF_TEE_TRUSTKERNEL)) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_TRUSTKERNEL)) || \
    (((SF_PLATFORM_SEL == SF_TEE_BEANPOD)) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V2)) || \
    (((SF_PLATFORM_SEL == SF_TEE_BEANPOD)) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V2_7)) || \
    (((SF_PLATFORM_SEL == SF_TEE_RONGCARD)) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_RONGCARD))
#define SF_PROBE_ID_EN              1
#undef  MULTI_HAL_COMPATIBLE
#define MULTI_HAL_COMPATIBLE        0
#else
#define SF_PROBE_ID_EN              0
#endif

/* trustkernel compatible or not */
#if (SF_PLATFORM_SEL == SF_TEE_TRUSTKERNEL) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_TRUSTKERNEL)
#define SF_TRUSTKERNEL_COMPATIBLE   1
#else
#define SF_TRUSTKERNEL_COMPATIBLE   0
#endif

/* beanpod V1 compatible or not */
#if (SF_PLATFORM_SEL == SF_TEE_BEANPOD) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V1)
#define SF_BEANPOD_COMPATIBLE_V1    1
#else
#define SF_BEANPOD_COMPATIBLE_V1    0
#endif

#if (SF_PLATFORM_SEL == SF_TEE_BEANPOD) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V2)
#define SF_BEANPOD_COMPATIBLE_V2    1
#else
#define SF_BEANPOD_COMPATIBLE_V2    0
#endif

#if (SF_PLATFORM_SEL == SF_TEE_BEANPOD) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V2_7 || \
    SF_COMPATIBLE_SEL == SF_COMPATIBLE_NOF_BP_V2_7)
#define SF_BEANPOD_COMPATIBLE_V2_7  1
#else
#define SF_BEANPOD_COMPATIBLE_V2_7  0
#endif

#ifdef CONFIG_ARCH_MT6580
#define MT_CG_PERI_SPI0 MT_CG_SPI_SW_CG
#endif

/* rongcard compatible or not */
#if (SF_PLATFORM_SEL == SF_TEE_RONGCARD) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_RONGCARD)
#define SF_RONGCARD_COMPATIBLE      1
#else
#define SF_RONGCARD_COMPATIBLE      0
#endif

#if (SF_BEANPOD_COMPATIBLE_V1 || SF_BEANPOD_COMPATIBLE_V2 || SF_BEANPOD_COMPATIBLE_V2_7)
#undef  MULTI_HAL_COMPATIBLE
#define MULTI_HAL_COMPATIBLE        0
#endif

#if (SF_PLATFORM_SEL == SF_REE_SPREAD) || (SF_PLATFORM_SEL == SF_TEE_TRUSTY)
#undef  SF_INT_TRIG_HIGH
#define SF_INT_TRIG_HIGH            1
#endif

/* check log debug */
#if SF_LOG_ENABLE
#define SF_LOG_LEVEL                KERN_INFO
#else
#define SF_LOG_LEVEL                KERN_DEBUG
#endif

/* define androidL mtk */
#if (SF_PLATFORM_SEL == SF_REE_MTK_L5_X)
#define REE_MTK_ANDROID_L           1
#else
#define REE_MTK_ANDROID_L           0
#endif

#if SF_BEANPOD_COMPATIBLE_V2 || SF_REE_PLATFORM || \
    (SF_PLATFORM_SEL == SF_TEE_BEANPOD && SF_COMPATIBLE_SEL == SF_COMPATIBLE_BEANPOD_V2_7)
#define SF_SPI_TRANSFER             1
#else
#define SF_SPI_TRANSFER             0
#endif

/* beanpod and trustkernel compatible in CONFIG_SPI_MT65XX */
#if (defined(CONFIG_SPI_MT65XX) && (SF_COMPATIBLE_SEL == SF_COMPATIBLE_TRUSTKERNEL))
#define SF_TRUSTKERNEL_COMPAT_SPI_MT65XX         1
#else
#define SF_TRUSTKERNEL_COMPAT_SPI_MT65XX         0
#endif

#endif //__SF_AUTO_H__