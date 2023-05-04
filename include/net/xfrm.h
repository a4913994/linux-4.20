/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _NET_XFRM_H
#define _NET_XFRM_H

#include <linux/compiler.h>
#include <linux/xfrm.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/pfkeyv2.h>
#include <linux/ipsec.h>
#include <linux/in6.h>
#include <linux/mutex.h>
#include <linux/audit.h>
#include <linux/slab.h>
#include <linux/refcount.h>

#include <net/sock.h>
#include <net/dst.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/ipv6.h>
#include <net/ip6_fib.h>
#include <net/flow.h>
#include <net/gro_cells.h>

#include <linux/interrupt.h>

#ifdef CONFIG_XFRM_STATISTICS
#include <net/snmp.h>
#endif
// 用于在Linux内核中指定不同类型的IPsec协议（XFRM_PROTO）
// XFRM_PROTO_ESP：ESP是一种IPsec协议，用于提供加密和身份验证机制，可以保护数据的机密性和完整性。ESP被定义为50，因此该宏为XFRM_PROTO_ESP。
#define XFRM_PROTO_ESP		50
// XFRM_PROTO_AH：AH是另一种IPsec协议，提供数据完整性和身份验证机制。AH被定义为51，因此该宏为XFRM_PROTO_AH。
#define XFRM_PROTO_AH		51
// XFRM_PROTO_COMP：COMP是一种IPsec协议，它提供数据压缩机制，可以在数据传输过程中减少它们的大小。COMP被定义为108，因此该宏为XFRM_PROTO_COMP。
#define XFRM_PROTO_COMP		108
// XFRM_PROTO_IPIP：IPIP（IP-in-IP）是通过在IP数据包中封装它们来实现运输，它允许通过Internet连接两个相同或不同的通信节点。IPIP被定义为4，因此该宏为XFRM_PROTO_IPIP。
#define XFRM_PROTO_IPIP		4
// XFRM_PROTO_IPV6：IPv6是一个互联网协议标准，用于在IPv6网络中传输数据。IPv6被定义为41，因此该宏为XFRM_PROTO_IPV6。
#define XFRM_PROTO_IPV6		41
// XFRM_PROTO_ROUTING：ROUTING是一种协议，它允许路由器通过直接路由选择目标地址，而不是通过转发数据包。ROUTING使用网关而不是邮递员异地工作。该宏等于IPPROTO_ROUTING。
#define XFRM_PROTO_ROUTING	IPPROTO_ROUTING
// XFRM_PROTO_DSTOPTS：DSTOPTS是一种协议，它允许数据包在传输过程中传递特定于目的地的处理选项。该宏等于IPPROTO_DSTOPTS。
#define XFRM_PROTO_DSTOPTS	IPPROTO_DSTOPTS

#define XFRM_ALIGN4(len)	(((len) + 3) & ~3)
#define XFRM_ALIGN8(len)	(((len) + 7) & ~7)
#define MODULE_ALIAS_XFRM_MODE(family, encap) \
	MODULE_ALIAS("xfrm-mode-" __stringify(family) "-" __stringify(encap))
#define MODULE_ALIAS_XFRM_TYPE(family, proto) \
	MODULE_ALIAS("xfrm-type-" __stringify(family) "-" __stringify(proto))
#define MODULE_ALIAS_XFRM_OFFLOAD_TYPE(family, proto) \
	MODULE_ALIAS("xfrm-offload-" __stringify(family) "-" __stringify(proto))

#ifdef CONFIG_XFRM_STATISTICS
#define XFRM_INC_STATS(net, field)	SNMP_INC_STATS((net)->mib.xfrm_statistics, field)
#else
#define XFRM_INC_STATS(net, field)	((void)(net))
#endif


/* Organization of SPD aka "XFRM rules"
   SPD（Security Policy Database，安全策略数据库）的组织架构 “XFRM规则”
   ------------------------------------

   Basic objects:
   基本对象:
   - policy rule, struct xfrm_policy (=SPD entry) // 策略规则，结构体 xfrm_policy（= SPD 条目）
   - bundle of transformations, struct dst_entry == struct xfrm_dst (=SA bundle) // 变换的绑定，结构体 dst_entry == struct xfrm_dst （= 安全关联绑定）
   - instance of a transformer, struct xfrm_state (=SA) // 变换的实例，结构体 xfrm_state（= 安全关联）
   - template to clone xfrm_state, struct xfrm_tmpl // 克隆 xfrm_state 的模板，结构体 xfrm_tmpl

   SPD is plain linear list of xfrm_policy rules, ordered by priority. 
   (To be compatible with existing pfkeyv2 implementations,
   many rules with priority of 0x7fffffff are allowed to exist and
   SPD 是 xfrm_policy 规则的纯线性列表，按优先级排序。（为了兼容现有的 pfkeyv2 实现，允许存在许多优先级为 0x7fffffff 的规则且这些规则是以不可预测的方式排序的，
   感谢 BSD 的开发人员。） 
   查找只需执行下列纯线性搜索操作，直到匹配选择器为止。

	如果“操作”为“阻止”，则禁止流量，否则：
	如果“xfrms_nr”为零，则经过的流量未经变换。否则，
	策略条目具有多达 XFRM_MAX_DEPTH 个变换的列表，
	其中每个模板由 xfrm_tmpl 描述。每个模板都解析为完整的 xfrm_state（请参见下文），
	然后我们将变换的绑定打包到返回请求方的 dst_entry 中。

   Lookup is plain linear search until the first match with selector.

   If "action" is "block", then we prohibit the flow, otherwise:
   if "xfrms_nr" is zero, the flow passes untransformed. Otherwise,
   policy entry has list of up to XFRM_MAX_DEPTH transformations,
   described by templates xfrm_tmpl. Each template is resolved
   to a complete xfrm_state (see below) and we pack bundle of transformations
   to a dst_entry returned to requestor.

   dst -. xfrm  .-> xfrm_state #1
    |---. child .-> dst -. xfrm .-> xfrm_state #2
                     |---. child .-> dst -. xfrm .-> xfrm_state #3
                                      |---. child .-> NULL

   Bundles are cached at xrfm_policy struct (field ->bundles).


   Resolution of xrfm_tmpl
   -----------------------
   Template contains:
   模板包括以下内容：
   1. ->mode		Mode: transport or tunnel
   2. ->id.proto	Protocol: AH/ESP/IPCOMP
   3. ->id.daddr	Remote tunnel endpoint, ignored for transport mode.
      Q: allow to resolve security gateway?
   4. ->id.spi          If not zero, static SPI.
   5. ->saddr		Local tunnel endpoint, ignored for transport mode.
   6. ->algos		List of allowed algos. Plain bitmask now.
      Q: ealgos, aalgos, calgos. What a mess...
   7. ->share		Sharing mode.
      Q: how to implement private sharing mode? To add struct sock* to
      flow id?

->mode 模式: 传输或隧道
->id.proto 协议: AH/ESP/IPCOMP
->id.daddr 远程隧道终端点，在传输模式下忽略。
Q: 允许解析安全网关吗？
->id.spi 如果不为零，则是静态 SPI。
->saddr 本地隧道终端点，在传输模式下忽略。
->algos 允许算法的列表。现在是普通的位掩码。
Q: ealgos、aalgos、calgos。这是一团糟……
->share 共享模式。
Q: 如何实现私有共享模式？将struct sock* 添加到流 ID 中？

   Having this template we search through SAD searching for entries
   with appropriate mode/proto/algo, permitted by selector.
   If no appropriate entry found, it is requested from key manager.
   有了这个模板，我们会在 SAD 中查找适当的 mode/proto/algo 条目，并获得选择器允许的权限。如果没有找到适当的条目，则从密钥管理器请求。

   PROBLEMS:
   Q: How to find all the bundles referring to a physical path for
      PMTU discovery? Seems, dst should contain list of all parents...
      and enter to infinite locking hierarchy disaster.
      No! It is easier, we will not search for them, let them find us.
      We add genid to each dst plus pointer to genid of raw IP route,
      pmtu disc will update pmtu on raw IP route and increase its genid.
      dst_check() will see this for top level and trigger resyncing
      metrics. Plus, it will be made via sk->sk_dst_cache. Solved.

	Q: 如何找到所有引用物理路径的 bunlde，以便进行 PMTU 发现？似乎，dst 应包含所有父项的列表...并进入无限的锁层次结构灾难。不，这很容易，我们不会寻找它们，让它们找到我们。我们为每个 dst 添加 genid，以及指向原始 IP 路由的 genid 指针，PMTU 发现将更新原始 IP 路由上的 PMTU 并增加其 genid。dst_check() 将在顶层看到这一点，并触发重新同步指标。此外，这将通过 sk->sk_dst_cache 完成。问题解决。
 */

// 用于在 Linux 内核中处理遍历（walk）由 xfrm_state 结构体组成的链表，该结构体用于表示 IPsec 的安全策略。
// struct list_head成员用于维护链表，state 、dying、proto 和 seq 成员分别用于描述该链表节点的状态，协议类型，序列号等信息。
// filter 成员是一个指向地址过滤器的指针，用于筛选符合条件的节点。
struct xfrm_state_walk {
	// 表示该数据结构的链表头
	struct list_head	all;
	// 用于表示安全关联对象（Security Association，SA）的状态。
	u8			state;
	// 用于表示该 SA 是否处于 “dying” 状态（即已被删除）。
	u8			dying;
	// 用于表示 SA 的协议类型
	u8			proto;
	// 用于表示该 SA 的序列号
	u32			seq;
	// 指向地址过滤器（address filter）的指针
	struct xfrm_address_filter *filter;
};

// xfrm_state_offload: 于描述 IPsec 安全关联对象（SA）的卸载状态（offload state）
// dev 成员指定了要卸载的网络设备，
// offload_handle 用于标识该卸载状态，可以用于后续的卸载操作或查询。
// num_exthdrs 指示该 SA 需要处理的扩展报头数。扩展报头是在 IP 报文中携带额外信息的报头，如扩展首部、分片首部等。flags 表示卸载状态，可能包括以下标志位：
// - XFRM_STATE_OFFLOAD_NOPAD: 不进行填充（padding）
// - XFRM_STATE_OFFLOAD_DEAD: 下层设备已经不支持卸载状态
// - XFRM_STATE_OFFLOAD_WILDRECV: 接收方使用了泛接收（wildcard reception），接收了本不应该接收的 SA 包。
struct xfrm_state_offload {
	// 表示要卸载的网络设备
	struct net_device	*dev;
	// 表示卸载句柄（offload handle），用于标识该卸载状态
	unsigned long		offload_handle;
	// 表示 SA 需要处理的扩展报头（extended header）数
	unsigned int		num_exthdrs;
	// 表示卸载状态的标志位
	u8			flags;
};

/* Full description of state of transformer. */
// 用于描述一个 IPsec 安全关联对象（SA）的状态（state）
struct xfrm_state {
	// 该 SA 归属的网络命名空间
	possible_net_t		xs_net;
	union {
		// 全局链表节点，用于维护所有 SA 的全局链表
		struct hlist_node	gclist;
		// 用于维护以目的地址为键值的哈希表，以加快根据目的地址查找 SA 的速度
		struct hlist_node	bydst;
	};
	// 用于维护以源地址为键值的哈希表
	struct hlist_node	bysrc;
	// 用于维护以安全关联标识符（SPI）为键值的哈希表
	struct hlist_node	byspi;
	// 用于记录该 SA 的引用计数，避免出现内存泄漏
	refcount_t		refcnt;
	// 用于保护该 SA 的修改操作，避免出现竞态条件
	spinlock_t		lock;

