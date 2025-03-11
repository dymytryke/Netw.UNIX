# Lab 3

## Завдання на роботу

### Розробити клієнт та сервер, які виконують наступне:

1. Клієнт підтримує такі аргументи командного рядка: адреса сервера, порт сервера, ім’я
файлу, максимальний розмір файлу.
2. Сервер підтримує такі аргументи командного рядка: адреса сервера, порт сервера, шляхове ім’я директорії.
3. Клієнт та сервер використовують транспортний протокол TCP для мережевого з’єднання.
4. Клієнт відправляє запит серверу з ім’ям файлу, яке вказано в аргументі його командного
рядка. Сервер отримує запит від клієнта, шукає звичайний файл з вказаним ім’ям у ди-
ректорії, шляхове ім’я якої вказано в аргументі його командного рядка, та відправляє клі-
єнту вміст файлу. Клієнт записує отриманий вміст у звичайний файл.

### Протокол рівня застосунку має наступні характеристики:

1. Протокол має версію. Якщо версії протоколів, які використовують клієнт та сервер не збі-
гаються, тоді з’єднання між клієнтом та сервером треба завершити.
2. Ім’я файлу в запиті клієнта повинно складатися з символів ASCII, які дозволені для імені
файлу в наявній ФС (літери, цифри, знаки пунктуації і т. ін.). Максимальна довжина імені
файлу обмежена. Клієнт може надіслати які-завгодно дані замість імені файлу, сервер має
перевірити коректність цих даних. Клієнт має надіслати ім’я файлу, а не шляхове ім’я.
3. Сервер відправляє клієнту інформацію чи було знайдено файл з вказаним ім’ям та його
розмір, у випадку помилки серер відправляє клієнту номер помилки та завершує з’єднан-
ня. Розмір файлу не перевищує значення (2^64 - 1) байт (тобто є 64 біт для значення розміру
файлу в заголовку). У випадку помилки клієнт виводить інформацію про помилку та
завершує з’єднання. Якщо розмір файлу перевищує вказаний максимальний розмір файлу
в аргументі командного рядка клієнта, тоді клієнт відправляє серверу повідомлення про
відмову отримувати вміст файлу, інакше клієнт відправляє серверу повідомлення про го-
товність отримувати вміст файлу.
4. Якщо сервер отримав повідомлення від клієнта про готовність отримувати вміст файлу,
тоді він відправляє вміст файлу частинами (тобто може потребуватися кілька викликів
відповідного системного виклику для відправлення вмісту файлу). Розмір частини ви-
значається   в   сервері   константним   значенням.   Відправивши   весь   вміст   файлу,   сервер
завершує з’єднання. Отримавши весь вміст файлу клієнт завершує з’єднання.

### Треба реалізувати наступні реалізації серверів:

1. Ітеративний сервер, який опрацьовує запити одного клієнта повністю, перед тим, як поча-
ти опрацьовувати запити наступного клієнта.
2. Паралельний сервер, який створює нові процеси для опрацювання запитів нових клієнтів.
Сервер має обмеження на максимальну кількість дочірніх процесів, які опрацьовують
запити   клієнтів.   Ця   максимальна   кількість   вказується   в   аргументі   командного   рядка
сервера. Сервер не приймає нові TCP з’єднання від клієнтів після досягнення цієї кі-
лькості.
3. Паралельний сервер, який заздалегідь створює нові процеси для опрацювання запитів
клієнтів, кожний дочірній процес є ітеративним сервером, як у першому пункті. Кількість
дочірніх процесів, які має створити сервер, вказується в аргументі командного рядка
сервера.

### Tips:

- Сервер не має завершувати своє виконання у випадку виникнення несистемної помилки.

- Для перевірки коректності роботи програм рекомендується виводити повідомлення про дії в
програмах (адреси, номери портів, вміст заголовків).

## Код програми

[protocol.h](../src/protocol.h)

[utils.h](../src/utils.h)

[client.c](../src/client.c)

[iterative_server.c](../src/iterative_server.c)

[parallel_server.c](../src/parallel_server.c)

[preforked_server.c](../src/preforked_server.c)

## Опис програми

### 1. Відмінності між серверами

Кожна реалізація сервера має свої переваги та недоліки, їх ефективність залежить від характеру навантаження:

- **Ітеративний сервер** – обробляє лише одного клієнта за раз. Він простий у реалізації (відсутнє розділення на процеси), займає мінімум ресурсів і виключає накладні витрати на створення процесів. Однак при одночасних запитах інші клієнти чекають завершення поточної передачі, що значно знижує продуктивність у сценаріях з багатьма клієнтами. Такий сервер підходить для одиночних запитів або відлагодження, але погано масштабується під навантаженням.

