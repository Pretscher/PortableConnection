#pragma once
// Client side C/C++ program to demonstrate Socket programming
#include "Socket.hpp"
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

class PortableClient : public Socket {
public:
    PortableClient();
    vector<int> myClientIndex;
    void connectToServer(string ip);
    /**
    * @brief Searches for hosts in local network that use sockets on this port.
    * @param waitTime Time the algorithm for searching server sockets gets, until it has to return a list of available sockets
    */
    void searchHosts(int waitTime);

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
    struct sockaddr_in address;

    int portableConnect(string connectIP);
    //No getter exists, because thread safety could not be guaranteed. Please outsource every action with connect sockets to the class.
    vector<int> connectSockets;
    mutex connectSocketsMtx;
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

private:
    /**
     * @brief Get the index that this client has in the server's client array
     */
    void getMyIndexFromServer(int serverIndex);
    thread searchingHosts;
    mutex avHostsMtx;
    vector<string> avHosts;
};

