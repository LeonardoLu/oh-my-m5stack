#include "WatchButtonFeedback.h"

#include <assert.h>

int main() {
    using namespace watchbuttons;

    Feedback feedback;
    assert(feedback.held() == 0);
    assert(!feedback.sample(false, false, true, false, 90));

    assert(feedback.sample(true, false, true, false, 100));
    assert(feedback.isHeld(A));
    assert(!feedback.isHeld(B));
    assert(!feedback.isHeld(Power));
    assert(feedback.expandStep(A) == 0);
    assert(!feedback.sample(true, false, true, false, 100));
    assert(feedback.advance(160));
    assert(feedback.expandStep(A) == 3);
    assert(feedback.advance(260));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(!feedback.advance(270));

    assert(feedback.sample(true, true, true, true, 280));
    assert(feedback.held() == (A | B | Power));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 0);
    assert(feedback.expandStep(Power) == 0);
    assert(feedback.advance(340));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 3);
    assert(feedback.expandStep(Power) == 3);

    // A failed PMIC read means unknown, so power feedback disappears instead
    // of getting stuck while the independently sampled GPIO buttons remain.
    assert(feedback.sample(true, true, false, true, 340));
    assert(feedback.held() == (A | B));
    assert(!feedback.isHeld(Power));

    assert(feedback.sample(false, false, true, false, 341));
    assert(feedback.held() == 0);

    Blob a = restingBlob(A), b = restingBlob(B), power = restingBlob(Power);
    assert(a.centerDegrees == 220 && a.halfDegrees == 18 && a.depth == 24 && a.color == 0xFFE0);
    assert(b.centerDegrees == 320 && b.halfDegrees == 18 && b.depth == 24 && b.color == 0x34BF);
    assert(power.centerDegrees == 135 && power.halfDegrees == 15 && power.depth == 20 && power.color == 0xF800);
    assert(blobInset(a, a.centerDegrees - a.halfDegrees) == 0.0f);
    assert(blobInset(a, a.centerDegrees + a.halfDegrees) == 0.0f);
    assert(blobInset(a, a.centerDegrees) == a.depth);
    assert(blobInset(a, a.centerDegrees - 7) == blobInset(a, a.centerDegrees + 7));
    assert(expandedBlob(A, 0).halfDegrees == 2 && expandedBlob(A, 0).depth == 8);
    assert(expandedBlob(A, 5).halfDegrees > a.halfDegrees);
    assert(expandedBlob(A, 5).depth > a.depth);
    assert(expandedBlob(A, Feedback::ExpandSteps).halfDegrees == a.halfDegrees);
    assert(expandedBlob(Power, Feedback::ExpandSteps).depth == power.depth);
    const Button buttons[] = {A, B, Power};
    for (Button button : buttons) {
        for (uint8_t step = 0; step <= Feedback::ExpandSteps; ++step) {
            Blob blob = expandedBlob(button, step);
            float prior = 0.0f;
            for (int16_t delta = -blob.halfDegrees; delta <= 0; ++delta) {
                float inset = blobInset(blob, blob.centerDegrees + delta);
                assert(inset >= prior && inset <= blob.depth);
                assert(inset == blobInset(blob, blob.centerDegrees - delta));
                prior = inset;
            }
        }
    }
    return 0;
}
