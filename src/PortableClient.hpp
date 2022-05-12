#pragma once
// Client side C/C++ program to demonstrate Socket programming
#include "iostream" 
using namespace std;
#include <vector>
#include <string>
#include <mutex>
#include <memory>

#ifdef __linux__ 

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <mutex>
#include <thread>
#include <vector>

#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#define PORT 8080


#elif _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex>
#include <vector>
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#endif

class PortableClient {
public:
    int myPlayerIndex;

    PortableClient();
    void receiveMultithreaded();
    void sendToServer(string message);

    string getLastMessage() const;
    bool isConnected() const;
    shared_ptr<mutex> getMutex() const;
    bool newMessage();

    string getIP() const;

    void connectToServer(string ip);
    void searchHosts(int waitTime);

    bool isConnected() {
        connectedMtx.lock();
        bool temp = connected;
        connectedMtx.unlock();
        return temp;
    }

    void pushToAvailableHosts(string s) {
        avHostsMtx.lock();
        avHosts.push_back(s);
        avHostsMtx.unlock();
    }

    vector<string> getAvailableHosts() {
        avHostsMtx.lock();
        vector<string> copy = avHosts;
        avHostsMtx.unlock();
        return copy;
    }

#ifdef  __linux__ 
    int addrlen;
    int serverSocket;
    struct sockaddr_in address;

    int portableConnect(string connectIP);
    int portableRecv(int socket, char* recvBuf);
    int portableSend(int socket, string message) const;
    void portableShutdown(int socket);
private:
    //No getter exists, because thread safety could not be guaranteed. Please outsource every action with connect sockets to the class.
    vector<int> connectSockets;
    mutex connectSocketsMtx;
public:
/**
 * @brief Threadsafe way to add a connect socket to the client. No getter exists, because thread safety could not be guaranteed.
 * 
 * @param i_connectSocketIndex 
 */
    void addConnectSocket(int i_connectSocketIndex) {
        connectSocketsMtx.lock();
        connectSockets.push_back(i_connectSocketIndex);
        connectSocketsMtx.unlock();
    }
#elif _WIN32
    SOCKET serverSocket;
    
    SOCKET portableConnect(const char* connectIP);
    int portableRecv(SOCKET& socket, char* recvBuf);
    int portableSend(SOCKET& socket, const char* message) const;
    void portableShutdown(SOCKET& socket);
private:
    //No getter exists, because thread safety could not be guaranteed. Please outsource every action with connect sockets to the class.
    vector<SOCKET> connectSockets;
    mutex connectSocketsMtx;
public:
/**
 * @brief Threadsafe way to add a connect socket to the client. No getter exists, because thread safety could not be guaranteed.
 * 
 * @param i_connectSocketIndex 
 */
    void addConnectSocket(SOCKET i_connectSocketIndex) {
        connectSocketsMtx.lock();
        connectSockets.push_back(i_connectSocketIndex);
        connectSocketsMtx.unlock();
    }
#endif
    /**
     * @brief Threadsafe way to set wait bool.
     *
     * @param newWaitStatus
     */
    inline void setWait(bool newWaitStatus) {
        waitMutex.lock();
        wait = newWaitStatus;
        waitMutex.unlock();
    }
    /**
    * @brief Threadsafe way to get wait bool.
    *
    * @param newWaitStatus
    */
    inline bool getWait() {
        waitMutex.lock();
        return wait;
        waitMutex.unlock();
    }
private:
    PortableClient(PortableClient& copy) {

    }
    void getMyIndexFromServer();
    string readMsgBuffer(int msgLenght, char recvbuf[]);
    string receiveMessage();

    string port = "8080";
    string lastMessage;

    bool connected = false;

    mutex connectedMtx;
    mutex waitMutex;
    bool wait = false;//initially set to false because the first message is always from the client



    bool gotNewMessage = false;


    void setConnected(bool c) {
        connectedMtx.lock();
        connected = c;
        connectedMtx.unlock();
    }

    
    thread searchingHosts;

    mutex avHostsMtx;
    vector<string> avHosts;
};

