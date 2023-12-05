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
#include <fstream>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "80"
#define FILE_PATH "Ideas.txt"
#define TIME_LENGTH 20

CRITICAL_SECTION gCriticalSection;
int connectedStudents = 0;
int votedStudents = 0;
bool isVoiting = false;

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
std::vector<std::string> voitingStrings;
std::vector<int> voitingVotes;

using namespace std;


DWORD WINAPI HandleClient(LPVOID clientSocket);
void WriteToFile(string text);
void WriteWinnersToFile(string text);
void ClearFile();

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
                break;
            }
        }
    }

    // Voiting part
    isVoiting = true;
    cout << "-------------------------------------------------" << endl;
    cout << "|                    Voiting                    |" << endl;
    cout << "-------------------------------------------------" << endl;
    for (int i = 0; i < studentsNames.size(); i++) {
        string message = "Voiting";
        send(studetsSockets[i], message.c_str(), message.length() * 2, 0);
    }
    for (int i = 0; i < voitingStrings.size(); ++i) {
        cout << voitingStrings[i];
    }

    // Check if all voted
    while (true) {
        if (votedStudents == studentsNames.size()) {
            for (int i = 0; i < voitingVotes.size(); ++i) {
                cout << "Votes: " << voitingVotes[i] << "    " << voitingStrings[i];
            }
            int firstMax = INT_MIN, secondMax = INT_MIN, thirdMax = INT_MIN;
            int firstIndex = -1, secondIndex = -1, thirdIndex = -1;

            // Finding three winners
            for (int i = 0; i < voitingVotes.size(); ++i) {
                int num = voitingVotes[i];

                if (num > firstMax) {
                    thirdMax = secondMax;
                    thirdIndex = secondIndex;

                    secondMax = firstMax;
                    secondIndex = firstIndex;

                    firstMax = num;
                    firstIndex = i;
                }
                else if (num > secondMax) {
                    thirdMax = secondMax;
                    thirdIndex = secondIndex;

                    secondMax = num;
                    secondIndex = i;
                }
                else if (num > thirdMax) {
                    thirdMax = num;
                    thirdIndex = i;
                }
            }
            // Cout out vinners
            cout << "------------------------------------------------" << endl;
            cout << "|                    Winers                    |" << endl;
            cout << "------------------------------------------------" << endl;
            string winners = "First winner with " + to_string(firstMax) + " votes is: \n" + voitingStrings[firstIndex] + "\n";
            winners += "Second winner with " + to_string(secondMax) + " votes is: \n" + voitingStrings[secondIndex] + "\n";
            winners += "Third winner with " + to_string(thirdMax) + " votes is: \n" + voitingStrings[thirdIndex] + "\n";
            cout << winners;

            for (int i = 0; i < studentsThreads.size();) {
                if (studentsStatus[i] == Work) {
                    studentsStatus[i] = Disconected;

                    TerminateThread(studentsThreads[i], 0);
                    CloseHandle(studentsThreads[i]);
                    studentsThreads.erase(studentsThreads.begin() + i);

                    continue;
                }
                ++i;
            }

            for (int i = 0; i < studetsSockets.size(); ++i) {
                send(studetsSockets[i], winners.c_str(), winners.length() * 2, 0);
            }

            // Write results to file
            ClearFile();
            WriteWinnersToFile(winners);
            break;
        }
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // shutdown the connection since we're done
    for (int i = 0; i < studetsSockets.size(); ++i) {
        shutdown(studetsSockets[i], SD_SEND);
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

                studentsNames.push_back(income);
                ++connectedStudents;

                LeaveCriticalSection(&gCriticalSection);

            }
            else if (income.find("Idea") != string::npos && isVoiting) {
                string message = "Voiting";
                send(clientSocket, message.c_str(), message.length() * 2, 0);
                continue;
            }
            else if(income.find("Idea") != string::npos) {
                EnterCriticalSection(&gCriticalSection);

                income += "\n";
                cout << studentName << endl << income;

                // Write ideas to file
                WriteToFile(income);
                string tempLine = to_string(voitingStrings.size() + 1) + ") ";
                tempLine += income;
                voitingStrings.push_back(tempLine);
                voitingVotes.push_back(0);
                outcome = "continue";

                LeaveCriticalSection(&gCriticalSection);
            }
            else if (income.find("Ready for voiting") != string::npos) {
                for (int i = 0; i < voitingStrings.size(); ++i) {
                    send(clientSocket, voitingStrings[i].c_str(), voitingStrings[i].length() * 2, 0);
                    recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);
                    income = "";
                    income += recvbuf;
                    if (income.find("Saved") != string::npos) continue;
                    else break;
                }
                outcome = "END";
                send(clientSocket, outcome.c_str(), outcome.length() * 2, 0);
                outcome = "Vote now";
                send(clientSocket, outcome.c_str(), outcome.length() * 2, 0);

                // Now ew count votes
                vector<int> studentVotes(voitingVotes.size());
                for (int i = 0; i < 3; ++i) {
                    recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);
                    int vote = atoi(recvbuf);
                    if(vote<= studentVotes.size())
                        studentVotes[vote - 1] += 1;
                }
                EnterCriticalSection(&gCriticalSection);

                for (int i = 0; i < voitingVotes.size(); ++i) {
                    if (studentVotes[i] > 0) voitingVotes[i] += 1;
                }
                outcome = "END";

                ++votedStudents;

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

// Write to file function
void WriteToFile(string text) {
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
        LPWSTR wideIdea = new wchar_t[text.size() + 1];
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wideIdea, text.size() + 1);
        WriteFile(hFile, wideIdea, text.length() * 2, NULL, NULL);
        CloseHandle(hFile);
    }
}

// Write Winners to file
void WriteWinnersToFile(string text) {
    HANDLE hFile = CreateFileA(
        FILE_PATH,
        FILE_WRITE_DATA,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        LPWSTR wideIdea = new wchar_t[text.size() + 1];
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wideIdea, text.size() + 1);
        WriteFile(hFile, wideIdea, text.length() * 2, NULL, NULL);
        CloseHandle(hFile);
    }
}

// clear file
void ClearFile() {
    HANDLE hFile = CreateFileA(
        FILE_PATH,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    SetEndOfFile(hFile);
    CloseHandle(hFile);
}