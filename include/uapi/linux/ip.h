/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the IP protocol.
 *
 * Version:	@(#)ip.h	1.0.2	04/28/93
 *
 * Authors:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _UAPI_LINUX_IP_H
#define _UAPI_LINUX_IP_H
#include <linux/types.h>
#include <asm/byteorder.h>

// 定义了一组标准的IPv4服务类型 (Type of Service, TOS) 宏。
// IPv4服务类型字段是一个8位字段，它表示了IP包的优先级和服务质量需求。
// 这些宏有助于处理这些Servics类型字段的选项
// IPTOS_TOS_MASK (0x1E): 一个掩码，用于从IPv4 TOS字段中提取服务类型 (TOS) 部分。
#define IPTOS_TOS_MASK		0x1E
// IPTOS_TOS(tos): 一个宏，提取给定TOS字段值的TOS部分
#define IPTOS_TOS(tos)		((tos)&IPTOS_TOS_MASK)
// IPTOS_LOWDELAY: 优先考虑低延迟。
#define	IPTOS_LOWDELAY		0x10
// IPTOS_THROUGHPUT: 优先考虑高吞吐量。
#define	IPTOS_THROUGHPUT	0x08
// IPTOS_RELIABILITY: 优先考虑高可靠性。
#define	IPTOS_RELIABILITY	0x04
// IPTOS_MINCOST: 优先考虑最小成本。
#define	IPTOS_MINCOST		0x02

// IPTOS_PREC_MASK (0xE0): 一个掩码，用于从Ipv4 TOS字段中提取优先级（precedence）部分。
#define IPTOS_PREC_MASK		0xE0
// IPTOS_PREC(tos): 一个宏，提取给定TOS字段值的优先级部分。
#define IPTOS_PREC(tos)		((tos)&IPTOS_PREC_MASK)
// IPTOS_PREC_NETCONTROL: 网络控制级别最大优先级。
#define IPTOS_PREC_NETCONTROL           0xe0
// IPTOS_PREC_INTERNETCONTROL: 网络管理通信优先级。
#define IPTOS_PREC_INTERNETCONTROL      0xc0
// IPTOS_PREC_CRITIC_ECP: 临界性能通知优先级。
#define IPTOS_PREC_CRITIC_ECP           0xa0
// IPTOS_PREC_FLASHOVERRIDE: 用于闪烁的优先级。
#define IPTOS_PREC_FLASHOVERRIDE        0x80
// IPTOS_PREC_FLASH: 用于现场响应优先级。
#define IPTOS_PREC_FLASH                0x60
// IPTOS_PREC_IMMEDIATE: 用于实时响应优先级。
#define IPTOS_PREC_IMMEDIATE            0x40
// IPTOS_PREC_PRIORITY: 用于综合指挥优先级。
#define IPTOS_PREC_PRIORITY             0x20
// IPTOS_PREC_ROUTINE: 用于经常出现的流量优先级。
#define IPTOS_PREC_ROUTINE              0x00


/* IP options */
// IPOPT_COPY (0x80) - 这是一个用于判断选项需要复制到所有数据包片段还是仅复制到不带选项的第一个片段的标记。
#define IPOPT_COPY		0x80
// IPOPT_CLASS_MASK (0x60) 和 IPOPT_NUMBER_MASK (0x1f) - 这两个值分别用于提取IPv4选项中的分类信息和选项编号的掩码。
#define IPOPT_CLASS_MASK	0x60
#define IPOPT_NUMBER_MASK	0x1f
// 它们之后与选项值进行位与操作，从而得到分类和编号字段：
// IPOPT_COPIED(o) - 根据选项值提取是否复制标志。
#define	IPOPT_COPIED(o)		((o)&IPOPT_COPY)
// IPOPT_CLASS(o) - 根据选项值提取选项分类。
#define	IPOPT_CLASS(o)		((o)&IPOPT_CLASS_MASK)
// IPOPT_NUMBER(o) - 根据选项值提取选项编号。
#define	IPOPT_NUMBER(o)		((o)&IPOPT_NUMBER_MASK)

// 选项分类常数定义:
// IPOPT_CONTROL (0x00) - 控制选项。
#define	IPOPT_CONTROL		0x00
// IPOPT_RESERVED1 (0x20) - 预留选项1。
#define	IPOPT_RESERVED1		0x20
// IPOPT_MEASUREMENT (0x40) - 测量选项。
#define	IPOPT_MEASUREMENT	0x40
// IPOPT_RESERVED2 (0x60) - 预留选项2。
#define	IPOPT_RESERVED2		0x60

// 各种IPv4选项的定义：
// IPOPT_END (0) - 数据包选项的结束标志。
#define IPOPT_END	(0 |IPOPT_CONTROL)
// IPOPT_NOOP (1) - "NO-OP"选项，用于填充数据包，但不执行任何Action。
#define IPOPT_NOOP	(1 |IPOPT_CONTROL)
// IPOPT_SEC (2) - 安全选项，用于标识数据包的安全等级。
#define IPOPT_SEC	(2 |IPOPT_CONTROL|IPOPT_COPY)
// IPOPT_LSRR (3) - Loose Source and Record Route（宽松源选路并记录路由)选项， 用于记录数据包经过的路由。
#define IPOPT_LSRR	(3 |IPOPT_CONTROL|IPOPT_COPY)
// IPOPT_TIMESTAMP (4) - 时间戳选项，用于在数据包中嵌入发送时间和接收时间。
#define IPOPT_TIMESTAMP	(4 |IPOPT_MEASUREMENT)
// IPOPT_CIPSO (6) - Commercial IP Security Option，用于支持商业IP安全。
#define IPOPT_CIPSO	(6 |IPOPT_CONTROL|IPOPT_COPY)
// IPOPT_RR (7) - Record Route（记录路由)选项，用于记录数据包经过的路由。
#define IPOPT_RR	(7 |IPOPT_CONTROL)
// IPOPT_SID (8) - Stream ID选项，用于唯一识别一个数据流。
#define IPOPT_SID	(8 |IPOPT_CONTROL|IPOPT_COPY)
// IPOPT_SSRR (9) - Strict Source and Record Route（严格源选路并记录路由)选项， 要求数据包必须按照指定的源通过路由。
#define IPOPT_SSRR	(9 |IPOPT_CONTROL|IPOPT_COPY)
// IPOPT_RA (20) - 路由器提醒选项，用于标识数据包需要由路由器处理。
#define IPOPT_RA	(20|IPOPT_CONTROL|IPOPT_COPY)

// IPVERSION: 这是网络协议版本，在这里被定义为4，表示IPv4。
#define IPVERSION	4
// MAXTTL: 最大生存时间（时间-生存，Time-To-Live）是一个8位二进制数，表示数据包在网络中经过的最大路由跳数。它被设为255，意味着数据包可经过最多255个路由器。
#define MAXTTL		255
// IPDEFTTL: 这是默认的TTL值，被设置为64，表示新创建的数据包创建时的默认TTL值。
#define IPDEFTTL	64
// IPOPT_*: 这些宏定义了IPv4标头选项字段的相关值。选项字段是IPv4标头的一部分，用于实现一些特殊功能，如安全性、记录数据包源地址等。
// IPOPT_OPTVAL: 选项类型的偏移量，即，选项字段的第一个字节。
#define IPOPT_OPTVAL 0
// IPOPT_OLEN: 选项字段中选项长度的偏移量，即，选项字段的第二个字节。
#define IPOPT_OLEN   1
// IPOPT_OFFSET: 选项数据开始的偏移量
#define IPOPT_OFFSET 2
// IPOPT_MINOFF: 最小允许的选项数据长度
#define IPOPT_MINOFF 4
// MAX_IPOPTLEN: 选项字段的最大长度
#define MAX_IPOPTLEN 40
// IPOPT_NOP, IPOPT_EOL, IPOPT_TS: 这些宏定义了特定的选项类型值，分别表示：无操作，选项结束，时间戳选项。
#define IPOPT_NOP IPOPT_NOOP
#define IPOPT_EOL IPOPT_END
#define IPOPT_TS  IPOPT_TIMESTAMP

// IPOPT_TS_*: 这些宏定义了时间戳选项字段的相关值，用于描述如何处理时间戳。
// IPOPT_TS_TSONLY: 仅记录时间戳。
#define	IPOPT_TS_TSONLY		0		/* timestamps only */
// IPOPT_TS_TSANDADDR: 记录时间戳和源地址。
#define	IPOPT_TS_TSANDADDR	1		/* timestamps and addresses */
// IPOPT_TS_PRESPEC: 只记录预先指定的路由器的时间戳。
#define	IPOPT_TS_PRESPEC	3		/* specified modules only */
// IPV4_BEET_PHMAXLEN: 这个宏定义了IPv4 BEET(Blind Enhanced Encryption Transport)伪头部的最大长度。
// BEET是一种用于加密数据包的技术，这个值表示BEET在数据包中伪头部所占用的最大长度。
#define IPV4_BEET_PHMAXLEN 8

// 用于表示IPv4的网络层头部（IP header）
struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	// ihl：Internet Header Length。用4位无符号整数表示IPv4头部长度，单位为4bit。
	__u8	ihl:4,
	// version：IP 版本号。用4位无符号整数表示，对于IPv4来说，这个值为4。
		version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	__u8	version:4,
  		ihl:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	// tos: Type of Service，8位无符号整数表示服务类型，用于区分不同的服务质量。
	__u8	tos;
	// tot_len: Total Length，16位无符号整数表示整个IP数据报的长度，单位为字节。
	__be16	tot_len;
	// id: Identification，16位无符号整数表示数据报的标识符，用于唯一标识主机发送的每一个数据报。
	__be16	id;
	// frag_off：Fragment Offset，16位无符号整数，用于表示分片中数据字段相对于原始数据报开头的偏移量。
	__be16	frag_off;
	// ttl：Time to Live，8位无符号整数，表示IP数据报在网络中可以经过的最大跳数（如何跳数超过这个值，数据报将被丢弃）。
	__u8	ttl;
	// protocol：Protocol，8位无符号整数，表示封装在IP数据报中的传输层协议类型，如TCP（值为6）或UDP（值为17）。
	__u8	protocol;
	// check：Header Checksum，16位无符号整数，用于对IP头部进行差错检测。
	__sum16	check;
	// saddr：Source Address，32位无符号整数，表示数据报的源IP地址。
	__be32	saddr;
	// daddr：Destination Address，32位无符号整数，表示数据报的目的IP地址。
	__be32	daddr;
	/*The options start here. */
};

