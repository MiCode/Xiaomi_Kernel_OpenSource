#ifndef __HWID_H__
#define __HWID_H__

#define MAX_PRODUCT_SIZE            16
#define MAX_REVIRSION_SIZE          8
#define MAX_COUNTRY_SIZE            8

#define HARDWARE_PROJECT_UNKNOWN    0
#define HARDWARE_PROJECT_K2         3

#define HW_MAJOR_VERSION_SHIFT      16
#define HW_MINOR_VERSION_SHIFT      0
#define HW_COUNTRY_VERSION_SHIFT    20
#define HW_BUILD_VERSION_SHIFT      16
#define HW_MAJOR_VERSION_MASK       0xFFFF0000
#define HW_MINOR_VERSION_MASK       0x0000FFFF
#define HW_COUNTRY_VERSION_MASK     0xFFF00000
#define HW_BUILD_VERSION_MASK       0x000F0000

#define SMEM_ID_VENDOR1	135
#define	ADCDEV_MAJOR	0
#define	ADCDEV_MINOR	200
#define	XIAOMI_ADC_MODULE	"xiaomi_adc_module"
#define	XIAOMI_ADC_DEVICE	"xiaomi_adc_device"
#define	XIAOMI_ADC_CLASS	"xiaomi_adc_class"

typedef enum {
	CountryCN = 0x00,
	CountryGlobal = 0x01,
	CountryIndia = 0x02,
	INVALID = 0x03,
	CountryIDMax = 0x7FFFFFFF
} CountryType;

typedef enum {
	CHIPINFO_ID_UNKNOWN = 0x00,
	CHIPINFO_ID_MAX = 0x7FFFFFFF
}ChipInfoIdType;

typedef enum {
	PROJECT_ID_UNKNOWN = 0x00,
	PROJECT_ID_MAX = 0x7FFFFFFF
}ProjectInfoType;

/**
  Stores the target project and the hw version.
 */
struct project_info
{
	ChipInfoIdType chiptype;                        /* Chip identification type */
	uint32_t pro_r1;                                /* resistance of the project*/
	uint32_t pr_min_adc;                            /* min adc value from project resistance*/
	uint32_t pr_max_adc;                            /* max adc value from project resistance*/
	ProjectInfoType project;                        /* Project type of the mi predefine*/
	char productname[MAX_PRODUCT_SIZE];             /* product name*/
	uint32_t hw_r1;                                 /* resistance of the hwid*/
	uint32_t hr_min_adc;                            /* min adc value from hwid resistance*/
	uint32_t hr_max_adc;                            /* max adc value from hwid resistance*/
	char hw_level[MAX_REVIRSION_SIZE];              /* hardware reversion*/
	char hw_country[MAX_COUNTRY_SIZE];              /* hardware country*/
	uint32_t hw_id;                                 /* hardware id*/
	uint8_t  ddr_id;                                /* ddr id */
	uint32_t reserved1;                             /* reserved field1 for further*/
	uint32_t reserved2;                             /* reserved field2 for further*/
};

const char *product_name_get(void);
uint32_t get_hw_version_platform(void);
uint32_t get_hw_country_version(void);
uint32_t get_hw_version_major(void);
uint32_t get_hw_version_minor(void);
uint32_t get_hw_version_build(void);
uint32_t get_hw_project_adc(void);
uint32_t get_hw_build_adc(void);

#endif
