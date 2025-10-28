#include "Config.hpp"
#include "Utils.hpp"

#include <syslog.h>

#include <cstdlib>
#include <string>

Config &Config::instance() { static Config cfg; return cfg; }

bool Config::loadFromFile(const std::string &path) {
  std::string abs = utils::toAbsolutePath(path);
  std::string content;
  if (!utils::readFileToString(abs, content)) {
    syslog(LOG_ERR, "Failed to read config file: %s", abs.c_str());
    return false;
  }
  if (!parse(content)) return false;
  configPathAbs = abs;
  return true;
}

void Config::setConfigPath(const std::string &absPath) { configPathAbs = absPath; }
const std::string &Config::getConfigPath() const { return configPathAbs; }

unsigned int Config::getIntervalMs() const { return intervalMs; }
const std::vector<CopyRule> &Config::getRules() const { return rules; }

bool Config::parse(const std::string &content) {
  rules.clear();
  intervalMs = 5000;
  std::string line;
  std::string buf = content;

  size_t pos = 0;
  while (pos < buf.size()) {
    size_t end = buf.find('\n', pos);
    if (end == std::string::npos) end = buf.size();
    line = buf.substr(pos, end - pos);
    utils::trimWhitespace(line);
    pos = end + 1;
    if (line.empty() || line[0] == '#') continue;

    // interval=<ms>
    const std::string prefix = "interval=";
    if (line.rfind(prefix, 0) == 0) {
      std::string val = line.substr(prefix.size());
      utils::trimWhitespace(val);
      char *endp = nullptr;
      long ms = std::strtol(val.c_str(), &endp, 10);
      if (endp && *endp == '\0' && ms > 0) {
        intervalMs = static_cast<unsigned int>(ms);
      } else {
        syslog(LOG_WARNING, "Invalid interval value: %s", val.c_str());
      }
      continue;
    }

    // rule: folder1 folder2 ext
    auto tokens = utils::splitWhitespace(line);
    if (tokens.size() == 3) {
      CopyRule r;
      r.sourceDir = utils::toAbsolutePath(tokens[0]);
      r.destDir = utils::toAbsolutePath(tokens[1]);
      r.extension = tokens[2];
      if (!r.extension.empty() && r.extension[0] == '.') r.extension.erase(0, 1);
      rules.push_back(r);
    } else {
      syslog(LOG_WARNING, "Invalid rule line: %s", line.c_str());
    }
  }
  if (rules.empty()) {
    syslog(LOG_ERR, "No valid copy rules found in config");
    return false;
  }
  return true;
}