- **Паралельний сервер з fork** – підтримує багатозадачність, створюючи окремий процес на кожного клієнта. Це дозволяє обслуговувати відразу декілька клієнтів (до заданого ліміту), ефективно використовуючи багатоядерність CPU. Паралельний підхід значно швидший для багатокористувацької роботи, оскільки, наприклад, один клієнт може отримувати великий файл, тоді як інший у той самий час вже підключений і теж отримує свій файл. Недоліком є витрати на створення і завершення процесів при кожному з’єднанні: fork копіює процес, що потребує часу і пам’яті. При великій кількості коротких запитів ці витрати можуть скласти помітну долю. Введення обмеження на максимальну кількість одночасних процесів запобігає вичерпуванню ресурсів, але якщо ліміт занадто малий, то при піковому навантаженні зайві клієнти чекатимуть, хоча й паралельно обслуговуючи групу клієнтів.

- **Префоркований сервер** – поєднує паралельність з відсутністю накладних витрат на кожне нове підключення. Наперед створений пул процесів дозволяє відразу приймати підключення, що особливо ефективно при високому і постійному навантаженні (наприклад, сервер, до якого одночасно звертаються десятки клієнтів). Цей сервер має стабільний рівень використання ресурсів: фіксована кількість процесів постійно зайнята очікуванням або обробкою, і не витрачається час на запуск/завершення процесу для кожного клієнта. У порівнянні з динамічним fork-підходом, префорк дає виграш у продуктивності при великому потоці запитів. Якщо ж клієнтські запити надходять рідко, префоркований сервер все одно тримає запущені процеси (деякі можуть простоювати), тобто може використовувати більше пам’яті, ніж паралельний сервер з fork, який у стані спокою має лише один процес. Також префоркована модель складніша в реалізації, адже потребує механізмів керування пулом (коректне завершення дітей тощо), але в даній лабораторній роботі це реалізовано через пересилання сигналів SIGTERM усім дочірнім процесам при зупинці сервера.

**Отже,** Ітеративний сервер мінімізує накладні витрати, але не масштабований для багатокористувацького режиму. Паралельний (fork-на-запит) сервер добре масштабується і простий у розумінні, але має накладні витрати на процеси; він підходить, якщо клієнти підключаються нерегулярно або терпимий певний оверхед. Префоркований сервер забезпечує найкращу продуктивність при інтенсивному навантаженні за рахунок постійного пулу процесів, зменшуючи затримки на створення, ціною фіксованого споживання ресурсів. Вибір реалізації залежить від сценарію: для максимального навантаження оптимальний префорк, для помірного – паралельний з контролем числа процесів, для простоти або дуже малого навантаження – ітеративний.

---

### 2. Протокол взаємодії

Клієнт і сервер обмінюються повідомленнями за заздалегідь визначеним протоколом прикладного рівня, який має чітко визначений формат структур даних:

- **Запит файлу (FileRequest):**  
  - **protocol_version** (1 байт) – версія протоколу (у даній реалізації використовується версія 1).  
  - **message_type** (1 байт) – код повідомлення, що позначає запит на файл (MSG_FILE_REQUEST).  
  - **filename_length** (2 байти) – фактична довжина назви файлу.  
  - **filename** – рядок ASCII, що містить лише назву файлу (без шляхів), обмежений до 255 символів плюс термінатор.

- **Відповідь на запит (FileResponse):**  
  - **protocol_version** (1 байт) – версія протоколу, що має збігатися з версією клієнта.  
  - **message_type** (1 байт) – код повідомлення, що позначає відповідь (MSG_FILE_RESPONSE).  
  - **status** (1 байт) – статус виконання запиту (0 – успіх; інші значення – коди помилок).  
  - **file_size** (8 байт) – розмір файлу у байтах (значущий лише при успішному запиті).

- **Відповідь клієнта (ClientResponse):**  
  - **message_type** (1 байт) – повідомлення, яке сигналізує про готовність приймати файл (MSG_READY_TO_RECEIVE) або про відмову (MSG_REFUSE_TO_RECEIVE).

- **Передача файлу:**  
  Після успішного обміну заголовками і підтвердження готовності клієнта, сервер передає вміст файлу у вигляді послідовних блоків байтів (розмір блоку визначено за замовчуванням, наприклад, 64 КБ). Завершення передачі відзначається закриттям TCP-з’єднання.

---

### 3. Обробка помилок

Система передбачає обробку декількох типових помилок:

- **Невідповідність версії протоколу:**  
  Якщо версії клієнта і сервера не співпадають, сервер відправляє повідомлення з кодом помилки і закриває з’єднання, а клієнт повідомляє про невідповідність.

