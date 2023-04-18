/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MMZONE_H
#define _LINUX_MMZONE_H

#ifndef __ASSEMBLY__
#ifndef __GENERATING_BOUNDS_H

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/cache.h>
#include <linux/threads.h>
#include <linux/numa.h>
#include <linux/init.h>
#include <linux/seqlock.h>
#include <linux/nodemask.h>
#include <linux/pageblock-flags.h>
#include <linux/page-flags-layout.h>
#include <linux/atomic.h>
#include <asm/page.h>

/* Free memory management - zoned buddy allocator.  
定义了内存管理子系统中使用的 buddy system 管理内存页的最大 order。
buddy system 是一种用于内存碎片管理的算法，通过将内存分割成大小为 2 的整数次幂的块来工作。
*/
// 如果没有定义 CONFIG_FORCE_MAX_ZONEORDER，则 MAX_ORDER 被设置为 11。这意味着最大的内存块将是 2^(11-1) 个连续的页面。
#ifndef CONFIG_FORCE_MAX_ZONEORDER
#define MAX_ORDER 11
#else
// 如果定义了 CONFIG_FORCE_MAX_ZONEORDER，则 MAX_ORDER 被设置为 CONFIG_FORCE_MAX_ZONEORDER 的值。
// 这允许系统管理员或内核构建配置强制指定一个不同的最大 order 值。
#define MAX_ORDER CONFIG_FORCE_MAX_ZONEORDER
#endif
// MAX_ORDER_NR_PAGES 被定义为 (1 << (MAX_ORDER - 1))，它表示最大 order 对应的连续页面数 
// 对于默认的 MAX_ORDER 值 11，MAX_ORDER_NR_PAGES 将是 2^(11-1) = 1024 个页面。
#define MAX_ORDER_NR_PAGES (1 << (MAX_ORDER - 1))

/*
 * PAGE_ALLOC_COSTLY_ORDER is the order at which allocations are deemed
 * costly to service.  That is between allocation orders which should
 * coalesce naturally under reasonable reclaim pressure and those which
 * will not.
 */
// 表示内存分配器在执行更昂贵的内存分配操作之前的阈值。具体来说，这个阈值用于确定何时需要执行内存回收操作来满足内存分配请求。
// #define PAGE_ALLOC_COSTLY_ORDER 3 表示当内存分配器收到一个 order 大于或等于 3 的分配请求时，内存回收操作可能被触发。
// 换句话说，当请求的连续内存页面数量大于或等于 2^(3-1) = 4 个页面时，内核可能会执行内存回收。

// 内存回收是一种用于在内存紧张情况下释放被占用的内存资源的机制。当系统内存紧张时，内核会尝试回收尽可能多的内存以满足内存分配请求。这包括回收缓存、交换出不活跃的页面等操作。当然，这些操作会增加内存分配的成本，因为它们涉及到更多的内存管理操作。
#define PAGE_ALLOC_COSTLY_ORDER 3

// 它表示 Linux 内核中不同类型的内存迁移策略。内存迁移类型用于区分系统中不同类型的内存页，以便于内核能够根据需求对内存进行分配和管理。
enum migratetype {
	// MIGRATE_UNMOVABLE：表示不可移动的内存页，例如内核数据结构、模块等，这些页面通常不会被释放。
	MIGRATE_UNMOVABLE,
	// MIGRATE_MOVABLE：表示可移动的内存页，例如用户空间进程的内存。当内存紧张时，这些页面可以被交换出或者移动到其他位置。
	MIGRATE_MOVABLE,
	// MIGRATE_RECLAIMABLE：表示可回收的内存页，例如文件系统缓存。当内存紧张时，这些页面可以被回收或者移动。
	MIGRATE_RECLAIMABLE,
	// MIGRATE_PCPTYPES：表示 per-CPU 页面的类型数量，它实际上是一个计数器，用于统计可用于 per-CPU 页面的迁移类型。
	MIGRATE_PCPTYPES,	/* the number of types on the pcp lists */
	// MIGRATE_HIGHATOMIC：表示高原子性操作的内存页，这些页面用于高优先级的内核操作，例如中断处理。
	MIGRATE_HIGHATOMIC = MIGRATE_PCPTYPES,
#ifdef CONFIG_CMA
	/*
	 * MIGRATE_CMA migration type is designed to mimic the way
	 * ZONE_MOVABLE works.  Only movable pages can be allocated
	 * from MIGRATE_CMA pageblocks and page allocator never
	 * implicitly change migration type of MIGRATE_CMA pageblock.
	 *
	 * The way to use it is to change migratetype of a range of
	 * pageblocks to MIGRATE_CMA which can be done by
	 * __free_pageblock_cma() function.  What is important though
	 * is that a range of pageblocks must be aligned to
	 * MAX_ORDER_NR_PAGES should biggest page be bigger then
	 * a single pageblock.
	 */
	// MIGRATE_CMA（仅在启用了 CONFIG_CMA 时）：表示 Contiguous Memory Allocator（连续内存分配器）的内存页，用于为需要连续物理内存的设备分配内存。
	MIGRATE_CMA,
#endif
#ifdef CONFIG_MEMORY_ISOLATION
	// MIGRATE_ISOLATE（仅在启用了 CONFIG_MEMORY_ISOLATION 时）：表示被隔离的内存页，这些页面无法用于内存分配。
	MIGRATE_ISOLATE,	/* can't allocate from here */
#endif
	// MIGRATE_TYPES 表示迁移类型的总数。这个值可用于数组大小的声明或其他需要知道迁移类型数量的情景。
	MIGRATE_TYPES
};

/* In mm/page_alloc.c; keep in sync also with show_migration_types() there */
// 该数组用于存储与前面提到的 migratetype 枚举类型相对应的名称。这使得内核可以方便地将 migratetype 的值转换为易于理解的字符串，以便于调试和日志记录。
extern char * const migratetype_names[MIGRATE_TYPES];

#ifdef CONFIG_CMA
// 检查给定的内存迁移类型或内存页是否属于 Contiguous Memory Allocator（CMA，连续内存分配器）类别。
// CMA 用于分配连续的物理内存，通常用于 DMA（Direct Memory Access，直接内存访问）设备或其他需要连续物理内存的硬件设备
#  define is_migrate_cma(migratetype) unlikely((migratetype) == MIGRATE_CMA)
#  define is_migrate_cma_page(_page) (get_pageblock_migratetype(_page) == MIGRATE_CMA)
#else
#  define is_migrate_cma(migratetype) false
#  define is_migrate_cma_page(_page) false
#endif

static inline bool is_migrate_movable(int mt)
{
	return is_migrate_cma(mt) || mt == MIGRATE_MOVABLE;
}

#define for_each_migratetype_order(order, type) \
	for (order = 0; order < MAX_ORDER; order++) \
		for (type = 0; type < MIGRATE_TYPES; type++)

extern int page_group_by_mobility_disabled;

#define NR_MIGRATETYPE_BITS (PB_migrate_end - PB_migrate + 1)
#define MIGRATETYPE_MASK ((1UL << NR_MIGRATETYPE_BITS) - 1)

#define get_pageblock_migratetype(page)					\
	get_pfnblock_flags_mask(page, page_to_pfn(page),		\
			PB_migrate_end, MIGRATETYPE_MASK)

// free_area 结构定义了一个用于表示内存空闲区域的数据结构。Linux 内核使用这个结构来跟踪和管理物理内存中空闲的连续页面块。
// 这个结构在 buddy system 内存分配器中使用，该分配器负责将连续的页面分组成 2 的整数次幂大小的块，以减少内存碎片和提高分配效率。
// 在内核中，每个内存域（memory zone）都会维护一个 free_area 结构数组，数组的大小等于 MAX_ORDER。每个数组元素表示具有相应 order 大小的空闲内存块。
// 例如，free_area[0] 表示大小为 1 个页面的空闲内存块，而 free_area[MAX_ORDER - 1] 表示大小为 MAX_ORDER_NR_PAGES 个页面的空闲内存块。
// 这样，内核可以快速找到满足特定大小和迁移类型要求的空闲页面。
struct free_area {
	// 每个链表头表示一个迁移类型（如 MIGRATE_UNMOVABLE、MIGRATE_MOVABLE 等），并维护着相应类型的空闲页面列表。
	struct list_head	free_list[MIGRATE_TYPES];
	// nr_free：一个无符号长整型，表示当前空闲区域中空闲页面的数量。
	unsigned long		nr_free;
};
// Numa 内存节点的数据结构
struct pglist_data;

/*
 * zone->lock and the zone lru_lock are two of the hottest locks in the kernel.
 * So add a wild amount of padding here to ensure that they fall into separate
 * cachelines.  There are very few zone structures in the machine, so space
 * consumption is not a concern here.
 * 只在多处理器（SMP，Symmetric Multi-Processing）系统中使用。
 * 它的目的是在多处理器系统中确保struct zone结构体的某些字段对齐于不同处理器节点之间的缓存行，以减少伪共享（False Sharing）现象。
 * 
 * 伪共享是指多个处理器访问同一缓存行中的不同数据时可能导致性能下降的现象。伪共享会导致处理器之间的缓存行失效，从而增加内存访问延迟。
 * 
 * ZONE_PADDING(name)宏用于在struct zone结构体中插入填充，以确保多处理器系统中的结构体字段对齐于不同处理器节点之间的缓存行。在非多处理器系统中，ZONE_PADDING(name)宏什么也不做。
 */
