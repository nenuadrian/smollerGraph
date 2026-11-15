# 9\. Errors as absl::Status, without exceptions

## Status

Accepted

## Context

Google's C++ style guide, which the code follows ([0001](0001-cpp-in-googles-style.md)), doesn't
use exceptions. So from episode 1, a function that could fail returned a `bool`, and wrote what went
wrong into a string its caller passed it, as `LoadFacts` did. Episode 1 gave the reason for a
value: "An error that comes back as a value also shows up in the function's signature, so a caller
can see that it has to check." By episode 4 the pager, the record files, the graph and the facts
all worked that way, and a string can't say what kind of error it holds.

## Decision

- A function that can fail returns `absl::Status`, or `absl::StatusOr<T>` when it returns a value
  (`ep004/step-2`). A status is a code and a message, and the codes are a fixed list, the canonical
  errors, such as not found, invalid argument and permission denied, "so the code that gets an
  error can tell one kind from another without reading the message".
- Abseil comes from its release of May 2025, 20250512.1, the one nanoMMO, the MMO from another
  series on this channel, uses too. CMake fetches it, and the library links its status, status-or
  and strings publicly.
- `ErrnoToStatus` turns the system's error into the code that goes with it, such as not found for a
  missing folder, and a fact that can't be read is an invalid argument, with the line as its
  message.
- Abseil's string functions come with it: `absl::StrCat` puts messages together, and
  `absl::SimpleAtoi` reads numbers from the command line.

## Alternatives considered

- Exceptions, which the style guide doesn't use.
- A `bool` and an error string, as the code did until episode 4.

## Consequences

- The JSON answers are `absl::StatusOr<std::string>`, and `sg serve` turns their codes into HTTP's:
  404 for something that isn't there, and 400 for a request that's wrong, such as a query that
  doesn't parse (episodes 4 and 7).
- The tests compare statuses with `absl::OkStatus()`, so a failure prints the code and the message.
- Three failures stop the program rather than return. A constructor can't return a status, so a
  `Graph` that can't get its temporary files stops with the status's text (episodes 2 and 4). A
  page that can't be read or written once its file is open stops it too, as that "leaves nothing
  sensible to carry on with"; "in a database meant to run for months, we'd want something gentler"
  (`Fail` in `pager.cc`, episode 2). And a buffer pool whose pages are all pinned has "nothing to
  evict, so we stop with an error" (`BufferPool::Victim`, episode 5).
- `.clang-tidy` tells `performance-no-automatic-move` that an `absl::Status` returned without a
  move is fine, as a status is cheap to copy (episode 4).
- Everything that links `smollergraph` gets Abseil too, and a first build compiles the parts of it
  we link.
