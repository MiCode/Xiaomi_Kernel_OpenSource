extern void __init bootmem_init(void);

void fixup_init(void);

extern bool memory_ssvp_inited(void);
extern phys_addr_t memory_ssvp_cma_base(void);
extern phys_addr_t memory_ssvp_cma_size(void);
