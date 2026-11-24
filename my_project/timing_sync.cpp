#include "timing_sync.h"
#include "transmitter.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void timing_sync_init(TimingSync *ts, int L, float alpha, float step){
    ts->L = L;
    ts->tau_s = 0.0f;
    ts->err_avg = 0.0f;
    ts->alpha = alpha;
    ts->step = step;
}

void matched_filter(const Complex *in, int in_len, int L, Complex *out){
    for (int n = 0; n < in_len; n++) {
        out[n].I = 0.0f;
        out[n].Q = 0.0f;
        if (n < L - 1)
            continue;
        for (int k = 0; k < L; k++) {
            out[n].I += in[n - L + 1 + k].I;
            out[n].Q += in[n - L + 1 + k].Q;
        }
    }
}

int timing_sync_step(TimingSync *ts, const Complex *filtered_signal, int filtered_signal_len, int *idx, Complex *sym){
    int n = *idx;
    if (n < 2 || n >= filtered_signal_len)
        return 0;

    int n2 = n - 2;
    int n1 = n - 1;
    int n0 = n;

    float x0_i = filtered_signal[n2].I, x0_q = filtered_signal[n2].Q;
    float x1_i = filtered_signal[n1].I, x1_q = filtered_signal[n1].Q;
    float x2_i = filtered_signal[n0].I, x2_q = filtered_signal[n0].Q;

    float e = x1_i * (x0_i - x2_i) + x1_q * (x0_q - x2_q);

    // фильтр ошибки 
    ts->err_avg = (1.0f - ts->alpha) * ts->err_avg + ts->alpha * e;

    // time corrector: знак ошибки - сдвиг 
    if (ts->err_avg > 0.0f)
        ts->tau_s += ts->step;
    else if (ts->err_avg < 0.0f)
        ts->tau_s -= ts->step;

    // перенос накопленного смещения (если 1 накопилась) 
    while (ts->tau_s >= 1.0f) {
        ts->tau_s -= 1.0f;
        n += 1;
    }
    while (ts->tau_s <= -1.0f) {
        ts->tau_s += 1.0f;
        n -= 1;
    }

    // выход символа (средний отсчёт окна) 
    sym->I = x1_i;
    sym->Q = x1_q;

    // следующий символ: смещаемся на L отсчётов 
    *idx = n + ts->L;

    return 1;
}

int timing_sync_process(TimingSync *ts, const Complex *in, int in_len, Complex *symbols, int max_sym){
    if (in_len < ts->L || max_sym <= 0)
        return 0;

    Complex *filtered_signal = (Complex *)malloc((size_t)in_len * sizeof(Complex));
    if (!filtered_signal)
        return 0;

    matched_filter(in, in_len, ts->L, filtered_signal);

    int idx = ts->L + 2;   // чтобы были n-2, n-1, n 
    int count = 0;
    Complex sym;

    while (count < max_sym && idx < in_len) {
        int old = idx;
        if (timing_sync_step(ts, filtered_signal, in_len, &idx, &sym)) {
            symbols[count++] = sym;
        }
        if (idx <= old)   // защита от зацикливания 
            idx = old + ts->L;
    }

    free(filtered_signal);
    return count;
}