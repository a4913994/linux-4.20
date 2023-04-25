/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NUMA memory policies for Linux.
 * Copyright 2003,2004 Andi Kleen SuSE Labs
 */
#ifndef _LINUX_MEMPOLICY_H
#define _LINUX_MEMPOLICY_H 1


#include <linux/mmzone.h>
#include <linux/dax.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/nodemask.h>
#include <linux/pagemap.h>
#include <uapi/linux/mempolicy.h>

struct mm_struct;

#ifdef CONFIG_NUMA

/*
 * Describe a memory policy.
 * 描述内存策略。
 *
 * A mempolicy can be either associated with a process or with a VMA.
 * For VMA related allocations the VMA policy is preferred, otherwise
 * the process policy is used. Interrupts ignore the memory policy
 * of the current process.
 * 一个内存策略可以与进程或者 VMA 相关联。对于 VMA 相关的内存分配，VMA 策略被优先选择，否则会使用进程策略。中断会忽略当前进程的内存策略。
 *
 * Locking policy for interlave:
 * 交错锁定策略：
 * 
 * In process context there is no locking because only the process accesses
 * its own state. All vma manipulation is somewhat protected by a down_read on
 * mmap_sem.
 * 在进程上下文中，没有锁定，因为只有进程能访问自己的状态。所有 VMA 操作都进行了 mmap_sem 的 down_read 保护。
 *
 * Freeing policy:
 * 释放策略：
 * 
 * Mempolicy objects are reference counted.  A mempolicy will be freed when
 * mpol_put() decrements the reference count to zero.
 * Mempolicy 对象是引用计数的。当 mpol_put() 函数将引用计数减为0时，该 mempolicy 将被释放。
 *
 * Duplicating policy objects:
 * 复制策略对象：
 * mpol_dup() allocates a new mempolicy and copies the specified mempolicy
 * to the new storage.  The reference count of the new object is initialized
 * to 1, representing the caller of mpol_dup().
 * mpol_dup() 函数会分配一个新的 mempolicy，并将指定的 mempolicy 复制到新的内存中。新对象的引用计数被初始化为 1，表示 mpol_dup() 的调用方。
 */
struct mempolicy {
	// refcnt：引用计数器，用于管理该结构体的生命周期。
	atomic_t refcnt;
	// mode：内存策略模式，可以是以下之一：
	// MPOL_DEFAULT：使用系统默认的内存策略。
	// MPOL_PREFERRED：优先选择指定节点（仅适用于 NUMA 系统）。
	// MPOL_BIND：仅使用指定的节点。
	// MPOL_INTERLEAVE：在所有指定的节点之间交错内存。
	unsigned short mode; 	/* See MPOL_* above */
	// flags：内存策略标志，可以是以下之一：
	// MPOL_F_NODE：v.nodes 中指定的节点是一个节点号。
	// MPOL_F_ADDR：v.nodes 中指定的节点是一个内存地址。
	// MPOL_F_MEMS_ALLOWED：w.cpuset_mems_allowed 中指定的节点是“有权访问进程地址空间的 CPU 数量”的子集。该标志仅在 cgroup v2 中的 NUMA 控制器上使用。
	unsigned short flags;	/* See set_mempolicy() MPOL_F_* above */
	// v：策略具体内容，可能是以下之一：
	union {
		// preferred_node：仅当 mode 为 MPOL_PREFERRED 时使用，表示首选节点的节点号。
		short 		 preferred_node; /* preferred */
		// nodes：当 mode 为 MPOL_BIND 或 MPOL_INTERLEAVE 时使用，包含了要使用的节点列表。
		nodemask_t	 nodes;		/* interleave/bind */
		/* undefined for default */
	} v;
	// w：策略具体内容，可能是以下之一：
	union {
		// cpuset_mems_allowed：仅在 cgroup v2 中使用，表示“有权访问进程地址空间的 CPU 数量”的子集。
		nodemask_t cpuset_mems_allowed;	/* relative to these nodes */
		// user_nodemask：当使用 set_mempolicy 函数进行设置时使用，表示用户指定的节点列表。
		nodemask_t user_nodemask;	/* nodemask passed by user */
	} w;
};

/*
 * Support for managing mempolicy data objects (clone, copy, destroy)
 * The default fast path of a NULL MPOL_DEFAULT policy is always inlined.
 */

extern void __mpol_put(struct mempolicy *pol);
static inline void mpol_put(struct mempolicy *pol)
{
	if (pol)
		__mpol_put(pol);
}

/*
 * Does mempolicy pol need explicit unref after use?
 * Currently only needed for shared policies.
 */
static inline int mpol_needs_cond_ref(struct mempolicy *pol)
{
	return (pol && (pol->flags & MPOL_F_SHARED));
}

static inline void mpol_cond_put(struct mempolicy *pol)
{
	if (mpol_needs_cond_ref(pol))
		__mpol_put(pol);
}

extern struct mempolicy *__mpol_dup(struct mempolicy *pol);
static inline struct mempolicy *mpol_dup(struct mempolicy *pol)
{
	if (pol)
		pol = __mpol_dup(pol);
	return pol;
}

#define vma_policy(vma) ((vma)->vm_policy)

static inline void mpol_get(struct mempolicy *pol)
{
	if (pol)
		atomic_inc(&pol->refcnt);
}

extern bool __mpol_equal(struct mempolicy *a, struct mempolicy *b);
static inline bool mpol_equal(struct mempolicy *a, struct mempolicy *b)
{
	if (a == b)
		return true;
	return __mpol_equal(a, b);
}

