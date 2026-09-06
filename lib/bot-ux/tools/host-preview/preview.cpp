#include <BotUx.h>

#include <assert.h>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
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

struct ExpressionCase {
    botux::BotUx::Expression expression;
    const char* label;
};

static const ExpressionCase kExpressions[] = {
    {botux::BotUx::Expression::Neutral, "Neutral"},
    {botux::BotUx::Expression::Curious, "Curious"},
    {botux::BotUx::Expression::Focused, "Focused"},
    {botux::BotUx::Expression::Joy, "Joy"},
    {botux::BotUx::Expression::Skeptical, "Skeptical"},
    {botux::BotUx::Expression::Bashful, "Bashful"},
    {botux::BotUx::Expression::Wink, "Wink"},
    {botux::BotUx::Expression::Dizzy, "Dizzy"},
    {botux::BotUx::Expression::Alarmed, "Alarmed"},
};

struct AnimationCase {
    botux::BotUx::Animation animation;
    const char* label;
};

static const AnimationCase kAnimations[] = {
    {botux::BotUx::Animation::Calm, "Calm"},
    {botux::BotUx::Animation::Curious, "Curious"},
    {botux::BotUx::Animation::Orbit, "Orbit"},
    {botux::BotUx::Animation::Bounce, "Bounce"},
    {botux::BotUx::Animation::Glitch, "Glitch"},
    {botux::BotUx::Animation::Wave, "Wave"},
    {botux::BotUx::Animation::Sparkle, "Sparkle"},
};

static size_t countToken(const std::string& value, const char* token) {
    size_t count = 0, pos = 0;
    while ((pos = value.find(token, pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    return count;
}

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

static std::string renderExpression(const ExpressionCase& item, int size) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.setAnimation(botux::BotUx::Animation::Calm);
    bot.setMotionAmount(0.0f);
    bot.setExpression(item.expression, 0);
    bot.update(gNow);
    bot.draw();
    return canvas.svgBody();
}

static std::string renderAnimation(const AnimationCase& item, int size,
                                   bool reduced = false) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.setAnimation(item.animation);
    bot.setAnimationSpeed(1.25f);
    bot.setReducedMotion(reduced);
    bot.update(2473);
    bot.draw();
    return canvas.svgBody();
}

template <typename Case, size_t N, typename RenderFn>
static bool writePickerSheet(const char* path, const Case (&items)[N], RenderFn renderItem, int size = 120) {
    const int columns = 3, labelH = 20;
    const int cellWidth = size < 90 ? 90 : size;
    const int rows = ((int)N + columns - 1) / columns;
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << columns * cellWidth
        << "' height='" << rows * (size + labelH) << "'>\n"
        << "<rect width='100%' height='100%' fill='#11151D'/>\n";
    for (int i = 0; i < (int)N; ++i) {
        int x = (i % columns) * cellWidth + (cellWidth - size) / 2;
        int y = (i / columns) * (size + labelH);
        out << "<g transform='translate(" << x << " " << y << ")'>\n"
            << renderItem(items[i], size) << "</g>\n"
            << "<text x='" << x + size / 2 << "' y='" << y + size + 14
            << "' text-anchor='middle' font-family='sans-serif' font-size='12' fill='#D7DFEA'>"
            << items[i].label << "</text>\n";
    }
    out << "</svg>\n";
    return true;
}

static void writeJsString(std::ofstream& out, const std::string& value) {
    out << '"';
    for (char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': break;
            default: out << c; break;
        }
    }
    out << '"';
}

