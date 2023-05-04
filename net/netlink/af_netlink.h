/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _AF_NETLINK_H
#define _AF_NETLINK_H

#include <linux/rhashtable.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <net/sock.h>

/* flags */
// NETLINK_F_KERNEL_SOCKET：指示创建一个由内核管理的套接字，如果未设置此标志，则套接字由用户空间进程管理。
#define NETLINK_F_KERNEL_SOCKET		0x1
// NETLINK_F_RECV_PKTINFO：指示在接收数据包时，一并返回数据包的元数据信息。
#define NETLINK_F_RECV_PKTINFO		0x2
// NETLINK_F_BROADCAST_SEND_ERROR：指示在广播消息时，即使某个接收方返回错误，也继续广播给其他接收方。
#define NETLINK_F_BROADCAST_SEND_ERROR	0x4
// NETLINK_F_RECV_NO_ENOBUFS：指示即使缓冲区已满，仍然接收数据包。
#define NETLINK_F_RECV_NO_ENOBUFS	0x8
// NETLINK_F_LISTEN_ALL_NSID：指示接收所有网络命名空间的消息。
#define NETLINK_F_LISTEN_ALL_NSID	0x10
// NETLINK_F_CAP_ACK：指示当套接字的用户ID拥有CAP_NET_ADMIN权限时，自动确认ACK。
#define NETLINK_F_CAP_ACK		0x20
// NETLINK_F_EXT_ACK：指示使用带有额外ACK数据的扩展ACK机制。
#define NETLINK_F_EXT_ACK		0x40
// NETLINK_F_STRICT_CHK：指示在接收和发送消息时严格检查Netlink头部的标志。
#define NETLINK_F_STRICT_CHK		0x80

#define NLGRPSZ(x)	(ALIGN(x, sizeof(unsigned long) * 8) / 8)
#define NLGRPLONGS(x)	(NLGRPSZ(x)/sizeof(unsigned long))

struct netlink_sock {
	/* struct sock has to be the first member of netlink_sock */
	// sk：继承自struct sock的通用套接字结构体，用于管理Netlink socket。
	struct sock		sk;
	// portid：用于标识Netlink套接字的唯一ID，由用户进程传递给内核。
	u32			portid;
	// dst_portid：用于标识目标套接字的ID。
	u32			dst_portid;
	// dst_group：用于标识目标套接字所在的多播组。
	u32			dst_group;
	// flags：用于设置套接字的标志，例如NETLINK_F_RECV_PKTINFO、NETLINK_F_BROADCAST_SEND_ERROR等。
	u32			flags;
	// subscriptions：订阅的消息类型，包括多个位掩码。
	u32			subscriptions;
	// ngroups：标识系统中存在的多播组的数量。
	u32			ngroups;
	// groups：指向一个位数组，表示哪些多播组已经加入。
	unsigned long		*groups;
	// state：套接字的状态，包括NETLINK_SOCK_DIAG_RUNNING、NETLINK_SOCK_DEAD等。
	unsigned long		state;
	// max_recvmsg_len：最大接收消息的长度。
	size_t			max_recvmsg_len;
	// wait：用于阻塞套接字的等待队列。
	wait_queue_head_t	wait;
	// bound：指示套接字是否已绑定到端口。
	bool			bound;
	// cb_running：用于标识是否有Netlink callback正在运行。
	bool			cb_running;
	// dump_done_errno：当通过dump()方法进行消息转发时，用于指示是否发生错误。
	int			dump_done_errno;
	// cb：一个Netlink callback结构体，用于处理消息的接收和处理。
	struct netlink_callback	cb;
	// cb_mutex：用于保护Netlink callback的互斥锁。
	struct mutex		*cb_mutex;
	// cb_def_mutex：用于保护Netlink callback默认处理器的互斥锁。
	struct mutex		cb_def_mutex;
	// netlink_rcv：用于接收Netlink消息的回调函数。
	void			(*netlink_rcv)(struct sk_buff *skb);
	// netlink_bind：用于绑定Netlink套接字到一个组。
	int			(*netlink_bind)(struct net *net, int group);
	// netlink_unbind：用于取消绑定Netlink套接字到一个组。
	void			(*netlink_unbind)(struct net *net, int group);
	// module：用于指向模块的指针，表示该套接字由哪个模块创建。
	struct module		*module;
	// node：用于将该Netlink套接字添加到系统Netlink套接字的哈希表中。
	struct rhash_head	node;
	// rcu：用于管理RCU机制，当套接字需要释放时会使用到。
	struct rcu_head		rcu;
	// work：用于处理异步事件的工作队列。
	struct work_struct	work;
};

static inline struct netlink_sock *nlk_sk(struct sock *sk)
{
	return container_of(sk, struct netlink_sock, sk);
}

// netlink_table，用于管理系统中的所有Netlink套接字。
struct netlink_table {
	// hash：使用rhashtable实现的哈希表，用于快速查找Netlink套接字。
	struct rhashtable	hash;
	// mc_list：一个哈希链表，用于管理多播组。
	struct hlist_head	mc_list;
	// listeners：一个指向所有Netlink套接字的监听器链表的指针。
	struct listeners __rcu	*listeners;
	// flags：表示Netlink套接字表的标志，包括NETLINK_GENERIC、NETLINK_ROUTE等。
	unsigned int		flags;
	// groups：表示系统中所有Netlink多播组的总数。
	unsigned int		groups;
	// cb_mutex：用于保护Netlink callback的互斥锁。
	struct mutex		*cb_mutex;
	// module：用于指向模块的指针，表示该Netlink套接字表由哪个模块创建。
	struct module		*module;
	// bind：用于绑定Netlink套接字到一个组。
	int			(*bind)(struct net *net, int group);
	// unbind：用于取消绑定Netlink套接字到一个组。
	void			(*unbind)(struct net *net, int group);
	// compare：用于比较两个Netlink套接字是否相等。
	bool			(*compare)(struct net *net, struct sock *sock);
	// registered：用于表示该Netlink套接字表是否已被注册。
	int			registered;
};

extern struct netlink_table *nl_table;
extern rwlock_t nl_table_lock;

#endif
