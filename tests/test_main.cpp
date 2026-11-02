#include "dsp/PianoEngine.h"
#include <algorithm>

#include <array>
#include <cmath>
#include <iostream>
#include <span>

int main() {
    int failures=0;
    auto check=[&](bool ok, const char* name){ if(!ok){std::cerr<<"FAIL "<<name<<"\n";++failures;} };
    aether::PianoEngine engine;
    engine.prepare(48000.0, 8);
    std::array<float,8> l, r;
    l.fill(1.0f); r.fill(-1.0f);
    engine.process(l.data(),r.data(),8);
    check(std::all_of(l.begin(),l.end(),[](float x){return x==0.0f;}),"left zero output");
    check(std::all_of(r.begin(),r.end(),[](float x){return x==0.0f;}),"right zero output");
    engine.process(l.data(),r.data(),0);
    check(engine.validEvents({},0),"zero block");
    const std::array<aether::MidiEvent,2> good{{{0,0x90,60,90},{7,0x80,60,0}}};
    const std::array<aether::MidiEvent,2> unsorted{{{7,0x90,60,90},{0,0x80,60,0}}};
    const std::array<aether::MidiEvent,1> outOfRange{{{8,0x90,60,90}}};
    check(engine.validEvents(good,8),"valid sample offsets");
    check(!engine.validEvents(unsorted,8),"reject unsorted offsets");
    check(!engine.validEvents(outOfRange,8),"reject offset at block end");
    check(!engine.validEvents({},9),"reject oversized block");
    engine.reset(); engine.prepare(44100.0,16); engine.reset();
    std::array<float,16> l2{},r2{}; engine.process(l2.data(),r2.data(),16);
    check(std::all_of(l2.begin(),l2.end(),[](float x){return std::isfinite(x)&&x==0.0f;}),"repeated prepare/reset");
    return failures ? 1 : 0;
}
