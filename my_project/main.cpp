#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

int main() {
    //1. Инициализация устройства
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", "usb:");
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args); //указатель на открытое устройство
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL) {
        printf("Ошибка: не удалось открыть устройство\n");
        return EXIT_FAILURE;
    }
    printf("Устройство успешно открыто\n");

    //2. Настройка параметров 
    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq , NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq , NULL);

    // Инициализация количества каналов RX\\TX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    // Настройки усилителей на RX\\TX
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 10.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, -90.0);// Усиление передатчика
    //3. Создание потоков 
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    // Формирование потоков для передачи и приема сэмплов
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

    // 4. Буферы
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    printf("RX MTU: %zu сэмплов\n", rx_mtu);
    printf("TX MTU: %zu сэмплов\n", tx_mtu);

    int16_t tx_buff[2*tx_mtu];
    int16_t rx_buffer[2*rx_mtu];


    //5. Основной цикл
    const long timeoutUs = 400000;
    long long last_time = 0;
    size_t iteration_count = 40; // количество итераций

    FILE *file = fopen("rxdata.pcm", "a+");

    if (file == NULL) {
        perror("Не удалось открыть файл");
        return EXIT_FAILURE;
    }

    printf("\nНачинаем приём...\n");

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        void *rx_buffs[] = {rx_buffer};
        int flags = 0;
        long long timeNs = 0;

        // Приём
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr > 0) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, file);
        }
        printf("Buffer %2zu | Samples: %4d | Flags: %d | Time: %lld | Diff: %lld нс\n",
               buffers_read, sr, flags, timeNs, timeNs - last_time);

        last_time = timeNs;

        // Один раз отправляем тестовый сигнал (на 3-й итерации)
        if (buffers_read == 2)
        {
            printf("Отправляем тестовый сигнал...\n");

            // Заполняем буфер тестовыми данными (постоянный сигнал)
            for (int i = 2; i < 2 * tx_mtu; i += 2) {
                // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
                tx_buff[i] = 2000 << 4;   // I
                tx_buff[i+1] = 2000 << 4;   // Q
            }

            // Время передачи = текущее время + 4 мс
            long long tx_time = timeNs + (4 * 1000 * 1000);

            // Записываем timestamp в специальный формат в начало буфера
            // (так требует прошивка с timestamp)
            for (size_t i = 0; i < 2; i++) {
                tx_buff[0 + i]  = 0xffff;
                tx_buff[10 + i] = 0xffff;
            }

            for (size_t i = 0; i < 8; i++) {
                uint8_t byte = (tx_time >> (i * 8)) & 0xff;
                tx_buff[2 + i] = byte << 4;
            }

            // Отправка
            void *tx_buffs[] = {tx_buff};
            if( (buffers_read %2 == 0) ){
                printf("buffers_read: %d\n", buffers_read);
                flags = SOAPY_SDR_HAS_TIME;
                int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
            if ((size_t)st != tx_mtu) {
                printf("TX Failed: %i\n", st);
            }
        }
    }
}
    fclose(file);

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);
    SoapySDRDevice_unmake(sdr);

    printf("\nГотово.\n");
    return EXIT_SUCCESS;
}
