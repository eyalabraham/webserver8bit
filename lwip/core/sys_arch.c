/*
 *  sys_arch.c
 *
 *      implementation of system architecture functionality
 *      for FlashLite v25 SBC running propriatery XDOS or
 *      my LMTE multi-tasking executive
 *
 *      Created, April 2017 - EYal Abraham
 *
 */

#include    "lwip/sys.h"
#include    "arch/sys_arch.h"

#ifdef  SYSTEM_DOS

#include    <dos.h>
struct dostime_t    sysTime;                    // hold system time for timeout calculations
u32_t               dosTimetic;                 // DOS time tick temp
#endif

#ifdef  SYSTEM_LMTE
#include    "lmte.h"
#endif

/* -----------------------------------------
 * sys_init()
 *
 *  Is called to initialize the sys_arch layer
 *
 * ----------------------------------------- */
void sys_init(void)
{
#ifdef  SYSTEM_DOS
    dosTimetic = (u32_t) sysTime.hsecond;       // DOS is only 1/100 second resolution!
#endif  /* SYSTEM_DOS */
#ifdef  SYSTEM_LMTE
#endif  /* SYSTEM_LMTE */
}

/* -----------------------------------------
 *  sys_arch_protect()
 *
 * This optional function does a "fast" critical region protection and returns
 * the previous protection level. This function is only called during very short
 * critical regions. An embedded system which supports ISR-based drivers might
 * want to implement this function by disabling interrupts. Task-based systems
 * might want to implement this by using a mutex or disabling tasking. This
 * function should support recursive calls from the same task or interrupt. In
 * other words, sys_arch_protect() could be called while already protected. In
 * that case the return value indicates that it is already protected.
 *
 * ----------------------------------------- */
sys_prot_t sys_arch_protect(void)
{
#ifdef  SYSTEM_DOS
#endif  /* SYSTEM_DOS */
#ifdef  SYSTEM_LMTE
    setCSflag();                                // prevent task switching
#endif  /* SYSTEM_LMTE */
    return 0;
}

/* -----------------------------------------
 *  sys_arch_unprotect()
 *
 * This optional function does a "fast" set of critical region protection to the
 * value specified by pval. See the documentation for sys_arch_protect() for
 * more information. This function is only required if your port is supporting
 * an operating system.
 *
 * ----------------------------------------- */
void sys_arch_unprotect(sys_prot_t pval)
{
#ifdef  SYSTEM_DOS
#endif  /* SYSTEM_DOS */
#ifdef  SYSTEM_LMTE
    clearCSflag();                              // reenable task switching
#endif  /* SYSTEM_LMTE */
}

/* -----------------------------------------
 *  sys_now(void)
 *
 * This optional function returns the current time in milliseconds (don't care
 * for wraparound, this is only used for time diffs).
 * Not implementing this function means you cannot use some modules (e.g. TCP
 * timestamps, internal timeouts for NO_SYS==1).
 *
 * ----------------------------------------- */
u32_t sys_now(void)
{
#ifdef  SYSTEM_DOS
    _dos_gettime(&sysTime);                     // get system time
    dosTimetic = (u32_t) sysTime.hsecond +
                 (u32_t) (100 * sysTime.second) +
                 (u32_t) (6000 * sysTime.minute) +
                 (u32_t) (360000 * sysTime.hour);
    return dosTimetic;
#endif  /* SYSTEM_DOS */
#ifdef  SYSTEM_LMTE
    return getGlobalTicks();                    // return LMTE global tick count
#endif  /* SYSTEM_LMTE */
#ifndef SYSTEM_DOS                              // protection in case nothing is defined
#ifndef SYSTEM_LMTE
    return 0L;
#endif
#endif
}
