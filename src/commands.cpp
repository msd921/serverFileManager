#include "commands.h"

CommandType parse_command(const std::string& command)
{
    if (command == "LIST")
        return CommandType::ListFiles;
    if (command == "GET")
        return CommandType::GetFile;
    if (command == "UPLOAD")
        return CommandType::UploadFile;
    return CommandType::Invalid;
}
