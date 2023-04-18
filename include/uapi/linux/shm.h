/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SHM_H_
#define _UAPI_LINUX_SHM_H_

#include <linux/ipc.h>
#include <linux/errno.h>
#include <asm-generic/hugetlb_encode.h>
#ifndef __KERNEL__
#include <unistd.h>
#endif

/*
 * SHMMNI, SHMMAX and SHMALL are default upper limits which can be
 * modified by sysctl. The SHMMAX and SHMALL values have been chosen to
 * be as large possible without facilitating scenarios where userspace
 * causes overflows when adjusting the limits via operations of the form
 * "retrieve current limit; add X; update limit". It is therefore not
 * advised to make SHMMAX and SHMALL any larger. These limits are
 * suitable for both 32 and 64-bit systems.
 */
// SHMMIN 定义了每个共享内存段的最小大小，单位为字节。如果共享内存段小于该值，则将被系统视为无效，无法使用。
#define SHMMIN 1			 /* min shared seg size (bytes) */
// SHMMNI 定义了系统范围内最大的可创建共享内存段数目。如果超过该值，将无法创建新的共享内存段。
#define SHMMNI 4096			 /* max num of segs system wide */
// SHMMAX 定义了系统中最大的一块可用于共享内存的连续物理内存大小，单位为字节。即使系统中有足够大的可用内存，如果无法满足一块连续的物理内存空间，则无法创建超过该大小的共享内存段。
#define SHMMAX (ULONG_MAX - (1UL << 24)) /* max shared seg size (bytes) */
// SHMALL 定义了系统可用于共享内存的最大物理页面数。每个物理页面通常为4KB，因此该值的单位是页面。如果超过该值，则无法创建更多的共享内存段，即使系统中有足够的可用内存。
#define SHMALL (ULONG_MAX - (1UL << 24)) /* max shm system wide (pages) */
// SHMSEG 定义了每个进程可以创建的最大共享内存段数量。如果超过该值，则无法创建更多的共享内存段。
#define SHMSEG SHMMNI			 /* max shared segs per process */

/* Obsolete, used only for backwards compatibility and libc5 compiles */
// struct shmid_ds是共享内存的相关信息结构体，在Linux系统中用于记录共享内存的权限、大小、最近一次访问、修改及其创建者和当前连接情况等信息
struct shmid_ds {
	// shm_perm：共享内存的操作权限, 包含了共享内存的所有者、所属组、权限等信息
	struct ipc_perm		shm_perm;	/* operation perms */
	// shm_segsz：共享内存的大小，单位为字节
	int			shm_segsz;	/* size of segment (bytes) */
	// shm_atime：共享内存最近一次的连接时间
	__kernel_time_t		shm_atime;	/* last attach time */
	// shm_dtime：共享内存最近一次的断开连接时间
	__kernel_time_t		shm_dtime;	/* last detach time */
	// shm_ctime：共享内存最近一次的修改时间
	__kernel_time_t		shm_ctime;	/* last change time */
	// shm_cpid：创建共享内存的进程ID
	__kernel_ipc_pid_t	shm_cpid;	/* pid of creator */
	// shm_lpid：共享内存最近一次的连接者的进程ID
	__kernel_ipc_pid_t	shm_lpid;	/* pid of last operator */
	// shm_nattch：当前连接到共享内存的进程数
	unsigned short		shm_nattch;	/* no. of current attaches */
	// shm_unused：保留字段
	unsigned short 		shm_unused;	/* compatibility */
	// shm_unused2：保留字段
	void 			*shm_unused2;	/* ditto - used by DIPC */
	// shm_unused3：保留字段
	void			*shm_unused3;	/* unused */
};

/* Include the definition of shmid64_ds and shminfo64 */
#include <asm/shmbuf.h>

/*
 * shmget() shmflg values.
 */
/* The bottom nine bits are the same as open(2) mode flags */
// SHM_R：表示共享内存段的读权限
#define SHM_R		0400	/* or S_IRUGO from <linux/stat.h> */
// SHM_W：表示共享内存段的写权限
#define SHM_W		0200	/* or S_IWUGO from <linux/stat.h> */
/* Bits 9 & 10 are IPC_CREAT and IPC_EXCL */
// SHM_HUGETLB：表示共享内存段将使用大页面（HugeTLB）方式
#define SHM_HUGETLB	04000	/* segment will use huge TLB pages */
// SHM_NORESERVE：表示内核在创建共享内存段时不检查申请内存空间是否超过系统最大限制，从而可以允许用户创建超过系统限制的共享内存。
#define SHM_NORESERVE	010000	/* don't check for reservations */

