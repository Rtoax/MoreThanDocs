<center><font size='6'>Linux brk(),mmap()系统调用源码分析</font></center>
<center><font size='6'>mmap()基础</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2021年5月</font></center>
<br/>


* 内核版本：linux-5.10.13
* 注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)

# 1. 基础

## 1.1. libc函数声明

* libc

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags,
          int fd, off_t offset);
int munmap(void *addr, size_t length);
```


### 1.1.1. prot取值

* PROT_EXEC  Pages may be executed.
* PROT_READ  Pages may be read.
* PROT_WRITE Pages may be written.
* PROT_NONE  Pages may not be accessed.


### 1.1.2. flags取值

* MAP_SHARED：进程间共享，共享内存的一种
* MAP_PRIVATE：进程私有，当malloc一大块内存时，即为该选项
* MAP_32BIT: 映射到用户地址空间的前2G中；
* MAP_ANON
* MAP_ANONYMOUS：初始化为0的匿名映射
* MAP_DENYWRITE：忽略
* MAP_EXECUTABLE：忽略
* MAP_FILE：忽略
* MAP_FIXED：准确解释地址，如果addr和len指定的内存区域与任何现有映射的页面重叠，则现有映射的重叠部分将被丢弃
* MAP_GROWSDOWN：用于栈
* MAP_HUGETLB：大页内存，闻名于DPDK
* MAP_LOCKED：不会被swap出去
* MAP_NONBLOCK：仅与MAP_POPULATE结合使用才有意义，不执行预读：仅为RAM中已经存在的页面创建页表条目。 从Linux 2.6.23开始，此标志使MAP_POPULATE不执行任何操作。
* MAP_NORESERVE：不要为此映射提供swap空间
* MAP_POPULATE：填充页表以进行映射。 对于文件映射，这将导致文件上的预读。 pagefault不会阻止以后对映射的访问。 仅从Linux 2.6.23开始，专用映射才支持MAP_POPULATE。
* MAP_STACK：用于栈
* MAP_UNINITIALIZED：用于提高嵌入式设备的性能

需要注意的是，以上的选项很多，不用每个选项都清楚其原理和使用，就我来说，目前我是用过的只有`MAP_SHARED`，`MAP_PRIVATE`，`MAP_HUGETLB`和`MAP_LOCKED`这些常用的，剩下的我也没做过研究。



## 1.2. 内核入口

* mmap

```c
//arch\x86\kernel\sys_x86_64.c
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, off)
{
	long error;
	error = -EINVAL;
	if (off & ~PAGE_MASK)
		goto out;
	error = ksys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
out:
	return error;
}
```

```c
//mm\mmap.c
SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, pgoff)
{
	return ksys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}
```

* munmap

```c
//mm\mmap.c
SYSCALL_DEFINE2(munmap, unsigned long, addr, size_t, len)
{
	addr = untagged_addr(addr);
	profile_munmap(addr);
	return __vm_munmap(addr, len, true);
}
```







# 2. 参考链接

* [《Linux内存管理 brk(),mmap()系统调用源码分析1：基础部分》](https://rtoax.blog.csdn.net/article/details/116306341)




call_mmap

alloc_file_pseudo：
    aio_private_file
    anon_inode_getfile
        anon_inode_getfd
    hugetlb_file_setup
    create_pipe_files
    __shmem_file_setup
    sock_alloc_file
















