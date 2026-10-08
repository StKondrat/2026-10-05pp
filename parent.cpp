#include <windows.h>
#include <cstddef>
#include <iostream>
#include <string>

size_t send_msg(DWORD& err, HANDLE wr, const char* buffer, size_t size)
{
  err = ERROR_SUCCESS;
  size_t total = 0;

  while (total < size)
  {
    size_t remaining = size - total;
    DWORD portion = static_cast< DWORD >(remaining > 65536 ? 65536 : remaining);
    DWORD count = 0;

    if (!WriteFile(wr, buffer + total, portion, &count, nullptr))
    {
      err = GetLastError();
      return total;
    }

    if (count == 0)
    {
      err = ERROR_WRITE_FAULT;
      return total;
    }

    total += count;
  }

  return total;
}

int main()
{
  HANDLE rd = nullptr;
  HANDLE wr = nullptr;

  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = FALSE;

  if (!CreatePipe(&rd, &wr, &attributes, 0))
  {
    std::cerr << "CreatePipe: " << GetLastError() << '\n';
    return 1;
  }

  if (!SetHandleInformation(rd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
  {
    std::cerr << "SetHandleInformation: " << GetLastError() << '\n';
    CloseHandle(rd);
    CloseHandle(wr);
    return 1;
  }

  std::string command = "child.exe ";
  command += std::to_string(reinterpret_cast< ULONG_PTR >(rd));

  STARTUPINFOA startup = {};
  startup.cb = sizeof(startup);

  PROCESS_INFORMATION process = {};

  if (!CreateProcessA(
        ".\\child.exe",
        command.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startup,
        &process))
  {
    std::cerr << "CreateProcessA: " << GetLastError() << '\n';
    CloseHandle(rd);
    CloseHandle(wr);
    return 1;
  }

  CloseHandle(rd);
  CloseHandle(process.hThread);

  std::string message;
  std::cout << "Enter the message: ";

  int result = 0;
  DWORD err = ERROR_SUCCESS;

  if (!std::getline(std::cin, message))
  {
    std::cerr << "Failed to read the message\n";
    result = 1;
  }
  else
  {
    size_t size = message.size();

    send_msg(err, wr, reinterpret_cast< const char* >(&size), sizeof(size));

    if (err == ERROR_SUCCESS)
    {
      send_msg(err, wr, message.data(), size);
    }

    if (err != ERROR_SUCCESS)
    {
      std::cerr << "WriteFile: " << err << '\n';
      result = 1;
    }
  }

  CloseHandle(wr);

  DWORD waited = WaitForSingleObject(process.hProcess, INFINITE);

  if (waited != WAIT_OBJECT_0)
  {
    std::cerr << "WaitForSingleObject: " << GetLastError() << '\n';
    CloseHandle(process.hProcess);
    return 1;
  }

  DWORD exitCode = 0;

  if (!GetExitCodeProcess(process.hProcess, &exitCode))
  {
    std::cerr << "GetExitCodeProcess: " << GetLastError() << '\n';
    result = 1;
  }
  else if (exitCode != 0)
  {
    result = 1;
  }

  CloseHandle(process.hProcess);

  return result;
}
