#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

struct HItem {
    int id;
    int val;
};

struct HFrame {
    std::vector<HItem> a;
    int i = -1, j = -1;
    bool swap = false;
    int doneIdx = -1; // highlight a single index as final/found
    std::vector<int> out;
    std::string msg;
};

constexpr int kMax = 31; // 5 full levels

class HeapScene : public Scene {
public:
    HeapScene() {
        pb.speedT = 0.22f;
        for (int v : {3, 9, 5, 14, 11, 8, 7}) heap.push_back({nextId++, v});
        Begin();
        Snap("A binary heap is a complete tree stored in a plain array. Hover a node to see the index maths.");
        Finish();
    }

    std::string Name() const override { return "Heap"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override { return frames.empty() ? "" : frames[pb.cur].msg; }

    void Controls(UI::Layout& L) override {
        L.Heading("TYPE");
        auto t = L.Columns(2);
        if (UI::Button(t[0], "Min-heap", true, isMin)) SetType(true);
        if (UI::Button(t[1], "Max-heap", true, !isMin)) SetType(false);

        L.Heading("OPERATIONS");
        bool enter = UI::TextInput(L.Row(34), field, "value(s), e.g. 4 or 6,2,9");
        auto o = L.Columns(2);
        if (UI::Button(o[0], "Insert") || enter) InsertFromField();
        if (UI::Button(o[1], isMin ? "Extract min" : "Extract max")) { Begin(); ExtractOne(); Finish(); }
        auto o2 = L.Columns(2);
        if (UI::Button(o2[0], "Build from list")) BuildFromField();
        if (UI::Button(o2[1], "Heap sort")) HeapSort();
        auto o3 = L.Columns(2);
        if (UI::Button(o3[0], "Random")) RandomHeap();
        if (UI::Button(o3[1], "Clear")) { heap.clear(); Begin(); Snap("Heap cleared."); Finish(); }
        if (!err.empty()) L.Paragraph(err, 14, Theme::Swap);

        L.Heading("ABOUT HEAPS");
        L.Paragraph(isMin ? "Every parent is ≤ its children, so the smallest value is always at the root."
                          : "Every parent is ≥ its children, so the largest value is always at the root.", 15, Theme::Text);
        L.Paragraph("For index i:\n  parent = (i - 1) / 2\n  left child = 2i + 1\n  right child = 2i + 2", 14, Theme::Text);
        L.Paragraph("Insert O(log n) · Extract O(log n) · Peek O(1)\nBuild from a list O(n) · Heap sort O(n log n)\n"
                    "Used for priority queues, Dijkstra, scheduling.", 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const HFrame& f = frames[pb.cur];
        int n = (int)f.a.size();

        float arrH = 48;
        float outBlock = f.out.empty() ? 0.0f : 76.0f;
        float arrY = r.y + r.height - arrH - 50 - outBlock;
        float top = r.y + 44;
        int levels = std::max(1, (int)std::floor(std::log2(std::max(n, 1))) + 1);
        float levelH = std::min(95.0f, (arrY - 40 - top) / std::max(1, levels));
        float rad = Clamp(std::min(levelH * 0.34f, r.width / 16 * 0.36f), 12.0f, 24.0f);
        float cw = std::min(58.0f, (r.width - 60) / std::max(n, 1));
        float arrX = r.x + (r.width - cw * n) / 2;

        std::unordered_map<int, Vector2> tTarget, aTarget;
        for (int i = 0; i < n; i++) {
            int lvl = (int)std::floor(std::log2(i + 1));
            int inLvl = i - ((1 << lvl) - 1);
            float span = r.width / (float)(1 << lvl);
            tTarget[f.a[i].id] = {r.x + (inLvl + 0.5f) * span, top + lvl * levelH + rad};
            aTarget[f.a[i].id] = {arrX + i * cw, arrY};
        }
        for (auto* m : {&tpos, &apos}) {
            auto& target = m == &tpos ? tTarget : aTarget;
            for (auto& [id, p] : target) {
                auto it = m->find(id);
                if (it == m->end()) it = m->emplace(id, Vector2{p.x, p.y + (m == &tpos ? -50.0f : 50.0f)}).first;
                it->second = UI::Approach(it->second, p, dt, 9.0f);
            }
        }

        // Hover → index relationships
        int hov = -1;
        for (int i = 0; i < n; i++) {
            Vector2 c = tpos[f.a[i].id];
            Vector2 a = apos[f.a[i].id];
            if (CheckCollisionPointCircle(GetMousePosition(), c, rad) || UI::Hover({a.x, a.y, cw - 4, arrH})) hov = i;
        }
        auto role = [&](int i) -> Color {
            if (i == f.doneIdx) return Theme::Done;
            if (i == f.i || i == f.j) return f.swap ? Theme::Swap : Theme::Compare;
            if (hov >= 0) {
                if (i == hov) return Theme::Path;
                if (i == (hov - 1) / 2 && hov > 0) return Theme::Visit;
                if (i == 2 * hov + 1 || i == 2 * hov + 2) return Theme::Frontier;
            }
            return Theme::Accent;
        };

        if (n == 0) UI::TextCentered("Empty heap - insert some values.", {r.x + r.width / 2, r.y + r.height / 3}, 20, Theme::Muted);

        for (int i = 1; i < n; i++)
            DrawLineEx(tpos[f.a[(i - 1) / 2].id], tpos[f.a[i].id], 2.5f, Theme::Border);
        for (int i = 0; i < n; i++) {
            Color c = role(i);
            bool plain = c.r == Theme::Accent.r && c.g == Theme::Accent.g && c.b == Theme::Accent.b;
            UI::Node(tpos[f.a[i].id], rad, std::to_string(f.a[i].val), plain ? Theme::NodeFill : UI::Mix(Theme::NodeFill, c, 0.35f), c);
            UI::Text(std::to_string(i), tpos[f.a[i].id].x + rad + 3, tpos[f.a[i].id].y - rad - 6, 12, Theme::Muted);
        }

        UI::Text("Array view", arrX, arrY - 26, 15, Theme::Muted);
        for (int i = 0; i < n; i++) {
            Color c = role(i);
            bool plain = c.r == Theme::Accent.r && c.g == Theme::Accent.g && c.b == Theme::Accent.b;
            Vector2 p = apos[f.a[i].id];
            UI::Cell({p.x, p.y, cw - 4, arrH}, std::to_string(f.a[i].val), plain ? Theme::NodeFill : UI::Mix(Theme::NodeFill, c, 0.35f),
                     plain ? Theme::Border : c, 17);
            UI::TextCentered(std::to_string(i), {arrX + i * cw + (cw - 4) / 2, arrY + arrH + 12}, 13, Theme::Muted);
        }

        if (!f.out.empty()) {
            float y = r.y + r.height - outBlock - 6;
            UI::Text(UI::Fmt("Extracted in order (%s):", isMin ? "ascending" : "descending"), r.x + 30, y, 15, Theme::Muted);
            float ow = std::min(44.0f, (r.width - 60) / (float)f.out.size() - 4);
            for (int i = 0; i < (int)f.out.size(); i++)
                UI::Cell({r.x + 30 + i * (ow + 4), y + 24, ow, 34}, std::to_string(f.out[i]), Theme::NodeFill, Theme::Done, 15);
        }

        if (hov >= 0) {
            std::string tip = UI::Fmt("index %d", hov);
            if (hov > 0) tip += UI::Fmt(" · parent (%d-1)/2 = %d", hov, (hov - 1) / 2);
            if (2 * hov + 1 < n) tip += UI::Fmt(" · children %d", 2 * hov + 1);
            if (2 * hov + 2 < n) tip += UI::Fmt(", %d", 2 * hov + 2);
            UI::Tooltip(tip);
        }
        UI::Legend(r.x + 20, r.y + 12, {{Theme::Path, "Hovered"}, {Theme::Visit, "Parent"}, {Theme::Frontier, "Children"}});
    }

private:
    bool isMin = true;
    std::vector<HItem> heap;
    std::vector<HFrame> frames;
    Playback pb;
    int nextId = 1;
    std::unordered_map<int, Vector2> tpos, apos;
    UI::TextField field;
    std::string err;

    std::vector<HItem> work;
    std::vector<int> out;

    bool Better(int x, int y) const { return isMin ? x < y : x > y; }
    const char* Cmp() const { return isMin ? "<" : ">"; }

    void Begin() { frames.clear(); err.clear(); work = heap; out.clear(); }
    void Snap(std::string msg, int i = -1, int j = -1, bool swap = false, int done = -1) {
        frames.push_back({work, i, j, swap, done, out, std::move(msg)});
    }
    void Finish() { heap = work; pb.Load((int)frames.size(), true); }

    void SiftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            bool better = Better(work[i].val, work[p].val);
            Snap(UI::Fmt("Compare %d with parent %d: %s", work[i].val, work[p].val,
                         better ? UI::Fmt("%d %s %d → swap up", work[i].val, Cmp(), work[p].val).c_str() : "order is fine, stop"), i, p);
            if (!better) return;
            std::swap(work[i], work[p]);
            Snap(UI::Fmt("Swapped - %d moves up to index %d", work[p].val, p), p, i, true);
            i = p;
        }
        Snap(UI::Fmt("%d reached the root.", work[0].val), -1, -1, false, 0);
    }

