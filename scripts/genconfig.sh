#!/bin/sh
# Produce .config by merging the chosen values in configs/config over
# the declared options and defaults in Kconfig.
#
# Usage: scripts/genconfig.sh [output-file]   (default: ./.config)

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
kconfig="$root/Kconfig"
chosen="$root/configs/config"
out=${1:-"$root/.config"}

errors=0

awk -v kcfg="$kconfig" -v chosen="$chosen" -v out="$out" '
    function fail(file, msg)
    {
        printf "%s: %s\n", file, msg > "/dev/stderr"
        errors++
    }

    FILENAME == kcfg {
        if (NF == 0 || $1 ~ /^#/)
            next
        if ($1 == "mainmenu")
            next
        if ($1 == "choice") {
            in_choice++
            members = ""
            cdefault = ""
            cdep = ""
            next
        }
        if ($1 == "endchoice") {
            if (!in_choice) {
                fail(FILENAME, "endchoice without choice")
                next
            }
            n_choices++
            choice_members[n_choices] = members
            choice_default[n_choices] = cdefault
            choice_depends[n_choices] = cdep
            in_choice--
            next
        }
        if (in_choice && $1 == "depends" && $2 == "on") {
            cdep = $3
            next
        }
        if ($1 == "prompt")
            next
        if ($1 == "config") {
            name = $2
            if (name !~ /^[A-Za-z0-9_]+$/) {
                fail(FILENAME, "invalid option name \"" name "\"")
                name = ""
                next
            }
            if (!(name in declared)) {
                declared[name] = 1
                order[++n] = name
            }
            value[name] = "n"
            if (in_choice) {
                members = members (members == "" ? "" : " ") name
                if (cdep != "")
                    depends_on[name] = cdep
            }
            next
        }
        if (in_choice && $1 == "default") {
            cdefault = $2
            next
        }
        if (name == "")
            next
        if ($1 == "bool")
            next
        if ($1 == "depends" && $2 == "on") {
            depends_on[name] = $3
            next
        }
        if ($1 == "default") {
            val = $2
            if (val != "y" && val != "n") {
                fail(FILENAME,
                     "unsupported default for " name ": " val)
                next
            }
            value[name] = val
            next
        }
        # help text and anything else is ignored
        next
    }

    FILENAME == chosen {
        line = $0
        sub(/[ \t]*#.*$/, "", line)
        gsub(/^[ \t]+|[ \t]+$/, "", line)
        if (line == "" || line ~ /^#/)
            next
        if (line !~ /^CONFIG_[A-Za-z0-9_]+=(y|n)$/) {
            fail(FILENAME, "not a valid assignment: " line)
            next
        }
        sym = line
        sub(/^CONFIG_/, "", sym)
        sub(/=.*/, "", sym)
        val = line
        sub(/^.*=/, "", val)
        if (!(sym in declared)) {
            fail(FILENAME, "undeclared option CONFIG_" sym)
            next
        }
        value[sym] = val
        next
    }

    END {
        for (ci = 1; ci <= n_choices; ci++) {
            dep = choice_depends[ci]
            m = split(choice_members[ci], member, " ")
            if (dep != "" && value[dep] != "y") {
                for (j = 1; j <= m; j++)
                    value[member[j]] = "n"
                continue
            }
            count = 0
            picked = ""
            for (j = 1; j <= m; j++)
                if (value[member[j]] == "y") {
                    count++
                    picked = member[j]
                }
            if (count == 0) {
                d = choice_default[ci]
                if (d == "" || index(" " choice_members[ci] " ",
                                     " " d " ") == 0) {
                    fail(kcfg, "choice " ci " has no valid default")
                    continue
                }
                value[d] = "y"
                count = 1
            }
            if (count != 1)
                fail(kcfg, "choice must select exactly one of: " \
                           choice_members[ci])
        }

        for (i = 1; i <= n; i++) {
            s = order[i]
            if (s in depends_on) {
                dep = depends_on[s]
                if (value[dep] != "y")
                    value[s] = "n"
            }
        }

        printf "#\n" \
               "# Automatically generated file; DO NOT EDIT.\n" \
               "# RvKernel Manager Configuration\n" \
               "#\n" > out
        for (i = 1; i <= n; i++) {
            s = order[i]
            if (value[s] == "y")
                printf "CONFIG_%s=y\n", s > out
            else
                printf "# CONFIG_%s is not set\n", s > out
        }
        close(out)
        status = errors > 0 ? 1 : 0
        exit status
    }
' "$kconfig" "$chosen"

exit $?