static bool writeAnimationPlayer(const char* path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
        << "<title>BotUx source-rendered animation preview</title>"
        << "<style>body{margin:0;background:#11151d;color:#d7dfea;font:16px system-ui;display:grid;"
        << "place-items:center;min-height:100vh}.card{width:min(88vw,520px);display:grid;gap:14px}"
        << "#stage{aspect-ratio:1;border:1px solid #29313d;border-radius:20px;overflow:hidden}"
        << "#stage svg{width:100%;height:100%;display:block}.row{display:flex;gap:10px;align-items:center}"
        << "select,button,input{font:inherit}select{flex:1;padding:8px}input{width:100%}</style>"
        << "<main class='card'><h1>BotUx actual-source preview</h1><div id='stage'></div>"
        << "<div class='row'><select id='mode'></select><button id='play'>Pause</button></div>"
        << "<input id='scrub' type='range' min='0' max='59' value='0'>"
        << "<small>60 frames per mode at 15 fps, generated by BotUx.cpp. "
        << "The browser only plays the captured M5GFX primitive calls.</small></main><script>\n"
        << "const modes=[\n";

    const int modeCount = 10;
    for (int mode = 0; mode < modeCount; ++mode) {
        const char* name = (mode == 0) ? "Auto lifecycle"
                         : (mode <= 7) ? kAnimations[mode - 1].label
                         : (mode == 8) ? "Expression transitions" : "IMU tilt + shake";
        out << "{name:";
        writeJsString(out, name);
        out << ",frames:[\n";

        gNow = 1000;
        M5Canvas canvas(200, 200);
        botux::BotUx bot;
        bot.begin(&canvas);
        bot.setAnimationSpeed(1.25f);
        if (mode == 0) {
            bot.setAnimation(botux::BotUx::Animation::Auto);
        } else if (mode <= 7) {
            bot.setExpression(botux::BotUx::Expression::Neutral, 0);
            bot.setAnimation(kAnimations[mode - 1].animation);
        } else {
            bot.setAnimation(botux::BotUx::Animation::Calm);
        }

        std::set<std::string> uniqueFrames;
        for (int frame = 0; frame < 60; ++frame) {
            gNow = 1000u + (uint32_t)frame * 67u;
            if (mode == 0 && frame % 10 == 0) {
                static const botux::BotUx::Mood lifecycle[] = {
                    botux::BotUx::Mood::Idle, botux::BotUx::Mood::Listening,
                    botux::BotUx::Mood::Working, botux::BotUx::Mood::Waiting,
                    botux::BotUx::Mood::Blocked, botux::BotUx::Mood::Done,
                };
                bot.setMood(lifecycle[frame / 10], 180);
            } else if (mode == 8) {
                if (frame == 0) bot.setExpression(botux::BotUx::Expression::Neutral, 0);
                else if (frame == 15) bot.setExpression(botux::BotUx::Expression::Skeptical, 500);
                else if (frame == 30) bot.setExpression(botux::BotUx::Expression::Wink, 500);
                else if (frame == 45) bot.setExpression(botux::BotUx::Expression::Joy, 500);
            } else if (mode == 9) {
                float tiltX = sinf(frame * 0.14f);
                float tiltY = cosf(frame * 0.11f) * 0.75f;
                float shake = (frame >= 26 && frame <= 33) ? 0.85f : 0.0f;
                bot.setMotion(tiltX, tiltY, shake);
            }
            canvas.clear();
            bot.update(gNow);
            bot.draw();
            std::string frameSvg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 200'>\n";
            frameSvg += canvas.svgBody();
            frameSvg += "</svg>";
            uniqueFrames.insert(frameSvg);
            writeJsString(out, frameSvg);
            out << ((frame == 59) ? "\n" : ",\n");
        }
        assert(uniqueFrames.size() >= 8);
        out << "]}" << ((mode == modeCount - 1) ? "\n" : ",\n");
    }

    out << "];const mode=document.querySelector('#mode'),stage=document.querySelector('#stage'),"
        << "scrub=document.querySelector('#scrub'),play=document.querySelector('#play');"
        << "modes.forEach((m,i)=>mode.add(new Option(m.name,i)));let frame=0,running=true;"
        << "function draw(){stage.innerHTML=modes[+mode.value].frames[frame];scrub.value=frame}"
        << "mode.onchange=()=>{frame=0;draw()};scrub.oninput=()=>{frame=+scrub.value;draw()};"
        << "play.onclick=()=>{running=!running;play.textContent=running?'Pause':'Play'};"
        << "setInterval(()=>{if(running){frame=(frame+1)%60;draw()}},67);draw();</script>\n";
    return true;
}

