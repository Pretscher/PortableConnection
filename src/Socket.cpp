#include "Socket.hpp"


string Socket::getIP() const {
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
                cout << "getnameinfo() failed: " << gai_strerror(s) << "\n";
                exit(EXIT_FAILURE);
            }
            cout << "\tInterface : <" << ifa->ifa_name << ">\n";
            cout << "\t  Address : <" << host << ">\n";
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
    return string();
#endif
}


#ifdef __linux__ 
int Socket::portableSend(int socket, const char* message) const {
    int result = send(socket, message, strlen(message), 0);
    return result;
}

int Socket::portableRecv(int socket, char* recvBuffer) {
    return read(socket, recvBuffer, recvbuflen);
}

void Socket::portableShutdown(int socket) {
    shutdown(socket, SHUT_RDWR);
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
#endif

/**
 * @brief Reads the message in the passed receive-buffer with lenght msgLenght. Makes sending the next message possible(in portableRecv).
 *
 * @param msgLenght Amount of characters that should be read
 * @param recvbuf
 * @return string
 */
string Socket::readMsgBuffer(int msgLenght, char* recvbuf) {
    string msg = string();
    if(msgLenght > 0) {
        //save message
        for(int i = 0; i < msgLenght; i++) {
            msg.push_back(recvbuf[i]);
        }
    }
    if(msgLenght < 0) {
        cout << "error, client received message with negative lenght";
        exit(0);
    }
    return msg;
}

void Socket::receiveMultithreaded(int socketIndex) {
    this_thread::sleep_for(chrono::milliseconds(1));
    char* recvBuffer = new char[recvbuflen];
    int msgLenght = portableRecv(getSocket(socketIndex), recvBuffer);
    if(msgLenght > 0) setGotNewMessage(socketIndex, true);
    string newMsg = readMsgBuffer(msgLenght, recvBuffer);

    if(loggingEnabled == true && newMsg.compare(getLastMessage(socketIndex)) != 0) {
        cout << "Received message '" << newMsg << "' from server\n";
    }
    //respondToCommands(socketIndex); TODO implement in server, that this is also executed
    
}

bool Socket::newMessage(int socketIndex) {
    bool temp = hasNewMessage(socketIndex);
    setGotNewMessage(socketIndex, false);
    return temp;
}


vector<string> Socket::getLastMessages() {
    vector<string> temp = lastMessages;//copy
    return temp;
}

void Socket::sendToSocket(int socketIndex, string message) {
    if(isWaiting(socketIndex) == false) {
        int iResult = portableSend(getSocket(socketIndex), message.c_str());
        if(loggingEnabled == true) cout << "Sent message '" << message << "' to client " << socketIndex << "\n";
        setWait(socketIndex, true);
    }
}

#ifdef  __linux__
void Socket::addSocket(int socket) {
    socketMtx.lock();
    sockets.push_back(socket);
    socketMtx.unlock();
    int mutexIndex = lastMessages.size();
    waitMutices[mutexIndex].lock();
    wait.push_back(false);
    waitMutices[mutexIndex].unlock();
    lastMessageMutices[mutexIndex].lock();
    lastMessages.push_back(string());
    lastMessageMutices[mutexIndex].unlock();

    gotNewMessageMutices[mutexIndex].lock();
    gotNewMessage.push_back(false);
    gotNewMessageMutices[mutexIndex].unlock();
}

#elif _WIN32
void addSocket(SOCKET socket) {
    socketMtx.lock();
    sockets.push_back(socket);
    socketMtx.unlock();
    int mutexIndex = lastMessages.size();
    waitMutices[mutexIndex].lock();
    wait.push_back(false);
    waitMutices[mutexIndex].unlock();
    lastMessageMutices[mutexIndex].lock();
    lastMessages.push_back(string());
    lastMessageMutices[mutexIndex].unlock();

    gotNewMessageMutices[mutexIndex].lock();
    gotNewMessage.push_back(false);
    gotNewMessageMutices[mutexIndex].unlock();
}
#endif