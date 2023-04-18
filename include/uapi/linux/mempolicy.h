/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * NUMA memory policies for Linux.
 * Copyright 2003,2004 Andi Kleen SuSE Labs
 */
#ifndef _UAPI_LINUX_MEMPOLICY_H
#define _UAPI_LINUX_MEMPOLICY_H

#include <linux/errno.h>


/*
 * Both the MPOL_* mempolicy mode and the MPOL_F_* optional mode flags are
 * passed by the user to either set_mempolicy() or mbind() in an 'int' actual.
 * The MPOL_MODE_FLAGS macro determines the legal set of optional mode flags.
 */

/* 
Policies 
内存节点的策略类型

这些策略可以在使用内存映射函数（例如 mmap()）时指定。例如，使用mmap() 映射一块内存时，可以使用 MAP_ANONYMOUS 标志和 MAP_HUGETLB 标志来指定内存节点的类型，
例如使用 MAP_HUGETLB | MAP_ANONYMOUS | MPOL_BIND 来表示强制绑定到指定的内存节点。
*/
enum {
	// MPOL_DEFAULT 表示默认策略。
	MPOL_DEFAULT,
	// MPOL_PREFERRED 表示优先选择指定的内存节点。
	MPOL_PREFERRED,
	// MPOL_BIND 表示强制绑定到指定的内存节点。
	MPOL_BIND,
	// MPOL_INTERLEAVE 表示将内存映射到所有节点，以实现内存闲置和负载均衡。
	MPOL_INTERLEAVE,
	// MPOL_LOCAL 表示仅从本地内存节点分配内存，适用于低延迟场景。
	MPOL_LOCAL,
	// MPOL_MAX 是枚举的最后一个元素，用于标记枚举的最大值。
	MPOL_MAX,	/* always last member of enum */
};

/* Flags for set_mempolicy */
// MPOL_F_STATIC_NODES：表示节点ID在系统生命周期内保持不变，如果该标志被设置，则系统不应该使用动态分配的节点ID。
#define MPOL_F_STATIC_NODES	(1 << 15)
// MPOL_F_RELATIVE_NODES：表示节点ID是相对于进程中第一个节点的ID，如果该标志被设置，则节点ID被视为相对于进程中的第一个节点，并且节点ID应偏移以反映第一个节点的ID。
#define MPOL_F_RELATIVE_NODES	(1 << 14)

/*
 * MPOL_MODE_FLAGS is the union of all possible optional mode flags passed to
 * either set_mempolicy() or mbind().
 */
#define MPOL_MODE_FLAGS	(MPOL_F_STATIC_NODES | MPOL_F_RELATIVE_NODES)

/* Flags for get_mempolicy */
// 这些标志位可以在使用内存策略函数（例如 mbind()）时使用，以指定更具体的内存分配策略。

// MPOL_F_NODE：当这个标志位被设置时，返回的不是节点掩码（node mask），而是下一个插值模式（interleave mode）。这个标志位的作用是用于确定插值模式是如何影响节点的。（注：插值模式是用于内存分配的一种策略，通过轮流选取不同的物理地址来平衡内存使用情况。）
#define MPOL_F_NODE	(1<<0)	/* return next IL mode instead of node mask */
// MPOL_F_ADDR：当这个标志位被设置时，内核不使用传入的 pid 参数来查找进程地址空间中的 VMA（Virtual Memory Area），而是使用传入的 addr 参数来确定VMA。这个标志通常与 MPOL_F_NODE 一起使用，用于返回与给定地址相关联的内存策略。
#define MPOL_F_ADDR	(1<<1)	/* look up vma using address */
// MPOL_F_MEMS_ALLOWED：当这个标志位被设置时，函数将返回允许的内存节点（allowed memories）而不是实际使用的内存节点。这个标志通常用于检查给定的内存策略是否符合要求。
#define MPOL_F_MEMS_ALLOWED (1<<2) /* return allowed memories */

/* Flags for mbind */
// MPOL_MF_STRICT：当新的内存策略被应用到现有映射上时，会验证现有页面是否符合该策略。如果页面与策略不符，则无法创建新映射。
#define MPOL_MF_STRICT	(1<<0)	/* Verify existing pages in the mapping */
// MPOL_MF_MOVE：将进程所拥有的内存页移动到符合新的内存策略的节点上。这个标志必须与 MPOL_MF_STRICT 一起使用。
#define MPOL_MF_MOVE	 (1<<1)	/* Move pages owned by this process to conform to policy */
// MPOL_MF_MOVE_ALL：将内存映射的所有页面都移动到符合新内存策略的节点上。这个标志必须与 MPOL_MF_STRICT 和 MPOL_MF_MOVE 一起使用。
#define MPOL_MF_MOVE_ALL (1<<2)	/* Move every page to conform to policy */
// MPOL_MF_LAZY：与 MPOL_MF_MOVE 标志一起使用时，仅在发生缺页异常时才进行懒惰迁移（lazy migrate）。
#define MPOL_MF_LAZY	 (1<<3)	/* Modifies '_MOVE:  lazy migrate on fault */
// MPOL_MF_INTERNAL：这个标志作为内部标志的起点，通常不对用户公开使用。
#define MPOL_MF_INTERNAL (1<<4)	/* Internal flags start here */

#define MPOL_MF_VALID	(MPOL_MF_STRICT   | 	\
			 MPOL_MF_MOVE     | 	\
			 MPOL_MF_MOVE_ALL)

/*
 * Internal flags that share the struct mempolicy flags word with
 * "mode flags".  These flags are allocated from bit 0 up, as they
 * are never OR'ed into the mode in mempolicy API arguments.
 */
// MPOL_F_SHARED：标记为共享内存策略。这个标志通常用于在多个进程之间共享内存，以确保多个进程使用的内存页面位于同一节点上，从而提高性能。
#define MPOL_F_SHARED  (1 << 0)	/* identify shared policies */
// MPOL_F_LOCAL：标记为首选本地分配。在使用 NUMA（Non-Uniform Memory Access）系统时，这个标志可以在多个节点之间选择一个本地节点来分配内存页面，这样可以在节点之间减少数据交换并提供更高的性能。
#define MPOL_F_LOCAL   (1 << 1)	/* preferred local allocation */
// MPOL_F_MOF：这个策略希望通过缺页异常时进行迁移（migrate on fault）以满足内存分配策略。
#define MPOL_F_MOF	(1 << 3) /* this policy wants migrate on fault */
// MPOL_F_MORON：表示这个内存分配策略希望在节点上发生缺页故障时将访问非法地址（protnone reference）的内存页面迁移到另一个节点中。这种迁移通常用于 NUMA 架构中的进程间通信，以确保进程访问的所有数据都处于同一节点上。
#define MPOL_F_MORON	(1 << 4) /* Migrate On protnone Reference On Node */


#endif /* _UAPI_LINUX_MEMPOLICY_H */
