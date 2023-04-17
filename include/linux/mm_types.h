/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/mm_types_task.h>

#include <linux/auxvec.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/uprobes.h>
#include <linux/page-flags-layout.h>
#include <linux/workqueue.h>

#include <asm/mmu.h>

#ifndef AT_VECTOR_SIZE_ARCH
#define AT_VECTOR_SIZE_ARCH 0
#endif
#define AT_VECTOR_SIZE (2*(AT_VECTOR_SIZE_ARCH + AT_VECTOR_SIZE_BASE + 1))

typedef int vm_fault_t;

struct address_space;
struct mem_cgroup;
struct hmm;

/*
 * Each physical page in the system has a struct page associated with
 * it to keep track of whatever it is we are using the page for at the
 * moment. Note that we have no way to track which tasks are using
 * a page, though if it is a pagecache page, rmap structures can tell us
 * who is mapping it.
 *
 * If you allocate the page using alloc_pages(), you can use some of the
 * space in struct page for your own purposes.  The five words in the main
 * union are available, except for bit 0 of the first word which must be
 * kept clear.  Many users use this word to store a pointer to an object
 * which is guaranteed to be aligned.  If you use the same storage as
 * page->mapping, you must restore it to NULL before freeing the page.
 *
 * If your page will not be mapped to userspace, you can also use the four
 * bytes in the mapcount union, but you must call page_mapcount_reset()
 * before freeing it.
 *
 * If you want to use the refcount field, it must be used in such a way
 * that other CPUs temporarily incrementing and then decrementing the
 * refcount does not cause problems.  On receiving the page from
 * alloc_pages(), the refcount will be positive.
 *
 * If you allocate pages of order > 0, you can use some of the fields
 * in each subpage, but you may need to restore some of their values
 * afterwards.
 *
 * SLUB uses cmpxchg_double() to atomically update its freelist and
 * counters.  That requires that freelist & counters be adjacent and
 * double-word aligned.  We align all struct pages to double-word
 * boundaries, and ensure that 'freelist' is aligned within the
 * struct.
 */
#ifdef CONFIG_HAVE_ALIGNED_STRUCT_PAGE
#define _struct_page_alignment	__aligned(2 * sizeof(unsigned long))
#else
#define _struct_page_alignment
#endif
/*
Buddy和Slab算法是用于不同的内存分配场景的。Buddy算法用于物理内存分配，而Slab算法用于内核内存分配。
 	|-------------|
    | Page Frames |
    |-------------|
             |
             V
    |-------------|
    |  Page Frame |
    |   Allocator |
    |-------------|
             |
             V
    |-------------|
    |    Buddy    |
    |   Allocator |
    |-------------|
             |
             V
    |-------------|
    |     Slab    |
    |   Allocator |
    |-------------|
             |
             V
    |-------------|
    |  Page Frame |
    |   Reclaimer |
    |-------------|
*/
// 系统中的每一页物理内存的属性
struct page {
	// flags：用于记录该页的一些标志位，例如是否被锁定、是否为脏页等。
	unsigned long flags;		/* Atomic flags, some possibly
					 * updated asynchronously */
	/*
	 * Five words (20/40 bytes) are available in this union.
	 * WARNING: bit 0 of the first word is used for PageTail(). That
	 * means the other users of this union MUST NOT use the bit to
	 * avoid collision and false-positive PageTail().
	 */
	// union：这里是一个联合体，用于描述页面在不同场景下的属性。其中包括了页面缓存、匿名页、slab/slob/slub 等用途的页。
	union {
		struct {	/* Page cache and anonymous pages */
			/**
			 * @lru: Pageout list, eg. active_list protected by
			 * zone_lru_lock.  Sometimes used as a generic list
			 * by the page owner.
			 */
			// lru：页面在页表缓存中的 LRU 链表中的位置。
			struct list_head lru;
			/* See page-flags.h for PAGE_MAPPING_FLAGS */
			// mapping：如果该页为文件页，则该指针指向它所在文件的地址空间的 address_space 结构体。
			struct address_space *mapping;
			// index：该页在 mapping 中的偏移量。
			pgoff_t index;		/* Our offset within mapping. */
			/**
			 * @private: Mapping-private opaque data.
			 * Usually used for buffer_heads if PagePrivate.
			 * Used for swp_entry_t if PageSwapCache.
			 * Indicates order in the buddy system if PageBuddy.
			 */
			// private：存储映射私有数据，例如 buffer_heads 或 swp_entry_t。
			unsigned long private;
		};
		struct {	/* slab, slob and slub */
			union {
				// slab_list：该页所属于的 kmem_cache 的链表头。
				struct list_head slab_list;	/* uses lru */
				struct {	/* Partial pages */
					struct page *next;
#ifdef CONFIG_64BIT
					int pages;	/* Nr of pages left */
					int pobjects;	/* Approximate count */
#else
					short int pages;
					short int pobjects;
#endif
				};
			};
			// slab_cache：该页所属于的 kmem_cache 的指针。
			struct kmem_cache *slab_cache; /* not slob */
			/* Double-word boundary */
			// freelist：在 slab 模式下，该页上的第一个自由对象的地址。
			void *freelist;		/* first free object */
			union {
				// s_mem：在 slab 模式下，该页上第一个对象的地址。
				void *s_mem;	/* slab: first object */
				// SLUB是Linux内核的一种内存分配器，它是SLAB分配器的改进版。SLUB分配器相较于SLAB分配器的一个优势在于，
				// 它能够在一定程度上避免锁的争用。SLUB分配器不使用全局锁，而是使用更细粒度的锁，使得多个CPU可以并发地分配内存。
				// 此外，SLUB分配器对于内存的使用效率也有一定的提高。
				// SLUB分配器使用伙伴系统来管理内存池，并将每个池中的空闲内存按大小分类，以便在不同大小的内存请求之间进行快速查找。
				// counters：在 slub 模式下，该页中对象数目的计数器。
				unsigned long counters;		/* SLUB */
				// inuse/objects/frozen：在 slub 模式下，记录该页当前使用中的对象数目、总对象数目以及是否被冻结等信息。
				struct {			/* SLUB */
					// 这是一个位域(bitfield)结构体成员，表示该字段占用16个bit位，
					// 用来记录SLUB分配器中某个对象的使用情况，即该对象是否被占用。
					// 其中，inuse为字段名，16为字段宽度。位域的使用可以节省内存空间，
					// 因为它们允许多个字段共享同一块存储区域的不同部分，而不是每个字段都独占一块存储区域。
					unsigned inuse:16;
					unsigned objects:15;
					unsigned frozen:1;
				};
			};
		};
		// 组合页（Compound Page）是指 Linux 内核中的一种特殊页面类型，它由多个物理页框（Physical Frame）组合成一个逻辑页。
		// 组合页主要用于减小内存碎片化和优化页表。组合页的大小可以是 2、4、8 个物理页框的大小，由 compound_order 字段指定。
		// 组合页的第一个物理页框称为 head，其余的物理页框称为 tail。在 head 中保存了一些组合页相关的元数据信息，如 compound_dtor 和 compound_mapcount 等。
		// 而在 tail 中，则保存了组合页的实际数据。组合页主要用于两个方面：

