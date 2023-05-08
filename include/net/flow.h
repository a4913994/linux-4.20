/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 *	Generic internet FLOW.
 *
 */

#ifndef _NET_FLOW_H
#define _NET_FLOW_H

#include <linux/socket.h>
#include <linux/in6.h>
#include <linux/atomic.h>
#include <net/flow_dissector.h>
#include <linux/uidgid.h>

/*
 * ifindex generation is per-net namespace, and loopback is
 * always the 1st device in ns (see net_dev_init), thus any
 * loopback device should get ifindex 1
 */

#define LOOPBACK_IFINDEX	1

struct flowi_tunnel {
	__be64			tun_id;
};
// flowi_common 结构包含一些标识一个特定流程的公共属性
struct flowi_common {
	// flowic_oif：输出网络接口的索引。
	int	flowic_oif;
	// flowic_oif：输出网络接口的索引。
	int	flowic_iif;
	// flowic_mark：用于标记此流的值。这个值可以用作策略路由和过滤。
	__u32	flowic_mark;
	// flowic_tos：流的Type of Service，定义了服务的优先级。
	__u8	flowic_tos;
	// flowic_scope：流的网络范围，例如全局、主机等。
	__u8	flowic_scope;
	// flowic_proto：流使用的协议，例如TCP、UDP等。
	__u8	flowic_proto;
	// flowic_flags：内核用于处理流的一组标志位。例如：
	__u8	flowic_flags;
// FLOWI_FLAG_ANYSRC：表示可以使用任意源地址。
#define FLOWI_FLAG_ANYSRC		0x01
// FLOWI_FLAG_KNOWN_NH：表示已知的下一跳。
#define FLOWI_FLAG_KNOWN_NH		0x02
// FLOWI_FLAG_SKIP_NH_OIF：表示跳过下一跳输出网络接口。
#define FLOWI_FLAG_SKIP_NH_OIF		0x04
	// flowic_secid：用于指示安全策略的安全ID。
	__u32	flowic_secid;
	// flowic_tun_key：流对应的隧道键，用于隧道数据报文的封装和解封装。
	struct flowi_tunnel flowic_tun_key;
	// flowic_uid：与此流关联的用户ID。
	kuid_t  flowic_uid;
};

// flowi_uli 结构则是一个联合体（union），表示协议特定信息。这个结构允许多种不同类型的协议使用相同的内存空间。根据传输或其他协议的需求
union flowi_uli {
	// ports：用于TCP或UDP流，包含源端口（sport）和目标端口（dport）。
	struct {
		__be16	dport;
		__be16	sport;
	} ports;

	// icmpt：用于ICMP流，包含ICMP类型（type）和代码（code）。
	struct {
		__u8	type;
		__u8	code;
	} icmpt;

	// dnports：用于DECnet协议流，包含源端口（sport）和目标端口（dport）。
	struct {
		__le16	dport;
		__le16	sport;
	} dnports;

	// spi：表示安全参数索引，用于IPSec流。
	__be32		spi;
	// gre_key：用于GRE隧道流，表示GRE密钥。
	__be32		gre_key;

	// mht：用于Mobility Header协议（用于IPv6移动性），表示Mobility Header类型（type）。
	struct {
		__u8	type;
	} mht;
};

// flowi4 是一个用于保存与 IPv4 相关的流信息（流由源地址和目标地址组成）的数据结构
struct flowi4 {
	struct flowi_common	__fl_common;
#define flowi4_oif		__fl_common.flowic_oif
#define flowi4_iif		__fl_common.flowic_iif
#define flowi4_mark		__fl_common.flowic_mark
#define flowi4_tos		__fl_common.flowic_tos
#define flowi4_scope		__fl_common.flowic_scope
#define flowi4_proto		__fl_common.flowic_proto
#define flowi4_flags		__fl_common.flowic_flags
#define flowi4_secid		__fl_common.flowic_secid
#define flowi4_tun_key		__fl_common.flowic_tun_key
#define flowi4_uid		__fl_common.flowic_uid

	/* (saddr,daddr) must be grouped, same order as in IP header */
	// saddr;：源 IPv4 地址，以网络字节序表示。
	__be32			saddr;
	// daddr：目标 IPv4 地址，以网络字节序表示。
	__be32			daddr;

	union flowi_uli		uli;
#define fl4_sport		uli.ports.sport
#define fl4_dport		uli.ports.dport
#define fl4_icmp_type		uli.icmpt.type
#define fl4_icmp_code		uli.icmpt.code
#define fl4_ipsec_spi		uli.spi
#define fl4_mh_type		uli.mht.type
#define fl4_gre_key		uli.gre_key
} __attribute__((__aligned__(BITS_PER_LONG/8)));

