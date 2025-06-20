# SimpleNamedPipe
SimpleNamedPipe is a lightweight C++ library for creating and managing asynchronous named pipe servers on Windows.

## Полезные ссылки

- [Нamed Pipe Server using Overlapped I/O](https://learn.microsoft.com/ru-ru/windows/win32/ipc/named-pipe-server-using-overlapped-i-o)
- [Winsock2 Advanced Named Pipe](https://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancednamedpipe15a.html)
- [Основы разработки программ для операционных систем семейств Windows и Linux](https://repo.ssau.ru/bitstream/Uchebnye-izdaniya/Operacionnye-sistemy-Osnovy-razrabotki-programm-dlya-operacionnyh-sistem-semeistv-Windows-i-Linux-109472/1/978-5-7883-2035-9_%202024.pdf)

## Примеры

Исходники примеров расположены в каталоге `examples`.
- `callback_example.cpp` демонстрирует работу сервера с отдельными коллбэками (`on_connected`, `on_message` и т.д.).
- `universal_event_example.cpp` показывает аналогичную логику, но используя единый обработчик `on_event`.
