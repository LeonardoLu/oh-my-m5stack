#include "Settings.h"

#include <assert.h>
#include <map>
#include <string>

namespace settingstest {
std::map<std::string, bool> bools;
std::map<std::string, uint8_t> bytes;
std::map<std::string, uint16_t> ushorts;
std::map<std::string, std::string> strings;
}

void botux::BotUx::setStyle(const Style& style) {
    _style = style;
}

int main() {
    Settings defaults;
    assert(defaults.data().dimTimeout == 1);
    assert(defaults.data().screenOffTimeout == 2);
    assert(!defaults.data().buttonWakeOnly);
    assert(Settings::TIMEOUT_COUNT == 6);
    assert(Settings::timeoutIndex(0, 1) == 0);
    assert(Settings::timeoutIndex(5, 1) == 5);
    assert(Settings::timeoutIndex(6, 1) == 1);

    // An old preference set has no new keys and keeps its existing values.
    settingstest::bools["hour24"] = false;
    settingstest::bools["buttonFx"] = false;
    settingstest::bytes["theme"] = Settings::THEME_MONO;
    Settings oldNvs;
    oldNvs.begin();
    assert(!oldNvs.data().hour24);
    assert(!oldNvs.data().buttonFeedback);
    assert(oldNvs.data().theme == Settings::THEME_MONO);
    assert(oldNvs.data().dimTimeout == 1);
    assert(oldNvs.data().screenOffTimeout == 2);
    assert(!oldNvs.data().buttonWakeOnly);

    // Corrupt timeout indices recover independently to their shipped defaults.
    settingstest::bytes["dimTimeout"] = 6;
    settingstest::bytes["screenOffTo"] = 255;
    settingstest::bools["buttonWake"] = true;
    Settings repaired;
    repaired.begin();
    assert(repaired.data().dimTimeout == 1);
    assert(repaired.data().screenOffTimeout == 2);
    assert(repaired.data().buttonWakeOnly);

    // All six selections and both wake modes survive a save/load cycle.
    for (uint8_t index = 0; index < Settings::TIMEOUT_COUNT; ++index) {
        repaired.data().dimTimeout = index;
        repaired.data().screenOffTimeout = Settings::TIMEOUT_COUNT - 1 - index;
        repaired.data().buttonWakeOnly = (index & 1) != 0;
        repaired.save();
        assert(settingstest::bytes["dimTimeout"] == index);
        assert(settingstest::bytes["screenOffTo"] == Settings::TIMEOUT_COUNT - 1 - index);
        assert(settingstest::bools["buttonWake"] == ((index & 1) != 0));

        Settings restored;
        restored.begin();
        assert(restored.data().dimTimeout == index);
        assert(restored.data().screenOffTimeout == Settings::TIMEOUT_COUNT - 1 - index);
        assert(restored.data().buttonWakeOnly == ((index & 1) != 0));
    }
}
