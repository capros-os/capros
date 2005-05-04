#ifndef __DEBUG_H__
#define __DEBUG_H__

#define dbg_eventmgr_cmds   0x001u

#define dbg_eventmgr_flags  (0)

#define CND_DEBUG(x) (dbg_##x & dbg_eventmgr_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_eventmgr_flags) == \
                                            (dbg_##x|dbg_##y))

#endif
