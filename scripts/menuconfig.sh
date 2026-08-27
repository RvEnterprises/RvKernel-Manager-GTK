#!/bin/bash
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
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

# ---- parse current config ------------------------------------------------
declare -A CONF
while IFS='=' read -r key val; do
        [[ $key == CONFIG_* ]] && CONF[${key#CONFIG_}]=$val
done < "$chosen"

# fd 3 = real terminal (dialog draws on stdout, result on stderr)
exec 3>&1
dlg() { dialog "$@" 2>&1 1>&3; }

bool_mark() { [ "${CONF[$1]}" = "y" ] && echo "[*]" || echo "[ ]"; }

# ---- save helper ----------------------------------------------------------
save_config() {
        cat <<OUT > "$chosen"
#
# RvKernel Manager Configuration
#

CONFIG_CC_GCC=${CONF[CC_GCC]:-y}
CONFIG_CC_CLANG=${CONF[CC_CLANG]:-n}
CONFIG_LTO=${CONF[LTO]:-n}
CONFIG_DEBUG=${CONF[DEBUG]:-n}
CONFIG_CCACHE=${CONF[CCACHE]:-y}
OUT
        sh "$root/scripts/genconfig.sh"
}

# ---- main loop -----------------------------------------------------------
while true; do
        if [ "${CONF[CC_CLANG]}" = "y" ]; then
                cc_desc="Build with Clang"
        else
                cc_desc="Build with GCC"
        fi

        items=(
                "CC"     "    C compiler ($cc_desc)  --->"
        )
        if [ "${CONF[CC_CLANG]}" = "y" ]; then
                items+=("LTO"    "    $(bool_mark LTO) Link-Time Optimization (LTO)")
        fi
        items+=(
                "DEBUG"  "    $(bool_mark DEBUG) Diagnostic logging"
                "CCACHE" "    $(bool_mark CCACHE) Use ccache"
        )
        num_items=$((${#items[@]} / 2))

        CHOICE=$(dlg --clear --no-tags \
                --title "RvKernel Manager Configuration" \
                --ok-label "Select" \
                --cancel-label "Exit" \
                --menu \
"Arrow keys navigate the menu.  <Enter> selects submenus --->.
Press <Esc><Esc> to exit.  Legend: [*] built-in  [ ] excluded" \
                18 70 "$num_items" \
                "${items[@]}") \
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

        case "$CHOICE" in
                CC)
                        gcc_mark="( )"; clang_mark="( )"
                        if [ "${CONF[CC_CLANG]}" = "y" ]; then
                                clang_mark="(X)"; def=CC_CLANG
                        else
                                gcc_mark="(X)"; def=CC_GCC
                        fi

                        SEL=$(dlg --clear --no-tags \
                                --title "C compiler" \
                                --ok-label "Select" \
                                --cancel-label "Exit" \
                                --default-item "$def" \
                                --menu \
"Use the arrow keys to navigate this window or
press the hotkey of the item you wish to select
followed by the <ENTER> key." \
                                15 50 2 \
                                "CC_GCC"   "$gcc_mark Build with GCC" \
                                "CC_CLANG" "$clang_mark Build with Clang") || true

                        if [ -n "$SEL" ]; then
                                if [ "$SEL" = "CC_GCC" ]; then
                                        CONF[CC_GCC]="y"; CONF[CC_CLANG]="n"
                                        CONF[LTO]="n"
                                else
                                        CONF[CC_GCC]="n"; CONF[CC_CLANG]="y"
                                fi
                        fi
                        ;;
                LTO)
                        [ "${CONF[LTO]}" = "y" ] && CONF[LTO]="n" || CONF[LTO]="y"
                        ;;
                DEBUG)
                        [ "${CONF[DEBUG]}" = "y" ] && CONF[DEBUG]="n" || CONF[DEBUG]="y"
                        ;;
                CCACHE)
                        [ "${CONF[CCACHE]}" = "y" ] && CONF[CCACHE]="n" || CONF[CCACHE]="y"
                        ;;
        esac
done

exec 3>&-
