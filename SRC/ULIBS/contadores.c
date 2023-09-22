
#include "contadores.h"

typedef enum { P_OUTSIDE=0, P_CHECKING, P_INSIDE, P_FILTER } pulse_t;

// Estructura de control de los contadores.
typedef struct {
	float caudal;                    // Caudal instantaneo en c/pulso
	uint32_t ticks_count;            // total de ticks entre el pulso actual y el anterior
    uint8_t  debounceTicks_count;    // ticks desde el flanco del pulso a validarlos
    uint16_t  filterTicks_count;     // ticks desde que lo valido hasta que permito que venga otro.   
	uint16_t pulse_count;            // contador de pulsos
	pulse_t state;                   // variable de estado que indica si estoy dentro o fuera de un pulso
} counter_cbk_t;

static counter_cbk_t CNTCB[NRO_COUNTER_CHANNELS];
static bool f_debug_counters;

// Los caudales los almaceno en un RB y lo que doy es el promedio !!
typedef struct {
    float caudal;
} t_caudal_s;

t_caudal_s caudal_storage_0[MAX_RB_CAUDAL_STORAGE_SIZE];
t_caudal_s caudal_storage_1[MAX_RB_CAUDAL_STORAGE_SIZE];
rBstruct_s caudal_RB_0,caudal_RB_1;

// Configuracion local del sistema de contadores
counters_conf_t counters_conf;

void promediar_rb_caudal(void);

static SemaphoreHandle_t countersLocalSem;