		// THP是Transparent Huge Pages（透明大页面）的缩写，是一种Linux内核提供的优化内存管理的技术。它的主要目的是提高内存的访问效率，减少页表的开销。传统的页大小（page size）通常是4KB或者8KB，而THP则将多个页合并成一个大页面，一般大小为2MB或者1GB，这样在内存管理时可以减少页表的数量，提高访问效率。

		// THP可以自动地将多个紧邻的页面合并成一个大页面，而不需要修改应用程序的代码。
		// 当需要大量内存时，THP可以减少页表的大小，从而降低内存访问时的延迟。
		// THP的实现过程中需要考虑很多细节，例如内存对齐、虚拟内存地址空间的连续性等等。
		// 虽然THP有一些局限性，例如内存分配时需要额外的内存碎片以及内存分配的时间开销，但在大多数情况下THP可以有效地提高内存访问效率。
		struct {	/* Tail pages of compound page */
			// compound_head：如果该页是组合页（例如 THP），则该字段记录它的头部页面的地址。
			unsigned long compound_head;	/* Bit zero is set */
			/* First tail page only */
			// compound_dtor：如果该页是组合页，则该字段记录析构函数的地址。
			unsigned char compound_dtor;
			// compound_order：如果该页是组合页，则该字段记录它所属的 2 的幂次方。
			unsigned char compound_order;
			// compound_mapcount：如果该页是组合页，则该字段记录所有尾部页面中的映射计数的和。
			atomic_t compound_mapcount;
		};
		struct {	/* Second tail page of compound page */
			unsigned long _compound_pad_1;	/* compound_head */
			unsigned long _compound_pad_2;
			// deferred_list：在组合页中，该页为最后一页时，它所属的组合页的下一个组合页的地址。
			struct list_head deferred_list;
		};
		struct {	/* Page table pages */
			unsigned long _pt_pad_1;	/* compound_head */
			// pmd_huge_pte：如果该页是页表页，则该字段指向页表项。
			pgtable_t pmd_huge_pte; /* protected by page->ptl */
			unsigned long _pt_pad_2;	/* mapping */
			// pt_mm/pt_frag_refcount：如果该页是页表页，则该字段记录其所属的进程的 mm_struct 结构体或记录页表片段的引用计数。
			union {
				struct mm_struct *pt_mm; /* x86 pgds only */
				atomic_t pt_frag_refcount; /* powerpc */
			};
#if ALLOC_SPLIT_PTLOCKS
			spinlock_t *ptl;
#else
			// ptl：如果该页是页表页，则该字段记录它的自旋锁。
			spinlock_t ptl;
#endif
		};
		struct {	/* ZONE_DEVICE pages */
			/** @pgmap: Points to the hosting device page map. */
			// pgmap：如果该页属于一个分配在设备上的内存区域，则该指针指向对应的 dev_pagemap 结构体。
			struct dev_pagemap *pgmap;
			// hmm_data：如果该页属于 HMM 区域，则该字段用于存储 HMM 相关的信息。
			unsigned long hmm_data;
			unsigned long _zd_pad_1;	/* uses mapping */
		};

