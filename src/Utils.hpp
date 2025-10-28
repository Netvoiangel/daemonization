#pragma once

#include <string>
#include <vector>

namespace utils {

std::string toAbsolutePath(const std::string &path);
bool fileExists(const std::string &path);
bool directoryExists(const std::string &path);
bool ensureDirectory(const std::string &path);
bool readFileToString(const std::string &path, std::string &out);
bool writeStringToFile(const std::string &path, const std::string &data);
bool trimWhitespace(std::string &s);
std::vector<std::string> splitWhitespace(const std::string &s);

}


