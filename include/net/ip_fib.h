/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Forwarding Information Base.
 *
 * Authors:	A.N.Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#ifndef _NET_IP_FIB_H
#define _NET_IP_FIB_H

#include <net/flow.h>
#include <linux/seq_file.h>
#include <linux/rcupdate.h>
#include <net/fib_notifier.h>
#include <net/fib_rules.h>
#include <net/inetpeer.h>
#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/refcount.h>

// fib_config结构体，用于在Linux内核中表示一条转发信息基础（FIB）表的配置。
// FIB表用于存储IP网络中的路由选择信息
struct fib_config {
	// fc_dst_len：目标前缀长度，用于表示目标IP地址的子网位数。
	u8			fc_dst_len;
	// fc_tos：Type of Service，服务类型字段，用于区分负载传输优先级。
	u8			fc_tos;
	// fc_protocol：用于表示添加此路由表项的路由协议（如RIP, OSPF等）。
	u8			fc_protocol;
	// fc_scope：路由范围，用于确定目的地址可以被认为是本地的还是远程的。
	u8			fc_scope;
	// fc_type：路由类型，表示路由条目的类型（如单播，广播等）。
	u8			fc_type;
	/* 3 bytes unused */
	// fc_table：路由表ID，用于将路由条目添加到指定的路由表。
	u32			fc_table;
	// fc_dst：目标IP地址。
	__be32			fc_dst;
	// fc_gw：网关IP地址，数据包将通过此网关路由。
	__be32			fc_gw;
	// fc_oif：输出接口的索引，数据包将通过该接口发送。
	int			fc_oif;
	// fc_flags：路由标志位，通常用于标识路由选项，如通知路由更改等。
	u32			fc_flags;
	// fc_priority：路由优先级，用于决定在多条路由可用时选择哪条路由。
	u32			fc_priority;
	// fc_prefsrc：在源路由情况下，优选的源IP地址。
	__be32			fc_prefsrc;
	// fc_mx：指向路由器(metric)属性的指针。
	struct nlattr		*fc_mx;
	// fc_mp：指向多路径路由条目的指针。
	struct rtnexthop	*fc_mp;
	// fc_mx_len：fc_mx中路由器属性的长度。
	int			fc_mx_len;
	// fc_mp_len：多路径路由条目的长度。
	int			fc_mp_len;
	// fc_flow：用于分类数据流的流标签。
	u32			fc_flow;
	// fc_nlflags：Netlink消息的标志位。
	u32			fc_nlflags;
	// fc_nlinfo：关于Netlink消息的一些信息。
	struct nl_info		fc_nlinfo;
	// fc_encap：指向封装属性的指针。
	struct nlattr		*fc_encap;
	// fc_encap_type：封装类型，如MPLS，GRE等。
	u16			fc_encap_type;
};

struct fib_info;
struct rtable;

// fib_nh_exception用于表示Linux路由子系统中的一种特殊的“下一跳”（Next Hop）
struct fib_nh_exception {
	// fnhe_next：这是一个指向下一个fib_nh_exception结构体的指针，用于链接形成具有相同异常信息的下一跳列表。
	struct fib_nh_exception __rcu	*fnhe_next;
	// fnhe_genid：用于表示这个特定下一跳异常结构的生成ID。用于区分更新过程中的旧结构和新结构。
	int				fnhe_genid;
	// fnhe_daddr：这是一个表示目的地址的32位无符号整数（以网络字节序表示）。这个地址会发生异常。
	__be32				fnhe_daddr;
	// fnhe_pmtu：这是一个表示路径MTU（Path Maximum Transmission Unit）的无符号32位整数。它表示发送到这个“下一跳”的数据包的最大大小。如果fnhe_mtu_locked为真，表示这个值是用户设置的。
	u32				fnhe_pmtu;
	// fnhe_mtu_locked：表示路径MTU（fnhe_pmtu字段）是否是用户锁定的。如果为真，表示由用户或管理员设置了固定的PMTU值，而不是由允许自动处理的内核。
	bool				fnhe_mtu_locked;
	// fnhe_gw：这是一个32位无符号整数，表示下一跳网关的地址。
	__be32				fnhe_gw;
	// fnhe_expires：这是一个表示下一跳异常信息过期时间的无符号长整数。从内核启动计算。
	unsigned long			fnhe_expires;
	// fnhe_rth_input：这是一个指向Linux路由表项（rtable）的指针，用于处理这个下一跳异常的输入方向。
	struct rtable __rcu		*fnhe_rth_input;
	// fnhe_rth_output：这是一个指向Linux路由表项（rtable）的指针，用于处理这个下一跳异常的输出方向。
	struct rtable __rcu		*fnhe_rth_output;
	// fnhe_stamp：这是一个保存下一跳异常创建时间戳的无符号长整数。
	unsigned long			fnhe_stamp;
	// rcu：这是一个表示RCU（Read Copy Update）机制的数据结构，用于支持内核中的并发读取和安全更新这个数据结构。
	struct rcu_head			rcu;
};

