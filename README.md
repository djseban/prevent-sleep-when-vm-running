# What is it and why it exists

**prevent-sleep-when-vm-running** is small C++ application for seamless Libvirt QEMU/KVM guest power management. 
Say, for example, you have a macOS KVM. Application will:
 - at guest start, prevent host from sleeping by invoking libvirt-nosleep@vm-name systemd service (so if you want this app to work, you must have that service installed),
 - at guest ACPI sleep (i.e. "pmsuspended" state), allow host to sleep by stopping libvirt-nosleep@vm-name systemd service,
 - at host wake, wake up the guest also,
 - optionally, at guest ACPI sleep, put the host to sleep also (--sleep-instantly option)

The app was coded for my specific case, in which I want to use my VM as a daily driver and not resign from power management options. Especially, in times when the power bill could knock your socks off.
# Building

This piece is too small for any greater Makefiles, so you can build it in a most simple way:

    g++ prevent-sleep-when-vm-running.cpp -o preventsleep -lstdc++

# Usage
Usage is fairly simple. You can just run it with root privileges and let it work in the background. In case you want the host to suspend when your guest suspends:

    ./preventsleep my-vm-name --sleep-instantly &>/var/log/preventsleep.log &

or if you just want it to prevent sleep and wake up your guest when your host wakes up:

    ./preventsleep my-vm-name &>/var/log/preventsleep.log &

In my case, I wanted it to be as seamless as it can be. If you have per-guest QEMU hooks already set up, go to `/etc/libvirt/hooks`. Copy the file you built (`preventsleep`) to said dir, then open the `qemu` file by the editor of choice. Most probably you will be presented with something like this:

    #!/bin/bash
    #
    # Author: Sebastiaan Meijer (sebastiaan@passthroughpo.st)
    #
    # Copy this file to /etc/libvirt/hooks, make sure it's called "qemu".
    # After this file is installed, restart libvirt.
    # From now on, you can easily add per-guest qemu hooks.
    # Add your hooks in /etc/libvirt/hooks/qemu.d/vm_name/hook_name/state_name.
    # For a list of available hooks, please refer to https://www.libvirt.org/hooks.html
    #
    
    GUEST_NAME="$1"
    HOOK_NAME="$2"
    STATE_NAME="$3"
    MISC="${@:4}"
    
    BASEDIR="$(dirname $0)"
    
    HOOKPATH="$BASEDIR/qemu.d/$GUEST_NAME/$HOOK_NAME/$STATE_NAME"
    
    set -e # If a script exits with an error, we should as well.
    
    # check if it's a non-empty executable file
    if [ -f "$HOOKPATH" ] && [ -s "$HOOKPATH"] && [ -x "$HOOKPATH" ]; then
        eval \"$HOOKPATH\" "$@"
    elif [ -d "$HOOKPATH" ]; then
        while read file; do
            # check for null string
            if [ ! -z "$file" ]; then
              eval \"$file\" "$@"
            fi
        done <<< "$(find -L "$HOOKPATH" -maxdepth 1 -type f -executable -print;)"
    fi

After the line `set -e # If a script exits with an error, we should as well.` add

    # run small cpp program :)
    case "$HOOK_NAME" in
	    "started")
		    $BASEDIR/preventsleep $GUEST_NAME --sleep-instantly &>/var/log/preventsleep.log &
		   ;;
    esac

This will ensure, that when your vm starts, so will this application. Happy use of seamless power management :)