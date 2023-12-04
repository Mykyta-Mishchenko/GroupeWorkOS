#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define FILE_PATH "Ideas.txt"
#define TIME_LENGTH 30

CRITICAL_SECTION gCriticalSection;
int connectedStudents = 0;

enum Status
{
    Connected,
    Work,
    Disconected
};

std::vector<Status> studentsStatus;
std::vector<HANDLE> studentsThreads;
std::vector<std::string> studentsNames;
std::vector<SOCKET> studetsSockets;

using namespace std;


DWORD WINAPI HandleClient(LPVOID clientSocket);

int __cdecl main(void)
{
    InitializeCriticalSection(&gCriticalSection);
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    // Asking for number of clients
    int clientsNumber = 0;
    int tempNumber = 0;
    cout << "Please enter clients number: "<<endl;
    cin >> clientsNumber;
    cout << "You are waiting for " << clientsNumber << " clients ..." << endl;
    tempNumber = clientsNumber;
    // Listening for a client
    iResult = listen(ListenSocket, SOMAXCONN);

    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    SOCKET clientSocket;

    while (clientsNumber>0) {
        // Accept a client connection
        clientSocket = accept(ListenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }

        // Create a thread to handle the client
        HANDLE clientThread = CreateThread(NULL, 0, HandleClient, &clientSocket, 0, NULL);
        studetsSockets.push_back(clientSocket);
        studentsThreads.push_back(clientThread); 
        studentsStatus.push_back(Connected); // say that this student connected
        --clientsNumber;
        if (clientThread == NULL) {
            printf("CreateThread failed with error: %d\n", GetLastError());
            closesocket(clientSocket);
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
    }
    clientsNumber = tempNumber;

    // Working with clients
    bool flag = true;
    auto start_time = chrono::high_resolution_clock::now();;

    while (true) {

        // Confirmation, that all clients come up
        if (flag && connectedStudents == clientsNumber) {
            cout << "All students come up" << endl;
            cout << "Studenst statuses: " << studentsStatus.size() << endl;
            flag = false;

            // Coosing necessary students
            cout << "Please select students you want to see on board: " << endl;
            for (int i = 0; i < studentsNames.size(); i++) {
                cout << i + 1 << ") " << studentsNames[i] << endl;
            }
            cout << "Input students group size: ";
            int numberOfSelectedStudents = 0;
            cin >> numberOfSelectedStudents;
            cout << "Input this students numbers: " << endl;
            for (int i = 0; i < numberOfSelectedStudents; i++) {
                int tempNumber;
                cin >> tempNumber;

                studentsStatus[tempNumber - 1] = Work; 
            }

            // Disconnect other students
            for (int i = 0; i < studentsStatus.size();) {
                if (studentsStatus[i] == Connected) {
                    studentsStatus[i] = Disconected;

                    string message = "You were tossed out";
                    send(studetsSockets[i], message.c_str(), message.length() * 2, 0);
                    closesocket(studetsSockets[i]);

                    studentsNames.erase(studentsNames.begin() + i);
                    studentsStatus.erase(studentsStatus.begin() + i);
                    studetsSockets.erase(studetsSockets.begin() + i);

                    TerminateThread(studentsThreads[i], 0);
                    CloseHandle(studentsThreads[i]);
                    studentsThreads.erase(studentsThreads.begin() + i);

                    continue;
                }
                ++i;
            }
            // Delete board file if it`s exist;
            DeleteFileA(FILE_PATH);

            cout << "Now this students can write on board: "<<endl;
            for (int i = 0; i < studentsNames.size(); i++) {
                cout << "Student: " << studentsNames[i] << endl;
                string message = "Now you can write: ";
                send(studetsSockets[i], message.c_str(), message.length() * 2, 0);
            }

            // Start timing
            start_time = chrono::high_resolution_clock::now();
        }
        else if(flag == false){
            auto end_time = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::seconds>(end_time - start_time);
            if (duration.count() >= TIME_LENGTH) {
                for (int i = 0; i < studentsNames.size(); i++) {
                    string message = "Stop";
                    send(studetsSockets[i], message.c_str(), message.length() * 2, 0);
                }
            }
        }
    }


    // No longer need server socket
    closesocket(ListenSocket);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

// Function that receives and send client info
DWORD WINAPI HandleClient(LPVOID client) {
    SOCKET clientSocket = *((SOCKET*)client);
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int currentStudentID = 0;
    string income = "";
    string outcome = "";
    string studentName;
    // Receive and send data with the client
    do {
        income = "";
        outcome = "";
        iResult = recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            income += recvbuf;
            // Check if we now just connect
            if (income.find("Name") != string::npos) {
                
                outcome = "Wait for other clients ...";
                cout << income << " come up" << endl;
                studentName = income;

                EnterCriticalSection(&gCriticalSection);

                currentStudentID = connectedStudents;
                studentsNames.push_back(income);
                ++connectedStudents;

                LeaveCriticalSection(&gCriticalSection);

            }
            else if(income.find("Idea") != string::npos) {
                EnterCriticalSection(&gCriticalSection);

                income += "\n";
                cout << studentName << endl << income;

                // Write ideas to file
                HANDLE hFile = CreateFileA(
                    FILE_PATH,
                    FILE_APPEND_DATA,
                    0,
                    NULL,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
                if (hFile != INVALID_HANDLE_VALUE) {
                    LPWSTR wideIdea = new wchar_t[income.size() + 1];
                    MultiByteToWideChar(CP_ACP, 0, income.c_str(), -1, wideIdea, income.size() + 1);
                    WriteFile(hFile, wideIdea, income.length() * 2, NULL, NULL);
                    CloseHandle(hFile);
                }
                outcome = "continue";

                LeaveCriticalSection(&gCriticalSection);
            }
            send(clientSocket, outcome.c_str(), outcome.length() * 2, 0);
        }
        else if (iResult == 0) {
            // Connection closed
            printf("Client disconnected.\n");
        }
        else {
            //printf("recv failed with error: %d\n", WSAGetLastError());
        }
    } while (iResult > 0);

    // Cleanup
    closesocket(clientSocket);
    return 0;
}
