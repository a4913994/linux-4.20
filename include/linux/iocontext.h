/* SPDX-License-Identifier: GPL-2.0 */
#ifndef IOCONTEXT_H
#define IOCONTEXT_H

#include <linux/radix-tree.h>
#include <linux/rcupdate.h>
#include <linux/workqueue.h>

enum {
	ICQ_EXITED		= 1 << 2,
};

/*
 * An io_cq (icq) is association between an io_context (ioc) and a
 * request_queue (q).  This is used by elevators which need to track
 * information per ioc - q pair.
 *
 * Elevator can request use of icq by setting elevator_type->icq_size and
 * ->icq_align.  Both size and align must be larger than that of struct
 * io_cq and elevator can use the tail area for private information.  The
 * recommended way to do this is defining a struct which contains io_cq as
 * the first member followed by private members and using its size and
 * align.  For example,
 *
 *	struct snail_io_cq {
 *		struct io_cq	icq;
 *		int		poke_snail;
 *		int		feed_snail;
 *	};
 *
 *	struct elevator_type snail_elv_type {
 *		.ops =		{ ... },
 *		.icq_size =	sizeof(struct snail_io_cq),
 *		.icq_align =	__alignof__(struct snail_io_cq),
 *		...
 *	};
 *
 * If icq_size is set, block core will manage icq's.  All requests will
 * have its ->elv.icq field set before elevator_ops->elevator_set_req_fn()
 * is called and be holding a reference to the associated io_context.
 *
 * Whenever a new icq is created, elevator_ops->elevator_init_icq_fn() is
 * called and, on destruction, ->elevator_exit_icq_fn().  Both functions
 * are called with both the associated io_context and queue locks held.
 *
 * Elevator is allowed to lookup icq using ioc_lookup_icq() while holding
 * queue lock but the returned icq is valid only until the queue lock is
 * released.  Elevators can not and should not try to create or destroy
 * icq's.
 *
 * As icq's are linked from both ioc and q, the locking rules are a bit
 * complex.
 *
 * - ioc lock nests inside q lock.
 *
 * - ioc->icq_list and icq->ioc_node are protected by ioc lock.
 *   q->icq_list and icq->q_node by q lock.
 *
 * - ioc->icq_tree and ioc->icq_hint are protected by ioc lock, while icq
 *   itself is protected by q lock.  However, both the indexes and icq
 *   itself are also RCU managed and lookup can be performed holding only
 *   the q lock.
 *
 * - icq's are not reference counted.  They are destroyed when either the
 *   ioc or q goes away.  Each request with icq set holds an extra
 *   reference to ioc to ensure it stays until the request is completed.
 *
 * - Linking and unlinking icq's are performed while holding both ioc and q
 *   locks.  Due to the lock ordering, q exit is simple but ioc exit
 *   requires reverse-order double lock dance.
 */
struct io_cq {
	struct request_queue	*q;
	struct io_context	*ioc;

	/*
	 * q_node and ioc_node link io_cq through icq_list of q and ioc
	 * respectively.  Both fields are unused once ioc_exit_icq() is
	 * called and shared with __rcu_icq_cache and __rcu_head which are
	 * used for RCU free of io_cq.
	 */
	union {
		struct list_head	q_node;
		struct kmem_cache	*__rcu_icq_cache;
	};
	union {
		struct hlist_node	ioc_node;
		struct rcu_head		__rcu_head;
	};

	unsigned int		flags;
};

/*
 * I/O subsystem state of the associated processes.  It is refcounted
 * and kmalloc'ed. These could be shared between processes.
 * 与关联进程相关的 I/O 子系统状态。它是引用计数的并且通过 kmalloc 进行分配。这些状态可以在进程之间共享。
 */
// io_context 结构体，它用于在 Linux 内核中控制并跟踪一组 I/O 操作
struct io_context {
	// refcount：引用计数器。用于管理 io_context 结构的生命周期。
	atomic_long_t refcount;
	// active_ref：表示该 io_context 当前是否处于活跃状态，用于进行暂停和重新启动 I/O 操作。
	atomic_t active_ref;
	// nr_tasks：已经关联到该 io_context 的进程个数。
	atomic_t nr_tasks;

	/* all the fields below are protected by this lock */
	// lock：用于锁定该上下文的所有字段，以确保并发访问的正确性。
	spinlock_t lock;

	// ioprio：该上下文中所有 I/O 请求使用的 I/O 优先级。
	unsigned short ioprio;

	/*
	 * For request batching
	 */
	// nr_batch_requests：用于请求批量处理，指示剩余未处理的请求数。
	int nr_batch_requests;     /* Number of requests left in the batch */
	// last_waited：表示上次等候请求的时间。
	unsigned long last_waited; /* Time last woken after wait for request */

	// icq_tree：一个基数树，用于跟踪与该 io_context 相关的所有请求。
	struct radix_tree_root	icq_tree;
	// icq_hint：一个指针，指向最近的与 io_context 相关的请求队列。
	struct io_cq __rcu	*icq_hint;
	// icq_list：一个哈希表，使用 hlist_head 连接，跟踪与该 io_context 关联的 I/O 请求。
	struct hlist_head	icq_list;

	// release_work：一个工作队列，用于异步释放 io_context。
	struct work_struct release_work;
};

/**
 * get_io_context_active - get active reference on ioc
 * @ioc: ioc of interest
 *
 * Only iocs with active reference can issue new IOs.  This function
 * acquires an active reference on @ioc.  The caller must already have an
 * active reference on @ioc.
 */
static inline void get_io_context_active(struct io_context *ioc)
{
	WARN_ON_ONCE(atomic_long_read(&ioc->refcount) <= 0);
	WARN_ON_ONCE(atomic_read(&ioc->active_ref) <= 0);
	atomic_long_inc(&ioc->refcount);
	atomic_inc(&ioc->active_ref);
}

static inline void ioc_task_link(struct io_context *ioc)
{
	get_io_context_active(ioc);

	WARN_ON_ONCE(atomic_read(&ioc->nr_tasks) <= 0);
	atomic_inc(&ioc->nr_tasks);
}

struct task_struct;
#ifdef CONFIG_BLOCK
void put_io_context(struct io_context *ioc);
void put_io_context_active(struct io_context *ioc);
void exit_io_context(struct task_struct *task);
struct io_context *get_task_io_context(struct task_struct *task,
				       gfp_t gfp_flags, int node);
#else
struct io_context;
static inline void put_io_context(struct io_context *ioc) { }
static inline void exit_io_context(struct task_struct *task) { }
#endif

#endif
