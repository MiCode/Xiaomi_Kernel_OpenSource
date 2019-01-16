#ifndef USIF_UTILS_H
#define USIF_UTILS_H

/**************************************************************************
 *  EXTERNAL FUNCTIONS
 *************************************************************************/
extern char* usif2pl (char* part_name);
extern char* pl2usif (char* part_name);
extern bool sec_usif_enabled(void);
extern void sec_usif_part_path(unsigned int part_num, char* part_path, unsigned int part_path_len);

#endif  // USIF_UTILS_H
