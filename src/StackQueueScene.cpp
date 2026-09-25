#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <unordered_map>

namespace {

struct Item {
    int id;
    std::string label;
};

struct SQFrame {
    std::vector<Item> items;
    int hlId = -1;
    Color hlColor = Theme::Accent;
    std::string expr; // bracket checker input (stack mode only)
    int exprPos = -1;
    std::string msg;
};

struct Ghost {
    Vector2 p, v;
    std::string label;
    float life;
};

constexpr int kCapacity = 10;

class StackQueueScene : public Scene {
public:
    StackQueueScene() {
        pb.speedT = 0.18f;
        for (int v : {12, 7, 42}) Push(std::to_string(v), false);
        Idle("Type a value and press Push, or try the bracket checker.");
    }

    std::string Name() const override { return "Stack & Queue"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override { return frames.empty() ? "" : frames[pb.cur].msg; }

    void Controls(UI::Layout& L) override {
        L.Heading("STRUCTURE");
        auto m = L.Columns(2);
        if (UI::Button(m[0], "Stack (LIFO)", true, !queueMode)) SetMode(false);
        if (UI::Button(m[1], "Queue (FIFO)", true, queueMode)) SetMode(true);

        L.Heading("OPERATIONS");
        bool enter = UI::TextInput(L.Row(34), input, "value(s), e.g. 5 or 3,8,1");
        auto ops = L.Columns(3);
        if (UI::Button(ops[0], queueMode ? "Enqueue" : "Push") || enter) AddFromInput();
        if (UI::Button(ops[1], queueMode ? "Dequeue" : "Pop")) Remove();
        if (UI::Button(ops[2], queueMode ? "Front" : "Peek")) Peek();
        auto ops2 = L.Columns(2);
        if (UI::Button(ops2[0], "Add random")) { Begin(); Push(std::to_string(UI::RandInt(1, 99)), true); Finish(); }
        if (UI::Button(ops2[1], "Clear")) { Data().clear(); Idle(queueMode ? "Queue cleared." : "Stack cleared."); }
        if (!err.empty()) L.Paragraph(err, 14, Theme::Swap);

        if (!queueMode) {
            L.Heading("TRY IT: BRACKET CHECKER");
            L.Paragraph("A stack can check if brackets are balanced. Push every opener, pop when you see a closer.", 14);
            bool go = UI::TextInput(L.Row(34), brackets, "e.g. {[()()]}");
            if (UI::Button(L.Row(), "Check brackets") || go) CheckBrackets();
        }

        L.Heading(queueMode ? "ABOUT QUEUES" : "ABOUT STACKS");
        if (queueMode) {
            L.Paragraph("First In, First Out. Items join at the rear and leave from the front - like a line at a shop.", 15, Theme::Text);
            L.Paragraph("Enqueue O(1) · Dequeue O(1) · Front O(1)\nUsed for: BFS, task scheduling, print queues, buffering.", 14);
        } else {
            L.Paragraph("Last In, First Out. You can only touch the top - like a stack of plates.", 15, Theme::Text);
            L.Paragraph("Push O(1) · Pop O(1) · Peek O(1)\nUsed for: undo, function calls, DFS, parsing expressions.", 14);
        }
        L.Paragraph(UI::Fmt("Capacity here is %d so you can see overflow.", kCapacity), 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const SQFrame& f = frames[pb.cur];
        std::unordered_map<int, Vector2> target;
        float cx = r.x + r.width / 2;

        if (!queueMode) {
            float bh = std::min(46.0f, (r.height - 150) / kCapacity - 6);
            float bw = 190;
            float bottom = r.y + r.height - 30;
            // Container outline
            float cTop = bottom - kCapacity * (bh + 6) - 8;
            DrawLineEx({cx - bw / 2 - 10, cTop}, {cx - bw / 2 - 10, bottom + 4}, 3, Theme::Border);
            DrawLineEx({cx + bw / 2 + 10, cTop}, {cx + bw / 2 + 10, bottom + 4}, 3, Theme::Border);
            DrawLineEx({cx - bw / 2 - 11, bottom + 4}, {cx + bw / 2 + 11, bottom + 4}, 3, Theme::Border);
            for (int i = 0; i < (int)f.items.size(); i++)
                target[f.items[i].id] = {cx - bw / 2, bottom - (i + 1) * (bh + 6)};
            DrawItems(f, target, {bw, bh}, {cx - bw / 2, r.y - 60}, dt);
            if (!f.items.empty()) {
                Vector2 p = pos[f.items.back().id];
                UI::Arrow({p.x + bw + 70, p.y + bh / 2}, {p.x + bw + 18, p.y + bh / 2}, 3, Theme::Accent);
                UI::Text("TOP", p.x + bw + 78, p.y + bh / 2 - 10, 18, Theme::Accent);
            }
            for (int i = 0; i < kCapacity; i++)
                UI::Text(std::to_string(i), cx - bw / 2 - 40, bottom - (i + 1) * (bh + 6) + bh / 2 - 8, 14, Theme::Muted);
            UI::Text(UI::Fmt("size %d / %d", (int)f.items.size(), kCapacity), cx - bw / 2 - 10, bottom + 10, 15, Theme::Muted);
            UpdateGhosts(dt, {0, -500});
        } else {
            float slot = std::min(96.0f, (r.width - 200) / kCapacity);
            float bw = slot - 10, bh = 70;
            float startX = cx - slot * kCapacity / 2;
            float y = r.y + r.height / 2 - bh / 2;
            DrawLineEx({startX - 14, y - 12}, {startX + slot * kCapacity + 4, y - 12}, 3, Theme::Border);
            DrawLineEx({startX - 14, y + bh + 12}, {startX + slot * kCapacity + 4, y + bh + 12}, 3, Theme::Border);
            for (int i = 0; i < (int)f.items.size(); i++)
                target[f.items[i].id] = {startX + i * slot, y};
            DrawItems(f, target, {bw, bh}, {r.x + r.width + 40, y}, dt);
            UI::TextCentered("← leave here", {startX - 70, y - 40}, 15, Theme::Muted);
            UI::TextCentered("join here ←", {startX + slot * kCapacity + 40, y - 40}, 15, Theme::Muted);
            if (!f.items.empty()) {
                Vector2 a = pos[f.items.front().id], b = pos[f.items.back().id];
                UI::Arrow({a.x + bw / 2, y + bh + 70}, {a.x + bw / 2, y + bh + 18}, 3, Theme::Done);
                UI::TextCentered("FRONT", {a.x + bw / 2, y + bh + 84}, 16, Theme::Done);
                UI::Arrow({b.x + bw / 2, y - 70}, {b.x + bw / 2, y - 18}, 3, Theme::Accent);
                UI::TextCentered("REAR", {b.x + bw / 2, y - 84}, 16, Theme::Accent);
            }
            UI::Text(UI::Fmt("size %d / %d", (int)f.items.size(), kCapacity), startX, y + bh + 110, 15, Theme::Muted);
            UpdateGhosts(dt, {-700, 0});
        }

        if (!f.expr.empty()) {
            float x = r.x + 30, y = r.y + 24;
            UI::Text("Checking:", x, y + 4, 16, Theme::Muted);
            x += 90;
            for (int i = 0; i < (int)f.expr.size(); i++) {
                Rectangle c{x + i * 30.0f, y, 26, 32};
                Color fill = i == f.exprPos ? Theme::Compare : (i < f.exprPos ? Theme::PanelAlt : Theme::NodeFill);
                UI::Cell(c, std::string(1, f.expr[i]), fill, i == f.exprPos ? Theme::Compare : Theme::Border, 18,
                         i == f.exprPos ? Theme::Bg : Theme::Text);
            }
        }
    }

private:
    bool queueMode = false;
    std::vector<Item> stack, queue;
    std::vector<SQFrame> frames;
    Playback pb;
    int nextId = 1;
    UI::TextField input, brackets;
    std::string err;

    std::unordered_map<int, Vector2> pos;
    std::unordered_map<int, std::string> lastLabel;
    std::vector<Ghost> ghosts;

    std::vector<Item>& Data() { return queueMode ? queue : stack; }

    void SetMode(bool q) {
        if (q == queueMode) return;
        queueMode = q;
        pos.clear();
        lastLabel.clear();
        ghosts.clear();
        err.clear();
        Idle(q ? "Queue mode: items join at the rear and leave from the front." : "Stack mode: everything happens at the top.");
    }

    void Rec(std::string msg, int hl = -1, Color c = Theme::Accent, const std::string& expr = "", int exprPos = -1) {
        frames.push_back({Data(), hl, c, expr, exprPos, std::move(msg)});
    }
    void Begin() { frames.clear(); err.clear(); }
    void Finish() { pb.Load((int)frames.size(), true); }
    void Idle(const std::string& msg) { Begin(); Rec(msg); Finish(); }

    bool Push(const std::string& label, bool record) {
        auto& d = Data();
        const char* verb = queueMode ? "enqueue" : "push";
        if ((int)d.size() >= kCapacity) {
            if (record) Rec(UI::Fmt("%s(%s) failed: %s is full (overflow)!", verb, label.c_str(), queueMode ? "queue" : "stack"),
                            d.back().id, Theme::Swap);
            return false;
        }
        d.push_back({nextId++, label});
        if (record) Rec(UI::Fmt(queueMode ? "enqueue(%s): add to the rear" : "push(%s): place on top", label.c_str()),
                        d.back().id, Theme::Done);
        return true;
    }

    void AddFromInput() {
        bool ok;
        auto vals = UI::ParseIntList(input.text, &ok);
        if (!ok) { err = "Enter whole numbers, e.g. 5 or 3,8,1"; return; }
        Begin();
        for (int v : vals)
            if (!Push(std::to_string(v), true)) break;
        Finish();
        input.text.clear();
    }

    void Remove() {
        Begin();
        auto& d = Data();
        if (d.empty()) {
            Rec(queueMode ? "dequeue() failed: queue is empty (underflow)." : "pop() failed: stack is empty (underflow).");
        } else if (queueMode) {
            Rec(UI::Fmt("dequeue() → %s: remove from the front", d.front().label.c_str()), d.front().id, Theme::Swap);
            d.erase(d.begin());
            Rec(d.empty() ? "Queue is now empty." : UI::Fmt("%s is the new front.", d.front().label.c_str()),
                d.empty() ? -1 : d.front().id, Theme::Done);
        } else {
            Rec(UI::Fmt("pop() → %s: remove the top item", d.back().label.c_str()), d.back().id, Theme::Swap);
            d.pop_back();
            Rec(d.empty() ? "Stack is now empty." : UI::Fmt("%s is the new top.", d.back().label.c_str()),
                d.empty() ? -1 : d.back().id, Theme::Done);
        }
        Finish();
    }

    void Peek() {
        Begin();
        auto& d = Data();
        if (d.empty()) Rec("Nothing to look at - it's empty.");
        else if (queueMode) Rec(UI::Fmt("front() → %s (looks without removing)", d.front().label.c_str()), d.front().id, Theme::Compare);
        else Rec(UI::Fmt("peek() → %s (looks without removing)", d.back().label.c_str()), d.back().id, Theme::Compare);
        Finish();
    }

    void CheckBrackets() {
        std::string e;
        for (char c : brackets.text)
            if (std::string("()[]{}").find(c) != std::string::npos) e += c;
        Begin();
        if (e.empty()) { err = "Type some brackets: ( ) [ ] { }"; Rec("No brackets to check."); Finish(); return; }
        if (e.size() > 24) e.resize(24);
        stack.clear();
        Rec("Start with an empty stack.", -1, Theme::Accent, e, -1);
        bool ok = true;
        for (int i = 0; i < (int)e.size() && ok; i++) {
            char c = e[i];
            if (c == '(' || c == '[' || c == '{') {
                if (!Push(std::string(1, c), false)) {
                    Rec("Stack overflow - too deeply nested for this demo.", -1, Theme::Swap, e, i);
                    ok = false;
                    break;
                }
                Rec(UI::Fmt("'%c' opens → push it", c), stack.back().id, Theme::Done, e, i);
            } else {
                char want = c == ')' ? '(' : c == ']' ? '[' : '{';
                if (stack.empty()) {
                    Rec(UI::Fmt("'%c' closes but the stack is empty → NOT balanced", c), -1, Theme::Swap, e, i);
                    ok = false;
                } else if (stack.back().label[0] != want) {
                    Rec(UI::Fmt("'%c' doesn't match top '%s' → NOT balanced", c, stack.back().label.c_str()), stack.back().id,
                        Theme::Swap, e, i);
                    ok = false;
                } else {
                    Rec(UI::Fmt("'%c' matches top '%c' → pop", c, want), stack.back().id, Theme::Compare, e, i);
                    stack.pop_back();
                    Rec(UI::Fmt("Popped '%c'", want), -1, Theme::Accent, e, i);
                }
            }
        }
        if (ok) {
            if (stack.empty()) Rec("End of input and the stack is empty → balanced!", -1, Theme::Done, e, (int)e.size());
            else Rec(UI::Fmt("End of input but %d opener(s) left on the stack → NOT balanced", (int)stack.size()),
                     stack.back().id, Theme::Swap, e, (int)e.size());
        }
        Finish();
    }

    void DrawItems(const SQFrame& f, const std::unordered_map<int, Vector2>& target, Vector2 size, Vector2 spawn, float dt) {
        // Anything that disappeared since last frame flies away as a fading ghost.
        for (auto it = pos.begin(); it != pos.end();) {
            if (!target.count(it->first)) {
                ghosts.push_back({it->second, {0, 0}, lastLabel[it->first], 1.0f});
                it = pos.erase(it);
            } else ++it;
        }
        for (const Item& item : f.items) {
            auto it = pos.find(item.id);
            Vector2 t = target.at(item.id);
            if (it == pos.end()) it = pos.emplace(item.id, spawn).first;
            it->second = UI::Approach(it->second, t, dt, 10.0f);
            lastLabel[item.id] = item.label;

            Color fill = Theme::NodeFill, border = Theme::Accent;
            if (item.id == f.hlId) { fill = UI::Mix(Theme::NodeFill, f.hlColor, 0.35f); border = f.hlColor; }
            Rectangle rc{it->second.x, it->second.y, size.x, size.y};
            UI::Cell(rc, item.label, fill, border, 22);
        }
        ghostSize = size;
    }

    Vector2 ghostSize{0, 0};
    void UpdateGhosts(float dt, Vector2 velocity) {
        for (auto& g : ghosts) {
            g.v = UI::Approach(g.v, velocity, dt, 4.0f);
            g.p = Vector2Add(g.p, Vector2Scale(g.v, dt));
            g.life -= dt * 1.6f;
            Color c = ColorAlpha(Theme::Swap, std::max(0.0f, g.life));
            UI::Cell({g.p.x, g.p.y, ghostSize.x, ghostSize.y}, g.label, ColorAlpha(Theme::NodeFill, std::max(0.0f, g.life)), c, 22,
                     ColorAlpha(Theme::Text, std::max(0.0f, g.life)));
        }
        ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(), [](const Ghost& g) { return g.life <= 0; }), ghosts.end());
    }
};

} // namespace

std::unique_ptr<Scene> MakeStackQueueScene() { return std::make_unique<StackQueueScene>(); }