	// 表示该 SA 对应的 IPsec ID
	struct xfrm_id		id;
	// 表示该 SA 对应的 IPsec 选择器
	struct xfrm_selector	sel;
	// 表示该 SA 对应的网络数据包标记
	struct xfrm_mark	mark;
	// 表示关联的网络接口标识符
	u32			if_id;
	// 表示填充长度，一般为 0
	u32			tfcpad;
	// 表示 SA 的生成标识符
	u32			genid;

	/* Key manager bits */
	// SA 模块用于管理 SA 的数据结构
	struct xfrm_state_walk	km;

	/* Parameters of this state. */
	// SA 的参数
	struct {
		// 该 SA 对应的请求标识符
		u32		reqid;
		//  SA 的加密模式（encryption mode）
		u8		mode;
		// 重放攻击检测窗口大小
		u8		replay_window;
		// SA 的认证算法（authentication algorithm）、加密算法（encryption algorithm）和压缩算法（compression algorithm）。
		u8		aalgo, ealgo, calgo;
		// SA 的标志位
		u8		flags;
		//  SA 对应的协议族（IPv4 或 IPv6）
		u16		family;
		// 源 IP 地址
		xfrm_address_t	saddr;
		// 头部长度
		int		header_len;
		// 尾部长度
		int		trailer_len;
		// SA 的额外标志量
		u32		extra_flags;
		// SA 相关的标记信息 
		struct xfrm_mark	smark;
	} props;

	// SA 对应的生命周期
	struct xfrm_lifetime_cfg lft;

	/* Data for transformer */
	// SA 对应的认证算法、加密算法、压缩算法、AEAD 算法、以及初始向量（Initialization Vector）。
	struct xfrm_algo_auth	*aalg;
	struct xfrm_algo	*ealg;
	struct xfrm_algo	*calg;
	struct xfrm_algo_aead	*aead;
	const char		*geniv;

	/* Data for encapsulator */
	// SA 对应的封装（encapsulation）信息
	struct xfrm_encap_tmpl	*encap;

	/* Data for care-of address */
	// SA 对应的地址
	xfrm_address_t	*coaddr;

	/* IPComp needs an IPIP tunnel for handling uncompressed packets */
	// SA 对应的隧道（tunnel）信息
	struct xfrm_state	*tunnel;

	/* If a tunnel, number of users + 1 */
	// 使用该 SA 的隧道数量
	atomic_t		tunnel_users;

	/* State for replay detection */
	// SA 的重放攻击检测相关信息
	struct xfrm_replay_state replay;
	struct xfrm_replay_state_esn *replay_esn;

	/* Replay detection state at the time we sent the last notification */
	struct xfrm_replay_state preplay;
	struct xfrm_replay_state_esn *preplay_esn;

	/* The functions for replay detection. */
	// 
	const struct xfrm_replay *repl;

	/* internal flag that only holds state for delayed aevent at the
	 * moment
	*/
	// xfrm_state 内部标记，仅用于保存延迟事件相关状态
	u32			xflags;

	/* Replay detection notification settings */
	// replay_maxage、replay_maxdiff、rtimer：表示重放攻击检测相关的最大时限和定时器
	u32			replay_maxage;
	u32			replay_maxdiff;

	/* Replay detection notification timer */
	struct timer_list	rtimer;

	/* Statistics */
	// stats：表示 SA 的统计信息。
	struct xfrm_stats	stats;
	// curlft、mtimer、xso：表示 SA 对应的生命周期、定时器等与时间相关的信息。
	struct xfrm_lifetime_cur curlft;
	struct tasklet_hrtimer	mtimer;

	struct xfrm_state_offload xso;

	/* used to fix curlft->add_time when changing date */
	// saved_tmo：表示上一次使用时间到当前时间的时间差。
	long		saved_tmo;

	/* Last used time */
	// lastused：表示上一次使用该 SA 的时间。
	time64_t		lastused;
	// xfrag：表示用于重组 IP 分片后的数据包的页面片段（Page Fragment）。
	struct page_frag xfrag;

	/* Reference to data common to all the instances of this
	 * transformer. */
	// type、inner_mode、inner_mode_iaf、outer_mode、type_offload：表示 SA 的类型、内部模式、IAF 内部模式、外部模式、以及类型的卸载状态。
	const struct xfrm_type	*type;
	struct xfrm_mode	*inner_mode;
	struct xfrm_mode	*inner_mode_iaf;
	struct xfrm_mode	*outer_mode;

	const struct xfrm_type_offload	*type_offload;

	/* Security context */
	// security：表示 SA 相关的安全上下文。
	struct xfrm_sec_ctx	*security;

	/* Private data of this transformer, format is opaque,
	 * interpreted by xfrm_type methods. */
	// data：表示 SA 相关的私有数据，由对应的 SA 类型方法解释。
	void			*data;
};

static inline struct net *xs_net(struct xfrm_state *x)
{
	return read_pnet(&x->xs_net);
}

/* xflags - make enum if more show up */
// XFRM_TIME_DEFER：表示该 SA 的时间已经延迟。
#define XFRM_TIME_DEFER	1
// XFRM_SOFT_EXPIRE：表示该 SA 已经过期，但还可以延迟时间。
#define XFRM_SOFT_EXPIRE 2

enum {
	// XFRM_STATE_VOID：表示 SA 处于未初始化状态。
	XFRM_STATE_VOID,
	// XFRM_STATE_ACQ：表示 SA 正在获取中。
	XFRM_STATE_ACQ,
	// XFRM_STATE_VALID：表示 SA 是有效的。
	XFRM_STATE_VALID,
	// XFRM_STATE_ERROR：表示 SA 出现错误。
	XFRM_STATE_ERROR,
	// XFRM_STATE_EXPIRED：表示 SA 已经过期。
	XFRM_STATE_EXPIRED,
	// XFRM_STATE_DEAD：表示 SA 已经失效。
	XFRM_STATE_DEAD
};

/* callback structure passed from either netlink or pfkey */
// 表示 IPsec SA Key Management related events（km_event，IPsec SA 中的密钥管理事件）。
struct km_event {
	union {
		// hard：表示 SA 强制删除的事件类型。
		u32 hard;
		// proto：表示协议类型相关的事件类型。
		u32 proto;
		// byid：表示通过 ID 相关的事件类型。
		u32 byid;
		// aevent：表示事件类型特定的附加事件。
		u32 aevent;
		// type：表示事件的类型。
		u32 type;
	} data;
	// seq：表示事件的序列号。
	u32	seq;
	// portid：表示请求该事件的进程标识符。
	u32	portid;
	// event：表示该事件的具体类型。
	u32	event;
	// net：表示该事件所在的网络命名空间。
	struct net *net;
};

//  IPsec SA 中的重放保护机制
struct xfrm_replay {
	// advance：用于通知 XFRM 状态，以便更新重放保护窗口并移除已完成的条目。
	void	(*advance)(struct xfrm_state *x, __be32 net_seq);
	// check：用于实现重放保护机制。该函数判断数据包是否为重复包，如果是则返回错误。
	int	(*check)(struct xfrm_state *x,
			 struct sk_buff *skb,
			 __be32 net_seq);
	// recheck：用于再次检查可能已经过期的重放窗口。如果该函数返回错误，则该数据包将被丢弃。
	int	(*recheck)(struct xfrm_state *x,
			   struct sk_buff *skb,
			   __be32 net_seq);
	// notify：用于通知相关 XFRM 状态。
	void	(*notify)(struct xfrm_state *x, int event);
	// overflow：用于处理在重放窗口溢出的情况下该如何处理数据包。如果该函数返回错误，则该数据包将被丢弃。
	int	(*overflow)(struct xfrm_state *x, struct sk_buff *skb);
};

struct xfrm_if_cb {
	struct xfrm_if	*(*decode_session)(struct sk_buff *skb);
};

// xfrm_if_register_cb 函数将回调函数注册到系统中，以实现从 sk_buff 解码 XFRM 会话
void xfrm_if_register_cb(const struct xfrm_if_cb *ifcb);
// xfrm_if_unregister_cb 函数用于注销该回调函数。这两个函数的作用是在 XFRM subsystem 中注册一个回调接口，用于提供一个新的数据源解析 XFRM session 的方法，以替代原本的 xfrm_if_proto。呼叫方通過xfrm_if_cb中 decode_session 类型为sk_buff指定数据源，该数据源可以是任何不同的 XFRM 协议。
void xfrm_if_unregister_cb(void);

struct net_device;
struct xfrm_type;
struct xfrm_dst;
// 用于 XFRM 策略与地址族相关信息的管理。该结构体包含了一些成员函数，用于处理不同的XFRM策略请求。
struct xfrm_policy_afinfo {
	// dst_ops 是网络层的目标地址相关操作结构体指针，指向可以用来处理网络层目标地址的操作结构体。
	struct dst_ops		*dst_ops;
	// dst_lookup 是一个用于查找网络层目标地址的函数指针，返回查询到的 dst_entry 指针。
	struct dst_entry	*(*dst_lookup)(struct net *net,
					       int tos, int oif,
					       const xfrm_address_t *saddr,
					       const xfrm_address_t *daddr,
					       u32 mark);
	// get_saddr 是一个用于获取本地源地址的函数指针，返回获取到的源地址。
	int			(*get_saddr)(struct net *net, int oif,
					     xfrm_address_t *saddr,
					     xfrm_address_t *daddr,
					     u32 mark);
	// decode_session 是一个用于解码 XFRM 会话的函数指针，将会使用 flow 信息对会话进行解码。
	void			(*decode_session)(struct sk_buff *skb,
						  struct flowi *fl,
						  int reverse);
	// get_tos 是一个用于获取服务类型的函数指针，返回获取到的服务类型（ToS）。
	int			(*get_tos)(const struct flowi *fl);
	// init_path 是一个用于初始化 XFRM 转发路径的函数指针，返回初始化的转发路径。
	int			(*init_path)(struct xfrm_dst *path,
					     struct dst_entry *dst,
					     int nfheader_len);
	// fill_dst 是一个用于填充目标地址的函数指针，用于为 afinfo 结构体用传入的 flow 填充目标地址（用于新建策略时）。
	int			(*fill_dst)(struct xfrm_dst *xdst,
					    struct net_device *dev,
					    const struct flowi *fl);
	// blackhole_route 是一个用于设置路由策略的函数指针，可以用来将原来的路由转换成一个黑洞路由策略。
	struct dst_entry	*(*blackhole_route)(struct net *net, struct dst_entry *orig);
};

int xfrm_policy_register_afinfo(const struct xfrm_policy_afinfo *afinfo, int family);
void xfrm_policy_unregister_afinfo(const struct xfrm_policy_afinfo *afinfo);
void km_policy_notify(struct xfrm_policy *xp, int dir,
		      const struct km_event *c);
void km_state_notify(struct xfrm_state *x, const struct km_event *c);

struct xfrm_tmpl;
int km_query(struct xfrm_state *x, struct xfrm_tmpl *t,
	     struct xfrm_policy *pol);
void km_state_expired(struct xfrm_state *x, int hard, u32 portid);
int __xfrm_state_delete(struct xfrm_state *x);

