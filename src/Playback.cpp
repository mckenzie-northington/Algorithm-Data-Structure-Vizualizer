#include "Scene.h"
#include <cmath>

float Playback::StepsPerSecond() const {
    return 0.75f * std::pow(800.0f, speedT); // ~0.75 .. 600 steps/sec
}

void Playback::Load(int frames, bool autoplay) {
    count = frames;
    cur = 0;
    acc = 0;
    playing = autoplay && frames > 1;
}

void Playback::Update(float dt) {
    if (!playing) return;
    if (AtEnd()) { playing = false; return; }
    acc += dt * StepsPerSecond();
    while (acc >= 1.0f && !AtEnd()) {
        acc -= 1.0f;
        cur++;
    }
    if (AtEnd()) { playing = false; acc = 0; }
}

void Playback::Step(int delta) {
    playing = false;
    cur += delta;
    if (cur < 0) cur = 0;
    if (cur > count - 1) cur = count - 1;
}

void DrawPlaybackBar(Rectangle r, Playback& pb) {
    DrawRectangleRec(r, Theme::Panel);
    DrawLine((int)r.x, (int)r.y, (int)(r.x + r.width), (int)r.y, Theme::Border);

    bool has = pb.count > 1;
    float x = r.x + 16, y = r.y + 10, h = r.height - 20;

    if (UI::Button({x, y, 38, h}, "|<", has && pb.cur > 0)) { pb.cur = 0; pb.playing = false; }
    x += 44;
    if (UI::Button({x, y, 38, h}, "<", has && pb.cur > 0)) pb.Step(-1);
    x += 44;
    std::string play = pb.playing ? "Pause" : (pb.AtEnd() && has ? "Replay" : "Play");
    if (UI::Button({x, y, 76, h}, play, has, pb.playing)) {
        if (pb.playing) pb.playing = false;
        else {
            if (pb.AtEnd()) pb.cur = 0;
            pb.playing = true;
        }
    }
    x += 82;
    if (UI::Button({x, y, 38, h}, ">", has && !pb.AtEnd())) pb.Step(1);
    x += 44;
    if (UI::Button({x, y, 38, h}, ">|", has && !pb.AtEnd())) pb.JumpEnd();
    x += 56;

    // Speed control on the right, scrubber fills the middle.
    float speedW = 150;
    float right = r.x + r.width - 16;
    std::string spd = UI::Fmt("%.1f steps/s", pb.StepsPerSecond());
    if (pb.StepsPerSecond() >= 10) spd = UI::Fmt("%.0f steps/s", pb.StepsPerSecond());
    UI::Text(spd, right - 96, r.y + r.height / 2 - 9, 15, Theme::Muted);
    UI::Slider({right - 104 - speedW, y, speedW, h}, pb.speedT, 0.0f, 1.0f);
    UI::Text("Speed", right - 150 - speedW, r.y + r.height / 2 - 9, 15, Theme::Muted);

    std::string stepText = has ? UI::Fmt("%d / %d", pb.cur, pb.count - 1) : "—";
    float stepW = 100;
    UI::Text(stepText, x, r.y + r.height / 2 - 9, 15, Theme::Muted);
    x += stepW;

    float scrubRight = right - 170 - speedW;
    if (scrubRight - x > 60) {
        float pos = (float)pb.cur;
        if (has && UI::Slider({x, y, scrubRight - x, h}, pos, 0.0f, (float)(pb.count - 1))) {
            pb.cur = (int)std::lround(pos);
            pb.playing = false;
        } else if (!has) {
            UI::Slider({x, y, scrubRight - x, h}, pos, 0.0f, 1.0f);
        }
    }
}
