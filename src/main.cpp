// serverFileManager.cpp: определяет точку входа для приложения.
//

#include "server.h"
#include <boost/asio.hpp>

#include <locale>
#include <iostream> 

using namespace std;

int main()
{
    //SetConsoleCP(1251);
    //SetConsoleOutputCP(1251);
    try {
        boost::asio::io_context io_context;
        FileServer server(io_context, 8000);
        io_context.run();
    }
    catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }
    
    return 0;
}
