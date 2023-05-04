/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__LINUX_NETLINK_H
#define _UAPI__LINUX_NETLINK_H

#include <linux/kernel.h>
#include <linux/socket.h> /* for __kernel_sa_family_t */
#include <linux/types.h>
// 用于定义不同类型的Netlink协议
// NETLINK_ROUTE：用于路由/设备钩子。
#define NETLINK_ROUTE		0	/* Routing/device hook				*/
// NETLINK_UNUSED：未使用的编号。
#define NETLINK_UNUSED		1	/* Unused number				*/
// NETLINK_USERSOCK：保留给用户模式套接字协议。
#define NETLINK_USERSOCK	2	/* Reserved for user mode socket protocols 	*/
// NETLINK_FIREWALL：未使用的编号，曾用于ip_queue。
#define NETLINK_FIREWALL	3	/* Unused number, formerly ip_queue		*/
// NETLINK_SOCK_DIAG：用于套接字监控。
#define NETLINK_SOCK_DIAG	4	/* socket monitoring				*/
// NETLINK_NFLOG：用于netfilter/iptables ULOG。
#define NETLINK_NFLOG		5	/* netfilter/iptables ULOG */
// NETLINK_XFRM：用于ipsec。
#define NETLINK_XFRM		6	/* ipsec */
// NETLINK_SELINUX：用于SELinux事件通知。
#define NETLINK_SELINUX		7	/* SELinux event notifications */
// NETLINK_ISCSI：用于Open-iSCSI。
#define NETLINK_ISCSI		8	/* Open-iSCSI */
// NETLINK_AUDIT：用于审计。
#define NETLINK_AUDIT		9	/* auditing */
// NETLINK_FIB_LOOKUP：用于FIB查找。
#define NETLINK_FIB_LOOKUP	10	
// NETLINK_CONNECTOR：用于Connector。
#define NETLINK_CONNECTOR	11
// NETLINK_NETFILTER：用于netfilter子系统。
#define NETLINK_NETFILTER	12	/* netfilter subsystem */
// NETLINK_IP6_FW：用于IPv6防火墙。
#define NETLINK_IP6_FW		13
// NETLINK_DNRTMSG：用于DECnet路由消息。
#define NETLINK_DNRTMSG		14	/* DECnet routing messages */
// NETLINK_KOBJECT_UEVENT：用于内核向用户空间发送的消息。
#define NETLINK_KOBJECT_UEVENT	15	/* Kernel messages to userspace */
// NETLINK_GENERIC：通用类型的Netlink协议。
#define NETLINK_GENERIC		16
/* leave room for NETLINK_DM (DM Events) */
// NETLINK_SCSITRANSPORT：用于SCSI传输。
#define NETLINK_SCSITRANSPORT	18	/* SCSI Transports */
// NETLINK_ECRYPTFS：用于加密文件系统。
#define NETLINK_ECRYPTFS	19
// NETLINK_RDMA：用于远程直接内存访问（RDMA）。
#define NETLINK_RDMA		20
// NETLINK_CRYPTO：用于加密层。
#define NETLINK_CRYPTO		21	/* Crypto layer */
// NETLINK_SMC：用于SMC监控。
#define NETLINK_SMC		22	/* SMC monitoring */

#define NETLINK_INET_DIAG	NETLINK_SOCK_DIAG

#define MAX_LINKS 32		

// sockaddr_nl，用于在Netlink套接字中表示地址信息
struct sockaddr_nl {
	// nl_family：表示地址族，其值应为AF_NETLINK。
	__kernel_sa_family_t	nl_family;	/* AF_NETLINK	*/
	// nl_pad：用于对齐，其值应为0。
	unsigned short	nl_pad;		/* zero		*/
	// nl_pid：表示端口ID，即Netlink套接字的唯一标识符。
	__u32		nl_pid;		/* port ID	*/
	// nl_groups：表示多播组掩码，用于加入多个多播组。
       	__u32		nl_groups;	/* multicast groups mask */
};

// nlmsghdr，用于在Netlink协议中表示消息头
struct nlmsghdr {
	// nlmsg_len：表示消息的总长度，包括消息头的长度。
	__u32		nlmsg_len;	/* Length of message including header */
	// nlmsg_type：表示消息的类型，包含了消息的具体内容。
	__u16		nlmsg_type;	/* Message content */
	// nlmsg_flags：表示消息的附加标志，如NLM_F_REQUEST、NLM_F_ACK、NLM_F_DUMP等。
	__u16		nlmsg_flags;	/* Additional flags */
	// nlmsg_seq：表示消息的序列号，用于标识消息的顺序。
	__u32		nlmsg_seq;	/* Sequence number */
	// nlmsg_pid：表示发送进程的端口ID，用于接收端确认消息的来源。
	__u32		nlmsg_pid;	/* Sending process port ID */
};

