/**
 * @file lv_demo_stress.h
 *
 */

#ifndef LV_DEMO_STRESS_H
#define LV_DEMO_STRESS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lv_demos.h"

/*********************
 *      DEFINES
 *********************/

#define LV_DEMO_STRESS_TIME_STEP    50

#ifndef LV_DEMO_STRESS_DRAW_MULT
/** Repeat drawable count for stress benchmark (1 = stock demo). */
#define LV_DEMO_STRESS_DRAW_MULT    1
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void lv_demo_stress(void);

/**
 * Check if stress demo has finished one round.
 */
bool lv_demo_stress_finished(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_STRESS_H*/
