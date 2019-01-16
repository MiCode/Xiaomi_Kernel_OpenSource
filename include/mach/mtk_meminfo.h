#ifndef __MTK_MEMINFO_H__
#define __MTK_MEMINFO_H__

/* physical offset */
extern phys_addr_t get_phys_offset(void);
/* physical DRAM size */
extern phys_addr_t get_max_DRAM_size(void);
/* DRAM size controlled by kernel */
extern phys_addr_t get_memory_size(void);

#endif /* end __MTK_MEMINFO_H__ */
