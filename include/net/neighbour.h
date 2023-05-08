/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _NET_NEIGHBOUR_H
#define _NET_NEIGHBOUR_H

#include <linux/neighbour.h>

/*
 *	Generic neighbour manipulation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Alexey Kuznetsov	<kuznet@ms2.inr.ac.ru>
 *
 * 	Changes:
 *
 *	Harald Welte:		<laforge@gnumonks.org>
 *		- Add neighbour cache statistics like rtstat
 */

#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rcupdate.h>
#include <linux/seq_file.h>
#include <linux/bitmap.h>

#include <linux/err.h>
#include <linux/sysctl.h>
#include <linux/workqueue.h>
#include <net/rtnetlink.h>

/*
 * NUD stands for "neighbor unreachability detection"
 */

#define NUD_IN_TIMER	(NUD_INCOMPLETE|NUD_REACHABLE|NUD_DELAY|NUD_PROBE)
#define NUD_VALID	(NUD_PERMANENT|NUD_NOARP|NUD_REACHABLE|NUD_PROBE|NUD_STALE|NUD_DELAY)
#define NUD_CONNECTED	(NUD_PERMANENT|NUD_NOARP|NUD_REACHABLE)

struct neighbour;

enum {
	// NEIGH_VAR_MCAST_PROBES - 多播探测次数，对于发现或确认邻居设备的可达性，发送多播探测请求的次数
	NEIGH_VAR_MCAST_PROBES,
	// NEIGH_VAR_UCAST_PROBES - 单播探测次数，对于发现或确认邻居设备的可达性，发送单播探测请求的次数。
	NEIGH_VAR_UCAST_PROBES,
	// NEIGH_VAR_APP_PROBES - 应用程序探测次数，对于应用程序触发的邻居表项探测请求的次数。
	NEIGH_VAR_APP_PROBES,
	// NEIGH_VAR_MCAST_REPROBES - 多播重新探测次数，当之前的多播探测请求未收到回复时，重试多播探测请求的次数。
	NEIGH_VAR_MCAST_REPROBES,
	// NEIGH_VAR_RETRANS_TIME - 重传时间，如果还没有收到回复，在发送下一个探测请求之间等待的时间。
	NEIGH_VAR_RETRANS_TIME,
	// NEIGH_VAR_BASE_REACHABLE_TIME - 基本可达时间，邻居设备被认为是可达的最大时间，超过这个时间，邻居设备将被认为是不可达的。
	NEIGH_VAR_BASE_REACHABLE_TIME,
	// NEIGH_VAR_DELAY_PROBE_TIME - 延迟探测时间，在发送第一个探测请求之前等待的时间。
	NEIGH_VAR_DELAY_PROBE_TIME,
	// NEIGH_VAR_GC_STALETIME - 垃圾回收过期时间，可用于确定邻居表项何时过期并需要清理。
	NEIGH_VAR_GC_STALETIME,
	// NEIGH_VAR_QUEUE_LEN_BYTES - 队列长度（以字节为单位），为邻居表项保留的暂存数据包的最大字节量。
	NEIGH_VAR_QUEUE_LEN_BYTES,
	// NEIGH_VAR_PROXY_QLEN - 代理队列长度，在网络设备充当代理的情况下，为邻居表项保留的暂存请求的最大数量。
	NEIGH_VAR_PROXY_QLEN,
	// NEIGH_VAR_ANYCAST_DELAY - 任播延迟，网络设备在处理任播请求之前等待的时间。
	NEIGH_VAR_ANYCAST_DELAY,
	// NEIGH_VAR_PROXY_DELAY - 代理延迟，在网络设备充当代理的情况下，在处理邻居请求之前等待的时间。
	NEIGH_VAR_PROXY_DELAY,
	// NEIGH_VAR_LOCKTIME - 锁定时间，在邻居表项锁定期间，不允许修改的时间。
	NEIGH_VAR_LOCKTIME,
#define NEIGH_VAR_DATA_MAX (NEIGH_VAR_LOCKTIME + 1)
	/* Following are used as a second way to access one of the above */
	// NEIGH_VAR_QUEUE_LEN - 同 NEIGH_VAR_QUEUE_LEN_BYTES。
	NEIGH_VAR_QUEUE_LEN, /* same data as NEIGH_VAR_QUEUE_LEN_BYTES */
	// NEIGH_VAR_RETRANS_TIME_MS - 同 NEIGH_VAR_RETRANS_TIME，但以毫秒为单位。
	NEIGH_VAR_RETRANS_TIME_MS, /* same data as NEIGH_VAR_RETRANS_TIME */
	// NEIGH_VAR_BASE_REACHABLE_TIME_MS - 同 NEIGH_VAR_BASE_REACHABLE_TIME，但以毫秒为单位。
	NEIGH_VAR_BASE_REACHABLE_TIME_MS, /* same data as NEIGH_VAR_BASE_REACHABLE_TIME */
	/* Following are used by "default" only */
	// NEIGH_VAR_GC_INTERVAL - 垃圾回收检查间隔，用于检查邻居表项过期或清理的时间间隔。
	NEIGH_VAR_GC_INTERVAL,
	// NEIGH_VAR_GC_THRESH1 - 垃圾回收阈值1，邻居表项数量低于此阈值时，不进行垃圾回收。
	NEIGH_VAR_GC_THRESH1,
	// NEIGH_VAR_GC_THRESH2 - 垃圾回收阈值2，邻居表项数量高于此阈值时，对过期邻居表项进行垃圾回收。
	NEIGH_VAR_GC_THRESH2,
	// NEIGH_VAR_GC_THRESH3 - 垃圾回收阈值3，当邻居表项数量达到此阈值时，加速垃圾回收过程。
	NEIGH_VAR_GC_THRESH3,
	NEIGH_VAR_MAX
};