		/** @rcu_head: You can use this to free a page by RCU. */
		struct rcu_head rcu_head;
	};

	union {		/* This union is 4 bytes in size. */
		/*
		 * If the page can be mapped to userspace, encodes the number
		 * of times this page is referenced by a page table.
		 */
		// _mapcount：如果该页可以映射到用户空间，则该字段记录其在页表中的引用次数。
		atomic_t _mapcount;

		/*
		 * If the page is neither PageSlab nor mappable to userspace,
		 * the value stored here may help determine what this page
		 * is used for.  See page-flags.h for a list of page types
		 * which are currently stored here.
		 */
		// page_type：如果该页既不是 slab 页，也不是可映射到用户空间的匿名页，则该字段可能记录它所属的特定类型，例如 DMA 内存、高端
		unsigned int page_type;

		unsigned int active;		/* SLAB */
		int units;			/* SLOB */
	};

	/* Usage count. *DO NOT USE DIRECTLY*. See page_ref.h */
	atomic_t _refcount;

#ifdef CONFIG_MEMCG
	struct mem_cgroup *mem_cgroup;
#endif

	/*
	 * On machines where all RAM is mapped into kernel address space,
	 * we can simply calculate the virtual address. On machines with
	 * highmem some memory is mapped into kernel virtual memory
	 * dynamically, so we need a place to store that address.
	 * Note that this field could be 16 bits on x86 ... ;)
	 *
	 * Architectures with slow multiplication can define
	 * WANT_PAGE_VIRTUAL in asm/page.h
	 */
#if defined(WANT_PAGE_VIRTUAL)
	void *virtual;			/* Kernel virtual address (NULL if
					   not kmapped, ie. highmem) */
#endif /* WANT_PAGE_VIRTUAL */

#ifdef LAST_CPUPID_NOT_IN_PAGE_FLAGS
	int _last_cpupid;
#endif
} _struct_page_alignment;

/*
 * Used for sizing the vmemmap region on some architectures
 */
#define STRUCT_PAGE_MAX_SHIFT	(order_base_2(sizeof(struct page)))

#define PAGE_FRAG_CACHE_MAX_SIZE	__ALIGN_MASK(32768, ~PAGE_MASK)
#define PAGE_FRAG_CACHE_MAX_ORDER	get_order(PAGE_FRAG_CACHE_MAX_SIZE)

struct page_frag_cache {
	void * va;
#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
	__u16 offset;
	__u16 size;
#else
	__u32 offset;
#endif
	/* we maintain a pagecount bias, so that we dont dirty cache line
	 * containing page->_refcount every time we allocate a fragment.
	 */
	unsigned int		pagecnt_bias;
	bool pfmemalloc;
};

typedef unsigned long vm_flags_t;

/*
 * A region containing a mapping of a non-memory backed file under NOMMU
 * conditions.  These are held in a global tree and are pinned by the VMAs that
 * map parts of them.
 * 包含 NOMMU 下非内存备份文件映射的区域状况。它们保存在全局树中，并由 VMA 固定映射其中的一部分。
 */
struct vm_region {
	struct rb_node	vm_rb;		/* link in global region tree */
	vm_flags_t	vm_flags;	/* VMA vm_flags */
	unsigned long	vm_start;	/* start address of region */
	unsigned long	vm_end;		/* region initialised to here */
	unsigned long	vm_top;		/* region allocated to here */
	unsigned long	vm_pgoff;	/* the offset in vm_file corresponding to vm_start */
	struct file	*vm_file;	/* the backing file or NULL */

	int		vm_usage;	/* region usage count (access under nommu_region_sem) */
	bool		vm_icache_flushed : 1; /* true if the icache has been flushed for
						* this region */
};

#ifdef CONFIG_USERFAULTFD
#define NULL_VM_UFFD_CTX ((struct vm_userfaultfd_ctx) { NULL, })
struct vm_userfaultfd_ctx {
	struct userfaultfd_ctx *ctx;
};
#else /* CONFIG_USERFAULTFD */
#define NULL_VM_UFFD_CTX ((struct vm_userfaultfd_ctx) {})
struct vm_userfaultfd_ctx {};
#endif /* CONFIG_USERFAULTFD */

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
/*
 * VMM是Virtual Memory Manager的缩写，指的是虚拟内存管理器，是操作系统内存管理的一部分，
 * 用于将虚拟地址转换为物理地址，使得进程可以使用多个地址空间，并在需要时自动将数据交换到物理内存或磁盘上。
 * VMM还负责管理内存的分配和释放，以及虚拟内存和物理内存之间的映射。在Linux中，VMM的实现是通过内核中的mm子系统来完成的。
 * 
 * 这个结构定义了一个VMM内存区域。每个VM区域/任务都有一个这样的结构体。
 * VM区域是进程虚拟内存空间的任何部分，它对于页面故障处理程序有特殊规则（例如共享库、可执行区域等）
 * 
 * struct vm_area_struct 表示了一个进程的虚拟内存区域，它是由一系列页面组成的连续内存区域，
 * 每个页面都有一个虚拟地址和一个物理地址。它存储了一些关键信息，如起始和结束地址、访问权限、所属进程、映射的文件、私有数据等等。
 * 在内核中，它以红黑树的形式被组织起来，每个进程都有一个 vm_area_struct 的链表用于管理它的内存空间。
*/

struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */
	// vm_start：当前虚拟内存区域的起始地址；
	unsigned long vm_start;		/* Our start address within vm_mm. */
	// vm_end：当前虚拟内存区域的结束地址；
	unsigned long vm_end;		/* The first byte after our end address
					   within vm_mm. */
	// vm_next：当前虚拟内存区域在进程虚拟地址空间中下一个虚拟内存区域的指针；
	// vm_prev：当前虚拟内存区域在进程虚拟地址空间中上一个虚拟内存区域的指针；
	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

	// vm_rb：用于红黑树的节点；
	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
	// rb_subtree_gap：在红黑树中，当前节点与左边兄弟节点的空闲空间大小；
	unsigned long rb_subtree_gap;

	/* Second cache line starts here. */
	// vm_mm：指向当前虚拟内存区域所属的进程的内存描述符（struct mm_struct）；
	struct mm_struct *vm_mm;	/* The address space we belong to. */
	// vm_page_prot：当前虚拟内存区域的访问权限；
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */
	// vm_flags：当前虚拟内存区域的属性标志，例如读写、共享、私有等；
	unsigned long vm_flags;		/* Flags, see mm.h. */

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap interval tree.
	 */
	// shared：用于共享文件映射的红黑树节点和对应的偏移量；
	struct {
		struct rb_node rb;
		unsigned long rb_subtree_last;
	} shared;

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
	// 匿名内存指的是在进程地址空间中分配但不关联任何文件或设备的内存，也称为未命名内存或私有内存。
	// 通常用于堆、栈和共享内存等场景，对应的函数是mmap或brk/sbrk。
	// 与之相对的是显式内存，即通过mmap等函数显式地将某个文件或设备映射到进程地址空间。
	// 匿名内存分配后可以用于存储进程的数据，但在进程退出时会被释放，数据会丢失。
	// anon_vma_chain：当前虚拟内存区域所属的匿名内存映射链表；
	struct list_head anon_vma_chain; /* Serialized by mmap_sem &
					  * page_table_lock */
	// anon_vma：指向匿名内存映射的描述符（struct anon_vma）；
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
	// vm_ops：虚拟内存区域的操作函数；
	const struct vm_operations_struct *vm_ops;

	/* Information about our backing store: */
	// vm_pgoff：当前虚拟内存区域在文件中的偏移量；
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
					   units */
	// vm_file：映射到当前虚拟内存区域的文件指针；
	struct file * vm_file;		/* File we map to (can be NULL). */
	// vm_private_data：用于保存当前虚拟内存区域的私有数据；
	void * vm_private_data;		/* was vm_pte (shared mem) */
	// swap_readahead_info：用于异步读取交换区页面的计数器；
	atomic_long_t swap_readahead_info;
#ifndef CONFIG_MMU
	// vm_region：用于保存非常规映射的区域信息；
	struct vm_region *vm_region;	/* NOMMU mapping region */
#endif
#ifdef CONFIG_NUMA
	// vm_policy：用于保存NUMA内存策略；
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
	// vm_userfaultfd_ctx：用户异常处理相关上下文。
	struct vm_userfaultfd_ctx vm_userfaultfd_ctx;
} __randomize_layout;

struct core_thread {
	struct task_struct *task;
	struct core_thread *next;
};

struct core_state {
	atomic_t nr_threads;
	struct core_thread dumper;
	struct completion startup;
};

struct kioctx_table;
// mm_struct: 用于描述进程的内存映射
// 一个进程的内存空间通常由多个虚拟内存空间组成。每个虚拟内存空间（VMA）都对应着一段连续的虚拟地址空间，
// 它们可以映射到物理内存中的不同区域。每个 VMA 通常具有特定的属性，如可读可写可执行、私有共享等。
// 进程的内存空间可以包含代码段、数据段、堆、栈以及其他映射区域等。
struct mm_struct {
	struct {
		// mmap: 链表指向进程的VMA区间
		struct vm_area_struct *mmap;		/* list of VMAs */
		// mm_rb: 一个红黑树，用于快速查找一个给定虚拟地址所在的VMA区间
		struct rb_root mm_rb;
		// vmacache_seqnum: 用于快速查找最近使用的VMA区间
		u64 vmacache_seqnum;                   /* per-thread vmacache */
#ifdef CONFIG_MMU
		// get_unmapped_area: 寻找进程空间中指定大小的未映射的线性地址，供动态库加载、mmap等系统调用使用
		unsigned long (*get_unmapped_area) (struct file *filp,
				unsigned long addr, unsigned long len,
				unsigned long pgoff, unsigned long flags);
#endif
		// mmap_base: 进程地址空间最低的地址
		unsigned long mmap_base;	/* base of mmap area */
		// mmap_legacy_base: 废弃的进程地址空间最低的地址
		unsigned long mmap_legacy_base;	/* base of mmap area in bottom-up allocations */
#ifdef CONFIG_HAVE_ARCH_COMPAT_MMAP_BASES
		/* Base adresses for compatible mmap() */
		// mmap_compat_base: 32位应用程序的 mmap() 兼容地址基址
		unsigned long mmap_compat_base;
		// mmap_compat_legacy_base: 废弃的32位应用程序的 mmap() 兼容地址基址
		unsigned long mmap_compat_legacy_base;
#endif
		// task_size: 进程地址空间的大小
		unsigned long task_size;	/* size of task vm space */
		// highest_vm_end: 进程地址空间中目前最高的VMA区间的结束地址
		unsigned long highest_vm_end;	/* highest vma end address */
		// pgd: 进程的页全局目录表，存放虚拟地址和物理地址的映射关系
		pgd_t * pgd;

