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
	if (is_first_pass_for_writing || (current_filename_for_writing != filename))
	{
		if (write_stream.is_open()) {
			write_stream.close();
		}
		current_filename_for_writing = filename;
		is_first_pass_for_writing = false;
		write_stream = std::ofstream((base_directory_ + "/" + filename), std::ios::app | std::ios::binary);
		if (!write_stream) {
			throw std::runtime_error("Не удалось открыть файл для записи: " + current_filename_for_writing);
		}
	}

	write_stream.write(data.data(), data.size());
	if (!write_stream.good()) {
		throw std::runtime_error("Ошибка записи в файл - вообще не гуд" );
	}
	if (!write_stream) {
		throw std::runtime_error("Ошибка записи в файл: " + current_filename_for_writing);
	}
}

std::string FileHandler::create_file(const string& filename, bool overwrite)
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
	new_file.close();
	target_filename.erase(0, base_directory_.size() + 1);
	return target_filename;
}

void FileHandler::close_file()
{
	if (write_stream.is_open()) {
		write_stream.close();
	}
	is_first_pass_for_writing = true;
	if (read_stream.is_open()) {
		read_stream.close();
	}
	is_first_pass_for_reading = true;
}

void FileHandler::read_from_file(const std::string& filename, std::ostream& output_stream, std::size_t offset, std::size_t chunk_size)
{
	if (is_first_pass_for_reading || (current_filename_for_reading != filename)) {
		if (read_stream.is_open()) {
			read_stream.close();
		}
		current_filename_for_reading = filename;
		is_first_pass_for_reading = false;
		read_stream = std::ifstream(base_directory_ + "/" + filename, std::ios::binary);
		if (!read_stream) {
			throw std::runtime_error("Не удалось открыть файл для чтения: " + filename);
		}
	}
	read_stream.clear();
	// Переходим к нужной позиции в файле
	read_stream.seekg(offset, std::ios::beg);
	if (!read_stream) {
		throw std::runtime_error("Ошибка позиционирования в файле: " + filename);
	}

	// Читаем данные
	std::vector<char> buffer(chunk_size);
	read_stream.read(buffer.data(), chunk_size);
	output_stream.write(buffer.data(), read_stream.gcount());
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
	std::string base_name = fs::path(filename).stem().string(); // Имя файла без расширения
	std::string extension = fs::path(filename).extension().string();

	std::string new_filename = filename;
	int count = 1;

	while (file_exists(new_filename)) {
		std::ostringstream oss;
		oss << base_name << "_copy" << count++ << extension;
		new_filename = oss.str();
	}
	return (base_directory_ + "/" + new_filename);
}

bool FileHandler::file_exists(const std::string& filename)
{
	return fs::exists(base_directory_ + "/" + filename);
}