// neigh_parms: 包含了管理和设置网络邻居参数所需的数据和函数指针。这些参数通常用于网络协议，如 ARP（IPv4）和 NDP（IPv6），以便更有效地与网络设备和其他网络邻居进行通信。
struct neigh_parms {
	// 与网络相关的数据
	possible_net_t net;
	// 与网络设备相关的信息
	struct net_device *dev;
	// 用于存储 neigh_parms 实例的链表结构
	struct list_head list;
	// 在创建邻居时进行必要的初始化
	int	(*neigh_setup)(struct neighbour *);
	// 删除邻居时进行必要的清理工作
	void	(*neigh_cleanup)(struct neighbour *);
	// 该参数对应的邻居表数据结构
	struct neigh_table *tbl;
	// 存储与邻居参数相关的系统控制参数
	void	*sysctl_table;
	// 表示当前 neigh_parms 结构体是否已经废弃/不再使用
	int dead;
	refcount_t refcnt;
	// 一个 RCU(list) 头数据结构，用于支持无锁同步（如读写锁定）的遍历和更新链表操作。
	struct rcu_head rcu_head;

	// 表示邻居项主动确定为可达的时间，通常由网络协议计算得出。
	int	reachable_time;
	// 存储邻居参数的数据（由 NEIGH_VAR_DATA_MAX 定义数组的大小）。
	int	data[NEIGH_VAR_DATA_MAX];
	// 声明一个位图，用来表示邻居参数数据的状态
	DECLARE_BITMAP(data_state, NEIGH_VAR_DATA_MAX);
};

static inline void neigh_var_set(struct neigh_parms *p, int index, int val)
{
	set_bit(index, p->data_state);
	p->data[index] = val;
}

#define NEIGH_VAR(p, attr) ((p)->data[NEIGH_VAR_ ## attr])

/* In ndo_neigh_setup, NEIGH_VAR_INIT should be used.
 * In other cases, NEIGH_VAR_SET should be used.
 */
#define NEIGH_VAR_INIT(p, attr, val) (NEIGH_VAR(p, attr) = val)
#define NEIGH_VAR_SET(p, attr, val) neigh_var_set(p, NEIGH_VAR_ ## attr, val)

static inline void neigh_parms_data_state_setall(struct neigh_parms *p)
{
	bitmap_fill(p->data_state, NEIGH_VAR_DATA_MAX);
}

static inline void neigh_parms_data_state_cleanall(struct neigh_parms *p)
{
	bitmap_zero(p->data_state, NEIGH_VAR_DATA_MAX);
}

struct neigh_statistics {
	unsigned long allocs;		/* number of allocated neighs */
	unsigned long destroys;		/* number of destroyed neighs */
	unsigned long hash_grows;	/* number of hash resizes */

	unsigned long res_failed;	/* number of failed resolutions */

