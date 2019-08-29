#ifndef __INSPECT_OF_INFO_H__
#define __INSPECT_OF_INFO_H__

#include <string>
#include <windows.h>
#pragma warning(disable: 4996)
//error C4996: 'GetVersionExA': was declared deprecated
/*
  windows version  을 수집할때 GetVersionEx 를 사용.
  GetVersionEx 의 경우 윈도우 호환성 모드를 사용할 경우 호환 모드가 적용된 Windows 의
  version 을 가져오는 문제가 있다.
  윈도우에서 사용하지 않도록 넘어가는 중.
  또한 Windows 8.1 이상에서 제대로 동작되지 않아 Registry 를 한번더 검색해주어야 하는 이슈.
  https://msdn.microsoft.com/en-us/library/windows/desktop/dn424972(v=vs.85).aspx
  Version Helper functions 를 적용하도록 변경할 예정.
*/

namespace inspect {

/*
  @class  OSInfo
  @brief  OS 의 정보를 수집 및 저장하는 클래스
*/
class OSInfo {
public:
  OSInfo();
  ~OSInfo();
  // Windows API 를 이용하여 Windows OS 의 각정보를 멤버변수에 저장한다.
  // 성공시 true, 실패시 false
  bool GetInfo();
  // OS 정보수집이 성공일 경우. OS full string 을 가져온다.
  // ex) Microsoft Windows Server 2008 R2 Standard Edition Korean x64
  std::string GetFullName();

  std::string os_base_name() const { return os_base_name_; }
  std::string os_product_name() const { return os_product_name_; }
  std::string os_language_string() const { return os_language_string_; }
  std::string os_architecture() const { return os_architecture_; }

private:
  // major version, minor version, product type, suite mask 값을 이용하여
  // Windows OS 명을 확인하여 리턴.
  std::string GetName_(DWORD major_version, DWORD minor_version,
    BYTE product_type, WORD suite_mask);
  // 이전 API 버그로 windows 6.2 버전 이상의 경우 Registry 를 체크하도록 추가
  std::string GetNameException_(bool is_workstation = false);
  // Major version 5 의 Windows OS product 정보를 확인하여 리턴
  std::string GetProductNameVersion5_(DWORD minor_version, DWORD product_type,
                                      WORD architecture, WORD suite_mask);
  // Major version 6 의 Windows OS product 정보를 확인하여 리턴
  std::string GetProductNameVersion6_(DWORD product_type);
  // Windows OS 의 언어 정보를 확인하여 국가정보를 리턴
  std::string GetLanguageString_(DWORD language_id);
  // 서버의 architecture 정보를 리턴
  std::string GetArchitectureString_(WORD architecture);

private:
  std::string os_base_name_ = "";
  std::string os_product_name_ = "";
  std::string os_language_string_ = "";
  std::string os_architecture_ = "";
  std::string last_error_message_ = "";
};
} // namespace inspect

#endif