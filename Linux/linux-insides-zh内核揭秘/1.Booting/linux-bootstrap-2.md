# 在内核安装代码的第一步

内核启动的第一步  
--------------------------------------------------------------------------------

在[上一节中](https://xinqiu.gitbooks.io/linux-insides-cn/content/Booting/linux-bootstrap-1.html)我们开始接触到内核启动代码，并且分析了初始化部分，最后我们停在了对`main`函数（`main`函数是第一个用C写的函数）的调用（`main`函数位于[arch/x86/boot/main.c](http://lxr.free-electrons.com/source/arch/x86/boot/main.c?v=3.18)）。

在这一节中我们将继续对内核启动过程的研究，我们将  

* 认识`保护模式` 
* 如何从实模式进入保护模式 
* 堆和控制台初始化 
* 内存检测，cpu验证，键盘初始化 
* 还有更多 

现在让我们开始我们的旅程

保护模式 
--------------------------------------------------------------------------------
在操作系统可以使用Intel 64位CPU的[长模式](http://en.wikipedia.org/wiki/Long_mode)之前，内核必须首先将CPU切换到保护模式运行。

什么是[保护模式](https://en.wikipedia.org/wiki/Protected_mode)？保护模式于1982年被引入到Intel CPU家族，并且从那之后，直到Intel 64出现，保护模式都是Intel CPU的主要运行模式。

淘汰[实模式](http://wiki.osdev.org/Real_Mode)的主要原因是因为在实模式下，系统能够访问的内存非常有限。如果你还记得我们在上一节说的，在实模式下，系统最多只能访问1M内存，而且在很多时候，实际能够访问的内存只有640K。

保护模式带来了很多的改变，不过主要的改变都集中在内存管理方法。在保护模式中，实模式的20位地址线被替换成32位地址线，因此系统可以访问多达4GB的地址空间。另外，在保护模式中引入了[内存分页](http://en.wikipedia.org/wiki/Paging)功能，在后面的章节中我们将介绍这个功能。

保护模式提供了2种完全不同的内存管理机制：

* 段式内存管理
* 内存分页

在这一节中，我们只介绍**段式内存管理**，内存分页我们将在后面的章节进行介绍。

在上一节中我们说过，在实模式下，一个物理地址是由2个部分组成的：

* 内存段的基地址 
* 从基地址开始的偏移 
 
使用这2个信息，我们可以通过下面的公式计算出对应的物理地址 

```
PhysicalAddress = Segment * 16 + Offset
```

在保护模式中，内存段的定义和实模式完全不同。在保护模式中，每个内存段不再是64K大小，段的大小和起始位置是通过一个叫做`段描述符`的数据结构进行描述。所有内存段的段描述符存储在一个叫做`全局描述符表`(GDT)的内存结构中。

`全局描述符表`这个内存数据结构在内存中的位置并不是固定的，它的地址保存在一个特殊寄存器 `GDTR` 中。在后面的章节中，我们将在Linux内核代码中看到全局描述符表的地址是如何被保存到 `GDTR` 中的。具体的汇编代码看起来是这样的：

```assembly
lgdt gdt
```

`lgdt` 汇编代码将把全局描述符表的基地址和大小保存到 `GDTR` 寄存器中。`GDTR` 是一个48位的寄存器，这个寄存器中的保存了2部分的内容:

* 全局描述符表的大小 (16位）
* 全局描述符表的基址 (32位)
 
就像前面的段落说的，全局描述符表包含了所有内存段的`段描述符`。每个段描述符长度是64位，结构如下图描述：

```
31          24        19      16              7            0
------------------------------------------------------------
|             | |B| |A|       | |   | |0|E|W|A|            |
| BASE 31:24  |G|/|L|V| LIMIT |P|DPL|S|  TYPE | BASE 23:16 | 4
|             | |D| |L| 19:16 | |   | |1|C|R|A|            |
------------------------------------------------------------
|                             |                            |
|        BASE 15:0            |       LIMIT 15:0           | 0
|                             |                            |
------------------------------------------------------------
```

粗粗一看，上面的结构非常吓人，不过实际上这个结构是非常容易理解的。比如在上图中的 LIMIT 15:0 表示这个数据结构的0到15位保存的是内存段的大小的0到15位。相似的 LIMITE 19:16 表示上述数据结构的16到19位保存的是内存段大小的16到19位。从这个分析中，我们可以看出每个内存段的大小是通过20位进行描述的。下面我们将对这个数据结构进行仔细分析：

1. Limit[20位] 被保存在上述内存结构的0-15和16-19位。根据上述内存结构中`G`位的设置，这20位内存定义的内存长度是不一样的。下面是一些具体的例子：

  * 如果`G` = 0, 并且Limit = 0， 那么表示段长度是1 byte
  * 如果`G` = 1, 并且Limit = 0, 那么表示段长度是4K bytes
  * 如果`G` = 0，并且Limit = 0xfffff，那么表示段长度是1M bytes
  * 如果`G` = 1，并且Limit = 0xfffff，那么表示段长度是4G bytes

  从上面的例子我们可以看出：
  
  * 如果G = 0, 那么内存段的长度是按照1 byte进行增长的 ( Limit每增加1，段长度增加1 byte )，最大的内存段长度将是1M bytes；
  * 如果G = 1, 那么内存段的长度是按照4K bytes进行增长的 ( Limit每增加1，段长度增加4K bytes )，最大的内存段长度将是4G bytes;
  * 段长度的计算公式是 base_seg_length * ( LIMIT + 1)。
   
2. Base[32-bits] 被保存在上述地址结构的0-15， 32-39以及56-63位。Base定义了段基址。

3. Type/Attribute (40-47 bits) 定义了内存段的类型以及支持的操作。
  * `S` 标记（ 第44位 ）定义了段的类型，`S` = 0说明这个内存段是一个`系统段`；`S` = 1说明这个内存段是一个`代码段或者是数据段`（ 堆栈段是一种特殊类型的数据段，堆栈段必须是可以进行读写的段 ）。
   
在`S` = 1的情况下，上述内存结构的第43位决定了内存段是`数据段`还是`代码段`。如果43位 = 0，说明是一个数据段，否则就是一个代码段。

对于数据段和代码段，下面的表格给出了段类型定义

```
|           Type Field        | Descriptor Type | Description
|-----------------------------|-----------------|------------------
| Decimal    (43) (42)(41)(40)|                 |
|             0    E    W   A |                 |
| 0           0    0    0   0 | Data            | Read-Only
| 1           0    0    0   1 | Data            | Read-Only, accessed
| 2           0    0    1   0 | Data            | Read/Write
| 3           0    0    1   1 | Data            | Read/Write, accessed
| 4           0    1    0   0 | Data            | Read-Only, expand-down
| 5           0    1    0   1 | Data            | Read-Only, expand-down, accessed
| 6           0    1    1   0 | Data            | Read/Write, expand-down
| 7           0    1    1   1 | Data            | Read/Write, expand-down, accessed
|                  C    R   A |                 |
| 8           1    0    0   0 | Code            | Execute-Only
| 9           1    0    0   1 | Code            | Execute-Only, accessed
| 10          1    0    1   0 | Code            | Execute/Read
| 11          1    0    1   1 | Code            | Execute/Read, accessed
| 12          1    1    0   0 | Code            | Execute-Only, conforming
| 14          1    1    0   1 | Code            | Execute-Only, conforming, accessed
| 13          1    1    1   0 | Code            | Execute/Read, conforming
| 15          1    1    1   1 | Code            | Execute/Read, conforming, accessed
```

从上面的表格我们可以看出，当第43位是`0`的时候，这个段描述符对应的是一个数据段，如果该位是`1`，那么表示这个段描述符对应的是一个代码段。对于数据段，第42，41，40位表示的是(*E*扩展，*W*可写，*A*可访问）；对于代码段，第42，41，40位表示的是(*C*一致，*R*可读，*A*可访问）。 

  * 如果`E` = 0，数据段是向上扩展数据段，反之为向下扩展数据段。关于向上扩展和向下扩展数据段，可以参考下面的[链接](http://www.sudleyplace.com/dpmione/expanddown.html)。在一般情况下，**应该是`不会使用向下扩展数据段`的**。
  * 如果`W` = 1，说明这个数据段是可写的，否则不可写。所有数据段都是可读的。
  * A位表示该内存段是否已经被CPU访问。
  * 如果`C` = 1，说明这个代码段可以被低优先级的代码访问，比如可以被用户态代码访问。反之如果`C` = 0，说明只能同优先级的代码段可以访问。
  * 如果`R` = 1，说明该代码段可读。代码段是永远没有写权限的。

4. DPL（2-bits, bit 45 和 46）定义了该段的`优先级`。具体数值是0-3。

5. P 标志(bit 47) - 说明该内存段`是否已经存在于内存中`。如果`P` = 0，那么在访问这个内存段的时候将报错。

6. AVL 标志(bit 52) - 这个位在Linux内核中没有被使用。

7. L 标志(bit 53) - 只对代码段有意义，如果`L` = 1，说明该代码段需要运行在`64位模式`下。

8. D/B flag(bit 54) - 根据段描述符描述的是一个可执行代码段、下扩数据段还是一个堆栈段，这个标志具有不同的功能。（对于32位代码和数据段，这个标志应该总是设置为1；对于16位代码和数据段，这个标志被设置为0。）。

  * **`可执行代码段`**。此时这个标志称为D标志并用于指出该段中的指令引用有效地址和操作数的默认长度。如果该标志置位，则默认值是32位地址和32位或8位的操作数；如果该标志为0，则默认值是16位地址和16位或8位的操作数。指令前缀0x66可以用来选择非默认值的操作数大小；前缀0x67可用来选择非默认值的地址大小。
  * **`栈段`**（由SS寄存器指向的数据段）。此时该标志称为B（Big）标志，用于指明隐含堆栈操作（如PUSH、POP或CALL）时的栈指针大小。如果该标志置位，则使用32位栈指针并存放在ESP寄存器中；如果该标志为0，则使用16位栈指针并存放在SP寄存器中。如果堆栈段被设置成一个下扩数据段，这个B标志也同时指定了堆栈段的上界限。
  * **`下扩数据段`**。此时该标志称为B标志，用于指明堆栈段的上界限。如果设置了该标志，则堆栈段的上界限是0xFFFFFFFF（4GB）；如果没有设置该标志，则堆栈段的上界限是0xFFFF（64KB）。

在**保护模式**下，段寄存器保存的不再是一个内存段的基地址，而是一个称为`段选择子`的结构。每个段描述符都对应一个`段选择子`。`段选择子`是一个16位的数据结构，下图显示了这个数据结构的内容：

```
-----------------------------
|       Index    | TI | RPL |
-----------------------------
```

其中，

* **Index** 表示在GDT中，对应段描述符的索引号。
* **TI** 表示要在GDT还是LDT中查找对应的段描述符
* **RPL** 表示请求者优先级。这个优先级将和段描述符中的优先级协同工作，共同确定访问是否合法。

在保护模式下，每个段寄存器实际上包含下面2部分内容：

* 可见部分 - 段选择子
* 隐藏部分 - 段描述符

**在保护模式中，cpu是通过下面的步骤来找到一个具体的物理地址的：**

* 代码必须将相应的`段选择子`装入某个段寄存器
* CPU根据`段选择子`从GDT中找到一个匹配的段描述符，然后将段描述符放入段寄存器的隐藏部分
* 在没有使用向下扩展段的时候，那么内存段的基地址就是`段描述符中的基地址`，段描述符的`limit + 1`就是内存段的长度。如果你知道一个内存地址的`偏移`，那么在没有开启分页机制的情况下，这个内存的物理地址就是`基地址+偏移`

![linear_address](_v_images/20210316131104362_3359.png)

当代码要从实模式进入保护模式的时候，需要执行下面的操作：

* 禁止中断发生
* 使用命令 `lgdt` 将GDT表装入 `GDTR` 寄存器
* 设置CR0寄存器的PE位为1，使CPU进入保护模式
* 跳转开始执行保护模式代码

在后面的章节中，我们将看到Linux 内核中完整的转换代码。不过在系统进入保护模式之前，内核有很多的准备工作需要进行。

让我们打开C文件 [arch/x86/boot/main.c](http://lxr.free-electrons.com/source/arch/x86/boot/main.c?v=3.18)。这个文件包含了很多的函数，这些函数分别会执行键盘初始化，内存堆初始化等等操作...，下面让我们来具体看一些重要的函数。


将启动参数拷贝到"zeropage"
--------------------------------------------------------------------------------


```c
//linux-5.10.13
void main(void) /* 这是无参数 的 no args */
{
	/* First, copy the boot header into the "zeropage" */
	copy_boot_params();

	/* Initialize the early-boot console 初始化控制台 early_serial_console.c */
	console_init();
	if (cmdline_find_option_bool("debug"))
		puts("early console in setup code\n");

	/* End of heap check */
    //在header.S中准备好stack和bss部分之后（请参阅上一部分），内核需要使用 init_heap 函数来初始化堆。
    //堆已初始化，我们可以使用 GET_HEAP 方法使用它
	init_heap();

	/* Make sure we have all the proper CPU support */
    //cpu验证
	if (validate_cpu()) {
		puts("Unable to boot - please use a kernel appropriate "
		     "for your CPU.\n");
		die();
	}

	/* Tell the BIOS what CPU mode we intend to run in. */
    //set_bios_mode 可能会在安装代码发现合适的CPU之后对该函数调用
	set_bios_mode();

	/* Detect memory layout */
    //提供了可用RAM到CPU的映射
    //它使用的内存检测喜欢不同的编程接口0xe820，0xe801和0x88。在这里，我们将仅看到0xE820接口的实现。
	detect_memory();

	/* Set keyboard repeat rate (why?) and query the lock flags */
    //初始化键盘
	keyboard_init();

	/* Query Intel SpeedStep (IST) information */
	query_ist();

	/* Query APM information */
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
    //从BIOS获取高级电源管理信息
	query_apm_bios();
#endif

	/* Query EDD information */
#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
    //从 BIOS 查询 Enhanced Disk Drive 信息
	query_edd();
#endif

	/* Set the video mode */
    //视频模式设置    : arch/x86/boot/video.c
	set_video();

	/* Do the last things and invoke protected mode */
	go_to_protected_mode();
}
```


让我们从`main`函数开始看起，这个函数中，首先调用了[`copy_boot_params(void)`](http://lxr.free-electrons.com/source/arch/x86/boot/main.c?v=3.18#L30)。 

这个函数将内核设置信息拷贝到`boot_params`结构的相应字段。大家可以在[arch/x86/include/uapi/asm/bootparam.h](http://lxr.free-electrons.com/source/arch/x86/include/uapi/asm/bootparam.h?v=3.18#L113)找到`boot_params`结构的定义。

`boot_params`结构中包含`struct setup_header hdr`字段。这个结构包含了[linux boot protocol](https://www.kernel.org/doc/Documentation/x86/boot.txt)中定义的相同字段，并且由boot loader填写。在内核编译的时候`copy_boot_params`完成两个工作：

1. 将[header.S](http://lxr.free-electrons.com/source/arch/x86/boot/header.S?v=3.18#L281)中定义的 `hdr` 结构中的内容拷贝到 `boot_params` 结构的字段 `struct setup_header hdr` 中。

2. 如果内核是通过老的命令行协议运行起来的，那么就更新内核的命令行指针。

这里需要注意的是拷贝 `hdr` 数据结构的 `memcpy` 函数不是C语言中的函数，而是定义在 [copy.S](http://lxr.free-electrons.com/source/arch/x86/boot/copy.S?v=3.18)。让我们来具体分析一下这段代码：

```assembly
GLOBAL(memcpy)
	pushw	%si          ;push si to stack
	pushw	%di          ;push di to stack
	movw	%ax, %di     ;move &boot_param.hdr to di
	movw	%dx, %si     ;move &hdr to si
	pushw	%cx          ;push cx to stack ( sizeof(hdr) )
	shrw	$2, %cx    
	rep; movsl           ;copy based on 4 bytes
	popw	%cx          ;pop cx
	andw	$3, %cx      ;cx = cx % 4
	rep; movsb           ;copy based on one byte
	popw	%di
	popw	%si
	retl
ENDPROC(memcpy)
```

在`copy.S`文件中，你可以看到所有的方法都开始于 `GLOBAL` 宏定义，而结束于 `ENDPROC` 宏定义。 

你可以在 [arch/x86/include/asm/linkage.h](http://lxr.free-electrons.com/source/arch/x86/include/asm/linkage.h?v=3.18)中找到 `GLOBAL` 宏定义。这个宏给代码段分配了一个名字标签，并且让这个名字全局可用。 

```assembly
#define GLOBAL(name)	\
	.globl name;	\
	name:
```

你可以在[include/linux/linkage.h](http://lxr.free-electrons.com/source/include/linux/linkage.h?v=3.18)中找到 `ENDPROC` 宏的定义。 这个宏通过 `END(name)` 代码标识了汇编函数的结束，同时将函数名输出，从而静态分析工具可以找到这个函数。

```assembly
#define ENDPROC(name) \
	.type name, @function ASM_NL \
	END(name)
```

`memcpy` 的实现代码是很容易理解的。首先，代码将 `si` 和 `di` 寄存器的值压入堆栈进行保存，这么做的原因是因为后续的代码将修改 `si` 和 `di` 寄存器的值。`memcpy` 函数（也包括其他定义在copy.s中的其他函数）使用了 `fastcall` 调用规则，意味着所有的函数调用参数是通过 `ax`, `dx`,  `cx`寄存器传入的，而不是传统的通过堆栈传入。因此在使用下面的代码调用 `memcpy` 函数的时候

```c
memcpy(&boot_params.hdr, &hdr, sizeof hdr);
```

函数的参数是这样传递的

* `ax` 寄存器指向 `boot_param.hdr` 的内存地址
* `dx` 寄存器指向 `hdr` 的内存地址
* `cx` 寄存器包含 `hdr` 结构的大小
 
`memcpy` 函数在将 `si` 和 `di` 寄存器压栈之后，将 `boot_param.hdr` 的地址放入 `di` 寄存器，将 `hdr` 的地址放入 `si` 寄存器，并且将 `hdr` 数据结构的大小压栈。 接下来代码首先以4个字节为单位，将 `si` 寄存器指向的内存内容拷贝到 `di` 寄存器指向的内存。当剩下的字节数不足4字节的时候，代码将原始的 `hdr` 数据结构大小出栈放入 `cx` ，然后对 `cx` 的值对4求模，接下来就是根据 `cx` 的值，以字节为单位将 `si` 寄存器指向的内存内容拷贝到 `di` 寄存器指向的内存。当拷贝操作完成之后，将保留的 `si` 以及 `di` 寄存器值出栈，函数返回。

控制台初始化
--------------------------------------------------------------------------------

```c
//它尝试earlyprintk在命令行中找到该选项，如果搜索成功，它将解析串行端口的端口地址和波特率，并初始化串行端口。
void console_init(void) /* 被 arch/x86/boot/main.c:main 调用 */
{
	parse_earlyprintk();

	if (!early_serial_base)
		parse_console_uart8250();
}
```

在 `hdr` 结构体被拷贝到 `boot_params.hdr` 成员之后，系统接下来将进行控制台的初始化。控制台初始化时通过调用[arch/x86/boot/early_serial_console.c](http://lxr.free-electrons.com/source/arch/x86/boot/early_serial_console.c?v=3.18)中定义的 `console_init` 函数实现的。

这个函数首先查看命令行参数是否包含 `earlyprintk` 选项。如果命令行参数包含该选项，那么函数将分析这个选项的内容。得到控制台将使用的串口信息，然后进行串口的初始化。以下是 `earlyprintk` 选项可能的取值：

* serial,0x3f8,115200
* serial,ttyS0,115200
* ttyS0,115200

```c

static void parse_earlyprintk(void)
{
	int baud = DEFAULT_BAUD;
	char arg[32];
	int pos = 0;
	int port = 0;

    //它尝试earlyprintk在命令行中找到该选项，如果搜索成功，它将解析串行端口的端口地址和波特率，并初始化串行端口。
	if (cmdline_find_option("earlyprintk", arg, sizeof(arg)) > 0) {
		char *e;

		if (!strncmp(arg, "serial", 6)) {
			port = DEFAULT_SERIAL_PORT;
			pos += 6;
		}
		...
		/*
		 * make sure we have
		 *	"serial,0x3f8,115200"   串行，0x3f8,115200
		 *	"serial,ttyS0,115200"   序列号，ttyS0,115200
		 *	"ttyS0,115200"          ttyS0,115200
		 */
		if (pos == 7 && !strncmp(arg + pos, "0x", 2)) {
			port = simple_strtoull(arg + pos, &e, 16);
			if (port == 0 || arg + pos == e)
				port = DEFAULT_SERIAL_PORT;
			else
				pos = e - arg;
		} else if (!strncmp(arg + pos, "ttyS", 4)) {
			static const int bases[] = { 0x3f8, 0x2f8 };
			int idx = 0;

			/* += strlen("ttyS"); */
			pos += 4;

			if (arg[pos++] == '1')
				idx = 1;

			port = bases[idx];
		}
		...
	}
	...
}
```

当串口初始化成功之后，如果命令行参数包含 `debug` 选项，我们将看到如下的输出。

```C
if (cmdline_find_option_bool("debug"))
    puts("early console in setup code\n");
```

`puts` 函数定义在[tty.c](http://lxr.free-electrons.com/source/arch/x86/boot/tty.c?v=3.18)。这个函数只是简单的调用 `putchar` 函数将输入字符串中的内容按字节输出。下面让我们来看看  `putchar`函数的实现：

```C
void __attribute__((section(".inittext"))) putchar(int ch)
{
    if (ch == '\n')
        putchar('\r');

    bios_putchar(ch);

    if (early_serial_base != 0)
        serial_putchar(ch);
}
```

`__attribute__((section(".inittext")))` 说明这段代码将被放入 `.inittext` 代码段。关于 `.inittext` 代码段的定义你可以在 [setup.ld](http://lxr.free-electrons.com/source/arch/x86/boot/setup.ld?v=3.18#L19)中找到。

如果需要输出的字符是 `\n` ，那么 `putchar` 函数将调用自己首先输出一个字符 `\r`。接下来，就调用 `bios_putchar` 函数将字符输出到显示器（使用bios int10中断）：

```C
static void __attribute__((section(".inittext"))) bios_putchar(int ch)
{
    struct biosregs ireg;

    initregs(&ireg);
    ireg.bx = 0x0007;
    ireg.cx = 0x0001;
    ireg.ah = 0x0e;
    ireg.al = ch;
    intcall(0x10, &ireg, NULL);
}
```

在上面的代码中 `initreg` 函数接受一个 `biosregs` 结构的地址作为输入参数，该函数首先调用 `memset` 函数将 `biosregs` 结构体所有成员清0。

```C
    memset(reg, 0, sizeof *reg);
    reg->eflags |= X86_EFLAGS_CF;
    reg->ds = ds();
    reg->es = ds();
    reg->fs = fs();
    reg->gs = gs();
```

下面让我们来看看[memset](http://lxr.free-electrons.com/source/arch/x86/boot/copy.S?v=3.18#L36)函数的实现 :

```assembly
GLOBAL(memset)
    pushw   %di
    movw    %ax, %di
    movzbl  %dl, %eax
    imull   $0x01010101,%eax
    pushw   %cx
    shrw    $2, %cx
    rep; stosl
    popw    %cx
    andw    $3, %cx
    rep; stosb
    popw    %di
    retl
ENDPROC(memset)
```

首先你会发现，`memset` 函数和 `memcpy` 函数一样使用了 `fastcall` 调用规则，因此函数的参数是通过 `ax`，`dx` 以及 `cx` 寄存器传入函数内部的。

就像memcpy函数一样，`memset` 函数一开始将 `di` 寄存器入栈，然后将 `biosregs` 结构的地址从 `ax` 寄存器拷贝到`di`寄存器。接下来，使用 `movzbl` 指令将 `dl` 寄存器的内容拷贝到 `ax` 寄存器的低字节，到这里 `ax` 寄存器就包含了需要拷贝到 `di` 寄存器所指向的内存的值。

接下来的 `imull` 指令将 `eax` 寄存器的值乘上 `0x01010101`。这么做的原因是代码每次将尝试拷贝4个字节内存的内容。下面让我们来看一个具体的例子，假设我们需要将 `0x7` 这个数值放到内存中，在执行 `imull` 指令之前，`eax` 寄存器的值是 `0x7`，在 `imull` 指令被执行之后，`eax` 寄存器的内容变成了 `0x07070707`（4个字节的 `0x7`）。在 `imull` 指令之后，代码使用 `rep; stosl` 指令将 `eax` 寄存器的内容拷贝到 `es:di` 指向的内存。

在 `bisoregs` 结构体被 `initregs` 函数正确填充之后，`bios_putchar` 调用中断 [0x10](http://www.ctyme.com/intr/rb-0106.htm) 在显示器上输出一个字符。接下来 `putchar` 函数检查是否初始化了串口，如果串口被初始化了，那么将调用[serial_putchar](http://lxr.free-electrons.com/source/arch/x86/boot/tty.c?v=3.18#L30)将字符输出到串口。

堆初始化
--------------------------------------------------------------------------------

```c
//在header.S中准备好stack和bss部分之后（请参阅上一部分），内核需要使用 init_heap 函数来初始化堆。
static void init_heap(void)
{
	char *stack_end;

    //检查CAN_USE_HEAP标志
	if (boot_params.hdr.loadflags & CAN_USE_HEAP) {

        //并loadflags在设置了该标志的情况下计算堆栈的结尾
        //stack_end = esp - STACK_SIZE
		asm("leal %P1(%%esp),%0"
		    : "=r" (stack_end) : "i" (-STACK_SIZE/* 1024 */));

        //heap_end = heap_end_ptr(=_end) + 512（0x200h）
		heap_end = (char *)
			((size_t)boot_params.hdr.heap_end_ptr + 0x200);
		if (heap_end > stack_end)
			heap_end = stack_end;
	} else {
		/* Boot protocol 2.00 only, no heap available */
		puts("WARNING: Ancient bootloader, some functionality "
		     "may be limited!\n");
	}
}
```

当堆栈和bss段在[header.S](http://lxr.free-electrons.com/source/arch/x86/boot/header.S?v=3.18)中被初始化之后 (细节请参考上一篇[part](linux-bootstrap-1.md))， 内核需要初始化全局堆，全局堆的初始化是通过 [`init_heap`](http://lxr.free-electrons.com/source/arch/x86/boot/main.c?v=3.18#L116) 函数实现的。

代码首先检查内核设置头中的[`loadflags`](http://lxr.free-electrons.com/source/arch/x86/boot/header.S?v=3.18#L321)是否设置了 [`CAN_USE_HEAP`](http://lxr.free-electrons.com/source/arch/x86/include/uapi/asm/bootparam.h?v=3.18#L21)标志。 如果该标记被设置了，那么代码将计算堆栈的结束地址：:

```C
    char *stack_end;
    
    //%P1 is (-STACK_SIZE)
    if (boot_params.hdr.loadflags & CAN_USE_HEAP) {
        asm("leal %P1(%%esp),%0"
            : "=r" (stack_end) : "i" (-STACK_SIZE));
```

换言之`stack_end = esp - STACK_SIZE`.

在计算了堆栈结束地址之后，代码计算了堆的结束地址：

```c
    //heap_end = heap_end_ptr + 512
    heap_end = (char *)((size_t)boot_params.hdr.heap_end_ptr + 0x200);
```

接下来代码判断 `heap_end` 是否大于 `stack_end`，如果条件成立，将 `stack_end` 设置成 `heap_end`（这么做是因为在大部分系统中全局堆和堆栈是相邻的，但是增长方向是相反的）。

到这里为止，全局堆就被正确初始化了。在全局堆被初始化之后，我们就可以使用 `GET_HEAP` 方法。至于这个函数的实现和使用，我们将在后续的章节中看到。

检查CPU类型
--------------------------------------------------------------------------------

```c
//cpu验证
int validate_cpu(void)
{
	u32 *err_flags;
	int cpu_level, req_level;//cpu级别和所需的cpu级别

    //将cpu级别和所需的cpu级别传递给该函数，并检查内核是否在正确的cpu级别上启动
	check_cpu(&cpu_level, &req_level, &err_flags);

	if (cpu_level < req_level) {
		printf("This kernel requires an %s CPU, ",
		       cpu_name(req_level));
		printf("but only detected an %s CPU.\n",
		       cpu_name(cpu_level));
		return -1;
	}

	if (err_flags) {
		puts("This kernel requires the following features "
		     "not present on the CPU:\n");
		show_cap_strs(err_flags);
		putchar('\n');
		return -1;
	} else if (check_knl_erratum()) {
		return -1;
	} else {
		return 0;
	}
}
```

在堆栈初始化之后，内核代码通过调用[arch/x86/boot/cpu.c](http://lxr.free-electrons.com/source/arch/x86/boot/cpu.c?v=3.18)提供的 `validate_cpu` 方法检查CPU级别以确定系统是否能够在当前的CPU上运行。

`validate_cpu` 调用了[`check_cpu`](http://lxr.free-electrons.com/source/arch/x86/boot/cpucheck.c?v=3.18#L102)方法得到当前系统的CPU级别，并且和系统预设的最低CPU级别进行比较。如果不满足条件，则不允许系统运行。

```c
/*from cpu.c*/
check_cpu(&cpu_level, &req_level, &err_flags);
/*after check_cpu call, req_level = req_level defined in cpucheck.c*/
if (cpu_level < req_level) {
    printf("This kernel requires an %s CPU, ", cpu_name(req_level)); 
    printf("but only detected an %s CPU.\n", cpu_name(cpu_level));
    return -1;
}
```

除此之外，`check_cpu` 方法还做了大量的其他检测和设置工作，下面就简单介绍一些：

* 1）检查cpu标志，如果cpu是64位cpu，那么就设置[long mode](http://en.wikipedia.org/wiki/Long_mode), 
* 2) 检查CPU的制造商，根据制造商的不同，设置不同的CPU选项。比如对于AMD出厂的cpu，如果不支持 `SSE+SSE2`，那么就禁止这些选项。

内存分布侦测
--------------------------------------------------------------------------------

```c
/* Detect memory layout */
//提供了可用RAM到CPU的映射
//它使用的内存检测喜欢不同的编程接口 0xe820 ， 0xe801 和 0x88
void detect_memory(void)
{
	detect_memory_e820();   //arch/x86/boot/memory.c
	detect_memory_e801();
	detect_memory_88();
}
```

接下来，内核调用 `detect_memory` 方法进行内存侦测，以得到系统当前内存的使用分布。该方法使用多种编程接口，包括 `0xe820`（获取全部内存分配），`0xe801` 和 `0x88`（获取临近内存大小），进行内存分布侦测。在这里我们只介绍[arch/x86/boot/memory.c](http://lxr.free-electrons.com/source/arch/x86/boot/memory.c?v=3.18)中提供的 `detect_memory_e820` 方法。

```c


#define SMAP	0x534d4150	/* ASCII "SMAP" */

static void detect_memory_e820(void)
{
	int count = 0;
	struct biosregs ireg, oreg;
	struct boot_e820_entry *desc = boot_params.e820_table;
	static struct boot_e820_entry buf; /* static so it is zeroed */

    //初始化biosregs
	initregs(&ireg);

    //为0xe820调用填充特殊值的寄存器
    //ax 包含函数的编号（本例中为0xe820）
    //cx 包含缓冲区的大小，该缓冲区将包含有关内存的数据
    //edx 必须包含SMAP幻数
    //es:di 必须包含将包含内存数据的缓冲区的地址
    //ebx 必须为零。
	ireg.ax  = 0xe820;
	ireg.cx  = sizeof(buf);
	ireg.edx = SMAP;
	ireg.di  = (size_t)&buf;

	/*
	 * Note: at least one BIOS is known which assumes that the
	 * buffer pointed to by one e820 call is the same one as
	 * the previous call, and only changes modified fields.  Therefore,
	 * we use a temporary buffer and copy the results entry by entry.
	 *
	 * This routine deliberately does not try to account for
	 * ACPI 3+ extended attributes.  This is because there are
	 * BIOSes in the field which report zero for the valid bit for
	 * all ranges, and we don't currently make any use of the
	 * other attribute bits.  Revisit this if we see the extended
	 * attribute bits deployed in a meaningful way in the future.
	 */
    //接下来是一个循环，收集有关内存的数据
	do {
        //它始于对 0x15 BIOS中断的调用，该中断从地址分配表中写入一行。
        //为了获得下一行，我们需要再次调用此中断（在循环中进行此操作）
		intcall(0x15, &ireg, &oreg);

        //在下一次调用之前，ebx必须包含先前返回的值
		ireg.ebx = oreg.ebx; /* for next iteration... */

		/* BIOSes which terminate the chain with CF = 1 as opposed
		   to %ebx = 0 don't always report the SMAP signature on
		   the final, failing, probe. */
		if (oreg.eflags & X86_EFLAGS_CF)
			break;

		/* Some BIOSes stop returning SMAP in the middle of
		   the search loop.  We don't know exactly how the BIOS
		   screwed up the map at that point, we might have a
		   partial map, the full map, or complete garbage, so
		   just return failure. */
		if (oreg.eax != SMAP) {
			count = 0;
			break;
		}

		*desc++ = buf;
		count++;
	} while (ireg.ebx && count < ARRAY_SIZE(boot_params.e820_table));

    //从地址分配表中收集数据，并将此数据写入e820_entry数组
    //内存段的开始
    //内存段的大小
    //内存段的类型（特定段是可用的还是保留的）
    //$ sudo cat /var/log/messages | grep BIOS-e820
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x000000000009fc00-0x000000000009ffff] reserved
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x00000000000f0000-0x00000000000fffff] reserved
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x0000000000100000-0x00000000bff7ffff] usable
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x00000000bff80000-0x00000000bfffffff] reserved
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x00000000feffc000-0x00000000feffffff] reserved
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x00000000fffc0000-0x00000000ffffffff] reserved
    //Mar  2 08:58:32 localhost kernel: BIOS-e820: [mem 0x0000000100000000-0x000000023fffffff] usable
	boot_params.e820_entries = count;
}
```

该方法首先调用 `initregs` 方法初始化 `biosregs` 数据结构，然后向该数据结构填入 `0xe820` 编程接口所要求的参数：

```assembly
    initregs(&ireg);
    ireg.ax  = 0xe820;
    ireg.cx  = sizeof buf;
    ireg.edx = SMAP;
    ireg.di  = (size_t)&buf;
```

* `ax` 固定为 `0xe820`
* `cx` 包含数据缓冲区的大小，该缓冲区将包含系统内存的信息数据
* `edx` 必须是 `SMAP` 这个魔术数字，就是 `0x534d4150`
* `es:di` 包含数据缓冲区的地址
* `ebx` 必须为0.

接下来就是通过一个循环来收集内存信息了。每个循环都开始于一个 `0x15` 中断调用，这个中断调用返回地址分配表中的一项，接着程序将返回的 `ebx` 设置到 `biosregs` 数据结构中，然后进行下一次的 `0x15` 中断调用。那么循环什么时候结束呢？直到 `0x15` 调用返回的eflags包含标志 `X86_EFLAGS_CF`:

```C
do {
    //它始于对 0x15 BIOS中断的调用，该中断从地址分配表中写入一行。
    //为了获得下一行，我们需要再次调用此中断（在循环中进行此操作）
	intcall(0x15, &ireg, &oreg);

    //在下一次调用之前，ebx必须包含先前返回的值
	ireg.ebx = oreg.ebx; /* for next iteration... */

	/* BIOSes which terminate the chain with CF = 1 as opposed
	   to %ebx = 0 don't always report the SMAP signature on
	   the final, failing, probe. */
	if (oreg.eflags & X86_EFLAGS_CF)
		break;

	/* Some BIOSes stop returning SMAP in the middle of
	   the search loop.  We don't know exactly how the BIOS
	   screwed up the map at that point, we might have a
	   partial map, the full map, or complete garbage, so
	   just return failure. */
	if (oreg.eax != SMAP) {
		count = 0;
		break;
	}

	*desc++ = buf;
	count++;
} while (ireg.ebx && count < ARRAY_SIZE(boot_params.e820_table));
```

在循环结束之后，整个内存分配信息将被写入到 `e820entry` 数组中，这个数组的每个元素包含下面3个信息:

```c
boot_params.e820_table;
/*
 * The E820 memory region entry of the boot protocol ABI:
 */
struct boot_e820_entry {
	__u64 addr;
	__u64 size;
	__u32 type;
} __attribute__((packed));
```

* 内存段的起始地址
* 内存段的大小
* 内存段的类型（类型可以是reserved, usable等等)。

你可以在 `dmesg` 输出中看到这个数组的内容：

```
[    0.000000] e820: BIOS-provided physical RAM map:
[    0.000000] BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable
[    0.000000] BIOS-e820: [mem 0x000000000009fc00-0x000000000009ffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000000f0000-0x00000000000fffff] reserved
[    0.000000] BIOS-e820: [mem 0x0000000000100000-0x000000003ffdffff] usable
[    0.000000] BIOS-e820: [mem 0x000000003ffe0000-0x000000003fffffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000fffc0000-0x00000000ffffffff] reserved
```

键盘初始化
--------------------------------------------------------------------------------

```c
/*
 * Query the keyboard lock status as given by the BIOS, and
 * set the keyboard repeat rate to maximum.  Unclear why the latter
 * is done here; this might be possible to kill off as stale code.
 */
static void keyboard_init(void)
{
	struct biosregs ireg, oreg;
	initregs(&ireg);

	ireg.ah = 0x02;		/* Get keyboard status */

    //调用0x16中断以查询键盘的状态
	intcall(0x16, &ireg, &oreg);
	boot_params.kbd_status = oreg.al;

    //再次调用0x16来设置重复率和延迟
	ireg.ax = 0x0305;	/* Set keyboard repeat rate */
	intcall(0x16, &ireg, NULL);
}
```

接下来内核调用[`keyboard_init()`](http://lxr.free-electrons.com/source/arch/x86/boot/main.c?v=3.18#L65) 方法进行键盘初始化操作。 首先，方法调用`initregs`初始化寄存器结构，然后调用[0x16](http://www.ctyme.com/intr/rb-1756.htm)中断来获取键盘状态。

```c
    initregs(&ireg);
    ireg.ah = 0x02;     /* Get keyboard status */
    intcall(0x16, &ireg, &oreg);
    boot_params.kbd_status = oreg.al;
```

在获取了键盘状态之后，代码再次调用[0x16](http://www.ctyme.com/intr/rb-1757.htm)中断来设置键盘的按键检测频率。

```c
    ireg.ax = 0x0305;   /* Set keyboard repeat rate */
    intcall(0x16, &ireg, NULL);
```

系统参数查询
--------------------------------------------------------------------------------

接下来内核将进行一系列的参数查询。我们在这里将不深入介绍所有这些查询，我们将在后续章节中再进行详细介绍。在这里我们将简单介绍一些系统参数查询:

[query_mca](http://lxr.free-electrons.com/source/arch/x86/boot/mca.c?v=3.18#L18) 方法调用[0x15](http://www.ctyme.com/intr/rb-1594.htm)中断来获取机器的型号信息，BIOS版本以及其他一些硬件相关的属性：

```c
struct sys_desc_table {
	__u16 length;
	__u8  table[14];
};
int query_mca(void)
{
    struct biosregs ireg, oreg;
    u16 len;

    initregs(&ireg);
    ireg.ah = 0xc0;
    intcall(0x15, &ireg, &oreg);

    if (oreg.eflags & X86_EFLAGS_CF)
        return -1;  /* No MCA present */

    set_fs(oreg.es);
    len = rdfs16(oreg.bx);

    if (len > sizeof(boot_params.sys_desc_table))
        len = sizeof(boot_params.sys_desc_table);

    copy_from_fs(&boot_params.sys_desc_table, oreg.bx, len);
    return 0;
}
```
在最新的Linux-5.10.13内核中，该函数不复存在？？

这个方法设置 `ah` 寄存器的值为 `0xc0`，然后调用 `0x15` BIOS中断。中断返回之后代码检查 [carry flag](http://en.wikipedia.org/wiki/Carry_flag)。如果它被置位，说明BIOS不支持[**MCA**](https://en.wikipedia.org/wiki/Micro_Channel_architecture)。如果CF被设置成0，那么 `ES:BX` 指向系统信息表。这个表的内容如下所示：

```
Offset  Size    Description
 00h    WORD    number of bytes following
 02h    BYTE    model (see #00515)
 03h    BYTE    submodel (see #00515)
 04h    BYTE    BIOS revision: 0 for first release, 1 for 2nd, etc.
 05h    BYTE    feature byte 1 (see #00510)
 06h    BYTE    feature byte 2 (see #00511)
 07h    BYTE    feature byte 3 (see #00512)
 08h    BYTE    feature byte 4 (see #00513)
 09h    BYTE    feature byte 5 (see #00514)
---AWARD BIOS---
 0Ah  N BYTEs   AWARD copyright notice
---Phoenix BIOS---
 0Ah    BYTE    ??? (00h)
 0Bh    BYTE    major version
 0Ch    BYTE    minor version (BCD)
 0Dh  4 BYTEs   ASCIZ string "PTL" (Phoenix Technologies Ltd)
---Quadram Quad386---
 0Ah 17 BYTEs   ASCII signature string "Quadram Quad386XT"
---Toshiba (Satellite Pro 435CDS at least)---
 0Ah  7 BYTEs   signature "TOSHIBA"
 11h    BYTE    ??? (8h)
 12h    BYTE    ??? (E7h) product ID??? (guess)
 13h  3 BYTEs   "JPN"
```

接下来代码调用 `set_fs` 方法，将 `es` 寄存器的值写入 `fs` 寄存器:

```c
static inline void set_fs(u16 seg)
{
    asm volatile("movw %0,%%fs" : : "rm" (seg));
}
```

在[boot.h](http://lxr.free-electrons.com/source/arch/x86/boot/boot.h?v=3.18) 存在很多类似于 `set_fs` 的方法, 比如 `set_gs`。

在 `query_mca` 的最后，代码将 `es:bx` 指向的内存地址的内容拷贝到 `boot_params.sys_desc_table`。

接下来，内核调用 `query_ist` 方法获取[Intel SpeedStep](http://en.wikipedia.org/wiki/SpeedStep)信息。这个方法首先检查CPU类型，然后调用 `0x15` 中断获得这个信息并放入 `boot_params` 中。


```c
/*
 * Get Intel SpeedStep (IST) information.
 */
static void query_ist(void)
{
	struct biosregs ireg, oreg;

	/* Some older BIOSes apparently crash on this call, so filter
	   it from machines too old to have SpeedStep at all. */
	   //检查CPU级别
	if (cpu.level < 6)
		return;

    //检查CPU级别，如果正确，则调用0x15以获取信息并将结果保存到boot_params
	initregs(&ireg);
	ireg.ax  = 0xe980;	 /* IST Support */
	ireg.edx = 0x47534943;	 /* Request value */
	intcall(0x15, &ireg, &oreg);

	boot_params.ist_info.signature  = oreg.eax;
	boot_params.ist_info.command    = oreg.ebx;
	boot_params.ist_info.event      = oreg.ecx;
	boot_params.ist_info.perf_level = oreg.edx;
}
```


接下来，内核会调用[query_apm_bios](http://lxr.free-electrons.com/source/arch/x86/boot/apm.c?v=3.18#L21) 方法从BIOS获得 [高级电源管理](http://en.wikipedia.org/wiki/Advanced_Power_Management) 信息。`query_apm_bios` 也是调用 `0x15` 中断，只不过将 `ax` 设置成 `0x5300` 以得到APM设置信息。中断调用返回之后，代码将检查 `bx` 和 `cx` 的值，如果 `bx` 不是 `0x504d` ( PM 标记 )，或者 `cx` 不是 `0x02` (0x02，表示支持32位模式)，那么代码直接返回错误。否则，将进行下面的步骤。

```c

//从BIOS获取高级电源管理信息
int query_apm_bios(void)
{
	struct biosregs ireg, oreg;

	/* APM BIOS installation check */
	initregs(&ireg);

    //调用0x15BIOS中断，但带有ah=0x53来检查APM安装
	ireg.ah = 0x53;
	intcall(0x15, &ireg, &oreg);

    //检查进位标志（如果APM支持，则必须为0 ）和
	if (oreg.flags & X86_EFLAGS_CF)
		return -1;		/* No APM BIOS */

    //检查PM签名（必须为0x504d）
	if (oreg.bx != 0x504d)		/* "PM" signature */
		return -1;

    //检查cx寄存器的值（如果为0x02，则支持保护模式接口）
	if (!(oreg.cx & 0x02))		/* 32 bits supported? */
		return -1;

    //接下来，它0x15再次调用，但是使用ax = 0x5304 来断开APM接口并连接32位保护模式接口。
    //最后，它填充boot_params.apm_bios_info了从BIOS获得的值。

	/* Disconnect first, just in case */
	ireg.al = 0x04;
	intcall(0x15, &ireg, NULL);

	/* 32-bit connect */
	ireg.al = 0x03;
	intcall(0x15, &ireg, &oreg);

	boot_params.apm_bios_info.cseg        = oreg.ax;
	boot_params.apm_bios_info.offset      = oreg.ebx;
	boot_params.apm_bios_info.cseg_16     = oreg.cx;
	boot_params.apm_bios_info.dseg        = oreg.dx;
	boot_params.apm_bios_info.cseg_len    = oreg.si;
	boot_params.apm_bios_info.cseg_16_len = oreg.hsi;
	boot_params.apm_bios_info.dseg_len    = oreg.di;

	if (oreg.flags & X86_EFLAGS_CF)
		return -1;

	/* Redo the installation check as the 32-bit connect;
	   some BIOSes return different flags this way... */

	ireg.al = 0x00;
	intcall(0x15, &ireg, &oreg);

	if ((oreg.eflags & X86_EFLAGS_CF) || oreg.bx != 0x504d) {
		/* Failure with 32-bit connect, try to disconnect and ignore */
		ireg.al = 0x04;
		intcall(0x15, &ireg, NULL);
		return -1;
	}

	boot_params.apm_bios_info.version = oreg.ax;
	boot_params.apm_bios_info.flags   = oreg.cx;
	return 0;
}
```

接下来，代码使用 `ax = 0x5304` 来调用 `0x15` 中断，以断开 `APM` 接口；然后使用 `ax = 0x5303` 调用 `0x15` 中断，使用32位接口重新连接 `APM`；最后使用 `ax = 0x5300` 调用 `0x15` 中断再次获取APM设置，然后将信息写入 `boot_params.apm_bios_info`。

需要注意的是，只有在 `CONFIG_APM` 或者 `CONFIG_APM_MODULE` 被设置的情况下，`query_apm_bios` 方法才会被调用：

```C
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
    query_apm_bios();
#endif
```

最后是[`query_edd`](http://lxr.free-electrons.com/source/arch/x86/boot/edd.c?v=3.18#L122) 方法调用, 这个方法从BIOS中查询 `Enhanced Disk Drive` 信息。下面让我们看看 `query_edd` 方法的实现。

```c
//从 BIOS 查询 Enhanced Disk Drive 信息
void query_edd(void)
{
	char eddarg[8];
	int do_mbr = 1;
#ifdef CONFIG_EDD_OFF
	int do_edd = 0;
#else
	int do_edd = 1;
#endif
	int be_quiet;
	int devno;
	struct edd_info ei, *edp;
	u32 *mbrptr;

	if (cmdline_find_option("edd", eddarg, sizeof(eddarg)) > 0) {
		if (!strcmp(eddarg, "skipmbr") || !strcmp(eddarg, "skip")) {
			do_edd = 1;
			do_mbr = 0;
		}
		else if (!strcmp(eddarg, "off"))
			do_edd = 0;
		else if (!strcmp(eddarg, "on"))
			do_edd = 1;
	}

	be_quiet = cmdline_find_option_bool("quiet");

	edp    = boot_params.eddbuf;
	mbrptr = boot_params.edd_mbr_sig_buffer;

    //从内核的命令行读取edd选项，如果将其设置为off, 则query_edd仅返回。
	if (!do_edd)
		return;

	/* Bugs in OnBoard or AddOnCards Bios may hang the EDD probe,
	 * so give a hint if this happens.
	 */

	if (!be_quiet)
		printf("Probing EDD (edd=off to disable)... ");

    //遍历BIOS支持的硬盘并在以下循环中查询EDD信息
	for (devno = 0x80; devno < 0x80+EDD_MBR_SIG_MAX; devno++) {
		/*
		 * Scan the BIOS-supported hard disks and query EDD
		 * information...
		 */
		//查询EDD信息(Enhanced Disk Drive)
		if (!get_edd_info(devno, &ei)
		    && boot_params.eddbuf_entries < EDDMAXNR) {
			memcpy(edp, &ei, sizeof(ei));
			edp++;
			boot_params.eddbuf_entries++;
		}

		if (do_mbr && !read_mbr_sig(devno, &ei, mbrptr++))
			boot_params.edd_mbr_sig_buf_entries = devno-0x80+1;
	}

	if (!be_quiet)
		printf("ok\n");
}
```
首先，代码检查内核命令行参数是否设置了[edd](http://lxr.free-electrons.com/source/Documentation/kernel-parameters.txt?v=3.18#L1023) 选项，如果edd选项设置成 `off`，`query_edd` 不做任何操作，直接返回。

如果EDD被激活了，`query_edd` 遍历所有BIOS支持的硬盘，并获取相应硬盘的EDD信息：

```C
for (devno = 0x80; devno < 0x80+EDD_MBR_SIG_MAX; devno++) {
    if (!get_edd_info(devno, &ei) && boot_params.eddbuf_entries < EDDMAXNR) {
        memcpy(edp, &ei, sizeof ei);
        edp++;
        boot_params.eddbuf_entries++;
    }
    ...
    ...
    ...
```

在代码中 `0x80` 是第一块硬盘，`EDD_MBR_SIG_MAX` 是一个宏，值为16。代码把获得的信息放入数组[edd_info](http://lxr.free-electrons.com/source/include/uapi/linux/edd.h?v=3.18#L172)中。`get_edd_info` 方法通过调用 `0x13` 中断调用（设置 `ah = 0x41` ) 来检查EDD是否被硬盘支持。如果EDD被支持，代码将再次调用 `0x13` 中断，在这次调用中 `ah = 0x48`，并且 `si` 指向一个数据缓冲区地址。中断调用之后，EDD信息将被保存到 `si` 指向的缓冲区地址。

结束语
--------------------------------------------------------------------------------

本章到此就结束了，在下一章我们将讲解显示模式设置，以及在进入保护模式之前的其他准备工作，在下一章的最后我们将成功进入保护模式。

如果你有任何的问题或者建议，你可以留言，也可以直接发消息给我[twitter](https://twitter.com/0xAX).

**如果你发现文中描述有任何问题，请提交一个 PR 到 [linux-insides-zh](https://github.com/MintCN/linux-insides-zh) 。**

相关链接
--------------------------------------------------------------------------------

* [Protected mode](http://en.wikipedia.org/wiki/Protected_mode)
* [Protected mode](http://wiki.osdev.org/Protected_Mode)
* [Long mode](http://en.wikipedia.org/wiki/Long_mode)
* [Nice explanation of CPU Modes with code](http://www.codeproject.com/Articles/45788/The-Real-Protected-Long-mode-assembly-tutorial-for)
* [How to Use Expand Down Segments on Intel 386 and Later CPUs](http://www.sudleyplace.com/dpmione/expanddown.html)
* [earlyprintk documentation](http://lxr.free-electrons.com/source/Documentation/x86/earlyprintk.txt?v=3.18)
* [Kernel Parameters](http://lxr.free-electrons.com/source/Documentation/kernel-parameters.txt?v=3.18)
* [Serial console](http://lxr.free-electrons.com/source/Documentation/serial-console.txt?v=3.18)
* [Intel SpeedStep](http://en.wikipedia.org/wiki/SpeedStep)
* [APM](https://en.wikipedia.org/wiki/Advanced_Power_Management)
* [EDD specification](http://www.t13.org/documents/UploadedDocuments/docs2004/d1572r3-EDD3.pdf)
* [TLDP documentation for Linux Boot Process](http://www.tldp.org/HOWTO/Linux-i386-Boot-Code-HOWTO/setup.html) (old)
* [Previous Part](linux-bootstrap-1.md)
* [BIOS Interrupt](http://wiki.osdev.org/BIOS)