	unsigned long lookups;		/* number of lookups */
	unsigned long hits;		/* number of hits (among lookups) */

	unsigned long rcv_probes_mcast;	/* number of received mcast ipv6 */
	unsigned long rcv_probes_ucast; /* number of received ucast ipv6 */

	unsigned long periodic_gc_runs;	/* number of periodic GC runs */
	unsigned long forced_gc_runs;	/* number of forced GC runs */

	unsigned long unres_discards;	/* number of unresolved drops */
	unsigned long table_fulls;      /* times even gc couldn't help */
};

#define NEIGH_CACHE_STAT_INC(tbl, field) this_cpu_inc((tbl)->stats->field)

// neighbour: 表示Linux内核中的一个网络邻居项。网络邻居是指与本地计算机直接相连的其他设备，如路由器、交换机、服务器等。
// 这个结构体包含了一些必要的数据成员和函数指针，以维护与网络邻居的连接状态、地址信息、操作等
struct neighbour {
	// next：是指向下一个邻居结构体的指针，实现邻居列表。
	struct neighbour __rcu	*next;
	// tbl: 指向与当前邻居项相关联的邻居表（neigh_table）的指针。
	struct neigh_table	*tbl;
	// parms: 指向与当前邻居项相关联的参数的指针。
	struct neigh_parms	*parms;
	// confirmed：上次确认邻居项有效性的时间戳（jiffies）。
	unsigned long		confirmed;
	// updated：上次更新邻居项时的时间戳。
	unsigned long		updated;
	// lock： 读写锁，用于保护邻居项的结构和状态。
	rwlock_t		lock;
	// refcnt：引用计数器，用于跟踪邻居项的引用数。
	refcount_t		refcnt;
	// arp_queue: 存储待确定邻居MAC地址的数据包队列。
	struct sk_buff_head	arp_queue;
	// arp_queue_len_bytes：arp_queue队列中数据包的总字节数。
	unsigned int		arp_queue_len_bytes;
	// timer：一个定时器，用于定时执行邻居项的任务，如重试ARP请求或刷新缓存。
	struct timer_list	timer;
	// used：最近一次使用邻居项的时间戳。
	unsigned long		used;
	// probes：用于跟踪发送的探测数据包数量的原子变量。
	atomic_t		probes;
	// flags：用于描述邻居项的标志位。
	__u8			flags;
	// nud_state：邻居项的状态，如可达、失败等。
	__u8			nud_state;
	// type：邻居项所属类型的标识符，如ARP表项、IPv6邻居项等。
	__u8			type;
	// dead：标记邻居项是否已经废弃。
	__u8			dead;
	// ha_lock： 序列锁，用于保护邻居的硬件地址（如MAC地址）。
	seqlock_t		ha_lock;
	// ha：硬件地址（如MAC地址），对齐到unsigned long类型的地址。
	unsigned char		ha[ALIGN(MAX_ADDR_LEN, sizeof(unsigned long))];
	// hh：hash值的缓存，用于加速硬件报头查询。
	struct hh_cache		hh;
	// output：一个函数指针，用于输出数据包到邻居。
	int			(*output)(struct neighbour *, struct sk_buff *);
	// ops：指向邻居操作的结构体指针，包括一系列操作函数，如获取邻居、删除邻居等。
	const struct neigh_ops	*ops;
	// rcu：一个RCU（Read-Copy-Update）结构体，用于实现读者-写者同步机制。
	struct rcu_head		rcu;
	// dev：指向与当前邻居项关联的网络设备（如网络接口）的指针。
	struct net_device	*dev;
	// primary_key: 动态长度数组，存储邻居项的键（如IPv4地址、IPv6地址等）。
	u8			primary_key[0];
} __randomize_layout; // __randomize_layout是一种编译器优化策略，它为结构体数据成员生成随机布局，以提高安全性。
struct neigh_ops {
	int			family;
	void			(*solicit)(struct neighbour *, struct sk_buff *);
	void			(*error_report)(struct neighbour *, struct sk_buff *);
	int			(*output)(struct neighbour *, struct sk_buff *);
	int			(*connected_output)(struct neighbour *, struct sk_buff *);
};

struct pneigh_entry {
	struct pneigh_entry	*next;
	possible_net_t		net;
	struct net_device	*dev;
	u8			flags;
	u8			key[0];
};