#if defined(CONFIG_SMP)
struct zone_padding {
	char x[0];
} ____cacheline_internodealigned_in_smp;
#define ZONE_PADDING(name)	struct zone_padding name;
#else
#define ZONE_PADDING(name)
#endif

#ifdef CONFIG_NUMA
/*
enum numa_stat_item定义了一组用于追踪NUMA（Non-Uniform Memory Access，非统一内存访问）相关统计信息的指标。
在NUMA系统中，内存访问时间取决于访问的内存位置相对于处理器的位置
*/
enum numa_stat_item {
	// NUMA_HIT：在期望的节点上分配内存。
	NUMA_HIT,		/* allocated in intended node */
	// NUMA_MISS：在非期望的节点上分配内存。
	NUMA_MISS,		/* allocated in non intended node */
	// NUMA_FOREIGN：本应在此节点上分配，但实际上在其他地方分配。
	NUMA_FOREIGN,		/* was intended here, hit elsewhere */
	// NUMA_INTERLEAVE_HIT：内存交织策略优先选择了这个内存区域。
	NUMA_INTERLEAVE_HIT,	/* interleaver preferred this zone */
	// NUMA_LOCAL：从本地节点分配内存。
	NUMA_LOCAL,		/* allocation from local node */
	// NUMA_OTHER：从其他节点分配内存。
	NUMA_OTHER,		/* allocation from other node */
	// NR_VM_NUMA_STAT_ITEMS表示这个枚举中的项目数量。这个值可以用于定义存储这些统计数据的数组。
	NR_VM_NUMA_STAT_ITEMS
};
#else
#define NR_VM_NUMA_STAT_ITEMS 0
#endif

// enum zone_stat_item定义了一组用于追踪内存区域（zone）相关统计信息的指标
enum zone_stat_item {
	/* First 128 byte cacheline (assuming 64 bit words) */
	// NR_FREE_PAGES：可用的空闲页面数量。
	NR_FREE_PAGES,
	// NR_ZONE_LRU_BASE：仅用于内存压缩和回收重试的基础值。
	NR_ZONE_LRU_BASE, /* Used only for compaction and reclaim retry */
	// NR_ZONE_INACTIVE_ANON：非活动匿名页面数量。
	NR_ZONE_INACTIVE_ANON = NR_ZONE_LRU_BASE,
	// NR_ZONE_ACTIVE_ANON：活动匿名页面数量。
	NR_ZONE_ACTIVE_ANON,
	// NR_ZONE_INACTIVE_FILE：非活动文件页面数量。
	NR_ZONE_INACTIVE_FILE,
	// NR_ZONE_ACTIVE_FILE：活动文件页面数量。
	NR_ZONE_ACTIVE_FILE,
	// NR_ZONE_UNEVICTABLE：不可回收页面数量。
	NR_ZONE_UNEVICTABLE,
	// NR_ZONE_WRITE_PENDING：脏页、回写页和不稳定页的数量。
	NR_ZONE_WRITE_PENDING,	/* Count of dirty, writeback and unstable pages */
	// NR_MLOCK：通过mlock()锁定并从LRU列表中移除的页面数量。
	NR_MLOCK,		/* mlock()ed pages found and moved off LRU */
	// NR_PAGETABLE：用于页表的页面数量。
	NR_PAGETABLE,		/* used for pagetables */
	// NR_KERNEL_STACK_KB：内核栈的大小，以KiB为单位。
	NR_KERNEL_STACK_KB,	/* measured in KiB */
	// NR_BOUNCE：用于I/O操作的bounce buffer页面数量。
	/* Second 128 byte cacheline */
	NR_BOUNCE,
#if IS_ENABLED(CONFIG_ZSMALLOC)
	// NR_ZSPAGES（如果启用了CONFIG_ZSMALLOC）：在zsmalloc中分配的页面数量。
	NR_ZSPAGES,		/* allocated in zsmalloc */
#endif
	// NR_FREE_CMA_PAGES：可用的连续内存分配（CMA）页面数量。
	NR_FREE_CMA_PAGES,
	// NR_VM_ZONE_STAT_ITEMS表示这个枚举中的项目数量。这个值可以用于定义存储这些统计数据的数组。
	NR_VM_ZONE_STAT_ITEMS };

// enum node_stat_item定义了一组用于追踪内存节点（node）相关统计信息的指标
enum node_stat_item {
	// NR_LRU_BASE：基本LRU指标。
	NR_LRU_BASE,
	// NR_INACTIVE_ANON：非活动匿名页面数量。
	NR_INACTIVE_ANON = NR_LRU_BASE, /* must match order of LRU_[IN]ACTIVE */
	// NR_ACTIVE_ANON：活动匿名页面数量。
	NR_ACTIVE_ANON,		/*  "     "     "   "       "         */
	// NR_INACTIVE_FILE：非活动文件页面数量。
	NR_INACTIVE_FILE,	/*  "     "     "   "       "         */
	// NR_ACTIVE_FILE：活动文件页面数量。
	NR_ACTIVE_FILE,		/*  "     "     "   "       "         */
	// NR_UNEVICTABLE：不可回收页面数量。
	NR_UNEVICTABLE,		/*  "     "     "   "       "         */
	// NR_SLAB_RECLAIMABLE：可回收的SLAB页面数量。
	NR_SLAB_RECLAIMABLE,
	// NR_SLAB_UNRECLAIMABLE：不可回收的SLAB页面数量。
	NR_SLAB_UNRECLAIMABLE,
	// NR_ISOLATED_ANON：暂时从匿名LRU列表中隔离的页面数量。
	NR_ISOLATED_ANON,	/* Temporary isolated pages from anon lru */
	// NR_ISOLATED_FILE：暂时从文件LRU列表中隔离的页面数量。
	NR_ISOLATED_FILE,	/* Temporary isolated pages from file lru */
	// WORKINGSET_NODES：工作集节点数量。
	WORKINGSET_NODES,
	// WORKINGSET_REFAULT：工作集重新发生错误的数量。
	WORKINGSET_REFAULT,
	// WORKINGSET_ACTIVATE：工作集激活次数。
	WORKINGSET_ACTIVATE,
	// WORKINGSET_RESTORE：工作集还原次数。
	WORKINGSET_RESTORE,
	// WORKINGSET_NODERECLAIM：工作集节点回收次数。
	WORKINGSET_NODERECLAIM,
	// NR_ANON_MAPPED：已映射的匿名页面数量。
	NR_ANON_MAPPED,	/* Mapped anonymous pages */
	// NR_FILE_MAPPED：已映射到页表的缓存页面数量（仅在进程上下文中修改）。
	NR_FILE_MAPPED,	/* pagecache pages mapped into pagetables.
			   only modified from process context */
	// NR_FILE_PAGES：文件页面数量。
	NR_FILE_PAGES,
	// NR_FILE_DIRTY：脏文件页面数量。
	NR_FILE_DIRTY,
	// NR_WRITEBACK：回写页面数量。
	NR_WRITEBACK,
	// NR_WRITEBACK_TEMP：使用临时缓冲区回写的页面数量。
	NR_WRITEBACK_TEMP,	/* Writeback using temporary buffers */
	// NR_SHMEM：共享内存页面数量（包括tmpfs/GEM页面）。
	NR_SHMEM,		/* shmem pages (included tmpfs/GEM pages) */
	// NR_SHMEM_THPS：共享内存的大页面（THP）数量。
	NR_SHMEM_THPS,
	// NR_SHMEM_PMDMAPPED：共享内存的PMD映射页面数量。
	NR_SHMEM_PMDMAPPED,
	// NR_ANON_THPS：匿名的大页面（THP）数量。
	NR_ANON_THPS,
	// NR_UNSTABLE_NFS：NFS不稳定页面数量。
	NR_UNSTABLE_NFS,	/* NFS unstable pages */
	// NR_VMSCAN_WRITE：虚拟内存扫描写入次数。
	NR_VMSCAN_WRITE,
	// NR_VMSCAN_IMMEDIATE：在回写结束时优先回收。
	NR_VMSCAN_IMMEDIATE,	/* Prioritise for reclaim when writeback ends */
	// NR_DIRTIED：自系统启动以来页面脏次数。
	NR_DIRTIED,		/* page dirtyings since bootup */
	// NR_WRITTEN：自系统启动以来页面写入次数。
	NR_WRITTEN,		/* page writings since bootup */
	// NR_KERNEL_MISC_RECLAIMABLE：可回收的非slab内核页面数量。
	NR_KERNEL_MISC_RECLAIMABLE,	/* reclaimable non-slab kernel pages */
	// NR_VM_NODE_STAT_ITEMS表示这个枚举中的项目数量
	NR_VM_NODE_STAT_ITEMS
};

/*
 * We do arithmetic on the LRU lists in various places in the code,
 * so it is important to keep the active lists LRU_ACTIVE higher in
 * the array than the corresponding inactive lists, and to keep
 * the *_FILE lists LRU_FILE higher than the corresponding _ANON lists.
 *
 * This has to be kept in sync with the statistics in zone_stat_item
 * above and the descriptions in vmstat_text in mm/vmstat.c
 */
#define LRU_BASE 0
#define LRU_ACTIVE 1
#define LRU_FILE 2

