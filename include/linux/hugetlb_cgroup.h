/*
 * Copyright IBM Corporation, 2012
 * Author Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _LINUX_HUGETLB_CGROUP_H
#define _LINUX_HUGETLB_CGROUP_H

#include <linux/mmdebug.h>

struct hugetlb_cgroup;
/*
 * Minimum page order trackable by hugetlb cgroup.
 * At least 3 pages are necessary for all the tracking information.
 */
/*
HUGETLB_CGROUP_MIN_ORDER是hugetlb cgroup的最小order值。 hugetlb cgroup在创建时指定了一个最小的hugepage order，它限制了cgroup中可以使用的hugepage大小。
默认情况下，最小的hugepage order是2（即4KB），并且可以通过在挂载cgroup文件系统时指定hugepage大小的参数来更改此值。
例如，指定“hugepages-2048KB”将最小hugepage order设置为21（即2MB）。 HUGETLB_CGROUP_MIN_ORDER宏指定默认值为2，即4KB。
*/
#define HUGETLB_CGROUP_MIN_ORDER	2

#ifdef CONFIG_CGROUP_HUGETLB

/*
函数hugetlb_cgroup_from_page的作用是返回一个struct page所对应的hugetlb_cgroup结构体指针。
首先通过VM_BUG_ON_PAGE宏判断该page是否为huge page，如果不是则输出错误信息。接着判断该page的大小是否超过了HUGETLB_CGROUP_MIN_ORDER所表示的2的次幂页，如果小于则直接返回NULL。
如果大于等于HUGETLB_CGROUP_MIN_ORDER，则将page结构体中的第三项作为hugetlb_cgroup指针返回。需要注意的是，page结构体中第三项存放的是私有数据，一般用于指向相关的数据结构。
*/
static inline struct hugetlb_cgroup *hugetlb_cgroup_from_page(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHuge(page), page);

	if (compound_order(page) < HUGETLB_CGROUP_MIN_ORDER)
		return NULL;
	return (struct hugetlb_cgroup *)page[2].private;
}

static inline
int set_hugetlb_cgroup(struct page *page, struct hugetlb_cgroup *h_cg)
{
	VM_BUG_ON_PAGE(!PageHuge(page), page);

	if (compound_order(page) < HUGETLB_CGROUP_MIN_ORDER)
		return -1;
	page[2].private	= (unsigned long)h_cg;
	return 0;
}

static inline bool hugetlb_cgroup_disabled(void)
{
	return !cgroup_subsys_enabled(hugetlb_cgrp_subsys);
}
/*
hugetlb_cgroup_charge_cgroup 函数用于在 cgroup 中为 huge page 分配内存。
idx，表示要分配的 huge page 大小的索引；
nr_pages，表示要分配的页数；
ptr，表示返回的 hugetlb_cgroup 对象。
如果分配成功，则返回 0，否则返回一个负错误码。
*/
extern int hugetlb_cgroup_charge_cgroup(int idx, unsigned long nr_pages,
					struct hugetlb_cgroup **ptr);

/*
hugetlb_cgroup_commit_charge函数用于在hugetlb cgroup中扣除nr_pages个huge page。
int idx: huge page size的索引，指定要操作的huge page大小，参见HUGE_MAX_HSTATE宏定义。
unsigned long nr_pages: 扣除的huge page数量。
struct hugetlb_cgroup *h_cg: 要扣除huge page的hugetlb cgroup。
struct page *page: 扣除的huge page，该page应该属于给定的hugetlb cgroup。
*/
extern void hugetlb_cgroup_commit_charge(int idx, unsigned long nr_pages,
					 struct hugetlb_cgroup *h_cg,
					 struct page *page);
extern void hugetlb_cgroup_uncharge_page(int idx, unsigned long nr_pages,
					 struct page *page);
extern void hugetlb_cgroup_uncharge_cgroup(int idx, unsigned long nr_pages,
					   struct hugetlb_cgroup *h_cg);
extern void hugetlb_cgroup_file_init(void) __init;
extern void hugetlb_cgroup_migrate(struct page *oldhpage,
				   struct page *newhpage);

#else
static inline struct hugetlb_cgroup *hugetlb_cgroup_from_page(struct page *page)
{
	return NULL;
}

static inline
int set_hugetlb_cgroup(struct page *page, struct hugetlb_cgroup *h_cg)
{
	return 0;
}

static inline bool hugetlb_cgroup_disabled(void)
{
	return true;
}

static inline int
hugetlb_cgroup_charge_cgroup(int idx, unsigned long nr_pages,
			     struct hugetlb_cgroup **ptr)
{
	return 0;
}

static inline void
hugetlb_cgroup_commit_charge(int idx, unsigned long nr_pages,
			     struct hugetlb_cgroup *h_cg,
			     struct page *page)
{
}

static inline void
hugetlb_cgroup_uncharge_page(int idx, unsigned long nr_pages, struct page *page)
{
}

static inline void
hugetlb_cgroup_uncharge_cgroup(int idx, unsigned long nr_pages,
			       struct hugetlb_cgroup *h_cg)
{
}

static inline void hugetlb_cgroup_file_init(void)
{
}

static inline void hugetlb_cgroup_migrate(struct page *oldhpage,
					  struct page *newhpage)
{
}

#endif  /* CONFIG_MEM_RES_CTLR_HUGETLB */
#endif