/*
 *	neighbour table manipulation
 */

#define NEIGH_NUM_HASH_RND	4

struct neigh_hash_table {
	struct neighbour __rcu	**hash_buckets;
	unsigned int		hash_shift;
	__u32			hash_rnd[NEIGH_NUM_HASH_RND];
	struct rcu_head		rcu;
};


// neigh_table in the Linux kernel. The structure is used to represent a neighbor table (also known as an ARP table for IPv4 or an NDP table for IPv6). 
// A neighbor table is a data structure that stores information about the destination addresses and their associated network 
// layer to link layer address mappings within the kernel
struct neigh_table {
	// family: This field represents the address family of the entries stored in the table. Examples of address families are AF_INET (IPv4) and AF_INET6 (IPv6).
	int			family;
	// entry_size: The size of each entry in the neighbor table.
	unsigned int		entry_size;
	// key_len: The length of the key used for the hashing function.
	unsigned int		key_len;
	// protocol: This field represents the protocol used by the entries in the table, such as ETH_P_IP (IPv4) or ETH_P_IPV6 (IPv6).
	__be16			protocol;
	//  A pointer to the hash function used for hashing the neighbor entries.
	__u32			(*hash)(const void *pkey,
					const struct net_device *dev,
					__u32 *hash_rnd);
	// A pointer to the function that checks if the keys of two entries in the neighbor table are equal.
	bool			(*key_eq)(const struct neighbour *, const void *pkey);
	// A pointer to the function that initializes new neighbor entries.
	int			(*constructor)(struct neighbour *);
	// A pointer to the function that initializes new proxy neighbor entries.
	int			(*pconstructor)(struct pneigh_entry *);
	// A pointer to the function that destroys proxy neighbor entries.
	void			(*pdestructor)(struct pneigh_entry *);
	// A pointer to the function that re-processes the packets queued for proxying.
	void			(*proxy_redo)(struct sk_buff *skb);
	// id: An identifier for the table (e.g., "arp_cache" for ARP table).
	char			*id;
	// parms: The parameters used for this neighbor table.
	struct neigh_parms	parms;
	// parms_list: A list head for linking neighbor parameter entries.
	struct list_head	parms_list;
	// gc_interval: The interval in jiffies between garbage collection runs (cleaning up stale entries).
	int			gc_interval;
	// gc_thresh1: The minimum number of entries in the table before garbage collection starts.
	int			gc_thresh1;
	// gc_thresh2: The minimum number of entries left after garbage collection.
	int			gc_thresh2;
	// gc_thresh3: The maximum number of entries allowed in the table.
	int			gc_thresh3;
	// last_flush: The timestamp of the last garbage collection run.
	unsigned long		last_flush;
	// gc_work: A work structure that schedules and executes garbage collection.
	struct delayed_work	gc_work;
	// proxy_timer: A timer for proxy neighbor entries.
	struct timer_list 	proxy_timer;
	// proxy_queue: A list head for storing queued packets for proxying.
	struct sk_buff_head	proxy_queue;
	// entries: An atomic variable representing the current number of entries in the table
	atomic_t		entries;
	// lock: A read-write lock protecting access and modification of the neighbor table.
	rwlock_t		lock;
	// last_rand: A field to store the last random value, used for timer variance.
	unsigned long		last_rand;
	// *stats: Per-CPU neighbor table statistics.
	struct neigh_statistics	__percpu *stats;
	// *nht: A pointer to the resizable neighbor hash table.
	struct neigh_hash_table __rcu *nht;
	// **phash_buckets: A pointer to an array of pointers to proxy neighbor entries.
	struct pneigh_entry	**phash_buckets;
};

enum {
	NEIGH_ARP_TABLE = 0,
	NEIGH_ND_TABLE = 1,
	NEIGH_DN_TABLE = 2,
	NEIGH_NR_TABLES,
	NEIGH_LINK_TABLE = NEIGH_NR_TABLES /* Pseudo table for neigh_xmit */
};

static inline int neigh_parms_family(struct neigh_parms *p)
{
	return p->tbl->family;
}

#define NEIGH_PRIV_ALIGN	sizeof(long long)
#define NEIGH_ENTRY_SIZE(size)	ALIGN((size), NEIGH_PRIV_ALIGN)

