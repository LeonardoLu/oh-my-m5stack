#include "UxPointer.h"
#include <assert.h>

int main() {
    ux::PointerSession p;
    ux::Rect button{244,374,138,50};
    // Slow/long button holds and 12 px fingertip drift remain valid.
    p.begin(2,button,310,396,100,false);
    p.move(322,408); assert(p.pressed());
    assert(p.end(322,408)==2); assert(!p.pressed());
    p.begin(2,button,250,380,0xFFFFFFF0u,false);
    p.move(240,380); assert(p.pressed()); // six-pixel release slop
    assert(p.end(240,380)==2);
    // Captured target cannot become an adjacent button; leaving cancels.
    p.begin(2,button,300,399,0,false); p.move(170,399); p.move(300,399);
    assert(!p.pressed()); assert(p.end(300,399)==-1);
    ux::Rect row{62,76,342,58};
    p.begin(100,row,200,100,0,true);
    p.move(212,112); assert(!p.scrolling()); assert(p.pressed());
    assert(p.end(212,112)==100);
    p.begin(100,row,200,100,0,true); p.move(202,86);
    assert(p.scrolling()); assert(!p.pressed()); p.move(200,100);
    assert(p.end(200,100)==-1); // a drag returning to origin never clicks
    p.begin(100,row,200,100,0,true);
    p.move(225,102); assert(!p.scrolling()); assert(!p.pressed());
    assert(p.end(200,100)==-1);
    p.begin(-1,{0,0,0,0},200,140,0,true); p.move(200,110);
    assert(p.scrolling()); assert(p.end(200,110)==-1);
    p.begin(100,row,200,100,0,true); p.cancel();
    assert(p.end(200,100)==-1);
    // Promotion and final release use the same exact coordinates and offset.
    ux::ScrollModel scroll; scroll.setBounds(720,288); scroll.setOffset(100);
    p.begin(100,row,200,100,100,true); p.move(200,70);
    scroll.begin(p.startY(),p.startedAt()); scroll.move(p.y(),140);
    assert(scroll.offset()==130);
    p.move(200,60); scroll.move(p.y(),160);
    assert(p.end(200,60)==-1); assert(!scroll.end(160)); assert(scroll.offset()==140);
    assert(scroll.update(16));
    // A fresh press stops inertia before the visible target is captured.
    scroll.cancel(); float stopped=scroll.offset(); assert(!scroll.update(50)); assert(scroll.offset()==stopped);
}
