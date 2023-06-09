#ifndef _LINUX_MEMBLOCK_H
#define _LINUX_MEMBLOCK_H
#ifdef __KERNEL__

/*
 * Logical memory blocks.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <asm/dma.h>

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;

/*
 * highest page
 */
extern unsigned long max_pfn;
/*
 * highest possible page
 */
extern unsigned long long max_possible_pfn;
// INIT_MEMBLOCK_REGIONS定义了系统启动时预分配的内存块描述符数量，用于管理内存。在Linux内核中，内存块描述符用于描述物理内存，因此这个宏定义的值会影响到物理内存的最大支持数量。
#define INIT_MEMBLOCK_REGIONS	128
// INIT_PHYSMEM_REGIONS定义了系统启动时预分配的物理内存区域描述符数量，也用于管理内存。这个宏定义的值会影响到物理内存区域的最大支持数量。
#define INIT_PHYSMEM_REGIONS	4

/**
 * enum memblock_flags - definition of memory region attributes
 * @MEMBLOCK_NONE: no special request
 * @MEMBLOCK_HOTPLUG: hotpluggable region
 * @MEMBLOCK_MIRROR: mirrored region
 * @MEMBLOCK_NOMAP: don't add to kernel direct mapping
 */
enum memblock_flags {
	// MEMBLOCK_NONE：无特殊标志
	MEMBLOCK_NONE		= 0x0,	/* No special request */
	// MEMBLOCK_HOTPLUG：可热插拔的区域
	MEMBLOCK_HOTPLUG	= 0x1,	/* hotpluggable region */
	// MEMBLOCK_MIRROR：镜像区域
	MEMBLOCK_MIRROR		= 0x2,	/* mirrored region */
	// MEMBLOCK_NOMAP：不添加到内核直接映射中
	MEMBLOCK_NOMAP		= 0x4,	/* don't add to kernel direct mapping */
};

/**
 * struct memblock_region - represents a memory region
 * @base: physical address of the region
 * @size: size of the region
 * @flags: memory region attributes
 * @nid: NUMA node id
 */
// struct memblock_region 描述了一个物理内存区域, 
// 该结构体主要用于内核在初始化时记录可用的物理内存区域，以及在运行时进行内存热插拔操作时管理物理内存。
struct memblock_region {
	// base: 区域的起始物理地址。
	phys_addr_t base;
	// size: 区域的大小。
	phys_addr_t size;
	// flags: 标志该内存区域的特性，包括是否可热插拔、是否是镜像区域和是否需要添加到内核直接映射等。
	enum memblock_flags flags;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	// nid: NUMA 节点的编号，在 NUMA 架构中会用到。
	int nid;
#endif
};

/**
 * struct memblock_type - collection of memory regions of certain type
 * @cnt: number of regions
 * @max: size of the allocated array
 * @total_size: size of all regions
 * @regions: array of regions
 * @name: the memory type symbolic name
 */
// struct memblock_type 是用来描述内存块类型的结构体。每个内存块类型可以包含多个内存块区域(struct memblock_region)，每个内存块区域描述了一个连续的物理内存区域。结构体中包含以下字段：
// memblock 用于描述物理内存。系统启动时，memblock 会扫描整个物理地址空间，将物理内存按照内存块类型存储在 memblock 中，后续内核启动过程中各个模块会根据 memblock 中的信息来进行内存管理。
struct memblock_type {
	// cnt: 当前内存块类型中内存块区域的数量
	unsigned long cnt;
	// max: 内存块区域数组中可以存储的最大内存块区域数量
	unsigned long max;
	// total_size: 当前内存块类型中所有内存块区域的总大小
	phys_addr_t total_size;
	// regions: 内存块区域数组，存储了当前内存块类型的所有内存块区域的信息
	struct memblock_region *regions;
	// name: 内存块类型的名称
	char *name;
};

/**
 * struct memblock - memblock allocator metadata
 * @bottom_up: is bottom up direction?
 * @current_limit: physical address of the current allocation limit
 * @memory: usabe memory regions
 * @reserved: reserved memory regions
 * @physmem: all physical memory
 */
