#include "WatchControls.h"
#include "UxPointer.h"
#include <assert.h>
#include <initializer_list>
using namespace watchcontrols;
int main() {
    // Every editor shares exactly the same Back/Done targets.
    for(int e=(int)Editor::Time;e<=(int)Editor::Gaze;++e) {
        assert(at(Screen::Editor,(Editor)e,0,150,398).id==Back);
        assert(at(Screen::Editor,(Editor)e,0,310,398).id==Done);
    }
    for(auto screen:{Screen::Settings,Screen::Personalize}) for(int off=0;off<500;++off) {
        assert(at(screen,Editor::None,off,150,398).id==Back);
        assert(at(screen,Editor::None,off,310,398).id==Done);
    }
    // Menu hit geometry follows arbitrary sub-row offsets, including partial rows.
    for(int off=0;off<=144;++off) {
        for(int y=76;y<364;++y) {
            auto t=at(Screen::Settings,Editor::None,off,233,y);
            if(t.id>=MenuRow) {
                int row=t.id-MenuRow, top=76+row*72-off;
                assert(y>=top && y<top+58);
                assert(t.bounds.contains(233,y));
            }
        }
    }
    assert(at(Screen::Settings,Editor::None,37,233,75).id==None);
    assert(at(Screen::Settings,Editor::None,37,233,364).id==None);
    // Bottom rows can be tapped after scrolling fully into view.
    assert(at(Screen::Settings,Editor::None,144,233,321).id==MenuRow+(int)MenuItem::Layout);
    int personalMax=(int)PersonalItem::Back*72-288;
    assert(at(Screen::Personalize,Editor::None,personalMax,233,321).id==PersonalRow+(int)PersonalItem::Speed);
    // Shared keyboard draw/hit rectangles, including gap rejection.
    ux::Rect area{78,170,310,205};
    for(int k=0;k<30;++k) {
        auto r=ux::nameKeyRect(area,k);
        assert(at(Screen::Editor,Editor::Name,0,r.x+r.w/2,r.y+r.h/2).id==NameKey+k);
    }
    assert(at(Screen::Editor,Editor::Name,0,78,170).id==None);
    assert(at(Screen::Editor,Editor::Color,0,160,290).id==ColorPad);
    assert(at(Screen::Editor,Editor::Color,0,360,290).id==HueBar);
    // Buttons accept up-inside through 1 s; page cancellation consumes the release.
    ux::PointerSession p; auto t=at(Screen::Editor,Editor::Format,0,310,398);
    p.begin(t.id,t.bounds,310,398,100,false); p.move(320,409);
    assert(p.end(320,409,1000)==Done);
    p.begin(t.id,t.bounds,310,398,100,false); p.cancel(); assert(p.end(310,398,300)==None);
}
