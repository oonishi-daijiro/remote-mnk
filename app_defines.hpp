#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <initguid.h>
#include <ws2bth.h>

namespace bth_service {
// {AEC8D2BD-C873-4295-9638-D457BE3E6EE4}
DEFINE_GUID(inline GUID_service_class, 0xaec8d2bd, 0xc873, 0x4295, 0x96, 0x38,
            0xd4, 0x57, 0xbe, 0x3e, 0x6e, 0xe4);

} // namespace bth_service
