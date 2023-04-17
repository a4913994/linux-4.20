/*
 * include/linux/kmemleak.h
 *
 * Copyright (C) 2008 ARM Limited
 * Written by Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __KMEMLEAK_H
#define __KMEMLEAK_H

#include <linux/slab.h>
#include <linux/vmalloc.h>
/**
 * kmemleak是Linux内核中的一个工具，用于检测内核中的内存泄漏。它会在内核中对每一个分配的内存进行跟踪，如果在内存释放时发现存在泄漏，
 * 就会打印出相关信息，包括泄漏的地址和大小，以及分配的代码行号和堆栈跟踪信息等。
 * kmemleak的使用可以帮助开发者更快速地发现和修复内核中的内存泄漏问题，提高系统的稳定性和可靠性。
 */
#ifdef CONFIG_DEBUG_KMEMLEAK

extern void kmemleak_init(void) __init;
// kmemleak_alloc()是一个在内核中进行内存分配时自动检测和跟踪内存泄漏的工具函数。
// 它会在内核中用于分配内存时自动跟踪所分配的内存，并可以在后续的内存释放操作中检测这些内存是否被正确地释放。
// kmemleak_alloc()用于标记已分配的内存以进行跟踪，它需要传递指向所分配内存的指针、分配内存的大小以及其他参数。
extern void kmemleak_alloc(const void *ptr, size_t size, int min_count,
			   gfp_t gfp) __ref;
extern void kmemleak_alloc_percpu(const void __percpu *ptr, size_t size,
				  gfp_t gfp) __ref;
extern void kmemleak_vmalloc(const struct vm_struct *area, size_t size,
			     gfp_t gfp) __ref;
extern void kmemleak_free(const void *ptr) __ref;
extern void kmemleak_free_part(const void *ptr, size_t size) __ref;
extern void kmemleak_free_percpu(const void __percpu *ptr) __ref;
extern void kmemleak_update_trace(const void *ptr) __ref;
extern void kmemleak_not_leak(const void *ptr) __ref;
extern void kmemleak_ignore(const void *ptr) __ref;
extern void kmemleak_scan_area(const void *ptr, size_t size, gfp_t gfp) __ref;
extern void kmemleak_no_scan(const void *ptr) __ref;
extern void kmemleak_alloc_phys(phys_addr_t phys, size_t size, int min_count,
				gfp_t gfp) __ref;
extern void kmemleak_free_part_phys(phys_addr_t phys, size_t size) __ref;
extern void kmemleak_not_leak_phys(phys_addr_t phys) __ref;
extern void kmemleak_ignore_phys(phys_addr_t phys) __ref;

static inline void kmemleak_alloc_recursive(const void *ptr, size_t size,
					    int min_count, slab_flags_t flags,
					    gfp_t gfp)
{
	if (!(flags & SLAB_NOLEAKTRACE))
		kmemleak_alloc(ptr, size, min_count, gfp);
}

static inline void kmemleak_free_recursive(const void *ptr, slab_flags_t flags)
{
	if (!(flags & SLAB_NOLEAKTRACE))
		kmemleak_free(ptr);
}

static inline void kmemleak_erase(void **ptr)
{
	*ptr = NULL;
}

#else

static inline void kmemleak_init(void)
{
}
static inline void kmemleak_alloc(const void *ptr, size_t size, int min_count,
				  gfp_t gfp)
{
}
static inline void kmemleak_alloc_recursive(const void *ptr, size_t size,
					    int min_count, slab_flags_t flags,
					    gfp_t gfp)
{
}
static inline void kmemleak_alloc_percpu(const void __percpu *ptr, size_t size,
					 gfp_t gfp)
{
}
static inline void kmemleak_vmalloc(const struct vm_struct *area, size_t size,
				    gfp_t gfp)
{
}
static inline void kmemleak_free(const void *ptr)
{
}
static inline void kmemleak_free_part(const void *ptr, size_t size)
{
}
static inline void kmemleak_free_recursive(const void *ptr, slab_flags_t flags)
{
}
static inline void kmemleak_free_percpu(const void __percpu *ptr)
{
}
static inline void kmemleak_update_trace(const void *ptr)
{
}
static inline void kmemleak_not_leak(const void *ptr)
{
}
static inline void kmemleak_ignore(const void *ptr)
{
}
static inline void kmemleak_scan_area(const void *ptr, size_t size, gfp_t gfp)
{
}
static inline void kmemleak_erase(void **ptr)
{
}
static inline void kmemleak_no_scan(const void *ptr)
{
}
static inline void kmemleak_alloc_phys(phys_addr_t phys, size_t size,
				       int min_count, gfp_t gfp)
{
}
static inline void kmemleak_free_part_phys(phys_addr_t phys, size_t size)
{
}
static inline void kmemleak_not_leak_phys(phys_addr_t phys)
{
}
static inline void kmemleak_ignore_phys(phys_addr_t phys)
{
}

#endif	/* CONFIG_DEBUG_KMEMLEAK */

#endif	/* __KMEMLEAK_H */
