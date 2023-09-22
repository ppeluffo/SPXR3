/* 
 * File:   utils.h
 * Author: pablo
 *
 * Created on 4 de septiembre de 2023, 03:43 PM
 */

#ifndef UTILS_H
#define	UTILS_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <avr/pgmspace.h>
#include "stdint.h"
    
uint8_t u_hash(uint8_t seed, char ch );
        

#ifdef	__cplusplus
}
#endif

#endif	/* UTILS_H */

