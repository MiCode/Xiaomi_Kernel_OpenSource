#ifndef DEV_H
#define DEV_H

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern void sec_dev_dump_part(void);
extern void sec_dev_find_parts(void);
extern int sec_dev_read_rom_info(void);
extern int sec_dev_read_secroimg(void);
extern int sec_dev_read_secroimg_v5(unsigned int index);
extern unsigned int sec_dev_read_image(char* part_name, char* buf, u64 off, unsigned int sz, unsigned int image_type);


#endif  // DEV_H
