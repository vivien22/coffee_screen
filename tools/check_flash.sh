#!/bin/sh
# Builds the firmware with PlatformIO and fails if the flash or the RAM goes over its budget: ./tools/check_flash.sh
# Used by the CI (.github/workflows/test.yml) and usable by hand (PIO=/path/to/pio to use another pio).
#   FLASH_BUDGET  bytes of flash allowed (the Nano has 30720): the rest is kept free for what comes next
#   RAM_BUDGET    bytes of RAM allowed (the Nano has 2048): the rest is the stack
#   LABEL         name of this build in the CI summary
set -e
cd "$(dirname "$0")/.."

FLASH_BUDGET=${FLASH_BUDGET:-29800}
RAM_BUDGET=${RAM_BUDGET:-1600}
PIO=${PIO:-pio}

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

echo "flash: $flash B (budget $FLASH_BUDGET B, limit 30720 B)"
echo "RAM:   $ram B (budget $RAM_BUDGET B, limit 2048 B)"

# Shown in the summary of the CI run
if [ -n "$GITHUB_STEP_SUMMARY" ]; then
  echo "- ${LABEL:-firmware}: flash **$flash B** (budget $FLASH_BUDGET B), RAM **$ram B** (budget $RAM_BUDGET B)" >> "$GITHUB_STEP_SUMMARY"
fi

status=0
if [ "$flash" -gt "$FLASH_BUDGET" ]; then
  echo "FLASH OVER BUDGET by $((flash - FLASH_BUDGET)) B"
  status=1
fi
if [ "$ram" -gt "$RAM_BUDGET" ]; then
  echo "RAM OVER BUDGET by $((ram - RAM_BUDGET)) B"
  status=1
fi
exit $status
