#ifndef _DFO_BOOT_H_
#define _DFO_BOOT_H_

/* 
 * DFO data 
 */
#define ATAG_DFO_DATA 0x41000805
typedef struct
{
    char name[32];              // kernel dfo name
    unsigned long value;        // kernel dfo value
} dfo_boot_info;

typedef struct
{
    dfo_boot_info info[1];      /* this is the minimum size */
} tag_dfo_boot;

#endif 