static inline void *neighbour_priv(const struct neighbour *n)
{
	return (char *)n + n->tbl->entry_size;
}

/* flags for neigh_update() */
#define NEIGH_UPDATE_F_OVERRIDE			0x00000001
#define NEIGH_UPDATE_F_WEAK_OVERRIDE		0x00000002
#define NEIGH_UPDATE_F_OVERRIDE_ISROUTER	0x00000004
#define NEIGH_UPDATE_F_EXT_LEARNED		0x20000000
#define NEIGH_UPDATE_F_ISROUTER			0x40000000
#define NEIGH_UPDATE_F_ADMIN			0x80000000


static inline bool neigh_key_eq16(const struct neighbour *n, const void *pkey)
{
	return *(const u16 *)n->primary_key == *(const u16 *)pkey;
}

static inline bool neigh_key_eq32(const struct neighbour *n, const void *pkey)
{
	return *(const u32 *)n->primary_key == *(const u32 *)pkey;
}

static inline bool neigh_key_eq128(const struct neighbour *n, const void *pkey)
{
	const u32 *n32 = (const u32 *)n->primary_key;
	const u32 *p32 = pkey;

	return ((n32[0] ^ p32[0]) | (n32[1] ^ p32[1]) |
		(n32[2] ^ p32[2]) | (n32[3] ^ p32[3])) == 0;
}

static inline struct neighbour *___neigh_lookup_noref(
	struct neigh_table *tbl,
	bool (*key_eq)(const struct neighbour *n, const void *pkey),
	__u32 (*hash)(const void *pkey,
		      const struct net_device *dev,
		      __u32 *hash_rnd),
	const void *pkey,
	struct net_device *dev)
{
	struct neigh_hash_table *nht = rcu_dereference_bh(tbl->nht);
	struct neighbour *n;
	u32 hash_val;

	hash_val = hash(pkey, dev, nht->hash_rnd) >> (32 - nht->hash_shift);
	for (n = rcu_dereference_bh(nht->hash_buckets[hash_val]);
	     n != NULL;
	     n = rcu_dereference_bh(n->next)) {
		if (n->dev == dev && key_eq(n, pkey))
			return n;
	}

	return NULL;
}

static inline struct neighbour *__neigh_lookup_noref(struct neigh_table *tbl,
						     const void *pkey,
						     struct net_device *dev)
{
	return ___neigh_lookup_noref(tbl, tbl->key_eq, tbl->hash, pkey, dev);
}

void neigh_table_init(int index, struct neigh_table *tbl);
int neigh_table_clear(int index, struct neigh_table *tbl);
struct neighbour *neigh_lookup(struct neigh_table *tbl, const void *pkey,
			       struct net_device *dev);
struct neighbour *neigh_lookup_nodev(struct neigh_table *tbl, struct net *net,
				     const void *pkey);
struct neighbour *__neigh_create(struct neigh_table *tbl, const void *pkey,
				 struct net_device *dev, bool want_ref);
static inline struct neighbour *neigh_create(struct neigh_table *tbl,
					     const void *pkey,
					     struct net_device *dev)
{
	return __neigh_create(tbl, pkey, dev, true);
}
void neigh_destroy(struct neighbour *neigh);
int __neigh_event_send(struct neighbour *neigh, struct sk_buff *skb);
int neigh_update(struct neighbour *neigh, const u8 *lladdr, u8 new, u32 flags,
		 u32 nlmsg_pid);
void __neigh_set_probe_once(struct neighbour *neigh);
bool neigh_remove_one(struct neighbour *ndel, struct neigh_table *tbl);
void neigh_changeaddr(struct neigh_table *tbl, struct net_device *dev);
int neigh_ifdown(struct neigh_table *tbl, struct net_device *dev);
int neigh_carrier_down(struct neigh_table *tbl, struct net_device *dev);
int neigh_resolve_output(struct neighbour *neigh, struct sk_buff *skb);
int neigh_connected_output(struct neighbour *neigh, struct sk_buff *skb);
int neigh_direct_output(struct neighbour *neigh, struct sk_buff *skb);
struct neighbour *neigh_event_ns(struct neigh_table *tbl,
						u8 *lladdr, void *saddr,
						struct net_device *dev);

struct neigh_parms *neigh_parms_alloc(struct net_device *dev,
				      struct neigh_table *tbl);
