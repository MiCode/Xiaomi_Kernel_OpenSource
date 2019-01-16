#include <linux/ion_debugger.h>
#include <linux/ion_debugger_kernel.h>
#define OBJECT_TABLE_SIZE      1543

// for BT
typedef struct ObjectEntry{
    size_t slot;
    struct ObjectEntry* prev;
    struct ObjectEntry* next;
    size_t numEntries;
    size_t reference;
    void *object[0];
}ObjectEntry, *PObjectEntry;

typedef struct {
    size_t count;
    ObjectEntry* slots[OBJECT_TABLE_SIZE];
}ObjectTable;

typedef struct StringEntry{
    size_t slot;
    struct StringEntry* prev;
    struct StringEntry* next;
    size_t reference;
    size_t string_len;
    char *name;
}StringEntry, *PStringEntry;

typedef struct {
    size_t count;
    StringEntry* slots[OBJECT_TABLE_SIZE];
}StringTable;
struct ion_process_record *ion_get_inuse_process_usage_record2(void);
int record_ion_info(int from_kernel,ion_sys_record_t *param);
unsigned int get_kernel_backtrace(unsigned long *backtrace);
void get_kernel_symbol(unsigned long *backtrace,unsigned int numEntries, unsigned int *kernel_symbol);
char *get_userString_from_hashTable(char *string_name,unsigned int len);
char *get_kernelString_from_hashTable(char *string_name,unsigned int len);
char *get_string(char *string_name,unsigned int len,StringTable *table);
char *ion_get_backtrace_info(struct ion_record_basic_info *tracking_info,char *backtrace_string,unsigned int backtrace_string_len, unsigned int backtrace_index,unsigned int show_backtrace_type);