// 一个地址族相关的 XFRM 状态信息，用来管理不同地址族的 XFRM 状态
struct xfrm_state_afinfo {
	// family 表示该地址族的协议族，如 AF_INET、AF_INET6 等。
	unsigned int			family;
	// proto 表示该地址族协议对应的 IP 协议号，如 IPPROTO_IPV4、IPPROTO_IPV6 等。
	unsigned int			proto;
	// eth_proto 表示该地址族协议所对应的以太网类型，如 ETH_P_IP、ETH_P_IPV6 等。
	__be16				eth_proto;
	// owner 表示拥有该地址族相关信息的内核模块（若有）。
	struct module			*owner;
	// type_map[] 表示地址族所支持的 XFRM 类型映射表，该数组的下标对应着 IP 协议号，指向的值为该协议号对应的 XFRM 类型。
	const struct xfrm_type		*type_map[IPPROTO_MAX];
	// type_offload_map[] 表示地址族所支持的 XFRM 类型硬件卸载映射表，该数组的下标对应着 IP 协议号，指向的值为该协议号对应的 XFRM 类型硬件卸载的 XFRM 类型数据结构。
	const struct xfrm_type_offload	*type_offload_map[IPPROTO_MAX];
	// mode_map[] 表示地址族所支持的 XFRM 模式映射表，用于根据模式获取对应的结构体类型。
	struct xfrm_mode		*mode_map[XFRM_MODE_MAX];

	int			(*init_flags)(struct xfrm_state *x);
	void			(*init_tempsel)(struct xfrm_selector *sel,
						const struct flowi *fl);
	void			(*init_temprop)(struct xfrm_state *x,
						const struct xfrm_tmpl *tmpl,
						const xfrm_address_t *daddr,
						const xfrm_address_t *saddr);
	int			(*tmpl_sort)(struct xfrm_tmpl **dst, struct xfrm_tmpl **src, int n);
	int			(*state_sort)(struct xfrm_state **dst, struct xfrm_state **src, int n);
	int			(*output)(struct net *net, struct sock *sk, struct sk_buff *skb);
	int			(*output_finish)(struct sock *sk, struct sk_buff *skb);
	int			(*extract_input)(struct xfrm_state *x,
						 struct sk_buff *skb);
	int			(*extract_output)(struct xfrm_state *x,
						  struct sk_buff *skb);
	int			(*transport_finish)(struct sk_buff *skb,
						    int async);
	void			(*local_error)(struct sk_buff *skb, u32 mtu);
};

int xfrm_state_register_afinfo(struct xfrm_state_afinfo *afinfo);
int xfrm_state_unregister_afinfo(struct xfrm_state_afinfo *afinfo);
struct xfrm_state_afinfo *xfrm_state_get_afinfo(unsigned int family);
struct xfrm_state_afinfo *xfrm_state_afinfo_get_rcu(unsigned int family);

// xfrm_input_afinfo 的结构体，表示一个地址族中 XFRM 输入相关的信息
struct xfrm_input_afinfo {
	// family 表示该地址族的协议族，如 AF_INET、AF_INET6 等。
	unsigned int		family;
	// callback 是一个函数指针，表示处理接收到的 skb（socket buffer）数据包的回调函数。此函数会在数据包接收和处理时被调用，
	// 首先会根据协议号来区分不同的协议，然后根据需要进行解密等相关操作，最后将处理后的数据包传递给上层进行处理。
	int			(*callback)(struct sk_buff *skb, u8 protocol,
					    int err);
};

int xfrm_input_register_afinfo(const struct xfrm_input_afinfo *afinfo);
int xfrm_input_unregister_afinfo(const struct xfrm_input_afinfo *afinfo);

void xfrm_flush_gc(void);
void xfrm_state_delete_tunnel(struct xfrm_state *x);
// xfrm_type 的结构体，表示一个 XFRM 协议类型
struct xfrm_type {
	// description 描述该 XFRM 协议类型的字符串。
	char			*description;
	// owner 表示拥有该 XFRM 协议类型的内核模块（若有）。
	struct module		*owner;
	// proto 表示此 XFRM 协议类型使用的协议号。
	u8			proto;
	// flags 是一个标志位，表示该协议类型的特性，如是否支持分片、重放保护、本地地址协商、远端地址协商等。
	u8			flags;
#define XFRM_TYPE_NON_FRAGMENT	1
#define XFRM_TYPE_REPLAY_PROT	2
#define XFRM_TYPE_LOCAL_COADDR	4
#define XFRM_TYPE_REMOTE_COADDR	8

	// init_state 表示初始化该 XFRM 协议类型的状态信息。
	int			(*init_state)(struct xfrm_state *x);
	// destructor 表示析构该 XFRM 协议类型的状态信息。
	void			(*destructor)(struct xfrm_state *);
	int			(*input)(struct xfrm_state *, struct sk_buff *skb);
	int			(*output)(struct xfrm_state *, struct sk_buff *pskb);
	int			(*reject)(struct xfrm_state *, struct sk_buff *,
					  const struct flowi *);
	// hdr_offset 表示计算数据包中 XFRM 协议头偏移量的回调函数。
	int			(*hdr_offset)(struct xfrm_state *, struct sk_buff *, u8 **);
	/* Estimate maximal size of result of transformation of a dgram */
	u32			(*get_mtu)(struct xfrm_state *, int size);
};

int xfrm_register_type(const struct xfrm_type *type, unsigned short family);
int xfrm_unregister_type(const struct xfrm_type *type, unsigned short family);

// 一个 XFRM 协议类型的硬件卸载相关信息
struct xfrm_type_offload {
	// description 描述该 XFRM 协议类型的字符串。
	char		*description;
	// owner 表示拥有该 XFRM 协议类型的内核模块（若有）。
	struct module	*owner;
	// proto 表示此 XFRM 协议类型使用的协议号。
	u8		proto;
	// encap 是一个函数指针，表示硬件加速的封装操作。
	void		(*encap)(struct xfrm_state *, struct sk_buff *pskb);
	// input_tail 是一个函数指针，表示硬件卸载的接收操作。
	int		(*input_tail)(struct xfrm_state *x, struct sk_buff *skb);
	// xmit 是一个函数指针，表示硬件加速的发送操作。其中 features 表示 netdev_features_t 类型的标志位，用于指定网络特性（如硬件校验和、TSO 等）。
	int		(*xmit)(struct xfrm_state *, struct sk_buff *pskb, netdev_features_t features);
};

int xfrm_register_type_offload(const struct xfrm_type_offload *type, unsigned short family);
int xfrm_unregister_type_offload(const struct xfrm_type_offload *type, unsigned short family);

// xfrm_mode 的结构体，表示 IPsec 的模式信息
struct xfrm_mode {
	/*
	 * Remove encapsulation header.
	 *
	 * The IP header will be moved over the top of the encapsulation
	 * header.
	 *
	 * On entry, the transport header shall point to where the IP header
	 * should be and the network header shall be set to where the IP
	 * header currently is.  skb->data shall point to the start of the
	 * payload.
	 */
	// input2 表示解封装操作。从报文中移除封装头，并将报文的 IP 头向上移动到封装层的顶部。
	int (*input2)(struct xfrm_state *x, struct sk_buff *skb);

	/*
	 * This is the actual input entry point.
	 *
	 * For transport mode and equivalent this would be identical to
	 * input2 (which does not need to be set).  While tunnel mode
	 * and equivalent would set this to the tunnel encapsulation function
	 * xfrm4_prepare_input that would in turn call input2.
	 */
	// input 表示实际的输入操作。在传输模式下，该函数指针与 input2 相同，无需设置；
	// 而在隧道模式下，应将其设置为隧道封装函数（如 xfrm4_prepare_input），该函数会调用 input2。
	int (*input)(struct xfrm_state *x, struct sk_buff *skb);

	/*
	 * Add encapsulation header.
	 *
	 * On exit, the transport header will be set to the start of the
	 * encapsulation header to be filled in by x->type->output and
	 * the mac header will be set to the nextheader (protocol for
	 * IPv4) field of the extension header directly preceding the
	 * encapsulation header, or in its absence, that of the top IP
	 * header.  The value of the network header will always point
	 * to the top IP header while skb->data will point to the payload.
	 */
	// output2 表示封装操作。将报文封装到一个新的封装头中，并将报文 IP 头指针指向新的封装头
	int (*output2)(struct xfrm_state *x,struct sk_buff *skb);

	/*
	 * This is the actual output entry point.
	 *
	 * For transport mode and equivalent this would be identical to
	 * output2 (which does not need to be set).  While tunnel mode
	 * and equivalent would set this to a tunnel encapsulation function
	 * (xfrm4_prepare_output or xfrm6_prepare_output) that would in turn
	 * call output2.
	 */
	// output 表示实际的输出操作。在传输模式下，该函数指针与 output2 相同，无需设置；
	// 而在隧道模式下，应将其设置为隧道封装函数（如 xfrm4_prepare_output 或 xfrm6_prepare_output），该函数会调用 output2 。
	int (*output)(struct xfrm_state *x, struct sk_buff *skb);

	/*
	 * Adjust pointers into the packet and do GSO segmentation.
	 */
	// gso_segment 用于调整报文指针并进行 GSO 分段。
	struct sk_buff *(*gso_segment)(struct xfrm_state *x, struct sk_buff *skb, netdev_features_t features);

	/*
	 * Adjust pointers into the packet when IPsec is done at layer2.
	 */
	// xmit 用于调整 L2 协议头和一些其他的访问指针。
	void (*xmit)(struct xfrm_state *x, struct sk_buff *skb);

	// afinfo 表示 IPsec 支持的协议族相关信息。
	struct xfrm_state_afinfo *afinfo;
	// owner 表示拥有该 IPsec 模式的内核模块（若有）
	struct module *owner;
	// encap 表示封装方式。
	unsigned int encap;
	// flags 定义一个标志变量。
	int flags;
};

/* Flags for xfrm_mode. */
enum {
	XFRM_MODE_FLAG_TUNNEL = 1,
};

int xfrm_register_mode(struct xfrm_mode *mode, int family);
int xfrm_unregister_mode(struct xfrm_mode *mode, int family);

static inline int xfrm_af2proto(unsigned int family)
{
	switch(family) {
	case AF_INET:
		return IPPROTO_IPIP;
	case AF_INET6:
		return IPPROTO_IPV6;
	default:
		return 0;
	}
}

static inline struct xfrm_mode *xfrm_ip2inner_mode(struct xfrm_state *x, int ipproto)
{
	if ((ipproto == IPPROTO_IPIP && x->props.family == AF_INET) ||
	    (ipproto == IPPROTO_IPV6 && x->props.family == AF_INET6))
		return x->inner_mode;
	else
		return x->inner_mode_iaf;
}

// 表示一个 XFRM SA 的模板信息
struct xfrm_tmpl {
/* id in template is interpreted as:
 * daddr - destination of tunnel, may be zero for transport mode.
 * spi   - zero to acquire spi. Not zero if spi is static, then
 *	   daddr must be fixed too.
 * proto - AH/ESP/IPCOMP
 */
	// id 表示 XFRM SA 的 ID 信息，包括协议类型、源 IP、目标 IP、安全参数索引和 SPI。
	struct xfrm_id		id;

/* Source address of tunnel. Ignored, if it is not a tunnel. */
	// saddr 表示要使用的源 IP （如果是隧道模式的话）。
	xfrm_address_t		saddr;
	// encap_family 表示封装方式。
	unsigned short		encap_family;

