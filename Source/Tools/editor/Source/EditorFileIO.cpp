#include "EditorFileIO.hpp"
#include "File.hpp"

std::unique_ptr<ReliveAPI::IFile> EditorFileIO::Open(const std::string& fileName, ReliveAPI::IFileIO::Mode mode)
{
    auto ret = std::make_unique<File>(fileName, mode);
    if (!ret->IsOpen())
    {
        return nullptr;
    }
    return ret;
}
