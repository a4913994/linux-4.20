/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_NETFILTER_H
#define __LINUX_NETFILTER_H

#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/static_key.h>
#include <linux/netfilter_defs.h>
#include <linux/netdevice.h>
#include <net/net_namespace.h>

#ifdef CONFIG_NETFILTER
static inline int NF_DROP_GETERR(int verdict)
{
	return -(verdict >> NF_VERDICT_QBITS);
}

static inline int nf_inet_addr_cmp(const union nf_inet_addr *a1,
				   const union nf_inet_addr *a2)
{
	return a1->all[0] == a2->all[0] &&
	       a1->all[1] == a2->all[1] &&
	       a1->all[2] == a2->all[2] &&
	       a1->all[3] == a2->all[3];
}

static inline void nf_inet_addr_mask(const union nf_inet_addr *a1,
				     union nf_inet_addr *result,
				     const union nf_inet_addr *mask)
{
	result->all[0] = a1->all[0] & mask->all[0];
	result->all[1] = a1->all[1] & mask->all[1];
	result->all[2] = a1->all[2] & mask->all[2];
	result->all[3] = a1->all[3] & mask->all[3];
}

int netfilter_init(void);

struct sk_buff;

struct nf_hook_ops;

struct sock;

// nf_hook_state结构是Linux内核中用于保存Netfilter钩子状态信息的结构体。
// 它包含了关于网络数据包的处理阶段、协议族、输入/输出设备、套接字、网络命名空间等关键信息，并允许向下传递数据包的回调函数。
struct nf_hook_state {
	// hook: 表示Netfilter钩子点的类型，如NF_INET_PRE_ROUTING、NF_INET_LOCAL_IN等。它决定了回调函数被调用的网络数据包处理阶段
	unsigned int hook;
	// pf: 表示协议族（protocol family），例如PF_INET（对应IPv4）、PF_INET6（对应IPv6）等。
	u_int8_t pf;
	// *in: 是一个指向输入网络设备结构体的指针，表示数据包从哪个设备接收到。对于进入本地系统的数据包，这个字段有意义。
	struct net_device *in;
	// *out: 是一个指向输出网络设备结构体的指针，表示数据包将通过哪个设备发送出去。对于离开本地系统的数据包，这个字段有意义
	struct net_device *out;
	// sock *sk: 是一个指向与数据包关联的套接字（socket）结构体的指针。
	struct sock *sk;
	// *net: 是一个指向与数据包关联的网络命名空间（network namespace）的指针。网络命名空间是Linux内核中用于隔离进程组的网络资源，如套接字、接口等
	struct net *net;
	// 这是一个回调函数指针。在网络过滤框架处理完数据包并允许它继续向下传输时，这个函数被调用。传入的参数分别是网络命名空间、套接字和包含网络数据包的sk_buff结构体。
	int (*okfn)(struct net *, struct sock *, struct sk_buff *);
};

typedef unsigned int nf_hookfn(void *priv,
			       struct sk_buff *skb,
			       const struct nf_hook_state *state);
// struct nf_hook_ops结构体是用于在netfilter子系统中注册自定义数据包处理函数（钩子函数）的必要信息。
// 钩子函数将依据指定的协议族、挂载位置和优先级在适当的时机处理网络数据包。
struct nf_hook_ops {
	/* User fills in from here down. */
	// nf_hookfn是一个类型，表示一个钩子函数的原型。该钩子函数在内核定义的指定钩子点被调用。
	nf_hookfn		*hook;
	// 该指针指向注册钩子的网络设备。如果该钩子适用于所有网络设备，则此项应为NULL。
	struct net_device	*dev;
	// 钩子函数可以存储私有数据，例如表示其内部状态的结构。这些数据可以由priv字段传递给钩子函数
	void			*priv;
	// 这是协议族标志，例如PF_INET（IPv4）或PF_INET6（IPv6），用于指定钩子操作适用于哪种协议族的数据包
	u_int8_t		pf;
	// 该字段描述了钩子操作所挂载的位置。在netfilter系统中存在5个预定义的位置，分别为NF_INET_PRE_ROUTING（在路由前）、NF_INET_LOCAL_IN（在到达本地socket之前）、
	// NF_INET_FORWARD（在转发过程中）、NF_INET_LOCAL_OUT（在离开本地socket之后）和NF_INET_POST_ROUTING（在路由后）。这些预定义的位置允许开发人员根据不同钩子点自定义不同的过滤规则。
	unsigned int		hooknum;
	/* Hooks are ordered in ascending priority. */
	// 字段定义了钩子操作在给定钩子点处的执行顺序。优先级越高（数值越低），钩子函数越早被执行。这允许实现不同的过滤规则，以便处理不同的情况。
	int			priority;
};

