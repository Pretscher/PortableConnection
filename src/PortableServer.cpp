#include "PortableServer.hpp"
#include <thread>
using namespace std;

void PortableServer::waitForClient() {
    portableConnect();//listen for clients
}

void PortableServer::sendToClient(int clientIndex, string message) {
    if(wait[clientIndex] == false) {
        int iResult = portableSend(clientSockets[clientIndex], message.c_str());
        if(loggingEnabled == true) cout << "Sent message '" << message << "' to client " << clientIndex << "\n";
        wait[clientIndex] = true;
    }
}

void PortableServer::receiveMultithreaded(int clientIndex) {
    // Receive until the peer shuts down the connection
    while(true) {
        this_thread::sleep_for(chrono::milliseconds(1));
        char* recvBuffer = new char[recvbuflen];
        if(clientIndex == 1) {
            int a = 0;
        }
        int iResult = portableRecv(clientSockets[clientIndex], recvBuffer);
        //save message
        if(iResult > 0) {
            setGotNewMessage(clientIndex, true);
            //save message
            string newMsg;
            for(int j = 0; j < iResult; j++) {
                newMsg.push_back(recvBuffer[j]);
            }
            delete[] recvBuffer;
            setLastMessage(clientIndex, newMsg);
            wait[clientIndex] = false;
            //connection setup
            if(loggingEnabled == true) cout << "Received message '" << getLastMessageFromClient(clientIndex) << "' from client " << clientIndex << "\n";
            this->respondToCommands(clientIndex);
        }

        if(iResult < 0) {
            if(loggingEnabled == true) cout << "Lost connection to client.";
            portableShutdown(clientSockets[clientIndex]);
            return;
        }
    }
}

void PortableServer::respondToCommands(int clientIndex) {
    bool isCommand = false;//commands should not be used by handlers so we clear them at the end
    string lastMessage = getLastMessageFromClient(clientIndex);
    if(lastMessage.compare("12345") == 0) {
        sendToClient(clientIndex, "12345");//sets wait to false
        isCommand = true;
    }
    if(lastMessage.compare("getMyClientIndex") == 0) {
        sendToClient(clientIndex, to_string(this->clientSockets.size() - 1));//sets wait to false
        isCommand = true;
    }

    if(isCommand == true) {
        lastMessage.clear();
        setGotNewMessage(clientIndex, false);
        wait[clientIndex] = true;
    }
}

vector<string> PortableServer::getLastMessages() {
    vector<string> temp = lastMessages;//copy
    return temp;
}


//Portable functions used by the above code:------------------------------------------------------------------------------------------------

bool PortableServer::newMessage(int clientIndex) {
    bool temp = hasNewMessage(clientIndex);
    setGotNewMessage(clientIndex, false);
    return temp;
}

#ifdef __linux__ 
int PortableServer::portableSend(int socket, const char* message) const {
    int result = send(socket, message, strlen(message), 0);
    return result;
}

int PortableServer::portableRecv(int socket, char* recvBuffer) {
    return read(socket, recvBuffer, recvbuflen);
}

void PortableServer::portableShutdown(int socket) {
    shutdown(socket, SHUT_RDWR);
}

#elif _WIN64
int PortableServer::portableSend(SOCKET socket, const char* message) const {
    int result = send(socket, message,(int) strlen(message), 0);
    if(result == SOCKET_ERROR) {
        cout << "Server Message Sending Error: \n" << message;
    }
    return result;
}

void PortableServer::portableShutdown(SOCKET socket) {
    closesocket(socket);
}

int PortableServer::portableRecv(SOCKET socket, char* recvBuffer) {
    return recv(socket, recvBuffer, recvbuflen, 0);
}
#endif


//Server setup---------------------------------------------------------------------------------------------------------------------------


/**
 * @brief Sets up a connection to the first client that tries to connect to this server's adress.
 */
void listenForNextClient(PortableServer* server) {
    server->portableConnect();
}

