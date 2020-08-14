#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <fstream>
#include <string>
#include <vector>

#ifdef WIN32
#include <comdef.h>
#include <pathcch.h>
#include <windows.h>
#endif

#ifdef WIN32
constexpr char pathSeparator = '\\';
#else
constexpr char pathSeparator = '/';
#endif

std::string getExecDirectory() {

#ifdef WIN32

  WCHAR path[MAX_PATH];
  GetModuleFileNameW(NULL, path, MAX_PATH);
  PathCchRemoveFileSpec(path, MAX_PATH);
  return std::string(_bstr_t(path));

#endif
}

std::vector<char> readFile(const std::string &filePath) {

  // ate -> start reading at the end of the file
  // binary -> read as binary (avoid text transformations)
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file " + filePath + " !");
  }

  // tellg tells us the cursor position; since we started reading from the end,
  // it matches with the file size
  size_t fileSize = static_cast<size_t>(file.tellg());

  std::vector<char> buffer(fileSize);

  // back to the start of the file
  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return std::move(buffer);
}

#endif // FILESYSTEM_UTILS_H