// lru_list定义了一个枚举类型，表示内存中的各种最近最少使用（Least Recently Used, LRU）页面列表
enum lru_list {
	// LRU_INACTIVE_ANON：非活动匿名页面列表。这些页面不经常使用，可能会被换出到交换区。
	LRU_INACTIVE_ANON = LRU_BASE,
	// LRU_ACTIVE_ANON：活动匿名页面列表。这些页面经常使用，可能会被换出到交换区。
	LRU_ACTIVE_ANON = LRU_BASE + LRU_ACTIVE,
	// LRU_INACTIVE_FILE：非活动文件页面列表。这些页面对应不经常使用的文件缓存，可能会被回收。
	LRU_INACTIVE_FILE = LRU_BASE + LRU_FILE,
	// LRU_ACTIVE_FILE：活动文件页面列表。这些页面对应经常使用的文件缓存，不太可能被回收。
	LRU_ACTIVE_FILE = LRU_BASE + LRU_FILE + LRU_ACTIVE,
	// LRU_UNEVICTABLE：不可回收页面列表。这些页面因为某种原因不能被换出或回收。
	LRU_UNEVICTABLE,
	// NR_LRU_LISTS表示这个枚举中的列表数量
	NR_LRU_LISTS
};

#define for_each_lru(lru) for (lru = 0; lru < NR_LRU_LISTS; lru++)

#define for_each_evictable_lru(lru) for (lru = 0; lru <= LRU_ACTIVE_FILE; lru++)

static inline int is_file_lru(enum lru_list lru)
{
	return (lru == LRU_INACTIVE_FILE || lru == LRU_ACTIVE_FILE);
}

static inline int is_active_lru(enum lru_list lru)
{
	return (lru == LRU_ACTIVE_ANON || lru == LRU_ACTIVE_FILE);
}

// struct zone_reclaim_stat 结构体用于跟踪内存区域（zone）回收统计信息
// 通过比较 recent_rotated 和 recent_scanned 的比例，可以评估缓存的价值。较高的旋转/扫描比率表示该缓存更有价值。
struct zone_reclaim_stat {
	/*
	 * The pageout code in vmscan.c keeps track of how many of the
	 * mem/swap backed and file backed pages are referenced.
	 * The higher the rotated/scanned ratio, the more valuable
	 * that cache is.
	 *
	 * The anon LRU stats live in [0], file LRU stats in [1]
	 */
	// recent_rotated：一个包含两个元素的数组，用于存储最近从LRU列表中旋转（重新激活）的页面数量。recent_rotated[0] 用于跟踪内存/交换空间支持的匿名页面，recent_rotated[1] 用于跟踪文件支持的页面。
	unsigned long		recent_rotated[2];
	// recent_scanned：一个包含两个元素的数组，用于存储最近从LRU列表中扫描的页面数量。recent_scanned[0] 用于跟踪内存/交换空间支持的匿名页面，recent_scanned[1] 用于跟踪文件支持的页面。
	unsigned long		recent_scanned[2];
};

// struct lruvec 结构体表示一组LRU（Least Recently Used，最近最少使用）列表，用于跟踪不同类型的内存页面。
// struct lruvec 结构体的主要目的是组织和管理内存页面，以便根据其最近的使用情况进行回收。
struct lruvec {
	// lists：一个包含 NR_LRU_LISTS 个元素的数组，每个元素都是一个 list_head 类型的结构体。数组中的每个元素表示一个不同类型的LRU列表（例如，活动/非活动匿名页面和活动/非活动文件页面）。
	struct list_head		lists[NR_LRU_LISTS];
	// reclaim_stat：一个 zone_reclaim_stat 类型的结构体，用于跟踪与该LRU向量相关的区域回收统计信息。
	struct zone_reclaim_stat	reclaim_stat;
	/* Evictions & activations on the inactive file list */
	// inactive_age：一个原子长整型变量，表示在非活动文件列表上进行的驱逐和激活操作的计数。
	atomic_long_t			inactive_age;
	/* Refaults at the time of last reclaim cycle */
	// refaults：一个无符号长整型变量，表示上一次回收周期时的重新故障（refault）次数。
	unsigned long			refaults;
#ifdef CONFIG_MEMCG
	// pglist_data *pgdat：当配置了内存控制组（CONFIG_MEMCG）时，这是一个指向 pglist_data 结构体的指针，表示与此LRU向量相关的内存节点。
	struct pglist_data *pgdat;
#endif
};

/* Mask used at gathering information at once (see memcontrol.c) */
#define LRU_ALL_FILE (BIT(LRU_INACTIVE_FILE) | BIT(LRU_ACTIVE_FILE))
#define LRU_ALL_ANON (BIT(LRU_INACTIVE_ANON) | BIT(LRU_ACTIVE_ANON))
#define LRU_ALL	     ((1 << NR_LRU_LISTS) - 1)

/* Isolate unmapped file */
#define ISOLATE_UNMAPPED	((__force isolate_mode_t)0x2)
/* Isolate for asynchronous migration */
#define ISOLATE_ASYNC_MIGRATE	((__force isolate_mode_t)0x4)
/* Isolate unevictable pages */
#define ISOLATE_UNEVICTABLE	((__force isolate_mode_t)0x8)

/* LRU Isolation modes. */
typedef unsigned __bitwise isolate_mode_t;

enum zone_watermarks {
	WMARK_MIN,
	WMARK_LOW,
	WMARK_HIGH,
	NR_WMARK
};

#define min_wmark_pages(z) (z->watermark[WMARK_MIN])
#define low_wmark_pages(z) (z->watermark[WMARK_LOW])
#define high_wmark_pages(z) (z->watermark[WMARK_HIGH])

// struct per_cpu_pages 结构体的主要目的是跟踪和管理每个CPU上的高速缓存页面，以便在需要时快速分配和回收内存
struct per_cpu_pages {
	// count：一个整型变量，表示列表中页面的数量。
	int count;		/* number of pages in the list */
	// high：一个整型变量，表示高水位线。当页面数量达到高水位线时，需要将页面归还到伙伴系统（buddy system）以腾出空间。
	int high;		/* high watermark, emptying needed */
	// batch：一个整型变量，表示伙伴系统（buddy system）添加/删除页面时的块大小。
	int batch;		/* chunk size for buddy add/remove */

	/* Lists of pages, one per migrate type stored on the pcp-lists */
	// lists：一个包含 MIGRATE_PCPTYPES 个元素的数组，每个元素都是一个 list_head 类型的结构体。数组中的每个元素表示一个不同迁移类型的页面列表。
	struct list_head lists[MIGRATE_PCPTYPES];
};

// struct per_cpu_pageset 结构体表示每个CPU上的页面集（pageset），用于跟踪每个CPU的高速缓存页面以及相关的统计信息。
// 这有助于提高内存分配性能，因为每个CPU可以独立管理其本地高速缓存页面，而无需在多个CPU之间进行同步
struct per_cpu_pageset {
	// pcp：一个 struct per_cpu_pages 类型的变量，表示每个CPU的高速缓存页面列表信息。
	struct per_cpu_pages pcp;
#ifdef CONFIG_NUMA
	// expire (只在 CONFIG_NUMA 定义时存在)：一个8位有符号整数，用于标识节点相关的到期时间。这通常用于NUMA（Non-Uniform Memory Access）架构中，以便在到期时调整节点之间的内存平衡。
	s8 expire;
	// vm_numa_stat_diff (只在 CONFIG_NUMA 定义时存在)：一个包含 NR_VM_NUMA_STAT_ITEMS 个元素的无符号16位整数数组，用于跟踪与NUMA相关的虚拟内存统计信息差异。
	u16 vm_numa_stat_diff[NR_VM_NUMA_STAT_ITEMS];
#endif
#ifdef CONFIG_SMP
	// stat_threshold (只在 CONFIG_SMP 定义时存在)：一个8位有符号整数，用于表示在更新全局统计信息之前需要累积的本地统计信息差异的阈值。这有助于减少在多处理器系统中更新全局统计信息时的同步开销。
	s8 stat_threshold;
	// vm_stat_diff (只在 CONFIG_SMP 定义时存在)：一个包含 NR_VM_ZONE_STAT_ITEMS 个元素的8位有符号整数数组，用于跟踪与虚拟内存区域（zone）相关的统计信息差异。
	s8 vm_stat_diff[NR_VM_ZONE_STAT_ITEMS];
#endif
};

struct per_cpu_nodestat {
	s8 stat_threshold;
	s8 vm_node_stat_diff[NR_VM_NODE_STAT_ITEMS];
};

#endif /* !__GENERATING_BOUNDS.H */

