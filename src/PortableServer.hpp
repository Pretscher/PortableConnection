#pragma once
#include "iostream"
#include "Socket.hpp"

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

class PortableServer : public Socket{
public:
    void waitForClient();
    /**
    * @brief Sets up a connection to the first client that tries to connect to this server's adress. Multithreaded method.
    */
   void portableConnect();
private:
    /**
     * @brief reads the last message from a client and, if it was a command defined in this function, responds accordingly
     *
     * @param clientIndex
     */
    void respondToCommands(int index);
};