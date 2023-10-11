/*
 * File:   tkSystem.c
 * Author: pablo
 *
 * Espera timerPoll y luego muestra los valores de las entradas analogicas.
 * 
 */


#include "spxR3.h"

dataRcd_s dataRcd;

//------------------------------------------------------------------------------
void tkSys(void * pvParameters)
{

//TickType_t xLastWakeTime = 0;
uint32_t waiting_ticks;
uint8_t i;

	while (! run_tasks )
		vTaskDelay( ( TickType_t)( 100 / portTICK_PERIOD_MS ) );
    
    xprintf_P(PSTR("Starting tkSys..\r\n"));
           
    ainputs_init(systemConf.samples_count);
    
    // Si uso un timer para ejecutar una FSM de los contadores entonces:
    CNT0_init();
    CNT1_init();
    
//    xLastWakeTime = xTaskGetTickCount();
    // Espero solo 10s para el primer poleo ( no lo almaceno !!)
    vTaskDelay( ( TickType_t)( 10000 / portTICK_PERIOD_MS ) );
    poll_data(&dataRcd);
    xprint_dr(&dataRcd);
    
	for( ;; )
	{
        // Espero timerpoll ms.
        //waiting_ticks = (uint32_t)systemConf.timerpoll * 1000 / portTICK_PERIOD_MS;
        //vTaskDelayUntil( &xLastWakeTime, ( TickType_t)( waiting_ticks ));
        // El poleo se lleva 5 secs.
        waiting_ticks = (uint32_t) ( systemConf.timerpoll * 1000 - PWRSENSORES_SETTLETIME_MS )  / portTICK_PERIOD_MS / 10;
        for (i=0; i< 10; i++) {
            vTaskDelay( ( TickType_t)( waiting_ticks ) );
        }
        
        // Leo datos
        poll_data(&dataRcd); 
        // Proceso ( transmito o almaceno) frame de datos por la WAN
        WAN_process_data_rcd(&dataRcd);
        // Imprimo localmente en pantalla
        xprint_dr(&dataRcd);
        
	}
}
//------------------------------------------------------------------------------
bool poll_data(dataRcd_s *dataRcd)
{
    /*
     * Se encarga de leer los datos.
     * Lo hacemos aqui asi es una funcion que se puede invocar desde Cmd.
     */
bool f_status;
uint8_t channel;
float mag;
uint16_t raw;
bool retS = false;

    // Prendo los sensores
    ainputs_prender_sensores();
                
    // los valores publicados en el systemVars los leo en variables locales.
    while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
  		vTaskDelay( ( TickType_t)( 1 ) );
        
    // ANALOG: Leo los 3 canales analogicos
    for ( channel = 0; channel < NRO_ANALOG_CHANNELS; channel++) {
        if ( systemConf.ainputs_conf.channel[channel].enabled ) {
            ainputs_read_channel ( channel, &mag, &raw );
            systemVars.ainputs[channel] = mag;
        }
    }
        
    // Leo la bateria
    if ( systemConf.ainputs_conf.channel[2].enabled ) {
        systemVars.battery = -1.0;
    } else{
        ainputs_read_channel ( 99, &mag, &raw );
        systemVars.battery = mag;
    }
    
    // Apago los sensores
    ainputs_apagar_sensores(); 
       
    // COUNTERS
    systemVars.counters[0] = CNT0_read();
    systemVars.counters[1] = CNT1_read();
    counters_convergencia();
    CNT0_clear();
    CNT1_clear();
    
    // Armo el dr.
    memcpy(dataRcd->l_ainputs, systemVars.ainputs, sizeof(dataRcd->l_ainputs));
    memcpy(dataRcd->l_counters, systemVars.counters, sizeof(dataRcd->l_counters)); 
    dataRcd->battery = systemVars.battery;  
    
    // Agrego el timestamp.
    f_status = RTC_read_dtime( &dataRcd->rtc );
    if ( ! f_status ) {
        xprintf_P(PSTR("ERROR: I2C:RTC:data_read_inputs\r\n"));
        retS = false;
        goto quit;
    }
            
    retS = true;
        
 quit:
 
    xSemaphoreGive( sem_SYSVars );
    return(retS);
        
}
//------------------------------------------------------------------------------
dataRcd_s *get_system_dr(void)
{
    return(&dataRcd);
}
//------------------------------------------------------------------------------
