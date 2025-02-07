/*
 * The x86 Win32 incarnation of arch-dependent OS-dependent routines.
 * See also "win32-os.c".
 */

/*
 * This software is part of the SBCL system. See the README file for
 * more information.
 *
 * This software is derived from the CMU CL system, which was
 * written at Carnegie Mellon University and released into the
 * public domain. The software is in the public domain and is
 * provided with absolutely no warranty. See the COPYING and CREDITS
 * files for more information.
 */

#include <stdio.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "os.h"
#include "arch.h"
#include "globals.h"
#include "interrupt.h"
#include "interr.h"
#include "lispregs.h"
#include "sbcl.h"

#include <sys/types.h>
#include "runtime.h"
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "thread.h"             /* dynamic_values_bytes */
#include "align.h"

#include "validate.h"

int arch_os_thread_init(struct thread *thread)
{
#ifndef LISP_FEATURE_OS_THREAD_STACK
    void *cur_stack_end;
    MEMORY_BASIC_INFORMATION stack_memory;

    asm volatile ("mov %%gs:8,%0": "=r" (cur_stack_end));

    /* Can't pull stack start from fs:4 or fs:8 or whatever,
     * because that's only what currently has memory behind
     * it from being used, so do a quick VirtualQuery() and
     * grab the AllocationBase. -AB 2006/11/25
     */

    /* This memset is probably not really necessary but it shuts up a warning
     * about uninitialized memory. Compiler might be confused that stack_memory
     * is both the address being queried, and the address of the result */
    memset(&stack_memory, 0, sizeof(stack_memory));
    if (!VirtualQuery(&stack_memory, &stack_memory, sizeof(stack_memory))) {
        fprintf(stderr, "VirtualQuery: 0x%lx.\n", GetLastError());
        lose("Could not query stack memory information.");
    }

    thread->control_stack_start = stack_memory.AllocationBase;
    thread->control_stack_end = cur_stack_end;
#endif

    extern void win32_set_stack_guarantee(void);
    win32_set_stack_guarantee();

    return 1;
}

/* free any arch/os-specific resources used by thread, which is now
 * defunct.  Not called on live threads
 */

int arch_os_thread_cleanup(struct thread *thread) {
    return 0;
}

sigset_t *os_context_sigmask_addr(os_context_t *context)
{
  return &context->sigmask;
}

void visit_context_registers(void (*proc)(os_context_register_t,void*),
                             os_context_t *context, void* arg)
{
    proc(context->win32_context->Rip, arg);
    proc(context->win32_context->Rax, arg);
    proc(context->win32_context->Rcx, arg);
    proc(context->win32_context->Rdx, arg);
    proc(context->win32_context->Rbx, arg);
    // don't bother with rsp or rbp
    proc(context->win32_context->Rsi, arg);
    proc(context->win32_context->Rdi, arg);
    proc(context->win32_context->R8,  arg);
    proc(context->win32_context->R9,  arg);
    proc(context->win32_context->R10, arg);
    proc(context->win32_context->R11, arg);
    proc(context->win32_context->R12, arg);
    proc(context->win32_context->R13, arg);
    proc(context->win32_context->R14, arg);
    proc(context->win32_context->R15, arg);
}

os_context_register_t *
os_context_register_addr(os_context_t *context, int offset)
{
    static const size_t offsets[16] = {
        offsetof(CONTEXT,Rax),
        offsetof(CONTEXT,Rcx),
        offsetof(CONTEXT,Rdx),
        offsetof(CONTEXT,Rbx),
        offsetof(CONTEXT,Rsp),
        offsetof(CONTEXT,Rbp),
        offsetof(CONTEXT,Rsi),
        offsetof(CONTEXT,Rdi),
        offsetof(CONTEXT,R8),
        offsetof(CONTEXT,R9),
        offsetof(CONTEXT,R10),
        offsetof(CONTEXT,R11),
        offsetof(CONTEXT,R12),
        offsetof(CONTEXT,R13),
        offsetof(CONTEXT,R14),
        offsetof(CONTEXT,R15),
    };
    return (void*)
       ((offset >= 0 && offset < 16) ?
        ((char*)(context->win32_context)) + offsets[offset] : 0);
}

os_context_register_t *
os_context_sp_addr(os_context_t *context)
{
    return (void*)&context->win32_context->Rsp; /* REG_UESP */
}

os_context_register_t *
os_context_fp_addr(os_context_t *context)
{
    return (void*)&context->win32_context->Rbp; /* REG_EBP */
}

unsigned long
os_context_fp_control(os_context_t *context)
{
    return ((((context->win32_context->FloatSave.ControlWord) & 0xffff) ^ 0x3f) |
            (((context->win32_context->FloatSave.StatusWord) & 0xffff) << 16));
}

void
os_restore_fp_control(os_context_t *context)
{
    asm ("fldcw %0" : : "m" (context->win32_context->FloatSave.ControlWord));
}

os_context_register_t *
os_context_float_register_addr(os_context_t *context, int offset)
{
    return (os_context_register_t*)&context->win32_context->FloatSave.XmmRegisters[offset];
}

void
os_flush_icache(os_vm_address_t address, os_vm_size_t length)
{
}
