#pragma once

#include <memory>
#include <string>

namespace scarlet {
// Control from the game thread. The SDL callback is synchronized internally.
class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    bool init(const std::string& assetDir);
    void music(int stage);
    // 0: shot, 1: hit, 2: bomb, 3: spell clear, 4: graze.
    void effect(int kind);
    void setMuted(bool muted);
    void shutdown();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
