<center><font size='6'>Linux内存管理：ELF文件格式</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>


# 1. ELF文件格式


ELF (Executable and Linkable Format)是一种为可执行文件，目标文件，共享链接库和内核转储(core dumps)准备的标准文件格式。 Linux和很多类Unix操作系统都使用这个格式。 让我们来看一下64位ELF文件格式的结构以及内核源码中有关于它的一些定义。

一个ELF文件由以下三部分组成：

* ELF头(ELF header) - 描述文件的主要特性：类型，CPU架构，入口地址，现有部分的大小和偏移等等；

* 程序头表(Program header table) - 列举了所有有效的段(segments)和他们的属性。 程序头表需要加载器将文件中的节加载到虚拟内存段中；

* 节头表(Section header table) - 包含对节(sections)的描述。

现在让我们对这些部分有一些更深的了解。

# 2. ELF头(ELF header)

ELF头(ELF header)位于文件的开始位置。 它的主要目的是定位文件的其他部分。 文件头主要包含以下字段：

* ELF文件鉴定 - 一个字节数组用来确认文件是否是一个ELF文件，并且提供普通文件特征的信息；
* 文件类型 - 确定文件类型。 这个字段描述文件是一个重定位文件，或可执行文件,或...；
* 目标结构；
* ELF文件格式的版本；
* 程序入口地址；
* 程序头表的文件偏移；
* 节头表的文件偏移；
* ELF头(ELF header)的大小；
* 程序头表的表项大小；
* 其他字段...

你可以在内核源码种找到表示ELF64 header的结构体 `elf64_hdr`：

```C
typedef struct elf64_hdr {
	unsigned char	e_ident[EI_NIDENT];
	Elf64_Half e_type;
	Elf64_Half e_machine;
	Elf64_Word e_version;
	Elf64_Addr e_entry;
	Elf64_Off e_phoff;
	Elf64_Off e_shoff;
	Elf64_Word e_flags;
	Elf64_Half e_ehsize;
	Elf64_Half e_phentsize;
	Elf64_Half e_phnum;
	Elf64_Half e_shentsize;
	Elf64_Half e_shnum;
	Elf64_Half e_shstrndx;
} Elf64_Ehdr;
```

