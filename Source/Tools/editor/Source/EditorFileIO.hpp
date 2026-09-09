#pragma once

#include "relive_api.hpp"
#include "file_api.hpp"

class EditorFileIO final : public ReliveAPI::IFileIO
{
public:
    std::unique_ptr<ReliveAPI::IFile> Open(const std::string& fileName, ReliveAPI::IFileIO::Mode mode) override;
};
