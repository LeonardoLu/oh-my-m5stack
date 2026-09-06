#include "FeedbackLevel.h"

#include <assert.h>

int main()
{
    assert(corefeedback::normalizeLevel(0) == 0);
    assert(corefeedback::normalizeLevel(5) == 5);
    assert(corefeedback::normalizeLevel(6) == corefeedback::kDefaultLevel);
    assert(corefeedback::normalizeLevel(255) == corefeedback::kDefaultLevel);
    assert(corefeedback::synthVolume(0) == 0);
    assert(corefeedback::synthVolume(3) == 180);
    assert(corefeedback::synthVolume(5) == 255);
    for (uint8_t level = 1; level < corefeedback::kLevelCount; ++level)
        assert(corefeedback::synthVolume(level) > corefeedback::synthVolume(level - 1));
}
