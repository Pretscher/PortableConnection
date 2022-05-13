#include <PortableClient.hpp>
#include <PortableServer.hpp>
#include <iostream>
#include <thread>
using namespace std;

//Those two are outside of func because if those classes are destroyed, 
//the threads are destroyed, which will crash the program. 
//Could edit destructor but i can't be bothered.
PortableServer* server;
PortableClient* client;
void initConnection(bool isServer) {
    if(isServer == true) {
        server = new PortableServer();
        server->waitForClient();
    } else {
        client = new PortableClient();
        client->searchHosts(1000);
        vector<string> avHosts = client->getAvailableHosts();
        cout << "Please choose your host by index from the following list:\n";
        for (unsigned int i = 0; i < avHosts.size(); i++) {
            cout << "\nHost at index [" << i << "]: " << avHosts[i] << "\n";
        }
        //let user type the index of the server in the terminal
        string chosenIndex;
        cin >> chosenIndex;
        int chosenIndexInt = stoi(chosenIndex);
        if(chosenIndexInt >= avHosts.size()) {
            cout << "Index invalid.\n";
            exit(0);
        }
        client->connectToServer(avHosts[chosenIndexInt]);
    }
}

int main() {
    bool isServer = false;
    initConnection(isServer);
    while(true){

    }
    return 0;
}