	// reqid 表示 XFRM SA 的请求ID。
	u32			reqid;

/* Mode: transport, tunnel etc. */
	// mode 表示 XFRM SA 的模式（如传输模式或隧道模式）。
	u8			mode;

/* Sharing mode: unique, this session only, this user only etc. */
	// share 表示 XFRM SA 的共享方式（如唯一共享或与用户共享）。
	u8			share;

/* May skip this transfomration if no SA is found */
	// optional 表示 SA 是否可选，即可以略过此转换操作。
	u8			optional;

/* Skip aalgos/ealgos/calgos checks. */
	// allalgs 表示是否跳过算法检查，以及使用哪些算法。
	u8			allalgs;

/* Bit mask of algos allowed for acquisition */
	// aalgos、ealgos 和 calgos 分别表示允许使用的认证算法、加密算法和压缩算法的位掩码。
	u32			aalgos;
	u32			ealgos;
	u32			calgos;
};

#define XFRM_MAX_DEPTH		6
#define XFRM_MAX_OFFLOAD_DEPTH	1

struct xfrm_policy_walk_entry {
	struct list_head	all;
	u8			dead;
};

struct xfrm_policy_walk {
	struct xfrm_policy_walk_entry walk;
	u8 type;
	u32 seq;
};

struct xfrm_policy_queue {
	struct sk_buff_head	hold_queue;
	struct timer_list	hold_timer;
	unsigned long		timeout;
};

// 表示 XFRM 策略（policy）
struct xfrm_policy {
	// xp_net 表示该策略所属的网络命名空间（可能因为多个 network namespace 管理下的 policy 链都是在全局策略链表上）。
	possible_net_t		xp_net;
	// bydst 和 byidx 分别表示采用哈希表的两种策略索引方式。
	struct hlist_node	bydst;
	struct hlist_node	byidx;

	/* This lock only affects elements except for entry. */
	// lock 表示该策略的读写锁。
	rwlock_t		lock;
	// refcnt 表示该策略的引用计数。
	refcount_t		refcnt;
	// timer 是一个计时器，用于检测策略是否到期。
	struct timer_list	timer;

	// genid 表示策略的权限，在 policy 控制路径下被使用。
	atomic_t		genid;
	// priority 表示该策略的优先级。
	u32			priority;
	// index 表示该策略的策略索引（policy index，PI）。
	u32			index;
	// if_id 表示该策略所属的网络接口 ID。
	u32			if_id;
	// mark 表示策略的标志。
	struct xfrm_mark	mark;
	// selector 表示选择器，用于匹配数据包。
	struct xfrm_selector	selector;
	// lft 和 curlft 分别表示策略的生存周期及当前状态。
	struct xfrm_lifetime_cfg lft;
	struct xfrm_lifetime_cur curlft;
	// walk 表示策略遍历信息，用于遍历匹配的策略。
	struct xfrm_policy_walk_entry walk;
	// polq 表示策略队列，用于在 XFRM 模块内部传递策略。
	struct xfrm_policy_queue polq;
	// type 表示策略的类型。
	u8			type;
	// action 表示对匹配的数据包要采取的操作（如加密、解密、转发等）。
	u8			action;
	// flags 表示策略的标志。
	u8			flags;
	// xfrm_nr 表示 XFRM 仿真堆栈中的元素数量。
	u8			xfrm_nr;
	// family 表示策略所属的地址族。
	u16			family;
	// security 表示策略的安全上下文。
	struct xfrm_sec_ctx	*security;
	// xfrm_vec 表示 XFRM SA 的模板信息。
	struct xfrm_tmpl       	xfrm_vec[XFRM_MAX_DEPTH];
	// rcu 表示用于 RCU 机制的垃圾收集链表的头部。
	struct rcu_head		rcu;
};

static inline struct net *xp_net(const struct xfrm_policy *xp)
{
	return read_pnet(&xp->xp_net);
}

// 用于表示 XFRM 传输层管理进程(kernel-managed process)与用户进程之间通信时的地址信息
struct xfrm_kmaddress {
	// local 表示传输层管理进程监听的本地地址。
	xfrm_address_t          local;
	// remote 表示与传输层管理进程通信的用户进程地址。
	xfrm_address_t          remote;
	// reserved 表示预留位。
	u32			reserved;
	// family 表示地址族（IPv4或IPv6）。
	u16			family;
};

struct xfrm_migrate {
	xfrm_address_t		old_daddr;
	xfrm_address_t		old_saddr;
	xfrm_address_t		new_daddr;
	xfrm_address_t		new_saddr;
	u8			proto;
	u8			mode;
	u16			reserved;
	u32			reqid;
	u16			old_family;
	u16			new_family;
};

#define XFRM_KM_TIMEOUT                30
/* what happened */
#define XFRM_REPLAY_UPDATE	XFRM_AE_CR
#define XFRM_REPLAY_TIMEOUT	XFRM_AE_CE

/* default aevent timeout in units of 100ms */
#define XFRM_AE_ETIME			10
/* Async Event timer multiplier */
#define XFRM_AE_ETH_M			10
/* default seq threshold size */
#define XFRM_AE_SEQT_SIZE		2

struct xfrm_mgr {
	struct list_head	list;
	int			(*notify)(struct xfrm_state *x, const struct km_event *c);
	int			(*acquire)(struct xfrm_state *x, struct xfrm_tmpl *, struct xfrm_policy *xp);
	struct xfrm_policy	*(*compile_policy)(struct sock *sk, int opt, u8 *data, int len, int *dir);
	int			(*new_mapping)(struct xfrm_state *x, xfrm_address_t *ipaddr, __be16 sport);
	int			(*notify_policy)(struct xfrm_policy *x, int dir, const struct km_event *c);
	int			(*report)(struct net *net, u8 proto, struct xfrm_selector *sel, xfrm_address_t *addr);
	int			(*migrate)(const struct xfrm_selector *sel,
					   u8 dir, u8 type,
					   const struct xfrm_migrate *m,
					   int num_bundles,
					   const struct xfrm_kmaddress *k,
					   const struct xfrm_encap_tmpl *encap);
	bool			(*is_alive)(const struct km_event *c);
};

int xfrm_register_km(struct xfrm_mgr *km);
int xfrm_unregister_km(struct xfrm_mgr *km);

struct xfrm_tunnel_skb_cb {
	union {
		struct inet_skb_parm h4;
		struct inet6_skb_parm h6;
	} header;

	union {
		struct ip_tunnel *ip4;
		struct ip6_tnl *ip6;
	} tunnel;
};

#define XFRM_TUNNEL_SKB_CB(__skb) ((struct xfrm_tunnel_skb_cb *)&((__skb)->cb[0]))

/*
 * This structure is used for the duration where packets are being
 * transformed by IPsec.  As soon as the packet leaves IPsec the
 * area beyond the generic IP part may be overwritten.
 */
struct xfrm_skb_cb {
	struct xfrm_tunnel_skb_cb header;

        /* Sequence number for replay protection. */
	union {
		struct {
			__u32 low;
			__u32 hi;
		} output;
		struct {
			__be32 low;
			__be32 hi;
		} input;
	} seq;
};

#define XFRM_SKB_CB(__skb) ((struct xfrm_skb_cb *)&((__skb)->cb[0]))

/*
 * This structure is used by the afinfo prepare_input/prepare_output functions
 * to transmit header information to the mode input/output functions.
 */
struct xfrm_mode_skb_cb {
	struct xfrm_tunnel_skb_cb header;

	/* Copied from header for IPv4, always set to zero and DF for IPv6. */
	__be16 id;
	__be16 frag_off;

	/* IP header length (excluding options or extension headers). */
	u8 ihl;

	/* TOS for IPv4, class for IPv6. */
	u8 tos;

	/* TTL for IPv4, hop limitfor IPv6. */
	u8 ttl;

	/* Protocol for IPv4, NH for IPv6. */
	u8 protocol;

	/* Option length for IPv4, zero for IPv6. */
	u8 optlen;

	/* Used by IPv6 only, zero for IPv4. */
	u8 flow_lbl[3];
};

#define XFRM_MODE_SKB_CB(__skb) ((struct xfrm_mode_skb_cb *)&((__skb)->cb[0]))

/*
 * This structure is used by the input processing to locate the SPI and
 * related information.
 */
struct xfrm_spi_skb_cb {
	struct xfrm_tunnel_skb_cb header;

	unsigned int daddroff;
	unsigned int family;
	__be32 seq;
};

#define XFRM_SPI_SKB_CB(__skb) ((struct xfrm_spi_skb_cb *)&((__skb)->cb[0]))

#ifdef CONFIG_AUDITSYSCALL
static inline struct audit_buffer *xfrm_audit_start(const char *op)
{
	struct audit_buffer *audit_buf = NULL;

	if (audit_enabled == AUDIT_OFF)
		return NULL;
	audit_buf = audit_log_start(audit_context(), GFP_ATOMIC,
				    AUDIT_MAC_IPSEC_EVENT);
	if (audit_buf == NULL)
		return NULL;
	audit_log_format(audit_buf, "op=%s", op);
	return audit_buf;
}

static inline void xfrm_audit_helper_usrinfo(bool task_valid,
					     struct audit_buffer *audit_buf)
{
	const unsigned int auid = from_kuid(&init_user_ns, task_valid ?
					    audit_get_loginuid(current) :
					    INVALID_UID);
	const unsigned int ses = task_valid ? audit_get_sessionid(current) :
		AUDIT_SID_UNSET;

	audit_log_format(audit_buf, " auid=%u ses=%u", auid, ses);
	audit_log_task_context(audit_buf);
}

void xfrm_audit_policy_add(struct xfrm_policy *xp, int result, bool task_valid);
void xfrm_audit_policy_delete(struct xfrm_policy *xp, int result,
			      bool task_valid);
void xfrm_audit_state_add(struct xfrm_state *x, int result, bool task_valid);
void xfrm_audit_state_delete(struct xfrm_state *x, int result, bool task_valid);
void xfrm_audit_state_replay_overflow(struct xfrm_state *x,
				      struct sk_buff *skb);
void xfrm_audit_state_replay(struct xfrm_state *x, struct sk_buff *skb,
			     __be32 net_seq);
void xfrm_audit_state_notfound_simple(struct sk_buff *skb, u16 family);
void xfrm_audit_state_notfound(struct sk_buff *skb, u16 family, __be32 net_spi,
			       __be32 net_seq);
void xfrm_audit_state_icvfail(struct xfrm_state *x, struct sk_buff *skb,
			      u8 proto);
#else

static inline void xfrm_audit_policy_add(struct xfrm_policy *xp, int result,
					 bool task_valid)
{
}

static inline void xfrm_audit_policy_delete(struct xfrm_policy *xp, int result,
					    bool task_valid)
{
}

static inline void xfrm_audit_state_add(struct xfrm_state *x, int result,
					bool task_valid)
{
}

static inline void xfrm_audit_state_delete(struct xfrm_state *x, int result,
					   bool task_valid)
{
}

static inline void xfrm_audit_state_replay_overflow(struct xfrm_state *x,
					     struct sk_buff *skb)
{
}

static inline void xfrm_audit_state_replay(struct xfrm_state *x,
					   struct sk_buff *skb, __be32 net_seq)
{
}

static inline void xfrm_audit_state_notfound_simple(struct sk_buff *skb,
				      u16 family)
{
}

static inline void xfrm_audit_state_notfound(struct sk_buff *skb, u16 family,
				      __be32 net_spi, __be32 net_seq)
{
}

static inline void xfrm_audit_state_icvfail(struct xfrm_state *x,
				     struct sk_buff *skb, u8 proto)
{
}
#endif /* CONFIG_AUDITSYSCALL */

static inline void xfrm_pol_hold(struct xfrm_policy *policy)
{
	if (likely(policy != NULL))
		refcount_inc(&policy->refcnt);
}

void xfrm_policy_destroy(struct xfrm_policy *policy);

static inline void xfrm_pol_put(struct xfrm_policy *policy)
{
	if (refcount_dec_and_test(&policy->refcnt))
		xfrm_policy_destroy(policy);
}