/*
 * Huge page size encoding when SHM_HUGETLB is specified, and a huge page
 * size other than the default is desired.  See hugetlb_encode.h
 */
#define SHM_HUGE_SHIFT	HUGETLB_FLAG_ENCODE_SHIFT
#define SHM_HUGE_MASK	HUGETLB_FLAG_ENCODE_MASK

#define SHM_HUGE_64KB	HUGETLB_FLAG_ENCODE_64KB
#define SHM_HUGE_512KB	HUGETLB_FLAG_ENCODE_512KB
#define SHM_HUGE_1MB	HUGETLB_FLAG_ENCODE_1MB
#define SHM_HUGE_2MB	HUGETLB_FLAG_ENCODE_2MB
#define SHM_HUGE_8MB	HUGETLB_FLAG_ENCODE_8MB
#define SHM_HUGE_16MB	HUGETLB_FLAG_ENCODE_16MB
#define SHM_HUGE_32MB	HUGETLB_FLAG_ENCODE_32MB
#define SHM_HUGE_256MB	HUGETLB_FLAG_ENCODE_256MB
#define SHM_HUGE_512MB	HUGETLB_FLAG_ENCODE_512MB
#define SHM_HUGE_1GB	HUGETLB_FLAG_ENCODE_1GB
#define SHM_HUGE_2GB	HUGETLB_FLAG_ENCODE_2GB
#define SHM_HUGE_16GB	HUGETLB_FLAG_ENCODE_16GB

/*
 * shmat() shmflg values
 */
// SHM_RDONLY：表示只读访问共享内存
#define	SHM_RDONLY	010000	/* read-only access */
// SHM_RND：表示在连接共享内存时将其地址舍入到SHMLBA（共享内存当前大页面大小）的边界上，以提高应用程序的性能
#define	SHM_RND		020000	/* round attach address to SHMLBA boundary */
// SHM_REMAP：表示在连接共享内存时，如果前一个连接已经分离，本次连接尝试复用之前的内存区域
#define	SHM_REMAP	040000	/* take-over region on attach */
// SHM_EXEC：表示共享内存可用于执行程序
#define	SHM_EXEC	0100000	/* execution access */

/* super user shmctl commands */
#define SHM_LOCK 	11
#define SHM_UNLOCK 	12

/* ipcs ctl commands */
// 共享内存操作的一些命令码
// SHM_STAT：用于获取共享内存段的状态信息
#define SHM_STAT	13
// SHM_INFO：用于获取系统中的共享内存信息
#define SHM_INFO	14
// SHM_STAT_ANY：用于获取任意一个共享内存段的状态信息，不受进程是否拥有该共享内存段的限制
#define SHM_STAT_ANY    15

/* Obsolete, used only for backwards compatibility */
// 获取系统共享内存参数的结构体
// 可以通过 shmctl() 系统调用中的 IPC_INFO 命令来获取系统中的 shminfo 结构体信息。
struct	shminfo {
	// shmmax：每个共享内存段最大的大小（字节数）
	int shmmax;
	// shmmin：每个共享内存段最小的大小（字节数）
	int shmmin;
	// shmmni：系统中共享内存段的最大数目
	int shmmni;
	// shmseg：每个进程可以拥有的共享内存段的最大数目
	int shmseg;
	// shmall：所有进程能够使用的共享内存的最大大小（字节数）
	int shmall;
};

// 可以通过 shmctl() 系统调用中的 IPC_INFO 命令来获取系统中的 shm_info 结构体信息。
struct shm_info {
	// used_ids：系统中已经使用的共享内存段的数量
	int used_ids;
	// shm_tot：系统中共享内存段的总大小（字节数）
	__kernel_ulong_t shm_tot;	/* total allocated shm */
	// shm_rss：系统中共享内存段的驻留大小（字节数），即当前被使用的共享内存段的大小
	__kernel_ulong_t shm_rss;	/* total resident shm */
	// shm_swp：系统中共享内存段在交换空间中的大小（字节数）
	__kernel_ulong_t shm_swp;	/* total swapped shm */
	// swap_attempts：尝试将共享内存段交换出主存的次数
	__kernel_ulong_t swap_attempts;
	// swap_successes：实际成功将共享内存段交换出主存的次数
	__kernel_ulong_t swap_successes;
};


#endif /* _UAPI_LINUX_SHM_H_ */