struct nf_hook_entry {
	nf_hookfn			*hook;
	void				*priv;
};

struct nf_hook_entries_rcu_head {
	struct rcu_head head;
	void	*allocation;
};

struct nf_hook_entries {
	u16				num_hook_entries;
	/* padding */
	struct nf_hook_entry		hooks[];

	/* trailer: pointers to original orig_ops of each hook,
	 * followed by rcu_head and scratch space used for freeing
	 * the structure via call_rcu.
	 *
	 *   This is not part of struct nf_hook_entry since its only
	 *   needed in slow path (hook register/unregister):
	 * const struct nf_hook_ops     *orig_ops[]
	 *
	 *   For the same reason, we store this at end -- its
	 *   only needed when a hook is deleted, not during
	 *   packet path processing:
	 * struct nf_hook_entries_rcu_head     head
	 */
};

static inline struct nf_hook_ops **nf_hook_entries_get_hook_ops(const struct nf_hook_entries *e)
{
	unsigned int n = e->num_hook_entries;
	const void *hook_end;

	hook_end = &e->hooks[n]; /* this is *past* ->hooks[]! */

	return (struct nf_hook_ops **)hook_end;
}

static inline int
nf_hook_entry_hookfn(const struct nf_hook_entry *entry, struct sk_buff *skb,
		     struct nf_hook_state *state)
{
	return entry->hook(entry->priv, skb, state);
}

static inline void nf_hook_state_init(struct nf_hook_state *p,
				      unsigned int hook,
				      u_int8_t pf,
				      struct net_device *indev,
				      struct net_device *outdev,
				      struct sock *sk,
				      struct net *net,
				      int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	p->hook = hook;
	p->pf = pf;
	p->in = indev;
	p->out = outdev;
	p->sk = sk;
	p->net = net;
	p->okfn = okfn;
}



struct nf_sockopt_ops {
	struct list_head list;

	u_int8_t pf;

	/* Non-inclusive ranges: use 0/0/NULL to never get called. */
	int set_optmin;
	int set_optmax;
	int (*set)(struct sock *sk, int optval, void __user *user, unsigned int len);
#ifdef CONFIG_COMPAT
	int (*compat_set)(struct sock *sk, int optval,
			void __user *user, unsigned int len);
#endif
	int get_optmin;
	int get_optmax;
	int (*get)(struct sock *sk, int optval, void __user *user, int *len);
#ifdef CONFIG_COMPAT
	int (*compat_get)(struct sock *sk, int optval,
			void __user *user, int *len);
#endif
	/* Use the module struct to lock set/get code in place */
	struct module *owner;
};

/* Function to register/unregister hook points. */
int nf_register_net_hook(struct net *net, const struct nf_hook_ops *ops);
void nf_unregister_net_hook(struct net *net, const struct nf_hook_ops *ops);
int nf_register_net_hooks(struct net *net, const struct nf_hook_ops *reg,
			  unsigned int n);
void nf_unregister_net_hooks(struct net *net, const struct nf_hook_ops *reg,
			     unsigned int n);

/* Functions to register get/setsockopt ranges (non-inclusive).  You
   need to check permissions yourself! */
int nf_register_sockopt(struct nf_sockopt_ops *reg);
void nf_unregister_sockopt(struct nf_sockopt_ops *reg);

#ifdef HAVE_JUMP_LABEL
extern struct static_key nf_hooks_needed[NFPROTO_NUMPROTO][NF_MAX_HOOKS];
#endif

int nf_hook_slow(struct sk_buff *skb, struct nf_hook_state *state,
		 const struct nf_hook_entries *e, unsigned int i);

/**
 *	nf_hook - call a netfilter hook
 *
 *	Returns 1 if the hook has allowed the packet to pass.  The function
 *	okfn must be invoked by the caller in this case.  Any other return
 *	value indicates the packet has been consumed by the hook.
 */
