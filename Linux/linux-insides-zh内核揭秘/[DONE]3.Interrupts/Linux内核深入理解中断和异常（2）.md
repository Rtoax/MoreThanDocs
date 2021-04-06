<center><font size='6'>Linux内核深入理解中断和异常（2）：初步中断处理-中断加载</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

# 1. 总体概览

关于`idt_table`结构的填充，在5.10.13中流程为：
```c
	idt_setup_early_traps();/* 中断描述符表 */
```
上面函数实现很简单：
```c
void __init idt_setup_early_traps(void)/* 中断描述符表 */
{
	idt_setup_from_table(idt_table, early_idts, ARRAY_SIZE(early_idts),
			     true);
    //调用 `load_idt` 函数来执行 `ldtr` 指令来重新加载 `IDT` 表
	load_idt(&idt_descr);/* 中断描述符 */
}
```
`early_idts`定义为：
```c
/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_idts[] = {/* 中断描述符表 陷阱 trap */
	INTG(X86_TRAP_DB,		asm_exc_debug),
	SYSG(X86_TRAP_BP,		asm_exc_int3),

#ifdef CONFIG_X86_32
	/*
	 * Not possible on 64-bit. See idt_setup_early_pf() for details.
	 */
//	INTG(X86_TRAP_PF,		asm_exc_page_fault),
#endif
};
```
可见只设置了`#DB`和`#BP`。接着：
```c
void __init trap_init(void) /* 陷阱初始化 */
{
	/* Init cpu_entry_area before IST entries are set up */
	setup_cpu_entry_areas();    /*  */

	/* Init GHCB memory pages when running as an SEV-ES guest */
	sev_es_init_vc_handling();  /*  */

	idt_setup_traps();  /* 中断描述附表 */

	/*
	 * Should be a barrier for any external CPU state:
	 */
	cpu_init(); /*  */

	idt_setup_ist_traps();  /* 中断栈表 irq stack table */
}
```
其中`idt_setup_traps`为：
```c
/**
 * idt_setup_traps - Initialize the idt table with default traps
 */
void __init idt_setup_traps(void)   /*  中断描述符表*/
{
	idt_setup_from_table(idt_table, def_idts, ARRAY_SIZE(def_idts), true);
}
```
默认的中断描述符表`def_idts`如下所述：
```c
/*
 * The default IDT entries which are set up in trap_init() before
 * cpu_init() is invoked. Interrupt stacks cannot be used at that point and
 * the traps which use them are reinitialized with IST after cpu_init() has
 * set up TSS.
 */
static const __initconst struct idt_data def_idts[] = {/* 默认的 中断描述符表 */
	INTG(X86_TRAP_DE,		asm_exc_divide_error),
	INTG(X86_TRAP_NMI,		asm_exc_nmi),   //arch/x86/entry/entry_64.S
	INTG(X86_TRAP_BR,		asm_exc_bounds),
	INTG(X86_TRAP_UD,		asm_exc_invalid_op),
	INTG(X86_TRAP_NM,		asm_exc_device_not_available),
	INTG(X86_TRAP_OLD_MF,		asm_exc_coproc_segment_overrun),
	INTG(X86_TRAP_TS,		asm_exc_invalid_tss),
	INTG(X86_TRAP_NP,		asm_exc_segment_not_present),
	INTG(X86_TRAP_SS,		asm_exc_stack_segment),
	INTG(X86_TRAP_GP,		asm_exc_general_protection),
	INTG(X86_TRAP_SPURIOUS,		asm_exc_spurious_interrupt_bug),
	INTG(X86_TRAP_MF,		asm_exc_coprocessor_error),
	INTG(X86_TRAP_AC,		asm_exc_alignment_check),
	INTG(X86_TRAP_XF,		asm_exc_simd_coprocessor_error),

#ifdef CONFIG_X86_32
//	TSKG(X86_TRAP_DF,		GDT_ENTRY_DOUBLEFAULT_TSS),
#else
	INTG(X86_TRAP_DF,		asm_exc_double_fault),
#endif
	INTG(X86_TRAP_DB,		asm_exc_debug),

#ifdef CONFIG_X86_MCE
	INTG(X86_TRAP_MC,		asm_exc_machine_check),
#endif

	SYSG(X86_TRAP_OF,		asm_exc_overflow),
#if defined(CONFIG_IA32_EMULATION)
	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_compat),
#elif defined(CONFIG_X86_32)
//	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_32),
#endif
};
```


其中`idt_setup_ist_traps`为：
```c
/**
 * idt_setup_ist_traps - Initialize the idt table with traps using IST
 */
void __init idt_setup_ist_traps(void)/* IST(Interrupt Stack Table) */
{
	idt_setup_from_table(idt_table, ist_idts/*  */, ARRAY_SIZE(ist_idts), true);
}
```
这里就是将`ist_idts`装入`idt_table`中，`ist_idts`结构如下：
```c
/*
 * The exceptions which use Interrupt stacks. They are setup after
 * cpu_init() when the TSS has been initialized.
 */
static const __initconst struct idt_data ist_idts[] = { /* IST(Interrupt Stack Table) */
	ISTG(X86_TRAP_DB,	asm_exc_debug,			IST_INDEX_DB),
	ISTG(X86_TRAP_NMI,	asm_exc_nmi,			IST_INDEX_NMI),
	ISTG(X86_TRAP_DF,	asm_exc_double_fault,		IST_INDEX_DF),
#ifdef CONFIG_X86_MCE
	ISTG(X86_TRAP_MC,	asm_exc_machine_check,		IST_INDEX_MCE),
#endif
#ifdef CONFIG_AMD_MEM_ENCRYPT
	ISTG(X86_TRAP_VC,	asm_exc_vmm_communication,	IST_INDEX_VC),
#endif
};
```

# 2. Exception Handling

