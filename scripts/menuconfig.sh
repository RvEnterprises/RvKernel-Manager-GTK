#!/bin/bash
# Interactive TUI configuration generator — identical to Linux kernel's menuconfig.
# 100% dynamically parsed and driven from Kconfig.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
kconfig="$root/Kconfig"
chosen="$root/configs/config"

# The kernel menuconfig depends on dialog; this script does the same.
if ! command -v dialog &>/dev/null; then
        echo "*** Unable to find the 'dialog' program, which is"
        echo "*** required to run menuconfig."
        echo "***"
        echo "*** Debian / Ubuntu:  sudo apt install dialog"
        echo "*** Fedora / RHEL:    sudo dnf install dialog"
        echo "*** Arch:             sudo pacman -S dialog"
        exit 1
fi

# ---- read current values --------------------------------------------------
declare -A CONF
if [ -f "$chosen" ]; then
        while IFS='=' read -r key val; do
                [[ $key == CONFIG_* ]] && CONF[${key#CONFIG_}]=$val
        done < "$chosen"
fi

# ---- parse Kconfig --------------------------------------------------------
declare -a ITEMS=()        # "choice:<ci>" or "config:<sym>" in visual order
declare -a ALL_SYMS=()     # every symbol declared

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
title=""

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
                        ITEMS+=("choice:$ci")
                        ;;
                prompt)
                        if [ "$in_choice" = "1" ]; then
                                choice_title=$(echo "$rest" | sed 's/^"//;s/"$//')
                        fi
                        ;;
                config)
                        finish_help
                        cur_sym="$rest"
                        ALL_SYMS+=("$cur_sym")
                        DEFAULT[$cur_sym]="n"
                        if [ "$in_choice" = "1" ]; then
                                [ -n "$choice_syms" ] && choice_syms+=" "
                                choice_syms+="$cur_sym"
                                [ -n "$choice_depends" ] && DEPENDS[$cur_sym]="$choice_depends"
                        else
                                ITEMS+=("config:$cur_sym")
                        fi
                        ;;
                bool)
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

# ---- initialize unset config values ---------------------------------------
for sym in "${ALL_SYMS[@]}"; do
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

# fd 3 = real terminal (dialog draws on stdout, result on stderr)
exec 3>&1
dlg() { dialog "$@" 2>&1 1>&3; }

eval_dep() {
        local dep="$1"
        if [ -z "$dep" ] || [ "${CONF[$dep]}" = "y" ]; then
                return 0
        fi
        return 1
}

enforce_dependencies() {
        # Evaluate choice dependencies
        for ci in "${!CHOICE_MEMBERS[@]}"; do
                read -ra members <<< "${CHOICE_MEMBERS[$ci]}"
                if ! eval_dep "${CHOICE_DEPENDS[$ci]}"; then
                        for m in "${members[@]}"; do CONF[$m]="n"; done
                else
                        has_y=0
                        for m in "${members[@]}"; do
                                [ "${CONF[$m]}" = "y" ] && has_y=$((has_y + 1))
                        done
                        if [ "$has_y" -ne 1 ]; then
                                for m in "${members[@]}"; do CONF[$m]="n"; done
                                def="${CHOICE_DEFAULT[$ci]}"
                                [ -n "$def" ] && CONF[$def]="y" || CONF[${members[0]}]="y"
                        fi
                fi
        done

        # Evaluate standalone config dependencies
        for sym in "${ALL_SYMS[@]}"; do
                if [ -n "${DEPENDS[$sym]}" ]; then
                        if ! eval_dep "${DEPENDS[$sym]}"; then
                                CONF[$sym]="n"
                        fi
                fi
        done
}

save_config() {
        {
                echo "#"
                echo "# RvKernel Manager Configuration"
                echo "#"
                for sym in "${ALL_SYMS[@]}"; do
                        echo "CONFIG_${sym}=${CONF[$sym]:-n}"
                done
        } > "$chosen"
        sh "$root/scripts/genconfig.sh"
}

# ---- main menu loop -------------------------------------------------------
while true; do
        enforce_dependencies

        menu_items=()
        for item in "${ITEMS[@]}"; do
                if [[ "$item" == choice:* ]]; then
                        ci="${item#choice:}"
                        if eval_dep "${CHOICE_DEPENDS[$ci]}"; then
                                read -ra members <<< "${CHOICE_MEMBERS[$ci]}"
                                active_desc=""
                                for m in "${members[@]}"; do
                                        if [ "${CONF[$m]}" = "y" ]; then
                                                active_desc="${PROMPT[$m]}"
                                                break
                                        fi
                                done
                                [ -z "$active_desc" ] && active_desc="None"
                                menu_items+=("CHOICE_$ci" "    ${CHOICE_TITLE[$ci]} ($active_desc)  --->")
                        fi
                elif [[ "$item" == config:* ]]; then
                        sym="${item#config:}"
                        if eval_dep "${DEPENDS[$sym]}"; then
                                mark="[ ]"
                                [ "${CONF[$sym]}" = "y" ] && mark="[*]"
                                menu_items+=("CONFIG_$sym" "    $mark ${PROMPT[$sym]}")
                        fi
                fi
        done

        num_items=$((${#menu_items[@]} / 2))

        CHOICE=$(dlg --clear --no-tags \
                --title "${title:-RvKernel Manager Configuration}" \
                --ok-label "Select" \
                --cancel-label "Exit" \
                --menu \
"Arrow keys navigate the menu.  <Enter> selects submenus --->.
Press <Esc><Esc> to exit.  Legend: [*] built-in  [ ] excluded" \
                18 70 "$num_items" \
                "${menu_items[@]}") \
        || {
                # Exit or Esc — prompt to save, exactly like kernel menuconfig
                if dlg --yesno \
                        "Do you wish to save your new configuration?" 6 60
                then
                        save_config
                        clear
                        echo ""
                        echo "#"
                        echo "# configuration written to .config"
                        echo "#"
                        echo ""
                else
                        clear
                        echo ""
                        echo "Your configuration changes were NOT saved."
                        echo ""
                fi
                break
        }

        if [[ "$CHOICE" == CHOICE_* ]]; then
                ci="${CHOICE#CHOICE_}"
                read -ra members <<< "${CHOICE_MEMBERS[$ci]}"

                choice_items=()
                def_item=""
                for m in "${members[@]}"; do
                        mark="( )"
                        if [ "${CONF[$m]}" = "y" ]; then
                                mark="(X)"
                                def_item="$m"
                        fi
                        choice_items+=("$m" "$mark ${PROMPT[$m]}")
                done
                [ -z "$def_item" ] && def_item="${members[0]}"

                SEL=$(dlg --clear --no-tags \
                        --title "${CHOICE_TITLE[$ci]}" \
                        --ok-label "Select" \
                        --cancel-label "Exit" \
                        --default-item "$def_item" \
                        --menu \
"Use the arrow keys to navigate this window or
press the hotkey of the item you wish to select
followed by the <ENTER> key." \
                        15 50 "${#members[@]}" \
                        "${choice_items[@]}") || true

                if [ -n "$SEL" ]; then
                        for m in "${members[@]}"; do CONF[$m]="n"; done
                        CONF[$SEL]="y"
                fi
        elif [[ "$CHOICE" == CONFIG_* ]]; then
                sym="${CHOICE#CONFIG_}"
                if [ "${CONF[$sym]}" = "y" ]; then
                        CONF[$sym]="n"
                else
                        CONF[$sym]="y"
                fi
        fi
done

exec 3>&-
