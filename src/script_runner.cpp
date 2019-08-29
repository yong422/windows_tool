#include "script_runner.h"
#include "inspect_common.h"

#include <iostream>
#include <windows.h>
#include <plog/Log.h>


#define DEFAULT_SR_WAIT_MSEC 100
#define DEFAULT_SR_READ_BUFFER_SIZE 4096
// TODO:  popen 을 사용하는 스크립트 실행과 관련되어 sub process 와 windows pipe 사용하도록 변경.
// 참조: https://docs.microsoft.com/ko-kr/windows/desktop/ProcThread/creating-a-child-process-with-redirected-input-and-output

// TODO: 결과에 대해 라인별로 버퍼리스트에 추가하는 기능 추가.
ScriptRunner::ScriptRunner()
{
  // ScriptRunner 에서 사용할 파이프 생성
  SECURITY_ATTRIBUTES security_attr;
  security_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attr.bInheritHandle = TRUE;
  security_attr.lpSecurityDescriptor = NULL;
  if (!CreatePipe(&handle_child_output_read_, &handle_child_output_write_, &security_attr, 0))
    LOG_ERROR_(inspect::kScriptRunner) << "StdoutRd CreatePipe >> " << GetLastError();
  if (!SetHandleInformation(handle_child_output_read_, HANDLE_FLAG_INHERIT, 0))
    LOG_ERROR_(inspect::kScriptRunner) << "Stdout SetHandleInformation >> " << GetLastError();
  if (!CreatePipe(&handle_child_input_read_, &handle_child_input_write_,&security_attr, 0))
    LOG_ERROR_(inspect::kScriptRunner) << "Stdin CreatePipe >> " << GetLastError();
  if (!SetHandleInformation(handle_child_input_write_, HANDLE_FLAG_INHERIT, 0))
    LOG_ERROR_(inspect::kScriptRunner) <<  "Stdin SetHandleInformation >> " << GetLastError();
  // ScriptRunner 생성시 만든 파이프는 ScriptRunner 삭제시 반환
}

ScriptRunner::~ScriptRunner()
{
  if (handle_child_input_read_) {
    CloseHandle(handle_child_input_read_);
    handle_child_input_read_ = nullptr;
  }
  if (handle_child_input_write_) {
    CloseHandle(handle_child_input_write_);
    handle_child_input_write_ = nullptr;
  }
  if (handle_child_output_read_) {
    CloseHandle(handle_child_output_read_);
    handle_child_output_read_ = nullptr;
  }
  if (handle_child_output_write_) {
    CloseHandle(handle_child_output_write_);
    handle_child_output_write_ = nullptr;
  }
}

// command_line 을 실행. 결과에 대해 버퍼에 저장
#if 1