struct fnhe_hash_bucket {
	struct fib_nh_exception __rcu	*chain;
};

#define FNHE_HASH_SHIFT		11
#define FNHE_HASH_SIZE		(1 << FNHE_HASH_SHIFT)
#define FNHE_RECLAIM_DEPTH	5

// fib_nh 的定义，用于表示 Linux 内核中的前向信息基本（Forwarding Information Base, FIB）的下一跳（Next Hop）数据结构
struct fib_nh {
	//  *nh_dev: 指向代表下一跳所在网络接口的 net_device 结构的指针。
	struct net_device	*nh_dev;
	// nh_hash: 用于将此 fib_nh 结构插入到散列表中的双链表节点。
	struct hlist_node	nh_hash;
	// *nh_parent: 指针，指向包含此下一跳的父级 fib_info 结构。
	struct fib_info		*nh_parent;
	// nh_flags: 下一跳的标志字段，用于存储如活动状态、死亡状态等的功能位。
	unsigned int		nh_flags;
	// nh_scope: 路由范围类型，表示路由的作用范围，例如全局、主机、链路等
	unsigned char		nh_scope;
//  如果定义了多路径路由功能
#ifdef CONFIG_IP_ROUTE_MULTIPATH
	// nh_weight: 权重值，用于多路径路由中，平衡不同下一跳之间的负载。
	int			nh_weight;
	// nh_upper_bound: 表示 fib_nh 结构的原子引用计数器，用于多路径路由的负载平衡。
	atomic_t		nh_upper_bound;
#endif
// 如果定义了路由分类标识功能
#ifdef CONFIG_IP_ROUTE_CLASSID
	// nh_tclassid: Traffic class identifier，用于表示路由流量的分类ID
	__u32			nh_tclassid;
#endif
	// nh_oif: 网络接口索引，用于指示输出网络接口。
	int			nh_oif;
	// nh_gw: 下一跳网关的 IP 地址（大端字节序）。
	__be32			nh_gw;
	// nh_saddr: 数据包的源 IP 地址（大端字节序）。
	__be32			nh_saddr;
	// nh_saddr_genid: 源地址的生成标识。每次重新配置路由时，这个值就会增加，以便知道老的数据包应该发送到新地址。
	int			nh_saddr_genid;
	// *nh_pcpu_rth_output: 指向一个 per-CPU 路由缓存表，其中每个 CPU 都有自己的路由缓存，用于加速输出路由查找
	struct rtable __rcu * __percpu *nh_pcpu_rth_output;
	// *nh_rth_input: 指向输入路由缓存表的指针，用于加速输入路由查找。
	struct rtable __rcu	*nh_rth_input;
	// *nh_exceptions: 指向存储特殊转发行为（如 PMTU 信息）的散列表的指针。
	struct fnhe_hash_bucket	__rcu *nh_exceptions;
	// *nh_lwtstate: 指向轻量级隧道状态（lwtunnel_state）数据结构的指针，用于保存隧道相关信息。
	struct lwtunnel_state	*nh_lwtstate;
};

/*
 * This structure contains data shared by many of routes.
 */

