#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define QCM_SM_DELAY_1000MS	1000
#define QCM_SM_DELAY_500MS	500
#define QCM_SM_DELAY_400MS	400
#define QCM_SM_DELAY_300MS	300
#define QCM_SM_DELAY_200MS	200

#define QC3_VBUS_STEP		200
#define QC35_VBUS_STEP		20

#define QC3_MAX_TUNE_STEP	1
#define QC35_MAX_TUNE_STEP	4

#define QCM_BBC_ICL		100

#define ANTI_WAVE_COUNT_QC3	7
#define ANTI_WAVE_COUNT_QC35	1
#define MAX_TAPER_COUNT		5
#define MIN_JEITA_CHG_INDEX	3
#define MAX_JEITA_CHG_INDEX	4

#define MIN_CP_IBUS		100
#define MIN_ENTRY_FCC		2000
