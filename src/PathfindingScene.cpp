#include "Scenes.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>
#include <deque>
#include <queue>

namespace {

constexpr int kCols = 41, kRows = 23;
constexpr int kWeightCost = 5;

enum Cell : unsigned char { Empty = 0, Wall = 1, Heavy = 2 };
// Visual state per cell: 0 none, 1 frontier, 2 visited, 3 path
struct PFrame {
    std::vector<unsigned char> vis;
    int current = -1;
    int visited = 0;
    std::string msg;
};

const Color kWallColor{96, 110, 132, 255};

const char* kAlgoNames[] = {"BFS", "DFS", "Dijkstra", "A*", "Greedy"};

class PathfindingScene : public Scene {
public:
    PathfindingScene() {
        pb.speedT = 0.62f;
        grid.assign(kCols * kRows, Empty);
        startCell = Idx(8, kRows / 2);
        endCell = Idx(kCols - 9, kRows / 2);
    }

    std::string Name() const override { return "Pathfinding"; }
    Playback* GetPlayback() override { return &pb; }
    std::string Status() const override {
        if (!frames.empty()) return frames[pb.cur].msg;
        return "Draw walls with the mouse, drag the green start / red goal, then press Visualize.";
    }

    void Controls(UI::Layout& L) override {
        L.Heading("ALGORITHM");
        auto a = L.Columns(3);
        for (int i = 0; i < 3; i++) if (UI::Button(a[i], kAlgoNames[i], true, algo == i)) SelectAlgo(i);
        auto b = L.Columns(2);
        for (int i = 3; i < 5; i++) if (UI::Button(b[i - 3], kAlgoNames[i], true, algo == i)) SelectAlgo(i);
        if (UI::Button(L.Row(38), "Visualize!", true, true)) Run(false);

        L.Heading("DRAW");
        auto t = L.Columns(3);
        if (UI::Button(t[0], "Walls", true, brush == Wall)) brush = Wall;
        if (UI::Button(t[1], "Weights", true, brush == Heavy)) brush = Heavy;
        if (UI::Button(t[2], "Eraser", true, brush == Empty)) brush = Empty;
        L.Paragraph(UI::Fmt("Left-drag paints, right-drag erases. Weighted cells cost %d to cross instead of 1.", kWeightCost), 14);

        L.Heading("GRID");
        auto g = L.Columns(2);
        if (UI::Button(g[0], "Maze")) Maze();
        if (UI::Button(g[1], "Random walls")) Scatter(Wall, 28);
        auto g2 = L.Columns(2);
        if (UI::Button(g2[0], "Random weights")) Scatter(Heavy, 18);
        if (UI::Button(g2[1], "Clear all")) { std::fill(grid.begin(), grid.end(), Empty); Changed(); }
        if (UI::Button(L.Row(), "Clear path only")) { frames.clear(); pb.Clear(); live = false; }

        L.Heading("ABOUT " + std::string(kAlgoNames[algo]));
        const char* info[] = {
            "Breadth-first search spreads out evenly in every direction. Guarantees the path with the fewest "
            "steps, but ignores weights.",
            "Depth-first search charges down one route and backtracks at dead ends. Does NOT guarantee a shortest path - "
            "watch how weird its paths look.",
            "Dijkstra expands the cheapest-so-far cell first. Guarantees the cheapest path, and respects weights.",
            "A* is Dijkstra plus a hint: it prefers cells that look closer to the goal (Manhattan distance). "
            "Cheapest path, far fewer cells explored.",
            "Greedy best-first only follows the hint. Very fast, but can be fooled into long detours."};
        L.Paragraph(info[algo], 14, Theme::Text);
        L.Paragraph("Tip: after a run finishes, drag the start or goal and the result updates live.", 14);
    }

