#pragma once

#include <string>
#include <vector>
#include <optional>

struct CopyRule {
  std::string sourceDir;
  std::string destDir;
  std::string extension; // without dot
};

class Config {
public:
  static Config &instance();

  bool loadFromFile(const std::string &path);
  void setConfigPath(const std::string &absPath);
  const std::string &getConfigPath() const;

  // milliseconds between iterations
  unsigned int getIntervalMs() const;
  const std::vector<CopyRule> &getRules() const;

private:
  Config() = default;
  bool parse(const std::string &content);

  std::string configPathAbs;
  unsigned int intervalMs{5000};
  std::vector<CopyRule> rules;
};