// fib_info的结构体，用于存储转发信息基（FIB）表中的路由条目。FIB表在内核中用于查找和存储网络路由信息
struct fib_info {
	// fib_hash: 此字段表示在基于哈希的FIB表（例如：主FIB表）中的哈希表节点。在这种数据结构中，类似的路由信息被放置在单独的哈希表项中，用于快速查找。
	struct hlist_node	fib_hash;
	// fib_lhash: 此字段在基于链表的FIB表（如局部FIB表）中表示链表节点，便于对链表执行插入和删除操作。
	struct hlist_node	fib_lhash;
	// fib_net: 结构体所属的网络命名空间。
	struct net		*fib_net;
	// fib_treeref: 用于计算这个路由表项被引用次数。
	int			fib_treeref;
	// fib_clntref: 用于计算此路由条目的客户端引用数。
	refcount_t		fib_clntref;
	// fib_flags: 表示特定于FIB的各个标志，如通知、重定向等。
	unsigned int		fib_flags;
	// fib_dead: 表示路由的状态，如活动、死亡等。
	unsigned char		fib_dead;
	// fib_protocol: 表示路由的协议类型，如RTPROT_KERNEL、RTPROT_BOOT等。
	unsigned char		fib_protocol;
	// fib_scope: 表示路由的作用范围，如全局、主机、链路等。
	unsigned char		fib_scope;
	// fib_type: 表示路由条目的类型（例如：unicast，multicast，anycast等）。
	unsigned char		fib_type;
	// fib_prefsrc: 表示路由的首选源地址。
	__be32			fib_prefsrc;
	// fib_tb_id: FIB表的唯一ID，又称为表ID。
	u32			fib_tb_id;
	// fib_priority: 表示路由的优先级。
	u32			fib_priority;
	// fib_metrics: 指向路由度量结构体的指针，用于存储路由的度量信息。
	struct dst_metrics	*fib_metrics;
#define fib_mtu fib_metrics->metrics[RTAX_MTU-1]
#define fib_window fib_metrics->metrics[RTAX_WINDOW-1]
#define fib_rtt fib_metrics->metrics[RTAX_RTT-1]
#define fib_advmss fib_metrics->metrics[RTAX_ADVMSS-1]
	// fib_nhs: 表示路由的下一跳数目。
	int			fib_nhs;
	struct rcu_head		rcu;
	// fib_nh: 指向下一跳信息的指针，用于存储路由的下一跳信息。
	struct fib_nh		fib_nh[0];
	// fib_dev: 表示路由的输出网络接口。
#define fib_dev		fib_nh[0].nh_dev
};


#ifdef CONFIG_IP_MULTIPLE_TABLES
struct fib_rule;
#endif

struct fib_table;
// 用于表示一个存储在FIB（转发信息基础设施 Forwarding Information Base）表中的路由结果
struct fib_result {
	// prefix;: 这是一个转发的IP前缀
	__be32		prefix;
	// 它指定了IP地址之前对应的子网掩码位数。
	unsigned char	prefixlen;
	// 下一跳的数量 
	unsigned char	nh_sel;
	// type: 决定了处理数据包的方式
	unsigned char	type;
	// scope: 表示路由的作用范围，如全局、主机、链路等。
	unsigned char	scope;
	// 表示流量类的ID。tclassid用于分类和选择符合特定路由决策的数据包。
	u32		tclassid;
	// 表示与此fib_result关联的额外信息，如：度量、策略等。
	struct fib_info *fi;
	// 表示此路由结果所属的FIB表。Linux内核可以管理多个FIB表，以支持不同的策略或应用。
	struct fib_table *table;
	// 用于表示一个FIB Alias列表的头结点，FIB Alias可能包含针对给定目标的多个路由（如ECMP - 等价路径多路复用）。
	struct hlist_head *fa_head;
};

struct fib_result_nl {
	__be32		fl_addr;   /* To be looked up*/
	u32		fl_mark;
	unsigned char	fl_tos;
	unsigned char   fl_scope;
	unsigned char   tb_id_in;

	unsigned char   tb_id;      /* Results */
	unsigned char	prefixlen;
	unsigned char	nh_sel;
	unsigned char	type;
	unsigned char	scope;
	int             err;
};

