#ifndef __DEBUG_H__
#define __DEBUG_H__

#define dbg_winsys_none           0x0000u 
#define dbg_msg_trunc             0x0001u
#define dbg_window_cmds           0x0002u
#define dbg_domain_cmds           0x0004u
#define dbg_session_cmds          0x0008u
#define dbg_session_creator_cmds  0x0010u
#define dbg_font                  0x0020u
#define dbg_focus                 0x0040u
#define dbg_root_drawops          0x0080u
#define dbg_recursion             0x0100u
#define dbg_zones                 0x0200u
#define dbg_map                   0x0400u
#define dbg_expose                0x0800u
#define dbg_events                0x1000u
#define dbg_titlebar              0x2000u
#define dbg_zone_xform            0x4000u
#define dbg_clip                  0x8000u
#define dbg_unmap                 0x10000u
#define dbg_winid                 0x20000u
#define dbg_move                  0x40000u
#define dbg_drag                  0x80000u
#define dbg_sessclose             0x100000u
#define dbg_mouse                 0x200000u
#define dbg_keyb                  0x400000u
#define dbg_front                 0x800000u
#define dbg_resize                0x1000000u
#define dbg_decoration            0x2000000u
#define dbg_destroy               0x4000000u

#define dbg_winsys_flags  (dbg_winsys_none | dbg_msg_trunc)

#define CND_DEBUG(x) (dbg_##x & dbg_winsys_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_winsys_flags) == \
                                            (dbg_##x|dbg_##y))

#endif
