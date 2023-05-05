/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the ICMP protocol.
 *
 * Version:	@(#)icmp.h	1.0.3	04/28/93
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _UAPI_LINUX_ICMP_H
#define _UAPI_LINUX_ICMP_H

#include <linux/types.h>

#define ICMP_ECHOREPLY		0	/* Echo Reply			*/
#define ICMP_DEST_UNREACH	3	/* Destination Unreachable	*/
#define ICMP_SOURCE_QUENCH	4	/* Source Quench		*/
#define ICMP_REDIRECT		5	/* Redirect (change route)	*/
#define ICMP_ECHO		8	/* Echo Request			*/
#define ICMP_TIME_EXCEEDED	11	/* Time Exceeded		*/
#define ICMP_PARAMETERPROB	12	/* Parameter Problem		*/
#define ICMP_TIMESTAMP		13	/* Timestamp Request		*/
#define ICMP_TIMESTAMPREPLY	14	/* Timestamp Reply		*/
#define ICMP_INFO_REQUEST	15	/* Information Request		*/
#define ICMP_INFO_REPLY		16	/* Information Reply		*/
#define ICMP_ADDRESS		17	/* Address Mask Request		*/
#define ICMP_ADDRESSREPLY	18	/* Address Mask Reply		*/
#define NR_ICMP_TYPES		18


/* Codes for UNREACH. */
#define ICMP_NET_UNREACH	0	/* Network Unreachable		*/
#define ICMP_HOST_UNREACH	1	/* Host Unreachable		*/
#define ICMP_PROT_UNREACH	2	/* Protocol Unreachable		*/
#define ICMP_PORT_UNREACH	3	/* Port Unreachable		*/
#define ICMP_FRAG_NEEDED	4	/* Fragmentation Needed/DF set	*/
#define ICMP_SR_FAILED		5	/* Source Route failed		*/
#define ICMP_NET_UNKNOWN	6
#define ICMP_HOST_UNKNOWN	7
#define ICMP_HOST_ISOLATED	8
#define ICMP_NET_ANO		9
#define ICMP_HOST_ANO		10
#define ICMP_NET_UNR_TOS	11
#define ICMP_HOST_UNR_TOS	12
#define ICMP_PKT_FILTERED	13	/* Packet filtered */
#define ICMP_PREC_VIOLATION	14	/* Precedence violation */
#define ICMP_PREC_CUTOFF	15	/* Precedence cut off */
#define NR_ICMP_UNREACH		15	/* instead of hardcoding immediate value */

/* Codes for REDIRECT. */
#define ICMP_REDIR_NET		0	/* Redirect Net			*/
#define ICMP_REDIR_HOST		1	/* Redirect Host		*/
#define ICMP_REDIR_NETTOS	2	/* Redirect Net for TOS		*/
#define ICMP_REDIR_HOSTTOS	3	/* Redirect Host for TOS	*/

/* Codes for TIME_EXCEEDED. */
#define ICMP_EXC_TTL		0	/* TTL count exceeded		*/
#define ICMP_EXC_FRAGTIME	1	/* Fragment Reass time exceeded	*/


// 定义了ICMP（Internet Control Message Protocol）报文头部格式
struct icmphdr {
	// type：ICMP报文类型，如请求回显（ping命令）或目的不可达
  __u8		type;
  // 描述报文的子类型，例如目标不可达报文中的网络不可达或主机不可达
  __u8		code;
  // 表示一种防止数据传输中产生错误的简单校验和
  __sum16	checksum;
  // 允许根据报文类型和代码组合，在相同内存区域存储不同的数据
  union {
	// 用于处理回显请求（ICMP类型为8）和回显应答（ICMP类型为0）的报文
	struct {
		// 表示回显报文的唯一标识符
		__be16	id;
		// 表示报文的序列号，用于跟踪和匹配请求与应答
		__be16	sequence;
	} echo;
	// 用于重定向报文（ICMP类型为5）中存储重定向目标IP地址信息。
	__be32	gateway;
	// 用于处理“需要分片但不分片位已设置”的报文（ICMP类型为3，代码为4）
	struct {
		// 未使用的字段，填充为0。
		__be16	__unused;
		// 表示通过该路径最大允许的传输单元（MTU）
		__be16	mtu;
	} frag;
	// 预留字段，用于填充不使用联合体的ICMP类型。含义根据具体类型而定。
	__u8	reserved[4];
  } un;
};


/*
 *	constants for (set|get)sockopt
 */

#define ICMP_FILTER			1

struct icmp_filter {
	__u32		data;
};


#endif /* _UAPI_LINUX_ICMP_H */
