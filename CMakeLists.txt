# Устанавливаем минимальную версию CMake и название проекта
cmake_minimum_required(VERSION 3.8)
project(serverFileManager)

# Устанавливаем стандарт C++20, если поддерживается
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Добавляем Boost, если он нужен
set(BOOST_ROOT "C:/Program Files/boost/boost_1_86_0")
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(include)

# Определяем исполняемый файл и включаем исходные файлы
add_executable(serverFileManager 
    src/main.cpp 
    src/server.cpp 
    include/server.h 
    include/serverFileManager.h
)

# Линкуем библиотеки
target_link_libraries(serverFileManager ${Boost_LIBRARIES})
