#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "80"

using namespace std;

int __cdecl main(int argc, char** argv)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    const char* sendbuf = "14";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Send your name
    cout << "Please enter your name: ";
    string clientName;
    string sendMessage = "Name: ";
    cin >> clientName;
    sendMessage+=clientName;

    iResult = send(ConnectSocket, sendMessage.c_str(), sendMessage.length() * 2, 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Wait for server responses
    bool flag = true;
    bool isWritingTime = false;
    while (true) {
        if (flag) {
            cout << "Waiting for teacher ..." << endl;
            flag = false;
        }
        if (!flag) {
            if (isWritingTime == true) {
                string outcome;
                getline(cin,outcome);
                if (outcome == "")  continue;
                sendMessage = "Idea: " + outcome;

                iResult = send(ConnectSocket, sendMessage.c_str(), sendMessage.length() * 2, 0);
            }
            iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                string income = "";
                income += recvbuf;
                if (income != "")
                    cout << income <<endl;
                if (income.find("tossed") != string::npos) {
                    break;
                }
                else if (income.find("Now you can write") != string::npos) {
                    cout << "-------------------------------------------------" << endl;
                    cout << "|                    Writing                    |" << endl;
                    cout << "-------------------------------------------------" << endl;
                    isWritingTime = true;
                }
                else if (income.find("Stop") != string::npos) {
                    isWritingTime = false;
                    break;
                }
            }
        }
    }

    // Voiting system
    while (true) {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            string income = "";
            income += recvbuf;
            if (income.find("Voiting") != string::npos) {
                cout << "-------------------------------------------------" << endl;
                cout << "|                    Voiting                    |" << endl;
                cout << "-------------------------------------------------" << endl;
                sendMessage = "Ready for voiting";
                send(ConnectSocket, sendMessage.c_str(), sendMessage.length() * 2, 0);
                while (true) {
                    recv(ConnectSocket, recvbuf, recvbuflen, 0);
                    income = "";
                    income += recvbuf;
                    if (income.find("END") != string::npos) break;
                    printf("%s", recvbuf);
                    sendMessage = "Saved";
                    send(ConnectSocket, sendMessage.c_str(), sendMessage.length() * 2, 0);
                }
                continue;
            }
            if (income.find("Vote now") != string::npos) {
                cout << "Now wote for winner:" << endl;
                for (int i = 0; i < 3; ++i) {
                    string winnerNumber = "";
                    cout << "Winner is: ";
                    cin >> winnerNumber;
                    send(ConnectSocket, winnerNumber.c_str(), winnerNumber.length() * 2, 0);
                }
                continue;
            }
            if (income.find("winner") != string::npos) {
                cout << "-----------------------------------------------" << endl;
                cout << "|                    Winers                    |" << endl;
                cout << "-----------------------------------------------" << endl;
                income = "";
                income += recvbuf;
                cout << income;
                break;
            }
        }
    }
    
    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }
    
    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}