static void checkExpressionAndAnimationRange() {
    std::set<std::string> expressions;
    for (const auto& item : kExpressions) expressions.insert(renderExpression(item, 200));
    assert(expressions.size() == sizeof(kExpressions) / sizeof(kExpressions[0]));

    std::set<std::string> animations;
    for (const auto& item : kAnimations) {
        const std::string svg = renderAnimation(item, 200);
        animations.insert(svg);
        assert(countToken(svg, "<") <= 2200); // perimeter coverage stays bounded
    }
    assert(animations.size() == sizeof(kAnimations) / sizeof(kAnimations[0]));
}

static void checkTransitionsMotionAndSparseFrames() {
    M5Canvas canvas(200, 200);
    botux::BotUx bot;
    gNow = 1000;
    bot.begin(&canvas);
    bot.setAnimation(botux::BotUx::Animation::Calm);
    bot.setAnimationSpeed(99.0f);
    assert(bot.animationSpeed() == 3.0f);
    bot.setAnimationSpeed(-1.0f);
    assert(bot.animationSpeed() == 0.25f);
    bot.setAnimationSpeed(1.0f);
    bot.setMotionAmount(99.0f);
    assert(bot.motionAmount() == 2.0f);
    bot.setMotionAmount(1.0f);
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.update(gNow);
    bot.draw();
    const std::string neutral = canvas.svgBody();

    canvas.clear();
    bot.setExpression(botux::BotUx::Expression::Skeptical, 600);
    bot.update(1016);
    bot.draw();
    const std::string transition = canvas.svgBody();
    canvas.clear();
    bot.update(3016);
    bot.draw();
    const std::string settled = canvas.svgBody();
    assert(neutral != transition && transition != settled && neutral != settled);

    canvas.clear();
    bot.setMotion(1.0f, -0.8f, 0.7f);
    bot.update(3316); // 3 fps sensor/render loop still converges in real time.
    bot.draw();
    const std::string moved = canvas.svgBody();
    assert(moved != settled);
    assert(moved.find("nan") == std::string::npos);

    const AnimationCase bounce = {botux::BotUx::Animation::Bounce, "Bounce"};
    assert(renderAnimation(bounce, 200, false) != renderAnimation(bounce, 200, true));

    // A very late frame must finish an overdue blink rather than holding a
    // stale intermediate phase until several future frames arrive.
    canvas.clear();
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.setMotion(0.0f, 0.0f, 0.0f);
    bot.update(10000);
    bot.update(11000);
    bot.draw();
    assert(countToken(canvas.svgBody(), "<rect") > 30);
}

static void checkMotionStaysInCanvas() {
    for (int size : {40, 72, 200, 310}) {
        for (const auto& item : kAnimations) {
            gNow = 1000;
            M5Canvas canvas((int16_t)size, (int16_t)size);
            botux::BotUx bot;
            bot.begin(&canvas);
            bot.setExpression(botux::BotUx::Expression::Curious, 0);
            bot.setAnimation(item.animation);
            for (int frame = 0; frame < 60; ++frame) {
                float direction = (frame < 30) ? 1.0f : -1.0f;
                bot.setMotion(direction, -direction, 1.0f);
                gNow = 1000u + (uint32_t)frame * 67u;
                canvas.clear();
                bot.update(gNow);
                bot.draw();
                assert(!canvas.outOfBounds());
            }
        }
    }
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
    std::set<uint16_t> coverageColors;
    int lit = 0;
    for (int y = 0; y < 72; ++y) for (int x = 0; x < 72; ++x) {
        uint16_t c = canvas.readPixel(x,y);
        if (c != style.bgColor) { ++lit; coverageColors.insert(c); }
    }
    assert(lit > 150 && lit < 500);
    assert(coverageColors.size() > 12); // three opaque dot colors plus native AA fringes
    assert(canvas.svgBody().find("<circle") == std::string::npos);

    canvas.clear();
    bot.setMood(botux::BotUx::Mood::Blocked);
    bot.update(3200);
    bot.draw();
    const std::string blocked = canvas.svgBody();
    assert(blocked.find("<circle") == std::string::npos);
    assert(canvas.readPixel(36, 36) != style.bgColor);
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
    assert(countToken(canvas.svgBody(), "<rect") > 30);
}

