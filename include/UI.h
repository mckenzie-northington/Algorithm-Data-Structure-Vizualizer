#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// Shared colour palette so every scene looks like part of the same app.
namespace Theme {
    inline const Color Bg        {13, 17, 23, 255};
    inline const Color Panel     {22, 27, 34, 255};
    inline const Color PanelAlt  {30, 36, 46, 255};
    inline const Color Hover     {40, 48, 60, 255};
    inline const Color Border    {48, 54, 61, 255};
    inline const Color Text      {230, 237, 243, 255};
    inline const Color Muted     {139, 148, 158, 255};
    inline const Color NodeFill  {33, 41, 54, 255};

    inline const Color Accent    {56, 189, 248, 255};  // default / selected
    inline const Color Compare   {245, 158, 11, 255};  // comparing / probing
    inline const Color Swap      {239, 68, 68, 255};   // swap / remove / wall
    inline const Color Done      {34, 197, 94, 255};   // sorted / found / final
    inline const Color Visit     {167, 139, 250, 255}; // visited
    inline const Color Frontier  {45, 212, 191, 255};  // queued / frontier
    inline const Color Path      {244, 114, 182, 255}; // pivot / path / special
}

// Small immediate-mode UI toolkit on top of raylib. Widgets are drawn and
// hit-tested in the same call, so scene code reads top-to-bottom like a form.
namespace UI {
    void Init();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    // Restrict widget interaction to a region (used for the scrolling side panel).
    void SetClip(Rectangle r);
    void ClearClip();

    // --- Text ---
    Font& GetFont(int size);
    Vector2 Measure(const std::string& s, int size);
    void Text(const std::string& s, float x, float y, int size, Color c);
    void TextCentered(const std::string& s, Vector2 center, int size, Color c);
    float TextWrapped(const std::string& s, Rectangle r, int size, Color c); // returns height used
    std::string Fmt(const char* fmt, ...);

    // --- Widgets ---
    bool Button(Rectangle r, const std::string& label, bool enabled = true, bool active = false);

    struct TextField {
        std::string text;
        size_t maxLen = 80;
    };
    // Returns true when the user presses Enter while the field is focused.
    bool TextInput(Rectangle r, TextField& field, const char* placeholder);
    bool AnyTextFocused();
    void Focus(TextField* field);

    bool Slider(Rectangle r, float& value, float min, float max);
    bool Checkbox(Rectangle r, const std::string& label, bool& value);
    int Tabs(Rectangle r, const std::vector<std::string>& labels, int active);
    void Tooltip(const std::string& s);

    // Like IsKeyPressed, but also catches taps shorter than one frame.
    bool KeyPressed(int key);

    // True if the pointer is over r and inside the current clip region.
    bool Hover(Rectangle r);
    void SetCursor(int cursor);

    // --- Drawing helpers ---
    void Node(Vector2 c, float r, const std::string& label, Color fill, Color ring,
              Color text = Theme::Text);
    void Cell(Rectangle r, const std::string& label, Color fill, Color border, int fontSize,
              Color text = Theme::Text);
    void Arrow(Vector2 from, Vector2 to, float thick, Color c, float head = 10.0f);
    void CurvedArrow(Vector2 from, Vector2 to, float bend, float thick, Color c, float head = 10.0f);
    void Legend(float x, float y, const std::vector<std::pair<Color, std::string>>& items);
    Color Mix(Color a, Color b, float t);

    // --- Parsing / maths ---
    bool ParseInt(const std::string& s, int& out);
    std::vector<int> ParseIntList(const std::string& s, bool* ok = nullptr);
    float Approach(float cur, float target, float dt, float speed = 12.0f);
    Vector2 Approach(Vector2 cur, Vector2 target, float dt, float speed = 12.0f);
    int RandInt(int lo, int hi); // inclusive

    // Vertical layout cursor for the side panel.
    struct Layout {
        Rectangle area{};
        float y = 0;
        float pad = 16;
        float gap = 8;

        float Width() const { return area.width - 2 * pad; }
        Rectangle Row(float h = 32);
        std::vector<Rectangle> Columns(int n, float h = 32);
        void Space(float h = 8) { y += h; }
        void Heading(const std::string& t);
        void Paragraph(const std::string& t, int size = 15, Color c = Theme::Muted);
    };
}
