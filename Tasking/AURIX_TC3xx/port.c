/*
 * Copyright (c) 2023 Infineon Technologies AG. All rights reserved.
 *
 *
 *                               IMPORTANT NOTICE
 *
 *
 * Use of this file is subject to the terms of use agreed between (i) you or
 * the company in which ordinary course of business you are acting and (ii)
 * Infineon Technologies AG or its licensees. If and as long as no such
 * terms of use are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or
 * organization obtaining a copy of the software and accompanying
 * documentation covered by this license (the "Software") to use, reproduce,
 * display, distribute, execute, and transmit the Software, and to prepare
 * derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer, must
 * be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are
 * solely in the form of machine-executable object code generated by a source
 * language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/* Prgoram status word macros */
#define portINITIAL_SYSTEM_PSW \
    ( 0x000008FFUL ) /* Supervisor Mode, MPU Register Set 0 and Call Depth Counting disabled. */

/* Context save area macros */
#define portCSA_FCX_MASK          ( 0x000FFFFFUL )
#define portINITIAL_LOWER_PCXI    ( 0x00300000UL ) /* Set UL to upper and PIE to 1 */
#define portINITIAL_UPPER_PCXI    ( 0x00200000UL ) /* Set UL to lower and PIE to 1 */
#define portNUM_WORDS_IN_CSA      ( 16 )

extern volatile unsigned long * pxCurrentTCB;

/* Tick and context switch config */
#define portTICK_COUNT    ( configSTM_CLOCK_HZ / configTICK_RATE_HZ )

/* Register defines */
static volatile uint32_t *const pxStm = configSTM;
static volatile uint32_t *const pxStmSrc = configSTM_SRC;
static volatile uint32_t *const pxContextSrc = configCONTEXT_SRC;
#define portSTM_TIM0                 0x10
#define portSTM_CMP0                 0x30
#define portSTM_COMCON               0x38
#define portSTM_ICR                  0x3C
#define portSTM_ISCR                 0x40
#define portSTM_OCS                  0xE8

#define portSTM_CMCON_MSTART0_OFF    8
#define portSTM_CMCON_MSIZE0_OFF     0
#define portSTM_ICR_CMP0EN_OFF       0
#define portSTM_ICR_CMP0OS_OFF       2
#define portSTM_ISCR_CMP0IRR_OFF     0

static inline void vPortStartFirstTask( void );
static inline void vPortInitContextSrc( void );
static inline void vPortInitTickTimer( void );

static inline void __attribute__( ( always_inline ) ) vPortLoadContext( unsigned char ucCallDepth );
static inline void __attribute__( ( always_inline ) ) vPortSaveContext( unsigned char ucCallDepth );

static inline uint32_t * __attribute__( ( always_inline ) ) pxPortCsaToAddress( uint32_t xCsa );

static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/* FreeRTOS required functions */
BaseType_t xPortStartScheduler( void )
{
    vPortInitTickTimer();
    vPortInitContextSrc();
    vPortStartFirstTask();

    return 0;
}

void vPortEndScheduler()
{
    pxStmSrc[ 0 ] &= ~( 1 << portSRC_SRCR_SRE_OFF );
    pxContextSrc[ 0 ] &= ~( 1 << portSRC_SRCR_SRE_OFF );
}