		/**
		 * @mm_users: The number of users including userspace.
		 *
		 * Use mmget()/mmget_not_zero()/mmput() to modify. When this
		 * drops to 0 (i.e. when the task exits and there are no other
		 * temporary reference holders), we also release a reference on
		 * @mm_count (which may then free the &struct mm_struct if
		 * @mm_count also drops to 0).
		 */
		// mm_users: 该进程的引用计数，包括用户进程引用和内核进程引用，当引用计数变为 0 时进程的 mm_struct 可以被释放
		atomic_t mm_users;

		/**
		 * @mm_count: The number of references to &struct mm_struct
		 * (@mm_users count as 1).
		 *
		 * Use mmgrab()/mmdrop() to modify. When this drops to 0, the
		 * &struct mm_struct is freed.
		 */
		// mm_count: 该 mm_struct 的引用计数，当引用计数变为 0 时 mm_struct 可以被释放
		atomic_t mm_count;

#ifdef CONFIG_MMU
		// pgtables_bytes: 页表的占用字节数
		atomic_long_t pgtables_bytes;	/* PTE page table pages */
#endif
		// map_count: 进程映射的 VMA 区间的数量
		int map_count;			/* number of VMAs */
		// page_table_lock: 页表的锁
		spinlock_t page_table_lock; /* Protects page tables and some
					     * counters
					     */
		// mmap_sem: 用于保护 VMA 区间链表的信号量
		struct rw_semaphore mmap_sem;
		// mmlist: 挂载的进程的列表，用于内存压缩时记录已经压缩的进程
		struct list_head mmlist; /* List of maybe swapped mm's.	These
					  * are globally strung together off
					  * init_mm.mmlist, and are protected
					  * by mmlist_lock
					  */

		// hiwater_rss: 进程的高水位 RSS，即进程所用的最大内存
		unsigned long hiwater_rss; /* High-watermark of RSS usage */
		// hiwater_vm: 进程的高水位虚拟地址，即进程所用的最大虚拟地址
		unsigned long hiwater_vm;  /* High-water virtual memory usage */
		// total_vm: 进程的地址空间中的总页数
		unsigned long total_vm;	   /* Total pages mapped */
		// locked_vm: 进程中被锁住的页面数
		unsigned long locked_vm;   /* Pages that have PG_mlocked set */
		// pinned_vm: 进程中被固定的页面数
		unsigned long pinned_vm;   /* Refcount permanently increased */
		// data_vm: 进程中数据段占用的页数
		unsigned long data_vm;	   /* VM_WRITE & ~VM_SHARED & ~VM_STACK */
		// exec_vm: 进程中代码段占用的页数
		unsigned long exec_vm;	   /* VM_EXEC & ~VM_WRITE & ~VM_STACK */
		// stack_vm: 进程中栈段占用的页数
		unsigned long stack_vm;	   /* VM_STACK */
		// def_flags: 默认的 VMA 标志位
		unsigned long def_flags;

		// arg_lock: 用于保护以下成员的自旋锁
		spinlock_t arg_lock; /* protect the below fields */
		// start_code: 进程代码段开始的地址, end_code: 进程代码段结束的地址, start_data: 进程数据段开始的地址, end_data: 进程数据段结束的地址
		unsigned long start_code, end_code, start_data, end_data;
		// start_brk：进程的堆的开始地址。brk：当前进程的“程序员可扩展堆”的结尾地址, start_stack：栈的开始地址，一般为 brk 的下一个地址。
		unsigned long start_brk, brk, start_stack;
		// arg_start：参数区域的开始地址。arg_end：参数区域的结束地址。env_start：环境变量区域的开始地址。env_end：环境变量区域的结束地址。
		unsigned long arg_start, arg_end, env_start, env_end;
		// saved_auxv：进程的AUXV向量，用于给动态链接器传递信息。
		unsigned long saved_auxv[AT_VECTOR_SIZE]; /* for /proc/PID/auxv */

		/*
		 * Special counters, in some configurations protected by the
		 * page_table_lock, in other configurations by being atomic.
		 */
		// rss_stat：RSS相关的统计信息，如高水印和计数器。
		struct mm_rss_stat rss_stat;
		// binfmt：二进制格式解析器，解析并载入可执行文件。
		struct linux_binfmt *binfmt;