static inline void flowi4_init_output(struct flowi4 *fl4, int oif,
				      __u32 mark, __u8 tos, __u8 scope,
				      __u8 proto, __u8 flags,
				      __be32 daddr, __be32 saddr,
				      __be16 dport, __be16 sport,
				      kuid_t uid)
{
	fl4->flowi4_oif = oif;
	fl4->flowi4_iif = LOOPBACK_IFINDEX;
	fl4->flowi4_mark = mark;
	fl4->flowi4_tos = tos;
	fl4->flowi4_scope = scope;
	fl4->flowi4_proto = proto;
	fl4->flowi4_flags = flags;
	fl4->flowi4_secid = 0;
	fl4->flowi4_tun_key.tun_id = 0;
	fl4->flowi4_uid = uid;
	fl4->daddr = daddr;
	fl4->saddr = saddr;
	fl4->fl4_dport = dport;
	fl4->fl4_sport = sport;
}

/* Reset some input parameters after previous lookup */
static inline void flowi4_update_output(struct flowi4 *fl4, int oif, __u8 tos,
					__be32 daddr, __be32 saddr)
{
	fl4->flowi4_oif = oif;
	fl4->flowi4_tos = tos;
	fl4->daddr = daddr;
	fl4->saddr = saddr;
}


struct flowi6 {
	struct flowi_common	__fl_common;
#define flowi6_oif		__fl_common.flowic_oif
#define flowi6_iif		__fl_common.flowic_iif
#define flowi6_mark		__fl_common.flowic_mark
#define flowi6_scope		__fl_common.flowic_scope
#define flowi6_proto		__fl_common.flowic_proto
#define flowi6_flags		__fl_common.flowic_flags
#define flowi6_secid		__fl_common.flowic_secid
#define flowi6_tun_key		__fl_common.flowic_tun_key
#define flowi6_uid		__fl_common.flowic_uid
	struct in6_addr		daddr;
	struct in6_addr		saddr;
	/* Note: flowi6_tos is encoded in flowlabel, too. */
	__be32			flowlabel;
	union flowi_uli		uli;
#define fl6_sport		uli.ports.sport
#define fl6_dport		uli.ports.dport
#define fl6_icmp_type		uli.icmpt.type
#define fl6_icmp_code		uli.icmpt.code
#define fl6_ipsec_spi		uli.spi
#define fl6_mh_type		uli.mht.type
#define fl6_gre_key		uli.gre_key
	__u32			mp_hash;
} __attribute__((__aligned__(BITS_PER_LONG/8)));

struct flowidn {
	struct flowi_common	__fl_common;
#define flowidn_oif		__fl_common.flowic_oif
#define flowidn_iif		__fl_common.flowic_iif
#define flowidn_mark		__fl_common.flowic_mark
#define flowidn_scope		__fl_common.flowic_scope
#define flowidn_proto		__fl_common.flowic_proto
#define flowidn_flags		__fl_common.flowic_flags
	__le16			daddr;
	__le16			saddr;
	union flowi_uli		uli;
#define fld_sport		uli.ports.sport
#define fld_dport		uli.ports.dport
} __attribute__((__aligned__(BITS_PER_LONG/8)));

struct flowi {
	union {
		struct flowi_common	__fl_common;
		struct flowi4		ip4;
		struct flowi6		ip6;
		struct flowidn		dn;
	} u;
#define flowi_oif	u.__fl_common.flowic_oif
#define flowi_iif	u.__fl_common.flowic_iif
#define flowi_mark	u.__fl_common.flowic_mark
#define flowi_tos	u.__fl_common.flowic_tos
#define flowi_scope	u.__fl_common.flowic_scope
#define flowi_proto	u.__fl_common.flowic_proto
#define flowi_flags	u.__fl_common.flowic_flags
#define flowi_secid	u.__fl_common.flowic_secid
#define flowi_tun_key	u.__fl_common.flowic_tun_key
#define flowi_uid	u.__fl_common.flowic_uid
} __attribute__((__aligned__(BITS_PER_LONG/8)));

static inline struct flowi *flowi4_to_flowi(struct flowi4 *fl4)
{
	return container_of(fl4, struct flowi, u.ip4);
}

static inline struct flowi *flowi6_to_flowi(struct flowi6 *fl6)
{
	return container_of(fl6, struct flowi, u.ip6);
}

static inline struct flowi *flowidn_to_flowi(struct flowidn *fldn)
{
	return container_of(fldn, struct flowi, u.dn);
}

typedef unsigned long flow_compare_t;

static inline unsigned int flow_key_size(u16 family)
{
	switch (family) {
	case AF_INET:
		BUILD_BUG_ON(sizeof(struct flowi4) % sizeof(flow_compare_t));
		return sizeof(struct flowi4) / sizeof(flow_compare_t);
	case AF_INET6:
		BUILD_BUG_ON(sizeof(struct flowi6) % sizeof(flow_compare_t));
		return sizeof(struct flowi6) / sizeof(flow_compare_t);
	case AF_DECnet:
		BUILD_BUG_ON(sizeof(struct flowidn) % sizeof(flow_compare_t));
		return sizeof(struct flowidn) / sizeof(flow_compare_t);
	}
	return 0;
}

__u32 __get_hash_from_flowi6(const struct flowi6 *fl6, struct flow_keys *keys);

#endif
