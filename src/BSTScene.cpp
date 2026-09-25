#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <unordered_map>

namespace {

struct BNode {
    int val;
    int left = -1, right = -1;
    bool alive = true;
};

struct BTree {
    std::vector<BNode> n; // node id = index; deleted nodes are marked !alive
    int root = -1;
};

struct BFrame {
    BTree t;
    std::unordered_map<int, Color> hl;
    std::vector<int> out;
    std::string outLabel;
    std::string msg;
};

constexpr int kMaxNodes = 40;
constexpr int kMaxDepth = 8;

class BSTScene : public Scene {
public:
    BSTScene() {
        pb.speedT = 0.2f;
        for (int v : {50, 30, 70, 20, 40, 60, 80, 35}) QuietInsert(v);
        Begin();
        Snap("Every node's left subtree holds smaller values and its right subtree holds larger ones.");
        Finish();
    }

    std::string Name() const override { return "BST"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override { return frames.empty() ? "" : frames[pb.cur].msg; }

    void Controls(UI::Layout& L) override {
        L.Heading("VALUE(S)");
        bool enter = UI::TextInput(L.Row(34), field, "e.g. 45  or  10,5,15");
        auto a = L.Columns(3);
        if (UI::Button(a[0], "Insert") || enter) InsertFromField();
        if (UI::Button(a[1], "Search")) WithValue([&](int v) { Search(v); });
        if (UI::Button(a[2], "Delete")) WithValue([&](int v) { Delete(v); });
        auto mm = L.Columns(2);
        if (UI::Button(mm[0], "Find min")) Extreme(true);
        if (UI::Button(mm[1], "Find max")) Extreme(false);
        if (!err.empty()) L.Paragraph(err, 14, Theme::Swap);

        L.Heading("TRAVERSALS");
        auto t1 = L.Columns(2);
        if (UI::Button(t1[0], "In-order")) Traverse(0);
        if (UI::Button(t1[1], "Pre-order")) Traverse(1);
        auto t2 = L.Columns(2);
        if (UI::Button(t2[0], "Post-order")) Traverse(2);
        if (UI::Button(t2[1], "Level-order")) LevelOrder();

        L.Heading("BUILD A TREE");
        auto b = L.Columns(3);
        if (UI::Button(b[0], "Random")) Rebuild(RandomValues(11), "Random tree built.");
        if (UI::Button(b[1], "Sorted")) Rebuild({10, 20, 30, 40, 50, 60, 70}, "Inserting sorted values makes a 'stick' - searches become O(n)!");
        if (UI::Button(b[2], "Clear")) Rebuild({}, "Tree cleared.");

        L.Heading("ABOUT BSTs");
        L.Paragraph(UI::Fmt("Nodes: %d   Height: %d", Count(tree), Height(tree, tree.root)), 15, Theme::Text);
        L.Paragraph("Search, insert and delete all walk one path from the root, so they cost O(height): "
                    "O(log n) when balanced, O(n) in the worst case.", 14);
        L.Paragraph("In-order traversal always visits values in sorted order.", 14);
        L.Paragraph("Tip: click a node to copy its value.", 14);
    }

    void Canvas(Rectangle r, float dt) override {
        const BFrame& f = frames[pb.cur];
        const BTree& t = f.t;
        int count = Count(t);
        int h = std::max(1, Height(t, t.root));

        float outH = f.out.empty() && f.outLabel.empty() ? 0.0f : 70.0f;
        float top = r.y + 50, bottom = r.y + r.height - 40 - outH;
        float levelH = std::min(90.0f, (bottom - top) / std::max(1, h - 1 + 1));
        float colW = (r.width - 60) / std::max(1, count);
        float rad = Clamp(std::min(levelH, colW) * 0.36f, 13.0f, 26.0f);

        // In-order rank → x, depth → y
        std::unordered_map<int, Vector2> target;
        int rank = 0;
        std::function<void(int, int)> place = [&](int id, int depth) {
            if (id == -1) return;
            place(t.n[id].left, depth + 1);
            target[id] = {r.x + 30 + (rank++ + 0.5f) * colW, top + depth * levelH + rad};
            place(t.n[id].right, depth + 1);
        };
        place(t.root, 0);

        for (auto& [id, p] : target) {
            auto it = pos.find(id);
            if (it == pos.end()) it = pos.emplace(id, Vector2{p.x, p.y - 60}).first;
            it->second = UI::Approach(it->second, p, dt, 9.0f);
        }

        if (t.root == -1) UI::TextCentered("Empty tree - insert a value to plant the root.", {r.x + r.width / 2, r.y + r.height / 2}, 20, Theme::Muted);

        for (auto& [id, p] : target) {
            for (int c : {t.n[id].left, t.n[id].right})
                if (c != -1) DrawLineEx(pos[id], pos[c], 2.5f, Theme::Border);
        }
        int hovered = -1;
        for (auto& [id, p] : target) {
            Vector2 c = pos[id];
            Color ring = f.hl.count(id) ? f.hl.at(id) : Theme::Accent;
            Color fill = f.hl.count(id) ? UI::Mix(Theme::NodeFill, ring, 0.35f) : Theme::NodeFill;
            UI::Node(c, rad, std::to_string(t.n[id].val), fill, ring);
            if (UI::Hover({c.x - rad, c.y - rad, rad * 2, rad * 2}) && CheckCollisionPointCircle(GetMousePosition(), c, rad))
                hovered = id;
        }
        if (t.root != -1) UI::Text("root", pos[t.root].x + rad + 8, pos[t.root].y - 10, 15, Theme::Muted);

        if (hovered >= 0) {
            UI::SetCursor(MOUSE_CURSOR_POINTING_HAND);
            UI::Tooltip(UI::Fmt("%d  (depth %d, click to use)", t.n[hovered].val, Depth(t, hovered)));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) field.text = std::to_string(t.n[hovered].val);
        }

        if (outH > 0) {
            float y = r.y + r.height - outH - 10;
            UI::Text(f.outLabel, r.x + 30, y, 15, Theme::Muted);
            float cw = std::min(44.0f, (r.width - 60) / std::max(1, (int)f.out.size()) - 4);
            for (int i = 0; i < (int)f.out.size(); i++) {
                Rectangle c{r.x + 30 + i * (cw + 4), y + 24, cw, 34};
                bool last = i + 1 == (int)f.out.size();
                UI::Cell(c, std::to_string(f.out[i]), last ? UI::Mix(Theme::NodeFill, Theme::Compare, 0.4f) : Theme::NodeFill,
                         last ? Theme::Compare : Theme::Visit, 15);
            }
        }
    }

private:
    BTree tree;
    std::vector<BFrame> frames;
    Playback pb;
    std::unordered_map<int, Vector2> pos;
    UI::TextField field;
    std::string err;

