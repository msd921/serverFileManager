#pragma once

#include <string>

enum class CommandType {
    ListFiles,
    GetFile,
    UploadFile,
    Invalid // ��� ����������� ������
};

CommandType parse_command(const std::string& command);