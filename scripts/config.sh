#!/bin/bash
# Line-by-line interactive configuration — identical to the Linux kernel's
# "make config" (scripts/kconfig/conf --oldaskconfig Kconfig).
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
kconfig="$root/Kconfig"
chosen="$root/configs/config"

# ---- read current values --------------------------------------------------
declare -A CONF
if [ -f "$chosen" ]; then
        while IFS='=' read -r key val; do
                [[ $key == CONFIG_* ]] && CONF[${key#CONFIG_}]=$val
        done < "$chosen"
fi

# ---- parse Kconfig --------------------------------------------------------
declare -a ORDER=()
declare -A TYPE=()
declare -A PROMPT=()
declare -A DEFAULT=()
declare -A DEPENDS=()
declare -A HELP=()

declare -a CHOICE_MEMBERS=()
declare -A CHOICE_TITLE=()
declare -A CHOICE_DEFAULT=()
declare -A CHOICE_DEPENDS=()

cur_sym=""
cur_help=""
in_help=0
in_choice=0
choice_syms=""
choice_title=""
choice_default=""
choice_depends=""

finish_help() {
        if [ -n "$cur_sym" ] && [ -n "$cur_help" ]; then
                HELP[$cur_sym]="$cur_help"
        fi
        cur_help=""
        in_help=0
}

while IFS= read -r line; do
        if [ "$in_help" = "1" ]; then
                if [[ "$line" =~ ^[[:space:]] ]] || [[ -z "$line" ]]; then
                        cur_help+="${line}"$'\n'
                        continue
                else
                        finish_help
                fi
        fi

        trimmed="${line#"${line%%[![:space:]]*}"}"
        [[ -z "$trimmed" || "$trimmed" == \#* ]] && continue

        keyword="${trimmed%% *}"
        rest="${trimmed#* }"
        [ "$keyword" = "$rest" ] && rest=""

        case "$keyword" in
                mainmenu)
                        title=$(echo "$rest" | sed 's/^"//;s/"$//')
                        ;;
                choice)
                        in_choice=1
                        choice_syms=""
                        choice_title=""
                        choice_default=""
                        choice_depends=""
                        ;;
                endchoice)
                        finish_help
                        in_choice=0
                        ci=${#CHOICE_MEMBERS[@]}
                        CHOICE_MEMBERS+=("$choice_syms")
                        CHOICE_TITLE[$ci]="$choice_title"
                        CHOICE_DEFAULT[$ci]="$choice_default"
                        CHOICE_DEPENDS[$ci]="$choice_depends"
                        ;;
                prompt)
                        if [ "$in_choice" = "1" ]; then
                                choice_title=$(echo "$rest" | sed 's/^"//;s/"$//')
                        fi
                        ;;
                config)
                        finish_help
                        cur_sym="$rest"
                        ORDER+=("$cur_sym")
                        DEFAULT[$cur_sym]="n"
                        if [ "$in_choice" = "1" ]; then
                                [ -n "$choice_syms" ] && choice_syms+=" "
                                choice_syms+="$cur_sym"
                                [ -n "$choice_depends" ] && DEPENDS[$cur_sym]="$choice_depends"
                        fi
                        ;;
                bool)
                        TYPE[$cur_sym]="bool"
                        PROMPT[$cur_sym]=$(echo "$rest" | sed 's/^"//;s/"$//')
                        ;;
                default)
                        if [ "$in_choice" = "1" ] && [ "$rest" != "y" ] && [ "$rest" != "n" ]; then
                                choice_default="$rest"
                        else
                                DEFAULT[$cur_sym]="$rest"
                        fi
                        ;;
                depends)
                        dep="${rest#on }"
                        if [ "$in_choice" = "1" ]; then
                                choice_depends="$dep"
                        else
                                DEPENDS[$cur_sym]="$dep"
                        fi
                        ;;
                help)
                        in_help=1
                        cur_help=""
                        ;;
        esac
done < "$kconfig"
finish_help

# ---- apply defaults for options not yet in configs/config -----------------
for sym in "${ORDER[@]}"; do
        [ -z "${CONF[$sym]+x}" ] && CONF[$sym]="${DEFAULT[$sym]}"
