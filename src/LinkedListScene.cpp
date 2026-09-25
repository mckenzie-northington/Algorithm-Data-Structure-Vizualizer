#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <unordered_map>

namespace {

struct LNode {
    int id;
    int val;
    int next; // id of next node, -1 = null
};

struct LFrame {
    std::vector<LNode> nodes; // display order (left → right)
    int head = -1;
    std::unordered_map<int, Color> hl;
    std::vector<std::pair<int, std::string>> ptrs; // pointer labels drawn under nodes
    std::vector<int> raised;                       // nodes drawn lifted (being inserted / removed)
    std::string msg;
};

constexpr int kMaxNodes = 10;

class LinkedListScene : public Scene {
public:
    LinkedListScene() {
        pb.speedT = 0.2f;
        for (int v : {4, 18, 9, 27}) list.push_back({nextId++, v, -1});
        Relink();
        Begin();
        Snap("A singly linked list: each node stores a value and a pointer to the next node.");
        Finish();
    }

    std::string Name() const override { return "Linked List"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override { return frames.empty() ? "" : frames[pb.cur].msg; }

    void Controls(UI::Layout& L) override {
        L.Heading("VALUE");
        bool enter = UI::TextInput(L.Row(34), valueField, "value, e.g. 15");
        auto ins = L.Columns(2);
        if (UI::Button(ins[0], "Insert at head")) WithValue([&](int v) { InsertAt(v, 0); });
        if (UI::Button(ins[1], "Insert at tail") || enter) WithValue([&](int v) { InsertAt(v, (int)list.size()); });
        auto ds = L.Columns(2);
        if (UI::Button(ds[0], "Search")) WithValue([&](int v) { Search(v); });
        if (UI::Button(ds[1], "Delete")) WithValue([&](int v) { Delete(v); });

        L.Heading("INSERT AT INDEX");
        auto ix = L.Columns(2, 34);
        UI::TextInput(ix[0], indexField, "index");
        if (UI::Button(ix[1], "Insert")) {
            int idx;
            if (!UI::ParseInt(indexField.text, idx) || idx < 0 || idx > (int)list.size())
                err = UI::Fmt("Index must be between 0 and %d.", (int)list.size());
            else WithValue([&](int v) { InsertAt(v, idx); });
        }

        L.Heading("WHOLE LIST");
        auto w = L.Columns(3);
        if (UI::Button(w[0], "Reverse")) Reverse();
        if (UI::Button(w[1], "Random")) RandomList();
        if (UI::Button(w[2], "Clear")) { list.clear(); Begin(); Snap("List cleared: head = null."); Finish(); }
        if (!err.empty()) L.Paragraph(err, 14, Theme::Swap);
        L.Paragraph("Tip: click a node to copy its value into the box.", 14);

        L.Heading("ABOUT LINKED LISTS");
        L.Paragraph("Nodes live anywhere in memory and are chained by pointers, so there's no index "
                    "lookup - you have to walk from the head.", 15, Theme::Text);
        L.Paragraph("Insert at head O(1) · Insert at tail O(n)*\nSearch O(n) · Delete O(n) · Access i-th O(n)\n"
                    "*O(1) if you also keep a tail pointer.", 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const LFrame& f = frames[pb.cur];
        int n = (int)f.nodes.size();
        float spacing = std::min(140.0f, (r.width - 100) / std::max(n, 1));
        float bw = std::min(96.0f, spacing * 0.68f), bh = 54;
        float startX = r.x + (r.width - spacing * n) / 2 + (spacing - bw) / 2;
        float cy = r.y + r.height * 0.45f;

        std::unordered_map<int, Rectangle> box;
        for (int i = 0; i < n; i++) {
            const LNode& node = f.nodes[i];
            bool raised = std::find(f.raised.begin(), f.raised.end(), node.id) != f.raised.end();
            Vector2 t{startX + i * spacing, cy - bh / 2 - (raised ? 110.0f : 0.0f)};
            auto it = pos.find(node.id);
            if (it == pos.end()) it = pos.emplace(node.id, Vector2{t.x, t.y - 160}).first;
            it->second = UI::Approach(it->second, t, dt, 9.0f);
            box[node.id] = {it->second.x, it->second.y, bw, bh};
        }

        if (n == 0) UI::TextCentered("head → null   (the list is empty)", {r.x + r.width / 2, cy}, 22, Theme::Muted);

        // Pointers first so boxes sit on top of them
        for (const LNode& node : f.nodes) {
            Rectangle a = box[node.id];
            Vector2 dot{a.x + bw * 0.84f, a.y + bh / 2};
            if (node.next == -1) {
                UI::Arrow(dot, {dot.x + 34, dot.y}, 2, Theme::Muted, 8);
                UI::Text("null", dot.x + 38, dot.y - 9, 15, Theme::Muted);
                continue;
            }
            Rectangle b = box[node.next];
            Color c = f.hl.count(node.id) ? f.hl.at(node.id) : Theme::Muted;
            if (b.x > a.x + 4) {
                UI::Arrow(dot, {b.x - 3, b.y + bh / 2}, 2.5f, c, 11);
            } else {
                UI::CurvedArrow({a.x + bw / 2, a.y + bh + 2}, {b.x + bw / 2, b.y + bh + 4}, -55, 2.5f, c, 11);
            }
            DrawCircleV(dot, 4, c);
        }

        int hovered = -1;
        for (int i = 0; i < n; i++) {
            const LNode& node = f.nodes[i];
            Rectangle b = box[node.id];
            Color border = f.hl.count(node.id) ? f.hl.at(node.id) : Theme::Accent;
            Color fill = f.hl.count(node.id) ? UI::Mix(Theme::NodeFill, border, 0.3f) : Theme::NodeFill;
            UI::Cell(b, "", fill, border, 20);
            DrawLineEx({b.x + bw * 0.68f, b.y + 4}, {b.x + bw * 0.68f, b.y + bh - 4}, 2, border);
            UI::TextCentered(std::to_string(node.val), {b.x + bw * 0.34f, b.y + bh / 2}, 22, Theme::Text);
            DrawCircleV({b.x + bw * 0.84f, b.y + bh / 2}, 4, Theme::Muted);
            if (UI::Hover(b)) hovered = i;
        }

        // head label
        if (f.head != -1 && box.count(f.head)) {
            Rectangle h = box[f.head];
            UI::Arrow({h.x + bw * 0.34f, h.y - 50}, {h.x + bw * 0.34f, h.y - 6}, 3, Theme::Done);
            UI::TextCentered("head", {h.x + bw * 0.34f, h.y - 62}, 17, Theme::Done);
        }

        // pointer labels (curr, prev, next ...) stacked beneath their node
        std::unordered_map<int, int> stackCount;
        for (auto& [id, label] : f.ptrs) {
            if (!box.count(id)) continue;
            Rectangle b = box[id];
            int k = stackCount[id]++;
            float y = b.y + bh + 70 + k * 26;
            Color c = label == "prev" ? Theme::Visit : label == "next" ? Theme::Frontier : Theme::Compare;
            UI::Arrow({b.x + bw * 0.34f, y - 8}, {b.x + bw * 0.34f, b.y + bh + 50}, 2, c, 8);
            UI::TextCentered(label, {b.x + bw * 0.34f, y + 4}, 16, c);
        }

        // index labels
        for (int i = 0; i < n; i++) {
            Rectangle b = box[f.nodes[i].id];
            UI::TextCentered(UI::Fmt("[%d]", i), {b.x + bw * 0.34f, b.y + bh + 14}, 13, Theme::Muted);
        }

        if (hovered >= 0) {
            UI::SetCursor(MOUSE_CURSOR_POINTING_HAND);
            UI::Tooltip(UI::Fmt("value %d (click to use)", f.nodes[hovered].val));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) valueField.text = std::to_string(f.nodes[hovered].val);
        }
    }

private:
    std::vector<LNode> list; // the real list, always in logical order
    std::vector<LFrame> frames;
    Playback pb;
    int nextId = 1;
    std::unordered_map<int, Vector2> pos;
    UI::TextField valueField, indexField;
    std::string err;

    // Working state while recording an operation
    std::vector<LNode> work;
    int workHead = -1;

    void Relink() {
        for (size_t i = 0; i < list.size(); i++) list[i].next = i + 1 < list.size() ? list[i + 1].id : -1;
    }

    LNode* Find(int id) {
        for (auto& nd : work)
            if (nd.id == id) return &nd;
        return nullptr;
    }

    void Begin() {
        err.clear();
        frames.clear();
        work = list;
        workHead = list.empty() ? -1 : list[0].id;
    }

    void Snap(std::string msg, std::unordered_map<int, Color> hl = {},
              std::vector<std::pair<int, std::string>> ptrs = {}, std::vector<int> raised = {}) {
        frames.push_back({work, workHead, std::move(hl), std::move(ptrs), std::move(raised), std::move(msg)});
    }

    void Finish() {
        // Commit: rebuild the real list in logical order starting from head.
        std::vector<LNode> ordered;
        for (int id = workHead; id != -1;) {
            LNode* nd = Find(id);
            if (!nd) break;
            ordered.push_back(*nd);
            id = nd->next;
        }
        list = ordered;
        // If pointers were rewired out of display order (e.g. reverse), add a
        // final frame that slides the nodes into their new left-to-right order.
        bool reordered = false;
        for (size_t i = 0; i < list.size() && i < work.size(); i++)
            if (list[i].id != work[i].id) reordered = true;
        if (reordered && !frames.empty()) {
            LFrame last = frames.back();
            last.nodes = list;
            last.head = list.empty() ? -1 : list[0].id;
            last.raised.clear();
            frames.push_back(last);
        }
        pb.Load((int)frames.size(), true);
    }

    template <typename Fn>
    void WithValue(Fn fn) {
        int v;
        if (!UI::ParseInt(valueField.text, v)) { err = "Type a whole number in the value box first."; return; }
        if (v < -999 || v > 9999) { err = "Keep values between -999 and 9999."; return; }
        fn(v);
    }

    void InsertAt(int v, int idx) {
        if ((int)list.size() >= kMaxNodes) { err = UI::Fmt("This demo holds up to %d nodes.", kMaxNodes); return; }
        Begin();
        LNode fresh{nextId++, v, -1};

        if (idx == 0) {
            work.insert(work.begin(), fresh);
            Snap(UI::Fmt("Create a new node holding %d", v), {{fresh.id, Theme::Done}}, {}, {fresh.id});
            Find(fresh.id)->next = workHead;
            Snap(workHead == -1 ? "new.next = head (which is null)" : "new.next = head: point the new node at the old first node",
                 {{fresh.id, Theme::Done}}, {}, {fresh.id});
            workHead = fresh.id;
            Snap("head = new node. Inserting at the head is O(1) - no walking needed.", {{fresh.id, Theme::Done}});
            Finish();
            return;
        }

        // Walk to the node before the insertion point.
        int prevId = workHead;
        for (int i = 0; i < idx - 1; i++) {
            Snap(UI::Fmt("Walking: at index %d, need to reach index %d", i, idx - 1), {{prevId, Theme::Compare}},
                 {{prevId, "curr"}});
            prevId = Find(prevId)->next;
        }
        Snap(UI::Fmt("Reached index %d - insert after this node", idx - 1), {{prevId, Theme::Compare}}, {{prevId, "curr"}});

        work.insert(work.begin() + idx, fresh);
        LNode* prev = Find(prevId);
        Snap(UI::Fmt("Create a new node holding %d", v), {{fresh.id, Theme::Done}, {prevId, Theme::Compare}},
             {{prevId, "curr"}}, {fresh.id});
        Find(fresh.id)->next = prev->next;
        Snap("new.next = curr.next", {{fresh.id, Theme::Done}, {prevId, Theme::Compare}}, {{prevId, "curr"}}, {fresh.id});
        Find(prevId)->next = fresh.id;
        Snap("curr.next = new: the chain now passes through the new node",
             {{fresh.id, Theme::Done}, {prevId, Theme::Compare}}, {{prevId, "curr"}}, {fresh.id});
        Snap(UI::Fmt("Inserted %d at index %d.", v, idx), {{fresh.id, Theme::Done}});
        Finish();
        valueField.text.clear();
    }

    void Search(int v) {
        Begin();
        int i = 0;
        for (int id = workHead; id != -1; id = Find(id)->next, i++) {
            LNode* nd = Find(id);
            if (nd->val == v) {
                Snap(UI::Fmt("%d == %d → found at index %d after %d step(s)", nd->val, v, i, i + 1), {{id, Theme::Done}},
                     {{id, "curr"}});
                Finish();
                return;
            }
            Snap(UI::Fmt("%d ≠ %d → follow the next pointer", nd->val, v), {{id, Theme::Compare}}, {{id, "curr"}});
        }
        Snap(UI::Fmt("Reached null - %d is not in the list.", v));
        Finish();
    }

    void Delete(int v) {
        Begin();
        if (workHead == -1) { Snap("The list is empty - nothing to delete."); Finish(); return; }
        LNode* h = Find(workHead);
        if (h->val == v) {
            Snap(UI::Fmt("The head holds %d", v), {{h->id, Theme::Swap}}, {{h->id, "curr"}});
            int old = h->id;
            workHead = h->next;
            Snap("head = head.next: skip over the old head", {{old, Theme::Swap}}, {}, {old});
            work.erase(std::remove_if(work.begin(), work.end(), [&](const LNode& nd) { return nd.id == old; }), work.end());
            Snap(UI::Fmt("Deleted %d. The old node is unreachable and freed.", v));
            Finish();
            return;
        }
        int prevId = workHead;
        Snap(UI::Fmt("%d ≠ %d, keep a 'prev' pointer as we walk", h->val, v), {{prevId, Theme::Compare}}, {{prevId, "prev"}});
        for (int cur = h->next; cur != -1; prevId = cur, cur = Find(cur)->next) {
            LNode* c = Find(cur);
            if (c->val == v) {
                Snap(UI::Fmt("Found %d", v), {{cur, Theme::Swap}, {prevId, Theme::Visit}}, {{prevId, "prev"}, {cur, "curr"}});
                Find(prevId)->next = c->next;
                Snap("prev.next = curr.next: route around the node", {{cur, Theme::Swap}, {prevId, Theme::Visit}},
                     {{prevId, "prev"}, {cur, "curr"}}, {cur});
                work.erase(std::remove_if(work.begin(), work.end(), [&](const LNode& nd) { return nd.id == cur; }), work.end());
                Snap(UI::Fmt("Deleted %d.", v));
                Finish();
                return;
            }
            Snap(UI::Fmt("%d ≠ %d → move prev and curr forward", c->val, v), {{cur, Theme::Compare}, {prevId, Theme::Visit}},
                 {{prevId, "prev"}, {cur, "curr"}});
        }
        Snap(UI::Fmt("Reached null - %d is not in the list.", v));
        Finish();
    }

    void Reverse() {
        Begin();
        if (work.size() < 2) { Snap("Need at least two nodes to see a reversal."); Finish(); return; }
        Snap("Reverse in place with three pointers: prev = null, curr = head");
        int prev = -1, cur = workHead;
        while (cur != -1) {
            int next = Find(cur)->next;
            std::vector<std::pair<int, std::string>> p{{cur, "curr"}};
            if (prev != -1) p.push_back({prev, "prev"});
            if (next != -1) p.push_back({next, "next"});
            Snap("next = curr.next (remember the rest of the list)", {{cur, Theme::Compare}}, p);
            Find(cur)->next = prev;
            Snap(prev == -1 ? "curr.next = prev (null): this node becomes the new tail"
                            : "curr.next = prev: flip the pointer backwards",
                 {{cur, Theme::Swap}}, p);
            prev = cur;
            cur = next;
        }
        workHead = prev;
        Snap("curr is null, so prev is the new head. head = prev", {{prev, Theme::Done}}, {{prev, "prev"}});
        Finish();
        frames.back().msg = "Reversed! Every pointer was flipped in one O(n) pass.";
    }

    void RandomList() {
        list.clear();
        int n = UI::RandInt(4, 7);
        for (int i = 0; i < n; i++) list.push_back({nextId++, UI::RandInt(1, 99), -1});
        Relink();
        Begin();
        Snap("Random list created.");
        Finish();
    }
};

} // namespace

std::unique_ptr<Scene> MakeLinkedListScene() { return std::make_unique<LinkedListScene>(); }
