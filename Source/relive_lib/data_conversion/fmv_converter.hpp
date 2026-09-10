#pragma once

#include "file_system.hpp"

class ThreadPool;

void ConvertFMVs(ThreadPool& tp, FileSystem& fs, const FileSystem::Path& dataDir, bool isAo);
