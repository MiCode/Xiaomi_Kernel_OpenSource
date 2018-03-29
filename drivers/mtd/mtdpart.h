#include "mtdcore.h"

#ifdef DYNAMIC_CHANGE_MTD_WRITEABLE /* wschen 2011-01-05 */
static struct mtd_info *my_mtd;
#endif

