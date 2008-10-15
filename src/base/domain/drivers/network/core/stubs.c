#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
  panic("synchronize_rcu called!");
}

// From net/sched/sch_generic.c:
#include <net/sch_generic.h>
void dev_shutdown(struct net_device *dev)
{
  panic("dev_shutdown called!");
}

void dev_activate(struct net_device *dev)
{
  panic("dev_activate called!");
}

void dev_deactivate(struct net_device *dev)
{
  panic("dev_deactivate called!");
}

// From net/dev_mcast.c:
void dev_mc_discard(struct net_device *dev)
{
  panic("dev_mc_discard called!");
}

// From net/core/rtnetlink.c:
void rtmsg_ifinfo(int type, struct net_device *dev, unsigned change)
{
  panic("rtmsg_ifinfo called!");
}

// From hrtimer.h:
#include <linux/hrtimer.h>
ktime_t ktime_get_real(void)
{
  panic("ktime_get_real called!");
  ktime_t t = {0};
  return t;
}

// From arp.h:
#include <net/arp.h>
int      arp_find(unsigned char *haddr, struct sk_buff *skb)
{
  panic("arp_find called!");
  return 0;
}

