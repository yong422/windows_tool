#include "os_info.h"

#include <winnt.h>

/*
  @warning  winnt.h 참조시 windows.h 를 먼저참조해야한다.
            fatal error C1189 "No Target Architecture" 발생.
*/
const std::string g_osversion_registry_path = R"reg(SOFTWARE\Microsoft\Windows NT\CurrentVersion)reg";

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL(WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

typedef ULONGLONG(WINAPI *fnGetTickCount64)(void);
// GetSystemMetrics() in User32.lib (win2000 이상가능)
// OSVERSIONINFOEX struct in Windows.h (win2000 이상가능)

namespace inspect {
OSInfo::OSInfo()
{

}

OSInfo::~OSInfo()
{

}

std::string OSInfo::GetFullName()
{
  std::string result = os_base_name_ + " " + os_product_name_ + " "
    + os_language_string_ + " " + os_architecture_;
  return result;
}

bool OSInfo::GetInfo()
{
  OSVERSIONINFOEX info; // server 2000 < : OS 버전정보 구조체
  SYSTEM_INFO sysInfo;  // server 2000 < : 시스템정보 구조체
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx((OSVERSIONINFO*)&info)) {  // OS version 정보를 가져온다. server 2000 <
    PGNSI pGNSI = NULL;
    // 서버의 아키텍쳐 정보
    pGNSI = (PGNSI)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo");
    // GetSystemInfo 와 GetNativeSystemInfo 함수는 리턴값이 없음.
    if (pGNSI)	pGNSI(&sysInfo);  // server 2003 <
    else GetSystemInfo(&sysInfo); // server 2000 <

    os_base_name_ = GetName_(info.dwMajorVersion, info.dwMinorVersion, info.wProductType,
                            info.wSuiteMask);
    
    if (info.dwMajorVersion >= 6) {
      // GetProductInfo 함수는 server 2008 이상 지원이므로 함수포인터 사용 한다.
      PGPI pGPI;
      DWORD product_type = 0;
      pGPI = (PGPI)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetProductInfo");

      pGPI(info.dwMajorVersion, info.dwMinorVersion,
        info.wServicePackMajor, info.wServicePackMinor, &product_type);
      os_product_name_ = GetProductNameVersion6_(product_type);
    } else {
      os_product_name_ = GetProductNameVersion5_(info.dwMinorVersion, info.wProductType,
                                                  sysInfo.wProcessorArchitecture, info.wSuiteMask);
    }
    // OS의 언어 정보
    os_language_string_ = GetLanguageString_(static_cast<DWORD>(GetSystemDefaultUILanguage()));
    // OS architecture
    os_architecture_ = GetArchitectureString_(sysInfo.wProcessorArchitecture);

  }
  return true;
}

// 변수로받은 버전에 해당하는 Windows OS 명을 확인하여 리턴한다.
std::string OSInfo::GetName_(DWORD major_version, DWORD minor_version, 
                              BYTE product_type, WORD suite_mask)
{
  std::string os_name("Microsoft ");

  if (major_version == 10) {
    //! Windows Major version 10 (workstation : windows 10, server : 2016)
    //! 현재 api 문제상 Major version 10 은 넘어오지않음. 별도처리.
    if (minor_version == 0) {
      if (product_type == VER_NT_WORKSTATION)
        os_name.append("Windows 10");
      else
        os_name.append("Windows Server 2016");
    } else {
      os_name.clear();
      os_name.append("");
    }
  } else if (major_version == 6) {
    // Windows Major version 6 (workstation : vista 이상, server : 2008이상)
    if (minor_version == 0) {
      if (product_type == VER_NT_WORKSTATION)
        os_name.append("Windows Vista");
      else
        os_name.append("Windows Server 2008");
    } else if (minor_version == 1) {
      if (product_type == VER_NT_WORKSTATION) {
        os_name.append("Windows 7");
      } else {
        os_name.append("Windows Server 2008 R2");
      }
    } else if (minor_version == 2) {
      //! 현재 윈도우 버젼가져오기 버그.
      //! 6.2 (워크스테이션 : Windows 8 , 서버 : Windows Server 2012) 이상은 전부 6.2를 리턴.
      os_name.append(GetNameException_(product_type == VER_NT_WORKSTATION ? true : false));
    } else if (minor_version == 3) {
      if (product_type == VER_NT_WORKSTATION) {
        os_name.append("Windows 8.1");
      } else {
        os_name.append("Windows Server 2012 R2");
      }
    } else {
      os_name.clear();
      os_name.append("");
    }
  } else if (major_version == 5) {
    if (minor_version == 0) {
      os_name.append("Windows 2000 ");
      if (product_type == VER_NT_WORKSTATION) {
        os_name.append("Professional");
      } else {
        if (suite_mask & VER_SUITE_DATACENTER)
          os_name.append("Datacenter Server");
        else if (suite_mask & VER_SUITE_ENTERPRISE)
          os_name.append("Advanced Server");
        else
          os_name.append("Server");
      }
    } else if (minor_version == 1) {
      os_name.append("Windows XP ");
      if (suite_mask & VER_SUITE_PERSONAL) {
        os_name.append("Home Edition");
      } else {
        os_name.append("Professional");
      }
    } else if (minor_version == 2) {
      if (GetSystemMetrics(SM_SERVERR2)) {
        os_name.append("Windows Server 2003 R2 ");
      } else if (suite_mask & VER_SUITE_STORAGE_SERVER) {
        os_name.append("Windows Storage Server 2003 ");
      } else if (suite_mask & VER_SUITE_WH_SERVER) {
        os_name.append("Windows Home Server");
      } else if (product_type == VER_NT_WORKSTATION) {
        os_name.append("Windows XP Professional");
      } else {
        os_name.append("Windows Server 2003 ");
      }
    } else {  // major version 5,6,10 아닌 경우 에러
      os_name.clear();
      os_name.append("");
    }
  }
  return os_name;
}

/*
  @brief  
    major version 6, minor version 2 이상일 경우 버그 체크.
    해당 버전 이상의 OS 는 모두 같은 값으로 나오므로 registry 로 OS 정보를 수집.
    version api 의 버그이며 이후 개선된 version api 를 사용하도록 변경 할 예정.
    저버전 OS 에서 지원하지 않으므로 Run-Time dynamic linking 이 되도록 해야하므로
    추후 작업.
*/
std::string OSInfo::GetNameException_(bool is_workstation) 
{
  HKEY hGetKey;
  CHAR buffer[256] = { 0, };
  DWORD value_type = 0, dwLength = sizeof(buffer);
  std::string buffer_string;
  std::string result_string;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_osversion_registry_path.c_str(), 
                    0, KEY_READ, &hGetKey) == ERROR_SUCCESS) {
    if (RegQueryValueEx(hGetKey, "ProductName", 0, &value_type,
                        (LPBYTE)buffer, &dwLength) == ERROR_SUCCESS) {
      if (REG_SZ == value_type && dwLength > 0) {
        buffer_string.assign(buffer);
        if (is_workstation) {
          if (buffer_string.find("Windows 8.1") != std::string::npos) {
            result_string.assign("Windows 8.1 ");
          } else if (buffer_string.find("Windows 8") != std::string::npos) {
            result_string.assign("Windows 8 ");
          } else if (buffer_string.find("Windows 10") != std::string::npos) {
            result_string.assign("Windows 10 ");
          }
        } else {
          if (buffer_string.find("Server 2012 R2") != std::string::npos) {
          result_string.assign("Windows Server 2012 R2 ");
          } else if (buffer_string.find("Server 2012") != std::string::npos) {
          result_string.assign("Windows Server 2012 ");
          } else if (buffer_string.find("Server 2016") != std::string::npos) {
          result_string.assign("Windows Server 2016 ");
          }
        }
      }
    } else {
      last_error_message_ = "Product is empty > " + std::to_string(GetLastError());
    }
    RegCloseKey(hGetKey); // HKEY close
  } else {
    last_error_message_ = "Failed to HKEY_LOCAL_MACHINE " 
      + g_osversion_registry_path + " > " + std::to_string(GetLastError());
  }
  return result_string;
}