		// context：体系结构相关的MM上下文，如页表、寄存器值等。
		/* Architecture-specific MM context */
		mm_context_t context;

		// flags：MM的标志位，必须使用原子操作读写。
		unsigned long flags; /* Must use atomic bitops to access */
		// core_state：核心转储支持，用于实现进程的核心转储功能。
		struct core_state *core_state; /* coredumping support */
#ifdef CONFIG_MEMBARRIER
		// membarrier_state：内存屏障状态，可用于内存屏障相关的同步操作。
		atomic_t membarrier_state;
#endif
#ifdef CONFIG_AIO
		// ioctx_lock：AIO操作的自旋锁。
		spinlock_t			ioctx_lock;
		// ioctx_table：AIO操作的上下文管理表。
		struct kioctx_table __rcu	*ioctx_table;
#endif
#ifdef CONFIG_MEMCG
		/*
		 * "owner" points to a task that is regarded as the canonical
		 * user/owner of this mm. All of the following must be true in
		 * order for it to be changed:
		 *
		 * current == mm->owner
		 * current->mm != mm
		 * new_owner->mm == mm
		 * new_owner->alloc_lock is held
		 */
		// owner：指向被视为此MM的规范用户/所有者的任务。
		struct task_struct __rcu *owner;
#endif
		// user_ns：用户命名空间，进程所属的命名空间。
		struct user_namespace *user_ns;

		// exe_file：指向与进程可执行文件/解释器的/proc/<pid>/exe符号链接相关联的文件对象。
		/* store ref to file /proc/<pid>/exe symlink points to */
		struct file __rcu *exe_file;
#ifdef CONFIG_MMU_NOTIFIER
		// mmu_notifier_mm：MMU通知器，可用于与MMU操作有关的事件通知和同步操作。
		struct mmu_notifier_mm *mmu_notifier_mm;
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && !USE_SPLIT_PMD_PTLOCKS
		// pmd_huge_pte：透明大页的页表项。
		pgtable_t pmd_huge_pte; /* protected by page_table_lock */
#endif
#ifdef CONFIG_NUMA_BALANCING
		/*
		 * numa_next_scan is the next time that the PTEs will be marked
		 * pte_numa. NUMA hinting faults will gather statistics and
		 * migrate pages to new nodes if necessary.
		 */
		// numa_next_scan：下一次标记PTE为pte_numa的时间戳。
		unsigned long numa_next_scan;

		/* Restart point for scanning and setting pte_numa */
		// numa_scan_offset：扫描和设置pte_numa的重启点。
		unsigned long numa_scan_offset;

		/* numa_scan_seq prevents two threads setting pte_numa */
		// numa_scan_seq：用于防止两个线程同时设置pte_numa的计数器。
		int numa_scan_seq;
#endif
		/*
		 * An operation with batched TLB flushing is going on. Anything
		 * that can move process memory needs to flush the TLB when
		 * moving a PROT_NONE or PROT_NUMA mapped page.
		 */
		// tlb_flush_pending：用于指示是否有操作需要刷新TLB的原子变量。
		atomic_t tlb_flush_pending;
#ifdef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH
		/* See flush_tlb_batched_pending() */
		// tlb_flush_batched：用于指示是否有挂起的批量TLB刷新操作的标志。
		bool tlb_flush_batched;
#endif
		// uprobes_state：用户态probes的状态，用于实现uprobes机制。
		struct uprobes_state uprobes_state;
#ifdef CONFIG_HUGETLB_PAGE
		// hugetlb_usage：用于跟踪使用的巨大页数的原子计数器。
		atomic_long_t hugetlb_usage;
#endif
		// async_put_work：用于异步释放MM对象的工作项。
		struct work_struct async_put_work;

#if IS_ENABLED(CONFIG_HMM)
		/* HMM needs to track a few things per mm */
		// cpu_bitmap：指向动态大小的mm_cpumask，用于跟踪与MM相关的CPU和NUMA节点的使用情况。
		struct hmm *hmm;
#endif
	} __randomize_layout;

	/*
	 * The mm_cpumask needs to be at the end of mm_struct, because it
	 * is dynamically sized based on nr_cpu_ids.
	 */
	// cpu_bitmap 被用于存储一个进程可运行在哪些 CPU 上的位图
	unsigned long cpu_bitmap[];
};

extern struct mm_struct init_mm;

/* Pointer magic because the dynamic array size confuses some compilers. */
/*指针魔术，因为动态数组大小混淆了一些编译器。 */
/**
 * 将 mm 结构体的地址转换为无符号长整型，并将结果赋值给 cpu_bitmap 变量。
 * 使用 offsetof 宏计算出 mm_struct 结构体中 cpu_bitmap 成员相对于结构体起始位置的偏移量，并将其加到 cpu_bitmap 变量上，得到指向 cpu_bitmap 的指针。
 * 将指针转换为 cpumask 类型，并清空其内容，即初始化 CPU 掩码。
 */
static inline void mm_init_cpumask(struct mm_struct *mm)
{
	unsigned long cpu_bitmap = (unsigned long)mm;

	cpu_bitmap += offsetof(struct mm_struct, cpu_bitmap);
	cpumask_clear((struct cpumask *)cpu_bitmap);
}

