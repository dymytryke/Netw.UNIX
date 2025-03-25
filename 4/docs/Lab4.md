# Lab 4

## Завдання на роботу
Розробити однопотоковий сервер, який виконує наступне:

1. Сервер підтримує аргументи командного рядка, визначені в лабораторній роботі No3. Та-
кож сервер підтримує аргумент командного рядка, який визначає максимальну кількість
клієнтів, з якими сервер може одночасно працювати. Сервер не приймає нові TCP з’єдна-
ння після досягнення цього значення.
2. Сервер працює з клієнтами відповідно до користувальницького протоколу, визначеного в
лабораторній роботі No3.
3. Сервер   дозволяє   одночасно   працювати   з   кількома   клієнтами   за   допомогою   мультипле-
ксування   введення-виведення.   Сервер   послуговується   системними   викликами  select()
або poll() для мультиплексування введення-виведення.
4. Кількість   даних,   які   сервер   зчитує   або   відправляє   одному   клієнту   під   час   виконання введення-виведення   з  ним,  треба   обмежити.  Ця   кількість  задається   в  коді   сервера   кон-стантою, яка може мати значення 1 байт та більше. Тобто, якщо сервер отримав інформа-
цію від ядра про можливість виконати введення або виведення для якогось дескриптора
файлу сокета, тоді серверу дозволено відправити або отримати даних розміром не більше
вказаної   константи.   Це   обмеження   дає   змогу   майже   порівну   розподіляти   час   роботи
сервера   для   кожного   клієнта,   який   потребує   комунікації.   Також   невеликі   значення   цієї
константи   дозволяють   імітувати   проблеми   з   мережею   та   частково   імітувати   різну
поведінку клієнтів.

Сервер не має завершувати своє виконання у випадку виникнення несистемної помилки.

Рекомендації для сервера такі самі, які були дані в лабораторній роботі No3.

## Код програми

[client](../src/client.c)

[server](../src/multiplexing_server.c)

[protocol](../src/protocol.h)

[utils](../src/utils.h)

## Опис програми

Програма представляє собою однопотоковий мультиплексований сервер для передачі файлів. Розглянемо детальніше особливості його реалізації:

### Архітектурні рішення

Сервер використовує **мультиплексування вводу-виводу** замість створення окремих потоків/процесів для кожного клієнта. Це дозволяє одному потоку виконання обробляти багато з'єднань одночасно. У реалізації використано системний виклик `poll()`, який відстежує стан усіх сокетів.

### Основні компоненти сервера:

1. **Структура ClientContext** - зберігає стан взаємодії з кожним клієнтом:
   - поточний стан обміну (читання запиту, відправка відповіді тощо)
   - буфер для введення-виведення
   - стан передачі файлу і метадані
   - інформація про активність клієнта

2. **Кінцевий автомат станів клієнта** реалізовано через `enum ClientState`:
   - `STATE_READ_REQUEST` - читання запиту на файл
   - `STATE_SEND_RESPONSE` - відправка відповіді з метаінформацією
   - `STATE_WAIT_DECISION` - очікування рішення клієнта
   - `STATE_SEND_FILE` - передача файлу
   - `STATE_DONE` - завершення обробки

### Особливості реалізації

#### 1. Обмеження на розмір операцій вводу-виводу

Ключовою особливістю є константа `MAX_IO_CHUNK` (512 байт), яка обмежує розмір даних для операцій читання/запису за одну ітерацію. Це забезпечує:

- **Справедливий розподіл ресурсів** - сервер не "зависає" на обробці великого файлу для одного клієнта
- **Імітацію мережевих умов** - дозволяє тестувати роботу з повільними з'єднаннями
- **Рівномірне обслуговування клієнтів** - кожен клієнт отримує обмежену порцію уваги сервера

#### 2. Неблокуючий ввід-вивід

Усі сокети налаштовуються в неблокуючий режим через функцію `set_nonblocking()`. Це дозволяє функціям читання/запису повертати керування негайно, навіть якщо дані недоступні.

