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
    assert(feedback.advance(220));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(!feedback.advance(240));

    assert(feedback.sample(true, true, true, true, 240));
    assert(feedback.held() == (A | B | Power));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 0);
    assert(feedback.expandStep(Power) == 0);
    assert(feedback.advance(300));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 3);
    assert(feedback.expandStep(Power) == 3);

    // A failed PMIC read means unknown, so power feedback disappears instead
    // of getting stuck while the independently sampled GPIO buttons remain.
    assert(feedback.sample(true, true, false, true, 300));
    assert(feedback.held() == (A | B));
    assert(!feedback.isHeld(Power));

    assert(feedback.sample(false, false, true, false, 301));
    assert(feedback.held() == 0);

    Arc a = arc(A), b = arc(B), power = arc(Power);
    assert(a.startDegrees == 205 && a.endDegrees == 235 && a.color == 0xFFE0);
    assert(b.startDegrees == 305 && b.endDegrees == 335 && b.color == 0x34BF);
    assert(power.startDegrees == 78 && power.endDegrees == 102 && power.color == 0xF800);
    assert(a.endDegrees - a.startDegrees == b.endDegrees - b.startDegrees);
    return 0;
}
