/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_RESOURCE_H
#define _UAPI_LINUX_RESOURCE_H

#include <linux/time.h>
#include <linux/types.h>

/*
 * Resource control/accounting header file for linux
 */

/*
 * Definition of struct rusage taken from BSD 4.3 Reno
 * 
 * We don't support all of these yet, but we might as well have them....
 * Otherwise, each time we add new items, programs which depend on this
 * structure will lose.  This reduces the chances of that happening.
 */
#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	(-1)
#define RUSAGE_BOTH	(-2)		/* sys_wait4() uses this */
#define	RUSAGE_THREAD	1		/* only the calling thread */

// rusage: 进程资源使用情况的结构体定义
struct	rusage {
	// ru_utime：该进程所有线程在用户态下运行消耗的时间。
	struct timeval ru_utime;	/* user time used */
	// ru_stime：该进程所有线程在内核态下运行消耗的时间。
	struct timeval ru_stime;	/* system time used */
	// ru_maxrss：该进程使用内存中最大的数据占用空间大小，以kbyte为单位。
	__kernel_long_t	ru_maxrss;	/* maximum resident set size */
	// ru_ixrss：该进程所使用的共享内存空间大小，以kbyte为单位。
	__kernel_long_t	ru_ixrss;	/* integral shared memory size */
	// ru_idrss：该进程所使用的私有内存空间大小，以kbyte为单位。
	__kernel_long_t	ru_idrss;	/* integral unshared data size */
	// ru_isrss：该进程所使用的私有栈空间大小，以kbyte为单位。
	__kernel_long_t	ru_isrss;	/* integral unshared stack size */
	// ru_minflt：该进程发生的非必要缺页（page fault）次数。
	__kernel_long_t	ru_minflt;	/* page reclaims */
	// ru_majflt：该进程发生的必要缺页（page fault）次数。
	__kernel_long_t	ru_majflt;	/* page faults */
	// ru_nswap：该进程被交换到磁盘的次数。
	__kernel_long_t	ru_nswap;	/* swaps */
	// ru_inblock：是指进程在等待 stdin 的阻塞时间。这包括等待键盘输入、管道输入、网络输入等。
	__kernel_long_t	ru_inblock;	/* block input operations */
	// ru_oublock：是指进程在等待 stdout 或 stderr 的阻塞时间。这包括等待输出到终端、管道输出、网络输出等。
	__kernel_long_t	ru_oublock;	/* block output operations */
	// ru_msgsnd：该进程发起的消息发送次数。
	__kernel_long_t	ru_msgsnd;	/* messages sent */
	// ru_msgrcv：该进程发起的消息接收次数。
	__kernel_long_t	ru_msgrcv;	/* messages received */
	// ru_nsignals：该进程接收到的信号数量。
	__kernel_long_t	ru_nsignals;	/* signals received */
	// ru_nvcsw：是指进程从阻塞状态返回用户空间的次数
	__kernel_long_t	ru_nvcsw;	/* voluntary context switches */
	// ru_nivcsw：是指进程从内核进入阻塞状态的次数。
	__kernel_long_t	ru_nivcsw;	/* involuntary " */
};

// 可以通过调用 setrlimit()，getrlimit() 等函数来获取并设置进程的系统资源限制信息，用于限制进程能够使用的资源量，防止进程耗尽系统资源导致系统崩溃。通常在编写对系统资源敏感的程序时需要使用这些函数来确保程序运行的稳定性。
struct rlimit {
	// rlim_cur：当前资源限制的数值。对一个新的进程，该值就是它的硬限制值，但是它可以被 setrlimit() 等函数改变。该字段通常定义了进程当前可用的资源（比如打开文件的数量）。
	__kernel_ulong_t	rlim_cur;
	// rlim_max：资源限制的最大值。该字段通常定义了特权进程可以使用的资源上限（比如最大可打开文件数），如果该字段的值为 RLIM_INFINITY，表示特权进程没有资源上限限制。
	__kernel_ulong_t	rlim_max;
};

// 该宏定义用于表示进程资源限制的最大值。当进程要使用的资源数量超过最大值时，就会达到限制，从而可能导致进程出错。
// RLIM64_INFINITY 的值为无符号长整型（unsigned long long）0xFFFFFFFFFFFFFFFF，即无限制的最大值。
#define RLIM64_INFINITY		(~0ULL)

struct rlimit64 {
	__u64 rlim_cur;
	__u64 rlim_max;
};

#define	PRIO_MIN	(-20)
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Limit the stack by to some sane default: root can always
 * increase this limit if needed..  8MB seems reasonable.
 */
#define _STK_LIM	(8*1024*1024)

/*
 * GPG2 wants 64kB of mlocked memory, to make sure pass phrases
 * and other sensitive information are never written to disk.
 */
#define MLOCK_LIMIT	((PAGE_SIZE > 64*1024) ? PAGE_SIZE : 64*1024)

/*
 * Due to binary compatibility, the actual resource numbers
 * may be different for different linux versions..
 */
#include <asm/resource.h>


#endif /* _UAPI_LINUX_RESOURCE_H */