    void SiftDown(int i, int n) {
        while (true) {
            int l = 2 * i + 1, r = l + 1, best = i;
            if (l < n && Better(work[l].val, work[best].val)) best = l;
            if (r < n && Better(work[r].val, work[best].val)) best = r;
            if (l >= n) { Snap(UI::Fmt("%d has no children - done sifting.", work[i].val), i); return; }
            int shownChild = (r < n && Better(work[r].val, work[l].val)) ? r : l;
            if (best == i) {
                Snap(UI::Fmt("%d is %s its children - heap property holds.", work[i].val, isMin ? "≤" : "≥"), i, shownChild);
                return;
            }
            Snap(UI::Fmt("Pick the %s child %d; it beats %d → swap down", isMin ? "smaller" : "larger", work[best].val, work[i].val), i, best);
            std::swap(work[i], work[best]);
            Snap(UI::Fmt("%d moved down to index %d", work[best].val, best), i, best, true);
            i = best;
        }
    }

    void InsertFromField() {
        bool ok;
        auto vals = UI::ParseIntList(field.text, &ok);
        if (!ok) { err = "Enter whole numbers, e.g. 4 or 6,2,9"; return; }
        Begin();
        for (int v : vals) {
            if ((int)work.size() >= kMax) { Snap(UI::Fmt("This demo holds up to %d values.", kMax)); break; }
            work.push_back({nextId++, v});
            int i = (int)work.size() - 1;
            Snap(UI::Fmt("Insert %d: add it at the next free spot (index %d), then bubble up", v, i), i);
            SiftUp(i);
        }
        Finish();
        field.text.clear();
    }