// zone_type 枚举定义了内核中可用的内存区域（zone）类型
enum zone_type {
#ifdef CONFIG_ZONE_DMA
	/*
	 * ZONE_DMA is used when there are devices that are not able
	 * to do DMA to all of addressable memory (ZONE_NORMAL). Then we
	 * carve out the portion of memory that is needed for these devices.
	 * The range is arch specific.
	 *
	 * Some examples
	 *
	 * Architecture		Limit
	 * ---------------------------
	 * parisc, ia64, sparc	<4G
	 * s390			<2G
	 * arm			Various
	 * alpha		Unlimited or 0-16MB.
	 *
	 * i386, x86_64 and multiple other arches
	 * 			<16M.
	 */
	// ZONE_DMA (仅在 CONFIG_ZONE_DMA 定义时存在): 用于那些无法对所有可寻址内存执行DMA的设备。
	// 这个区域包含了这些设备所需的内存部分。其范围是特定于架构的，例如，对于parisc、ia64、sparc架构，它的限制是小于4GB。
	ZONE_DMA,
#endif
#ifdef CONFIG_ZONE_DMA32
	/*
	 * x86_64 needs two ZONE_DMAs because it supports devices that are
	 * only able to do DMA to the lower 16M but also 32 bit devices that
	 * can only do DMA areas below 4G.
	 */
	// ZONE_DMA32 (仅在 CONFIG_ZONE_DMA32 定义时存在): x86_64架构需要两个ZONE_DMA，
	// 因为它支持仅能对低16M内存执行DMA的设备，也支持仅能对低4G内存执行DMA的32位设备。
	ZONE_DMA32,
#endif
	/*
	 * Normal addressable memory is in ZONE_NORMAL. DMA operations can be
	 * performed on pages in ZONE_NORMAL if the DMA devices support
	 * transfers to all addressable memory.
	 */
	// ZONE_NORMAL: 正常可寻址内存位于ZONE_NORMAL。如果DMA设备支持对所有可寻址内存进行传输，可以在ZONE_NORMAL中执行DMA操作。
	ZONE_NORMAL,
#ifdef CONFIG_HIGHMEM
	/*
	 * A memory area that is only addressable by the kernel through
	 * mapping portions into its own address space. This is for example
	 * used by i386 to allow the kernel to address the memory beyond
	 * 900MB. The kernel will set up special mappings (page
	 * table entries on i386) for each page that the kernel needs to
	 * access.
	 */
	// ZONE_HIGHMEM (仅在 CONFIG_HIGHMEM 定义时存在): 内核只能通过将部分区域映射到其自身地址空间来寻址的内存区域。
	// 例如，i386使用这个区域使内核能够寻址超过900MB的内存。对于每个内核需要访问的页面，内核会为其设置特殊映射（如i386上的页表条目）。
	ZONE_HIGHMEM,
#endif
	// ZONE_MOVABLE: 用于可移动页的内存区域。这些页可以在物理内存中迁移，以便在需要时收回和重新分配内存。
	ZONE_MOVABLE,
#ifdef CONFIG_ZONE_DEVICE
	// ZONE_DEVICE (仅在 CONFIG_ZONE_DEVICE 定义时存在): 用于设备专用内存的区域。这些区域包含了与特定硬件设备关联的内存，例如GPU或其他加速器设备。
	ZONE_DEVICE,
#endif
	__MAX_NR_ZONES

};

#ifndef __GENERATING_BOUNDS_H

// struct zone 结构体描述了一个内存区域，用于表示系统内存分布的不同区域（如 DMA、DMA32、Normal 和 HighMem）。
// 这个结构体包含了有关区域内存使用情况的详细信息，包括空闲内存块、各种内存阈值和统计信息
/*
水位标记（Watermarks）：水位标记用于确定区域内存的使用情况。有三个主要的水位标记：低水位（WMARK_LOW）、高水位（WMARK_HIGH）和最小水位（WMARK_MIN）。内存分配器根据当前内存使用量和这些水位标记来决定是否需要进行内存回收。

迁移类型（Migratetype）：迁移类型用于追踪页面在内存中的迁移行为。不同的迁移类型表示不同的页面分配策略，例如，MIGRATE_UNMOVABLE 表示不可移动的页面，MIGRATE_MOVABLE 表示可移动的页面，MIGRATE_RECLAIMABLE 表示可回收的页面等。

伙伴系统（Buddy System）：伙伴系统是 Linux 内核用于管理物理内存的一种算法。伙伴系统将物理内存分成大小为 2 的幂次方的块。每个 order 对应一种大小的块，例如 order 0 对应大小为 1 页的块，order 1 对应大小为 2 页的块，依此类推。free_area[MAX_ORDER]数组表示不同 order 的空闲内存块列表。

内存回收（Memory Reclaim）：内存回收是在内存压力较大时释放内存的过程。当内存使用量接近水位标记时，内核将触发内存回收过程，以回收不再使用的内存页。

内存紧凑（Memory Compaction）：内存紧凑是一种优化内存布局的过程，目的是将空闲内存页聚集在一起，从而提供足够大的连续内存块以满足大内存分配需求。紧凑性操作涉及到迁移页面，以便将相邻的空闲页面组合成更大的连续内存块。

内存热插拔（Memory Hotplug）：内存热插拔是指在系统运行过程中动态添加或删除物理内存。内存热插拔需要对 struct zone 结构体中的一些字段进行保护，以确保在内存热插拔操作过程中不会发生数据不一致。

NUMA（Non-Uniform Memory Access）：NUMA 是一种内存架构，用于解决多处理器系统中的内存访问延迟问题。在 NUMA 架构中，不同的处理器有自己的本地内存，而且可以访问其他处理器的远程内存。NUMA 系统中的内存访问延
*/
struct zone {
	/* Read-mostly fields */

	/* zone watermarks, access with *_wmark_pages(zone) macros */
	// watermark[NR_WMARK]：表示区域的水位标记，用于确定区域内存的使用情况。这些水位标记用于控制内存分配器的行为和内存回收策略。
	unsigned long watermark[NR_WMARK];

	// nr_reserved_highatomic：表示为高原子性操作保留的页面数。
	unsigned long nr_reserved_highatomic;

	/*
	 * We don't know if the memory that we're going to allocate will be
	 * freeable or/and it will be released eventually, so to avoid totally
	 * wasting several GB of ram we must reserve some of the lower zone
	 * memory (otherwise we risk to run OOM on the lower zones despite
	 * there being tons of freeable ram on the higher zones).  This array is
	 * recalculated at runtime if the sysctl_lowmem_reserve_ratio sysctl
	 * changes.
	 */
	// lowmem_reserve[MAX_NR_ZONES]：表示为低内存区域预留的内存。
	long lowmem_reserve[MAX_NR_ZONES];

#ifdef CONFIG_NUMA
	// node：表示所属的 NUMA 节点。
	int node;
#endif
	// zone_pgdat：指向与区域关联的 pglist_data 结构体。
	struct pglist_data	*zone_pgdat;
	// pageset：表示每个 CPU 的页面集。
	struct per_cpu_pageset __percpu *pageset;

#ifndef CONFIG_SPARSEMEM
	/*
	 * Flags for a pageblock_nr_pages block. See pageblock-flags.h.
	 * In SPARSEMEM, this map is stored in struct mem_section
	 */
	// pageblock_flags：表示用于跟踪页面块迁移类型的标志。
	unsigned long		*pageblock_flags;
#endif /* CONFIG_SPARSEMEM */

	/* zone_start_pfn == zone_start_paddr >> PAGE_SHIFT */
	// zone_start_pfn：表示区域开始的 PFN（页帧号）。
	unsigned long		zone_start_pfn;

	/*
	 * spanned_pages is the total pages spanned by the zone, including
	 * holes, which is calculated as:
	 * 	spanned_pages = zone_end_pfn - zone_start_pfn;
	 *
	 * present_pages is physical pages existing within the zone, which
	 * is calculated as:
	 *	present_pages = spanned_pages - absent_pages(pages in holes);
	 *
	 * managed_pages is present pages managed by the buddy system, which
	 * is calculated as (reserved_pages includes pages allocated by the
	 * bootmem allocator):
	 *	managed_pages = present_pages - reserved_pages;
	 *
	 * So present_pages may be used by memory hotplug or memory power
	 * management logic to figure out unmanaged pages by checking
	 * (present_pages - managed_pages). And managed_pages should be used
	 * by page allocator and vm scanner to calculate all kinds of watermarks
	 * and thresholds.
	 *
	 * Locking rules:
	 *
	 * zone_start_pfn and spanned_pages are protected by span_seqlock.
	 * It is a seqlock because it has to be read outside of zone->lock,
	 * and it is done in the main allocator path.  But, it is written
	 * quite infrequently.
	 *
	 * The span_seq lock is declared along with zone->lock because it is
	 * frequently read in proximity to zone->lock.  It's good to
	 * give them a chance of being in the same cacheline.
	 *
	 * Write access to present_pages at runtime should be protected by
	 * mem_hotplug_begin/end(). Any reader who can't tolerant drift of
	 * present_pages should get_online_mems() to get a stable value.
	 *
	 * Read access to managed_pages should be safe because it's unsigned
	 * long. Write access to zone->managed_pages and totalram_pages are
	 * protected by managed_page_count_lock at runtime. Idealy only
	 * adjust_managed_page_count() should be used instead of directly
	 * touching zone->managed_pages and totalram_pages.
	 */
	// managed_pages：表示由伙伴系统管理的页面数。
	unsigned long		managed_pages;
	// spanned_pages：表示区域跨越的页面总数，包括空洞。
	unsigned long		spanned_pages;
	// present_pages：表示区域内存在的物理页面数。
	unsigned long		present_pages;

	// name：表示区域的名称。
	const char		*name;

#ifdef CONFIG_MEMORY_ISOLATION
	/*
	 * Number of isolated pageblock. It is used to solve incorrect
	 * freepage counting problem due to racy retrieving migratetype
	 * of pageblock. Protected by zone->lock.
	 */
	// nr_isolate_pageblock：表示已隔离的页面块数。
	unsigned long		nr_isolate_pageblock;
#endif

#ifdef CONFIG_MEMORY_HOTPLUG
	/* see spanned/present_pages for more description */
	// span_seqlock：用于保护 zone_start_pfn 和 spanned_pages 的顺序锁。
	seqlock_t		span_seqlock;
#endif
	// initialized：表示区域是否已初始化。
	int initialized;

	/* Write-intensive fields used from the page allocator */
	ZONE_PADDING(_pad1_)
	// free_area[MAX_ORDER]：表示不同大小的空闲区域。
	/* free areas of different sizes */
	struct free_area	free_area[MAX_ORDER];

