#!/bin/bash

# Color Variables
RED="\e[1;31m"
GREEN="\e[1;32m"
RESET="\e[0m"

# Print message function
print_msg() {
    local COLOR=$1
    shift
    echo -e "${COLOR}$*${RESET}"
}

# Script header
print_msg "$GREEN" "\n - Cleanup script for Samsung kernel image - "
print_msg "$RED" "       by poqdavid \n"

print_msg "$GREEN" "Started cleaning up..."

git restore kernel-5.10/
git clean -fd kernel-5.10/
rm -rf kernel-5.10/KernelSU
rm -rf out

print_msg "$GREEN" "Finsished cleaning up..."