# 7\. Fixed-size records, in pages of 4 KiB

## Status

Accepted

## Context

Episode 2 moved the graph out of vectors in memory and into files, behind the functions episode 1's
tests call ([0003](0003-relationships-beside-their-nodes.md)). It looked first at SQLite's file
format, where the database file is pages of one size, a power of two from 512 bytes to 64 KiB, read
and written a page at a time.

## Decision

- A `Pager` (`pager.h`) reads and writes a file in pages of 4 KiB, `kPageSize`, whole, by their
  number, with `pread` and `pwrite`. A page starts at its id times 4,096.
- Episode 2's reason for 4 KiB: modern disks store data in sectors of 4 KiB, and most file systems
  hand out space in blocks of that size, so reading one of our pages is reading one block. Bigger
  pages mean fewer reads when a query scans a lot of data, smaller ones move less data for a single
  hop, and graph walks hop between records all over a file, "which suits smallish pages".
- `RecordFile<Record>` (`record_file.h`) keeps records of one size in a file: page 0 is a header
  with how many there are, and the records follow, as many to a page as fit. A record's id gives
  its page, the id divided by the records to a page, plus one, and its place in the page, so
  finding one needs no search, "which is what Neo4j relies on too". A `static_assert` keeps a
  record trivially copyable, as it's copied to and from a page as bytes.
- Each kind of record has a file of its own in the graph's folder: `nodes` and `relationships`,
  then `properties` (episode 9), `labels` and `types` (episode 11), and `embeddings` (episode 14).

## Alternatives considered

- Another page size. It's a single constant, and 4 KiB "is where we start". The Mac the episodes
  are made on, with an Apple chip, has memory pages of 16 KiB, "something we can measure later on";
  no episode up to 16 has.
- Small records, with a long name in records of its own, as Neo4j keeps long strings. Episode 10
  weighed it when Wikidata's names didn't fit: "A name in records of its own is a chain to follow,
  though, and bigger records keep a name one read away." The records grew instead.

## Consequences

- A fixed size is a limit on each name. Episode 2's records were 32 bytes, with names up to 27
  bytes and types up to 19, and episode 9 gave four bytes of each to properties, leaving names 23.
  Wikidata's names run to 42 bytes, so about 2,800 names didn't fit, and about 16,000 scientific
  names didn't fit a string of 14 (episode 10). From episode 10 a node and a property take 64
  bytes, and a name can be 43 bytes long, and from episode 11, with tokens in place of labels and
  types, 47 ([0017](0017-tokens-and-two-chains.md)).
- Bigger records take more pages. A page held 128 nodes, and from episode 10, 64, so a graph's
  nodes take twice the pages. A relationship takes 24 bytes from episode 11, 170 to a page, and an
  embedding a kibibyte, four to a page (episode 14).
- `LoadFacts` checks each name, type, key and string against the limits before it adds anything,
  rather than cut one short ([0004](0004-facts-as-cypher-patterns.md)).
- Until episode 5, writing a record read its whole page and wrote it back, and appending also wrote
  the header, so adding a relationship read or wrote six pages. On the generated million
  relationships, a load read about 3.1 million pages and wrote 3.2 million, each a system call
  (`ep005/step-1`). The buffer pool took them off the disk
  ([0011](0011-a-buffer-pool-of-our-own.md)).
- Records are copied as bytes, so numbers in the files are as the CPU keeps them, lowest byte
  first, which episode 2 read back with `xxd`.