static inline void xfrm_pols_put(struct xfrm_policy **pols, int npols)
{
	int i;
	for (i = npols - 1; i >= 0; --i)
		xfrm_pol_put(pols[i]);
}

void __xfrm_state_destroy(struct xfrm_state *);

static inline void __xfrm_state_put(struct xfrm_state *x)
{
	refcount_dec(&x->refcnt);
}

static inline void xfrm_state_put(struct xfrm_state *x)
{
	if (refcount_dec_and_test(&x->refcnt))
		__xfrm_state_destroy(x);
}

static inline void xfrm_state_hold(struct xfrm_state *x)
{
	refcount_inc(&x->refcnt);
}

static inline bool addr_match(const void *token1, const void *token2,
			      unsigned int prefixlen)
{
	const __be32 *a1 = token1;
	const __be32 *a2 = token2;
	unsigned int pdw;
	unsigned int pbi;

	pdw = prefixlen >> 5;	  /* num of whole u32 in prefix */
	pbi = prefixlen &  0x1f;  /* num of bits in incomplete u32 in prefix */

	if (pdw)
		if (memcmp(a1, a2, pdw << 2))
			return false;

	if (pbi) {
		__be32 mask;

		mask = htonl((0xffffffff) << (32 - pbi));

		if ((a1[pdw] ^ a2[pdw]) & mask)
			return false;
	}

	return true;
}

static inline bool addr4_match(__be32 a1, __be32 a2, u8 prefixlen)
{
	/* C99 6.5.7 (3): u32 << 32 is undefined behaviour */
	if (sizeof(long) == 4 && prefixlen == 0)
		return true;
	return !((a1 ^ a2) & htonl(~0UL << (32 - prefixlen)));
}

static __inline__
__be16 xfrm_flowi_sport(const struct flowi *fl, const union flowi_uli *uli)
{
	__be16 port;
	switch(fl->flowi_proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
	case IPPROTO_UDPLITE:
	case IPPROTO_SCTP:
		port = uli->ports.sport;
		break;
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		port = htons(uli->icmpt.type);
		break;
	case IPPROTO_MH:
		port = htons(uli->mht.type);
		break;
	case IPPROTO_GRE:
		port = htons(ntohl(uli->gre_key) >> 16);
		break;
	default:
		port = 0;	/*XXX*/
	}
	return port;
}

static __inline__
__be16 xfrm_flowi_dport(const struct flowi *fl, const union flowi_uli *uli)
{
	__be16 port;
	switch(fl->flowi_proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
	case IPPROTO_UDPLITE:
	case IPPROTO_SCTP:
		port = uli->ports.dport;
		break;
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		port = htons(uli->icmpt.code);
		break;
	case IPPROTO_GRE:
		port = htons(ntohl(uli->gre_key) & 0xffff);
		break;
	default:
		port = 0;	/*XXX*/
	}
	return port;
}

bool xfrm_selector_match(const struct xfrm_selector *sel,
			 const struct flowi *fl, unsigned short family);

#ifdef CONFIG_SECURITY_NETWORK_XFRM
/*	If neither has a context --> match
 * 	Otherwise, both must have a context and the sids, doi, alg must match
 */
static inline bool xfrm_sec_ctx_match(struct xfrm_sec_ctx *s1, struct xfrm_sec_ctx *s2)
{
	return ((!s1 && !s2) ||
		(s1 && s2 &&
		 (s1->ctx_sid == s2->ctx_sid) &&
		 (s1->ctx_doi == s2->ctx_doi) &&
		 (s1->ctx_alg == s2->ctx_alg)));
}
#else
static inline bool xfrm_sec_ctx_match(struct xfrm_sec_ctx *s1, struct xfrm_sec_ctx *s2)
{
	return true;
}
#endif

/* A struct encoding bundle of transformations to apply to some set of flow.
 *
 * xdst->child points to the next element of bundle.
 * dst->xfrm  points to an instanse of transformer.
 *
 * Due to unfortunate limitations of current routing cache, which we
 * have no time to fix, it mirrors struct rtable and bound to the same
 * routing key, including saddr,daddr. However, we can have many of
 * bundles differing by session id. All the bundles grow from a parent
 * policy rule.
 */
// 表示 XFRM 目的地（destination）
struct xfrm_dst {
	union {
		struct dst_entry	dst;
		struct rtable		rt;
		struct rt6_info		rt6;
	} u;
	// route 表示路径中的路由缓存。
	struct dst_entry *route;
	// child 表示路径中的下一个路由表项，用于路由重定向。
	struct dst_entry *child;
	// path 是当前路径中的下一个地址。
	struct dst_entry *path;
	// pols 是一个指针数组，表示应用到该目的地的 XFRM 策略，每个元素对应一个策略类型。
	struct xfrm_policy *pols[XFRM_POLICY_TYPE_MAX];
	// num_pols 表示 XFRM 策略的数量。num_xfrms 表示仿真机制中的元素数量。
	int num_pols, num_xfrms;
	// xfrm_genid 表示目的地的 XFRM 处理随机编号。
	u32 xfrm_genid;
	// policy_genid 表示目的地的策略版本号。
	u32 policy_genid;
	// route_mtu_cached 表示缓存的路径的 MTU。
	u32 route_mtu_cached;
	// child_mtu_cached 表示缓存的下一个路由的 MTU。
	u32 child_mtu_cached;
	// route_cookie 和 path_cookie 分别表示缓存的路径和下一个地址的标识符。
	u32 route_cookie;
	u32 path_cookie;
};

static inline struct dst_entry *xfrm_dst_path(const struct dst_entry *dst)
{
#ifdef CONFIG_XFRM
	if (dst->xfrm) {
		const struct xfrm_dst *xdst = (const struct xfrm_dst *) dst;

		return xdst->path;
	}
#endif
	return (struct dst_entry *) dst;
}

static inline struct dst_entry *xfrm_dst_child(const struct dst_entry *dst)
{
#ifdef CONFIG_XFRM
	if (dst->xfrm) {
		struct xfrm_dst *xdst = (struct xfrm_dst *) dst;
		return xdst->child;
	}
#endif
	return NULL;
}

#ifdef CONFIG_XFRM
static inline void xfrm_dst_set_child(struct xfrm_dst *xdst, struct dst_entry *child)
{
	xdst->child = child;
}

static inline void xfrm_dst_destroy(struct xfrm_dst *xdst)
{
	xfrm_pols_put(xdst->pols, xdst->num_pols);
	dst_release(xdst->route);
	if (likely(xdst->u.dst.xfrm))
		xfrm_state_put(xdst->u.dst.xfrm);
}
#endif

void xfrm_dst_ifdown(struct dst_entry *dst, struct net_device *dev);

// 用于表示 XFRM 接口的参数
struct xfrm_if_parms {
	// name 字符数组，用来表示 XFRM 接口的名称，字符串长度最大为 IFNAMSIZ。
	char name[IFNAMSIZ];	/* name of XFRM device */
	// link 整数值，用来表示底层 L2 接口的索引（ifindex），该索引是在系统中唯一的，可以通过 if_nametoindex() 或 if_indextoname() 函数获取 L2 接口名或索引值。
	int link;		/* ifindex of underlying L2 interface */
	// if_id 无符号整数值，用来表示 XFRM 接口的唯一标识符，当启用时，需要使用唯一的标识符（以太网通常使用 MAC 地址作为标识符）。
	u32 if_id;		/* interface identifyer */
};

// 用于表示 XFRM 接口
struct xfrm_if {
	// next 是一个指向下一个 struct xfrm_if 结构体的指针，用于在链表中将多个接口连接起来。这里使用了 __rcu 修饰符，表示该指针是一个 RCU（Read Copy Update）指针，用于实现读者优化的数据结构。
	struct xfrm_if __rcu *next;	/* next interface in list */
	// dev 是一个指向 Linux 内核中的 struct net_device 结构体的指针，表示与该接口关联的虚拟网络设备。
	struct net_device *dev;		/* virtual device associated with interface */
	// 表示与该接口关联的底层物理网络设备
	struct net_device *phydev;	/* physical device */
	// 用于表示该接口所属的网络命名空间
	struct net *net;		/* netns for packet i/o */
	// 表示该接口的参数。
	struct xfrm_if_parms p;		/* interface parms */
	// 表示该接口使用的通用接收处理机制（GRO - Generic Receive Offload）的数据结构。gro_cells 主要用于优化接收数据包的处理效率。
	struct gro_cells gro_cells;
};

// 表示 XFRM 协议的 offload 信息
struct xfrm_offload {
	/* Output sequence number for replay protection on offloading. */
	struct {
		__u32 low;
		__u32 hi;
	} seq;

	__u32			flags;
#define	SA_DELETE_REQ		1
#define	CRYPTO_DONE		2
#define	CRYPTO_NEXT_DONE	4
#define	CRYPTO_FALLBACK		8
#define	XFRM_GSO_SEGMENT	16
#define	XFRM_GRO		32
#define	XFRM_ESP_NO_TRAILER	64
#define	XFRM_DEV_RESUME		128

	__u32			status;
#define CRYPTO_SUCCESS				1
#define CRYPTO_GENERIC_ERROR			2
#define CRYPTO_TRANSPORT_AH_AUTH_FAILED		4
#define CRYPTO_TRANSPORT_ESP_AUTH_FAILED	8
#define CRYPTO_TUNNEL_AH_AUTH_FAILED		16
#define CRYPTO_TUNNEL_ESP_AUTH_FAILED		32
#define CRYPTO_INVALID_PACKET_SYNTAX		64
#define CRYPTO_INVALID_PROTOCOL			128

	__u8			proto;
};

struct sec_path {
	refcount_t		refcnt;
	int			len;
	int			olen;

	struct xfrm_state	*xvec[XFRM_MAX_DEPTH];
	struct xfrm_offload	ovec[XFRM_MAX_OFFLOAD_DEPTH];
};

static inline int secpath_exists(struct sk_buff *skb)
{
#ifdef CONFIG_XFRM
	return skb->sp != NULL;
#else
	return 0;
#endif
}

static inline struct sec_path *
secpath_get(struct sec_path *sp)
{
	if (sp)
		refcount_inc(&sp->refcnt);
	return sp;
}

void __secpath_destroy(struct sec_path *sp);

static inline void
secpath_put(struct sec_path *sp)
{
	if (sp && refcount_dec_and_test(&sp->refcnt))
		__secpath_destroy(sp);
}

struct sec_path *secpath_dup(struct sec_path *src);
int secpath_set(struct sk_buff *skb);

static inline void
secpath_reset(struct sk_buff *skb)
{
#ifdef CONFIG_XFRM
	secpath_put(skb->sp);
	skb->sp = NULL;
#endif
}

static inline int
xfrm_addr_any(const xfrm_address_t *addr, unsigned short family)
{
	switch (family) {
	case AF_INET:
		return addr->a4 == 0;
	case AF_INET6:
		return ipv6_addr_any(&addr->in6);
	}
	return 0;
}

static inline int
__xfrm4_state_addr_cmp(const struct xfrm_tmpl *tmpl, const struct xfrm_state *x)
{
	return	(tmpl->saddr.a4 &&
		 tmpl->saddr.a4 != x->props.saddr.a4);
}

static inline int
__xfrm6_state_addr_cmp(const struct xfrm_tmpl *tmpl, const struct xfrm_state *x)
{
	return	(!ipv6_addr_any((struct in6_addr*)&tmpl->saddr) &&
		 !ipv6_addr_equal((struct in6_addr *)&tmpl->saddr, (struct in6_addr*)&x->props.saddr));
}