static inline int nf_hook(u_int8_t pf, unsigned int hook, struct net *net,
			  struct sock *sk, struct sk_buff *skb,
			  struct net_device *indev, struct net_device *outdev,
			  int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	struct nf_hook_entries *hook_head = NULL;
	int ret = 1;

#ifdef HAVE_JUMP_LABEL
	if (__builtin_constant_p(pf) &&
	    __builtin_constant_p(hook) &&
	    !static_key_false(&nf_hooks_needed[pf][hook]))
		return 1;
#endif

	rcu_read_lock();
	switch (pf) {
	case NFPROTO_IPV4:
		hook_head = rcu_dereference(net->nf.hooks_ipv4[hook]);
		break;
	case NFPROTO_IPV6:
		hook_head = rcu_dereference(net->nf.hooks_ipv6[hook]);
		break;
	case NFPROTO_ARP:
#ifdef CONFIG_NETFILTER_FAMILY_ARP
		if (WARN_ON_ONCE(hook >= ARRAY_SIZE(net->nf.hooks_arp)))
			break;
		hook_head = rcu_dereference(net->nf.hooks_arp[hook]);
#endif
		break;
	case NFPROTO_BRIDGE:
#ifdef CONFIG_NETFILTER_FAMILY_BRIDGE
		hook_head = rcu_dereference(net->nf.hooks_bridge[hook]);
#endif
		break;
#if IS_ENABLED(CONFIG_DECNET)
	case NFPROTO_DECNET:
		hook_head = rcu_dereference(net->nf.hooks_decnet[hook]);
		break;
#endif
	default:
		WARN_ON_ONCE(1);
		break;
	}

	if (hook_head) {
		struct nf_hook_state state;

		nf_hook_state_init(&state, hook, pf, indev, outdev,
				   sk, net, okfn);

		ret = nf_hook_slow(skb, &state, hook_head, 0);
	}
	rcu_read_unlock();

	return ret;
}

/* Activate hook; either okfn or kfree_skb called, unless a hook
   returns NF_STOLEN (in which case, it's up to the hook to deal with
   the consequences).

   Returns -ERRNO if packet dropped.  Zero means queued, stolen or
   accepted.
*/

/* RR:
   > I don't want nf_hook to return anything because people might forget
   > about async and trust the return value to mean "packet was ok".

   AK:
   Just document it clearly, then you can expect some sense from kernel
   coders :)
*/

static inline int
NF_HOOK_COND(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	     struct sk_buff *skb, struct net_device *in, struct net_device *out,
	     int (*okfn)(struct net *, struct sock *, struct sk_buff *),
	     bool cond)
{
	int ret;

	if (!cond ||
	    ((ret = nf_hook(pf, hook, net, sk, skb, in, out, okfn)) == 1))
		ret = okfn(net, sk, skb);
	return ret;
}

static inline int
NF_HOOK(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk, struct sk_buff *skb,
	struct net_device *in, struct net_device *out,
	int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	int ret = nf_hook(pf, hook, net, sk, skb, in, out, okfn);
	if (ret == 1)
		ret = okfn(net, sk, skb);
	return ret;
}

static inline void
NF_HOOK_LIST(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	     struct list_head *head, struct net_device *in, struct net_device *out,
	     int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	struct sk_buff *skb, *next;
	struct list_head sublist;

	INIT_LIST_HEAD(&sublist);
	list_for_each_entry_safe(skb, next, head, list) {
		list_del(&skb->list);
		if (nf_hook(pf, hook, net, sk, skb, in, out, okfn) == 1)
			list_add_tail(&skb->list, &sublist);
	}
	/* Put passed packets back on main list */
	list_splice(&sublist, head);
}

/* Call setsockopt() */
int nf_setsockopt(struct sock *sk, u_int8_t pf, int optval, char __user *opt,
		  unsigned int len);
int nf_getsockopt(struct sock *sk, u_int8_t pf, int optval, char __user *opt,
		  int *len);
#ifdef CONFIG_COMPAT
int compat_nf_setsockopt(struct sock *sk, u_int8_t pf, int optval,
		char __user *opt, unsigned int len);
int compat_nf_getsockopt(struct sock *sk, u_int8_t pf, int optval,
		char __user *opt, int *len);
#endif

/* Call this before modifying an existing packet: ensures it is
   modifiable and linear to the point you care about (writable_len).
   Returns true or false. */
int skb_make_writable(struct sk_buff *skb, unsigned int writable_len);

struct flowi;
struct nf_queue_entry;

__sum16 nf_checksum(struct sk_buff *skb, unsigned int hook,
		    unsigned int dataoff, u_int8_t protocol,
		    unsigned short family);

__sum16 nf_checksum_partial(struct sk_buff *skb, unsigned int hook,
			    unsigned int dataoff, unsigned int len,
			    u_int8_t protocol, unsigned short family);
int nf_route(struct net *net, struct dst_entry **dst, struct flowi *fl,
	     bool strict, unsigned short family);
int nf_reroute(struct sk_buff *skb, struct nf_queue_entry *entry);