// Windows OS 의 major version 이 5 일 경우의 product 정보를 가져온다.
std::string OSInfo::GetProductNameVersion5_(DWORD minor_version, DWORD product_type,
                                      WORD architecture, WORD suite_mask)
{
  std::string product_name("");
  if (product_type != VER_NT_WORKSTATION) {
    if (architecture == PROCESSOR_ARCHITECTURE_IA64) {
      if (suite_mask & VER_SUITE_DATACENTER) {
        product_name.append("Datacenter Edition for Itanium-based Systems");
      } else if (suite_mask & VER_SUITE_ENTERPRISE) {
        product_name.append("Enterprise Edition for Itanium-based Systems");
      }
    } else if (architecture == PROCESSOR_ARCHITECTURE_AMD64) {
      if (suite_mask & VER_SUITE_DATACENTER) {
        product_name.append("Datacenter x64 Edition");
      } else if (suite_mask & VER_SUITE_ENTERPRISE) {
        product_name.append("Enterprise x64 Edition");
      } else {
        product_name.append("Standard x64 Edition");
      }
    } else {
      if (suite_mask & VER_SUITE_COMPUTE_SERVER) {
        product_name.append("Compute Cluster Edition");
      } else if (suite_mask & VER_SUITE_DATACENTER) {
        product_name.append("Datacenter Edition");
      } else if (suite_mask & VER_SUITE_ENTERPRISE) {
        product_name.append("Enterprise Edition");
      } else if (suite_mask & VER_SUITE_BLADE) {
        product_name.append("Web Edition");
      } else {
        product_name.append("Standard Edition");
      }
    }
  }
  return product_name;
}

