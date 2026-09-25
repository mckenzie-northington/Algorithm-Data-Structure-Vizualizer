# AlgoPlayground — Algorithm & Data Structure Visualizer (C++ / raylib)

An interactive desktop playground for data structures and algorithms. Instead of only
watching canned animations, you type your own values, edit data directly with the mouse,
build your own graphs and mazes, and step forwards/backwards through every operation with
a plain-English explanation of what is happening.

## What you can play with

| Tab | Try this |
|---|---|
| **Sorting** | Bubble, Selection, Insertion, Quick, Merge, Heap sort. Type your own array, pick presets (random / reversed / nearly sorted / few unique), resize the array, or **drag any bar** to change its value. Live comparison & swap counters. |
| **Stack & Queue** | Push/pop/peek, enqueue/dequeue, overflow & underflow, plus a **bracket checker** that uses a stack to test strings like `{[()()]}`. |
| **Linked List** | Insert at head/tail/index, search, delete, and an **in-place reverse** that shows `prev` / `curr` / `next` pointers flipping. |
| **BST** | Insert (one value or a list), search, delete (all 3 cases, including in-order successor), min/max, and in/pre/post/level-order traversals. Build a "sorted" tree to see the O(n) worst case. |
| **Heap** | Min-heap or max-heap, shown as a tree **and** the underlying array. Insert with sift-up, extract with sift-down, O(n) build-heap, and heap sort. Hover any node to see the parent/child index formulas. |
| **Hash Table** | Separate chaining vs. linear probing (with tombstones). Change the table size to watch keys rehash, see the hash computation and load factor. |
| **Graph** | Click to add nodes, drag between nodes to add weighted edges, scroll to change weights, toggle directed. Run BFS, DFS, Dijkstra and Prim's MST; after Dijkstra, hover a node to see its shortest path. |
| **Pathfinding** | Paint walls and weighted cells on a grid, drag start/goal, generate mazes. Compare BFS, DFS, Dijkstra, A* and Greedy best-first. After a run, moving the start/goal re-solves instantly. |

## Controls

- **Playback bar** (bottom): jump to start, step back, play/pause, step forward, jump to end,
  a scrubber to seek anywhere, and a speed slider.
- `Space` play/pause · `←` / `→` step · `Home` / `End` jump (when not typing in a text box).
- Text boxes accept a single number or a comma-separated list; press `Enter` to submit, `Esc` to leave the box.
- The side panel scrolls with the mouse wheel on small windows.

## Building

Prerequisites: a C++17 compiler and CMake ≥ 3.15. raylib is downloaded automatically.

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Then run `build/Release/AlgorithmVisualizer.exe` (Windows) or `build/AlgorithmVisualizer`.

## Project Structure
```
algorithm-visualizer/
├── CMakeLists.txt          # Build config; fetches raylib via FetchContent
├── include/
│   ├── UI.h                # Theme colours + tiny immediate-mode UI toolkit
│   ├── Scene.h             # Scene interface + Playback (shared timeline)
│   └── Scenes.h            # Factory functions for each tab
└── src/
    ├── main.cpp            # Window, DPI scaling, tabs, layout, shortcuts
    ├── UI.cpp              # Buttons, text inputs, sliders, drawing helpers
    ├── Playback.cpp        # Play/pause/step/scrub/speed logic and bar
    ├── SortingScene.cpp
    ├── StackQueueScene.cpp
    ├── LinkedListScene.cpp
    ├── BSTScene.cpp
    ├── HeapScene.cpp
    ├── HashTableScene.cpp
    ├── GraphScene.cpp
    └── PathfindingScene.cpp
```

## How it works

Every operation is **recorded, then played back**. When you press a button, the scene runs
the real algorithm once and records a list of frames (a snapshot of the structure, what to
highlight, and a one-line explanation). The shared `Playback` timeline then only decides
*which* frame to show. That one design gives every tab the same pause / step-back / scrub /
speed controls for free, and cleanly separates algorithm logic from rendering and timing.

Rendering is immediate-mode: each frame the scene draws the current snapshot, and nodes glide
to their new positions by easing toward per-node target positions (keyed by a stable id), so
swaps, rotations and insertions animate smoothly without any per-algorithm animation code.
