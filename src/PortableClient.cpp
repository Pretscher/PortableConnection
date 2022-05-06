#include "PortableClient.hpp"
#include <iostream>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

const int recvbuflen = 512;
shared_ptr<mutex> mtx = shared_ptr<mutex>(new mutex());
/**
 * @brief Starts up client (on Windows, not necessary on Linux), does not yet try to connect to a server
 */
PortableClient::PortableClient() {
#ifdef _WIN32
    WSADATA wsaData;
    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "Client WSAStartup failed with error: \n" << iResult;
        return;
    }
#endif
}

/**
 * @brief Request the client count from the server to calculate this client's index in the servers client array
 * 
 */
void PortableClient::getMyIndex() {
    this->sendToServer("getMyClientIndex");
    //receive my clientIndex
    char recvbuf[recvbuflen];
    int msgLen = this->portableRecv(this->serverSocket, recvbuf);
    string myClientIndex = readMsgBuffer(msgLen, recvbuf);

    this->myPlayerIndex = std::stoi(myClientIndex);
}        

/**
 * @brief Tries to connect to the server with the passed ip. Gets client index from server for later operations.
 * 
 * @param ip 
 */
void PortableClient::connectToServer(string ip) {
    auto copy = avHosts;
    for (int i = 0; i < copy.size(); i++) {
        if (copy.at(i).compare(ip) == 0) {
            serverSocket = connectSockets.at(i);//same index, pushed back simultanioisly
            getMyIndex();//get index in server's client array
        }
        else {
            portableShutdown(connectSockets.at(i));
        }
    }
    connectSockets.clear();
    setConnected(true);
    cout << "Client successfully connected to server!\n";
}

/**
 * @brief Sends a message to the server. Cannot send another message until the server answers.
 * 
 * @param message 
 */
void PortableClient::sendToServer(string message) {
    if (getWait() == false) {
        portableSend(serverSocket, message.c_str());
        setWait(true);
    }
}

/**
 * @brief Reads the message in the passed receive-buffer with lenght msgLenght. Makes sending the next message possible (in portableRecv).
 * 
 * @param msgLenght Amount of characters that should be read
 * @param recvbuf
 * @return string 
 */
string PortableClient::readMsgBuffer(int msgLenght, char* recvbuf) {
    string msg = string();
    if (msgLenght > 0) {
        mtx->lock();
        gotNewMessage = true;
        //save message
        for (int i = 0; i < msgLenght; i++) {
            msg.push_back(recvbuf[i]);
        }
        mtx->unlock();
    }
    if (msgLenght < 0) {
         cout << "error, client received message with negative lenght";
         exit(0);
    }
    return msg;
}

/**
 * @brief Receives all messages in a seperate thread that you have to create. Sending happens in the main thread.
 */
void PortableClient::receiveMultithreaded() {
    while (true) {
        char recvbuf[recvbuflen];
        int msgLenght = portableRecv(serverSocket, recvbuf);
        lastMessage = readMsgBuffer(msgLenght, recvbuf);

        this_thread::sleep_for(chrono::milliseconds(1));
    }
    portableShutdown(serverSocket);
}

string PortableClient::getLastMessage() const {
    return lastMessage;
}

shared_ptr<mutex> PortableClient::getMutex() const {
    return mtx;
}

bool PortableClient::isConnected() const {
    return isConnected();
}

bool PortableClient::newMessage() {
    bool temp = gotNewMessage;
    gotNewMessage = false;
    return temp;
}

string PortableClient::getIP() const {
#ifdef __linux__ 
    struct ifaddrs* ifaddr, * ifa;
    int family, s;
    char host[265];

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, 265, NULL, 0, 1);

        if ( /*(strcmp(ifa->ifa_name,"wlan0")==0)&&( */ ifa->ifa_addr->sa_family == AF_INET) // )
        {
            if (s != 0)
            {
                cout << "getnameinfo() failed: %s\n", gai_strerror(s);
                exit(EXIT_FAILURE);
            }
        }
    }
    freeifaddrs(ifaddr);
    return string(host);

#elif _WIN64
    char hostname[255];
    struct hostent* he;
    struct in_addr** addr_list;

    WSAData data;
    WSAStartup(MAKEWORD(2, 2), &data);

    gethostname(hostname, 255);
    if ((he = gethostbyname(hostname)) == NULL) {
        std::cout << "gethostbyname error" << std::endl;
        return string();
    }
    else {
        addr_list = (struct in_addr**) he->h_addr_list;
        return string(inet_ntoa(*addr_list[0]));
    }
#endif
  
}

const unsigned int checkedIpCount = 128;
thread threads[checkedIpCount];
mutex mutices[checkedIpCount];
bool threadFinished[checkedIpCount];
string foundIP = "";

/**
 * @brief checks if the ip passed is a server socket.
 * 
 * @param myIP 
 * @param index 
 * @param client 
 */
