#include "raylib.h"
#include "Scenes.h"

#include <algorithm>
#include <memory>
#include <vector>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1200, 720, "Algorithm & Data Structure Playground");

    // We handle DPI ourselves: the window works in physical pixels, everything is
    // drawn in logical units through a zoomed camera, and the mouse is scaled to match.
    const float S = std::max(1.0f, GetWindowScaleDPI().x);
    {
        int m = GetCurrentMonitor();
        float mw = GetMonitorWidth(m) / S, mh = GetMonitorHeight(m) / S;
        float w = std::min(1440.0f, mw * 0.96f), h = std::min(900.0f, mh * 0.88f);
        SetWindowSize((int)(w * S), (int)(h * S));
        SetWindowPosition((int)((mw - w) / 2 * S), (int)(std::max(30.0f, (mh - h) / 2 - 20) * S));
    }
    SetMouseScale(1.0f / S, 1.0f / S);
    Camera2D cam{};
    cam.zoom = S;
    auto scissor = [&](Rectangle r) {
        BeginScissorMode((int)(r.x * S), (int)(r.y * S), (int)(r.width * S), (int)(r.height * S));
    };
    SetWindowMinSize((int)(1050 * S), (int)(640 * S));
    SetExitKey(KEY_NULL); // Esc is used to leave text fields, not to quit
    SetTargetFPS(60);
    UI::Init();

    std::vector<std::unique_ptr<Scene>> scenes;
    scenes.push_back(MakeSortingScene());
    scenes.push_back(MakeStackQueueScene());
    scenes.push_back(MakeLinkedListScene());
    scenes.push_back(MakeBSTScene());
    scenes.push_back(MakeHeapScene());
    scenes.push_back(MakeHashTableScene());
    scenes.push_back(MakeGraphScene());
    scenes.push_back(MakePathfindingScene());

    std::vector<std::string> tabNames;
    for (auto& s : scenes) tabNames.push_back(s->Name());
    std::vector<float> panelScroll(scenes.size(), 0.0f);
    std::vector<float> panelContent(scenes.size(), 0.0f);
    int active = 0;

    const float tabH = 50, panelW = 330, statusH = 36, barH = 56;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        float W = GetScreenWidth() / S, H = GetScreenHeight() / S;
        Scene& scene = *scenes[active];
        Playback* pb = scene.GetPlayback();


        UI::BeginFrame();

        // Global playback shortcuts (disabled while typing in a text box).
        if (pb && pb->count > 1 && !UI::AnyTextFocused()) {
            if (UI::KeyPressed(KEY_SPACE)) {
                if (pb->playing) pb->playing = false;
                else { if (pb->AtEnd()) pb->cur = 0; pb->playing = true; }
            }
            if (UI::KeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) pb->Step(1);
            if (UI::KeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) pb->Step(-1);
            if (UI::KeyPressed(KEY_HOME)) pb->Step(-pb->count);
            if (UI::KeyPressed(KEY_END)) pb->JumpEnd();
        }
        if (pb) pb->Update(dt);

        Rectangle panel{0, tabH, panelW, H - tabH};
        Rectangle canvas{panelW, tabH, W - panelW, H - tabH - statusH - barH};
        Rectangle status{panelW, H - statusH - barH, W - panelW, statusH};
        Rectangle bar{panelW, H - barH, W - panelW, barH};

        // Side panel scrolling
        float& scroll = panelScroll[active];
        if (CheckCollisionPointRec(GetMousePosition(), panel)) scroll -= GetMouseWheelMove() * 40;
        scroll = std::clamp(scroll, 0.0f, std::max(0.0f, panelContent[active] - panel.height + 20));

        BeginDrawing();
        ClearBackground(Theme::Bg);
        BeginMode2D(cam);

        // --- Canvas ---
        scissor(canvas);
        UI::SetClip(canvas);
        scene.Canvas(canvas, dt);
        UI::ClearClip();
        EndScissorMode();

        // --- Status line + playback ---
        DrawRectangleRec(status, Theme::Panel);
        DrawLine((int)status.x, (int)status.y, (int)(status.x + status.width), (int)status.y, Theme::Border);
        std::string msg = scene.Status();
        if (!msg.empty()) {
            DrawCircle((int)status.x + 22, (int)(status.y + statusH / 2), 4, Theme::Accent);
            UI::Text(msg, status.x + 36, status.y + statusH / 2 - 10, 17, Theme::Text);
        }
        if (pb) DrawPlaybackBar(bar, *pb);
        else DrawRectangleRec(bar, Theme::Panel);

        // --- Side panel ---
        DrawRectangleRec(panel, Theme::Panel);
        DrawLine((int)(panel.x + panel.width), (int)panel.y, (int)(panel.x + panel.width), (int)H, Theme::Border);
        scissor(panel);
        UI::SetClip(panel);
        UI::Layout L;
        L.area = panel;
        L.y = panel.y + 6 - scroll;
        float startY = L.y;
        scene.Controls(L);
        panelContent[active] = L.y - startY + 20;
        UI::ClearClip();
        EndScissorMode();
        if (panelContent[active] > panel.height) {
            float frac = panel.height / panelContent[active];
            float th = panel.height * frac;
            float ty = panel.y + (panel.height - th) * (scroll / (panelContent[active] - panel.height + 20));
            DrawRectangleRounded({panel.x + panel.width - 6, ty, 4, th}, 1.0f, 4, Theme::Border);
        }

        // --- Tab bar ---
        DrawRectangle(0, 0, (int)W, (int)tabH, Theme::Panel);
        DrawLine(0, (int)tabH, (int)W, (int)tabH, Theme::Border);
        UI::Text("AlgoPlayground", 18, tabH / 2 - 12, 22, Theme::Accent);
        int picked = UI::Tabs({200, 0, W - 200, tabH}, tabNames, active);
        if (picked != active) {
            active = picked;
            UI::Focus(nullptr);
        }

        UI::EndFrame();
        EndMode2D();
        EndDrawing();
    }

    UI::Shutdown();
    CloseWindow();
    return 0;
}
