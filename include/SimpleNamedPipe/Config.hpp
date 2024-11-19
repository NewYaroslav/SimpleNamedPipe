#pragma once
#ifndef SIMLPE_NAMED_PIPE_CONFIG_HPP_INCLUDED
#define SIMLPE_NAMED_PIPE_CONFIG_HPP_INCLUDED

/// \file Config.hpp
/// \brief

#include <string>

namespace SimpleNamedPipe {

    /// \class
    /// \brief
    class Config {
    public:
        std::string pipe_name;  ///<  Имя именованного канала
        size_t buffer_size;     ///< Размер буфера для чтения и записи
        size_t timeout;         ///< Время ожидания в миллисекундах

        /// \brief
        /// \param
        /// \param
        /// \param
        Config(const std::string& pipe_name = "server",
               size_t buffer_size = 2048,
               size_t timeout = 50)
            : pipe_name(pipe_name), buffer_size(buffer_size), timeout(timeout) {}
    };
}

#endif // NAMED_PIPE_CONFIG_HPP_INCLUDED