// Windows major version 6 의 제품 정보를 produt type 값으로 가져온다.
// 참조 : https://msdn.microsoft.com/en-us/library/windows/desktop/ms724358(v=vs.85).aspx
std::string OSInfo::GetProductNameVersion6_(DWORD product_type)
{
  std::string product_name("");
  switch (product_type) {
    case PRODUCT_ULTIMATE:
      product_name.append("Ultimate");
      break;
    case PRODUCT_PROFESSIONAL:
      product_name.append("Professional");
      break;
    case PRODUCT_HOME_PREMIUM:
      product_name.append("Home Premium Edition");
      break;
    case PRODUCT_HOME_BASIC:
      product_name.append("Home Basic Edition");
      break;
    case PRODUCT_ENTERPRISE:
      product_name.append("Enterprise Edition");
      break;
    case PRODUCT_BUSINESS:
      product_name.append("Business Edition");
      break;
    case PRODUCT_STARTER:
      product_name.append("Starter Edition");
      break;
    case PRODUCT_CLUSTER_SERVER:
      product_name.append("Cluster Server Edition");
      break;
    case PRODUCT_DATACENTER_SERVER:
      product_name.append("Datacenter Edition");
      break;
    case PRODUCT_DATACENTER_SERVER_CORE:
      product_name.append("Datacenter Edition (core installation)");
      break;
    case PRODUCT_ENTERPRISE_SERVER:
      product_name.append("Enterprise Edition");
      break;
    case PRODUCT_ENTERPRISE_SERVER_CORE:
      product_name.append("Enterprise Edition (core installation)");
      break;
    case PRODUCT_ENTERPRISE_SERVER_IA64:
      product_name.append("Enterprise Edition for Itanium-based Systems");
      break;
    case PRODUCT_SMALLBUSINESS_SERVER:
      product_name.append("Small Business Server");
      break;
    case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
      product_name.append("Small Business Server Premium Edition");
      break;
    case PRODUCT_STANDARD_SERVER:
      product_name.append("Standard Edition");
      break;
    case PRODUCT_STANDARD_SERVER_CORE:
      product_name.append("Standard Edition (core installation)");
      break;
    case PRODUCT_STANDARD_SERVER_CORE_V:
      product_name.append("Standard without Hyper-V (core installation)");
      break;
    case PRODUCT_STANDARD_SERVER_V:
      product_name.append("Standard without Hyper-V");
      break;
    case PRODUCT_WEB_SERVER:
      product_name.append("Web Server Edition");
      break;
    case PRODUCT_STORAGE_ENTERPRISE_SERVER:
      product_name.append("Storage Server Enterprise");
      break;
    case PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE:
      product_name.append("Storage Server Enterprise (core installation)");
      break;
    case PRODUCT_STORAGE_EXPRESS_SERVER:
      product_name.append("Storage Server Express");
      break;
    case PRODUCT_STORAGE_EXPRESS_SERVER_CORE:
      product_name.append("Storage Server Express (core installation)");
      break;
    //case PRODUCT_STORAGE_STANDARD_EVALUATION_SERVER:
    //  product_name.append("Storage Server Standard (evaluation installation)");
    //  break;
    case PRODUCT_STORAGE_STANDARD_SERVER_CORE:
      product_name.append("Storage Server Standard (core installation)");
      break;
    case PRODUCT_STORAGE_STANDARD_SERVER:
      product_name.append("Storage Server Standard");
      break;
    //case PRODUCT_STORAGE_WORKGROUP_EVALUATION_SERVER:
    //  product_name.append("Storage Server Workgroup (evaluation installation)");
    //  break;
    case PRODUCT_STORAGE_WORKGROUP_SERVER:
      product_name.append("Storage Server Workgroup");
      break;
    case PRODUCT_STORAGE_WORKGROUP_SERVER_CORE:
      product_name.append("Storage Server Workgroup (core installation)");
      break;
  }
  return product_name;
}

