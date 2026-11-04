#pragma once
#ifdef _WIN32
#ifndef NOMINMAX // MinGW 标准库可能已经设置此宏。
#define NOMINMAX // 防止系统宏覆盖标准库算法。
#endif           // 仅在需要时定义。
#include <winsock2.h>
#include <ws2tcpip.h>
using SockLength = int;

inline bool socketReady() {
    WSADATA d{};
    return WSAStartup(MAKEWORD(2, 2), &d) == 0;
}

inline void socketCleanup() {
    WSACleanup();
}

inline bool wouldBlock() {
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}

inline bool nonblocking(SOCKET s) {
    u_long n = 1;
    return ioctlsocket(s, FIONBIO, &n) == 0;
}
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
using SOCKET = int;
using SockLength = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;

inline int closesocket(SOCKET s) {
    return close(s);
}

inline bool socketReady() {
    return true;
}

inline void socketCleanup() {}

inline bool wouldBlock() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
}

inline bool nonblocking(SOCKET s) {
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == 0;
}
#endif
inline void closeSocket(SOCKET &s) {
    if (s != INVALID_SOCKET)
        closesocket(s);
    s = INVALID_SOCKET;
}