	/* zone flags, see below */
	// flags：表示区域的标志。
	unsigned long		flags;

	/* Primarily protects free_area */
	// lock：表示主要用于保护 free_area 的自旋锁。
	spinlock_t		lock;

	/* Write-intensive fields used by compaction and vmstats. */
	ZONE_PADDING(_pad2_)

	/*
	 * When free pages are below this point, additional steps are taken
	 * when reading the number of free pages to avoid per-cpu counter
	 * drift allowing watermarks to be breached
	 */
	// percpu_drift_mark：表示当空闲页面低于此值时，在读取空闲页面数量时需要采取额外措施，以避免每个 CPU 计数器漂移导致的水印泄露。
	unsigned long percpu_drift_mark;

#if defined CONFIG_COMPACTION || defined CONFIG_CMA
	/* pfn where compaction free scanner should start */
	// compact_cached_free_pfn 和 compact_cached_migrate_pfn：用于紧凑性扫描的起始 PFN。
	unsigned long		compact_cached_free_pfn;
	/* pfn where async and sync compaction migration scanner should start */
	unsigned long		compact_cached_migrate_pfn[2];
#endif

#ifdef CONFIG_COMPACTION
	/*
	 * On compaction failure, 1<<compact_defer_shift compactions
	 * are skipped before trying again. The number attempted since
	 * last failure is tracked with compact_considered.
	 */
	/*
	内存紧凑（Memory Compaction）：内存紧凑是一种优化内存布局的过程，目的是将空闲内存页聚集在一起，
	从而提供足够大的连续内存块以满足大内存分配需求。紧凑性操作涉及到迁移页面，以便将相邻的空闲页面组合成更大的连续内存块。
	*/
	// compact_considered 和 compact_defer_shift：用于紧凑性失败时的重试策略。
	unsigned int		compact_considered;
	unsigned int		compact_defer_shift;
	// compact_order_failed：表示上次紧凑性尝试失败的 order。
	int			compact_order_failed;
#endif

#if defined CONFIG_COMPACTION || defined CONFIG_CMA
	/* Set to true when the PG_migrate_skip bits should be cleared */
	// compact_blockskip_flush：表示是否应清除 PG_migrate_skip 位。
	bool			compact_blockskip_flush;
#endif
	// contiguous：表示内存是否连续。
	bool			contiguous;

	ZONE_PADDING(_pad3_)
	/* Zone statistics */
	// vm_stat[NR_VM_ZONE_STAT_ITEMS] 和 vm_numa_stat[NR_VM_NUMA_STAT_ITEMS]：表示区域统计信息。
	atomic_long_t		vm_stat[NR_VM_ZONE_STAT_ITEMS];
	atomic_long_t		vm_numa_stat[NR_VM_NUMA_STAT_ITEMS];
} ____cacheline_internodealigned_in_smp;

enum pgdat_flags {
	PGDAT_CONGESTED,		/* pgdat has many dirty pages backed by
					 * a congested BDI
					 */
	PGDAT_DIRTY,			/* reclaim scanning has recently found
					 * many dirty file pages at the tail
					 * of the LRU.
					 */
	PGDAT_WRITEBACK,		/* reclaim scanning has recently found
					 * many pages under writeback
					 */
	PGDAT_RECLAIM_LOCKED,		/* prevents concurrent reclaim */
};

static inline unsigned long zone_end_pfn(const struct zone *zone)
{
	return zone->zone_start_pfn + zone->spanned_pages;
}

static inline bool zone_spans_pfn(const struct zone *zone, unsigned long pfn)
{
	return zone->zone_start_pfn <= pfn && pfn < zone_end_pfn(zone);
}

static inline bool zone_is_initialized(struct zone *zone)
{
	return zone->initialized;
}

static inline bool zone_is_empty(struct zone *zone)
{
	return zone->spanned_pages == 0;
}

/*
 * Return true if [start_pfn, start_pfn + nr_pages) range has a non-empty
 * intersection with the given zone
 */
static inline bool zone_intersects(struct zone *zone,
		unsigned long start_pfn, unsigned long nr_pages)
{
	if (zone_is_empty(zone))
		return false;
	if (start_pfn >= zone_end_pfn(zone) ||
	    start_pfn + nr_pages <= zone->zone_start_pfn)
		return false;

	return true;
}

/*
 * The "priority" of VM scanning is how much of the queues we will scan in one
 * go. A value of 12 for DEF_PRIORITY implies that we will scan 1/4096th of the
 * queues ("queue_length >> 12") during an aging round.
 */
#define DEF_PRIORITY 12

/* Maximum number of zones on a zonelist */
#define MAX_ZONES_PER_ZONELIST (MAX_NUMNODES * MAX_NR_ZONES)

enum {
	ZONELIST_FALLBACK,	/* zonelist with fallback */
#ifdef CONFIG_NUMA
	/*
	 * The NUMA zonelists are doubled because we need zonelists that
	 * restrict the allocations to a single node for __GFP_THISNODE.
	 */
	ZONELIST_NOFALLBACK,	/* zonelist without fallback (__GFP_THISNODE) */
#endif
	MAX_ZONELISTS
};

/*
 * This struct contains information about a zone in a zonelist. It is stored
 * here to avoid dereferences into large structures and lookups of tables
 */
struct zoneref {
	struct zone *zone;	/* Pointer to actual zone */
	int zone_idx;		/* zone_idx(zoneref->zone) */
};

/*
 * One allocation request operates on a zonelist. A zonelist
 * is a list of zones, the first one is the 'goal' of the
 * allocation, the other zones are fallback zones, in decreasing
 * priority.
 *
 * To speed the reading of the zonelist, the zonerefs contain the zone index
 * of the entry being read. Helper functions to access information given
 * a struct zoneref are
 *
 * zonelist_zone()	- Return the struct zone * for an entry in _zonerefs
 * zonelist_zone_idx()	- Return the index of the zone for an entry
 * zonelist_node_idx()	- Return the index of the node for an entry
 */
struct zonelist {
	struct zoneref _zonerefs[MAX_ZONES_PER_ZONELIST + 1];
};

#ifndef CONFIG_DISCONTIGMEM
/* The array of struct pages - for discontigmem use pgdat->lmem_map */
extern struct page *mem_map;
#endif

/*
 * On NUMA machines, each NUMA node would have a pg_data_t to describe
 * it's memory layout. On UMA machines there is a single pglist_data which
 * describes the whole memory.
 *
 * Memory statistics and page replacement data structures are maintained on a
 * per-zone basis.
 */