//------------------------------------------------------------------------------
void counters_init_outofrtos( SemaphoreHandle_t semph)
{
    countersLocalSem = semph;
}
// -----------------------------------------------------------------------------
void counters_update_local_config( counters_conf_t *counters_system_conf)
{
    while ( xSemaphoreTake( countersLocalSem, ( TickType_t ) 5 ) != pdTRUE )
  		vTaskDelay( ( TickType_t)( 1 ) );
    memcpy( &counters_conf, counters_system_conf, sizeof(counters_conf_t));
    xSemaphoreGive( countersLocalSem );
}
// -----------------------------------------------------------------------------
void counters_read_local_config( counters_conf_t *counters_system_conf)
{
    while ( xSemaphoreTake( countersLocalSem, ( TickType_t ) 5 ) != pdTRUE )
  		vTaskDelay( ( TickType_t)( 1 ) );
    memcpy( counters_system_conf, &counters_conf, sizeof(counters_conf_t)); 
    xSemaphoreGive( countersLocalSem );
}
// -----------------------------------------------------------------------------
void counters_init( void )
{
    /*
     * Agrego rb_size para que el tamaño de los buffers sea ajustable
     */
    
uint8_t i;

    CNT0_CONFIG();
    CNT1_CONFIG();
            
    f_debug_counters = false;
       
    for ( i=0;i<NRO_COUNTER_CHANNELS;i++) {
		CNTCB[i].caudal = 0.0;          
		CNTCB[i].ticks_count = 0;
        CNTCB[i].debounceTicks_count = 0;
        CNTCB[i].filterTicks_count = 0;
		CNTCB[i].pulse_count = 0;
		CNTCB[i].state = P_OUTSIDE;
	}

    counters_clear();
    
    rBstruct_CreateStatic ( 
        &caudal_RB_0, 
        &caudal_storage_0, 
        //MAX_RB_CAUDAL_STORAGE_SIZE, 
        counters_conf.channel[0].rb_size,
        sizeof(t_caudal_s), 
        true  
    );


    rBstruct_CreateStatic ( 
        &caudal_RB_1, 
        &caudal_storage_1, 
        //MAX_RB_CAUDAL_STORAGE_SIZE, 
        counters_conf.channel[1].rb_size,
        sizeof(t_caudal_s), 
        true  
    );
}
// -----------------------------------------------------------------------------
uint8_t CNT0_read(void)
{
    return ( ( CNT0_PORT.IN & CNT0_PIN_bm ) >> CNT0_PIN) ;
}
// -----------------------------------------------------------------------------
uint8_t CNT1_read(void)
{
    return ( ( CNT1_PORT.IN & CNT1_PIN_bm ) >> CNT1_PIN) ;
}
// -----------------------------------------------------------------------------
void counters_config_defaults( void )
{
    /*
     * Realiza la configuracion por defecto de los canales digitales.
     */

uint8_t i = 0;

	for ( i = 0; i < NRO_COUNTER_CHANNELS; i++ ) {
		//snprintf_P( counters_conf[i].name, CNT_PARAMNAME_LENGTH, PSTR("X%d\0"),i );
        snprintf_P( counters_conf.channel[i].name, CNT_PARAMNAME_LENGTH, PSTR("X") );
        counters_conf.channel[i].enabled = false;
		counters_conf.channel[i].magpp = 1;
        counters_conf.channel[i].modo_medida = CAUDAL;
        counters_conf.channel[i].rb_size = 1;
	}
}
//------------------------------------------------------------------------------
void counters_print_configuration( void )
{
    /*
     * Muestra la configuracion de todos los canales de contadores en la terminal
     * La usa el comando tkCmd::status.
     */
    
uint8_t i = 0;

    xprintf_P(PSTR("Counters:\r\n"));
    xprintf_P(PSTR(" debug: "));
    f_debug_counters ? xprintf_P(PSTR("true\r\n")) : xprintf_P(PSTR("false\r\n"));
    
	for ( i = 0; i < NRO_COUNTER_CHANNELS; i++) {
        
        if ( counters_conf.channel[i].enabled ) {
            xprintf_P( PSTR(" c%d: +"),i);
        } else {
            xprintf_P( PSTR(" c%d: -"),i);
        }
                
        xprintf_P( PSTR("[%s,magpp=%.03f,"),counters_conf.channel[i].name, counters_conf.channel[i].magpp );
        if ( counters_conf.channel[i].modo_medida == CAUDAL ) {
            xprintf_P(PSTR("CAUDAL,"));
        } else {
            xprintf_P(PSTR("PULSO,"));
        }
        
        xprintf_P( PSTR("rbsize=%d]\r\n"), counters_conf.channel[i].rb_size );
    }       
}
//------------------------------------------------------------------------------
bool counters_config_channel( uint8_t ch, char *s_enable, char *s_name, char *s_magpp, char *s_modo, char *s_rb_size )
{
	// Configuro un canal contador.
	// channel: id del canal
	// s_param0: string del nombre del canal
	// s_param1: string con el valor del factor magpp.
	//
	// {0..1} dname magPP

bool retS = false;

    //xprintf_P(PSTR("DEBUG COUNTERS: en=%s,name=%s,magpp=%s,modo=%s,rbsize=%s\r\n"), s_enable,s_name,s_magpp,s_modo,s_rb_size  );

	if ( s_name == NULL ) {
		return(retS);
	}

	if ( ( ch >=  0) && ( ch < NRO_COUNTER_CHANNELS ) ) {

        // Enable ?
        if (!strcmp_P( strupr(s_enable), PSTR("TRUE"))  ) {
            counters_conf.channel[ch].enabled = true;
        
        } else if (!strcmp_P( strupr(s_enable), PSTR("FALSE"))  ) {
            counters_conf.channel[ch].enabled = false;
        }
        
		// NOMBRE
		snprintf_P( counters_conf.channel[ch].name, CNT_PARAMNAME_LENGTH, PSTR("%s"), s_name );

		// MAGPP
		if ( s_magpp != NULL ) { counters_conf.channel[ch].magpp = atof(s_magpp); }

        // MODO ( PULSO/CAUDAL )
		if ( s_modo != NULL ) {
			if ( strcmp_P( strupr(s_modo), PSTR("PULSO")) == 0 ) {
				counters_conf.channel[ch].modo_medida = PULSOS;

			} else if ( strcmp_P( strupr(s_modo) , PSTR("CAUDAL")) == 0 ) {
				counters_conf.channel[ch].modo_medida = CAUDAL;

			} else {
				xprintf_P(PSTR("ERROR: counters modo: PULSO/CAUDAL only!!\r\n"));
                return (false);
			}
		}
        
        if ( s_rb_size != NULL ) {
            counters_conf.channel[ch].rb_size = atoi(s_rb_size);
        } else {
            counters_conf.channel[ch].rb_size = 1;
        }
        
        if ( counters_conf.channel[ch].rb_size > MAX_RB_CAUDAL_STORAGE_SIZE ) {
            counters_conf.channel[ch].rb_size = 1;
        }
        
		retS = true;
	}

	return(retS);

}
//------------------------------------------------------------------------------
void counters_config_debug(bool debug )
{
    if ( debug ) {
        f_debug_counters = true;
    } else {
        f_debug_counters = false;
    }
}
//------------------------------------------------------------------------------
bool counters_read_debug(void)
{
    return (f_debug_counters);
}
//------------------------------------------------------------------------------
uint8_t counter_read_pin(uint8_t cnt)
{
    switch(cnt) {
        case 0:
            return(CNT0_read());
            break;
        case 1:
            return(CNT1_read());
            break;
        default:
            return(0);
            
    }
    return(0);
}
//------------------------------------------------------------------------------
void counter_FSM(uint8_t i )
{

uint16_t duracion_pulso_ticks = 0;
t_caudal_s rb_element;
 
    // Esta funcion la invoca el timerCallback. c/vez sumo 1 para tener los ticks
    // desde el pulso anterior.
	CNTCB[i].ticks_count++;
    
    switch ( CNTCB[i].state ) {
        case P_OUTSIDE:
            if ( counter_read_pin(i) == 0 ) {   // Llego un flanco. Inicializo
                CNTCB[i].state = P_CHECKING;
                CNTCB[i].debounceTicks_count = 0;
                CNTCB[i].filterTicks_count = 0;
                return;                
            }
            break;
            
        case P_CHECKING:
            if ( counter_read_pin(i) == 1 ) {   // Falso pulso. Me rearmo y salgo
                CNTCB[i].state = P_OUTSIDE;
                return;
            }
            // Controlo el periodo de debounce. (2ticks = 20ms)
            CNTCB[i].debounceTicks_count++;
            if ( CNTCB[i].debounceTicks_count == 2 ) {   
                CNTCB[i].pulse_count++;     // Pulso valido
                
                // Calculo el caudal instantaneo
                if ( counters_conf.channel[i].modo_medida == CAUDAL ) {
                    // Tengo 1 pulso en N ticks.
                    // 1 pulso -------> ticks_counts * 10 mS
                    // magpp (mt3) ---> ticks_counts * 10 mS
                    duracion_pulso_ticks = CNTCB[i].ticks_count;
                    CNTCB[i].ticks_count = 0;
                    
                    if ( duracion_pulso_ticks > 0 ) {
                        CNTCB[i].caudal =  (( counters_conf.channel[i].magpp * 3600000) /  ( duracion_pulso_ticks * 10)  ); // En mt3/h  
                    } else {   
                        CNTCB[i].caudal = 0;
                    } 
                    // Guardo el caudal en el RB
                    rb_element.caudal = CNTCB[i].caudal;
                    if (i==0) {
                        rBstruct_Poke(&caudal_RB_0, &rb_element);
                    } else {
                        rBstruct_Poke(&caudal_RB_1, &rb_element);
                    }
                    
                }
                
                CNTCB[i].state = P_INSIDE;  // Paso a esperar que suba
                
                if ( f_debug_counters ) {
                    if ( counters_conf.channel[i].modo_medida == CAUDAL ) {
                        xprintf_P( PSTR("COUNTERS: Q%d=%0.3f, P=%d, ticks=%d\r\n"), i, CNTCB[i].caudal, CNTCB[i].pulse_count, duracion_pulso_ticks );
                    } else {
                        xprintf_P( PSTR("COUNTERS: P%d=%d\r\n"), i, CNTCB[i].pulse_count );
                    }
                } 
                return;
            }
            break;
            
        case P_INSIDE:
            if ( counter_read_pin(i) == 0 ) {   // Flanco: Fin de pulso; Me rearmo y salgo
                CNTCB[i].state = P_FILTER;
                CNTCB[i].filterTicks_count = 0;
                return;
            }
            break;
            
        case P_FILTER:
            // Debo esperar al menos 10 ticks (100ms) antes de contar de nuevo
             CNTCB[i].filterTicks_count++;
            if ( (  CNTCB[i].filterTicks_count > 10 ) && ( counter_read_pin(i) == 1 ) ) {   
                CNTCB[i].state = P_OUTSIDE;
                return;
            }
            break;
            
        default:
            // Error: inicializo
            CNTCB[i].caudal = 0.0;          
            CNTCB[i].ticks_count = 0;
            CNTCB[i].debounceTicks_count = 0;
            CNTCB[i].filterTicks_count = 0;
            CNTCB[i].pulse_count = 0;
            CNTCB[i].state = P_OUTSIDE;
            return;
    }

}
//------------------------------------------------------------------------------
void counters_convergencia(void)
{
    /*
     * Esta opcion es para asegurar la convergencia del valor
     * en la medida que el caudal es 0.
     * En este caso debemos insertar un registro en el ringbuffer en 0 si 
     * en el periodo no llegaron pulsos.
     * OJO: Esto va antes de poner los contadores en 0. !!!
     */
  
uint8_t cnt;
t_caudal_s rb_element;

    //xprintf_P(PSTR("DEBUG COUNTERS CLEAR\r\n"));
    for ( cnt=0; cnt < NRO_COUNTER_CHANNELS; cnt++) {
        
        // Aseguro la convergencia a 0 de los caudales
        if ( CNTCB[cnt].pulse_count == 0 ) {
            rb_element.caudal = 0.0;
            if (cnt==0) {
                rBstruct_insert_at_tail( &caudal_RB_0, &rb_element );
            } else {
                rBstruct_insert_at_tail( &caudal_RB_1, &rb_element );
            }
        }
    }

}
//------------------------------------------------------------------------------
void counters_clear(void)
{
   /*
    * Una vez por periodo ( timerpoll ) borro los contadores.
    * Si en el periodo NO llegaron pulsos, aqui debo entonces en los
    * caudales agregar un 0.0 al ring buffer para que luego de un tiempo
    * converja a 0.
    * 
    */
    
uint8_t cnt;

    //xprintf_P(PSTR("DEBUG COUNTERS CLEAR\r\n"));
    for ( cnt=0; cnt < NRO_COUNTER_CHANNELS; cnt++) {        
        CNTCB[cnt].pulse_count = 0;
        CNTCB[cnt].caudal = 0.0;
    }
}
//------------------------------------------------------------------------------
void counters_read( float *l_counters )
{

uint8_t i;
    
    promediar_rb_caudal();
    
    for (i=0; i < NRO_COUNTER_CHANNELS; i++) {
        
        //xprintf_P( PSTR("DEBUG1: C%d=%d\r\n"), i, CNTCB[i].pulse_count );

        if ( counters_conf.channel[i].modo_medida == CAUDAL ) {
            l_counters[i] = CNTCB[i].caudal;
        } else {
            l_counters[i] = (float) CNTCB[i].pulse_count;
        }

    }
    
}
//------------------------------------------------------------------------------
void promediar_rb_caudal(void)
{
uint8_t i;
t_caudal_s rb_element;
float q0,q1, Qavg0,Qavg1;

    // Promedio los ringBuffers
    Qavg0=0.0;
    for (i=0; i < counters_conf.channel[0].rb_size; i++) {
        
        rb_element = caudal_storage_0[i];
        q0 = rb_element.caudal;
        Qavg0 += q0;
        if ( f_debug_counters ) {
            xprintf_P(PSTR("DEBUG: i=%d [q0=%0.3f, avgQ0=%0.3f]\r\n"), i, q0, Qavg0 );
        }        
    }
    Qavg0 /= counters_conf.channel[0].rb_size;
    CNTCB[0].caudal = Qavg0;
    if ( f_debug_counters ) {
        xprintf_P(PSTR("DEBUG: Qavg0=%0.3f\r\n"), Qavg0 );
    }
    
    
    Qavg1=0.0;
    for (i=0; i < counters_conf.channel[1].rb_size; i++) {
        
        rb_element = caudal_storage_1[i];
        q1 = rb_element.caudal;
        Qavg1 += q1;
        if ( f_debug_counters ) {
            xprintf_P(PSTR("DEBUG: i=%d [q1=%0.3f, avgQ1=%0.3f]\r\n"), i, q1, Qavg1 );
        }        
    }
    Qavg1 /= counters_conf.channel[1].rb_size;
    CNTCB[1].caudal = Qavg1;
    if ( f_debug_counters ) {
        xprintf_P(PSTR("DEBUG: Qavg1=%0.3f\r\n"), Qavg1);
    }
    
}
//------------------------------------------------------------------------------
//void counters_test_rb(char *data)
//{
    
    //guardo el data en el RB, lo promedio, imprimo el RB y el promedio
    
