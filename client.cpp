#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <stdexcept>

using namespace std;

string GetUserInput(){
    string message;
    cout << ">";
    getline(cin, message);
    return message;
}

void* Konusucu(void* arg);
void* Dinleyici(void* arg);

bool BaglantiyiKapat = false;

int main(){

    struct sockaddr_in ListenerAddr;
    socklen_t ListenerAddrLen = sizeof(ListenerAddr);
    int SunucuPort = 9190;
    const char ServerIP[] = "127.0.0.1";
    int ClientSocket;

    if((ClientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        cout << "Socket error." << endl;
        return 0;
    }

    ListenerAddr.sin_family = AF_INET;
    ListenerAddr.sin_port = htons(SunucuPort);

    if(inet_pton(AF_INET, ServerIP, &ListenerAddr.sin_addr) <= 0){
        cout << "IP error." << endl;
    }

    while (true)
    {
        int c = connect(
            ClientSocket,
            (struct sockaddr*)&ListenerAddr,
            sizeof(ListenerAddr)
        );
        cout << c << endl;
        if(c > -1) break;
        printf("Waiting for connection.\n");
        sleep(1);
    }
    
    cout << "Connected" << endl;

    pthread_t Konusma;
    pthread_t Dinleme;
    pthread_create(&Konusma, NULL, Konusucu, &ClientSocket);
    pthread_create(&Dinleme, NULL, Dinleyici, &ClientSocket);
    pthread_join(Konusma, NULL);
    pthread_join(Dinleme, NULL);

    return 0;
}

void* Konusucu(void* arg){
    int ClientSocket = *(int*)arg;
    char mesaj[1024] = {0};

    while (!BaglantiyiKapat)
    {
        string userInput = GetUserInput();
        send(ClientSocket, userInput.c_str(), userInput.length(), 0);
        if(userInput == "logout") break;
    }
    BaglantiyiKapat = true;
    close(ClientSocket);
    return NULL;
}

void* Dinleyici(void* arg){
    int ClientSocket = *(int*)arg;
    char mesaj[1024] = {0};
    while(!BaglantiyiKapat){
        memset(mesaj, 0, sizeof(mesaj));
        int oku = read(ClientSocket, mesaj, 1024);
        if(oku > 0){
            cout << mesaj << endl;
        }
    }
    return NULL;
}
