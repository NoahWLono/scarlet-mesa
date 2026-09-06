#include "audio.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace scarlet {
namespace {
constexpr int Rate = 22050;
constexpr double Tau = 6.283185307179586;

std::vector<float> loadMusic(const std::string& path) {
    SDL_AudioSpec spec{};
    Uint8* data = nullptr;
    Uint32 bytes = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &data, &bytes)) {
        std::cerr << "Audio: " << path << ": " << SDL_GetError() << '\n';
        return {};
    }
    SDL_AudioCVT cvt{};
    const int result = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                                       AUDIO_F32SYS, 2, Rate);
    if (result < 0 || bytes > 100000000u) {
        SDL_FreeWAV(data);
        return {};
    }
    std::vector<Uint8> converted(static_cast<std::size_t>(bytes) * cvt.len_mult);
    std::memcpy(converted.data(), data, bytes);
    SDL_FreeWAV(data);
    cvt.buf = converted.data();
    cvt.len = static_cast<int>(bytes);
    if (SDL_ConvertAudio(&cvt) < 0) return {};
    const int length = result == 0 ? static_cast<int>(bytes) : cvt.len_cvt;
    std::vector<float> samples(static_cast<std::size_t>(length) / sizeof(float));
    std::memcpy(samples.data(), converted.data(), samples.size() * sizeof(float));
    if (samples.size() % 2 != 0) samples.pop_back();
    return samples;
}

std::vector<float> synthEffect(int kind) {
    const std::array<double, 5> durations{{0.055, 0.24, 1.25, 0.78, 0.095}};
    std::vector<float> samples(static_cast<std::size_t>(durations[kind] * Rate));
    uint32_t noise = 0x19F31u;
    double phase = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / Rate;
        const double progress = t / durations[kind];
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        const double n = static_cast<double>(noise & 65535u) / 32768.0 - 1.0;
        double wave = 0.0;
        if (kind == 0) {
            phase += Tau * (1450.0 - 700.0 * progress) / Rate;
            wave = std::sin(phase) * 0.16;
        } else if (kind == 1) {
            phase += Tau * (230.0 - 170.0 * progress) / Rate;
            wave = (n * 0.50 + std::sin(phase) * 0.25) * 0.42;
        } else if (kind == 2) {
            phase += Tau * (90.0 - 55.0 * progress) / Rate;
            wave = (std::sin(phase) * 0.65 + n * 0.27) * 0.54;
        } else if (kind == 3) {
            const int step = std::min(7, static_cast<int>(t * 11));
            const std::array<int, 8> notes{{0, 4, 7, 12, 16, 19, 24, 24}};
            phase += Tau * 523.25 * std::pow(2.0, notes[step] / 12.0) / Rate;
            wave = (std::sin(phase) + 0.2 * std::sin(phase * 2.0)) * 0.25;
        } else {
            phase += Tau * (2400.0 + 1700.0 * progress) / Rate;
            wave = std::sin(phase) * 0.115;
        }
        const double attack = std::min(1.0, t / 0.003);
        const double envelope = attack * std::pow(1.0 - progress, kind == 2 ? 1.4 : 2.0);
        samples[i] = static_cast<float>(wave * envelope);
    }
    return samples;
}
}

struct Audio::Impl {
    struct Voice { int sound = 0; std::size_t cursor = 0; bool active = false; };
    SDL_AudioDeviceID device = 0;
    bool subsystem = false;
    bool muted = false;
    std::array<std::vector<float>, 3> tracks;
    std::array<std::vector<float>, 5> effects;
    std::array<Voice, 24> voices{};
    int track = -1, oldTrack = -1;
    std::size_t cursor = 0, oldCursor = 0;
    float fade = 1.0f;
    float gain = 0.0f;

