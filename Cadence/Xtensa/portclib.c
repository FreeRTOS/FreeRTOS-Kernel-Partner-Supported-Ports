/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2015-2025 Cadence Design Systems, Inc.
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include "FreeRTOS.h"

#if XT_USE_THREAD_SAFE_CLIB

#if XSHAL_CLIB == XTHAL_CLIB_XCLIB

#include <errno.h>
#include <sys/reent.h>

#include "semphr.h"

typedef SemaphoreHandle_t       _Rmtx;

//-----------------------------------------------------------------------------
//  Override this and set to nonzero to enable locking.
//-----------------------------------------------------------------------------
int32_t _xclib_use_mt = 1;


//-----------------------------------------------------------------------------
//  Init lock.
//-----------------------------------------------------------------------------
void
_Mtxinit(_Rmtx * mtx)
{
    // There is a little problem here with recursion. At startup _Initlocks()
    // is called (within xclib) which attempts to create all the locks which
    // comes here and can result in a call to malloc() which can try to lock
    // access to the heap which results in another call to _Initlocks(). This
    // infinite recursion must be avoided. What we do here is temporarily set
    // the _xclib_use_mt to zero so that recursive calls to _Initlocks() will
    // become nops. This allows the xSemaphoreCreateRecursiveMutex() function
    // to call malloc() safely.
    // Note that we could have used statically allocated mutexes instead but
    // that requires configSUPPORT_STATIC_ALLOCATION to be defined which in
    // turn requires other data such as idle task TCB/stack, timer task TCB/
    // stack to be statically defined.

    _xclib_use_mt = 0;
    *mtx = xSemaphoreCreateRecursiveMutex();
    _xclib_use_mt = 1;
}

//-----------------------------------------------------------------------------
//  Destroy lock.
//-----------------------------------------------------------------------------
void
_Mtxdst(_Rmtx * mtx)
{
    if ((mtx != NULL) && (*mtx != NULL)) {
        vSemaphoreDelete(*mtx);
    }
}

//-----------------------------------------------------------------------------
//  Lock.
//-----------------------------------------------------------------------------
void
_Mtxlock(_Rmtx * mtx)
{
    if ((mtx != NULL) && (*mtx != NULL)) {
        xSemaphoreTakeRecursive(*mtx, portMAX_DELAY);
    }
}

//-----------------------------------------------------------------------------
//  Unlock.
//-----------------------------------------------------------------------------
void
_Mtxunlock(_Rmtx * mtx)
{
    if ((mtx != NULL) && (*mtx != NULL)) {
        xSemaphoreGiveRecursive(*mtx);
    }
}

extern char _end[];
extern char _heap_sentry;
char * _heap_sentry_ptr = &_heap_sentry;
char * heap_ptr;

//-----------------------------------------------------------------------------
//  Called by malloc() to allocate blocks of memory from the heap.
//-----------------------------------------------------------------------------
void *
_sbrk_r (struct _reent * reent, int32_t incr)
{
    char * base;

    if (!heap_ptr)
        heap_ptr = (char *) &_end[0];

    base = heap_ptr;
    if (heap_ptr + incr >= _heap_sentry_ptr) {
        reent->_errno = ENOMEM;
        return (char *) -1;
    }

    heap_ptr += incr;
    return base;
}

//-----------------------------------------------------------------------------
//  Global initialization for C library.
//-----------------------------------------------------------------------------
void
vPortClibInit(void)
{
}

//-----------------------------------------------------------------------------
//  Per-thread cleanup stub provided for linking, does nothing.
//-----------------------------------------------------------------------------
void
_reclaim_reent(struct _reent * ptr)
{
    (void ) ptr;    /* Avoid compiler warning */
}

//-----------------------------------------------------------------------------
//  If xclib supports overriding reent_ptr_ as a function, then use it instead
//  of FreeRTOS' configSET_TLS_BLOCK() hook. Required for coherent libc support
//  on SMP; single-core is overridden too since _reent_ptr is not an lvalue.
//-----------------------------------------------------------------------------
#if (defined __DYNAMIC_REENT__)

  #if (configNUMBER_OF_CORES > 1)

