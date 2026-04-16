// Simple TCP echo server
// Listens on port 9000, echoes back received data with a prefix
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>

#define PORT 9000
#define BUF_SIZE 4096

static volatile int running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main() {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[server] Echo server starting on port %d...\n", PORT);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("[server] Listening....\n");

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!running) break;
            perror("accept");
            continue;
        }

        printf("[server] Client connected\n");

        char buf[BUF_SIZE];
        int n;
        int request_count = 0;
        while (running && (n = recv(client_fd, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = '\0';
            request_count++;

            // Get current time
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

            // Build response with metadata
            char response[BUF_SIZE];
            int resp_len = snprintf(response, sizeof(response),
                "[echo #%d @ %s] %s", request_count, time_str, buf);

            send(client_fd, response, resp_len, 0);
        }

        printf("[server] Client disconnected (served %d requests)\n", request_count);
        close(client_fd);
    }

    printf("[server] Shutting down\n");
    close(server_fd);
    return 0;
}
