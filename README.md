# Remote Console Application (Удаленная консоль)

Программа для удаленного выполнения команд на Windows 11, реализующая функционал, похожий на SSH, но упрощенный.

## Описание

Приложение работает в двух режимах:
- **Сервер** (`-s`) - принимает соединения и выполняет команды
- **Клиент** (`-c`) - подключается к серверу и отправляет команды

## Компиляция

### Способ 1: Использование Makefile (MinGW)

```bash
# Компиляция основного приложения
make

# Компиляция C++ примера с wrapper
make cpp

# Очистка артефактов сборки
make clean
```

### Способ 2: Использование build.bat

Просто запустите скрипт сборки:
```bash
build.bat
```

Скрипт автоматически определит доступный компилятор (MinGW или MSVC).

### Способ 3: Ручная компиляция

#### MinGW-w64:
```bash
# Основное приложение (C)
gcc -Wall -O2 -o my.exe my.c -lws2_32 -ladvapi32

# C++ wrapper пример
g++ -Wall -O2 -std=c++11 -o process_wrapper_example.exe process_wrapper_example.cpp process_wrapper.cpp -lws2_32
```

#### MSVC (из Developer Command Prompt):
```bash
# Основное приложение (C)
cl /O2 /Fe:my.exe my.c ws2_32.lib advapi32.lib

# C++ wrapper пример
cl /O2 /EHsc /Fe:process_wrapper_example.exe process_wrapper_example.cpp process_wrapper.cpp ws2_32.lib
```

## Использование

### Базовое использование

#### 1. Запуск сервера (локальный режим)

На машине-сервере:
```bash
my.exe -s
```

Сервер начнет прослушивать порт 9999 и ожидать подключения клиента.

#### 2. Запуск клиента

На машине-клиенте (или той же машине для тестирования):
```bash
# Подключение к локальному серверу
my.exe -c

# Подключение к удаленному серверу
my.exe -c 192.168.1.100
```

После подключения вы можете вводить команды, которые будут выполняться на сервере:
```
C:\Users\User> dir
C:\Users\User> ipconfig
C:\Users\User> echo Hello from remote console!
```

Для выхода введите: `exit`

### Режим Windows Service

#### Установка службы

```bash
# Установить службу (требуются права администратора)
my.exe -install
```

#### Запуск службы

```bash
# Запустить службу
my.exe -start
```

Или через Services Manager:
1. Нажмите Win+R
2. Введите `services.msc`
3. Найдите "Remote Console Service"
4. Щелкните правой кнопкой → Start

#### Остановка службы

```bash
# Остановить службу
my.exe -stop
```

#### Удаление службы

```bash
# Удалить службу
my.exe -uninstall
```

### Использование C++ Wrapper

Класс `ProcessWrapper` предоставляет удобный объектно-ориентированный интерфейс для работы с процессами:

```cpp
#include "process_wrapper.h"

int main() {
    ProcessWrapper proc;
    
    // Запуск процесса
    if (proc.Start("cmd.exe", true)) {
        // Отправка команды
        proc.WriteToStdin("dir\r\n");
        
        // Чтение вывода
        Sleep(500);
        std::string output = proc.ReadFromStdout();
        std::cout << output << std::endl;
        
        // Проверка, работает ли процесс
        if (proc.IsRunning()) {
            proc.Terminate();
        }
    }
    
    return 0;
}
```

Скомпилируйте пример:
```bash
make cpp
# или
build.bat
```

Запустите:
```bash
process_wrapper_example.exe
```

## Настройка сети (DevOps - этап 4)

### Настройка для локального тестирования (127.0.0.1)

Никаких дополнительных настроек не требуется. Просто запустите сервер и клиент на одной машине.

### Настройка для удаленного доступа

#### 1. Проверка сетевых настроек

**На сервере:**
```bash
# Проверить IP-адрес
ipconfig

# Проверить сетевое подключение
ping <IP-адрес-клиента>
```

#### 2. Настройка брандмауэра Windows

**На машине-сервере:**

1. Откройте "Windows Defender Firewall with Advanced Security"
2. Выберите "Inbound Rules" → "New Rule..."
3. Тип правила: Port
4. Протокол: TCP, Порт: 9999
5. Действие: Allow the connection
6. Профили: выберите соответствующие (Domain, Private, Public)
7. Имя: "Remote Console Server"

**Или через командную строку (требуются права администратора):**
```bash
netsh advfirewall firewall add rule name="Remote Console Server" dir=in action=allow protocol=TCP localport=9999
```

#### 3. Настройка виртуальной машины (если используется)

Если вы тестируете в виртуальной среде (VirtualBox, VMware, Hyper-V):

**VirtualBox:**
- Настройки → Сеть → Адаптер 1
- Тип подключения: "Bridged Adapter" (для прямого доступа к сети)
- Или: "Host-only Adapter" (только для связи между хостом и VM)

**VMware:**
- VM Settings → Network Adapter
- Network connection: Bridged (для доступа к локальной сети)

**Hyper-V:**
- VM Settings → Network Adapter
- Virtual Switch: выберите внешний коммутатор

#### 4. Проверка подключения

**На клиенте:**
```bash
# Проверить доступность сервера
ping <IP-адрес-сервера>

# Проверить открытость порта
telnet <IP-адрес-сервера> 9999
```

## Структура проекта

```
remote-console/
│
├── my.c                          # Основной файл программы (C)
├── process_wrapper.h             # Заголовочный файл C++ wrapper
├── process_wrapper.cpp           # Реализация C++ wrapper
├── process_wrapper_example.cpp   # Пример использования wrapper
├── Makefile                      # Файл сборки для make
├── build.bat                     # Скрипт сборки для Windows
└── README.md                     # Этот файл
```
