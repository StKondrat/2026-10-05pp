#include <windows.h>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <vector>

size_t recv_msg(DWORD& err, HANDLE rd, char* buffer, size_t size)
{
  err = ERROR_SUCCESS;
  size_t total = 0;

  while (total < size)
  {
    size_t remaining = size - total;
    DWORD portion = static_cast< DWORD >(
      remaining > 65536 ? 65536 : remaining
    );
    DWORD count = 0;

    if (!ReadFile(rd, buffer + total, portion, &count, nullptr))
    {
      err = GetLastError();
      return total;
    }

    if (count == 0)
    {
      err = ERROR_HANDLE_EOF;
      return total;
    }

    total += count;
  }

  return total;
}

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cerr << "Usage: child <handle>\n";
    return 1;
  }

  char* end = nullptr;
  errno = 0;

  unsigned long long value = std::strtoull(argv[1], &end, 10);

  if (errno != 0 || end == argv[1] || *end != '\0' ||
      argv[1][0] == '-' || value == 0 ||
      value > (std::numeric_limits< ULONG_PTR >::max)())
  {
    std::cerr << "Invalid handle\n";
    return 1;
  }

  HANDLE rd = reinterpret_cast< HANDLE >(
    static_cast< ULONG_PTR >(value)
  );

  if (rd == INVALID_HANDLE_VALUE)
  {
    std::cerr << "Invalid handle\n";
    return 1;
  }

  DWORD err = ERROR_SUCCESS;
  size_t size = 0;

  recv_msg(err, rd, reinterpret_cast< char* >(&size), sizeof(size));

  if (err != ERROR_SUCCESS)
  {
    std::cerr << "Failed to read the message size: " << err << '\n';
    CloseHandle(rd);
    return 1;
  }

  std::vector< char > message;

  try
  {
    message.resize(size);
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    CloseHandle(rd);
    return 1;
  }

  recv_msg(err, rd, message.data(), size);
  CloseHandle(rd);

  if (err != ERROR_SUCCESS)
  {
    std::cerr << "Failed to read the message: " << err << '\n';
    return 1;
  }

  std::cout << "The child process received: ";

  for (char symbol : message)
  {
    std::cout.put(symbol);
  }

  std::cout << '\n';
  std::cout.flush();

  if (!std::cout)
  {
    return 1;
  }

  return 0;
}