- **Некоректне ім’я файлу:**  
  При виявленні заборонених символів або недопустимого формату імені (наприклад, містить шлях або є порожнім) сервер негайно повертає код помилки, що сигналізує про недопустиме ім’я файлу, і припиняє обробку запиту.

- **Файл не знайдено або недоступний:**  
  Якщо файл не існує або сервер не може отримати до нього доступ, повертається відповідний код помилки, і з’єднання закривається. У цьому випадку клієнт повідомляє про те, що файл не знайдено.

- **Відмова клієнта приймати файл:**  
  Якщо розмір файлу перевищує встановлений ліміт, клієнт відразу відправляє повідомлення про відмову приймати файл, і з’єднання завершується, що запобігає марній передачі даних.

- **Помилки мережі та системні помилки:**  
  Якщо при мережевих операціях або роботі з файловою системою виникають помилки, сервер обробляє їх для конкретного з’єднання (надсилаючи повідомлення про помилку або закриваючи сокет), але продовжує свою роботу для інших клієнтів. Таким чином, і клієнт, і сервер завжди отримують зрозумілі повідомлення про помилки і коректно завершають сесію.

---

##  Приклади використання програми

### Клієнт

#### Успішний варіант виконання запиту

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./client 127.0.0.1 12345 g101.pdf 104857600
Connecting to server at 127.0.0.1:12345
Connected to server
Sending file request for: g101.pdf
Received response: protocol version 1, message type 2, status 0, file size 1156958
Sending ready to receive message
Receiving file content (1156958 bytes)...
Received: 1156958/1156958 bytes (100.0%)
File transfer complete

dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ls -la | grep g101.pdf
-rwxrwxrwx 1 dymytryke dymytryke 1156958 Mar 11 16:05 g101.pdf
```

#### Варіант виконання запиту коли розмір файлу перевищує встановлене значення

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./client 127.0.0.1 12345 g101.pdf 10485
Connecting to server at 127.0.0.1:12345
Connected to server
Sending file request for: g101.pdf
Received response: protocol version 1, message type 2, status 0, file size 1156958
File size (1156958 bytes) exceeds maximum allowed size (10485 bytes)
```

#### Варіант виконання запиту коли запитуваний файл не існує в директорії

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./client 127.0.0.1 12345 g1011.pdf 1048500000
Connecting to server at 127.0.0.1:12345
Connected to server
Sending file request for: g1011.pdf
Received response: protocol version 1, message type 2, status 3, file size 0
Error: File not found
```

#### Варіант виконання запиту коли не вдається встановити з'єднання

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./client 127.0.0.1 12344 g101.pdf 1048500000
Connecting to server at 127.0.0.1:12344
Connection failed: Connection refused
```

### Ітеративний сервер

