#pragma once

#include <string>
#include <vector>

using namespace std;

class FileHandler
{
public:
	FileHandler(const string& base_dir);
	std::vector<std::string> list_files() const;
	void add_to_file(const string& filename, const string& data);
	string write_to_file(const string& filename, const string& data, bool overwrite);
	void read_from_file(const std::string& filename, std::ostream& output_stream, std::size_t offset, std::size_t chunk_size);
	void delete_file(const std::string& filename);
	size_t get_file_size(const string& filename);
	bool file_exists(const std::string& filename);
private:
	string get_unique_filename(const string& filename); //  если файл уже существует и мы его не перезаписываем

	string base_directory_;
};