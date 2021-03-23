Kernel initialization. Part 10.
================================================================================

End of the linux kernel initialization process
================================================================================

This is tenth part of the chapter about linux kernel [initialization process](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/index.html) and in the [previous part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-9.html) we saw the initialization of the [RCU](http://en.wikipedia.org/wiki/Read-copy-update) and stopped on the call of the `acpi_early_init` function. This part will be the last part of the [Kernel initialization process](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/index.html) chapter, so let's finish it.

After the call of the `acpi_early_init` function from the [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c), we can see the following code:

```C
#ifdef CONFIG_X86_ESPFIX64
	init_espfix_bsp();
#endif
```

Here we can see the call of the `init_espfix_bsp` function which depends on the `CONFIG_X86_ESPFIX64` kernel configuration option. 

```c
void __init init_espfix_bsp(void)   /*  */
{
	pgd_t *pgd;
	p4d_t *p4d;

	/* Install the espfix pud into the kernel page directory */
	pgd = &init_top_pgt[pgd_index(ESPFIX_BASE_ADDR)];
	p4d = p4d_alloc(&init_mm, pgd, ESPFIX_BASE_ADDR);
	p4d_populate(&init_mm, p4d, espfix_pud_page);

	/* Randomize the locations */
	init_espfix_random();

	/* The rest is the same as for any other processor */
	init_espfix_ap(0);
}
```

As we can understand from the function name, it does something with the stack. This function is defined in the [arch/x86/kernel/espfix_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/espfix_64.c) and prevents leaking of `31:16` bits of the `esp` register during returning to 16-bit stack. First of all we install `espfix` page upper directory into the kernel page directory in the `init_espfix_bs`:

```C
pgd_p = &init_level4_pgt[pgd_index(ESPFIX_BASE_ADDR)];
pgd_populate(&init_mm, pgd_p, (pud_t *)espfix_pud_page);
```

Where `ESPFIX_BASE_ADDR` is:

```C
#define PGDIR_SHIFT     39
#define ESPFIX_PGD_ENTRY _AC(-2, UL)
#define ESPFIX_BASE_ADDR (ESPFIX_PGD_ENTRY << PGDIR_SHIFT)
```

Also we can find it in the [Documentation/x86/x86_64/mm](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt):

```
... unused hole ...
ffffff0000000000 - ffffff7fffffffff (=39 bits) %esp fixup stacks
... unused hole ...
```

After we've filled page global directory with the `espfix` pud, the next step is call of the `init_espfix_random` and `init_espfix_ap` functions. The first function returns random locations for the `espfix` page and the second enables the `espfix` for the current CPU. 

After the `init_espfix_bsp` finished the work, we can see the call of the `thread_info_cache_init` function which defined in the [kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c) and allocates cache for the `thread_info` if `THREAD_SIZE` is less than `PAGE_SIZE`:

```C
# if THREAD_SIZE >= PAGE_SIZE
...
...
...
void thread_info_cache_init(void)
{
        thread_info_cache = kmem_cache_create("thread_info", THREAD_SIZE,
                                              THREAD_SIZE, 0, NULL);
        BUG_ON(thread_info_cache == NULL);
}
...
...
...
#endif
```
5.10.13是：
```c
void thread_stack_cache_init(void)  /*线程栈  */
{
	thread_stack_cache = kmem_cache_create_usercopy("thread_stack",
					THREAD_SIZE, THREAD_SIZE, 0, 0,
					THREAD_SIZE, NULL);
	BUG_ON(thread_stack_cache == NULL);
}
```

As we already know the `PAGE_SIZE` is `(_AC(1,UL) << PAGE_SHIFT)` or `4096` bytes and `THREAD_SIZE` is `(PAGE_SIZE << THREAD_SIZE_ORDER)` or `16384` bytes for the `x86_64`. 


The next function after the `thread_info_cache_init` is the `cred_init` from the [kernel/cred.c](https://github.com/torvalds/linux/blob/master/kernel/cred.c). This function just allocates cache for the credentials (like `uid`, `gid`, etc.):

```C
/*
 * initialise the credentials stuff 初始化凭据的东西
 */
void __init cred_init(void) /*  */
{
	/* allocate a slab in which we can store credentials */
	cred_jar = kmem_cache_create("cred_jar", sizeof(struct cred), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT, NULL);
}
```

more about credentials you can read in the [Documentation/security/credentials.txt](https://github.com/torvalds/linux/blob/master/Documentation/security/credentials.txt). 

Next step is the `fork_init` function from the [kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c). The `fork_init` function allocates cache for the `task_struct`. Let's look on the implementation of the `fork_init`. 

First of all we can see definitions of the `ARCH_MIN_TASKALIGN` macro and creation of a slab where task_structs will be allocated:

```C
#ifndef CONFIG_ARCH_TASK_STRUCT_ALLOCATOR
#ifndef ARCH_MIN_TASKALIGN
#define ARCH_MIN_TASKALIGN      L1_CACHE_BYTES
#endif
        task_struct_cachep =
                kmem_cache_create("task_struct", sizeof(struct task_struct),
                        ARCH_MIN_TASKALIGN, SLAB_PANIC | SLAB_NOTRACK, NULL);
#endif
```

As we can see this code depends on the `CONFIG_ARCH_TASK_STRUCT_ACLLOCATOR` kernel configuration option. This configuration option shows the presence of the `alloc_task_struct` for the given architecture. As `x86_64` has no `alloc_task_struct` function, this code will not work and even will not be compiled on the `x86_64`.


Allocating cache for init task
--------------------------------------------------------------------------------

After this we can see the call of the `arch_task_cache_init` function in the `fork_init`:

> 5.10.13中arch_task_cache_init为空。

```C
void arch_task_cache_init(void)
{
    task_xstate_cachep =
            kmem_cache_create("task_xstate", xstate_size,
                              __alignof__(union thread_xstate),
                              SLAB_PANIC | SLAB_NOTRACK, NULL);
    setup_xstate_comp();
}
```

The `arch_task_cache_init` does initialization of the architecture-specific caches. In our case it is `x86_64`, so as we can see, the `arch_task_cache_init` allocates cache for the `task_xstate` which represents [FPU](http://en.wikipedia.org/wiki/Floating-point_unit) state and sets up offsets and sizes of all extended states in [xsave](http://www.felixcloutier.com/x86/XSAVES.html) area with the call of the `setup_xstate_comp` function. After the `arch_task_cache_init` we calculate default maximum number of threads with the:

```C
set_max_threads(MAX_THREADS);
```

where default maximum number of threads is:

```C
#define FUTEX_TID_MASK  0x3fffffff
#define MAX_THREADS     FUTEX_TID_MASK
```

In the end of the `fork_init` function we initialize [signal](http://www.win.tue.nl/~aeb/linux/lk/lk-5.html) handler:

```C
init_task.signal->rlim[RLIMIT_NPROC].rlim_cur = max_threads/2;
init_task.signal->rlim[RLIMIT_NPROC].rlim_max = max_threads/2;
init_task.signal->rlim[RLIMIT_SIGPENDING] =
		init_task.signal->rlim[RLIMIT_NPROC];
```

As we know the `init_task` is an instance of the `task_struct` structure, so it contains `signal` field which represents signal handler. It has following type `struct signal_struct`. On the first two lines we can see setting of the current and maximum limit of the `resource limits`. Every process has an associated set of resource limits. These limits specify amount of resources which current process can use. Here `rlim` is resource control limit and presented by the:

```C
struct rlimit {
    __kernel_ulong_t        rlim_cur;
    __kernel_ulong_t        rlim_max;
};
```

structure from the [include/uapi/linux/resource.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/resource.h). In our case the resource is the `RLIMIT_NPROC` which is the maximum number of processes that user can own and `RLIMIT_SIGPENDING` - the maximum number of pending signals. We can see it in the:

```C
cat /proc/self/limits
Limit                     Soft Limit           Hard Limit           Units     
...
...
...
Max processes             63815                63815                processes 
Max pending signals       63815                63815                signals   
...
...
...
```

Initialization of the caches
--------------------------------------------------------------------------------

The next function after the `fork_init` is the `proc_caches_init` from the [kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c). 
```c

void __init proc_caches_init(void)  /* /proc/slabinfo 中可查到的  */
{
	unsigned int mm_size;

	sighand_cachep = kmem_cache_create("sighand_cache",
			sizeof(struct sighand_struct), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_TYPESAFE_BY_RCU|
			SLAB_ACCOUNT, sighand_ctor);
	signal_cachep = kmem_cache_create("signal_cache",
			sizeof(struct signal_struct), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT,
			NULL);
	files_cachep = kmem_cache_create("files_cache",
			sizeof(struct files_struct), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT,
			NULL);
	fs_cachep = kmem_cache_create("fs_cache",
			sizeof(struct fs_struct), 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT,
			NULL);

	/*
	 * The mm_cpumask is located at the end of mm_struct, and is
	 * dynamically sized based on the maximum CPU number this system
	 * can have, taking hotplug into account (nr_cpu_ids).
	 */
	mm_size = sizeof(struct mm_struct) + cpumask_size();

	mm_cachep = kmem_cache_create_usercopy("mm_struct",
			mm_size, ARCH_MIN_MMSTRUCT_ALIGN,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT,
			offsetof(struct mm_struct, saved_auxv),
			sizeof_field(struct mm_struct, saved_auxv),
			NULL);
	vm_area_cachep = KMEM_CACHE(vm_area_struct, SLAB_PANIC|SLAB_ACCOUNT);
	mmap_init();    /* 初始化percpu计数器 for VM 和 region 记录  slabs */
	nsproxy_cache_init();   /* namesapce proxy 缓存分配 */
}
```

This function allocates caches for the memory descriptors (or `mm_struct` structure). At the beginning of the `proc_caches_init` we can see allocation of the different [SLAB](http://en.wikipedia.org/wiki/Slab_allocation) caches with the call of the `kmem_cache_create`:

* `sighand_cachep` - manage information about installed signal handlers;
* `signal_cachep` - manage information about process signal descriptor; 
* `files_cachep` - manage information about opened files;
* `fs_cachep` - manage filesystem information.

在我的系统中：
```c
[rongtao@localhost src]$ sudo cat /proc/slabinfo | grep -e signal -e fs_cache -e signal -e files_cache -e mm_struct
mm_struct            180    180   1600   20    8 : tunables    0    0    0 : slabdata      9      9      0
files_cache          459    459    640   51    8 : tunables    0    0    0 : slabdata      9      9      0
signal_cache         560    560   1152   28    8 : tunables    0    0    0 : slabdata     20     20      0

```

After this we allocate `SLAB` cache for the `mm_struct` structures:

```C
mm_cachep = kmem_cache_create("mm_struct",
                         sizeof(struct mm_struct), ARCH_MIN_MMSTRUCT_ALIGN,
                         SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_NOTRACK, NULL);
```


After this we allocate `SLAB` cache for the important `vm_area_struct` which used by the kernel to manage virtual memory space:

```C
vm_area_cachep = KMEM_CACHE(vm_area_struct, SLAB_PANIC);
```

Note, that we use `KMEM_CACHE` macro here instead of the `kmem_cache_create`. This macro is defined in the [include/linux/slab.h](https://github.com/torvalds/linux/blob/master/include/linux/slab.h) and just expands to the `kmem_cache_create` call:

```C
#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
                sizeof(struct __struct), __alignof__(struct __struct),\
                (__flags), NULL)
```

The `KMEM_CACHE` has one difference from `kmem_cache_create`. Take a look on `__alignof__` operator. The `KMEM_CACHE` macro aligns `SLAB` to the size of the given structure, but `kmem_cache_create` uses given value to align space. 

After this we can see the call of the `mmap_init` and `nsproxy_cache_init` functions. The first function initializes virtual memory area `SLAB` and the second function initializes `SLAB` for namespaces.
```c
int __init nsproxy_cache_init(void) /* namespace proxy 缓存 */
{
	nsproxy_cachep = KMEM_CACHE(nsproxy, SLAB_PANIC);
	return 0;
}
```
The next function after the `proc_caches_init` is `buffer_init`. This function is defined in the [fs/buffer.c](https://github.com/torvalds/linux/blob/master/fs/buffer.c) source code file and allocate cache for the `buffer_head`. The `buffer_head` is a special structure which defined in the [include/linux/buffer_head.h](https://github.com/torvalds/linux/blob/master/include/linux/buffer_head.h) and used for managing buffers.

```
$ sudo cat /proc/slabinfo | grep buffer
[sudo] rongtao 的密码：
buffer_head       486781 594984    104   39    1 : tunables    0    0    0 : slabdata  15256  15256      0
```
In the start of the `buffer_init` function we allocate cache for the `struct buffer_head` structures with the call of the `kmem_cache_create` function as we did in the previous functions. And calculate the maximum size of the buffers in memory with:

```C
nrpages = (nr_free_buffer_pages() * 10) / 100;
max_buffer_heads = nrpages * (PAGE_SIZE / sizeof(struct buffer_head));
```

which will be equal to the `10%` of the `ZONE_NORMAL` (all RAM from the 4GB on the `x86_64`). 

The next function after the `buffer_init` is - `vfs_caches_init`. 

```c
void __init vfs_caches_init(void)   /*虚拟文件系统 缓存  */
{
	names_cachep = kmem_cache_create_usercopy("names_cache", PATH_MAX, 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC, 0, PATH_MAX, NULL);

	dcache_init();  /* 文件目录缓存 */
	inode_init();   /* inode 缓存 */
	files_init();   /* 文件缓存 */
	files_maxfiles_init();  /*  */
	mnt_init();     /* 挂载 */
	bdev_cache_init();  /* 块设备 缓存 */
	chrdev_init();  /* 字符设备 */
}
```

This function allocates `SLAB` caches and hashtable for different [VFS](http://en.wikipedia.org/wiki/Virtual_file_system) caches. We already saw the `vfs_caches_init_early` function in the eighth part of the linux kernel [initialization process](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-8.html) which initialized caches for `dcache` (or directory-cache) and [inode](http://en.wikipedia.org/wiki/Inode) cache. 

The `vfs_caches_init` function makes post-early initialization of the `dcache` and `inode` caches, private data cache, hash tables for the mount points, etc. More details about [VFS](http://en.wikipedia.org/wiki/Virtual_file_system) will be described in the separate part. 

After this we can see `signals_init` function.

```c
void __init signals_init(void)  /*  */
{
	siginfo_buildtime_checks();

	sigqueue_cachep = KMEM_CACHE(sigqueue, SLAB_PANIC);/*  */
}
```

This function is defined in the [kernel/signal.c](https://github.com/torvalds/linux/blob/master/kernel/signal.c) and allocates a cache for the `sigqueue` structures which represents queue of the real time signals. 

The next function is `page_writeback_init`. This function initializes the ratio for the dirty pages. Every low-level page entry contains the `dirty` bit which indicates whether a page has been written to after been loaded into memory.

该函数已经转移至如下函数：
```c
void __init pagecache_init(void)    /* 页缓存 */
{
	int i;

	for (i = 0; i < PAGE_WAIT_TABLE_SIZE; i++)
		init_waitqueue_head(&page_wait_table[i]);

	page_writeback_init();  /* 页回写 */
}
```


Creation of the root for the procfs
--------------------------------------------------------------------------------

After all of this preparations we need to create the root for the [proc](http://en.wikipedia.org/wiki/Procfs) filesystem. We will do it with the call of the `proc_root_init` function from the [fs/proc/root.c](https://github.com/torvalds/linux/blob/master/fs/proc/root.c). 

```c
void __init proc_root_init(void)    /*  */
{
	proc_init_kmemcache();  /* kmem_cache */
	set_proc_pid_nlink();   /* /proc/PID/  */
	proc_self_init();       /* /proc/self/ */
	proc_thread_self_init();/*  */
	proc_symlink("mounts", NULL, "self/mounts");    /* /proc/PID/mounts */

	proc_net_init();        /* /proc/net/ */
	proc_mkdir("fs", NULL); /* /proc/fs/ */
	proc_mkdir("driver", NULL); /* /proc/driver/ */
	proc_create_mount_point("fs/nfsd"); /* somewhere for the nfsd filesystem to be mounted */
#if defined(CONFIG_SUN_OPENPROMFS) || defined(CONFIG_SUN_OPENPROMFS_MODULE)
	/* just give it a mountpoint */
	proc_create_mount_point("openprom");
#endif
	proc_tty_init();        /* /proc/tty */
	proc_mkdir("bus", NULL);/* /proc/bus */
	proc_sys_init();        /* /proc/sys */

	register_filesystem(&proc_fs_type);
}
```

At the start of the `proc_root_init` function we allocate the cache for the inodes and register a new filesystem in the system with the:

```C
err = register_filesystem(&proc_fs_type);
if (err)
    return;
```
proc_fs_type结构如下：
```c
static struct file_system_type proc_fs_type = {
	.name			= "proc",
	.init_fs_context	= proc_init_fs_context,
	.parameters		= proc_fs_parameters,
	.kill_sb		= proc_kill_sb,
	.fs_flags		= FS_USERNS_MOUNT | FS_DISALLOW_NOTIFY_PERM,
};
```

As I wrote above we will not dive into details about [VFS](http://en.wikipedia.org/wiki/Virtual_file_system) and different filesystems in this chapter, but will see it in the chapter about the `VFS`. After we've registered a new filesystem in our system, we call the `proc_self_init` function from the [fs/proc/self.c](https://github.com/torvalds/linux/blob/master/fs/proc/self.c) and this function allocates `inode` number for the `self` (`/proc/self` directory refers to the process accessing the `/proc` filesystem). The next step after the `proc_self_init` is `proc_setup_thread_self` which setups the `/proc/thread-self` directory which contains information about current thread. After this we create `/proc/self/mounts` symlink which will contains mount points with the call of the

```C
proc_symlink("mounts", NULL, "self/mounts");
```

and a couple of directories depends on the different configuration options:

```C
#ifdef CONFIG_SYSVIPC
    proc_mkdir("sysvipc", NULL);
#endif
    proc_mkdir("fs", NULL);
    proc_mkdir("driver", NULL);
    proc_mkdir("fs/nfsd", NULL);
#if defined(CONFIG_SUN_OPENPROMFS) || defined(CONFIG_SUN_OPENPROMFS_MODULE)
    proc_mkdir("openprom", NULL);
#endif
    proc_mkdir("bus", NULL);
    ...
    ...
    ...
    if (!proc_mkdir("tty", NULL))
             return;
    proc_mkdir("tty/ldisc", NULL);
    ...
    ...
    ...
```

In the end of the `proc_root_init` we call the `proc_sys_init` function which creates `/proc/sys` directory and initializes the [Sysctl](http://en.wikipedia.org/wiki/Sysctl).

It is the end of `start_kernel` function. I did not describe all functions which are called in the `start_kernel`. I skipped them, because they are not important for the generic kernel initialization stuff and depend on only different kernel configurations.

* `taskstats_init_early` which exports per-task statistic to the user-space, 
* `delayacct_init` - initializes per-task delay accounting, 
* `key_init` and `security_init` initialize different security stuff, 
* `check_bugs` - fix some architecture-dependent bugs, 
* `ftrace_init` function executes initialization of the [ftrace](https://www.kernel.org/doc/Documentation/trace/ftrace.txt), 
* `cgroup_init` makes initialization of the rest of the [cgroup](http://en.wikipedia.org/wiki/Cgroups) subsystem,etc. 

Many of these parts and subsystems will be described in the other chapters.

That's all. 

Finally we have passed through the long-long `start_kernel` function. But it is not the end of the linux kernel initialization process. We haven't run the first process yet. In the end of the `start_kernel` we can see the last call of the - `rest_init` function. Let's go ahead.

```c
void __init __weak arch_call_rest_init(void)    /*  */
{
	rest_init();    /* 在linux启动的阶段start_kernel()的最后，
	rest_init()会开启两个进程：kernel_init，kthreadd，之后主线程变成idle线程，init/main.c。
    linux下的3个特殊的进程：idle进程（PID=0），init进程（PID=1）和kthreadd（PID=2） */
}
```


First steps after the start_kernel
--------------------------------------------------------------------------------

The `rest_init` function is defined in the same source code file as `start_kernel` function, and this file is [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c). In the beginning of the `rest_init` we can see call of the two following functions:

```C
	rcu_scheduler_starting();
	smpboot_thread_init();
```
在5.10.13中为：

```c
noinline void __ref rest_init(void) /*  */
{
	struct task_struct *tsk;
	int pid;

	rcu_scheduler_starting();   /* 调度器启动 */
	/*
	 * We need to spawn init first so that it obtains pid 1, however
	 * the init task will end up wanting to create kthreads, which, if
	 * we schedule it before we create kthreadd, will OOPS.
	 *//* 创建内核线程 */
	pid = kernel_thread(kernel_init, NULL, CLONE_FS);/* init/systemd 内核线程 PID=1*/
	/*
	 * Pin init on the boot CPU. Task migration is not properly working
	 * until sched_init_smp() has been run. It will set the allowed
	 * CPUs for init to the non isolated CPUs.
	 */
	rcu_read_lock();
	tsk = find_task_by_pid_ns(pid, &init_pid_ns);
	set_cpus_allowed_ptr(tsk, cpumask_of(smp_processor_id()));
	rcu_read_unlock();

	numa_default_policy();
	pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);    /* kthreadd 内核线程 PID=2 */
	rcu_read_lock();
	kthreadd_task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	/*
	 * Enable might_sleep() and smp_processor_id() checks.
	 * They cannot be enabled earlier because with CONFIG_PREEMPTION=y
	 * kernel_thread() would trigger might_sleep() splats. With
	 * CONFIG_PREEMPT_VOLUNTARY=y the init task might have scheduled
	 * already, but it's stuck on the kthreadd_done completion.
	 */
	system_state = SYSTEM_SCHEDULING;

	complete(&kthreadd_done);   /* kernel_init 中 等待 此处完成 */

	/*
	 * The boot idle thread must execute schedule()
	 * at least once to get things moving:
	 */
	schedule_preempt_disabled();    /*  */
	/* Call into cpu_idle with preempt disabled */
	cpu_startup_entry(CPUHP_ONLINE);
}
```

The first `rcu_scheduler_starting` makes [RCU](http://en.wikipedia.org/wiki/Read-copy-update) scheduler active and the second `smpboot_thread_init` registers the `smpboot_thread_notifier` CPU notifier (more about it you can read in the [CPU hotplug documentation](https://www.kernel.org/doc/Documentation/cpu-hotplug.txt). After this we can see the following calls:

```C
pid = kernel_thread(kernel_init, NULL, CLONE_FS);
pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
```

Here the `kernel_thread` function (defined in the [kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c)) creates new kernel thread. As we can see the `kernel_thread` function takes three arguments:

* Function which will be executed in a new thread;
* Parameter for the `kernel_init` function;
* Flags.

We will not dive into details about `kernel_thread` implementation (we will see it in the chapter which describe scheduler, just need to say that `kernel_thread` invokes [clone](http://www.tutorialspoint.com/unix_system_calls/clone.htm)). 

Now we only need to know that we create new kernel thread with `kernel_thread` function, parent and child of the thread will use shared information about filesystem and it will start to execute `kernel_init` function. 

A kernel thread differs from a user thread that it runs in kernel mode. So with these two `kernel_thread` calls we create two new kernel threads with the 

* `PID = 1` for `init` process, 在CentOS中是 systemd线程；
* `PID = 2` for `kthreadd`. 

We already know what is `init` process. Let's look on the `kthreadd`. It is a special kernel thread which manages and helps different parts of the kernel to create another kernel thread. We can see it in the output of the `ps` util:

```C
[rongtao@localhost src]$ ps -ef | grep -e kthread -e systemd
root          1      0  0 3月02 ?       00:05:39 systemd --switched-root --system --deserialize 21
root          2      0  0 3月02 ?       00:00:00 [kthreadd]
```

Let's postpone `kernel_init` and `kthreadd` for now and go ahead in the `rest_init`. In the next step after we have created two new kernel threads we can see the following code:

```C
	rcu_read_lock();
	kthreadd_task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();
```
    
The first `rcu_read_lock` function marks the beginning of an [RCU](http://en.wikipedia.org/wiki/Read-copy-update) read-side critical section and the `rcu_read_unlock` marks the end of an RCU read-side critical section. We call these functions because we need to protect the `find_task_by_pid_ns`. 

The `find_task_by_pid_ns` returns pointer to the `task_struct` by the given pid. So, here we are getting the pointer to the `task_struct` for `PID = 2` (we got it after `kthreadd` creation with the `kernel_thread`). In the next step we call `complete` function

```C
complete(&kthreadd_done);
```

and pass address of the `kthreadd_done`. The `kthreadd_done` defined as

```C
static __initdata DECLARE_COMPLETION(kthreadd_done);
```

where `DECLARE_COMPLETION` macro defined as:

```C
#define DECLARE_COMPLETION(work) \
         struct completion work = COMPLETION_INITIALIZER(work)
```

and expands to the definition of the `completion` structure. This structure is defined in the [include/linux/completion.h](https://github.com/torvalds/linux/blob/master/include/linux/completion.h) and presents `completions` concept. 

```c
/*
 * struct completion - structure used to maintain state for a "completion"
 *
 * This is the opaque structure used to maintain the state for a "completion".
 * Completions currently use a FIFO to queue threads that have to wait for
 * the "completion" event.
 *
 * See also:  complete(), wait_for_completion() (and friends _timeout,
 * _interruptible, _interruptible_timeout, and _killable), init_completion(),
 * reinit_completion(), and macros DECLARE_COMPLETION(),
 * DECLARE_COMPLETION_ONSTACK().
 */
struct completion { /*  */
	unsigned int done;
	struct swait_queue_head wait;
};
```

Completions is a code synchronization mechanism which provides race-free solution for the threads that must wait for some process to have reached a point or a specific state. 

Using completions consists of three parts: 

1. The first is definition of the `complete` structure and we did it with the `DECLARE_COMPLETION`. 
2. The second is call of the `wait_for_completion`. 
3. After the call of this function, a thread which called it will not continue to execute and will wait while other thread did not call `complete` function. 

Note that we call `wait_for_completion` with the `kthreadd_done` in the beginning of the `kernel_init_freeable`:

```C
wait_for_completion(&kthreadd_done);
```

And the last step is to call `complete` function as we saw it above. After this the `kernel_init_freeable` function will not be executed while `kthreadd` thread will not be set. After the `kthreadd` was set, we can see three following functions in the `rest_init`:

```C
	init_idle_bootup_task(current);
	schedule_preempt_disabled();
    cpu_startup_entry(CPUHP_ONLINE);
```

> 5.10.13中没有 init_idle_bootup_task。

The first `init_idle_bootup_task` function from the [kernel/sched/core.c](https://github.com/torvalds/linux/blob/master/kernel/sched/core.c) sets the Scheduling class for the current process (`idle` class in our case):

```C
void init_idle_bootup_task(struct task_struct *idle)
{
         idle->sched_class = &idle_sched_class;
}
```

where `idle` class is a low task priority and tasks can be run only when the processor doesn't have anything to run besides this tasks. 

The second function `schedule_preempt_disabled` disables preempt in `idle` tasks. 

```c
/**
 * schedule_preempt_disabled - called with preemption disabled
 *
 * Returns with preemption disabled. Note: preempt_count must be 1
 */
void __sched schedule_preempt_disabled(void)
{
	sched_preempt_enable_no_resched();
	schedule();
	preempt_disable();
}
```

And the third function `cpu_startup_entry` is defined in the [kernel/sched/idle.c](https://github.com/torvalds/linux/blob/master/sched/idle.c) and calls `cpu_idle_loop` from the [kernel/sched/idle.c](https://github.com/torvalds/linux/blob/master/sched/idle.c). 

在5.10.13中，该函数为：
```c
void cpu_startup_entry(enum cpuhp_state state)
{
	arch_cpu_idle_prepare();
	cpuhp_online_idle(state);
	while (1)
		do_idle();
}
```

The `cpu_idle_loop` function works as process with `PID = 0` and works in the background. Main purpose of the `cpu_idle_loop` is to consume the idle CPU cycles. 

When there is no process to run, this process starts to work. 

We have one process with `idle` scheduling class (we just set the `current` task to the `idle` with the call of the `init_idle_bootup_task` function), so the `idle` thread does not do useful work but just checks if there is an active task to switch to: 

```C
static void cpu_idle_loop(void)
{
    ...
    ...
    ...
    while (1) {
        while (!need_resched()) {
        ...
        ...
        ...
        }
        ...
    }
```
在5.10.13中对应的是：

```c
/*
 * Generic idle loop implementation
 *
 * Called with polling cleared.
 */
static void do_idle(void)
{
	int cpu = smp_processor_id();
	/*
	 * If the arch has a polling bit, we maintain an invariant:
	 *
	 * Our polling bit is clear if we're not scheduled (i.e. if rq->curr !=
	 * rq->idle). This means that, if rq->idle has the polling bit set,
	 * then setting need_resched is guaranteed to cause the CPU to
	 * reschedule.
	 */

	__current_set_polling();
	tick_nohz_idle_enter();

	while (!need_resched()) {
		rmb();

		local_irq_disable();

		if (cpu_is_offline(cpu)) {
			tick_nohz_idle_stop_tick();
			cpuhp_report_idle_dead();
			arch_cpu_idle_dead();
		}

		arch_cpu_idle_enter();

		/*
		 * In poll mode we reenable interrupts and spin. Also if we
		 * detected in the wakeup from idle path that the tick
		 * broadcast device expired for us, we don't want to go deep
		 * idle as we know that the IPI is going to arrive right away.
		 */
		if (cpu_idle_force_poll || tick_check_broadcast_expired()) {
			tick_nohz_idle_restart_tick();
			cpu_idle_poll();
		} else {
			cpuidle_idle_call();
		}
		arch_cpu_idle_exit();
	}

	/*
	 * Since we fell out of the loop above, we know TIF_NEED_RESCHED must
	 * be set, propagate it into PREEMPT_NEED_RESCHED.
	 *
	 * This is required because for polling idle loops we will not have had
	 * an IPI to fold the state for us.
	 */
	preempt_set_need_resched();
	tick_nohz_idle_exit();
	__current_clr_polling();

	/*
	 * We promise to call sched_ttwu_pending() and reschedule if
	 * need_resched() is set while polling is set. That means that clearing
	 * polling needs to be visible before doing these things.
	 */
	smp_mb__after_atomic();

	/*
	 * RCU relies on this call to be done outside of an RCU read-side
	 * critical section.
	 */
	flush_smp_call_function_from_idle();
	schedule_idle();

	if (unlikely(klp_patch_pending(current)))
		klp_update_patch_state(current);
}
```

More about it will be in the chapter about scheduler. So for this moment the `start_kernel` calls the `rest_init` function which spawns an `init` (`kernel_init` function) process and become `idle` process itself. 


Now is time to look on the `kernel_init`. Execution of the `kernel_init` function starts from the call of the `kernel_init_freeable` function. The `kernel_init_freeable` function first of all waits for the completion of the `kthreadd` setup. I already wrote about it above:

```C
wait_for_completion(&kthreadd_done);
```

After this we set `gfp_allowed_mask` to `__GFP_BITS_MASK` which means that system is already running, 
```c
	/* Now the scheduler is fully set up and can do blocking allocations */
    //>>>>>>means that system is already running<<<<<<<<
	gfp_allowed_mask = __GFP_BITS_MASK;
```

set allowed [cpus/mems](https://www.kernel.org/doc/Documentation/cgroups/cpusets.txt) to all CPUs and [NUMA](http://en.wikipedia.org/wiki/Non-uniform_memory_access) nodes with the `set_mems_allowed` function, 

allow `init` process to run on any CPU with the `set_cpus_allowed_ptr`, 

```c
	/*
	 * init can allocate pages on any node
	 *
	 * allow `init` process to run on any CPU with the `set_cpus_allowed_ptr`
	 */
	set_mems_allowed(node_states[N_MEMORY]);
```

set pid for the `cad` or `Ctrl-Alt-Delete`, do preparation for booting of the other CPUs with the call of the `smp_prepare_cpus`, call early [initcalls](http://kernelnewbies.org/Documents/InitcallMechanism) with the `do_pre_smp_initcalls`, initialize `SMP` with the `smp_init` and initialize [lockup_detector](https://www.kernel.org/doc/Documentation/lockup-watchdogs.txt) with the call of the `lockup_detector_init` and initialize scheduler with the `sched_init_smp`.

After this we can see the call of the following functions - `do_basic_setup`. Before we will call the `do_basic_setup` function, our kernel already initialized for this moment. As comment says:

```
Now we can finally start doing some real work..
```
do_basic_setup函数定义：
```c
/*
 * Ok, the machine is now initialized. None of the devices
 * have been touched yet, but the CPU subsystem is up and
 * running, and memory and process management works.
 *
 * Now we can finally start doing some real work..
 */
static void __init do_basic_setup(void)
{
	cpuset_init_smp();  /* reinitialize [cpuset] */
	driver_init();      /*  */
	init_irq_proc();    /*  */
	do_ctors();         /*  */
	usermodehelper_enable();    /*  */
	do_initcalls();/* xxx_initcall() */
}
```
The `do_basic_setup` will reinitialize [cpuset](https://www.kernel.org/doc/Documentation/cgroups/cpusets.txt) to the active CPUs, initialize the `khelper` - which is a kernel thread which used for making calls out to userspace from within the kernel, initialize [tmpfs](http://en.wikipedia.org/wiki/Tmpfs), initialize `drivers` subsystem, enable the user-mode helper `workqueue`  and make post-early call of the `initcalls`. 

```c
static initcall_entry_t __initdata*initcall_levels[]  = {
	__initcall0_start,
	__initcall1_start,
	__initcall2_start,
	__initcall3_start,
	__initcall4_start,
	__initcall5_start,
	__initcall6_start,
	__initcall7_start,
	__initcall_end,
};
static void __init do_initcalls(void)
{
	int level;
	size_t len = strlen(saved_command_line) + 1;
	char *command_line;

	command_line = kzalloc(len, GFP_KERNEL);
	if (!command_line)
		panic("%s: Failed to allocate %zu bytes\n", __func__, len);

	for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++) {
		/* Parser modifies command_line, restore it each time */
		strcpy(command_line, saved_command_line);
		do_initcall_level(level, command_line);
	}

	kfree(command_line);
}
```

We can see opening of the `dev/console` and dup twice file descriptors from `0` to `2` after the `do_basic_setup`:


```C
if (sys_open((const char __user *) "/dev/console", O_RDWR, 0) < 0)
	pr_err("Warning: unable to open an initial console.\n");

(void) sys_dup(0);
(void) sys_dup(0);
```
5.10.13中是：
```c
/* Open /dev/console, for stdin/stdout/stderr, this should never fail 
    opening of the `dev/console` and dup twice file descriptors from `0` to `2` */
void __init console_on_rootfs(void)
{
	struct file *file = filp_open("/dev/console", O_RDWR, 0);

	if (IS_ERR(file)) {
		pr_err("Warning: unable to open an initial console.\n");
		return;
	}
	init_dup(file);
	init_dup(file);
	init_dup(file);
	fput(file);
}
```
We are using two system calls here `sys_open` and `sys_dup`. In the next chapters we will see explanation and implementation of the different system calls. After we opened initial console, we check that `rdinit=` option was passed to the kernel command line or set default path of the ramdisk:

```C
if (!ramdisk_execute_command)
	ramdisk_execute_command = "/init";
```
同时：
```c
static int __init rdinit_setup(char *str)
{
	unsigned int i;

	ramdisk_execute_command = str;
	/* See "auto" comment in init_setup */
	for (i = 1; i < MAX_INIT_ARGS; i++)
		argv_init[i] = NULL;
	return 1;
}
__setup("rdinit=", rdinit_setup);
```
Check user's permissions for the `ramdisk` and call the `prepare_namespace` function from the [init/do_mounts.c](https://github.com/torvalds/linux/blob/master/init/do_mounts.c) which checks and mounts the [initrd](http://en.wikipedia.org/wiki/Initrd):

```C
if (sys_access((const char __user *) ramdisk_execute_command, 0) != 0) {
	ramdisk_execute_command = NULL;
	prepare_namespace();
}
```

This is the end of the `kernel_init_freeable` function and we need return to the `kernel_init`.

The next step after the `kernel_init_freeable` finished its execution is the `async_synchronize_full`. This function waits until all asynchronous function calls have been done 。


and after it we will call the `free_initmem` which will release all memory occupied by the initialization stuff which located between `__init_begin` and `__init_end`. After this we protect `.rodata` with the `mark_rodata_ro` and update state of the system from the `SYSTEM_BOOTING` to the
```c
static void mark_readonly(void)
{
	if (rodata_enabled) {
		/*
		 * load_module() results in W+X mappings, which are cleaned
		 * up with call_rcu().  Let's make sure that queued work is
		 * flushed so that we don't hit false positives looking for
		 * insecure pages which are W+X.
		 */
		rcu_barrier();
		mark_rodata_ro();
		rodata_test();
	} else
		pr_info("Kernel memory protection disabled.\n");
}
```
```C
system_state = SYSTEM_RUNNING;
```

And tries to run the `init` process:

```C
if (ramdisk_execute_command) {
	ret = run_init_process(ramdisk_execute_command);
	if (!ret)
		return 0;
	pr_err("Failed to execute %s (error %d)\n",
	       ramdisk_execute_command, ret);
}
```

First of all it checks the `ramdisk_execute_command` which we set in the `kernel_init_freeable` function and it will be equal to the value of the `rdinit=` kernel command line parameters or `/init` by default. The `run_init_process` function fills the first element of the `argv_init` array:

```C
static const char *argv_init[MAX_INIT_ARGS+2] = { "init", NULL, };
```

which represents arguments of the `init` program and call `do_execve` function:

```C
argv_init[0] = init_filename;
return do_execve(getname_kernel(init_filename),
	(const char __user *const __user *)argv_init,
	(const char __user *const __user *)envp_init);
```

```c
static int run_init_process(const char *init_filename)
{
	const char *const *p;

	argv_init[0] = init_filename;
	pr_info("Run %s as init process\n", init_filename);
	pr_debug("  with arguments:\n");
	for (p = argv_init; *p; p++)
		pr_debug("    %s\n", *p);
	pr_debug("  with environment:\n");
	for (p = envp_init; *p; p++)
		pr_debug("    %s\n", *p);
	return kernel_execve(init_filename, argv_init, envp_init);
}
```

The `do_execve` function is defined in the [include/linux/sched.h](https://github.com/torvalds/linux/blob/master/include/linux/sched.h) and runs program with the given file name and arguments. If we did not pass `rdinit=` option to the kernel command line, kernel starts to check the `execute_command` which is equal to value of the `init=` kernel command line parameter:

```C
	if (execute_command) {
		ret = run_init_process(execute_command);
		if (!ret)
			return 0;
		panic("Requested init %s failed (error %d).",
		      execute_command, ret);
	}
```

If we did not pass `init=` kernel command line parameter either, kernel tries to run one of the following executable files: 

```C
//If we did not pass `init=` kernel command line parameter either, 
//kernel tries to run one of the following executable files
//    
//[rongtao@localhost src]$ ll /sbin/init
//lrwxrwxrwx 1 root root 22 1月  28 11:18 /sbin/init -> ../lib/systemd/systemd
if (!try_to_run_init_process("/sbin/init") ||
    !try_to_run_init_process("/etc/init") ||
    !try_to_run_init_process("/bin/init") ||
    !try_to_run_init_process("/bin/sh"))
	return 0;
```

Otherwise we finish with [panic](http://en.wikipedia.org/wiki/Kernel_panic):

```C
panic("No working init found.  Try passing init= option to kernel. "
      "See Linux Documentation/init.txt for guidance.");
```

That's all! Linux kernel initialization process is finished!

Conclusion
--------------------------------------------------------------------------------

It is the end of the tenth part about the linux kernel [initialization process](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/index.html). It is not only the `tenth` part, but also is the last part which describes initialization of the linux kernel. As I wrote in the first [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-1.html) of this chapter, we will go through all steps of the kernel initialization and we did it. We started at the first architecture-independent function - `start_kernel` and finished with the launch of the first `init` process in the our system. I skipped details about different subsystem of the kernel, for example I almost did not cover scheduler, interrupts, exception handling, etc. From the next part we will start to dive to the different kernel subsystems. Hope it will be interesting.

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/MintCN/linux-insides-zh).**

Links
--------------------------------------------------------------------------------

* [SLAB](http://en.wikipedia.org/wiki/Slab_allocation)
* [xsave](http://www.felixcloutier.com/x86/XSAVES.html)
* [FPU](http://en.wikipedia.org/wiki/Floating-point_unit)
* [Documentation/security/credentials.txt](https://github.com/torvalds/linux/blob/master/Documentation/security/credentials.txt)
* [Documentation/x86/x86_64/mm](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt)
* [RCU](http://en.wikipedia.org/wiki/Read-copy-update)
* [VFS](http://en.wikipedia.org/wiki/Virtual_file_system)
* [inode](http://en.wikipedia.org/wiki/Inode)
* [proc](http://en.wikipedia.org/wiki/Procfs)
* [man proc](http://linux.die.net/man/5/proc)
* [Sysctl](http://en.wikipedia.org/wiki/Sysctl)
* [ftrace](https://www.kernel.org/doc/Documentation/trace/ftrace.txt)
* [cgroup](http://en.wikipedia.org/wiki/Cgroups)
* [CPU hotplug documentation](https://www.kernel.org/doc/Documentation/cpu-hotplug.txt)
* [completions - wait for completion handling](https://www.kernel.org/doc/Documentation/scheduler/completion.txt)
* [NUMA](http://en.wikipedia.org/wiki/Non-uniform_memory_access)
* [cpus/mems](https://www.kernel.org/doc/Documentation/cgroups/cpusets.txt)
* [initcalls](http://kernelnewbies.org/Documents/InitcallMechanism)
* [Tmpfs](http://en.wikipedia.org/wiki/Tmpfs)
* [initrd](http://en.wikipedia.org/wiki/Initrd)
* [panic](http://en.wikipedia.org/wiki/Kernel_panic)
* [Previous part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-9.html)
