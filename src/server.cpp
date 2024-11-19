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
	: socket_(io_context)
{
}

void tcp_connection::read_command()
{

	auto self = shared_from_this();
	log_socket_state("Вызов перед async_read_until");
	std::cout << (boost::asio::buffer_cast<const char*>(buffer_.data()), buffer_.size()) << std::endl;

	boost::asio::async_read_until(socket_, buffer_, '\n',
		[this, self](const boost::system::error_code& error, std::size_t bytes_transferred) {
			handle_read(error, bytes_transferred);
		});
	log_socket_state("Вызов после async_read_until");
}

void tcp_connection::handle_read(const boost::system::error_code& error, std::size_t bytes_transferred/*, boost::asio::streambuf* buffer_ptr*/)
{
	std::cout << "[DEBUG] Вызов async_read_until завершен. Ошибка: " << error.message() << std::endl;
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
				// Продолжаем читать следующую команду
				read_command();
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
		std::cout << "Команда: UPLOAD - Получение файла от клиента" << std::endl;
		//  пока отключено
		//receive_file(); 
		break;

	case CommandType::Invalid:
	default:
		std::cerr << "Неизвестная команда: " << command << std::endl;
		send_error_message("Unknown command");
		break;
	}
}

void tcp_connection::send_file_list()
{
	//  пока смотрим в текущей директории
	try {
		boost::filesystem::path dir = boost::filesystem::current_path();

		auto file_list = std::make_shared<std::string>();

		for (auto& entry : boost::filesystem::directory_iterator(dir)) {
			*file_list += entry.path().filename().string() + "\n";
			if (file_list->size() > 1024) { // Ограничиваем размер (пример: 1 KB)
				break;
			}
		}

		// Асинхронно отправляем список файлов
		auto self = shared_from_this();
		boost::asio::async_write(socket_, boost::asio::buffer(*file_list),
			[self, file_list](const boost::system::error_code& error, std::size_t) {
				if (error) {
					std::cerr << "Ошибка отправки списка файлов: " << error.message() << std::endl;
				}
			});
	}
	catch (const std::exception& e) {
		std::cerr << "Исключение в send_file_list: " << e.what() << std::endl;
		send_error_message("Internal server error");
	}
}

void tcp_connection::send_file(const std::string& filename) {
	auto file_ptr = std::make_shared<std::ifstream>(filename, std::ios::binary);
	if (!file_ptr->is_open()) {
		send_error_message("file not found");
		return;
	}

	auto buffer = std::make_shared<std::vector<char>>(1024); // Буфер на 1024 байта
	auto self = shared_from_this();

	// Лямбда для отправки следующего блока
	auto send_next_block = std::make_shared<std::function<void()>>();
	*send_next_block = [self, file_ptr, buffer, send_next_block]() { 
		if (!file_ptr->eof()) {
			file_ptr->read(buffer->data(), buffer->size());
			std::size_t bytes_read = file_ptr->gcount(); // Сколько байт было прочитано

			boost::asio::async_write(self->socket_, boost::asio::buffer(buffer->data(), bytes_read),
				[self, file_ptr, buffer, bytes_read, send_next_block](const boost::system::error_code& error, std::size_t bytes_transferred) {
					if (error) {
						std::cerr << "Ошибка отправки файла: " << error.message() << std::endl;
					}
					else {
						std::cout << "Отправлено: " << bytes_transferred << " байт" << std::endl;
						(*send_next_block)(); // Отправляем следующий блок
					}
				});
		}
		else {
			std::cout << "Файл успешно отправлен!" << std::endl;
		}
		};

	// Начинаем отправку
	(*send_next_block)();
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
		std::cout << "ERROR " << error.to_string() << std::endl;

		connection->start();
	}

	start_accept();
	std::cout << "Клиент подключен: " << connection->socket().remote_endpoint().address() << std::endl;
}