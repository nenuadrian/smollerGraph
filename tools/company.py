"""Writes a made-up company's Active Directory as facts for sg load: its
people and their groups, their computers, and which of them can take over
which, the way BloodHound draws it, each relationship pointing the way an
attacker would go. The company and its people are made up, from two lists of
names and a seeded random generator, so each run writes the same company.

From the repository's root:

    python3 tools/company.py data/company.txt
"""

import random
import sys

SEED = 42
# The teams and how many people are in each, then the three that run IT.
TEAMS = {
    "Sales": 80, "Engineering": 80, "Marketing": 40, "Finance": 30, "HR": 20,
}
IT = {"Helpdesk": 12, "IT Admins": 8, "Domain Admins": 3}
# The domain controller first, which holds the directory, then the servers
# the IT admins look after.
SERVERS = ["DC01", "FS01", "SQL01", "BACKUP01"]
FIRST = [
    "Amelia", "Ben", "Chloe", "Daniel", "Elena", "Farah", "George", "Hana",
    "Isaac", "Jade", "Kofi", "Laura", "Mateo", "Nadia", "Oscar", "Priya",
    "Quentin", "Rosa", "Samir", "Tara", "Umar", "Vera", "Wei", "Yusuf",
    "Zoe", "Aaron", "Bianca", "Carlos", "Dina", "Ethan",
]
LAST = [
    "Adams", "Bose", "Clarke", "Diaz", "Evans", "Fischer", "Garcia", "Hughes",
    "Ito", "Jensen", "Khan", "Lopez", "Morris", "Novak", "Okafor", "Patel",
    "Quinn", "Reyes", "Silva", "Taylor", "Usman", "Varga", "Walsh", "Young",
]


def company(rng):
    """Returns the company's nodes, each a name and a label, and its
    relationships, each a start, a type and an end."""
    groups = ["Domain Users", *TEAMS, *IT]
    nodes = [(group, "Group") for group in groups]
    nodes += [(server, "Computer") for server in SERVERS]
    relationships = []
    # Each person has a made-up name, a workstation of their own, which they
    # log on to, and their team, inside Domain Users, which is everyone.
    sizes = {**TEAMS, **IT}
    everyone = [f"{first} {last}" for first in FIRST for last in LAST]
    names = rng.sample(everyone, sum(sizes.values()))
    members = {team: [] for team in sizes}
    workstations = {}
    for team, size in sizes.items():
        for _ in range(size):
            name = names.pop()
            workstation = f"WS{len(workstations) + 1:03}"
            members[team].append(name)
            workstations[name] = workstation
            nodes += [(name, "User"), (workstation, "Computer")]
            relationships += [
                (name, "MemberOf", "Domain Users"),
                (name, "MemberOf", team),
                (workstation, "HasSession", name),
            ]

    # Engineers are admins on their own workstations, to install their tools.
    for name in members["Engineering"]:
        relationships.append((name, "AdminTo", workstations[name]))
    # The helpdesk is an admin on each workstation, resets the teams'
    # passwords, and logs on to a few workstations to fix things.
    for workstation in workstations.values():
        relationships.append(("Helpdesk", "AdminTo", workstation))
    for team in TEAMS:
        for name in members[team]:
            relationships.append(("Helpdesk", "ForceChangePassword", name))
    for name in members["Helpdesk"]:
        for workstation in rng.sample(sorted(workstations.values()), 4):
            relationships.append((workstation, "HasSession", name))
    # IT admins look after the servers, and each logs on to two of them.
    for server in SERVERS[1:]:
        relationships.append(("IT Admins", "AdminTo", server))
    for name in members["IT Admins"]:
        for server in rng.sample(SERVERS[1:], 2):
            relationships.append((server, "HasSession", name))
    # Domain admins look after the domain controller.
    relationships.append(("Domain Admins", "AdminTo", "DC01"))
    for name in members["Domain Admins"]:
        relationships.append(("DC01", "HasSession", name))

    # Two service accounts. The backup's runs on each server, and it's in
    # Domain Admins, as it copies the domain controller too. The database's
    # runs SQL01, and Engineering has full control of it, to restart it.
    nodes += [("svc_backup", "User"), ("svc_sql", "User")]
    for server in SERVERS:
        relationships.append(("svc_backup", "AdminTo", server))
        relationships.append((server, "HasSession", "svc_backup"))
    relationships += [
        ("svc_backup", "MemberOf", "Domain Admins"),
        ("svc_sql", "AdminTo", "SQL01"),
        ("SQL01", "HasSession", "svc_sql"),
        ("Engineering", "GenericAll", "svc_sql"),
    ]
    return nodes, relationships


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python3 tools/company.py <facts file>")
    nodes, relationships = company(random.Random(SEED))
    with open(sys.argv[1], "w", encoding="utf-8") as out:
        out.write(
            "# A made-up company's Active Directory, written by\n"
            "# tools/company.py: its people, groups and computers, and which\n"
            "# of them can take over which, each relationship pointing the\n"
            "# way an attacker would go.\n"
        )
        for name, label in nodes:
            out.write(f"({name}:{label})\n")
        for start, type_, end in relationships:
            out.write(f"({start})-[:{type_}]->({end})\n")


if __name__ == "__main__":
    main()
