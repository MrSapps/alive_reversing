#pragma once

#include "file_system.hpp"

class ThreadPool;
class ConversionProgress;

// fmvVersion is DataConversion::DataVersions::LatestVersion().mFmvVersion - stamped into a
// per-theme-dir manifest (see fmv_converter.cpp's FmvConversionManifest) as each movie finishes,
// so an interrupted conversion (killed, or cancelled because the user quit - see ThreadPool::
// RequestCancel/Engine::Run()'s quit handling) can resume without redoing already-completed movies,
// while a version bump (which changes the actual encoding settings) still forces everything to
// be redone rather than trusting old-version leftovers.
//
// progress's Fmvs category total is populated here from an upfront per-movie frame-count dry run
// (AE: header read; AO: a full decode scan wave, since raw PSX STR streams have no frame-count
// header - see fmv_converter.cpp) before any real encoding is dispatched.
void ConvertFMVs(ThreadPool& tp, FileSystem& fs, const FileSystem::Path& dataDir, bool isAo, u32 fmvVersion, ConversionProgress& progress);
