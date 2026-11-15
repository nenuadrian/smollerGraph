# 11\. Keep pages in a buffer pool of our own, with the clock

## Status

Accepted

## Context

Until episode 5, writing a record read its page and wrote it back, and appending wrote the header
too ([0007](0007-fixed-size-records-in-pages-of-4-kib.md)). Counted on the generated million
relationships, a load read about 3.1 million pages and wrote 3.2 million, six for each
relationship, each a system call, and a two-hop walk read 121 pages
([0003](0003-relationships-beside-their-nodes.md)). With the optimized build, before any change, a
load took 12.03 s and a walk 46.3 µs, and "most of that is still the system calls".

## Decision

- `BufferPool` (`buffer_pool.h`, `ep005/step-2`) keeps up to a number of a file's pages in frames,
  each a page-sized piece of memory, with a hash map from a page to its frame. `Fetch` pins a page
  and returns a `PageHandle`, which unpins it when it goes away, and marks it dirty when it's
  written to. A dirty page is written back before its frame takes another page.
- When the pool is full, the clock picks the page that leaves: a hand goes round the frames, gives a
  page fetched since it last passed a second chance by clearing its bit, skips pinned pages, and
  takes the first page that's neither. PostgreSQL uses a version of it.
- Each record file has a pool of up to 16,384 pages, 64 MiB, which holds the generated million
  relationships (`kPoolPages` in `record_file.h`), and frames are made as pages come in. The name
  index's pool has 4,096 pages, 16 MiB ([0012](0012-a-b-plus-tree-for-names.md)).
- One mutex, the pool's latch, keeps `sg serve`'s threads from changing the frames and the table
  together (`ep005/step-4`). It's held while a page is read or written too, "which keeps this
  simple, and makes other threads wait for the disk". A pinned page is read without it.

## Alternatives considered

- Leaving the caching to the operating system, with mmap. Episode 5 followed Crotty, Leis and
  Pavlo's paper, "Are you sure you want to use mmap in your database management system?": with
  mmap, the operating system decides when a changed page is written, which gets in the way of a log
  that has to be written first; a page that isn't in memory stalls the program while it's read; and
  with many threads, the operating system's own bookkeeping slows things down. Neo4j's page cache
  and Kùzu's buffer manager are pools of their own too.
- A list of the pages in order of use, where the clock "costs a bit for each frame", "which is why
  databases like it" (episode 5).

## Consequences

- In episode 5's comparison, which times `sg load` as a process from start to exit, a load went
  from 12.03 s to 1.07 s, and a two-hop walk from 46.3 µs to 5.8 µs, reading no pages. Run on its
  own in episode 5, `sg load` timed itself at 0.6 s, read no pages, and wrote 8,597, once each, when
  it flushed.
- A pool takes memory, up to 64 MiB for each record file, though a small graph uses little of it.
- A changed page lives in memory until it's flushed or evicted. A crash in the middle of a load
  loses what's in the pool, and can leave the files half written, "with a node pointing at a
  relationship that didn't make it to disk". That's what a write-ahead log is for, and the database
  has none up to episode 16.
- Without the latch, a test fetching pages on four threads through four frames crashed each time
  (episode 5). With it, threads wait for each other's reads from the disk, where "a busier database
  would let go of it while it waits".
- `Clear` flushes before it takes the latch, as a thread that takes a `std::mutex` it already holds
  waits forever.
- The pager counts the pages it reads and writes in atomics (`ep005/step-1`), which `sg load`,
  `sg bench`, `sg query`, `sg path` and the page's status line report.
