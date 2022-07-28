#define HX_TAB "\t"
#define DBG_REG_NAME  "register"
#define DBG_INT_NAME "int_en"
#define DBG_SNS_NAME "SenseOnOff"
#define DBG_RST_NAME "reset"
#define DBG_LVL_NAME "debug_level"
#define DBG_VER_NAME "version"
#define DBG_DIAG_NAME "diag"
#define DBG_DIAG_ARR_NAME "diag_arr"
#define DBG_LAYOUT_NAME "layout"
#define DBG_EXCPT_NAME "excp_cnt"
#define DBG_GUEST_INFO_NAME "guest_info"
#define DBG_UPDATE_NAME "update"

#define HELP_INT_EN DBG_INT_NAME ":\n"\
HX_TAB "0 : disable irq\n"\
HX_TAB "1 : enable irq\n"
#define HELP_REGISTER DBG_REG_NAME ":\n"\
HX_TAB "echo " DBG_REG_NAME ",r:x > debug\n"\
HX_TAB "echo " DBG_REG_NAME ",w:x:x > debug\n"
#define HELP_SNS DBG_SNS_NAME ":\n"\
HX_TAB "0 : Sense off\n"\
HX_TAB "1 : Sesne on by leave safe mode\n"\
HX_TAB "1s : Sesne on with reset\n"
#define HELP_RST DBG_RST_NAME ":\n"\
HX_TAB "1 : trigger reset without reload\n"\
HX_TAB "2 : trigger reset with reload config\n"\
HX_TAB "test : test reset pin, show in kernel log\n"
#define HELP_LVL DBG_LVL_NAME ":\n"\
HX_TAB "0 : turn off all\n"\
HX_TAB "1 : all of event stack data,56~128 bytes\n"\
HX_TAB "2 : point data, all of actions of finger point\n"\
HX_TAB "4 : process time of irq\n"\
HX_TAB "8 : info of finger point down/up\n"\
HX_TAB "10 : detail info of processing self_test\n"\
HX_TAB "tsdbg0 : turn off irq process state\n"\
HX_TAB "tsdbg1 : turn on irq process state\n"
#define HELP_VER DBG_VER_NAME ":\n"\
HX_TAB "echo version > debug, it will RE-LOAD the FW version, Becare to use..\n"
#define HELP_DIAG DBG_DIAG_NAME ":\n"\
HX_TAB " - enter one number to use event stack or dsram rawdata\n"\
HX_TAB " - enter 4 number to choose type(stack/dsram)"\
"and rawout select(fw) to get rawdata"
#define HELP_DIAG_ARR DBG_DIAG_ARR_NAME ":\n"\
HX_TAB "0 : turn off all\n"\
HX_TAB "1 : rx reverse\n"\
HX_TAB "2 : tx reverse\n"\
HX_TAB "3 : rx & tx reverse\n"\
HX_TAB "4 : rotate 90 degree without reverse\n"\
HX_TAB "5 : rotate 90 degree with rx reverse\n"\
HX_TAB "6 : rotate 90 degree with tx reverse\n"\
HX_TAB "7 : rotate 90 degree with rx & tx reverse\n"
#define HELP_LAYOUT DBG_LAYOUT_NAME ":\n"\
HX_TAB "To change the touch resolution in driver\n"\
HX_TAB "min_x,max_x,min_y,max_y\n"
#define HELP_DD_DBG DBG_DDDBG_NAME ":\n"\
HX_TAB "To read DD register\n"\
HX_TAB "r:x[DD reg]:x[Bank]:x[Size]\n"
#define HELP_EXCPT DBG_EXCPT_NAME ":\n"\
HX_TAB "Show Exception Event\n"\
HX_TAB "0 : clear now all of state of exception event\n"
#define HELP_GUEST_INFO DBG_GUEST_INFO_NAME ":\n"\
HX_TAB "Only for project with FLASH\n"\
HX_TAB "It should turn on in define (Macro of source code)\n"\
HX_TAB "r : read customer info from flash\n"
#define HELP_LOT DBG_LOT_NAME ":\n"\
HX_TAB "Read lot id(ic id) from dd reg\n"
#define HELP_UPDATE DBG_UPDATE_NAME ":\n"\
HX_TAB "Using file name to update FW\n"\
HX_TAB "echo update,[file name bin file] > debug\n"
#define HELP_ALL_DEBUG "All:\n"\
HELP_REGISTER \
HELP_INT_EN \
HELP_SNS \
HELP_RST \
HELP_LVL \
HELP_VER \
HELP_DIAG \
HELP_DIAG_ARR \
HELP_EXCPT \
HELP_GUEST_INFO \
HELP_UPDATE

