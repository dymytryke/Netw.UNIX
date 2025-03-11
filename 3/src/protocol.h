#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Версія протоколу
#define PROTOCOL_VERSION 1

// Максимальна довжина імені файлу (без нульового символу)
#define MAX_FILENAME_LENGTH 255

// Коди помилок
#define ERR_PROTOCOL_MISMATCH 1
#define ERR_INVALID_FILENAME 2
#define ERR_FILE_NOT_FOUND 3
#define ERR_FILE_ACCESS 4
#define ERR_INTERNAL 5

// Типи повідомлень
#define MSG_FILE_REQUEST     1
#define MSG_FILE_RESPONSE    2
#define MSG_READY_TO_RECEIVE 3
#define MSG_REFUSE_TO_RECEIVE 4

// Розмір блоку при передачі файлу (64КБ)
#define DEFAULT_CHUNK_SIZE (64 * 1024)

// Заголовок запиту від клієнта до сервера
typedef struct {
    uint8_t protocol_version;
    uint8_t message_type;
    uint16_t filename_length;  // Довжина імені файлу
    char filename[MAX_FILENAME_LENGTH + 1];  // +1 для термінатора '\0'
} FileRequestHeader;

// Заголовок відповіді від сервера до клієнта
typedef struct {
    uint8_t protocol_version;
    uint8_t message_type;
    uint8_t status;  // 0 = успіх, інакше код помилки
    uint64_t file_size;  // Розмір файлу у байтах
} FileResponseHeader;

// Заголовок відповіді клієнта серверу
typedef struct {
    uint8_t message_type;  // MSG_READY_TO_RECEIVE або MSG_REFUSE_TO_RECEIVE
} ClientResponseHeader;

#endif // PROTOCOL_H
