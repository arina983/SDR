#ifndef TIMING_SYNC_H
#define TIMING_SYNC_H
#include "transmitter.h"

typedef struct {
    int L; // отсчётов на символ 
    float tau_s; // текущее смещение tau_s 
    float err_avg; // усреднённая ошибка после фильтра
    float alpha; // коэффициент фильтра ошибки (0..1), например 0.1
    float step; // шаг коррекции tau_s, например 0.05
} TimingSync;

void timing_sync_init(TimingSync *ts, int L, float alpha, float step);
void matched_filter(const Complex *in, int in_len, int L, Complex *out);
int timing_sync_step(TimingSync *ts, const Complex *filtered_signal, int filtered_signal_len, int *idx, Complex *sym);
int timing_sync_process(TimingSync *ts, const Complex *in, int in_len, Complex *symbols, int max_sym);

#endif