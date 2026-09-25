#include "UI.h"
#include "raymath.h"

#include <cmath>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <random>
#include <sstream>

namespace {
    std::map<int, Font> fonts;
    std::string fontPath;
    std::vector<int> codepoints;

    UI::TextField* focused = nullptr;
    bool fieldClaimedClick = false;
    const float* activeSlider = nullptr;

    Rectangle clip{0, 0, 0, 0};
    bool useClip = false;

    int cursor = MOUSE_CURSOR_DEFAULT;
    std::string tooltip;
    std::vector<int> pressedKeys;

    float Spacing(int size) { return fontPath.empty() ? size / 10.0f : 0.0f; }
}

namespace UI {

void Init() {
    // Prefer a proper TTF so text stays crisp at any size; fall back to raylib's
    // built-in bitmap font if none of these exist.
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* c : candidates) {
        if (FileExists(c)) { fontPath = c; break; }
    }
    for (int c = 32; c < 127; c++) codepoints.push_back(c);
    for (int c : {0x00B2, 0x2713, 0x2190, 0x2191, 0x2192, 0x2193, 0x221E, 0x2264, 0x2265, 0x2260, 0x2022, 0x2014, 0x00B7, 0x03B1})
        codepoints.push_back(c);
}

void Shutdown() {
    for (auto& [size, f] : fonts) UnloadFont(f);
    fonts.clear();
}

void BeginFrame() {
    pressedKeys.clear();
    for (int k = GetKeyPressed(); k != 0; k = GetKeyPressed()) pressedKeys.push_back(k);
    fieldClaimedClick = false;
    cursor = MOUSE_CURSOR_DEFAULT;
    tooltip.clear();
}

void EndFrame() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !fieldClaimedClick) focused = nullptr;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) activeSlider = nullptr;

    if (!tooltip.empty()) {
        Vector2 m = GetMousePosition();
        Vector2 sz = Measure(tooltip, 14);
        Rectangle r{m.x + 14, m.y + 18, sz.x + 14, sz.y + 8};
        float sw = GetScreenWidth() / std::max(1.0f, GetWindowScaleDPI().x);
        float sh = GetScreenHeight() / std::max(1.0f, GetWindowScaleDPI().y);
        if (r.x + r.width > sw) r.x = m.x - r.width - 6;
        if (r.y + r.height > sh) r.y = m.y - r.height - 6;
        DrawRectangleRounded(r, 0.3f, 6, Theme::PanelAlt);
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.0f, Theme::Border);
        Text(tooltip, r.x + 7, r.y + 4, 14, Theme::Text);
    }
    SetMouseCursor(cursor);
}

bool KeyPressed(int key) {
    return IsKeyPressed(key) || std::find(pressedKeys.begin(), pressedKeys.end(), key) != pressedKeys.end();
}

void SetClip(Rectangle r) { clip = r; useClip = true; }
void ClearClip() { useClip = false; }

bool Hover(Rectangle r) {
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, r)) return false;
    return !useClip || CheckCollisionPointRec(m, clip);
}

void SetCursor(int c) { cursor = c; }

// ---------------------------------------------------------------- Text

Font& GetFont(int size) {
    static Font fallback = GetFontDefault();
    if (fontPath.empty()) return fallback;
    auto it = fonts.find(size);
    if (it != fonts.end()) return it->second;
    // Rasterise at the display's DPI scale so text stays sharp on high-DPI screens.
    float dpi = std::max(1.0f, GetWindowScaleDPI().x);
    Font f = LoadFontEx(fontPath.c_str(), (int)std::ceil(size * dpi), codepoints.data(), (int)codepoints.size());
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return fonts[size] = f;
}

Vector2 Measure(const std::string& s, int size) {
    return MeasureTextEx(GetFont(size), s.c_str(), (float)size, Spacing(size));
}

void Text(const std::string& s, float x, float y, int size, Color c) {
    DrawTextEx(GetFont(size), s.c_str(), {std::round(x), std::round(y)}, (float)size, Spacing(size), c);
}

