#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

inline constexpr int EqualizerGainLimit = 6;
using EqualizerGains = std::array<int, 3>;

class AudioEqualizer {
    struct Filter {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        struct State { float z1 = 0, z2 = 0; };
        std::array<State, 8> states{};
        bool active = false;

        float process(float input, size_t channel) {
            if (!active) return input;
            auto& state = states[channel];
            const float output = b0 * input + state.z1;
            state.z1 = b1 * input - a1 * output + state.z2;
            state.z2 = b2 * input - a2 * output;
            return output;
        }
    };
    // One atomic snapshot for the UI-to-audio handoff. Filter state belongs to the mixer thread.
    std::atomic<uint32_t> requested{0};
    uint32_t applied = 0;
    int frequency = 0;
    size_t channels = 0;
    float headroom = 1;
    std::array<Filter, 3> filters{};

    void configure(uint32_t packed) {
        constexpr std::array<float, 3> centers{120, 1000, 8000};
        int boost = 0;
        for (size_t i = 0; i < filters.size(); ++i) {
            auto& filter = filters[i];
            const auto gain = static_cast<int>((packed >> (4 * i)) & 15) - EqualizerGainLimit;
            const bool active = gain != 0 && centers[i] < frequency * 0.5f;
            if (active != filter.active) filter.states = {};
            filter.active = active;
            if (!active) continue;
            boost += std::max(0, gain);
            const float omega = 6.283185307f * centers[i] / frequency;
            const float alpha = std::sin(omega) / (2 * 0.65f);
            const float amplitude = std::pow(10.0f, gain / 40.0f);
            const float a0 = 1 + alpha / amplitude;
            filter.b0 = (1 + alpha * amplitude) / a0;
            filter.b1 = filter.a1 = -2 * std::cos(omega) / a0;
            filter.b2 = (1 - alpha * amplitude) / a0;
            filter.a2 = (1 - alpha / amplitude) / a0;
        }
        headroom = std::pow(10.0f, -boost / 20.0f);
    }

public:
    AudioEqualizer() { setGains({}); }

    // Called before registering the mixer callback.
    void setFormat(int rate, size_t channelCount) {
        frequency = rate;
        channels = channelCount > 0 && channelCount <= 8 ? channelCount : 0;
        filters = {};
        applied = 0;
    }

    void setGains(const EqualizerGains& gains) {
        uint32_t packed = 0;
        for (size_t i = 0; i < gains.size(); ++i)
            packed |= static_cast<uint32_t>(std::clamp(gains[i], -EqualizerGainLimit, EqualizerGainLimit)
                + EqualizerGainLimit) << (i * 4);
        requested.store(packed, std::memory_order_relaxed);
    }

    void process(int16_t* pcm, size_t samples) {
        if (frequency <= 0 || !channels) return;
        const auto gains = requested.load(std::memory_order_relaxed);
        if (gains != applied) {
            configure(gains);
            applied = gains;
        }
        if (!filters[0].active && !filters[1].active && !filters[2].active) return;
        const auto complete = samples - samples % channels;
        for (size_t frame = 0; frame < complete; frame += channels) {
            for (size_t channel = 0; channel < channels; ++channel) {
                float value = pcm[frame + channel] * headroom;
                for (auto& filter : filters) value = filter.process(value, channel);
                pcm[frame + channel] = static_cast<int16_t>(std::round(std::clamp(value, -32768.0f, 32767.0f)));
            }
        }
    }
};