// struct memblock 是 Linux 内核中用于管理物理内存块的结构体
struct memblock {
	// bottom_up 变量指示分配的内存是否从底部向上进行
	bool bottom_up;  /* is bottom up direction? */
	// current_limit 变量跟踪分配的内存的当前位置
	phys_addr_t current_limit;
	// 存储内存、保留和物理内存映射的信息
	struct memblock_type memory;
	struct memblock_type reserved;
#ifdef CONFIG_HAVE_MEMBLOCK_PHYS_MAP
	struct memblock_type physmem;
#endif
};

extern struct memblock memblock;
extern int memblock_debug;

#ifdef CONFIG_ARCH_DISCARD_MEMBLOCK
#define __init_memblock __meminit
#define __initdata_memblock __meminitdata
void memblock_discard(void);
#else
#define __init_memblock
#define __initdata_memblock
#endif

#define memblock_dbg(fmt, ...) \
	if (memblock_debug) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

phys_addr_t memblock_find_in_range_node(phys_addr_t size, phys_addr_t align,
					phys_addr_t start, phys_addr_t end,
					int nid, enum memblock_flags flags);
phys_addr_t memblock_find_in_range(phys_addr_t start, phys_addr_t end,
				   phys_addr_t size, phys_addr_t align);
void memblock_allow_resize(void);
int memblock_add_node(phys_addr_t base, phys_addr_t size, int nid);
int memblock_add(phys_addr_t base, phys_addr_t size);
int memblock_remove(phys_addr_t base, phys_addr_t size);
int memblock_free(phys_addr_t base, phys_addr_t size);
int memblock_reserve(phys_addr_t base, phys_addr_t size);
void memblock_trim_memory(phys_addr_t align);
bool memblock_overlaps_region(struct memblock_type *type,
			      phys_addr_t base, phys_addr_t size);
int memblock_mark_hotplug(phys_addr_t base, phys_addr_t size);
int memblock_clear_hotplug(phys_addr_t base, phys_addr_t size);
int memblock_mark_mirror(phys_addr_t base, phys_addr_t size);
int memblock_mark_nomap(phys_addr_t base, phys_addr_t size);
int memblock_clear_nomap(phys_addr_t base, phys_addr_t size);
enum memblock_flags choose_memblock_flags(void);

unsigned long memblock_free_all(void);
void reset_node_managed_pages(pg_data_t *pgdat);
void reset_all_zones_managed_pages(void);

/* Low level functions */
int memblock_add_range(struct memblock_type *type,
		       phys_addr_t base, phys_addr_t size,
		       int nid, enum memblock_flags flags);

void __next_mem_range(u64 *idx, int nid, enum memblock_flags flags,
		      struct memblock_type *type_a,
		      struct memblock_type *type_b, phys_addr_t *out_start,
		      phys_addr_t *out_end, int *out_nid);

void __next_mem_range_rev(u64 *idx, int nid, enum memblock_flags flags,
			  struct memblock_type *type_a,
			  struct memblock_type *type_b, phys_addr_t *out_start,
			  phys_addr_t *out_end, int *out_nid);

void __next_reserved_mem_region(u64 *idx, phys_addr_t *out_start,
				phys_addr_t *out_end);

void __memblock_free_early(phys_addr_t base, phys_addr_t size);
void __memblock_free_late(phys_addr_t base, phys_addr_t size);

/**
 * for_each_mem_range - iterate through memblock areas from type_a and not
 * included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define for_each_mem_range(i, type_a, type_b, nid, flags,		\
			   p_start, p_end, p_nid)			\
	for (i = 0, __next_mem_range(&i, nid, flags, type_a, type_b,	\
				     p_start, p_end, p_nid);		\
	     i != (u64)ULLONG_MAX;					\
	     __next_mem_range(&i, nid, flags, type_a, type_b,		\
			      p_start, p_end, p_nid))

/**
 * for_each_mem_range_rev - reverse iterate through memblock areas from
 * type_a and not included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define for_each_mem_range_rev(i, type_a, type_b, nid, flags,		\
			       p_start, p_end, p_nid)			\
	for (i = (u64)ULLONG_MAX,					\
		     __next_mem_range_rev(&i, nid, flags, type_a, type_b,\
					  p_start, p_end, p_nid);	\
	     i != (u64)ULLONG_MAX;					\
	     __next_mem_range_rev(&i, nid, flags, type_a, type_b,	\
				  p_start, p_end, p_nid))

/**
 * for_each_reserved_mem_region - iterate over all reserved memblock areas
 * @i: u64 used as loop variable
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 *
 * Walks over reserved areas of memblock. Available as soon as memblock
 * is initialized.
 */
