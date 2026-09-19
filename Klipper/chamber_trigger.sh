#!/bin/bash
# ==============================================================================
# Chamber Master - Klipper Trigger Script
# Tested and contributed by Richard Kennett (https://github.com/richard-kennett - Issue #3)
# 
# Usage:
#   bash $HOME/chamber_trigger.sh <URL> <MATERIAL> [TEMPERATURE]
# 
# Example:
#   bash $HOME/chamber_trigger.sh enclosure-monitor.local PLA
#   bash $HOME/chamber_trigger.sh enclosure-monitor.local CUSTOM 55
#   bash $HOME/chamber_trigger.sh enclosure-monitor.local COOLDOWN
# ==============================================================================

URL=$1
MATERIAL=$2
TEMPERATURE=$3

# Fix: Force an IPv4 ping lookup to grab the monitor's IP straight from router / mDNS cache
MONITOR_IP=$(ping -4 -c 1 "${URL}" 2>/dev/null | grep -oP '\(\K[0-9.]+(?=\))' | head -n 1)

# Ensure an IP address was resolved before calling curl
if [ -z "$MONITOR_IP" ]; then
    echo "Error: ${URL} IP address not found."
    exit 1
fi

# Send HTTP request to Chamber Master /material endpoint
if [ -z "$TEMPERATURE" ]; then
    curl -s -o /dev/null -w "%{http_code}" "http://${MONITOR_IP}/material?material=${MATERIAL}"
else
    curl -s -o /dev/null -w "%{http_code}" "http://${MONITOR_IP}/material?material=${MATERIAL}&temperature=${TEMPERATURE}"
fi
