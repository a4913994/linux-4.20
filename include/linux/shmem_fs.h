/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SHMEM_FS_H
#define __SHMEM_FS_H

#include <linux/file.h>
#include <linux/swap.h>
#include <linux/mempolicy.h>
#include <linux/pagemap.h>
#include <linux/percpu_counter.h>
#include <linux/xattr.h>

/* inode in-kernel data */

// shmem_inode_info 结构体表示一个共享内存（shmem）文件系统的 inode 信息
struct shmem_inode_info {
	// lock: 一个自旋锁，用于保护该结构体的并发访问。
	spinlock_t		lock;
	// seals: shmem 密封标志，用于限制对共享内存对象的操作，例如限制扩展或缩小大小。
	unsigned int		seals;		/* shmem seals */
	// flags: inode 的一些特殊标志，例如标记文件是否已被删除。
	unsigned long		flags;
	// alloced: 分配给文件的数据页数。
	unsigned long		alloced;	/* data pages alloced to file */
	// swapped: 分配给交换空间（swap）的数据页数。
	unsigned long		swapped;	/* subtotal assigned to swap */
	// shrinklist: 可收缩的巨页（huge page）inode 列表。
	struct list_head        shrinklist;     /* shrinkable hpage inodes */
	// swaplist: 可能在交换空间上的链表。
	struct list_head	swaplist;	/* chain of maybes on swap */
	// policy: NUMA 内存分配策略。
	struct shared_policy	policy;		/* NUMA memory alloc policy */
	// xattrs: inode 的扩展属性列表。
	struct simple_xattrs	xattrs;		/* list of xattrs */
	// vfs_inode: 嵌入的 VFS inode 结构体，用于与通用文件系统（VFS）层进行交互。
	struct inode		vfs_inode;
};

// shmem_sb_info 表示一个共享内存（shmem）文件系统的 superblock 信息
struct shmem_sb_info {
	// max_blocks: 允许的最大块数。
	unsigned long max_blocks;   /* How many blocks are allowed */
	// used_blocks: 已分配的块数（使用 percpu_counter 以便在多核系统上高效计数）。
	struct percpu_counter used_blocks;  /* How many are allocated */
	// max_inodes: 允许的最大 inode 数。
	unsigned long max_inodes;   /* How many inodes are allowed */
	// free_inodes: 仍可分配的 inode 数。
	unsigned long free_inodes;  /* How many are left for allocation */
	// stat_lock: 用于序列化 shmem_sb_info 更改的自旋锁。
	spinlock_t stat_lock;	    /* Serialize shmem_sb_info changes */
	// mode: 根目录的挂载模式。
	umode_t mode;		    /* Mount mode for root directory */
	// huge: 是否尝试使用巨页（hugepages）。
	unsigned char huge;	    /* Whether to try for hugepages */
	// uid: 根目录的挂载用户 ID。
	kuid_t uid;		    /* Mount uid for root directory */
	// gid: 根目录的挂载组 ID。
	kgid_t gid;		    /* Mount gid for root directory */
	// mpol: 映射的默认内存策略。
	struct mempolicy *mpol;     /* default memory policy for mappings */
	// shrinklist_lock: 用于保护 shrinklist 的自旋锁。
	spinlock_t shrinklist_lock;   /* Protects shrinklist */
	// shrinklist: 可收缩 inode 的列表。
	struct list_head shrinklist;  /* List of shinkable inodes */
	// shrinklist_len: shrinklist 的长度。
	unsigned long shrinklist_len; /* Length of shrinklist */
};

static inline struct shmem_inode_info *SHMEM_I(struct inode *inode)
{
	return container_of(inode, struct shmem_inode_info, vfs_inode);
}

/*
 * Functions in mm/shmem.c called directly from elsewhere:
 */
extern int shmem_init(void);
extern int shmem_fill_super(struct super_block *sb, void *data, int silent);
extern struct file *shmem_file_setup(const char *name,
					loff_t size, unsigned long flags);
extern struct file *shmem_kernel_file_setup(const char *name, loff_t size,
					    unsigned long flags);
extern struct file *shmem_file_setup_with_mnt(struct vfsmount *mnt,
		const char *name, loff_t size, unsigned long flags);