#define for_each_reserved_mem_region(i, p_start, p_end)			\
	for (i = 0UL, __next_reserved_mem_region(&i, p_start, p_end);	\
	     i != (u64)ULLONG_MAX;					\
	     __next_reserved_mem_region(&i, p_start, p_end))

static inline bool memblock_is_hotpluggable(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_HOTPLUG;
}

static inline bool memblock_is_mirror(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_MIRROR;
}

static inline bool memblock_is_nomap(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_NOMAP;
}

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
int memblock_search_pfn_nid(unsigned long pfn, unsigned long *start_pfn,
			    unsigned long  *end_pfn);
void __next_mem_pfn_range(int *idx, int nid, unsigned long *out_start_pfn,
			  unsigned long *out_end_pfn, int *out_nid);

/**
 * for_each_mem_pfn_range - early memory pfn range iterator
 * @i: an integer used as loop variable
 * @nid: node selector, %MAX_NUMNODES for all nodes
 * @p_start: ptr to ulong for start pfn of the range, can be %NULL
 * @p_end: ptr to ulong for end pfn of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over configured memory ranges.
 */
#define for_each_mem_pfn_range(i, nid, p_start, p_end, p_nid)		\
	for (i = -1, __next_mem_pfn_range(&i, nid, p_start, p_end, p_nid); \
	     i >= 0; __next_mem_pfn_range(&i, nid, p_start, p_end, p_nid))
#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */

/**
 * for_each_free_mem_range - iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock.  Available as
 * soon as memblock is initialized.
 */
#define for_each_free_mem_range(i, nid, flags, p_start, p_end, p_nid)	\
	for_each_mem_range(i, &memblock.memory, &memblock.reserved,	\
			   nid, flags, p_start, p_end, p_nid)

/**
 * for_each_free_mem_range_reverse - rev-iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock in reverse
 * order.  Available as soon as memblock is initialized.
 */
#define for_each_free_mem_range_reverse(i, nid, flags, p_start, p_end,	\
					p_nid)				\
	for_each_mem_range_rev(i, &memblock.memory, &memblock.reserved,	\
			       nid, flags, p_start, p_end, p_nid)

static inline void memblock_set_region_flags(struct memblock_region *r,
					     enum memblock_flags flags)
{
	r->flags |= flags;
}

static inline void memblock_clear_region_flags(struct memblock_region *r,
					       enum memblock_flags flags)
{
	r->flags &= ~flags;
}

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
int memblock_set_node(phys_addr_t base, phys_addr_t size,
		      struct memblock_type *type, int nid);

static inline void memblock_set_region_node(struct memblock_region *r, int nid)
{
	r->nid = nid;
}

static inline int memblock_get_region_node(const struct memblock_region *r)
{
	return r->nid;
}
#else
static inline void memblock_set_region_node(struct memblock_region *r, int nid)
{
}

static inline int memblock_get_region_node(const struct memblock_region *r)
{
	return 0;
}
#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */

/* Flags for memblock allocation APIs */
#define MEMBLOCK_ALLOC_ANYWHERE	(~(phys_addr_t)0)
#define MEMBLOCK_ALLOC_ACCESSIBLE	0

/* We are using top down, so it is safe to use 0 here */
#define MEMBLOCK_LOW_LIMIT 0

#ifndef ARCH_LOW_ADDRESS_LIMIT
#define ARCH_LOW_ADDRESS_LIMIT  0xffffffffUL
#endif

phys_addr_t memblock_phys_alloc_nid(phys_addr_t size, phys_addr_t align, int nid);
phys_addr_t memblock_phys_alloc_try_nid(phys_addr_t size, phys_addr_t align, int nid);

phys_addr_t memblock_phys_alloc(phys_addr_t size, phys_addr_t align);

void *memblock_alloc_try_nid_raw(phys_addr_t size, phys_addr_t align,
				 phys_addr_t min_addr, phys_addr_t max_addr,
				 int nid);
void *memblock_alloc_try_nid_nopanic(phys_addr_t size, phys_addr_t align,
				     phys_addr_t min_addr, phys_addr_t max_addr,
				     int nid);
