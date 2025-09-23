/**
 * @file file_utils.h
 * @brief File system utilities
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_FILE_UTILS_H
#define JP_EDGE_TTS_FILE_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

namespace jp_edge_tts {

/**
 * @class FileUtils
 * @brief Utility functions for file operations
 */
class FileUtils {
public:
    /**
     * @brief Check if file exists
     *
     * @param path File path
     * @return true if file exists
     */
    static bool Exists(const std::string& path);

    /**
     * @brief Check if path is directory
     *
     * @param path Path to check
     * @return true if directory
     */
    static bool IsDirectory(const std::string& path);

    /**
     * @brief Read entire file as string
     *
     * @param path File path
     * @return File contents as string
     */
    static std::string ReadTextFile(const std::string& path);

    /**
     * @brief Read entire file as binary
     *
     * @param path File path
     * @return File contents as byte array
     */
    static std::vector<uint8_t> ReadBinaryFile(const std::string& path);

    /**
     * @brief Write string to file
     *
     * @param path File path
     * @param content String content
     * @return true if successful
     */
    static bool WriteTextFile(const std::string& path, const std::string& content);

    /**
     * @brief Write binary data to file
     *
     * @param path File path
     * @param data Binary data
     * @return true if successful
     */
    static bool WriteBinaryFile(const std::string& path,
                                const std::vector<uint8_t>& data);

    /**
     * @brief Create directory
     *
     * @param path Directory path
     * @return true if successful or already exists
     */
    static bool CreateDirectory(const std::string& path);

    /**
     * @brief Create directory recursively
     *
     * @param path Directory path
     * @return true if successful
     */
    static bool CreateDirectories(const std::string& path);

    /**
     * @brief List files in directory
     *
     * @param directory Directory path
     * @param extension Optional file extension filter
     * @return List of file paths
     */
    static std::vector<std::string> ListFiles(const std::string& directory,
                                              const std::string& extension = "");

    /**
     * @brief Get file size
     *
     * @param path File path
     * @return Size in bytes, or -1 if error
     */
    static int64_t GetFileSize(const std::string& path);

    /**
     * @brief Get file extension
     *
     * @param path File path
     * @return Extension including dot
     */
    static std::string GetExtension(const std::string& path);

    /**
     * @brief Get filename without extension
     *
     * @param path File path
     * @return Filename stem
     */
    static std::string GetStem(const std::string& path);

    /**
     * @brief Get filename from path
     *
     * @param path File path
     * @return Filename with extension
     */
    static std::string GetFilename(const std::string& path);

    /**
     * @brief Get directory from path
     *
     * @param path File path
     * @return Parent directory
     */
    static std::string GetDirectory(const std::string& path);

    /**
     * @brief Join path components
     *
     * @param base Base path
     * @param relative Relative path
     * @return Joined path
     */
    static std::string JoinPath(const std::string& base, const std::string& relative);

    /**
     * @brief Get temporary directory
     * @return Temporary directory path
     */
    static std::string GetTempDirectory();

    /**
     * @brief Delete file
     *
     * @param path File path
     * @return true if successful
     */
    static bool DeleteFile(const std::string& path);

    /**
     * @brief Copy file
     *
     * @param source Source path
     * @param destination Destination path
     * @return true if successful
     */
    static bool CopyFile(const std::string& source, const std::string& destination);

    /**
     * @brief Move/rename file
     *
     * @param source Source path
     * @param destination Destination path
     * @return true if successful
     */
    static bool MoveFile(const std::string& source, const std::string& destination);
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_FILE_UTILS_H