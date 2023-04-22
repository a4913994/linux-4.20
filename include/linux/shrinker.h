/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SHRINKER_H
#define _LINUX_SHRINKER_H

/*
 * This struct is used to pass information from page reclaim to the shrinkers.
 * We consolidate the values for easier extention later.
 *
 * The 'gfpmask' refers to the allocation we are currently trying to
 * fulfil.
 * 
 * 这个结构体用于从页面回收到收缩器中传递信息。我们将值合并在一起，以便以后更容易进行扩展。
 * “gfpmask”指的是我们当前正在尝试满足的分配。
 */
struct shrink_control {
	gfp_t gfp_mask;

	/* current node being shrunk (for NUMA aware shrinkers) */
	int nid;

	/*
	 * How many objects scan_objects should scan and try to reclaim.
	 * This is reset before every call, so it is safe for callees
	 * to modify.
	 */
	unsigned long nr_to_scan;

	/*
	 * How many objects did scan_objects process?
	 * This defaults to nr_to_scan before every call, but the callee
	 * should track its actual progress.
	 */
	unsigned long nr_scanned;

	/* current memcg being shrunk (for memcg aware shrinkers) */
	struct mem_cgroup *memcg;
};

#define SHRINK_STOP (~0UL)
#define SHRINK_EMPTY (~0UL - 1)
/*
 * A callback you can register to apply pressure to ageable caches.
 *
 * @count_objects should return the number of freeable items in the cache. If
 * there are no objects to free, it should return SHRINK_EMPTY, while 0 is
 * returned in cases of the number of freeable items cannot be determined
 * or shrinker should skip this cache for this time (e.g., their number
 * is below shrinkable limit). No deadlock checks should be done during the
 * count callback - the shrinker relies on aggregating scan counts that couldn't
 * be executed due to potential deadlocks to be run at a later call when the
 * deadlock condition is no longer pending.
 *
 * @scan_objects will only be called if @count_objects returned a non-zero
 * value for the number of freeable objects. The callout should scan the cache
 * and attempt to free items from the cache. It should then return the number
 * of objects freed during the scan, or SHRINK_STOP if progress cannot be made
 * due to potential deadlocks. If SHRINK_STOP is returned, then no further
 * attempts to call the @scan_objects will be made from the current reclaim
 * context.
 *
 * @flags determine the shrinker abilities, like numa awareness
 * 
 * 一个回调函数，用于向可老化缓存施加压力。
 * 
 * @count_objects 应返回缓存中可释放项目的数量。如果没有可释放的对象，则应返回SHRINK_EMPTY，
 * 如果无法确定可释放项目的数量或收缩程序应在此时跳过此缓存（例如，它们的数量低于可收缩限制），则应返回0。
 * 在计数回调中不应进行死锁检查——收缩程序依赖于聚合扫描计数，这些计数由于潜在的死锁而无法执行，在后续调用中运行时，死锁条件不再挂起。
 * 
 * @scan_objects 仅在@count_objects返回可释放对象数量的非零值时调用。调用应扫描缓存并尝试从缓存中释放项目。
 * 然后，它应返回扫描期间释放的对象数量，或者如果由于潜在的死锁无法取得进展，则返回SHRINK_STOP。
 * 如果返回SHRINK_STOP，则从当前回收上下文不会再尝试调用@scan_objects。
 * 
 * @flags 确定收缩程序的能力，例如NUMA感知性。
 */
// 用于内存回收
struct shrinker {
	// count_objects和scan_objects是函数指针，用于对一个对象计数和扫描操作
	unsigned long (*count_objects)(struct shrinker *,
				       struct shrink_control *sc);
	unsigned long (*scan_objects)(struct shrinker *,
				      struct shrink_control *sc);

	// batch表示一次回收的对象数，默认为0；
	long batch;	/* reclaim batch size, 0 = default */
	// seeks表示重建一个对象需要的操作数；
	int seeks;	/* seeks to recreate an obj */
	// flags表示标志位，具体含义可能与具体应用相关；
	unsigned flags;

	/* These are for internal use */
	// list是用于将多个shrinker结构体连接成链表的成员；
	struct list_head list;
#ifdef CONFIG_MEMCG_KMEM
	/* ID in shrinker_idr */
	// id用于标识一个shrinker结构体，用于内存控制组中的使用；
	int id;
#endif
	/* objs pending delete, per node */
	// nr_deferred是一个指向原子长整型的指针，用于记录每个节点中需要删除的对象数。
	atomic_long_t *nr_deferred;
};
#define DEFAULT_SEEKS 2 /* A good number if you don't know better. */

/* Flags */
#define SHRINKER_NUMA_AWARE	(1 << 0)
#define SHRINKER_MEMCG_AWARE	(1 << 1)

extern int prealloc_shrinker(struct shrinker *shrinker);
extern void register_shrinker_prepared(struct shrinker *shrinker);
extern int register_shrinker(struct shrinker *shrinker);
extern void unregister_shrinker(struct shrinker *shrinker);
extern void free_prealloced_shrinker(struct shrinker *shrinker);
#endif