extern int shmem_zero_setup(struct vm_area_struct *);
extern unsigned long shmem_get_unmapped_area(struct file *, unsigned long addr,
		unsigned long len, unsigned long pgoff, unsigned long flags);
extern int shmem_lock(struct file *file, int lock, struct user_struct *user);
#ifdef CONFIG_SHMEM
extern bool shmem_mapping(struct address_space *mapping);
#else
static inline bool shmem_mapping(struct address_space *mapping)
{
	return false;
}
#endif /* CONFIG_SHMEM */
extern void shmem_unlock_mapping(struct address_space *mapping);
extern struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
					pgoff_t index, gfp_t gfp_mask);
extern void shmem_truncate_range(struct inode *inode, loff_t start, loff_t end);
extern int shmem_unuse(swp_entry_t entry, struct page *page);

extern unsigned long shmem_swap_usage(struct vm_area_struct *vma);
extern unsigned long shmem_partial_swap_usage(struct address_space *mapping,
						pgoff_t start, pgoff_t end);

/* Flag allocation requirements to shmem_getpage */
// sgp_type 表示在获取共享内存文件系统（shmem）页时要采取的策略
enum sgp_type {
	// SGP_READ: 不超过 i_size，不分配页。
	SGP_READ,	/* don't exceed i_size, don't allocate page */
	// SGP_CACHE: 不超过 i_size，可能分配页。
	SGP_CACHE,	/* don't exceed i_size, may allocate page */
	// SGP_NOHUGE: 与 SGP_CACHE 类似，但不使用巨页（huge pages）。
	SGP_NOHUGE,	/* like SGP_CACHE, but no huge pages */
	// SGP_HUGE: 与 SGP_CACHE 类似，优先使用巨页（huge pages）。
	SGP_HUGE,	/* like SGP_CACHE, huge pages preferred */
	// SGP_WRITE: 可能超过 i_size，可能分配非 Uptodate（未更新）页。
	SGP_WRITE,	/* may exceed i_size, may allocate !Uptodate page */
	// SGP_FALLOC: 与 SGP_WRITE 类似，但使现有页处于 Uptodate（已更新）状态。
	SGP_FALLOC,	/* like SGP_WRITE, but make existing page Uptodate */
};

extern int shmem_getpage(struct inode *inode, pgoff_t index,
		struct page **pagep, enum sgp_type sgp);

static inline struct page *shmem_read_mapping_page(
				struct address_space *mapping, pgoff_t index)
{
	return shmem_read_mapping_page_gfp(mapping, index,
					mapping_gfp_mask(mapping));
}

static inline bool shmem_file(struct file *file)
{
	if (!IS_ENABLED(CONFIG_SHMEM))
		return false;
	if (!file || !file->f_mapping)
		return false;
	return shmem_mapping(file->f_mapping);
}

extern bool shmem_charge(struct inode *inode, long pages);
extern void shmem_uncharge(struct inode *inode, long pages);

#ifdef CONFIG_TRANSPARENT_HUGE_PAGECACHE
extern bool shmem_huge_enabled(struct vm_area_struct *vma);
#else
static inline bool shmem_huge_enabled(struct vm_area_struct *vma)
{
	return false;
}
#endif

#ifdef CONFIG_SHMEM
extern int shmem_mcopy_atomic_pte(struct mm_struct *dst_mm, pmd_t *dst_pmd,
				  struct vm_area_struct *dst_vma,
				  unsigned long dst_addr,
				  unsigned long src_addr,
				  struct page **pagep);
extern int shmem_mfill_zeropage_pte(struct mm_struct *dst_mm,
				    pmd_t *dst_pmd,
				    struct vm_area_struct *dst_vma,
				    unsigned long dst_addr);
#else
#define shmem_mcopy_atomic_pte(dst_mm, dst_pte, dst_vma, dst_addr, \
			       src_addr, pagep)        ({ BUG(); 0; })
#define shmem_mfill_zeropage_pte(dst_mm, dst_pmd, dst_vma, \
				 dst_addr)      ({ BUG(); 0; })
#endif

#endif
