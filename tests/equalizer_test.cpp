#include "equalizer.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    constexpr int rate = 48000;
    auto tone = [](int frequency) {
        std::vector<int16_t> samples(rate * 2);
        for (int i = 0; i < rate; ++i) {
            samples[i * 2] = std::lround(12000 * std::sin(6.283185307 * frequency * i / rate));
        }
        return samples;
    };
    auto energy = [](const std::vector<int16_t>& samples) {
        double sum = 0;
        for (size_t i = samples.size() / 2; i < samples.size(); i += 2) {
            assert(samples[i + 1] == 0);
            sum += double(samples[i]) * samples[i];
        }
        return sum;
    };

    AudioEqualizer eq;
    eq.setFormat(rate, 2);
    constexpr std::array<int, 3> frequencies{120, 1000, 8000};
    auto flat = tone(1000);
    const auto raw = flat;
    eq.process(flat.data(), flat.size());
    assert(flat == raw);

    for (size_t band = 0; band < frequencies.size(); ++band) {
        EqualizerGains gain{};
        gain[band] = -6;
        eq.setGains(gain);
        auto samples = tone(frequencies[band]);
        const auto originalEnergy = energy(samples);
        eq.process(samples.data(), samples.size());
        const double ratio = energy(samples) / originalEnergy;
        assert(ratio > 0.24 && ratio < 0.27);
    }

    eq.setGains({6, 0, 0});
    auto bass = tone(120), treble = tone(8000);
    eq.process(bass.data(), bass.size());
    eq.process(treble.data(), treble.size());
    assert(energy(bass) > energy(treble) * 3.5);
    eq.setGains({});
    auto reset = raw;
    eq.process(reset.data(), reset.size());
    assert(reset == raw);
    std::cout << "Equalizer band response, stereo isolation and flat bypass passed\n";
}
