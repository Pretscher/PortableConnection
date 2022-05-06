#include <PortableClient.hpp>
#include <PortableServer.hpp>
#include <iostream>
#include <thread>
using namespace std;


void initConnection (bool isServer);
int main ( ) {
    bool isServer = true;
    thread networking (&initConnection, isServer);

    return 0;
}

void initConnection (bool isServer) {
    if (isServer) {
        PortableServer server = PortableServer ( );
        server.waitForClient ( );
    }
    else {
        PortableClient client = PortableClient ( );
        client.searchHosts ( );
        vector<string> avHosts = client.getAvailableHosts ( );
        cout << "Please choose your host by index from the following list:";
        for (unsigned int i = 0; i < avHosts.size ( ); i++) {
            cout << "\nHost at index [" << i << "]: " << avHosts[i] << "\n";
        }
        int chosenIndex;
        cin >> chosenIndex;
        client.connectToServer (avHosts[chosenIndex]);
    }
}