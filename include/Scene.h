#pragma once

#include "UI.h"
#include <string>

// Every algorithm/operation in the app is *recorded* as a list of frames up
// front, then played back. Playback only tracks which frame is showing, so the
// same controls (play, pause, step, scrub, speed) work for every scene.
struct Playback {
    int count = 0;
    int cur = 0;
    bool playing = false;
    float speedT = 0.3f; // 0..1 slider position, mapped exponentially to steps/sec
    float acc = 0.0f;

    float StepsPerSecond() const;
    void Load(int frames, bool autoplay);
    void Clear() { count = 0; cur = 0; playing = false; acc = 0; }
    void Update(float dt);
    void Step(int delta);
    void JumpEnd() { if (count > 0) cur = count - 1; playing = false; }
    bool Active() const { return count > 0; }
    bool AtEnd() const { return count == 0 || cur >= count - 1; }
};

void DrawPlaybackBar(Rectangle r, Playback& pb);

class Scene {
public:
    virtual ~Scene() = default;
    virtual std::string Name() const = 0;

    // Left-hand control panel (inputs, buttons, explanations).
    virtual void Controls(UI::Layout& L) = 0;

    // Main visualisation area. dt is used for smooth motion.
    virtual void Canvas(Rectangle r, float dt) = 0;

    virtual Playback* GetPlayback() { return nullptr; }

    // One-line narration of what the current frame is doing.
    virtual std::string Status() const { return ""; }
};
