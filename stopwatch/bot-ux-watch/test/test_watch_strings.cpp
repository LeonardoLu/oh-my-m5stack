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
    assert(!strcmp(watchstrings::translate("Looking around",true),"到处看看"));
    assert(!strcmp(watchstrings::translate("BUTTON FX",true),"按下效果"));
    assert(!strcmp(watchstrings::translate("DIM TIMEOUT",true),"调暗延时"));
    assert(!strcmp(watchstrings::translate("SCREEN OFF",true),"息屏延时"));
    assert(!strcmp(watchstrings::translate("WAKE",true),"唤醒"));
    assert(!strcmp(watchstrings::translate("TOUCH + KEYS",true),"触摸 + 按键"));
    assert(!strcmp(watchstrings::translate("KEYS ONLY",true),"仅按键"));
    assert(!strcmp(watchstrings::translate("5 S",true),"5秒"));
    assert(!strcmp(watchstrings::translate("15 MIN",true),"15分钟"));
}
