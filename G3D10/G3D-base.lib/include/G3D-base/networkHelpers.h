/**
  \file G3D-base.lib/include/G3D-base/networkHelpers.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
// Included by NetworkDevice.cpp and TCPConduit.cpp to provide a platform-independent networking base
#pragma once
#include <cstring>
#include <stdlib.h>
#include <time.h>
#include "G3D-base/G3DString.h"

#if defined(G3D_LINUX) || defined(G3D_OSX) || defined(G3D_FREEBSD)
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <ifaddrs.h>
#   include <netinet/in.h>
#   include <net/if.h>
#   ifdef __linux__
#       include <sys/ioctl.h>
#       include <netinet/in.h>
#       include <unistd.h>
#       include <string.h>
//    Match Linux to FreeBSD
#       define AF_LINK AF_PACKET
#   else
#       include <net/if_dl.h>
#       include <sys/sockio.h>
#   endif

    #include <unistd.h>
    #include <errno.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <sys/ioctl.h>
    #include <netinet/if_ether.h>
    #include <net/ethernet.h>
    #include <net/if.h>

    #include <sys/types.h>

    #define _alloca alloca

    /** Define an error code for non-windows platforms. */
    int WSAGetLastError() {
        return -1;
    }

    #define SOCKET_ERROR -1

    static G3D::String socketErrorCode(int code) {
        return G3D::format("CODE %d: %s\n", code, strerror(code));
    }

    static G3D::String socketErrorCode() {
        return socketErrorCode(errno);
    }

    static const int WSAEWOULDBLOCK = -100;

    typedef int SOCKET;
    typedef struct sockaddr_in SOCKADDR_IN;

#else 

    // Windows
    static G3D::String socketErrorCode(int code) {
        LPTSTR formatMsg = nullptr;

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_IGNORE_INSERTS |
                      FORMAT_MESSAGE_FROM_SYSTEM,
                        nullptr,
                        code,
                        0,
                        (LPTSTR)&formatMsg,
                        0,
                        nullptr);

        return G3D::format("CODE %d: %s\n", code, formatMsg).c_str();
    }

    static G3D::String socketErrorCode() {
        return socketErrorCode(GetLastError());
    }

#endif


#ifndef _SOCKLEN_T
#   if defined(G3D_WINDOWS) || defined(G3D_OSX)
        typedef int socklen_t;
#   endif
#endif