/* Future-safe accessor for struct mm_struct's cpu_vm_mask. */
static inline cpumask_t *mm_cpumask(struct mm_struct *mm)
{
	return (struct cpumask *)&mm->cpu_bitmap;
}

struct mmu_gather;
/*
tlb_gather_mmu()函数用于收集TLB缓存中虚拟内存区域对应的页表项。它的参数tlb是一个指向struct mmu_gather的指针，
用于表示TLB的收集状态；mm是一个指向struct mm_struct的指针，表示虚拟地址空间；start和end表示虚拟地址空间中需要收集的起始地址和结束地址。
该函数会调用硬件提供的TLB缓存管理指令，遍历虚拟地址空间中的页表项，并把这些页表项从TLB缓存中删除。
这个过程中，收集的页表项存储在tlb->active成员中，等到收集结束后会统一写回页表中。
*/
extern void tlb_gather_mmu(struct mmu_gather *tlb, struct mm_struct *mm,
				unsigned long start, unsigned long end);
/*
tlb_finish_mmu()函数用于结束mmu的收集，并将它们刷新到TLB(翻译为快表)中。在更新虚拟内存映射的过程中，由于处理器高速缓存(TLB)的存在，
内存页的映射关系发生变化，为保证内存访问的正确性，需要刷新TLB中对应内存页的映射信息。tlb_finish_mmu()函数就是负责执行这一操作。
*/
extern void tlb_finish_mmu(struct mmu_gather *tlb,
				unsigned long start, unsigned long end);

/*
该变量表示需要刷新的 TLB 页表项数量，当该变量值大于0时表示需要进行 TLB 刷新操作。
该原子变量是在多 CPU 内核上对 TLB 刷新操作进行同步的一种手段。
在进行修改虚拟内存映射的操作时，该变量可能会被递增。在某些情况下，操作系统需要等待所有 TLB 刷新操作完成之后，才能完成某些操作，
如卸载一个内存区域。因此，通过初始化该变量为 0，可以避免不必要的等待。
*/
static inline void init_tlb_flush_pending(struct mm_struct *mm)
{
	atomic_set(&mm->tlb_flush_pending, 0);
}

/*
用于增加一个进程中需要刷新的TLB条目的数量，即在进程发生页表更新时需要刷新的页表项数目。这个数量是通过对进程结构体中的原子计数器 tlb_flush_pending 进行加一操作实现的。
*/
static inline void inc_tlb_flush_pending(struct mm_struct *mm)
{
	atomic_inc(&mm->tlb_flush_pending);
	/*
	 * The only time this value is relevant is when there are indeed pages
	 * to flush. And we'll only flush pages after changing them, which
	 * requires the PTL.
	 *
	 * So the ordering here is:
	 *
	 *	atomic_inc(&mm->tlb_flush_pending);
	 *	spin_lock(&ptl);
	 *	...
	 *	set_pte_at();
	 *	spin_unlock(&ptl);
	 *
	 *				spin_lock(&ptl)
	 *				mm_tlb_flush_pending();
	 *				....
	 *				spin_unlock(&ptl);
	 *
	 *	flush_tlb_range();
	 *	atomic_dec(&mm->tlb_flush_pending);
	 *
	 * Where the increment if constrained by the PTL unlock, it thus
	 * ensures that the increment is visible if the PTE modification is
	 * visible. After all, if there is no PTE modification, nobody cares
	 * about TLB flushes either.
	 *
	 * This very much relies on users (mm_tlb_flush_pending() and
	 * mm_tlb_flush_nested()) only caring about _specific_ PTEs (and
	 * therefore specific PTLs), because with SPLIT_PTE_PTLOCKS and RCpc
	 * locks (PPC) the unlock of one doesn't order against the lock of
	 * another PTL.
	 *
	 * The decrement is ordered by the flush_tlb_range(), such that
	 * mm_tlb_flush_pending() will not return false unless all flushes have
	 * completed.
	 */
}
/*
 * 在多处理器系统中，每个 CPU 都有自己的 TLB，当内核需要刷新 TLB 时，需要确保所有 CPU 的 TLB 都被刷新。
 每次执行完 TLB flush 操作后，内核都会调用这个函数，将 tlb_flush_pending 原子变量减 1。当原子变量的值减为 0 时，表示所有的 TLB flush 操作都已经执行完毕。
*/
static inline void dec_tlb_flush_pending(struct mm_struct *mm)
{
	/*
	 * See inc_tlb_flush_pending().
	 *
	 * This cannot be smp_mb__before_atomic() because smp_mb() simply does
	 * not order against TLB invalidate completion, which is what we need.
	 *
	 * Therefore we must rely on tlb_flush_*() to guarantee order.
	 */
	atomic_dec(&mm->tlb_flush_pending);
}

