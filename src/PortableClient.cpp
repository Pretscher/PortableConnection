#include "PortableClient.hpp"
#include <iostream>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

const int recvbuflen = 512;
/**
 * @brief Starts up client(on Windows, not necessary on Linux), does not yet try to connect to a server
 */
PortableClient::PortableClient() : Socket() {
#ifdef _WIN32
    WSADATA wsaData;
    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(iResult != 0) {
        cout << "Client WSAStartup failed with error: \n" << iResult;
        return;
    }
#endif
}

/**
 * @brief Request the client count from the server to calculate this client's index in the servers client array
 *
 */
void PortableClient::getMyIndexFromServer(int serverIndex) {
    sendToSocket(serverIndex, "getMyClientIndex");
    string myClientIndex = receiveNextMessage(serverIndex);
    myClientIndex[serverIndex] = std::stoi(myClientIndex);
}

/**
 * @brief Tries to connect to the server with the passed ip. Gets client index from server for later operations.
 *
 * @param ip
 */
void PortableClient::connectToServer(string ip) {
    if(loggingEnabled == true) cout << "CLIENT_SOCKET: Trying to connect to server with ip " << ip << "\n";
    //search for socket matching ip
    avHostsMtx.lock();
    auto copy = avHosts;
    avHostsMtx.unlock();
    connectSocketsMtx.lock();//very important mutex. We DO NOT stop the searching threads when the time is over, thus we still have to use the mutex here.
    for(int i = 0; i < copy.size(); i++) {
        if(copy.at(i).compare(ip) == 0) {
            addSocket(connectSockets.at(i));
            getMyIndexFromServer(sockets.size() - 1);//get index from this new server
        }
        else {
            portableShutdown(connectSockets.at(i));
        }
    }
    connectSockets.clear();
    connectSocketsMtx.unlock();
    if(loggingEnabled == true) cout << "CLIENT_SOCKET: successfully connected to server!\n";
}

//used by searchHostMultithreaded() and testIP()
const unsigned int checkedIpCount = 255;
thread* threads;
mutex* mutices;
bool* threadFinished;
mutex* threadFinishedMtx;
string foundIP;
void testIP(string myIP, int index, PortableClient* client);

/**
 * @brief Searches for local hosts in multiple threads.
 *
 * @param client normally just use "this"
 */
void searchHostsMultiThreaded(PortableClient* client) {
    threads = new thread[checkedIpCount];
    mutices = new mutex[checkedIpCount];
    threadFinished = new bool[checkedIpCount];
    threadFinishedMtx = new mutex[checkedIpCount];
    foundIP = "";

    string myIP = client->getIP();
    for(int i = 0; i < 2; i++) {
        myIP.pop_back();
    }
    //go through possible ip adresses with the same start as our home network. only change last 3 digits
    for(int i = 1; i < checkedIpCount; i++) {
        //set i as(possibly 3) last digits of i
        int prevSize = myIP.size();//save to go back to original size after appending 1 to 3 digits
        myIP.append(to_string(i));
        //start new thread that tests this ip (has to be done this way because recv and connect block the thread)
        threads[i] = std::thread(&testIP, myIP, i, client);

        //delete appended digits
        for(int i = myIP.size(); i > prevSize; i--) {
            myIP.pop_back();
        }
    }

    while(true) {
        this_thread::sleep_for(chrono::milliseconds(5));
        if(foundIP != "") {
            client->pushToAvailableHosts(std::move(foundIP));//foundIP is now empty again
        }

        //check if all threads finished searching. If so, end this thread.
        bool finished = true;
        for(int i = 1; i < checkedIpCount; i++) {
            threadFinishedMtx[i].lock();
            if(threadFinished[i] == false) {
                finished = false;
            }
            threadFinishedMtx[i].unlock();
        }
        if(finished == true) {
           // cout << "All threads for searching hosts deleted. Trying to free memory: \n";
            //this_thread::sleep_for(1000ms);
          //  delete[] threads;
           // delete[] mutices;
            //delete[] threadFinished;
        }
    }
}


void PortableClient::searchHosts(int waitTime) {
    searchingHosts = std::thread(&searchHostsMultiThreaded, this);
    this_thread::sleep_for(chrono::milliseconds(waitTime));
}
/**
 * @brief checks if the ip passed is a server socket.
 *
 * @param myIP
 * @param index
 * @param client
 */
void testIP(string myIP, int myIndex, PortableClient* client) {
    threadFinished[myIndex] = false;
    bool connectSuccess = false;
    //tempConnectSocket has different data type depending on OS so uhm... this code is awful but it does the job
#ifdef __linux__ 
    int tempConnectSocket = client->portableConnect(myIP);
    if(tempConnectSocket >= 0) connectSuccess = true;
#elif _WIN64
    SOCKET tempConnectSocket = client->portableConnect(myIP);
    if(tempConnectSocket != INVALID_SOCKET) connectSuccess = true;
#endif
    if(connectSuccess == true) {
        client->portableSend(tempConnectSocket, "12345");
        char* recvBuf = new char[recvbuflen];
        int res = client->portableRecv(tempConnectSocket, recvBuf);
        string msg;
        //save message
        for(int i = 0; i < res; i++) {
            msg.push_back(recvBuf[i]);
        }
        delete[] recvBuf;

        if(msg.compare("12345") == 0) {
            client->addConnectSocket(tempConnectSocket);
            foundIP = myIP;//this will be pushed back to string
        }
    }
    threadFinishedMtx[myIndex].lock();
    threadFinished[myIndex] = true;
    threadFinishedMtx[myIndex].unlock();
}



//Portable functions used on both operating systems for the above code:--------------------------------------------------------------------





#ifdef __linux__ 
int PortableClient::portableConnect(string connectIP) {
    int listenSocket;
    // Attempt to connect to an address until one succeeds
    // Create a SOCKET for connecting to server
    if((listenSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "\n Socket creation error \n";
        cout << "Connection to server with ip << " << connectIP << " failed\n";
        exit(0);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // Convert IPv4 and IPv6 addresses from text to binary form
    inet_pton(AF_INET, connectIP.c_str(), &serv_addr.sin_addr);
    if(connect(listenSocket,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) >= 0) {
        return listenSocket;
    }
    return -1;
}
#elif _WIN64
SOCKET PortableClient::portableConnect(string connectIP) {
    struct addrinfo* result = nullptr;
    struct addrinfo* hints = new addrinfo();

    int res = getaddrinfo(connectIP.c_str(), port.c_str(), hints, &result);
    if(res != 0) {
        cout << "Client getaddrinfo failed with error: \n";
        WSACleanup();
        return INVALID_SOCKET;
    }
    if(result != nullptr) {
        ZeroMemory(hints, sizeof(*hints));
        hints->ai_family = AF_UNSPEC;
        hints->ai_socktype = SOCK_STREAM;
        hints->ai_protocol = IPPROTO_TCP;

        // Create a SOCKET for connecting to server
        SOCKET listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if(listenSocket == INVALID_SOCKET) {
            cout << "Client socket connection failed with error: %ld\n" << WSAGetLastError();
            WSACleanup();
            return INVALID_SOCKET;
        }

        res = connect(listenSocket, result->ai_addr,(int) result->ai_addrlen);
        if(res == SOCKET_ERROR) {
            closesocket(listenSocket);
            return INVALID_SOCKET;
        }
        freeaddrinfo(result);
        delete hints;
        return listenSocket;
    }
    return INVALID_SOCKET;
}
#endif