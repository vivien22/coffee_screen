#!/bin/sh
# Builds the firmware with PlatformIO, shows its size, and fails if the RAM goes over its budget: ./tools/check_flash.sh
# Used by the CI (.github/workflows/test.yml) and usable by hand (PIO=/path/to/pio to use another pio).
#   RAM_BUDGET    bytes of RAM allowed (the Nano has 2048): the rest is the stack, whose overflow does not stop the
#                 build and corrupts the memory without a message
#   FLASH_BUDGET  optional: fail over this many bytes of flash. No budget by default: the linker already refuses a
#                 program over the 30720 bytes of the Nano, the size is only shown
#   LABEL         name of this build in the CI summary
set -e
cd "$(dirname "$0")/.."

RAM_BUDGET=${RAM_BUDGET:-1600}
FLASH_BUDGET=${FLASH_BUDGET:-}
PIO=${PIO:-pio}
FLASH_LIMIT=30720

if ! output=$("$PIO" run 2>&1); then
  echo "$output"
  echo "BUILD FAILED"
  exit 1
fi

flash=$(echo "$output" | sed -n 's/^Flash:.*used \([0-9]*\) bytes.*/\1/p')
ram=$(echo "$output" | sed -n 's/^RAM:.*used \([0-9]*\) bytes.*/\1/p')
if [ -z "$flash" ] || [ -z "$ram" ]; then
  echo "$output"
  echo "The size of the firmware was not found in the output of pio"
  exit 1
fi

# the share of the flash, in tenths of percent, rounded like pio does (96.2 %)
tenths=$(((flash * 1000 + FLASH_LIMIT / 2) / FLASH_LIMIT))
percent="$((tenths / 10)).$((tenths % 10))"

echo "flash: $flash B of $FLASH_LIMIT B ($percent %)"
echo "RAM:   $ram B (budget $RAM_BUDGET B, limit 2048 B)"

# Shown in the summary of the CI run
if [ -n "$GITHUB_STEP_SUMMARY" ]; then
  echo "- ${LABEL:-firmware}: flash **$flash B** ($percent % of $FLASH_LIMIT B), RAM **$ram B** (budget $RAM_BUDGET B)" >> "$GITHUB_STEP_SUMMARY"
fi

status=0
if [ -n "$FLASH_BUDGET" ] && [ "$flash" -gt "$FLASH_BUDGET" ]; then
  echo "FLASH OVER BUDGET by $((flash - FLASH_BUDGET)) B"
  status=1
fi
if [ "$ram" -gt "$RAM_BUDGET" ]; then
  echo "RAM OVER BUDGET by $((ram - RAM_BUDGET)) B"
  status=1
fi
exit $status
