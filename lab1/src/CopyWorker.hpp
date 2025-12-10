#pragma once

#include "Config.hpp"

#include <string>

class CopyWorker {
public:
  static bool performOnce(const std::vector<CopyRule> &rules);

private:
  static bool clearDirectory(const std::string &path);
  static bool copyByExtension(const std::string &srcDir, const std::string &dstDir, const std::string &ext);
};


