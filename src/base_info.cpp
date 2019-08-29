#include "base_info.h"

// winsock api 경고무시
// 상위 컴파일러에서 inet_ntoa 를 사용하기 위해서 경고무시.
// inet_ntop 를 권장하나 sever2003, xp 배포를 위해 빌드시 inet_ntop 사용불가
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <tchar.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <Windows.h>

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3
#define INSPECT_IPV4_LOCALADDRESS "127.0.0.1"
#define INSPECT_IPV6_LOCALADDRESS "::1"
#define INSPECT_IPV6_MAXLEN       40

// 어댑터 정보 수집. ADAPTER_INFO
#pragma comment(lib, "IPHLPAPI.lib")
namespace inspect {

BaseInfo::BaseInfo()
{
}


BaseInfo::~BaseInfo()
{
}

// family 에 해당하는 IP version 의 IP list 를 결과로 리턴하는 함수.
// family(5) : IPv4 , family(6) : IPv6 이다.
// 
int32_t BaseInfo::GetDeviceIpList(int32_t family, std::list<std::string>& ip_list) 
{
  int32_t result = 0;
  char ipbuffer[INSPECT_IPV6_MAXLEN] = { 0, };
  DWORD out_buffer_length = WORKING_BUFFER_SIZE;
  DWORD api_result = NO_ERROR, retries = 0;
  PIP_ADAPTER_ADDRESSES adapter_address = nullptr, current_adapter_address = nullptr;
  PIP_ADAPTER_UNICAST_ADDRESS unicast_address = nullptr;
  ULONG get_family = AF_UNSPEC;
  if (family == 4)  get_family = AF_INET;
  else if (family == 6) get_family = AF_INET6;
  do {
    adapter_address = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(),
      0, out_buffer_length);
    if (adapter_address == nullptr) {
      last_error_message_ = "Buffer allocation failed";
      break;
    }
    // server 2003 Iphlpapi.dll
    // https://msdn.microsoft.com/en-us/library/aa365915(v=vs.85).aspx
    // Adapter 정보를 가져온다.
    api_result = GetAdaptersAddresses(get_family, GAA_FLAG_INCLUDE_PREFIX,
      nullptr, adapter_address, &out_buffer_length);

    if (api_result == ERROR_BUFFER_OVERFLOW) {
      // 버퍼사이즈가 부족한 경우 할당된 메모리를 반환 후 필요 사이즈로 재생성.
      HeapFree(GetProcessHeap(), 0, adapter_address);
      adapter_address = nullptr;
    } else {
      break;
    }
  } while ((api_result == ERROR_BUFFER_OVERFLOW) && (++retries < MAX_TRIES));

  // GetAdaptersAddresses 함수 호출결과가 정상일 경우 진행
  if (adapter_address && api_result == NO_ERROR) {
    ip_list.clear();
    current_adapter_address = adapter_address;
    while (current_adapter_address) {
      for (unicast_address = current_adapter_address->FirstUnicastAddress;
            nullptr != unicast_address; unicast_address = unicast_address->Next) {
        memset(ipbuffer, 0x00, sizeof(ipbuffer));
        if (family == 4) {  // AF_INET
          struct sockaddr_in* addr
            = (struct sockaddr_in*)unicast_address->Address.lpSockaddr;
#if defined(_MINIMUM_MAJOR_SIX)
          inet_ntop(AF_INET, &(addr->sin_addr), ipbuffer, sizeof(ipbuffer));
#else
          strncpy_s(ipbuffer, sizeof(ipbuffer), inet_ntoa(addr->sin_addr), sizeof(ipbuffer) - 1);
#endif
          if (strcmp(ipbuffer, INSPECT_IPV4_LOCALADDRESS)) {
            ++result;
            ip_list.push_back(ipbuffer);
          }
        } else if (family == 6) { // AF_INET6
          // 128bit 형식의 주소체계 ipv6 를 변환하기 위해서는 inet_ntop 를 사용해야 한다.
          // server 2003 or windows xp 를 지원하기 위해서는 다이렉트 사용불가
          // 현재 ipv6 사용하지 않으므로 추후 2003 미지원시 적용
#if defined(_MINIMUM_MAJOR_SIX)
          struct sockaddr_in6* addr
            = (sockaddr_in6*)unicast_address->Address.lpSockaddr;
          inet_ntop(AF_INET6, &(addr->sin6_addr), ipbuffer, sizeof(ipbuffer));
          if (strcmp(ipbuffer, INSPECT_IPV6_LOCALADDRESS)) {
            ++result;
            ip_list.push_back(ipbuffer);
          }    
#endif        
        }
      }
      current_adapter_address = current_adapter_address->Next;
    }
  } else {
    // api 결과에 대한 상세 정보를 받아 에러 결과에 저장한다.
    LPTSTR message_buffer = nullptr;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, api_result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR)& message_buffer, 0, NULL)) {
      last_error_message_.assign(message_buffer);
      LocalFree(message_buffer);
    }
  }
  if (adapter_address) {
    HeapFree(GetProcessHeap(), 0, adapter_address);
    adapter_address = nullptr;
  }
  return result;
}

// 현재 장비의 등록된 IP 리스트를 가져온다.
int32_t BaseInfo::GetDeviceIpList(int32_t family, std::string& ip_list)
{
  std::list<std::string> get_ip_list;
  int32_t result = GetDeviceIpList(family, get_ip_list);
  if (result > 0) {
    ip_list.clear();
    uint32_t count = 0;
    while (!get_ip_list.empty()) {
      if (count++) {
        ip_list.append(",");
      }
      ip_list.append(get_ip_list.front());
      get_ip_list.pop_front();
    }
  }
  return result;
}

// 현재 서버의 호스트명을 가져온다.
std::string BaseInfo::GetServerName()
{
  LPTSTR buffer = new TCHAR[MAX_COMPUTERNAME_LENGTH];
  DWORD buffer_length = 0;
  std::string result = "";
  if (buffer) {
    // minimum supported windows 2000 server
    if (GetComputerNameEx(ComputerNameDnsHostname, buffer, &buffer_length)) {
      result.assign(buffer);
    } else {
      if (ERROR_MORE_DATA == GetLastError()) {
        // 결과 버퍼 사이즈가 작은 경우 버퍼 재생성 후 결과를 받아온다.
        delete[] buffer;
        buffer = nullptr;
        buffer = new TCHAR[buffer_length + 1];
        if (GetComputerNameEx(ComputerNameDnsHostname, buffer, &buffer_length)) {
          result.assign(buffer);
        } else {
          last_error_message_ = "GetComputerNameEx failed [ "
            + std::to_string(GetLastError()) + "]";
        }
      } else {
        last_error_message_ = "GetComputerNameEx failed [ "
          + std::to_string(GetLastError()) +"]";
      }
    }
  } else {
    last_error_message_.assign("Buffer allocation failed");
  }
  if (buffer) {
    delete[] buffer;
    buffer = nullptr;
  }
  return result;
}

} // namespace inspect