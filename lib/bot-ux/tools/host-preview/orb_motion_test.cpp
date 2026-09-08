#include <BotUx.h>

#include <assert.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static uint32_t gNow = 1000;
uint32_t millis() { return gNow; }

using Bot = botux::BotUx;

struct Raster {
    uint64_t hash;
    int ink;
    int components;
    int left, top, right, bottom;
};

static uint16_t bgColor() { return botux::rgb565(0x0A, 0x0E, 0x14); }

static Raster inspect(const M5Canvas& canvas, int size) {
    Raster result = {1469598103934665603ULL, 0, 0, size, size, -1, -1};
    std::vector<uint8_t> seen((size_t)size * size, 0);
    std::vector<int> queue((size_t)size * size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint16_t pixel = canvas.readPixel(x, y);
            result.hash ^= pixel;
            result.hash *= 1099511628211ULL;
            if (pixel == bgColor()) continue;
            ++result.ink;
            if (x < result.left) result.left = x;
            if (x > result.right) result.right = x;
            if (y < result.top) result.top = y;
            if (y > result.bottom) result.bottom = y;
        }
    }
    for (int start = 0; start < size * size; ++start) {
        int sx = start % size, sy = start / size;
        if (seen[start] || canvas.readPixel(sx, sy) == bgColor()) continue;
        ++result.components;
        int head = 0, tail = 0;
        queue[tail++] = start;
        seen[start] = 1;
        while (head < tail) {
            int pos = queue[head++], x = pos % size, y = pos / size;
            static const int dx[] = {-1, 1, 0, 0};
            static const int dy[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                int next = ny * size + nx;
                if (seen[next] || canvas.readPixel(nx, ny) == bgColor()) continue;
                seen[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    return result;
}

static Raster render(Bot::Mood mood, uint32_t offsetMs, float speed = 1.0f,
                     float amount = 1.0f, bool reduced = false) {
    const int size = 200;
    gNow = 1000;
    M5Canvas canvas(size, size);
    canvas.setRecording(false);
    Bot bot;
    Bot::Style style;
    style.eyeColor = botux::rgb565(0xF8, 0x10, 0xB0);
    bot.begin(&canvas);
    bot.setStyle(style);
    bot.setMood(mood, 0);
    bot.setAnimationSpeed(speed);
    bot.setMotionAmount(amount);
    bot.setReducedMotion(reduced);
    gNow += offsetMs;
    bot.update(gNow);
    bot.draw();
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            assert(canvas.readPixel(x, y) != style.eyeColor);
    return inspect(canvas, size);
}

static void renderAtSize(M5Canvas& canvas, Bot::Mood mood, uint32_t offsetMs,
                         int size, float amount = 1.0f,
                         Bot::Animation animation = Bot::Animation::Auto,
                         Bot::Expression expression = Bot::Expression::Auto) {
    gNow = 1000;
    canvas.setRecording(false);
    Bot bot;
    Bot::Style style;
    style.eyeColor = botux::rgb565(0xF8, 0x10, 0xB0);
    bot.begin(&canvas);
    bot.setStyle(style);
    bot.setMood(mood, 0);
    bot.setExpression(expression, 0);
    bot.setAnimation(animation);
    bot.setMotionAmount(amount);
    bot.update(1000 + offsetMs);
    bot.draw();
    assert(!canvas.outOfBounds());
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            assert(canvas.readPixel(x, y) != style.eyeColor);
}

static int changedAtSize(Bot::Mood mood, int size, uint32_t firstMs,
                         uint32_t secondMs, float amount = 1.0f) {
    M5Canvas first(size, size), second(size, size);
    renderAtSize(first, mood, firstMs, size, amount);
    renderAtSize(second, mood, secondMs, size, amount);
    int changed = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            changed += first.readPixel(x, y) != second.readPixel(x, y);
    return changed;
}

static void checkLoop(Bot::Mood mood, uint32_t period, int size) {
    M5Canvas start(size, size), end(size, size);
    renderAtSize(start, mood, 0, size);
    renderAtSize(end, mood, period, size);
    assert(inspect(start, size).hash == inspect(end, size).hash);

    int ordinaryMax = 0;
    for (int sample = 0; sample < 24; ++sample) {
        uint32_t time = (uint32_t)((uint64_t)period * sample / 24u);
        int changed = changedAtSize(mood, size, time, time + 16u);
        if (changed > ordinaryMax) ordinaryMax = changed;
    }
    int wrap = changedAtSize(mood, size, period - 8u, period + 8u);
    assert(ordinaryMax > 0);
    assert(wrap <= ordinaryMax * 5 / 4 + 8);

    // Vortex points fade at both poles, so re-entry stays populated without
    // a frame-wide flash or an empty shell around the exact phase wrap.
    if (mood == Bot::Mood::Working) {
        M5Canvas before(size, size), after(size, size);
        renderAtSize(before, mood, period - 32u, size);
        renderAtSize(after, mood, period + 32u, size);
        Raster beforeStats = inspect(before, size);
        Raster afterStats = inspect(after, size);
        assert(beforeStats.ink > size / 2 && afterStats.ink > size / 2);
    }
}

static void writePpm(const std::string& path, Bot::Mood mood, uint32_t time,
                     int size = 200) {
    M5Canvas canvas(size, size);
    renderAtSize(canvas, mood, time, size);
    std::ofstream out(path.c_str(), std::ios::binary);
    assert(out);
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint16_t c = canvas.readPixel(x, y);
            unsigned char rgb[3] = {
                (unsigned char)(((c >> 11) & 31u) * 255u / 31u),
                (unsigned char)(((c >> 5) & 63u) * 255u / 63u),
                (unsigned char)((c & 31u) * 255u / 31u)
            };
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
}

static int changedPixels(Bot::Mood mood, uint32_t firstMs, uint32_t secondMs,
                         float amount, bool reduced) {
    const int size = 200;
    M5Canvas first(size, size), second(size, size);
    first.setRecording(false); second.setRecording(false);
    Bot a, b;
    gNow = 1000; a.begin(&first); b.begin(&second);
    a.setMood(mood, 0); b.setMood(mood, 0);
    a.setMotionAmount(amount); b.setMotionAmount(amount);
    a.setReducedMotion(reduced); b.setReducedMotion(reduced);
    a.update(1000 + firstMs); b.update(1000 + secondMs);
    a.draw(); b.draw();
    int changed = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            changed += first.readPixel(x, y) != second.readPixel(x, y);
    return changed;
}

int main(int argc, char** argv) {
    Raster thinking = render(Bot::Mood::Thinking, 0);
    Raster working = render(Bot::Mood::Working, 0);
    assert(thinking.ink > 180 && thinking.ink < 3000);
    assert(working.ink > 120 && working.ink < 3000);
    assert(thinking.components > 20);
    assert(working.components > 20);
    assert(thinking.right - thinking.left > 85 && thinking.bottom - thinking.top > 85);
    assert(working.right - working.left > 85 && working.bottom - working.top > 85);
    assert(thinking.hash != working.hash);

    // Speed remains an exact scalar of phase for both reference loops.
    assert(render(Bot::Mood::Thinking, 900, 1.0f).hash ==
           render(Bot::Mood::Thinking, 450, 2.0f).hash);
    assert(render(Bot::Mood::Working, 1100, 1.0f).hash ==
           render(Bot::Mood::Working, 550, 2.0f).hash);

    // Zero freezes every point. Small amounts and reduced motion both calm the
    // entire choreography instead of leaving a full-speed shell rotation.
    assert(changedPixels(Bot::Mood::Thinking, 0, 1400, 0.0f, false) == 0);
    assert(changedPixels(Bot::Mood::Working, 0, 1400, 0.0f, false) == 0);
    int thinkingFull = changedPixels(Bot::Mood::Thinking, 0, 180, 1.0f, false);
    int thinkingLow = changedPixels(Bot::Mood::Thinking, 0, 180, 0.10f, false);
    int thinkingReduced = changedPixels(Bot::Mood::Thinking, 0, 180, 1.0f, true);
    int workingFull = changedPixels(Bot::Mood::Working, 0, 180, 1.0f, false);
    int workingLow = changedPixels(Bot::Mood::Working, 0, 180, 0.10f, false);
    int workingReduced = changedPixels(Bot::Mood::Working, 0, 180, 1.0f, true);
    assert(thinkingFull > thinkingReduced && thinkingReduced > thinkingLow);
    assert(workingFull > workingReduced && workingReduced > workingLow);
    assert(thinkingLow > 0 && workingLow > 0);

    static const int sizes[] = {40, 72, 178, 286};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        checkLoop(Bot::Mood::Thinking, 3600, sizes[i]);
        checkLoop(Bot::Mood::Working, 4400, sizes[i]);
    }

    // Explicit expressions cannot restore eyes over either shell. Explicit
    // animations retain their documented whole-character translation layer.
    M5Canvas thinkingAuto(178, 178), thinkingAlarmed(178, 178);
    renderAtSize(thinkingAuto, Bot::Mood::Thinking, 700, 178);
    renderAtSize(thinkingAlarmed, Bot::Mood::Thinking, 700, 178, 1.0f,
                 Bot::Animation::Auto, Bot::Expression::Alarmed);
    assert(inspect(thinkingAuto, 178).hash == inspect(thinkingAlarmed, 178).hash);
    M5Canvas workingAuto(178, 178), workingCurious(178, 178);
    renderAtSize(workingAuto, Bot::Mood::Working, 700, 178);
    renderAtSize(workingCurious, Bot::Mood::Working, 700, 178, 1.0f,
                 Bot::Animation::Curious);
    assert(inspect(workingAuto, 178).hash != inspect(workingCurious, 178).hash);

    if (argc > 1) {
        std::string directory = argv[1];
        writePpm(directory + "/thinking-orbs.ppm", Bot::Mood::Thinking, 700);
        writePpm(directory + "/working-vortex.ppm", Bot::Mood::Working, 700);
        writePpm(directory + "/thinking-orbs-286.ppm", Bot::Mood::Thinking, 700, 286);
        writePpm(directory + "/working-vortex-286.ppm", Bot::Mood::Working, 700, 286);
    }

    std::cout << "Orb motion: Thinking " << thinking.ink << " px/"
              << thinking.components << " clusters, Working " << working.ink
              << " px/" << working.components << " clusters; 180ms deltas full/reduced/0.1 = "
              << thinkingFull << "/" << thinkingReduced << "/" << thinkingLow << " and "
              << workingFull << "/" << workingReduced << "/" << workingLow << "\n";
}