#ifdef CONFIG_IP_ROUTE_MULTIPATH
#define FIB_RES_NH(res)		((res).fi->fib_nh[(res).nh_sel])
#else /* CONFIG_IP_ROUTE_MULTIPATH */
#define FIB_RES_NH(res)		((res).fi->fib_nh[0])
#endif /* CONFIG_IP_ROUTE_MULTIPATH */

#ifdef CONFIG_IP_MULTIPLE_TABLES
#define FIB_TABLE_HASHSZ 256
#else
#define FIB_TABLE_HASHSZ 2
#endif

__be32 fib_info_update_nh_saddr(struct net *net, struct fib_nh *nh);

#define FIB_RES_SADDR(net, res)				\
	((FIB_RES_NH(res).nh_saddr_genid ==		\
	  atomic_read(&(net)->ipv4.dev_addr_genid)) ?	\
	 FIB_RES_NH(res).nh_saddr :			\
	 fib_info_update_nh_saddr((net), &FIB_RES_NH(res)))
#define FIB_RES_GW(res)			(FIB_RES_NH(res).nh_gw)
#define FIB_RES_DEV(res)		(FIB_RES_NH(res).nh_dev)
#define FIB_RES_OIF(res)		(FIB_RES_NH(res).nh_oif)

#define FIB_RES_PREFSRC(net, res)	((res).fi->fib_prefsrc ? : \
					 FIB_RES_SADDR(net, res))

struct fib_entry_notifier_info {
	struct fib_notifier_info info; /* must be first */
	u32 dst;
	int dst_len;
	struct fib_info *fi;
	u8 tos;
	u8 type;
	u32 tb_id;
};

struct fib_nh_notifier_info {
	struct fib_notifier_info info; /* must be first */
	struct fib_nh *fib_nh;
};

int call_fib4_notifier(struct notifier_block *nb, struct net *net,
		       enum fib_event_type event_type,
		       struct fib_notifier_info *info);
int call_fib4_notifiers(struct net *net, enum fib_event_type event_type,
			struct fib_notifier_info *info);

int __net_init fib4_notifier_init(struct net *net);
void __net_exit fib4_notifier_exit(struct net *net);

void fib_notify(struct net *net, struct notifier_block *nb);

// fib_table的结构体，用于Linux内核中的IP路由子系统。在内核中，Forwarding Information Base (FIB) 表用于存储路由信息。这个结构体定义了FIB表的基本属性
struct fib_table {
	// tb_hlist; - 这是一个哈希表节点，用于将此结构插入到哈希表中。哈希表用于加速通过ID查找fib_table的过程。
	struct hlist_node	tb_hlist;
	// tb_id; - 表示FIB表的ID，通常一个关联到特定网络namespace的表会有自己的ID
	u32			tb_id;
	// tb_num_default; - 表示存储在此FIB表中的默认路由条目的数量。默认路由是当没有明确匹配到目标IP的路由时使用的路由。
	int			tb_num_default;
	// rcu; - 是一个RCU (Read-Copy-Update) 的头结构，它允许在高度并发的环境下对数据结构进行无锁读。RCU 提供了grace period（优雅周期）机制，确保没有读者读取将要被删除的数据。
	struct rcu_head		rcu;
	// *tb_data; - FIB表中存储的实际数据的指针。这个指针指向一个长度可变的数组，用于存储路由信息。
	unsigned long 		*tb_data;
	// __data[0]; - 是一个存储在结构体内部的长度可变的数组，它允许动态地分配所需的存储空间。这种结构允许在分配结构体时直接分配存储实际数据所需的内存，而无需分配额外的内存空间。
	unsigned long		__data[0];
};

struct fib_dump_filter {
	u32			table_id;
	/* filter_set is an optimization that an entry is set */
	bool			filter_set;
	bool			dump_all_families;
	unsigned char		protocol;
	unsigned char		rt_type;
	unsigned int		flags;
	struct net_device	*dev;
};

int fib_table_lookup(struct fib_table *tb, const struct flowi4 *flp,
		     struct fib_result *res, int fib_flags);
