#ifndef __INSPECT_SCRIPT_RUNNER_H__
#define __INSPECT_SCRIPT_RUNNER_H__

#include <string>
#include <list>
#include <windows.h>

// @class ScriptRunner
// @brief  
//  스크립트를 실행하여 결과를 저장하기 위한 클래스
//  TODO: 생성자 실패시에 대한 예외체크 추가
class ScriptRunner {
public:
  ScriptRunner();
  ~ScriptRunner();
  // command_line 을 실행하여 결과를 버퍼에 저장. 성공시 true, 아니면 false
  bool Exec(const char* command_line);
  // 결과버퍼 초기화
  void FreeResult();
  // 테스트를 위한 버퍼출력
  void Print();
  
  //std::string PopResult();
  //bool IsEmptyResult() { return result_buffer_list_.empty(); }
  bool IsEmptyResult() { return result_buffer_.empty(); }
  std::string result_buffer() const { return result_buffer_; }

private:
  //std::list<std::string> result_buffer_list_;
  std::string result_buffer_ = "";

  HANDLE handle_child_input_read_ = nullptr;
  HANDLE handle_child_input_write_ = nullptr;
  HANDLE handle_child_output_read_ = nullptr;
  HANDLE handle_child_output_write_ = nullptr;

};

#endif // __INSPECT_SCRIPT_RUNNER_H__