bool ScriptRunner::Exec(const char* command_line)
{
  bool result = false;
  char buffer[1024] = { 0, };
  // 스크립트 실행전 버퍼 초기화
  result_buffer_.clear();
  if (command_line) {
    PROCESS_INFORMATION  proc_info;
    STARTUPINFO startup_info;
    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startup_info, sizeof(STARTUPINFO));
    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.hStdError = handle_child_output_write_;
    startup_info.hStdOutput = handle_child_output_write_;
    startup_info.hStdInput = handle_child_input_read_;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    result = CreateProcess(NULL,
      const_cast<char*>(command_line),      // command line 
      NULL,                                 // process security attributes 
      NULL,                                 // primary thread security attributes 
      TRUE,                                 // handles are inherited 
      CREATE_NO_WINDOW,                     // creation flags 
      NULL,                                 // use parent's environment 
      NULL,                                 // use parent's current directory 
      &startup_info,                        // STARTUPINFO pointer 
      &proc_info);                          // receives PROCESS_INFORMATION
    if (result) {
      DWORD read_size = 0;
      DWORD exit_code = 0;
      CHAR buffer[DEFAULT_SR_READ_BUFFER_SIZE] = { 0, };
      COMMTIMEOUTS timeout_value = { 0, };
      // named or anonymous pipe 버퍼를 체크
      // 파이프가 생성되있으면 true
      while (true) {
        // 호출한 프로세스의 대기상태 체크.
        exit_code = WaitForSingleObject(proc_info.hProcess, DEFAULT_SR_WAIT_MSEC);
        //std::cout << "process status > " << exit_code << std::endl;
        if (!PeekNamedPipe(handle_child_output_read_, NULL, 0, NULL, &read_size, NULL)) {
          // pipe 오류시 로깅후 브레이크
          LOG_ERROR_(inspect::kScriptRunner) << " broken pipe >> " << GetLastError();
          break;
        }
        if (read_size > 0) { // 파이프에 값이 있는 경우
          ReadFile(handle_child_output_read_, buffer, sizeof(buffer), &read_size, NULL);
          buffer[read_size] = NULL;
          result_buffer_.append(buffer);
          result = true;
        } else { 
          // 파이프에 값이 없는 경우
          // timeout (호출한 프로세스가 작업중인 경우) 또는 abandoned 인 경우 재시도.
          if (exit_code == WAIT_TIMEOUT || exit_code == WAIT_ABANDONED) {
            // 대기하던 process(thread) 가 대기중 강제종료되는 경우 WAIT_ABANDONED 리턴
            // 다시한번 파이프를 읽도록 체크하며 이벤트 초기화후 다시한번 체크.
            // 다시한번 체크시 종료처리 되있는 상태.
            if (exit_code == WAIT_ABANDONED) ResetEvent(proc_info.hProcess);
            Sleep(DEFAULT_SR_WAIT_MSEC);
          } else {
            // WAIT_OBJECT_0 (프로세스 종료) WAIT_FAILED (실패) 인 경우 종료
            break;
          }
        }
      }
    } else {
      LOG_ERROR_(inspect::kScriptRunner) << " CreateProcess failed >> "<<  command_line << " >> " << GetLastError();
    }
    if (proc_info.hProcess)  CloseHandle(proc_info.hProcess);
    if (proc_info.hThread)   CloseHandle(proc_info.hThread);
  }
  return result;
}
#else // popen version 일부서버에서 문제 발생으로 주석 추후삭제.
bool ScriptRunner::Exec(const char* command_line)
{
  bool result = false;
  FILE* file = nullptr;
  char buffer[1024] = { 0, };
  // 스크립트 실행전 버퍼 초기화
  result_buffer_.clear();
  if (command_line) {
    if ((file = _popen(command_line, "r")) != nullptr) {
#if defined(_TEST_MODE)
      std::cout << "script ok : " << command_line << std::endl;
#endif
      while (fgets(buffer, sizeof(buffer) - 1, file)) {
#if defined(_TEST_MODE)
        std::cout << "script result : " << buffer;
#endif
        //result_buffer_list_.push_back(buffer);
        result_buffer_.append(buffer);
      }
      //if (!result_buffer_list_.empty()) result = true;
      if (!result_buffer_.empty()) result = true;

      if (feof(file)) {
#if defined(_TEST_MODE)
        std::cout << "script close" << std::endl;
#endif
        _pclose(file);
      } else {
#if defined (_TEST_MODE)
        std::cout << "Failed to read the pipe to the end\n";
#endif
      }
    } else {
      LOG_ERROR_(inspect::kScriptRunner) << "failed popen["
        << GetLastError() << "] : " << command_line;
    }
  } else {
    LOG_ERROR_(inspect::kScriptRunner) << " empty command line";
  }
  return result;
}
#endif

#if 0
std::string ScriptRunner::PopResult()
{
  std::string result = result_buffer_list_.front();
  result_buffer_list_.pop_front();
  return result;
}
#endif

void ScriptRunner::FreeResult()
{
  result_buffer_.clear();
}

void ScriptRunner::Print()
{
  std::cout << "result : " << result_buffer_ << std::endl;
}