done
for ci in "${!CHOICE_MEMBERS[@]}"; do
        read -ra members <<< "${CHOICE_MEMBERS[$ci]}"
        has_y=0
        for m in "${members[@]}"; do
                [ "${CONF[$m]}" = "y" ] && has_y=1
        done
        if [ "$has_y" = "0" ] && [ -n "${CHOICE_DEFAULT[$ci]}" ]; then
                CONF[${CHOICE_DEFAULT[$ci]}]="y"
        fi
done

# ---- prompt ---------------------------------------------------------------
echo ""
echo "*"
echo "* ${title:-RvKernel Manager Configuration}"
echo "*"

declare -A choice_done=()

for sym in "${ORDER[@]}"; do
        # Is this symbol part of a choice?
        in_ci=""
        for ci in "${!CHOICE_MEMBERS[@]}"; do
                if [[ " ${CHOICE_MEMBERS[$ci]} " == *" $sym "* ]]; then
                        in_ci="$ci"
                        break
                fi
        done

        if [ -n "$in_ci" ] && [ -z "${choice_done[$in_ci]+x}" ]; then
                choice_done[$in_ci]=1
                read -ra members <<< "${CHOICE_MEMBERS[$in_ci]}"

                if [ -n "${CHOICE_DEPENDS[$in_ci]}" ]; then
                        cdep="${CHOICE_DEPENDS[$in_ci]}"
                        if [ "${CONF[$cdep]}" != "y" ]; then
                                for m in "${members[@]}"; do CONF[$m]="n"; done
                                continue
                        fi
                fi

                # current selection index (1-based)
                cur_sel=1
                for i in "${!members[@]}"; do
                        [ "${CONF[${members[$i]}]}" = "y" ] && cur_sel=$((i + 1))
                done

                echo "${CHOICE_TITLE[$in_ci]}"
                for i in "${!members[@]}"; do
                        echo "  $((i + 1)). ${PROMPT[${members[$i]}]} (${members[$i]})"
                done

                while true; do
                        read -rp "choice[1-${#members[@]}?]: " ans
                        ans=${ans:-$cur_sel}
                        if [ "$ans" = "?" ]; then
                                for m in "${members[@]}"; do
                                        [ -n "${HELP[$m]}" ] && printf "\n%s:\n%s" "${PROMPT[$m]}" "${HELP[$m]}"
                                done
                                continue
                        fi
                        if [[ "$ans" =~ ^[0-9]+$ ]] && [ "$ans" -ge 1 ] && [ "$ans" -le "${#members[@]}" ]; then
                                for m in "${members[@]}"; do CONF[$m]="n"; done
                                CONF[${members[$((ans - 1))]}]="y"
                                break
                        fi
                done
        elif [ -n "$in_ci" ]; then
                continue
        else
                if [ -n "${DEPENDS[$sym]}" ]; then
                        dep="${DEPENDS[$sym]}"
                        if [ "${CONF[$dep]}" != "y" ]; then
                                CONF[$sym]="n"
                                continue
                        fi
                fi

                cur="${CONF[$sym]}"
                if [ "$cur" = "y" ]; then ds="Y/n/?"; else ds="y/N/?"; fi

                while true; do
                        read -rp "${PROMPT[$sym]} ($sym) [$ds] " ans
                        ans=${ans:-$cur}
                        case "$ans" in
                                y|Y) CONF[$sym]="y"; break ;;
                                n|N) CONF[$sym]="n"; break ;;
                                "?") [ -n "${HELP[$sym]}" ] && printf "\n%s\n" "${HELP[$sym]}" || echo "  No help available." ;;
                                *) ;;
                        esac
                done
        fi
done

# ---- save -----------------------------------------------------------------
{
        echo "#"
        echo "# RvKernel Manager Configuration"
        echo "#"
        for sym in "${ORDER[@]}"; do
                echo "CONFIG_${sym}=${CONF[$sym]}"
        done
} > "$chosen"

sh "$root/scripts/genconfig.sh"

echo ""
echo "#"
echo "# configuration written to .config"
echo "#"
