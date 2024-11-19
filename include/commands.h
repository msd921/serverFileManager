#pragma once

#include <string>

enum class CommandType {
    ListFiles,
    GetFile,
    UploadFile,
    Invalid // Для неизвестных команд
};

CommandType parse_command(const std::string& command);