/* Flags values */
// Netlink消息的附加标志
// NLM_F_REQUEST：表示这是一个请求消息。
#define NLM_F_REQUEST		0x01	/* It is request message. 	*/
// NLM_F_MULTI：表示这是一个多部分消息，由NLMSG_DONE结束。
#define NLM_F_MULTI		0x02	/* Multipart message, terminated by NLMSG_DONE */
// NLM_F_ACK：表示接收方应答消息，如果成功则返回0，否则返回错误码。
#define NLM_F_ACK		0x04	/* Reply with ack, with zero or error code */
// NLM_F_ECHO：表示回显请求消息。
#define NLM_F_ECHO		0x08	/* Echo this request 		*/
// NLM_F_DUMP_INTR：表示序列号发生变化，导致传输不一致。
#define NLM_F_DUMP_INTR		0x10	/* Dump was inconsistent due to sequence change */
// NLM_F_DUMP_FILTERED：表示传输已经按要求过滤。
#define NLM_F_DUMP_FILTERED	0x20	/* Dump was filtered as requested */

/* Modifiers to GET request */
// NLM_F_ROOT：指定树的根。
#define NLM_F_ROOT	0x100	/* specify tree	root	*/
// NLM_F_MATCH：返回所有匹配的结果。
#define NLM_F_MATCH	0x200	/* return all matching	*/
// NLM_F_ATOMIC：原子性地获取结果。
#define NLM_F_ATOMIC	0x400	/* atomic GET		*/
// NLM_F_DUMP：返回整个数据树或者匹配的结果。
#define NLM_F_DUMP	(NLM_F_ROOT|NLM_F_MATCH)

/* Modifiers to NEW request */
// NLM_F_REPLACE：覆盖已有的条目。
#define NLM_F_REPLACE	0x100	/* Override existing		*/
// NLM_F_EXCL：仅在不存在时创建。
#define NLM_F_EXCL	0x200	/* Do not touch, if it exists	*/
// NLM_F_CREATE：仅在存在时创建。
#define NLM_F_CREATE	0x400	/* Create, if it does not exist	*/
// NLM_F_APPEND：将消息添加到列表的末尾。
#define NLM_F_APPEND	0x800	/* Add to end of list		*/

/* Modifiers to DELETE request */
#define NLM_F_NONREC	0x100	/* Do not delete recursively	*/

/* Flags for ACK message */
// NLM_F_CAPPED：请求已被限制。
#define NLM_F_CAPPED	0x100	/* request was capped */
// NLM_F_ACK_TLVS：附带了扩展ACK TVLs。
#define NLM_F_ACK_TLVS	0x200	/* extended ACK TVLs were included */

/*
   4.4BSD ADD		NLM_F_CREATE|NLM_F_EXCL
   4.4BSD CHANGE	NLM_F_REPLACE

   True CHANGE		NLM_F_CREATE|NLM_F_REPLACE
   Append		NLM_F_CREATE
   Check		NLM_F_EXCL
 */

#define NLMSG_ALIGNTO	4U
#define NLMSG_ALIGN(len) ( ((len)+NLMSG_ALIGNTO-1) & ~(NLMSG_ALIGNTO-1) )
#define NLMSG_HDRLEN	 ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(len) ((len) + NLMSG_HDRLEN)
#define NLMSG_SPACE(len) NLMSG_ALIGN(NLMSG_LENGTH(len))
#define NLMSG_DATA(nlh)  ((void*)(((char*)nlh) + NLMSG_LENGTH(0)))
#define NLMSG_NEXT(nlh,len)	 ((len) -= NLMSG_ALIGN((nlh)->nlmsg_len), \
				  (struct nlmsghdr*)(((char*)(nlh)) + NLMSG_ALIGN((nlh)->nlmsg_len)))
#define NLMSG_OK(nlh,len) ((len) >= (int)sizeof(struct nlmsghdr) && \
			   (nlh)->nlmsg_len >= sizeof(struct nlmsghdr) && \
			   (nlh)->nlmsg_len <= (len))
#define NLMSG_PAYLOAD(nlh,len) ((nlh)->nlmsg_len - NLMSG_SPACE((len)))

#define NLMSG_NOOP		0x1	/* Nothing.		*/
#define NLMSG_ERROR		0x2	/* Error		*/
#define NLMSG_DONE		0x3	/* End of a dump	*/
#define NLMSG_OVERRUN		0x4	/* Data lost		*/

#define NLMSG_MIN_TYPE		0x10	/* < 0x10: reserved control messages */

struct nlmsgerr {
	int		error;
	struct nlmsghdr msg;
	/*
	 * followed by the message contents unless NETLINK_CAP_ACK was set
	 * or the ACK indicates success (error == 0)
	 * message length is aligned with NLMSG_ALIGN()
	 */
	/*
	 * followed by TLVs defined in enum nlmsgerr_attrs
	 * if NETLINK_EXT_ACK was set
	 */
};