struct bootmem_data;
/*
pglist_data 结构（通常简称为 pg_data_t）是 Linux 内核中用于表示内存节点（NUMA节点）的数据结构。
每个 NUMA 节点包含一组内存域（zones，如 DMA、Normal、HighMem 等），这些内存域被用于不同类型的内存分配。
pg_data_t 结构包含了关于节点中的内存域、可用内存和相关内核线程等信息。

在 UMA（Uniform Memory Access，统一内存访问）系统中，通常只有一个 pg_data_t 实例，因为整个系统只有一个内存节点
*/
typedef struct pglist_data {
	// node_zones[MAX_NR_ZONES]：一个数组，表示节点中的内存域（zones）。
	struct zone node_zones[MAX_NR_ZONES];
	// node_zonelists[MAX_ZONELISTS]：一个数组，表示节点中的内存域列表（zonelists），用于分配内存时按照优先级进行遍历。
	struct zonelist node_zonelists[MAX_ZONELISTS];
	// nr_zones：表示节点中内存域的数量。
	int nr_zones;
#ifdef CONFIG_FLAT_NODE_MEM_MAP	/* means !SPARSEMEM */
	// node_mem_map：指向节点的 struct page 数组，表示节点中所有物理页的元数据。
	struct page *node_mem_map;
#ifdef CONFIG_PAGE_EXTENSION
	// node_page_ext：指向节点的 struct page_ext 数组，表示节点中所有物理页的扩展元数据。
	struct page_ext *node_page_ext;
#endif
#endif
#if defined(CONFIG_MEMORY_HOTPLUG) || defined(CONFIG_DEFERRED_STRUCT_PAGE_INIT)
	/*
	 * Must be held any time you expect node_start_pfn, node_present_pages
	 * or node_spanned_pages stay constant.  Holding this will also
	 * guarantee that any pfn_valid() stays that way.
	 *
	 * pgdat_resize_lock() and pgdat_resize_unlock() are provided to
	 * manipulate node_size_lock without checking for CONFIG_MEMORY_HOTPLUG
	 * or CONFIG_DEFERRED_STRUCT_PAGE_INIT.
	 *
	 * Nests above zone->lock and zone->span_seqlock
	 */
	// node_size_lock：一个自旋锁，用于在内存热插拔和其他需要保持节点大小不变的操作期间保护节点的大小。
	spinlock_t node_size_lock;
#endif
	// node_start_pfn：表示节点的起始页帧号（Page Frame Number）。
	unsigned long node_start_pfn;
	// node_present_pages：表示节点中实际存在的物理页数量。
	unsigned long node_present_pages; /* total number of physical pages */
	// node_spanned_pages：表示节点中所有物理页范围的大小，包括空洞（未使用的内存区域）。
	unsigned long node_spanned_pages; /* total size of physical page
					     range, including holes */
	// node_id：表示节点的 ID。
	int node_id;
	// kswapd_wait 和 pfmemalloc_wait：等待队列，用于唤醒 kswapd 和其他内存回收线程。
	wait_queue_head_t kswapd_wait;
	wait_queue_head_t pfmemalloc_wait;
	// kswapd：指向负责节点的内存回收的内核线程的指针。
	struct task_struct *kswapd;	/* Protected by
					   mem_hotplug_begin/end() */
	// kswapd_order 和 kswapd_classzone_idx：用于指示 kswapd 期望回收的内存大小和类型。
	int kswapd_order;
	enum zone_type kswapd_classzone_idx;

	// kswapd_failures：表示 kswapd 运行期间 "回收数量为0" 的失败次数。
	int kswapd_failures;		/* Number of 'reclaimed == 0' runs */

#ifdef CONFIG_COMPACTION
	// kcompactd_max_order 和 kcompactd_classzone_idx：表示 kcompactd 期望回收的内存大小和类型。
	int kcompactd_max_order;
	enum zone_type kcompactd_classzone_idx;
	// kcompactd_wait：等待队列，用于唤醒 kcompactd 内存压缩线程。
	wait_queue_head_t kcompactd_wait;
	// kcompactd：指向负责节点的内存压缩的内核线程的指针。
	struct task_struct *kcompactd;
#endif
	/*
	 * This is a per-node reserve of pages that are not available
	 * to userspace allocations.
	 */
	// totalreserve_pages：表示节点中保留且不可用于用户空间分配的总页数。
	unsigned long		totalreserve_pages;

#ifdef CONFIG_NUMA
	/*
	 * zone reclaim becomes active if more unmapped pages exist.
	 */
	// min_unmapped_pages 和 min_slab_pages：表示启用区域回收（zone reclaim）所需的最小未映射和 slab 页面数。
	unsigned long		min_unmapped_pages;
	unsigned long		min_slab_pages;
#endif /* CONFIG_NUMA */

	/* Write-intensive fields used by page reclaim */
	ZONE_PADDING(_pad1_)
	// lru_lock：自旋锁，用于保护与节点相关的 LRU（Least Recently Used）列表。
	spinlock_t		lru_lock;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
	/*
	 * If memory initialisation on large machines is deferred then this
	 * is the first PFN that needs to be initialised.
	 */
	// first_deferred_pfn：在启用 CONFIG_DEFERRED_STRUCT_PAGE_INIT 时，表示需要初始化的第一个 PFN。
	unsigned long first_deferred_pfn;
	/* Number of non-deferred pages */
	// static_init_pgcnt：表示非延迟初始化的页数。
	unsigned long static_init_pgcnt;
#endif /* CONFIG_DEFERRED_STRUCT_PAGE_INIT */

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	// split_queue_lock：在启用 CONFIG_TRANSPARENT_HUGEPAGE 时，表示用于保护分裂队列的自旋锁。
	spinlock_t split_queue_lock;
	// split_queue：在启用 CONFIG_TRANSPARENT_HUGEPAGE 时，表示用于存储需要拆分的巨大页的链表。
	struct list_head split_queue;
	// split_queue_len：在启用 CONFIG_TRANSPARENT_HUGEPAGE 时，表示分裂队列的长度。
	unsigned long split_queue_len;
#endif

	/* Fields commonly accessed by the page reclaim scanner */
	// lruvec：表示与节点相关的 LRU 向量，它包含了不同迁移类型的 LRU 链表。
	struct lruvec		lruvec;
	// flags：表示与节点相关的标志。
	unsigned long		flags;

	ZONE_PADDING(_pad2_)

	/* Per-node vmstats */
	// per_cpu_nodestats：表示每个 CPU 的节点统计信息。
	struct per_cpu_nodestat __percpu *per_cpu_nodestats;
	// vm_stat[NR_VM_NODE_STAT_ITEMS]：表示与虚拟内存相关的节点统计信息。
	atomic_long_t		vm_stat[NR_VM_NODE_STAT_ITEMS];
} pg_data_t;

#define node_present_pages(nid)	(NODE_DATA(nid)->node_present_pages)
#define node_spanned_pages(nid)	(NODE_DATA(nid)->node_spanned_pages)
#ifdef CONFIG_FLAT_NODE_MEM_MAP
#define pgdat_page_nr(pgdat, pagenr)	((pgdat)->node_mem_map + (pagenr))
#else
#define pgdat_page_nr(pgdat, pagenr)	pfn_to_page((pgdat)->node_start_pfn + (pagenr))
#endif
#define nid_page_nr(nid, pagenr) 	pgdat_page_nr(NODE_DATA(nid),(pagenr))

#define node_start_pfn(nid)	(NODE_DATA(nid)->node_start_pfn)
#define node_end_pfn(nid) pgdat_end_pfn(NODE_DATA(nid))
static inline spinlock_t *zone_lru_lock(struct zone *zone)
{
	return &zone->zone_pgdat->lru_lock;
}

static inline struct lruvec *node_lruvec(struct pglist_data *pgdat)
{
	return &pgdat->lruvec;
}

static inline unsigned long pgdat_end_pfn(pg_data_t *pgdat)
{
	return pgdat->node_start_pfn + pgdat->node_spanned_pages;
}

static inline bool pgdat_is_empty(pg_data_t *pgdat)
{
	return !pgdat->node_start_pfn && !pgdat->node_spanned_pages;
}

#include <linux/memory_hotplug.h>

void build_all_zonelists(pg_data_t *pgdat);
void wakeup_kswapd(struct zone *zone, gfp_t gfp_mask, int order,
		   enum zone_type classzone_idx);
bool __zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
			 int classzone_idx, unsigned int alloc_flags,
			 long free_pages);
bool zone_watermark_ok(struct zone *z, unsigned int order,
		unsigned long mark, int classzone_idx,
		unsigned int alloc_flags);
bool zone_watermark_ok_safe(struct zone *z, unsigned int order,
		unsigned long mark, int classzone_idx);
enum memmap_context {
	MEMMAP_EARLY,
	MEMMAP_HOTPLUG,
};
extern void init_currently_empty_zone(struct zone *zone, unsigned long start_pfn,
				     unsigned long size);

extern void lruvec_init(struct lruvec *lruvec);

static inline struct pglist_data *lruvec_pgdat(struct lruvec *lruvec)
{
#ifdef CONFIG_MEMCG
	return lruvec->pgdat;
#else
	return container_of(lruvec, struct pglist_data, lruvec);
#endif
}

extern unsigned long lruvec_lru_size(struct lruvec *lruvec, enum lru_list lru, int zone_idx);

#ifdef CONFIG_HAVE_MEMORY_PRESENT
void memory_present(int nid, unsigned long start, unsigned long end);
#else
static inline void memory_present(int nid, unsigned long start, unsigned long end) {}
#endif

#if defined(CONFIG_SPARSEMEM)
void memblocks_present(void);
#else
static inline void memblocks_present(void) {}
#endif

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
int local_memory_node(int node_id);
#else
static inline int local_memory_node(int node_id) { return node_id; };
#endif

/*
 * zone_idx() returns 0 for the ZONE_DMA zone, 1 for the ZONE_NORMAL zone, etc.
 */
#define zone_idx(zone)		((zone) - (zone)->zone_pgdat->node_zones)

#ifdef CONFIG_ZONE_DEVICE
static inline bool is_dev_zone(const struct zone *zone)
{
	return zone_idx(zone) == ZONE_DEVICE;
}
#else
static inline bool is_dev_zone(const struct zone *zone)
{
	return false;
}
#endif

/*
 * Returns true if a zone has pages managed by the buddy allocator.
 * All the reclaim decisions have to use this function rather than
 * populated_zone(). If the whole zone is reserved then we can easily
 * end up with populated_zone() && !managed_zone().
 */
static inline bool managed_zone(struct zone *zone)
{
	return zone->managed_pages;
}

/* Returns true if a zone has memory */
static inline bool populated_zone(struct zone *zone)
{
	return zone->present_pages;
}

#ifdef CONFIG_NUMA
static inline int zone_to_nid(struct zone *zone)
{
	return zone->node;
}

static inline void zone_set_nid(struct zone *zone, int nid)
{
	zone->node = nid;
}
#else
static inline int zone_to_nid(struct zone *zone)
{
	return 0;
}

static inline void zone_set_nid(struct zone *zone, int nid) {}
#endif

extern int movable_zone;

#ifdef CONFIG_HIGHMEM
static inline int zone_movable_is_highmem(void)
{
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	return movable_zone == ZONE_HIGHMEM;
#else
	return (ZONE_MOVABLE - 1) == ZONE_HIGHMEM;
#endif
}
#endif

static inline int is_highmem_idx(enum zone_type idx)
{
#ifdef CONFIG_HIGHMEM
	return (idx == ZONE_HIGHMEM ||
		(idx == ZONE_MOVABLE && zone_movable_is_highmem()));
#else
	return 0;
#endif
}

/**
 * is_highmem - helper function to quickly check if a struct zone is a
 *              highmem zone or not.  This is an attempt to keep references
 *              to ZONE_{DMA/NORMAL/HIGHMEM/etc} in general code to a minimum.
 * @zone - pointer to struct zone variable
 */
static inline int is_highmem(struct zone *zone)
{
#ifdef CONFIG_HIGHMEM
	return is_highmem_idx(zone_idx(zone));
#else
	return 0;
#endif
}

/* These two functions are used to setup the per zone pages min values */
struct ctl_table;
int min_free_kbytes_sysctl_handler(struct ctl_table *, int,
					void __user *, size_t *, loff_t *);
