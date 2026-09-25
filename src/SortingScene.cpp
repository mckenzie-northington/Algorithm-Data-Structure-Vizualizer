#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace {

// One recorded moment of a sort: the whole array plus what to highlight.
struct SortFrame {
    std::vector<int> arr;
    int a = -1, b = -1;       // indices being compared / written
    bool write = false;       // true = swap/write, false = comparison
    int pivot = -1;           // quick sort pivot index
    int lo = -1, hi = -1;     // active sub-range (quick/merge/heap)
    std::vector<char> sorted; // index is in its final position
    int comparisons = 0, writes = 0;
    std::string msg;
};

struct AlgoInfo {
    const char* name;
    const char* best;
    const char* avg;
    const char* worst;
    const char* space;
    bool stable;
    const char* blurb;
};

const AlgoInfo kAlgos[] = {
    {"Bubble", "O(n)", "O(n²)", "O(n²)", "O(1)", true,
     "Repeatedly walks the array swapping adjacent out-of-order pairs. After each pass the largest "
     "remaining value has 'bubbled' to the end."},
    {"Selection", "O(n²)", "O(n²)", "O(n²)", "O(1)", false,
     "Scans the unsorted part for the minimum and swaps it into the next slot. Few swaps, many comparisons."},
    {"Insertion", "O(n)", "O(n²)", "O(n²)", "O(1)", true,
     "Grows a sorted prefix by sliding each new value left until it fits. Very fast on nearly-sorted data - "
     "try the 'Nearly sorted' preset."},
    {"Quick", "O(n log n)", "O(n log n)", "O(n²)", "O(log n)", false,
     "Picks a pivot (last element), partitions smaller values to its left, then recurses on each side. "
     "Try 'Reversed' to see the worst case."},
    {"Merge", "O(n log n)", "O(n log n)", "O(n log n)", "O(n)", true,
     "Splits the array in half, sorts each half, then merges the two sorted halves together."},
    {"Heap", "O(n log n)", "O(n log n)", "O(n log n)", "O(1)", false,
     "Builds a max-heap inside the array, then repeatedly swaps the max to the end and re-heapifies."},
};
constexpr int kAlgoCount = sizeof(kAlgos) / sizeof(kAlgos[0]);

class SortingScene : public Scene {
public:
    SortingScene() {
        pb.speedT = 0.45f;
        Randomize(0);
    }

