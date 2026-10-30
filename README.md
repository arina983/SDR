# SDR - цифровой передатчик на PlutoSDR

Передача и приём сигналов через два ADALM-Pluto с помощью SoapySDR.

## Что реализовано

Цифровая цепочка передатчика:

1. **Генератор битов** - случайная последовательность 0/1  
2. **Модуляция** - BPSK и QPSK (комплексные символы I/Q)  
3. **Формирование импульсов** - прямоугольные импульсы (каждый символ растягивается на `L` отсчётов)  
4. **Передача / приём** - SoapySDR + PlutoSDR, запись RX в PCM-файл  

Частоты RX/TX скрещены между двумя устройствами, чтобы они могли работать в паре.

## Структура

```bash
SDR/
├── my_project/
│   ├── main.cpp          # SoapySDR: RX/TX, вызов передатчика
│   ├── transmitter.h     # generate_bits, bpsk, qpsk, upsample, rectangular
│   ├── CMakeLists.txt
│   └── graph.py          # просмотр принятого сигнала (I/Q)
├── Reports/              # отчёты и графики
└── README.md
```

## Требования

- Linux  
- SoapySDR + драйвер PlutoSDR  
- CMake, gcc  
- Два ADALM-Pluto 
- Python 3 + numpy, matplotlib

## Сборка

```bash
cd my_project
mkdir -p build && cd build
cmake ..
make 
```

## Пример запуска

### Терминал 1 - приёмник
```bash
sudo ./main sdr2 rx usb:1.3.5
```

### Терминал 2 - передатчик
```bash
sudo ./main sdr1 tx usb:1.4.5
```

### Просмотр сигнала
```bash
cd my_project
python3 graph.py
```