/*
 * Tree of shared policies for a shared memory region.
 * Maintain the policies in a pseudo mm that contains vmas. The vmas
 * carry the policy. As a special twist the pseudo mm is indexed in pages, not
 * bytes, so that we can work with shared memory segments bigger than
 * unsigned long.
 */

struct sp_node {
	struct rb_node nd;
	unsigned long start, end;
	struct mempolicy *policy;
};

struct shared_policy {
	struct rb_root root;
	rwlock_t lock;
};

int vma_dup_policy(struct vm_area_struct *src, struct vm_area_struct *dst);
void mpol_shared_policy_init(struct shared_policy *sp, struct mempolicy *mpol);
int mpol_set_shared_policy(struct shared_policy *info,
				struct vm_area_struct *vma,
				struct mempolicy *new);
void mpol_free_shared_policy(struct shared_policy *p);
struct mempolicy *mpol_shared_policy_lookup(struct shared_policy *sp,
					    unsigned long idx);

struct mempolicy *get_task_policy(struct task_struct *p);
struct mempolicy *__get_vma_policy(struct vm_area_struct *vma,
		unsigned long addr);
bool vma_policy_mof(struct vm_area_struct *vma);

extern void numa_default_policy(void);
extern void numa_policy_init(void);
extern void mpol_rebind_task(struct task_struct *tsk, const nodemask_t *new);
extern void mpol_rebind_mm(struct mm_struct *mm, nodemask_t *new);

extern int huge_node(struct vm_area_struct *vma,
				unsigned long addr, gfp_t gfp_flags,
				struct mempolicy **mpol, nodemask_t **nodemask);
extern bool init_nodemask_of_mempolicy(nodemask_t *mask);
extern bool mempolicy_nodemask_intersects(struct task_struct *tsk,
				const nodemask_t *mask);
extern unsigned int mempolicy_slab_node(void);

extern enum zone_type policy_zone;

static inline void check_highest_zone(enum zone_type k)
{
	if (k > policy_zone && k != ZONE_MOVABLE)
		policy_zone = k;
}

int do_migrate_pages(struct mm_struct *mm, const nodemask_t *from,
		     const nodemask_t *to, int flags);


#ifdef CONFIG_TMPFS
extern int mpol_parse_str(char *str, struct mempolicy **mpol);
#endif

extern void mpol_to_str(char *buffer, int maxlen, struct mempolicy *pol);

/* Check if a vma is migratable */
static inline bool vma_migratable(struct vm_area_struct *vma)
{
	if (vma->vm_flags & (VM_IO | VM_PFNMAP))
		return false;

	/*
	 * DAX device mappings require predictable access latency, so avoid
	 * incurring periodic faults.
	 */
	if (vma_is_dax(vma))
		return false;

#ifndef CONFIG_ARCH_ENABLE_HUGEPAGE_MIGRATION
	if (vma->vm_flags & VM_HUGETLB)
		return false;
#endif

	/*
	 * Migration allocates pages in the highest zone. If we cannot
	 * do so then migration (at least from node to node) is not
	 * possible.
	 */
	if (vma->vm_file &&
		gfp_zone(mapping_gfp_mask(vma->vm_file->f_mapping))
								< policy_zone)
			return false;
	return true;
}

extern int mpol_misplaced(struct page *, struct vm_area_struct *, unsigned long);
extern void mpol_put_task_policy(struct task_struct *);

#else

struct mempolicy {};

static inline bool mpol_equal(struct mempolicy *a, struct mempolicy *b)
{
	return true;
}

static inline void mpol_put(struct mempolicy *p)
{
}

static inline void mpol_cond_put(struct mempolicy *pol)
{
}

static inline void mpol_get(struct mempolicy *pol)
{
}

struct shared_policy {};

static inline void mpol_shared_policy_init(struct shared_policy *sp,
						struct mempolicy *mpol)
{
}

static inline void mpol_free_shared_policy(struct shared_policy *p)
{
}

static inline struct mempolicy *
mpol_shared_policy_lookup(struct shared_policy *sp, unsigned long idx)
{
	return NULL;
}

#define vma_policy(vma) NULL

static inline int
vma_dup_policy(struct vm_area_struct *src, struct vm_area_struct *dst)
{
	return 0;
}

static inline void numa_policy_init(void)
{
}

static inline void numa_default_policy(void)
{
}

static inline void mpol_rebind_task(struct task_struct *tsk,
				const nodemask_t *new)
{
}

static inline void mpol_rebind_mm(struct mm_struct *mm, nodemask_t *new)
{
}

static inline int huge_node(struct vm_area_struct *vma,
				unsigned long addr, gfp_t gfp_flags,
				struct mempolicy **mpol, nodemask_t **nodemask)
{
	*mpol = NULL;
	*nodemask = NULL;
	return 0;
}

static inline bool init_nodemask_of_mempolicy(nodemask_t *m)
{
	return false;
}

static inline int do_migrate_pages(struct mm_struct *mm, const nodemask_t *from,
				   const nodemask_t *to, int flags)
{
	return 0;
}

static inline void check_highest_zone(int k)
{
}

#ifdef CONFIG_TMPFS
static inline int mpol_parse_str(char *str, struct mempolicy **mpol)
{
	return 1;	/* error */
}
#endif

static inline int mpol_misplaced(struct page *page, struct vm_area_struct *vma,
				 unsigned long address)
{
	return -1; /* no node preference */
}

static inline void mpol_put_task_policy(struct task_struct *task)
{
}
#endif /* CONFIG_NUMA */
#endif
