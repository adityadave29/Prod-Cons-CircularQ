#pragma once
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

constexpr const char* SOCKET_PATH = "/tmp/global_queue.sock";

enum MsgType : uint8_t {
    REQ_PRODUCE = 1,   
    REQ_CONSUME = 2,   
    RESP_OK     = 3,   
    RESP_VALUE  = 4,   
};

struct Message {
    uint8_t type;
    int32_t value;
};

inline bool writeAll(int fd, const void* buf, size_t len) {
    const char* p = (const char*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, p + sent, len - sent);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

inline bool readAll(int fd, void* buf, size_t len) {
    char* p = (char*)buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, p + got, len - got);
        if (n <= 0) return false;
        got += n;
    }
    return true;
}

inline int api_connect() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

inline bool api_produce(int fd, int value) {
    Message req{REQ_PRODUCE, value};
    if (!writeAll(fd, &req, sizeof(req))) return false;

    Message resp;
    if (!readAll(fd, &resp, sizeof(resp))) return false;
    return resp.type == RESP_OK;
}

inline bool api_consume(int fd, int& out_value) {
    Message req{REQ_CONSUME, 0};
    if (!writeAll(fd, &req, sizeof(req))) return false;

    Message resp;
    if (!readAll(fd, &resp, sizeof(resp))) return false;
    if (resp.type != RESP_VALUE) return false;

    out_value = resp.value;
    return true;
}

inline void api_disconnect(int fd) {
    if (fd >= 0) close(fd);
}