int fib_table_insert(struct net *, struct fib_table *, struct fib_config *,
		     struct netlink_ext_ack *extack);
int fib_table_delete(struct net *, struct fib_table *, struct fib_config *,
		     struct netlink_ext_ack *extack);
int fib_table_dump(struct fib_table *table, struct sk_buff *skb,
		   struct netlink_callback *cb, struct fib_dump_filter *filter);
int fib_table_flush(struct net *net, struct fib_table *table);
struct fib_table *fib_trie_unmerge(struct fib_table *main_tb);
void fib_table_flush_external(struct fib_table *table);
void fib_free_table(struct fib_table *tb);

#ifndef CONFIG_IP_MULTIPLE_TABLES

#define TABLE_LOCAL_INDEX	(RT_TABLE_LOCAL & (FIB_TABLE_HASHSZ - 1))
#define TABLE_MAIN_INDEX	(RT_TABLE_MAIN  & (FIB_TABLE_HASHSZ - 1))

static inline struct fib_table *fib_get_table(struct net *net, u32 id)
{
	struct hlist_node *tb_hlist;
	struct hlist_head *ptr;

	ptr = id == RT_TABLE_LOCAL ?
		&net->ipv4.fib_table_hash[TABLE_LOCAL_INDEX] :
		&net->ipv4.fib_table_hash[TABLE_MAIN_INDEX];

	tb_hlist = rcu_dereference_rtnl(hlist_first_rcu(ptr));

	return hlist_entry(tb_hlist, struct fib_table, tb_hlist);
}

static inline struct fib_table *fib_new_table(struct net *net, u32 id)
{
	return fib_get_table(net, id);
}

static inline int fib_lookup(struct net *net, const struct flowi4 *flp,
			     struct fib_result *res, unsigned int flags)
{
	struct fib_table *tb;
	int err = -ENETUNREACH;

	rcu_read_lock();

	tb = fib_get_table(net, RT_TABLE_MAIN);
	if (tb)
		err = fib_table_lookup(tb, flp, res, flags | FIB_LOOKUP_NOREF);

	if (err == -EAGAIN)
		err = -ENETUNREACH;

	rcu_read_unlock();

	return err;
}

static inline bool fib4_rule_default(const struct fib_rule *rule)
{
	return true;
}

static inline int fib4_rules_dump(struct net *net, struct notifier_block *nb)
{
	return 0;
}

static inline unsigned int fib4_rules_seq_read(struct net *net)
{
	return 0;
}

static inline bool fib4_rules_early_flow_dissect(struct net *net,
						 struct sk_buff *skb,
						 struct flowi4 *fl4,
						 struct flow_keys *flkeys)
{
	return false;
}
#else /* CONFIG_IP_MULTIPLE_TABLES */
int __net_init fib4_rules_init(struct net *net);
void __net_exit fib4_rules_exit(struct net *net);

struct fib_table *fib_new_table(struct net *net, u32 id);
struct fib_table *fib_get_table(struct net *net, u32 id);

int __fib_lookup(struct net *net, struct flowi4 *flp,
		 struct fib_result *res, unsigned int flags);

static inline int fib_lookup(struct net *net, struct flowi4 *flp,
			     struct fib_result *res, unsigned int flags)
{
	struct fib_table *tb;
	int err = -ENETUNREACH;

	flags |= FIB_LOOKUP_NOREF;
	if (net->ipv4.fib_has_custom_rules)
		return __fib_lookup(net, flp, res, flags);

	rcu_read_lock();

	res->tclassid = 0;

	tb = rcu_dereference_rtnl(net->ipv4.fib_main);
	if (tb)
		err = fib_table_lookup(tb, flp, res, flags);

	if (!err)
		goto out;

	tb = rcu_dereference_rtnl(net->ipv4.fib_default);
	if (tb)
		err = fib_table_lookup(tb, flp, res, flags);

out:
	if (err == -EAGAIN)
		err = -ENETUNREACH;

	rcu_read_unlock();

	return err;
}

