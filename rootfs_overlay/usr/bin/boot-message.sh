#!/bin/sh
# Display boot message on framebuffer console
# Called by erlinit --pre-run-exec before starting the BEAM VM

TTY=/dev/tty0

if [ -c "$TTY" ]; then
    # Clear screen and show boot message
    printf '\033[2J\033[H' > "$TTY"
    printf '\n\n' > "$TTY"
    printf '  Rock 5T - Nerves System\n' > "$TTY"
    printf '  Starting Erlang VM...\n\n' > "$TTY"
fi