    // recording state
    BTree work;
    std::unordered_map<int, Color> hl;
    std::vector<int> out;
    std::string outLabel;

    static int Count(const BTree& t) {
        int c = 0;
        std::function<void(int)> go = [&](int id) { if (id == -1) return; c++; go(t.n[id].left); go(t.n[id].right); };
        go(t.root);
        return c;
    }
    static int Height(const BTree& t, int id) {
        if (id == -1) return 0;
        return 1 + std::max(Height(t, t.n[id].left), Height(t, t.n[id].right));
    }
    static int Depth(const BTree& t, int target) {
        int d = 0;
        for (int id = t.root; id != -1; d++) {
            if (id == target) return d;
            id = t.n[target].val < t.n[id].val ? t.n[id].left : t.n[id].right;
        }
        return d;
    }

    void Begin() { frames.clear(); err.clear(); work = tree; hl.clear(); out.clear(); outLabel.clear(); }
    void Snap(std::string msg) { frames.push_back({work, hl, out, outLabel, std::move(msg)}); }
    void Finish() { tree = work; pb.Load((int)frames.size(), true); }

    template <typename Fn>
    void WithValue(Fn fn) {
        int v;
        if (!UI::ParseInt(field.text, v)) { err = "Type a whole number first."; return; }
        fn(v);
    }

    // Attach a new node under `parent` (or as root when parent == -1).
    static int Attach(BTree& t, int parent, int v) {
        t.n.push_back({v});
        int id = (int)t.n.size() - 1;
        if (parent == -1) t.root = id;
        else if (v < t.n[parent].val) t.n[parent].left = id;
        else t.n[parent].right = id;
        return id;
    }

