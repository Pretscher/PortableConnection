#pragma once
#include "iostream" 
using namespace std;
#include <vector>
#include <thread>
#include <string>
#include <mutex>
#include <memory>

#ifdef __linux__ 
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <ifaddrs.h>
#include <netdb.h>
#define PORT 8080
#elif _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#endif

class Socket {
public:
    string port = "8080";
    int recvbuflen = 512;
    bool connected = false;
    mutex socketMtx;
    bool loggingEnabled = true;
    Socket(){}
    
    inline int getConnectionCount() {
        socketMtx.lock();
        int temp = sockets.size();
        socketMtx.unlock();
        return 0;
    }

    bool newMessage(int index);
    string getIP() const;

    vector<string> getLastMessages();
    /**
    * @brief If the socket was added to the socket array, this higher-level function can be used
    * to send a message to a socket.
    * If logging is enabled, it prints the message. 
    * ONLY SENDS IF WAIT STATUS IS FALSE (a message was received through 
    * "string receiveNextMessage(int socketIndex)" after the last call of this message) 
    * Changes the wait status to true (After sending a message, you should receive a message first).
    *
    * @param socketIndex
    * @param message
    */
    void sendToSocket(int socketIndex, string message);

    /**
    * @brief If the socket was added to the socket array, this higher-level function can be used
    * to receive the next message from a socket. 
    * If logging is enabled, it prints the message. 
    * Changes the wait status to false (After receiving a message, you should be able to send).
    *
    * @param socketIndex Index in the Socket array (NOT ID)
    */
    string receiveNextMessage(int socketIndex);

    /**
     * @brief Threadsafely Get the Last Message
     * 
     * @param socketIndex 
     * @return string 
     */
    inline string getLastMessage(int socketIndex) {
        lastMessageMutices[socketIndex].lock();
        string temp = lastMessages[socketIndex];
        lastMessageMutices[socketIndex].unlock();
        return temp;
    }

    /**
     * @brief Threadsafely Get if the message is new
     * 
     * @param socketIndex 
     * @return bool 
     */
    inline bool hasNewMessage(int socketIndex) {
        gotNewMessageMutices[socketIndex].lock();
        bool temp = gotNewMessage[socketIndex];
        gotNewMessageMutices[socketIndex].unlock();
        return temp;
    }

    inline bool isWaiting(int socketIndex){
        waitMutices[socketIndex].lock();
        bool temp = wait[socketIndex];
        waitMutices[socketIndex].unlock();
        return temp;
    }

protected:

    Socket(Socket& copy) {}//not copyable
    Socket(Socket&& moveable) {}//not moveable

    #define maxConnections 12
    //cant resize a vector of mutices or add elements, so we need a fixed amount
    mutex lastMessageMutices[maxConnections];
    mutex gotNewMessageMutices[maxConnections];
    mutex waitMutices[maxConnections];
    vector<bool> wait;
    vector<string> lastMessages;
    vector<bool> gotNewMessage;
    /**
     * @brief Threadsafely set the last message received from a client
     * 
     * @param socketIndex 
     * @param newMessage 
     */
    inline void setLastMessage(int socketIndex, string newMessage) {
        lastMessageMutices[socketIndex].lock();
        lastMessages[socketIndex] = newMessage;
        lastMessageMutices[socketIndex].unlock();
    }
    /**
     * @brief Threadsafely set if there is a new message
     * 
     * @param socketIndex 
     * @param isNew 
     */
    inline void setGotNewMessage(int socketIndex, bool isNew) {
        gotNewMessageMutices[socketIndex].lock();
        gotNewMessage[socketIndex] = isNew;
        gotNewMessageMutices[socketIndex].unlock();
    }
    /**
    * @brief Threadsafe way to set the wait bool for a client
    *
    * @param socketIndex
    * @param newWaitStatus
    */
    inline void setWait(int socketIndex, bool newWaitStatus) {
        waitMutices[socketIndex].lock();
        wait[socketIndex] = newWaitStatus;
        waitMutices[socketIndex].unlock();
    }

    string readMsgBuffer(int msgLenght, char* recvbuf);
#ifdef  __linux__
    int addrlen;
    vector<int> sockets;
    struct sockaddr_in address;
public:
    /**
     * @brief Very raw way to send a message to a socket. Only works with the socket's ID, not it's Index. 
     * 
     * @param socket Internal Socket ID
     * @param message 
     * @return int: error code
     */
    int portableSend(int socket, const char* message) const;
    /**
     * @brief Very raw way to receive a message from a socket. Only works with the socket's ID, not it's Index. 
     * 
     * @param socket Internal Socket ID
     * @param recvBuffer Message will be written into this buffer
     * @return int: message lenght
     */
    int portableRecv(int socket, char* recvBuffer);
    void portableShutdown(int socket);
protected:
    /**
     * @brief Threadsafe Linux version of adding a socket to the socket array. Adds relevant information
     * to vectors "wait", "lastMessages" and "gotNewMessage"
     * 
     * @param socket 
     */
    void addSocket(int internalSocketID);
    inline int getSocket(int socketIndex) {
        socketMtx.lock();
        int temp = sockets[socketIndex];
        socketMtx.unlock();
        return temp;
    }
private:
#elif _WIN32
    vector<SOCKET> sockets;
public:
    int portableSend(SOCKET socket, const char* message) const;
    int portableRecv(SOCKET socket, char* recvBuffer);
    void portableShutdown(SOCKET socket);
protected:
    /**
     * @brief Threadsafe Windows version of adding a socket to the socket array. Adds relevant information
     * to vectors "wait", "lastMessages" and "gotNewMessage"
     * 
     * @param socket 
     */
    void addSocket(SOCKET socket);
    inline SOCKET getSocket(int socketIndex) {
        socketMtx.lock();
        SOCKET temp = sockets[socketIndex];
        socketMtx.unlock();
        return temp;
    }
private:
#endif
};