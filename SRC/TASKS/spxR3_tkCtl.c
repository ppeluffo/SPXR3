/*
 * File:   tkCtl.c
 * Author: pablo
 *
 * Created on 25 de octubre de 2021, 12:50 PM
 */


#include "spxR3.h"

#define TKCTL_DELAY_S	5

void sys_watchdog_check(void);
void sys_daily_reset(void);


uint16_t count;

//------------------------------------------------------------------------------
void tkCtl(void * pvParameters)
{

	// Esta es la primer tarea que arranca.

( void ) pvParameters;
fat_s l_fat;

	vTaskDelay( ( TickType_t)( 500 / portTICK_PERIOD_MS ) );
    xprintf_P(PSTR("Starting tkCtl..\r\n"));
    
    // Leo la configuracion de EE en systemConf
    if ( ! load_config_from_NVM())  {
       xprintf_P(PSTR("Loading config default..\r\n"));
       config_default();
    }
       
    // Actualizo las configuraciones locales en el systemConf
    ainputs_update_local_config(&systemConf.ainputs_conf);
    counters_update_local_config(&systemConf.counters_conf);
    piloto_update_local_config(&systemConf.piloto_conf);
    
    // Funciones que se inician con el RTOS corriendo
    // Inicializo la memoria EE ( fileSysyem)
	if ( FS_open() ) {
		xprintf_P( PSTR("FSInit OK\r\n"));
	} else {
        FAT_flush();
        xprintf_P( PSTR("FSInit FAIL !!.Reformatted...\r\n"));
		//FS_format(false );	// Reformateo soft.( solo la FAT )
		//xprintf_P( PSTR("FSInit FAIL !!.Reformatted...\r\n"));
	}

    FAT_read(&l_fat);
	xprintf_P( PSTR("MEMsize=%d, wrPtr=%d, rdPtr=%d,count=%d\r\n"),FF_MAX_RCDS, l_fat.head,l_fat.tail, l_fat.count );

    
	// Imprimo el tamanio de registro de memoria
	xprintf_P( PSTR("RCD size %d bytes.\r\n"),sizeof(dataRcd_s));

    RTC_init();
    
    run_tasks = true;
    
    count = 0;
	for( ;; )
	{
        //vTaskDelay( ( TickType_t)( 1000 / portTICK_PERIOD_MS ) );
		vTaskDelay( ( TickType_t)( 1000 * TKCTL_DELAY_S / portTICK_PERIOD_MS ) );
        
        if ( ! WAN_sleeping() ) {
            led_flash();
        }
        sys_watchdog_check();
        sys_daily_reset();
	}
}
//------------------------------------------------------------------------------
void sys_watchdog_check(void)
{
    wdt_reset();
    return;
}
//------------------------------------------------------------------------------
void sys_daily_reset(void)
{
	// Todos los dias debo resetearme para restaturar automaticamente posibles
	// problemas.
	// Se invoca 1 vez por minuto ( 60s ).

static uint32_t ticks_to_reset = 86400 / TKCTL_DELAY_S ; // ticks en 1 dia.

	while ( --ticks_to_reset > 0 ) {
		return;
	}

    vTaskDelay( ( TickType_t)( 2000 / portTICK_PERIOD_MS ) );
    reset();
    
}
//------------------------------------------------------------------------------
