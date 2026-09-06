#include "WatchEdgeGeometry.h"
#include <assert.h>
#include <math.h>

using namespace watchedge;

int main() {
    assert(displaySize()==466&&displayCenter()==233&&displayRadius()==233);
    for(int y=-1;y<=466;++y) {
        auto span=displayRowSpan((int16_t)y);
        bool valid=y>=0&&y<displaySize();
        assert((span.width>0)==valid);
        for(int x=-1;x<=466;++x) {
            int dx=x-displayCenter(),dy=y-displayCenter();
            bool expected=x>=0&&x<displaySize()&&valid
                &&dx*dx+dy*dy<=displayRadius()*displayRadius();
            assert(displayContains(x,y)==expected);
            assert((valid&&x>=span.x&&x<span.x+span.width)==expected);
        }
    }

    assert(displayRowSpan(0).x==233&&displayRowSpan(0).width==1);
    assert(displayRowSpan(21).x==137&&displayRowSpan(21).width==193);
    assert(batteryToothLocalRowSpan(21).x==137);
    assert(batteryToothLocalRowSpan(21).width==193);
    assert(batteryToothLocalRowSpan(22).x==137);
    assert(batteryToothLocalRowSpan(22).width==193);
    assert(batteryToothLocalRowSpan(62).width==193);
    assert(batteryToothLocalRowSpan(63).width==193);
    assert(batteryToothLocalRowSpan(82).x==151);
    assert(batteryToothLocalRowSpan(82).width==165);
    assert(batteryToothLocalRowSpan(83).x==157);
    assert(batteryToothLocalRowSpan(83).width==153);
    assert(batteryToothLocalRowSpan(84).width==0);

    int priorWidth=0;
    for(int y=0;y<=batteryToothBottomY();++y) {
        auto span=batteryToothLocalRowSpan((int16_t)y);
        assert(span.width>0&&span.x+span.width-1==2*displayCenter()-span.x);
        if(y<=batteryToothShoulderY()) {
            auto circle=displayRowSpan((int16_t)y);
            assert(span.x==circle.x&&span.width==circle.width);
        } else if(y<batteryToothRoundStartY()) {
            assert(span.x==batteryToothBodyLeft());
            assert(span.width==batteryToothBodyRight()-batteryToothBodyLeft()+1);
        } else {
            assert(span.width<=priorWidth);
        }
        priorWidth=span.width;
    }

    assert(batteryToothOffset(0)==-84);
    assert(batteryToothOffset(1)==0);
    assert(batteryToothOffset(-1)==-84);
    assert(batteryToothOffset(2)==0);
    int16_t priorOffset=-85;
    for(int i=0;i<=100;++i) {
        int16_t offset=batteryToothOffset(i/100.0f);
        assert(offset>=priorOffset&&offset>=-84&&offset<=0);
        priorOffset=offset;
        for(int y=0;y<hudHeight();++y) {
            auto span=batteryToothRowSpan((int16_t)y,offset);
            auto local=batteryToothLocalRowSpan((int16_t)(y-offset));
            auto circle=displayRowSpan((int16_t)y);
            for(int x=0;x<displaySize();++x) {
                bool expected=local.width>0&&circle.width>0
                    &&x>=local.x&&x<local.x+local.width
                    &&x>=circle.x&&x<circle.x+circle.width;
                assert((span.width>0&&x>=span.x&&x<span.x+span.width)==expected);
                assert(batteryToothContains(x,y,offset)==expected);
            }
        }
    }
    assert(!batteryToothContains(233,0,batteryToothOffset(0)));
    assert(batteryToothContains(233,0,batteryToothOffset(1)));
    assert(batteryToothContains(233,82,batteryToothOffset(1)));
    assert(batteryToothContains(233,83,batteryToothOffset(1)));
    assert(!batteryToothContains(233,84,batteryToothOffset(1)));

    // The native text and cell live in the straight body. Rendering clips all
    // antialiasing to the current animated tooth before it reaches the HUD.
    assert(batteryPercentX()>=batteryToothBodyLeft());
    assert(batteryPercentY()>batteryToothShoulderY());
    assert(batteryGaugeX()+68<=batteryToothBodyRight());
    assert(batteryGaugeY()+21<batteryToothRoundStartY());

    for(int pct=0;pct<=100;++pct) {
        auto band=batteryBand((uint8_t)pct);
        assert(band==(pct<=10?BatteryBand::Red:pct<=30?BatteryBand::Yellow:BatteryBand::Green));
        float fill=batteryGaugeFill((uint8_t)pct);
        assert(fill>=0&&fill<=38);
        if(pct) assert(fill>batteryGaugeFill((uint8_t)(pct-1)));
    }
    assert(fabsf(batteryGaugeFill(255)-38.0f)<0.001f);
    assert(batteryBandRgb(BatteryBand::Green)==0x42D978u);
    assert(batteryBandRgb(BatteryBand::Yellow)==0xF2C94Cu);
    assert(batteryBandRgb(BatteryBand::Red)==0xFF5B57u);
    assert(batteryInkRgb()==0x101820u);
    assert(batteryChargeShade(0)==8&&batteryChargeShade(800)==48);
    assert(batteryChargeShade(1600)==8&&batteryChargeShade(2400)==48);
}
