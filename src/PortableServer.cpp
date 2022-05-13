#include "PortableServer.hpp"

using namespace std;

void PortableServer::startEventloop(int clientIndex) {
    while(true) {
        this_thread::sleep_for(chrono::milliseconds(1));
        receiveNextMessage(clientIndex);
        respondToCommands(clientIndex);
    }
}

void PortableServer::waitForClient() {
    portableConnect();//listen for clients
}

void PortableServer::respondToCommands(int clientIndex) {
    bool isCommand = false;//commands should not be used by handlers so we clear them at the end
    string lastMessage = getLastMessage(clientIndex);
    if(lastMessage.compare("12345") == 0) {
        sendToSocket(clientIndex, "12345");//sets wait to false
        isCommand = true;
    }
    if(lastMessage.compare("getMyClientIndex") == 0) {
        socketMtx.lock();
        int newSocketIndex = this->sockets.size() - 1;
        socketMtx.unlock();

        sendToSocket(clientIndex, to_string(newSocketIndex));//sets wait to false
        isCommand = true;
    }

    if(isCommand == true) {
        lastMessage.clear();
        setGotNewMessage(clientIndex, false);
        setWait(clientIndex, true);
    }
}

//Server setup---------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Sets up a connection to the first client that tries to connect to this server's adress.
 */
void listenForNextClient(PortableServer* server) {
    server->portableConnect();
}

vector<thread> connectThreads;
void PortableServer::portableConnect() {
    socketMtx.lock();

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

    socketMtx.unlock();
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

    addSocket(tempClientSocket);
    connectThreads.push_back(thread(&listenForNextClient, this));

    startEventloop(connectThreads.size() - 1);
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

    addSocket(tempClientSocket);
    connectThreads.push_back(thread(&listenForNextClient, this));

    this->receiveMultithreaded(connectThreads.size() - 1);
#endif
}