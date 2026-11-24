#ifndef TRANSMITTER_H
#define TRANSMITTER_H
#include <stdint.h>
#include <stdlib.h>

typedef struct{
    float I;
    float Q;
} Complex;

void generate_bits(int *bits, int N);

void bpsk(int *bits, Complex *symbols, int N);

void qpsk(int *bits, Complex *symbols, int N);

void upsample(Complex *symbols, Complex *out, int N, int L);
void rectangular(Complex *symbols, Complex *out, int N, int L);

#endif