void neigh_parms_release(struct neigh_table *tbl, struct neigh_parms *parms);

static inline
struct net *neigh_parms_net(const struct neigh_parms *parms)
{
	return read_pnet(&parms->net);
}

unsigned long neigh_rand_reach_time(unsigned long base);

void pneigh_enqueue(struct neigh_table *tbl, struct neigh_parms *p,
		    struct sk_buff *skb);
struct pneigh_entry *pneigh_lookup(struct neigh_table *tbl, struct net *net,
				   const void *key, struct net_device *dev,
				   int creat);
struct pneigh_entry *__pneigh_lookup(struct neigh_table *tbl, struct net *net,
				     const void *key, struct net_device *dev);
int pneigh_delete(struct neigh_table *tbl, struct net *net, const void *key,
		  struct net_device *dev);

static inline struct net *pneigh_net(const struct pneigh_entry *pneigh)
{
	return read_pnet(&pneigh->net);
}

void neigh_app_ns(struct neighbour *n);
void neigh_for_each(struct neigh_table *tbl,
		    void (*cb)(struct neighbour *, void *), void *cookie);
void __neigh_for_each_release(struct neigh_table *tbl,
			      int (*cb)(struct neighbour *));
int neigh_xmit(int fam, struct net_device *, const void *, struct sk_buff *);
void pneigh_for_each(struct neigh_table *tbl,
		     void (*cb)(struct pneigh_entry *));

struct neigh_seq_state {
	struct seq_net_private p;
	struct neigh_table *tbl;
	struct neigh_hash_table *nht;
	void *(*neigh_sub_iter)(struct neigh_seq_state *state,
				struct neighbour *n, loff_t *pos);
	unsigned int bucket;
	unsigned int flags;
#define NEIGH_SEQ_NEIGH_ONLY	0x00000001
#define NEIGH_SEQ_IS_PNEIGH	0x00000002
#define NEIGH_SEQ_SKIP_NOARP	0x00000004
};
void *neigh_seq_start(struct seq_file *, loff_t *, struct neigh_table *,
		      unsigned int);
void *neigh_seq_next(struct seq_file *, void *, loff_t *);
void neigh_seq_stop(struct seq_file *, void *);

int neigh_proc_dointvec(struct ctl_table *ctl, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos);
int neigh_proc_dointvec_jiffies(struct ctl_table *ctl, int write,
				void __user *buffer,
				size_t *lenp, loff_t *ppos);
int neigh_proc_dointvec_ms_jiffies(struct ctl_table *ctl, int write,
				   void __user *buffer,
				   size_t *lenp, loff_t *ppos);

int neigh_sysctl_register(struct net_device *dev, struct neigh_parms *p,
			  proc_handler *proc_handler);
void neigh_sysctl_unregister(struct neigh_parms *p);

static inline void __neigh_parms_put(struct neigh_parms *parms)
{
	refcount_dec(&parms->refcnt);
}

static inline struct neigh_parms *neigh_parms_clone(struct neigh_parms *parms)
{
	refcount_inc(&parms->refcnt);
	return parms;
}

/*
 *	Neighbour references
 */

static inline void neigh_release(struct neighbour *neigh)
{
	if (refcount_dec_and_test(&neigh->refcnt))
		neigh_destroy(neigh);
}

static inline struct neighbour * neigh_clone(struct neighbour *neigh)
{
	if (neigh)
		refcount_inc(&neigh->refcnt);
	return neigh;
}

#define neigh_hold(n)	refcount_inc(&(n)->refcnt)

static inline int neigh_event_send(struct neighbour *neigh, struct sk_buff *skb)
{
	unsigned long now = jiffies;
	
	if (neigh->used != now)
		neigh->used = now;
	if (!(neigh->nud_state&(NUD_CONNECTED|NUD_DELAY|NUD_PROBE)))
		return __neigh_event_send(neigh, skb);
	return 0;
}

#if IS_ENABLED(CONFIG_BRIDGE_NETFILTER)
static inline int neigh_hh_bridge(struct hh_cache *hh, struct sk_buff *skb)
{
	unsigned int seq, hh_alen;

	do {
		seq = read_seqbegin(&hh->hh_lock);
		hh_alen = HH_DATA_ALIGN(ETH_HLEN);
		memcpy(skb->data - hh_alen, hh->hh_data, ETH_ALEN + hh_alen - ETH_HLEN);
	} while (read_seqretry(&hh->hh_lock, seq));
	return 0;
}
#endif

