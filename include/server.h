#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#define _WIN32_WINNT 0x0A00
#pragma once


using boost::asio::ip::tcp;

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
public:
    typedef std::shared_ptr<tcp_connection> pointer;
    static tcp_connection::pointer tcp_connection::create(boost::asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }
    tcp::socket& socket();
    void start();
private:
    tcp_connection(boost::asio::io_context& io_context);
   
    void read_command();
    void handle_read(const boost::system::error_code& error, std::size_t bytes_transferred/*, boost::asio::streambuf* buffer_ptr*/);
    void handle_command(const std::string& command);
    

    void send_file_list();
    void send_file(const std::string& filename);
    //void receive_file();
    void send_error_message(const std::string& error_message);

    void log_socket_state(const std::string& context);

    tcp::socket socket_;
    std::string message_;
    boost::asio::streambuf buffer_;
};

using boost::asio::ip::tcp;
namespace fs = boost::filesystem;
class FileServer {
public:
    FileServer(boost::asio::io_context& io_context, short port);

private:
    void start_accept();

    void handle_client(tcp_connection::pointer connection, const boost::system::error_code & err);
    tcp_connection::pointer new_connection;
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};