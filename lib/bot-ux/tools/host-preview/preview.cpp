#include <BotUx.h>

#include <assert.h>
#include <fstream>
#include <iostream>
#include <string>

static uint32_t gNow = 1000;
uint32_t millis() { return gNow; }

struct MoodCase {
    botux::BotUx::Mood mood;
    const char* label;
    bool talking;
};

static const MoodCase kMoods[] = {
    {botux::BotUx::Mood::Idle, "Idle", false},
    {botux::BotUx::Mood::Listening, "Listening", false},
    {botux::BotUx::Mood::Thinking, "Thinking", false},
    {botux::BotUx::Mood::Speaking, "Speaking", true},
    {botux::BotUx::Mood::Happy, "Happy", false},
    {botux::BotUx::Mood::Sad, "Sad", false},
    {botux::BotUx::Mood::Sleepy, "Sleepy", false},
    {botux::BotUx::Mood::Surprised, "Surprised", false},
    {botux::BotUx::Mood::Working, "Working", false},
    {botux::BotUx::Mood::Waiting, "Waiting", false},
    {botux::BotUx::Mood::Blocked, "Blocked", false},
    {botux::BotUx::Mood::Done, "Done", false},
};

static std::string render(const MoodCase& item, int size, int index) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.seedBlink(0x123400u + (uint32_t)index);
    bot.begin(&canvas);
    bot.setMood(item.mood);
    bot.setTalking(item.talking);

    // Exercise the same time-based update path used on device and let pose easing settle.
    for (int frame = 0; frame < 100; ++frame) {
        gNow = 1000u + (uint32_t)frame * 16u;
        bot.update(gNow);
    }
    bot.draw();
    assert(bot.metrics().cx == size / 2);
    assert(bot.metrics().bodyR > 0);
    std::string body = canvas.svgBody();
    assert(!body.empty());
    return body;
}

static bool writeSheet(const char* path, int size) {
    const int columns = (size <= 72) ? 6 : 4;
    const int rows = ((int)(sizeof(kMoods) / sizeof(kMoods[0])) + columns - 1) / columns;
    const int labelH = (size <= 72) ? 16 : 22;
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << columns * size
        << "' height='" << rows * (size + labelH) << "' viewBox='0 0 " << columns * size
        << " " << rows * (size + labelH) << "'>\n";
    out << "<rect width='100%' height='100%' fill='#11151D'/>\n";
    for (int i = 0; i < (int)(sizeof(kMoods) / sizeof(kMoods[0])); ++i) {
        int x = (i % columns) * size;
        int y = (i / columns) * (size + labelH);
        out << "<g transform='translate(" << x << " " << y << ")'>\n"
            << render(kMoods[i], size, i) << "</g>\n";
        out << "<text x='" << x + size / 2 << "' y='" << y + size + labelH * 2 / 3
            << "' text-anchor='middle' font-family='sans-serif' font-size='"
            << ((size <= 72) ? 9 : 13) << "' fill='#D7DFEA'>" << kMoods[i].label << "</text>\n";
    }
    out << "</svg>\n";
    return true;
}

static void checkBodylessStateGlyphs() {
    gNow = 1000;
    M5Canvas canvas(72, 72);
    botux::BotUx::Style style;
    style.bodyStyle = botux::BotUx::BodyStyle::None;
    botux::BotUx bot;
    bot.setStyle(style);
    bot.begin(&canvas);

    bot.setMood(botux::BotUx::Mood::Thinking);
    bot.update(3000);
    bot.draw();
    assert(canvas.svgBody().find("<circle") != std::string::npos);

    canvas.clear();
    bot.setMood(botux::BotUx::Mood::Blocked);
    bot.update(3200);
    bot.draw();
    const std::string blocked = canvas.svgBody();
    assert(blocked.find("<circle") != std::string::npos);
    assert(blocked.find("rx='") != std::string::npos);
}

static void checkLateUptimeHasNoPhantomReaction() {
    gNow = 1000;
    M5Canvas canvas(200, 200);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.update(0x80001000u);
    bot.draw();
    // Idle pill eyes use capsule triangles; a phantom Surprised state uses circles.
    assert(canvas.svgBody().find("<polygon") != std::string::npos);
}

int main(int argc, char** argv) {
    checkBodylessStateGlyphs();
    checkLateUptimeHasNoPhantomReaction();
    const char* outDir = (argc > 1) ? argv[1] : ".";
    std::string tiny = std::string(outDir) + "/moods-40.svg";
    std::string small = std::string(outDir) + "/moods-72.svg";
    std::string large = std::string(outDir) + "/moods-200.svg";
    if (!writeSheet(tiny.c_str(), 40) || !writeSheet(small.c_str(), 72) ||
        !writeSheet(large.c_str(), 200)) {
        std::cerr << "could not write preview sheets\n";
        return 1;
    }
    std::cout << tiny << "\n" << small << "\n" << large << "\n";
    return 0;
}
