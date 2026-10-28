#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include "transmitter.h"

#define SAMPLE_RATE 1000000.0

#define RX_FREQ_SDR1 790000000.0
#define TX_FREQ_SDR1 700000000.0

#define RX_FREQ_SDR2 700000000.0
#define TX_FREQ_SDR2 790000000.0

#define RX_GAIN 60.0
#define TX_GAIN 60.0

#define ITERATION_COUNT 200
#define TIMEOUT_US 400000

static volatile sig_atomic_t running = 1;

// Обработчик сигнала остановки программы
static void stop_program(int sig){
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]){
    if (argc != 4){
        return EXIT_FAILURE;
    }

    const char *sdr_id = argv[1];
    const char *mode = argv[2];
    const char *uri = argv[3];

    bool sdr1 = (strcmp(sdr_id, "sdr1") == 0);
    bool sdr2 = (strcmp(sdr_id, "sdr2") == 0);
    bool rx_mode = (strcmp(mode, "rx") == 0);
    bool tx_mode = (strcmp(mode, "tx") == 0);

    if ((!sdr1 && !sdr2) || (!rx_mode && !tx_mode)){
        return EXIT_FAILURE;
    }

    // Частоты выбираются по номеру SDR
    double rx_freq = sdr1 ? RX_FREQ_SDR1 : RX_FREQ_SDR2;
    double tx_freq = sdr1 ? TX_FREQ_SDR1 : TX_FREQ_SDR2;

    printf("Устройство: %s\n", sdr_id);
    printf("Режим: %s\n", rx_mode ? "RX" : "TX");
    printf("RX частота: %.3f МГц\n", rx_freq / 1e6);
    if (tx_mode){
        printf("TX частота: %.3f МГц\n", tx_freq / 1e6);
    }

    SoapySDRDevice *sdr = NULL;
    SoapySDRStream *rxStream = NULL;
    SoapySDRStream *txStream = NULL;
    int16_t *rx_buffer = NULL;
    int16_t *tx_buff = NULL;
    FILE *file = NULL;
    int exit_code = EXIT_FAILURE;
    size_t rx_mtu = 0;
    size_t tx_mtu = 0;
    size_t channels[] = {0};
    long long last_time = 0;
    size_t buffers_read = 0;
    const long timeoutUs = TIMEOUT_US;
    int *bits = NULL;
    Complex *symbols = NULL;
    Complex *tx_signal = NULL;
    size_t L = 64;
    size_t symbol_count = 0;
    size_t bit_count = 0;
    size_t sample_count = 0;

    // Открытие выбранного SDR
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", uri);
    // SoapySDRKwargs_set(&args, "direct", "1");
    // SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    // SoapySDRKwargs_set(&args, "loopback", "0");

    sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL){
        fprintf(stderr, "Ошибка: не удалось открыть SDR: %s\n", uri);
        return EXIT_FAILURE;
    }

    printf("Устройство успешно открыто\n");

    // Настройка RX и TX
    int result = SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SAMPLE_RATE);

    if (result != 0){
        fprintf(stderr, "Ошибка настройки RX sample rate: %d\n", result);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    result = SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, SAMPLE_RATE);

    if (result != 0){
        fprintf(stderr, "Ошибка настройки TX sample rate: %d\n", result);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    result = SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, rx_freq, NULL);

    if (result != 0){
        fprintf(stderr, "Ошибка настройки RX frequency: %d\n", result);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    result = SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, tx_freq, NULL);

    if (result != 0){
        fprintf(stderr, "Ошибка настройки TX frequency: %d\n", result);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, RX_GAIN); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, TX_GAIN); // Усиление передатчика

    // Создание и запуск RX-потока
    rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, 1, NULL);
    if (rxStream == NULL){
        fprintf(stderr, "Ошибка создания RX потока: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    if (SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0) != 0){
        fprintf(stderr, "Ошибка активации RX потока: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream); // MTU (Maximum Transmission Unit),получение размера буфера
    if (rx_mtu == 0){
        fprintf(stderr, "Ошибка: RX MTU равен нулю\n");
        return EXIT_FAILURE;
    }
    printf("RX MTU: %zu сэмплов\n", rx_mtu);

    rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));
    if (rx_buffer == NULL){
        fprintf(stderr, "Ошибка выделения памяти для RX буфера\n");
        return EXIT_FAILURE;
    }

    // Создание TX-потока только в режиме передачи
    if (tx_mode){
        txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, 1, NULL);
        if (txStream == NULL){
            fprintf(stderr, "Ошибка создания TX потока: %s\n", SoapySDRDevice_lastError());
            return EXIT_FAILURE;
        }
        if (SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0) != 0){
            fprintf(stderr, "Ошибка активации TX потока: %s\n", SoapySDRDevice_lastError());
            return EXIT_FAILURE;
        }

        tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream); // MTU (Maximum Transmission Unit),
        if (tx_mtu == 0){
            fprintf(stderr, "Ошибка: TX MTU равен нулю\n");
            return EXIT_FAILURE;
        }
        printf("TX MTU: %zu сэмплов\n", tx_mtu);

        tx_buff = (int16_t *)malloc(2 * tx_mtu * sizeof(int16_t));
        if (tx_buff == NULL){
            fprintf(stderr, "Ошибка выделения памяти для TX буфера\n");
            return EXIT_FAILURE;
        }

        symbol_count = tx_mtu / L;
        bit_count = symbol_count;
        sample_count = symbol_count * L;

        bits = (int *)malloc(bit_count * sizeof(int));
        symbols = (Complex *)malloc(symbol_count * sizeof(Complex));
        tx_signal = (Complex *)malloc(sample_count * sizeof(Complex));

        if (!bits || !symbols || !tx_signal){
            fprintf(stderr, "Ошибка выделения памяти mapper\n");
            return EXIT_FAILURE;
        }
    }
    // Обработка сигналов остановки
    signal(SIGINT, stop_program);
    signal(SIGTERM, stop_program);

    if (rx_mode){
        char filename[64];
        snprintf(filename, sizeof(filename), "%s_rxdata.pcm", sdr_id);

        file = fopen(filename, "wb");

        if (file == NULL){
            perror("Не удалось открыть файл");
            return EXIT_FAILURE;
        }

        printf("Приём записывается в %s\n", filename);
    }

    printf("Работа началась\n");

    while (running && buffers_read < ITERATION_COUNT){
        void *rx_buffs[] = {rx_buffer};
        int flags = 0;
        long long timeNs = 0;
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr > 0){
            if (rx_mode && file != NULL){
                size_t written = fwrite(rx_buffer, 2 * sizeof(int16_t), (size_t)sr, file);
                if (written != (size_t)sr){
                    fprintf(stderr, "Ошибка записи в файл\n");
                    return EXIT_FAILURE;
                }
            }

            if (flags & SOAPY_SDR_HAS_TIME){
                printf("Буфер %zu/%d | Сэмплы: %d | Флаги: %d | Время: %lld нс | Разница: %lld нс\n", buffers_read + 1, ITERATION_COUNT, sr, flags, timeNs, timeNs - last_time);
                last_time = timeNs;
            }
            else{
                printf("Буфер %zu/%d | Сэмплы: %d | Флаги: %d | Время: н/д\n", buffers_read + 1, ITERATION_COUNT, sr, flags);
            }

            if (tx_mode){
                
                    /*for (size_t i = 0; i < tx_mtu; i++)
                    {
                        if (i < 200)
                        {
                            tx_buff[2 * i] = 2000 << 4;
                            tx_buff[2 * i + 1] = 2000 << 4;
                        }
                        else
                        {
                            tx_buff[2 * i] = 0;
                            tx_buff[2 * i + 1] = 0;
                        }
                    }*/

                    generate_bits(bits, bit_count);
                    bpsk(bits, symbols, bit_count);
                    rectangular(symbols, tx_signal, symbol_count, L);

                    for (size_t i = 0; i < 2 * tx_mtu; i++)
                        tx_buff[i] = 0;

                    for (size_t i = 0; i < sample_count; i++){
                        tx_buff[2 * i] = (int16_t)(tx_signal[i].I * 20000.0f);
                        tx_buff[2 * i + 1] = (int16_t)(tx_signal[i].Q * 20000.0f);
                    }

                    void *tx_buffs[] = {tx_buff};
                    int tx_flags = 0;
                    long long tx_time = 0;

                    int st = SoapySDRDevice_writeStream(sdr, txStream,(const void *const *)tx_buffs, tx_mtu, &tx_flags, tx_time, timeoutUs);

                    if (st < 0 || (size_t)st != tx_mtu){
                        fprintf(stderr, "Ошибка передачи (TX Failed): %d\n", st);
                    }
            }

            buffers_read++;
        }
    }

    printf("Работа успешно завершена.\n");

    if (file != NULL)
        fclose(file);

    free(rx_buffer);
    free(tx_buff);
    free(bits);
    free(symbols);
    free(tx_signal);

    if (sdr != NULL){
        if (rxStream != NULL){
            SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
            SoapySDRDevice_closeStream(sdr, rxStream);
        }

        if (txStream != NULL){
            SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
            SoapySDRDevice_closeStream(sdr, txStream);
        }

        SoapySDRDevice_unmake(sdr);
    }

    return exit_code;
}