static void checkWaitingAndSleepyStayDistinct() {
    const MoodCase sleepy = {botux::BotUx::Mood::Sleepy, "Sleepy", false};
    const MoodCase waiting = {botux::BotUx::Mood::Waiting, "Waiting", false};
    for (int size : {40, 72}) {
        const std::string sleepySvg = render(sleepy, size, 0);
        const std::string waitingSvg = render(waiting, size, 0);
        const size_t sleepyBody = sleepySvg.find("<rect", 1);
        const size_t waitingBody = waitingSvg.find("<rect", 1);
        const size_t sleepyBodyEnd = sleepySvg.find("/>", sleepyBody);
        const size_t waitingBodyEnd = waitingSvg.find("/>", waitingBody);
        assert(sleepyBodyEnd != std::string::npos && waitingBodyEnd != std::string::npos);
        assert(sleepySvg.substr(sleepyBodyEnd) != waitingSvg.substr(waitingBodyEnd));
    }
}

static void checkCoverageAndFacePlacement() {
    for (int size : {40, 72, 200, 310}) {
        for (const auto& item : kExpressions) {
            gNow = 1000;
            M5Canvas canvas(size, size);
            botux::BotUx bot;
            bot.begin(&canvas);
            bot.setAnimation(botux::BotUx::Animation::Calm);
            bot.setMotionAmount(0.0f);
            bot.setExpression(item.expression, 0);
            bot.update(gNow);
            bot.draw();
            float r = bot.metrics().bodyR;
            int minX = size, maxX = 0, minY = size, maxY = 0;
            std::set<uint16_t> colors;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                uint16_t pixel = canvas.readPixel(x, y);
                colors.insert(pixel);
                if (((pixel >> 11) & 31u) < 16u && fabsf(x - size * 0.5f) < r * 0.70f &&
                    fabsf(y - size * 0.5f) < r * 0.65f) {
                    minX = std::min(minX, x); maxX = std::max(maxX, x);
                    minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            }
            // Actual RGB565 boundary coverage, not browser smoothing of a path.
            assert(colors.size() > 12);
            assert(!canvas.outOfBounds());
            // Every face retains Neutral's upper-right placement and separated
            // pair scale; no expression throws the eyes to the lower hemisphere.
            assert(minX < maxX && minY <= maxY);
            assert((minX + maxX) * 0.5f > size * 0.5f);
            assert((minY + maxY) * 0.5f < size * 0.5f - r * 0.16f);
            assert(maxX - minX < r * 0.95f);
            assert(maxY - minY < r * 0.65f);
        }
    }
}

static void checkExplicitNeutralOverridesRestingMood() {
    for (int size : {40, 72, 200}) {
        for (auto mood : {botux::BotUx::Mood::Sleepy, botux::BotUx::Mood::Waiting}) {
            int heights[2] = {};
            for (int selected = 0; selected < 2; ++selected) {
                gNow = 1000;
                M5Canvas canvas(size, size);
                botux::BotUx bot;
                bot.begin(&canvas);
                bot.setMotionAmount(0.0f);
                bot.setMood(mood, 0);
                if (selected) bot.setExpression(botux::BotUx::Expression::Neutral, 0);
                bot.update(gNow); bot.draw();
                int minY = size, maxY = -1;
                float r = bot.metrics().bodyR;
                for (int y = (int)(size * 0.5f - r * 0.55f); y < size / 2; ++y)
                    for (int x = size / 2; x < size / 2 + r * 0.60f; ++x)
                        if (((canvas.readPixel(x, y) >> 11) & 31u) < 16u) {
                            minY = std::min(minY, y); maxY = std::max(maxY, y);
                        }
                heights[selected] = maxY - minY + 1;
            }
            // Auto retains the resting marks; explicit Neutral reopens the face,
            // including the tiny toolbar renderer used by Core2 settings.
            assert(heights[1] >= heights[0] + 2);
        }
    }
}