StackType_t *pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void * pvParameters )
{
    uint32_t xLowerCsa = 0, xUpperCsa = 0;
    uint32_t * pxUpperCSA = NULL;
    uint32_t * pxLowerCSA = NULL;

    /* Have to disable interrupts here because the CSAs are going to be
     * manipulated. */
    __disable();
    {
        /* DSync to ensure that buffering is not a problem. */
        __dsync();

        /* Consume two free CSAs. */
        xLowerCsa = __mfcr( portCPU_FCX );
        pxLowerCSA = pxPortCsaToAddress( xLowerCsa );

        if( pxLowerCSA != NULL )
        {
            /* The Lower Links to the Upper. */
            xUpperCsa = pxLowerCSA[ 0 ];
            pxUpperCSA = pxPortCsaToAddress( pxLowerCSA[ 0 ] );
        }

        /* Check that we have successfully reserved two CSAs. */
        if( ( pxLowerCSA != NULL ) && ( pxUpperCSA != NULL ) )
        {
            /* Remove the two consumed CSAs from the free CSA list. */
            __mtcr( portCPU_FCX, pxUpperCSA[ 0 ] );
        }
        else
        {
            /* Simply trigger a context list depletion trap. */
            __asm( "\tsvlcx" );
        }
    }
    __enable();

    /* Upper Context. */
    memset( pxUpperCSA, 0, portNUM_WORDS_IN_CSA * sizeof( uint32_t ) );
    pxUpperCSA[ 2 ] = ( uint32_t ) pxTopOfStack; /* A10;    Stack Return aka Stack Pointer */
    pxUpperCSA[ 1 ] = portINITIAL_SYSTEM_PSW;    /* PSW    */
    pxUpperCSA[ 0 ] = portINITIAL_UPPER_PCXI;

    /* Lower Context. */
    memset( pxLowerCSA, 0, portNUM_WORDS_IN_CSA * sizeof( uint32_t ) );
    pxLowerCSA[ 8 ] = ( uint32_t ) pvParameters;          /* A4;    Address Type Parameter Register    */
    pxLowerCSA[ 1 ] = ( uint32_t ) pxCode;                /* A11;    Return Address aka RA */
    pxLowerCSA[ 0 ] = portINITIAL_LOWER_PCXI | xUpperCsa; /* PCXI pointing to the Upper context. */

    /* Initialize the uxCriticalNesting. */
    pxTopOfStack--;
    *pxTopOfStack = 0;
    /* Save the link to the CSA to the top of stack. */
    pxTopOfStack--;
    *pxTopOfStack = xLowerCsa;

    return pxTopOfStack;
}

void __interrupt( configCONTEXT_INTERRUPT_PRIORITY ) __vector_table( configCPU_NR )
vPortSystemContextHandler()
{
    /* Disable interrupts to protect section*/
    __disable();

    /* Do a save, switch, execute */
    vPortSaveContext( 0 );
    vTaskSwitchContext();
    vPortLoadContext( 0 );

    __enable();
}

void __interrupt( configTIMER_INTERRUPT_PRIORITY ) __vector_table( configCPU_NR )
vPortSystemTickHandler()
{
    unsigned long ulSavedInterruptMask;
    BaseType_t xYieldRequired;

    /* Increment compare value by tick count */
    pxStm[ portSTM_CMP0 >> 2 ] = pxStm[ portSTM_CMP0 >> 2 ] + portTICK_COUNT;
    pxStm[ portSTM_ISCR >> 2 ] |= ( 1 << portSTM_ISCR_CMP0IRR_OFF );

    /* Check for possible tick drop.
     * If the time is beyond the compare value, the next tick will need a complete
     * wrap around. The tick count isn't accruate any more. Increase the tick count
     * or adapt to execute xTaskIncrementTick multiple times depending on the
     * counts missed.   */
    #if configCPU_STM_DEBUG != 0
        configASSERT( ( pxStm[ portSTM_CMP0 >> 2 ] - pxStm[ portSTM_TIM0 >> 2 ] ) <= portTICK_COUNT );
    #endif

    /* Kernel API calls require Critical Sections. */
    ulSavedInterruptMask = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* Increment the Tick. */
        xYieldRequired = xTaskIncrementTick();
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR( ulSavedInterruptMask );

    portYIELD_FROM_ISR( xYieldRequired );
}

void __attribute__( ( noinline ) ) vPortSyscallYield( void );

int
#if configPROVIDE_SYSCALL_TRAP != 0
    __trap( configTIMER_INTERRUPT_PRIORITY ) __vector_table( configCPU_NR )
#endif
vPortSyscallHandler( unsigned char id )
{
    switch( id )
    {
        case 0:
            vPortSyscallYield();
            break;

        default:
            break;
    }

    return 0;
}

