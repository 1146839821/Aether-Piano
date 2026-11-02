#include "dsp/PianoEngine.h"
#include "dsp/string/FractionalDelay.h"
#include "dsp/string/DispersionFilter.h"
#include "dsp/string/HammerContact.h"
#include "dsp/string/LossFilter.h"
#include "dsp/string/WaveguideString.h"
#include "model/NoteParameters.h"
#include <algorithm>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<std::uint64_t> allocationCount{0};
}

void* operator new(std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

template <typename Processor>
double measureSinePhase(Processor&& processor, double omega) {
    double ys=0.0,yc=0.0,ss=0.0,cc=0.0,sc=0.0;
    for(int n=0;n<120000;++n) {
        const double sine=std::sin(omega*n), cosine=std::cos(omega*n);
        const double output=processor(sine);
        if(n>1000) {
            ys+=output*sine; yc+=output*cosine;
            ss+=sine*sine; cc+=cosine*cosine; sc+=sine*cosine;
        }
    }
    const double det=ss*cc-sc*sc;
    const double a=(ys*cc-yc*sc)/det;
    const double b=(yc*ss-ys*sc)/det;
    return std::atan2(b,a);
}

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
    aether::string::FractionalDelay fractionalDelay;
    check(fractionalDelay.prepare(6.25),"prepare fractional delay");
    double sineProjection=0.0, cosineProjection=0.0;
    double sineEnergy=0.0, cosineEnergy=0.0, crossEnergy=0.0;
    constexpr double probeOmega=0.001;
    for (int n=0;n<120000;++n) {
        const double input=std::sin(probeOmega*n);
        const double output=fractionalDelay.process(input);
        if(n>1000) {
            const double sine=std::sin(probeOmega*n);
            const double cosine=std::cos(probeOmega*n);
            sineProjection += output*sine;
            cosineProjection += output*cosine;
            sineEnergy += sine*sine;
            cosineEnergy += cosine*cosine;
            crossEnergy += sine*cosine;
        }
    }
    const double determinant=sineEnergy*cosineEnergy-crossEnergy*crossEnergy;
    const double fittedSine=(sineProjection*cosineEnergy-cosineProjection*crossEnergy)/determinant;
    const double fittedCosine=(cosineProjection*sineEnergy-sineProjection*crossEnergy)/determinant;
    const double measuredPhase=std::atan2(fittedCosine,fittedSine);
    const double measuredDelay=-measuredPhase/probeOmega;
    check(std::abs(measuredDelay-6.25)<0.1,"fractional delay low-frequency phase");
    constexpr double tunedOmega=2.0*3.14159265358979323846*261.625565/48000.0;
    aether::string::FractionalDelay longDelay;
    check(longDelay.prepare(149.8695401994492),"prepare tuned long fractional delay");
    const double longDelayPhase=measureSinePhase(
        [&](double input){return longDelay.process(input);},tunedOmega);
    check(std::abs(std::remainder(longDelayPhase-longDelay.phaseAt(tunedOmega),
                                  2.0*3.14159265358979323846))<1e-5,
          "fractional-delay phase agrees with implementation at C4");
    aether::string::LossFilter lossFilter;
    constexpr double lossFundamental=261.625565;
    constexpr double lossSampleRate=48000.0;
    constexpr double lossT60=210.0;
    check(lossFilter.prepare(lossSampleRate,lossFundamental,lossT60,0.08),"prepare passive loss filter");
    const double omega0=6.2831853071795864769*lossFundamental/lossSampleRate;
    const double lossMeasuredPhase=measureSinePhase(
        [&](double input){return lossFilter.process(input);},omega0);
    check(std::abs(std::remainder(lossMeasuredPhase-lossFilter.phaseAt(omega0),
                                  2.0*3.14159265358979323846))<1e-5,
          "loss-filter phase agrees with implementation at C4");
    for (const double omega : {0.02,0.2,0.6,1.5,2.7,omega0}) {
        lossFilter.reset();
        double ys=0.0, yc=0.0, ss=0.0, cc=0.0, sc=0.0;
        for(int n=0;n<12000;++n) {
            const double sine=std::sin(omega*n), cosine=std::cos(omega*n);
            const double output=lossFilter.process(sine);
            if(n>4000) {
                ys+=output*sine; yc+=output*cosine;
                ss+=sine*sine; cc+=cosine*cosine; sc+=sine*cosine;
            }
        }
        const double det=ss*cc-sc*sc;
        const double a=(ys*cc-yc*sc)/det;
        const double b=(yc*ss-ys*sc)/det;
        const double measuredGain=std::hypot(a,b);
        check(measuredGain<=1.0+1e-6,"loss filter is passive across frequency");
        if(std::abs(omega-omega0)<1e-10) {
            const double target=std::pow(10.0,-3.0/(lossFundamental*lossT60));
            check(std::abs(measuredGain-target)<1e-6,"loss filter fundamental T60 calibration");
        }
    }
    aether::string::DispersionFilter stiffDispersion, idealDispersion;
    check(stiffDispersion.prepare(48000.0,261.625565,0.0003),"prepare stiff-string dispersion");
    check(idealDispersion.prepare(48000.0,261.625565,0.0),"prepare ideal-string dispersion");
    check(stiffDispersion.isStable(),"dispersion poles inside unit circle");
    const double dispersionMeasuredPhase=measureSinePhase(
        [&](double input){return stiffDispersion.process(input);},tunedOmega);
    check(std::abs(std::remainder(dispersionMeasuredPhase-stiffDispersion.phaseAt(tunedOmega),
                                  2.0*3.14159265358979323846))<1e-5,
          "dispersion-filter phase agrees with implementation at C4");
    check(std::abs(stiffDispersion.magnitudeAt(1.7)-1.0)<1e-12,"dispersion allpass magnitude");
    check(std::abs(stiffDispersion.phaseAt(0.7)-idealDispersion.phaseAt(0.7))>1e-4,
          "stiff dispersion changes modal phase");
    for (const double omega : {0.1,0.7,1.9,2.8}) {
        stiffDispersion.reset();
        double ys=0.0,yc=0.0,ss=0.0,cc=0.0,sc=0.0;
        for(int n=0;n<12000;++n) {
            const double sine=std::sin(omega*n), cosine=std::cos(omega*n);
            const double output=stiffDispersion.process(sine);
            if(n>4000) { ys+=output*sine; yc+=output*cosine; ss+=sine*sine; cc+=cosine*cosine; sc+=sine*cosine; }
        }
        const double det=ss*cc-sc*sc;
        const double a=(ys*cc-yc*sc)/det, b=(yc*ss-ys*sc)/det;
        check(std::abs(std::hypot(a,b)-1.0)<1e-6,"dispersion output preserves sine magnitude");
    }
    bool dispersionBoundariesPrepare = true;
    for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        for (const std::uint8_t midiNote : {std::uint8_t{21}, std::uint8_t{108}}) {
            aether::model::NoteParameters parameters{};
            aether::string::DispersionFilter boundaryFilter;
            const bool prepared = aether::model::makeNoteParameters(midiNote, {}, parameters)
                && boundaryFilter.prepare(rate, parameters.string.fundamentalHz,
                                          parameters.string.inharmonicityB);
            check(prepared, "dispersion prepares both pitch endpoints at every supported rate");
            dispersionBoundariesPrepare = dispersionBoundariesPrepare && prepared;
            if (!prepared)
                continue;

            const double omega = 2.0 * 3.14159265358979323846
                * parameters.string.fundamentalHz / rate;
            const double delta = omega * 1.0e-4;
            const double groupDelay = -(boundaryFilter.phaseAt(omega + delta)
                - boundaryFilter.phaseAt(omega - delta)) / (2.0 * delta);
            const double coefficient = boundaryFilter.coefficient();
            const double expectedGroupDelay = 2.0 * (1.0 - coefficient * coefficient)
                / (1.0 + coefficient * coefficient + 2.0 * coefficient * std::cos(omega));
            check(boundaryFilter.isStable() && std::abs(coefficient) < 1.0,
                  "dispersion endpoint poles remain inside the unit circle");
            check(std::isfinite(boundaryFilter.phaseAt(omega))
                  && std::isfinite(groupDelay) && groupDelay > 0.0
                  && std::abs(groupDelay - expectedGroupDelay)
                       < 1.0e-4 * expectedGroupDelay,
                  "dispersion endpoint group delay matches allpass response");
            for (const double probeOmega : {omega, 0.7, 1.7, 2.8})
                check(std::abs(boundaryFilter.magnitudeAt(probeOmega) - 1.0) < 1.0e-12,
                      "dispersion endpoint frequency response remains allpass");
        }
    }
    if (dispersionBoundariesPrepare) {
        aether::model::NoteParameters lowestNote{};
        const bool lowestPrepared = aether::model::makeNoteParameters(21, {}, lowestNote);
        aether::string::DispersionFilter lowestNoteFilter;
        const bool filterPrepared = lowestPrepared && lowestNoteFilter.prepare(
            192000.0, lowestNote.string.fundamentalHz, lowestNote.string.inharmonicityB);
        check(filterPrepared, "prepare A0 dispersion frequency-response measurement");
        const double omega = 2.0 * 3.14159265358979323846
            * lowestNote.string.fundamentalHz / 192000.0;
        double ys = 0.0, yc = 0.0, ss = 0.0, cc = 0.0, sc = 0.0;
        for (int sample = 0; sample < 120000; ++sample) {
            const double input = std::sin(omega * sample);
            const double output = lowestNoteFilter.process(input);
            if (sample > 20000) {
                const double cosine = std::cos(omega * sample);
                ys += output * input;
                yc += output * cosine;
                ss += input * input;
                cc += cosine * cosine;
                sc += input * cosine;
            }
        }
        const double determinant = ss * cc - sc * sc;
        const double inPhase = (ys * cc - yc * sc) / determinant;
        const double quadrature = (yc * ss - ys * sc) / determinant;
        check(std::abs(std::hypot(inPhase, quadrature) - 1.0) < 1.0e-5,
              "A0 dispersion boundary preserves measured sinusoid magnitude at 192 kHz");
    }
    aether::string::HammerContact hammerContact;
    constexpr double hammerMass=0.008, hammerSpeed=0.8, characteristicImpedance=2.11;
    constexpr double contactSampleRate=48000.0, contactDt=1.0/contactSampleRate;
    check(hammerContact.prepare(contactSampleRate,hammerMass,8.0e9,2.5),"prepare nonlinear hammer contact");
    check(hammerContact.trigger(hammerSpeed,0.0),"trigger nonlinear hammer contact");
    double stringPosition=0.0, radiatedEnergy=0.0;
    for(int n=0;n<10000 && hammerContact.isInContact();++n) {
        const auto step=hammerContact.process(stringPosition,0.0,characteristicImpedance);
        check(std::isfinite(step.force_N) && step.force_N>=0.0,"finite nonnegative hammer force");
        stringPosition+=step.stringVelocity_m_s*contactDt;
        radiatedEnergy+=step.force_N*step.stringVelocity_m_s*contactDt;
    }
    const double hammerMomentum=hammerMass*(hammerSpeed-hammerContact.velocity_m_s());
    check(!hammerContact.isInContact(),"hammer separates in finite time");
    check(hammerContact.peakForce_N()>0.0,"hammer contact produces force");
    check(std::abs(hammerMomentum-hammerContact.impulse_N_s())<1e-6,"hammer impulse equals momentum change");
    const double finalEnergy=0.5*hammerMass*std::pow(hammerContact.velocity_m_s(),2)+radiatedEnergy;
    check(std::abs(finalEnergy-0.5*hammerMass*hammerSpeed*hammerSpeed)<5e-5,
          "contact energy flows from hammer into string impedance");
    for (const double sampleRate : {44100.0,48000.0,96000.0}) {
        aether::string::WaveguideString waveguide;
        aether::string::StringParameters parameters;
        check(waveguide.prepare(sampleRate,parameters),"prepare C4 waveguide at supported sample rate");
        check(waveguide.noteOn(0.8),"start C4 waveguide hammer");
        double peak=0.0;
        bool finite=true;
        for(int n=0;n<24000;++n) {
            const double output=waveguide.processSample();
            finite=finite && std::isfinite(output);
            peak=std::max(peak,std::abs(output));
        }
        check(finite,"waveguide output remains finite");
        check(peak>1e-7,"waveguide produces a physical string response");
        waveguide.noteOff();
        for(int n=0;n<12000;++n)
            finite=finite && std::isfinite(waveguide.processSample());
        check(finite,"waveguide release remains finite");
    }
    aether::PianoEngine offsetEngine, segmentedEngine;
    offsetEngine.prepare(48000.0, 512);
    segmentedEngine.prepare(48000.0, 512);
    std::array<float,512> offsetLeft{}, offsetRight{}, segmentedLeft{}, segmentedRight{};
    const std::array<aether::MidiEvent,1> offsetNote{{{173,0x90,60,80}}};
    offsetEngine.process(offsetLeft.data(),offsetRight.data(),512,offsetNote);
    segmentedEngine.process(segmentedLeft.data(),segmentedRight.data(),173);
    segmentedEngine.handleEvent({0,0x90,60,80});
    segmentedEngine.process(segmentedLeft.data()+173,segmentedRight.data()+173,512-173);
    check(std::equal(offsetLeft.begin(),offsetLeft.end(),segmentedLeft.begin()),
          "MIDI sample offset is authoritative");
    check(offsetEngine.diagnostics().noteOnCount==1,"engine handles C4 MIDI note-on");
    check(offsetEngine.hammerSpeedForVelocity(80)>1.0
          && offsetEngine.hammerSpeedForVelocity(80)<1.1,"velocity maps to bounded hammer speed");

    aether::PianoEngine repeatedEngine, freshEngine;
    repeatedEngine.prepare(48000.0,4096);
    freshEngine.prepare(48000.0,4096);
    std::array<float,4096> repeatLeft{}, repeatRight{}, freshLeft{}, freshRight{};
    repeatedEngine.handleEvent({0,0x90,60,80});
    repeatedEngine.process(repeatLeft.data(),repeatRight.data(),3000);
    repeatedEngine.handleEvent({0,0x90,60,80});
    freshEngine.handleEvent({0,0x90,60,80});
    repeatedEngine.process(repeatLeft.data(),repeatRight.data(),2048);
    freshEngine.process(freshLeft.data(),freshRight.data(),2048);
    double retriggerDifference=0.0;
    for (int i=0;i<2048;++i)
        retriggerDifference=std::max(retriggerDifference,
                                     std::abs(double(repeatLeft[i])-freshLeft[i]));
    check(retriggerDifference>1.0e-6,"retrigger preserves old string wave state");
    check(repeatedEngine.diagnostics().noteOnCount==2,"retrigger starts a new hammer interaction");

    aether::PianoEngine extremeEngine;
    extremeEngine.prepare(48000.0,512);
    std::array<float,512> extremeLeft{},extremeRight{};
    const std::array<aether::MidiEvent,2> rapidNotes{{
        {0,0x90,60,1},{1,0x90,60,127}}};
    extremeEngine.process(extremeLeft.data(),extremeRight.data(),512,rapidNotes);
    for(int block=0;block<8;++block)
        extremeEngine.process(extremeLeft.data(),extremeRight.data(),512);
    bool extremesFinite=true;
    for(float sample:extremeLeft)
        extremesFinite=extremesFinite && std::isfinite(sample);
    check(extremesFinite,"weak and strong rapid retrigger stay finite");
    check(extremeEngine.diagnostics().noteOnCount==2,"one-sample repeated C4 is accepted");

    aether::PianoEngine dampedEngine, ringingEngine;
    dampedEngine.prepare(48000.0,512);
    ringingEngine.prepare(48000.0,512);
    dampedEngine.handleEvent({0,0x90,60,80});
    ringingEngine.handleEvent({0,0x90,60,80});
    double dampedEnergy=0.0,ringingEnergy=0.0;
    for(std::uint32_t done=0;done<48000;done+=512) {
        if(done==12288)
            dampedEngine.handleEvent({0,0x80,60,0});
        dampedEngine.process(extremeLeft.data(),extremeRight.data(),512);
        ringingEngine.process(repeatLeft.data(),repeatRight.data(),512);
        if(done>=36000) {
            for(int i=0;i<512;++i) {
                dampedEnergy+=double(extremeLeft[i])*extremeLeft[i];
                ringingEnergy+=double(repeatLeft[i])*repeatLeft[i];
            }
        }
    }
    check(dampedEnergy<ringingEnergy*0.1,"MIDI note-off engages the string damper");

    aether::PianoEngine realtimeEngine;
    realtimeEngine.prepare(48000.0,512);
    std::array<float,512> realtimeLeft{}, realtimeRight{};
    realtimeEngine.handleEvent({0,0x90,60,80});
    const auto allocationsBefore=allocationCount.load(std::memory_order_relaxed);
    countAllocations.store(true,std::memory_order_relaxed);
    for(int block=0;block<64;++block) {
        realtimeEngine.process(realtimeLeft.data(),realtimeRight.data(),512);
        if(block==20) realtimeEngine.handleEvent({0,0x80,60,0});
        if(block==30) realtimeEngine.handleEvent({0,0x90,60,96});
    }
    countAllocations.store(false,std::memory_order_relaxed);
    check(allocationCount.load(std::memory_order_relaxed)==allocationsBefore,
          "audio processing and MIDI handling allocate zero times");
    engine.reset(); engine.prepare(44100.0,16); engine.reset();
    std::array<float,16> l2{},r2{}; engine.process(l2.data(),r2.data(),16);
    check(std::all_of(l2.begin(),l2.end(),[](float x){return std::isfinite(x)&&x==0.0f;}),"repeated prepare/reset");
    return failures ? 1 : 0;
}
