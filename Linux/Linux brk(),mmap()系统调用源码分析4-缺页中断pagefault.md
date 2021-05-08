<center><font size='6'>Linux brk(),mmap()系统调用源码分析</font></center>
<center><font size='6'>缺页中断pagefault</font></center>
<br/>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2021年5月6日</font></center>
<br/>


* 内核版本：linux-5.10.13
* 注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)

```c
#define X86_TRAP_PF		14	/* 14, 页错误 *//* Page Fault */    /* 页 fault */
                            /**
                             *  X86_TRAP_PF
                             *      exc_page_fault  : fault.c arch\x86\mm 
                             *      handle_page_fault()
                             */
```
函数调用关系：
```c
exc_page_fault ~ do_page_fault
    handle_page_fault
        do_kern_addr_fault
            spurious_kernel_fault
            kprobe_page_fault
            bad_area_nosemaphore
        do_user_addr_fault
            is_vsyscall_vaddr
                emulate_vsyscall
            find_vma
            handle_mm_fault
                hugetlb_fault
                    huge_pte_alloc
                    hugetlb_no_page
                    pte_page
                    get_page
                __handle_mm_fault
                    p4d_alloc
                    pud_alloc
                    pmd_alloc
                    handle_pte_fault
                        do_anonymous_page
                            pte_alloc
                            alloc_zeroed_user_highpage_movable
                        do_fault
                            do_read_fault
                                __do_fault
                            do_cow_fault
                                alloc_page_vma
                                __do_fault
                                copy_user_highpage
                            do_shared_fault
                                __do_fault
                        do_swap_page
                            alloc_page_vma
            fault_signal_pending
            check_v8086_mode
```

# 1. 基础部分

在较早版本的缺页处理函数为`do_page_fault`，在5.10.13中为`exc_page_fault`。声明如下：

```c
DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)
```
首先从cr2寄存器获取发生缺页的地址：

```c
unsigned long address = read_cr2();
```
然后预取内存mm的读写信号量
```c
prefetchw(&current->mm->mmap_lock);
```

略过kvm和trace相关代码，然后，将执行`handle_page_fault`函数。


# 2. handle_page_fault

根据地址区分缺页发生在用户态还是内核态，如下：

```c
bool fault_in_kernel_space(unsigned long address)
{
	/*
	 * On 64-bit systems, the vsyscall page is at an address above
	 * TASK_SIZE_MAX, but is not considered part of the kernel
	 * address space.
	 */
	if (IS_ENABLED(CONFIG_X86_64) && is_vsyscall_vaddr(address))
		return false;

    /*
     * 五级页表时 = 0x00fffffffffff000
     * 四级页表时 = 0x00007ffffffff000
     */
	return address >= TASK_SIZE_MAX;
}
```

然后分别选择执行`do_kern_addr_fault`还是`do_user_addr_fault`，内核中的缺页是比较少见的。

# 3. do_kern_addr_fault

内核几乎不会出错，没错，就是这么自信。

# 4. do_user_addr_fault


































# 5. 相关链接

* [https://www.cs.unc.edu/~porter/courses/cse506/f12/slides/address-spaces.pdf](https://www.cs.unc.edu/~porter/courses/cse506/f12/slides/address-spaces.pdf)
* [https://stackoverflow.com/questions/14943990/overlapping-pages-with-mmap-map-fixed](https://stackoverflow.com/questions/14943990/overlapping-pages-with-mmap-map-fixed)
* [《Linux内存管理 brk(),mmap()系统调用源码分析1：基础部分》](https://rtoax.blog.csdn.net/article/details/116306341)
* [《Linux内存管理 brk(),mmap()系统调用源码分析2：brk()的内存释放流程》](https://rtoax.blog.csdn.net/article/details/116306499)
* [内核实现mmap的关键点-get_unmapped_area](http://blog.chinaunix.net/uid-26902809-id-5587837.html)
* [mmap随机化](http://rk700.github.io/2016/11/22/mmap-aslr/)
* [Meltdown(熔断漏洞)- Reading Kernel Memory from User Space/KASLR | 原文+中文翻译](https://rtoax.blog.csdn.net/article/details/114794325)













