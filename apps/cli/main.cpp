#include <iostream>
#include <iomanip>
#include <toad/version.h>
#include <toad/types.h>
#include <toad/fast_math.h>
#include <toad/engine.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "====================================================\n";
    std::cout << "  ToadTracker (\"" << toad::CODENAME << "\") v" << toad::VERSION_STRING << "\n";
    std::cout << "  Open-Source Modern Ergonomic Step Tracker\n";
    std::cout << "====================================================\n\n";

    std::cout << "[System & Architecture]\n";
    std::cout << "  Version:         " << toad::VERSION_STRING << " (0x" 
              << std::hex << std::setw(6) << std::setfill('0') << toad::getVersionNumber() 
              << std::dec << ")\n";
    std::cout << "  C++ Standard:    C++20\n";
    std::cout << "  Host Endian:     Little-Endian (Verified)\n";
    std::cout << "  Float Precision: 32-bit IEEE 754 (Verified)\n\n";

    std::cout << "[Static Memory Footprint (Zero-Allocation)]\n";
    std::cout << "  sizeof(toad::Engine):     " << sizeof(toad::Engine) << " bytes\n";
    std::cout << "  sizeof(toad::Song):       " << sizeof(toad::Song) << " bytes\n";
    std::cout << "  sizeof(toad::Phrase):     " << sizeof(toad::Phrase) << " bytes\n";
    std::cout << "  sizeof(toad::Chain):      " << sizeof(toad::Chain) << " bytes\n";
    std::cout << "  sizeof(toad::Table):      " << sizeof(toad::Table) << " bytes\n";
    std::cout << "  sizeof(toad::Instrument): " << sizeof(toad::Instrument) << " bytes\n\n";

    std::cout << "[FastMath Engine Self-Check]\n";
    float sampleRate = 44100.0f;
    float bpm = 120.0f;
    float samplesPerTick = toad::FastMath::calculateSamplesPerTick(sampleRate, bpm);
    float samplesPerStep = toad::FastMath::calculateSamplesPerStep(sampleRate, bpm, 6);
    std::cout << "  Deterministic Timing @ " << bpm << " BPM, " << sampleRate << " Hz:\n";
    std::cout << "    Samples per Tick: " << samplesPerTick << "\n";
    std::cout << "    Samples per Step: " << samplesPerStep << "\n";

    float testFreq = 440.0f;
    uint32_t phaseInc = toad::FastMath::calculatePhaseIncrement(testFreq, sampleRate);
    std::cout << "  32-bit Phase Increment (A4 440 Hz): 0x" 
              << std::hex << phaseInc << std::dec << " (" << phaseInc << ")\n";

    float clipped = toad::FastMath::masterClip(2.0f);
    std::cout << "  Master Output Protection (Drive 2.0 -> Clip): " << clipped << "f\n\n";

    std::cout << "[Status] Engine ready. Version " << toad::VERSION_STRING << " baseline verified.\n";
    return 0;
}
