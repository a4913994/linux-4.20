#ifndef __LINUX_CPU_RMAP_H
#define __LINUX_CPU_RMAP_H

/*
 * cpu_rmap.c: CPU affinity reverse-map support
 * cpu_rmap.c：CPU亲和力反向映射支持
 * Copyright 2011 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <linux/cpumask.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/kref.h>

/**
 * struct cpu_rmap - CPU affinity reverse-map
 * @refcount: kref for object
 * @size: Number of objects to be reverse-mapped
 * @used: Number of objects added
 * @obj: Pointer to array of object pointers
 * @near: For each CPU, the index and distance to the nearest object,
 *      based on affinity masks
 * cpu_rmap结构体表示一个CPU的逆映射表，用于表示与CPU相关的对象之间的关系。逆映射表可以加速查找与给定CPU相邻的其他CPU或对象的过程
 */
struct cpu_rmap {
	// refcount：表示此逆映射表的引用计数。当引用计数变为0时，可以安全地释放这个结构体。
	struct kref	refcount;
	// size：表示逆映射表的大小。这是逆映射表可容纳的对象数量
	// used：表示逆映射表中当前已用的对象数量
	u16		size, used;
	// 表示一个指向对象指针数组的指针。这些对象通常是其他CPU或与CPU相关的设备
	void		**obj;
	// 表示一个包含有关相邻对象的数组。每个元素包含一个索引，表示obj数组中相邻对象的位置，以及一个距离值，表示给定CPU与相邻对象之间的距离。数组的大小由size决定。
	struct {
		u16	index;
		u16	dist;
	}		near[0];
};
#define CPU_RMAP_DIST_INF 0xffff

extern struct cpu_rmap *alloc_cpu_rmap(unsigned int size, gfp_t flags);
extern int cpu_rmap_put(struct cpu_rmap *rmap);

extern int cpu_rmap_add(struct cpu_rmap *rmap, void *obj);
extern int cpu_rmap_update(struct cpu_rmap *rmap, u16 index,
			   const struct cpumask *affinity);

static inline u16 cpu_rmap_lookup_index(struct cpu_rmap *rmap, unsigned int cpu)
{
	return rmap->near[cpu].index;
}

static inline void *cpu_rmap_lookup_obj(struct cpu_rmap *rmap, unsigned int cpu)
{
	return rmap->obj[rmap->near[cpu].index];
}

/**
 * alloc_irq_cpu_rmap - allocate CPU affinity reverse-map for IRQs
 * @size: Number of objects to be mapped
 *
 * Must be called in process context.
 */
static inline struct cpu_rmap *alloc_irq_cpu_rmap(unsigned int size)
{
	return alloc_cpu_rmap(size, GFP_KERNEL);
}
extern void free_irq_cpu_rmap(struct cpu_rmap *rmap);

extern int irq_cpu_rmap_add(struct cpu_rmap *rmap, int irq);

#endif /* __LINUX_CPU_RMAP_H */