#### 3. Обмеження кількості клієнтів

Сервер підтримує параметр `max_clients`, який обмежує кількість одночасних підключень. При досягненні ліміту нові з'єднання не приймаються.

#### 4. Системний виклик poll()

Центральна частина мультиплексування - використання `poll()` для моніторингу кількох файлових дескрипторів. Сервер відстежує готовність дескрипторів до операцій читання або запису, а також помилки.

#### 5. Поетапна обробка протоколу

Обробка протоколу розбита на окремі функції згідно зі станами клієнта:
- `process_read_request()` - обробка вхідних запитів на файл
- `process_send_response()` - відправка метаінформації про файл
- `process_wait_decision()` - очікування рішення клієнта
- `process_send_file()` - відправка вмісту файлу частинами

### Алгоритм роботи сервера

1. **Ініціалізація**: 
   - Розбір параметрів командного рядка
   - Створення сокету та прив'язка до адреси і порту
   - Встановлення сокету в режим прослуховування
   - Налаштування неблокуючого режиму

2. **Основний цикл**:
   - Очікування подій на всіх дескрипторах через `poll()`
   - Обробка нових підключень (якщо не досягнуто ліміту)
   - Обробка готових для вводу-виводу дескрипторів
   - Обробка помилок і закриття з'єднань
   - Оновлення структур даних і звільнення ресурсів для завершених з'єднань

3. **Обробка клієнтів**:
   - Прийом запиту на файл частинами (до `MAX_IO_CHUNK` за раз)
   - Перевірка існування файлу і підготовка відповіді
   - Відправка відповіді з метаінформацією (частинами)
   - Очікування підтвердження клієнта
   - Передача вмісту файлу невеликими блоками

Як видно з прикладів використання, сервер здатен одночасно обслуговувати декілька клієнтів, обмежуючи їх максимальну кількість і розподіляючи ресурси процесора та мережі між ними. Це дозволяє ефективно передавати файли навіть при роботі з декількома клієнтами одночасно.

## Приклади використання програми

### Запуск сервера

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/4/bin$ ./multiplexing_server 127.0.0.1 12345 ../test_files/ 3
Multiplexing server started at 127.0.0.1:12345, serving files from '../test_files/', max clients: 3
```

### Робота з одним клієнтом

**Клієнт**
```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/4/bin$ ./client 127.0.0.1 12345 baseball-cap-collection.zip 1200000000
Connecting to server at 127.0.0.1:12345
Connected to server
Sending file request for: baseball-cap-collection.zip
Received response: protocol version 1, message type 2, status 0, file size 13142141
Sending ready to receive message
Receiving file content (13142141 bytes)...
Received: 13142141/13142141 bytes (100.0%)
File transfer complete
```

**Сервер**
```bash
Accepted connection from 127.0.0.1:47430 (fd=4)
Received file request from fd=4 for: baseball-cap-collection.zip
Sending file response to fd=4: status=0, file_size=13142141
Client fd=4 is ready to receive the file
Client fd=4: Sent 13142141/13142141 bytes (100%)
File transfer complete for client fd=4
Closed connection for client fd=4
```

### Робота з декількома клієнтами

**Скрипт для одночасного запуску 5-ти клієнтів**
```bash
#!/bin/bash

