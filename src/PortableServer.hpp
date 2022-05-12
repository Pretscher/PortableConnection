#pragma once
#include "iostream" 
using namespace std;
#include <vector>
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

class PortableServer {
public:
    PortableServer() {

    }
    void waitForClient();
    /**
    * @brief Waits for messages from a client. Has to be done in a seperate thread.
    *
    * @param clientIndex
    */
    void receiveMultithreaded(int i);
    /**
     * @brief Sends a message to a client. Cannot be done twice in a row without the client responding in between
    *
    * @param clientIndex
    * @param message
    */
    void sendToClient(int index, string message);

    vector<string> getLastMessages();
    /**
    * @brief Checks if there is a new message from this client, since this method was last called
    *
    * @param clientIndex
    * @return true There is a new message since this method was last called
    * @return false There is no new message since this method was last called
    */
    bool newMessage(int index);

    string getIP() const;

    inline int getClientCount() {
        connectedMtx.lock();
        int temp = clientSockets.size();
        connectedMtx.unlock();
        return 0;
    }
    void portableConnect();

    /**
     * @brief Threadsafe way to set the wait bool for a client
     *
     * @param clientIndex
     * @param newWaitStatus
     */
    inline void setWait(int clientIndex, bool newWaitStatus) {
        waitMutices[clientIndex].lock();
        wait[clientIndex] = newWaitStatus;
        waitMutices[clientIndex].unlock();
    }
    /**
    * @brief Threadsafe way to get wait bool.
    *
    * @param newWaitStatus
    */
    inline bool getWait(int clientIndex) {
        waitMutices[clientIndex].lock();
        return wait[clientIndex];
        waitMutices[clientIndex].unlock();
    }

    bool loggingEnabled = true;
private:
    PortableServer(PortableServer& copy) {

    }
#ifdef  __linux__
    int addrlen;
    vector<int> clientSockets;
    struct sockaddr_in address;
    int portableSend(int socket, const char* message) const;
    int portableRecv(int socket, char* recvBuffer);
    void portableShutdown(int socket);
#elif _WIN32
    vector<SOCKET> clientSockets;

    int portableSend(SOCKET socket, const char* message) const;
    int portableRecv(SOCKET socket, char* recvBuffer);
    void portableShutdown(SOCKET socket);
#endif


//There should be a public way to get the last message from a client, but only the server should be able to set the last message
public:
    /**
     * @brief Threadsafely Get the Last Message
     * 
     * @param clientIndex 
     * @return string 
     */
    inline string getLastMessageFromClient(int clientIndex) {
        lastMessageMutices[clientIndex].lock();
        string temp = lastMessages[clientIndex];
        lastMessageMutices[clientIndex].unlock();
        return temp;
    }
    /**
     * @brief Threadsafely Get if the message is new
     * 
     * @param clientIndex 
     * @return bool 
     */
    inline bool hasNewMessage(int clientIndex) {
        gotNewMessageMutices[clientIndex].lock();
        bool temp = gotNewMessage[clientIndex];
        gotNewMessageMutices[clientIndex].unlock();
        return temp;
    }
private:
    vector<mutex> lastMessageMutices;
    vector<mutex> gotNewMessageMutices;
    vector<string> lastMessages;
    vector<bool> gotNewMessage;
    /**
     * @brief Threadsafely set the last message received from a client
     * 
     * @param clientIndex 
     * @param newMessage 
     */
    inline void setLastMessage(int clientIndex, string newMessage) {
        lastMessageMutices[clientIndex].lock();
        lastMessages[clientIndex] = newMessage;
        lastMessageMutices[clientIndex].unlock();
    }

    inline void setGotNewMessage(int clientIndex, bool isNew) {
        gotNewMessageMutices[clientIndex].lock();
        gotNewMessage[clientIndex] = isNew;
        gotNewMessageMutices[clientIndex].unlock();
    }


    /**
     * @brief reads the last message from a client and, if it was a command defined in this function, responds accordingly
     *
     * @param clientIndex
     */
    void respondToCommands(int index);
    string port = "8080";
    int recvbuflen = 512;

    bool connected = false;
    mutex connectedMtx;
    vector<mutex> waitMutices;
    vector<bool> wait;


};