int watermark_scale_factor_sysctl_handler(struct ctl_table *, int,
					void __user *, size_t *, loff_t *);
extern int sysctl_lowmem_reserve_ratio[MAX_NR_ZONES];
int lowmem_reserve_ratio_sysctl_handler(struct ctl_table *, int,
					void __user *, size_t *, loff_t *);
int percpu_pagelist_fraction_sysctl_handler(struct ctl_table *, int,
					void __user *, size_t *, loff_t *);
int sysctl_min_unmapped_ratio_sysctl_handler(struct ctl_table *, int,
			void __user *, size_t *, loff_t *);
int sysctl_min_slab_ratio_sysctl_handler(struct ctl_table *, int,
			void __user *, size_t *, loff_t *);

extern int numa_zonelist_order_handler(struct ctl_table *, int,
			void __user *, size_t *, loff_t *);
extern char numa_zonelist_order[];
#define NUMA_ZONELIST_ORDER_LEN	16

#ifndef CONFIG_NEED_MULTIPLE_NODES

extern struct pglist_data contig_page_data;
#define NODE_DATA(nid)		(&contig_page_data)
#define NODE_MEM_MAP(nid)	mem_map

#else /* CONFIG_NEED_MULTIPLE_NODES */

#include <asm/mmzone.h>

#endif /* !CONFIG_NEED_MULTIPLE_NODES */

extern struct pglist_data *first_online_pgdat(void);
extern struct pglist_data *next_online_pgdat(struct pglist_data *pgdat);
extern struct zone *next_zone(struct zone *zone);

/**
 * for_each_online_pgdat - helper macro to iterate over all online nodes
 * @pgdat - pointer to a pg_data_t variable
 */
#define for_each_online_pgdat(pgdat)			\
	for (pgdat = first_online_pgdat();		\
	     pgdat;					\
	     pgdat = next_online_pgdat(pgdat))
/**
 * for_each_zone - helper macro to iterate over all memory zones
 * @zone - pointer to struct zone variable
 *
 * The user only needs to declare the zone variable, for_each_zone
 * fills it in.
 */
#define for_each_zone(zone)			        \
	for (zone = (first_online_pgdat())->node_zones; \
	     zone;					\
	     zone = next_zone(zone))

#define for_each_populated_zone(zone)		        \
	for (zone = (first_online_pgdat())->node_zones; \
	     zone;					\
	     zone = next_zone(zone))			\
		if (!populated_zone(zone))		\
			; /* do nothing */		\
		else

static inline struct zone *zonelist_zone(struct zoneref *zoneref)
{
	return zoneref->zone;
}

static inline int zonelist_zone_idx(struct zoneref *zoneref)
{
	return zoneref->zone_idx;
}

static inline int zonelist_node_idx(struct zoneref *zoneref)
{
	return zone_to_nid(zoneref->zone);
}

struct zoneref *__next_zones_zonelist(struct zoneref *z,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes);

/**
 * next_zones_zonelist - Returns the next zone at or below highest_zoneidx within the allowed nodemask using a cursor within a zonelist as a starting point
 * @z - The cursor used as a starting point for the search
 * @highest_zoneidx - The zone index of the highest zone to return
 * @nodes - An optional nodemask to filter the zonelist with
 *
 * This function returns the next zone at or below a given zone index that is
 * within the allowed nodemask using a cursor as the starting point for the
 * search. The zoneref returned is a cursor that represents the current zone
 * being examined. It should be advanced by one before calling
 * next_zones_zonelist again.
 */
static __always_inline struct zoneref *next_zones_zonelist(struct zoneref *z,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes)
{
	if (likely(!nodes && zonelist_zone_idx(z) <= highest_zoneidx))
		return z;
	return __next_zones_zonelist(z, highest_zoneidx, nodes);
}

/**
 * first_zones_zonelist - Returns the first zone at or below highest_zoneidx within the allowed nodemask in a zonelist
 * @zonelist - The zonelist to search for a suitable zone
 * @highest_zoneidx - The zone index of the highest zone to return
 * @nodes - An optional nodemask to filter the zonelist with
 * @return - Zoneref pointer for the first suitable zone found (see below)
 *
 * This function returns the first zone at or below a given zone index that is
 * within the allowed nodemask. The zoneref returned is a cursor that can be
 * used to iterate the zonelist with next_zones_zonelist by advancing it by
 * one before calling.
 *
 * When no eligible zone is found, zoneref->zone is NULL (zoneref itself is
 * never NULL). This may happen either genuinely, or due to concurrent nodemask
 * update due to cpuset modification.
 */
static inline struct zoneref *first_zones_zonelist(struct zonelist *zonelist,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes)
{
	return next_zones_zonelist(zonelist->_zonerefs,
							highest_zoneidx, nodes);
}

/**
 * for_each_zone_zonelist_nodemask - helper macro to iterate over valid zones in a zonelist at or below a given zone index and within a nodemask
 * @zone - The current zone in the iterator
 * @z - The current pointer within zonelist->zones being iterated
 * @zlist - The zonelist being iterated
 * @highidx - The zone index of the highest zone to return
 * @nodemask - Nodemask allowed by the allocator
 *
 * This iterator iterates though all zones at or below a given zone index and
 * within a given nodemask
 */
#define for_each_zone_zonelist_nodemask(zone, z, zlist, highidx, nodemask) \
	for (z = first_zones_zonelist(zlist, highidx, nodemask), zone = zonelist_zone(z);	\
		zone;							\
		z = next_zones_zonelist(++z, highidx, nodemask),	\
			zone = zonelist_zone(z))

#define for_next_zone_zonelist_nodemask(zone, z, zlist, highidx, nodemask) \
	for (zone = z->zone;	\
		zone;							\
		z = next_zones_zonelist(++z, highidx, nodemask),	\
			zone = zonelist_zone(z))


/**
 * for_each_zone_zonelist - helper macro to iterate over valid zones in a zonelist at or below a given zone index
 * @zone - The current zone in the iterator
 * @z - The current pointer within zonelist->zones being iterated
 * @zlist - The zonelist being iterated
 * @highidx - The zone index of the highest zone to return
 *
 * This iterator iterates though all zones at or below a given zone index.
 */
#define for_each_zone_zonelist(zone, z, zlist, highidx) \
	for_each_zone_zonelist_nodemask(zone, z, zlist, highidx, NULL)

#ifdef CONFIG_SPARSEMEM
#include <asm/sparsemem.h>
#endif

#if !defined(CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID) && \
	!defined(CONFIG_HAVE_MEMBLOCK_NODE_MAP)
static inline unsigned long early_pfn_to_nid(unsigned long pfn)
{
	BUILD_BUG_ON(IS_ENABLED(CONFIG_NUMA));
	return 0;
}
#endif

#ifdef CONFIG_FLATMEM
#define pfn_to_nid(pfn)		(0)
#endif

#ifdef CONFIG_SPARSEMEM

/*
 * SECTION_SHIFT    		#bits space required to store a section #
 *
 * PA_SECTION_SHIFT		physical address to/from section number
 * PFN_SECTION_SHIFT		pfn to/from section number
 */
#define PA_SECTION_SHIFT	(SECTION_SIZE_BITS)
#define PFN_SECTION_SHIFT	(SECTION_SIZE_BITS - PAGE_SHIFT)

#define NR_MEM_SECTIONS		(1UL << SECTIONS_SHIFT)

#define PAGES_PER_SECTION       (1UL << PFN_SECTION_SHIFT)
#define PAGE_SECTION_MASK	(~(PAGES_PER_SECTION-1))

#define SECTION_BLOCKFLAGS_BITS \
	((1UL << (PFN_SECTION_SHIFT - pageblock_order)) * NR_PAGEBLOCK_BITS)

#if (MAX_ORDER - 1 + PAGE_SHIFT) > SECTION_SIZE_BITS
#error Allocator MAX_ORDER exceeds SECTION_SIZE
#endif

static inline unsigned long pfn_to_section_nr(unsigned long pfn)
{
	return pfn >> PFN_SECTION_SHIFT;
}
static inline unsigned long section_nr_to_pfn(unsigned long sec)
{
	return sec << PFN_SECTION_SHIFT;
}

#define SECTION_ALIGN_UP(pfn)	(((pfn) + PAGES_PER_SECTION - 1) & PAGE_SECTION_MASK)
#define SECTION_ALIGN_DOWN(pfn)	((pfn) & PAGE_SECTION_MASK)

struct page;
struct page_ext;
// mem_section 结构体定义了内存分段的信息。在内核中，物理内存被划分为大小相等的内存分段，以便于管理和分配。
// mem_section 结构体的大小必须是 2 的幂，以便于计算和使用 SECTION_ROOT_MASK。这有助于简化内存管理操作和提高性能。
struct mem_section {
	/*
	 * This is, logically, a pointer to an array of struct
	 * pages.  However, it is stored with some other magic.
	 * (see sparse.c::sparse_init_one_section())
	 *
	 * Additionally during early boot we encode node id of
	 * the location of the section here to guide allocation.
	 * (see sparse.c::memory_present())
	 *
	 * Making it a UL at least makes someone do a cast
	 * before using it wrong.
	 */
	// section_mem_map: 从逻辑上讲，这是一个指向 struct page 数组的指针。
	// 然而，在存储过程中会有一些其他魔法操作（详见 sparse.c::sparse_init_one_section()）。
	// 此外，在早期启动期间，我们在这里编码内存分段所在位置的节点ID，以指导内存分配（详见 sparse.c::memory_present()）。
	// 将其定义为 unsigned long 至少使得在错误使用前需要进行类型转换。
	unsigned long section_mem_map;

