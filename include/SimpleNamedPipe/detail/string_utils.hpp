#pragma once
#ifndef SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_DETAIL_STRING_UTILS_HPP_INCLUDED
#define SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_DETAIL_STRING_UTILS_HPP_INCLUDED

#include <windows.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace SimpleNamedPipe {
namespace detail {

    inline std::wstring utf8_to_wide(const std::string& value) {
        if (value.empty()) {
            return {};
        }
        if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
            throw std::length_error("Named pipe string is too long");
        }

        const int input_size = static_cast<int>(value.size());
        const int output_size = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            input_size,
            nullptr,
            0);

        if (output_size <= 0) {
            throw std::system_error(
                static_cast<int>(GetLastError()),
                std::system_category(),
                "Failed to convert UTF-8 named pipe string to UTF-16");
        }

        std::wstring output(static_cast<size_t>(output_size), L'\0');
        const int converted = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            input_size,
            &output[0],
            output_size);

        if (converted != output_size) {
            throw std::system_error(
                static_cast<int>(GetLastError()),
                std::system_category(),
                "Failed to write converted UTF-16 named pipe string");
        }

        return output;
    }

} // namespace detail
} // namespace SimpleNamedPipe

#endif // SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_DETAIL_STRING_UTILS_HPP_INCLUDED