static inline int
xfrm_state_addr_cmp(const struct xfrm_tmpl *tmpl, const struct xfrm_state *x, unsigned short family)
{
	switch (family) {
	case AF_INET:
		return __xfrm4_state_addr_cmp(tmpl, x);
	case AF_INET6:
		return __xfrm6_state_addr_cmp(tmpl, x);
	}
	return !0;
}

#ifdef CONFIG_XFRM
int __xfrm_policy_check(struct sock *, int dir, struct sk_buff *skb,
			unsigned short family);

static inline int __xfrm_policy_check2(struct sock *sk, int dir,
				       struct sk_buff *skb,
				       unsigned int family, int reverse)
{
	struct net *net = dev_net(skb->dev);
	int ndir = dir | (reverse ? XFRM_POLICY_MASK + 1 : 0);

	if (sk && sk->sk_policy[XFRM_POLICY_IN])
		return __xfrm_policy_check(sk, ndir, skb, family);

	return	(!net->xfrm.policy_count[dir] && !skb->sp) ||
		(skb_dst(skb)->flags & DST_NOPOLICY) ||
		__xfrm_policy_check(sk, ndir, skb, family);
}

static inline int xfrm_policy_check(struct sock *sk, int dir, struct sk_buff *skb, unsigned short family)
{
	return __xfrm_policy_check2(sk, dir, skb, family, 0);
}

static inline int xfrm4_policy_check(struct sock *sk, int dir, struct sk_buff *skb)
{
	return xfrm_policy_check(sk, dir, skb, AF_INET);
}

static inline int xfrm6_policy_check(struct sock *sk, int dir, struct sk_buff *skb)
{
	return xfrm_policy_check(sk, dir, skb, AF_INET6);
}

static inline int xfrm4_policy_check_reverse(struct sock *sk, int dir,
					     struct sk_buff *skb)
{
	return __xfrm_policy_check2(sk, dir, skb, AF_INET, 1);
}

static inline int xfrm6_policy_check_reverse(struct sock *sk, int dir,
					     struct sk_buff *skb)
{
	return __xfrm_policy_check2(sk, dir, skb, AF_INET6, 1);
}

int __xfrm_decode_session(struct sk_buff *skb, struct flowi *fl,
			  unsigned int family, int reverse);

static inline int xfrm_decode_session(struct sk_buff *skb, struct flowi *fl,
				      unsigned int family)
{
	return __xfrm_decode_session(skb, fl, family, 0);
}

static inline int xfrm_decode_session_reverse(struct sk_buff *skb,
					      struct flowi *fl,
					      unsigned int family)
{
	return __xfrm_decode_session(skb, fl, family, 1);
}

int __xfrm_route_forward(struct sk_buff *skb, unsigned short family);

static inline int xfrm_route_forward(struct sk_buff *skb, unsigned short family)
{
	struct net *net = dev_net(skb->dev);

	return	!net->xfrm.policy_count[XFRM_POLICY_OUT] ||
		(skb_dst(skb)->flags & DST_NOXFRM) ||
		__xfrm_route_forward(skb, family);
}

static inline int xfrm4_route_forward(struct sk_buff *skb)
{
	return xfrm_route_forward(skb, AF_INET);
}

static inline int xfrm6_route_forward(struct sk_buff *skb)
{
	return xfrm_route_forward(skb, AF_INET6);
}

int __xfrm_sk_clone_policy(struct sock *sk, const struct sock *osk);

static inline int xfrm_sk_clone_policy(struct sock *sk, const struct sock *osk)
{
	sk->sk_policy[0] = NULL;
	sk->sk_policy[1] = NULL;
	if (unlikely(osk->sk_policy[0] || osk->sk_policy[1]))
		return __xfrm_sk_clone_policy(sk, osk);
	return 0;
}

int xfrm_policy_delete(struct xfrm_policy *pol, int dir);

static inline void xfrm_sk_free_policy(struct sock *sk)
{
	struct xfrm_policy *pol;

	pol = rcu_dereference_protected(sk->sk_policy[0], 1);
	if (unlikely(pol != NULL)) {
		xfrm_policy_delete(pol, XFRM_POLICY_MAX);
		sk->sk_policy[0] = NULL;
	}
	pol = rcu_dereference_protected(sk->sk_policy[1], 1);
	if (unlikely(pol != NULL)) {
		xfrm_policy_delete(pol, XFRM_POLICY_MAX+1);
		sk->sk_policy[1] = NULL;
	}
}

#else

static inline void xfrm_sk_free_policy(struct sock *sk) {}
static inline int xfrm_sk_clone_policy(struct sock *sk, const struct sock *osk) { return 0; }
static inline int xfrm6_route_forward(struct sk_buff *skb) { return 1; }
static inline int xfrm4_route_forward(struct sk_buff *skb) { return 1; }
static inline int xfrm6_policy_check(struct sock *sk, int dir, struct sk_buff *skb)
{
	return 1;
}
static inline int xfrm4_policy_check(struct sock *sk, int dir, struct sk_buff *skb)
{
	return 1;
}
static inline int xfrm_policy_check(struct sock *sk, int dir, struct sk_buff *skb, unsigned short family)
{
	return 1;
}
static inline int xfrm_decode_session_reverse(struct sk_buff *skb,
					      struct flowi *fl,
					      unsigned int family)
{
	return -ENOSYS;
}
static inline int xfrm4_policy_check_reverse(struct sock *sk, int dir,
					     struct sk_buff *skb)
{
	return 1;
}
static inline int xfrm6_policy_check_reverse(struct sock *sk, int dir,
					     struct sk_buff *skb)
{
	return 1;
}
#endif

static __inline__
xfrm_address_t *xfrm_flowi_daddr(const struct flowi *fl, unsigned short family)
{
	switch (family){
	case AF_INET:
		return (xfrm_address_t *)&fl->u.ip4.daddr;
	case AF_INET6:
		return (xfrm_address_t *)&fl->u.ip6.daddr;
	}
	return NULL;
}

static __inline__
xfrm_address_t *xfrm_flowi_saddr(const struct flowi *fl, unsigned short family)
{
	switch (family){
	case AF_INET:
		return (xfrm_address_t *)&fl->u.ip4.saddr;
	case AF_INET6:
		return (xfrm_address_t *)&fl->u.ip6.saddr;
	}
	return NULL;
}

static __inline__
void xfrm_flowi_addr_get(const struct flowi *fl,
			 xfrm_address_t *saddr, xfrm_address_t *daddr,
			 unsigned short family)
{
	switch(family) {
	case AF_INET:
		memcpy(&saddr->a4, &fl->u.ip4.saddr, sizeof(saddr->a4));
		memcpy(&daddr->a4, &fl->u.ip4.daddr, sizeof(daddr->a4));
		break;
	case AF_INET6:
		saddr->in6 = fl->u.ip6.saddr;
		daddr->in6 = fl->u.ip6.daddr;
		break;
	}
}

static __inline__ int
__xfrm4_state_addr_check(const struct xfrm_state *x,
			 const xfrm_address_t *daddr, const xfrm_address_t *saddr)
{
	if (daddr->a4 == x->id.daddr.a4 &&
	    (saddr->a4 == x->props.saddr.a4 || !saddr->a4 || !x->props.saddr.a4))
		return 1;
	return 0;
}

static __inline__ int
__xfrm6_state_addr_check(const struct xfrm_state *x,
			 const xfrm_address_t *daddr, const xfrm_address_t *saddr)
{
	if (ipv6_addr_equal((struct in6_addr *)daddr, (struct in6_addr *)&x->id.daddr) &&
	    (ipv6_addr_equal((struct in6_addr *)saddr, (struct in6_addr *)&x->props.saddr) ||
	     ipv6_addr_any((struct in6_addr *)saddr) ||
	     ipv6_addr_any((struct in6_addr *)&x->props.saddr)))
		return 1;
	return 0;
}

static __inline__ int
xfrm_state_addr_check(const struct xfrm_state *x,
		      const xfrm_address_t *daddr, const xfrm_address_t *saddr,
		      unsigned short family)
{
	switch (family) {
	case AF_INET:
		return __xfrm4_state_addr_check(x, daddr, saddr);
	case AF_INET6:
		return __xfrm6_state_addr_check(x, daddr, saddr);
	}
	return 0;
}

static __inline__ int
xfrm_state_addr_flow_check(const struct xfrm_state *x, const struct flowi *fl,
			   unsigned short family)
{
	switch (family) {
	case AF_INET:
		return __xfrm4_state_addr_check(x,
						(const xfrm_address_t *)&fl->u.ip4.daddr,
						(const xfrm_address_t *)&fl->u.ip4.saddr);
	case AF_INET6:
		return __xfrm6_state_addr_check(x,
						(const xfrm_address_t *)&fl->u.ip6.daddr,
						(const xfrm_address_t *)&fl->u.ip6.saddr);
	}
	return 0;
}

static inline int xfrm_state_kern(const struct xfrm_state *x)
{
	return atomic_read(&x->tunnel_users);
}

static inline int xfrm_id_proto_match(u8 proto, u8 userproto)
{
	return (!userproto || proto == userproto ||
		(userproto == IPSEC_PROTO_ANY && (proto == IPPROTO_AH ||
						  proto == IPPROTO_ESP ||
						  proto == IPPROTO_COMP)));
}

/*
 * xfrm algorithm information
 */
struct xfrm_algo_aead_info {
	char *geniv;
	u16 icv_truncbits;
};

struct xfrm_algo_auth_info {
	u16 icv_truncbits;
	u16 icv_fullbits;
};

struct xfrm_algo_encr_info {
	char *geniv;
	u16 blockbits;
	u16 defkeybits;
};

struct xfrm_algo_comp_info {
	u16 threshold;
};

struct xfrm_algo_desc {
	char *name;
	char *compat;
	u8 available:1;
	u8 pfkey_supported:1;
	union {
		struct xfrm_algo_aead_info aead;
		struct xfrm_algo_auth_info auth;
		struct xfrm_algo_encr_info encr;
		struct xfrm_algo_comp_info comp;
	} uinfo;
	struct sadb_alg desc;
};

/* XFRM protocol handlers.  */
struct xfrm4_protocol {
	int (*handler)(struct sk_buff *skb);
	int (*input_handler)(struct sk_buff *skb, int nexthdr, __be32 spi,
			     int encap_type);
	int (*cb_handler)(struct sk_buff *skb, int err);
	int (*err_handler)(struct sk_buff *skb, u32 info);

	struct xfrm4_protocol __rcu *next;
	int priority;
};

struct xfrm6_protocol {
	int (*handler)(struct sk_buff *skb);
	int (*cb_handler)(struct sk_buff *skb, int err);
	int (*err_handler)(struct sk_buff *skb, struct inet6_skb_parm *opt,
			   u8 type, u8 code, int offset, __be32 info);

	struct xfrm6_protocol __rcu *next;
	int priority;
};

/* XFRM tunnel handlers.  */
struct xfrm_tunnel {
	int (*handler)(struct sk_buff *skb);
	int (*err_handler)(struct sk_buff *skb, u32 info);

	struct xfrm_tunnel __rcu *next;
	int priority;
};

struct xfrm6_tunnel {
	int (*handler)(struct sk_buff *skb);
	int (*err_handler)(struct sk_buff *skb, struct inet6_skb_parm *opt,
			   u8 type, u8 code, int offset, __be32 info);
	struct xfrm6_tunnel __rcu *next;
	int priority;
};