void testIP(const char myIP[], int index, PortableClient* client) {
    threadFinished[index] = false;
    bool connectSuccess = false;
    //tempConnectSocket has different data type depending on OS so uhm... this code is awful but it does the job
#ifdef __linux__ 
    int tempConnectSocket = client->portableConnect(myIP);
    if (tempConnectSocket >= 0) connectSuccess = true;
#elif _WIN64
    SOCKET tempConnectSocket = client->portableConnect(myIP);
    if (tempConnectSocket != INVALID_SOCKET) connectSuccess = true;
#endif
    if(connectSuccess == true) {
        client->portableSend(tempConnectSocket, "12345");

        long long startTime = std::chrono::system_clock::now().time_since_epoch().count();
        while (std::chrono::system_clock::now().time_since_epoch().count() - startTime < 200) {
            this_thread::sleep_for(chrono::milliseconds(5));
            char* recvBuf = new char[recvbuflen];
            int res = client->portableRecv(tempConnectSocket, recvBuf);
            string msg;
            //save message
            for (int i = 0; i < res; i++) {
                msg.push_back(recvBuf[i]);
            }
            delete[] recvBuf;

            if (msg.compare("12345") == 0) {
                client->connectSockets.push_back(std::move(tempConnectSocket));
                foundIP = myIP;//this will be pushed back to string. 
                break;//dont set threadFinished to true so that no multithreading error can occur where the filled string is ignored
            }
        }
    }
    threadFinished[index] = true;
}

/**
 * @brief Searches for hosts in multiple threads.
 * 
 * @param client 
 */
void searchHostsMultiThreaded(PortableClient* client) {
    string myIP = client->getIP();
    for (int i = 0; i < 2; i++) {
        myIP.pop_back();
    }
    for (int i = 1; i < checkedIpCount; i++) {
        //set i as (possibly 3) last digits of i�
        int prevSize = myIP.size();
        myIP.append(to_string(i));

        threads[i] = std::thread(&testIP, (new string(myIP))->c_str(), i, client);

        //delete appended numbers
        for (int i = myIP.size(); i > prevSize; i--) {
            myIP.pop_back();
        }
        if (foundIP != "") {
            break;
        }
    }

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(5));
        if (foundIP != "") {
            client->pushToAvailableHosts(std::move(foundIP));
            foundIP = "";
        }

        //check if finished without connecting
        bool finished = true;
        for (int i = 0; i < checkedIpCount; i++) {
            if (threadFinished[i] == false) {
                finished = false;
            }
        }
        if (finished == true) {
            return;
        }
    }
}

/**
 * @brief Searches for hosts in local network that use sockets on this port.
 * 
 */
void PortableClient::searchHosts() {
    thread searchingHosts = std::thread(&searchHostsMultiThreaded, this);
}

#ifdef __linux__ 
int PortableClient::portableSend(int socket, const char* message) const {
    int result = send(socket, message, strlen(message), 0);
    return result;
}

int PortableClient::portableConnect(const char connectIP[]) {
    int listenSocket;
    // Attempt to connect to an address until one succeeds
    // Create a SOCKET for connecting to server
    if ((listenSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "\n Socket creation error \n";
        exit(0);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // Convert IPv4 and IPv6 addresses from text to binary form
    inet_pton(AF_INET, connectIP, &serv_addr.sin_addr);
    if (connect(listenSocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) >= 0) {
        return listenSocket;
    }
    return -1;
}
void PortableClient::portableShutdown(int socket) {
    shutdown(socket, SHUT_RDWR);
}

/**
 * @brief tries to receive a message and write it into the recvBuffer. 
 * Will block all action in this thread until message was received or time out
 * @param socket usually the server socket for a client
 * @param recvBuffer 
 * @return int <- lenght of the message.
 */
int PortableClient::portableRecv(int socket, char* recvBuffer) {
    int out = read(socket, recvBuffer, recvbuflen);
    setWait(false);
    return out;
}

#elif _WIN64

void PortableClient::portableShutdown(SOCKET& socket) {
    // shutdown the connection since we're done
    int iResult = shutdown(socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        cout << "Client shutdown failed with error: \n" << WSAGetLastError();
        closesocket(socket);
        WSACleanup();
        exit(0);
        return;
    }
    // cleanup
    closesocket(socket);
    WSACleanup();
}

int PortableClient::portableSend(SOCKET& socket, const char* message) const {
    int result = send(socket, message, (int) strlen(message), 0);
    if (result == SOCKET_ERROR) {
        cout << "Client Message Sending Error: \n" << message;
    }
    return result;
}

int PortableClient::portableRecv(SOCKET& socket, char* recvBuffer) {
    return recv(socket, recvBuffer, recvbuflen, 0);
}

SOCKET PortableClient::portableConnect(const char* connectIP) {
    struct addrinfo* result = nullptr;
    struct addrinfo* hints = new addrinfo();

    int res = getaddrinfo(connectIP, port.c_str(), hints, &result);
    if (res != 0) {
        cout << "Client getaddrinfo failed with error: \n";
        WSACleanup();
        return INVALID_SOCKET;
    }
    if (result != nullptr) {
        ZeroMemory(hints, sizeof(*hints));
        hints->ai_family = AF_UNSPEC;
        hints->ai_socktype = SOCK_STREAM;
        hints->ai_protocol = IPPROTO_TCP;

        // Create a SOCKET for connecting to server
        SOCKET listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (listenSocket == INVALID_SOCKET) {
            cout << "Client socket connection failed with error: %ld\n" << WSAGetLastError();
            WSACleanup();
            return INVALID_SOCKET;
        }

        res = connect(listenSocket, result->ai_addr, (int) result->ai_addrlen);
        if (res == SOCKET_ERROR) {
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
