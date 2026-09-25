#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <climits>
#include <functional>

namespace {

struct GNode {
    Vector2 p; // relative to the canvas top-left
    std::string name;
};

struct GEdge {
    int u, v, w;
};

// Node states: 0 unseen, 1 frontier, 2 current, 3 finished
// Edge states: 0 normal, 1 considering, 2 chosen (tree/path), 3 rejected
struct GFrame {
    std::vector<char> ns, es;
    std::vector<int> dist; // empty unless the algorithm uses distances
    std::vector<int> prev; // previous node on best path (-1 none)
    std::string ds;        // queue / stack / priority queue contents
    std::vector<int> order;
    std::string msg;
};

constexpr float kR = 22.0f;
constexpr int kMaxNodes = 26;
constexpr int kInf = INT_MAX;

class GraphScene : public Scene {
public:
    GraphScene() { pb.speedT = 0.25f; }

    std::string Name() const override { return "Graph"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override {
        if (!frames.empty()) return frames[pb.cur].msg;
        if (nodes.empty()) return "Click anywhere on the canvas to add nodes, then drag from one node to another to connect them.";
        return "Click a node to make it the start, then pick an algorithm on the left.";
    }

    void Controls(UI::Layout& L) override {
        L.Heading("BUILD YOUR GRAPH");
        auto t = L.Columns(2);
        if (UI::Button(t[0], "Build tool", true, tool == 0)) tool = 0;
        if (UI::Button(t[1], "Move tool", true, tool == 1)) tool = 1;
        L.Paragraph(tool == 0 ? "• Click empty space: add node\n• Drag node → node: add edge\n• Click a node: set start\n• Shift + drag: move a node"
                              : "• Drag nodes around\n• Click a node: set start",
                    14, Theme::Text);
        L.Paragraph("• Right-click: delete node/edge\n• Scroll over an edge: change weight", 14, Theme::Text);

        UI::Text("New edge weight", L.area.x + L.pad, L.y + 8, 15, Theme::Muted);
        Rectangle wf{L.area.x + L.pad + 130, L.y, L.Width() - 130, 32};
        UI::TextInput(wf, weightField, "random");
        L.y += 40;
        if (UI::Checkbox(L.Row(26), "Directed edges", directed)) Invalidate();

        auto p = L.Columns(3);
        if (UI::Button(p[0], "Sample")) Sample();
        if (UI::Button(p[1], "Random")) RandomGraph();
        if (UI::Button(p[2], "Clear")) { nodes.clear(); edges.clear(); start = -1; Invalidate(); }

        L.Heading("RUN AN ALGORITHM");
        auto a = L.Columns(2);
        if (UI::Button(a[0], "BFS", !nodes.empty(), algo == 0 && !frames.empty())) Run(0);
        if (UI::Button(a[1], "DFS", !nodes.empty(), algo == 1 && !frames.empty())) Run(1);
        auto b = L.Columns(2);
        if (UI::Button(b[0], "Dijkstra", !nodes.empty(), algo == 2 && !frames.empty())) Run(2);
        if (UI::Button(b[1], "Prim's MST", !nodes.empty(), algo == 3 && !frames.empty())) Run(3);
        if (!frames.empty() && UI::Button(L.Row(), "Stop / edit graph")) Invalidate();

        L.Heading("WHAT IT DOES");
        const char* info[] = {
            "Breadth-first search explores in rings: all neighbours first, then their neighbours. Uses a queue. "
            "Finds the fewest-edges path. O(V + E)",
            "Depth-first search dives as deep as possible before backtracking. Uses a stack (here, recursion). O(V + E)",
            "Dijkstra finds the cheapest path from the start to every node by always finalising the closest "
            "unfinished node. Needs non-negative weights. O((V + E) log V). After it runs, hover a node to see its path.",
            "Prim grows a minimum spanning tree: repeatedly add the cheapest edge that connects a new node. "
            "Edges are treated as undirected. O(E log V)"};
        L.Paragraph(info[algo], 14);
    }

    void Canvas(Rectangle r, float dt) override {
        (void)dt;
        canvas = r;
        if (!sampled) { Sample(); sampled = true; }
        Vector2 m = GetMousePosition();
        Vector2 local = Vector2Subtract(m, {r.x, r.y});
        bool inCanvas = UI::Hover(r);

        int hovNode = NodeAt(local);
        int hovEdge = hovNode == -1 ? EdgeAt(local) : -1;
        HandleInput(r, local, inCanvas, hovNode, hovEdge);

        const GFrame* f = frames.empty() ? nullptr : &frames[pb.cur];

        // Path highlight from hovering a node after Dijkstra/BFS
        std::vector<char> onPath(edges.size(), 0);
        if (f && !f->prev.empty() && hovNode >= 0 && hovNode < (int)f->prev.size()) {
            for (int v = hovNode; f->prev[v] != -1; v = f->prev[v]) {
                int e = FindEdge(f->prev[v], v, true);
                if (e >= 0) onPath[e] = 1;
            }
        }

        // Edges
        for (int i = 0; i < (int)edges.size(); i++) {
            const GEdge& e = edges[i];
            Vector2 a = Abs(e.u), b = Abs(e.v);
            char st = f ? f->es[i] : 0;
            Color c = st == 1 ? Theme::Compare : st == 2 ? Theme::Done : st == 3 ? ColorAlpha(Theme::Border, 0.5f) : Theme::Muted;
            float th = st == 2 ? 5.0f : 3.0f;
            if (onPath[i]) { c = Theme::Path; th = 6; }
            if (i == hovEdge) c = UI::Mix(c, WHITE, 0.35f);

            bool twoWay = directed && FindEdge(e.v, e.u, false) >= 0;
            float bend = twoWay ? 26.0f : 0.0f;
            Vector2 d = Vector2Normalize(Vector2Subtract(b, a));
            Vector2 a2 = Vector2Add(a, Vector2Scale(d, kR + 2)), b2 = Vector2Subtract(b, Vector2Scale(d, kR + 4));
            Vector2 n{-d.y, d.x};
            if (directed) {
                if (twoWay) UI::CurvedArrow(a2, b2, bend, th, c, 13);
                else UI::Arrow(a2, b2, th, c, 13);
            } else {
                DrawLineEx(a, b, th, c);
            }
            Vector2 mid = Vector2Add(Vector2Scale(Vector2Add(a, b), 0.5f), Vector2Scale(n, bend * 0.5f));
            std::string w = std::to_string(e.w);
            Vector2 sz = UI::Measure(w, 15);
            Rectangle pill{mid.x - sz.x / 2 - 6, mid.y - 11, sz.x + 12, 22};
            DrawRectangleRounded(pill, 0.5f, 6, Theme::Bg);
            DrawRectangleRoundedLinesEx(pill, 0.5f, 6, 1.0f, c);
            UI::TextCentered(w, {mid.x, mid.y}, 15, Theme::Text);
        }

        // Rubber band while dragging a new edge
        if (edgeFrom >= 0) {
            Vector2 a = Abs(edgeFrom);
            DrawLineEx(a, m, 2.5f, ColorAlpha(Theme::Accent, 0.7f));
        }

        // Nodes
        for (int i = 0; i < (int)nodes.size(); i++) {
            Vector2 c = Abs(i);
            char st = f ? f->ns[i] : 0;
            Color ring = st == 1 ? Theme::Frontier : st == 2 ? Theme::Compare : st == 3 ? Theme::Visit : Theme::Accent;
            Color fill = st == 0 ? Theme::NodeFill : UI::Mix(Theme::NodeFill, ring, 0.4f);
            if (i == start) DrawRing(c, kR + 6, kR + 9, 0, 360, 36, Theme::Done);
            if (i == hovNode) ring = UI::Mix(ring, WHITE, 0.4f);
            UI::Node(c, kR, nodes[i].name, fill, ring);
            if (i == start) UI::TextCentered("start", {c.x, c.y + kR + 20}, 13, Theme::Done);
            if (f && !f->dist.empty()) {
                int dv = f->dist[i];
                std::string s = dv == kInf ? "∞" : std::to_string(dv);
                UI::Text(s, c.x + kR + 6, c.y - kR - 12, 17, dv == kInf ? Theme::Muted : Theme::Compare);
            }
        }

        // Data structure + visit order box
        if (f) {
            float y = r.y + 14;
            UI::Text(f->ds, r.x + 18, y, 17, Theme::Frontier);
            std::string ord;
            for (int v : f->order) ord += (ord.empty() ? "" : " ") + nodes[v].name;
            UI::Text((algo == 3 ? "Tree order: " : "Visit order: ") + ord, r.x + 18, y + 26, 17, Theme::Visit);
            UI::Legend(r.x + 18, r.y + r.height - 30,
                       {{Theme::Frontier, "In queue/stack"}, {Theme::Compare, "Current"}, {Theme::Visit, "Finished"},
                        {Theme::Done, algo == 3 ? "MST edge" : "Tree edge"}});
        }

        if (hovEdge >= 0) UI::Tooltip("Scroll to change weight · right-click to delete");
        else if (hovNode >= 0 && f && !f->prev.empty()) UI::Tooltip("Showing the path from the start to " + nodes[hovNode].name);
    }

private:
    std::vector<GNode> nodes;
    std::vector<GEdge> edges;
    std::vector<GFrame> frames;
    Playback pb;
    Rectangle canvas{0, 0, 900, 600};
    bool sampled = false;
    bool directed = false;
    int tool = 0;
    int algo = 0;
    int start = -1;
    int dragging = -1, edgeFrom = -1;
    Vector2 pressPos{0, 0};
    bool moved = false;
    UI::TextField weightField;

    Vector2 Abs(int i) const { return {canvas.x + nodes[i].p.x, canvas.y + nodes[i].p.y}; }

    void Invalidate() { frames.clear(); pb.Clear(); }

    int NodeAt(Vector2 local) const {
        for (int i = (int)nodes.size() - 1; i >= 0; i--)
            if (Vector2Distance(local, nodes[i].p) <= kR + 4) return i;
        return -1;
    }

    int EdgeAt(Vector2 local) const {
        for (int i = 0; i < (int)edges.size(); i++) {
            Vector2 a = nodes[edges[i].u].p, b = nodes[edges[i].v].p;
            if (CheckCollisionPointLine(local, a, b, 8)) return i;
        }
        return -1;
    }

    int FindEdge(int u, int v, bool eitherWay) const {
        for (int i = 0; i < (int)edges.size(); i++) {
            if (edges[i].u == u && edges[i].v == v) return i;
            if ((eitherWay || !directed) && edges[i].u == v && edges[i].v == u) return i;
        }
        return -1;
    }

    std::string NextName() const {
        for (char c = 'A'; c <= 'Z'; c++) {
            bool used = false;
            for (auto& n : nodes) if (n.name[0] == c) used = true;
            if (!used) return std::string(1, c);
        }
        return "?";
    }

    int NewWeight() const {
        int w;
        if (UI::ParseInt(weightField.text, w) && w >= 1 && w <= 99) return w;
        return UI::RandInt(1, 15);
    }

    void AddEdge(int u, int v) {
        if (u == v) return;
        int e = FindEdge(u, v, false);
        if (e >= 0) edges[e].w = NewWeight();
        else edges.push_back({u, v, NewWeight()});
        Invalidate();
    }

    void DeleteNode(int k) {
        edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const GEdge& e) { return e.u == k || e.v == k; }), edges.end());
        for (auto& e : edges) { if (e.u > k) e.u--; if (e.v > k) e.v--; }
        nodes.erase(nodes.begin() + k);
        if (start == k) start = nodes.empty() ? -1 : 0;
        else if (start > k) start--;
        Invalidate();
    }

    void HandleInput(Rectangle r, Vector2 local, bool inCanvas, int hovNode, int hovEdge) {
        if (inCanvas && hovNode >= 0) UI::SetCursor(MOUSE_CURSOR_POINTING_HAND);

        // Scroll wheel adjusts edge weight
        float wheel = GetMouseWheelMove();
        if (inCanvas && hovEdge >= 0 && wheel != 0) {
            edges[hovEdge].w = (int)Clamp((float)edges[hovEdge].w + (wheel > 0 ? 1 : -1), 1, 99);
            Invalidate();
        }

        if (inCanvas && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (hovNode >= 0) DeleteNode(hovNode);
            else if (hovEdge >= 0) { edges.erase(edges.begin() + hovEdge); Invalidate(); }
            return;
        }

        if (inCanvas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            pressPos = local;
            moved = false;
            if (hovNode >= 0) {
                bool moveMode = tool == 1 || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if (moveMode) dragging = hovNode; else edgeFrom = hovNode;
            } else if (tool == 0 && hovEdge < 0) {
                if ((int)nodes.size() < kMaxNodes) {
                    nodes.push_back({local, NextName()});
                    if (start < 0) start = (int)nodes.size() - 1;
                    Invalidate();
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && Vector2Distance(local, pressPos) > 5) moved = true;

        if (dragging >= 0) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                nodes[dragging].p = {Clamp(local.x, kR + 4, r.width - kR - 4), Clamp(local.y, kR + 4, r.height - kR - 4)};
                UI::SetCursor(MOUSE_CURSOR_RESIZE_ALL);
            } else {
                if (!moved) { start = dragging; Invalidate(); }
                dragging = -1;
            }
        }
        if (edgeFrom >= 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (!moved) { start = edgeFrom; Invalidate(); }
            else if (hovNode >= 0 && hovNode != edgeFrom) AddEdge(edgeFrom, hovNode);
            edgeFrom = -1;
        }
    }

    void Sample() {
        nodes.clear();
        edges.clear();
        float w = canvas.width, h = canvas.height;
        std::vector<std::pair<float, float>> ps = {{0.12f, 0.45f}, {0.3f, 0.2f}, {0.3f, 0.72f}, {0.5f, 0.45f},
                                                   {0.68f, 0.2f}, {0.68f, 0.72f}, {0.86f, 0.45f}};
        for (auto& [x, y] : ps) nodes.push_back({{x * w, y * h}, NextName()});
        edges = {{0, 1, 4}, {0, 2, 2}, {1, 2, 5}, {1, 3, 10}, {2, 3, 3}, {1, 4, 7},
                 {3, 4, 4}, {3, 5, 6}, {2, 5, 12}, {4, 6, 3}, {5, 6, 2}, {4, 5, 1}};
        start = 0;
        Invalidate();
    }

    void RandomGraph() {
        nodes.clear();
        edges.clear();
        int n = UI::RandInt(7, 10);
        for (int tries = 0; (int)nodes.size() < n && tries < 2000; tries++) {
            Vector2 p{(float)UI::RandInt(60, (int)canvas.width - 60), (float)UI::RandInt(90, (int)canvas.height - 60)};
            bool ok = true;
            for (auto& o : nodes) if (Vector2Distance(o.p, p) < 110) ok = false;
            if (ok) nodes.push_back({p, NextName()});
        }
        // Connect each node to one of its nearest earlier nodes (keeps it connected), plus a few extras.
        for (int i = 1; i < (int)nodes.size(); i++) {
            int best = 0;
            for (int j = 0; j < i; j++)
                if (Vector2Distance(nodes[i].p, nodes[j].p) < Vector2Distance(nodes[i].p, nodes[best].p)) best = j;
            edges.push_back({best, i, UI::RandInt(1, 15)});
        }
        for (int k = 0; k < (int)nodes.size() / 2 + 1; k++) {
            int u = UI::RandInt(0, (int)nodes.size() - 1), v = UI::RandInt(0, (int)nodes.size() - 1);
            if (u != v && FindEdge(u, v, true) < 0 && Vector2Distance(nodes[u].p, nodes[v].p) < 320)
                edges.push_back({u, v, UI::RandInt(1, 15)});
        }
        start = 0;
        Invalidate();
    }

    // ---------------- recording ----------------
    GFrame cur;

    void Snap(std::string msg) {
        cur.msg = std::move(msg);
        frames.push_back(cur);
    }

    // Outgoing (neighbour, edge index) pairs, respecting direction unless forced undirected.
    std::vector<std::pair<int, int>> Adj(int u, bool undirected) const {
        std::vector<std::pair<int, int>> out;
        for (int i = 0; i < (int)edges.size(); i++) {
            if (edges[i].u == u) out.push_back({edges[i].v, i});
            else if ((undirected || !directed) && edges[i].v == u) out.push_back({edges[i].u, i});
        }
        std::sort(out.begin(), out.end(), [&](auto& a, auto& b) { return nodes[a.first].name < nodes[b.first].name; });
        return out;
    }

    std::string Names(const std::vector<int>& v) const {
        std::string s;
        for (int x : v) s += (s.empty() ? "" : ", ") + nodes[x].name;
        return s;
    }

    void Run(int which) {
        algo = which;
        frames.clear();
        if (start < 0 || start >= (int)nodes.size()) start = 0;
        int n = (int)nodes.size();
        cur = GFrame{};
        cur.ns.assign(n, 0);
        cur.es.assign(edges.size(), 0);
        switch (which) {
            case 0: BFS(); break;
            case 1: cur.ds = "Call stack: "; DFS(start, {}); Snap(UI::Fmt("DFS finished - reached %d of %d nodes.", (int)cur.order.size(), n)); break;
            case 2: Dijkstra(); break;
            case 3: Prim(); break;
        }
        pb.Load((int)frames.size(), true);
    }

    void BFS() {
        std::vector<int> q{start};
        cur.prev.assign(nodes.size(), -1);
        cur.ns[start] = 1;
        cur.ds = "Queue: [" + Names(q) + "]";
        Snap("Start at " + nodes[start].name + ": mark it seen and put it in the queue.");
        while (!q.empty()) {
            int u = q.front();
            q.erase(q.begin());
            cur.ns[u] = 2;
            cur.order.push_back(u);
            cur.ds = "Queue: [" + Names(q) + "]";
            Snap("Dequeue " + nodes[u].name + " and look at its neighbours.");
            for (auto [v, e] : Adj(u, false)) {
                cur.es[e] = 1;
                if (cur.ns[v] == 0) {
                    Snap(nodes[u].name + " → " + nodes[v].name + ": not seen yet, enqueue it.");
                    cur.ns[v] = 1;
                    cur.es[e] = 2;
                    cur.prev[v] = u;
                    q.push_back(v);
                    cur.ds = "Queue: [" + Names(q) + "]";
                } else {
                    Snap(nodes[u].name + " → " + nodes[v].name + ": already seen, skip.");
                    cur.es[e] = cur.es[e] == 2 ? 2 : 3;
                }
            }
            cur.ns[u] = 3;
        }
        Snap(UI::Fmt("BFS finished - reached %d node(s). Green edges form the BFS tree (fewest hops). Hover a node!",
                     (int)cur.order.size()));
    }

    void DFS(int u, std::vector<int> stack) {
        stack.push_back(u);
        cur.ns[u] = 2;
        cur.order.push_back(u);
        cur.ds = "Call stack: " + Names(stack);
        Snap("Visit " + nodes[u].name + ".");
        for (auto [v, e] : Adj(u, false)) {
            if (cur.es[e] == 2) continue;
            cur.es[e] = 1;
            if (cur.ns[v] == 0) {
                Snap(nodes[u].name + " → " + nodes[v].name + ": unvisited, go deeper.");
                cur.es[e] = 2;
                cur.ns[u] = 1;
                DFS(v, stack);
                cur.ns[u] = 2;
                cur.ds = "Call stack: " + Names(stack);
                Snap("Back at " + nodes[u].name + " (backtracked).");
            } else {
                Snap(nodes[u].name + " → " + nodes[v].name + ": already visited.");
                cur.es[e] = 3;
            }
        }
        cur.ns[u] = 3;
        stack.pop_back();
        cur.ds = "Call stack: " + Names(stack);
        Snap(nodes[u].name + " has no unvisited neighbours left - finished.");
    }

    std::string PQString(const std::vector<char>& done) const {
        std::vector<std::pair<int, int>> items;
        for (int i = 0; i < (int)nodes.size(); i++)
            if (!done[i] && cur.dist[i] != kInf) items.push_back({cur.dist[i], i});
        std::sort(items.begin(), items.end());
        std::string s;
        for (auto [d, i] : items) s += UI::Fmt("%s(%s,%d)", s.empty() ? "" : " ", nodes[i].name.c_str(), d);
        return "Priority queue: " + s;
    }

    void Dijkstra() {
        int n = (int)nodes.size();
        cur.dist.assign(n, kInf);
        cur.prev.assign(n, -1);
        std::vector<int> prevEdge(n, -1);
        std::vector<char> done(n, 0);
        cur.dist[start] = 0;
        cur.ns[start] = 1;
        cur.ds = PQString(done);
        Snap("Every distance starts at ∞ except the start, which is 0.");
        while (true) {
            int u = -1;
            for (int i = 0; i < n; i++)
                if (!done[i] && cur.dist[i] != kInf && (u < 0 || cur.dist[i] < cur.dist[u])) u = i;
            if (u < 0) break;
            done[u] = 1;
            cur.ns[u] = 2;
            cur.order.push_back(u);
            cur.ds = PQString(done);
            Snap(UI::Fmt("Take the closest unfinished node: %s (distance %d). Its distance is now final.", nodes[u].name.c_str(), cur.dist[u]));
            for (auto [v, e] : Adj(u, false)) {
                if (done[v]) continue;
                int nd = cur.dist[u] + edges[e].w;
                char old = cur.es[e];
                cur.es[e] = 1;
                if (nd < cur.dist[v]) {
                    std::string before = cur.dist[v] == kInf ? "∞" : std::to_string(cur.dist[v]);
                    Snap(UI::Fmt("Relax %s → %s: %d + %d = %d < %s → update!", nodes[u].name.c_str(), nodes[v].name.c_str(),
                                 cur.dist[u], edges[e].w, nd, before.c_str()));
                    if (prevEdge[v] >= 0) cur.es[prevEdge[v]] = 3;
                    cur.dist[v] = nd;
                    cur.prev[v] = u;
                    prevEdge[v] = e;
                    cur.es[e] = 2;
                    cur.ns[v] = 1;
                    cur.ds = PQString(done);
                } else {
                    Snap(UI::Fmt("Relax %s → %s: %d + %d = %d ≥ %d → no improvement.", nodes[u].name.c_str(), nodes[v].name.c_str(),
                                 cur.dist[u], edges[e].w, nd, cur.dist[v]));
                    cur.es[e] = old == 0 ? 3 : old;
                }
            }
            cur.ns[u] = 3;
        }
        cur.ds = "Priority queue: (empty)";
        Snap("Done! Green edges are the shortest-path tree. Hover any node to trace its cheapest route.");
    }

    void Prim() {
        int n = (int)nodes.size();
        std::vector<char> in(n, 0);
        in[start] = 1;
        cur.ns[start] = 3;
        cur.order.push_back(start);
        int total = 0;
        Snap("Start the tree at " + nodes[start].name + ".");
        while (true) {
            int best = -1;
            std::vector<int> crossing;
            for (int i = 0; i < (int)edges.size(); i++) {
                if (in[edges[i].u] != in[edges[i].v]) {
                    crossing.push_back(i);
                    if (best < 0 || edges[i].w < edges[best].w) best = i;
                }
            }
            if (best < 0) break;
            for (int e : crossing) cur.es[e] = 1;
            cur.ds = UI::Fmt("Candidate edges leaving the tree: %d", (int)crossing.size());
            Snap(UI::Fmt("Look at every edge leaving the tree - the cheapest has weight %d.", edges[best].w));
            for (int e : crossing) cur.es[e] = 0;
            int v = in[edges[best].u] ? edges[best].v : edges[best].u;
            in[v] = 1;
            cur.ns[v] = 3;
            cur.es[best] = 2;
            cur.order.push_back(v);
            total += edges[best].w;
            Snap(UI::Fmt("Add %s–%s (weight %d). Tree weight so far: %d.", nodes[edges[best].u].name.c_str(),
                         nodes[edges[best].v].name.c_str(), edges[best].w, total));
        }
        for (int i = 0; i < (int)edges.size(); i++) if (cur.es[i] != 2) cur.es[i] = 3;
        cur.ds = UI::Fmt("Total MST weight: %d", total);
        int reached = (int)cur.order.size();
        Snap(reached == n ? UI::Fmt("Minimum spanning tree complete: %d edges, total weight %d.", n - 1, total)
                          : UI::Fmt("Some nodes can't be reached - this is the MST of %s's component (weight %d).", nodes[start].name.c_str(), total));
    }
};

} // namespace

std::unique_ptr<Scene> MakeGraphScene() { return std::make_unique<GraphScene>(); }
