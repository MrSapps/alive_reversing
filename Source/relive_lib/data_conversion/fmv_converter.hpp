#pragma once

#include "file_system.hpp"

class ThreadPool;

// fmvVersion is DataConversion::DataVersions::LatestVersion().mFmvVersion - stamped into a
// per-theme-dir manifest (see fmv_converter.cpp's FmvConversionManifest) as each movie finishes,
// so an interrupted conversion (killed, or cancelled because the user quit - see ThreadPool::
// RequestCancel/Engine::Run()'s quit handling) can resume without redoing already-completed movies,
// while a version bump (which changes the actual encoding settings) still forces everything to
// be redone rather than trusting old-version leftovers.
void ConvertFMVs(ThreadPool& tp, FileSystem& fs, const FileSystem::Path& dataDir, bool isAo, u32 fmvVersion);