	/* See declaration of similar field in struct zone */
	// pageblock_flags: 一个指向无符号长整型数组的指针，用于存储与内存分段相关的 pageblock 的标志信息。这个字段的声明与 struct zone 中的类似字段相似。
	unsigned long *pageblock_flags;
#ifdef CONFIG_PAGE_EXTENSION
	/*
	 * If SPARSEMEM, pgdat doesn't have page_ext pointer. We use
	 * section. (see page_ext.h about this.)
	 */
	// page_ext: （仅在 CONFIG_PAGE_EXTENSION 定义时存在） 如果使用 SPARSEMEM，pgdat 没有指向 page_ext 的指针。在这种情况下，我们使用内存分段（有关此内容，请参阅 page_ext.h）。
	struct page_ext *page_ext;
	// pad: （仅在 CONFIG_PAGE_EXTENSION 定义时存在）一个用于填充的无符号长整型变量，以确保结构体的大小保持为 2 的幂。
	unsigned long pad;
#endif
	/*
	 * WARNING: mem_section must be a power-of-2 in size for the
	 * calculation and use of SECTION_ROOT_MASK to make sense.
	 */
};

#ifdef CONFIG_SPARSEMEM_EXTREME
#define SECTIONS_PER_ROOT       (PAGE_SIZE / sizeof (struct mem_section))
#else
#define SECTIONS_PER_ROOT	1
#endif

#define SECTION_NR_TO_ROOT(sec)	((sec) / SECTIONS_PER_ROOT)
#define NR_SECTION_ROOTS	DIV_ROUND_UP(NR_MEM_SECTIONS, SECTIONS_PER_ROOT)
#define SECTION_ROOT_MASK	(SECTIONS_PER_ROOT - 1)

#ifdef CONFIG_SPARSEMEM_EXTREME
extern struct mem_section **mem_section;
#else
extern struct mem_section mem_section[NR_SECTION_ROOTS][SECTIONS_PER_ROOT];
#endif

static inline struct mem_section *__nr_to_section(unsigned long nr)
{
#ifdef CONFIG_SPARSEMEM_EXTREME
	if (!mem_section)
		return NULL;
#endif
	if (!mem_section[SECTION_NR_TO_ROOT(nr)])
		return NULL;
	return &mem_section[SECTION_NR_TO_ROOT(nr)][nr & SECTION_ROOT_MASK];
}
extern int __section_nr(struct mem_section* ms);
extern unsigned long usemap_size(void);

/*
 * We use the lower bits of the mem_map pointer to store
 * a little bit of information.  The pointer is calculated
 * as mem_map - section_nr_to_pfn(pnum).  The result is
 * aligned to the minimum alignment of the two values:
 *   1. All mem_map arrays are page-aligned.
 *   2. section_nr_to_pfn() always clears PFN_SECTION_SHIFT
 *      lowest bits.  PFN_SECTION_SHIFT is arch-specific
 *      (equal SECTION_SIZE_BITS - PAGE_SHIFT), and the
 *      worst combination is powerpc with 256k pages,
 *      which results in PFN_SECTION_SHIFT equal 6.
 * To sum it up, at least 6 bits are available.
 */
#define	SECTION_MARKED_PRESENT	(1UL<<0)
#define SECTION_HAS_MEM_MAP	(1UL<<1)
#define SECTION_IS_ONLINE	(1UL<<2)
#define SECTION_MAP_LAST_BIT	(1UL<<3)
#define SECTION_MAP_MASK	(~(SECTION_MAP_LAST_BIT-1))
#define SECTION_NID_SHIFT	3

static inline struct page *__section_mem_map_addr(struct mem_section *section)
{
	unsigned long map = section->section_mem_map;
	map &= SECTION_MAP_MASK;
	return (struct page *)map;
}

static inline int present_section(struct mem_section *section)
{
	return (section && (section->section_mem_map & SECTION_MARKED_PRESENT));
}

static inline int present_section_nr(unsigned long nr)
{
	return present_section(__nr_to_section(nr));
}

static inline int valid_section(struct mem_section *section)
{
	return (section && (section->section_mem_map & SECTION_HAS_MEM_MAP));
}

static inline int valid_section_nr(unsigned long nr)
{
	return valid_section(__nr_to_section(nr));
}

static inline int online_section(struct mem_section *section)
{
	return (section && (section->section_mem_map & SECTION_IS_ONLINE));
}

static inline int online_section_nr(unsigned long nr)
{
	return online_section(__nr_to_section(nr));
}

#ifdef CONFIG_MEMORY_HOTPLUG
void online_mem_sections(unsigned long start_pfn, unsigned long end_pfn);
#ifdef CONFIG_MEMORY_HOTREMOVE
void offline_mem_sections(unsigned long start_pfn, unsigned long end_pfn);
#endif
#endif

static inline struct mem_section *__pfn_to_section(unsigned long pfn)
{
	return __nr_to_section(pfn_to_section_nr(pfn));
}

extern int __highest_present_section_nr;

#ifndef CONFIG_HAVE_ARCH_PFN_VALID
static inline int pfn_valid(unsigned long pfn)
{
	if (pfn_to_section_nr(pfn) >= NR_MEM_SECTIONS)
		return 0;
	return valid_section(__nr_to_section(pfn_to_section_nr(pfn)));
}
#endif

static inline int pfn_present(unsigned long pfn)
{
	if (pfn_to_section_nr(pfn) >= NR_MEM_SECTIONS)
		return 0;
	return present_section(__nr_to_section(pfn_to_section_nr(pfn)));
}

/*
 * These are _only_ used during initialisation, therefore they
 * can use __initdata ...  They could have names to indicate
 * this restriction.
 */
#ifdef CONFIG_NUMA
#define pfn_to_nid(pfn)							\
({									\
	unsigned long __pfn_to_nid_pfn = (pfn);				\
	page_to_nid(pfn_to_page(__pfn_to_nid_pfn));			\
})
#else
#define pfn_to_nid(pfn)		(0)
#endif

#define early_pfn_valid(pfn)	pfn_valid(pfn)
void sparse_init(void);
#else
#define sparse_init()	do {} while (0)
#define sparse_index_init(_sec, _nid)  do {} while (0)
#endif /* CONFIG_SPARSEMEM */

/*
 * During memory init memblocks map pfns to nids. The search is expensive and
 * this caches recent lookups. The implementation of __early_pfn_to_nid
 * may treat start/end as pfns or sections.
 */
// mminit_pfnnid_cache 结构体是一个简单的缓存结构，用于存储最近访问的物理帧号（PFN）范围及其对应的节点ID（nid）
// 通过使用这个缓存结构，如果查询的物理帧号在最近访问的范围内，就可以直接从缓存中获取其对应的节点ID，从而避免了重复的查找操作，提高了查找效率。
struct mminit_pfnnid_cache {
	// last_start: 一个无符号长整型变量，表示最近访问的物理帧号范围的起始值。
	unsigned long last_start;
	// last_end: 一个无符号长整型变量，表示最近访问的物理帧号范围的结束值。
	unsigned long last_end;
	// last_nid: 一个整型变量，表示最近访问的物理帧号范围所属的节点ID。
	int last_nid;
};

#ifndef early_pfn_valid
#define early_pfn_valid(pfn)	(1)
#endif

void memory_present(int nid, unsigned long start, unsigned long end);

/*
 * If it is possible to have holes within a MAX_ORDER_NR_PAGES, then we
 * need to check pfn validility within that MAX_ORDER_NR_PAGES block.
 * pfn_valid_within() should be used in this case; we optimise this away
 * when we have no holes within a MAX_ORDER_NR_PAGES block.
 */
#ifdef CONFIG_HOLES_IN_ZONE
#define pfn_valid_within(pfn) pfn_valid(pfn)
#else
#define pfn_valid_within(pfn) (1)
#endif

#ifdef CONFIG_ARCH_HAS_HOLES_MEMORYMODEL
/*
 * pfn_valid() is meant to be able to tell if a given PFN has valid memmap
 * associated with it or not. This means that a struct page exists for this
 * pfn. The caller cannot assume the page is fully initialized in general.
 * Hotplugable pages might not have been onlined yet. pfn_to_online_page()
 * will ensure the struct page is fully online and initialized. Special pages
 * (e.g. ZONE_DEVICE) are never onlined and should be treated accordingly.
 *
 * In FLATMEM, it is expected that holes always have valid memmap as long as
 * there is valid PFNs either side of the hole. In SPARSEMEM, it is assumed
 * that a valid section has a memmap for the entire section.
 *
 * However, an ARM, and maybe other embedded architectures in the future
 * free memmap backing holes to save memory on the assumption the memmap is
 * never used. The page_zone linkages are then broken even though pfn_valid()
 * returns true. A walker of the full memmap must then do this additional
 * check to ensure the memmap they are looking at is sane by making sure
 * the zone and PFN linkages are still valid. This is expensive, but walkers
 * of the full memmap are extremely rare.
 */
bool memmap_valid_within(unsigned long pfn,
					struct page *page, struct zone *zone);
#else
static inline bool memmap_valid_within(unsigned long pfn,
					struct page *page, struct zone *zone)
{
	return true;
}
#endif /* CONFIG_ARCH_HAS_HOLES_MEMORYMODEL */

#endif /* !__GENERATING_BOUNDS.H */
#endif /* !__ASSEMBLY__ */
#endif /* _LINUX_MMZONE_H */
