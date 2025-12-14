#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

using namespace std;

#define PORT 12345
#define MAX_PLAYERS 3
#define BUFFER_SIZE 1024

struct Player {
    int socket;
    string name;
    int score = 0;
    char answer = '\0';
};

struct Question {
    string question;
    map<char, string> choices;
    char answer;
    bool hasAppeared = false;
};

vector<Question> questionList = {
    {"Sino ang Pambansang Bayani ng Pilipinas?\n", {{'A', "Jose Rizal"}, {'B', "Andres Bonifacio"}, {'C', "Emilio Aguinaldo"}}, 'A'},
    {"Kailan idineklara ang kalayaan ng Pilipinas?\n", {{'A', "Hulyo 12, 1898"}, {'B', "Hunyo 12, 1898"}, {'C', "Hunyo 12, 1889"}}, 'B'},
    {"Anong bahagi ng pananalita ang mga salitang ako, ikaw, at sila?\n", {{'A', "Pang-abay"}, {'B', "Pang-uri"}, {'C', "Panghalip"}}, 'C'},
    {"Ano ang tawag sa pinakamataas na bundok sa Pilipinas?\n", {{'A', "Mt. Banahaw"}, {'B', "Mt. Apo"}, {'C', "Mt. Pulag"}}, 'B'},
    {"Sino ang kasalukuyang pambansang alagad ng sining sa Panitikan?\n", {{'A', "Virgilio Almario"}, {'B', "Nick Joaquin"}, {'C', "F. Sionil Jose"}}, 'A'},
    {"Alin sa mga ito ang isang epikong-bayan ng mga Ifugao?\n", {{'A', "Biag ni Lam-ang"}, {'B', "Hudhud"}, {'C', "Ibalon"}}, 'B'},
    {"Saan matatagpuan ang Banaue Rice Terraces?\n", {{'A', "Ifugao"}, {'B', "Benguet"}, {'C', "Ilocos"}}, 'A'}
};

mutex mtx;
vector<Player> players;

void sendToPlayer(int sock, const string& msg) {
    send(sock, msg.c_str(), msg.size(), 0);
}

string recvFromPlayer(int sock) {
    char buffer[BUFFER_SIZE] = {0};
    recv(sock, buffer, BUFFER_SIZE, 0);
    return string(buffer);
}

void handlePlayer(int clientSocket, int id) {
    sendToPlayer(clientSocket, "Enter name: ");
    string name = recvFromPlayer(clientSocket);
    lock_guard<mutex> lock(mtx);
    players.push_back({clientSocket, name});
    cout << name << " joined the game.\n";
}

void runGame() {
    for (int round = 0; round < 3; ++round) {
        Question* q = nullptr;
        for (auto& qu : questionList) {
            if (!qu.hasAppeared) {
                q = &qu;
                q->hasAppeared = true;
                break;
            }
        }
        if (!q) break;

        string qText = q->question;
        for (auto& c : q->choices) {
            qText += string(1, c.first) + ". " + c.second + "\n";
        }

        for (auto& p : players) {
            sendToPlayer(p.socket, "\nRound " + to_string(round + 1) + "\n" + qText + "\nYour answer (A/B/C): ");
            string ans = recvFromPlayer(p.socket);
            p.answer = toupper(ans[0]);
        }

        for (auto& p : players) {
            if (p.answer == q->answer) {
                p.score += 10;
            }
        }

        string scoreboard = "\nSCOREBOARD\n------------------\n";
        for (auto& p : players) {
            scoreboard += p.name + ": " + to_string(p.score) + " points\n";
        }

        for (auto& p : players) {
            sendToPlayer(p.socket, scoreboard);
        }
    }

    string result = "\nFINAL RESULTS\n------------------\n";
    int highest = 0;
    for (auto& p : players) highest = max(highest, p.score);
    for (auto& p : players) result += p.name + ": " + to_string(p.score) + " points\n";

    vector<Player*> winners;
    for (auto& p : players) {
        if (p.score == highest) winners.push_back(&p);
    }

    if (winners.size() == 1) {
        result += "\nWinner: " + winners[0]->name + "\n";
        for (auto& p : players) sendToPlayer(p.socket, result);
        return;
    }

    result += "\nIt's a tie between: ";
    for (auto* p : winners) result += p->name + " ";
    result += "\nEntering Tiebreaker Round...\n";
    for (auto& p : players) sendToPlayer(p.socket, result);

    while (true) {
        Question* q = nullptr;
        for (auto& qu : questionList) {
            if (!qu.hasAppeared) {
                q = &qu;
                qu.hasAppeared = true;
                break;
            }
        }
        if (!q) {
            for (auto& p : players) sendToPlayer(p.socket, "\nNo more questions available for tiebreaker!\n");
            return;
        }

        string qText = "\nTIEBREAKER QUESTION\n" + q->question;
        for (auto& c : q->choices) {
            qText += string(1, c.first) + ". " + c.second + "\n";
        }

        for (auto* p : winners) {
            sendToPlayer(p->socket, qText + "\nYour answer (A/B/C): ");
            string ans = recvFromPlayer(p->socket);
            p->answer = toupper(ans[0]);
        }

        for (auto* p : winners) {
            if (p->answer == q->answer) {
                p->score += 10;
            }
        }

        int topScore = 0;
        for (auto* p : winners) topScore = max(topScore, p->score);
        vector<Player*> newWinners;
        for (auto* p : winners) {
            if (p->score == topScore) newWinners.push_back(p);
        }

        string tbStatus = "\nTIEBREAKER SCOREBOARD\n------------------\n";
        for (auto* p : winners) {
            tbStatus += p->name + ": " + to_string(p->score) + " points\n";
        }
        for (auto& p : players) sendToPlayer(p.socket, tbStatus);

        if (newWinners.size() == 1) {
            string winMsg = "\nTiebreaker Winner: " + newWinners[0]->name + "\n";
            for (auto& p : players) sendToPlayer(p.socket, winMsg);
            return;
        }

        winners = newWinners;
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, MAX_PLAYERS);

    cout << "Waiting for players to connect...\n";

    vector<thread> threads;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        threads.emplace_back(handlePlayer, new_socket, i);
    }

    for (auto& t : threads) t.join();

    cout << "All players joined. Starting game...\n";
    runGame();

    for (auto& p : players) close(p.socket);
    close(server_fd);
    return 0;
}
