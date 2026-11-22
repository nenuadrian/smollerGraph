# Turns docs/decisions/README.md into pages for the API docs. Doxygen nests a
# page under another in its menu when the other names it with @subpage, so:
#
# - each group of the index, a "### " heading, becomes a page of its own,
#   which the index names, and which holds its group's table;
# - each record's title in a table names the record's page, decision-N, which
#   puts it under its group;
# - the page's other headings become HTML ones, which show the same but stay
#   out of the menu, so that the menu under Decisions is its groups alone;
# - the link to the template, which isn't in the docs, goes to it on GitHub.
#
# The groups are the end of the file, so each runs to the next. awk reads the
# file twice, the first time for the groups' names, which the index lists
# before the first of them.

function label(name,    l) {
  l = tolower(name)
  gsub(/[^a-z0-9]+/, "-", l)
  sub(/-$/, "", l)
  return "decisions-" l
}

FNR == NR {
  if (/^### /) groups[++count] = substr($0, 5)
  next
}

/^## / {
  print "<h2>" substr($0, 4) "</h2>"
  next
}

/^### / {
  if (!listed) {
    for (i = 1; i <= count; i++) print "- @subpage " label(groups[i])
    print ""
    listed = 1
  }
  print "@page " label(substr($0, 5)) " " substr($0, 5)
  next
}

# A record's row: | [0007](0007-a-title.md) | Its title | Status | Episodes |
/^\| \[[0-9]+\]/ {
  cells = split($0, cell, "|")
  number = cell[2]
  sub(/^ *\[0*/, "", number)
  sub(/\].*$/, "", number)
  title = cell[3]
  gsub(/^ +| +$/, "", title)
  gsub(/"/, "\\&quot;", title)
  cell[3] = " @subpage decision-" number " \"" title "\" "
  row = "|"
  for (i = 2; i < cells; i++) row = row cell[i] "|"
  print row
  next
}

{
  gsub(/\(template\.md\)/, "(" repo "/blob/main/docs/decisions/template.md)")
  print
}
