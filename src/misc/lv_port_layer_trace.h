/**
 * @file lv_port_layer_trace.h
 * @brief Optional layer-tagged stderr traces for LVGL internals walkthrough.
 */
#ifndef LV_PORT_LAYER_TRACE_H
#define LV_PORT_LAYER_TRACE_H

#include <stdio.h>

#ifndef LV_USE_PORT_LAYER_TRACE
#define LV_USE_PORT_LAYER_TRACE 0
#endif

#if LV_USE_PORT_LAYER_TRACE
#define LV_PORT_LAYER_TRACE(layer, fmt, ...) \
    do { \
        fprintf(stderr, "[LVGL:" layer "] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)
#else
#define LV_PORT_LAYER_TRACE(layer, fmt, ...) ((void)0)
#endif

#endif /*LV_PORT_LAYER_TRACE_H*/