    void Canvas(Rectangle r, float dt) override {
        (void)dt;
        float cs = std::floor(std::min((r.width - 40) / kCols, (r.height - 60) / kRows));
        float ox = r.x + (r.width - cs * kCols) / 2, oy = r.y + 44 + (r.height - 60 - cs * kRows) / 2;

        HandleInput(r, cs, ox, oy);

        const PFrame* f = frames.empty() ? nullptr : &frames[pb.cur];
        for (int y = 0; y < kRows; y++) {
            for (int x = 0; x < kCols; x++) {
                int i = Idx(x, y);
                Rectangle c{ox + x * cs, oy + y * cs, cs - 1, cs - 1};
                Color col = Theme::NodeFill;
                if (f) {
                    unsigned char v = f->vis[i];
                    if (v == 1) col = UI::Mix(Theme::PanelAlt, Theme::Frontier, 0.55f);
                    if (v == 2) col = UI::Mix(Theme::PanelAlt, Theme::Visit, 0.45f);
                    if (v == 3) col = Theme::Compare;
                    if (i == f->current) col = Theme::Path;
                }
                if (grid[i] == Wall) col = kWallColor;
                DrawRectangleRec(c, col);
                if (grid[i] == Heavy) {
                    DrawRectangleRec(c, ColorAlpha(Theme::Swap, 0.18f));
                    UI::TextCentered(std::to_string(kWeightCost), {c.x + c.width / 2, c.y + c.height / 2}, (int)(cs * 0.55f), Theme::Swap);
                }
            }
        }
        auto marker = [&](int i, Color c, const char* label) {
            Vector2 p{ox + (i % kCols) * cs + cs / 2 - 0.5f, oy + (i / kCols) * cs + cs / 2 - 0.5f};
            DrawCircleV(p, cs * 0.46f, c);
            UI::TextCentered(label, p, (int)(cs * 0.5f), Theme::Bg);
        };
        marker(startCell, Theme::Done, "S");
        marker(endCell, Theme::Swap, "G");

        std::string stats = f ? UI::Fmt("Cells explored: %d", f->visited) : "Cells explored: -";
        if (f && pathLen >= 0 && pb.AtEnd()) stats += UI::Fmt("     Path length: %d     Path cost: %d", pathLen, pathCost);
        UI::Text(stats, r.x + 24, r.y + 14, 17, Theme::Text);
        UI::Legend(r.x + r.width - 420, r.y + 14,
                   {{Theme::Frontier, "Frontier"}, {Theme::Visit, "Explored"}, {Theme::Compare, "Path"}, {kWallColor, "Wall"}});

        if (f && pb.AtEnd() && !pb.playing) live = true;
    }

private:
    std::vector<unsigned char> grid;
    int startCell, endCell;
    int algo = 3;
    unsigned char brush = Wall;
    std::vector<PFrame> frames;
    Playback pb;
    int draggingMarker = 0; // 1 start, 2 end
    int paintValue = -1;
    bool live = false; // re-run instantly on edits once a run has completed
    int pathLen = -1, pathCost = 0;

    static int Idx(int x, int y) { return y * kCols + x; }

    void SelectAlgo(int i) {
        algo = i;
        if (live) Run(true);
    }

    void Changed() {
        if (live) Run(true);
        else { frames.clear(); pb.Clear(); }
    }

    void HandleInput(Rectangle r, float cs, float ox, float oy) {
        Vector2 m = GetMousePosition();
        int gx = (int)std::floor((m.x - ox) / cs), gy = (int)std::floor((m.y - oy) / cs);
        bool onGrid = UI::Hover(r) && gx >= 0 && gy >= 0 && gx < kCols && gy < kRows;
        int cell = onGrid ? Idx(gx, gy) : -1;

        if (onGrid) UI::SetCursor(cell == startCell || cell == endCell ? MOUSE_CURSOR_RESIZE_ALL : MOUSE_CURSOR_CROSSHAIR);

        if (onGrid && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (cell == startCell) draggingMarker = 1;
            else if (cell == endCell) draggingMarker = 2;
            else paintValue = brush;
        }
        if (onGrid && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) paintValue = Empty;
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            draggingMarker = 0;
            paintValue = -1;
        }
        if (!onGrid) return;

        if (draggingMarker && grid[cell] != Wall && cell != startCell && cell != endCell) {
            (draggingMarker == 1 ? startCell : endCell) = cell;
            Changed();
        } else if (paintValue >= 0 && cell != startCell && cell != endCell && grid[cell] != paintValue) {
            grid[cell] = (unsigned char)paintValue;
            Changed();
        }
    }

    void Scatter(unsigned char type, int percent) {
        for (int i = 0; i < kCols * kRows; i++) {
            if (i == startCell || i == endCell) continue;
            if (grid[i] == type) grid[i] = Empty;
            if (grid[i] == Empty && UI::RandInt(1, 100) <= percent) grid[i] = type;
        }
        Changed();
    }

    void Maze() {
        // Recursive backtracker on odd coordinates.
        std::fill(grid.begin(), grid.end(), Wall);
        std::vector<std::pair<int, int>> stack{{1, 1}};
        grid[Idx(1, 1)] = Empty;
        while (!stack.empty()) {
            auto [x, y] = stack.back();
            std::vector<std::pair<int, int>> opts;
            for (auto [dx, dy] : {std::pair{2, 0}, {-2, 0}, {0, 2}, {0, -2}}) {
                int nx = x + dx, ny = y + dy;
                if (nx > 0 && ny > 0 && nx < kCols - 1 && ny < kRows - 1 && grid[Idx(nx, ny)] == Wall) opts.push_back({nx, ny});
            }
            if (opts.empty()) { stack.pop_back(); continue; }
            auto [nx, ny] = opts[UI::RandInt(0, (int)opts.size() - 1)];
            grid[Idx((x + nx) / 2, (y + ny) / 2)] = Empty;
            grid[Idx(nx, ny)] = Empty;
            stack.push_back({nx, ny});
        }
        // Knock out a few extra walls so there are multiple routes.
        for (int k = 0; k < 40; k++) {
            int x = UI::RandInt(1, kCols - 2), y = UI::RandInt(1, kRows - 2);
            if ((x + y) % 2 == 1) grid[Idx(x, y)] = Empty;
        }
        startCell = Idx(1, 1);
        endCell = Idx(kCols - 2, kRows - 2);
        grid[startCell] = grid[endCell] = Empty;
        Changed();
    }

