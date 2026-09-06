#pragma once
#include <UxInput.h>
#include <UxKeyboard.h>

enum class Screen : uint8_t { Face, Settings, Personalize, Editor };
enum class Editor : uint8_t { None, Time, Date, Format, Expression, Appearance, Motion, Color, Display, Name, Preview, Layout, Language, Gaze };
enum class MenuItem : uint8_t { Time, Date, Format, Personalize, Display, Layout, Done, Count };
enum class PersonalItem : uint8_t { Expression, Action, Appearance, Color, Name, Language, Preview, Gaze, Intensity, Speed, Back, Count };

namespace watchcontrols {
enum Id { None=-1, Back=1, Done=2, First=10, NameKey=40, MenuRow=100, PersonalRow=200, ColorPad=300, HueBar=301 };
struct Target { int id; ux::Rect bounds; };
inline Target at(Screen screen, Editor editor, float offset, int x, int y) {
    Target result{None,{0,0,0,0}};
    auto match = [&](int id, ux::Rect bounds) {
        if (result.id == None && bounds.contains(x,y)) result = {id,bounds};
    };
    if (screen == Screen::Settings || screen == Screen::Personalize) {
        if (y < 76 || y >= 364) return result;
        int count = screen == Screen::Settings ? (int)MenuItem::Count : (int)PersonalItem::Count;
        for (int i=0; i<count; ++i) {
            int top=105+i*72-(int)offset-29;
            int bottom=top+58;
            if (top<76) top=76;
            if (bottom>364) bottom=364;
            if (bottom>top) match((screen==Screen::Settings?MenuRow:PersonalRow)+i,{62,top,342,bottom-top});
        }
        return result;
    }
    if (screen != Screen::Editor) return result;
    match(Back,{84,374,138,50}); match(Done,{244,374,138,50});
    if (editor==Editor::Name) {
        int key=ux::nameKeyAt({78,170,310,205},x,y);
        if (key>=0) {
            // Match the shared keyboard's exact rectangle, including its gaps.
            const auto rect=ux::nameKeyRect({78,170,310,205},key);
            match(NameKey+key,rect);
        }
    } else if (editor==Editor::Language) {
        match(First,{72,160,152,70}); match(First+1,{242,160,152,70});
    } else if (editor==Editor::Layout || editor==Editor::Preview || editor==Editor::Gaze) {
        const int rows[]={editor==Editor::Layout?170:250,editor==Editor::Layout?250:296,342};
        int count=editor==Editor::Layout?2:editor==Editor::Gaze?1:3;
        for(int i=0;i<count;++i) match(First+i,{58,rows[i]-22,350,44});
    } else if (editor==Editor::Color) {
        match(First,{163,207,140,34}); match(ColorPad,{66,246,260,108}); match(HueBar,{344,246,56,108});
    } else if (editor==Editor::Time || editor==Editor::Date) {
        const int timeCenters[]={157,309}, dateCenters[]={108,233,358};
        const int* centers=editor==Editor::Time?timeCenters:dateCenters;
        int count=editor==Editor::Time?2:3, width=editor==Editor::Time?100:86;
        for(int i=0;i<count;++i) {
            match(First+i*3,{centers[i]-width/2,115,width,54});
            match(First+i*3+1,{centers[i]-width/2,255,width,54});
            match(First+i*3+2,{centers[i]-width/2-5,175,width+10,72});
        }
    } else if (editor==Editor::Format) {
        match(First,{78,128,146,76}); match(First+1,{242,128,146,76}); match(First+2,{100,242,266,58});
    } else if (editor==Editor::Expression) {
        match(First,{48,126,76,96}); match(First+1,{342,126,76,96}); match(First+2,{144,62,178,178});
    } else if (editor==Editor::Appearance) {
        match(First,{58,255,350,46}); match(First+1,{58,317,350,46});
    } else if (editor==Editor::Motion) {
        const int rows[]={218,262,306,350};
        for(int i=0;i<4;++i) match(First+i,{58,rows[i]-22,350,44});
    } else if (editor==Editor::Display) {
        const int rows[]={170,250,330};
        for(int i=0;i<3;++i) match(First+i,{58,rows[i]-30,350,60});
    }
    return result;
}
}
