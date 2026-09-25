#pragma once

namespace dg {

constexpr float kMinEnvTimeS = 0.001f;
constexpr float kKillTimeS = 0.005f;

class Envelope
{
public:
    enum class Stage { Idle, Attack, Sustain, Release, Kill };

    void prepare(double sampleRate);
    void noteOn(float attackS);
    void noteOff(float releaseS);
    void kill();
    float process();

    Stage stage() const { return stage_; }
    bool isActive() const { return stage_ != Stage::Idle; }
    bool isReleasing() const { return stage_ == Stage::Release || stage_ == Stage::Kill; }
    float level() const { return level_; }

private:
    double sampleRate_ = 44100.0;
    Stage stage_ = Stage::Idle;
    float level_ = 0.0f;
    float step_ = 0.0f;
};

} // namespace dg
