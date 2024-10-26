#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

#define QUEUE_NAME "/chat_server_queue"
#define MAX_SIZE 1024

// Function to create a folder for each client
void create_client_folder(const char *client_name) {
    char folder_path[64];
    snprintf(folder_path, sizeof(folder_path), "%s_files", client_name);
    if (mkdir(folder_path, 0750) == -1 && errno != EEXIST) {
        perror("Error creating client folder");
    }
}

// Function to decrypt and store files securely
void save_encrypted_file(const char *sender, const char *recipient, const char *encrypted_filename, const char *password) {
    create_client_folder(recipient);

    const char *filename = strrchr(encrypted_filename, '/');
    if (filename) filename++; else filename = encrypted_filename;

    char destination[128];
    snprintf(destination, sizeof(destination), "%s_files/%s.dec", recipient, filename);

    char decrypt_command[256];
    snprintf(decrypt_command, sizeof(decrypt_command),
             "openssl enc -aes-256-cbc -pbkdf2 -d -in %s -out %s -pass pass:%s",
             encrypted_filename, destination, password);

    if (system(decrypt_command) != 0) {
        printf("Decryption failed\n");
        return;
    }

    chmod(destination, 0600);
    chown(destination, getuid(), getgid());
}

// Function to send message to the recipient's queue
void send_to_recipient(const char *recipient, const char *message) {
    char queue_name[32];
    snprintf(queue_name, sizeof(queue_name), "/%s_queue", recipient);

    mqd_t mq = mq_open(queue_name, O_WRONLY | O_CREAT, 0644, NULL);
    if (mq == -1) {
        perror("Cannot open recipient message queue");
        return;
    }

    mq_send(mq, message, strlen(message) + 1, 0);
    mq_close(mq);
}

void process_message(const char *sender, const char *recipient, const char *message, const char *password) {
    if (strstr(message, ".enc")) {
        save_encrypted_file(sender, recipient, message, password);
        char notification[256];
        snprintf(notification, sizeof(notification), "File from %s saved securely.", sender);
        send_to_recipient(recipient, notification);
    } else {
        send_to_recipient(recipient, message);
        printf("Message from %s to %s: %s\n", sender, recipient, message);
    }
}

int main() {
    mqd_t mq;
    struct mq_attr attr;
    char buffer[MAX_SIZE];

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == -1) {
        perror("Message queue creation failed");
        exit(1);
    }

    printf("Chat server started...\n");

    while (1) {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            char sender[32], recipient[32], message[256], password[32];
            sscanf(buffer, "%s %s %s %s", sender, recipient, message, password);
            process_message(sender, recipient, message, password);
        } else {
            perror("Message receive error");
        }
    }

    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    return 0;
}
