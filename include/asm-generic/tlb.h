/* include/asm-generic/tlb.h
 *
 *	Generic TLB shootdown code
 *
 * Copyright 2001 Red Hat, Inc.
 * Based on code from mm/memory.c Copyright Linus Torvalds and others.
 *
 * Copyright 2011 Red Hat, Inc., Peter Zijlstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_GENERIC__TLB_H
#define _ASM_GENERIC__TLB_H

#include <linux/mmu_notifier.h>
#include <linux/swap.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

#ifdef CONFIG_MMU

#ifdef CONFIG_HAVE_RCU_TABLE_FREE
/*
 * Semi RCU freeing of the page directories.
 *
 * This is needed by some architectures to implement software pagetable walkers.
 *
 * gup_fast() and other software pagetable walkers do a lockless page-table
 * walk and therefore needs some synchronization with the freeing of the page
 * directories. The chosen means to accomplish that is by disabling IRQs over
 * the walk.
 *
 * Architectures that use IPIs to flush TLBs will then automagically DTRT,
 * since we unlink the page, flush TLBs, free the page. Since the disabling of
 * IRQs delays the completion of the TLB flush we can never observe an already
 * freed page.
 *
 * Architectures that do not have this (PPC) need to delay the freeing by some
 * other means, this is that means.
 *
 * What we do is batch the freed directory pages (tables) and RCU free them.
 * We use the sched RCU variant, as that guarantees that IRQ/preempt disabling
 * holds off grace periods.
 *
 * However, in order to batch these pages we need to allocate storage, this
 * allocation is deep inside the MM code and can thus easily fail on memory
 * pressure. To guarantee progress we fall back to single table freeing, see
 * the implementation of tlb_remove_table_one().
 *
 */
struct mmu_table_batch {
	struct rcu_head		rcu;
	unsigned int		nr;
	void			*tables[0];
};

#define MAX_TABLE_BATCH		\
	((PAGE_SIZE - sizeof(struct mmu_table_batch)) / sizeof(void *))

extern void tlb_table_flush(struct mmu_gather *tlb);
extern void tlb_remove_table(struct mmu_gather *tlb, void *table);

#endif

/*
 * If we can't allocate a page to make a big batch of page pointers
 * to work on, then just handle a few from the on-stack structure.
 */
#define MMU_GATHER_BUNDLE	8

// 用于在 struct mmu_gather 中保存待处理的 struct page 页面
struct mmu_gather_batch {
	// next: 指向下一个 struct mmu_gather_batch 的指针，构成链表结构。
	struct mmu_gather_batch	*next;
	// nr: 当前 struct mmu_gather_batch 中实际保存的 struct page 数量。
	unsigned int		nr;
	// max: 当前 struct mmu_gather_batch 最多可以保存的 struct page 数量。
	unsigned int		max;
	// pages[0]: 保存 struct page 页面的数组。由于这是一个 C 语言中的“柔性数组成员”，因此它不占用 struct mmu_gather_batch 的实际空间，只在需要时分配足够的内存。
	struct page		*pages[0];
};

#define MAX_GATHER_BATCH	\
	((PAGE_SIZE - sizeof(struct mmu_gather_batch)) / sizeof(void *))

/*
 * Limit the maximum number of mmu_gather batches to reduce a risk of soft
 * lockups for non-preemptible kernels on huge machines when a lot of memory
 * is zapped during unmapping.
 * 10K pages freed at once should be safe even without a preemption point.
 */
#define MAX_GATHER_BATCH_COUNT	(10000UL/MAX_GATHER_BATCH)

/* struct mmu_gather is an opaque type used by the mm code for passing around
 * any data needed by arch specific code for tlb_remove_page.
 struct mmu_gather 是 mm 代码用于传递的不透明类型
 *arch 特定代码为 tlb_remove_page 所需的任何数据。

 struct mmu_gather 是一个用于对一个进程的内存空间进行操作的数据结构。它包含以下字段：
 */
struct mmu_gather {
	// mm：指向被操作的内存空间对应的 mm_struct 结构体的指针。
	struct mm_struct	*mm;
#ifdef CONFIG_HAVE_RCU_TABLE_FREE
	// batch：指向 MMU table batch 的指针，用于批量释放页表项。
	struct mmu_table_batch	*batch;
#endif
	// start 和 end：指定了被操作内存区域的开始和结束地址。
	unsigned long		start;
	unsigned long		end;
	/*
	 * we are in the middle of an operation to clear
	 * a full mm and can make some optimizations
	 */
	// fullmm：一个标志位，表示当前操作是否是清空整个内存空间，从而可以进行一些优化。
	unsigned int		fullmm : 1;

	/*
	 * we have performed an operation which
	 * requires a complete flush of the tlb
	 */
	// need_flush_all：一个标志位，表示进行了需要清空整个 TLB 的操作。
	unsigned int		need_flush_all : 1;

	/*
	 * we have removed page directories
	 */
	// freed_tables：一个标志位，表示已经删除了页目录表项。
	unsigned int		freed_tables : 1;