This is the third part of the [chapter](http://0xax.gitbooks.io/linux-insides/content/interrupts/index.html) about an interrupts and an exceptions handling in the Linux kernel and in the previous [part](http://0xax.gitbooks.io/linux-insides/content/interrupts/index.html) we stopped at the `setup_arch` function from the [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/blame/master/arch/x86/kernel/setup.c) source code file.

We already know that this function executes initialization of architecture-specific stuff. In our case the `setup_arch` function does [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture related initializations. The `setup_arch` is big function, and in the previous part we stopped on the setting of the two exceptions handlers for the two following exceptions:

* `#DB` - debug exception, transfers control from the interrupted process to the debug handler;
* `#BP` - breakpoint exception, caused by the `int 3` instruction.

These exceptions allow the `x86_64` architecture to have early exception processing for the purpose of debugging via the [kgdb](https://en.wikipedia.org/wiki/KGDB).

As you can remember we set these exceptions handlers in the `early_trap_init` function:

```C
void __init early_trap_init(void)
{
    set_intr_gate_ist(X86_TRAP_DB, &debug, DEBUG_STACK);
    set_system_intr_gate_ist(X86_TRAP_BP, &int3, DEBUG_STACK);
    load_idt(&idt_descr);
}
```

from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). We already saw implementation of the `set_intr_gate_ist` and `set_system_intr_gate_ist` functions in the previous part and now we will look on the implementation of these two exceptions handlers.

在5.10.13中流程为：
```c
	idt_setup_early_traps();/* 中断描述符表 */
```
上面函数实现很简单：
```c
void __init idt_setup_early_traps(void)/* 中断描述符表 */
{
	idt_setup_from_table(idt_table, early_idts, ARRAY_SIZE(early_idts),
			     true);
    //调用 `load_idt` 函数来执行 `ldtr` 指令来重新加载 `IDT` 表
	load_idt(&idt_descr);/* 中断描述符 */
}
```
`early_idts`定义为：
```c
/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_idts[] = {/* 中断描述符表 陷阱 trap */
	INTG(X86_TRAP_DB,		asm_exc_debug),
	SYSG(X86_TRAP_BP,		asm_exc_int3),

#ifdef CONFIG_X86_32
	/*
	 * Not possible on 64-bit. See idt_setup_early_pf() for details.
	 */
//	INTG(X86_TRAP_PF,		asm_exc_page_fault),
#endif
};
```
可见只设置了`#DB`和`#BP`。


## 2.1. Debug and Breakpoint exceptions

Ok, we setup exception handlers in the `early_trap_init` function for the `#DB` and `#BP` exceptions and now time is to consider their implementations. But before we will do this, first of all let's look on details of these exceptions.

The first exceptions - `#DB` or `debug` exception occurs when a debug event occurs. For example - attempt to change the contents of a [debug register](http://en.wikipedia.org/wiki/X86_debug_register). Debug registers are special registers that were presented in `x86` processors starting from the [Intel 80386](http://en.wikipedia.org/wiki/Intel_80386) processor and as you can understand from name of this CPU extension, main purpose of these registers is debugging.

These registers allow to set breakpoints on the code and read or write data to trace it. Debug registers may be accessed only in the privileged mode and an attempt to read or write the debug registers when executing at any other privilege level causes a [general protection fault](https://en.wikipedia.org/wiki/General_protection_fault) exception. That's why we have used `set_intr_gate_ist` for the `#DB` exception, but not the `set_system_intr_gate_ist`.

The verctor number of the `#DB` exceptions is `1` (we pass it as `X86_TRAP_DB`) and as we may read in specification, this exception has no error code:

```
+-----------------------------------------------------+
|Vector|Mnemonic|Description         |Type |Error Code|
+-----------------------------------------------------+
|1     | #DB    |Reserved            |F/T  |NO        |
+-----------------------------------------------------+
```

The second exception is `#BP` or `breakpoint` exception occurs when processor executes the [int 3](http://en.wikipedia.org/wiki/INT_%28x86_instruction%29#INT_3) instruction. Unlike the `DB` exception, the `#BP` exception may occur in userspace. We can add it anywhere in our code, for example let's look on the simple program:

```C
// breakpoint.c
#include <stdio.h>

int main() {
    int i;
    while (i < 6){
	    printf("i equal to: %d\n", i);
	    __asm__("int3");
		++i;
    }
}
```

If we will compile and run this program, we will see following output:

```
$ gcc breakpoint.c -o breakpoint
i equal to: 0
Trace/breakpoint trap
```

But if will run it with gdb, we will see our breakpoint and can continue execution of our program:

```
$ gdb breakpoint
...
...
...
(gdb) run
Starting program: /home/alex/breakpoints 
i equal to: 0

Program received signal SIGTRAP, Trace/breakpoint trap.
0x0000000000400585 in main ()
=> 0x0000000000400585 <main+31>:	83 45 fc 01	add    DWORD PTR [rbp-0x4],0x1
(gdb) c
Continuing.
i equal to: 1

Program received signal SIGTRAP, Trace/breakpoint trap.
0x0000000000400585 in main ()
=> 0x0000000000400585 <main+31>:	83 45 fc 01	add    DWORD PTR [rbp-0x4],0x1
(gdb) c
Continuing.
i equal to: 2

Program received signal SIGTRAP, Trace/breakpoint trap.
0x0000000000400585 in main ()
=> 0x0000000000400585 <main+31>:	83 45 fc 01	add    DWORD PTR [rbp-0x4],0x1
...
...
...
```
奇怪，我再main中只能得到一个sigtrap，如下：
```c
(gdb) r
i equal to: 0

Program received signal SIGTRAP, Trace/breakpoint trap.
0x0000000000401151 in main ()
(gdb) n
Single stepping until exit from function main,
which has no line number information.
i equal to: 1
i equal to: 2
i equal to: 3
i equal to: 4
i equal to: 5
0x00007ffff7a2f505 in __libc_start_main () from /usr/lib64/libc.so.6
(gdb) q
A debugging session is active.

	Inferior 1 [process 154513] will be killed.

Quit anyway? (y or n) y
```

From this moment we know a little about these two exceptions and we can move on to consideration of their handlers.

## 2.2. Preparation before an exception handler

As you may note before, the `set_intr_gate_ist` and `set_system_intr_gate_ist` functions takes an addresses of exceptions handlers in theirs second parameter. In or case our two exception handlers will be:

* `debug`;
* `int3`.

You will not find these functions in the C code. all of that could be found in the kernel's `*.c/*.h` files only definition of these functions which are located in the [arch/x86/include/asm/traps.h](https://github.com/torvalds/linux/tree/master/arch/x86/include/asm/traps.h) kernel header file:

```C
asmlinkage void debug(void);
```

and

```C
asmlinkage void int3(void);
```

You may note `asmlinkage` directive in definitions of these functions. The directive is the special specificator of the [gcc](http://en.wikipedia.org/wiki/GNU_Compiler_Collection). Actually for a `C` functions which are called from assembly, we need in explicit declaration of the function calling convention. In our case, if function made with `asmlinkage` descriptor, then `gcc` will compile the function to retrieve parameters from stack.

So, both handlers are defined in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly source code file with the `idtentry` macro:

```assembly
idtentry debug do_debug has_error_code=0 paranoid=1 shift_ist=DEBUG_STACK
```

and

```assembly
idtentry int3 do_int3 has_error_code=0 paranoid=1 shift_ist=DEBUG_STACK
```

Each exception handler may be consists from two parts. 

* The first part is generic part and it is the same for all exception handlers. An exception handler should to save  [general purpose registers](https://en.wikipedia.org/wiki/Processor_register) on the stack, switch to kernel stack if an exception came from userspace and transfer control to the second part of an exception handler. 
* The second part of an exception handler does certain work depends on certain exception. For example page fault exception handler should find virtual page for given address, invalid opcode exception handler should send `SIGILL` [signal](https://en.wikipedia.org/wiki/Unix_signal) and etc.

As we just saw, an exception handler starts from definition of the `idtentry` macro from the [arch/x86/kernel/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/entry_64.S) assembly source code file, so let's look at implementation of this macro. As we may see, the `idtentry` macro takes five arguments:

* `sym` - defines global symbol with the `.globl name` which will be an an entry of exception handler;
* `do_sym` - symbol name which represents a secondary entry of an exception handler;
* `has_error_code` - information about existence of an error code of exception.

The last two parameters are optional:

* `paranoid` - shows us how we need to check current mode (will see explanation in details later);
* `shift_ist` - shows us is an exception running at `Interrupt Stack Table`.

Definition of the `.idtentry` macro looks:

```assembly
.macro idtentry sym do_sym has_error_code:req paranoid=0 shift_ist=-1
ENTRY(\sym)
...
...
...
END(\sym)
.endm
```
这里我们需要着重关注：
```c
/*
 * Interrupt entry/exit.
 *
 + The interrupt stubs push (vector) onto the stack, which is the error_code
 * position of idtentry exceptions, and jump to one of the two idtentry points
 * (common/spurious).
 *
 * common_interrupt is a hotpath, align it to a cache line
 */
.macro idtentry_irq vector cfunc
	.p2align CONFIG_X86_L1_CACHE_SHIFT
	idtentry \vector asm_\cfunc \cfunc has_error_code=1
.endm
```
就可以知道前缀`asm_`从哪里来。

Before we will consider internals of the `idtentry` macro, **we should to know state of stack when an exception occurs.** As we may read in the [Intel® 64 and IA-32 Architectures Software Developer’s Manual 3A](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html), the state of stack when an exception occurs is following:

```
    +------------+
+40 | %SS        |
+32 | %RSP       |
+24 | %RFLAGS    |
+16 | %CS        |
 +8 | %RIP       |
  0 | ERROR CODE | <-- %RSP
    +------------+
```

Now we may start to consider implementation of the `idtmacro`. Both `#DB` and `BP` exception handlers are defined as:

```assembly
idtentry debug do_debug has_error_code=0 paranoid=1 shift_ist=DEBUG_STACK
idtentry int3 do_int3 has_error_code=0 paranoid=1 shift_ist=DEBUG_STACK
```

If we will look at these definitions, we may know that compiler will generate two routines with `debug` and `int3` names and both of these exception handlers will call `do_debug` and `do_int3` secondary handlers after some preparation. The **third parameter **defines existence of error code and as we may see both our exception do not have them. As we may see on the diagram above, processor pushes error code on stack if an exception provides it. In our case, the `debug` and `int3` exception do not have error codes. This may bring some difficulties because stack will look differently for exceptions which provides error code and for exceptions which not. That's why implementation of the `idtentry` macro starts from putting a fake error code to the stack if an exception does not provide it:

```assembly
.ifeq \has_error_code
    pushq	$-1
.endif
```

But it is not only fake error-code. Moreover the `-1` also represents invalid system call number, so that the system call restart logic will not be triggered.

> 有些栈是进程共享的，例如异常栈。**Interrupt Stack Table**

The last two parameters of the `idtentry` macro `shift_ist` and `paranoid` allow to know do an exception handler runned at stack from `Interrupt Stack Table` or not. You already may know that each kernel thread in the system has own stack. In addition to these stacks, **there are some specialized stacks associated with each processor in the system**. One of these stacks is - **exception stack**. The [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture provides special feature which is called - `Interrupt Stack Table`. This feature allows to switch to a new stack for designated events such as an atomic exceptions like `double fault` and etc. So the `shift_ist` parameter allows us to know do we need to switch on `IST` stack for an exception handler or not.

The second parameter - `paranoid` defines the method which helps us to know did we come from userspace or not to an exception handler. The easiest way to determine this is to via `CPL` or `Current Privilege Level` in `CS` segment register. If it is **equal to `3`, we came from userspace**, if zero we came from kernel space:

```
testl $3,CS(%rsp)
jnz userspace
...
...
...
// we are from the kernel space
```

But unfortunately this method does not give a 100% guarantee. As described in the kernel documentation:

> if we are in an NMI/MCE/DEBUG/whatever super-atomic entry context,
> which might have triggered right after a normal entry wrote CS to the
> stack but before we executed SWAPGS, then the only safe way to check
> for GS is the slower method: the RDMSR.

In other words for example `NMI` could happen inside the critical section of a [swapgs](http://www.felixcloutier.com/x86/SWAPGS.html) instruction. In this way we should check value of the `MSR_GS_BASE` [model specific register](https://en.wikipedia.org/wiki/Model-specific_register) which stores pointer to the start of per-cpu area. So to check did we come from userspace or not, we should to check value of the `MSR_GS_BASE` model specific register and if it is negative we came from kernel space, in other way we came from userspace:

```assembly
movl $MSR_GS_BASE,%ecx
rdmsr
testl %edx,%edx
js 1f
```

In first two lines of code we read value of the `MSR_GS_BASE` model specific register into `edx:eax` pair. We can't set negative value to the `gs` from userspace. But from other side we know that direct mapping of the physical memory starts from the `0xffff880000000000` virtual address. In this way, `MSR_GS_BASE` will contain an address from `0xffff880000000000` to `0xffffc7ffffffffff`. After the `rdmsr` instruction will be executed, the smallest possible value in the `%edx` register will be - `0xffff8800` which is `-30720` in unsigned 4 bytes. That's why kernel space `gs` which points to start of `per-cpu` area will contain negative value.

After we pushed fake error code on the stack, we should allocate space for general purpose registers with:

```assembly
ALLOC_PT_GPREGS_ON_STACK
```

macro which is defined in the [arch/x86/entry/calling.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/calling.h) header file. This macro just allocates 15*8 bytes space on the stack to preserve general purpose registers:

```assembly
.macro ALLOC_PT_GPREGS_ON_STACK addskip=0
    addq	$-(15*8+\addskip), %rsp
.endm
```

So the stack will look like this after execution of the `ALLOC_PT_GPREGS_ON_STACK`:

```
     +------------+
+160 | %SS        |
+152 | %RSP       |
+144 | %RFLAGS    |
+136 | %CS        |
+128 | %RIP       |
+120 | ERROR CODE |
     |------------|
+112 |            |
+104 |            |
 +96 |            |
 +88 |            |
 +80 |            |
 +72 |            |
 +64 |            |
 +56 |            |
 +48 |            |
 +40 |            |
 +32 |            |
 +24 |            |
 +16 |            |
  +8 |            |
  +0 |            | <- %RSP
     +------------+
```

After we allocated space for general purpose registers, we do some checks to understand did an exception come from userspace or not and if yes, we should move back to an interrupted process stack or stay on exception stack:

```assembly
.if \paranoid
    .if \paranoid == 1
	    testb	$3, CS(%rsp)
	    jnz	1f
	.endif
	call	paranoid_entry
.else
	call	error_entry
.endif
```

Let's consider all of these there cases in course.

```asm
/*
 * Save all registers in pt_regs. Return GSBASE related information
 * in EBX depending on the availability of the FSGSBASE instructions:
 *
 * FSGSBASE	R/EBX
 *     N        0 -> SWAPGS on exit
 *              1 -> no SWAPGS on exit
 *
 *     Y        GSBASE value at entry, must be restored in paranoid_exit
 */
SYM_CODE_START_LOCAL(paranoid_entry)
	UNWIND_HINT_FUNC
	cld
	PUSH_AND_CLEAR_REGS save_ret=1
	ENCODE_FRAME_POINTER 8

	/*
	 * Always stash CR3 in %r14.  This value will be restored,
	 * verbatim, at exit.  Needed if paranoid_entry interrupted
	 * another entry that already switched to the user CR3 value
	 * but has not yet returned to userspace.
	 *
	 * This is also why CS (stashed in the "iret frame" by the
	 * hardware at entry) can not be used: this may be a return
	 * to kernel code, but with a user CR3 value.
	 *
	 * Switching CR3 does not depend on kernel GSBASE so it can
	 * be done before switching to the kernel GSBASE. This is
	 * required for FSGSBASE because the kernel GSBASE has to
	 * be retrieved from a kernel internal table.
	 */
	SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg=%rax save_reg=%r14

	/*
	 * Handling GSBASE depends on the availability of FSGSBASE.
	 *
	 * Without FSGSBASE the kernel enforces that negative GSBASE
	 * values indicate kernel GSBASE. With FSGSBASE no assumptions
	 * can be made about the GSBASE value when entering from user
	 * space.
	 */
	ALTERNATIVE "jmp .Lparanoid_entry_checkgs", "", X86_FEATURE_FSGSBASE

	/*
	 * Read the current GSBASE and store it in %rbx unconditionally,
	 * retrieve and set the current CPUs kernel GSBASE. The stored value
	 * has to be restored in paranoid_exit unconditionally.
	 *
	 * The unconditional write to GS base below ensures that no subsequent
	 * loads based on a mispredicted GS base can happen, therefore no LFENCE
	 * is needed here.
	 */
	SAVE_AND_SET_GSBASE scratch_reg=%rax save_reg=%rbx
	ret

.Lparanoid_entry_checkgs:
	/* EBX = 1 -> kernel GSBASE active, no restore required */
	movl	$1, %ebx
	/*
	 * The kernel-enforced convention is a negative GSBASE indicates
	 * a kernel value. No SWAPGS needed on entry and exit.
	 */
	movl	$MSR_GS_BASE, %ecx
	rdmsr
	testl	%edx, %edx
	jns	.Lparanoid_entry_swapgs
	ret

.Lparanoid_entry_swapgs:
	SWAPGS

	/*
	 * The above SAVE_AND_SWITCH_TO_KERNEL_CR3 macro doesn't do an
	 * unconditional CR3 write, even in the PTI case.  So do an lfence
	 * to prevent GS speculation, regardless of whether PTI is enabled.
	 */
	FENCE_SWAPGS_KERNEL_ENTRY

	/* EBX = 0 -> SWAPGS required on exit */
	xorl	%ebx, %ebx
	ret
SYM_CODE_END(paranoid_entry)
```

## 2.3. An exception occured in userspace

In the first let's consider a case when an exception has `paranoid=1` like our `debug` and `int3` exceptions. In this case we check selector from `CS` segment register and jump at `1f` label if we came from userspace or the `paranoid_entry` will be called in other way.

Let's consider first case when we came from userspace to an exception handler. As described above we should jump at `1` label. The `1` label starts from the call of the

```assembly
call	error_entry
```
关于error_entry的注释为：
```c
/*
 * Save all registers in pt_regs, and switch GS if needed.
 */
SYM_CODE_START_LOCAL(error_entry)
```
routine which saves all general purpose registers in the previously allocated area on the stack:

```assembly
SAVE_C_REGS 8
SAVE_EXTRA_REGS 8
```

These both macros are defined in the  [arch/x86/entry/calling.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/calling.h) header file and just move values of general purpose registers to a certain place at the stack, for example:

```assembly
.macro SAVE_EXTRA_REGS offset=0
	movq %r15, 0*8+\offset(%rsp)
	movq %r14, 1*8+\offset(%rsp)
	movq %r13, 2*8+\offset(%rsp)
	movq %r12, 3*8+\offset(%rsp)
	movq %rbp, 4*8+\offset(%rsp)
	movq %rbx, 5*8+\offset(%rsp)
.endm
```

After execution of `SAVE_C_REGS` and `SAVE_EXTRA_REGS` the stack will look:

```
     +------------+
+160 | %SS        |
+152 | %RSP       |
+144 | %RFLAGS    |
+136 | %CS        |
+128 | %RIP       |
+120 | ERROR CODE |
     |------------|
+112 | %RDI       |
+104 | %RSI       |
 +96 | %RDX       |
 +88 | %RCX       |
 +80 | %RAX       |
 +72 | %R8        |
 +64 | %R9        |
 +56 | %R10       |
 +48 | %R11       |
 +40 | %RBX       |
 +32 | %RBP       |
 +24 | %R12       |
 +16 | %R13       |
  +8 | %R14       |
  +0 | %R15       | <- %RSP
     +------------+
```

After the kernel saved general purpose registers at the stack, we should check that we came from userspace space again with:

```assembly
testb	$3, CS+8(%rsp)
jz	.Lerror_kernelspace
```

because we may have potentially fault if as described in documentation truncated `%RIP` was reported. Anyway, in both cases the [SWAPGS](http://www.felixcloutier.com/x86/SWAPGS.html) instruction will be executed and values from `MSR_KERNEL_GS_BASE` and `MSR_GS_BASE` will be swapped. From this moment the `%gs` register will point to the base address of kernel structures. So, the `SWAPGS` instruction is called and it was main point of the `error_entry` routing.

Now we can back to the `idtentry` macro. We may see following assembler code after the call of `error_entry`:

```assembly
movq	%rsp, %rdi
call	sync_regs
```

Here we put base address of stack pointer `%rdi` register which will be first argument (according to [x86_64 ABI](https://www.uclibc.org/docs/psABI-x86_64.pdf)) of the `sync_regs` function and call this function which is defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) source code file:

```C
/*
 * Help handler running on a per-cpu (IST or entry trampoline) stack
 * to switch to the normal thread stack if the interrupted code was in
 * user mode. The actual stack switch is done in entry_64.S
 */
asmlinkage __visible noinstr struct pt_regs *sync_regs(struct pt_regs *eregs)
{
	struct pt_regs *regs = (struct pt_regs *)this_cpu_read(cpu_current_top_of_stack) - 1;
	if (regs != eregs)
		*regs = *eregs;
	return regs;
}
```

This function takes the result of the `task_ptr_regs` macro which is defined in the [arch/x86/include/asm/processor.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/processor.h) header file, stores it in the stack pointer and return it. The `task_ptr_regs` macro expands to the address of `thread.sp0` which represents pointer to the normal kernel stack:

```C
#define task_pt_regs(tsk)       ((struct pt_regs *)(tsk)->thread.sp0 - 1)
```

As we came from userspace, this means that exception handler will run in real process context. After we got stack pointer from the `sync_regs` we switch stack:

```assembly
movq	%rax, %rsp
```

```c
/**
 * idtentry_body - Macro to emit code calling the C function
 * @cfunc:		C function to be called
 * @has_error_code:	Hardware pushed error code on stack
 */
.macro idtentry_body cfunc has_error_code:req

	call	error_entry
	UNWIND_HINT_REGS

	movq	%rsp, %rdi			/* pt_regs pointer into 1st argument*/

	.if \has_error_code == 1
		movq	ORIG_RAX(%rsp), %rsi	/* get error code into 2nd argument*/
		movq	$-1, ORIG_RAX(%rsp)	/* no syscall to restart */
	.endif

	call	\cfunc

	jmp	error_return
.endm
```
The last two steps before an exception handler will call secondary handler are:

1. Passing pointer to `pt_regs` structure which contains preserved general purpose registers to the `%rdi` register:

```assembly
movq	%rsp, %rdi
```

as it will be passed as first parameter of secondary exception handler.

2. Pass error code to the `%rsi` register as it will be second argument of an exception handler and set it to `-1` on the stack for the same purpose as we did it before - to prevent restart of a system call:

```
.if \has_error_code
	movq	ORIG_RAX(%rsp), %rsi
	movq	$-1, ORIG_RAX(%rsp)
.else
	xorl	%esi, %esi
.endif
```

Additionally you may see that we zeroed the `%esi` register above in a case if an exception does not provide error code. 

In the end we just call secondary exception handler:

```assembly
call	\do_sym
```
5.10.13是：
```asm
	call	\cfunc
```
which:

```C
dotraplinkage void do_debug(struct pt_regs *regs, long error_code);
```

will be for `debug` exception and:

```C
dotraplinkage void notrace do_int3(struct pt_regs *regs, long error_code);
```

will be for `int 3` exception. In this part we will not see implementations of secondary handlers, because of they are very specific, but will see some of them in one of next parts.

We just considered first case when an exception occurred in userspace. Let's consider last two.


## 2.4. An exception with paranoid > 0 occurred in kernelspace

> paranoid: 多疑的; 恐惧的; 患偏执症的; 有妄想狂的；

In this case an exception was occurred in kernelspace and `idtentry` macro is defined with `paranoid=1` for this exception. This **value of `paranoid` means that we should use slower way that we saw in the beginning of this part to check do we really came from kernelspace** or not. The `paranoid_entry` routing allows us to know this:

```assembly
ENTRY(paranoid_entry)
	cld
	SAVE_C_REGS 8
	SAVE_EXTRA_REGS 8
	movl	$1, %ebx
	movl	$MSR_GS_BASE, %ecx
	rdmsr
	testl	%edx, %edx
	js	1f
	SWAPGS
	xorl	%ebx, %ebx
1:	ret
END(paranoid_entry)
```

As you may see, this function represents the same that we covered before. We use second (slow) method to get information about previous state of an interrupted task. As we checked this and executed `SWAPGS` in a case if we came from userspace, we should to do the same that we did before: We need to put pointer to a structure which holds general purpose registers to the `%rdi` (which will be first parameter of a secondary handler) and put error code if an exception provides it to the `%rsi` (which will be second parameter of a secondary handler):

```assembly
movq	%rsp, %rdi

.if \has_error_code
	movq	ORIG_RAX(%rsp), %rsi
	movq	$-1, ORIG_RAX(%rsp)
.else
	xorl	%esi, %esi
.endif
```

The last step before a secondary handler of an exception will be called is cleanup of new `IST` stack fram:

```assembly
.if \shift_ist != -1
	subq	$EXCEPTION_STKSZ, CPU_TSS_IST(\shift_ist)
.endif
```

You may remember that we passed the `shift_ist` as argument of the `idtentry` macro. Here we check its value and if its not equal to `-1`, we get pointer to a stack from `Interrupt Stack Table` by `shift_ist` index and setup it.

In the end of this second way we just call secondary exception handler as we did it before:

```assembly
call	\do_sym
```

The last method is similar to previous both, but an exception occured with `paranoid=0` and we may use fast method determination of where we are from.

## 2.5. Exit from an exception handler

After secondary handler will finish its works, we will return to the `idtentry` macro and the next step will be jump to the `error_exit`:

```assembly
jmp	error_exit
```

routine. The `error_exit` function defined in the same [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly source code file and the main goal of this function is to know where we are from (from userspace or kernelspace) and execute `SWPAGS` depends on this. Restore registers to previous state and execute `iret` instruction to transfer control to an interrupted task.
```c
/*
 * "Paranoid" exit path from exception stack.  This is invoked
 * only on return from non-NMI IST interrupts that came
 * from kernel space.
 *
 * We may be returning to very strange contexts (e.g. very early
 * in syscall entry), so checking for preemption here would
 * be complicated.  Fortunately, there's no good reason to try
 * to handle preemption here.
 *
 * R/EBX contains the GSBASE related information depending on the
 * availability of the FSGSBASE instructions:
 *
 * FSGSBASE	R/EBX
 *     N        0 -> SWAPGS on exit
 *              1 -> no SWAPGS on exit
 *
 *     Y        User space GSBASE, must be restored unconditionally
 */
SYM_CODE_START_LOCAL(paranoid_exit)
	UNWIND_HINT_REGS
	/*
	 * The order of operations is important. RESTORE_CR3 requires
	 * kernel GSBASE.
	 *
	 * NB to anyone to try to optimize this code: this code does
	 * not execute at all for exceptions from user mode. Those
	 * exceptions go through error_exit instead.
	 */
	RESTORE_CR3	scratch_reg=%rax save_reg=%r14

	/* Handle the three GSBASE cases */
	ALTERNATIVE "jmp .Lparanoid_exit_checkgs", "", X86_FEATURE_FSGSBASE

	/* With FSGSBASE enabled, unconditionally restore GSBASE */
	wrgsbase	%rbx
	jmp		restore_regs_and_return_to_kernel

.Lparanoid_exit_checkgs:
	/* On non-FSGSBASE systems, conditionally do SWAPGS */
	testl		%ebx, %ebx
	jnz		restore_regs_and_return_to_kernel

	/* We are returning to a context with user GSBASE */
	SWAPGS_UNSAFE_STACK
	jmp		restore_regs_and_return_to_kernel
SYM_CODE_END(paranoid_exit)
```
That's all.

## 2.6. Conclusion

It is the end of the third part about interrupts and interrupt handling in the Linux kernel. We saw the initialization of the [Interrupt descriptor table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table) in the previous part with the `#DB` and `#BP` gates and started to dive into preparation before control will be transferred to an exception handler and implementation of some interrupt handlers in this part. In the next part we will continue to dive into this theme and will go next by the `setup_arch` function and will try to understand interrupts handling related stuff.

```c
/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_idts[] = {/* 中断描述符表 陷阱 trap */
	INTG(X86_TRAP_DB,		asm_exc_debug),
	SYSG(X86_TRAP_BP,		asm_exc_int3),

#ifdef CONFIG_X86_32
	/*
	 * Not possible on 64-bit. See idt_setup_early_pf() for details.
	 */
//	INTG(X86_TRAP_PF,		asm_exc_page_fault),
#endif
};
```

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 2.7. Links

* [Debug registers](http://en.wikipedia.org/wiki/X86_debug_register)
* [Intel 80385](http://en.wikipedia.org/wiki/Intel_80386)
* [INT 3](http://en.wikipedia.org/wiki/INT_%28x86_instruction%29#INT_3)
* [gcc](http://en.wikipedia.org/wiki/GNU_Compiler_Collection)
* [TSS](http://en.wikipedia.org/wiki/Task_state_segment)
* [GNU assembly .error directive](https://sourceware.org/binutils/docs/as/Error.html#Error)
* [dwarf2](http://en.wikipedia.org/wiki/DWARF)
* [CFI directives](https://sourceware.org/binutils/docs/as/CFI-directives.html)
* [IRQ](http://en.wikipedia.org/wiki/Interrupt_request_%28PC_architecture%29)
* [system call](http://en.wikipedia.org/wiki/System_call)
* [swapgs](http://www.felixcloutier.com/x86/SWAPGS.html)
* [SIGTRAP](https://en.wikipedia.org/wiki/Unix_signal#SIGTRAP)
* [Per-CPU variables](http://0xax.gitbooks.io/linux-insides/content/Concepts/per-cpu.html)
* [kgdb](https://en.wikipedia.org/wiki/KGDB)
* [ACPI](https://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface)
* [Previous part](http://0xax.gitbooks.io/linux-insides/content/interrupts/index.html)



# 3. Initialization of non-early interrupt gates

This is fourth part about an interrupts and exceptions handling in the Linux kernel and in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3) we saw first early `#DB` and `#BP` exceptions handlers from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). We stopped on the right after the `early_trap_init` function that called in the `setup_arch` function which defined in the [arch/x86/kernel/setup.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/setup.c). In this part we will continue to dive into an interrupts and exceptions handling in the Linux kernel for `x86_64` and continue to do it from the place where we left off in the last part. First thing which is related to the interrupts and exceptions handling is the setup of the `#PF` or [page fault](https://en.wikipedia.org/wiki/Page_fault) handler with the `early_trap_pf_init` function. Let's start from it.

## 3.1. Early page fault handler

The `early_trap_pf_init` function defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). It uses `set_intr_gate` macro that fills [Interrupt Descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table) with the given entry:

```C
void __init early_trap_pf_init(void)
{
#ifdef CONFIG_X86_64
         set_intr_gate(X86_TRAP_PF, page_fault);
#endif
}
```

This macro defined in the [arch/x86/include/asm/desc.h](https://github.com/torvalds/linux/tree/master/arch/x86/include/asm/desc.h). We already saw macros like this in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3) - `set_system_intr_gate` and `set_intr_gate_ist`. This macro checks that given vector number is not greater than `255` (maximum vector number) and calls `_set_gate` function as `set_system_intr_gate` and `set_intr_gate_ist` did it:

```C
#define set_intr_gate(n, addr)                                  \
do {                                                            \
        BUG_ON((unsigned)n > 0xFF);                             \
        _set_gate(n, GATE_INTERRUPT, (void *)addr, 0, 0,        \
                  __KERNEL_CS);                                 \
        _trace_set_gate(n, GATE_INTERRUPT, (void *)trace_##addr,\
                        0, 0, __KERNEL_CS);                     \
} while (0)
```

The `set_intr_gate` macro takes two parameters:

* vector number of a interrupt;
* address of an interrupt handler;

In our case they are:

* `X86_TRAP_PF` - `14`;
* `page_fault` - the interrupt handler entry point.

5.10.13中不是这么做的：
```c
/**
 * idt_setup_early_pf - Initialize the idt table with early pagefault handler
 *
 * On X8664 this does not use interrupt stacks as they can't work before
 * cpu_init() is invoked and sets up TSS. The IST variant is installed
 * after that.
 *
 * Note, that X86_64 cannot install the real #PF handler in
 * idt_setup_early_traps() because the memory intialization needs the #PF
 * handler from the early_idt_handler_array to initialize the early page
 * tables.
 *
 * 用于建立 `#PF` 处理函数
 */
void __init idt_setup_early_pf(void)    /* page fault */
{
	idt_setup_from_table(idt_table, early_pf_idts,
			     ARRAY_SIZE(early_pf_idts), true);
}
```
其中`early_pf_idts`为：

```c
/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_pf_idts[] = {
	INTG(X86_TRAP_PF,		asm_exc_page_fault),    /* Page Fault */
};
```
`idt_setup_early_pf`在setup_arch中被调用。

The `X86_TRAP_PF` is the element of enum which defined in the [arch/x86/include/asm/traprs.h](https://github.com/torvalds/linux/tree/master/arch/x86/include/asm/traprs.h):

```C
enum {
	...
	...
	...
	...
	X86_TRAP_PF,            /* 14, Page Fault */
	...
	...
	...
}
```

When the `early_trap_pf_init` will be called, the `set_intr_gate` will be expanded to the call of the `_set_gate` which will fill the `IDT` with the handler for the page fault. Now let's look on the implementation of the `page_fault` handler. The `page_fault` handler defined in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/master/arch/x86/entry/entry_64.S) assembly source code file as all exceptions handlers. Let's look on it:

```assembly
trace_idtentry page_fault do_page_fault has_error_code=1
```
5.10.13中的内核路径则不一样：
```c
DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)
{
    ...
	handle_page_fault(regs, error_code, address);
    ...
}
```

We saw in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3) how `#DB` and `#BP` handlers defined. They were defined with the `idtentry` macro, but here we can see `trace_idtentry`. This macro defined in the same source code file and depends on the `CONFIG_TRACING` kernel configuration option:

```assembly
#ifdef CONFIG_TRACING
.macro trace_idtentry sym do_sym has_error_code:req
idtentry trace(\sym) trace(\do_sym) has_error_code=\has_error_code
idtentry \sym \do_sym has_error_code=\has_error_code
.endm
#else
.macro trace_idtentry sym do_sym has_error_code:req
idtentry \sym \do_sym has_error_code=\has_error_code
.endm
#endif
```

We will not dive into exceptions [Tracing](https://en.wikipedia.org/wiki/Tracing_%28software%29) now. If `CONFIG_TRACING` is not set, we can see that `trace_idtentry` macro just expands to the normal `idtentry`. We already saw implementation of the `idtentry` macro in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3), so let's start from the `page_fault` exception handler.

As we can see in the `idtentry` definition, the handler of the `page_fault` is `do_page_fault` function which defined in the [arch/x86/mm/fault.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/mm/fault.c) and as all exceptions handlers it takes two arguments:

* `regs` - `pt_regs` structure that holds state of an interrupted process;
* `error_code` - error code of the page fault exception.

Let's look inside this function. First of all we read content of the [cr2](https://en.wikipedia.org/wiki/Control_register) control register:

```C
dotraplinkage void notrace
do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
	unsigned long address = read_cr2();
	...
	...
	...
}
```

This register contains a linear address which caused `page fault`. In the next step we make a call of the `exception_enter` function from the [include/linux/context_tracking.h](https://github.com/torvalds/linux/blob/master/include/linux/context_tracking.h). The `exception_enter` and `exception_exit` are functions from context tracking subsystem in the Linux kernel used by the [RCU](https://en.wikipedia.org/wiki/Read-copy-update) to remove its dependency on the timer tick while a processor runs in userspace. Almost in the every exception handler we will see similar code:

```C
enum ctx_state prev_state;
prev_state = exception_enter();
...
... // exception handler here
...
exception_exit(prev_state);
```

The `exception_enter` function checks that `context tracking` is enabled with the `context_tracking_is_enabled` and if it is in enabled state, we get previous context with the `this_cpu_read` (more about `this_cpu_*` operations you can read in the [Documentation](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/this_cpu_ops.txt)). After this it calls `context_tracking_user_exit` function which informs the context tracking that the processor is exiting userspace mode and entering the kernel:

```C
static inline enum ctx_state exception_enter(void)
{
        enum ctx_state prev_ctx;

        if (!context_tracking_is_enabled())
                return 0;

        prev_ctx = this_cpu_read(context_tracking.state);
        context_tracking_user_exit();

        return prev_ctx;
}
```

The state can be one of the:

```C
enum ctx_state {
    IN_KERNEL = 0,
	IN_USER,
} state;
```

And in the end we return previous context. Between the `exception_enter` and `exception_exit` we call actual page fault handler:

```C
__do_page_fault(regs, error_code, address);
```

The `__do_page_fault` is defined in the same source code file as `do_page_fault` - [arch/x86/mm/fault.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/mm/fault.c). In the beginning of the `__do_page_fault` we check state of the [kmemcheck](https://www.kernel.org/doc/Documentation/kmemcheck.txt) checker. The `kmemcheck` detects warns about some uses of uninitialized memory. We need to check it because page fault can be caused by kmemcheck:

```C
if (kmemcheck_active(regs))
		kmemcheck_hide(regs);
	prefetchw(&mm->mmap_sem);
```

After this we can see the call of the `prefetchw` which executes instruction with the same [name](http://www.felixcloutier.com/x86/PREFETCHW.html) which fetches [X86_FEATURE_3DNOW](https://en.wikipedia.org/?title=3DNow!) to get exclusive [cache line](https://en.wikipedia.org/wiki/CPU_cache). The main purpose of prefetching is to hide the latency of a memory access. In the next step we check that we got page fault not in the kernel space with the following condition:

```C
if (unlikely(fault_in_kernel_space(address))) {
...
...
...
}
```
在5.10.13中：
```c
static __always_inline void
handle_page_fault(struct pt_regs *regs, unsigned long error_code,
			      unsigned long address)
{
	trace_page_fault_entries(regs, error_code, address);

	if (unlikely(kmmio_fault(regs, address)))
		return;

	/* Was the fault on kernel-controlled part of the address space? */
	if (unlikely(fault_in_kernel_space(address))) {
		do_kern_addr_fault(regs, error_code, address);
	} else {
		do_user_addr_fault(regs, error_code, address);
		/*
		 * User address page fault handling might have reenabled
		 * interrupts. Fixing up all potential exit points of
		 * do_user_addr_fault() and its leaf functions is just not
		 * doable w/o creating an unholy mess or turning the code
		 * upside down.
		 */
		local_irq_disable();
	}
}
```

where `fault_in_kernel_space` is:

```C
static int fault_in_kernel_space(unsigned long address)
{
        return address >= TASK_SIZE_MAX;
}
```
在5.10.13中：
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

	return address >= TASK_SIZE_MAX;
}
```
The `TASK_SIZE_MAX` macro expands to the:

```C
#define TASK_SIZE_MAX   ((1UL << 47) - PAGE_SIZE)
```
详细点：
```c
#ifdef CONFIG_X86_5LEVEL
#define __VIRTUAL_MASK_SHIFT	(pgtable_l5_enabled() ? 56 : 47)
#else
#define __VIRTUAL_MASK_SHIFT	47
#endif

#define TASK_SIZE_MAX	((_AC(1,UL) << __VIRTUAL_MASK_SHIFT) - PAGE_SIZE)
```

or `0x00007ffffffff000`. Pay attention on `unlikely` macro. There are two macros in the Linux kernel:

```C
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
```

You can [often](http://lxr.free-electrons.com/ident?i=unlikely) find these macros in the code of the Linux kernel. Main purpose of these macros is optimization. Sometimes this situation is that we need to check the condition of the code and we know that it will rarely be `true` or `false`. With these macros we can tell to the compiler about this. For example 

```C
static int proc_root_readdir(struct file *file, struct dir_context *ctx)
{
        if (ctx->pos < FIRST_PROCESS_ENTRY) {
                int error = proc_readdir(file, ctx);
                if (unlikely(error <= 0))
                        return error;
...
...
...
}
```

Here we can see `proc_root_readdir` function which will be called when the Linux [VFS](https://en.wikipedia.org/wiki/Virtual_file_system) needs to read the `root` directory contents. If condition marked with `unlikely`, compiler can put `false` code right after branching. 

Now let's back to the our address check. 

Comparison between the given address and the `0x00007ffffffff000` will give us to know, was page fault in the kernel mode or user mode. After this check we know it. After this `__do_page_fault` routine will try to understand the problem that provoked page fault exception and then will pass address to the appropriate routine. It can be `kmemcheck` fault, spurious fault, [kprobes](https://www.kernel.org/doc/Documentation/kprobes.txt) fault and etc. Will not dive into implementation details of the page fault exception handler in this part, because we need to know many different concepts which are provided by the Linux kernel, but will see it in the chapter about the [memory management](https://0xax.gitbook.io/linux-insides/summary/mm) in the Linux kernel.

## 3.2. Back to start_kernel

There are many different function calls after the `early_trap_pf_init` in the `setup_arch` function from different kernel subsystems, but there are no one interrupts and exceptions handling related. So, we have to go back where we came from - `start_kernel` function from the [init/main.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/init/main.c#L492). 

The first things after the `setup_arch` is the `trap_init` function from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). This function makes initialization of the remaining exceptions handlers (remember that we already setup 3 handlers for the `#DB` - debug exception, `#BP` - breakpoint exception and `#PF` - page fault exception).

先给出5.10.13中的定义：
```c
void __init trap_init(void) /* 陷阱初始化 */
{
	/* Init cpu_entry_area before IST entries are set up */
	setup_cpu_entry_areas();    /*  */

	/* Init GHCB memory pages when running as an SEV-ES guest */
	sev_es_init_vc_handling();  /*  */

	idt_setup_traps();  /* 中断描述附表 */

	/*
	 * Should be a barrier for any external CPU state:
	 */
	cpu_init(); /*  */

	idt_setup_ist_traps();  /* 中断栈表 irq stack table */
}
```

其中`idt_setup_traps`为：
```c
/**
 * idt_setup_traps - Initialize the idt table with default traps
 */
void __init idt_setup_traps(void)   /*  中断描述符表*/
{
	idt_setup_from_table(idt_table, def_idts, ARRAY_SIZE(def_idts), true);
}
```
默认的中断描述符表`def_idts`如下所述：
```c
/*
 * The default IDT entries which are set up in trap_init() before
 * cpu_init() is invoked. Interrupt stacks cannot be used at that point and
 * the traps which use them are reinitialized with IST after cpu_init() has
 * set up TSS.
 */
static const __initconst struct idt_data def_idts[] = {/* 默认的 中断描述符表 */
	INTG(X86_TRAP_DE,		asm_exc_divide_error),
	INTG(X86_TRAP_NMI,		asm_exc_nmi),   //arch/x86/entry/entry_64.S
	INTG(X86_TRAP_BR,		asm_exc_bounds),
	INTG(X86_TRAP_UD,		asm_exc_invalid_op),
	INTG(X86_TRAP_NM,		asm_exc_device_not_available),
	INTG(X86_TRAP_OLD_MF,		asm_exc_coproc_segment_overrun),
	INTG(X86_TRAP_TS,		asm_exc_invalid_tss),
	INTG(X86_TRAP_NP,		asm_exc_segment_not_present),
	INTG(X86_TRAP_SS,		asm_exc_stack_segment),
	INTG(X86_TRAP_GP,		asm_exc_general_protection),
	INTG(X86_TRAP_SPURIOUS,		asm_exc_spurious_interrupt_bug),
	INTG(X86_TRAP_MF,		asm_exc_coprocessor_error),
	INTG(X86_TRAP_AC,		asm_exc_alignment_check),
	INTG(X86_TRAP_XF,		asm_exc_simd_coprocessor_error),

#ifdef CONFIG_X86_32
//	TSKG(X86_TRAP_DF,		GDT_ENTRY_DOUBLEFAULT_TSS),
#else
	INTG(X86_TRAP_DF,		asm_exc_double_fault),
#endif
	INTG(X86_TRAP_DB,		asm_exc_debug),

#ifdef CONFIG_X86_MCE
	INTG(X86_TRAP_MC,		asm_exc_machine_check),
#endif

	SYSG(X86_TRAP_OF,		asm_exc_overflow),
#if defined(CONFIG_IA32_EMULATION)
	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_compat),
#elif defined(CONFIG_X86_32)
//	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_32),
#endif
};
```


其中`idt_setup_ist_traps`为：
```c
/**
 * idt_setup_ist_traps - Initialize the idt table with traps using IST
 */
void __init idt_setup_ist_traps(void)/* IST(Interrupt Stack Table) */
{
	idt_setup_from_table(idt_table, ist_idts/*  */, ARRAY_SIZE(ist_idts), true);
}
```
这里就是将`ist_idts`装入`idt_table`中，`ist_idts`结构如下：
```c
/*
 * The exceptions which use Interrupt stacks. They are setup after
 * cpu_init() when the TSS has been initialized.
 */
static const __initconst struct idt_data ist_idts[] = { /* IST(Interrupt Stack Table) */
	ISTG(X86_TRAP_DB,	asm_exc_debug,			IST_INDEX_DB),
	ISTG(X86_TRAP_NMI,	asm_exc_nmi,			IST_INDEX_NMI),
	ISTG(X86_TRAP_DF,	asm_exc_double_fault,		IST_INDEX_DF),
#ifdef CONFIG_X86_MCE
	ISTG(X86_TRAP_MC,	asm_exc_machine_check,		IST_INDEX_MCE),
#endif
#ifdef CONFIG_AMD_MEM_ENCRYPT
	ISTG(X86_TRAP_VC,	asm_exc_vmm_communication,	IST_INDEX_VC),
#endif
};
```

The `trap_init` function starts from the check of the [Extended Industry Standard Architecture](https://en.wikipedia.org/wiki/Extended_Industry_Standard_Architecture):

```C
#ifdef CONFIG_EISA
    void __iomem *p = early_ioremap(0x0FFFD9, 4);

    if (readl(p) == 'E' + ('I'<<8) + ('S'<<16) + ('A'<<24))
            EISA_bus = 1;
    early_iounmap(p, 4);
#endif
```

Note that it depends on the `CONFIG_EISA` kernel configuration parameter which represents `EISA` support. Here we use `early_ioremap` function to map `I/O` memory on the page tables. We use `readl` function to read first `4` bytes from the mapped region and if they are equal to `EISA` string we set `EISA_bus` to one. In the end we just unmap previously mapped region. More about `early_ioremap` you can read in the part which describes [Fix-Mapped Addresses and ioremap](https://0xax.gitbook.io/linux-insides/summary/mm/linux-mm-2).

After this we start to fill the `Interrupt Descriptor Table` with the different interrupt gates. First of all we set `#DE` or `Divide Error` and `#NMI` or `Non-maskable Interrupt`:

```C
set_intr_gate(X86_TRAP_DE, divide_error);
set_intr_gate_ist(X86_TRAP_NMI, &nmi, NMI_STACK);
```

We use `set_intr_gate` macro to set the interrupt gate for the `#DE` exception and `set_intr_gate_ist` for the `#NMI`. You can remember that we already used these macros when we have set the interrupts gates for the page fault handler, debug handler and etc, you can find explanation of it in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3). After this we setup exception gates for the following exceptions:

```C
set_system_intr_gate(X86_TRAP_OF, &overflow);
set_intr_gate(X86_TRAP_BR, bounds);
set_intr_gate(X86_TRAP_UD, invalid_op);
set_intr_gate(X86_TRAP_NM, device_not_available);
```

Here we can see:

* `#OF` or `Overflow` exception. This exception indicates that an overflow trap occurred when an special [INTO](http://x86.renejeschke.de/html/file_module_x86_id_142.html) instruction was executed;
* `#BR` or `BOUND Range exceeded` exception. This exception indicates that a `BOUND-range-exceed` fault occurred when a [BOUND](http://pdos.csail.mit.edu/6.828/2005/readings/i386/BOUND.htm) instruction was executed;
* `#UD` or `Invalid Opcode` exception. Occurs when a processor attempted to execute invalid or reserved [opcode](https://en.wikipedia.org/?title=Opcode), processor attempted to execute instruction with invalid operand(s) and etc;
* `#NM` or `Device Not Available` exception. Occurs when the processor tries to execute `x87 FPU` floating point instruction while `EM` flag in the [control register](https://en.wikipedia.org/wiki/Control_register#CR0) `cr0` was set.

In the next step we set the interrupt gate for the `#DF` or `Double fault` exception:

```C
set_intr_gate_ist(X86_TRAP_DF, &double_fault, DOUBLEFAULT_STACK);
```

This exception occurs when processor detected a second exception while calling an exception handler for a prior exception. In usual way when the processor detects another exception while trying to call an exception handler, the two exceptions can be handled serially. If the processor cannot handle them serially, it signals the double-fault or `#DF` exception.

The following set of the interrupt gates is:

```C
set_intr_gate(X86_TRAP_OLD_MF, &coprocessor_segment_overrun);
set_intr_gate(X86_TRAP_TS, &invalid_TSS);
set_intr_gate(X86_TRAP_NP, &segment_not_present);
set_intr_gate_ist(X86_TRAP_SS, &stack_segment, STACKFAULT_STACK);
set_intr_gate(X86_TRAP_GP, &general_protection);
set_intr_gate(X86_TRAP_SPURIOUS, &spurious_interrupt_bug);
set_intr_gate(X86_TRAP_MF, &coprocessor_error);
set_intr_gate(X86_TRAP_AC, &alignment_check);
```

Here we can see setup for the following exception handlers:

* `#CSO` or `Coprocessor Segment Overrun` - this exception indicates that math [coprocessor](https://en.wikipedia.org/wiki/Coprocessor) of an old processor detected a page or segment violation. Modern processors do not generate this exception
* `#TS` or `Invalid TSS` exception - indicates that there was an error related to the [Task State Segment](https://en.wikipedia.org/wiki/Task_state_segment).
* `#NP` or `Segment Not Present` exception indicates that the `present flag` of a segment or gate descriptor is clear during attempt to load one of `cs`, `ds`, `es`, `fs`, or `gs` register.
* `#SS` or `Stack Fault` exception indicates one of the stack related conditions was detected, for example a not-present stack segment is detected when attempting to load the `ss` register.
* `#GP` or `General Protection` exception indicates that the processor detected one of a class of protection violations called general-protection violations. There are many different conditions that can cause general-protection exception. For example loading the `ss`, `ds`, `es`, `fs`, or `gs` register with a segment selector for a system segment, writing to a code segment or a read-only data segment, referencing an entry in the `Interrupt Descriptor Table` (following an interrupt or exception) that is not an interrupt, trap, or task gate and many many more.
* `Spurious Interrupt` - a hardware interrupt that is unwanted.
* `#MF` or `x87 FPU Floating-Point Error` exception caused when the [x87 FPU](https://en.wikipedia.org/wiki/X86_instruction_listings#x87_floating-point_instructions) has detected a floating point error.
* `#AC` or `Alignment Check` exception Indicates that the processor detected an unaligned memory operand when alignment checking was enabled.

After that we setup this exception gates, we can see setup of the `Machine-Check` exception:

```C
#ifdef CONFIG_X86_MCE
	set_intr_gate_ist(X86_TRAP_MC, &machine_check, MCE_STACK);
#endif
```

Note that it depends on the `CONFIG_X86_MCE` kernel configuration option and indicates that the processor detected an internal [machine error](https://en.wikipedia.org/wiki/Machine-check_exception) or a bus error, or that an external agent detected a bus error. The next exception gate is for the [SIMD](https://en.wikipedia.org/?title=SIMD) Floating-Point exception:

```C
set_intr_gate(X86_TRAP_XF, &simd_coprocessor_error);
```

which indicates the processor has detected an `SSE` or `SSE2` or `SSE3` SIMD floating-point exception. There are six classes of numeric exception conditions that can occur while executing an SIMD floating-point instruction:

* Invalid operation
* Divide-by-zero
* Denormal operand
* Numeric overflow
* Numeric underflow
* Inexact result (Precision)

在`traps_init()`后面调用了`early_irq_init`，这里有个配置宏`CONFIG_SPARSE_IRQ`.但不管怎样，都会调用下面的流程：
```c
int __init early_irq_init(void)
{
    ...
	init_irq_default_affinity();
    ...
	return arch_early_irq_init();
}
```

In the next step we fill the `used_vectors` array which defined in the [arch/x86/include/asm/desc.h](https://github.com/torvalds/linux/tree/master/arch/x86/include/asm/desc.h) header file and represents `bitmap`:

```C
DECLARE_BITMAP(used_vectors, NR_VECTORS);
```

of the first `32` interrupts (more about bitmaps in the Linux kernel you can read in the part which describes [cpumasks and bitmaps](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-2))

```C
for (i = 0; i < FIRST_EXTERNAL_VECTOR; i++)
	set_bit(i, used_vectors)
```

where `FIRST_EXTERNAL_VECTOR` is:

```C
#define FIRST_EXTERNAL_VECTOR           0x20
```

After this we setup the interrupt gate for the `ia32_syscall` and add `0x80` to the `used_vectors` bitmap:

```C
#ifdef CONFIG_IA32_EMULATION
        set_system_intr_gate(IA32_SYSCALL_VECTOR, ia32_syscall);
        set_bit(IA32_SYSCALL_VECTOR, used_vectors);
#endif
```

There is `CONFIG_IA32_EMULATION` kernel configuration option on `x86_64` Linux kernels. This option provides ability to execute 32-bit processes in compatibility-mode. In the next parts we will see how it works, in the meantime we need only to know that there is yet another interrupt gate in the `IDT` with the vector number `0x80`. In the next step we maps `IDT` to the fixmap area:

```C
__set_fixmap(FIX_RO_IDT, __pa_symbol(idt_table), PAGE_KERNEL_RO);
idt_descr.address = fix_to_virt(FIX_RO_IDT);
```

and write its address to the `idt_descr.address` (more about fix-mapped addresses you can read in the second part of the [Linux kernel memory management](https://0xax.gitbook.io/linux-insides/summary/mm/linux-mm-2) chapter). After this we can see the call of the `cpu_init` function that defined in the [arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/cpu/common.c). This function makes initialization of the all `per-cpu` state. In the beginning of the `cpu_init` we do the following things: First of all we wait while current cpu is initialized and than we call the `cr4_init_shadow` function which stores shadow copy of the `cr4` control register for the current cpu and load CPU microcode if need with the following function calls:

```C
wait_for_master_cpu(cpu);
cr4_init_shadow();
load_ucode_ap();
```

Next we get the `Task State Segment` for the current cpu and `orig_ist` structure which represents origin `Interrupt Stack Table` values with the:

```C
t = &per_cpu(cpu_tss, cpu);
oist = &per_cpu(orig_ist, cpu);
```

As we got values of the `Task State Segment` and `Interrupt Stack Table` for the current processor, we clear following bits in the `cr4` control register:

```C
cr4_clear_bits(X86_CR4_VME|X86_CR4_PVI|X86_CR4_TSD|X86_CR4_DE);
```

with this we disable `vm86` extension, virtual interrupts, timestamp ([RDTSC](https://en.wikipedia.org/wiki/Time_Stamp_Counter) can only be executed with the highest privilege) and debug extension. After this we reload the `Global Descriptor Table` and `Interrupt Descriptor table` with the:

```C
	switch_to_new_gdt(cpu);
	loadsegment(fs, 0);
	load_current_idt();
```

After this we setup array of the Thread-Local Storage Descriptors, configure [NX](https://en.wikipedia.org/wiki/NX_bit) and load CPU microcode. Now is time to setup and load `per-cpu` Task State Segments. We are going in a loop through the all exception stack which is `N_EXCEPTION_STACKS` or `4` and fill it with `Interrupt Stack Tables`:

```C
	if (!oist->ist[0]) {
		char *estacks = per_cpu(exception_stacks, cpu);

		for (v = 0; v < N_EXCEPTION_STACKS; v++) {
			estacks += exception_stack_sizes[v];
			oist->ist[v] = t->x86_tss.ist[v] =
					(unsigned long)estacks;
			if (v == DEBUG_STACK-1)
				per_cpu(debug_stack_addr, cpu) = (unsigned long)estacks;
		}
	}
```

As we have filled `Task State Segments` with the `Interrupt Stack Tables` we can set `TSS` descriptor for the current processor and load it with the:

```C
set_tss_desc(cpu, t);
load_TR_desc();
```

where `set_tss_desc` macro from the [arch/x86/include/asm/desc.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/desc.h) writes given  descriptor to the `Global Descriptor Table` of the given processor:

```C
#define set_tss_desc(cpu, addr) __set_tss_desc(cpu, GDT_ENTRY_TSS, addr)
static inline void __set_tss_desc(unsigned cpu, unsigned int entry, void *addr)
{
        struct desc_struct *d = get_cpu_gdt_table(cpu);
        tss_desc tss;
        set_tssldt_descriptor(&tss, (unsigned long)addr, DESC_TSS,
                              IO_BITMAP_OFFSET + IO_BITMAP_BYTES +
                              sizeof(unsigned long) - 1);
        write_gdt_entry(d, entry, &tss, DESC_TSS);
}
```

and `load_TR_desc` macro expands to the `ltr` or `Load Task Register` instruction:

```C
#define load_TR_desc()                          native_load_tr_desc()
static inline void native_load_tr_desc(void)
{
        asm volatile("ltr %w0"::"q" (GDT_ENTRY_TSS*8));
}
```
在5.10.13中见：
```c
/*
 * Setup everything needed to handle exceptions from the IDT, including the IST
 * exceptions which use paranoid_entry().
 */
void cpu_init_exception_handling(void)
{
	struct tss_struct *tss = this_cpu_ptr(&cpu_tss_rw);
	int cpu = raw_smp_processor_id();

	/* paranoid_entry() gets the CPU number from the GDT */
	setup_getcpu(cpu);

	/* IST vectors need TSS to be set up. */
	tss_setup_ist(tss);
	tss_setup_io_bitmap(tss);
	set_tss_desc(cpu, &get_cpu_entry_area(cpu)->tss.x86_tss);

	load_TR_desc();

	/* Finally load the IDT */
	load_current_idt();
}
```

In the end of the `trap_init` function we can see the following code:

```C
set_intr_gate_ist(X86_TRAP_DB, &debug, DEBUG_STACK);
set_system_intr_gate_ist(X86_TRAP_BP, &int3, DEBUG_STACK);
...
...
...
#ifdef CONFIG_X86_64
        memcpy(&nmi_idt_table, &idt_table, IDT_ENTRIES * 16);
        set_nmi_gate(X86_TRAP_DB, &debug);
        set_nmi_gate(X86_TRAP_BP, &int3);
#endif
```

Here we copy `idt_table` to the `nmi_dit_table` and setup exception handlers for the `#DB` or `Debug exception` and `#BR` or `Breakpoint exception`. You can remember that we already set these interrupt gates in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3), so why do we need to setup it again? We setup it again because when we initialized it before in the `early_trap_init` function, the `Task State Segment` was not ready yet, but now it is ready after the call of the `cpu_init` function.

That's all. Soon we will consider all handlers of these interrupts/exceptions.

## 3.3. Conclusion

It is the end of the fourth part about interrupts and interrupt handling in the Linux kernel. We saw the initialization of the [Task State Segment](https://en.wikipedia.org/wiki/Task_state_segment) in this part and initialization of the different interrupt handlers as `Divide Error`, `Page Fault` exception and etc. You can note that we saw just initialization stuff, and will dive into details about handlers for these exceptions. In the next part we will start to do it.

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 3.4. Links

* [page fault](https://en.wikipedia.org/wiki/Page_fault)
* [Interrupt Descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table)
* [Tracing](https://en.wikipedia.org/wiki/Tracing_%28software%29)
* [cr2](https://en.wikipedia.org/wiki/Control_register)
* [RCU](https://en.wikipedia.org/wiki/Read-copy-update)
* [this_cpu_* operations](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/this_cpu_ops.txt)
* [kmemcheck](https://www.kernel.org/doc/Documentation/kmemcheck.txt)
* [prefetchw](http://www.felixcloutier.com/x86/PREFETCHW.html)
* [3DNow](https://en.wikipedia.org/?title=3DNow!)
* [CPU caches](https://en.wikipedia.org/wiki/CPU_cache)
* [VFS](https://en.wikipedia.org/wiki/Virtual_file_system) 
* [Linux kernel memory management](https://0xax.gitbook.io/linux-insides/summary/mm)
* [Fix-Mapped Addresses and ioremap](https://0xax.gitbook.io/linux-insides/summary/mm/linux-mm-2)
* [Extended Industry Standard Architecture](https://en.wikipedia.org/wiki/Extended_Industry_Standard_Architecture)
* [INT isntruction](https://en.wikipedia.org/wiki/INT_%28x86_instruction%29)
* [INTO](http://x86.renejeschke.de/html/file_module_x86_id_142.html)
* [BOUND](http://pdos.csail.mit.edu/6.828/2005/readings/i386/BOUND.htm)
* [opcode](https://en.wikipedia.org/?title=Opcode)
* [control register](https://en.wikipedia.org/wiki/Control_register#CR0)
* [x87 FPU](https://en.wikipedia.org/wiki/X86_instruction_listings#x87_floating-point_instructions)
* [MCE exception](https://en.wikipedia.org/wiki/Machine-check_exception)
* [SIMD](https://en.wikipedia.org/?title=SIMD)
* [cpumasks and bitmaps](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-2)
* [NX](https://en.wikipedia.org/wiki/NX_bit)
* [Task State Segment](https://en.wikipedia.org/wiki/Task_state_segment)
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3)