void xfrm_init(void);
void xfrm4_init(void);
int xfrm_state_init(struct net *net);
void xfrm_state_fini(struct net *net);
void xfrm4_state_init(void);
void xfrm4_protocol_init(void);
#ifdef CONFIG_XFRM
int xfrm6_init(void);
void xfrm6_fini(void);
int xfrm6_state_init(void);
void xfrm6_state_fini(void);
int xfrm6_protocol_init(void);
void xfrm6_protocol_fini(void);
#else
static inline int xfrm6_init(void)
{
	return 0;
}
static inline void xfrm6_fini(void)
{
	;
}
#endif

#ifdef CONFIG_XFRM_STATISTICS
int xfrm_proc_init(struct net *net);
void xfrm_proc_fini(struct net *net);
#endif

int xfrm_sysctl_init(struct net *net);
#ifdef CONFIG_SYSCTL
void xfrm_sysctl_fini(struct net *net);
#else
static inline void xfrm_sysctl_fini(struct net *net)
{
}
#endif

void xfrm_state_walk_init(struct xfrm_state_walk *walk, u8 proto,
			  struct xfrm_address_filter *filter);
int xfrm_state_walk(struct net *net, struct xfrm_state_walk *walk,
		    int (*func)(struct xfrm_state *, int, void*), void *);
void xfrm_state_walk_done(struct xfrm_state_walk *walk, struct net *net);
struct xfrm_state *xfrm_state_alloc(struct net *net);
void xfrm_state_free(struct xfrm_state *x);
struct xfrm_state *xfrm_state_find(const xfrm_address_t *daddr,
				   const xfrm_address_t *saddr,
				   const struct flowi *fl,
				   struct xfrm_tmpl *tmpl,
				   struct xfrm_policy *pol, int *err,
				   unsigned short family, u32 if_id);
struct xfrm_state *xfrm_stateonly_find(struct net *net, u32 mark, u32 if_id,
				       xfrm_address_t *daddr,
				       xfrm_address_t *saddr,
				       unsigned short family,
				       u8 mode, u8 proto, u32 reqid);
struct xfrm_state *xfrm_state_lookup_byspi(struct net *net, __be32 spi,
					      unsigned short family);
int xfrm_state_check_expire(struct xfrm_state *x);
void xfrm_state_insert(struct xfrm_state *x);
int xfrm_state_add(struct xfrm_state *x);
int xfrm_state_update(struct xfrm_state *x);
struct xfrm_state *xfrm_state_lookup(struct net *net, u32 mark,
				     const xfrm_address_t *daddr, __be32 spi,
				     u8 proto, unsigned short family);
struct xfrm_state *xfrm_state_lookup_byaddr(struct net *net, u32 mark,
					    const xfrm_address_t *daddr,
					    const xfrm_address_t *saddr,
					    u8 proto,
					    unsigned short family);
#ifdef CONFIG_XFRM_SUB_POLICY
int xfrm_tmpl_sort(struct xfrm_tmpl **dst, struct xfrm_tmpl **src, int n,
		   unsigned short family, struct net *net);
int xfrm_state_sort(struct xfrm_state **dst, struct xfrm_state **src, int n,
		    unsigned short family);
#else
static inline int xfrm_tmpl_sort(struct xfrm_tmpl **dst, struct xfrm_tmpl **src,
				 int n, unsigned short family, struct net *net)
{
	return -ENOSYS;
}

static inline int xfrm_state_sort(struct xfrm_state **dst, struct xfrm_state **src,
				  int n, unsigned short family)
{
	return -ENOSYS;
}
#endif

struct xfrmk_sadinfo {
	u32 sadhcnt; /* current hash bkts */
	u32 sadhmcnt; /* max allowed hash bkts */
	u32 sadcnt; /* current running count */
};

struct xfrmk_spdinfo {
	u32 incnt;
	u32 outcnt;
	u32 fwdcnt;
	u32 inscnt;
	u32 outscnt;
	u32 fwdscnt;
	u32 spdhcnt;
	u32 spdhmcnt;
};

struct xfrm_state *xfrm_find_acq_byseq(struct net *net, u32 mark, u32 seq);
int xfrm_state_delete(struct xfrm_state *x);
int xfrm_state_flush(struct net *net, u8 proto, bool task_valid);
int xfrm_dev_state_flush(struct net *net, struct net_device *dev, bool task_valid);
void xfrm_sad_getinfo(struct net *net, struct xfrmk_sadinfo *si);
void xfrm_spd_getinfo(struct net *net, struct xfrmk_spdinfo *si);
u32 xfrm_replay_seqhi(struct xfrm_state *x, __be32 net_seq);
int xfrm_init_replay(struct xfrm_state *x);
int xfrm_state_mtu(struct xfrm_state *x, int mtu);
int __xfrm_init_state(struct xfrm_state *x, bool init_replay, bool offload);
int xfrm_init_state(struct xfrm_state *x);
int xfrm_prepare_input(struct xfrm_state *x, struct sk_buff *skb);
int xfrm_input(struct sk_buff *skb, int nexthdr, __be32 spi, int encap_type);
int xfrm_input_resume(struct sk_buff *skb, int nexthdr);
int xfrm_trans_queue(struct sk_buff *skb,
		     int (*finish)(struct net *, struct sock *,
				   struct sk_buff *));
int xfrm_output_resume(struct sk_buff *skb, int err);
int xfrm_output(struct sock *sk, struct sk_buff *skb);
int xfrm_inner_extract_output(struct xfrm_state *x, struct sk_buff *skb);
void xfrm_local_error(struct sk_buff *skb, int mtu);
int xfrm4_extract_header(struct sk_buff *skb);
int xfrm4_extract_input(struct xfrm_state *x, struct sk_buff *skb);
int xfrm4_rcv_encap(struct sk_buff *skb, int nexthdr, __be32 spi,
		    int encap_type);
int xfrm4_transport_finish(struct sk_buff *skb, int async);
int xfrm4_rcv(struct sk_buff *skb);
int xfrm_parse_spi(struct sk_buff *skb, u8 nexthdr, __be32 *spi, __be32 *seq);

static inline int xfrm4_rcv_spi(struct sk_buff *skb, int nexthdr, __be32 spi)
{
	XFRM_TUNNEL_SKB_CB(skb)->tunnel.ip4 = NULL;
	XFRM_SPI_SKB_CB(skb)->family = AF_INET;
	XFRM_SPI_SKB_CB(skb)->daddroff = offsetof(struct iphdr, daddr);
	return xfrm_input(skb, nexthdr, spi, 0);
}

int xfrm4_extract_output(struct xfrm_state *x, struct sk_buff *skb);
int xfrm4_prepare_output(struct xfrm_state *x, struct sk_buff *skb);
int xfrm4_output(struct net *net, struct sock *sk, struct sk_buff *skb);
int xfrm4_output_finish(struct sock *sk, struct sk_buff *skb);
int xfrm4_rcv_cb(struct sk_buff *skb, u8 protocol, int err);
int xfrm4_protocol_register(struct xfrm4_protocol *handler, unsigned char protocol);
int xfrm4_protocol_deregister(struct xfrm4_protocol *handler, unsigned char protocol);
int xfrm4_tunnel_register(struct xfrm_tunnel *handler, unsigned short family);
int xfrm4_tunnel_deregister(struct xfrm_tunnel *handler, unsigned short family);
void xfrm4_local_error(struct sk_buff *skb, u32 mtu);
int xfrm6_extract_header(struct sk_buff *skb);
int xfrm6_extract_input(struct xfrm_state *x, struct sk_buff *skb);
int xfrm6_rcv_spi(struct sk_buff *skb, int nexthdr, __be32 spi,
		  struct ip6_tnl *t);
int xfrm6_transport_finish(struct sk_buff *skb, int async);
int xfrm6_rcv_tnl(struct sk_buff *skb, struct ip6_tnl *t);
int xfrm6_rcv(struct sk_buff *skb);
int xfrm6_input_addr(struct sk_buff *skb, xfrm_address_t *daddr,
		     xfrm_address_t *saddr, u8 proto);
void xfrm6_local_error(struct sk_buff *skb, u32 mtu);
int xfrm6_rcv_cb(struct sk_buff *skb, u8 protocol, int err);
int xfrm6_protocol_register(struct xfrm6_protocol *handler, unsigned char protocol);
int xfrm6_protocol_deregister(struct xfrm6_protocol *handler, unsigned char protocol);
int xfrm6_tunnel_register(struct xfrm6_tunnel *handler, unsigned short family);
int xfrm6_tunnel_deregister(struct xfrm6_tunnel *handler, unsigned short family);
__be32 xfrm6_tunnel_alloc_spi(struct net *net, xfrm_address_t *saddr);
__be32 xfrm6_tunnel_spi_lookup(struct net *net, const xfrm_address_t *saddr);
int xfrm6_extract_output(struct xfrm_state *x, struct sk_buff *skb);
int xfrm6_prepare_output(struct xfrm_state *x, struct sk_buff *skb);
int xfrm6_output(struct net *net, struct sock *sk, struct sk_buff *skb);
int xfrm6_output_finish(struct sock *sk, struct sk_buff *skb);
int xfrm6_find_1stfragopt(struct xfrm_state *x, struct sk_buff *skb,
			  u8 **prevhdr);

#ifdef CONFIG_XFRM
int xfrm4_udp_encap_rcv(struct sock *sk, struct sk_buff *skb);
int xfrm_user_policy(struct sock *sk, int optname,
		     u8 __user *optval, int optlen);
#else
static inline int xfrm_user_policy(struct sock *sk, int optname, u8 __user *optval, int optlen)
{
 	return -ENOPROTOOPT;
}

static inline int xfrm4_udp_encap_rcv(struct sock *sk, struct sk_buff *skb)
{
 	/* should not happen */
 	kfree_skb(skb);
	return 0;
}
#endif

struct dst_entry *__xfrm_dst_lookup(struct net *net, int tos, int oif,
				    const xfrm_address_t *saddr,
				    const xfrm_address_t *daddr,
				    int family, u32 mark);

struct xfrm_policy *xfrm_policy_alloc(struct net *net, gfp_t gfp);

void xfrm_policy_walk_init(struct xfrm_policy_walk *walk, u8 type);
int xfrm_policy_walk(struct net *net, struct xfrm_policy_walk *walk,
		     int (*func)(struct xfrm_policy *, int, int, void*),
		     void *);
void xfrm_policy_walk_done(struct xfrm_policy_walk *walk, struct net *net);
int xfrm_policy_insert(int dir, struct xfrm_policy *policy, int excl);
struct xfrm_policy *xfrm_policy_bysel_ctx(struct net *net, u32 mark, u32 if_id,
					  u8 type, int dir,
					  struct xfrm_selector *sel,
					  struct xfrm_sec_ctx *ctx, int delete,
					  int *err);
struct xfrm_policy *xfrm_policy_byid(struct net *net, u32 mark, u32 if_id, u8,
				     int dir, u32 id, int delete, int *err);
int xfrm_policy_flush(struct net *net, u8 type, bool task_valid);
void xfrm_policy_hash_rebuild(struct net *net);
u32 xfrm_get_acqseq(void);
int verify_spi_info(u8 proto, u32 min, u32 max);
int xfrm_alloc_spi(struct xfrm_state *x, u32 minspi, u32 maxspi);
struct xfrm_state *xfrm_find_acq(struct net *net, const struct xfrm_mark *mark,
				 u8 mode, u32 reqid, u32 if_id, u8 proto,
				 const xfrm_address_t *daddr,
				 const xfrm_address_t *saddr, int create,
				 unsigned short family);
