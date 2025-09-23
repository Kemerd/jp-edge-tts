/**
 * @file string_utils.h
 * @brief String manipulation utilities
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_STRING_UTILS_H
#define JP_EDGE_TTS_STRING_UTILS_H

#include <string>
#include <vector>

namespace jp_edge_tts {

/**
 * @class StringUtils
 * @brief Utility functions for string manipulation
 */
class StringUtils {
public:
    /**
     * @brief Split string by delimiter
     *
     * @param str Input string
     * @param delimiter Delimiter character
     * @return Vector of split strings
     */
    static std::vector<std::string> Split(const std::string& str, char delimiter);

    /**
     * @brief Split string by string delimiter
     *
     * @param str Input string
     * @param delimiter Delimiter string
     * @return Vector of split strings
     */
    static std::vector<std::string> Split(const std::string& str,
                                          const std::string& delimiter);

    /**
     * @brief Join strings with delimiter
     *
     * @param strings Vector of strings
     * @param delimiter Delimiter string
     * @return Joined string
     */
    static std::string Join(const std::vector<std::string>& strings,
                           const std::string& delimiter);

    /**
     * @brief Trim whitespace from both ends
     *
     * @param str Input string
     * @return Trimmed string
     */
    static std::string Trim(const std::string& str);

    /**
     * @brief Convert string to lowercase
     *
     * @param str Input string
     * @return Lowercase string
     */
    static std::string ToLower(const std::string& str);

    /**
     * @brief Convert string to uppercase
     *
     * @param str Input string
     * @return Uppercase string
     */
    static std::string ToUpper(const std::string& str);

    /**
     * @brief Replace all occurrences of substring
     *
     * @param str Input string
     * @param from Substring to replace
     * @param to Replacement string
     * @return Modified string
     */
    static std::string Replace(const std::string& str,
                              const std::string& from,
                              const std::string& to);

    /**
     * @brief Check if string starts with prefix
     *
     * @param str Input string
     * @param prefix Prefix to check
     * @return true if starts with prefix
     */
    static bool StartsWith(const std::string& str, const std::string& prefix);

    /**
     * @brief Check if string ends with suffix
     *
     * @param str Input string
     * @param suffix Suffix to check
     * @return true if ends with suffix
     */
    static bool EndsWith(const std::string& str, const std::string& suffix);

    /**
     * @brief Convert UTF-8 to UTF-32
     *
     * @param utf8 UTF-8 string
     * @return UTF-32 string
     */
    static std::u32string UTF8ToUTF32(const std::string& utf8);

    /**
     * @brief Convert UTF-32 to UTF-8
     *
     * @param utf32 UTF-32 string
     * @return UTF-8 string
     */
    static std::string UTF32ToUTF8(const std::u32string& utf32);

    /**
     * @brief Check if string contains only ASCII
     *
     * @param str Input string
     * @return true if ASCII only
     */
    static bool IsASCII(const std::string& str);

    /**
     * @brief Generate hash for string
     *
     * @param str Input string
     * @return Hash value
     */
    static size_t Hash(const std::string& str);
};

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_STRING_UTILS_H