    void QuietInsert(int v) {
        int parent = -1;
        for (int id = tree.root; id != -1;) {
            if (v == tree.n[id].val) return;
            parent = id;
            id = v < tree.n[id].val ? tree.n[id].left : tree.n[id].right;
        }
        Attach(tree, parent, v);
    }

    std::vector<int> RandomValues(int k) {
        std::vector<int> v;
        while ((int)v.size() < k) {
            int x = UI::RandInt(1, 99);
            if (std::find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
        }
        return v;
    }

    void Rebuild(const std::vector<int>& vals, const std::string& msg) {
        tree = BTree{};
        pos.clear();
        for (int v : vals) QuietInsert(v);
        Begin();
        Snap(msg);
        Finish();
    }

    void InsertFromField() {
        bool ok;
        auto vals = UI::ParseIntList(field.text, &ok);
        if (!ok) { err = "Enter whole numbers, e.g. 45 or 10,5,15"; return; }
        Begin();
        for (int v : vals) {
            if (!InsertOne(v)) break;
            hl.clear();
        }
        Finish();
        field.text.clear();
    }

    bool InsertOne(int v) {
        if (Count(work) >= kMaxNodes) { Snap(UI::Fmt("This demo holds up to %d nodes.", kMaxNodes)); return false; }
        if (work.root == -1) {
            hl[Attach(work, -1, v)] = Theme::Done;
            Snap(UI::Fmt("The tree is empty, so %d becomes the root.", v));
            return true;
        }
        int id = work.root, parent = -1, depth = 0;
        while (id != -1) {
            int nv = work.n[id].val;
            hl[id] = Theme::Compare;
            if (v == nv) { Snap(UI::Fmt("%d is already in the tree - BSTs here keep values unique.", v)); return true; }
            bool left = v < nv;
            Snap(UI::Fmt("Insert %d: %d %s %d → go %s", v, v, left ? "<" : ">", nv, left ? "left" : "right"));
            hl[id] = Theme::Visit;
            parent = id;
            id = left ? work.n[id].left : work.n[id].right;
            if (++depth >= kMaxDepth) { Snap("Tree is too tall to draw - try deleting some nodes or clearing it."); return false; }
        }
        int nid = Attach(work, parent, v);
        hl[nid] = Theme::Done;
        Snap(UI::Fmt("Found an empty spot: %d becomes the %s child of %d.", v,
                     v < work.n[parent].val ? "left" : "right", work.n[parent].val));
        return true;
    }

    void Search(int v) {
        Begin();
        int id = work.root, steps = 0;
        while (id != -1) {
            steps++;
            int nv = work.n[id].val;
            if (nv == v) {
                hl[id] = Theme::Done;
                Snap(UI::Fmt("Found %d after checking %d node(s).", v, steps));
                Finish();
                return;
            }
            hl[id] = Theme::Compare;
            Snap(UI::Fmt("%d %s %d → go %s", v, v < nv ? "<" : ">", nv, v < nv ? "left" : "right"));
            hl[id] = Theme::Visit;
            id = v < nv ? work.n[id].left : work.n[id].right;
        }
        Snap(UI::Fmt("Hit an empty branch - %d is not in the tree (checked %d node(s)).", v, steps));
        Finish();
    }

    void Extreme(bool min) {
        Begin();
        if (work.root == -1) { Snap("The tree is empty."); Finish(); return; }
        int id = work.root;
        while (true) {
            int next = min ? work.n[id].left : work.n[id].right;
            if (next == -1) break;
            hl[id] = Theme::Visit;
            Snap(UI::Fmt("Keep going %s…", min ? "left" : "right"));
            id = next;
        }
        hl[id] = Theme::Done;
        Snap(UI::Fmt("The %s value is %d - it's the %s-most node.", min ? "minimum" : "maximum", work.n[id].val, min ? "left" : "right"));
        Finish();
    }

    // Returns a reference to whichever pointer (root or a parent's child) holds `id`.
    int& LinkTo(int id) {
        if (work.root == id) return work.root;
        for (auto& nd : work.n) {
            if (!nd.alive) continue;
            if (nd.left == id) return nd.left;
            if (nd.right == id) return nd.right;
        }
        return work.root;
    }

    void Delete(int v) {
        Begin();
        int id = work.root;
        while (id != -1 && work.n[id].val != v) {
            hl[id] = Theme::Compare;
            Snap(UI::Fmt("Looking for %d: %d %s %d → go %s", v, v, v < work.n[id].val ? "<" : ">", work.n[id].val,
                         v < work.n[id].val ? "left" : "right"));
            hl[id] = Theme::Visit;
            id = v < work.n[id].val ? work.n[id].left : work.n[id].right;
        }
        if (id == -1) { Snap(UI::Fmt("%d is not in the tree - nothing to delete.", v)); Finish(); return; }

        hl[id] = Theme::Swap;
        BNode nd = work.n[id];
        if (nd.left == -1 && nd.right == -1) {
            Snap(UI::Fmt("Found %d. Case 1: it's a leaf, so just remove it.", v));
            LinkTo(id) = -1;
            work.n[id].alive = false;
            hl.erase(id);
            Snap(UI::Fmt("Deleted %d.", v));
        } else if (nd.left == -1 || nd.right == -1) {
            int child = nd.left != -1 ? nd.left : nd.right;
            hl[child] = Theme::Done;
            Snap(UI::Fmt("Found %d. Case 2: one child (%d), so the child takes its place.", v, work.n[child].val));
            LinkTo(id) = child;
            work.n[id].alive = false;
            hl.erase(id);
            Snap(UI::Fmt("Deleted %d - %d moved up.", v, work.n[child].val));
        } else {
            Snap(UI::Fmt("Found %d. Case 3: two children. Replace it with its in-order successor.", v));
            int s = nd.right;
            hl[s] = Theme::Compare;
            Snap("The successor is the smallest value in the right subtree: step right once…");
            while (work.n[s].left != -1) {
                hl[s] = Theme::Visit;
                s = work.n[s].left;
                hl[s] = Theme::Compare;
                Snap("…then keep going left.");
            }
            hl[s] = Theme::Done;
            Snap(UI::Fmt("Successor is %d.", work.n[s].val));
            int sv = work.n[s].val;
            work.n[id].val = sv;
            hl[id] = Theme::Done;
            hl[s] = Theme::Swap;
            Snap(UI::Fmt("Copy %d into the node we're deleting…", sv));
            LinkTo(s) = work.n[s].right;
            work.n[s].alive = false;
            hl.erase(s);
            Snap(UI::Fmt("…then remove the old successor node. %d is gone.", v));
        }
        Finish();
    }

    void Traverse(int kind) {
        Begin();
        const char* names[] = {"In-order (left, node, right)", "Pre-order (node, left, right)", "Post-order (left, right, node)"};
        outLabel = names[kind];
        if (work.root == -1) { Snap("The tree is empty."); Finish(); return; }
        std::function<void(int)> go = [&](int id) {
            if (id == -1) return;
            if (kind == 1) Visit(id);
            go(work.n[id].left);
            if (kind == 0) Visit(id);
            go(work.n[id].right);
            if (kind == 2) Visit(id);
        };
        go(work.root);
        Snap(UI::Fmt("%s complete.%s", names[kind], kind == 0 ? " Notice the output is sorted!" : ""));
        Finish();
    }

    void Visit(int id) {
        for (auto& [k, c] : hl) c = Theme::Visit;
        hl[id] = Theme::Compare;
        out.push_back(work.n[id].val);
        Snap(UI::Fmt("Visit %d", work.n[id].val));
    }

    void LevelOrder() {
        Begin();
        outLabel = "Level-order (breadth-first, uses a queue)";
        if (work.root == -1) { Snap("The tree is empty."); Finish(); return; }
        std::deque<int> q{work.root};
        int prev = -1;
        while (!q.empty()) {
            int id = q.front();
            q.pop_front();
            for (int c : {work.n[id].left, work.n[id].right})
                if (c != -1) { q.push_back(c); hl[c] = Theme::Frontier; }
            if (prev != -1) hl[prev] = Theme::Visit;
            hl[id] = Theme::Compare;
            prev = id;
            out.push_back(work.n[id].val);
            std::string qs;
            for (int x : q) qs += (qs.empty() ? "" : ", ") + std::to_string(work.n[x].val);
            Snap(UI::Fmt("Visit %d, enqueue its children. Queue: [%s]", work.n[id].val, qs.c_str()));
        }
        Snap("Level-order complete: the tree was read row by row.");
        Finish();
    }
};

} // namespace

std::unique_ptr<Scene> MakeBSTScene() { return std::make_unique<BSTScene>(); }
