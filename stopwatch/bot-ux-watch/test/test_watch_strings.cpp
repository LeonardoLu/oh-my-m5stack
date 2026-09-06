#include "WatchStrings.h"
#include <assert.h>
int main() {
    for(const auto& e:watchstrings::entries) {
        assert(strcmp(watchstrings::translate(e.en,true),e.en));
        assert(!strcmp(watchstrings::translate(e.en,false),e.en));
        assert(!strcmp(watchstrings::translate(e.en,true),e.zh));
    }
    assert(!strcmp(watchstrings::translate("Milo",true),"Milo"));
    assert(!strcmp(watchstrings::translate("Working",true),"工作"));
    assert(!strcmp(watchstrings::translate("BUTTON FX",true),"按下效果"));
}
