#!/bin/sh
# Simple demo app: prints system info then echoes input from stdin
echo "=== WSLC Container Demo ==="
echo "Hostname: $(hostname)"
echo "OS:       $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "Uptime:   $(uptime -p 2>/dev/null || echo 'N/A')"
echo "=========================="
echo ""

# If arguments are provided, run them; otherwise just exit
if [ $# -gt 0 ]; then
    exec "$@"
fi