int xfrm_sk_policy_insert(struct sock *sk, int dir, struct xfrm_policy *pol);

#ifdef CONFIG_XFRM_MIGRATE
int km_migrate(const struct xfrm_selector *sel, u8 dir, u8 type,
	       const struct xfrm_migrate *m, int num_bundles,
	       const struct xfrm_kmaddress *k,
	       const struct xfrm_encap_tmpl *encap);
struct xfrm_state *xfrm_migrate_state_find(struct xfrm_migrate *m, struct net *net);
struct xfrm_state *xfrm_state_migrate(struct xfrm_state *x,
				      struct xfrm_migrate *m,
				      struct xfrm_encap_tmpl *encap);
int xfrm_migrate(const struct xfrm_selector *sel, u8 dir, u8 type,
		 struct xfrm_migrate *m, int num_bundles,
		 struct xfrm_kmaddress *k, struct net *net,
		 struct xfrm_encap_tmpl *encap);
#endif

int km_new_mapping(struct xfrm_state *x, xfrm_address_t *ipaddr, __be16 sport);
void km_policy_expired(struct xfrm_policy *pol, int dir, int hard, u32 portid);
int km_report(struct net *net, u8 proto, struct xfrm_selector *sel,
	      xfrm_address_t *addr);

void xfrm_input_init(void);
int xfrm_parse_spi(struct sk_buff *skb, u8 nexthdr, __be32 *spi, __be32 *seq);

void xfrm_probe_algs(void);
int xfrm_count_pfkey_auth_supported(void);
int xfrm_count_pfkey_enc_supported(void);
struct xfrm_algo_desc *xfrm_aalg_get_byidx(unsigned int idx);
struct xfrm_algo_desc *xfrm_ealg_get_byidx(unsigned int idx);
struct xfrm_algo_desc *xfrm_aalg_get_byid(int alg_id);
struct xfrm_algo_desc *xfrm_ealg_get_byid(int alg_id);
struct xfrm_algo_desc *xfrm_calg_get_byid(int alg_id);
struct xfrm_algo_desc *xfrm_aalg_get_byname(const char *name, int probe);
struct xfrm_algo_desc *xfrm_ealg_get_byname(const char *name, int probe);
struct xfrm_algo_desc *xfrm_calg_get_byname(const char *name, int probe);
struct xfrm_algo_desc *xfrm_aead_get_byname(const char *name, int icv_len,
					    int probe);

static inline bool xfrm6_addr_equal(const xfrm_address_t *a,
				    const xfrm_address_t *b)
{
	return ipv6_addr_equal((const struct in6_addr *)a,
			       (const struct in6_addr *)b);
}

static inline bool xfrm_addr_equal(const xfrm_address_t *a,
				   const xfrm_address_t *b,
				   sa_family_t family)
{
	switch (family) {
	default:
	case AF_INET:
		return ((__force u32)a->a4 ^ (__force u32)b->a4) == 0;
	case AF_INET6:
		return xfrm6_addr_equal(a, b);
	}
}

static inline int xfrm_policy_id2dir(u32 index)
{
	return index & 7;
}

#ifdef CONFIG_XFRM
static inline int xfrm_aevent_is_on(struct net *net)
{
	struct sock *nlsk;
	int ret = 0;

	rcu_read_lock();
	nlsk = rcu_dereference(net->xfrm.nlsk);
	if (nlsk)
		ret = netlink_has_listeners(nlsk, XFRMNLGRP_AEVENTS);
	rcu_read_unlock();
	return ret;
}

static inline int xfrm_acquire_is_on(struct net *net)
{
	struct sock *nlsk;
	int ret = 0;

	rcu_read_lock();
	nlsk = rcu_dereference(net->xfrm.nlsk);
	if (nlsk)
		ret = netlink_has_listeners(nlsk, XFRMNLGRP_ACQUIRE);
	rcu_read_unlock();

	return ret;
}
#endif

static inline unsigned int aead_len(struct xfrm_algo_aead *alg)
{
	return sizeof(*alg) + ((alg->alg_key_len + 7) / 8);
}

static inline unsigned int xfrm_alg_len(const struct xfrm_algo *alg)
{
	return sizeof(*alg) + ((alg->alg_key_len + 7) / 8);
}

static inline unsigned int xfrm_alg_auth_len(const struct xfrm_algo_auth *alg)
{
	return sizeof(*alg) + ((alg->alg_key_len + 7) / 8);
}

static inline unsigned int xfrm_replay_state_esn_len(struct xfrm_replay_state_esn *replay_esn)
{
	return sizeof(*replay_esn) + replay_esn->bmp_len * sizeof(__u32);
}

#ifdef CONFIG_XFRM_MIGRATE
static inline int xfrm_replay_clone(struct xfrm_state *x,
				     struct xfrm_state *orig)
{
	x->replay_esn = kzalloc(xfrm_replay_state_esn_len(orig->replay_esn),
				GFP_KERNEL);
	if (!x->replay_esn)
		return -ENOMEM;

	x->replay_esn->bmp_len = orig->replay_esn->bmp_len;
	x->replay_esn->replay_window = orig->replay_esn->replay_window;

	x->preplay_esn = kmemdup(x->replay_esn,
				 xfrm_replay_state_esn_len(x->replay_esn),
				 GFP_KERNEL);
	if (!x->preplay_esn) {
		kfree(x->replay_esn);
		return -ENOMEM;
	}

	return 0;
}

static inline struct xfrm_algo_aead *xfrm_algo_aead_clone(struct xfrm_algo_aead *orig)
{
	return kmemdup(orig, aead_len(orig), GFP_KERNEL);
}


static inline struct xfrm_algo *xfrm_algo_clone(struct xfrm_algo *orig)
{
	return kmemdup(orig, xfrm_alg_len(orig), GFP_KERNEL);
}

static inline struct xfrm_algo_auth *xfrm_algo_auth_clone(struct xfrm_algo_auth *orig)
{
	return kmemdup(orig, xfrm_alg_auth_len(orig), GFP_KERNEL);
}

static inline void xfrm_states_put(struct xfrm_state **states, int n)
{
	int i;
	for (i = 0; i < n; i++)
		xfrm_state_put(*(states + i));
}

static inline void xfrm_states_delete(struct xfrm_state **states, int n)
{
	int i;
	for (i = 0; i < n; i++)
		xfrm_state_delete(*(states + i));
}
#endif

#ifdef CONFIG_XFRM
static inline struct xfrm_state *xfrm_input_state(struct sk_buff *skb)
{
	return skb->sp->xvec[skb->sp->len - 1];
}
#endif

static inline struct xfrm_offload *xfrm_offload(struct sk_buff *skb)
{
#ifdef CONFIG_XFRM
	struct sec_path *sp = skb->sp;

	if (!sp || !sp->olen || sp->len != sp->olen)
		return NULL;

	return &sp->ovec[sp->olen - 1];
#else
	return NULL;
#endif
}

void __init xfrm_dev_init(void);

#ifdef CONFIG_XFRM_OFFLOAD
void xfrm_dev_resume(struct sk_buff *skb);
void xfrm_dev_backlog(struct softnet_data *sd);
struct sk_buff *validate_xmit_xfrm(struct sk_buff *skb, netdev_features_t features, bool *again);
int xfrm_dev_state_add(struct net *net, struct xfrm_state *x,
		       struct xfrm_user_offload *xuo);
bool xfrm_dev_offload_ok(struct sk_buff *skb, struct xfrm_state *x);

static inline void xfrm_dev_state_advance_esn(struct xfrm_state *x)
{
	struct xfrm_state_offload *xso = &x->xso;

	if (xso->dev && xso->dev->xfrmdev_ops->xdo_dev_state_advance_esn)
		xso->dev->xfrmdev_ops->xdo_dev_state_advance_esn(x);
}

static inline bool xfrm_dst_offload_ok(struct dst_entry *dst)
{
	struct xfrm_state *x = dst->xfrm;
	struct xfrm_dst *xdst;

	if (!x || !x->type_offload)
		return false;

	xdst = (struct xfrm_dst *) dst;
	if (!x->xso.offload_handle && !xdst->child->xfrm)
		return true;
	if (x->xso.offload_handle && (x->xso.dev == xfrm_dst_path(dst)->dev) &&
	    !xdst->child->xfrm)
		return true;

	return false;
}

static inline void xfrm_dev_state_delete(struct xfrm_state *x)
{
	struct xfrm_state_offload *xso = &x->xso;

	if (xso->dev)
		xso->dev->xfrmdev_ops->xdo_dev_state_delete(x);
}

static inline void xfrm_dev_state_free(struct xfrm_state *x)
{
	struct xfrm_state_offload *xso = &x->xso;
	 struct net_device *dev = xso->dev;

	if (dev && dev->xfrmdev_ops) {
		if (dev->xfrmdev_ops->xdo_dev_state_free)
			dev->xfrmdev_ops->xdo_dev_state_free(x);
		xso->dev = NULL;
		dev_put(dev);
	}
}
#else
static inline void xfrm_dev_resume(struct sk_buff *skb)
{
}

static inline void xfrm_dev_backlog(struct softnet_data *sd)
{
}

static inline struct sk_buff *validate_xmit_xfrm(struct sk_buff *skb, netdev_features_t features, bool *again)
{
	return skb;
}

static inline int xfrm_dev_state_add(struct net *net, struct xfrm_state *x, struct xfrm_user_offload *xuo)
{
	return 0;
}

static inline void xfrm_dev_state_delete(struct xfrm_state *x)
{
}

static inline void xfrm_dev_state_free(struct xfrm_state *x)
{
}

static inline bool xfrm_dev_offload_ok(struct sk_buff *skb, struct xfrm_state *x)
{
	return false;
}

static inline void xfrm_dev_state_advance_esn(struct xfrm_state *x)
{
}

static inline bool xfrm_dst_offload_ok(struct dst_entry *dst)
{
	return false;
}
#endif

static inline int xfrm_mark_get(struct nlattr **attrs, struct xfrm_mark *m)
{
	if (attrs[XFRMA_MARK])
		memcpy(m, nla_data(attrs[XFRMA_MARK]), sizeof(struct xfrm_mark));
	else
		m->v = m->m = 0;

	return m->v & m->m;
}

static inline int xfrm_mark_put(struct sk_buff *skb, const struct xfrm_mark *m)
{
	int ret = 0;

	if (m->m | m->v)
		ret = nla_put(skb, XFRMA_MARK, sizeof(struct xfrm_mark), m);
	return ret;
}

static inline __u32 xfrm_smark_get(__u32 mark, struct xfrm_state *x)
{
	struct xfrm_mark *m = &x->props.smark;

	return (m->v & m->m) | (mark & ~m->m);
}

static inline int xfrm_if_id_put(struct sk_buff *skb, __u32 if_id)
{
	int ret = 0;

	if (if_id)
		ret = nla_put_u32(skb, XFRMA_IF_ID, if_id);
	return ret;
}

static inline int xfrm_tunnel_check(struct sk_buff *skb, struct xfrm_state *x,
				    unsigned int family)
{
	bool tunnel = false;

	switch(family) {
	case AF_INET:
		if (XFRM_TUNNEL_SKB_CB(skb)->tunnel.ip4)
			tunnel = true;
		break;
	case AF_INET6:
		if (XFRM_TUNNEL_SKB_CB(skb)->tunnel.ip6)
			tunnel = true;
		break;
	}
	if (tunnel && !(x->outer_mode->flags & XFRM_MODE_FLAG_TUNNEL))
		return -EINVAL;

	return 0;
}
#endif	/* _NET_XFRM_H */