static bool writeTransitionSheet(const char* path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='960' height='280'>";
    for (int row = 0; row < 2; ++row) {
        gNow = 1000;
        M5Canvas canvas(120, 120);
        botux::BotUx bot;
        bot.begin(&canvas);
        bot.setMotionAmount(0.0f);
        bot.setExpression(botux::BotUx::Expression::Neutral, 0);
        bot.update(gNow);
        bot.setExpression(row == 0 ? botux::BotUx::Expression::Joy :
                                    botux::BotUx::Expression::Wink, 650);
        for (int frame = 0; frame < 8; ++frame) {
            gNow += 100;
            bot.update(gNow);
            canvas.clear(); bot.draw();
            out << "<g transform='translate(" << frame * 120 << " " << row * 140 << ")'>"
                << canvas.svgBody() << "</g>";
        }
    }
    out << "</svg>";
    return true;
}

static void checkCompanionSemantics() {
    using Bot = botux::BotUx;
    Bot bot;
    assert(std::strcmp(bot.name(), "Milo") == 0);
    bot.setName("  Ava<script>你好 !  ");
    assert(std::strcmp(bot.name(), "Avascript") == 0);
    bot.setName("abcdefghijklmnopqrst"); assert(std::strlen(bot.name()) == Bot::kNameMax);
    bot.setName(nullptr); assert(std::strcmp(bot.name(), "Milo") == 0);
    for (uint8_t i = 0; i < Bot::presetCount(); ++i) {
        bot.applyPreset(i); const auto p = Bot::preset(i);
        assert(bot.mood() == p.mood && bot.expression() == p.expression && bot.animation() == p.animation);
        assert(bot.randomPreset() != i);
    }
    bot.resetToIdle(); assert(bot.mood() == Bot::Mood::Idle && bot.expression() == Bot::Expression::Auto);
    assert(bot.nextPreset() == 1);
    char desc[96]; bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strstr(desc, "Milo ") == desc);
    for (size_t capacity = 1; capacity < 30; ++capacity) {
        std::memset(desc, 0x7f, sizeof(desc));
        bot.describe(desc, capacity, Bot::Language::Chinese);
        assert(std::strlen(desc) < capacity && desc[capacity] == 0x7f);
        // Our corpus consists of ASCII and three-byte UTF-8 codepoints.
        for (size_t i = 0; desc[i];) {
            if ((unsigned char)desc[i] < 128) ++i;
            else { assert(desc[i + 1] && desc[i + 2]); i += 3; }
        }
    }
    assert(Bot::moodCount() * Bot::expressionCount() * Bot::animationCount() == 960);
    M5Canvas canvas(72, 72); bot.begin(&canvas);
    bot.resetToIdle(); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo is resting") == 0);
    bot.setExpression(Bot::Expression::Dizzy, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo feels dizzy") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Milo 感到眩晕") == 0);
    bot.setMood(Bot::Mood::Thinking, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo is thinking") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Milo 正在思考") == 0);
    bot.setName("Ava"); bot.setMood(Bot::Mood::Happy, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Ava is happy") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Ava 很高兴") == 0);
    for (uint8_t m = 0; m < Bot::moodCount(); ++m)
        for (uint8_t e = 0; e < Bot::expressionCount(); ++e)
            for (uint8_t a = 0; a < Bot::animationCount(); ++a) {
                assert(*Bot::moodName((Bot::Mood)m, Bot::Language::Chinese));
                assert(*Bot::expressionName((Bot::Expression)e, Bot::Language::Chinese));
                assert(*Bot::animationName((Bot::Animation)a, Bot::Language::Chinese));
                bot.setMood((Bot::Mood)m, 0); bot.setExpression((Bot::Expression)e, 0);
                bot.setAnimation((Bot::Animation)a); gNow += 16; bot.update(gNow);
                canvas.clear(); bot.draw(); assert(!canvas.outOfBounds());
            }
}

