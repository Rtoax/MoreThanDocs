<center><font size='6'>Linux brk(),mmap()系统调用源码分析</font></center>
<center><font size='6'>brk()的内存申请流程</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2021年4月30日</font></center>
<br/>


* 内核版本：linux-5.10.13
* 注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)


# 1. 基础部分

在之前文章中已经介绍了基础部分 [《Linux内存管理 brk(),mmap()系统调用源码分析1：基础部分》](https://rtoax.blog.csdn.net/article/details/116306341)，本文介绍brk的释放部分。

# 2. brk内存释放

在之前文章中已经介绍了brk内存释放过程[《Linux内存管理 brk(),mmap()系统调用源码分析2：brk()的内存释放流程》](https://rtoax.blog.csdn.net/article/details/116306499)

# 3. brk内存申请

本文介绍申请流程。如果新的 brk 位置高于 旧的 brk 位置，首先会查找旧brk所在的vma的下一个vma结构：

```c
	next = find_vma(mm, oldbrk);
	if (next && newbrk + PAGE_SIZE > vm_start_gap(next))
		goto out;
```
如果下一个vma结构存在，并且新的brk+pagesize落在vma上，那么说明现在的brk满足要求，直接返回就行了，如果不是，就迎来了`do_brk_flags`。

# 4. do_brk_flags

函数原型为：
```c
static int do_brk_flags(unsigned long addr, unsigned long len, unsigned long flags, struct list_head *uf)
```

入参分别为：起始地址，长度，标志。

函数是这么调用的`do_brk_flags(oldbrk, newbrk-oldbrk, 0, &uf)`，该函数基本上是释放流程的逆向操作，这里只就几个核心的函数进行讲解，第一个`get_unmapped_area`。

## 4.1. get_unmapped_area



brk系统调用肯定不是文件，所以file=NULL，
```c
get_unmapped_area(NULL, addr, len, 0, MAP_FIXED);
```

> `MAP_FIXED`：**准确解释地址，如果addr和len指定的内存区域与任何现有映射的页面重叠，则现有映射的重叠部分将被丢弃.**



首先调用`arch_mmap_check`，在x86下为0。接下来获取未映射区域，这区分了mmap类型：

```c
	get_area = current->mm->get_unmapped_area;
	if (file) { /* 如果是文件映射 */
		if (file->f_op->get_unmapped_area)
			get_area = file->f_op->get_unmapped_area;
	} else if (flags & MAP_SHARED) {    /* 如果是共享的映射 */
		/*
		 * mmap_region() will call shmem_zero_setup() to create a file,
		 * so use shmem's get_unmapped_area in case it can be huge.
		 * do_mmap() will clear pgoff, so match alignment.
		 */
		pgoff = 0;
		get_area = shmem_get_unmapped_area; /* 共享 */
	}
```
首先从mm结构中获取了`get_unmapped_area`函数指针，这个指针牛的一批，在`arch\x86\kernel\sys_x86_64.c`里，通过函数指针调用`addr = get_area(file, addr, len, pgoff, flags);`。



## 4.2. arch_get_unmapped_area

该结构是在`arch_pick_mmap_layout`函数中被赋予`get_unmapped_area`指针的，如下：

```c
void arch_pick_mmap_layout(struct mm_struct *mm, struct rlimit *rlim_stack)
{
	if (mmap_is_legacy())
		mm->get_unmapped_area = arch_get_unmapped_area;
	else
		mm->get_unmapped_area = arch_get_unmapped_area_topdown;
	...
```

[https://www.kernel.org/doc/gorman/html/understand/understand021.html#func:%20arch_get_unmapped_area](https://www.kernel.org/doc/gorman/html/understand/understand021.html#func:%20arch_get_unmapped_area)

函数不长，但是操作很骚。先看参数：

```c
do_brk_flags(oldbrk, newbrk-oldbrk, 0, &uf)
    get_unmapped_area(NULL, addr, len, 0, MAP_FIXED);
        get_area(file, addr, len, pgoff, flags); -> arch_get_unmapped_area
            arch_get_unmapped_area
                find_start_end
                    get_mmap_base
                vm_unmapped_area
                    unmapped_area
```

* file=NULL;
* addr=oldbrk;
* len=newbrk-oldbrk;
* pgoff=0;
* flags=MAP_FIXED;(准确解释地址，如果addr和len指定的内存区域与任何现有映射的页面重叠，则现有映射的重叠部分将被丢弃.)

使用`find_start_end`获取begin和end：
```c
static void find_start_end(unsigned long addr, unsigned long flags,
		unsigned long *begin, unsigned long *end)
{
	if (!in_32bit_syscall() && (flags & MAP_32BIT)) {   /* 32 位 */
		/* This is usually used needed to map code in small
		   model, so it needs to be in the first 31bit. Limit
		   it to that.  This means we need to move the
		   unmapped base down for this case. This can give
		   conflicts with the heap, but we assume that glibc
		   malloc knows how to fall back to mmap. Give it 1GB
		   of playground for now. -AK */
		*begin = 0x40000000;
		*end = 0x80000000;
		if (current->flags & PF_RANDOMIZE) {
			*begin = randomize_page(*begin, 0x02000000);
		}
		return;
	}

	*begin	= get_mmap_base(1); /*  */
	if (in_32bit_syscall())
		*end = task_size_32bit();
	else
		*end = task_size_64bit(addr > DEFAULT_MAP_WINDOW);
}
```
首先判断如果不是32bit系统调用`!in_32bit_syscall()`并且设置了标记位`(flags & MAP_32BIT)`，之类不成立，因为flags值为`MAP_FIXED`，那么接下来会执行`*begin	= get_mmap_base(1);`。这个函数`get_mmap_base`直接返回`is_legacy ? mm->mmap_legacy_base : mm->mmap_base;`也就是` mm->mmap_legacy_base`，这个值等于几？他是在`arch_pick_mmap_base`设置的，在文章[mmap随机化](http://rk700.github.io/2016/11/22/mmap-aslr/)中有解释，也就是是否将mmap随机化，这是在一个漏洞的解决方法，此处不所解释，感兴趣可以参考一篇论文《[Meltdown(熔断漏洞)- Reading Kernel Memory from User Space/KASLR | 原文+中文翻译](https://rtoax.blog.csdn.net/article/details/114794325)》。
接着，调用`task_size_64bit`获取end地址。


然后判断长度：

```c
	if (len > end)
		return -ENOMEM;
```
如果已存在，直接返回：
```c
	if (addr) {
		addr = PAGE_ALIGN(addr);    /* 对齐 */
		vma = find_vma(mm, addr);   /* 查找对应 vma */
		if (end - len >= addr &&
		    (!vma || addr + len <= vm_start_gap(vma)))
			return addr;
	}
```

接着是对数据结构`vm_unmapped_area_info`的填充

```c
struct vm_unmapped_area_info {  /*  */
#define VM_UNMAPPED_AREA_TOPDOWN 1
	unsigned long flags;
	unsigned long length;
	unsigned long low_limit;
	unsigned long high_limit;
	unsigned long align_mask;
	unsigned long align_offset;
};
```
它是这么填充的：

```c
	info.flags = 0;
	info.length = len;
	info.low_limit = begin;
	info.high_limit = end;
	info.align_mask = 0;
	info.align_offset = pgoff << PAGE_SHIFT;
	if (filp) {
		info.align_mask = get_align_mask();
		info.align_offset += get_align_bits();
	}
```
接着调用`vm_unmapped_area`，其调用`unmapped_area`(`flags=0`)




## 4.3. unmapped_area

这里的入参为：

* file=NULL;
* addr=oldbrk;
* len=newbrk-oldbrk;
* pgoff=0;
* flags=MAP_FIXED;

他的操作在函数注释中给出：

```c
/*
 * We implement the search by looking for an rbtree node that
 * immediately follows a suitable gap. That is,
 * - gap_start = vma->vm_prev->vm_end <= info->high_limit - length;
 * - gap_end   = vma->vm_start        >= info->low_limit  + length;
 * - gap_end - gap_start >= length
 */
```

接着`get_unmapped_area`返回，并进行合法性判断：

```c
	mapped_addr = get_unmapped_area(NULL, addr, len, 0, MAP_FIXED);
	if (IS_ERR_VALUE(mapped_addr))  /* unlikely */
		return mapped_addr;
```

## 4.4. munmap_vma_range

该函数的注释为

> munmap VMAs that overlap a range.
> /* Clear old maps, set up prev, rb_link, rb_parent, and uf */

在这，我发现一个问题，`find_vma_links`函数永远不会返回真值，那么此处的`while`的作用是什么呢？
```c
static inline int
munmap_vma_range(struct mm_struct *mm, unsigned long start, unsigned long len,
		 struct vm_area_struct **pprev, struct rb_node ***link,
		 struct rb_node **parent, struct list_head *uf)
{
    /*  */
	while (find_vma_links(mm, start, start + len, pprev, link, parent))
		if (do_munmap(mm, start, len, uf))
			return -ENOMEM;

	return 0;
}
```

这里具体关于mm的操作可以参考函数`copy_mm`、`dup_mm`、`vm_area_dup`。


## 4.5. may_expand_vm

```c
/*
 * Return true if the calling process may expand its vm space by the passed
 * number of pages
 */
bool may_expand_vm(struct mm_struct *mm, vm_flags_t flags, unsigned long npages)
{
    /* 检查映射的页数有没有超限 */
	if (mm->total_vm + npages > rlimit(RLIMIT_AS) >> PAGE_SHIFT)
		return false;

    /* 数据 mapping 
        1.在 brk系统调用传入的是0，此代码不执行*/
	if (is_data_mapping(flags) &&
	    mm->data_vm + npages > rlimit(RLIMIT_DATA) >> PAGE_SHIFT) {
		/* Workaround for Valgrind */
		if (rlimit(RLIMIT_DATA) == 0 &&
		    mm->data_vm + npages <= rlimit_max(RLIMIT_DATA) >> PAGE_SHIFT)
			return true;

		pr_warn_once("%s (%d): VmData %lu exceed data ulimit %lu. Update limits%s.\n",
			     current->comm, current->pid,
			     (mm->data_vm + npages) << PAGE_SHIFT,
			     rlimit(RLIMIT_DATA),
			     ignore_rlimit_data ? "" : " or use boot option ignore_rlimit_data");

		if (!ignore_rlimit_data)
			return false;
	}

	return true;
}
```

接下来检查系统配置，是否映射数量超限：
```c
    /* 检查sysctl */
	if (mm->map_count > sysctl_max_map_count)
		return -ENOMEM;
```

## 4.6. vma_merge

> brk 此处不对其进行讲解，将在mprotect系统调用中讲解。

## 4.7. vma_link

接下来，分配新的vma结构，并且填充响应的数据，并将vma添加至mm结构的链表和红黑树中：

```c
	/*
	 * create a vma struct for an anonymous mapping
	 */
	vma = vm_area_alloc(mm);    /* 分配这个结构 */
	if (!vma) {
		vm_unacct_memory(len >> PAGE_SHIFT);
		return -ENOMEM;
	}

	vma_set_anonymous(vma);     /* 匿名vma */
	vma->vm_start = addr;       /* start */
	vma->vm_end = addr + len;   /* end */
	vma->vm_pgoff = pgoff;      /* 页内偏移 */
	vma->vm_flags = flags;      /* 标志 */
	vma->vm_page_prot = vm_get_page_prot(flags);    /* VMA 的权限 */
	vma_link(mm, vma, prev, rb_link, rb_parent);    /* 插入 */
```

其中vm_link函数：
```c
static void vma_link(struct mm_struct *mm, struct vm_area_struct *vma,
			struct vm_area_struct *prev, struct rb_node **rb_link,
			struct rb_node *rb_parent)
{
	struct address_space *mapping = NULL;

	if (vma->vm_file) { /* 文件映射 */
		mapping = vma->vm_file->f_mapping;
		i_mmap_lock_write(mapping);
	}

	__vma_link(mm, vma, prev, rb_link, rb_parent);  /* 添加至链表和红黑树 */
	__vma_link_file(vma);   /* 文件映射的话，更新缓存 */

	if (mapping)
		i_mmap_unlock_write(mapping);

	mm->map_count++;    /* 映射计数++ */
	validate_mm(mm);    /*  */
}
```
这里的`validate_mm`在本文中不做过多讲解，将在后续文章中详细解说。

## 4.8. perf_event_mmap

> brk 此处不对其进行讲解，将在手续文章中进行讲解。

然后，对mm结构字段进行更新：

```c
	mm->total_vm += len >> PAGE_SHIFT;  /* 共映射的页数计数 */
	mm->data_vm += len >> PAGE_SHIFT;   /* 数据映射计数 */
	if (flags & VM_LOCKED)
		mm->locked_vm += (len >> PAGE_SHIFT);   /* 锁定的页面计数 */
	vma->vm_flags |= VM_SOFTDIRTY;
	return 0;
```
至此，`do_brk_flags`就返回了。接着，更新brk位置：
```c
mm->brk = brk;
```

## 4.9. mm_populate

> brk 此处不对其进行讲解，将在手续文章中进行讲解。


至此brk系统调用就返回至用户态程序。


# 5. 相关链接

* [https://www.cs.unc.edu/~porter/courses/cse506/f12/slides/address-spaces.pdf](https://www.cs.unc.edu/~porter/courses/cse506/f12/slides/address-spaces.pdf)
* [https://stackoverflow.com/questions/14943990/overlapping-pages-with-mmap-map-fixed](https://stackoverflow.com/questions/14943990/overlapping-pages-with-mmap-map-fixed)
* [《Linux内存管理 brk(),mmap()系统调用源码分析1：基础部分》](https://rtoax.blog.csdn.net/article/details/116306341)
* [《Linux内存管理 brk(),mmap()系统调用源码分析2：brk()的内存释放流程》](https://rtoax.blog.csdn.net/article/details/116306499)
* [内核实现mmap的关键点-get_unmapped_area](http://blog.chinaunix.net/uid-26902809-id-5587837.html)
* [mmap随机化](http://rk700.github.io/2016/11/22/mmap-aslr/)
* [Meltdown(熔断漏洞)- Reading Kernel Memory from User Space/KASLR | 原文+中文翻译](https://rtoax.blog.csdn.net/article/details/114794325)

