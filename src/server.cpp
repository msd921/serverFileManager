#include "server.h"
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

tcp::socket& tcp_connection::socket()
{
    return socket_;
}

void tcp_connection::start()
{
    message_ = make_daytime_string();

    boost::asio::async_write(socket_, boost::asio::buffer(message_),
        std::bind(&tcp_connection::handle_write, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}

tcp_connection::tcp_connection(boost::asio::io_context& io_context)
    : socket_(io_context)
{
}

void tcp_connection::handle_write(const boost::system::error_code& /*error*/,
    size_t /*bytes_transferred*/)
{
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
            std::cout << "ERROR" << error.to_string() << std::endl;
            connection->start();
        }

        start_accept();
        std::cout << "Клиент подключен: " << connection->socket().remote_endpoint().address() << std::endl;
}



std::string make_daytime_string()
{
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}


