#include "WatchControls.h"
#include "UxPointer.h"
#include <assert.h>
#include <initializer_list>
using namespace watchcontrols;
int main() {
    // Every non-face screen shares the exact same single Done target.
    for(int e=(int)Editor::Time;e<=(int)Editor::Gaze;++e) {
        assert(at(Screen::Editor,(Editor)e,0,233,399).id==Done);
        assert(at(Screen::Editor,(Editor)e,0,154,351).id==None);
    }
    for(auto screen:{Screen::Settings,Screen::Personalize}) for(int off=0;off<500;++off) {
        assert(at(screen,Editor::None,off,233,399).id==Done);
    }
    auto done=doneBounds();
    assert(roundedContains(done,doneRadius(),233,399));
    assert(!roundedContains(done,doneRadius(),done.x,done.y));
    assert(!roundedContains(done,doneRadius(),done.x+done.w-1,done.y));
    assert(done.x+done.w/2==233&&done.y+done.h/2==399);
    assert(done.h==96&&doneRadius()==done.h/2);
    // The complete rounded control remains inside the 466 px round display.
    for(int y=done.y;y<done.y+done.h;++y) for(int x=done.x;x<done.x+done.w;++x) {
        if(!roundedContains(done,doneRadius(),x,y)) continue;
        int dx=x-233,dy=y-233;
        assert(dx*dx+dy*dy<=233*233);
    }
    // Menu hit geometry follows arbitrary sub-row offsets, including partial rows.
    for(int off=0;off<=132;++off) {
        for(int y=76;y<340;++y) {
            auto t=at(Screen::Settings,Editor::None,off,233,y);
            if(t.id>=MenuRow) {
                int row=t.id-MenuRow, top=80+row*66-off;
                assert(y>=top && y<top+58);
                assert(t.bounds.contains(233,y));
            }
        }
    }
    assert(at(Screen::Settings,Editor::None,37,233,75).id==None);
    assert(at(Screen::Settings,Editor::None,37,233,340).id==None);
    // Bottom rows can be tapped after scrolling fully into view; clipped rows
    // cannot be released through chrome outside the viewport.
    assert(at(Screen::Settings,Editor::None,132,233,321).id==MenuRow+(int)MenuItem::Layout);
    int personalMax=(int)PersonalItem::Back*66-264;
    assert(at(Screen::Personalize,Editor::None,personalMax,233,321).id==PersonalRow+(int)PersonalItem::Speed);
    assert(at(Screen::Settings,Editor::None,37,233,345).id==None);
    // Preview editors show two 48 px steps with unchanged 44 px controls.
    assert(editorRowCount(Editor::Motion)==4&&previewList().visibleRows==2);
    assert(at(Screen::Editor,Editor::Motion,0,233,266).id==First);
    assert(at(Screen::Editor,Editor::Motion,96,233,314).id==First+3);
    assert(at(Screen::Editor,Editor::Preview,48,233,314).id==First+2);
    // Shared keyboard draw/hit rectangles, including gap rejection.
    ux::Rect area=nameKeyboardBounds();
    for(int k=0;k<30;++k) {
        auto r=ux::nameKeyRect(area,k);
        assert(at(Screen::Editor,Editor::Name,0,r.x+r.w/2,r.y+r.h/2).id==NameKey+k);
        assert(r.h>=38);
    }
    assert(area.y+area.h==340);
    assert(at(Screen::Editor,Editor::Name,0,area.x,area.y).id==None);
    assert(at(Screen::Editor,Editor::Color,0,160,290).id==ColorPad);
    assert(at(Screen::Editor,Editor::Color,0,360,290).id==HueBar);
    assert(colorPadBounds().y+colorPadBounds().h==338);
    assert(hueBarBounds().y+hueBarBounds().h==338);
    assert(at(Screen::Editor,Editor::Color,0,160,338).id==None);
    for(uint8_t i=0;i<4;++i) {
        auto row=displayRowBounds(i);
        assert(row.y+row.h<=344);
        assert(at(Screen::Editor,Editor::Display,0,233,displayRowCenter(i)).id==First+i);
    }
    assert(at(Screen::Editor,Editor::Display,0,233,345).id==None);
    // Buttons accept up-inside through 1 s; page cancellation consumes the release.
    ux::PointerSession p; auto t=at(Screen::Editor,Editor::Format,0,233,399);
    p.begin(t.id,t.bounds,233,399,100,false); p.move(250,409);
    assert(p.end(250,409,1000)==Done);
    p.begin(t.id,t.bounds,233,399,100,false); p.cancel(); assert(p.end(233,399,300)==None);
}
