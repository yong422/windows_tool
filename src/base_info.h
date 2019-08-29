#ifndef __INSPECT_BASE_INFO_H__
#define __INSPECT_BASE_INFO_H__
#define _WINSOCKAPI_
#include <string>
#include <list>
namespace inspect {
/*
  @class  inspect::BaseInfo
  @brief  
    기본 메타정보에 대해서 수집하여 리턴하는 데이터 수집 클래스
    서버의 Ip list, 호스트명 을 수집하며 추후 필요 메타데이터는 추가예정.
*/
class BaseInfo
{
public:
  BaseInfo();
  ~BaseInfo();

  std::string last_error_message() const { return last_error_message_; }

  // Address famil 에 해당하는 장비의 IP 리스트를 가져온다.
  // family = 4 AF_INET , family = 6 AF_INET6
  int32_t GetDeviceIpList(int32_t family, std::string& ip_list);
  int32_t GetDeviceIpList(int32_t family, std::list<std::string>& ip_list);
  // 호스트네임을 가져온다.
  std::string GetServerName();

private:
  std::string last_error_message_ = "";
};
} // namespace inspect
#endif // __INSPECT_BASE_INFO_H__