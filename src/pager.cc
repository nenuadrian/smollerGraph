#include "pager.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace smollergraph {
namespace {

// A page that can't be read or written once the file is open leaves nothing
// sensible to carry on with, so the program stops and says why.
[[noreturn]] void Fail(const char* what) {
  std::fprintf(stderr, "smollergraph: %s: %s\n", what, std::strerror(errno));
  std::abort();
}

}  // namespace

Pager::~Pager() {
  if (fd_ >= 0) close(fd_);
}

bool Pager::Open(const std::string& path, std::string& error) {
  return Adopt(open(path.c_str(), O_RDWR | O_CREAT, 0644), error);
}

bool Pager::OpenTemporary(std::string& error) {
  char path[] = "/tmp/smollergraph-XXXXXX";
  int fd = mkstemp(path);
  // Without a name, the file lasts as long as it's open.
  if (fd >= 0) unlink(path);
  return Adopt(fd, error);
}

bool Pager::Adopt(int fd, std::string& error) {
  struct stat info;
  if (fd < 0 || fstat(fd, &info) != 0) {
    error = std::strerror(errno);
    if (fd >= 0) close(fd);
    return false;
  }
  if (fd_ >= 0) close(fd_);
  fd_ = fd;
  page_count_ = static_cast<PageId>(info.st_size / kPageSize);
  return true;
}

void Pager::Read(PageId id, Page& page) const {
  off_t offset = static_cast<off_t>(id) * kPageSize;
  if (pread(fd_, page.data(), kPageSize, offset) !=
      static_cast<ssize_t>(kPageSize)) {
    Fail("can't read a page");
  }
}

void Pager::Write(PageId id, const Page& page) {
  off_t offset = static_cast<off_t>(id) * kPageSize;
  if (pwrite(fd_, page.data(), kPageSize, offset) !=
      static_cast<ssize_t>(kPageSize)) {
    Fail("can't write a page");
  }
  if (id >= page_count_) page_count_ = id + 1;
}

}  // namespace smollergraph
