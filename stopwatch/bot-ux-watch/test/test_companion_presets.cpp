#include "CompanionPresets.h"
#include <assert.h>

int main() {
    watchcompanion::MoodDeck deck(13,0x12345678u);
    for(uint8_t mood=0;mood<13;++mood) assert(deck.next(mood)==(mood+1)%13);
    bool seen[13]={}; uint8_t current=0;
    for(int i=0;i<1024;++i) {
        uint8_t next=deck.random(current);
        assert(next<13&&next!=current); seen[next]=true; current=next;
    }
    for(bool value:seen) assert(value);
    const uint8_t expected[]={0,1,2,4,8,9,11};
    static_assert(watchcompanion::ambientMoodCount()==7,"safe ambient mood count");
    for(uint8_t i=0;i<watchcompanion::ambientMoodCount();++i)
        assert(watchcompanion::ambientMood(i)==expected[i]);
}
