/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_IPSEC_H
#define _LINUX_IPSEC_H

/* The definitions, required to talk to KAME racoon IKE. */

#include <linux/pfkeyv2.h>

// 这些常量通常用于IPsec的策略匹配中，用来指定不需要特定的传输层协议，端口或网络层协议。在某些情况下，这些常量可以提高IPsec协议的性能，并减少策略表的大小。

// IPSEC_PORT_ANY：表示IPsec策略匹配时不考虑源端口和目的端口
#define IPSEC_PORT_ANY		0
// IPSEC_ULPROTO_ANY：表示IPsec策略匹配时不考虑传输层协议类型
#define IPSEC_ULPROTO_ANY	255
// IPSEC_PROTO_ANY：表示IPsec策略匹配时不考虑网络层协议类型
#define IPSEC_PROTO_ANY		255

enum {
	// IPSEC_MODE_ANY：表示IPsec SA中未指定特定的模式
	IPSEC_MODE_ANY		= 0,	/* We do not support this for SA */
	// IPSEC_MODE_TRANSPORT：表示使用IPsec传输模式
	IPSEC_MODE_TRANSPORT	= 1,
	// IPSEC_MODE_TUNNEL：表示使用IPsec隧道模式
	IPSEC_MODE_TUNNEL	= 2,
	// IPSEC_MODE_BEET：表示使用BEET协议
	IPSEC_MODE_BEET         = 3
};

enum {
	// IPSEC_DIR_ANY：表示未指定特定的IPsec策略方向
	IPSEC_DIR_ANY		= 0,
	// IPSEC_DIR_INBOUND：表示输入方向的IPsec策略
	IPSEC_DIR_INBOUND	= 1,
	// IPSEC_DIR_OUTBOUND：表示输出方向的IPsec策略
	IPSEC_DIR_OUTBOUND	= 2,
	// IPSEC_DIR_FWD：表示转发方向的IPsec策略
	IPSEC_DIR_FWD		= 3,	/* It is our own */
	// IPSEC_DIR_MAX：表示方向的最大值，当需要循环遍历方向时使用
	IPSEC_DIR_MAX		= 4,
	// IPSEC_DIR_INVALID：表示无效的IPsec策略方向
	IPSEC_DIR_INVALID	= 5
};

enum {
	// IPSEC_POLICY_DISCARD：表示IPsec策略是丢弃的（即拒绝）
	IPSEC_POLICY_DISCARD	= 0,
	// IPSEC_POLICY_NONE：表示不存在IPsec策略
	IPSEC_POLICY_NONE	= 1,
	// IPSEC_POLICY_IPSEC：表示存在IPsec策略
	IPSEC_POLICY_IPSEC	= 2,
	// IPSEC_POLICY_ENTRUST：表示IPsec策略由其他实体信任
	IPSEC_POLICY_ENTRUST	= 3,
	// IPSEC_POLICY_BYPASS：表示IPsec策略绕过
	IPSEC_POLICY_BYPASS	= 4
};

// 这些常量通常用于确定IPsec策略的级别和要求的严格程度。例如，如果将级别设置为IPSEC_LEVEL_DEFAULT，则IPsec策略将被关闭。如果将级别设置为IPSEC_LEVEL_USE，则将使用IPsec策略来保护网络通信。如果将级别设置为IPSEC_LEVEL_REQUIRE，则要求强制使用IPsec策略。如果需要多个策略来保护通信，则需要将级别设置为IPSEC_LEVEL_UNIQUE。
enum {
	// IPSEC_LEVEL_DEFAULT：表示使用默认级别的IPsec策略
	IPSEC_LEVEL_DEFAULT	= 0,
	// IPSEC_LEVEL_USE：表示使用IPsec策略
	IPSEC_LEVEL_USE		= 1,
	// IPSEC_LEVEL_REQUIRE：表示IPsec策略是必须的
	IPSEC_LEVEL_REQUIRE	= 2,
	// IPSEC_LEVEL_UNIQUE：表示IPsec策略是唯一的
	IPSEC_LEVEL_UNIQUE	= 3
};

// IPSEC_MANUAL_REQID_MAX：这个宏定义了一个最大的IPsec请求ID（ReqID）值，其十六进制值为0x3fff，即16383。IPsec请求ID是一种唯一标识符，用于识别IPsec SA。在手动模式下指定IPsec请求ID时，可以使用此宏指定可能的最大值，以避免超出范围。
#define IPSEC_MANUAL_REQID_MAX	0x3fff
// IPSEC_REPLAYWSIZE：这个宏定义了一个IPsec重放保护窗口的大小，其值为32. IPsec重放保护机制用于防止攻击者将重复的IPsec数据包注入到通信链路上。该宏定义了重放保护窗口的大小，表示最大允许多少个数据包可以接收，然后发送重放攻击警告或按规则处理。
#define IPSEC_REPLAYWSIZE  32
/**
 * 重放攻击（Replay Attack）是一种网络攻击手段，通常指攻击者通过窃取并重复发送已经获取的合法数据包，来绕过认证机制和控制访问的保护措施。攻击者可以窃取并重复发送已授权的数据包，欺骗系统，让系统误认为这些包是合法的，并接受执行相关操作。这种攻击通常用于绕过认证，授权和访问控制机制，如通过重放已知的用户名和密码来登录系统等。
 * 在网络安全中，硬件和软件通常会使用不同的机制来缓解这种攻击，例如可以使用时间戳或序列号来对数据包进行标记，或在通信中使用对称密钥加密等方式防止攻击者窃取数据。在IPsec协议中，重放攻击通常使用窗口技术检测和抵御，如上面代码中的IPSEC_REPLAYWSIZE宏所定义的IPsec重放保护窗口大小，可以限制系统接收和处理的数据包数量，并防止系统被重复发送的数据包攻击。@brief 
 */
#endif	/* _LINUX_IPSEC_H */