#### Успішний варіант виконання запиту

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./server_iterative 127.0.0.1 12345 /home/dymytryke/Netw.UNIX/
Iterative server started at 127.0.0.1:12345, serving files from '/home/dymytryke/Netw.UNIX/'
Waiting for connections...
Accepted connection from 127.0.0.1:36782
Received file request for: g101.pdf
Sending file response: status=0, file_size=1156958
Client is ready to receive the file
Sending file content (1156958 bytes)...
Sent: 1156958/1156958 bytes (100.0%)
File transfer complete
Client connection handled and closed
Waiting for connections...
```

#### Варіант виконання запиту коли розмір файлу більший за встановлене значення на клієнті

```bash
Accepted connection from 127.0.0.1:50580
Received file request for: g101.pdf
Sending file response: status=0, file_size=1156958
Client refused to receive the file (too large)
Client connection handled and closed
Waiting for connections...
```

#### Термінування процесу ітеративного серверу

```bash
Waiting for connections...
^C
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ 
```

### Паралельний сервер

#### Варіант виконання запиту з використанням декількох дочірніх процесів

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./server_parallel 127.0.0.1 12345 /home/dymytryke/Netw.UNIX/3 2
Parallel server started at 127.0.0.1:12345, serving files from '/home/dymytryke/Netw.UNIX/3', max children: 2
Waiting for connections (active children: 0/2)...
Accepted connection from 127.0.0.1:53898
Child process created (PID: 126645), active children: 1/2
Waiting for connections (active children: 1/2)...
Accepted connection from 127.0.0.1:53930
[PID 126645] Child process created to handle client 127.0.0.1:53898
[PID 126645] Received file request for: VMware-player2.exe.tar
[PID 126645] Sending file response: status=0, file_size=232673280
Child process created (PID: 126646), active children: 2/2
Max children reached (2), waiting for a child to exit...
[PID 126646] Child process created to handle client 127.0.0.1:53930
[PID 126645] Client is ready to receive the file
[PID 126645] Sending file content (232673280 bytes)...
[PID 126646] Received file request for: VMware-player.exe.tar
[PID 126646] Sending file response: status=0, file_size=232673280
Sent: 262144/232673280 bytes (0.1%)[PID 126646] Client is ready to receive the file
[PID 126646] Sending file content (232673280 bytes)...
Sent: 29163520/232673280 bytes (12.5%)Max children reached (2), waiting for a child to exit...
Sent: 56557568/232673280 bytes (24.3%)Max children reached (2), waiting for a child to exit...
Sent: 82247680/232673280 bytes (35.3%)Max children reached (2), waiting for a child to exit...
Sent: 111411200/232673280 bytes (47.9%)Max children reached (2), waiting for a child to exit...
Sent: 138870784/232673280 bytes (59.7%)Max children reached (2), waiting for a child to exit...
Sent: 151257088/232673280 bytes (65.0%)Max children reached (2), waiting for a child to exit...
Sent: 179568640/232673280 bytes (77.2%)Max children reached (2), waiting for a child to exit...
Sent: 208732160/232673280 bytes (89.7%)Max children reached (2), waiting for a child to exit...
Sent: 232673280/232673280 bytes (100.0%)[PID 126646] 
[PID 126646] File transfer complete
[PID 126646] Client connection handled and closed
Waiting for connections (active children: 1/2)...
Accepted connection from 127.0.0.1:53914
Child process created (PID: 126746), active children: 2/2
Max children reached (2), waiting for a child to exit...
[PID 126746] Child process created to handle client 127.0.0.1:53914
[PID 126746] Received file request for: VMware-player1.exe.tar
[PID 126746] Sending file response: status=0, file_size=232673280
[PID 126746] Client is ready to receive the file
[PID 126746] Sending file content (232673280 bytes)...
Sent: 51249152/232673280 bytes (22.0%)Max children reached (2), waiting for a child to exit...
Sent: 56557568/232673280 bytes (24.3%)Max children reached (2), waiting for a child to exit...
Sent: 98172928/232673280 bytes (42.2%)Max children reached (2), waiting for a child to exit...
Sent: 123797504/232673280 bytes (53.2%)Max children reached (2), waiting for a child to exit...
Sent: 132644864/232673280 bytes (57.0%)Max children reached (2), waiting for a child to exit...
Sent: 159186944/232673280 bytes (68.4%)Max children reached (2), waiting for a child to exit...
Sent: 188416000/232673280 bytes (81.0%)Max children reached (2), waiting for a child to exit...
Sent: 182190080/232673280 bytes (78.3%)Max children reached (2), waiting for a child to exit...
Sent: 232673280/232673280 bytes (100.0%)PID 126746]  
[PID 126746] File transfer complete
[PID 126746] Client connection handled and closed
Waiting for connections (active children: 1/2)...
Accepted connection from 127.0.0.1:53944
Child process created (PID: 126817), active children: 2/2
Max children reached (2), waiting for a child to exit...
[PID 126817] Child process created to handle client 127.0.0.1:53944
[PID 126817] Received file request for: VMware-player3.exe.tar
[PID 126817] Error: File not found: /home/dymytryke/Netw.UNIX/3/VMware-player3.exe.tar
[PID 126817] Client connection handled and closed
Waiting for connections (active children: 1/2)...
Sent: 232673280/232673280 bytes (100.0%)[PID 126645] 
[PID 126645] File transfer complete
[PID 126645] Client connection handled and closed

dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ Receiving file content (232673280 bytes)...
Received: 90112/232673280 bytes (0.0%)Receiving file content (232673280 bytes)...
Received: 230932480/232673280 bytes (99.3%)Received response: protocol version 1, message type 2, status 0, file size 232673280
Sending ready to receive message
Received: 231464960/232673280 bytes (99.5%)Receiving file content (232673280 bytes)...
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
Received: 230928384/232673280 bytes (99.3%)Received response: protocol version 1, message type 2, status 3, file size 0
Received: 230932480/232673280 bytes (99.3%)Error: File not found
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
```

### Префорк сервер