// system language id 를 language 국가명을 리턴한다.
std::string OSInfo::GetLanguageString_(DWORD language_id)
{
  std::string language_string = "";
  switch (language_id) {
    case 3081:
    case 4105:
    case 2057:
    case 16393:
    case 17417:
    case 5129:
    case 61449:
    case 18441:
    case 7177:
    case 1033:
      language_string.append("English");
      break;
    case 3079:
    case 1031:
    case 2055:
      language_string.append("German");
      break;
    case 2060:
    case 3084:
    case 1036:
    case 4108:
      language_string.append("French");
      break;
    case 58378:
    case 2058:
    case 3082:
    case 21514:
      language_string.append("Spanish");
      break;
    case 1041:
      language_string.append("Japanese");
      break;
    case 1042:
      language_string.append("Korean");
      break;
    default:
      language_string.append("Other");
      break;
  }
  return language_string;
}

// architecture 값으로 해당하는 architecture 명을 리턴한다.
std::string OSInfo::GetArchitectureString_(WORD architecture)
{
  std::string architecture_string = "";
  switch (architecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      architecture_string = "x64";
      break;
    case PROCESSOR_ARCHITECTURE_ARM:
      architecture_string = "ARM";
      break;
    //case PROCESSOR_ARCHITECTURE_ARM64:
    //  architecture_string = "ARM64";
    //  break;
    case PROCESSOR_ARCHITECTURE_IA64:
      architecture_string = "Intel Itanium-based";
      break;
    case PROCESSOR_ARCHITECTURE_INTEL:
      architecture_string = "x86";
      break;
    case PROCESSOR_ARCHITECTURE_UNKNOWN:
      architecture_string = "Unknown architecture";
      break;
  }
  return architecture_string;
}

} // namespace inspect
