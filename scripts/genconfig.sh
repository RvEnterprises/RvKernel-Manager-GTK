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
    function fail(file, msg, line)
    {
        printf "%s: %s\n", file, msg > "/dev/stderr"
        errors++
    }

    FILENAME == kcfg {
        if (NF == 0 || $1 ~ /^#/)
            next
        if ($1 == "mainmenu")
            next
        if ($1 == "config") {
            name = $2
            if (name !~ /^[A-Za-z0-9_]+$/) {
                fail(FILENAME, "invalid option name \"" name "\"", NR)
                name = ""
                next
            }
            if (!(name in declared)) {
                declared[name] = 1
                order[++n] = name
            }
            value[name] = "n"
            next
        }
        if (name == "")
            next
        if ($1 == "bool") {
            next
        }
        if ($1 == "default") {
            val = $2
            if (val != "y" && val != "n") {
                fail(FILENAME,
                     "unsupported default for " name ": " val, NR)
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
            fail(FILENAME, "not a valid assignment: " line, NR)
            next
        }
        sym = line
        sub(/^CONFIG_/, "", sym)
        sub(/=.*/, "", sym)
        val = line
        sub(/^.*=/, "", val)
        if (!(sym in declared)) {
            fail(FILENAME, "undeclared option CONFIG_" sym, NR)
            next
        }
        value[sym] = val
        next
    }

    END {
        printf "#\n" \
               "# Automatically generated; edit configs/config or\n" \
               "# run \"make config\" instead of editing this file.\n" \
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
