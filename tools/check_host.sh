#!/bin/sh
# Run platform-independent contracts; artifacts stay in ignored tmp/.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
OUT="$ROOT/tmp/host-checks"
mkdir -p "$OUT"
for name in test_calendar_math test_input_semantics test_timed_state test_watch_interaction test_companion_controls; do
  c++ -std=c++11 -Wall -Wextra -Werror -Istopwatch/bot-ux-watch/include \
    "stopwatch/bot-ux-watch/test/$name.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
for name in hid_framing analog_input battery_double_tap agent_signal bottom_led_frame; do
  case "$name" in
    hid_framing) source=core2/bot-ux-codex-core2/src/HidFraming.cpp ;;
    analog_input) source=core2/bot-ux-codex-core2/src/AnalogInput.cpp ;;
    *) source= ;;
  esac
  c++ -std=c++11 -Wall -Wextra -Werror -Icore2/bot-ux-codex-core2/include \
    $source "core2/bot-ux-codex-core2/test/${name}_test.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
c++ -std=c++11 -Wall -Wextra -Werror -Ilib/ux-components/src \
  lib/ux-components/test/components_test.cpp lib/ux-components/src/Font*.cpp -o "$OUT/ux-components"
"$OUT/ux-components" "$OUT/ux-components.ppm"
echo 'PASS ux-components'
sh lib/bot-ux/tools/host-preview/render.sh "$OUT/bot-preview"
echo 'PASS bot-preview'