这个结构体定义在 [elf.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h#L220)

# 3. 节(sections)

所有的数据都存储在ELF文件的节(sections)中。 我们通过节头表中的索引(index)来确认节(sections)。 节头表表项包含以下字段：

* 节的名字；
* 节的类型；
* 节的属性；
* 内存地址；
* 文件中的偏移；
* 节的大小；
* 到其他节的链接；
* 各种各样的信息；
* 地址对齐；
* 这个表项的大小，如果有的话；

而且，在linux内核中结构体 `elf64_shdr` 如下所示:

```C
typedef struct elf64_shdr {
	Elf64_Word sh_name;
	Elf64_Word sh_type;
	Elf64_Xword sh_flags;
	Elf64_Addr sh_addr;
	Elf64_Off sh_offset;
	Elf64_Xword sh_size;
	Elf64_Word sh_link;
	Elf64_Word sh_info;
	Elf64_Xword sh_addralign;
	Elf64_Xword sh_entsize;
} Elf64_Shdr;
```

[elf.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h#L312)

# 4. 程序头表(Program header table)

在可执行文件或者共享链接库中所有的节(sections)都被分为多个段(segments)。 程序头是一个结构的数组，每一个结构都表示一个段(segments)。 它的结构就像这样：

```C
typedef struct elf64_phdr {
	Elf64_Word p_type;
	Elf64_Word p_flags;
	Elf64_Off p_offset;
	Elf64_Addr p_vaddr;
	Elf64_Addr p_paddr;
	Elf64_Xword p_filesz;
	Elf64_Xword p_memsz;
	Elf64_Xword p_align;
} Elf64_Phdr;
```

在内核源码中。

`elf64_phdr` 定义在相同的 [elf.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h#L254) 文件中.

EFL文件也包含其他的字段或结构。 你可以在 [Documentation](http://www.uclibc.org/docs/elf-64-gen.pdf) 中查看。 现在我们来查看一下 `vmlinux` 这个ELF文件。

# 5. vmlinux

`vmlinux` 也是一个可重定位的ELF文件。 我们可以使用 `readelf` 工具来查看它。 首先，让我们看一下它的头部：

```
[rongtao@localhost linux-5.10.13]$ readelf -h vmlinux
ELF 头：
  Magic：  7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  类别:                              ELF64
  数据:                              2 补码，小端序 (little endian)
  版本:                              1 (current)
  OS/ABI:                            UNIX - System V
  ABI 版本:                          0
  类型:                              EXEC (可执行文件)
  系统架构:                          Advanced Micro Devices X86-64
  版本:                              0x1
  入口点地址：              0x1000000
  程序头起点：              64 (bytes into file)
  Start of section headers:          130867392 (bytes into file)
  标志：             0x0
  本头的大小：       64 (字节)
  程序头大小：       56 (字节)
  Number of program headers:         5
  节头大小：         64 (字节)
  节头数量：         69
  字符串表索引节头： 68
```

我们可以看出 `vmlinux` 是一个64位可执行文件。
我们可以从 [Documentation/x86/x86_64/mm.txt](https://github.com/torvalds/linux/blob/master/Documentation/x86/x86_64/mm.txt#L19) 读到相关信息:

```
ffffffff80000000 - ffffffffa0000000 (=512 MB)  kernel text mapping, from phys 0
```

之后我们可以在 `vmlinux` ELF文件中查看这个地址：

```
[rongtao@localhost linux-5.10.13]$ readelf -s vmlinux |  grep ffffffff81000000
     1: ffffffff81000000     0 SECTION LOCAL  DEFAULT    1 
 96515: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 _text
121063: ffffffff81000000     0 NOTYPE  GLOBAL DEFAULT    1 _stext
122809: ffffffff81000000    60 NOTYPE  GLOBAL DEFAULT    1 startup_64
```

值得注意的是，`startup_64` 例程的地址不是 `ffffffff80000000`, 而是 `ffffffff81000000`。 现在我们来解释一下。

我们可以在 [arch/x86/kernel/vmlinux.lds.S](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/vmlinux.lds.S) 看见如下的定义 :

```
    . = __START_KERNEL;
	...
	...
	..
	/* Text and read-only data */
	.text :  AT(ADDR(.text) - LOAD_OFFSET) {
		_text = .;
		...
		...
		...
	}
```

其中，`__START_KERNEL` 定义如下:

```
#define __START_KERNEL		(__START_KERNEL_map + __PHYSICAL_START)
```

从这个文档中看出，`__START_KERNEL_map` 的值是 `ffffffff80000000` 以及 `__PHYSICAL_START` 的值是 `0x1000000`。 这就是 `startup_64`的地址是 `ffffffff81000000`的原因了。

最后我们通过以下命令来得到程序头表的内容：

```
[rongtao@localhost linux-5.10.13]$ readelf -l vmlinux

Elf 文件类型为 EXEC (可执行文件)
入口点 0x1000000
共有 5 个程序头，开始于偏移量64

程序头：
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000200000 0xffffffff81000000 0x0000000001000000
                 0x0000000001b93dc0 0x0000000001b93dc0  R E    200000
  LOAD           0x0000000001e00000 0xffffffff82c00000 0x0000000002c00000
                 0x0000000000fdc000 0x0000000000fdc000  RW     200000
  LOAD           0x0000000002e00000 0x0000000000000000 0x0000000003bdc000
                 0x0000000000036000 0x0000000000036000  RW     200000
  LOAD           0x0000000003012000 0xffffffff83c12000 0x0000000003c12000
                 0x0000000001dee000 0x0000000001dee000  RWE    200000
  NOTE           0x0000000001d93bec 0xffffffff82b93bec 0x0000000002b93bec
                 0x00000000000001d4 0x00000000000001d4         4

 Section to Segment mapping:
  段节...
   00     .text .rodata .pci_fixup .tracedata __ksymtab __ksymtab_gpl __kcrctab __kcrctab_gpl __ksymtab_strings __param __modver __ex_table .notes    01     .data __bug_table .orc_unwind_ip .orc_unwind .orc_lookup .vvar 
   02     .data..percpu 
   03     .init.text .altinstr_aux .init.data .x86_cpu_dev.init .parainstructions .altinstructions .altinstr_replac
ement .iommu_table .apicdrivers .exit.text .exit.data .smp_locks .data_nosave .bss .brk .init.scratch    04     .notes
```

这里我们可以看出五个包含节(sections)列表的段(segments)。 你可以在生成的链接器脚本 - `arch/x86/kernel/vmlinux.lds` 中找到所有的节(sections)。

就这样吧。 当然，它不是ELF(Executable and Linkable Format)的完整描述，但是如果你想要知道更多，可以参考这个文档 - [这里](http://www.uclibc.org/docs/elf-64-gen.pdf)
