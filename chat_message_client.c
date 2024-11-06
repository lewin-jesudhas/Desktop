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
void client(char *client_name) {
    mqd_t mq_server, mq_client;
    struct mq_attr attr;
    ChatMessage message;
    char buffer[MSG_BUFFER_SIZE];
    char recipient[32];
    char client_queue_name[64];
    // Message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = sizeof(ChatMessage);
    attr.mq_curmsgs = 0;
    // Open server queue
    mq_server = mq_open(SERVER_QUEUE_NAME, O_WRONLY);
    if (mq_server == (mqd_t) -1) {
        perror("Cannot open server queue");
        exit(1);
    }
    // Create a client queue for receiving messages
    snprintf(client_queue_name, 64, "/%s_queue", client_name);
    mq_client = mq_open(client_queue_name, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS, &attr);
    if (mq_client == (mqd_t) -1) {
        perror("Cannot create client queue");
        exit(1);
    }
    printf("Client %s is running. Type 'exit' to quit.\n", client_name);
    // Separate thread for receiving messages
    if (fork() == 0) {
        while (1) {
            ssize_t bytes_read = mq_receive(mq_client, (char*)&message, sizeof(ChatMessage), NULL);
            if (bytes_read < 0) {
                perror("Message receive error");
                continue;
            }
            printf("Message from %s: %s\n", message.sender, message.message);
        }
    }
    // Sending messages
    while (1) {
        printf("Enter recipient: ");
        scanf("%s", recipient);
        printf("Enter message: ");
        scanf(" %[^\n]s", message.message);
        // Prepare the message structure
        strcpy(message.sender, client_name);
        strcpy(message.recipient, recipient);
        // Send the message to the server
        mq_send(mq_server, (char*)&message, sizeof(ChatMessage), 0);
        // Exit condition
        if (strncmp(message.message, "exit", 4) == 0) {
            break;
        }
    }
    // Close queues
    mq_close(mq_client);
    mq_unlink(client_queue_name);
    mq_close(mq_server);
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <client_name>\n", argv[0]);
        exit(1);
    }
    client(argv[1]);
    return 0;
}