    void ExtractOne() {
        if (work.empty()) { Snap("The heap is empty."); return; }
        int n = (int)work.size();
        Snap(UI::Fmt("The root %d is the %s - remove it", work[0].val, isMin ? "minimum" : "maximum"), -1, -1, false, 0);
        if (n > 1) {
            std::swap(work[0], work[n - 1]);
            Snap(UI::Fmt("Move the last element %d to the root", work[0].val), 0, n - 1, true);
        }
        out.push_back(work.back().val);
        work.pop_back();
        Snap(UI::Fmt("Removed %d. Now sift the new root down.", out.back()));
        if (!work.empty()) SiftDown(0, (int)work.size());
    }

    void Heapify() {
        int n = (int)work.size();
        Snap("Right now it's just an array in level order - not a valid heap yet.");
        if (n > 1) Snap(UI::Fmt("Build-heap: sift down every parent, from the last parent (index %d) back to the root.", n / 2 - 1));
        for (int i = n / 2 - 1; i >= 0; i--) {
            Snap(UI::Fmt("Sift down index %d (value %d)", i, work[i].val), i);
            SiftDown(i, n);
        }
        Snap("Every parent now beats its children - it's a valid heap!", -1, -1, false, 0);
    }

    void BuildFromField() {
        bool ok;
        auto vals = UI::ParseIntList(field.text, &ok);
        if (!ok) { err = "Type a list first, e.g. 9,4,7,1,8,2"; return; }
        if ((int)vals.size() > kMax) vals.resize(kMax);
        heap.clear();
        tpos.clear();
        apos.clear();
        for (int v : vals) heap.push_back({nextId++, v});
        Begin();
        Heapify();
        Finish();
    }

    void RandomHeap() {
        heap.clear();
        tpos.clear();
        apos.clear();
        int n = UI::RandInt(9, 15);
        for (int i = 0; i < n; i++) heap.push_back({nextId++, UI::RandInt(1, 99)});
        Begin();
        Heapify();
        Finish();
    }

    void SetType(bool min) {
        if (min == isMin) return;
        isMin = min;
        Begin();
        Snap(UI::Fmt("Switched to a %s-heap: rebuild so the %s value rises to the top.", min ? "min" : "max", min ? "smallest" : "largest"));
        for (int i = (int)work.size() / 2 - 1; i >= 0; i--) SiftDown(i, (int)work.size());
        Snap("Rebuilt!", -1, -1, false, work.empty() ? -1 : 0);
        Finish();
    }

    void HeapSort() {
        Begin();
        if (work.empty()) { Snap("The heap is empty - add some values first."); Finish(); return; }
        Snap("Heap sort: keep extracting the root. Values come out in sorted order.");
        while (!work.empty()) ExtractOne();
        Snap(UI::Fmt("Done! Every extract took O(log n), so the whole sort is O(n log n)."));
        Finish();
    }
};

} // namespace

std::unique_ptr<Scene> MakeHeapScene() { return std::make_unique<HeapScene>(); }
