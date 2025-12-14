#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 12345
#define BUFFER_SIZE 1024

void gameLoop(int sock) {
    char buffer[BUFFER_SIZE] = {0};
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recv(sock, buffer, BUFFER_SIZE, 0);
        if (valread <= 0) break;

        cout << buffer;

        if (string(buffer).find("Your answer") != string::npos ||
            string(buffer).find("Enter name") != string::npos) {
            string input;
            getline(cin, input);
            send(sock, input.c_str(), input.size(), 0);
        }
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    gameLoop(sock);
    close(sock);
    return 0;
}