	/*
	 * at which levels have we cleared entries?
	 */
	// cleared_ptes、cleared_pmds、cleared_puds 和 cleared_p4ds：这些标志位分别表示在哪些页表级别上已经清空了页表项。
	unsigned int		cleared_ptes : 1;
	unsigned int		cleared_pmds : 1;
	unsigned int		cleared_puds : 1;
	unsigned int		cleared_p4ds : 1;
	// active：指向当前正在使用的 MMU table batch。
	struct mmu_gather_batch *active;
	// local：一个 MMU table batch，用于在不需要使用外部批量释放机制时使用。
	struct mmu_gather_batch	local;
	// __pages 和 batch_count：用于批量释放页表项的一些信息。
	struct page		*__pages[MMU_GATHER_BUNDLE];
	unsigned int		batch_count;
	// page_size：用于指定当前体系结构的页大小。
	int page_size;
};

#define HAVE_GENERIC_MMU_GATHER

void arch_tlb_gather_mmu(struct mmu_gather *tlb,
	struct mm_struct *mm, unsigned long start, unsigned long end);
void tlb_flush_mmu(struct mmu_gather *tlb);
void arch_tlb_finish_mmu(struct mmu_gather *tlb,
			 unsigned long start, unsigned long end, bool force);
void tlb_flush_mmu_free(struct mmu_gather *tlb);
extern bool __tlb_remove_page_size(struct mmu_gather *tlb, struct page *page,
				   int page_size);

static inline void __tlb_adjust_range(struct mmu_gather *tlb,
				      unsigned long address,
				      unsigned int range_size)
{
	tlb->start = min(tlb->start, address);
	tlb->end = max(tlb->end, address + range_size);
}

static inline void __tlb_reset_range(struct mmu_gather *tlb)
{
	if (tlb->fullmm) {
		tlb->start = tlb->end = ~0;
	} else {
		tlb->start = TASK_SIZE;
		tlb->end = 0;
	}
	tlb->freed_tables = 0;
	tlb->cleared_ptes = 0;
	tlb->cleared_pmds = 0;
	tlb->cleared_puds = 0;
	tlb->cleared_p4ds = 0;
}

static inline void tlb_flush_mmu_tlbonly(struct mmu_gather *tlb)
{
	if (!tlb->end)
		return;

	tlb_flush(tlb);
	mmu_notifier_invalidate_range(tlb->mm, tlb->start, tlb->end);
	__tlb_reset_range(tlb);
}

static inline void tlb_remove_page_size(struct mmu_gather *tlb,
					struct page *page, int page_size)
{
	if (__tlb_remove_page_size(tlb, page, page_size))
		tlb_flush_mmu(tlb);
}

static inline bool __tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	return __tlb_remove_page_size(tlb, page, PAGE_SIZE);
}

/* tlb_remove_page
 *	Similar to __tlb_remove_page but will call tlb_flush_mmu() itself when
 *	required.
 */
static inline void tlb_remove_page(struct mmu_gather *tlb, struct page *page)
{
	return tlb_remove_page_size(tlb, page, PAGE_SIZE);
}

#ifndef tlb_remove_check_page_size_change
#define tlb_remove_check_page_size_change tlb_remove_check_page_size_change
static inline void tlb_remove_check_page_size_change(struct mmu_gather *tlb,
						     unsigned int page_size)
{
	/*
	 * We don't care about page size change, just update
	 * mmu_gather page size here so that debug checks
	 * doesn't throw false warning.
	 */
#ifdef CONFIG_DEBUG_VM
	tlb->page_size = page_size;
#endif
}
#endif

static inline unsigned long tlb_get_unmap_shift(struct mmu_gather *tlb)
{
	if (tlb->cleared_ptes)
		return PAGE_SHIFT;
	if (tlb->cleared_pmds)
		return PMD_SHIFT;
	if (tlb->cleared_puds)
		return PUD_SHIFT;
	if (tlb->cleared_p4ds)
		return P4D_SHIFT;

	return PAGE_SHIFT;
}

static inline unsigned long tlb_get_unmap_size(struct mmu_gather *tlb)
{
	return 1UL << tlb_get_unmap_shift(tlb);
}

/*
 * In the case of tlb vma handling, we can optimise these away in the
 * case where we're doing a full MM flush.  When we're doing a munmap,
 * the vmas are adjusted to only cover the region to be torn down.
 */
#ifndef tlb_start_vma
#define tlb_start_vma(tlb, vma) do { } while (0)
#endif

#define __tlb_end_vma(tlb, vma)					\
	do {							\
		if (!tlb->fullmm)				\
			tlb_flush_mmu_tlbonly(tlb);		\
	} while (0)

#ifndef tlb_end_vma
#define tlb_end_vma	__tlb_end_vma
#endif

#ifndef __tlb_remove_tlb_entry
#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)
#endif