void TextCentered(const std::string& s, Vector2 center, int size, Color c) {
    Vector2 sz = Measure(s, size);
    Text(s, center.x - sz.x / 2, center.y - sz.y / 2, size, c);
}

float TextWrapped(const std::string& s, Rectangle r, int size, Color c) {
    float lineH = size * 1.3f;
    float y = r.y;
    std::istringstream paragraphs(s);
    std::string para;
    while (std::getline(paragraphs, para)) {
        std::istringstream words(para);
        std::string word, line;
        while (words >> word) {
            std::string attempt = line.empty() ? word : line + " " + word;
            if (!line.empty() && Measure(attempt, size).x > r.width) {
                Text(line, r.x, y, size, c);
                y += lineH;
                line = word;
            } else {
                line = attempt;
            }
        }
        Text(line, r.x, y, size, c);
        y += lineH;
    }
    return y - r.y;
}

std::string Fmt(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

// ---------------------------------------------------------------- Widgets

bool Button(Rectangle r, const std::string& label, bool enabled, bool active) {
    bool hov = enabled && Hover(r);
    Color fill = active ? Theme::Accent : (hov ? Theme::Hover : Theme::PanelAlt);
    Color border = active ? Theme::Accent : (hov ? Theme::Muted : Theme::Border);
    Color text = active ? Theme::Bg : Theme::Text;
    if (!enabled) { fill = Theme::Panel; text = Theme::Border; }

    DrawRectangleRounded(r, 0.25f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.25f, 6, 1.0f, border);
    int size = r.height >= 30 ? 16 : 14;
    TextCentered(label, {r.x + r.width / 2, r.y + r.height / 2}, size, text);

    if (hov) cursor = MOUSE_CURSOR_POINTING_HAND;
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool TextInput(Rectangle r, TextField& f, const char* placeholder) {
    bool hov = Hover(r);
    if (hov) cursor = MOUSE_CURSOR_IBEAM;
    if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        focused = &f;
        fieldClaimedClick = true;
    }

    bool isFocused = focused == &f;
    bool submitted = false;
    if (isFocused) {
        int c;
        while ((c = GetCharPressed()) > 0) {
            if (c >= 32 && c < 127 && f.text.size() < f.maxLen) f.text.push_back((char)c);
        }
        if ((KeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !f.text.empty())
            f.text.pop_back();
        if (KeyPressed(KEY_ENTER) || KeyPressed(KEY_KP_ENTER)) submitted = true;
        if (KeyPressed(KEY_ESCAPE)) focused = nullptr;
    }

    DrawRectangleRounded(r, 0.25f, 6, Theme::Bg);
    DrawRectangleRoundedLinesEx(r, 0.25f, 6, isFocused ? 2.0f : 1.0f,
                                isFocused ? Theme::Accent : (hov ? Theme::Muted : Theme::Border));

    const int size = 16;
    float innerW = r.width - 20;
    float ty = r.y + (r.height - size) / 2 - 1;
    if (f.text.empty() && !isFocused) {
        Text(placeholder, r.x + 10, ty, size, Theme::Muted);
    } else {
        // Show the tail of the text if it is wider than the box.
        std::string shown = f.text;
        while (!shown.empty() && Measure(shown, size).x > innerW) shown.erase(0, 1);
        Text(shown, r.x + 10, ty, size, Theme::Text);
        if (isFocused && std::fmod(GetTime(), 1.0) < 0.55) {
            float cx = r.x + 11 + Measure(shown, size).x;
            DrawRectangle((int)cx, (int)(r.y + 7), 2, (int)(r.height - 14), Theme::Accent);
        }
    }
    return submitted;
}

bool AnyTextFocused() { return focused != nullptr; }
void Focus(TextField* f) { focused = f; }

bool Slider(Rectangle r, float& value, float min, float max) {
    Rectangle hit{r.x - 6, r.y - 4, r.width + 12, r.height + 8};
    bool hov = Hover(hit);
    if (hov) cursor = MOUSE_CURSOR_POINTING_HAND;
    if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) activeSlider = &value;

    bool changed = false;
    if (activeSlider == &value && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float t = Clamp((GetMousePosition().x - r.x) / r.width, 0.0f, 1.0f);
        float nv = min + t * (max - min);
        if (nv != value) { value = nv; changed = true; }
        cursor = MOUSE_CURSOR_POINTING_HAND;
    }

    float t = max > min ? Clamp((value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
    float cy = r.y + r.height / 2;
    Rectangle track{r.x, cy - 3, r.width, 6};
    DrawRectangleRounded(track, 1.0f, 6, Theme::Border);
    DrawRectangleRounded({r.x, cy - 3, r.width * t, 6}, 1.0f, 6, Theme::Accent);
    DrawCircleV({r.x + r.width * t, cy}, activeSlider == &value ? 9.0f : 8.0f, Theme::Text);
    return changed;
}

bool Checkbox(Rectangle r, const std::string& label, bool& value) {
    bool hov = Hover(r);
    if (hov) cursor = MOUSE_CURSOR_POINTING_HAND;
    bool clicked = hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (clicked) value = !value;

    Rectangle box{r.x, r.y + (r.height - 20) / 2, 20, 20};
    DrawRectangleRounded(box, 0.3f, 6, value ? Theme::Accent : Theme::Bg);
    DrawRectangleRoundedLinesEx(box, 0.3f, 6, 1.0f, value ? Theme::Accent : Theme::Muted);
    if (value) {
        DrawLineEx({box.x + 5, box.y + 10}, {box.x + 9, box.y + 14}, 2.5f, Theme::Bg);
        DrawLineEx({box.x + 9, box.y + 14}, {box.x + 15, box.y + 6}, 2.5f, Theme::Bg);
    }
    Text(label, box.x + 30, r.y + (r.height - 16) / 2 - 1, 16, Theme::Text);
    return clicked;
}

int Tabs(Rectangle r, const std::vector<std::string>& labels, int active) {
    const int size = 16;
    float textTotal = 0;
    for (auto& l : labels) textTotal += Measure(l, size).x;
    float padding = Clamp((r.width - textTotal) / labels.size(), 10.0f, 28.0f);
    float x = r.x;
    int result = active;
    for (int i = 0; i < (int)labels.size(); i++) {
        float w = Measure(labels[i], size).x + padding;
        Rectangle tab{x, r.y, w, r.height};
        bool hov = Hover(tab);
        if (hov) {
            cursor = MOUSE_CURSOR_POINTING_HAND;
            DrawRectangleRec(tab, Theme::PanelAlt);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) result = i;
        }
        bool on = i == active;
        TextCentered(labels[i], {x + w / 2, r.y + r.height / 2}, size, on ? Theme::Text : Theme::Muted);
        if (on) DrawRectangle((int)x + 8, (int)(r.y + r.height - 3), (int)w - 16, 3, Theme::Accent);
        x += w;
    }
    return result;
}

void Tooltip(const std::string& s) { tooltip = s; }

// ---------------------------------------------------------------- Drawing

void Node(Vector2 c, float r, const std::string& label, Color fill, Color ring, Color text) {
    DrawCircleV(c, r + 2.5f, ring);
    DrawCircleV(c, r, fill);
    int size = (int)Clamp(r * (label.size() > 2 ? 0.7f : 0.85f), 10.0f, 22.0f);
    TextCentered(label, c, size, text);
}

void Cell(Rectangle r, const std::string& label, Color fill, Color border, int fontSize, Color text) {
    DrawRectangleRounded(r, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.18f, 6, 2.0f, border);
    if (!label.empty()) TextCentered(label, {r.x + r.width / 2, r.y + r.height / 2}, fontSize, text);
}

static void ArrowHead(Vector2 tip, Vector2 dir, float head, Color c) {
    Vector2 base = Vector2Subtract(tip, Vector2Scale(dir, head));
    Vector2 n{-dir.y, dir.x};
    Vector2 p1 = Vector2Add(base, Vector2Scale(n, head * 0.55f));
    Vector2 p2 = Vector2Subtract(base, Vector2Scale(n, head * 0.55f));
    // Draw both windings; raylib culls whichever one is clockwise.
    DrawTriangle(tip, p1, p2, c);
    DrawTriangle(tip, p2, p1, c);
}

void Arrow(Vector2 a, Vector2 b, float thick, Color c, float head) {
    Vector2 d = Vector2Subtract(b, a);
    float len = Vector2Length(d);
    if (len < 1.0f) return;
    Vector2 u = Vector2Scale(d, 1.0f / len);
    DrawLineEx(a, Vector2Subtract(b, Vector2Scale(u, head * 0.8f)), thick, c);
    ArrowHead(b, u, head, c);
}

void CurvedArrow(Vector2 a, Vector2 b, float bend, float thick, Color c, float head) {
    Vector2 mid = Vector2Scale(Vector2Add(a, b), 0.5f);
    Vector2 d = Vector2Subtract(b, a);
    float len = Vector2Length(d);
    if (len < 1.0f) return;
    Vector2 n{-d.y / len, d.x / len};
    Vector2 ctrl = Vector2Add(mid, Vector2Scale(n, bend));
    Vector2 u = Vector2Normalize(Vector2Subtract(b, ctrl));
    DrawSplineSegmentBezierQuadratic(a, ctrl, Vector2Subtract(b, Vector2Scale(u, head * 0.8f)), thick, c);
    ArrowHead(b, u, head, c);
}

void Legend(float x, float y, const std::vector<std::pair<Color, std::string>>& items) {
    for (auto& [color, label] : items) {
        DrawRectangleRounded({x, y + 3, 14, 14}, 0.3f, 4, color);
        Text(label, x + 20, y, 15, Theme::Muted);
        x += 20 + Measure(label, 15).x + 18;
    }
}

Color Mix(Color a, Color b, float t) {
    return {(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
            (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

// ---------------------------------------------------------------- Parsing / maths

bool ParseInt(const std::string& s, int& out) {
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') i++;
    if (i == s.size()) return false;
    try {
        size_t used = 0;
        long v = std::stol(s.substr(i), &used);
        for (size_t k = i + used; k < s.size(); k++)
            if (s[k] != ' ') return false;
        if (v < -99999 || v > 99999) return false;
        out = (int)v;
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<int> ParseIntList(const std::string& s, bool* ok) {
    std::vector<int> out;
    std::string token;
    bool good = true;
    auto flush = [&] {
        if (token.empty()) return;
        int v;
        if (ParseInt(token, v)) out.push_back(v); else good = false;
        token.clear();
    };
    for (char c : s) {
        if (c == ',' || c == ' ' || c == ';') flush();
        else token.push_back(c);
    }
    flush();
    if (ok) *ok = good && !out.empty();
    return out;
}

float Approach(float cur, float target, float dt, float speed) {
    return cur + (target - cur) * (1.0f - std::exp(-speed * dt));
}

Vector2 Approach(Vector2 cur, Vector2 target, float dt, float speed) {
    return {Approach(cur.x, target.x, dt, speed), Approach(cur.y, target.y, dt, speed)};
}

int RandInt(int lo, int hi) {
    static std::mt19937 rng(std::random_device{}());
    return std::uniform_int_distribution<int>(lo, hi)(rng);
}

// ---------------------------------------------------------------- Layout

Rectangle Layout::Row(float h) {
    Rectangle r{area.x + pad, y, Width(), h};
    y += h + gap;
    return r;
}

std::vector<Rectangle> Layout::Columns(int n, float h) {
    std::vector<Rectangle> cols;
    float w = (Width() - gap * (n - 1)) / n;
    for (int i = 0; i < n; i++) cols.push_back({area.x + pad + i * (w + gap), y, w, h});
    y += h + gap;
    return cols;
}

void Layout::Heading(const std::string& t) {
    y += 8;
    Text(t, area.x + pad, y, 13, Theme::Accent);
    y += 22;
}

void Layout::Paragraph(const std::string& t, int size, Color c) {
    y += TextWrapped(t, {area.x + pad, y, Width(), 1000}, size, c) + gap - 4;
}

} // namespace UI
