#include <stdint.h>
#include <stdlib.h>
#include "transmitter.h"

void generate_bits(int *bits, int N){
    const char *msg = "hello";
    int msg_len = 5;
    int msg_bits = msg_len * 8;

    for (int i = 0; i < N; i++) {
        int pos = i % msg_bits;
        int byte_index = pos / 8;
        int bit_index  = 7 - (pos % 8);

        bits[i] = (msg[byte_index] >> bit_index) & 1;
    }
}

void bpsk(int *bits, Complex *symbols, int N){
    for (int i = 0; i < N; i++){
        symbols[i].I = bits[i] ? -1.0f : 1.0f;
        symbols[i].Q = 0.0f;
    }
}

void qpsk(int *bits, Complex *symbols, int N){
    for (int i = 0; i < N / 2; i++){
        symbols[i].I = bits[2 * i] ? -1.0f : 1.0f;
        symbols[i].Q = bits[2 * i + 1] ? -1.0f : 1.0f;
    }
}

void upsample(Complex *symbols, Complex *out, int N, int L){
    for (int i = 0; i < N * L; i++){
        out[i].I = 0.0f;
        out[i].Q = 0.0f;
    }

    for (int i = 0; i < N; i++)
        out[i * L] = symbols[i];
}

void rectangular(Complex *symbols, Complex *out, int N, int L){
    for (int i = 0; i < N; i++)
        for (int j = 0; j < L; j++)
            out[i * L + j] = symbols[i];
}

