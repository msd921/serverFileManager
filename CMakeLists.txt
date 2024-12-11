# Устанавливаем минимальную версию CMake и название проекта
cmake_minimum_required(VERSION 3.20)
project(serverFileManager)
cmake_policy(SET CMP0074 NEW)

# Устанавливаем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Установка _WIN32_WINNT для поддержки Windows 10
add_definitions(-D_WIN32_WINNT=0x0A00)

# Добавляем Boost
find_package(Boost REQUIRED COMPONENTS system filesystem)

if(Boost_FOUND)
    message(STATUS "Boost found!")
    message(STATUS "Boost include dir: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARYDIR})
else()
    message(FATAL_ERROR "Boost not found. Please verify BOOST_ROOT path and Boost installation.")
endif()

include_directories(include)

# Определяем исполняемый файл и включаем исходные файлы
add_executable(serverFileManager 
    src/main.cpp 
    src/server.cpp
    src/commands.cpp
    src/file_manager.cpp
    include/server.h 
    include/file_manager.h
    include/commands.h )

# Линкуем библиотеки
target_link_libraries(serverFileManager ${Boost_LIBRARIES})

#message(STATUS "Boost library dir: ${Boost_LIBRARY_DIRS}")
