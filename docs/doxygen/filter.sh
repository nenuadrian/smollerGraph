#!/bin/sh
# Doxygen passes each Markdown file through this before it reads it, for the
# API docs only: GitHub shows the files as they are. The first argument is the
# repository's address on GitHub, and Doxygen adds the file's path after it.
#
# - The decisions' index becomes a page with a page under it for each of its
#   groups, and each record a page under its group (decisions.awk).
# - Each record's heading gets a label, decision-N, which its group's page
#   names, and loses the backslash records 1 to 24 have before their number's
#   dot, which Doxygen 1.9 would show.
# - The README's cover, an image in .github inside a link, becomes the same
#   link in HTML, around the copy the docs keep beside their pages, so that it
#   shows, and without a caption that repeats what it says.
repo=$1
file=$2
case $file in
  */docs/decisions/README.md)
    awk -v repo="$repo" -f "$(dirname "$0")/decisions.awk" "$file" "$file"
    ;;
  */docs/decisions/[0-9][0-9][0-9][0-9]-*.md)
    sed -E '1s/^# ([0-9]+)\\?\.(.*)$/# \1.\2 {#decision-\1}/' "$file"
    ;;
  *)
    sed -E 's#^\[!\[([^]]*)\]\(\.github/(cover\.jpg)\)\]\(([^)]*)\)$#<a href="\3"><img src="\2" alt="\1"></a>#' "$file"
    ;;
esac
