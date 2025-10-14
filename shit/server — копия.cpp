#include "main.hpp"

/*
int main(int argc, char *argv[])
{
    try {
        boost::asio::io_context io_ctx;
        boost::asio::ip::tcp::acceptor acceptor(
            io_ctx, 
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT)
        );        

        for (;;) {
            boost::asio::ip::tcp::socket socket(io_ctx);
            std::cout << "* waiting for clients...\n";
            acceptor.async_accept(socket, [](const boost::system::error_code& err)
                {

                });
            std::cout << "* connected!\n";

            std::thread handle_client_thread(handle_client, std::move(socket));
            handle_client_thread.detach();
        }
    }
    catch (std::exception& e) {
        std::cerr << "\n** exception: " << e.what() << ";\n";
    }
}

void handle_client(boost::asio::ip::tcp::socket socket)
{
    try {
        char data[1024];
        boost::system::error_code err;
        while (true) {
            std::memset(data, 0, sizeof(data));

            socket.read_some(boost::asio::buffer(data), err);
            if (err == boost::asio::error::eof) {
                std::cout << "\n* client disconected;\n";
                socket.close();
                break;
            }
            else if (err) {
                socket.close();
                throw boost::system::system_error(err);
            }

            std::cout << "* received request from client.\n";

            boost::json::value request_json = boost::json::parse(data);
            std::string command = request_json.as_object()["command"].as_string().c_str();

            boost::json::object response;
            if (command == "greet") {
                std::string name = request_json.as_object()["name"].as_string().c_str();
                response["message"] = "Hi, " + name + "!";
            }
            else if (command == "status") {
                response["message"] = "server is running LOL@";
            }
            else {
                response["error"] == "Unknown command. ( idk :/ )";
            }

            std::string response_str = boost::json::serialize(response);
            boost::asio::write(socket, boost::asio::buffer(response_str), err);
            if (err == boost::asio::error::eof) {
                socket.close();
                std::cout << "* eof;\n";
                break;
            }
            else if (err) {
                socket.close();
                throw boost::system::system_error(err);
            }

            std::cout << "* sent response to client.\n\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << "\n** exception: " << e.what() << ";\n";
    }
}
*/



class Connection
    : public std::enable_shared_from_this<Connection>
{
public:
    static std::shared_ptr<Connection> create(asio::io_context& io_ctx)
    {
        return std::shared_ptr<Connection>(new Connection(io_ctx));
        //std::shared_ptr<Connection>(new Connection(io_ctx));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        message_ = "Hello, bro";///

        asio::async_write(socket_, asio::buffer(message_), 
            [](const boost::system::error_code& err, size_t bytes)
                {
                    if (err) {
                        fprintf(stderr, "Failed to async_write.\n");
                        return;
                    }
                    printf("< wrote %lld butes.\n", bytes);
                }
        );
    }

private:
    explicit Connection(asio::io_context& io_ctx)
        : socket_(io_ctx)
    {}

    tcp::socket socket_;
    std::string message_;
};


class Server
{
public: 
    explicit Server()
        : acceptor_(io_ctx, tcp::endpoint(tcp::v4(), PORT))
    {        
        start_accept();
    }

    void run()
    {
        io_ctx.run();
    }

private:
    void start_accept()
    {
        std::shared_ptr<Connection> new_conn = 
            Connection::create(io_ctx);
        
        acceptor_.async_accept(new_conn->socket(), 
            [this, new_conn](const boost::system::error_code& err)
            {
                if (!err) {
                    new_conn->start();
                }

                start_accept();
            });
    }

    asio::io_context io_ctx;
    tcp::acceptor acceptor_;
};


int main (int argc, char *argv[])
{
    try {
        //asio::io_context io_ctx;
        Server server;
        server.run();
        //io_ctx.run();
    }
    catch (std::exception& e) {
        fprintf(stderr, "! Server failed. Err: %s.\n", e.what());
    }
    puts("* server closed.");

    return 0;
}