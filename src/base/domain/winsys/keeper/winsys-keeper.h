#define OC_WINSYS_KEEPER_SETUP 65535

unsigned fbfault_init(cap_t kr_keeper, cap_t kr_bank, cap_t kr_sched);

unsigned fbfault_received(void);