static float eyeCentroidX(M5Canvas& canvas) {
    float sum = 0, count = 0;
    for (int y = 0; y < canvas.height(); ++y) for (int x = 0; x < canvas.width(); ++x)
        if (canvas.readPixel(x, y) == botux::rgb565(32, 36, 41)) { sum += x; ++count; }
    assert(count); return sum / count;
}
static void checkTemporaryGaze() {
    using Bot = botux::BotUx;
    M5Canvas canvas(200, 200); Bot bot; bot.begin(&canvas);
    auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
    gNow += 1000; bot.gazeAt(-1, 0, 1800); bot.update(gNow); canvas.clear(); bot.draw();
    float left = eyeCentroidX(canvas);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.gazeAt(1, 0, 1800);
    gNow += 16; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeCentroidX(canvas) > left + 20);
    gNow += 1900; bot.setExpression(Bot::Expression::Neutral, 0); bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeCentroidX(canvas) < left + 36);
}

static void checkConnectedEyeMorphs() {
    using Bot = botux::BotUx;
    for (int size : {40, 72, 200}) for (int styleIndex = 0; styleIndex < 4; ++styleIndex) {
        M5Canvas canvas(size, size); Bot bot; bot.begin(&canvas);
        auto style = bot.style(); style.bodyStyle = Bot::BodyStyle::None;
        style.eyeStyle = (Bot::EyeStyle)styleIndex;
        style.bgColor = botux::rgb565(255,255,255);
        style.eyeColor = style.pupilColor = botux::rgb565(0,0,0);
        style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style);
        bot.setMotionAmount(0); bot.setExpression(Bot::Expression::Neutral, 0);
        gNow += 16; bot.update(gNow); bot.setExpression(Bot::Expression::Joy, 600);
        for (int frame = 0; frame < 40; ++frame) {
            gNow += 25; bot.update(gNow); canvas.clear(); bot.draw();
            std::vector<bool> seen(size * size, false); int components = 0;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                if (seen[y * size + x] || ((canvas.readPixel(x,y) >> 11) & 31) >= 24) continue;
                ++components; std::vector<int> queue(1, y * size + x); seen[y * size + x] = true;
                for (size_t k = 0; k < queue.size(); ++k) {
                    int px = queue[k] % size, py = queue[k] / size;
                    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                        int nx = px + dx, ny = py + dy;
                        if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                        int id = ny * size + nx;
                        if (!seen[id] && ((canvas.readPixel(nx,ny) >> 11) & 31) < 24) {
                            seen[id] = true; queue.push_back(id);
                        }
                    }
                }
            }
            assert(components == 2);
        }
    }
}

static uint32_t rasterHash(M5Canvas& canvas) {
    uint32_t hash = 2166136261u;
    for (int y = 0; y < canvas.height(); ++y) for (int x = 0; x < canvas.width(); ++x)
        hash = (hash ^ canvas.readPixel(x, y)) * 16777619u;
    return hash;
}