void vPortInitTickTimer()
{
    pxStm[ portSTM_COMCON >> 2 ] =
        ( 0 << portSTM_CMCON_MSTART0_OFF ) | ( 31 << portSTM_CMCON_MSIZE0_OFF );
    pxStm[ portSTM_ICR >> 2 ] &= ~( 1 << portSTM_ICR_CMP0OS_OFF );
    pxStmSrc[ 0 ] = ( ( configCPU_NR > 0 ?
                        configCPU_NR + 1 : configCPU_NR ) << portSRC_SRCR_TOS_OFF ) |
                    ( ( configTIMER_INTERRUPT_PRIORITY ) << portSRC_SRCR_SRPN_OFF );
    pxStmSrc[ 0 ] |= ( 1 << portSRC_SRCR_SRE_OFF );
    pxStm[ portSTM_CMP0 >> 2 ] = pxStm[ portSTM_TIM0 >> 2 ];
    pxStm[ portSTM_ISCR >> 2 ] |= ( 1 << portSTM_ISCR_CMP0IRR_OFF );
    pxStm[ portSTM_ICR >> 2 ] |= ( 1 << portSTM_ICR_CMP0EN_OFF );
    pxStm[ portSTM_CMP0 >> 2 ] = pxStm[ portSTM_TIM0 >> 2 ] + portTICK_COUNT;
    #if configTICK_STM_DEBUG != 0
        pxStm[ portSTM_OCS >> 2 ] = 0x12000000;
    #endif
}

void vPortInitContextSrc()
{
    pxContextSrc[ 0 ] =
        ( ( configCPU_NR > 0 ?
            configCPU_NR + 1 : configCPU_NR ) << portSRC_SRCR_TOS_OFF ) |
        ( ( configCONTEXT_INTERRUPT_PRIORITY ) << portSRC_SRCR_SRPN_OFF );
    pxContextSrc[ 0 ] |= ( 1 << portSRC_SRCR_SRE_OFF );
}

void vPortStartFirstTask()
{
    /* Disable interrupts  */
    __disable();

    vPortLoadContext( 0 );

    /* Reset the call stack counting, to avoid trap on rfe */
    unsigned long ulPsw = __mfcr( portCPU_PSW );

    ulPsw &= ~( portCPU_PSW_CSC_MSK );
    __mtcr( portCPU_PSW, ulPsw );

    /* Load the lower context and upper context through rfe to enable irqs */
    __asm( "\trslcx" );
    __asm( "\trfe" );
    __nop();
    __nop();
    __nop();
}

void vPortLoadContext( unsigned char ucCallDepth )
{
    uint32_t ** ppxTopOfStack;
    uint32_t uxLowerCSA;

    /* Dsync is required to complete any memory requests */
    __dsync();

    /* Load the new CSA id from the stack and update the stack pointer */
    ppxTopOfStack = ( uint32_t ** ) pxCurrentTCB;
    uxLowerCSA = **ppxTopOfStack;
    ( *ppxTopOfStack )++;
    uxCriticalNesting = **ppxTopOfStack;
    ( *ppxTopOfStack )++;

    /* Store the lower context directly if inside the syscall or interrupt,
     * else replace the lower context in the call stack. */
    if( !ucCallDepth )
    {
        /* Update the link register */
        __mtcr( portCPU_PCXI, uxLowerCSA );
    }
    else
    {
        /* Update previous lower context */
        uint32_t * pxCSA = pxPortCsaToAddress( __mfcr( portCPU_PCXI ) );
        int i;

        for(i = 0; i < ucCallDepth - 1; i++)
        {
            pxCSA = pxPortCsaToAddress( pxCSA[ 0 ] );
        }

        pxCSA[ 0 ] = uxLowerCSA;
    }
}

