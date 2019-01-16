#ifndef SEC_MTD_H
#define SEC_MTD_H

#include <mach/sec_osal.h>  

/**************************************************************************
*  MTD INTERNAL DEFINITION
**************************************************************************/
typedef struct _MtdRCtx 
{
    char *buf;
    ASF_FILE fd;    

} MtdRCtx;

/**************************************************************************
*  MTD CONFIGURATION
**************************************************************************/
#define ROM_INFO_SEARCH_LEN             (0x100000)
#define SECRO_SEARCH_START              (0x0)
#define SECRO_SEARCH_LEN                (0x100000)

/* mtd number */
#define MTD_PL_NUM                      (0x0)
#define MTD_SECCFG_NUM                  (0x3)

/* indicate the search region each time */
#define ROM_INFO_SEARCH_REGION          (0x2000)
#define SECRO_SEARCH_REGION             (0x4000)

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern void sec_mtd_find_partitions(void);
extern unsigned int sec_mtd_read_image(char* part_name, char* buf, unsigned int off, unsigned int sz);
extern unsigned int sec_mtd_get_off(char* part_name);

#endif  // SEC_MTD_H
