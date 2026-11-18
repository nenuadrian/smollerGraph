# Decisions

This folder keeps smollerGraph's decision records: a page for each choice that shaped the database,
with what forced it, what we picked, what else we weighed, and what it cost. They follow the format
of the architecture decision records Michael Nygard described in 2011, in "Documenting Architecture
Decisions", kept short, with a section his doesn't have, for the alternatives considered.

Records 1 to 22 were written in episode 17, from what episodes 1 to 16 said and what their commits
hold. Where an episode gave no reason for a choice, the record says so, rather than make one up
after the fact.

## Why keep them

The code says what the database does, and a commit says what changed. Neither says why it's this
way and not another, and that's what gets lost first.

- The why survives the people who knew it. Why pages of 4 KiB, and why records that point at each
  other rather than Kùzu's columns, are in [0007](0007-fixed-size-records-in-pages-of-4-kib.md) and
  [0003](0003-relationships-beside-their-nodes.md), with what was measured at the time and where.
- Newcomers learn the shape of the code. Read top to bottom, the records are the database's
  architecture in the order it was built, from a graph in memory to a model trained on it, each with
  the problem it answered.
- Settled questions stay settled. "Why not let the operating system cache the pages, with mmap?"
  has an answer in [0011](0011-a-buffer-pool-of-our-own.md), and asking again means saying what's
  changed since.
- Reversals are deliberate. Changing a decision takes a new record that says what's different now,
  as [0016](0016-draw-what-a-query-returns-on-a-canvas.md) did for
  [0010](0010-draw-the-whole-graph-with-svg.md) when the page met 21,834 taxa, and
  [0018](0018-vanilla-versions-not-races.md) did for [0008](0008-measure-against-neo4j-and-kuzu.md)
  when the series stopped racing Neo4j and Kùzu.

## When to write one

Write a record for a choice that would be costly to reverse, such as how the graph is laid out on
disk, the query language, or how other programs reach the database, or one that people will ask
about. A change that one commit could undo, or that the code makes plain, needs none.

## How

- One file for each decision, named with the next number and a short title, such as
  `0024-a-short-title.md`, from [template.md](template.md), and headed with its number and title,
  such as `# 24\. A short title`.
- The API docs publish the records, a page each, after this one. The backslash in a heading keeps
  Doxygen from reading its number as a list, and a link to this page from a record is
  `../decisions/README.md`, as Doxygen takes a bare `README.md` to be the repository's.
- Its sections are the status, the context, with the episode it came up in, the decision, the
  alternatives considered, and the consequences. An alternative goes in only if it was weighed, or
  if it's the obvious one, and then it says it wasn't weighed. Name the files, the settings and the
  numbers, with where each number was measured: the episode, the data, and the machine when it
  matters.
- A record's status is `Proposed` while it's discussed, `Accepted` once it's merged, and
  `Superseded by` a later record when one replaces it.
- An accepted record is never rewritten, like a published step
  ([0005](0005-a-tag-for-each-step.md)). To change a decision, write a new record, and set the old
  one's status to `Superseded by` the new one. That line is the only edit an accepted record gets.
- Add it to the index below.

## Working with others

On a team, a record is where a choice gets argued before it's made, instead of after.

- Propose a decision in a pull request, with a new record whose status is `Proposed`: on its own,
  before the code is written, or in the pull request that makes the change.
- The discussion happens in the record's review: the alternatives someone else would pick, and the
  numbers they'd want measured first. The record changes as the review goes, so it ends up saying
  what was weighed and why it lost.
- It becomes `Accepted` when it's merged, with its status changed in the pull request's last commit.
- Someone new reads the records before asking why the code is the way it is. A question they don't
  answer is a record that's missing.
- Disagreeing with an accepted decision means proposing a record that supersedes it, and saying
  what's changed since: a new need, a new number, or a cost that showed up. If it's accepted, the
  old record's status becomes `Superseded by` the new one in the same pull request.

## Index

| Number | Title | Status | Episode |
|---|---|---|---|
| [0001](0001-cpp-in-googles-style.md) | C++20, in Google's style, built with CMake and Ninja | Accepted | 1, 4 |
| [0002](0002-an-embedded-library-and-sg.md) | An embedded library, with sg as its program | Accepted | 1, 4, 15 |
| [0003](0003-relationships-beside-their-nodes.md) | Keep each node's relationships beside it, in records that point at each other | Accepted | 1, 2 |
| [0004](0004-facts-as-cypher-patterns.md) | Load a graph from facts written as Cypher patterns, one a line | Accepted | 1, 3, 9, 10, 15 |
| [0005](0005-a-tag-for-each-step.md) | A tag for each step of each episode | Accepted | 1, 2, 5, 16 |
| [0006](0006-build-test-and-check-in-docker.md) | Build, test and check in Docker, on each push | Accepted | 2, 4, 16 |
| [0007](0007-fixed-size-records-in-pages-of-4-kib.md) | Fixed-size records, in pages of 4 KiB | Accepted | 2, 9, 10, 11 |
| [0008](0008-measure-against-neo4j-and-kuzu.md) | Measure each change against Neo4j and Kùzu | Superseded by [0018](0018-vanilla-versions-not-races.md) | 3, 5, 11 |
| [0009](0009-errors-as-absl-status.md) | Errors as absl::Status, without exceptions | Accepted | 1, 4 |
| [0010](0010-draw-the-whole-graph-with-svg.md) | Draw the whole graph in the browser, with SVG | Superseded by [0016](0016-draw-what-a-query-returns-on-a-canvas.md) | 4 |
| [0011](0011-a-buffer-pool-of-our-own.md) | Keep pages in a buffer pool of our own, with the clock | Accepted | 5 |
| [0012](0012-a-b-plus-tree-for-names.md) | Find a node by its name with a B+ tree | Accepted | 6 |
| [0013](0013-a-subset-of-cypher.md) | Our own subset of Cypher, planned with Neo4j's operators | Accepted | 7, 9, 10, 11 |
| [0014](0014-deploy-to-one-server-behind-caddy.md) | Deploy to one server with deploy.sh, behind Caddy | Accepted | 8 |
| [0015](0015-properties-in-chains-of-records.md) | Properties in chains of records of their own | Accepted | 9, 13 |
| [0016](0016-draw-what-a-query-returns-on-a-canvas.md) | Draw what a query returns, on a canvas | Accepted | 10 |
| [0017](0017-tokens-and-two-chains.md) | Labels and types as tokens, and each relationship in two chains | Accepted | 11 |
| [0018](0018-vanilla-versions-not-races.md) | Build vanilla versions of what Neo4j and Kùzu offer, and stop racing them | Accepted | 12, 13, 14, 15 |
| [0019](0019-paths-from-both-ends.md) | Search for paths from both ends, and write them as Neo4j and Kùzu do | Accepted | 12, 13 |
| [0020](0020-embeddings-with-fastrp.md) | Embeddings with FastRP, kept in the graph's folder | Accepted | 14 |
| [0021](0021-machine-learning-outside-the-database.md) | Train models outside the database, through sg export and sg load | Accepted | 15 |
| [0022](0022-api-docs-from-the-comments.md) | API docs from the headers' comments, with Doxygen | Accepted | 16 |
| [0023](0023-the-explorer-in-a-browser.md) | Run the explorer in a browser, on a graph it builds when it opens | Accepted | 18 |
