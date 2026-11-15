# 22\. API docs from the headers' comments, with Doxygen

## Status

Accepted

## Context

After fifteen episodes, "to find out what a function does, you still have to open its header"
(episode 16). The headers have a comment over each class and function, saying what it does. They
start with two slashes, as Google's style guide writes them, and Doxygen reads comments written its
own ways, such as with three slashes, or a slash and two stars, and leaves plain comments out.

## Decision

- When Doxygen is installed, CMake's `docs` target writes the API docs to `build/docs/html`, with
  the README as the front page, in doxygen-awesome 2.5.0's theme. Without Doxygen, the rest is
  skipped, so building the database doesn't need it.
- `DOXYGEN_FILTER_PATTERNS` passes each header through `sed`, which gives each comment that starts a
  line its third slash, "so the headers stay as they are".
- `EXTRACT_ALL` puts each class and its public functions and fields in, with a comment or without,
  and `JAVADOC_AUTOBRIEF` takes the first sentence of a comment as its brief. A file's page draws the
  files that include it, but not what it includes, "which is most of the standard library".
- The Dockerfile's `docs` stage builds them on Ubuntu, with its Doxygen and Graphviz, and holds the
  pages and nothing else, which `docker build --target docs --output site .` copies out.
  Configuring is enough, as the target reads the sources and compiles nothing.
- In the public repository, CI builds the docs stage and hands the pages to GitHub Pages, and a
  second job deploys them to nenuadrian.github.io/smollerGraph on each push to main, once the tests
  pass. A private copy skips both, to save Actions minutes.

## Alternatives considered

- Adding a slash to each of the three hundred or so lines of comments in the headers (episode 16).

## Consequences

- No comment changed, and the comments written since episode 1 became the docs.
- On the Mac, the docs took under a second to build: about 250 files and 3.5 MB (episode 16).
  Ubuntu's Doxygen, in Docker, is three years older than the Mac's.
- The docs online match the last code in the public repository that passed, and "a step that fails
  stops the docs from changing".
- They're the docs of the latest code, where a library with releases keeps the docs of each, as
  each episode's tag would be here ([0005](0005-a-tag-for-each-step.md)).
- They list what's in the code. A guide to how the parts fit, such as how a query becomes a plan,
  would be pages written for the purpose (episode 16).
- Doxygen warns about what it can't make sense of, and CI doesn't treat a warning as a failure, so
  the docs can break without anyone noticing (episode 16).
