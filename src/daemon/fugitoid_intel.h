#ifndef FUGITOID_INTEL_H
#define FUGITOID_INTEL_H

/* Fugitoidd provides historical context */
typedef struct {
    int avg_daily_temp;
    int peak_cpu_observed;
    int total_resections; /* How many times Shredderd swung the blade */
} fugitoid_summary_t;

#endif