void *memblock_alloc_try_nid(phys_addr_t size, phys_addr_t align,
			     phys_addr_t min_addr, phys_addr_t max_addr,
			     int nid);

static inline void * __init memblock_alloc(phys_addr_t size,  phys_addr_t align)
{
	return memblock_alloc_try_nid(size, align, MEMBLOCK_LOW_LIMIT,
				      MEMBLOCK_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_raw(phys_addr_t size,
					       phys_addr_t align)
{
	return memblock_alloc_try_nid_raw(size, align, MEMBLOCK_LOW_LIMIT,
					  MEMBLOCK_ALLOC_ACCESSIBLE,
					  NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_from(phys_addr_t size,
						phys_addr_t align,
						phys_addr_t min_addr)
{
	return memblock_alloc_try_nid(size, align, min_addr,
				      MEMBLOCK_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_nopanic(phys_addr_t size,
						   phys_addr_t align)
{
	return memblock_alloc_try_nid_nopanic(size, align, MEMBLOCK_LOW_LIMIT,
					      MEMBLOCK_ALLOC_ACCESSIBLE,
					      NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_low(phys_addr_t size,
					       phys_addr_t align)
{
	return memblock_alloc_try_nid(size, align, MEMBLOCK_LOW_LIMIT,
				      ARCH_LOW_ADDRESS_LIMIT, NUMA_NO_NODE);
}
static inline void * __init memblock_alloc_low_nopanic(phys_addr_t size,
						       phys_addr_t align)
{
	return memblock_alloc_try_nid_nopanic(size, align, MEMBLOCK_LOW_LIMIT,
					      ARCH_LOW_ADDRESS_LIMIT,
					      NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_from_nopanic(phys_addr_t size,
							phys_addr_t align,
							phys_addr_t min_addr)
{
	return memblock_alloc_try_nid_nopanic(size, align, min_addr,
					      MEMBLOCK_ALLOC_ACCESSIBLE,
					      NUMA_NO_NODE);
}

static inline void * __init memblock_alloc_node(phys_addr_t size,
						phys_addr_t align, int nid)
{
	return memblock_alloc_try_nid(size, align, MEMBLOCK_LOW_LIMIT,
				      MEMBLOCK_ALLOC_ACCESSIBLE, nid);
}

static inline void * __init memblock_alloc_node_nopanic(phys_addr_t size,
							int nid)
{
	return memblock_alloc_try_nid_nopanic(size, SMP_CACHE_BYTES,
					      MEMBLOCK_LOW_LIMIT,
					      MEMBLOCK_ALLOC_ACCESSIBLE, nid);
}

static inline void __init memblock_free_early(phys_addr_t base,
					      phys_addr_t size)
{
	__memblock_free_early(base, size);
}

static inline void __init memblock_free_early_nid(phys_addr_t base,
						  phys_addr_t size, int nid)
{
	__memblock_free_early(base, size);
}

static inline void __init memblock_free_late(phys_addr_t base, phys_addr_t size)
{
	__memblock_free_late(base, size);
}

/*
 * Set the allocation direction to bottom-up or top-down.
 */
static inline void __init memblock_set_bottom_up(bool enable)
{
	memblock.bottom_up = enable;
}

/*
 * Check if the allocation direction is bottom-up or not.
 * if this is true, that said, memblock will allocate memory
 * in bottom-up direction.
 */
static inline bool memblock_bottom_up(void)
{
	return memblock.bottom_up;
}

phys_addr_t __init memblock_alloc_range(phys_addr_t size, phys_addr_t align,
					phys_addr_t start, phys_addr_t end,
					enum memblock_flags flags);
phys_addr_t memblock_alloc_base_nid(phys_addr_t size,
					phys_addr_t align, phys_addr_t max_addr,
					int nid, enum memblock_flags flags);
phys_addr_t memblock_alloc_base(phys_addr_t size, phys_addr_t align,
				phys_addr_t max_addr);
phys_addr_t __memblock_alloc_base(phys_addr_t size, phys_addr_t align,
				  phys_addr_t max_addr);
phys_addr_t memblock_phys_mem_size(void);
phys_addr_t memblock_reserved_size(void);
phys_addr_t memblock_mem_size(unsigned long limit_pfn);
phys_addr_t memblock_start_of_DRAM(void);
phys_addr_t memblock_end_of_DRAM(void);
void memblock_enforce_memory_limit(phys_addr_t memory_limit);
void memblock_cap_memory_range(phys_addr_t base, phys_addr_t size);
void memblock_mem_limit_remove_map(phys_addr_t limit);
bool memblock_is_memory(phys_addr_t addr);
bool memblock_is_map_memory(phys_addr_t addr);
bool memblock_is_region_memory(phys_addr_t base, phys_addr_t size);
bool memblock_is_reserved(phys_addr_t addr);
bool memblock_is_region_reserved(phys_addr_t base, phys_addr_t size);

extern void __memblock_dump_all(void);

static inline void memblock_dump_all(void)
{
	if (memblock_debug)
		__memblock_dump_all();
}

/**
 * memblock_set_current_limit - Set the current allocation limit to allow
 *                         limiting allocations to what is currently
 *                         accessible during boot
 * @limit: New limit value (physical address)
 */
void memblock_set_current_limit(phys_addr_t limit);


phys_addr_t memblock_get_current_limit(void);

/*
 * pfn conversion functions
 *
 * While the memory MEMBLOCKs should always be page aligned, the reserved
 * MEMBLOCKs may not be. This accessor attempt to provide a very clear
 * idea of what they return for such non aligned MEMBLOCKs.
 */

/**
 * memblock_region_memory_base_pfn - get the lowest pfn of the memory region
 * @reg: memblock_region structure
 *
 * Return: the lowest pfn intersecting with the memory region
 */
static inline unsigned long memblock_region_memory_base_pfn(const struct memblock_region *reg)
{
	return PFN_UP(reg->base);
}

/**
 * memblock_region_memory_end_pfn - get the end pfn of the memory region
 * @reg: memblock_region structure
 *
 * Return: the end_pfn of the reserved region
 */
static inline unsigned long memblock_region_memory_end_pfn(const struct memblock_region *reg)
{
	return PFN_DOWN(reg->base + reg->size);
}

/**
 * memblock_region_reserved_base_pfn - get the lowest pfn of the reserved region
 * @reg: memblock_region structure
 *
 * Return: the lowest pfn intersecting with the reserved region
 */
static inline unsigned long memblock_region_reserved_base_pfn(const struct memblock_region *reg)
{
	return PFN_DOWN(reg->base);
}

/**
 * memblock_region_reserved_end_pfn - get the end pfn of the reserved region
 * @reg: memblock_region structure
 *
 * Return: the end_pfn of the reserved region
 */
static inline unsigned long memblock_region_reserved_end_pfn(const struct memblock_region *reg)
{
	return PFN_UP(reg->base + reg->size);
}

#define for_each_memblock(memblock_type, region)					\
	for (region = memblock.memblock_type.regions;					\
	     region < (memblock.memblock_type.regions + memblock.memblock_type.cnt);	\
	     region++)

#define for_each_memblock_type(i, memblock_type, rgn)			\
	for (i = 0, rgn = &memblock_type->regions[0];			\
	     i < memblock_type->cnt;					\
	     i++, rgn = &memblock_type->regions[i])

extern void *alloc_large_system_hash(const char *tablename,
				     unsigned long bucketsize,
				     unsigned long numentries,
				     int scale,
				     int flags,
				     unsigned int *_hash_shift,
				     unsigned int *_hash_mask,
				     unsigned long low_limit,
				     unsigned long high_limit);

#define HASH_EARLY	0x00000001	/* Allocating during early boot? */
#define HASH_SMALL	0x00000002	/* sub-page allocation allowed, min
					 * shift passed via *_hash_shift */
#define HASH_ZERO	0x00000004	/* Zero allocated hash table */

/* Only NUMA needs hash distribution. 64bit NUMA architectures have
 * sufficient vmalloc space.
 */
#ifdef CONFIG_NUMA
#define HASHDIST_DEFAULT IS_ENABLED(CONFIG_64BIT)
extern int hashdist;		/* Distribute hashes across NUMA nodes? */
#else
#define hashdist (0)
#endif

#ifdef CONFIG_MEMTEST
extern void early_memtest(phys_addr_t start, phys_addr_t end);
#else
static inline void early_memtest(phys_addr_t start, phys_addr_t end)
{
}
#endif

#endif /* __KERNEL__ */

#endif /* _LINUX_MEMBLOCK_H */