    static void callback(void* userdata, Uint8* stream, int bytes) {
        auto& self = *static_cast<Impl*>(userdata);
        std::memset(stream, 0, static_cast<std::size_t>(bytes));
        auto* output = reinterpret_cast<float*>(stream);
        const int frames = bytes / (2 * static_cast<int>(sizeof(float)));
        for (int frame = 0; frame < frames; ++frame) {
            float left = 0.0f, right = 0.0f;
            if (self.track >= 0 && !self.tracks[self.track].empty()) {
                const auto& current = self.tracks[self.track];
                left = current[self.cursor] * self.fade * 0.44f;
                right = current[self.cursor + 1] * self.fade * 0.44f;
                self.cursor = (self.cursor + 2) % current.size();
            }
            if (self.oldTrack >= 0 && !self.tracks[self.oldTrack].empty()) {
                const auto& previous = self.tracks[self.oldTrack];
                left += previous[self.oldCursor] * (1.0f - self.fade) * 0.44f;
                right += previous[self.oldCursor + 1] * (1.0f - self.fade) * 0.44f;
                self.oldCursor = (self.oldCursor + 2) % previous.size();
            }
            self.fade = std::min(1.0f, self.fade + 1.0f / (Rate * 0.7f));
            if (self.fade >= 1.0f) self.oldTrack = -1;
            for (auto& voice : self.voices) {
                if (!voice.active) continue;
                const auto& sound = self.effects[voice.sound];
                if (voice.cursor >= sound.size()) { voice.active = false; continue; }
                const float sample = sound[voice.cursor++];
                left += sample;
                right += sample;
            }
            const float target = self.muted ? 0.0f : 1.0f;
            self.gain += std::clamp(target - self.gain, -0.001f, 0.001f);
            output[frame * 2] = std::clamp(left * self.gain, -0.95f, 0.95f);
            output[frame * 2 + 1] = std::clamp(right * self.gain, -0.95f, 0.95f);
        }
    }
};

Audio::Audio() : impl(std::make_unique<Impl>()) {}
Audio::~Audio() { shutdown(); }

bool Audio::init(const std::string& assetDir) {
    shutdown();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return false;
    impl->subsystem = true;
    for (int i = 0; i < 3; ++i)
        impl->tracks[i] = loadMusic(assetDir + "/stage" + std::to_string(i + 1) + ".wav");
    for (int i = 0; i < 5; ++i) impl->effects[i] = synthEffect(i);
    SDL_AudioSpec wanted{};
    wanted.freq = Rate;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 2;
    wanted.samples = 512;
    wanted.callback = Impl::callback;
    wanted.userdata = impl.get();
    // SDL converts to the hardware format when it differs from this fixed format.
    impl->device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
    if (impl->device == 0) {
        std::cerr << "Audio unavailable: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    SDL_PauseAudioDevice(impl->device, 0);
    return true;
}

void Audio::music(int stage) {
    if (!impl->device) return;
    stage = std::clamp(stage, 0, 2);
    SDL_LockAudioDevice(impl->device);
    if (stage != impl->track) {
        impl->oldTrack = impl->track;
        impl->oldCursor = impl->cursor;
        impl->track = stage;
        impl->cursor = 0;
        impl->fade = 0.0f;
    }
    SDL_UnlockAudioDevice(impl->device);
}

void Audio::effect(int kind) {
    if (!impl->device || kind < 0 || kind >= 5) return;
    SDL_LockAudioDevice(impl->device);
    // Bound the callback cost and prevent held fire from masking music or hits.
    int same = 0;
    for (const auto& voice : impl->voices) if (voice.active && voice.sound == kind) ++same;
    if (same < (kind == 0 || kind == 4 ? 2 : 4)) {
        for (auto& voice : impl->voices) {
            if (!voice.active) { voice = {kind, 0, true}; break; }
        }
    }
    SDL_UnlockAudioDevice(impl->device);
}

void Audio::setMuted(bool muted) {
    if (impl->device) SDL_LockAudioDevice(impl->device);
    impl->muted = muted;
    if (impl->device) SDL_UnlockAudioDevice(impl->device);
}

void Audio::shutdown() {
    if (impl->device) {
        SDL_CloseAudioDevice(impl->device);
        impl->device = 0;
    }
    impl->track = impl->oldTrack = -1;
    impl->cursor = impl->oldCursor = 0;
    impl->gain = 0.0f;
    impl->voices.fill({});
    for (auto& track : impl->tracks) track.clear();
    for (auto& effect : impl->effects) effect.clear();
    if (impl->subsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        impl->subsystem = false;
    }
}
}
