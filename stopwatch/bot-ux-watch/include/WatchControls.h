#pragma once
#include <UxInput.h>
#include <UxKeyboard.h>
#include "WatchEdgeGeometry.h"

enum class Screen : uint8_t { Face, Settings, Personalize, Editor };
enum class Editor : uint8_t { None, Time, Date, Format, Expression, Appearance, Motion, Color, Display, Name, Preview, Layout, Language, Gaze };
enum class MenuItem : uint8_t { Time, Date, Format, Personalize, Display, Layout, Done, Count };
enum class PersonalItem : uint8_t { Expression, Action, Appearance, Color, Name, Language, Preview, Gaze, Intensity, Speed, Back, Count };

namespace watchcontrols {
enum Id { None=-1, Done=1, First=10, NameKey=40, MenuRow=100, PersonalRow=200, ColorPad=300, HueBar=301 };
struct Target { int id; ux::Rect bounds; int16_t radius; };
struct ListLayout { int16_t x,y,w,h,step,rowHeight,radius; uint8_t visibleRows; };
constexpr ListLayout mainList() { return {62,76,342,264,66,58,25,4}; }
constexpr ListLayout previewList() { return {58,242,350,96,48,44,17,2}; }
constexpr ux::Rect doneBounds() { return {0,406,466,60}; }
constexpr int16_t doneCircleCenterX() { return watchedge::displayCenter(); }
constexpr int16_t doneCircleCenterY() { return watchedge::displayCenter(); }
constexpr int16_t doneLabelY() { return 432; }
constexpr int16_t doneLabelX() { return doneCircleCenterX(); }
constexpr int16_t doneCircleRadius() { return watchedge::displayRadius(); }
constexpr ux::Rect nameKeyboardBounds() { return {78,126,310,214}; }
constexpr ux::Rect useThemeBounds() { return {163,207,140,32}; }
constexpr ux::Rect colorPadBounds() { return {66,242,260,96}; }
constexpr ux::Rect hueBarBounds() { return {344,242,56,96}; }
constexpr int16_t displayRowCenter(uint8_t index) {
    return index==0?130:index==1?194:index==2?258:322;
}
constexpr ux::Rect displayRowBounds(uint8_t index) {
    return {58,(int16_t)(displayRowCenter(index)-22),350,44};
}

inline bool doneContains(int x, int y) {
    const auto bounds=doneBounds();
    return bounds.contains(x,y)&&watchedge::displayContains(x,y);
}

// The visible footer is the same circular segment used for hit testing. Each
// returned row is {left, y, width, 1}, with a half-open horizontal interval.
inline ux::Rect doneRowSpan(int16_t y) {
    const auto bounds=doneBounds();
    if(y<bounds.y||y>=bounds.y+bounds.h) return {0,y,0,0};
    auto span=watchedge::displayRowSpan(y);
    return {span.x,span.y,span.width,1};
}

inline bool roundedContains(ux::Rect r, int16_t radius, int x, int y) {
    if (!r.contains(x,y)) return false;
    if (radius <= 0) return true;
    int16_t maxRadius=(r.w<r.h?r.w:r.h)/2;
    if(radius>maxRadius) radius=maxRadius;
    if(x>=r.x+radius&&x<r.x+r.w-radius) return true;
    if(y>=r.y+radius&&y<r.y+r.h-radius) return true;
    int32_t cx=x<r.x+radius?r.x+radius:r.x+r.w-radius-1;
    int32_t cy=y<r.y+radius?r.y+radius:r.y+r.h-radius-1;
    int32_t dx=x-cx,dy=y-cy;
    return dx*dx+dy*dy<=(int32_t)radius*radius;
}

inline bool editorUsesPreviewList(Editor editor) {
    return editor==Editor::Appearance||editor==Editor::Motion||editor==Editor::Preview||editor==Editor::Gaze;
}
inline uint8_t editorRowCount(Editor editor) {
    switch(editor) {
        case Editor::Appearance:return 2; case Editor::Motion:return 4;
        case Editor::Preview:return 3; case Editor::Gaze:return 1;
        default:return 0;
    }
}
inline ux::Rect rowBounds(ListLayout layout,uint8_t index,float offset) {
    int16_t top=layout.y+index*layout.step-(int16_t)offset+(layout.step-layout.rowHeight)/2;
    return {layout.x,top,layout.w,layout.rowHeight};
}
inline bool inViewport(ListLayout layout,int x,int y) {
    return x>=layout.x&&x<layout.x+layout.w&&y>=layout.y&&y<layout.y+layout.h;
}
inline Target at(Screen screen, Editor editor, float offset, int x, int y) {
    Target result{None,{0,0,0,0},0};
    auto match = [&](int id, ux::Rect bounds, int16_t radius=0) {
        if(result.id==None&&roundedContains(bounds,radius,x,y)) result={id,bounds,radius};
    };
    if (screen != Screen::Face) {
        if(doneContains(x,y)) return {Done,doneBounds(),0};
    }
    if (screen == Screen::Settings || screen == Screen::Personalize) {
        const auto layout=mainList();
        if(!inViewport(layout,x,y)) return result;
        int count = screen == Screen::Settings ? (int)MenuItem::Done : (int)PersonalItem::Back;
        for (int i=0; i<count; ++i) {
            auto bounds=rowBounds(layout,(uint8_t)i,offset);
            if(roundedContains(bounds,layout.radius,x,y))
                return {screen==Screen::Settings?MenuRow+i:PersonalRow+i,bounds,layout.radius};
        }
        return result;
    }
    if (screen != Screen::Editor) return result;

    if(editorUsesPreviewList(editor)) {
        const auto layout=previewList();
        if(!inViewport(layout,x,y)) return result;
        for(uint8_t i=0;i<editorRowCount(editor);++i) {
            auto bounds=rowBounds(layout,i,offset);
            if(roundedContains(bounds,layout.radius,x,y)) return {First+i,bounds,layout.radius};
        }
    } else if (editor==Editor::Name) {
        const auto area=nameKeyboardBounds();
        int key=ux::nameKeyAt(area,x,y);
        if (key>=0) {
            const auto rect=ux::nameKeyRect(area,key);
            match(NameKey+key,rect,6);
        }
    } else if (editor==Editor::Language) {
        match(First,{72,160,152,70},35); match(First+1,{242,160,152,70},35);
    } else if (editor==Editor::Layout) {
        const int rows[]={170,250};
        for(int i=0;i<2;++i) match(First+i,{58,rows[i]-22,350,44},17);
    } else if (editor==Editor::Color) {
        match(First,useThemeBounds(),16); match(ColorPad,colorPadBounds()); match(HueBar,hueBarBounds());
    } else if (editor==Editor::Time || editor==Editor::Date) {
        const int timeCenters[]={157,309}, dateCenters[]={108,233,358};
        const int* centers=editor==Editor::Time?timeCenters:dateCenters;
        int count=editor==Editor::Time?2:3, width=editor==Editor::Time?100:86;
        for(int i=0;i<count;++i) {
            match(First+i*3,{centers[i]-width/2,116,width,52},26);
            match(First+i*3+1,{centers[i]-width/2,256,width,52},26);
            match(First+i*3+2,{centers[i]-width/2-5,175,width+10,72});
        }
    } else if (editor==Editor::Format) {
        match(First,{78,128,146,76},38); match(First+1,{242,128,146,76},38); match(First+2,{100,242,266,58},29);
    } else if (editor==Editor::Expression) {
        match(First,{48,126,76,96},38); match(First+1,{342,126,76,96},38); match(First+2,{144,62,178,178},89);
    } else if (editor==Editor::Display) {
        for(uint8_t i=0;i<4;++i) match(First+i,displayRowBounds(i),17);
    }
    return result;
}
}
