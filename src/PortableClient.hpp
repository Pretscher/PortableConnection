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
    void sendToServer(string message);
    shared_ptr<mutex> getMutex() const;
    bool newMessage();

    /**
     * @brief Gets the IP of the machine this program is running on
     * 
     * @return string 
     */
    string getMyIP() const;

    void connectToServer(string ip);
    /**
    * @brief Searches for hosts in local network that use sockets on this port.
    * @param waitTime Time the algorithm for searching server sockets gets, until it has to return a list of available sockets
    */
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
    /**
    * @brief tries to receive a message and write it into the recvBuffer.
    * Will block all action in this thread until message was received or time out
    * @param socket usually the server socket for a client
    * @param recvBuffer
    * @return int <- lenght of the message.
    */
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
        bool temp = wait;
        waitMutex.unlock();
        return wait;
    }
    bool loggingEnabled = true;
private:
    PortableClient(PortableClient& copy) {
    }
    /**
     * @brief Internally used method to receive from the server socket in a seperate thread.
     * 
     */
    void receiveMultithreaded();
    /**
     * @brief Get the index that this client has in the server's client array
     */
    void getMyIndexFromServer();
    /**
     * @brief Helper method to read from a buffer
     * 
     * @param msgLenght 
     * @param recvbuf 
     * @return string 
     */
    string readMsgBuffer(int msgLenght, char recvbuf[]);
    string receiveMessage();

    string port = "8080";
    string lastMessage;
    mutex lastMsgMtx;
    /**
     * @brief Threadsafe way to get lastMessage
     * 
     * @return string lastMessage
     */
    inline string getLastMessage() {
        lastMsgMtx.lock();
        string temp = lastMessage;
        lastMsgMtx.unlock();
        return temp;
    }
    /**
     * @brief Threadsafe way to set LastMessage
     * 
     * @param message 
     */
    inline void setLastMessage(string message) {
        lastMsgMtx.lock();
        lastMessage = message;
        lastMsgMtx.unlock();
    }

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
    /**
     * @brief Threadsafe way to check if the client is connected to a server
     * 
     * @return true 
     * @return false 
     */
    bool isConnected() {
        connectedMtx.lock();
        bool temp = connected;
        connectedMtx.unlock();
        return temp;
    }
    
    thread searchingHosts;

    mutex avHostsMtx;
    vector<string> avHosts;
};

