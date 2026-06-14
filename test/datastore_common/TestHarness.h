#pragma once

// Shared harness for native lib/DataStore tests.
//
// Each test .cpp compiles standalone (see the differential_rounding /
// release_json_parser tests for the pattern) and links against the
// simulator's HalStorage, which persists to a real POSIX directory rooted at
// $CROSSPOINT_SIM_SD. TempStorageDir below points that root at a fresh
// mkdtemp() directory per test binary run and removes it on destruction, so
// tests never touch the repo working tree and start from a clean filesystem.

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

inline int testsPassed = 0;
inline int testsFailed = 0;

#define ASSERT_EQ(a, b)                                                                                    \
  do {                                                                                                     \
    auto _va = (a);                                                                                        \
    auto _vb = (b);                                                                                        \
    if (!(_va == _vb)) {                                                                                   \
      std::ostringstream _oa, _ob;                                                                         \
      _oa << _va;                                                                                          \
      _ob << _vb;                                                                                          \
      fprintf(stderr, "  FAIL: %s:%d: %s == %s, expected %s\n", __FILE__, __LINE__, #a, _oa.str().c_str(), \
              _ob.str().c_str());                                                                          \
      testsFailed++;                                                                                       \
      return;                                                                                              \
    }                                                                                                      \
  } while (0)

#define ASSERT_TRUE(cond)                                                \
  do {                                                                   \
    if (!(cond)) {                                                       \
      fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      testsFailed++;                                                     \
      return;                                                            \
    }                                                                    \
  } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define PASS() testsPassed++

// Recursively remove a directory tree rooted at `path`.
inline void removeTree(const std::string& path) {
  DIR* d = opendir(path.c_str());
  if (!d) {
    ::remove(path.c_str());
    return;
  }
  struct dirent* entry;
  while ((entry = readdir(d)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    std::string child = path + "/" + entry->d_name;
    struct stat st;
    if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
      removeTree(child);
    } else {
      ::remove(child.c_str());
    }
  }
  closedir(d);
  ::rmdir(path.c_str());
}

// Points the simulator's HalStorage at a fresh temp directory for the
// lifetime of this object, then removes it on destruction.
class TempStorageDir {
 public:
  TempStorageDir() {
    char tmpl[] = "/tmp/myne_datastore_test_XXXXXX";
    char* dir = mkdtemp(tmpl);
    path_ = dir ? dir : "";
    setenv("CROSSPOINT_SIM_SD", path_.c_str(), 1);
    setenv("CROSSPOINT_EMU_SD", path_.c_str(), 1);
  }

  ~TempStorageDir() {
    if (!path_.empty()) removeTree(path_);
    unsetenv("CROSSPOINT_SIM_SD");
    unsetenv("CROSSPOINT_EMU_SD");
  }

  TempStorageDir(const TempStorageDir&) = delete;
  TempStorageDir& operator=(const TempStorageDir&) = delete;

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};