// 定义了IP认证头部（IP Authentication Header），它是在网络通信中使用的一种安全协议。其作用是保证数据在传输过程中不会被篡改
struct ip_auth_hdr {
	// nexthdr：下一个头部协议的类型。它是8位无符号整数（1个字节），表示在这个认证头部之后的协议类型，如TCP、UDP等。
	__u8  nexthdr;
	// hdrlen：头部长度。8位无符号整数，表示此结构占用的32位单位数。因此实际字节数为：hdrlen * 4。要注意这里使用的是32位单位，而不是常规的字节单位
	__u8  hdrlen;		/* This one is measured in 32 bit units! */
	// reserved：保留字段。通常用于对齐或日后拓展，当前无具体作用。占用16位（2个字节）
	__be16 reserved;
	// spi：安全参数索引（Security Parameters Index）。32位无符号整数（4个字节），用于唯一标识一组安全参数（例如，用于认证的密钥、算法等）。
	// 它与目标地址、安全协议等共同定义了一个安全关联（SA，Security Association）
	__be32 spi;
	// seq_no：序列号。32位无符号整数（也可以用来表示序列号的范围），用于对数据包进行排序和防止重放攻击。每个数据包发送时序列号递增，以确保数据包接收方可以检测到重放或失序情况。
	__be32 seq_no;		/* Sequence number */
	// auth_data[0]：认证数据。这是一个变长数组，表示存放认证数据的地方，至少应占用4字节（由于__u8表示一个字节，所以数组大小可以通过 hdrlen 字段来计算）。
	// 认证数据是用于验证数据包完整性和来源的信息，通常由发送方计算并附加到数据包中，接收方收到数据包后进行验证以确保数据未被篡改。请注意，auth_data字段需要确保64位对齐。
	__u8  auth_data[0];	/* Variable len but >=4. Mind the 64 bit alignment! */
};

