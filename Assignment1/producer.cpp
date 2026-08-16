#include <iostream>
#include <cstdlib>
#include "api.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <value>\n";
        return 1;
    }
    int value = std::atoi(argv[1]);

    int fd = api_connect();
    if (fd < 0) {
        std::cerr << "Could not connect. Is queue_server running?\n";
        return 1;
    }

    std::cout << "Sending " << value << "... (will wait here if queue is full)\n";
    if (api_produce(fd, value)) {
        std::cout << "Produced " << value << " successfully.\n";
    } else {
        std::cout << "Failed to produce (server gone?).\n";
    }

    api_disconnect(fd);
    return 0;
}