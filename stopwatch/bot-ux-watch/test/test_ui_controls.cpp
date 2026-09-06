#include "WatchControls.h"
#include "UxPointer.h"
#include <assert.h>
#include <initializer_list>
using namespace watchcontrols;
int main() {
    // Every non-face screen shares the exact same single Done target.
    for(int e=(int)Editor::Time;e<=(int)Editor::Gaze;++e) {
        assert(at(Screen::Editor,(Editor)e,0,233,399).id==Done);
        assert(at(Screen::Editor,(Editor)e,0,154,376).id==None);
    }
    for(auto screen:{Screen::Settings,Screen::Personalize}) for(int off=0;off<500;++off) {
        assert(at(screen,Editor::None,off,233,399).id==Done);
    }
    auto done=doneBounds();
    assert(roundedContains(done,doneRadius(),233,399));
    assert(!roundedContains(done,doneRadius(),done.x,done.y));
    assert(!roundedContains(done,doneRadius(),done.x+done.w-1,done.y));
    // Menu hit geometry follows arbitrary sub-row offsets, including partial rows.
    for(int off=0;off<=144;++off) {
        for(int y=76;y<364;++y) {
            auto t=at(Screen::Settings,Editor::None,off,233,y);
            if(t.id>=MenuRow) {
                int row=t.id-MenuRow, top=83+row*72-off;
                assert(y>=top && y<top+58);
                assert(t.bounds.contains(233,y));
            }
        }
    }
    assert(at(Screen::Settings,Editor::None,37,233,75).id==None);
    assert(at(Screen::Settings,Editor::None,37,233,364).id==None);
    // Bottom rows can be tapped after scrolling fully into view; clipped rows
    // cannot be released through chrome outside the viewport.
    assert(at(Screen::Settings,Editor::None,144,233,321).id==MenuRow+(int)MenuItem::Layout);
    int personalMax=(int)PersonalItem::Back*72-288;
    assert(at(Screen::Personalize,Editor::None,personalMax,233,321).id==PersonalRow+(int)PersonalItem::Speed);
    assert(at(Screen::Settings,Editor::None,37,233,365).id==None);
    // Preview editors show two 60 px rows and scroll any remainder.
    assert(editorRowCount(Editor::Motion)==4&&previewList().visibleRows==2);
    assert(at(Screen::Editor,Editor::Motion,0,233,273).id==First);
    assert(at(Screen::Editor,Editor::Motion,120,233,333).id==First+3);
    assert(at(Screen::Editor,Editor::Preview,60,233,333).id==First+2);
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
    ux::PointerSession p; auto t=at(Screen::Editor,Editor::Format,0,233,399);
    p.begin(t.id,t.bounds,233,399,100,false); p.move(250,409);
    assert(p.end(250,409,1000)==Done);
    p.begin(t.id,t.bounds,233,399,100,false); p.cancel(); assert(p.end(233,399,300)==None);
}
