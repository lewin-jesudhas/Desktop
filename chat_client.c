#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>

#define QUEUE_NAME "/chat_server_queue"
#define MAX_SIZE 1024

// Function to encrypt a file using OpenSSL
void encrypt_and_send_file(const char *client_name, const char *recipient, const char *filename, const char *password) {
    char encrypted_file[64];
    snprintf(encrypted_file, sizeof(encrypted_file), "%s.enc", filename);

    char encrypt_command[256];
    snprintf(encrypt_command, sizeof(encrypt_command),
             "openssl enc -aes-256-cbc -salt -pbkdf2 -in %s -out %s -pass pass:%s",
             filename, encrypted_file, password);

    if (system(encrypt_command) != 0) {
        printf("Encryption failed.\n");
        return;
    }

    mqd_t mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1) {
        perror("Client cannot open message queue");
        exit(1);
    }

    char buffer[MAX_SIZE];
    snprintf(buffer, MAX_SIZE, "%s %s %s %s", client_name, recipient, encrypted_file, password);
    mq_send(mq, buffer, strlen(buffer) + 1, 0);
    mq_close(mq);
}

// Function to send a text message
void send_message(const char *client_name, const char *recipient, const char *message) {
    mqd_t mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == -1) {
        perror("Client cannot open message queue");
        exit(1);
    }

    char buffer[MAX_SIZE];
    snprintf(buffer, MAX_SIZE, "%s %s %s %s", client_name, recipient, message, "");
    mq_send(mq, buffer, strlen(buffer) + 1, 0);
    mq_close(mq);
}

// Thread to poll server for messages
void *receive_messages(void *arg) {
    char queue_name[32];
    snprintf(queue_name, sizeof(queue_name), "/%s_queue", (char *)arg);

    mqd_t mq = mq_open(queue_name, O_RDONLY | O_CREAT, 0644, NULL);
    if (mq == -1) {
        perror("Message queue open error");
        pthread_exit(NULL);
    }

    char buffer[MAX_SIZE];
    while (1) {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            printf("Message received: %s\n", buffer);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <client_name>\n", argv[0]);
        return 1;
    }

    char client_name[32];
    strncpy(client_name, argv[1], sizeof(client_name) - 1);

    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_messages, client_name);

    while (1) {
        int choice;
        char recipient[32], message[256], password[32];

        printf("1. Send message\n2. Send encrypted file\nChoose an option: ");
        scanf("%d", &choice);

        printf("Enter recipient name: ");
        scanf("%s", recipient);

        if (choice == 1) {
            printf("Enter message: ");
            scanf(" %[^\n]", message);
            send_message(client_name, recipient, message);
        } else if (choice == 2) {
            printf("Enter file path: ");
            scanf("%s", message);
            printf("Enter password: ");
            scanf("%s", password);
            encrypt_and_send_file(client_name, recipient, message, password);
        } else {
            printf("Invalid option.\n");
        }
    }

    pthread_join(receive_thread, NULL);
    return 0;
}