struct _reent *
__getreent(void)
{
    xt_internal_data_t *xt_intdata_p = &(_xt_intdata[portGET_CORE_ID()]);
    #if ( configUSE_C_RUNTIME_TLS_SUPPORT == 1 )
    if (xt_intdata_p->xt_reent_p) {
        return xt_intdata_p->xt_reent_p;
    }
    #endif /* configUSE_C_RUNTIME_TLS_SUPPORT */

    // If TLS not configured or libc is used prior to starting the scheduler
    return &(xt_intdata_p->xt_reent);
}

  #else /* (configNUMBER_OF_CORES == 1) */

static struct _reent xt_reent __attribute__ ((section (".clib.percpu.bss")));

struct _reent *
__getreent(void)
{
    #if ( configUSE_C_RUNTIME_TLS_SUPPORT == 1 )
    if (_xt_intdata.xt_reent_p) {
        return _xt_intdata.xt_reent_p;
    }
    #endif /* configUSE_C_RUNTIME_TLS_SUPPORT */

    // If TLS not configured or libc is used prior to starting the scheduler
    return &xt_reent;
}

  #endif /* configNUMBER_OF_CORES */

#endif /* __DYNAMIC_REENT__ */

#endif /* XSHAL_CLIB == XTHAL_CLIB_XCLIB */

#if XSHAL_CLIB == XTHAL_CLIB_NEWLIB

#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semphr.h"

static SemaphoreHandle_t xClibMutex;
static uint32_t  ulClibInitDone = 0;

//-----------------------------------------------------------------------------
//  Get C library lock.
//-----------------------------------------------------------------------------
void
__malloc_lock(struct _reent * ptr)
{
    // Suppress compiler warning.
    (void) ptr;

    if (!ulClibInitDone)
        return;

    xSemaphoreTakeRecursive(xClibMutex, portMAX_DELAY);
}

//-----------------------------------------------------------------------------
//  Release C library lock.
//-----------------------------------------------------------------------------
void
__malloc_unlock(struct _reent * ptr)
{
    // Suppress compiler warning.
    (void) ptr;

    if (!ulClibInitDone)
        return;

    xSemaphoreGiveRecursive(xClibMutex);
}

//-----------------------------------------------------------------------------
//  Lock for environment. Since we have only one global lock we can just call
//  the malloc() lock function.
//-----------------------------------------------------------------------------
void
__env_lock(struct _reent * ptr)
{
    __malloc_lock(ptr);
}


//-----------------------------------------------------------------------------
//  Unlock environment.
//-----------------------------------------------------------------------------
void
__env_unlock(struct _reent * ptr)
{
    __malloc_unlock(ptr);
}

extern char _end[];
extern char _heap_sentry;
char * _heap_sentry_ptr = &_heap_sentry;
char * heap_ptr;

//-----------------------------------------------------------------------------
//  Called by malloc() to allocate blocks of memory from the heap.
//-----------------------------------------------------------------------------
void *
_sbrk_r (struct _reent * reent, int32_t incr)
{
    char * base;

    if (!heap_ptr)
        heap_ptr = (char *) &_end[0];

    base = heap_ptr;
    if (heap_ptr + incr >= _heap_sentry_ptr) {
        reent->_errno = ENOMEM;
        return (char *) -1;
    }

    heap_ptr += incr;
    return base;
}

//-----------------------------------------------------------------------------
//  Global initialization for C library.
//-----------------------------------------------------------------------------
void
vPortClibInit(void)
{
    configASSERT(!ulClibInitDone);

    xClibMutex = xSemaphoreCreateRecursiveMutex();
    ulClibInitDone  = 1;
}

//-----------------------------------------------------------------------------
//  Per-thread cleanup stub provided for linking, does nothing.
//-----------------------------------------------------------------------------
void
_reclaim_reent(struct _reent * ptr)
{
    (void ) ptr;    /* Avoid compiler warning */
}

#endif /* XSHAL_CLIB == XTHAL_CLIB_NEWLIB */

#endif /* XT_USE_THREAD_SAFE_CLIB */
