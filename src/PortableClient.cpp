#include "PortableClient.hpp"
#include <iostream>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
using namespace std;

const int recvbuflen = 512;
/**
 * @brief Starts up client(on Windows, not necessary on Linux), does not yet try to connect to a server
 */
PortableClient::PortableClient() {
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
void PortableClient::getMyIndexFromServer() {
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
    if(loggingEnabled == true) cout << "CLIENT_SOCKET: Trying to connect to server with ip " << ip << "\n";
    //search for socket matching ip
    avHostsMtx.lock();
    auto copy = avHosts;
    avHostsMtx.unlock();
    connectSocketsMtx.lock();//very important mutex. We DO NOT stop the searching threads when the time is over, thus we still have to use the mutex here.
    for(int i = 0; i < copy.size(); i++) {
        if(copy.at(i).compare(ip) == 0) {
            serverSocket = connectSockets.at(i);//same index, pushed back simultanioisly
            getMyIndexFromServer();//get index in server's client array
        }
        else {
            portableShutdown(connectSockets.at(i));
        }
    }
    connectSockets.clear();
    connectSocketsMtx.unlock();
    setConnected(true);
    if(loggingEnabled == true) cout << "CLIENT_SOCKET: successfully connected to server!\n";
}

/**
 * @brief Sends a message to the server. Cannot send another message until the server answers.
 *
 * @param message
 */
void PortableClient::sendToServer(string message) {
    if(getWait() == false) {
        portableSend(serverSocket, message.c_str());
        if(loggingEnabled == true) cout << "Sent message '" << message << "' to server\n";
        setWait(true);
    }
}

/**
 * @brief Reads the message in the passed receive-buffer with lenght msgLenght. Makes sending the next message possible(in portableRecv).
 *
 * @param msgLenght Amount of characters that should be read
 * @param recvbuf
 * @return string
 */
string PortableClient::readMsgBuffer(int msgLenght, char* recvbuf) {
    string msg = string();
    if(msgLenght > 0) {
        setGotNewMessage(true);
        //save message
        for(int i = 0; i < msgLenght; i++) {
            msg.push_back(recvbuf[i]);
        }
    }
    if(msgLenght < 0) {
        cout << "error, client received message with negative lenght";
        exit(0);
    }

    if(msg.compare(getLastMessage()) != 0) {
        if(loggingEnabled == true) cout << "Received message '" << msg << "' from server\n";
    }
    return msg;
}

/**
 * @brief Receives all messages in a seperate thread that you have to create. Sending happens in the main thread.
 */
void PortableClient::receiveMultithreaded() {
    while(true) {
        char recvbuf[recvbuflen];
        int msgLenght = portableRecv(serverSocket, recvbuf);
        setLastMessage(readMsgBuffer(msgLenght, recvbuf));
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    portableShutdown(serverSocket);
}

bool PortableClient::newMessage() {
    bool temp = hasNewMessage();
    setGotNewMessage(false);
    return temp;
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

    string myIP = client->getMyIP();
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
            //cout << "All threads for searching hosts deleted\n";
            delete[] threads;
            delete[] mutices;
            delete[] threadFinished;
        }
    }
}



//Portable functions used on both operating systems for the above code:--------------------------------------------------------------------



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
    int tempConnectSocket = client->portableConnect(myIP.c_str());
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

void PortableClient::searchHosts(int waitTime) {
    searchingHosts = std::thread(&searchHostsMultiThreaded, this);
    this_thread::sleep_for(chrono::milliseconds(waitTime));
}

#ifdef __linux__ 
int PortableClient::portableSend(int socket, string message) const {
    int result = send(socket, message.c_str(), message.size(), 0);
    return result;
}

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

void PortableClient::portableShutdown(int socket) {
    shutdown(socket, SHUT_RDWR);
}

int PortableClient::portableRecv(int socket, char* recvBuffer) {
    int out = read(socket, recvBuffer, recvbuflen);
    setWait(false);
    return out;
}

#elif _WIN64

void PortableClient::portableShutdown(SOCKET& socket) {
    // shutdown the connection since we're done
    int iResult = shutdown(socket, SD_SEND);
    if(iResult == SOCKET_ERROR) {
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
    int result = send(socket, message,(int) strlen(message), 0);
    if(result == SOCKET_ERROR) {
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

string PortableClient::getMyIP() const {
#ifdef __linux__ 
    struct ifaddrs* ifaddr, * ifa;
    int family, s;
    char host[265];

    if(getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == NULL)
            continue;

        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, 265, NULL, 0, 1);

        if( /*(strcmp(ifa->ifa_name,"wlan0")==0)&&( */ ifa->ifa_addr->sa_family == AF_INET) // )
        {
            if(s != 0)
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
    if((he = gethostbyname(hostname)) == NULL) {
        std::cout << "gethostbyname error" << std::endl;
        return string();
    }
    else {
        addr_list =(struct in_addr**) he->h_addr_list;
        return string(inet_ntoa(*addr_list[0]));
    }
#endif
}