// 用于定义 IP Encapsulating Security Payload (ESP) 头部。ESP 是 IPsec 协议套件的一个主要组成部分，用于为 IP 数据包提供加密和认证服务
struct ip_esp_hdr {
	// spi：spi 是 Security Parameters Index 的缩写，用于唯一标识一个安全关联（SA）。即，每个 IPsec 连接都有一个唯一的 spi。__be32 表示这是一个 32 位无符号整数，并且是 big endian 字节序。
	__be32 spi;
	// seq_no：seq_no 是 Sequence Number 的缩写，指该数据包在 ESP 数据流中的序列号。序列号用于防止 IP 数据包的重放攻击。每个发送的数据包都会根据其在数据流中的位置赋予一个唯一的序列号。__be32 表示这是一个 32 位无符号整数，并且是 big endian 字节序。
	__be32 seq_no;		/* Sequence number */
	// enc_data[0]：enc_data 是一个指向加密数据（Encrypted Data）的指针，该数据包含了原始 IP 数据包的有效负载和填充字节。
	// 数据必须是 8 字节多倍数且是 64 位内存对齐。__u8 表示这是一个无符号 8 位整数，相当于一个字节。数组长度为 0 表示这是一个大小可变的数组。
	__u8  enc_data[0];	/* Variable len but >=8. Mind the 64 bit alignment! */
};