    int Cost(int i) const { return grid[i] == Heavy ? kWeightCost : 1; }
    int H(int i) const { return std::abs(i % kCols - endCell % kCols) + std::abs(i / kCols - endCell / kCols); }

    std::vector<int> Neighbours(int i) const {
        std::vector<int> out;
        int x = i % kCols, y = i / kCols;
        const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= kCols || ny >= kRows) continue;
            int n = Idx(nx, ny);
            if (grid[n] != Wall) out.push_back(n);
        }
        return out;
    }

    void Run(bool instant) {
        frames.clear();
        int N = kCols * kRows;
        std::vector<unsigned char> vis(N, 0);
        std::vector<int> prev(N, -1), g(N, INT32_MAX);
        int visited = 0;
        bool found = false;
        auto snap = [&](int current, std::string msg) { frames.push_back({vis, current, visited, std::move(msg)}); };
        const char* name = kAlgoNames[algo];
        snap(-1, UI::Fmt("%s: start searching from S.", name));

        g[startCell] = 0;
        if (algo == 0 || algo == 1) {
            // BFS uses a queue, DFS a stack; both ignore weights.
            std::deque<int> dq{startCell};
            vis[startCell] = 1;
            while (!dq.empty()) {
                int u;
                if (algo == 0) { u = dq.front(); dq.pop_front(); }
                else { u = dq.back(); dq.pop_back(); }
                if (algo == 1 && vis[u] == 2) continue;
                vis[u] = 2;
                visited++;
                if (u == endCell) { found = true; break; }
                for (int v : Neighbours(u)) {
                    if (algo == 0 ? vis[v] != 0 : vis[v] == 2) continue;
                    vis[v] = 1;
                    prev[v] = u;
                    dq.push_back(v);
                }
                snap(u, UI::Fmt("%s: exploring… %d cells so far (%s)", name, visited, algo == 0 ? "queue - expands in rings" : "stack - dives deep first"));
            }
        } else {
            using QE = std::pair<int, int>; // priority, cell
            std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
            auto prio = [&](int c) { return algo == 2 ? g[c] : algo == 3 ? g[c] + H(c) : H(c); };
            pq.push({prio(startCell), startCell});
            vis[startCell] = 1;
            while (!pq.empty()) {
                auto [p, u] = pq.top();
                pq.pop();
                if (vis[u] == 2) continue;
                vis[u] = 2;
                visited++;
                if (u == endCell) { found = true; break; }
                for (int v : Neighbours(u)) {
                    if (vis[v] == 2) continue;
                    int ng = g[u] + Cost(v);
                    if (algo == 4 ? vis[v] == 0 : ng < g[v]) {
                        g[v] = ng;
                        prev[v] = u;
                        vis[v] = 1;
                        pq.push({prio(v), v});
                    }
                }
                snap(u, algo == 2 ? UI::Fmt("Dijkstra: expand the cheapest cell (cost %d)", g[u])
                        : algo == 3 ? UI::Fmt("A*: expand lowest cost + estimate = %d + %d", g[u], H(u))
                                    : UI::Fmt("Greedy: expand the cell that looks closest (estimate %d)", H(u)));
            }
        }

        pathLen = -1;
        pathCost = 0;
        if (found) {
            std::vector<int> path;
            for (int c = endCell; c != -1; c = prev[c]) path.push_back(c);
            std::reverse(path.begin(), path.end());
            pathLen = (int)path.size() - 1;
            for (size_t k = 1; k < path.size(); k++) pathCost += Cost(path[k]);
            for (int c : path) {
                vis[c] = 3;
                snap(-1, "Found the goal! Tracing the path back through each cell's parent…");
            }
            frames.back().msg = UI::Fmt("%s found a path: %d steps, cost %d, after exploring %d cells.", name, pathLen, pathCost, visited);
        } else {
            snap(-1, UI::Fmt("%s explored every reachable cell (%d) - the goal is walled off!", name, visited));
        }

        pb.Load((int)frames.size(), !instant);
        if (instant) pb.JumpEnd();
        live = instant;
    }
};

} // namespace

std::unique_ptr<Scene> MakePathfindingScene() { return std::make_unique<PathfindingScene>(); }
