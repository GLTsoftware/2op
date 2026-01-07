#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <hiredis/hiredis.h>

#define MAX_MESSAGES 100

int main(int argc, char **argv) {
    char *username = NULL;
    int opt;

    // Define long options
    static struct option long_options[] = {
        {"user", required_argument, 0, 'u'},
        {0, 0, 0, 0}
    };

    // Parse command line options
    while ((opt = getopt_long(argc, argv, "u:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'u':
                username = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-u|--user username] message\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Check if at least one argument is provided for the message
    if (optind >= argc) {
        fprintf(stderr, "Usage: %s [-u|--user username] message\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Set default username if not provided
    if (username == NULL) {
        username = "anon";
    }

    // Calculate total length of message arguments
    int total_length = 0;
    for (int i = optind; i < argc; i++) {
        total_length += strlen(argv[i]);
        if (i < argc - 1) total_length++; // Count space between arguments
    }

    if (total_length >= 500) {
        fprintf(stderr, "Error: Message too long. Must be less than 500 characters.\n");
        return EXIT_FAILURE;
    }

    // Concatenate all message arguments into one string
    char message[500] = {0};
    for (int i = optind; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }

    // Get current timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Format the complete message with timestamp and username
    char formatted_message[256];
    snprintf(formatted_message, sizeof(formatted_message), "%s (%s) %s",
             timestamp, username, message);

    // Connect to Redis server
    redisContext *c = redisConnect("192.168.1.141", 6379);
    if (c == NULL || c->err) {
        if (c) {
            fprintf(stderr, "Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            fprintf(stderr, "Connection error: cannot allocate redis context\n");
        }
        return EXIT_FAILURE;
    }

    // Push the message to the Redis list (LPUSH adds to the front)
    redisReply *reply = redisCommand(c, "LPUSH 2opmsg %s", formatted_message);
    if (reply == NULL) {
        fprintf(stderr, "Error: Failed to execute LPUSH command.\n");
        redisFree(c);
        return EXIT_FAILURE;
    }
    freeReplyObject(reply);

    // Trim the list to keep only the last MAX_MESSAGES entries
    reply = redisCommand(c, "LTRIM 2opmsg 0 %d", MAX_MESSAGES - 1);
    if (reply == NULL) {
        fprintf(stderr, "Error: Failed to execute LTRIM command.\n");
        redisFree(c);
        return EXIT_FAILURE;
    }

    // Free resources
    printf("Message added successfully: %s\n", formatted_message);
    freeReplyObject(reply);
    redisFree(c);

    return EXIT_SUCCESS;
}

