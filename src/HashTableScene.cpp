#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

struct Entry {
    int id;
    int key;
};

enum class CellState { Empty, Used, Tombstone };
struct Slot {
    CellState state = CellState::Empty;
    Entry e{-1, 0};
};

struct HTFrame {
    std::vector<std::vector<Entry>> chains; // separate chaining
    std::vector<Slot> slots;                // linear probing
    int bucket = -1;                        // bucket the hash pointed at
    std::vector<int> probed;                // slots / chain ids looked at, in order
    int hitId = -1;
    Color hitColor = Theme::Done;
    std::string hashText;
    std::string msg;
};

class HashTableScene : public Scene {
public:
    HashTableScene() {
        pb.speedT = 0.2f;
        Reset();
        for (int k : {15, 8, 22, 4, 11, 30}) QuietInsert(k);
        Begin();
        Snap("Keys are placed by hash(key) = key mod table size. Try inserting keys that collide!");
        Finish();
    }

    std::string Name() const override { return "Hash Table"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override { return frames.empty() ? "" : frames[pb.cur].msg; }

    void Controls(UI::Layout& L) override {
        L.Heading("COLLISION STRATEGY");
        auto modeCols = L.Columns(2);
        if (UI::Button(modeCols[0], "Chaining", true, chaining)) SetMode(true);
        if (UI::Button(modeCols[1], "Linear probing", true, !chaining)) SetMode(false);

        L.Heading("TABLE SIZE");
        UI::Text(UI::Fmt("m = %d buckets%s", (int)sizeSlider, IsPrime((int)sizeSlider) ? "  (prime ✓)" : ""), L.area.x + L.pad, L.y, 15, Theme::Text);
        L.Space(22);
        float before = sizeSlider;
        UI::Slider(L.Row(20), sizeSlider, 3, 17);
        sizeSlider = std::round(sizeSlider);
        if (sizeSlider != before && (int)sizeSlider != m) Resize((int)sizeSlider);

        L.Heading("KEYS");
        bool enter = UI::TextInput(L.Row(34), field, "key(s), e.g. 23 or 5,12,19");
        auto o = L.Columns(3);
        if (UI::Button(o[0], "Insert") || enter) InsertFromField();
        if (UI::Button(o[1], "Search")) WithKey([&](int k) { Search(k); });
        if (UI::Button(o[2], "Delete")) WithKey([&](int k) { Delete(k); });
        auto o2 = L.Columns(2);
        if (UI::Button(o2[0], "Insert 5 random")) {
            std::vector<int> ks;
            for (int i = 0; i < 5; i++) ks.push_back(UI::RandInt(1, 99));
            InsertMany(ks);
        }
        if (UI::Button(o2[1], "Clear")) { Reset(); Begin(); Snap("Table cleared."); Finish(); }
        if (!err.empty()) L.Paragraph(err, 14, Theme::Swap);

        int n = Size();
        L.Heading("STATS");
        L.Paragraph(UI::Fmt("Keys: %d   Load factor α = %d/%d = %.2f", n, n, m, (float)n / m), 15, Theme::Text);
        L.Paragraph(chaining
            ? "Chaining: each bucket holds a small linked list. Collisions just extend the chain; "
              "lookups cost O(1 + α) on average."
            : "Linear probing: on a collision, try the next slot, then the next… Deleting leaves a "
              "tombstone (DEL) so later searches don't stop too early. Keep α below ~0.7!", 14);
        L.Paragraph("Tip: shrink the table to force collisions, or switch strategy to compare the same keys.", 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const HTFrame& f = frames[pb.cur];
        int rows = chaining ? (int)f.chains.size() : (int)f.slots.size();
        float top = r.y + 90;
        float rowH = std::min(44.0f, (r.height - 110) / rows);
        float cellH = rowH - 6;
        float idxX = r.x + 40, bucketX = r.x + 90, bucketW = chaining ? 44.0f : 110.0f;

        if (!f.hashText.empty()) {
            UI::Text(f.hashText, r.x + 40, r.y + 22, 26, Theme::Compare);
        } else {
            UI::Text(UI::Fmt("hash(key) = key mod %d", rows), r.x + 40, r.y + 22, 22, Theme::Muted);
        }
        UI::Legend(r.x + r.width - 380, r.y + 28, {{Theme::Compare, "Checked"}, {Theme::Done, "Found / placed"}, {Theme::Swap, "Removed"}});

        std::unordered_map<int, Vector2> target;
        for (int b = 0; b < rows; b++) {
            float y = top + b * rowH;
            bool isBucket = b == f.bucket;
            UI::Text(std::to_string(b), idxX, y + cellH / 2 - 9, 17, isBucket ? Theme::Compare : Theme::Muted);
            Rectangle cell{bucketX, y, bucketW, cellH};

            if (chaining) {
                UI::Cell(cell, "", isBucket ? UI::Mix(Theme::NodeFill, Theme::Compare, 0.3f) : Theme::PanelAlt,
                         isBucket ? Theme::Compare : Theme::Border, 14);
                const auto& chain = f.chains[b];
                if (chain.empty()) UI::TextCentered("/", {cell.x + cell.width / 2, cell.y + cellH / 2}, 18, Theme::Border);
                float ew = std::min(64.0f, (r.width - 200) / 8.0f);
                for (int i = 0; i < (int)chain.size(); i++)
                    target[chain[i].id] = {bucketX + bucketW + 36 + i * (ew + 30), y};
                for (int i = 0; i < (int)chain.size(); i++) {
                    Vector2 p = Smooth(chain[i].id, target[chain[i].id], dt);
                    Vector2 from = i == 0 ? Vector2{cell.x + cell.width - 6, y + cellH / 2}
                                          : Vector2{pos[chain[i - 1].id].x + ew, pos[chain[i - 1].id].y + cellH / 2};
                    UI::Arrow(from, {p.x - 2, p.y + cellH / 2}, 2, Theme::Muted, 8);
                    DrawEntry({p.x, p.y, ew, cellH}, chain[i], f);
                }
                if (!chain.empty()) DrawCircleV({cell.x + cell.width - 8, y + cellH / 2}, 4, Theme::Muted);
            } else {
                const Slot& s = f.slots[b];
                int order = ProbeOrder(f, b);
                Color border = order >= 0 ? Theme::Compare : Theme::Border;
                Color fill = order >= 0 ? UI::Mix(Theme::NodeFill, Theme::Compare, 0.25f) : Theme::PanelAlt;
                if (s.state == CellState::Used && s.e.id == f.hitId) { border = f.hitColor; fill = UI::Mix(Theme::NodeFill, f.hitColor, 0.35f); }
                UI::Cell(cell, "", fill, border, 16);
                if (s.state == CellState::Used) UI::TextCentered(std::to_string(s.e.key), {cell.x + cell.width / 2, cell.y + cellH / 2}, 18, Theme::Text);
                else if (s.state == CellState::Tombstone) UI::TextCentered("DEL", {cell.x + cell.width / 2, cell.y + cellH / 2}, 14, Theme::Swap);
                if (order >= 0) UI::Text(UI::Fmt("probe %d", order + 1), cell.x + cell.width + 14, y + cellH / 2 - 8, 15, Theme::Compare);
                if (s.state == CellState::Used) {
                    int home = Hash(s.e.key, rows);
                    if (home != b) UI::Text(UI::Fmt("home %d, displaced +%d", home, (b - home + rows) % rows),
                                           cell.x + cell.width + 100, y + cellH / 2 - 8, 14, Theme::Muted);
                }
            }
        }
        // Drop smoothing state for entries no longer shown.
        for (auto it = pos.begin(); it != pos.end();)
            it = target.count(it->first) ? std::next(it) : pos.erase(it);
    }

private:
    bool chaining = true;
    int m = 7;
    float sizeSlider = 7;
    std::vector<std::vector<Entry>> chains;
    std::vector<Slot> slots;
    std::vector<HTFrame> frames;
    Playback pb;
    int nextId = 1;
    std::unordered_map<int, Vector2> pos;
    UI::TextField field;
    std::string err;

    // recording
    int bucket = -1;
    std::vector<int> probed;
    int hitId = -1;
    Color hitColor = Theme::Done;
    std::string hashText;

    static int Hash(int k, int mod) { return ((k % mod) + mod) % mod; }
    static bool IsPrime(int x) {
        if (x < 2) return false;
        for (int d = 2; d * d <= x; d++) if (x % d == 0) return false;
        return true;
    }
    int Size() const {
        int n = 0;
        if (chaining) for (auto& c : chains) n += (int)c.size();
        else for (auto& s : slots) n += s.state == CellState::Used;
        return n;
    }

    Vector2 Smooth(int id, Vector2 t, float dt) {
        auto it = pos.find(id);
        if (it == pos.end()) it = pos.emplace(id, Vector2{t.x + 80, t.y}).first;
        it->second = UI::Approach(it->second, t, dt, 10.0f);
        return it->second;
    }

    int ProbeOrder(const HTFrame& f, int slot) const {
        for (int i = 0; i < (int)f.probed.size(); i++) if (f.probed[i] == slot) return i;
        return -1;
    }

    void DrawEntry(Rectangle rc, const Entry& e, const HTFrame& f) {
        bool checked = std::find(f.probed.begin(), f.probed.end(), e.id) != f.probed.end();
        Color border = Theme::Accent, fill = Theme::NodeFill;
        if (checked) { border = Theme::Compare; fill = UI::Mix(Theme::NodeFill, Theme::Compare, 0.25f); }
        if (e.id == f.hitId) { border = f.hitColor; fill = UI::Mix(Theme::NodeFill, f.hitColor, 0.35f); }
        UI::Cell(rc, std::to_string(e.key), fill, border, 17);
    }

    void Reset() {
        chains.assign(m, {});
        slots.assign(m, {});
    }

    std::vector<int> AllKeys() const {
        std::vector<int> ks;
        if (chaining) { for (auto& c : chains) for (auto& e : c) ks.push_back(e.key); }
        else { for (auto& s : slots) if (s.state == CellState::Used) ks.push_back(s.e.key); }
        return ks;
    }

    void Resize(int newM) {
        auto ks = AllKeys();
        m = newM;
        Reset();
        pos.clear();
        int dropped = 0;
        for (int k : ks) if (!QuietInsert(k)) dropped++;
        Begin();
        Snap(dropped ? UI::Fmt("Rehashed into %d buckets - %d key(s) didn't fit and were dropped.", m, dropped)
                     : UI::Fmt("Rehashed every key into %d buckets. Notice how they move!", m));
        Finish();
    }

    void SetMode(bool c) {
        if (c == chaining) return;
        auto ks = AllKeys();
        chaining = c;
        Reset();
        pos.clear();
        int dropped = 0;
        for (int k : ks) if (!QuietInsert(k)) dropped++;
        Begin();
        Snap(std::string(c ? "Same keys, now with separate chaining." : "Same keys, now with linear probing.") +
             (dropped ? UI::Fmt(" %d key(s) didn't fit.", dropped) : ""));
        Finish();
    }

    bool QuietInsert(int k) {
        int h = Hash(k, m);
        if (chaining) {
            for (auto& e : chains[h]) if (e.key == k) return true;
            chains[h].push_back({nextId++, k});
            return true;
        }
        for (int i = 0; i < m; i++) {
            Slot& s = slots[(h + i) % m];
            if (s.state == CellState::Used && s.e.key == k) return true;
            if (s.state != CellState::Used) { s = {CellState::Used, {nextId++, k}}; return true; }
        }
        return false;
    }

    void Begin() { frames.clear(); err.clear(); bucket = -1; probed.clear(); hitId = -1; hashText.clear(); }
    void Snap(std::string msg) { frames.push_back({chains, slots, bucket, probed, hitId, hitColor, hashText, std::move(msg)}); }
    void Finish() { pb.Load((int)frames.size(), true); }

    template <typename Fn>
    void WithKey(Fn fn) {
        int k;
        if (!UI::ParseInt(field.text, k)) { err = "Type a whole-number key first."; return; }
        fn(k);
    }

    void StartOp(int k) {
        bucket = Hash(k, m);
        probed.clear();
        hitId = -1;
        hashText = UI::Fmt("hash(%d) = %d mod %d = %d", k, k, m, bucket);
        Snap(UI::Fmt("Compute the hash: %d mod %d = %d → start at bucket %d", k, m, bucket, bucket));
    }

    void InsertFromField() {
        bool ok;
        auto ks = UI::ParseIntList(field.text, &ok);
        if (!ok) { err = "Enter whole-number keys, e.g. 23 or 5,12,19"; return; }
        InsertMany(ks);
        field.text.clear();
    }

    void InsertMany(const std::vector<int>& ks) {
        Begin();
        for (int k : ks) InsertOne(k);
        Finish();
    }

    void InsertOne(int k) {
        StartOp(k);
        if (chaining) {
            auto& chain = chains[bucket];
            for (auto& e : chain) {
                probed.push_back(e.id);
                if (e.key == k) { hitId = e.id; hitColor = Theme::Compare; Snap(UI::Fmt("%d is already here - no duplicates.", k)); return; }
                Snap(UI::Fmt("Bucket %d already holds %d - collision! Walk the chain…", bucket, e.key));
            }
            if (chain.size() >= 8) { Snap("This chain is full for the demo - try a bigger table."); return; }
            chain.push_back({nextId++, k});
            hitId = chain.back().id;
            hitColor = Theme::Done;
            Snap(chain.size() == 1 ? UI::Fmt("Bucket %d was empty - %d goes straight in.", bucket, k)
                                   : UI::Fmt("Append %d to the end of bucket %d's chain (length %d).", k, bucket, (int)chain.size()));
            return;
        }
        int firstTomb = -1;
        for (int i = 0; i < m; i++) {
            int idx = (bucket + i) % m;
            Slot& s = slots[idx];
            probed.push_back(idx);
            if (s.state == CellState::Used && s.e.key == k) {
                hitId = s.e.id; hitColor = Theme::Compare;
                Snap(UI::Fmt("%d is already in slot %d - no duplicates.", k, idx));
                return;
            }
            if (s.state == CellState::Empty) {
                int place = firstTomb >= 0 ? firstTomb : idx;
                slots[place] = {CellState::Used, {nextId++, k}};
                hitId = slots[place].e.id;
                hitColor = Theme::Done;
                Snap(i == 0 ? UI::Fmt("Slot %d is free - place %d there.", idx, k)
                            : UI::Fmt("Slot %d is free - place %d there (%d probe(s)).", place, k, i + 1));
                return;
            }
            if (s.state == CellState::Tombstone) {
                if (firstTomb < 0) firstTomb = idx;
                Snap(UI::Fmt("Slot %d is a tombstone - remember it, but keep checking for a duplicate.", idx));
            } else {
                Snap(UI::Fmt("Slot %d is taken by %d - collision! Try slot %d.", idx, s.e.key, (idx + 1) % m));
            }
        }
        if (firstTomb >= 0) {
            slots[firstTomb] = {CellState::Used, {nextId++, k}};
            hitId = slots[firstTomb].e.id;
            hitColor = Theme::Done;
            Snap(UI::Fmt("Reuse the tombstone at slot %d for %d.", firstTomb, k));
        } else {
            Snap(UI::Fmt("Every slot is full - can't insert %d. Grow the table!", k));
        }
    }

    void Search(int k) {
        Begin();
        StartOp(k);
        if (chaining) {
            for (auto& e : chains[bucket]) {
                probed.push_back(e.id);
                if (e.key == k) { hitId = e.id; hitColor = Theme::Done; Snap(UI::Fmt("Found %d in bucket %d.", k, bucket)); Finish(); return; }
                Snap(UI::Fmt("%d ≠ %d, next in chain…", e.key, k));
            }
            Snap(UI::Fmt("End of chain - %d is not in the table.", k));
        } else {
            for (int i = 0; i < m; i++) {
                int idx = (bucket + i) % m;
                const Slot& s = slots[idx];
                probed.push_back(idx);
                if (s.state == CellState::Empty) { Snap(UI::Fmt("Slot %d is empty - %d is not in the table.", idx, k)); Finish(); return; }
                if (s.state == CellState::Used && s.e.key == k) {
                    hitId = s.e.id; hitColor = Theme::Done;
                    Snap(UI::Fmt("Found %d in slot %d after %d probe(s).", k, idx, i + 1));
                    Finish();
                    return;
                }
                Snap(s.state == CellState::Tombstone ? UI::Fmt("Slot %d is a tombstone - keep going.", idx)
                                                     : UI::Fmt("Slot %d holds %d ≠ %d - keep probing.", idx, s.e.key, k));
            }
            Snap(UI::Fmt("Checked every slot - %d is not here.", k));
        }
        Finish();
    }

    void Delete(int k) {
        Begin();
        StartOp(k);
        if (chaining) {
            auto& chain = chains[bucket];
            for (size_t i = 0; i < chain.size(); i++) {
                probed.push_back(chain[i].id);
                if (chain[i].key == k) {
                    hitId = chain[i].id; hitColor = Theme::Swap;
                    Snap(UI::Fmt("Found %d - unlink it from the chain.", k));
                    chain.erase(chain.begin() + i);
                    hitId = -1;
                    Snap(UI::Fmt("Deleted %d.", k));
                    Finish();
                    return;
                }
                Snap(UI::Fmt("%d ≠ %d, next in chain…", chain[i].key, k));
            }
            Snap(UI::Fmt("%d is not in the table.", k));
        } else {
            for (int i = 0; i < m; i++) {
                int idx = (bucket + i) % m;
                Slot& s = slots[idx];
                probed.push_back(idx);
                if (s.state == CellState::Empty) { Snap(UI::Fmt("Hit an empty slot - %d is not in the table.", k)); break; }
                if (s.state == CellState::Used && s.e.key == k) {
                    hitId = s.e.id; hitColor = Theme::Swap;
                    Snap(UI::Fmt("Found %d in slot %d.", k, idx));
                    s.state = CellState::Tombstone;
                    hitId = -1;
                    Snap("Leave a tombstone (DEL) instead of emptying the slot, so probe chains through it still work.");
                    break;
                }
                Snap(UI::Fmt("Slot %d: not %d, keep probing.", idx, k));
            }
        }
        Finish();
    }
};

} // namespace

std::unique_ptr<Scene> MakeHashTableScene() { return std::make_unique<HashTableScene>(); }
