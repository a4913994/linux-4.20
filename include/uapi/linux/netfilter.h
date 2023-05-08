/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__LINUX_NETFILTER_H
#define _UAPI__LINUX_NETFILTER_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/in.h>
#include <linux/in6.h>

/* Responses from hook functions. */
// 表示网络包在hook函数（自定义处理函数）中的处理结果
// NF_DROP: 丢弃该网络包
#define NF_DROP 0
// NF_ACCEPT: 接受该网络包，继续其正常处理过程
#define NF_ACCEPT 1
// NF_STOLEN: 已经由hook函数处理，内核不需要再处理
#define NF_STOLEN 2
// NF_QUEUE: 将该网络包放入队列，等待用户空间程序处理
#define NF_QUEUE 3
// NF_REPEAT: 重新调用相同的hook函数
#define NF_REPEAT 4
// NF_STOP: 不再处理该网络包，已经被废弃，仅供用户空间nf_queue兼容
#define NF_STOP 5	/* Deprecated, for userspace nf_queue compatibility. */
// NF_MAX_VERDICT: 结果的最大值，此处为NF_STOP
#define NF_MAX_VERDICT NF_STOP

/* we overload the higher bits for encoding auxiliary data such as the queue
 * number or errno values. Not nice, but better than additional function
 * arguments. */
// 定义了一些掩码和位移操作，用于在同一整数中存储处理结果和附加数据，如队列编号和错误值。
// NF_VERDICT_MASK: 用于提取处理结果的低8位
#define NF_VERDICT_MASK 0x000000ff

/* extra verdict flags have mask 0x0000ff00 */
// NF_VERDICT_FLAG_QUEUE_BYPASS: 标志位，表示绕过队列处理
#define NF_VERDICT_FLAG_QUEUE_BYPASS	0x00008000

/* queue number (NF_QUEUE) or errno (NF_DROP) */
// NF_VERDICT_QMASK和NF_VERDICT_QBITS: 提取队列号的掩码
#define NF_VERDICT_QMASK 0xffff0000
#define NF_VERDICT_QBITS 16

// NF_QUEUE_NR(x): 宏用来编码队列号和NF_QUEUE verdict
#define NF_QUEUE_NR(x) ((((x) << 16) & NF_VERDICT_QMASK) | NF_QUEUE)

// NF_DROP_ERR(x): 宏将错误值编码到NF_DROP枚举中
#define NF_DROP_ERR(x) (((-x) << 16) | NF_DROP)

/* only for userspace compatibility */
// 非内核部分的代码，仅用于用户空间兼容。
#ifndef __KERNEL__
/* Generic cache responses from hook functions.
   <= 0x2000 is used for protocol-flags. */
// NFC_UNKNOWN: 未知的缓存响应
#define NFC_UNKNOWN 0x4000
// NFC_ALTERED: 缓存已更改
#define NFC_ALTERED 0x8000

/* NF_VERDICT_BITS should be 8 now, but userspace might break if this changes */
// NF_VERDICT_BITS: 结果的位数，应为8位，但如果更改，则用户空间可能会出现问题，此处保留为16位。
#define NF_VERDICT_BITS 16
#endif

enum nf_inet_hooks {
	// NF_INET_PRE_ROUTING：在进行路由选择之前的钩子。
	NF_INET_PRE_ROUTING,
	// NF_INET_LOCAL_IN：当数据包进入本地系统时的钩子。
	NF_INET_LOCAL_IN,
	// NF_INET_FORWARD：在数据包从源地址转发到目标地址时的钩子。
	NF_INET_FORWARD,
	// NF_INET_LOCAL_OUT：当数据包从本地系统发出时的钩子。
	NF_INET_LOCAL_OUT,
	// NF_INET_POST_ROUTING：在进行路由选择之后的钩子。
	NF_INET_POST_ROUTING,
	// NF_INET_NUMHOOKS：钩子的数量。
	NF_INET_NUMHOOKS
};

// nf_dev_hooks：此枚举定义了设备级别（例如网络接口）可注册的钩子。
enum nf_dev_hooks {
	// NF_NETDEV_INGRESS：在数据包进入网络设备（即在传输之前）的钩子。
	NF_NETDEV_INGRESS,
	// NF_NETDEV_NUMHOOKS：用于表示钩子数量，通常用作数组大小。
	NF_NETDEV_NUMHOOKS
};

// 此枚举定义了Netfilter支持的网络协议
enum {
	// NFPROTO_UNSPEC：未指定协议。
	NFPROTO_UNSPEC =  0,
	// NFPROTO_INET：通用协议，可以同时支持IPv4和IPv6。
	NFPROTO_INET   =  1,
	// NFPROTO_IPV4：IPv4协议。
	NFPROTO_IPV4   =  2,
	// NFPROTO_ARP：地址解析协议(Address Resolution Protocol)。
	NFPROTO_ARP    =  3,
	// NFPROTO_NETDEV：与网络设备相关的过滤。
	NFPROTO_NETDEV =  5,
	// NFPROTO_BRIDGE：网桥协议。
	NFPROTO_BRIDGE =  7,
	// NFPROTO_IPV6：IPv6协议。
	NFPROTO_IPV6   = 10,
	// NFPROTO_DECNET：付费网协议。
	NFPROTO_DECNET = 12,
	// NFPROTO_NUMPROTO：协议的数量。
	NFPROTO_NUMPROTO,
};

// nf_inet_addr：此联合类型用于表示网络地址，支持IPv4和IPv6地址
union nf_inet_addr {
	// all：32位整型数组，用于表示四个32位值。
	__u32		all[4];
	// ip：用于表示IPv4地址的32位整数（大端字节序）。
	__be32		ip;
	// ip6：用于表示IPv6地址的128位整数（大端字节序）。
	__be32		ip6[4];
	// in：使用in_addr结构表示IPv4地址。
	struct in_addr	in;
	// in6：使用in6_addr结构表示IPv6地址。
	struct in6_addr	in6;
};

#endif /* _UAPI__LINUX_NETFILTER_H */