bool fib4_rule_default(const struct fib_rule *rule);
int fib4_rules_dump(struct net *net, struct notifier_block *nb);
unsigned int fib4_rules_seq_read(struct net *net);

static inline bool fib4_rules_early_flow_dissect(struct net *net,
						 struct sk_buff *skb,
						 struct flowi4 *fl4,
						 struct flow_keys *flkeys)
{
	unsigned int flag = FLOW_DISSECTOR_F_STOP_AT_ENCAP;

	if (!net->ipv4.fib_rules_require_fldissect)
		return false;

	skb_flow_dissect_flow_keys(skb, flkeys, flag);
	fl4->fl4_sport = flkeys->ports.src;
	fl4->fl4_dport = flkeys->ports.dst;
	fl4->flowi4_proto = flkeys->basic.ip_proto;

	return true;
}

#endif /* CONFIG_IP_MULTIPLE_TABLES */

/* Exported by fib_frontend.c */
extern const struct nla_policy rtm_ipv4_policy[];
void ip_fib_init(void);
__be32 fib_compute_spec_dst(struct sk_buff *skb);
bool fib_info_nh_uses_dev(struct fib_info *fi, const struct net_device *dev);
int fib_validate_source(struct sk_buff *skb, __be32 src, __be32 dst,
			u8 tos, int oif, struct net_device *dev,
			struct in_device *idev, u32 *itag);
#ifdef CONFIG_IP_ROUTE_CLASSID
static inline int fib_num_tclassid_users(struct net *net)
{
	return net->ipv4.fib_num_tclassid_users;
}
#else
static inline int fib_num_tclassid_users(struct net *net)
{
	return 0;
}
#endif
int fib_unmerge(struct net *net);

/* Exported by fib_semantics.c */
int ip_fib_check_default(__be32 gw, struct net_device *dev);
int fib_sync_down_dev(struct net_device *dev, unsigned long event, bool force);
int fib_sync_down_addr(struct net_device *dev, __be32 local);
int fib_sync_up(struct net_device *dev, unsigned int nh_flags);
void fib_sync_mtu(struct net_device *dev, u32 orig_mtu);

#ifdef CONFIG_IP_ROUTE_MULTIPATH
int fib_multipath_hash(const struct net *net, const struct flowi4 *fl4,
		       const struct sk_buff *skb, struct flow_keys *flkeys);
#endif
void fib_select_multipath(struct fib_result *res, int hash);
void fib_select_path(struct net *net, struct fib_result *res,
		     struct flowi4 *fl4, const struct sk_buff *skb);

/* Exported by fib_trie.c */
void fib_trie_init(void);
struct fib_table *fib_trie_table(u32 id, struct fib_table *alias);

static inline void fib_combine_itag(u32 *itag, const struct fib_result *res)
{
#ifdef CONFIG_IP_ROUTE_CLASSID
#ifdef CONFIG_IP_MULTIPLE_TABLES
	u32 rtag;
#endif
	*itag = FIB_RES_NH(*res).nh_tclassid<<16;
#ifdef CONFIG_IP_MULTIPLE_TABLES
	rtag = res->tclassid;
	if (*itag == 0)
		*itag = (rtag<<16);
	*itag |= (rtag>>16);
#endif
#endif
}

void free_fib_info(struct fib_info *fi);

static inline void fib_info_hold(struct fib_info *fi)
{
	refcount_inc(&fi->fib_clntref);
}

static inline void fib_info_put(struct fib_info *fi)
{
	if (refcount_dec_and_test(&fi->fib_clntref))
		free_fib_info(fi);
}

#ifdef CONFIG_PROC_FS
int __net_init fib_proc_init(struct net *net);
void __net_exit fib_proc_exit(struct net *net);
#else
static inline int fib_proc_init(struct net *net)
{
	return 0;
}
static inline void fib_proc_exit(struct net *net)
{
}
#endif

u32 ip_mtu_from_fib_result(struct fib_result *res, __be32 daddr);

int ip_valid_fib_dump_req(struct net *net, const struct nlmsghdr *nlh,
			  struct fib_dump_filter *filter,
			  struct netlink_callback *cb);
#endif  /* _NET_FIB_H */
