#include "smollergraph/pager.h"

#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <share.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace smollergraph {
namespace {

// A page that can't be read or written once the file is open leaves nothing
// sensible to carry on with, so the program stops and says why.
[[noreturn]] void Fail(const char* what) {
  std::fprintf(stderr, "smollergraph: %s: %s\n", what, std::strerror(errno));
  std::abort();
}

// What a pager asks of the operating system: open a file, read and write a
// page at an offset, and find the file's size. Windows has its own calls for
// each, so each has two versions.
#ifdef _WIN32

int OpenFile(const char* path) {
  int fd = -1;
  _sopen_s(&fd, path, _O_RDWR | _O_CREAT | _O_BINARY, _SH_DENYNO,
           _S_IREAD | _S_IWRITE);
  return fd;
}

// GetTempFileName makes an empty file, with a name no other file has, in the
// folder for temporary files, and _O_TEMPORARY deletes it once it's closed.
int OpenTemporaryFile() {
  char folder[MAX_PATH + 1];
  char path[MAX_PATH];
  if (GetTempPathA(sizeof(folder), folder) == 0 ||
      GetTempFileNameA(folder, "sg", 0, path) == 0) {
    errno = ENOENT;
    return -1;
  }
  int fd = -1;
  _sopen_s(&fd, path, _O_RDWR | _O_BINARY | _O_TEMPORARY, _SH_DENYNO,
           _S_IREAD | _S_IWRITE);
  return fd;
}

void CloseFile(int fd) { _close(fd); }

int64_t FileSize(int fd) {
  struct _stat64 info;
  return _fstat64(fd, &info) == 0 ? info.st_size : -1;
}

// Windows has no pread or pwrite. ReadFile and WriteFile take the offset in
// an OVERLAPPED, so each call says where it reads or writes, as pread and
// pwrite do, and threads reading pages at once don't move a position they
// share.
bool ReadAt(int fd, void* data, int64_t offset) {
  OVERLAPPED at = {};
  at.Offset = static_cast<DWORD>(offset);
  at.OffsetHigh = static_cast<DWORD>(offset >> 32);
  DWORD done = 0;
  HANDLE file = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (ReadFile(file, data, DWORD{kPageSize}, &done, &at) && done == kPageSize) {
    return true;
  }
  errno = EIO;
  return false;
}

bool WriteAt(int fd, const void* data, int64_t offset) {
  OVERLAPPED at = {};
  at.Offset = static_cast<DWORD>(offset);
  at.OffsetHigh = static_cast<DWORD>(offset >> 32);
  DWORD done = 0;
  HANDLE file = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (WriteFile(file, data, DWORD{kPageSize}, &done, &at) &&
      done == kPageSize) {
    return true;
  }
  errno = EIO;
  return false;
}

#else

int OpenFile(const char* path) { return open(path, O_RDWR | O_CREAT, 0644); }

int OpenTemporaryFile() {
  char path[] = "/tmp/smollergraph-XXXXXX";
  int fd = mkstemp(path);
  // Without a name, the file lasts as long as it's open.
  if (fd >= 0) unlink(path);
  return fd;
}

void CloseFile(int fd) { close(fd); }

int64_t FileSize(int fd) {
  struct stat info;
  return fstat(fd, &info) == 0 ? info.st_size : -1;
}

bool ReadAt(int fd, void* data, int64_t offset) {
  return pread(fd, data, kPageSize, static_cast<off_t>(offset)) ==
         static_cast<ssize_t>(kPageSize);
}

bool WriteAt(int fd, const void* data, int64_t offset) {
  return pwrite(fd, data, kPageSize, static_cast<off_t>(offset)) ==
         static_cast<ssize_t>(kPageSize);
}

#endif

}  // namespace

Pager::~Pager() {
  if (fd_ >= 0) CloseFile(fd_);
}

absl::Status Pager::Open(const std::string& path) {
  return Adopt(OpenFile(path.c_str()), path);
}

absl::Status Pager::OpenTemporary() {
  return Adopt(OpenTemporaryFile(), "a temporary file");
}

absl::Status Pager::Adopt(int fd, std::string_view name) {
  int64_t size = fd < 0 ? -1 : FileSize(fd);
  if (size < 0) {
    // ErrnoToStatus picks the status code that goes with the system's error.
    absl::Status status =
        absl::ErrnoToStatus(errno, absl::StrCat("can't open ", name));
    if (fd >= 0) CloseFile(fd);
    return status;
  }
  if (fd_ >= 0) CloseFile(fd_);
  fd_ = fd;
  page_count_ = static_cast<PageId>(size / kPageSize);
  return absl::OkStatus();
}

void Pager::Read(PageId id, Page& page) const {
  if (!ReadAt(fd_, page.data(), int64_t{id} * int64_t{kPageSize})) {
    Fail("can't read a page");
  }
  ++reads_;
}

void Pager::Write(PageId id, const Page& page) {
  if (!WriteAt(fd_, page.data(), int64_t{id} * int64_t{kPageSize})) {
    Fail("can't write a page");
  }
  if (id >= page_count_) page_count_ = id + 1;
  ++writes_;
}

}  // namespace smollergraph