static inline int neigh_hh_output(const struct hh_cache *hh, struct sk_buff *skb)
{
	unsigned int hh_alen = 0;
	unsigned int seq;
	unsigned int hh_len;

	do {
		seq = read_seqbegin(&hh->hh_lock);
		hh_len = hh->hh_len;
		if (likely(hh_len <= HH_DATA_MOD)) {
			hh_alen = HH_DATA_MOD;

			/* skb_push() would proceed silently if we have room for
			 * the unaligned size but not for the aligned size:
			 * check headroom explicitly.
			 */
			if (likely(skb_headroom(skb) >= HH_DATA_MOD)) {
				/* this is inlined by gcc */
				memcpy(skb->data - HH_DATA_MOD, hh->hh_data,
				       HH_DATA_MOD);
			}
		} else {
			hh_alen = HH_DATA_ALIGN(hh_len);

			if (likely(skb_headroom(skb) >= hh_alen)) {
				memcpy(skb->data - hh_alen, hh->hh_data,
				       hh_alen);
			}
		}
	} while (read_seqretry(&hh->hh_lock, seq));

	if (WARN_ON_ONCE(skb_headroom(skb) < hh_alen)) {
		kfree_skb(skb);
		return NET_XMIT_DROP;
	}

	__skb_push(skb, hh_len);
	return dev_queue_xmit(skb);
}

static inline int neigh_output(struct neighbour *n, struct sk_buff *skb)
{
	const struct hh_cache *hh = &n->hh;

	if ((n->nud_state & NUD_CONNECTED) && hh->hh_len)
		return neigh_hh_output(hh, skb);
	else
		return n->output(n, skb);
}

static inline struct neighbour *
__neigh_lookup(struct neigh_table *tbl, const void *pkey, struct net_device *dev, int creat)
{
	struct neighbour *n = neigh_lookup(tbl, pkey, dev);

	if (n || !creat)
		return n;

	n = neigh_create(tbl, pkey, dev);
	return IS_ERR(n) ? NULL : n;
}

static inline struct neighbour *
__neigh_lookup_errno(struct neigh_table *tbl, const void *pkey,
  struct net_device *dev)
{
	struct neighbour *n = neigh_lookup(tbl, pkey, dev);

	if (n)
		return n;

	return neigh_create(tbl, pkey, dev);
}

struct neighbour_cb {
	unsigned long sched_next;
	unsigned int flags;
};

#define LOCALLY_ENQUEUED 0x1

#define NEIGH_CB(skb)	((struct neighbour_cb *)(skb)->cb)

static inline void neigh_ha_snapshot(char *dst, const struct neighbour *n,
				     const struct net_device *dev)
{
	unsigned int seq;

	do {
		seq = read_seqbegin(&n->ha_lock);
		memcpy(dst, n->ha, dev->addr_len);
	} while (read_seqretry(&n->ha_lock, seq));
}

static inline void neigh_update_ext_learned(struct neighbour *neigh, u32 flags,
					    int *notify)
{
	u8 ndm_flags = 0;

	if (!(flags & NEIGH_UPDATE_F_ADMIN))
		return;

	ndm_flags |= (flags & NEIGH_UPDATE_F_EXT_LEARNED) ? NTF_EXT_LEARNED : 0;
	if ((neigh->flags ^ ndm_flags) & NTF_EXT_LEARNED) {
		if (ndm_flags & NTF_EXT_LEARNED)
			neigh->flags |= NTF_EXT_LEARNED;
		else
			neigh->flags &= ~NTF_EXT_LEARNED;
		*notify = 1;
	}
}

static inline void neigh_update_is_router(struct neighbour *neigh, u32 flags,
					  int *notify)
{
	u8 ndm_flags = 0;

	ndm_flags |= (flags & NEIGH_UPDATE_F_ISROUTER) ? NTF_ROUTER : 0;
	if ((neigh->flags ^ ndm_flags) & NTF_ROUTER) {
		if (ndm_flags & NTF_ROUTER)
			neigh->flags |= NTF_ROUTER;
		else
			neigh->flags &= ~NTF_ROUTER;
		*notify = 1;
	}
}
#endif