/*
检查一个进程的 TLB（Translation Lookaside Buffer，翻译后备缓存）是否有 flush（刷新）请求待处理的。
TLB 是一个硬件缓存，用于将虚拟地址映射为物理地址。在对虚拟地址空间做出修改（例如删除页面）时，需要刷新 TLB。
这个函数检查进程的 TLB 是否处于待刷新状态，即是否存在待处理的刷新请求。如果存在，返回 true，否则返回 false。
*/
static inline bool mm_tlb_flush_pending(struct mm_struct *mm)
{
	/*
	PTL是Page Table Lock的缩写。它是Linux内核中用于保护进程的页表的锁。由于在多核CPU上并发地更新进程的页表可能会导致一些问题，
	如内存泄漏和页面覆盖，因此Linux内核使用了一种锁机制来保护它们。PTL是一个自旋锁，只允许一个内核线程在同一时刻持有它。当一个线程需要修改页表时，它必须先获取该进程的PTL，以防止其他线程并发访问。
	 * Must be called after having acquired the PTL; orders against that
	 * PTLs release and therefore ensures that if we observe the modified
	 * PTE we must also observe the increment from inc_tlb_flush_pending().
	 *
	 * That is, it only guarantees to return true if there is a flush
	 * pending for _this_ PTL.
	 */
	return atomic_read(&mm->tlb_flush_pending);
}

/*
检查给定进程的 mm 结构是否需要执行嵌套的 TLB 刷新。
嵌套的 TLB 刷新是指由于某些操作（如 fork）而导致的对父进程和子进程的 TLB 刷新。
在这种情况下，父进程需要等待子进程的 TLB 刷新完成后才能继续执行，以确保一致性。这个函数会检查 mm 结构中的 tlb_flush_nested 标志是否被设置，并返回相应的布尔值。 */
static inline bool mm_tlb_flush_nested(struct mm_struct *mm)
{
	/*
	 * Similar to mm_tlb_flush_pending(), we must have acquired the PTL
	 * for which there is a TLB flush pending in order to guarantee
	 * we've seen both that PTE modification and the increment.
	 *
	 * (no requirement on actually still holding the PTL, that is irrelevant)
	 */
	return atomic_read(&mm->tlb_flush_pending) > 1;
}

struct vm_fault;

// 该结构体描述了一种特殊的内存映射，用于在进程地址空间中创建一些特殊区域，例如 VDSO、VVAR 等。其中成员包括：
struct vm_special_mapping {
	// name：表示该特殊映射的名称，如 "[vdso]"；
	const char *name;	/* The name, e.g. "[vdso]". */

	/*
	 * If .fault is not provided, this points to a
	 * NULL-terminated array of pages that back the special mapping.
	 *
	 * This must not be NULL unless .fault is provided.
	 */
	// pages：表示与该特殊映射相关联的页数组，它们可以在特殊映射的虚拟地址上提供内容。
	// 如果 pages 为 NULL，则该映射将通过 .fault 函数提供缺页处理，否则 pages 数组中的页将被映射到特殊映射的虚拟地址；
	struct page **pages;

	/*
	 * If non-NULL, then this is called to resolve page faults
	 * on the special mapping.  If used, .pages is not checked.
	 */
	// fault：表示缺页处理程序，当 pages 为 NULL 时，缺页处理程序将用于提供特殊映射的内容；
	vm_fault_t (*fault)(const struct vm_special_mapping *sm,
				struct vm_area_struct *vma,
				struct vm_fault *vmf);

	// mremap：表示 mremap() 的回调函数，如果在进行内存重映射时特殊映射被重新映射，则将调用此回调函数。
	int (*mremap)(const struct vm_special_mapping *sm,
		     struct vm_area_struct *new_vma);
};

enum tlb_flush_reason {
	// TLB_FLUSH_ON_TASK_SWITCH：在任务切换时刷新 TLB。
	TLB_FLUSH_ON_TASK_SWITCH,
	// TLB_REMOTE_SHOOTDOWN：在远程的 CPU 上执行 TLB shootdown 操作时刷新 TLB。
	TLB_REMOTE_SHOOTDOWN,
	// TLB_LOCAL_SHOOTDOWN：在本地 CPU 上执行 TLB shootdown 操作时刷新 TLB。
	TLB_LOCAL_SHOOTDOWN,
	// TLB_LOCAL_MM_SHOOTDOWN：在本地 CPU 上执行进程切换时刷新 TLB。
	TLB_LOCAL_MM_SHOOTDOWN,
	// TLB_REMOTE_SEND_IPI：向远程的 CPU 发送 IPI 时刷新 TLB。
	TLB_REMOTE_SEND_IPI,
	// NR_TLB_FLUSH_REASONS 用于记录枚举类型的数量。
	NR_TLB_FLUSH_REASONS,
};

 /*
  * A swap entry has to fit into a "unsigned long", as the entry is hidden
  * in the "index" field of the swapper address space.
  */
 /*
 swp_entry_t 用于表示交换空间中的一个页面。在一些内存紧张的情况下，内核会将一些长时间不活动的页面交换到磁盘上，
 以释放内存。交换后的页面就被标记为“被交换出去的页面”，它们的信息被存储在交换分区中，
 swp_entry_t 就是用来表示交换分区中的这些页面的。交换分区的大小和位置由系统管理员在系统配置时进行设置。
 */
typedef struct {
	unsigned long val;
} swp_entry_t;

#endif /* _LINUX_MM_TYPES_H */
