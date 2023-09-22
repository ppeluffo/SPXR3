/*
 * File:   newmain.c
 * Author: pablo
 *
 * Created on August 23, 2023, 4:32 PM
 */

#include "spxR3.h"

//------------------------------------------------------------------------------
int main( void )
{
    
    system_init_outofrtos();

	frtos_open(fdTERM, 9600 );
    frtos_open(fdRS485A, 9600 );
    frtos_open(fdI2C0, 100 );
    frtos_open(fdNVM,0);
    
	// Creamos las tareas
	run_tasks = false;

	xHandle_tkCtl = xTaskCreateStatic(tkCtl, "CTL", tkCtl_STACK_SIZE, (void *)1, tkCtl_TASK_PRIORITY, xTask_Ctl_Buffer, &xTask_Ctl_Buffer_Ptr );
    xHandle_tkCmd = xTaskCreateStatic( tkCmd, "CMD", tkCmd_STACK_SIZE, (void *)1, tkCmd_TASK_PRIORITY, xTask_Cmd_Buffer, &xTask_Cmd_Buffer_Ptr );
    xHandle_tkSys = xTaskCreateStatic( tkSys, "SYS", tkSys_STACK_SIZE, (void *)1, tkSys_TASK_PRIORITY, xTask_Sys_Buffer, &xTask_Sys_Buffer_Ptr );
    xHandle_tkRS485A = xTaskCreateStatic( tkRS485A, "RS485A", tkRS485A_STACK_SIZE, (void *)1, tkRS485A_TASK_PRIORITY, xTask_RS485A_Buffer, &xTask_RS485A_Buffer_Ptr );
    xHandle_tkWAN = xTaskCreateStatic( tkWAN, "WAN", tkWAN_STACK_SIZE, (void *)1, tkWAN_TASK_PRIORITY, xTask_WAN_Buffer, &xTask_WAN_Buffer_Ptr );
    xHandle_tkPILOTO = xTaskCreateStatic( tkPiloto, "PLT", tkPILOTO_STACK_SIZE, (void *)1, tkPILOTO_TASK_PRIORITY, xTask_PILOTO_Buffer, &xTask_PILOTO_Buffer_Ptr );
        
  
	/* Arranco el RTOS. */
	vTaskStartScheduler();

	// En caso de panico, aqui terminamos.
	exit (1);

}
//------------------------------------------------------------------------------
/*
void vApplicationIdleHook( void )
{
	// Como trabajo en modo tickless no entro mas en modo sleep aqui.
//	if ( sleepFlag == true ) {
//		sleep_mode();
//	}

}
 */
//------------------------------------------------------------------------------
/*
void vApplicationTickHook( void )
{
	//sysTicks++;

}
 */
//------------------------------------------------------------------------------
// Define the function that is called by portSUPPRESS_TICKS_AND_SLEEP().
//------------------------------------------------------------------------------
/*
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName )
{
	// Es invocada si en algun context switch se detecta un stack corrompido !!
	// Cuando el sistema este estable la removemos.
	// En FreeRTOSConfig.h debemos habilitar
	// #define configCHECK_FOR_STACK_OVERFLOW          2

//	xprintf_P( PSTR("PANIC:%s !!\r\n\0"),pcTaskName);

}
//------------------------------------------------------------------------------
 */

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
//------------------------------------------------------------------------------
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
//------------------------------------------------------------------------------


