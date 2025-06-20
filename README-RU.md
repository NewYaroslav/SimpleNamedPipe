# SimpleNamedPipe
![Logo](docs/logo-480x320.png)

SimpleNamedPipe — легковесная библиотека C++ для создания и управления асинхронными серверами именованных каналов в Windows.

Проект распространяется под лицензией MIT. Полный текст лицензии см. в файле [LICENSE](LICENSE).


## Основные возможности

- асинхронная обработка клиентов через IO Completion Port;
- работа либо в отдельном потоке, либо блокирующе в текущем (параметр `start()`);
- поддержка до 256 одновременных клиентов;
- очередь отправки с ограничением размера и количества сообщений;
- уведомления о событиях через колбэки или класс `ServerEventHandler`;
- лёгкий клиент для MQL5 с опциональными глобальными обратными вызовами.
- клиент MQL5 выполняет чтение/запись синхронно, обновление через метод `update()` например в таймере.

## Быстрый старт

### Простейший сервер

```cpp
#include "SimpleNamedPipe/NamedPipeServer.hpp"
using namespace SimpleNamedPipe;

int main() {
    NamedPipeServer server({"ExamplePipe"});

    server.on_message = [&server](int id, const std::string& msg) {
        server.send_to(id, "Echo: " + msg);
    };

    server.start(); // запускает сервер в отдельном потоке
    std::cin.get(); // ожидание Enter
    server.stop();
}
```

### Простейший клиент MQL5

```mql5
#include <SimpleNamedPipe\NamedPipeClient.mqh>

NamedPipeClient pipe;

int OnInit() {
    pipe.open("ExamplePipe");
    EventSetMillisecondTimer(10);
    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
    EventKillTimer();
}

void OnTimer() {
    pipe.update();
    if(pipe.connected())
        pipe.write("ping");
}
```

## Установка

1. Установите CMake и компилятор (Visual Studio или MinGW).
2. Для сборки библиотеки и примеров выполните `build_all.bat` (или `build_all_mingw.bat` для MinGW).
3. Скрипт `install_mql5.bat` копирует файлы из каталога `MQL5` во все найденные терминалы MetaTrader 5.

## Примеры

Исходники примеров расположены в каталоге `examples`.
- `callback_example.cpp` демонстрирует работу сервера с отдельными коллбэками (`on_connected`, `on_message` и т.д.).
- `universal_event_example.cpp` показывает аналогичную логику, но используя единый обработчик `on_event`.

## Полезные ссылки

- [Нamed Pipe Server using Overlapped I/O](https://learn.microsoft.com/ru-ru/windows/win32/ipc/named-pipe-server-using-overlapped-i-o)
- [Winsock2 Advanced Named Pipe](https://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancednamedpipe15a.html)
- [Основы разработки программ для операционных систем семейств Windows и Linux](https://repo.ssau.ru/bitstream/Uchebnye-izdaniya/Operacionnye-sistemy-Osnovy-razrabotki-programm-dlya-operacionnyh-sistem-semeistv-Windows-i-Linux-109472/1/978-5-7883-2035-9_%202024.pdf)

## Лицензия

SimpleNamedPipe распространяется по лицензии MIT. Подробнее см. файл [LICENSE](LICENSE).