/**
 * enum nlmsgerr_attrs - nlmsgerr attributes
 * @NLMSGERR_ATTR_UNUSED: unused
 * @NLMSGERR_ATTR_MSG: error message string (string)
 * @NLMSGERR_ATTR_OFFS: offset of the invalid attribute in the original
 *	 message, counting from the beginning of the header (u32)
 * @NLMSGERR_ATTR_COOKIE: arbitrary subsystem specific cookie to
 *	be used - in the success case - to identify a created
 *	object or operation or similar (binary)
 * @__NLMSGERR_ATTR_MAX: number of attributes
 * @NLMSGERR_ATTR_MAX: highest attribute number
 */
enum nlmsgerr_attrs {
	NLMSGERR_ATTR_UNUSED,
	NLMSGERR_ATTR_MSG,
	NLMSGERR_ATTR_OFFS,
	NLMSGERR_ATTR_COOKIE,

	__NLMSGERR_ATTR_MAX,
	NLMSGERR_ATTR_MAX = __NLMSGERR_ATTR_MAX - 1
};

#define NETLINK_ADD_MEMBERSHIP		1
#define NETLINK_DROP_MEMBERSHIP		2
#define NETLINK_PKTINFO			3
#define NETLINK_BROADCAST_ERROR		4
#define NETLINK_NO_ENOBUFS		5
#ifndef __KERNEL__
#define NETLINK_RX_RING			6
#define NETLINK_TX_RING			7
#endif
#define NETLINK_LISTEN_ALL_NSID		8
#define NETLINK_LIST_MEMBERSHIPS	9
#define NETLINK_CAP_ACK			10
#define NETLINK_EXT_ACK			11
#define NETLINK_GET_STRICT_CHK		12

struct nl_pktinfo {
	__u32	group;
};

struct nl_mmap_req {
	unsigned int	nm_block_size;
	unsigned int	nm_block_nr;
	unsigned int	nm_frame_size;
	unsigned int	nm_frame_nr;
};

struct nl_mmap_hdr {
	unsigned int	nm_status;
	unsigned int	nm_len;
	__u32		nm_group;
	/* credentials */
	__u32		nm_pid;
	__u32		nm_uid;
	__u32		nm_gid;
};

#ifndef __KERNEL__
enum nl_mmap_status {
	NL_MMAP_STATUS_UNUSED,
	NL_MMAP_STATUS_RESERVED,
	NL_MMAP_STATUS_VALID,
	NL_MMAP_STATUS_COPY,
	NL_MMAP_STATUS_SKIP,
};

#define NL_MMAP_MSG_ALIGNMENT		NLMSG_ALIGNTO
#define NL_MMAP_MSG_ALIGN(sz)		__ALIGN_KERNEL(sz, NL_MMAP_MSG_ALIGNMENT)
#define NL_MMAP_HDRLEN			NL_MMAP_MSG_ALIGN(sizeof(struct nl_mmap_hdr))
#endif

#define NET_MAJOR 36		/* Major 36 is reserved for networking 						*/

enum {
	NETLINK_UNCONNECTED = 0,
	NETLINK_CONNECTED,
};

/*
 *  <------- NLA_HDRLEN ------> <-- NLA_ALIGN(payload)-->
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 * |        Header       | Pad |     Payload       | Pad |
 * |   (struct nlattr)   | ing |                   | ing |
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 *  <-------------- nlattr->nla_len -------------->
 */

struct nlattr {
	__u16           nla_len;
	__u16           nla_type;
};

/*
 * nla_type (16 bits)
 * +---+---+-------------------------------+
 * | N | O | Attribute Type                |
 * +---+---+-------------------------------+
 * N := Carries nested attributes
 * O := Payload stored in network byte order
 *
 * Note: The N and O flag are mutually exclusive.
 */
#define NLA_F_NESTED		(1 << 15)
#define NLA_F_NET_BYTEORDER	(1 << 14)
#define NLA_TYPE_MASK		~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)

#define NLA_ALIGNTO		4
#define NLA_ALIGN(len)		(((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN		((int) NLA_ALIGN(sizeof(struct nlattr)))

/* Generic 32 bitflags attribute content sent to the kernel.
 *
 * The value is a bitmap that defines the values being set
 * The selector is a bitmask that defines which value is legit
 *
 * Examples:
 *  value = 0x0, and selector = 0x1
 *  implies we are selecting bit 1 and we want to set its value to 0.
 *
 *  value = 0x2, and selector = 0x2
 *  implies we are selecting bit 2 and we want to set its value to 1.
 *
 */
struct nla_bitfield32 {
	__u32 value;
	__u32 selector;
};

#endif /* _UAPI__LINUX_NETLINK_H */
