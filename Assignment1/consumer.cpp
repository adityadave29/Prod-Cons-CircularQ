#include <iostream>
using namespace std;
#include "api.h"

int main() {
    int fd = api_connect();
    if (fd < 0) {
        cerr << "Could not connect. Is queue_server running?\n";
        return 1;
    }

    cout << "Requesting an item... (will wait here if queue is empty)\n";
    int value;
    if (api_consume(fd, value)) {
        cout << "Consumed " << value << " successfully.\n";
    } else {
        cout << "Failed to consume (server gone?).\n";
    }

    api_disconnect(fd);
    return 0;
}