#include "Utils.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace utils {

std::string toAbsolutePath(const std::string &path) {
  if (path.empty()) {
    return path;
  }
  if (path[0] == '/') {
    return path;
  }
  char cwd[4096];
  if (getcwd(cwd, sizeof(cwd)) == nullptr) {
    return path;
  }
  std::string result = std::string(cwd) + "/" + path;
  return result;
}

bool fileExists(const std::string &path) {
  struct stat st{};
  return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool directoryExists(const std::string &path) {
  struct stat st{};
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool ensureDirectory(const std::string &path) {
  if (directoryExists(path)) {
    return true;
  }
  return ::mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

bool readFileToString(const std::string &path, std::string &out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << ifs.rdbuf();
  out = ss.str();
  return true;
}

bool writeStringToFile(const std::string &path, const std::string &data) {
  std::ofstream ofs(path, std::ios::trunc);
  if (!ofs.is_open()) {
    return false;
  }
  ofs << data;
  return ofs.good();
}

static inline bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool trimWhitespace(std::string &s) {
  size_t start = 0;
  while (start < s.size() && isSpace(s[start])) {
    start++;
  }
  size_t end = s.size();
  while (end > start && isSpace(s[end - 1])) {
    end--;
  }
  if (start == 0 && end == s.size()) {
    return false;
  }
  s = s.substr(start, end - start);
  return true;
}

std::vector<std::string> splitWhitespace(const std::string &s) {
  std::vector<std::string> tokens;
  std::string token;
  for (size_t i = 0; i < s.size(); ++i) {
    if (isSpace(s[i])) {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    } else {
      token.push_back(s[i]);
    }
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

}


