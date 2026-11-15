# 5\. A tag for each step of each episode

## Status

Accepted

## Context

smollerGraph is built on camera, a step at a time, and from episode 1 its code was going to be
"open source on GitHub, tagged at each step of each episode, so that you can follow along". So the
history isn't a by-product of the work: it's what a viewer checks out, and a choice about it is a
choice about what they find at a tag.

## Decision

- Each step is one commit, whose message says what changed and why and ends with its step, such as
  `Step: ep016/step-3`, and an annotated tag of the same name. `epNNN/end` marks an episode's last
  step. The README opens by saying so.
- The functions the rest of the code calls stay when what's behind them changes, so the tests from
  earlier episodes keep checking the answers. Episode 1 kept `AddNode`, `AddRelationship` and
  `Neighbours` for that, and when episode 2 moved the graph onto disk behind them, its four tests
  passed unchanged, "not by a line".
- An episode's code arrives in the public repository once the episode is out (episode 16).

## Alternatives considered

- A history kept as most projects keep one, with releases and no tags for steps. This is the
  obvious alternative, and it wasn't weighed.

## Consequences

- Any step can be checked out and built, and `git log --reverse ep016/end` reads as the series,
  with the reasons for each step in its commit's message.
- At episode 3's tags, a build on the host without a build type has no optimizations, and episode
  3's benchmark ran that build. The fix is `ep005/step-1`, which episode 5 began with as "a
  correction to episode three": Release became the default, and the before was measured again
  ([0008](0008-measure-against-neo4j-and-kuzu.md)).
- A step can show a bug before the step that fixes it. Episode 5 ran the test from `ep005/step-4`
  on `ep005/step-3`, where four threads fetching pages without the pool's latch crash each time
  ([0011](0011-a-buffer-pool-of-our-own.md)).
- The API docs are the docs of the latest code in the public repository, where a library with
  releases would keep the docs of each, as each episode's tag would be here (episode 16).
- These records follow the same rule as a published step: once accepted, a record isn't rewritten,
  and a later one supersedes it ([README](../decisions/README.md)).
