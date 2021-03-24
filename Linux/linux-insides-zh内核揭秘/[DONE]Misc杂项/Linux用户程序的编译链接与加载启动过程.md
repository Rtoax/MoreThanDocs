<center><font size='6'>Linux用户程序的编译链接与加载启动过程</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

# 1. 程序的编译链接

## 1.1. 介绍

如果我们打开维基百科的 `链接器` 页，我们将会看到如下定义：

>在计算机科学中，链接器（英文：Linker），是一个计算机程序，它将一个或多个由编译器生成的目标文件链接为一个单独的可执行文件，库文件或者另外一个目标文件

如果你曾经用 C 写过至少一个程序，那你就已经见过以 `*.o` 扩展名结尾的文件了。这些文件是[目标文件](https://en.wikipedia.org/wiki/Object_file)。目标文件是一块块的机器码和数据，其数据包含了引用其他目标文件或库的数据和函数的占位符地址，也包括其自身的函数和数据列表。链接器的主要目的就是收集/处理每一个目标文件的代码和数据，将它们转成最终的可执行文件或者库。在这篇文章里，我们会试着研究这个流程的各个方面。开始吧。

## 1.2. 链接流程


让我们按以下结构创建一个项目：

```
*-linkers
*--main.c
*--lib.c
*--lib.h
```

我们的 `main.c` 源文件包含了：

```C
#include <stdio.h>

#include "lib.h"

int main(int argc, char **argv) {
	printf("factorial of 5 is: %d\n", factorial(5));
	return 0;
}
```

`lib.c` 文件包含了：

```C
int factorial(int base) {
	int res,i = 1;
	
	if (base == 0) {
		return 1;
	}

	while (i <= base) {
		res *= i;
		i++;
	}

	return res;
}
```

`lib.h` 文件包含了：

```C
#ifndef LIB_H
#define LIB_H

int factorial(int base);

#endif
```

现在让我们用以下命令单独编译 `main.c` 源码：

```
$ gcc -c main.c
```

如果我们用 `nm` 工具查看输出的目标文件，我们将会看到如下输出：

```
$ nm -A main.o
main.o:                 U factorial
main.o:0000000000000000 T main
main.o:                 U printf
```
`nm` 工具让我们能够看到给定目标文件的符号表列表。其包含了三列：第一列是该目标文件的名称和解析得到的符号地址。第二列包含了一个表示该符号状态的字符。这里 `U` 表示 `未定义`， `T` 表示该符号被置于 `.text` 段。在这里， `nm` 工具向我们展示了 `main.c` 文件里包含的三个符号：

* `factorial` - 在 `lib.c` 文件中定义的阶乘函数。因为我们只编译了 `main.c`，所以其不知道任何有关 `lib.c` 文件的事；
* `main` - 主函数;
* `printf` - 来自 [glibc](https://en.wikipedia.org/wiki/GNU_C_Library) 库的函数。 `main.c` 同样不知道任何与其相关的事。

目前我们可以从 `nm` 的输出中了解哪些事情呢？ `main.o` 目标文件包含了在地址 `0000000000000000` 处的本地变量 `main` （在被链接后其将会被赋予正确的地址），以及两个无法解析的符号。我们可以从 `main.o` 的反汇编输出中了解这些信息：

```
$ objdump -S main.o

main.o:     file format elf64-x86-64
Disassembly of section .text:

0000000000000000 <main>:
   0:	55                   	push   %rbp
   1:	48 89 e5             	mov    %rsp,%rbp
   4:	48 83 ec 10          	sub    $0x10,%rsp
   8:	89 7d fc             	mov    %edi,-0x4(%rbp)
   b:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
   f:	bf 05 00 00 00       	mov    $0x5,%edi
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  19:	89 c6                	mov    %eax,%esi
  1b:	bf 00 00 00 00       	mov    $0x0,%edi
  20:	b8 00 00 00 00       	mov    $0x0,%eax
  25:	e8 00 00 00 00       	callq  2a <main+0x2a>
  2a:	b8 00 00 00 00       	mov    $0x0,%eax
  2f:	c9                   	leaveq 
  30:	c3                   	retq   
```

这里我们只关注两个 `callq` 操作。这两个 `callq` 操作包含了 `链接器存根`，或者函数的名称和其相对当前的下一条指令的偏移。这些存根将会被更新到函数的真实地址。我们可以在下面的 `objdump` 输出看到这些函数的名字：

```
$ objdump -S -r main.o

...
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  15: R_X86_64_PC32	               factorial-0x4
  19:	89 c6                	mov    %eax,%esi
...
  25:	e8 00 00 00 00       	callq  2a <main+0x2a>
  26:   R_X86_64_PC32	               printf-0x4
  2a:	b8 00 00 00 00       	mov    $0x0,%eax
...
```

`objdump` 工具中的 `-r` 或 `--reloc ` 选项会打印文件的 `重定位` 条目。现在让我们更加深入重定位流程。

## 1.3. 重定位

重定位是连接符号引用和符号定义的流程。让我们看看前一段 `objdump` 的输出：

```
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  15:   R_X86_64_PC32	               factorial-0x4
  19:	89 c6                	mov    %eax,%esi
```

注意第一行的 `e8 00 00 00 00` 。`e8` 是 `call` 的 [操作码](https://en.wikipedia.org/wiki/Opcode) ，这一行的剩余部分是一个相对偏移。所以 `e8 00 00 00` 包含了一个单字节操作码，跟着一个四字节地址。注意 `00 00 00 00` 是 4 个字节。为什么只有 4 字节 而不是 `x86_64` 64 位机器上的 8 字节地址？其实我们用了 `-mcmodel=small` 选项来编译 `main.c` ！从 `gcc` 的指南上看：

```
-mcmodel=small

为小代码模型生成代码: 目标程序及其符号必须被链接到低于 2GB 的地址空间。指针是 64 位的。程序可以被动态或静态的链接。这是默认的代码模型。
```

当然，我们在编译时并没有将这一选项传给 `gcc` ，但是这是默认的。从上面摘录的 `gcc` 指南我们知道，我们的程序会被链接到低于 2 GB 的地址空间。因此 4 字节已经足够。所以我们有了 `call` 指令和一个未知的地址。当我们编译 `main.c` 以及它的依赖形成一个可执行文件时，关注阶乘函数的调用，我们看到：

```
$ gcc main.c lib.c -o factorial | objdump -S factorial | grep factorial

factorial:     file format elf64-x86-64
...
...
0000000000400506 <main>:
	40051a:	e8 18 00 00 00       	callq  400537 <factorial>
...
...
0000000000400537 <factorial>:
	400550:	75 07                	jne    400559 <factorial+0x22>
	400557:	eb 1b                	jmp    400574 <factorial+0x3d>
	400559:	eb 0e                	jmp    400569 <factorial+0x32>
	40056f:	7e ea                	jle    40055b <factorial+0x24>
...
...
```

在前面的输出中我们可以看到， `main` 函数的地址是 `0x0000000000400506`。为什么它不是从 `0x0` 开始的呢？你可能已经知道标准 C 程序是使用 `glibc` 的 C 标准库链接的（假设参数 `-nostdlib` 没有被传给 `gcc` ）。编译后的程序代码包含了用于在程序启动时初始化程序中数据的构造函数。这些函数需要在程序启动前被调用，或者说在 `main` 函数之前被调用。为了让初始化和终止函数起作用，编译器必须在汇编代码中输出一些让这些函数在正确时间被调用的代码。执行这个程序将会启动位于特殊的 `.init` 段的代码。我们可以从以下的 objdump 输出中看出：

```
objdump -S factorial | less

factorial:     file format elf64-x86-64

Disassembly of section .init:

00000000004003a8 <_init>:
  4003a8:       48 83 ec 08             sub    $0x8,%rsp
  4003ac:       48 8b 05 a5 05 20 00    mov    0x2005a5(%rip),%rax        # 600958 <_DYNAMIC+0x1d0>
```

注意其开始于相对 `glibc` 代码偏移 `0x00000000004003a8` 的地址。我们也可以运行 `readelf` ，在 [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) 输出中检查： 

```
$ readelf -d factorial | grep \(INIT\)
 0x000000000000000c (INIT)               0x4003a8
```

所以， `main` 函数的地址是 `0000000000400506` ，为相对于 `.init` 段的偏移地址。我们可以从输出中看出，`factorial` 函数的地址是 `0x0000000000400537` ，并且现在调用 `factorial` 函数的二进制代码是 `e8 18 00 00 00`。

```
401146:	e8 18 00 00 00       	callq  401163 <factorial>
```

我们已经知道 `e8` 是 `call` 指令的操作码，接下来的 `18 00 00 00` （注意 `x86_64`中地址是小头存储的，所以是 `00 00 00 18` ）是从 `callq` 到 `factorial` 函数的偏移。

```python
>>> hex(0x40051a + 0x18 + 0x5) == hex(0x400537)
True
```

所以我们把 `0x18`和 `0x5` 加到 `call` 指令的地址上。偏移是从接下来一条指令开始算起的。我们的调用指令是 5 字节长（`e8 18 00 00 00`）并且 `0x18` 是从 `factorial` 函数之后的调用算起的偏移。编译器一般按程序地址从零开始创建目标文件。但是如果程序由多个目标文件生成，这些地址会重叠。

我们在这一段看到的是 `重定位` 流程。这个流程为程序中各个部分赋予加载地址，调整程序中的代码和数据以反映出赋值的地址。

好了，现在我们知道了一点关于链接器和重定位的知识，是时候通过链接我们的目标文件来来学习更多关于链接器的知识了。

## 1.4. GNU 链接器


如标题所说，在这篇文章中，我将会使用 [GNU 链接器](https://en.wikipedia.org/wiki/GNU_linker) 或者说 `ld` 。当然我们可以使用 `gcc` 来链接我们的 `factorial` 项目： 

```
$ gcc main.c lib.o -o factorial
```

在这之后，作为结果我们将会得到可执行文件—— `factorial`：

```
./factorial 
factorial of 5 is: 120
```

但是 `gcc` 不会链接目标文件。取而代之，其会使用 `GUN ld` 链接器的包装—— `collect2`。 

```
~$ /usr/lib/gcc/x86_64-linux-gnu/4.9/collect2 --version
collect2 version 4.9.3
/usr/bin/ld --version
GNU ld (GNU Binutils for Debian) 2.25
...
...
...
```

好，我们可以使用 gcc 并且其会为我们的程序生成可执行文件。但是让我们看看如何使用 `GUN ld` 实现相同的目的。首先，让我们尝试用如下样例链接这些目标文件：

```
ld main.o lib.o -o factorial
```

尝试一下，你将会得到如下错误：

```
$ ld main.o lib.o -o factorial
[ld] ld: 警告: 无法找到项目符号 _start; 缺省为 0000000000401000
[ld] ld: main.o: in function `main':
[ld] main.c:(.text+0x26): undefined reference to `printf'
```

这里我们可以看到两个问题：

* 链接器无法找到 `_start` 符号；
* 链接器对 `printf` 一无所知。

首先，让我们尝试理解好像是我们程序运行所需要的 `_start` 入口符号是什么。当我开始学习编程时，我知道了 `main` 函数是程序的入口点。我认为你们也是如此认为的 :) 但实际上这不是入口点，`_start` 才是。 `_start` 符号被 `crt1.0` 所定义。我们可以用如下指令发现它：

```
$ objdump -S /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o

/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_start>:
   0:	31 ed                	xor    %ebp,%ebp
   2:	49 89 d1             	mov    %rdx,%r9
   ...
   ...
   ...
```

我们将该目标文件作为第一个参数传递给 `ld` 指令（如上所示）。现在让我们尝试链接它，会得到如下结果：

```
ld /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
main.o lib.o -o factorial

/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o: In function `_start':
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:115: undefined reference to `__libc_csu_fini'
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:116: undefined reference to `__libc_csu_init'
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:122: undefined reference to `__libc_start_main'
main.o: In function `main':
main.c:(.text+0x26): undefined reference to `printf'
```

不幸的是，我们甚至会看到更多报错。我们可以在这里看到关于未定义 `printf` 的旧错误以及另外三个未定义的引用：

* `__libc_csu_fini`
* `__libc_csu_init`
* `__libc_start_main`

`_start` 符号被定义在 `glibc` 源文件的汇编文件 [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=0d27a38e9c02835ce17d1c9287aa01be222e72eb;hb=HEAD) 中。我们可以在那里找到如下汇编代码： 

```assembly
mov $__libc_csu_fini, %R8_LP
mov $__libc_csu_init, %RCX_LP
...
call __libc_start_main
```

这里我们传递了 `.init` 和 `.fini` 段的入口点地址，它们包含了程序开始和结束时被执行的代码。并且在结尾我们看到对我们程序的 `main` 函数的调用。这三个符号被定义在源文件 [csu/elf-init.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/elf-init.c;hb=1d4bbc54bd4f7d85d774871341b49f4357af1fb7) 中。如下两个目标文件：

* `crtn.o`;
* `crti.o`.

定义了 .init 和 .fini 段的开端和尾声（分别为符号 `_init` 和 `_fini` ）。

`crtn.o` 目标文件包含了 `.init` 和 `.fini` 这些段： 

```
$ objdump -S /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o

0000000000000000 <.init>:
   0:	48 83 c4 08          	add    $0x8,%rsp
   4:	c3                   	retq   

Disassembly of section .fini:

0000000000000000 <.fini>:
   0:	48 83 c4 08          	add    $0x8,%rsp
   4:	c3                   	retq   
```

且 `crti.o` 目标文件包含了符号 `_init` 和 `_fini`。让我们再次尝试链接这两个目标文件： 

```
$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o \
-o factorial
```

当然，我们会得到相同的错误。现在我们需要把 `-lc` 选项传递给 `ld` 。这个选项将会在环境变量 `$LD_LIBRARY_PATH` 指定的目录中搜索标准库。让我们再次尝试用 `-lc` 选项链接：

```
$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o -lc \
-o factorial
```

最后我们获得了一个可执行文件，但是如果我们尝试运行它，我们会遇到奇怪的结果：

```
$ ./factorial 
bash: ./factorial: No such file or directory
```

这里除了什么问题？让我们用 [readelf](https://sourceware.org/binutils/docs/binutils/readelf.html) 工具看看这个可执行文件：

```
$ readelf -l factorial 

Elf file type is EXEC (Executable file)
Entry point 0x4003c0
There are 7 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x0000000000400040 0x0000000000400040
                 0x0000000000000188 0x0000000000000188  R E    8
  INTERP         0x00000000000001c8 0x00000000004001c8 0x00000000004001c8
                 0x000000000000001c 0x000000000000001c  R      1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x0000000000000000 0x0000000000400000 0x0000000000400000
                 0x0000000000000610 0x0000000000000610  R E    200000
  LOAD           0x0000000000000610 0x0000000000600610 0x0000000000600610
                 0x00000000000001cc 0x00000000000001cc  RW     200000
  DYNAMIC        0x0000000000000610 0x0000000000600610 0x0000000000600610
                 0x0000000000000190 0x0000000000000190  RW     8
  NOTE           0x00000000000001e4 0x00000000004001e4 0x00000000004001e4
                 0x0000000000000020 0x0000000000000020  R      4
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     10

 Section to Segment mapping:
  Segment Sections...
   00     
   01     .interp 
   02     .interp .note.ABI-tag .hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn .rela.plt .init .plt .text .fini .rodata .eh_frame 
   03     .dynamic .got .got.plt .data 
   04     .dynamic 
   05     .note.ABI-tag 
   06     
```

注意这奇怪的一行：

```
  INTERP         0x00000000000001c8 0x00000000004001c8 0x00000000004001c8
                 0x000000000000001c 0x000000000000001c  R      1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
```

`elf` 文件的 `.interp` 段保存了一个程序解释器的路径名，或者说 `.interp` 段就包含了一个动态链接器名字的 `ascii` 字符串。动态链接器是 Linux 的一部分，其通过将库的内容从磁盘复制到内存中以加载和链接一个可执行文件被执行所需要的动态链接库。我们可以从 `readelf` 命令的输出中看到，针对 `x86_64` 架构，其被放在 `/lib64/ld-linux-x86-64.so.2`。现在让我们把 `ld-linux-x86-64.so.2` 的路径和 `-dynamic-linker` 选项一起传递给 `ld` 调用，然后会看到如下结果：

```
$ gcc -c main.c lib.c

$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o \
-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
-lc -o factorial
```

现在我们可以像普通可执行文件一样执行它了：

```
$ ./factorial

factorial of 5 is: 120
```

成功了！在第一行，我们把源文件 `main.c` 和 `lib.c` 编译成目标文件。执行 `gcc` 之后我们将会获得 `main.o` 和 `lib.o`： 

```
$ file lib.o main.o
lib.o:  ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
main.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

在这之后，我们用所需的系统目标文件和库连链接我们的程序。我们刚看了一个简单的关于如何用 `gcc` 编译器和 `GNU ld` 链接器编译和链接一个 C 程序的样例。在这个样例中，我们使用了一些 `GNU linker` 的命令行选项，但是除了 `-o`、`-dynamic-linker` 等，它还支持其他很多选项。此外，`GNU ld` 还拥有其自己的语言来控制链接过程。在接下来的两个段落中我们深入讨论。

## 1.5. 实用的 GNU 链接器命令行选项

正如我之前所说，你也可以从 `GNU linker` 的指南看到，其拥有大量的命令行选项。我们已经在这篇文章见到一些： `-o <output>` - 告诉 `ld` 将链接结果输出成一个叫做 `output` 的文件，`-l<name>` - 通过文件名添加指定存档或者目标文件，`-dynamic-linker` 通过名字指定动态链接器。当然， `ld` 支持更多选项，让我们看看其中的一些。

第一个实用的选项是 `@file` 。在这里 `file` 指定了命令行选项将读取的文件名。比如我们可以创建一个叫做 `linker.ld` 的文件，把我们上一个例子里面的命令行参数放进去然后执行：

```
$ ld @linker.ld
```

下一个命令行选项是 `-b` 或 `--format`。这个命令行选项指定了输入的目标文件的格式是 `ELF`, `DJGPP/COFF` 等。针对输出文件也有相同功能的选项 `--oformat=output-format` 。

下一个命令行选项是 `--defsym` 。该选项的完整格式是 `--defsym=symbol=expression` 。它允许在输出文件中创建包含了由表达式给出了绝对地址的全局符号。在下面的例子中，我们会发现这个命令行选项很实用：在 Linux 内核源码中关于 ARM 架构内核解压的 Makefile - [arch/arm/boot/compressed/Makefile](https://github.com/torvalds/linux/blob/master/arch/arm/boot/compressed/Makefile)，我们可以找到如下定义：

```
LDFLAGS_vmlinux = --defsym _kernel_bss_size=$(KBSS_SZ)
```

正如我们所知，其在输出文件中用 `.bss` 段的大小定义了 `_kernel_bss_size` 符号。这个符号将会作为第一个 [汇编文件](https://github.com/torvalds/linux/blob/master/arch/arm/boot/compressed/head.S) 在内核解压阶段被执行：

```assembly
ldr r5, =_kernel_bss_size
```

下一个选项是 `-shared` ，其允许我们创建共享库。`-M` 或者说 `-map <filename>` 命令行选项会打印带符号信息的链接映射内容。在这里是：

```
$ ld -M @linker.ld
...
...
...
.text           0x00000000004003c0      0x112
 *(.text.unlikely .text.*_unlikely .text.unlikely.*)
 *(.text.exit .text.exit.*)
 *(.text.startup .text.startup.*)
 *(.text.hot .text.hot.*)
 *(.text .stub .text.* .gnu.linkonce.t.*)
 .text          0x00000000004003c0       0x2a /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o
...
...
...
 .text          0x00000000004003ea       0x31 main.o
                0x00000000004003ea                main
 .text          0x000000000040041b       0x3f lib.o
                0x000000000040041b                factorial
```

当然，`GNU 链接器` 支持标准的命令行选项：`--help` 和 `--version` 能够打印 `ld` 的命令帮助、使用方法和版本。以上就是所有关于 `GNU 链接器` 命令行选项的内容。当然这不是 `ld` 工具支持的所有命令行选项。你可以在指南中找到 `ld` 工具的完整文档。

## 1.6. 链接器控制语言


如我之前所说， `ld` 支持它自己的语言。它接受由一种 `AT&T` 链接器控制语法的超集编写的链接器控制语言文件，以提供对链接过程明确且完全的控制。接下来让我们关注其中细节。

我们可以通过链接器语言控制：

* 输入文件；
* 输出文件；
* 文件格式；
* 段的地址；
* 其他更多...

用链接器控制语言编写的命令通常被放在一个被称作链接器脚本的文件中。我们可以通过命令行选项 `-T` 将其传递给 `ld` 。一个链接器脚本的主要命令是 `SECTIONS` 指令。每个链接器脚本必须包含这个指令，并且其决定了输出文件的 `映射` 。特殊变量 `.` 包含了当前输出的位置。让我们写一个简单的汇编程序，然后看看如何使用链接器脚本来控制程序的链接。我们将会使用一个 hello world 程序作为样例。

```assembly
.data
	msg	.ascii "hello, world!",`\n`

.text

	global	_start
  
_start:
	mov    $1,%rax
	mov    $1,%rdi
	mov    $msg,%rsi
	mov    $14,%rdx
	syscall

	mov    $60,%rax
	mov    $0,%rdi
	syscall
```

我们可以用以下命令编译并链接：

```
$ as -o hello.o hello.asm
$ ld -o hello hello.o
```

我们的程序包含了两个段： `.text` 包含了程序的代码， `.data` 段包含了被初始化的变量。让我们写一个简单的链接脚本，然后尝试用它来链接我们的 `hello.asm` 汇编文件。我们的脚本是：

```
/*
 * Linker script for the factorial
 */
OUTPUT(hello) 
OUTPUT_FORMAT("elf64-x86-64")
INPUT(hello.o)

SECTIONS
{
	. = 0x200000;
	.text : {
	      *(.text)
	}

	. = 0x400000;
	.data : {
	      *(.data)
	}
}
```

在前三行你可以看到 `C` 风格的注释。之后是 `OUTPUT` 和 `OUTPUT_FORMAT` 命令，指定了我们的可执行文件名称和格式。下一个指令，`INPUT`，指定了给 `ld` 的输入文件。接下来，我们可以看到主要的 `SECTIONS` 指令，正如我写的，它是必须存在于每个链接器脚本中。`SECTIONS` 命令表示了输出文件中的段的集合和顺序。在 `SECTIONS` 命令的开头，我们可以看到一行 `. = 0x200000` 。我上面已经写过，`.` 命令指向输出中的当前位置。这一行说明代码段应该被加载到地址 `0x200000`。`. = 0x400000`一行说明数据段应该被加载到地址`0x400000` 。`. = 0x200000`之后的第二行定义 `.text` 作为输出段。我们可以看到其中的 `*(.text)` 表达式。 `*` 符号是一个匹配任意文件名的通配符。换句话说，`*(.text)` 表达式代表所有输入文件中的所有 `.text` 输入段。在我们的样例中，我们可以将其重写为 `hello.o(.text)` 。在地址计数器 `. = 0x400000` 之后，我们可以看到数据段的定义。

我们可以用以下语句进行编译和链接：

```
$ as -o hello.o hello.S && ld -T linker.script && ./hello
hello, world!
```

如果我们用 `objdump` 工具深入查看，我们可以看到 `.text` 段从地址 `0x200000` 开始， `.data` 段从 `0x400000` 开始：

```
$ objdump -D hello

Disassembly of section .text:

0000000000200000 <_start>:
  200000:	48 c7 c0 01 00 00 00 	mov    $0x1,%rax
  ...

Disassembly of section .data:

0000000000400000 <msg>:
  400000:	68 65 6c 6c 6f       	pushq  $0x6f6c6c65
  ...
```

除了我们已经看到的命令，另外还有一些。首先是 `ASSERT(exp, message)` ，保证给定的表达式不为零。如果为零，那么链接器会退出同时返回错误码，打印错误信息。如果你已经阅读了 [linux-insides](https://xinqiu.gitbooks.io/linux-insides-cn/content/) 的 Linux 内核启动流程，你或许知道 Linux 内核的设置头的偏移为 `0x1f1`。在 Linux 内核的链接器脚本中，我们可以看到下面的校验：

```
. = ASSERT(hdr == 0x1f1, "The setup header has the wrong offset!");
```

`INCLUDE filename` 允许我们在当前的链接器脚本中包含外部符号。我们可以在一个链接器脚本中给一个符号赋值。 `ld` 支持一些赋值操作符：

* symbol = expression   ;
* symbol += expression  ;
* symbol -= expression  ;
* symbol *= expression  ;
* symbol /= expression  ;
* symbol <<= expression ;
* symbol >>= expression ;
* symbol &= expression  ;
* symbol |= expression  ;

正如你注意到的，所有操作符都是 C 赋值操作符。比如我们可以在我们的链接器脚本中使用：

```
START_ADDRESS = 0x200000;
DATA_OFFSET   = 0x200000;

SECTIONS
{
	. = START_ADDRESS;
	.text : {
	      *(.text)
	}

	. = START_ADDRESS + DATA_OFFSET;
	.data : {
	      *(.data)
	}
}
```

你可能已经注意到了链接器脚本中表达式的语法和 C 表达式相同。除此之外，这个链接控制语言还支持如下内嵌函数：

* `ABSOLUTE` - 返回给定表达式的绝对值；
* `ADDR` - 接受段，返回其地址；
* `ALIGN` - 返回和给定表达式下一句的边界对齐的位置计数器（ `.` 操作符）的值；
* `DEFINED` - 如果给定符号在全局符号表中，返回 `1`，否则 `0`；
* `MAX` and `MIN` - 返回两个给定表达式中的最大、最小值；
* `NEXT` - 返回一个是当前表达式倍数的未分配地址；
* `SIZEOF` - 返回给定名字的段以字节计数的大小。

以上就是全部了。

## 1.7. 源码列表

[本章所有源码GitHub下载地址](https://github.com/Rtoax/test/tree/master/gcc/linker)

C语言代码部分的脚本：
```bash
#[rongtao@localhost demo-linkers]$ cat run.sh 
#!/bin/bash

rm -f *.o *.out

function test1() {
	# 单独编译main.c
	gcc -c main.c $*

	# 查看 main.o 中的字符
	echo -e ""
	nm -A main.o | sed 's/^/[nm] /g'

	# 反汇编 objdump
	echo -e ""
	objdump -S main.o | sed 's/^/[objdump] /g'

	# 两个 `callq` 操作。这两个 `callq` 操作包含了 `链接器存根`
	# `-r` 或 `--reloc ` 选项会打印文件的 `重定位` 条目
	echo -e "\n显示 链接器 存根"
	objdump -S -r main.o | sed 's/^/[objdump-r] /g'
}

function test2() {
	gcc main.c lib.c -o factorial
	objdump -S factorial | grep -e ".init" -e factorial -e main | sed 's/^/[objdump] /g'
	readelf -d factorial | grep \(INIT\) | sed 's/^/[readelf] /g'
	#objdump -S factorial | sed 's/^/[objdump] /g' | more
	#rm -f factorial
}

function test3() {
	gcc lib.c -c
	gcc main.c lib.o -o factorial
	./factorial 2>&1 | sed 's/^/[factorial] /g'
}

# 使用链接器
function test4() {
	gcc lib.c -c
	gcc main.c -c
	file main.o lib.o 2>&1 | sed 's/^/[file] /g'
	ld main.o lib.o -o factorial 2>&1 | sed 's/^/[ld] /g'
	gcc main.o lib.o -o factorial
}

function test5() {
	nasm -felf64 hello.asm && ld hello.o && ./a.out
}


rm -f *.o *.out
echo -e ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
test1
echo -e ""
echo -e ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
test2
echo -e ""
echo -e ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
test3
echo -e ""
echo -e ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
test4
test5
rm -f *.o *.out
```

链接部分的Makefile：
```makefile
all:a.out hello.out

hello.o:hello.asm
	nasm hello.asm -felf64

a.out:hello.o
	ld hello.o

clean:
	rm -f *.o *.out

hello.out:
	ld -T linker.script
	@objdump -D hello.out | grep -e "\.text" -e "\.data" -A 2 | sed 's/^/[objdump] /g'
```


## 1.8. 总结


这是关于链接器文章的结尾。在这篇文章中，我们已经学习了很多关于链接器的知识，比如什么是链接器、为什么需要它、如何使用它等等...

如果你发现文中描述有任何问题，请提交一个 PR 到 [linux-insides-zh](https://github.com/MintCN/linux-insides-zh) 。

## 1.9. 相关链接


* [Book about Linux kernel insides](http://0xax.gitbooks.io/linux-insides/content/)
* [linker](https://en.wikipedia.org/wiki/Linker_%28computing%29)
* [object files](https://en.wikipedia.org/wiki/Object_file)
* [glibc](https://en.wikipedia.org/wiki/GNU_C_Library)
* [opcode](https://en.wikipedia.org/wiki/Opcode)
* [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
* [GNU linker](https://en.wikipedia.org/wiki/GNU_linker)
* [My posts about assembly programming for x86_64](http://0xax.github.io/categories/assembly/)
* [readelf](https://sourceware.org/binutils/docs/binutils/readelf.html)



# 2. 用户空间的程序启动过程


## 2.1. 简介

虽然 linux-insides大多描述的是内核相关的东西，但是我已经决定写一个大多与用户空间相关的部分。

[系统调用](https://zh.wikipedia.org/wiki/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8)章节的[第四部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-4.html)已经描述了当我们想运行一个程序， Linux 内核的行为。这部分我想研究一下从用户空间的角度，当我们在 Linux 系统上运行一个程序，会发生什么。

我不知道你知识储备如何，但是在我的大学时期我学到，一个 `C` 程序从一个叫做 main 的函数开始执行。而且，这是部分正确的。每时每刻，当我们开始写一个新的程序时，我们从下面的实例代码开始编程：

```C
int main(int argc, char *argv[]) {
	// Entry point is here
}
```

但是你如何对于底层编程感兴趣的话，可能你已经知道 `main` 函数并不是程序的真正入口。如果你在调试器中看了下面这个简单程序，就可以很确信这一点：

```C
int main(int argc, char *argv[]) {
	return 0;
}
```

让我们来编译并且在 [gdb](https://www.gnu.org/software/gdb/) 中运行这个程序：

```
$ gcc -ggdb program.c -o program
$ gdb ./program
The target architecture is assumed to be i386:x86-64:intel
Reading symbols from ./program...done.
```

让我们在 gdb 中执行 `info files` 这个指令。这个指令会打印关于被不同段占据的内存和调试目标的信息。

```
(gdb) info files
Symbols from "/home/alex/program".
Local exec file:
	`/home/alex/program', file type elf64-x86-64.
	Entry point: 0x400430
	0x0000000000400238 - 0x0000000000400254 is .interp
	0x0000000000400254 - 0x0000000000400274 is .note.ABI-tag
	0x0000000000400274 - 0x0000000000400298 is .note.gnu.build-id
	0x0000000000400298 - 0x00000000004002b4 is .gnu.hash
	0x00000000004002b8 - 0x0000000000400318 is .dynsym
	0x0000000000400318 - 0x0000000000400357 is .dynstr
	0x0000000000400358 - 0x0000000000400360 is .gnu.version
	0x0000000000400360 - 0x0000000000400380 is .gnu.version_r
	0x0000000000400380 - 0x0000000000400398 is .rela.dyn
	0x0000000000400398 - 0x00000000004003c8 is .rela.plt
	0x00000000004003c8 - 0x00000000004003e2 is .init
	0x00000000004003f0 - 0x0000000000400420 is .plt
	0x0000000000400420 - 0x0000000000400428 is .plt.got
	0x0000000000400430 - 0x00000000004005e2 is .text
	0x00000000004005e4 - 0x00000000004005ed is .fini
	0x00000000004005f0 - 0x0000000000400610 is .rodata
	0x0000000000400610 - 0x0000000000400644 is .eh_frame_hdr
	0x0000000000400648 - 0x000000000040073c is .eh_frame
	0x0000000000600e10 - 0x0000000000600e18 is .init_array
	0x0000000000600e18 - 0x0000000000600e20 is .fini_array
	0x0000000000600e20 - 0x0000000000600e28 is .jcr
	0x0000000000600e28 - 0x0000000000600ff8 is .dynamic
	0x0000000000600ff8 - 0x0000000000601000 is .got
	0x0000000000601000 - 0x0000000000601028 is .got.plt
	0x0000000000601028 - 0x0000000000601034 is .data
	0x0000000000601034 - 0x0000000000601038 is .bss
```

注意 `Entry point: 0x400430` 这一行。现在我们知道我们程序入口点的真正地址。让我们在这个地址下一个断点，然后运行我们的程序，看看会发生什么：

```
(gdb) break *0x400430
Breakpoint 1 at 0x400430
(gdb) run
Starting program: /home/alex/program 

Breakpoint 1, 0x0000000000400430 in _start ()
```

有趣。我们并没有看见 `main` 函数的执行，但是我们看见另外一个函数被调用。这个函数是 `_start` 而且根据调试器展现给我们看的，它是我们程序的真正入口。那么，这个函数是从哪里来的，又是谁调用了这个 `main` 函数，什么时候调用的。我会在后续部分尝试回答这些问题。


本节代码位置：[本节所有源码GitHub下载地址](https://github.com/Rtoax/test/tree/master/c/prog-exec)


## 2.2. 内核如何运行新程序

本节代码位置：[本节所有源码GitHub下载地址](https://github.com/Rtoax/test/tree/master/c/prog-exec-why-how)

首先，让我们来看一下下面这个简单的 `C` 程序：

```C
// program.c

#include <stdlib.h>
#include <stdio.h>

static int x = 1;

int y = 2;

int main(int argc, char *argv[]) {
	int z = 3;

	printf("x + y + z = %d\n", x + y + z);

	return EXIT_SUCCESS;
}
```

我们可以确定这个程序按照我们预期那样工作。让我们来编译它：

```
$ gcc -Wall program.c -o sum
```

并且执行：

```
$ ./sum
x + y + z = 6
```

好的，直到现在所有事情看起来听挺好。你可能已经知道一个特殊的[系统调用](https://zh.wikipedia.org/wiki/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8)家族 - [exec*](http://man7.org/linux/man-pages/man3/execl.3.html) 系统调用。正如我们从帮助手册中读到的：

> The exec() family of functions replaces the current process image with a new process image.

如果你已经阅读过[系统调用](https://zh.wikipedia.org/wiki/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8)章节的[第四部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-4.html)，你可能就知道 execve 这个系统调用定义在 [files/exec.c](https://github.com/torvalds/linux/blob/master/fs/exec.c#L1859) 文件中，并且如下所示，

```C
SYSCALL_DEFINE3(execve,
		const char __user *, filename,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp)
{
	return do_execve(getname(filename), argv, envp);
}
```

它以可执行文件的名字，命令行参数的集合以及环境变量的集合作为参数。正如你猜测的，每一件事都是 `do_execve` 函数完成的。在这里我将不描述这个函数的实现细节，因为你可以从[这里](https://xinqiu.gitbooks.io/linux-insides-cn/content/SysCall/linux-syscall-4.html)读到。但是，简而言之，`do_execve` 函数会检查诸如文件名是否有效，未超出进程数目限制等等。

在这些检查之后，这个函数会解析 [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) 格式的可执行文件，**为新的可执行文件创建内存描述符，并且在栈，堆等内存区域填上适当的值。**

当**二进制镜像**设置完成，`start_thread` 函数会设置一个新的进程。这个函数是框架相关的，而且对于 [x86_64](https://en.wikipedia.org/wiki/X86-64) 框架，它的定义是在 [arch/x86/kernel/process_64.c](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/process_64.c#L231) 文件中。

```c
static void
start_thread_common(struct pt_regs *regs, unsigned long new_ip,
		    unsigned long new_sp,
		    unsigned int _cs, unsigned int _ss, unsigned int _ds)
{
	WARN_ON_ONCE(regs != current_pt_regs());

	if (static_cpu_has(X86_BUG_NULL_SEG)) {
		/* Loading zero below won't clear the base. */
		loadsegment(fs, __USER_DS);
		load_gs_index(__USER_DS);
	}

	loadsegment(fs, 0);
	loadsegment(es, _ds);
	loadsegment(ds, _ds);
	load_gs_index(0);

	regs->ip		= new_ip;
	regs->sp		= new_sp;
	regs->cs		= _cs;
	regs->ss		= _ss;
	regs->flags		= X86_EFLAGS_IF;
}

void
start_thread(struct pt_regs *regs, unsigned long new_ip, unsigned long new_sp)
{
	start_thread_common(regs, new_ip, new_sp,
			    __USER_CS, __USER_DS, 0);
}
EXPORT_SYMBOL_GPL(start_thread);
```

`start_thread` 为[段寄存器](https://en.wikipedia.org/wiki/X86_memory_segmentation)设置新的值。从这一点开始，新进程已经准备就绪。一旦[进程切换]((https://en.wikipedia.org/wiki/Context_switch))完成，控制权就会返回到用户空间，并且新的可执行文件将会执行。

这就是所有内核方面的内容。Linux 内核为执行准备二进制镜像，而且它的执行从上下文切换开始，结束之后将控制权返回用户空间。但是它并**不能回答像 `_start` 来自哪里**这样的问题。让我们在下一段尝试回答这些问题。

## 2.3. 用户空间程序如何启动

在之前的段落汇总，我们看到了内核是如何为可执行文件运行做准备工作的。让我们从用户空间来看这相同的工作。我们已经知道一个程序的入口点是 `_start` 函数。但是这个函数是从哪里来的呢？它可能来自于一个库。但是如果你记得清楚的话，我们在程序编译过程中并没有链接任何库。

```
$ gcc -Wall program.c -o sum
```

你可能会猜 `_start` 来自于[标准库](https://en.wikipedia.org/wiki/Standard_library)。是的，确实是这样。如果你尝试去重新编译我们的程序，并给 gcc 传递可以开启 `verbose mode`（冗长模式） 的 `-v` 选项，你会看到下面的长输出。我们并不对整体输出感兴趣，让我们来看一下下面的步骤：

首先，使用 `gcc` 编译我们的程序：

```
$ gcc -v -ggdb program.c -o sum
...
...
...
/usr/libexec/gcc/x86_64-redhat-linux/6.1.1/cc1 -quiet -v program.c -quiet -dumpbase program.c -mtune=generic -march=x86-64 -auxbase test -ggdb -version -o /tmp/ccvUWZkF.s
...
...
...
```

`cc1` 编译器将编译我们的 `C` 代码并且生成 `/tmp/ccvUWZkF.s` 汇编文件。之后我们可以看见我们的汇编文件被 `GNU as` 编译器编译为目标文件：


```
$ gcc -v -ggdb program.c -o sum
...
...
...
as -v --64 -o /tmp/cc79wZSU.o /tmp/ccvUWZkF.s
...
...
...
```

最后我们的目标文件会被 `collect2` 链接到一起：

```
$ gcc -v -ggdb program.c -o sum
...
...
...
COLLECT_GCC_OPTIONS='-v' '-ggdb' '-o' 'sum.out' '-mtune=generic' '-march=x86-64'
 /opt/rh/devtoolset-9/root/usr/libexec/gcc/x86_64-redhat-linux/9/collect2 -plugin /opt/rh/devtoolset-9/root/usr/libexec/gcc/x86_64-redhat-linux/9/liblto_plugin.so -plugin-opt=/opt/rh/devtoolset-9/root/usr/libexec/gcc/x86_64-redhat-linux/9/lto-wrapper -plugin-opt=-fresolution=/tmp/ccQ8XQgu.res -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s -plugin-opt=-pass-through=-lc -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s --build-id --no-add-needed --eh-frame-hdr --hash-style=gnu -m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o sum.out /lib/../lib64/crt1.o /lib/../lib64/crti.o /opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/crtbegin.o -L/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9 -L/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/../../.. /tmp/cc0UqxVW.o -lgcc --push-state --as-needed -lgcc_s --pop-state -lc -lgcc --push-state --as-needed -lgcc_s --pop-state /opt/rh/devtoolset-9/root/usr/lib/gcc/x86_64-redhat-linux/9/crtend.o /lib/../lib64/crtn.o
...
...
...
```

是的，我们可以看见一个很长的命令行选项列表被传递给链接器。让我们从另一条路行进。我们知道我们的程序都依赖标准库。

```
$ ldd program
	linux-vdso.so.1 (0x00007ffc9afd2000)
	libc.so.6 => /lib64/libc.so.6 (0x00007f56b389b000)
	/lib64/ld-linux-x86-64.so.2 (0x0000556198231000)
```

从那里我们会用一些库函数，像 `printf` 。但是不止如此。这就是为什么当我们给编译器传递 `-nostdlib` 参数，我们会收到错误报告：
```
$ gcc -nostdlib program.c -o program
/usr/bin/ld: warning: cannot find entry symbol _start; defaulting to 000000000040017c
/tmp/cc02msGW.o: In function `main':
/home/alex/program.c:11: undefined reference to `printf'
collect2: error: ld returned 1 exit status
```

除了这些错误，我们还看见 `_start` 符号未定义。所以现在我们可以确定 `_start` 函数来自于标准库。但是即使我们链接标准库，它也无法成功编译：

```
$ gcc -nostdlib -lc -ggdb program.c -o program
/usr/bin/ld: warning: cannot find entry symbol _start; defaulting to 0000000000400350
```

好的，当我们使用 `/usr/lib64/libc.so.6` 链接我们的程序，编译器并不报告标准库函数的未定义引用，但是 `_start` 符号仍然未被解析。让我们重新回到 `gcc` 的冗长输出，看看 `collect2` 的参数。我们现在最重要的问题是我们的程序不仅链接了标准库，还有一些目标文件。第一个目标文件是 `/lib64/crt1.o` 。而且，如果我们使用 `objdump` 工具去看这个目标文件的内部，我们将看见 `_start` 符号：

```
$ objdump -d /lib64/crt1.o 

/lib64/crt1.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_start>:
   0:	31 ed                	xor    %ebp,%ebp
   2:	49 89 d1             	mov    %rdx,%r9
   5:	5e                   	pop    %rsi
   6:	48 89 e2             	mov    %rsp,%rdx
   9:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
   d:	50                   	push   %rax
   e:	54                   	push   %rsp
   f:	49 c7 c0 00 00 00 00 	mov    $0x0,%r8
  16:	48 c7 c1 00 00 00 00 	mov    $0x0,%rcx
  1d:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
  24:	e8 00 00 00 00       	callq  29 <_start+0x29>
  29:	f4                   	hlt    
```

因为 `crt1.o` 是一个共享目标文件，所以我们只看到桩而不是真正的函数调用。让我们来看一下 `_start` 函数的源码。因为这个函数是框架相关的，所以 `_start` 的实现是在 [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) 这个汇编文件中。

`_start` 始于对 `ebp` 寄存器的清零，正如 [ABI]((https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf)) 所建议的。 

```assembly
xorl %ebp, %ebp
```

之后，将终止函数的地址放到 `r9` 寄存器中：

```assembly
mov %RDX_LP, %R9_LP
```

正如 [ELF](http://flint.cs.yale.edu/cs422/doc/ELF_Format.pdf) 标准所述，

> After the dynamic linker has built the process image and performed the relocations, each shared object
> gets the opportunity to execute some initialization code.
> ...
> Similarly, shared objects may have termination functions, which are executed with the atexit (BA_OS)
> mechanism after the base process begins its termination sequence.

所以我们需要把终止函数的地址放到 `r9` 寄存器，因为将来它会被当作第六个参数传递给 `__libc_start_main` 。注意，终止函数的地址初始是存储在 `rdx` 寄存器。除了 `%rdx` 和 `%rsp` 之外的其他寄存器保存未确定的值。`_start` 函数中真正的重点是调用 `__libc_start_main`。所以下一步就是为调用这个函数做准备。

`__libc_start_main` 的实现是在 [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=0fb98f1606bab475ab5ba2d0fe08c64f83cce9df;hb=HEAD) 文件中。让我们来看一下这个函数：

```C
STATIC int LIBC_START_MAIN (int (*main) (int, char **, char **),
 			                int argc,
			                char **argv,
 			                __typeof (main) init,
			                void (*fini) (void),
			                void (*rtld_fini) (void),
			                void *stack_end)
```

It takes address of the `main` function of a program, `argc` and `argv`. `init` and `fini` functions are constructor and destructor of the program. The `rtld_fini` is termination function which will be called after the program will be exited to terminate and free dynamic section. The last parameter of the `__libc_start_main` is the pointer to the stack of the program. Before we can call the `__libc_start_main` function, all of these parameters must be prepared and passed to it. Let's return to the [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) assembly file and continue to see what happens before the `__libc_start_main` function will be called from there.

该函数以程序 `main` 函数的地址，`argc` 和 `argv` 作为输入。`init` 和 `fini` 函数分别是程序的构造函数和析构函数。`rtld_fini` 是当程序退出时调用的终止函数，用来终止以及释放动态段。`__libc_start_main` 函数的最后一个参数是一个指向程序栈的指针。在我们调用 `__libc_start_main` 函数之前，所有的参数都要被准备好，并且传递给它。让我们返回 [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) 这个文件，继续看在 `__libc_start_main` 被调用之前发生了什么。

我们可以从栈上获取我们所需的 `__libc_start_main` 的所有参数。当 `_start` 被调用的时候，我们的栈如下所示：

```
+-----------------+
|       NULL      |
+-----------------+ 
|       envp      |
+-----------------+ 
|       NULL      |
+------------------
|       argv      | <- rsp
+------------------
|       argc      |
+-----------------+ 
```

当我们清零了 `ebp` 寄存器，并且将终止函数的地址保存到 `r9` 寄存器中之后，我们取出栈顶元素，放到 `rsi` 寄存器中。最终 `rsp` 指向 `argv` 数组，`rsi` 保存传递给程序的命令行参数的数目：
```
+-----------------+
|       NULL      |
+-----------------+ 
|       envp      |
+-----------------+ 
|       NULL      |
+------------------
|       argv      | <- rsp
+-----------------+
```

这之后，我们将 `argv` 数组的地址赋值给 `rdx` 寄存器中。

```assembly
popq %rsi
mov %RSP_LP, %RDX_LP
```

从这一时刻开始，我们已经有了 `argc` 和 `argv`。我们仍要将构造函数和析构函数的指针放到合适的寄存器，以及传递指向栈的指针。下面汇编代码的前三行按照 [ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf) 中的建议设置栈为 `16` 字节对齐，并将 `rax` 压栈：

```assembly
and  $~15, %RSP_LP
pushq %rax

pushq %rsp
mov $__libc_csu_fini, %R8_LP
mov $__libc_csu_init, %RCX_LP
mov $main, %RDI_LP
```

栈对齐之后，我们压栈栈的地址，并且将构造函数和析构函数的地址放到 `r8` 和 `rcx` 寄存器中，同时将 `main` 函数的地址放到 `rdi` 寄存器中。从这个时刻开始，我们可以调用 [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=0fb98f1606bab475ab5ba2d0fe08c64f83cce9df;hb=HEAD) 中的 `__libc_start_main` 函数。

在我们查看 `__libc_start_main` 函数之前，让我们添加 `/lib64/crt1.o` 文件并且再次尝试编译我们的程序：

```
$ gcc -nostdlib /lib64/crt1.o -lc -ggdb program.c -o program
/lib64/crt1.o: In function `_start':
(.text+0x12): undefined reference to `__libc_csu_fini'
/lib64/crt1.o: In function `_start':
(.text+0x19): undefined reference to `__libc_csu_init'
collect2: error: ld returned 1 exit status
```

现在我们看见了另外一个错误 - 未找到 `__libc_csu_fini` 和 `__libc_csu_init` 。我们知道这两个函数的地址被传递给 `__libc_start_main` 作为参数，同时这两个函数还是我们程序的构造函数和析构函数。但是在 `C` 程序中，构造函数和析构函数意味着什么呢？我们已经在 [ELF](http://flint.cs.yale.edu/cs422/doc/ELF_Format.pdf) 标准中看到：

> After the dynamic linker has built the process image and performed the relocations, each shared object
> gets the opportunity to execute some initialization code.
> ...
> Similarly, shared objects may have termination functions, which are executed with the atexit (BA_OS)
> mechanism after the base process begins its termination sequence.

所以链接器除了一般的段，如 `.text`, `.data` 之外创建了两个特殊的段：

* `.init`
* `.fini`

We can find it with `readelf` util:

我们可以通过 `readelf` 工具找到它们：

```
$ readelf -e test | grep init
  [11] .init             PROGBITS         00000000004003c8  000003c8

$ readelf -e test | grep fini
  [15] .fini             PROGBITS         0000000000400504  00000504
```

这两个将被替换为二进制镜像的开始和结尾，包含分别被称为构造函数和析构函数的例程。这些例程的要点是在程序的真正代码执行之前，做一些初始化/终结，像全局变量如 [errno](http://man7.org/linux/man-pages/man3/errno.3.html) ，为系统例程分配和释放内存等等。

你可能可以从这些函数的名字推测，这两个会在 `main` 函数之前和之后被调用。`.init` 和 `.fini` 段的定义在 `/lib64/crti.o` 中。如果我们添加这个目标文件：

```
$ gcc -nostdlib /lib64/crt1.o /lib64/crti.o  -lc -ggdb program.c -o program
```

我们不会收到任何错误报告。但是让我们尝试去运行我们的程序，看看发生什么：

```
$ ./program
Segmentation fault (core dumped)
```

是的，我们收到 `segmentation fault` 。让我们通过 `objdump` 看看 `lib64/crti.o` 的内容：

```
$ objdump -D /lib64/crti.o

/lib64/crti.o:     file format elf64-x86-64


Disassembly of section .init:

0000000000000000 <_init>:
   0:	48 83 ec 08          	sub    $0x8,%rsp
   4:	48 8b 05 00 00 00 00 	mov    0x0(%rip),%rax        # b <_init+0xb>
   b:	48 85 c0             	test   %rax,%rax
   e:	74 05                	je     15 <_init+0x15>
  10:	e8 00 00 00 00       	callq  15 <_init+0x15>

Disassembly of section .fini:

0000000000000000 <_fini>:
   0:	48 83 ec 08          	sub    $0x8,%rsp
```

正如上面所写的， `/lib64/crti.o` 目标文件包含 `.init` 和 `.fini` 段的定义，但是我们可以看见这个函数的桩。让我们看一下 [sysdeps/x86_64/crti.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crti.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD) 文件中的源码：

```assembly
	.section .init,"ax",@progbits
	.p2align 2
	.globl _init
	.type _init, @function
_init:
	subq $8, %rsp
	movq PREINIT_FUNCTION@GOTPCREL(%rip), %rax
	testq %rax, %rax
	je .Lno_weak_fn
	call *%rax
.Lno_weak_fn:
	call PREINIT_FUNCTION
```

它包含 `.init` 段的定义，而且汇编代码设置 16 字节的对齐。之后，如果它不是零，我们调用 `PREINIT_FUNCTION`；否则不调用：

```
00000000004003c8 <_init>:
  4003c8:       48 83 ec 08             sub    $0x8,%rsp
  4003cc:       48 8b 05 25 0c 20 00    mov    0x200c25(%rip),%rax        # 600ff8 <_DYNAMIC+0x1d0>
  4003d3:       48 85 c0                test   %rax,%rax
  4003d6:       74 05                   je     4003dd <_init+0x15>
  4003d8:       e8 43 00 00 00          callq  400420 <__libc_start_main@plt+0x10>
  4003dd:       48 83 c4 08             add    $0x8,%rsp
  4003e1:       c3                      retq
```

where the `PREINIT_FUNCTION` is the `__gmon_start__` which does setup for profiling. You may note that we have no return instruction in the [sysdeps/x86_64/crti.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crti.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD). Actually that's why we got segmentation fault. Prolog of `_init` and `_fini` is placed in the [sysdeps/x86_64/crtn.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crtn.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD) assembly file:

其中，`PREINIT_FUNCTION` 是设置简况的 `__gmon_start__`。你可能发现，在 [sysdeps/x86_64/crti.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crti.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD)中，我们没有 `return` 指令。事实上，这就是我们获得 `segmentation fault` 的原因。`_init` 和 `_fini` 的序言被放在 [sysdeps/x86_64/crtn.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crtn.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD) 汇编文件中：

```assembly
.section .init,"ax",@progbits
addq $8, %rsp
ret

.section .fini,"ax",@progbits
addq $8, %rsp
ret
```

如果我们把它加到编译过程中，我们的程序会被成功编译和运行。

```
$ gcc -nostdlib /lib64/crt1.o /lib64/crti.o /lib64/crtn.o  -lc -ggdb program.c -o program

$ ./program
x + y + z = 6
```

## 2.4. 结论

现在让我们回到 `_start` 函数，以及尝试去浏览 `main` 函数被调用之前的完整调用链。

`_start` 总是被默认的 `ld` 脚本链接到程序 `.text` 段的起始位置：

```
$ ld --verbose | grep ENTRY
ENTRY(_start)
```

`_start` 函数定义在 [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) 汇编文件中，并且在 `__libc_start_main` 被调用之前做一些准备工作，像从栈上获取 `argc/argv`，栈准备等。来自于 [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=0fb98f1606bab475ab5ba2d0fe08c64f83cce9df;hb=HEAD) 文件中的 `__libc_start_main` 函数注册构造函数和析构函数，开启线程，做一些安全相关的操作，比如在有需要的情况下设置 `stack canary`，调用初始化，最后调用程序的 `main` 函数以及返回结果退出。而构造函数和析构函数分别是 `main` 之前和之后被调用。

```C
result = main (argc, argv, __environ MAIN_AUXVEC_PARAM);
exit (result);
```

为方便展示此章节内容，给出总体调试测试脚本为：
```bash
#[rongtao@localhost prog-exec-why-how]$ cat run.sh 
#!/bin/bash
# rongtao 日期

# 普通
gcc -Wall program.c -o sum.out

# verbose mode 冗长模式
# `cc1` 编译器将编译我们的 `C` 代码并且生成 `/tmp/ccvUWZkF.s` 汇编文件
# 汇编文件被 `GNU as` 编译器编译为目标文件
# 最后我们的目标文件会被 `collect2` 链接到一起
gcc -v -ggdb program.c -o sum.out 2>&1  | sed 's/^/[Verbose] /g'

ldd sum.out | sed 's/^/[ldd] /g'

# -nostdlib 报错
gcc -nostdlib program.c -o sum.out 2>&1  | sed 's/^/[-nostdlib] /g'

# 即使链接标准库，还是编译不行
gcc -nostdlib -lc -ggdb program.c -o sum.out 2>&1| sed 's/^/[-nostdlib-lc] /g'

# _start 在文件 /lib64/crt1.o 中
objdump -d /lib/../lib64/crt1.o 2>&1| sed 's/^/[crt1.o] /g'

# 不会段错误, 但是会段错误
gcc -nostdlib /lib64/crt1.o /lib64/crti.o -lc -ggdb program.c
# ./a.out 段错误

# 
objdump -D /lib64/crti.o 2>&1| sed 's/^/[crti.o] /g'

# 成功编译，成功运行
gcc -nostdlib /lib64/crt1.o /lib64/crti.o /lib64/crtn.o -lc -ggdb program.c 2>&1| sed 's/^/[crtn.o] /g'
./a.out

# `_start` 总是被默认的 `ld` 脚本链接到程序 `.text` 段的起始位置
ld --verbose | grep ENTRY 2>&1| sed 's/^/[ld--verbose] /g'
```

## 2.5. 链接

* [system call](https://en.wikipedia.org/wiki/System_call)
* [gdb](https://www.gnu.org/software/gdb/)
* [execve](http://linux.die.net/man/2/execve)
* [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [segment registers](https://en.wikipedia.org/wiki/X86_memory_segmentation)
* [context switch](https://en.wikipedia.org/wiki/Context_switch)
* [System V ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf)


<center><font size='6'>英文原文</font></center>

Introduction
---------------

During the writing of the [linux-insides](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md) book I have received many emails with questions related to the [linker](https://en.wikipedia.org/wiki/Linker_%28computing%29) script and linker-related subjects. So I've decided to write this to cover some aspects of the linker and the linking of object files.

If we open the `Linker` page on Wikipedia, we will see following definition:

>In computer science, a linker or link editor is a computer program that takes one or more object files generated by a compiler and combines them into a single executable file, library file, or another object file.

If you've written at least one program on C in your life, you will have seen files with the `*.o` extension. These files are [object files](https://en.wikipedia.org/wiki/Object_file). Object files are blocks of machine code and data with placeholder addresses that reference data and functions in other object files or libraries, as well as a list of its own functions and data. The main purpose of the linker is collect/handle the code and data of each object file, turning it into the final executable file or library. In this post we will try to go through all aspects of this process. Let's start.

Linking process
---------------

Let's create a simple project with the following structure:

```
*-linkers
*--main.c
*--lib.c
*--lib.h
```

Our `main.c` source code file contains:

```C
#include <stdio.h>

#include "lib.h"

int main(int argc, char **argv) {
	printf("factorial of 5 is: %d\n", factorial(5));
	return 0;
}
```

The `lib.c` file contains:

```C
int factorial(int base) {
	int res,i = 1;
	
	if (base == 0) {
		return 1;
	}

	while (i <= base) {
		res *= i;
		i++;
	}

	return res;
}
```

And the `lib.h` file contains:

```C
#ifndef LIB_H
#define LIB_H

int factorial(int base);

#endif
```

Now let's compile only the `main.c` source code file with:

```
$ gcc -c main.c
```

If we look inside the outputted object file with the `nm` util, we will see the
following output:

```
$ nm -A main.o
main.o:                 U factorial
main.o:0000000000000000 T main
main.o:                 U printf
```

The `nm` util allows us to see the list of symbols from the given object file. It consists of three columns: the first is the name of the given object file and the address of any resolved symbols. The second column contains a character that represents the status of the given symbol. In this case the `U` means `undefined` and the `T` denotes that the symbols are placed in the `.text` section of the object. The `nm` utility shows us here that we have three symbols in the `main.c` source code file:

* `factorial` - the factorial function defined in the `lib.c` source code file. It is marked as `undefined` here because we compiled only the `main.c` source code file, and it does not know anything about code from the `lib.c` file for now;
* `main` - the main function;
* `printf` - the function from the [glibc](https://en.wikipedia.org/wiki/GNU_C_Library) library. `main.c` does not know anything about it for now either.

What can we understand from the output of `nm` so far? The `main.o` object file contains the local symbol `main` at address `0000000000000000` (it will be filled with the correct address after it is linked), and two unresolved symbols. We can see all of this information in the disassembly output of the `main.o` object file:

```
$ objdump -S main.o

main.o:     file format elf64-x86-64
Disassembly of section .text:

0000000000000000 <main>:
   0:	55                   	push   %rbp
   1:	48 89 e5             	mov    %rsp,%rbp
   4:	48 83 ec 10          	sub    $0x10,%rsp
   8:	89 7d fc             	mov    %edi,-0x4(%rbp)
   b:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
   f:	bf 05 00 00 00       	mov    $0x5,%edi
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  19:	89 c6                	mov    %eax,%esi
  1b:	bf 00 00 00 00       	mov    $0x0,%edi
  20:	b8 00 00 00 00       	mov    $0x0,%eax
  25:	e8 00 00 00 00       	callq  2a <main+0x2a>
  2a:	b8 00 00 00 00       	mov    $0x0,%eax
  2f:	c9                   	leaveq 
  30:	c3                   	retq   
```

Here we are interested only in the two `callq` operations. The two `callq` operations contain `linker stubs`, or the function name and offset from it to the next instruction. These stubs will be updated to the real addresses of the functions. We can see these functions' names within the following `objdump` output:

```
$ objdump -S -r main.o

...
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  15: R_X86_64_PC32	               factorial-0x4
  19:	89 c6                	mov    %eax,%esi
...
  25:	e8 00 00 00 00       	callq  2a <main+0x2a>
  26:   R_X86_64_PC32	               printf-0x4
  2a:	b8 00 00 00 00       	mov    $0x0,%eax
...
```

The `-r` or `--reloc ` flags of the `objdump` util print the `relocation` entries of the file. Now let's look in more detail at the relocation process.

Relocation
------------

Relocation is the process of connecting symbolic references with symbolic definitions. Let's look at the previous snippet from the `objdump` output:

```
  14:	e8 00 00 00 00       	callq  19 <main+0x19>
  15:   R_X86_64_PC32	               factorial-0x4
  19:	89 c6                	mov    %eax,%esi
```

Note the `e8 00 00 00 00` on the first line. The `e8` is the [opcode](https://en.wikipedia.org/wiki/Opcode) of the `call`, and the remainder of the line is a relative offset. So the `e8 00 00 00 00` contains a one-byte operation code followed by a four-byte address. Note that the `00 00 00 00` is 4-bytes. Why only 4-bytes if an address can be 8-bytes in a `x86_64` (64-bit) machine? Actually, we compiled the `main.c` source code file with the `-mcmodel=small`! From the `gcc` man page:

```
-mcmodel=small

Generate code for the small code model: the program and its symbols must be linked in the lower 2 GB of the address space. Pointers are 64 bits. Programs can be statically or dynamically linked. This is the default code model.
```

Of course we didn't pass this option to the `gcc` when we compiled the `main.c`, but it is the default. We know that our program will be linked in the lower 2 GB of the address space from the `gcc` manual extract above. Four bytes is therefore enough for this. So we have the opcode of the `call` instruction and an unknown address. When we compile `main.c` with all its dependencies to an executable file, and then look at the factorial call, we see:

```
$ gcc main.c lib.c -o factorial | objdump -S factorial | grep factorial

factorial:     file format elf64-x86-64
...
...
0000000000400506 <main>:
	40051a:	e8 18 00 00 00       	callq  400537 <factorial>
...
...
0000000000400537 <factorial>:
	400550:	75 07                	jne    400559 <factorial+0x22>
	400557:	eb 1b                	jmp    400574 <factorial+0x3d>
	400559:	eb 0e                	jmp    400569 <factorial+0x32>
	40056f:	7e ea                	jle    40055b <factorial+0x24>
...
...
```

As we can see in the previous output, the address of the `main` function is `0x0000000000400506`. Why it does not start from `0x0`? You may already know that standard C programs are linked with the `glibc` C standard library (assuming the `-nostdlib` was not passed to the `gcc`). The compiled code for a program includes constructor functions to initialize data in the program when the program is started. These functions need to be called before the program is started, or in another words before the `main` function is called. To make the initialization and termination functions work, the compiler must output something in the assembler code to cause those functions to be called at the appropriate time. Execution of this program will start from the code placed in the special `.init` section. We can see this in the beginning of the objdump output:

```
objdump -S factorial | less

factorial:     file format elf64-x86-64

Disassembly of section .init:

00000000004003a8 <_init>:
  4003a8:       48 83 ec 08             sub    $0x8,%rsp
  4003ac:       48 8b 05 a5 05 20 00    mov    0x2005a5(%rip),%rax        # 600958 <_DYNAMIC+0x1d0>
```

Not that it starts at the `0x00000000004003a8` address relative to the `glibc` code. We can check it also in the [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) output by running `readelf`:

```
$ readelf -d factorial | grep \(INIT\)
 0x000000000000000c (INIT)               0x4003a8
 ```

So, the address of the `main` function is `0000000000400506` and is offset from the `.init` section. As we can see from the output, the address of the `factorial` function is `0x0000000000400537` and binary code for the call of the `factorial` function now is `e8 18 00 00 00`. We already know that `e8` is opcode for the `call` instruction, the next `18 00 00 00` (note that address represented as little endian for `x86_64`, so it is `00 00 00 18`) is the offset from the `callq` to the `factorial` function:

```python
>>> hex(0x40051a + 0x18 + 0x5) == hex(0x400537)
True
```

So we add `0x18` and `0x5` to the address of the `call` instruction. The offset is measured from the address of the following instruction. Our call instruction is 5-bytes long (`e8 18 00 00 00`) and the `0x18` is the offset after the call instruction to the `factorial` function. A compiler generally creates each object file with the program addresses starting at zero. But if a program is created from multiple object files, these will overlap.

What we have seen in this section is the `relocation` process. This process assigns load addresses to the various parts of the program, adjusting the code and data in the program to reflect the assigned addresses.

Ok, now that we know a little about linkers and relocation, it is time to learn more about linkers by linking our object files.

GNU linker
-----------------

As you can understand from the title, I will use [GNU linker](https://en.wikipedia.org/wiki/GNU_linker) or just `ld` in this post. Of course we can use `gcc` to link our `factorial` project:

```
$ gcc main.c lib.o -o factorial
```

and after it we will get executable file - `factorial` as a result:

```
./factorial 
factorial of 5 is: 120
```

But `gcc` does not link object files. Instead it uses `collect2` which is just wrapper for the `GNU ld` linker:

```
~$ /usr/lib/gcc/x86_64-linux-gnu/4.9/collect2 --version
collect2 version 4.9.3
/usr/bin/ld --version
GNU ld (GNU Binutils for Debian) 2.25
...
...
...
```

Ok, we can use gcc and it will produce executable file of our program for us. But let's look how to use `GNU ld` linker for the same purpose. First of all let's try to link these object files with the following example:

```
ld main.o lib.o -o factorial
```

Try to do it and you will get following error:

```
$ ld main.o lib.o -o factorial
ld: warning: cannot find entry symbol _start; defaulting to 00000000004000b0
main.o: In function `main':
main.c:(.text+0x26): undefined reference to `printf'
```

Here we can see two problems:

* Linker can't find `_start` symbol;
* Linker does not know anything about `printf` function.

First of all let's try to understand what is this `_start` entry symbol that appears to be required for our program to run? When I started to learn programming I learned that the `main` function is the entry point of the program. I think you learned this too :) But it actually isn't the entry point, it's `_start` instead. The `_start` symbol is defined in the `crt1.o` object file. We can find it with the following command:

```
$ objdump -S /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o

/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_start>:
   0:	31 ed                	xor    %ebp,%ebp
   2:	49 89 d1             	mov    %rdx,%r9
   ...
   ...
   ...
```

We pass this object file to the `ld` command as its first argument (see above). Now let's try to link it and will look on result:

```
ld /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
main.o lib.o -o factorial

/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o: In function `_start':
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:115: undefined reference to `__libc_csu_fini'
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:116: undefined reference to `__libc_csu_init'
/tmp/buildd/glibc-2.19/csu/../sysdeps/x86_64/start.S:122: undefined reference to `__libc_start_main'
main.o: In function `main':
main.c:(.text+0x26): undefined reference to `printf'
```

Unfortunately we will see even more errors. We can see here old error about undefined `printf` and yet another three undefined references:

* `__libc_csu_fini`
* `__libc_csu_init`
* `__libc_start_main`

The `_start` symbol is defined in the [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=0d27a38e9c02835ce17d1c9287aa01be222e72eb;hb=HEAD) assembly file in the `glibc` source code. We can find following assembly code lines there:

```assembly
mov $__libc_csu_fini, %R8_LP
mov $__libc_csu_init, %RCX_LP
...
call __libc_start_main
```

Here we pass address of the entry point to the `.init` and `.fini` section that contain code that starts to execute when the program is ran and the code that executes when program terminates. And in the end we see the call of the `main` function from our program. These three symbols are defined in the [csu/elf-init.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/elf-init.c;hb=1d4bbc54bd4f7d85d774871341b49f4357af1fb7) source code file. The following two object files:

* `crtn.o`;
* `crti.o`.

define the function prologs/epilogs for the .init and .fini sections (with the `_init` and `_fini` symbols respectively).

The `crtn.o` object file contains these `.init` and `.fini` sections:

```
$ objdump -S /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o

0000000000000000 <.init>:
   0:	48 83 c4 08          	add    $0x8,%rsp
   4:	c3                   	retq   

Disassembly of section .fini:

0000000000000000 <.fini>:
   0:	48 83 c4 08          	add    $0x8,%rsp
   4:	c3                   	retq   
```

And the `crti.o` object file contains the `_init` and `_fini` symbols. Let's try to link again with these two object files:

```
$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o \
-o factorial
```

And anyway we will get the same errors. Now we need to pass `-lc` option to the `ld`. This option will search for the standard library in the paths present in the `$LD_LIBRARY_PATH` environment variable. Let's try to link again wit the `-lc` option:

```
$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o -lc \
-o factorial
```

Finally we get an executable file, but if we try to run it, we will get strange results:

```
$ ./factorial 
bash: ./factorial: No such file or directory
```

What's the problem here? Let's look on the executable file with the [readelf](https://sourceware.org/binutils/docs/binutils/readelf.html) util:

```
$ readelf -l factorial 

Elf file type is EXEC (Executable file)
Entry point 0x4003c0
There are 7 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x0000000000400040 0x0000000000400040
                 0x0000000000000188 0x0000000000000188  R E    8
  INTERP         0x00000000000001c8 0x00000000004001c8 0x00000000004001c8
                 0x000000000000001c 0x000000000000001c  R      1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x0000000000000000 0x0000000000400000 0x0000000000400000
                 0x0000000000000610 0x0000000000000610  R E    200000
  LOAD           0x0000000000000610 0x0000000000600610 0x0000000000600610
                 0x00000000000001cc 0x00000000000001cc  RW     200000
  DYNAMIC        0x0000000000000610 0x0000000000600610 0x0000000000600610
                 0x0000000000000190 0x0000000000000190  RW     8
  NOTE           0x00000000000001e4 0x00000000004001e4 0x00000000004001e4
                 0x0000000000000020 0x0000000000000020  R      4
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     10

 Section to Segment mapping:
  Segment Sections...
   00     
   01     .interp 
   02     .interp .note.ABI-tag .hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn .rela.plt .init .plt .text .fini .rodata .eh_frame 
   03     .dynamic .got .got.plt .data 
   04     .dynamic 
   05     .note.ABI-tag 
   06     
```

Note on the strange line:

```
  INTERP         0x00000000000001c8 0x00000000004001c8 0x00000000004001c8
                 0x000000000000001c 0x000000000000001c  R      1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
```

The `.interp` section in the `elf` file holds the path name of a program interpreter or in another words the `.interp` section simply contains an `ascii` string that is the name of the dynamic linker. The dynamic linker is the part of Linux that loads and links shared libraries needed by an executable when it is executed, by copying the content of libraries from disk to RAM. As we can see in the output of the `readelf` command it is placed in the `/lib64/ld-linux-x86-64.so.2` file for the `x86_64` architecture. Now let's add the `-dynamic-linker` option with the path of `ld-linux-x86-64.so.2` to the `ld` call and will see the following results:

```
$ gcc -c main.c lib.c

$ ld \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crtn.o main.o lib.o \
-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
-lc -o factorial
```

Now we can run it as normal executable file:

```
$ ./factorial

factorial of 5 is: 120
```

It works! With the first line we compile the `main.c` and the `lib.c` source code files to object files. We will get the `main.o` and the `lib.o` after execution of the `gcc`:

```
$ file lib.o main.o
lib.o:  ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
main.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

and after this we link object files of our program with the needed system object files and libraries. We just saw a simple example of how to compile and link a C program with the `gcc` compiler and `GNU ld` linker. In this example we have used a couple command line options of the `GNU linker`, but it supports much more command line options than `-o`, `-dynamic-linker`, etc... Moreover `GNU ld` has its own language that allows to control the linking process. In the next two paragraphs we will look into it.

Useful command line options of the GNU linker
----------------------------------------------

As I already wrote and as you can see in the manual of the `GNU linker`, it has big set of the command line options. We've seen a couple of options in this post: `-o <output>` - that tells `ld` to produce an output file called `output` as the result of linking, `-l<name>` that adds the archive or object file specified by the name, `-dynamic-linker` that specifies the name of the dynamic linker. Of course `ld` supports much more command line options, let's look at some of them.

The first useful command line option is `@file`. In this case the `file` specifies filename where command line options will be read. For example we can create file with the name `linker.ld`, put there our command line arguments from the previous example and execute it with:

```
$ ld @linker.ld
```

The next command line option is `-b` or `--format`. This command line option specifies format of the input object files `ELF`, `DJGPP/COFF` and etc. There is a command line option for the same purpose but for the output file: `--oformat=output-format`.

The next command line option is `--defsym`. Full format of this command line option is the `--defsym=symbol=expression`. It allows to create global symbol in the output file containing the absolute address given by expression. We can find following case where this command line option can be useful: in the Linux kernel source code and more precisely in the Makefile that is related to the kernel decompression for the ARM architecture - [arch/arm/boot/compressed/Makefile](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/arm/boot/compressed/Makefile), we can find following definition:

```
LDFLAGS_vmlinux = --defsym _kernel_bss_size=$(KBSS_SZ)
```

As we already know, it defines the `_kernel_bss_size` symbol with the size of the `.bss` section in the output file. This symbol will be used in the first [assembly file](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/arm/boot/compressed/head.S) that will be executed during kernel decompressing:

```assembly
ldr r5, =_kernel_bss_size
```

The next command line options is the `-shared` that allows us to create shared library. The `-M` or `-map <filename>` command line option prints the linking map with the information about symbols. In our case:

```
$ ld -M @linker.ld
...
...
...
.text           0x00000000004003c0      0x112
 *(.text.unlikely .text.*_unlikely .text.unlikely.*)
 *(.text.exit .text.exit.*)
 *(.text.startup .text.startup.*)
 *(.text.hot .text.hot.*)
 *(.text .stub .text.* .gnu.linkonce.t.*)
 .text          0x00000000004003c0       0x2a /usr/lib/gcc/x86_64-linux-gnu/4.9/../../../x86_64-linux-gnu/crt1.o
...
...
...
 .text          0x00000000004003ea       0x31 main.o
                0x00000000004003ea                main
 .text          0x000000000040041b       0x3f lib.o
                0x000000000040041b                factorial
```

Of course the `GNU linker` support standard command line options: `--help` and `--version` that print common help of the usage of the `ld` and its version. That's all about command line options of the `GNU linker`. Of course it is not the full set of command line options supported by the `ld` util. You can find the complete documentation of the `ld` util in the manual.

Control Language linker
----------------------------------------------

As I wrote previously, `ld` has support for its own language. It accepts Linker Command Language files written in a superset of AT&T's Link Editor Command Language syntax, to provide explicit and total control over the linking process. Let's look on its details.

With the linker language we can control:

* input files;
* output files;
* file formats
* addresses of sections;
* etc...

Commands written in the linker control language are usually placed in a file called linker script. We can pass it to `ld` with the `-T` command line option. The main command in a linker script is the `SECTIONS` command. Each linker script must contain this command and it determines the `map` of the output file. The special variable `.` contains current position of the output. Let's write a simple assembly program and we will look at how we can use a linker script to control linking of this program. We will take a hello world program for this example:

```assembly
.data
        msg:    .ascii  "hello, world!\n"

.text

.global _start

_start:
        mov    $1,%rax
        mov    $1,%rdi
        mov    $msg,%rsi
        mov    $14,%rdx
        syscall

        mov    $60,%rax
        mov    $0,%rdi
        syscall
```

We can compile and link it with the following commands:

```
$ as -o hello.o hello.asm
$ ld -o hello hello.o
```

Our program consists from two sections: `.text` contains code of the program and `.data` contains initialized variables. Let's write simple linker script and try to link our `hello.asm` assembly file with it. Our script is:

```
/*
 * Linker script for the factorial
 */
OUTPUT(hello) 
OUTPUT_FORMAT("elf64-x86-64")
INPUT(hello.o)

SECTIONS
{
	. = 0x200000;
	.text : {
	      *(.text)
	}

	. = 0x400000;
	.data : {
	      *(.data)
	}
}
```

On the first three lines you can see a comment written in `C` style. After it the `OUTPUT` and the `OUTPUT_FORMAT` commands specify the name of our executable file and its format. The next command, `INPUT`, specifies the input file to the `ld` linker. Then, we can see the main `SECTIONS` command, which, as I already wrote, must be present in every linker script. The `SECTIONS` command represents the set and order of the sections which will be in the output file. At the beginning of the `SECTIONS` command we can see following line `. = 0x200000`. I already wrote above that `.` command points to the current position of the output. This line says that the code should be loaded at address `0x200000` and the line `. = 0x400000` says that data section should be loaded at address `0x400000`. The second line after the `. = 0x200000` defines `.text` as an output section. We can see `*(.text)` expression inside it. The `*` symbol is wildcard that matches any file name. In other words, the `*(.text)` expression says all `.text` input sections in all input files. We can rewrite it as `hello.o(.text)` for our example. After the following location counter `. = 0x400000`, we can see definition of the data section.

We can compile and link it with the following command:

```
$ as -o hello.o hello.S && ld -T linker.script && ./hello
hello, world!
```

If we look inside it with the `objdump` util, we can see that `.text` section starts from the address `0x200000` and the `.data` sections starts from the address `0x400000`:

```
$ objdump -D hello

Disassembly of section .text:

0000000000200000 <_start>:
  200000:	48 c7 c0 01 00 00 00 	mov    $0x1,%rax
  ...

Disassembly of section .data:

0000000000400000 <msg>:
  400000:	68 65 6c 6c 6f       	pushq  $0x6f6c6c65
  ...
```

Apart from the commands we have already seen, there are a few others. The first is the `ASSERT(exp, message)` that ensures that given expression is not zero. If it is zero, then exit the linker with an error code and print the given error message. If you've read about Linux kernel booting process in the [linux-insides](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md) book, you may know that the setup header of the Linux kernel has offset `0x1f1`. In the linker script of the Linux kernel we can find a check for this:

```
. = ASSERT(hdr == 0x1f1, "The setup header has the wrong offset!");
```

The `INCLUDE filename` command allows to include external linker script symbols in the current one. In a linker script we can assign a value to a symbol. `ld` supports a couple of assignment operators:

* symbol = expression   ;
* symbol += expression  ;
* symbol -= expression  ;
* symbol *= expression  ;
* symbol /= expression  ;
* symbol <<= expression ;
* symbol >>= expression ;
* symbol &= expression  ;
* symbol |= expression  ;

As you can note all operators are C assignment operators. For example we can use it in our linker script as:

```
START_ADDRESS = 0x200000;
DATA_OFFSET   = 0x200000;

SECTIONS
{
	. = START_ADDRESS;
	.text : {
	      *(.text)
	}

	. = START_ADDRESS + DATA_OFFSET;
	.data : {
	      *(.data)
	}
}
```

As you already may noted the syntax for expressions in the linker script language is identical to that of C expressions. Besides this the control language of the linking supports following builtin functions:

* `ABSOLUTE` - returns absolute value of the given expression;
* `ADDR` - takes the section and returns its address;
* `ALIGN` - returns the value of the location counter (`.` operator) that aligned by the boundary of the next expression after the given expression;
* `DEFINED` - returns `1` if the given symbol placed in the global symbol table and `0` in other way;
* `MAX` and `MIN` - return maximum and minimum of the two given expressions;
* `NEXT` - returns the next unallocated address that is a multiple of the give expression;
* `SIZEOF` - returns the size in bytes of the given named section.

That's all.

Conclusion
-----------------

This is the end of the post about linkers. We learned many things about linkers in this post, such as what is a linker and why it is needed, how to use it, etc..

If you have any questions or suggestions, write me an [email](mailto:kuleshovmail@gmail.com) or ping [me](https://twitter.com/0xAX) on twitter.

Please note that English is not my first language, and I am really sorry for any inconvenience. If you find any mistakes please let me know via email or send a PR.

Links
-----------------

* [Book about Linux kernel insides](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md)
* [linker](https://en.wikipedia.org/wiki/Linker_%28computing%29)
* [object files](https://en.wikipedia.org/wiki/Object_file)
* [glibc](https://en.wikipedia.org/wiki/GNU_C_Library)
* [opcode](https://en.wikipedia.org/wiki/Opcode)
* [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
* [GNU linker](https://en.wikipedia.org/wiki/GNU_linker)
* [My posts about assembly programming for x86_64](https://0xax.github.io/categories/assembler/)
* [readelf](https://sourceware.org/binutils/docs/binutils/readelf.html)


Program startup process in userspace
================================================================================

Introduction
--------------------------------------------------------------------------------

Despite the [linux-insides](https://www.gitbook.com/book/0xax/linux-insides/details) described mostly Linux kernel related stuff, I have decided to write this one part which mostly related to userspace.

There is already fourth [part](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-4) of [System calls](https://en.wikipedia.org/wiki/System_call) chapter which describes what does the Linux kernel do when we want to start a program. In this part I want to explore what happens when we run a program on a Linux machine from userspace perspective.

I don't know how about you, but in my university I learn that a `C` program starts executing from the function which is called `main`. And that's partly true. Whenever we are starting to write new program, we start our program from the following lines of code:

```C
int main(int argc, char *argv[]) {
	// Entry point is here
}
```

But if you are interested in low-level programming, you may already know that the `main` function isn't the actual entry point of a program. You will believe it's true after you look at this simple program in debugger:

```C
int main(int argc, char *argv[]) {
	return 0;
}
```

Let's compile this and run in [gdb](https://www.gnu.org/software/gdb/):

```
$ gcc -ggdb program.c -o program
$ gdb ./program
The target architecture is assumed to be i386:x86-64:intel
Reading symbols from ./program...done.
```

Let's execute gdb `info` subcommand with `files` argument. The `info files` prints information about debugging targets and memory spaces occupied by different sections.

```
(gdb) info files
Symbols from "/home/alex/program".
Local exec file:
	`/home/alex/program', file type elf64-x86-64.
	Entry point: 0x400430
	0x0000000000400238 - 0x0000000000400254 is .interp
	0x0000000000400254 - 0x0000000000400274 is .note.ABI-tag
	0x0000000000400274 - 0x0000000000400298 is .note.gnu.build-id
	0x0000000000400298 - 0x00000000004002b4 is .gnu.hash
	0x00000000004002b8 - 0x0000000000400318 is .dynsym
	0x0000000000400318 - 0x0000000000400357 is .dynstr
	0x0000000000400358 - 0x0000000000400360 is .gnu.version
	0x0000000000400360 - 0x0000000000400380 is .gnu.version_r
	0x0000000000400380 - 0x0000000000400398 is .rela.dyn
	0x0000000000400398 - 0x00000000004003c8 is .rela.plt
	0x00000000004003c8 - 0x00000000004003e2 is .init
	0x00000000004003f0 - 0x0000000000400420 is .plt
	0x0000000000400420 - 0x0000000000400428 is .plt.got
	0x0000000000400430 - 0x00000000004005e2 is .text
	0x00000000004005e4 - 0x00000000004005ed is .fini
	0x00000000004005f0 - 0x0000000000400610 is .rodata
	0x0000000000400610 - 0x0000000000400644 is .eh_frame_hdr
	0x0000000000400648 - 0x000000000040073c is .eh_frame
	0x0000000000600e10 - 0x0000000000600e18 is .init_array
	0x0000000000600e18 - 0x0000000000600e20 is .fini_array
	0x0000000000600e20 - 0x0000000000600e28 is .jcr
	0x0000000000600e28 - 0x0000000000600ff8 is .dynamic
	0x0000000000600ff8 - 0x0000000000601000 is .got
	0x0000000000601000 - 0x0000000000601028 is .got.plt
	0x0000000000601028 - 0x0000000000601034 is .data
	0x0000000000601034 - 0x0000000000601038 is .bss
```

Note on `Entry point: 0x400430` line. Now we know the actual address of entry point of our program. Let's put a breakpoint by this address, run our program and see what happens:

```
(gdb) break *0x400430
Breakpoint 1 at 0x400430
(gdb) run
Starting program: /home/alex/program 

Breakpoint 1, 0x0000000000400430 in _start ()
```

Interesting. We don't see execution of the `main` function here, but we have seen that another function is called. This function is `_start` and as our debugger shows us, it is the actual entry point of our program. Where is this function from? Who does call `main` and when is it called? I will try to answer all these questions in the following post.

How the kernel starts a new program
--------------------------------------------------------------------------------

First of all, let's take a look at the following simple `C` program:

```C
// program.c

#include <stdlib.h>
#include <stdio.h>

static int x = 1;

int y = 2;

int main(int argc, char *argv[]) {
	int z = 3;

	printf("x + y + z = %d\n", x + y + z);

	return EXIT_SUCCESS;
}
```

We can be sure that this program works as we expect. Let's compile it:

```
$ gcc -Wall program.c -o sum
```

and run:

```
$ ./sum
x + y + z = 6
```

Ok, everything looks pretty good up to now. You may already know that there is a special family of functions - [exec*](http://man7.org/linux/man-pages/man3/execl.3.html). As we read in the man page:

> The exec() family of functions replaces the current process image with a new process image.

All the `exec*` functions are simple frontends to the [execve](http://man7.org/linux/man-pages/man2/execve.2.html) system call. If you have read the fourth [part](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-4) of the chapter which describes [system calls](https://en.wikipedia.org/wiki/System_call), you may know that the [execve](http://linux.die.net/man/2/execve) system call is defined in the [files/exec.c](https://github.com/torvalds/linux/blob/08e4e0d0456d0ca8427b2d1ddffa30f1c3e774d7/fs/exec.c#L1888) source code file and looks like:

```C
SYSCALL_DEFINE3(execve,
		const char __user *, filename,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp)
{
	return do_execve(getname(filename), argv, envp);
}
```

It takes an executable file name, set of command line arguments, and set of enviroment variables. As you may guess, everything is done by the `do_execve` function. I will not describe the implementation of the `do_execve` function in detail because you can read about this in [here](https://0xax.gitbook.io/linux-insides/summary/syscall/linux-syscall-4). But in short words, the `do_execve` function does many checks like `filename` is valid, limit of launched processes is not exceed in our system and etc. After all of these checks, this function parses our executable file which is represented in [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) format, creates memory descriptor for newly executed executable file and fills it with the appropriate values like area for the stack, heap and etc. When the setup of new binary image is done, the `start_thread` function will set up one new process. This function is architecture-specific and for the [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture, its definition will be located in the [arch/x86/kernel/process_64.c](https://github.com/torvalds/linux/blob/08e4e0d0456d0ca8427b2d1ddffa30f1c3e774d7/arch/x86/kernel/process_64.c#L239) source code file.

The `start_thread` function sets new value to [segment registers](https://en.wikipedia.org/wiki/X86_memory_segmentation) and program execution address. From this point, our new process is ready to start. Once the [context switch](https://en.wikipedia.org/wiki/Context_switch) will be done, control will be returned to userspace with new values of registers and the new executable will be started to execute.

That's all from the kernel side. The Linux kernel prepares the binary image for execution and its execution starts right after the context switch and returns controll to userspace when it is finished. But it does not answer our questions like where does `_start` come from and others. Let's try to answer these questions in the next paragraph.

How does a program start in userspace
--------------------------------------------------------------------------------

In the previous paragraph we saw how an executable file is prepared to run by the Linux kernel. Let's look at the same, but from userspace side. We already know that the entry point of each program is its `_start` function. But where is this function from? It may came from a library. But if you remember correctly we didn't link our program with any libraries during compilation of our program:

```
$ gcc -Wall program.c -o sum
```

You may guess that `_start` comes from the [standard library](https://en.wikipedia.org/wiki/Standard_library) and that's true. If you try to compile our program again and pass the `-v` option to gcc which will enable `verbose mode`, you will see a long output. The full output is not interesting for us, let's look at the following steps: 

First of all, our program should be compiled with `gcc`:

```
$ gcc -v -ggdb program.c -o sum
...
...
...
/usr/libexec/gcc/x86_64-redhat-linux/6.1.1/cc1 -quiet -v program.c -quiet -dumpbase program.c -mtune=generic -march=x86-64 -auxbase test -ggdb -version -o /tmp/ccvUWZkF.s
...
...
...
```

The `cc1` compiler will compile our `C` source code and an produce assembly named `/tmp/ccvUWZkF.s` file. After this we can see that our assembly file will be compiled into object file with the `GNU as` assembler:

```
$ gcc -v -ggdb program.c -o sum
...
...
...
as -v --64 -o /tmp/cc79wZSU.o /tmp/ccvUWZkF.s
...
...
...
```

In the end our object file will be linked by `collect2`:

```
$ gcc -v -ggdb program.c -o sum
...
...
...
/usr/libexec/gcc/x86_64-redhat-linux/6.1.1/collect2 -plugin /usr/libexec/gcc/x86_64-redhat-linux/6.1.1/liblto_plugin.so -plugin-opt=/usr/libexec/gcc/x86_64-redhat-linux/6.1.1/lto-wrapper -plugin-opt=-fresolution=/tmp/ccLEGYra.res -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s -plugin-opt=-pass-through=-lc -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s --build-id --no-add-needed --eh-frame-hdr --hash-style=gnu -m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o test /usr/lib/gcc/x86_64-redhat-linux/6.1.1/../../../../lib64/crt1.o /usr/lib/gcc/x86_64-redhat-linux/6.1.1/../../../../lib64/crti.o /usr/lib/gcc/x86_64-redhat-linux/6.1.1/crtbegin.o -L/usr/lib/gcc/x86_64-redhat-linux/6.1.1 -L/usr/lib/gcc/x86_64-redhat-linux/6.1.1/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L. -L/usr/lib/gcc/x86_64-redhat-linux/6.1.1/../../.. /tmp/cc79wZSU.o -lgcc --as-needed -lgcc_s --no-as-needed -lc -lgcc --as-needed -lgcc_s --no-as-needed /usr/lib/gcc/x86_64-redhat-linux/6.1.1/crtend.o /usr/lib/gcc/x86_64-redhat-linux/6.1.1/../../../../lib64/crtn.o
...
...
...
```

Yes, we can see a long set of command line options which are passed to the linker. Let's go from another way. We know that our program depends on `stdlib`:

```
$ ldd program
	linux-vdso.so.1 (0x00007ffc9afd2000)
	libc.so.6 => /lib64/libc.so.6 (0x00007f56b389b000)
	/lib64/ld-linux-x86-64.so.2 (0x0000556198231000)
```

as we use some stuff from there like `printf` and etc. But not only. That's why we will get an error when we pass `-nostdlib` option to the compiler:

```
$ gcc -nostdlib program.c -o program
/usr/bin/ld: warning: cannot find entry symbol _start; defaulting to 000000000040017c
/tmp/cc02msGW.o: In function `main':
/home/alex/program.c:11: undefined reference to `printf'
collect2: error: ld returned 1 exit status
```

Besides other errors, we also see that `_start` symbol is undefined. So now we are sure that the `_start` function comes from standard library. But even if we link it with the standard library, it will not be compiled successfully anyway:

```
$ gcc -nostdlib -lc -ggdb program.c -o program
/usr/bin/ld: warning: cannot find entry symbol _start; defaulting to 0000000000400350
```

Ok, the compiler does not complain about undefined reference of standard library functions anymore as we linked our program with `/usr/lib64/libc.so.6`, but the `_start` symbol isn't resolved yet. Let's return to the verbose output of `gcc` and look at the parameters of `collect2`. The most important thing that we may see is that our program is linked not only with the standard library, but also with some object files. The first object file is: `/lib64/crt1.o`. And if we look inside this object file with `objdump`, we will see the `_start` symbol: 

```
$ objdump -d /lib64/crt1.o 

/lib64/crt1.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <_start>:
   0:	31 ed                	xor    %ebp,%ebp
   2:	49 89 d1             	mov    %rdx,%r9
   5:	5e                   	pop    %rsi
   6:	48 89 e2             	mov    %rsp,%rdx
   9:	48 83 e4 f0          	and    $0xfffffffffffffff0,%rsp
   d:	50                   	push   %rax
   e:	54                   	push   %rsp
   f:	49 c7 c0 00 00 00 00 	mov    $0x0,%r8
  16:	48 c7 c1 00 00 00 00 	mov    $0x0,%rcx
  1d:	48 c7 c7 00 00 00 00 	mov    $0x0,%rdi
  24:	e8 00 00 00 00       	callq  29 <_start+0x29>
  29:	f4                   	hlt    
```

As `crt1.o` is a shared object file, we see only stubs here instead of real calls. Let's look at the source code of the `_start` function. As this function is architecture specific, implementation for `_start` will be located in the [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) assembly file.

The `_start` starts from the clearing of `ebp` register as [ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf) suggests.

```assembly
xorl %ebp, %ebp
```

And after this we put the address of termination function to the `r9` register:

```assembly
mov %RDX_LP, %R9_LP
```

As described in the [ELF](http://flint.cs.yale.edu/cs422/doc/ELF_Format.pdf) specification:

> After the dynamic linker has built the process image and performed the relocations, each shared object
> gets the opportunity to execute some initialization code.
> ...
> Similarly, shared objects may have termination functions, which are executed with the atexit (BA_OS)
> mechanism after the base process begins its termination sequence.

So we need to put the address of the termination function to the `r9` register as it will be passed to `__libc_start_main` in future as sixth argument. Note that the address of the termination function initially is located in the `rdx` register. Other registers besides `rdx` and `rsp` contain unspecified values. Actually the main point of the `_start` function is to call `__libc_start_main`. So the next action is to prepare for this function.

The signature of the `__libc_start_main` function is located in the [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=9a56dcbbaeb7ef85c495b4df9ab1d0b13454c043;hb=HEAD#l107) source code file. Let's look on it:

```C
STATIC int LIBC_START_MAIN (int (*main) (int, char **, char **),
 			                int argc,
			                char **argv,
 			                __typeof (main) init,
			                void (*fini) (void),
			                void (*rtld_fini) (void),
			                void *stack_end)
```

It takes the address of the `main` function of a program, `argc` and `argv`. `init` and `fini` functions are constructor and destructor of the program. The `rtld_fini` is the termination function which will be called after the program will be exited to terminate and free its dynamic section. The last parameter of the `__libc_start_main` is a pointer to the stack of the program. Before we can call the `__libc_start_main` function, all of these parameters must be prepared and passed to it. Let's return to the [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) assembly file and continue to see what happens before the `__libc_start_main` function will be called from there.

We can get all the arguments we need for `__libc_start_main` function from the stack. At the very beginning, when `_start` is called, our stack looks like:

```
+-----------------+
|       NULL      |
+-----------------+ 
|       ...       |
|       envp      |
|       ...       |
+-----------------+ 
|       NULL      |
+------------------
|       ...       |
|       argv      |
|       ...       |
+------------------
|       argc      | <- rsp
+-----------------+ 
```

After we cleared `ebp` register and saved the address of the termination function in the `r9` register, we pop an element from the stack to the `rsi` register, so after this `rsp` will point to the `argv` array and `rsi` will contain count of command line arguemnts passed to the program:

```
+-----------------+
|       NULL      |
+-----------------+ 
|       ...       |
|       envp      |
|       ...       |
+-----------------+ 
|       NULL      |
+------------------
|       ...       |
|       argv      |
|       ...       | <- rsp
+-----------------+
```

After this we move the address of the `argv` array to the `rdx` register

```assembly
popq %rsi
mov %RSP_LP, %RDX_LP
```

From this moment we have `argc` and `argv`. We still need to put pointers to the construtor, destructor in appropriate registers and pass pointer to the stack. At the first following three lines we align stack to `16` bytes boundary as suggested in [ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf) and push `rax` which contains garbage:

```assembly
and  $~15, %RSP_LP
pushq %rax

pushq %rsp
mov $__libc_csu_fini, %R8_LP
mov $__libc_csu_init, %RCX_LP
mov $main, %RDI_LP
```

After stack aligning we push the address of the stack, move the addresses of contstructor and destructor to the `r8` and `rcx` registers and address of the `main` symbol to the `rdi`. From this moment we can call the `__libc_start_main` function from the [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=0fb98f1606bab475ab5ba2d0fe08c64f83cce9df;hb=HEAD).

Before we look at the `__libc_start_main` function, let's add the `/lib64/crt1.o` and try to compile our program again:

```
$ gcc -nostdlib /lib64/crt1.o -lc -ggdb program.c -o program
/lib64/crt1.o: In function `_start':
(.text+0x12): undefined reference to `__libc_csu_fini'
/lib64/crt1.o: In function `_start':
(.text+0x19): undefined reference to `__libc_csu_init'
collect2: error: ld returned 1 exit status
```

Now we see another error that both `__libc_csu_fini` and `__libc_csu_init` functions are not found. We know that the addresses of these two functions are passed to the `__libc_start_main` as parameters and also these functions are constructor and destructor of our programs. But what do `constructor` and `destructor` in terms of `C` program means? We already saw the quote from the [ELF](http://flint.cs.yale.edu/cs422/doc/ELF_Format.pdf) specification:

> After the dynamic linker has built the process image and performed the relocations, each shared object
> gets the opportunity to execute some initialization code.
> ...
> Similarly, shared objects may have termination functions, which are executed with the atexit (BA_OS)
> mechanism after the base process begins its termination sequence.

So the linker creates two special sections besides usual sections like `.text`, `.data` and others:

* `.init`
* `.fini`

We can find them with the `readelf` util:

```
$ readelf -e test | grep init
  [11] .init             PROGBITS         00000000004003c8  000003c8

$ readelf -e test | grep fini
  [15] .fini             PROGBITS         0000000000400504  00000504
```

Both of these sections will be placed at the start and end of the binary image and contain routines which are called constructor and destructor respectively. The main point of these routines is to do some initialization/finalization like initialization of global variables, such as [errno](http://man7.org/linux/man-pages/man3/errno.3.html), allocation and deallocation of memory for system routines and etc., before the actual code of a program is executed.

You may infer from the names of these functions, they will be called before the `main` function and after the `main` function. Definitions of `.init` and `.fini` sections are located in the `/lib64/crti.o` and if we add this object file:

```
$ gcc -nostdlib /lib64/crt1.o /lib64/crti.o  -lc -ggdb program.c -o program
```

we will not get any errors. But let's try to run our program and see what happens:

```
$ ./program
Segmentation fault (core dumped)
```

Yeah, we got segmentation fault. Let's look inside of the `lib64/crti.o` with `objdump`:

```
$ objdump -D /lib64/crti.o

/lib64/crti.o:     file format elf64-x86-64


Disassembly of section .init:

0000000000000000 <_init>:
   0:	48 83 ec 08          	sub    $0x8,%rsp
   4:	48 8b 05 00 00 00 00 	mov    0x0(%rip),%rax        # b <_init+0xb>
   b:	48 85 c0             	test   %rax,%rax
   e:	74 05                	je     15 <_init+0x15>
  10:	e8 00 00 00 00       	callq  15 <_init+0x15>

Disassembly of section .fini:

0000000000000000 <_fini>:
   0:	48 83 ec 08          	sub    $0x8,%rsp
```

As I wrote above, the `/lib64/crti.o` object file contains definition of the `.init` and `.fini` section, but also we can see here the stub for function. Let's look at the source code which is placed in the [sysdeps/x86_64/crti.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crti.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD) source code file:

```assembly
	.section .init,"ax",@progbits
	.p2align 2
	.globl _init
	.type _init, @function
_init:
	subq $8, %rsp
	movq PREINIT_FUNCTION@GOTPCREL(%rip), %rax
	testq %rax, %rax
	je .Lno_weak_fn
	call *%rax
.Lno_weak_fn:
	call PREINIT_FUNCTION
```

It contains the definition of the `.init` section and assembly code does 16-byte stack alignment and next we move address of the `PREINIT_FUNCTION` and if it is zero we don't call it:

```
00000000004003c8 <_init>:
  4003c8:       48 83 ec 08             sub    $0x8,%rsp
  4003cc:       48 8b 05 25 0c 20 00    mov    0x200c25(%rip),%rax        # 600ff8 <_DYNAMIC+0x1d0>
  4003d3:       48 85 c0                test   %rax,%rax
  4003d6:       74 05                   je     4003dd <_init+0x15>
  4003d8:       e8 43 00 00 00          callq  400420 <__libc_start_main@plt+0x10>
  4003dd:       48 83 c4 08             add    $0x8,%rsp
  4003e1:       c3                      retq
```

where the `PREINIT_FUNCTION` is the `__gmon_start__` which does setup for profiling. You may note that we have no return instruction in the [sysdeps/x86_64/crti.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crti.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD). Actually that's why we got a segmentation fault. Prolog of `_init` and `_fini` is placed in the [sysdeps/x86_64/crtn.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/crtn.S;h=e9d86ed08ab134a540e3dae5f97a9afb82cdb993;hb=HEAD) assembly file:

```assembly
.section .init,"ax",@progbits
addq $8, %rsp
ret

.section .fini,"ax",@progbits
addq $8, %rsp
ret
```

and if we will add it to the compilation, our program will be successfully compiled and run!

```
$ gcc -nostdlib /lib64/crt1.o /lib64/crti.o /lib64/crtn.o  -lc -ggdb program.c -o program

$ ./program
x + y + z = 6
```

Conclusion
--------------------------------------------------------------------------------

Now let's return to the `_start` function and try to go through a full chain of calls before the `main` of our program will be called.

The `_start` is always placed at the beginning of the `.text` section in our programs by the linked which is used default `ld` script:

```
$ ld --verbose | grep ENTRY
ENTRY(_start)
```

The `_start` function is defined in the [sysdeps/x86_64/start.S](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/start.S;h=f1b961f5ba2d6a1ebffee0005f43123c4352fbf4;hb=HEAD) assembly file and does preparation like getting `argc/argv` from the stack, stack preparation and etc., before the `__libc_start_main` function will be called. The `__libc_start_main` function from the [csu/libc-start.c](https://sourceware.org/git/?p=glibc.git;a=blob;f=csu/libc-start.c;h=0fb98f1606bab475ab5ba2d0fe08c64f83cce9df;hb=HEAD) source code file does a registration of the constructor and destructor of application which are will be called before `main` and after it, starts up threading, does some security related actions like setting stack canary if need, calls initialization related routines and in the end it calls `main` function of our application and exits with its result:

```C
result = main (argc, argv, __environ MAIN_AUXVEC_PARAM);
exit (result);
```

That's all.

Links
--------------------------------------------------------------------------------

* [system call](https://en.wikipedia.org/wiki/System_call)
* [gdb](https://www.gnu.org/software/gdb/)
* [execve](http://linux.die.net/man/2/execve)
* [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [segment registers](https://en.wikipedia.org/wiki/X86_memory_segmentation)
* [context switch](https://en.wikipedia.org/wiki/Context_switch)
* [System V ABI](https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf)