static void checkPersistentAnimationWindows() {
    using Bot = botux::BotUx;
    M5Canvas canvas(72, 72); canvas.setRecording(false);
    for (uint8_t mood = 0; mood < Bot::moodCount(); ++mood)
        for (uint8_t expr = 0; expr < Bot::expressionCount(); ++expr)
            for (uint8_t anim = 0; anim < Bot::animationCount(); ++anim) {
                gNow = 1000; Bot bot; bot.begin(&canvas);
                auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style); bot.seedBlink(1234);
                bot.setMood((Bot::Mood)mood, 0); bot.setExpression((Bot::Expression)expr, 0);
                bot.setAnimation((Bot::Animation)anim); bot.setTalking(mood == (uint8_t)Bot::Mood::Speaking);
                // Re-sample late windows too: changes must not depend on entry morph/blink.
                for (uint32_t window : {10000u, 30000u, 50000u}) {
                    std::set<uint32_t> frames;
                    for (int frame = 0; frame < 12; ++frame) {
                        gNow = window + frame * 250; bot.update(gNow); canvas.clear(); bot.draw();
                        assert(!canvas.outOfBounds()); frames.insert(rasterHash(canvas));
                    }
                    if (frames.size() < 4) std::cerr << "frozen " << (int)mood << "/" << (int)expr << "/" << (int)anim << " at " << window << " unique " << frames.size() << "\n";
                    assert(frames.size() >= 4);
                }
            }
    for (uint8_t preset = 0; preset < Bot::presetCount(); ++preset) {
        gNow = 1000; Bot bot; bot.begin(&canvas); bot.applyPreset(preset); bot.setMotionAmount(0.55f);
        for (uint32_t window : {10000u, 40000u, 80000u}) {
            std::set<uint32_t> frames;
            for (int frame = 0; frame < 16; ++frame) {
                gNow = window + frame * 250; bot.update(gNow); canvas.clear(); bot.draw();
                frames.insert(rasterHash(canvas));
            }
            assert(frames.size() >= 8);
        }
    }
}

static void checkGazeDirectionsAndMotionZero() {
    using Bot = botux::BotUx;
    M5Canvas canvas(200, 200); canvas.setRecording(false);
    gNow = 1000; Bot bot; bot.begin(&canvas);
    auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
    bot.setGazeDirection(Bot::GazeDirection::Left); bot.update(gNow); canvas.clear(); bot.draw();
    float left = eyeCentroidX(canvas);
    bot.setGazeDirection(Bot::GazeDirection::Right); bot.setExpression(Bot::Expression::Neutral, 0);
    gNow += 1000; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeCentroidX(canvas) > left + 25);
    bot.gazeAt(-1, 0); gNow += 200; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeCentroidX(canvas) < left + 5);
    gNow += 2000; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeCentroidX(canvas) > left + 25);
    assert(bot.gazeDirection() == Bot::GazeDirection::Right);
    for (uint8_t mood = 0; mood < Bot::moodCount(); ++mood) {
        bot.setMood((Bot::Mood)mood, 0); bot.setExpression(Bot::Expression::Dizzy, 0);
        bot.setAnimation(Bot::Animation::Orbit); bot.setGazeDirection(Bot::GazeDirection::Auto);
        for (int i = 0; i < 10; ++i) { gNow += 1000; bot.update(gNow); }
        canvas.clear(); bot.draw(); uint32_t first = rasterHash(canvas);
        for (int i = 0; i < 5; ++i) {
            gNow += 300; bot.update(gNow); canvas.clear(); bot.draw();
            assert(rasterHash(canvas) == first);
        }
    }
}

static void checkReducedMotionAmplitude() {
    using Bot = botux::BotUx;
    unsigned changes[2] = {};
    for (int reduced = 0; reduced < 2; ++reduced) {
        gNow = 1000; M5Canvas canvas(120, 120); canvas.setRecording(false);
        Bot bot; bot.begin(&canvas); bot.setReducedMotion(reduced);
        bot.setGazeDirection(Bot::GazeDirection::Center);
        bot.setExpression(Bot::Expression::Neutral, 0); bot.setAnimation(Bot::Animation::Calm);
        auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
        std::vector<uint16_t> previous(120 * 120);
        for (int frame = 0; frame < 40; ++frame) {
            gNow = 10000 + frame * 100; bot.update(gNow); canvas.clear(); bot.draw();
            for (int y = 0; y < 120; ++y) for (int x = 0; x < 120; ++x) {
                uint16_t pixel = canvas.readPixel(x, y);
                if (frame) {
                    uint16_t prior = previous[y * 120 + x];
                    changes[reduced] += 2 * std::abs((int)(pixel >> 11) - (int)(prior >> 11))
                        + std::abs((int)((pixel >> 5) & 63) - (int)((prior >> 5) & 63))
                        + 2 * std::abs((int)(pixel & 31) - (int)(prior & 31));
                }
                previous[y * 120 + x] = pixel;
            }
        }
    }
    assert(changes[0] > changes[1] * 2);
    assert(changes[1] > 0);
    std::cout << "Calm RGB565 temporal difference full/reduced: " << changes[0] << "/" << changes[1] << "\n";
}

