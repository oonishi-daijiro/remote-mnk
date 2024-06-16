#pragma once

#include <WinSock2.h>
#include <cstdlib>
#include <string_view>
#include <ws2bth.h>

#include <memory>
#include <string>
#include <vector>

struct bluetooth_socket {
  std::wstring device_name;
  std::unique_ptr<SOCKADDR_BTH> socket_address;
  bool operator==(std::wstring_view name) { return device_name == name; }
};

inline std::vector<bluetooth_socket> lookup_bluetooth_socket() {
  DWORD query_size = sizeof(WSAQUERYSETW);

  auto query =
      (WSAQUERYSETW *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, query_size);

  ULONG ulFlags = LUP_CONTAINERS | LUP_RETURN_NAME | LUP_RETURN_ADDR;
  HANDLE hLookup = 0;

  query->dwNameSpace = NS_BTH;
  query->dwSize = sizeof(WSAQUERYSETW);

  std::vector<bluetooth_socket> sockets = {};

  int iResult = WSALookupServiceBeginW(query, ulFlags, &hLookup);

  for (int i = 0; i < 100; i++) {
    if (i != 0) {
      ulFlags |= LUP_FLUSHCACHE;
    }

    auto lookup_err =
        WSALookupServiceNextW(hLookup, ulFlags, &query_size, query);

    auto device_name = query->lpszServiceInstanceName;

    if (device_name != NULL) {
      SOCKADDR_BTH *addr = new SOCKADDR_BTH{};

      ZeroMemory(addr, sizeof(*addr));
      memcpy(addr, (SOCKADDR_BTH *)query->lpcsaBuffer->RemoteAddr.lpSockaddr,
             sizeof(*addr));
      sockets.emplace_back(device_name, std::unique_ptr<SOCKADDR_BTH>(addr));
    }
    auto err = WSAGetLastError();

    if (err == WSA_E_NO_MORE) {
      break;
    } else if (err == WSAEFAULT) {
      HeapFree(GetProcessHeap(), 0, query);
      query = (WSAQUERYSETW *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                        query_size);
    }
  }

  WSALookupServiceEnd(hLookup);
  return sockets;
}