#### Варіант виконання запиту з використанням декількох дочірніх процесів

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./server_preforked 127.0.0.1 12345 /home/dymytryke/Netw.UNIX/3 3
Pre-forked server started at 127.0.0.1:12345, serving files from '/home/dymytryke/Netw.UNIX/3', creating 3 child processes
Child process created (PID: 125102, 1/3)
[PID 125102] Child process started, waiting for connections
Child process created (PID: 125103, 2/3)
[PID 125103] Child process started, waiting for connections
Child process created (PID: 125104, 3/3)
All child processes created. Parent process is now waiting.
[PID 125104] Child process started, waiting for connections
[PID 125102] Accepted connection from 127.0.0.1:49760
[PID 125102] Received file request for: VMware-player1.exe.tar
[PID 125102] Sending file response: status=0, file_size=232673280
[PID 125103] Accepted connection from 127.0.0.1:49764
[PID 125102] Client is ready to receive the file
[PID 125103] Received file request for: VMware-player2.exe.tar
[PID 125102] Sending file content (232673280 bytes)...
[PID 125103] Sending file response: status=0, file_size=232673280
[PID 125104] Accepted connection from 127.0.0.1:49766
[PID 125104] Received file request for: VMware-player.exe.tar
[PID 125104] Sending file response: status=0, file_size=232673280
[PID 125103] Client is ready to receive the file
Sent: 65536/232673280 bytes (0.0%)[PID 125103] Sending file content (232673280 bytes)...
[PID 125104] Client is ready to receive the file
Sent: 131072/232673280 bytes (0.1%)[PID 125104] Sending file content (232673280 bytes)...
Sent: 232673280/232673280 bytes (100.0%)PID 125102]  
[PID 125102] File transfer complete
[PID 125102] Client connection handled and closed
Sent: 232673280/232673280 bytes (100.0%)[PID 125104] 
[PID 125104] File transfer complete
[PID 125104] Client connection handled and closed
Sent: 232673280/232673280 bytes (100.0%)[PID 125103] 
[PID 125103] File transfer complete
[PID 125103] Client connection handled and closed


dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX/3$ ./multiple_clients.sh 
Connecting to server at 127.0.0.1:12345
Connected to server
Sending file request for: VMware-player1.exe.tar
Connecting to server at 127.0.0.1:12345
Connected to server
Connecting to server at 127.0.0.1:12345
Received response: protocol version 1, message type 2, status 0, file size 232673280
Sending file request for: VMware-player2.exe.tar
Sending ready to receive message
Connected to server
Sending file request for: VMware-player.exe.tar
Received response: protocol version 1, message type 2, status 0, file size 232673280
Sending ready to receive message
Received response: protocol version 1, message type 2, status 0, file size 232673280
Sending ready to receive message
Receiving file content (232673280 bytes)...
Received: 28672/232673280 bytes (0.0%)Receiving file content (232673280 bytes)...
Received: 49152/232673280 bytes (0.0%)Receiving file content (232673280 bytes)...
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
Received: 232673280/232673280 bytes (100.0%)
File transfer complete
```

#### Завершення роботи паралельного серверу

```bash
^CParent process received termination signal. Shutting down...
Sending SIGTERM to child process 125102
Sending SIGTERM to child process 125103
Sending SIGTERM to child process 125104
Waiting for all child processes to exit...
Child process 125102 exited with status -1
Child process 125103 exited with status -1
Child process 125104 exited with status -1
Server shutdown complete
```

### Мережева активність при виконанні запиту

```bash
dymytryke@dymytryke:/mnt/c/Users/dymyt/OneDrive/Документы/save/Учеба/4_курс/Netw.UNIX$ sudo tcpdump -i lo src port 12345 or dst port 12345
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on lo, link-type EN10MB (Ethernet), snapshot length 262144 bytes
16:13:59.569624 IP localhost.54846 > localhost.12345: Flags [S], seq 3975880678, win 65495, options [mss 65495,sackOK,TS val 2395129662 ecr 0,nop,wscale 7], length 0
16:13:59.569630 IP localhost.12345 > localhost.54846: Flags [S.], seq 3815518273, ack 3975880679, win 65483, options [mss 65495,sackOK,TS val 2395129662 ecr 2395129662,nop,wscale 7], length 0
16:13:59.569635 IP localhost.54846 > localhost.12345: Flags [.], ack 1, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 0
16:13:59.569671 IP localhost.54846 > localhost.12345: Flags [P.], seq 1:261, ack 1, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 260
16:13:59.569673 IP localhost.12345 > localhost.54846: Flags [.], ack 261, win 510, options [nop,nop,TS val 2395129662 ecr 2395129662], length 0
16:13:59.569691 IP localhost.12345 > localhost.54846: Flags [P.], seq 1:17, ack 261, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 16
16:13:59.569706 IP localhost.54846 > localhost.12345: Flags [.], ack 17, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 0
16:13:59.569757 IP localhost.54846 > localhost.12345: Flags [P.], seq 261:262, ack 17, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 1
16:13:59.569836 IP localhost.12345 > localhost.54846: Flags [.], seq 17:32785, ack 262, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 32768
16:13:59.569851 IP localhost.12345 > localhost.54846: Flags [P.], seq 32785:65553, ack 262, win 512, options [nop,nop,TS val 2395129662 ecr 2395129662], length 32768
16:13:59.571032 IP localhost.54846 > localhost.12345: Flags [.], ack 65553, win 0, options [nop,nop,TS val 2395129663 ecr 2395129662], length 0
16:13:59.572141 IP localhost.54846 > localhost.12345: Flags [.], ack 65553, win 379, options [nop,nop,TS val 2395129664 ecr 2395129662], length 0
16:13:59.572150 IP localhost.12345 > localhost.54846: Flags [.], seq 65553:98321, ack 262, win 512, options [nop,nop,TS val 2395129664 ecr 2395129664], length 32768
16:13:59.574565 IP localhost.54846 > localhost.12345: Flags [.], ack 98321, win 512, options [nop,nop,TS val 2395129667 ecr 2395129664], length 0
16:13:59.574611 IP localhost.12345 > localhost.54846: Flags [P.], seq 98321:131089, ack 262, win 512, options [nop,nop,TS val 2395129667 ecr 2395129667], length 32768
16:13:59.574616 IP localhost.12345 > localhost.54846: Flags [.], seq 131089:163857, ack 262, win 512, options [nop,nop,TS val 2395129667 ecr 2395129667], length 32768
16:13:59.574798 IP localhost.54846 > localhost.12345: Flags [.], ack 163857, win 0, options [nop,nop,TS val 2395129667 ecr 2395129667], length 0
16:13:59.575793 IP localhost.54846 > localhost.12345: Flags [.], ack 163857, win 379, options [nop,nop,TS val 2395129668 ecr 2395129667], length 0
16:13:59.575802 IP localhost.12345 > localhost.54846: Flags [P.], seq 163857:196625, ack 262, win 512, options [nop,nop,TS val 2395129668 ecr 2395129668], length 32768
16:13:59.578058 IP localhost.54846 > localhost.12345: Flags [.], ack 196625, win 512, options [nop,nop,TS val 2395129670 ecr 2395129668], length 0
16:13:59.578088 IP localhost.12345 > localhost.54846: Flags [.], seq 196625:229393, ack 262, win 512, options [nop,nop,TS val 2395129670 ecr 2395129670], length 32768
16:13:59.578092 IP localhost.12345 > localhost.54846: Flags [P.], seq 229393:262161, ack 262, win 512, options [nop,nop,TS val 2395129670 ecr 2395129670], length 32768
16:13:59.578214 IP localhost.54846 > localhost.12345: Flags [.], ack 262161, win 0, options [nop,nop,TS val 2395129670 ecr 2395129670], length 0
16:13:59.579087 IP localhost.54846 > localhost.12345: Flags [.], ack 262161, win 379, options [nop,nop,TS val 2395129671 ecr 2395129670], length 0
16:13:59.579096 IP localhost.12345 > localhost.54846: Flags [.], seq 262161:294929, ack 262, win 512, options [nop,nop,TS val 2395129671 ecr 2395129671], length 32768
16:13:59.581952 IP localhost.54846 > localhost.12345: Flags [.], ack 294929, win 512, options [nop,nop,TS val 2395129674 ecr 2395129671], length 0
16:13:59.581968 IP localhost.12345 > localhost.54846: Flags [P.], seq 294929:327697, ack 262, win 512, options [nop,nop,TS val 2395129674 ecr 2395129674], length 32768
16:13:59.581985 IP localhost.12345 > localhost.54846: Flags [.], seq 327697:360465, ack 262, win 512, options [nop,nop,TS val 2395129674 ecr 2395129674], length 32768
16:13:59.582115 IP localhost.54846 > localhost.12345: Flags [.], ack 360465, win 0, options [nop,nop,TS val 2395129674 ecr 2395129674], length 0
16:13:59.583194 IP localhost.54846 > localhost.12345: Flags [.], ack 360465, win 379, options [nop,nop,TS val 2395129675 ecr 2395129674], length 0
16:13:59.583205 IP localhost.12345 > localhost.54846: Flags [P.], seq 360465:393233, ack 262, win 512, options [nop,nop,TS val 2395129675 ecr 2395129675], length 32768
16:13:59.585366 IP localhost.54846 > localhost.12345: Flags [.], ack 393233, win 512, options [nop,nop,TS val 2395129678 ecr 2395129675], length 0
16:13:59.585383 IP localhost.12345 > localhost.54846: Flags [.], seq 393233:426001, ack 262, win 512, options [nop,nop,TS val 2395129678 ecr 2395129678], length 32768
16:13:59.585386 IP localhost.12345 > localhost.54846: Flags [P.], seq 426001:458769, ack 262, win 512, options [nop,nop,TS val 2395129678 ecr 2395129678], length 32768
16:13:59.585524 IP localhost.54846 > localhost.12345: Flags [.], ack 458769, win 0, options [nop,nop,TS val 2395129678 ecr 2395129678], length 0
16:13:59.586588 IP localhost.54846 > localhost.12345: Flags [.], ack 458769, win 379, options [nop,nop,TS val 2395129679 ecr 2395129678], length 0
16:13:59.586626 IP localhost.12345 > localhost.54846: Flags [.], seq 458769:491537, ack 262, win 512, options [nop,nop,TS val 2395129679 ecr 2395129679], length 32768
16:13:59.589688 IP localhost.54846 > localhost.12345: Flags [.], ack 491537, win 512, options [nop,nop,TS val 2395129682 ecr 2395129679], length 0
16:13:59.589710 IP localhost.12345 > localhost.54846: Flags [P.], seq 491537:524305, ack 262, win 512, options [nop,nop,TS val 2395129682 ecr 2395129682], length 32768
16:13:59.589714 IP localhost.12345 > localhost.54846: Flags [.], seq 524305:557073, ack 262, win 512, options [nop,nop,TS val 2395129682 ecr 2395129682], length 32768
16:13:59.589880 IP localhost.54846 > localhost.12345: Flags [.], ack 557073, win 0, options [nop,nop,TS val 2395129682 ecr 2395129682], length 0
16:13:59.591043 IP localhost.54846 > localhost.12345: Flags [.], ack 557073, win 379, options [nop,nop,TS val 2395129683 ecr 2395129682], length 0
16:13:59.591080 IP localhost.12345 > localhost.54846: Flags [P.], seq 557073:589841, ack 262, win 512, options [nop,nop,TS val 2395129683 ecr 2395129683], length 32768
16:13:59.593608 IP localhost.54846 > localhost.12345: Flags [.], ack 589841, win 512, options [nop,nop,TS val 2395129686 ecr 2395129683], length 0
16:13:59.593627 IP localhost.12345 > localhost.54846: Flags [.], seq 589841:622609, ack 262, win 512, options [nop,nop,TS val 2395129686 ecr 2395129686], length 32768
16:13:59.593630 IP localhost.12345 > localhost.54846: Flags [P.], seq 622609:655377, ack 262, win 512, options [nop,nop,TS val 2395129686 ecr 2395129686], length 32768
16:13:59.593781 IP localhost.54846 > localhost.12345: Flags [.], ack 655377, win 0, options [nop,nop,TS val 2395129686 ecr 2395129686], length 0
16:13:59.594723 IP localhost.54846 > localhost.12345: Flags [.], ack 655377, win 379, options [nop,nop,TS val 2395129687 ecr 2395129686], length 0
16:13:59.594736 IP localhost.12345 > localhost.54846: Flags [.], seq 655377:688145, ack 262, win 512, options [nop,nop,TS val 2395129687 ecr 2395129687], length 32768
16:13:59.597284 IP localhost.54846 > localhost.12345: Flags [.], ack 688145, win 512, options [nop,nop,TS val 2395129689 ecr 2395129687], length 0
16:13:59.597353 IP localhost.12345 > localhost.54846: Flags [P.], seq 688145:720913, ack 262, win 512, options [nop,nop,TS val 2395129689 ecr 2395129689], length 32768
16:13:59.597357 IP localhost.12345 > localhost.54846: Flags [.], seq 720913:753681, ack 262, win 512, options [nop,nop,TS val 2395129689 ecr 2395129689], length 32768
16:13:59.597510 IP localhost.54846 > localhost.12345: Flags [.], ack 753681, win 0, options [nop,nop,TS val 2395129690 ecr 2395129689], length 0
16:13:59.598432 IP localhost.54846 > localhost.12345: Flags [.], ack 753681, win 379, options [nop,nop,TS val 2395129691 ecr 2395129689], length 0
16:13:59.598442 IP localhost.12345 > localhost.54846: Flags [P.], seq 753681:786449, ack 262, win 512, options [nop,nop,TS val 2395129691 ecr 2395129691], length 32768
16:13:59.600913 IP localhost.54846 > localhost.12345: Flags [.], ack 786449, win 512, options [nop,nop,TS val 2395129693 ecr 2395129691], length 0
16:13:59.600956 IP localhost.12345 > localhost.54846: Flags [.], seq 786449:819217, ack 262, win 512, options [nop,nop,TS val 2395129693 ecr 2395129693], length 32768
16:13:59.600961 IP localhost.12345 > localhost.54846: Flags [P.], seq 819217:851985, ack 262, win 512, options [nop,nop,TS val 2395129693 ecr 2395129693], length 32768
16:13:59.601119 IP localhost.54846 > localhost.12345: Flags [.], ack 851985, win 0, options [nop,nop,TS val 2395129693 ecr 2395129693], length 0
16:13:59.602229 IP localhost.54846 > localhost.12345: Flags [.], ack 851985, win 379, options [nop,nop,TS val 2395129694 ecr 2395129693], length 0
16:13:59.602241 IP localhost.12345 > localhost.54846: Flags [.], seq 851985:884753, ack 262, win 512, options [nop,nop,TS val 2395129694 ecr 2395129694], length 32768
16:13:59.604437 IP localhost.54846 > localhost.12345: Flags [.], ack 884753, win 512, options [nop,nop,TS val 2395129697 ecr 2395129694], length 0
16:13:59.604466 IP localhost.12345 > localhost.54846: Flags [P.], seq 884753:917521, ack 262, win 512, options [nop,nop,TS val 2395129697 ecr 2395129697], length 32768
16:13:59.604470 IP localhost.12345 > localhost.54846: Flags [.], seq 917521:950289, ack 262, win 512, options [nop,nop,TS val 2395129697 ecr 2395129697], length 32768
16:13:59.604620 IP localhost.54846 > localhost.12345: Flags [.], ack 950289, win 0, options [nop,nop,TS val 2395129697 ecr 2395129697], length 0
16:13:59.605850 IP localhost.54846 > localhost.12345: Flags [.], ack 950289, win 379, options [nop,nop,TS val 2395129698 ecr 2395129697], length 0
16:13:59.605862 IP localhost.12345 > localhost.54846: Flags [P.], seq 950289:983057, ack 262, win 512, options [nop,nop,TS val 2395129698 ecr 2395129698], length 32768
16:13:59.608475 IP localhost.54846 > localhost.12345: Flags [.], ack 983057, win 512, options [nop,nop,TS val 2395129701 ecr 2395129698], length 0
16:13:59.608491 IP localhost.12345 > localhost.54846: Flags [.], seq 983057:1015825, ack 262, win 512, options [nop,nop,TS val 2395129701 ecr 2395129701], length 32768
16:13:59.608509 IP localhost.12345 > localhost.54846: Flags [P.], seq 1015825:1048593, ack 262, win 512, options [nop,nop,TS val 2395129701 ecr 2395129701], length 32768
16:13:59.608675 IP localhost.54846 > localhost.12345: Flags [.], ack 1048593, win 0, options [nop,nop,TS val 2395129701 ecr 2395129701], length 0
16:13:59.609907 IP localhost.54846 > localhost.12345: Flags [.], ack 1048593, win 379, options [nop,nop,TS val 2395129702 ecr 2395129701], length 0
16:13:59.609917 IP localhost.12345 > localhost.54846: Flags [.], seq 1048593:1081361, ack 262, win 512, options [nop,nop,TS val 2395129702 ecr 2395129702], length 32768
16:13:59.612337 IP localhost.54846 > localhost.12345: Flags [.], ack 1081361, win 512, options [nop,nop,TS val 2395129704 ecr 2395129702], length 0
16:13:59.612354 IP localhost.12345 > localhost.54846: Flags [P.], seq 1081361:1114129, ack 262, win 512, options [nop,nop,TS val 2395129704 ecr 2395129704], length 32768
16:13:59.612357 IP localhost.12345 > localhost.54846: Flags [.], seq 1114129:1146897, ack 262, win 512, options [nop,nop,TS val 2395129704 ecr 2395129704], length 32768
16:13:59.612498 IP localhost.54846 > localhost.12345: Flags [.], ack 1146897, win 0, options [nop,nop,TS val 2395129705 ecr 2395129704], length 0
16:13:59.613673 IP localhost.54846 > localhost.12345: Flags [.], ack 1146897, win 379, options [nop,nop,TS val 2395129706 ecr 2395129704], length 0
16:13:59.613680 IP localhost.12345 > localhost.54846: Flags [FP.], seq 1146897:1156975, ack 262, win 512, options [nop,nop,TS val 2395129706 ecr 2395129706], length 10078
16:13:59.616215 IP localhost.54846 > localhost.12345: Flags [F.], seq 262, ack 1156976, win 512, options [nop,nop,TS val 2395129708 ecr 2395129706], length 0
16:13:59.616299 IP localhost.12345 > localhost.54846: Flags [.], ack 263, win 512, options [nop,nop,TS val 2395129708 ecr 2395129708], length 0
```