static void benchmarkEyes() {
    using Bot = botux::BotUx;
    for (int size : {40, 72, 200}) for (auto expr : {Bot::Expression::Neutral, Bot::Expression::Joy}) {
        M5Canvas canvas(size, size); canvas.setRecording(false);
        Bot bot; bot.begin(&canvas); bot.setExpression(expr, 0); bot.setMotionAmount(0);
        bot.update(gNow);
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 500; ++i) bot.draw();
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 500;
        std::cout << "raster host " << size << " " << Bot::expressionName(expr) << ": " << us << " us/frame\n";
    }
}

static bool writeGazeSheet(const char* path) {
    using Bot = botux::BotUx;
    std::ofstream out(path); if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='720' height='140'>";
    for (uint8_t direction = 0; direction < Bot::gazeDirectionCount(); ++direction) {
        gNow = 1000; M5Canvas canvas(120, 120); Bot bot; bot.begin(&canvas);
        bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
        bot.setGazeDirection((Bot::GazeDirection)direction); bot.update(gNow); bot.draw();
        out << "<g transform='translate(" << direction * 120 << " 0)'>" << canvas.svgBody()
            << "<text x='60' y='132' text-anchor='middle' font-family='sans-serif' font-size='12'>"
            << Bot::gazeDirectionName((Bot::GazeDirection)direction) << "</text></g>";
    }
    out << "</svg>"; return true;
}

int main(int argc, char** argv) {
    assert(botux::BotUx().style().eyeColor == botux::rgb565(32, 36, 41));
    checkCompanionSemantics();
    checkTemporaryGaze();
    checkConnectedEyeMorphs();
    checkPersistentAnimationWindows();
    checkGazeDirectionsAndMotionZero();
    checkReducedMotionAmplitude();
    benchmarkEyes();
    checkCoverageAndFacePlacement();
    checkExplicitNeutralOverridesRestingMood();
    checkBodylessStateGlyphs();
    checkLateUptimeHasNoPhantomReaction();
    checkWaitingAndSleepyStayDistinct();
    checkExpressionAndAnimationRange();
    checkTransitionsMotionAndSparseFrames();
    checkMotionStaysInCanvas();
    const char* outDir = (argc > 1) ? argv[1] : ".";
    std::string tiny = std::string(outDir) + "/moods-40.svg";
    std::string small = std::string(outDir) + "/moods-72.svg";
    std::string large = std::string(outDir) + "/moods-200.svg";
    std::string expressions = std::string(outDir) + "/expressions.svg";
    std::string animations = std::string(outDir) + "/animations.svg";
    std::string player = std::string(outDir) + "/animation-player.html";
    if (!writeGazeSheet((std::string(outDir) + "/gaze-directions.svg").c_str()) ||
        !writeSheet(tiny.c_str(), 40) || !writeSheet(small.c_str(), 72) ||
        !writeSheet(large.c_str(), 200) ||
        !writePickerSheet(expressions.c_str(), kExpressions, renderExpression) ||
        !writePickerSheet((std::string(outDir) + "/expressions-40.svg").c_str(), kExpressions, renderExpression, 40) ||
        !writePickerSheet(animations.c_str(), kAnimations,
            [](const AnimationCase& item, int size) { return renderAnimation(item, size); }) ||
        !writeAnimationPlayer(player.c_str()) ||
        !writeTransitionSheet((std::string(outDir) + "/transitions.svg").c_str())) {
        std::cerr << "could not write preview sheets\n";
        return 1;
    }
    std::cout << tiny << "\n" << small << "\n" << large << "\n"
              << expressions << "\n" << animations << "\n" << player << "\n";
    return 0;
}