    std::string Name() const override { return "Sorting"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override {
        if (dragIndex >= 0) return UI::Fmt("Editing index %d → %d (release to apply)", dragIndex, base[dragIndex]);
        return frames.empty() ? "" : frames[pb.cur].msg;
    }

    void Controls(UI::Layout& L) override {
        L.Heading("ALGORITHM");
        for (int row = 0; row < 2; row++) {
            auto cols = L.Columns(3);
            for (int c = 0; c < 3; c++) {
                int i = row * 3 + c;
                if (UI::Button(cols[c], kAlgos[i].name, true, algo == i)) { algo = i; Generate(); }
            }
        }

        L.Heading("ARRAY");
        UI::Text(UI::Fmt("Size: %d", (int)sizeSlider), L.area.x + L.pad, L.y, 15, Theme::Text);
        L.Space(22);
        if (UI::Slider(L.Row(20), sizeSlider, 4, 150)) Randomize(lastPreset);

        auto p1 = L.Columns(2);
        if (UI::Button(p1[0], "Random")) Randomize(0);
        if (UI::Button(p1[1], "Reversed")) Randomize(1);
        auto p2 = L.Columns(2);
        if (UI::Button(p2[0], "Nearly sorted")) Randomize(2);
        if (UI::Button(p2[1], "Few unique")) Randomize(3);

        L.Heading("YOUR OWN VALUES");
        bool enter = UI::TextInput(L.Row(34), custom, "e.g. 8, 3, 5, 1, 9");
        auto ac = L.Columns(2);
        if (UI::Button(ac[0], "Use values") || enter) ApplyCustom();
        if (UI::Button(ac[1], "Current → box")) {
            custom.text.clear();
            for (size_t i = 0; i < base.size(); i++) custom.text += (i ? "," : "") + std::to_string(base[i]);
        }
        if (!inputError.empty()) L.Paragraph(inputError, 14, Theme::Swap);
        L.Paragraph("Tip: drag any bar up or down to change its value.", 14);

        const AlgoInfo& a = kAlgos[algo];
        std::string title = a.name;
        for (char& ch : title) ch = (char)toupper(ch);
        L.Heading(title + " SORT");
        L.Paragraph(a.blurb, 15, Theme::Text);
        L.Paragraph(UI::Fmt("Best %s · Average %s · Worst %s\nExtra space %s · %s", a.best, a.avg, a.worst,
                            a.space, a.stable ? "Stable" : "Not stable"), 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const SortFrame& f = frames[pb.cur];
        const std::vector<int>& arr = dragIndex >= 0 ? base : f.arr;
        int n = (int)arr.size();

        // Header stats + legend
        UI::Text(UI::Fmt("%s Sort", kAlgos[algo].name), r.x + 24, r.y + 16, 24, Theme::Text);
        UI::Text(UI::Fmt("Comparisons: %d     Swaps / writes: %d", f.comparisons, f.writes), r.x + 24,
                 r.y + 48, 16, Theme::Muted);
        UI::Legend(r.x + r.width - 470, r.y + 20,
                   {{Theme::Compare, "Compare"}, {Theme::Swap, "Swap/write"}, {Theme::Path, "Pivot"},
                    {Theme::Done, "Sorted"}});

        float left = r.x + 24, right = r.x + r.width - 24;
        float top = r.y + 90, bottom = r.y + r.height - 34;
        float barW = (right - left) / n;
        int maxVal = std::max(100, *std::max_element(arr.begin(), arr.end()));
        float scale = (bottom - top) / maxVal;

        if (anim.size() != arr.size()) {
            anim.clear();
            for (int v : arr) anim.push_back((float)v);
        }

        // Active range shading
        if (f.lo >= 0 && dragIndex < 0) {
            DrawRectangleRec({left + f.lo * barW, top, (f.hi - f.lo + 1) * barW, bottom - top},
                             ColorAlpha(Theme::Accent, 0.06f));
        }

        int hovered = -1;
        Vector2 m = GetMousePosition();
        if (m.x >= left && m.x < right && m.y >= top - 20 && m.y <= bottom + 20 && UI::Hover(r))
            hovered = (int)((m.x - left) / barW);

        if (hovered >= 0 && dragIndex < 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            base = f.arr;
            dragIndex = hovered;
            pb.playing = false;
        }
        if (dragIndex >= 0) {
            base[dragIndex] = (int)Clamp(std::round((bottom - m.y) / scale), 1.0f, 999.0f);
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { dragIndex = -1; Generate(); return; }
        }
        if (hovered >= 0) UI::SetCursor(MOUSE_CURSOR_RESIZE_NS);

        bool showNums = barW >= 20;
        for (int i = 0; i < n; i++) {
            anim[i] = UI::Approach(anim[i], (float)arr[i], dt, 25.0f);
            float h = std::max(3.0f, anim[i] * scale);
            Rectangle bar{left + i * barW + (barW > 6 ? 1.5f : 0.5f), bottom - h,
                          std::max(1.0f, barW - (barW > 6 ? 3.0f : 1.0f)), h};

            Color c = Theme::Accent;
            if (dragIndex < 0) {
                if (i < (int)f.sorted.size() && f.sorted[i]) c = Theme::Done;
                if (i == f.pivot) c = Theme::Path;
                if (i == f.a || i == f.b) c = f.write ? Theme::Swap : Theme::Compare;
            } else if (i == dragIndex) {
                c = Theme::Compare;
            }
            if (i == hovered && dragIndex < 0) c = UI::Mix(c, WHITE, 0.25f);

            if (barW > 6) DrawRectangleRounded(bar, std::min(0.3f, 4.0f / bar.width), 4, c);
            else DrawRectangleRec(bar, c);

            if (showNums) {
                int fs = barW >= 30 ? 15 : 12;
                UI::TextCentered(std::to_string(arr[i]), {bar.x + bar.width / 2, bar.y - 10}, fs, Theme::Text);
                UI::TextCentered(std::to_string(i), {bar.x + bar.width / 2, bottom + 12}, 12, Theme::Muted);
            }
        }
        if (hovered >= 0 && dragIndex < 0 && hovered < n)
            UI::Tooltip(UI::Fmt("index %d = %d  (drag to edit)", hovered, arr[hovered]));
    }

private:
    Playback pb;
    std::vector<int> base;
    std::vector<SortFrame> frames;
    std::vector<float> anim;
    int algo = 0;
    float sizeSlider = 30;
    int lastPreset = 0;
    int dragIndex = -1;
    UI::TextField custom;
    std::string inputError;

    // Recorder state while generating frames
    int cmp = 0, wr = 0;
    std::vector<char> sortedMark;
    int rangeLo = -1, rangeHi = -1, pivotIdx = -1;

    void Randomize(int preset) {
        lastPreset = preset;
        int n = (int)sizeSlider;
        base.resize(n);
        for (int i = 0; i < n; i++) base[i] = UI::RandInt(5, 100);
        if (preset == 1) std::sort(base.rbegin(), base.rend());
        if (preset == 2) {
            std::sort(base.begin(), base.end());
            for (int k = 0; k < std::max(1, n / 10); k++) std::swap(base[UI::RandInt(0, n - 1)], base[UI::RandInt(0, n - 1)]);
        }
        if (preset == 3) for (int& v : base) v = 20 * UI::RandInt(1, 5);
        inputError.clear();
        Generate();
    }

    void ApplyCustom() {
        bool ok;
        auto vals = UI::ParseIntList(custom.text, &ok);
        if (!ok) { inputError = "Couldn't read that - use whole numbers separated by commas."; return; }
        if (vals.size() < 2 || vals.size() > 150) { inputError = "Enter between 2 and 150 values."; return; }
        for (int v : vals)
            if (v < 1 || v > 999) { inputError = "Values must be between 1 and 999."; return; }
        inputError.clear();
        base = vals;
        sizeSlider = (float)vals.size();
        Generate();
    }

    void Rec(const std::vector<int>& arr, int a, int b, bool write, std::string msg) {
        SortFrame f;
        f.arr = arr;
        f.a = a; f.b = b; f.write = write;
        f.pivot = pivotIdx; f.lo = rangeLo; f.hi = rangeHi;
        f.sorted = sortedMark;
        f.comparisons = cmp; f.writes = wr;
        f.msg = std::move(msg);
        frames.push_back(std::move(f));
    }
    void Compare(const std::vector<int>& arr, int a, int b, const std::string& msg) { cmp++; Rec(arr, a, b, false, msg); }
    void Swap(std::vector<int>& arr, int a, int b, const std::string& msg) {
        std::swap(arr[a], arr[b]);
        wr++;
        Rec(arr, a, b, true, msg);
    }
    void MarkAll(const std::vector<int>& arr) {
        std::fill(sortedMark.begin(), sortedMark.end(), 1);
        rangeLo = rangeHi = pivotIdx = -1;
        Rec(arr, -1, -1, false, UI::Fmt("Done! %d comparisons, %d swaps/writes.", cmp, wr));
    }

    void Generate() {
        frames.clear();
        anim.clear();
        cmp = wr = 0;
        rangeLo = rangeHi = pivotIdx = -1;
        sortedMark.assign(base.size(), 0);
        std::vector<int> arr = base;
        Rec(arr, -1, -1, false, "Press Play (or Space) to start. Drag bars to edit values.");
        switch (algo) {
            case 0: Bubble(arr); break;
            case 1: Selection(arr); break;
            case 2: Insertion(arr); break;
            case 3: Quick(arr, 0, (int)arr.size() - 1); break;
            case 4: Merge(arr, 0, (int)arr.size() - 1); break;
            case 5: Heap(arr); break;
        }
        MarkAll(arr);
        pb.Load((int)frames.size(), false);
    }

    void Bubble(std::vector<int>& a) {
        int n = (int)a.size();
        for (int i = 0; i < n - 1; i++) {
            bool swapped = false;
            for (int j = 0; j < n - i - 1; j++) {
                bool bigger = a[j] > a[j + 1];
                Compare(a, j, j + 1, UI::Fmt("Pass %d: compare %d and %d%s", i + 1, a[j], a[j + 1],
                                             bigger ? " → out of order" : " → OK"));
                if (bigger) {
                    Swap(a, j, j + 1, UI::Fmt("Swap %d and %d", a[j], a[j + 1]));
                    swapped = true;
                }
            }
            sortedMark[n - i - 1] = 1;
            if (!swapped) {
                Rec(a, -1, -1, false, "No swaps in this pass → the array is already sorted, stop early.");
                return;
            }
        }
    }

    void Selection(std::vector<int>& a) {
        int n = (int)a.size();
        for (int i = 0; i < n - 1; i++) {
            int mn = i;
            for (int j = i + 1; j < n; j++) {
                bool smaller = a[j] < a[mn];
                Compare(a, mn, j, UI::Fmt("Is %d smaller than current min %d? %s", a[j], a[mn], smaller ? "Yes → new min" : "No"));
                if (smaller) mn = j;
            }
            if (mn != i) Swap(a, i, mn, UI::Fmt("Swap min %d into position %d", a[mn], i));
            sortedMark[i] = 1;
        }
        sortedMark[n - 1] = 1;
    }

    void Insertion(std::vector<int>& a) {
        int n = (int)a.size();
        for (int i = 1; i < n; i++) {
            int j = i;
            while (j > 0) {
                bool bigger = a[j - 1] > a[j];
                Compare(a, j - 1, j, UI::Fmt("Insert %d: is %d bigger? %s", a[j], a[j - 1], bigger ? "Yes → slide left" : "No → in place"));
                if (!bigger) break;
                Swap(a, j - 1, j, UI::Fmt("Slide %d left", a[j]));
                j--;
            }
        }
    }

    void Quick(std::vector<int>& a, int lo, int hi) {
        if (lo > hi) return;
        if (lo == hi) { sortedMark[lo] = 1; return; }
        rangeLo = lo; rangeHi = hi; pivotIdx = hi;
        int pivot = a[hi];
        Rec(a, -1, -1, false, UI::Fmt("Partition [%d..%d] around pivot %d", lo, hi, pivot));
        int i = lo - 1;
        for (int j = lo; j < hi; j++) {
            bool less = a[j] < pivot;
            Compare(a, j, hi, UI::Fmt("%d < pivot %d? %s", a[j], pivot, less ? "Yes → move to left side" : "No"));
            if (less) {
                i++;
                if (i != j) Swap(a, i, j, UI::Fmt("Swap %d to the left side", a[j]));
            }
        }
        pivotIdx = -1;
        if (i + 1 != hi) Swap(a, i + 1, hi, UI::Fmt("Place pivot %d at its final index %d", pivot, i + 1));
        sortedMark[i + 1] = 1;
        Quick(a, lo, i);
        Quick(a, i + 2, hi);
        rangeLo = rangeHi = -1;
    }

    // Merge by rotation so the displayed array is always a permutation of the input.
    void Merge(std::vector<int>& a, int lo, int hi) {
        if (lo >= hi) return;
        int mid = (lo + hi) / 2;
        Merge(a, lo, mid);
        Merge(a, mid + 1, hi);
        rangeLo = lo; rangeHi = hi;
        Rec(a, -1, -1, false, UI::Fmt("Merge sorted halves [%d..%d] and [%d..%d]", lo, mid, mid + 1, hi));
        int i = lo, j = mid + 1;
        while (i <= mid && j <= hi) {
            bool takeRight = a[j] < a[i];
            Compare(a, i, j, UI::Fmt("Compare %d and %d → take %d", a[i], a[j], takeRight ? a[j] : a[i]));
            if (takeRight) {
                int v = a[j];
                for (int k = j; k > i; k--) a[k] = a[k - 1];
                a[i] = v;
                wr++;
                Rec(a, i, j, true, UI::Fmt("Move %d into position %d", v, i));
                mid++; j++;
            }
            i++;
        }
        if (lo == 0 && hi == (int)a.size() - 1) std::fill(sortedMark.begin(), sortedMark.end(), 1);
        rangeLo = rangeHi = -1;
    }

    void SiftDown(std::vector<int>& a, int i, int n) {
        while (true) {
            int l = 2 * i + 1, r = l + 1, big = i;
            if (l < n) { Compare(a, big, l, UI::Fmt("Heapify: compare %d with child %d", a[big], a[l])); if (a[l] > a[big]) big = l; }
            if (r < n) { Compare(a, big, r, UI::Fmt("Heapify: compare %d with child %d", a[big], a[r])); if (a[r] > a[big]) big = r; }
            if (big == i) return;
            Swap(a, i, big, UI::Fmt("Sift %d down", a[i]));
            i = big;
        }
    }

    void Heap(std::vector<int>& a) {
        int n = (int)a.size();
        rangeLo = 0; rangeHi = n - 1;
        Rec(a, -1, -1, false, "Phase 1: build a max-heap");
        for (int i = n / 2 - 1; i >= 0; i--) SiftDown(a, i, n);
        for (int end = n - 1; end > 0; end--) {
            rangeHi = end;
            Swap(a, 0, end, UI::Fmt("Move max %d to the end (index %d)", a[0], end));
            sortedMark[end] = 1;
            rangeHi = end - 1;
            SiftDown(a, 0, end);
        }
        sortedMark[0] = 1;
    }
};

} // namespace

std::unique_ptr<Scene> MakeSortingScene() { return std::make_unique<SortingScene>(); }
