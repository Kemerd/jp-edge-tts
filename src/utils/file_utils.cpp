/**
 * @file file_utils.cpp
 * @brief Implementation of file system utilities
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/utils/file_utils.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

namespace jp_edge_tts {

bool FileUtils::Exists(const std::string& path) {
    return fs::exists(path);
}

bool FileUtils::IsDirectory(const std::string& path) {
    return fs::is_directory(path);
}

std::string FileUtils::ReadTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return "";
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(size, '\0');
    file.read(&content[0], size);

    return content;
}

std::vector<uint8_t> FileUtils::ReadBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

bool FileUtils::WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::out | std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(content.c_str(), content.size());
    return file.good();
}

bool FileUtils::WriteBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool FileUtils::CreateDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directory(path, ec) || fs::exists(path);
}

bool FileUtils::CreateDirectories(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec) || fs::exists(path);
}

std::vector<std::string> FileUtils::ListFiles(const std::string& directory, const std::string& extension) {
    std::vector<std::string> files;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            if (extension.empty() || entry.path().extension() == extension) {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}

int64_t FileUtils::GetFileSize(const std::string& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    return ec ? -1 : static_cast<int64_t>(size);
}

std::string FileUtils::GetExtension(const std::string& path) {
    return fs::path(path).extension().string();
}

std::string FileUtils::GetStem(const std::string& path) {
    return fs::path(path).stem().string();
}

std::string FileUtils::GetFilename(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string FileUtils::GetDirectory(const std::string& path) {
    return fs::path(path).parent_path().string();
}

std::string FileUtils::JoinPath(const std::string& base, const std::string& relative) {
    return (fs::path(base) / relative).string();
}

std::string FileUtils::GetTempDirectory() {
#ifdef _WIN32
    char* temp = std::getenv("TEMP");
    if (!temp) temp = std::getenv("TMP");
    return temp ? temp : "C:\\Temp";
#else
    return "/tmp";
#endif
}

bool FileUtils::DeleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec);
}

bool FileUtils::CopyFile(const std::string& source, const std::string& destination) {
    std::error_code ec;
    return fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
}

bool FileUtils::MoveFile(const std::string& source, const std::string& destination) {
    std::error_code ec;
    fs::rename(source, destination, ec);
    return !ec;
}

} // namespace jp_edge_tts