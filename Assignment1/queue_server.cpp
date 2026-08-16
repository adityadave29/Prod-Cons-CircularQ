#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include "api.h"

// ============================================================================
// The global circular queue, shared by every connected producer/consumer.
// One mutex protects it; two condition variables let blocked threads sleep
// instead of busy-waiting.
// ============================================================================
class CircularQueue {
public:
    explicit CircularQueue(size_t capacity) : buf_(capacity), cap_(capacity) {}

    // Blocks here (releasing the lock while waiting) until there's a free
    // slot, then inserts value.
    void produce(int value) {
        std::unique_lock<std::mutex> lock(mtx_);
        not_full_.wait(lock, [&] { return count_ < cap_; });   // wait while full

        buf_[tail_] = value;
        tail_ = (tail_ + 1) % cap_;
        count_++;

        std::cout << "[queue] produced " << value
                  << " (size now " << count_ << "/" << cap_ << ")\n";

        not_empty_.notify_one(); // wake one waiting consumer, if any
    }

    // Blocks here until there's an item, then removes and returns it.
    int consume() {
        std::unique_lock<std::mutex> lock(mtx_);
        not_empty_.wait(lock, [&] { return count_ > 0; });      // wait while empty

        int value = buf_[head_];
        head_ = (head_ + 1) % cap_;
        count_--;

        std::cout << "[queue] consumed " << value
                  << " (size now " << count_ << "/" << cap_ << ")\n";

        not_full_.notify_one(); // wake one waiting producer, if any
        return value;
    }

private:
    std::mutex mtx_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;

    std::vector<int> buf_;
    size_t cap_;
    size_t head_ = 0, tail_ = 0, count_ = 0;
};

static CircularQueue g_queue(5); // <-- fixed capacity of the global queue

// Handles one client connection: reads requests, calls into the queue
// (which may block), sends back exactly one response per request.
void handleClient(int client_fd) {
    try {
        Message req;
        while (readAll(client_fd, &req, sizeof(req))) {
            Message resp{};

            if (req.type == REQ_PRODUCE) {
                g_queue.produce(req.value);
                resp.type = RESP_OK;
            } else if (req.type == REQ_CONSUME) {
                resp.type = RESP_VALUE;
                resp.value = g_queue.consume();
            }

            if (!writeAll(client_fd, &resp, sizeof(resp))) break;
        }
    } catch (const std::exception& e) {
        std::cerr << "[queue] client handler error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[queue] client handler unknown error\n";
    }
    close(client_fd);
}

int main() {
    // Without this, writing to a socket whose peer already closed the
    // connection sends SIGPIPE, whose DEFAULT action is to kill the whole
    // process (not just that thread/connection). Ignoring it makes write()
    // simply return -1 instead, which we already handle.
    signal(SIGPIPE, SIG_IGN);

    unlink(SOCKET_PATH);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 64);

    std::cout << "Global queue server running on " << SOCKET_PATH << "\n";

    while (true) {
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        std::thread(handleClient, client_fd).detach(); // one thread per client
    }
}