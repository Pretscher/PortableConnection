#include <PortableClient.hpp>
#include <PortableServer.hpp>
#include <iostream>
#include <thread>
using namespace std;

void initConnection(bool isServer) {
    if(isServer == true) {
        PortableServer server;
        server.waitForClient();
    } else {
        PortableClient client;
        client.searchHosts(1000);
        vector<string> avHosts = client.getAvailableHosts();
        cout << "Please choose your host by index from the following list:\n";
        for (unsigned int i = 0; i < avHosts.size(); i++) {
            cout << "\nHost at index [" << i << "]: " << avHosts[i] << "\n";
        }
        string chosenIndex;
        cin >> chosenIndex;
        int chosenIndexInt = stoi(chosenIndex);
        if(chosenIndexInt >= avHosts.size()) {
            cout << "Index invalid.\n";
            exit(0);
        }
        client.connectToServer(avHosts[chosenIndexInt]);
    }
}

int main() {
    bool isServer = false;
    initConnection(isServer);
    //thread networking(&initConnection, isServer);
    while(true){

    }
    return 0;
}