#include <net/flow.h>

struct nf_conn;
enum nf_nat_manip_type;
struct nlattr;
enum ip_conntrack_dir;

struct nf_nat_hook {
	int (*parse_nat_setup)(struct nf_conn *ct, enum nf_nat_manip_type manip,
			       const struct nlattr *attr);
	void (*decode_session)(struct sk_buff *skb, struct flowi *fl);
	unsigned int (*manip_pkt)(struct sk_buff *skb, struct nf_conn *ct,
				  enum nf_nat_manip_type mtype,
				  enum ip_conntrack_dir dir);
};

extern struct nf_nat_hook __rcu *nf_nat_hook;

static inline void
nf_nat_decode_session(struct sk_buff *skb, struct flowi *fl, u_int8_t family)
{
#ifdef CONFIG_NF_NAT_NEEDED
	struct nf_nat_hook *nat_hook;

	rcu_read_lock();
	nat_hook = rcu_dereference(nf_nat_hook);
	if (nat_hook && nat_hook->decode_session)
		nat_hook->decode_session(skb, fl);
	rcu_read_unlock();
#endif
}

#else /* !CONFIG_NETFILTER */
static inline int
NF_HOOK_COND(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	     struct sk_buff *skb, struct net_device *in, struct net_device *out,
	     int (*okfn)(struct net *, struct sock *, struct sk_buff *),
	     bool cond)
{
	return okfn(net, sk, skb);
}

static inline int
NF_HOOK(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	return okfn(net, sk, skb);
}

static inline void
NF_HOOK_LIST(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	     struct list_head *head, struct net_device *in, struct net_device *out,
	     int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	/* nothing to do */
}

static inline int nf_hook(u_int8_t pf, unsigned int hook, struct net *net,
			  struct sock *sk, struct sk_buff *skb,
			  struct net_device *indev, struct net_device *outdev,
			  int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
	return 1;
}
struct flowi;
static inline void
nf_nat_decode_session(struct sk_buff *skb, struct flowi *fl, u_int8_t family)
{
}
#endif /*CONFIG_NETFILTER*/

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
#include <linux/netfilter/nf_conntrack_zones_common.h>

extern void (*ip_ct_attach)(struct sk_buff *, const struct sk_buff *) __rcu;
void nf_ct_attach(struct sk_buff *, const struct sk_buff *);
struct nf_conntrack_tuple;
bool nf_ct_get_tuple_skb(struct nf_conntrack_tuple *dst_tuple,
			 const struct sk_buff *skb);
#else
static inline void nf_ct_attach(struct sk_buff *new, struct sk_buff *skb) {}
struct nf_conntrack_tuple;
static inline bool nf_ct_get_tuple_skb(struct nf_conntrack_tuple *dst_tuple,
				       const struct sk_buff *skb)
{
	return false;
}
#endif

struct nf_conn;
enum ip_conntrack_info;

struct nf_ct_hook {
	int (*update)(struct net *net, struct sk_buff *skb);
	void (*destroy)(struct nf_conntrack *);
	bool (*get_tuple_skb)(struct nf_conntrack_tuple *,
			      const struct sk_buff *);
};
extern struct nf_ct_hook __rcu *nf_ct_hook;

struct nlattr;

struct nfnl_ct_hook {
	struct nf_conn *(*get_ct)(const struct sk_buff *skb,
				  enum ip_conntrack_info *ctinfo);
	size_t (*build_size)(const struct nf_conn *ct);
	int (*build)(struct sk_buff *skb, struct nf_conn *ct,
		     enum ip_conntrack_info ctinfo,
		     u_int16_t ct_attr, u_int16_t ct_info_attr);
	int (*parse)(const struct nlattr *attr, struct nf_conn *ct);
	int (*attach_expect)(const struct nlattr *attr, struct nf_conn *ct,
			     u32 portid, u32 report);
	void (*seq_adjust)(struct sk_buff *skb, struct nf_conn *ct,
			   enum ip_conntrack_info ctinfo, s32 off);
};
extern struct nfnl_ct_hook __rcu *nfnl_ct_hook;

/**
 * nf_skb_duplicated - TEE target has sent a packet
 *
 * When a xtables target sends a packet, the OUTPUT and POSTROUTING
 * hooks are traversed again, i.e. nft and xtables are invoked recursively.
 *
 * This is used by xtables TEE target to prevent the duplicated skb from
 * being duplicated again.
 */
DECLARE_PER_CPU(bool, nf_skb_duplicated);

#endif /*__LINUX_NETFILTER_H*/
