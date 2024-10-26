#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      // For O_* constants
#include <sys/stat.h>   // For mode constants
#include <mqueue.h>     // Message Queue library
#include <unistd.h>

#define SERVER_QUEUE_NAME   "/server_queue"
#define QUEUE_PERMISSIONS   0660
#define MAX_MESSAGES        10
#define MAX_MSG_SIZE        256
#define MSG_BUFFER_SIZE     (MAX_MSG_SIZE + 10)

typedef struct {
    char sender[32];
    char recipient[32];
    char message[MAX_MSG_SIZE];
} ChatMessage;

void server() {
    mqd_t mq_server;
    struct mq_attr attr;
    char buffer[MSG_BUFFER_SIZE];
    ChatMessage received_message;

    // Message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = sizeof(ChatMessage);
    attr.mq_curmsgs = 0;

    // Create the server queue
    mq_server = mq_open(SERVER_QUEUE_NAME, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (mq_server == (mqd_t) -1) {
        perror("Server queue creation failed");
        exit(1);
    }
    
    printf("Server is running, waiting for messages...\n");

    while (1) {
        // Receive a message from the message queue
        ssize_t bytes_read = mq_receive(mq_server, (char*)&received_message, sizeof(ChatMessage), NULL);
        if (bytes_read < 0) {
            perror("Message receive error");
            continue;
        }

        printf("Received message from %s to %s: %s\n", received_message.sender, received_message.recipient, received_message.message);

        // Check if the message is to exit
        if (strncmp(received_message.message, "exit", 4) == 0) {
            printf("Shutting down server...\n");
            break;
        }

        // Relay message to recipient queue
        char recipient_queue_name[64];
        snprintf(recipient_queue_name, 64, "/%s_queue", received_message.recipient);
        mqd_t mq_recipient = mq_open(recipient_queue_name, O_WRONLY);
        if (mq_recipient == (mqd_t) -1) {
            printf("Recipient %s queue not found.\n", received_message.recipient);
        } else {
            mq_send(mq_recipient, (char*)&received_message, sizeof(ChatMessage), 0);
            mq_close(mq_recipient);
        }
    }

    // Close and delete the server queue
    mq_close(mq_server);
    mq_unlink(SERVER_QUEUE_NAME);
}

int main() {
    server();
    return 0;
}
