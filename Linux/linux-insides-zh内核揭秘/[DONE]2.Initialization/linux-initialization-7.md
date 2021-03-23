Kernel initialization. Part 7.
================================================================================

The End of the architecture-specific initialization, almost...
================================================================================

This is the seventh part of the Linux Kernel initialization process which covers insides of the `setup_arch` function from the [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/setup.c#L861). As you can know from the previous [parts](http://0xax.gitbooks.io/linux-insides/content/Initialization/index.html), the `setup_arch` function does some architecture-specific (in our case it is [x86_64](http://en.wikipedia.org/wiki/X86-64)) initialization stuff like reserving memory for kernel code/data/bss, early scanning of the [Desktop Management Interface](http://en.wikipedia.org/wiki/Desktop_Management_Interface), early dump of the [PCI](http://en.wikipedia.org/wiki/PCI) device and many many more. If you have read the previous [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html), you can remember that we've finished it at the `setup_real_mode` function.

在5.10.13中，setup_real_mode已经在自动调用里面了：
```c
static int __init init_real_mode(void)
{
	if (!real_mode_header)
		panic("Real mode trampoline was not allocated");

    /**
     *  建立到 [实模式](http://en.wikipedia.org/wiki/Real_mode) 代码的跳板
     */
	setup_real_mode();
	set_real_mode_permissions();

	return 0;
}
early_initcall(init_real_mode);
```
这里的early调用定义为：
```c
/*
 * Early initcalls run before initializing SMP.
 *
 * Only for built-in code, not modules.
 */
#define early_initcall(fn)		__define_initcall(fn, early)

#define __define_initcall(fn, id) ___define_initcall(fn, id, .initcall##id)

#define ___define_initcall(fn, id, __sec)		/* vmlinux.lds.S  */	\
	__ADDRESSABLE(fn)					\
	asm(".section	\"" #__sec ".init\", \"a\"	\n"	\
	"__initcall_" #fn #id ":			\n"	\
	    ".long	" #fn " - .			\n"	\
	    ".previous					\n");
```
可见，early_initcall最终被展开为：
```c
early_initcall-> (fn,early) -> (fn,early,.initcallearly)
```

In the next step, as we set limit of the [memblock](http://xinqiu.gitbooks.io/linux-insides-cn/content/MM/linux-mm-1.html) to the all mapped pages, we can see the call of the `setup_log_buf` function from the [kernel/printk/printk.c](https://github.com/torvalds/linux/blob/master/kernel/printk/printk.c).
```c
/* Allocate bigger log buffer */
	setup_log_buf(1);
```
The `setup_log_buf` function setups kernel cyclic buffer and its length depends on the `CONFIG_LOG_BUF_SHIFT` configuration option. As we can read from the documentation of the `CONFIG_LOG_BUF_SHIFT` it can be between `12` and `21`. In the insides, buffer defined as array of chars:

```C
#define __LOG_BUF_LEN (1 << CONFIG_LOG_BUF_SHIFT)
static char __log_buf[__LOG_BUF_LEN] __aligned(LOG_ALIGN);
static char *log_buf = __log_buf;
```

Now let's look on the implementation of the `setup_log_buf` function. It starts with check that current buffer is empty (It must be empty, because we just setup it) and another check that it is early setup. If setup of the kernel log buffer is not early, we call the `log_buf_add_cpu` function which increase size of the buffer for every CPU:

```C
if (log_buf != __log_buf)
    return;
 
if (!early && !new_log_buf_len)
    log_buf_add_cpu();
```
```c
static void __init log_buf_add_cpu(void)//increase size of the buffer for every CPU
{
	unsigned int cpu_extra;

	/*
	 * archs should set up cpu_possible_bits properly with
	 * set_cpu_possible() after setup_arch() but just in
	 * case lets ensure this is valid.
	 */
	if (num_possible_cpus() == 1)
		return;

	cpu_extra = (num_possible_cpus() - 1) * __LOG_CPU_MAX_BUF_LEN;

	/* by default this will only continue through for large > 64 CPUs */
	if (cpu_extra <= __LOG_BUF_LEN / 2)
		return;

	pr_info("log_buf_len individual max cpu contribution: %d bytes\n",
		__LOG_CPU_MAX_BUF_LEN);
	pr_info("log_buf_len total cpu_extra contributions: %d bytes\n",
		cpu_extra);
	pr_info("log_buf_len min size: %d bytes\n", __LOG_BUF_LEN);

	log_buf_len_update(cpu_extra + __LOG_BUF_LEN);
}
```
We will not research `log_buf_add_cpu` function, because as you can see in the `setup_arch`, we call `setup_log_buf` as:

```C
setup_log_buf(1);
```

where `1` means that it is early setup. In the next step we check `new_log_buf_len` variable which is updated length of the kernel log buffer and allocate new space for the buffer with the `memblock_virt_alloc` （5.10.13中为`memblock_alloc`）function for it, or just return.

As kernel log buffer is ready, the next function is `reserve_initrd`. You can remember that we already called the `early_reserve_initrd` function in the fourth part of the [Kernel initialization](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-4.html). 

```c
/* Linux初始RAM磁盘（initrd）是在系统引导过程中挂载的一个临时根文件系统，用来支持两阶段的引导过程。 
        根文件系统就是通过这方式来进行初始化, 此函数获取RAM DISK的基地址以及大小以及大小加偏移*/
static void __init early_reserve_initrd(void)
{
    /**
     *  //Documentation/x86/zero-page 中有
     * 0C0/004		ALL	ext_ramdisk_image	ramdisk_image high 32bits
     * 0C4/004		ALL	ext_ramdisk_size	ramdisk_size high 32bits
     */
    //所有的这些啊参数都来自于`boot_params`
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();    /* struct setup_header */
	u64 ramdisk_size  = get_ramdisk_size();     /* struct setup_header */
	u64 ramdisk_end   = PAGE_ALIGN(ramdisk_image + ramdisk_size);

    //检查bootloader 提供的ramdisk信息
	if (!boot_params.hdr.type_of_loader ||
	    !ramdisk_image || !ramdisk_size)
		return;		/* No initrd provided by bootloader */

    //保留内存块将ramdisk传输到最终的内存地址
	memblock_reserve(ramdisk_image, ramdisk_end - ramdisk_image);/* 预留 */
}
```

Now, as we reconstructed direct memory mapping in the `init_mem_mapping` function, we need to move [initrd](http://en.wikipedia.org/wiki/Initrd) into directly mapped memory. 

```c
static void __init reserve_initrd(void)
{
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();    /* struct setup_header */
	u64 ramdisk_size  = get_ramdisk_size();     /* struct setup_header */
	u64 ramdisk_end   = PAGE_ALIGN(ramdisk_image + ramdisk_size);

    //检查bootloader 提供的ramdisk信息
	if (!boot_params.hdr.type_of_loader ||
	    !ramdisk_image || !ramdisk_size)
		return;		/* No initrd provided by bootloader */

	initrd_start = 0;

	printk(KERN_INFO "RAMDISK: [mem %#010llx-%#010llx]\n", ramdisk_image,
			ramdisk_end - 1);

	if (pfn_range_is_mapped(PFN_DOWN(ramdisk_image),
				PFN_DOWN(ramdisk_end))) {
		/* All are mapped, easy case */
		initrd_start = ramdisk_image + PAGE_OFFSET;
		initrd_end = initrd_start + ramdisk_size;
		return;
	}

    /* see early_reserve_initrd() */
	relocate_initrd();

    /* 对应将   early_reserve_initrd 中的 memblock_reserve 释放*/
	memblock_free(ramdisk_image, ramdisk_end - ramdisk_image);
}
```
The `reserve_initrd` function starts from the definition of the base address and end address of the `initrd` and check that `initrd` is provided by a bootloader. All the same as what we saw in the `early_reserve_initrd`. But instead of the reserving place in the `memblock` area with the call of the `memblock_reserve` function, we get the mapped size of the direct memory area and check that the size of the `initrd` is not greater than this area with:

```C
mapped_size = memblock_mem_size(max_pfn_mapped);
if (ramdisk_size >= (mapped_size>>1))
    panic("initrd too large to handle, "
	      "disabling initrd (%lld needed, %lld available)\n",
	      ramdisk_size, mapped_size>>1);
```
          
You can see here that we call `memblock_mem_size` function and pass the `max_pfn_mapped` to it, where `max_pfn_mapped` contains the highest direct mapped page frame number. If you do not remember what is `page frame number`, explanation is simple: First `12` bits of the virtual address represent offset in the physical page or page frame. If we right-shift out `12` bits of the virtual address, we'll discard offset part and will get `Page Frame Number`. In the `memblock_mem_size` we go through the all memblock `mem` (not reserved) regions and calculates size of the mapped pages and return it to the `mapped_size` variable (see code above). As we got amount of the direct mapped memory, we check that size of the `initrd` is not greater than mapped pages. If it is greater we just call `panic` which halts the system and prints famous [Kernel panic](http://en.wikipedia.org/wiki/Kernel_panic) message. In the next step we print information about the `initrd` size. We can see the result of this in the `dmesg` output:

```C
sudo dmesg | grep RAM
[    0.000000] RAMDISK: [mem 0x345ae000-0x362cefff]
```

and relocate `initrd` to the direct mapping area with the `relocate_initrd` function. In the start of the `relocate_initrd` function we try to find a free area with the `memblock_find_in_range` function:

```C
relocated_ramdisk = memblock_find_in_range(0, PFN_PHYS(max_pfn_mapped), area_size, PAGE_SIZE);

if (!relocated_ramdisk)
    panic("Cannot find place for new RAMDISK of size %lld\n",
	       ramdisk_size);
```
在5.10.13中将上述步骤封装成了一个函数：
```c
static void __init relocate_initrd(void)
{
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();
	u64 ramdisk_size  = get_ramdisk_size();
	u64 area_size     = PAGE_ALIGN(ramdisk_size);

	/* We need to move the initrd down into directly mapped mem */
	relocated_ramdisk = memblock_phys_alloc_range(area_size, PAGE_SIZE, 0,
						      PFN_PHYS(max_pfn_mapped));
	if (!relocated_ramdisk)
		panic("Cannot find place for new RAMDISK of size %lld\n",
		      ramdisk_size);

	initrd_start = relocated_ramdisk + PAGE_OFFSET;
	initrd_end   = initrd_start + ramdisk_size;

    //# sudo dmesg | grep RAM
    //[    0.000000] RAMDISK: [mem 0x345ae000-0x362cefff]
	printk(KERN_INFO "Allocated new RAMDISK: [mem %#010llx-%#010llx]\n",
	       relocated_ramdisk, relocated_ramdisk + ramdisk_size - 1);

    /* 从 early_reserve_initrd() 预留的地方拷贝 */
	copy_from_early_mem((void *)initrd_start, ramdisk_image, ramdisk_size);

	printk(KERN_INFO "Move RAMDISK from [mem %#010llx-%#010llx] to"
		" [mem %#010llx-%#010llx]\n",
		ramdisk_image, ramdisk_image + ramdisk_size - 1,
		relocated_ramdisk, relocated_ramdisk + ramdisk_size - 1);
}
```

The `memblock_find_in_range` function tries to find a free area in a given range, 
```c
/**
 * memblock_find_in_range - find free area in given range
 * @start: start of candidate range
 * @end: end of candidate range, can be %MEMBLOCK_ALLOC_ANYWHERE or
 *       %MEMBLOCK_ALLOC_ACCESSIBLE
 * @size: size of free area to find
 * @align: alignment of free area to find
 *
 * Find @size free area aligned to @align in the specified range.
 *
 * Return:
 * Found address on success, 0 on failure.
 */
phys_addr_t __init_memblock memblock_find_in_range(phys_addr_t start,
					phys_addr_t end, phys_addr_t size,
					phys_addr_t align)
{
	phys_addr_t ret;
	enum memblock_flags flags = choose_memblock_flags();

again:
	ret = memblock_find_in_range_node(size, align, start, end,
					    NUMA_NO_NODE, flags);

	if (!ret && (flags & MEMBLOCK_MIRROR)) {
		pr_warn("Could not allocate %pap bytes of mirrored memory\n",
			&size);
		flags &= ~MEMBLOCK_MIRROR;
		goto again;
	}

	return ret;
}
```

in our case from `0` to the maximum mapped physical address and size must equal to the aligned size of the `initrd`. If we didn't find a area with the given size, we call `panic` again. If all is good, we start to relocated RAM disk to the down of the directly mapped memory in the next step.

In the end of the `reserve_initrd` function, we free memblock memory which occupied by the ramdisk with the call of the:

```C
memblock_free(ramdisk_image, ramdisk_end - ramdisk_image);
```

After we relocated `initrd` ramdisk image, the next function is `vsmp_init` from the [arch/x86/kernel/vsmp_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/vsmp_64.c). 



This function initializes support of the `ScaleMP vSMP`. As I already wrote in the previous parts, this chapter will not cover non-related `x86_64` initialization parts (for example as the current or `ACPI`, etc.). So we will skip implementation of this for now and will back to it in the part which cover techniques of parallel computing.
```c
void __init vsmp_init(void)
{
	detect_vsmp_box();
	if (!is_vsmp_box())
		return;

	x86_platform.apic_post_init = vsmp_apic_post_init;

	vsmp_cap_cpus();

	set_vsmp_ctl();
	return;
}
```
The next function is `io_delay_init` from the [arch/x86/kernel/io_delay.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/io_delay.c). This function allows to override  default I/O delay `0x80` port. We already saw I/O delay in the [Last preparation before transition into protected mode](http://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-3.html), now let's look on the `io_delay_init` implementation:

```C
void __init io_delay_init(void)
{
    if (!io_delay_override)
        dmi_check_system(io_delay_0xed_port_dmi_table);
}
```

This function check `io_delay_override` variable and overrides I/O delay port if `io_delay_override` is set. We can set `io_delay_override` variably by passing `io_delay` option to the kernel command line. 

```c
static int __init io_delay_param(char *s)
{
	if (!s)
		return -EINVAL;

	if (!strcmp(s, "0x80"))
		io_delay_type = IO_DELAY_TYPE_0X80;
	else if (!strcmp(s, "0xed"))
		io_delay_type = IO_DELAY_TYPE_0XED;
	else if (!strcmp(s, "udelay"))
		io_delay_type = IO_DELAY_TYPE_UDELAY;
	else if (!strcmp(s, "none"))
		io_delay_type = IO_DELAY_TYPE_NONE;
	else
		return -EINVAL;

	io_delay_override = 1;
	return 0;
}

early_param("io_delay", io_delay_param);
```

As we can read from the [Documentation/kernel-parameters.txt](https://github.com/torvalds/linux/blob/master/Documentation/kernel-parameters.txt), `io_delay` option is:

```
io_delay=	[X86] I/O delay method
    0x80
        Standard port 0x80 based delay
    0xed
        Alternate port 0xed based delay (needed on some systems)
    udelay
        Simple two microseconds delay
    none
        No delay
```
而这里，只定义了编译宏80：
```c
#define CONFIG_IO_DELAY_0X80 1

#if defined(CONFIG_IO_DELAY_0X80)   //No delay
#define DEFAULT_IO_DELAY_TYPE	IO_DELAY_TYPE_0X80
#elif defined(CONFIG_IO_DELAY_0XED)
#define DEFAULT_IO_DELAY_TYPE	IO_DELAY_TYPE_0XED
#elif defined(CONFIG_IO_DELAY_UDELAY)
#define DEFAULT_IO_DELAY_TYPE	IO_DELAY_TYPE_UDELAY
#elif defined(CONFIG_IO_DELAY_NONE)
#define DEFAULT_IO_DELAY_TYPE	IO_DELAY_TYPE_NONE
#endif
```
We can see `io_delay` command line parameter setup with the `early_param` macro in the [arch/x86/kernel/io_delay.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/io_delay.c)

```C
early_param("io_delay", io_delay_param);
```

More about `early_param` you can read in the previous [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html). So the `io_delay_param` function which setups `io_delay_override` variable will be called in the [do_early_param](https://github.com/torvalds/linux/blob/master/init/main.c#L413) function. `io_delay_param` function gets the argument of the `io_delay` kernel command line parameter and sets `io_delay_type` depends on it:

```C
static int __init io_delay_param(char *s)
{
        if (!s)
                return -EINVAL;

        if (!strcmp(s, "0x80"))
                io_delay_type = CONFIG_IO_DELAY_TYPE_0X80;
        else if (!strcmp(s, "0xed"))
                io_delay_type = CONFIG_IO_DELAY_TYPE_0XED;
        else if (!strcmp(s, "udelay"))
                io_delay_type = CONFIG_IO_DELAY_TYPE_UDELAY;
        else if (!strcmp(s, "none"))
                io_delay_type = CONFIG_IO_DELAY_TYPE_NONE;
        else
                return -EINVAL;

        io_delay_override = 1;
        return 0;
}
```

The next functions are `acpi_boot_table_init`, `early_acpi_boot_init` and `initmem_init` after the `io_delay_init`, but as I wrote above we will not cover [ACPI](http://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface) related stuff in this `Linux Kernel initialization process` chapter.


Allocate area for DMA
--------------------------------------------------------------------------------

In the next step we need to allocate area for the [Direct memory access](http://en.wikipedia.org/wiki/Direct_memory_access) with the `dma_contiguous_reserve` function which is defined in the [drivers/base/dma-contiguous.c](https://github.com/torvalds/linux/blob/master/drivers/base/dma-contiguous.c). 
```c

/**
 * dma_contiguous_reserve() - reserve area(s) for contiguous memory handling
 * @limit: End address of the reserved memory (optional, 0 for any).
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory.
 */
void __init dma_contiguous_reserve(phys_addr_t limit)
{
	phys_addr_t selected_size = 0;
	phys_addr_t selected_base = 0;
	phys_addr_t selected_limit = limit;
	bool fixed = false;

	pr_debug("%s(limit %08lx)\n", __func__, (unsigned long)limit);

	if (size_cmdline != -1) {
		selected_size = size_cmdline;
		selected_base = base_cmdline;
		selected_limit = min_not_zero(limit_cmdline, limit);
		if (base_cmdline + size_cmdline == limit_cmdline)
			fixed = true;
	} else {
#ifdef CONFIG_CMA_SIZE_SEL_MBYTES
		selected_size = size_bytes;
#elif defined(CONFIG_CMA_SIZE_SEL_PERCENTAGE)
		selected_size = cma_early_percent_memory();
#elif defined(CONFIG_CMA_SIZE_SEL_MIN)
		selected_size = min(size_bytes, cma_early_percent_memory());
#elif defined(CONFIG_CMA_SIZE_SEL_MAX)
		selected_size = max(size_bytes, cma_early_percent_memory());
#endif
	}

	if (selected_size && !dma_contiguous_default_area) {
		pr_debug("%s: reserving %ld MiB for global area\n", __func__,
			 (unsigned long)selected_size / SZ_1M);

		dma_contiguous_reserve_area(selected_size, selected_base,
					    selected_limit,
					    &dma_contiguous_default_area,
					    fixed);
	}
}
```

`DMA` is a special mode when devices communicate with memory without CPU. Note that we pass one parameter - `max_pfn_mapped << PAGE_SHIFT`, to the `dma_contiguous_reserve` function and as you can understand from this expression, this is limit of the reserved memory. Let's look on the implementation of this function. It starts from the definition of the following variables:

```C
phys_addr_t selected_size = 0;
phys_addr_t selected_base = 0;
phys_addr_t selected_limit = limit;
bool fixed = false;
```

where first represents size in bytes of the reserved area, second is base address of the reserved area, third is end address of the reserved area and the last `fixed` parameter shows where to place reserved area. If `fixed` is `1` we just reserve area with the `memblock_reserve`, if it is `0` we allocate space with the `kmemleak_alloc`. In the next step we check `size_cmdline` variable and if it is not equal to `-1` we fill all variables which you can see above with the values from the `cma` kernel command line parameter:

```C
    //check `size_cmdline` variable
	if (size_cmdline != -1) {

        //fill all variables which you can see above with the values 
        //  from the `cma` kernel command line parameter
		selected_size = size_cmdline;
		selected_base = base_cmdline;
		selected_limit = min_not_zero(limit_cmdline, limit);
		if (base_cmdline + size_cmdline == limit_cmdline)
			fixed = true;
        
	}
```

You can find in this source code file definition of the early parameter:

```C
static int __init early_cma(char *p)
{
	if (!p) {
		pr_err("Config string not provided\n");
		return -EINVAL;
	}

	size_cmdline = memparse(p, &p);
	if (*p != '@')
		return 0;
	base_cmdline = memparse(p + 1, &p);
	if (*p != '-') {
		limit_cmdline = base_cmdline + size_cmdline;
		return 0;
	}
	limit_cmdline = memparse(p + 1, &p);

	return 0;
}
early_param("cma", early_cma);
```

where `cma` is:

```
cma=nn[MG]@[start[MG][-end[MG]]]
		[ARM,X86,KNL]
		Sets the size of kernel global memory area for
		contiguous memory allocations and optionally the
		placement constraint by the physical address range of
		memory allocations. A value of 0 disables CMA
		altogether. For more information, see
		include/linux/dma-contiguous.h
```

If we will not pass `cma` option to the kernel command line, `size_cmdline` will be equal to `-1`.
```c
static const phys_addr_t __initconst size_bytes  =
	(phys_addr_t)CMA_SIZE_MBYTES * SZ_1M;
static phys_addr_t  __initdata size_cmdline  = -1;
static phys_addr_t __initdata base_cmdline ;
static phys_addr_t __initdata limit_cmdline ;
```

In this way we need to calculate size of the reserved area which depends on the following kernel configuration options:

```c
//check `size_cmdline` variable
	if (size_cmdline != -1) {
        ///。。。
	} else {
        //如果命令行中没有指定cma
        //calculate size of the reserved area
    
#ifdef CONFIG_CMA_SIZE_SEL_MBYTES
		selected_size = size_bytes;
#elif defined(CONFIG_CMA_SIZE_SEL_PERCENTAGE)
		selected_size = cma_early_percent_memory();
#elif defined(CONFIG_CMA_SIZE_SEL_MIN)
		selected_size = min(size_bytes, cma_early_percent_memory());
#elif defined(CONFIG_CMA_SIZE_SEL_MAX)
		selected_size = max(size_bytes, cma_early_percent_memory());
#endif
	}
```

* `CONFIG_CMA_SIZE_SEL_MBYTES` - size in megabytes, default global `CMA` area, which is equal to `CMA_SIZE_MBYTES * SZ_1M` or `CONFIG_CMA_SIZE_MBYTES * 1M`;
* `CONFIG_CMA_SIZE_SEL_PERCENTAGE` - percentage of total memory;
* `CONFIG_CMA_SIZE_SEL_MIN` - use lower value;
* `CONFIG_CMA_SIZE_SEL_MAX` - use higher value.

As we calculated the size of the reserved area, we reserve area with the call of the `dma_contiguous_reserve_area` function which first of all calls:

```
int __init dma_contiguous_reserve_area(phys_addr_t size, phys_addr_t base,
				       phys_addr_t limit, struct cma **res_cma,
				       bool fixed)
{
	int ret;

	ret = cma_declare_contiguous(base, size, limit, 0, 0, fixed,
					"reserved", res_cma);
	if (ret)
		return ret;

	/* Architecture specific contiguous memory fixup. */
	dma_contiguous_early_fixup(cma_get_base(*res_cma),
				cma_get_size(*res_cma));

	return 0;
}
```

function. The `cma_declare_contiguous` reserves contiguous area from the given base address with given size. After we reserved area for the `DMA`, next function is the `memblock_find_dma_reserve`. As you can understand from its name, this function counts the reserved pages in the `DMA` area. This part will not cover all details of the `CMA` and `DMA`, because they are big. We will see much more details in the special part in the Linux Kernel Memory management which covers contiguous memory allocators and areas.

Initialization of the sparse(稀疏) memory
--------------------------------------------------------------------------------

The next step is the call of the function - `x86_init.paging.pagetable_init`. 

```c
struct x86_init_ops __initdata x86_init  = {
    ...
	.paging = {
		.pagetable_init		= native_pagetable_init,
	},
    ...
};
```

If you try to find this function in the linux kernel source code, in the end of your search, you will see the following macro:

```C
#define native_pagetable_init        paging_init
```

which expands as you can see to the call of the `paging_init` function from the [arch/x86/mm/init_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/mm/init_64.c). The `paging_init` function initializes sparse memory and zone sizes. First of all what's zones and what is it `Sparsemem`. 

>The `Sparsemem` is a special foundation in the linux kernel memory manager which used to split memory area into different memory banks in the [NUMA](http://en.wikipedia.org/wiki/Non-uniform_memory_access) systems. 

Let's look on the implementation of the `paginig_init` function:

```C
void __init paging_init(void)
{
	sparse_init();

	/*
	 * clear the default setting with node 0
	 * note: don't use nodes_clear here, that is really clearing when
	 *	 numa support is not compiled in, and later node_set_state
	 *	 will not set it back.
	 */
	node_clear_state(0, N_MEMORY);
	node_clear_state(0, N_NORMAL_MEMORY);

	zone_sizes_init();
}
```

As you can see there is call of the `sparse_memory_present_with_active_regions` function which records a memory area for every `NUMA` node to the array of the `mem_section` structure which contains a pointer to the structure of the array of `struct page`. The next `sparse_init` function allocates non-linear `mem_section` and `mem_map`. In the next step we clear state of the movable memory nodes and initialize sizes of zones. Every `NUMA` node is divided into a number of pieces which are called - `zones`. So, `zone_sizes_init` function from the [arch/x86/mm/init.c](https://github.com/torvalds/linux/blob/master/arch/x86/mm/init.c) initializes size of zones.

```c
void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

#ifdef CONFIG_ZONE_DMA
	max_zone_pfns[ZONE_DMA]		= min(MAX_DMA_PFN, max_low_pfn);
#endif
#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32]	= min(MAX_DMA32_PFN, max_low_pfn);
#endif
	max_zone_pfns[ZONE_NORMAL]	= max_low_pfn;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM]	= max_pfn;
#endif

	free_area_init(max_zone_pfns);
}
```

Again, this part and next parts do not cover this theme in full details. There will be special part about `NUMA`.

vsyscall mapping 
--------------------------------------------------------------------------------

The next step after `SparseMem` initialization is setting of the `trampoline_cr4_features` which must contain content of the `cr4` [Control register](http://en.wikipedia.org/wiki/Control_register). First of all we need to check that current CPU has support of the `cr4` register and if it has, we save its content to the `trampoline_cr4_features` which is storage for `cr4` in the real mode:

```C
if (boot_cpu_data.cpuid_level >= 0) {
    mmu_cr4_features = __read_cr4();
	if (trampoline_cr4_features)
	    *trampoline_cr4_features = mmu_cr4_features;
}
```

The next function which you can see is `map_vsyscall` from the [arch/x86/kernel/vsyscall_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/vsyscall_64.c). 



This function maps memory space for [vsyscalls](https://lwn.net/Articles/446528/) and depends on `CONFIG_X86_VSYSCALL_EMULATION` kernel configuration option. Actually `vsyscall` is a special segment which provides fast access to the certain system calls like `getcpu`, etc. Let's look on implementation of this function:

```c
void __init map_vsyscall(void)
{
	extern char __vsyscall_page;
	unsigned long physaddr_vsyscall = __pa_symbol(&__vsyscall_page);

	/*
	 * For full emulation, the page needs to exist for real.  In
	 * execute-only mode, there is no PTE at all backing the vsyscall
	 * page.
	 */
	if (vsyscall_mode == EMULATE) {
		__set_fixmap(VSYSCALL_PAGE, physaddr_vsyscall,
			     PAGE_KERNEL_VVAR);
		set_vsyscall_pgtable_user_bits(swapper_pg_dir);
	}

	if (vsyscall_mode == XONLY)
		gate_vma.vm_flags = VM_EXEC;

	BUILD_BUG_ON((unsigned long)__fix_to_virt(VSYSCALL_PAGE) !=
		     (unsigned long)VSYSCALL_ADDR);
}
```

In the beginning of the `map_vsyscall` we can see definition of two variables. The first is extern variable `__vsyscall_page`. As a extern variable, it defined somewhere in other source code file. Actually we can see definition of the `__vsyscall_page` in the [arch/x86/kernel/vsyscall_emu_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/vsyscall_emu_64.S). The `__vsyscall_page` symbol points to the aligned calls of the `vsyscalls` as `gettimeofday`, etc.:

```assembly
	.globl __vsyscall_page
	.balign PAGE_SIZE, 0xcc
	.type __vsyscall_page, @object
__vsyscall_page:

	mov $__NR_gettimeofday, %rax
	syscall
	ret

	.balign 1024, 0xcc
	mov $__NR_time, %rax
	syscall
	ret
    ...
    ...
    ...
```

The second variable is `physaddr_vsyscall` which just stores physical address of the `__vsyscall_page` symbol. In the next step we check the `vsyscall_mode` variable, and if it is not equal to `NONE`, it is `EMULATE` by default:

```C
static enum { EMULATE, NATIVE, NONE } vsyscall_mode = EMULATE;
```

And after this check we can see the call of the `__set_fixmap` function which calls `native_set_fixmap` with the same parameters:

```C
static inline void __set_fixmap(enum fixed_addresses idx,
				phys_addr_t phys, pgprot_t flags)
{
	native_set_fixmap(idx, phys, flags);
}
void native_set_fixmap(enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
        __native_set_fixmap(idx, pfn_pte(phys >> PAGE_SHIFT, flags));
}
int fixmaps_set;

void __native_set_fixmap(enum fixed_addresses idx, pte_t pte)
{
	unsigned long address = __fix_to_virt(idx);

#ifdef CONFIG_X86_64
       /*
	* Ensure that the static initial page tables are covering the
	* fixmap completely.
	*/
	BUILD_BUG_ON(__end_of_permanent_fixed_addresses >
		     (FIXMAP_PMD_NUM * PTRS_PER_PTE));
#endif

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_vaddr(address, pte);
	fixmaps_set++;
}
```

Here we can see that `native_set_fixmap` makes value of `Page Table Entry` from the given physical address (physical address of the `__vsyscall_page` symbol in our case) and calls internal function - `__native_set_fixmap`. Internal function gets the virtual address of the given `fixed_addresses` index (`VSYSCALL_PAGE` in our case) and checks that given index is not greater than end of the fix-mapped addresses. 

```c
//       +-----------+-----------------+---------------+------------------+
//       |           |                 |               |                  |
//       |kernel text|      kernel     |               |    vsyscalls     |
//       | mapping   |       text      |    Modules    |    fix-mapped    |
//       |from phys 0|       data      |               |    addresses     |
//       |           |                 |               |                  |
//       +-----------+-----------------+---------------+------------------+
//__START_KERNEL_map   __START_KERNEL    MODULES_VADDR            0xffffffffffffffff
```

After this we set page table entry with the call of the `set_pte_vaddr` function and increase count of the fix-mapped addresses.
```c
    //set page table entry with the call of the `set_pte_vaddr`
	set_pte_vaddr(address, pte);

    //increase count of the fix-mapped addresses
	fixmaps_set++;
```
And in the end of the `map_vsyscall` we check that virtual address of the `VSYSCALL_PAGE` (which is first index in the `fixed_addresses`) is not greater than `VSYSCALL_ADDR` which is `-10UL << 20` or `ffffffffff600000` with the `BUILD_BUG_ON` macro:

```C
BUILD_BUG_ON((unsigned long)__fix_to_virt(VSYSCALL_PAGE) !=
                     (unsigned long)VSYSCALL_ADDR);
```
>值得注意的是，当内核开启半虚拟化后，__set_fixmap不再直接为native_set_fixmap。

```c
static inline void __set_fixmap(unsigned /* enum fixed_addresses */ idx,
				phys_addr_t phys, pgprot_t flags)
{
	pv_ops.mmu.set_fixmap(idx, phys, flags);
}

struct paravirt_patch_template pv_ops = {
    ...
#ifdef CONFIG_PARAVIRT_XXL
    ...
	.mmu.set_fixmap		= native_set_fixmap,
    ...
#endif /* CONFIG_PARAVIRT_XXL */
    ...
};
```

Now `vsyscall` area is in the `fix-mapped` area. That's all about `map_vsyscall`, if you do not know anything about fix-mapped addresses, you can read [Fix-Mapped Addresses and ioremap](http://xinqiu.gitbooks.io/linux-insides-cn/content/MM/linux-mm-2.html). We will see more about `vsyscalls` in the `vsyscalls and vdso` part.


Getting the SMP configuration
--------------------------------------------------------------------------------

You may remember how we made a search of the [SMP](http://en.wikipedia.org/wiki/Symmetric_multiprocessing) configuration in the previous [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html). Now we need to get the `SMP` configuration if we found it. For this we check `smp_found_config` variable which we set in the `smp_scan_config` function (read about it the previous part) and call the `get_smp_config` function:

```C
if (smp_found_config)
	get_smp_config();
```
在5.10.13中是这样的：
```c
//解析 [SMP](http://en.wikipedia.org/wiki/Symmetric_multiprocessing) 的配置信息
//setup_arch()
//  find_smp_config()
//    x86_init.mpparse.find_smp_config()
//      default_find_smp_config()
static inline void find_smp_config(void)
{
	x86_init.mpparse.find_smp_config();
}
```
The `get_smp_config` expands to the `x86_init.mpparse.default_get_smp_config` function which is defined in the [arch/x86/kernel/mpparse.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/mpparse.c). 

```c
/*
 * Scan the memory blocks for an SMP configuration block.
 */
void __init default_get_smp_config(unsigned int early)
{
	struct mpf_intel *mpf;

	if (!smp_found_config)
		return;

	if (!mpf_found)
		return;

	if (acpi_lapic && early)
		return;

	/*
	 * MPS doesn't support hyperthreading, aka only have
	 * thread 0 apic id in MPS table
	 */
	if (acpi_lapic && acpi_ioapic)
		return;

	mpf = early_memremap(mpf_base, sizeof(*mpf));
	if (!mpf) {
		pr_err("MPTABLE: error mapping MP table\n");
		return;
	}

	pr_info("Intel MultiProcessor Specification v1.%d\n",
		mpf->specification);
#if defined(CONFIG_X86_LOCAL_APIC) && defined(CONFIG_X86_32)
	if (mpf->feature2 & (1 << 7)) {
		pr_info("    IMCR and PIC compatibility mode.\n");
		pic_mode = 1;
	} else {
		pr_info("    Virtual Wire compatibility mode.\n");
		pic_mode = 0;
	}
#endif
	/*
	 * Now see if we need to read further.
	 */
	if (mpf->feature1) {
		if (early) {
			/*
			 * local APIC has default address
			 */
			mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;
			goto out;
		}

		pr_info("Default MP configuration #%d\n", mpf->feature1);
		construct_default_ISA_mptable(mpf->feature1);

	} else if (mpf->physptr) {
		if (check_physptr(mpf, early))
			goto out;
	} else
		BUG();

	if (!early)
		pr_info("Processors: %d\n", num_processors);
	/*
	 * Only use the first configuration found.
	 */
out:
	early_memunmap(mpf, sizeof(*mpf));
}
```

This function defines a pointer to the multiprocessor floating pointer structure（多处理器配置数据结构） - `mpf_intel` (you can read about it in the previous [part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html)) and does some checks:

```C
struct mpf_intel *mpf = mpf_found;

if (!mpf)
    return;
 
if (acpi_lapic && early)
   return;
```

Here we can see that multiprocessor configuration was found in the `smp_scan_config` function or just return from the function if not. The next check is `acpi_lapic` and `early`. 


And as we did this checks, we start to read the `SMP` configuration. As we finished reading it, the next step is - `prefill_possible_map` function which makes preliminary filling of the possible CPU's `cpumask` (more about it you can read in the [Introduction to the cpumasks](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-2.html)).

```c
static inline void
set_cpu_possible(unsigned int cpu, bool possible)
{
	if (possible)
		cpumask_set_cpu(cpu, &__cpu_possible_mask);
	else
		cpumask_clear_cpu(cpu, &__cpu_possible_mask);
}
```

The rest of the setup_arch
--------------------------------------------------------------------------------

Here we are getting to the end of the `setup_arch` function. The rest of function of course is important, but details about these stuff will not will not be included in this part. We will just take a short look on these functions, because although they are important as I wrote above, but they cover non-generic kernel features related with the `NUMA`, `SMP`, `ACPI` and `APICs`, etc. First of all, the next call of the `init_apic_mappings` function. As we can understand this function sets the address of the local [APIC](http://en.wikipedia.org/wiki/Advanced_Programmable_Interrupt_Controller). 
```c

/**
 * init_apic_mappings - initialize APIC mappings
 */
void __init init_apic_mappings(void)
{
	unsigned int new_apicid;

	if (apic_validate_deadline_timer())
		pr_info("TSC deadline timer available\n");

	if (x2apic_mode) {
		boot_cpu_physical_apicid = read_apic_id();
		return;
	}

	/* If no local APIC can be found return early */
	if (!smp_found_config && detect_init_APIC()) {
		/* lets NOP'ify apic operations */
		pr_info("APIC: disable apic facility\n");
		apic_disable();
	} else {
		apic_phys = mp_lapic_addr;

		/*
		 * If the system has ACPI MADT tables or MP info, the LAPIC
		 * address is already registered.
		 */
		if (!acpi_lapic && !smp_found_config)
			register_lapic_address(apic_phys);
	}

	/*
	 * Fetch the APIC ID of the BSP in case we have a
	 * default configuration (or the MP table is broken).
	 */
	new_apicid = read_apic_id();
	if (boot_cpu_physical_apicid != new_apicid) {
		boot_cpu_physical_apicid = new_apicid;
		/*
		 * yeah -- we lie about apic_version
		 * in case if apic was disabled via boot option
		 * but it's not a problem for SMP compiled kernel
		 * since apic_intr_mode_select is prepared for such
		 * a case and disable smp mode
		 */
		boot_cpu_apic_version = GET_APIC_VERSION(apic_read(APIC_LVR));
	}
}
```

The next is `x86_io_apic_ops.init` and this function initializes I/O APIC. Please note that we will see all details related with `APIC` in the chapter about interrupts and exceptions handling. 

In the next step we reserve standard I/O resources like `DMA`, `TIMER`, `FPU`, etc., with the call of the `x86_init.resources.reserve_resources` function. 

```c
struct resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
	unsigned long desc;
	struct resource *parent, *sibling, *child;
    //+-------------+      +-------------+
    //|    parent   |------|    sibling  |
    //+-------------+      +-------------+
    //       |
    //+-------------+
    //|    child    | 
    //+-------------+
};

static struct resource standard_io_resources[] = {
	{ .name = "dma1", .start = 0x00, .end = 0x1f,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "pic1", .start = 0x20, .end = 0x21,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "timer0", .start = 0x40, .end = 0x43,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "timer1", .start = 0x50, .end = 0x53,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "keyboard", .start = 0x60, .end = 0x60,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "keyboard", .start = 0x64, .end = 0x64,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "dma page reg", .start = 0x80, .end = 0x8f,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "pic2", .start = 0xa0, .end = 0xa1,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "dma2", .start = 0xc0, .end = 0xdf,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "fpu", .start = 0xf0, .end = 0xff,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO }
};

void __init reserve_standard_io_resources(void)
{
	int i;

	/* request I/O space for devices used on all i[345]86 PCs */
	for (i = 0; i < ARRAY_SIZE(standard_io_resources); i++)
		request_resource(&ioport_resource, &standard_io_resources[i]);
}
```

Following is `mcheck_init` function initializes `Machine check Exception` ：

```c
int __init mcheck_init(void)
{
	mcheck_intel_therm_init();
	mce_register_decode_chain(&early_nb);
	mce_register_decode_chain(&mce_uc_nb);
	mce_register_decode_chain(&mce_default_nb);
	mcheck_vendor_init_severity();

	INIT_WORK(&mce_work, mce_gen_pool_process);
	init_irq_work(&mce_irq_work, mce_irq_work_cb);

	return 0;
}
```
会初始化中断work：
```c
static struct irq_work mce_irq_work;
```
and the last is `register_refined_jiffies` which registers [jiffy](http://en.wikipedia.org/wiki/Jiffy_%28time%29) (There will be separate chapter about timers in the kernel).
```c

static struct clocksource refined_jiffies;

int register_refined_jiffies(long cycles_per_second)
{
    /* The clock frequency of the i8253/i8254 PIT */
    //cycles_per_second = CLOCK_TICK_RATE = 1193182ul
	u64 nsec_per_tick, shift_hz;
	long cycles_per_tick;



	refined_jiffies = clocksource_jiffies;
	refined_jiffies.name = "refined-jiffies";
	refined_jiffies.rating++;

	/* Calc cycles per tick 每秒多少个滴答*/
	cycles_per_tick = (cycles_per_second + HZ/2)/HZ;/*HZ=1000 计算每秒多少个 滴答*/
    
	/* shift_hz stores hz<<8 for extra accuracy */
	shift_hz = (u64)cycles_per_second << 8; /* 其他精度 */
	shift_hz += cycles_per_tick/2;          /*  */
	do_div(shift_hz, cycles_per_tick);      /*  */
    
	/* Calculate nsec_per_tick using shift_hz */
	nsec_per_tick = (u64)NSEC_PER_SEC << 8;
	nsec_per_tick += (u32)shift_hz/2;
	do_div(nsec_per_tick, (u32)shift_hz);

	refined_jiffies.mult = ((u32)nsec_per_tick) << JIFFIES_SHIFT;

    /* 注册时钟源 */
	__clocksource_register(&refined_jiffies);
	return 0;
}
```
So that's all. Finally we have finished with the big `setup_arch` function in this part. Of course as I already wrote many times, we did not see full details about this function, but do not worry about it. We will be back more than once to this function from different chapters for understanding how different platform-dependent parts are initialized.

That's all, and now we can back to the `start_kernel` from the `setup_arch`.

Back to the main.c
================================================================================

As I wrote above, we have finished with the `setup_arch` function and now we can back to the `start_kernel` function from the [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c). As you may remember or saw yourself, `start_kernel` function as big as the `setup_arch`. So the couple of the next part will be dedicated to learning of this function. 

So, let's continue with it. After the `setup_arch` we can see the call of the `mm_init_cpumask` function. 在5.10.13中，该函数被移至mm_init中，mm_init在setup_arch的下方执行。This function sets the [cpumask](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-2.html) pointer to the memory descriptor `cpumask`. We can look on its implementation:

```C
/* Pointer magic because the dynamic array size confuses some compilers. */
static inline void mm_init_cpumask(struct mm_struct *mm)
{
	unsigned long cpu_bitmap = (unsigned long)mm;

	cpu_bitmap += offsetof(struct mm_struct, cpu_bitmap);
	cpumask_clear((struct cpumask *)cpu_bitmap);
}
```

As you can see in the [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c), we pass memory descriptor of the init process to the `mm_init_cpumask` and depends on `CONFIG_CPUMASK_OFFSTACK` configuration option we clear [TLB](http://en.wikipedia.org/wiki/Translation_lookaside_buffer) switch `cpumask`.

In the next step we can see the call of the following function:

```C
setup_command_line(command_line);
```
```c
static void __init setup_command_line(char *command_line)
{
	size_t len, xlen = 0, ilen = 0;

	if (extra_command_line)
		xlen = strlen(extra_command_line);
	if (extra_init_args)
		ilen = strlen(extra_init_args) + 4; /* for " -- " */

	len = xlen + strlen(boot_command_line) + 1;

	saved_command_line = memblock_alloc(len + ilen, SMP_CACHE_BYTES);
	if (!saved_command_line)
		panic("%s: Failed to allocate %zu bytes\n", __func__, len + ilen);

	static_command_line = memblock_alloc(len, SMP_CACHE_BYTES);
	if (!static_command_line)
		panic("%s: Failed to allocate %zu bytes\n", __func__, len);

	if (xlen) {
		/*
		 * We have to put extra_command_line before boot command
		 * lines because there could be dashes (separator of init
		 * command line) in the command lines.
		 */
		strcpy(saved_command_line, extra_command_line);
		strcpy(static_command_line, extra_command_line);
	}
	strcpy(saved_command_line + xlen, boot_command_line);
	strcpy(static_command_line + xlen, command_line);

	if (ilen) {
		/*
		 * Append supplemental init boot args to saved_command_line
		 * so that user can check what command line options passed
		 * to init.
		 */
		len = strlen(saved_command_line);
		if (initargs_found) {
			saved_command_line[len++] = ' ';
		} else {
			strcpy(saved_command_line + len, " -- ");
			len += 4;
		}

		strcpy(saved_command_line + len, extra_init_args);
	}
}
```
This function takes pointer to the kernel command line allocates a couple of buffers to store command line. We need a couple of buffers, because one buffer used for future reference and accessing to command line and one for parameter parsing. We will allocate space for the following buffers:

* `saved_command_line` - will contain boot command line;
* `initcall_command_line` - will contain boot command line. will be used in the `do_initcall_level`;
* `static_command_line` - will contain command line for parameters parsing.

We will allocate space with the `memblock_virt_alloc` function. 
```c
static inline void * __init memblock_alloc(phys_addr_t size,  phys_addr_t align)
{
	return memblock_alloc_try_nid(size, align, MEMBLOCK_LOW_LIMIT,
				      MEMBLOCK_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
}
```
This function calls `memblock_virt_alloc_try_nid` which allocates boot memory block with `memblock_reserve` if [slab](http://en.wikipedia.org/wiki/Slab_allocation) is not available or uses `kzalloc_node` (more about it will be in the linux memory management chapter). The `memblock_virt_alloc` uses `BOOTMEM_LOW_LIMIT` (physical address of the `(PAGE_OFFSET + 0x1000000)` value) and `BOOTMEM_ALLOC_ACCESSIBLE` (equal to the current value of the `memblock.current_limit`) as minimum address of the memory region and maximum address of the memory region.

Let's look on the implementation of the `setup_command_line`:

```C
static void __init setup_command_line(char *command_line)
{
    saved_command_line =
            memblock_virt_alloc(strlen(boot_command_line) + 1, 0);
    initcall_command_line =
            memblock_virt_alloc(strlen(boot_command_line) + 1, 0);
    static_command_line = memblock_virt_alloc(strlen(command_line) + 1, 0);
    strcpy(saved_command_line, boot_command_line);
    strcpy(static_command_line, command_line);
 }
```

Here we can see that we allocate space for the three buffers which will contain kernel command line for the different purposes (read above). And as we allocated space, we store `boot_command_line` in the `saved_command_line` and `command_line` (kernel command line from the `setup_arch`) to the `static_command_line`.

The next function after the `setup_command_line` is the `setup_nr_cpu_ids`. 

```c
void __init setup_nr_cpu_ids(void)  /*  */
{
	nr_cpu_ids = find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS) + 1;
}
```

This function setting `nr_cpu_ids` (number of CPUs) according to the last bit in the `cpu_possible_mask` (more about it you can read in the chapter describes [cpumasks](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-2.html) concept). Let's look on its implementation:

```C
void __init setup_nr_cpu_ids(void)
{
        nr_cpu_ids = find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS) + 1;
}
```

Here `nr_cpu_ids` represents number of CPUs, `NR_CPUS` represents the maximum number of CPUs which we can set in configuration time:

```c
/*
 * Maximum supported processors.  Setting this smaller saves quite a
 * bit of memory.  Use nr_cpu_ids instead of this except for static bitmaps.
 */
#ifndef CONFIG_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_NR_CPUS	1
#endif

/* Places which use this should consider cpumask_var_t. */
#define NR_CPUS		CONFIG_NR_CPUS
```

Actually we need to call this function, because `NR_CPUS` can be greater than actual amount of the CPUs in the your computer. Here we can see that we call `find_last_bit` function and pass two parameters to it:

* `cpu_possible_mask` bits;
* maximum number of CPUS.

```c
unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
	if (size) {
		unsigned long val = BITMAP_LAST_WORD_MASK(size);
		unsigned long idx = (size-1) / BITS_PER_LONG;

		do {
			val &= addr[idx];
			if (val)
				return idx * BITS_PER_LONG + __fls(val);

			val = ~0ul;
		} while (idx--);
	}
	return size;
}
EXPORT_SYMBOL(find_last_bit);
```

In the `setup_arch` we can find the call of the `prefill_possible_map` function which calculates and writes to the `cpu_possible_mask` actual number of the CPUs. We call the `find_last_bit` function which takes the address and maximum size to search and returns bit number of the first set bit. We passed `cpu_possible_mask` bits and maximum number of the CPUs. First of all the `find_last_bit` function splits given `unsigned long` address to the [words](http://en.wikipedia.org/wiki/Word_%28computer_architecture%29):

```C
words = size / BITS_PER_LONG;
```

where `BITS_PER_LONG` is `64` on the `x86_64`. As we got amount of words in the given size of the search data, we need to check is given size does not contain partial words with the following check:

```C
if (size & (BITS_PER_LONG-1)) {
     tmp = (addr[words] & (~0UL >> (BITS_PER_LONG
                             - (size & (BITS_PER_LONG-1)))));
     if (tmp)
             goto found;
}
```

if it contains partial word, we mask the last word and check it. If the last word is not zero, it means that current word contains at least one set bit. We go to the `found` label:

```C
found:
    return words * BITS_PER_LONG + __fls(tmp);
```

Here you can see `__fls` function which returns last set bit in a given word with help of the `bsr` instruction:

```C
static inline unsigned long __fls(unsigned long word)
{
        asm("bsr %1,%0"
            : "=r" (word)
            : "rm" (word));
        return word;
}
```

The `bsr` instruction which scans the given operand for first bit set. If the last word is not partial we going through the all words in the given address and trying to find first set bit:

```C
while (words) {
    tmp = addr[--words];
    if (tmp) {
found:
        return words * BITS_PER_LONG + __fls(tmp);
    }
}
```

Here we put the last word to the `tmp` variable and check that `tmp` contains at least one set bit. If a set bit found, we return the number of this bit. If no one words do not contains set bit we just return given size:

```C
return size;
```

After this `nr_cpu_ids` will contain the correct amount of the available CPUs.

That's all.

Conclusion
================================================================================

It is the end of the seventh part about the linux kernel initialization process. In this part, finally we have finished with the `setup_arch` function and returned to the `start_kernel` function. In the next part we will continue to learn generic kernel code from the `start_kernel` and will continue our way to the first `init` process.

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/MintCN/linux-insides-zh).**

Links
================================================================================

* [Desktop Management Interface](http://en.wikipedia.org/wiki/Desktop_Management_Interface)
* [x86_64](http://en.wikipedia.org/wiki/X86-64)
* [initrd](http://en.wikipedia.org/wiki/Initrd)
* [Kernel panic](http://en.wikipedia.org/wiki/Kernel_panic)
* [Documentation/kernel-parameters.txt](https://github.com/torvalds/linux/blob/master/Documentation/kernel-parameters.txt)
* [ACPI](http://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface)
* [Direct memory access](http://en.wikipedia.org/wiki/Direct_memory_access)
* [NUMA](http://en.wikipedia.org/wiki/Non-uniform_memory_access)
* [Control register](http://en.wikipedia.org/wiki/Control_register)
* [vsyscalls](https://lwn.net/Articles/446528/)
* [SMP](http://en.wikipedia.org/wiki/Symmetric_multiprocessing)
* [jiffy](http://en.wikipedia.org/wiki/Jiffy_%28time%29)
* [Previous part](http://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-6.html)