vector<thread> connectThreads;
/**
 * @brief Sets up a connection to the first client that tries to connect to this server's adress. Multithreaded method.
 */
void PortableServer::portableConnect() {
    connectedMtx.lock();
    wait.push_back(false);
    lastMessageMutices.push_back(mutex());
    gotNewMessageMutices.push_back(mutex());

    lastMessageMutices[lastMessageMutices.size() - 1].lock();
    lastMessages.push_back(string());
    lastMessageMutices[lastMessageMutices.size() - 1].unlock();

    gotNewMessageMutices[gotNewMessageMutices.size() - 1].lock();
    gotNewMessage.push_back(false);
    gotNewMessageMutices[gotNewMessageMutices.size() - 1].unlock();

    connectedMtx.unlock();
#ifdef __linux__ 

    int listenSocket = 0;
    int opt = 1;
    addrlen = sizeof(address);

    // Creating socket file descriptor
    if((listenSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cout << "socket failed";
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if(setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
        &opt, sizeof(opt)))
    {
        cout << "setsockopt";
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if(bind(listenSocket,(struct sockaddr*) &address,
        sizeof(address)) < 0)
    {
        cout << "bind failed";
        exit(EXIT_FAILURE);
    }
    if(listen(listenSocket, 3) < 0)
    {
        cout << "listen";
        exit(EXIT_FAILURE);
    }

    cout << "Server successfully set up.\n";

    int tempClientSocket = accept(listenSocket,(struct sockaddr*) &address,(socklen_t*) &addrlen);
    if(tempClientSocket < 0) {
        cout << "Server accept failed";
    }
    shutdown(listenSocket, SHUT_RDWR);

    connectedMtx.lock();
    clientSockets.push_back(tempClientSocket);
    connectThreads.push_back(thread(&listenForNextClient, this));
    connectedMtx.unlock();

    this->receiveMultithreaded(connectThreads.size() - 1);

#elif _WIN64
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET;

    struct addrinfo* result = nullptr;
    struct addrinfo hints;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(iResult != 0) {
        cout << "Server WSAStartup failed with error: \n" << iResult;
        return;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, port.c_str(), &hints, &result);
    if(iResult != 0) {
        cout << "Server getaddrinfo failed with error: \n" << iResult;
        WSACleanup();
        exit(0);
        return;
    }

    // Create a SOCKET for connecting to server
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(listenSocket == INVALID_SOCKET) {
        cout << "Server socket failed with error: %ld\n" << WSAGetLastError();
        freeaddrinfo(result);
        WSACleanup();
        exit(0);
        return;
    }

    // Setup the TCP listening socket
    iResult = bind(listenSocket, result->ai_addr,(int) result->ai_addrlen);
    if(iResult == SOCKET_ERROR) {
        cout << "Server bind failed with error: \n" << WSAGetLastError();
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        exit(0);
        return;
    }

    freeaddrinfo(result);

    iResult = listen(listenSocket, SOMAXCONN);
    if(iResult == SOCKET_ERROR) {
        cout << "Server listen failed with error: \n" << WSAGetLastError();
        closesocket(listenSocket);
        WSACleanup();
        exit(0);
        return;
    }

    cout << "Server successfully set up.\n";

    SOCKET tempClientSocket = accept(listenSocket, nullptr, nullptr);
    if(tempClientSocket == INVALID_SOCKET) {
        cout << "Server accept failed with error: \n" << WSAGetLastError();
        closesocket(listenSocket);
        WSACleanup();
        exit(0);
        return;
    }
    closesocket(listenSocket);

    connectedMtx.lock();
    clientSockets.push_back(tempClientSocket);
    connectThreads.push_back(thread(&listenForNextClient, this));
    connectedMtx.unlock();

    this->receiveMultithreaded(connectThreads.size() - 1);
#endif
}


/**
 * @brief Gets the IP4-Adress of the machine this code is executed on
 *
 * @return string
 */
string PortableServer::getIP() const {
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
