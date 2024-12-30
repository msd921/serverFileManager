#include "server.h"
#include <iostream>
#include <boost/asio.hpp>
#include "commands.h"
#include <fstream>

using boost::asio::ip::tcp;


tcp::socket& tcp_connection::socket()
{
	return socket_;
}

void tcp_connection::start()
{
	// Включаем опции сокета для улучшения работы соединения
	boost::asio::ip::tcp::no_delay no_delay_option(true);
	boost::asio::socket_base::keep_alive keep_alive_option(true);

	socket_.set_option(no_delay_option);   // Отключаем Nagle's algorithm
	socket_.set_option(keep_alive_option); // Включаем TCP Keep-Alive

	read_command();
}

tcp_connection::tcp_connection(boost::asio::io_context& io_context)
	: socket_(io_context),
	file_manager_(std::make_unique<FileHandler>("/home/msd/gitHubfiles/serverFile"))
{
}

void tcp_connection::read_command()
{
	auto self = shared_from_this();

	boost::asio::async_read_until(socket_, buffer_, '\n',
		[this, self](const boost::system::error_code& error, std::size_t bytes_transferred) {
			handle_read(error, bytes_transferred);
		});
}

void tcp_connection::handle_read(const boost::system::error_code& error, std::size_t bytes_transferred/*, boost::asio::streambuf* buffer_ptr*/)
{
	//std::cout << "[DEBUG] Вызов async_read_until завершен. Ошибка: " << error.message() << std::endl;
	if (!error) {
		try {
			// Проверяем, что в буфере есть данные
			if (bytes_transferred > 0) {
				std::istream is(&buffer_);
				std::string command;
				std::getline(is, command);

				//  очистка буфера после использования
				buffer_.consume(buffer_.size());

				// Убираем лишние символы
				if (!command.empty() && command.back() == '\r') {
					command.pop_back();
				}

				std::string command_copy = command;

				std::cout << "Получена команда: " << command_copy << std::endl;

				// Обработка команды
				handle_command(command_copy);
				// Продолжаем читать следующую 
				if (!is_uploading_file) {
					read_command();
				}
			}
			else {
				std::cerr << "Буфер пуст, пропускаем обработку" << std::endl;
				read_command(); // Повторяем чтение
			}
		}
		catch (const std::exception& e) {
			std::cout << "Исключение при обработке команды: " << e.what() << std::endl;
			socket_.close(); // Закрываем соединение
		}
	}
	else {
		std::cout << "Ошибка чтения команды: " << error.message() << std::endl;
		socket_.close(); // Закрываем соединение при ошибке
	}
}

void tcp_connection::handle_command(const std::string& command)
{
	size_t space_pos = command.find(' '); // Находим первый пробел
	std::string key_command = command.substr(0, space_pos); // Извлекаем первое слово
	CommandType commandType = parse_command(key_command);


	switch (commandType) {
	case CommandType::ListFiles:
		std::cout << "Команда: LIST - Отправка списка файлов" << std::endl;
		send_file_list();
		break;

	case CommandType::GetFile:
		std::cout << "Команда: GET - Отправка файла клиенту" << std::endl;

		if (space_pos == std::string::npos || space_pos + 1 >= command.size()) {
			send_error_message("Missing filename for GET command");
		}
		else {
			std::string filename = command.substr(space_pos + 1); // Извлекаем имя файла
			send_file(filename);
		}
		break;

	case CommandType::UploadFile:
		space_pos = command.find_last_of(' ');
		if (space_pos == std::string::npos || space_pos + 1 >= command.size()) {
			send_error_message("Invalid UPLOAD command format");
		}
		else {
			std::size_t size_str_upload = std::size("UPLOAD");
			std::string new_filename = command.substr(size_str_upload, space_pos - size_str_upload);
			std::size_t file_size = std::stoull(command.substr(space_pos + 1));
			std::cout << "Прием файла " << new_filename << ", размер: " << file_size << " байт" << std::endl;
			is_uploading_file = true;
			receive_file(new_filename, file_size); // Начинаем прием
		}
		break;

	case CommandType::Invalid:
	default:
		std::cerr << "Неизвестная команда: " << command << std::endl;
		send_error_message("Unknown command");
		break;
	}
}

void tcp_connection::send_end_of_transfer()
{
	auto end_of_transfer_ptr = std::make_shared<std::string>("\nEND\n");

	auto self = shared_from_this();
	boost::asio::async_write(socket_, boost::asio::buffer(*end_of_transfer_ptr),
		[self, end_of_transfer_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (error) {
				std::cerr << "Ошибка при отправке сообщения об ошибке: " << error.message() << std::endl;
			}
		});
}