/*
t_caudal_s rb_element;
uint8_t i;
float avg;

    rb_element.caudal = atof(data);
    rBstruct_Poke(&caudal_RB, &rb_element);

    avg = 0.0;
    for (i=0; i<MAX_RB_CAUDAL_STORAGE_SIZE; i++) {
        rb_element = caudal_storage[i];
        avg += rb_element.caudal;
        xprintf_P(PSTR("DEBUG: i=%d, element=%d, avg=%0.3f\r\n"), i, rb_element.caudal, avg);
    }
    avg /= MAX_RB_CAUDAL_STORAGE_SIZE;
    xprintf_P(PSTR("DEBUG: AVG=%0.3f\r\n"), avg);
*/
   
//}
//------------------------------------------------------------------------------
uint8_t counters_hash( void )
{
    
uint8_t hash_buffer[32];
uint8_t i,j;
uint8_t hash = 0;
char *p;

    // Calculo el hash de la configuracion de los contadores
    for(i=0; i < NRO_COUNTER_CHANNELS; i++) {

        memset(hash_buffer, '\0', sizeof(hash_buffer));
        j = 0;
        if ( counters_conf.channel[i].enabled ) {
            j += sprintf_P( (char *)&hash_buffer[j], PSTR("[C%d:TRUE,"), i );
        } else {
            j += sprintf_P( (char *)&hash_buffer[j], PSTR("[C%d:FALSE,"), i );
        }
        j += sprintf_P( (char *)&hash_buffer[j], PSTR("%s,"), counters_conf.channel[i].name );
        j += sprintf_P( (char *)&hash_buffer[j], PSTR("%.03f,"), counters_conf.channel[i].magpp );
        
        if ( counters_conf.channel[i].modo_medida == 0 ) {
            j += sprintf_P( (char *)&hash_buffer[j], PSTR("CAUDAL,"));
        } else {
            j += sprintf_P( (char *)&hash_buffer[j], PSTR("PULSOS,"));
        }

        j += sprintf_P( (char *)&hash_buffer[j], PSTR("%d]"), counters_conf.channel[i].rb_size );

       
        p = (char *)hash_buffer;
        while (*p != '\0') {
            hash = u_hash(hash, *p++);
        }
        
        //xprintf_P(PSTR("HASH_CNT:<%s>, hash=%d\r\n"),hash_buffer, hash );
    }
 
    return(hash);
    
}
//------------------------------------------------------------------------------

        