./client 127.0.0.1 12345 AnyDesk.exe 100000000000000 &
./client 127.0.0.1 12345 FigmaSetup.exe 100000000000000 &
./client 127.0.0.1 12345 baseball-cap-collection.zip 100000000000000 &
./client 127.0.0.1 12345 junos-vsrx-12.1X47-D10.4-domestic.ova 100000000000000 &
./client 127.0.0.1 12345 n3467.pdf 1000 &
```

**Клієнт**
```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/4/bin$ Received response: protocol version 1, message type 2, status 0, file size 13142141
Sending ready to receive message
Received: 512/5634880 bytes (0.0%)Receiving file content (13142141 bytes)...
Received response: protocol version 1, message type 2, status 0, file size 118362136
Sending ready to receive message
Received: 8704/5634880 bytes (0.2%)Receiving file content (118362136 bytes)...
Received: 5634880/5634880 bytes (100.0%)
File transfer complete
Received: 5635584/13142141 bytes (42.9%)Received response: protocol version 1, message type 2, status 0, file size 238397440
Sending ready to receive message
Received: 5636096/118362136 bytes (4.8%)Receiving file content (238397440 bytes)...
Received: 13142141/13142141 bytes (100.0%)
File transfer complete
Received: 13143040/118362136 bytes (11.1%)Received response: protocol version 1, message type 2, status 0, file size 4309251
File size (4309251 bytes) exceeds maximum allowed size (1000 bytes)
Received: 118362136/118362136 bytes (100.0%)
File transfer complete
Received: 238397440/238397440 bytes (100.0%)
File transfer complete
```

**Сервер**
```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/4/bin$ stdbuf -oL ./multiplexing_server 127.0.0.1 12345 ../test_files/ 3 | ts
Mar 25 08:49:48 Multiplexing server started at 127.0.0.1:12345, serving files from '../test_files/', max clients: 3
Mar 25 08:49:52 Accepted connection from 127.0.0.1:43086 (fd=4)
Mar 25 08:49:52 Accepted connection from 127.0.0.1:43082 (fd=5)
Mar 25 08:49:52 Received file request from fd=4 for: AnyDesk.exe
Mar 25 08:49:52 Sending file response to fd=4: status=0, file_size=5634880
Mar 25 08:49:52 Accepted connection from 127.0.0.1:43084 (fd=6)
Mar 25 08:49:52 Received file request from fd=5 for: baseball-cap-collection.zip
Mar 25 08:49:52 Sending file response to fd=5: status=0, file_size=13142141
Mar 25 08:49:52 Client fd=4 is ready to receive the file
Mar 25 08:49:52 Received file request from fd=6 for: FigmaSetup.exe
Mar 25 08:49:52 Sending file response to fd=6: status=0, file_size=118362136
Mar 25 08:49:52 Client fd=5 is ready to receive the file
Mar 25 08:49:52 Client fd=6 is ready to receive the file
Client fd=4: Sent 5634880/5634880 bytes (100%)
Mar 25 08:49:56 File transfer complete for client fd=4
Mar 25 08:49:56 Closed connection for client fd=4
Mar 25 08:49:56 Accepted connection from 127.0.0.1:43104 (fd=4)
Mar 25 08:49:56 Received file request from fd=4 for: junos-vsrx-12.1X47-D10.4-domestic.ova
Mar 25 08:49:56 Sending file response to fd=4: status=0, file_size=238397440
Mar 25 08:49:56 Client fd=4 is ready to receive the file
Client fd=5: Sent 13142141/13142141 bytes (100%)
Mar 25 08:50:03 File transfer complete for client fd=5
Mar 25 08:50:03 Closed connection for client fd=5
Mar 25 08:50:03 Accepted connection from 127.0.0.1:43112 (fd=5)
Mar 25 08:50:03 Received file request from fd=5 for: n3467.pdf
Mar 25 08:50:03 Sending file response to fd=5: status=0, file_size=4309251
Mar 25 08:50:03 Client fd=5 refused to receive the file (too large)
Mar 25 08:50:03 Closed connection for client fd=5
Client fd=6: Sent 118362136/118362136 bytes (100%)
Mar 25 08:50:58 File transfer complete for client fd=6
Mar 25 08:50:58 Closed connection for client fd=6
Client fd=4: Sent 238397440/238397440 bytes (100%)
Mar 25 08:51:25 File transfer complete for client fd=4
Mar 25 08:51:25 Closed connection for client fd=4
```

**Demo:**

[Video](./multiple_clients_demo.mp4)

<video src='./multiple_clients_demo.mp4' width=1080/>