void tcp_connection::send_file_list()
{
	//  пока смотрим в текущей директории
	try {

		auto file_list = std::make_shared<std::string>();

		for (auto& entry : file_manager_->list_files()) {
			*file_list += entry.c_str();
		}

		// Асинхронно отправляем список файлов
		auto self = shared_from_this();
		boost::asio::async_write(socket_, boost::asio::buffer(*file_list),
			[self, file_list](const boost::system::error_code& error, std::size_t) {
				if (error) {
					std::cerr << "Ошибка отправки списка файлов: " << error.message() << std::endl;
				}
				self->send_end_of_transfer();
			});
	}
	catch (const std::exception& e) {
		std::cerr << "Исключение в send_file_list: " << e.what() << std::endl;
		send_error_message("Internal server error");
	}
}

void tcp_connection::send_file(const std::string& filename) {
	if (!file_manager_->file_exists(filename)) {
		send_error_message("file not found");
		return;
	}

	auto file_info_ptr = std::make_shared<std::string>(filename + " " + std::to_string(file_manager_->get_file_size(filename)) + "\n");
	auto self = shared_from_this();
	boost::asio::async_write(socket_, boost::asio::buffer(*file_info_ptr),
		[self, file_info_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (error) {
				std::cerr << "Ошибка при отправке готовности к приему: " << error.message() << std::endl;
				return;
			}
		});
	
	const std::size_t chunk_size = 1024 * 1024; // Размер блока 

	auto filename_ptr = std::make_shared<std::string>(filename);

	// Позиция в файле
	auto current_position = std::make_shared<std::size_t>(0);
	auto chunk_data = std::make_shared<std::string>();
	auto send_next_chunk = std::make_shared<std::function<void()>>();
	*send_next_chunk = [self, current_position, chunk_size, send_next_chunk, chunk_data, filename_ptr]() {
		try {
			// Буфер для хранения данных текущего блока

			std::ostringstream buffer_stream;
			;
			// Выполняем операцию
			
			// Читаем следующую часть файла
			self->file_manager_->read_from_file(filename_ptr.get()->c_str(), buffer_stream, *current_position, chunk_size);
			
			// Извлекаем данные в строку
			*chunk_data = buffer_stream.str();
			if (chunk_data->empty()) {
				// Все данные отправлены
				std::cout << "Файл полностью отправлен!" << std::endl;
				self->file_manager_->close_file();
				self->send_end_of_transfer();
				//self->notify_end_of_transfer();
				return;
			}

			*current_position += chunk_data->size(); // Обновляем текущую позицию
			auto start = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());
			// Отправляем данные через сокет
			boost::asio::async_write(
				self->socket_,
				boost::asio::buffer(*chunk_data),
				[self, send_next_chunk, start](const boost::system::error_code& error, std::size_t bytes_transferred) {
					if (error) {
						std::cerr << "Ошибка при отправке данных: " << error.message() << std::endl;
						self->send_end_of_transfer();
						return;
					}
					std::cout << "Отправлено: " << bytes_transferred << " байт." << std::endl;
					auto end = std::chrono::steady_clock::now();
					std::cout << "Время отправки блока: "
						<< std::chrono::duration_cast<std::chrono::milliseconds>(end - *start).count()
						<< " мс, байт: " << bytes_transferred << "\n";
					// Отправляем следующий блок
					(*send_next_chunk)();
				}
			);
		}
		catch (const std::exception& e) {
			self->file_manager_->close_file();
			std::cerr << "Ошибка при чтении или отправке данных: " << e.what() << std::endl;
		}
		};

	// Начинаем отправку
	(*send_next_chunk)();
}