struct ip_comp_hdr {
	__u8 nexthdr;
	__u8 flags;
	__be16 cpi;
};

struct ip_beet_phdr {
	__u8 nexthdr;
	__u8 hdrlen;
	__u8 padlen;
	__u8 reserved;
};

/* index values for the variables in ipv4_devconf */
// 用于表示IPv4设备配置参数的不同选项。对于每个选项，都分配了一个唯一的整数值。这些整数值的用途是提供易于阅读且具有描述性的名称，以便在内核代码中表示配置参数值
enum
{
	// IPV4_DEVCONF_FORWARDING：IPv4包转发的开关。
	IPV4_DEVCONF_FORWARDING=1,
	// IPV4_DEVCONF_MC_FORWARDING：多播转发的开关。
	IPV4_DEVCONF_MC_FORWARDING,
	// IPV4_DEVCONF_PROXY_ARP：代理ARP的开关。
	IPV4_DEVCONF_PROXY_ARP,
	// IPV4_DEVCONF_ACCEPT_REDIRECTS：接受ICMP重定向消息的开关。
	IPV4_DEVCONF_ACCEPT_REDIRECTS,
	// IPV4_DEVCONF_SECURE_REDIRECTS：接受安全ICMP重定向消息的开关。
	IPV4_DEVCONF_SECURE_REDIRECTS,
	// IPV4_DEVCONF_SEND_REDIRECTS：发送ICMP重定向消息的开关。
	IPV4_DEVCONF_SEND_REDIRECTS,
	// IPV4_DEVCONF_SHARED_MEDIA：启用或禁用共享媒体。
	IPV4_DEVCONF_SHARED_MEDIA,
	// IPV4_DEVCONF_RP_FILTER：反向路径过滤的开关。
	IPV4_DEVCONF_RP_FILTER,
	// IPV4_DEVCONF_ACCEPT_SOURCE_ROUTE：接受源路由的开关。
	IPV4_DEVCONF_ACCEPT_SOURCE_ROUTE,
	// IPV4_DEVCONF_BOOTP_RELAY：启用或禁用BOOTP中继代理。
	IPV4_DEVCONF_BOOTP_RELAY,
	// IPV4_DEVCONF_LOG_MARTIANS：记录不合法地址来源的包。
	IPV4_DEVCONF_LOG_MARTIANS,
	// IPV4_DEVCONF_TAG：设备标识。
	IPV4_DEVCONF_TAG,
	// IPV4_DEVCONF_ARPFILTER：设备ARP过滤。
	IPV4_DEVCONF_ARPFILTER,
	// IPV4_DEVCONF_MEDIUM_ID：媒体类型ID。
	IPV4_DEVCONF_MEDIUM_ID,
	// IPV4_DEVCONF_NOXFRM：禁用XFRM（IPsec变换）的开关。
	IPV4_DEVCONF_NOXFRM,
	// IPV4_DEVCONF_NOPOLICY：禁用IPsec策略的开关。
	IPV4_DEVCONF_NOPOLICY,
	// IPV4_DEVCONF_FORCE_IGMP_VERSION：强制使用特定版本的IGMP（组管理协议）。
	IPV4_DEVCONF_FORCE_IGMP_VERSION,
	// IPV4_DEVCONF_ARP_ANNOUNCE：调整ARP公告级别。
	IPV4_DEVCONF_ARP_ANNOUNCE,
	// IPV4_DEVCONF_ARP_IGNORE：调整ARP忽略级别。
	IPV4_DEVCONF_ARP_IGNORE,
	// IPV4_DEVCONF_PROMOTE_SECONDARIES：提升次要地址的开关。
	IPV4_DEVCONF_PROMOTE_SECONDARIES,
	// IPV4_DEVCONF_ARP_ACCEPT：接受ARP请求的开关。
	IPV4_DEVCONF_ARP_ACCEPT,
	// IPV4_DEVCONF_ARP_NOTIFY：ARP通知的开关。
	IPV4_DEVCONF_ARP_NOTIFY,
	// IPV4_DEVCONF_ACCEPT_LOCAL：接受与本地主机相同的地址的开关。
	IPV4_DEVCONF_ACCEPT_LOCAL,
	// IPV4_DEVCONF_SRC_VMARK：源地址的虚拟标记。
	IPV4_DEVCONF_SRC_VMARK,
	// IPV4_DEVCONF_PROXY_ARP_PVLAN：非对称代理ARP的开关。
	IPV4_DEVCONF_PROXY_ARP_PVLAN,
	// IPV4_DEVCONF_ROUTE_LOCALNET：路由本地网络的开关。
	IPV4_DEVCONF_ROUTE_LOCALNET,
	// IPV4_DEVCONF_IGMPV2_UNSOLICITED_REPORT_INTERVAL：IGMPv2的未经请求报告时间间隔。
	IPV4_DEVCONF_IGMPV2_UNSOLICITED_REPORT_INTERVAL,
	// IPV4_DEVCONF_IGMPV3_UNSOLICITED_REPORT_INTERVAL：IGMPv3的未经请求报告时间间隔。
	IPV4_DEVCONF_IGMPV3_UNSOLICITED_REPORT_INTERVAL,
	// IPV4_DEVCONF_IGNORE_ROUTES_WITH_LINKDOWN：忽略链路关闭路由的开关。
	IPV4_DEVCONF_IGNORE_ROUTES_WITH_LINKDOWN,
	// IPV4_DEVCONF_DROP_UNICAST_IN_L2_MULTICAST：丢弃二层多播中的单播包。
	IPV4_DEVCONF_DROP_UNICAST_IN_L2_MULTICAST,
	// IPV4_DEVCONF_DROP_GRATUITOUS_ARP：丢弃无谓ARP。
	IPV4_DEVCONF_DROP_GRATUITOUS_ARP,
	// IPV4_DEVCONF_BC_FORWARDING：广播转发的开关。
	IPV4_DEVCONF_BC_FORWARDING,
	// __IPV4_DEVCONF_MAX：表示枚举值最大数量，用于内部范围检查等。
	__IPV4_DEVCONF_MAX
};

#define IPV4_DEVCONF_MAX (__IPV4_DEVCONF_MAX - 1)

#endif /* _UAPI_LINUX_IP_H */
