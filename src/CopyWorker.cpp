#include "CopyWorker.hpp"
#include "Utils.hpp"

#include <syslog.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

static bool hasExtension(const std::string &name, const std::string &ext) {
  if (ext.empty()) return false;
  size_t pos = name.rfind('.');
  if (pos == std::string::npos) return false;
  std::string e = name.substr(pos + 1);
  return e == ext;
}

bool CopyWorker::clearDirectory(const std::string &path) {
  DIR *dir = ::opendir(path.c_str());
  if (!dir) {
    syslog(LOG_ERR, "Failed to open dest dir for clear: %s: %s", path.c_str(), std::strerror(errno));
    return false;
  }
  struct dirent *entry;
  while ((entry = ::readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    std::string full = path + "/" + name;
    struct stat st{};
    if (::lstat(full.c_str(), &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) {
      // recursively delete directory
      // simple recursion: clear contents then rmdir
      CopyWorker::clearDirectory(full);
      if (::rmdir(full.c_str()) != 0) {
        syslog(LOG_WARNING, "Failed to rmdir %s: %s", full.c_str(), std::strerror(errno));
      }
    } else {
      if (::unlink(full.c_str()) != 0) {
        syslog(LOG_WARNING, "Failed to unlink %s: %s", full.c_str(), std::strerror(errno));
      }
    }
  }
  ::closedir(dir);
  return true;
}

static bool copyFile(const std::string &src, const std::string &dst) {
  std::ifstream in(src, std::ios::binary);
  if (!in.is_open()) return false;
  std::ofstream out(dst, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) return false;
  out << in.rdbuf();
  return out.good();
}

bool CopyWorker::copyByExtension(const std::string &srcDir, const std::string &dstDir, const std::string &ext) {
  DIR *dir = ::opendir(srcDir.c_str());
  if (!dir) {
    syslog(LOG_ERR, "Failed to open source dir: %s: %s", srcDir.c_str(), std::strerror(errno));
    return false;
  }
  struct dirent *entry;
  while ((entry = ::readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    std::string fullSrc = srcDir + "/" + name;
    struct stat st{};
    if (::lstat(fullSrc.c_str(), &st) != 0) continue;
    if (S_ISREG(st.st_mode)) {
      if (hasExtension(name, ext)) {
        std::string fullDst = dstDir + "/" + name;
        if (!copyFile(fullSrc, fullDst)) {
          syslog(LOG_WARNING, "Failed to copy %s to %s", fullSrc.c_str(), fullDst.c_str());
        }
      }
    }
  }
  ::closedir(dir);
  return true;
}

bool CopyWorker::performOnce(const std::vector<CopyRule> &rules) {
  bool ok = true;
  for (const auto &r : rules) {
    if (!utils::ensureDirectory(r.destDir)) {
      syslog(LOG_ERR, "Cannot ensure dest dir: %s", r.destDir.c_str());
      ok = false; continue;
    }
    if (!utils::directoryExists(r.sourceDir)) {
      syslog(LOG_ERR, "Source dir does not exist: %s", r.sourceDir.c_str());
      ok = false; continue;
    }
    clearDirectory(r.destDir);
    copyByExtension(r.sourceDir, r.destDir, r.extension);
  }
  return ok;
}