void tcp_connection::receive_file(const std::string& filename, std::size_t file_size)
{
	std::string new_filename;
	try {
		new_filename = file_manager_->create_file(filename, false);
	}
	catch (const std::runtime_error& e) {
		send_error_message("Failed to open file for writing");
		return;
	}

	auto ready_to_transfer_ptr = std::make_shared<std::string>("READY\n");
	auto self = shared_from_this();
	boost::asio::async_write(socket_, boost::asio::buffer(*ready_to_transfer_ptr),
		[self, ready_to_transfer_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (error) {
				std::cerr << "Ошибка при отправке готовности к приему: " << error.message() << std::endl;
				return;
			}
		});

	auto new_filename_ptr = std::make_shared<std::string>(new_filename);
	auto buffer = std::make_shared<std::vector<char>>((1024*64));
	//auto self = shared_from_this();
	auto bytes_received = std::make_shared<std::size_t>(0);  // Отслеживаем объем принятых данных

	// Лямбда для чтения данных из сокета
	auto receive_next_block = std::make_shared<std::function<void()>>();
	*receive_next_block = [self, buffer, bytes_received, file_size, receive_next_block, new_filename_ptr]() {
		self->socket_.async_read_some(boost::asio::buffer(*buffer),
			[self, buffer, bytes_received, file_size, receive_next_block, new_filename_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
				if (!error) {
					// Увеличиваем счетчик принятых байтов
					*bytes_received += bytes_transferred;
					// Рассчитываем, сколько данных еще нужно принять
					std::size_t bytes_to_write = std::min(bytes_transferred, file_size - *bytes_received + bytes_transferred);
					
					// Преобразуем данные в std::string для записи
					std::string data_to_write(buffer->begin(), buffer->begin() + bytes_transferred);

					// Записываем данные в файл
					self->file_manager_->add_to_file(new_filename_ptr->c_str(), data_to_write);

					// Записываем данные в файл
					std::cout << "Принято: " << bytes_transferred << " байт, всего: " << *bytes_received << "/" << file_size << std::endl;

					// Проверяем, достигли ли ожидаемого размера файла
					if (*bytes_received >= file_size) {
						if (*bytes_received > file_size) {
							std::cout << "Ошибка - принялось байтов больше, чем должно быть!\n";
						}
						self->file_manager_->close_file();
						std::cout << "Прием файла завершен!" << std::endl;
						self->send_end_of_transfer();
						self->is_uploading_file = false;
						self->read_command();
					}
					else {
						(*receive_next_block)(); // Читаем следующую часть
					}
				}
				else if (error == boost::asio::error::eof) {
					self->file_manager_->close_file();
					std::cout << "Передача файла завершена досрочно!" << std::endl;
					self->send_end_of_transfer();
					self->is_uploading_file = false;
					self->read_command();
				}
				else {
					self->file_manager_->close_file();
					std::cerr << "Ошибка приема файла: " << error.message() << std::endl;
					self->send_error_message("File transfer error");
					self->is_uploading_file = false;
					self->read_command();
				}
			});
		};
	if (is_uploading_file) {
		// Начинаем прием данных
		(*receive_next_block)();
	}
	else {
		//read_command();
	}

}

void tcp_connection::send_error_message(const std::string& error_message) {

	auto error_message_ptr = std::make_shared<std::string>("ERROR: " + error_message + "\n");

	auto self = shared_from_this();
	boost::asio::async_write(socket_, boost::asio::buffer(*error_message_ptr),
		[self, error_message_ptr](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (error) {
				std::cerr << "Ошибка при отправке сообщения об ошибке: " << error.message() << std::endl;
			}
			else {
				std::cout << "Сообщение об ошибке отправлено (" << bytes_transferred << " байт)." << std::endl;
				self->send_end_of_transfer();
			}
		});
}

void tcp_connection::log_socket_state(const std::string& context)
{
	if (socket_.is_open()) {
		std::cout << "[" << context << "] Сокет открыт." << std::endl;
	}
	else {
		std::cerr << "[" << context << "] Сокет закрыт." << std::endl;
	}
}

FileServer::FileServer(boost::asio::io_context& io_context, short port)
	: io_context_(io_context),
	acceptor_(io_context) {
	boost::system::error_code ec;
	acceptor_.open(tcp::v4(), ec);
	if (ec) {
		std::cerr << "Ошибка при открытии сокета: " << ec.message() << std::endl;
	}
	acceptor_.bind(tcp::endpoint(tcp::v4(), port), ec);
	if (ec) {
		std::cerr << "Ошибка при привязке сокета: " << ec.message() << std::endl;
	}
	acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
	if (ec) {
		std::cerr << "Ошибка при прослушивании порта: " << ec.message() << std::endl;
	}

	std::cout << "Сервер слушает порт " << port << std::endl;  // Проверка
	start_accept();
};

void FileServer::start_accept() {
	new_connection = tcp_connection::create(io_context_);
	std::cout << "Перед подключением\n";
	acceptor_.async_accept(new_connection->socket(),
		[this](const boost::system::error_code& error) {
			if (error) {
				std::cout << "Ошибка при подключении: " << error.message() << std::endl;
			}
			else {
				std::cout << "Новое подключение принято\n";
				handle_client(new_connection, error);  // Передаем новый сокет в обработчик
			}
		});
	std::cout << "Асинхронное ожидание подключения начато.\n" << std::endl;
}

void FileServer::handle_client(tcp_connection::pointer connection, const boost::system::error_code& error) {
	if (!error)
	{
		std::cout << "ERROR " << error.value() << std::endl;

		connection->start();
	}

	start_accept();
	std::cout << "Клиент подключен: " << connection->socket().remote_endpoint().address() << std::endl;
}