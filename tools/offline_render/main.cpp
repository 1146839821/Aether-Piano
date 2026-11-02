#include <algorithm>

#include "dsp/PianoEngine.h"
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void u16(std::ostream& o, std::uint16_t v) { o.put(char(v)); o.put(char(v >> 8)); }
static void u32(std::ostream& o, std::uint32_t v) { for (int i=0;i<4;++i) o.put(char(v >> (8*i))); }
int main(int argc, char** argv) {
    std::string path = "render.wav";
    double seconds = 1.0;
    for (int i=1;i<argc;++i) {
        const std::string arg = argv[i];
        if (arg == "--output" && i+1<argc) path = argv[++i];
        else if (arg == "--seconds" && i+1<argc) seconds = std::stod(argv[++i]);
        else { std::cerr << "Usage: offline_render [--output file.wav] [--seconds duration]\n"; return 2; }
    }
    constexpr std::uint32_t rate=48000, channels=2, bytes=4;
    if (!(seconds > 0.0 && seconds <= 3600.0)) { std::cerr << "duration must be in (0, 3600]\n"; return 2; }
    const auto frames = static_cast<std::uint32_t>(seconds * rate);
    if (frames == 0 || std::uint64_t(frames)*channels*bytes > UINT32_MAX-36) return 2;
    aether::PianoEngine engine; engine.prepare(rate, 512);
    std::vector<float> left(512), right(512);
    std::ofstream out(path, std::ios::binary);
    if (!out) { std::cerr << "cannot open output\n"; return 1; }
    out.write("RIFF",4); u32(out,36+frames*channels*bytes); out.write("WAVEfmt ",8);
    u32(out,16); u16(out,3); u16(out,channels); u32(out,rate); u32(out,rate*channels*bytes);
    u16(out,channels*bytes); u16(out,8*bytes); out.write("data",4); u32(out,frames*channels*bytes);
    for (std::uint32_t done=0; done<frames;) {
        const auto n = std::min<std::uint32_t>(512, frames-done);
        engine.process(left.data(), right.data(), n);
        for (std::uint32_t i=0;i<n;++i) { u32(out,std::bit_cast<std::uint32_t>(left[i])); u32(out,std::bit_cast<std::uint32_t>(right[i])); }
        done += n;
    }
    if (!out) { std::cerr << "write failed\n"; return 1; }
    std::cout << "sample_rate=" << rate << " channels=2 bits=32 format=float samples_per_channel=" << frames << " output=" << path << "\n";
}