/**
 * tlb_remove_tlb_entry - remember a pte unmapping for later tlb invalidation.
 *
 * Record the fact that pte's were really unmapped by updating the range,
 * so we can later optimise away the tlb invalidate.   This helps when
 * userspace is unmapping already-unmapped pages, which happens quite a lot.
 */
#define tlb_remove_tlb_entry(tlb, ptep, address)		\
	do {							\
		__tlb_adjust_range(tlb, address, PAGE_SIZE);	\
		tlb->cleared_ptes = 1;				\
		__tlb_remove_tlb_entry(tlb, ptep, address);	\
	} while (0)

#define tlb_remove_huge_tlb_entry(h, tlb, ptep, address)	\
	do {							\
		unsigned long _sz = huge_page_size(h);		\
		__tlb_adjust_range(tlb, address, _sz);		\
		if (_sz == PMD_SIZE)				\
			tlb->cleared_pmds = 1;			\
		else if (_sz == PUD_SIZE)			\
			tlb->cleared_puds = 1;			\
		__tlb_remove_tlb_entry(tlb, ptep, address);	\
	} while (0)

/**
 * tlb_remove_pmd_tlb_entry - remember a pmd mapping for later tlb invalidation
 * This is a nop so far, because only x86 needs it.
 */
#ifndef __tlb_remove_pmd_tlb_entry
#define __tlb_remove_pmd_tlb_entry(tlb, pmdp, address) do {} while (0)
#endif

#define tlb_remove_pmd_tlb_entry(tlb, pmdp, address)			\
	do {								\
		__tlb_adjust_range(tlb, address, HPAGE_PMD_SIZE);	\
		tlb->cleared_pmds = 1;					\
		__tlb_remove_pmd_tlb_entry(tlb, pmdp, address);		\
	} while (0)

/**
 * tlb_remove_pud_tlb_entry - remember a pud mapping for later tlb
 * invalidation. This is a nop so far, because only x86 needs it.
 */
#ifndef __tlb_remove_pud_tlb_entry
#define __tlb_remove_pud_tlb_entry(tlb, pudp, address) do {} while (0)
#endif

#define tlb_remove_pud_tlb_entry(tlb, pudp, address)			\
	do {								\
		__tlb_adjust_range(tlb, address, HPAGE_PUD_SIZE);	\
		tlb->cleared_puds = 1;					\
		__tlb_remove_pud_tlb_entry(tlb, pudp, address);		\
	} while (0)

/*
 * For things like page tables caches (ie caching addresses "inside" the
 * page tables, like x86 does), for legacy reasons, flushing an
 * individual page had better flush the page table caches behind it. This
 * is definitely how x86 works, for example. And if you have an
 * architected non-legacy page table cache (which I'm not aware of
 * anybody actually doing), you're going to have some architecturally
 * explicit flushing for that, likely *separate* from a regular TLB entry
 * flush, and thus you'd need more than just some range expansion..
 *
 * So if we ever find an architecture
 * that would want something that odd, I think it is up to that
 * architecture to do its own odd thing, not cause pain for others
 * http://lkml.kernel.org/r/CA+55aFzBggoXtNXQeng5d_mRoDnaMBE5Y+URs+PHR67nUpMtaw@mail.gmail.com
 *
 * For now w.r.t page table cache, mark the range_size as PAGE_SIZE
 */

#ifndef pte_free_tlb
#define pte_free_tlb(tlb, ptep, address)			\
	do {							\
		__tlb_adjust_range(tlb, address, PAGE_SIZE);	\
		tlb->freed_tables = 1;				\
		tlb->cleared_pmds = 1;				\
		__pte_free_tlb(tlb, ptep, address);		\
	} while (0)
#endif

#ifndef pmd_free_tlb
#define pmd_free_tlb(tlb, pmdp, address)			\
	do {							\
		__tlb_adjust_range(tlb, address, PAGE_SIZE);	\
		tlb->freed_tables = 1;				\
		tlb->cleared_puds = 1;				\
		__pmd_free_tlb(tlb, pmdp, address);		\
	} while (0)
#endif

#ifndef __ARCH_HAS_4LEVEL_HACK
#ifndef pud_free_tlb
#define pud_free_tlb(tlb, pudp, address)			\
	do {							\
		__tlb_adjust_range(tlb, address, PAGE_SIZE);	\
		tlb->freed_tables = 1;				\
		tlb->cleared_p4ds = 1;				\
		__pud_free_tlb(tlb, pudp, address);		\
	} while (0)
#endif
#endif

#ifndef __ARCH_HAS_5LEVEL_HACK
#ifndef p4d_free_tlb
#define p4d_free_tlb(tlb, pudp, address)			\
	do {							\
		__tlb_adjust_range(tlb, address, PAGE_SIZE);	\
		tlb->freed_tables = 1;				\
		__p4d_free_tlb(tlb, pudp, address);		\
	} while (0)
#endif
#endif

#endif /* CONFIG_MMU */

#define tlb_migrate_finish(mm) do {} while (0)

#endif /* _ASM_GENERIC__TLB_H */
