# 25\. Downloads for Linux, macOS, Windows and the web, built from a version's tag

## Status

Accepted

## Context

Until episode 21, using `sg` meant building it: CMake 3.28, Ninja and a C++20 compiler, and Linux
or macOS, as the pager read and wrote pages with `pread` and `pwrite`, and made its temporary file
with `mkstemp` in `/tmp`, which Windows doesn't have. A build copied to another computer didn't
work there either: `sg serve` served the UI's page from the source tree's `ui` folder, a path
compiled into the program. The explorer ran in a browser from GitHub Pages, and taking it anywhere
else meant building it with Emscripten. Built on Ubuntu 24.04, `sg` asked for glibc 2.38, and
didn't start on Ubuntu 22.04 or Debian 12.

## Decision

- The pager has a Windows version of each call it makes to the system, `ReadFile` and `WriteFile`
  with the offset in an `OVERLAPPED`, and `GetTempFileName` with `_O_TEMPORARY`. Folders are made
  with `std::filesystem`, which Google's style guide says not to use, and cpplint is told so.
- On Windows, `sg`'s manifest asks for UTF-8 as its code page, for its arguments, and it tells the
  console what it prints is UTF-8. It carries Microsoft's C++ runtime, as does Abseil's build.
- `sg serve` looks for a `ui` folder beside `sg` first, and for the source tree's after it.
- CMake's install rules say what a download holds, `sg`, `ui`, `data`, the README and the licence,
  and CPack packs it: a `.tar.gz` for Linux, a `.zip` for macOS and Windows, and a `.zip` of the
  explorer's folder from the browser build.
- Linux's archives are built in Docker, on Ubuntu 22.04 with GCC 12, the oldest system they're
  for, on an x86 runner and an Arm one, and `sg` carries its own libstdc++, as RHEL 9 has GCC 11's. macOS's is one universal binary, for Apple's processors and
  Intel's, for macOS 13 and later. Windows's is built with MSVC.
- A version's tag pushed to the public repository runs `.github/workflows/release.yml`, which builds
  each archive on GitHub's runner for its system, runs the tests there, writes the archives'
  SHA-256 checksums to `SHA256SUMS`, attests where each archive came from, and makes the release,
  with the tag's message as its notes.

## Alternatives considered

- Cross-compiling every archive on one Linux runner, with MinGW for Windows and osxcross for macOS:
  one machine to set up, but a Windows build with GCC, not MSVC, and macOS's SDK on a computer that
  isn't a Mac, which Apple's licence doesn't allow. A MinGW build was used once, to see whether the
  Windows code compiled, before a Windows runner ran it.
- A static Linux build with musl, which runs on any Linux, old or new. Not tried: building on the
  oldest system was a change to one image.
- Building the archives on our own computers and uploading them: nothing to say which commit built
  them, or how.
- Packages for Homebrew, apt and winget, which install `sg` with a command: a repository or a
  manifest to keep for each, for a database few people use yet.
- Signing the programs, with an Apple Developer ID and a Windows code signing certificate: both
  cost money every year. Left for later.

## Consequences

- `sg` isn't signed with an Apple Developer ID, so macOS won't open a copy downloaded with a browser
  until it's allowed in Privacy & Security, or `xattr -d com.apple.quarantine sg` takes the
  download's mark off it.
- Linux's archives need Ubuntu 22.04, Debian 12, Fedora 41, Rocky Linux 9, Amazon Linux 2023 or a
  newer system. Ubuntu 20.04 and Debian 11 have an older glibc.
- A release runs seven jobs, on Macs and Windows among them, which a private repository pays for
  in Actions minutes at several times Linux's rate, so a tag builds only in the public repository,
  where they're free. In the private one, the workflow runs by hand.
- Each archive holds the facts, 3.3 MB of them for Wikidata, so `sg` has something to load.
- The code has a Windows branch in two files, the pager and `sg`'s `main.cc`, which only a Windows
  build compiles, and only a release, or the workflow run by hand, builds.
