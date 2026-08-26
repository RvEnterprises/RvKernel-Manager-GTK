#!/bin/bash
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
kconfig="$root/Kconfig"
chosen="$root/configs/config"

# Parse current config
declare -A CONF
while IFS='=' read -r key val; do
    if [[ $key == CONFIG_* ]]; then
        sym=${key#CONFIG_}
        CONF[$sym]=$val
    fi
done < "$chosen"

while true; do
    cc_state="GCC"
    if [ "${CONF[CC_CLANG]}" = "y" ]; then
        cc_state="Clang"
    fi

    debug_state="OFF"
    if [ "${CONF[DEBUG]}" = "y" ]; then
        debug_state="ON"
    fi

    CHOICE=$(whiptail --title "RvKernel Manager Configuration" --menu "Arrow keys navigate, Enter selects" 15 60 4 \
        "1" "C compiler ($cc_state)" \
        "2" "Diagnostic logging ($debug_state)" \
        "S" "Save and Exit" \
        "Q" "Quit without saving" \
        3>&1 1>&2 2>&3) || CHOICE="Q"

    case "$CHOICE" in
        "1")
            COMPILER=$(whiptail --title "C compiler" --radiolist "Select compiler:" 15 50 2 \
                "CC_GCC" "Build with GCC" $([ "$cc_state" = "GCC" ] && echo "ON" || echo "OFF") \
                "CC_CLANG" "Build with Clang" $([ "$cc_state" = "Clang" ] && echo "ON" || echo "OFF") \
                3>&1 1>&2 2>&3) || true
            if [ -n "$COMPILER" ]; then
                if [ "$COMPILER" = "CC_GCC" ]; then
                    CONF[CC_GCC]="y"
                    CONF[CC_CLANG]="n"
                else
                    CONF[CC_GCC]="n"
                    CONF[CC_CLANG]="y"
                fi
            fi
            ;;
        "2")
            if [ "$debug_state" = "ON" ]; then
                CONF[DEBUG]="n"
            else
                CONF[DEBUG]="y"
            fi
            ;;
        "S")
            cat << OUT > "$chosen"
#
# Default option values.
#
# "make config" merges these over the declared defaults in Kconfig
# and writes the result to .config.
#

CONFIG_CC_GCC=${CONF[CC_GCC]:-y}
CONFIG_CC_CLANG=${CONF[CC_CLANG]:-n}
CONFIG_DEBUG=${CONF[DEBUG]:-n}
OUT
            echo "Configuration saved to configs/config"
            make config
            break
            ;;
        "Q")
            echo "Cancelled."
            break
            ;;
    esac
done
