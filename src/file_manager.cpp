#include "file_manager.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

FileHandler::FileHandler(const string& base_dir)
	: base_directory_(base_dir)
{
	if (!fs::exists(base_directory_)) {
		fs::create_directory(base_directory_);
	}
}

std::vector<std::string> FileHandler::list_files() const
{
	std::vector<std::string> file_list;
	for (auto& entry : fs::directory_iterator(base_directory_)) {
		file_list.push_back(entry.path().filename().string() + "\n");
	}
	return file_list;
}

void FileHandler::add_to_file(const string& filename, const string& data)
{
	std::string target_filename = base_directory_ + "/" + filename;

	std::ofstream file(target_filename, std::ios::app | std::ios::binary);

	if (!file) {
		throw std::runtime_error("Не удалось открыть файл для записи: " + target_filename);
	}
	file.write(data.data(), data.size());
	file.close();
}

std::string FileHandler::write_to_file(const string& filename, const string& data, bool overwrite)
{
	std::string target_filename = base_directory_ + "/" + filename;

	if (!overwrite && file_exists(filename)) {
		target_filename = get_unique_filename(filename);
		std::cout << "Файл уже существует. Сохраняю как: " << target_filename << "\n";
	}

	std::ofstream new_file(target_filename, std::ios::binary);
	if (!new_file) {
		throw std::runtime_error("Не удалось открыть файл для записи: " + target_filename);
	}
	new_file.write(data.data(), data.size());
	new_file.close();
	target_filename.erase(0, base_directory_.size() + 1);
	return target_filename;
}

void FileHandler::read_from_file(const std::string& filename, std::ostream& output_stream, std::size_t offset, std::size_t chunk_size)
{
	std::ifstream file(base_directory_ + "/" + filename, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Не удалось открыть файл для чтения: " + filename);
	}

	// Переходим к нужной позиции в файле
	file.seekg(offset, std::ios::beg);
	if (!file) {
		throw std::runtime_error("Ошибка позиционирования в файле: " + filename);
	}

	// Читаем данные
	std::vector<char> buffer(chunk_size);
	file.read(buffer.data(), chunk_size);
	output_stream.write(buffer.data(), file.gcount());
}

void FileHandler::delete_file(const std::string& filename)
{
	if (fs::remove(base_directory_ + "/" + filename) == 0)
	{
		std::cout << "Файл " << filename << " успешно удален" << std::endl;
	}
	else {
		std::cout << "При удалении файла " << filename << " произошла ошибка" << std::endl;
	}
}

std::size_t FileHandler::get_file_size(const string& filename)
{
	if (!file_exists(filename)) {
		return 0;
	}
	return fs::file_size(base_directory_ + "/" + filename);
}

string FileHandler::get_unique_filename(const string& filename)
{
	std::string new_filename = filename;
	int count = 1;

	while (file_exists(new_filename)) {
		std::ostringstream oss;
		oss << filename << "_copy" << count++ << fs::path(filename).extension().string();
		new_filename = oss.str();
	}
	return (base_directory_ + "/" + new_filename);
}

bool FileHandler::file_exists(const std::string& filename)
{
	return fs::exists(base_directory_ + "/" + filename);
}
