#pragma once

#include "Scene.h"
#include <memory>

std::unique_ptr<Scene> MakeSortingScene();
std::unique_ptr<Scene> MakeStackQueueScene();
std::unique_ptr<Scene> MakeLinkedListScene();
std::unique_ptr<Scene> MakeBSTScene();
std::unique_ptr<Scene> MakeHeapScene();
std::unique_ptr<Scene> MakeHashTableScene();
std::unique_ptr<Scene> MakeGraphScene();
std::unique_ptr<Scene> MakePathfindingScene();