void vPortSaveContext( unsigned char ucCallDepth )
{
    uint32_t ** ppxTopOfStack;
    uint32_t * pxLowerCSA, * pxUpperCSA;
    uint32_t uxLowerCSA;

    /* Dsync is required for save CSA access */
    __dsync();

    /* Get the current context information. */
    uxLowerCSA = __mfcr( portCPU_PCXI );

    /* If this function is used inside a function from the syscall or interrupt,
     * load the correct context from the call stack */
    if( ucCallDepth )
    {
        uint32_t * pxCSA = pxPortCsaToAddress( uxLowerCSA );
        int i;

        for(i = 0; i < ucCallDepth - 1; i++)
        {
            pxCSA = pxPortCsaToAddress( pxCSA[ 0 ] );
        }

        uxLowerCSA = pxCSA[ 0 ];
    }

    pxLowerCSA = pxPortCsaToAddress( uxLowerCSA );
    pxUpperCSA = pxPortCsaToAddress( pxLowerCSA[ 0 ] );

    /* Load the stack pointer */
    ppxTopOfStack = ( uint32_t ** ) pxCurrentTCB;
    /* Update the stack info in the TCB */
    *ppxTopOfStack = ( uint32_t * ) pxUpperCSA[ 2 ];
    /* Place ucNestedContext */
    ( *ppxTopOfStack )--;
    **ppxTopOfStack = uxCriticalNesting;
    /* Place the lower CSA id on the stack */
    ( *ppxTopOfStack )--;
    **ppxTopOfStack = uxLowerCSA;
}

void vPortSyscallYield()
{
    /* Do a save, switch, execute */
    vPortSaveContext( configSYSCALL_CALL_DEPTH );
    vTaskSwitchContext();
    vPortLoadContext( configSYSCALL_CALL_DEPTH );
}

uint32_t * pxPortCsaToAddress( uint32_t xCsa )
{
    uint32_t pxCsa;

    pxCsa = ( __extru( xCsa, 16, 4 ) << 28 );
    pxCsa = __insert( pxCsa, xCsa, 6, 16 );
    return ( uint32_t * ) pxCsa;
}

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    /* This is not the interrupt safe version of the enter critical function so
     * assert() if it is being called from an interrupt context.  Only API
     * functions that end in "FromISR" can be used in an interrupt.  Only assert if
     * the critical nesting count is 1 to protect against recursive calls if the
     * assert function also uses a critical section. */
    if( uxCriticalNesting == 1 )
    {
        portASSERT_IF_IN_ISR();
    }
}

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;

    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}

/*
 * When a task is deleted, it is yielded permanently until the IDLE task
 * has an opportunity to reclaim the memory that that task was using.
 * Typically, the memory used by a task is the TCB and Stack but in the
 * TriCore this includes the CSAs that were consumed as part of the Call
 * Stack. These CSAs can only be returned to the Globally Free Pool when
 * they are not part of the current Call Stack, hence, delaying the
 * reclamation until the IDLE task is freeing the task's other resources.
 * This function uses the head of the linked list of CSAs (from when the
 * task yielded for the last time) and finds the tail (the very bottom of
 * the call stack) and inserts this list at the head of the Free list,
 * attaching the existing Free List to the tail of the reclaimed call stack.
 *
 * NOTE: In highly loaded systems the release of used CSAs might be delayed,
 * since it is executed es part of the calling tasks, if the deleted task is
 * different from the calling tasks, or as part of the idle task, if the deleted
 * tasks is the same as the calling task.
 */
void vPortReclaimCSA( unsigned long ** pxTCB )
{
    uint32_t ulHeadCSA, ulFreeCSA;
    uint32_t * pulNextCSA;

    /* The lower context (PCXI value) to return to the task is stored as the
     * current element on the stack. Mask off everything in the PCXI register
     * other than the address. */
    ulHeadCSA = ( **pxTCB ) & portCSA_FCX_MASK;

    /* Iterate over the CSAs that were consumed as part of the task. */
    for(pulNextCSA = pxPortCsaToAddress( ulHeadCSA );
        ( pulNextCSA[ 0 ] & portCSA_FCX_MASK ) != 0;
        pulNextCSA = pxPortCsaToAddress( pulNextCSA[ 0 ] ) )
    {
        /* Mask off everything in the PCXI value other than the address. */
        pulNextCSA[ 0 ] &= portCSA_FCX_MASK;
    }

    __disable();
    {
        /* Look up the current free CSA head. */
        __dsync();
        ulFreeCSA = __mfcr( portCPU_FCX );

        /* Join the current free onto the tail of what is being reclaimed. */
        pulNextCSA[ 0 ] = ulFreeCSA;

        /* Move the head of the reclaimed into the Free. */
        __mtcr( portCPU_FCX, ulHeadCSA );
    }
    __enable();
}

void __attribute__( ( noreturn ) ) vPortLoopForever( void )
{
    while( 1 )
    {
    }
}
