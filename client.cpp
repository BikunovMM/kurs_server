#include "main.hpp"


int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "ru-RU.UTF-8");

    try {
        asio::io_context     io_ctx;
        tcp::socket          socket(io_ctx);
        std::string          request{};
        boost::json::object  req_json;            
        std::string          req_str{};
        std::time_t          ctime;
        std::tm             *ctime_tm = nullptr;
        char                 cdate[11];
        const char          *infn     = "super_img.jpg";
        const char          *outfn   = "converted_img.png";

        /*  LOGIN USER
        req_json["operation"] = SQL_SELECT_USER;
        req_json["login"]     = "mishaB";
        req_json["password"]  = "12345";
        */

        /* ADD_USER_CONVERTATION_TO_HESTORY_ON_DB */
        ctime    = std::time(nullptr);
        ctime_tm = std::localtime(&ctime);

        snprintf(cdate, sizeof(cdate), "%d-%d-%d",
            ctime_tm->tm_year + 1900, ctime_tm->tm_mon + 1, ctime_tm->tm_mday);            

        req_json["operation"] = SQL_INSERT_CONVERTS;
        req_json["infn"]      = infn;
        req_json["outfn"]     = outfn;
        req_json["infmt"]     = std::strrchr(infn, '.') + 1;
        req_json["outfmt"]    = std::strrchr(outfn, '.') + 1;
        req_json["date"]      = std::move(cdate);
        req_json["iduser"]    = 1;

        req_str = boost::json::serialize(req_json);

        socket.async_connect(tcp::endpoint(ip::make_address("127.0.0.1"), PORT), 
            [request = std::move(req_str), &socket]
                (const boost::system::error_code& err) mutable
            {
                if (err) {
                    fprintf(stderr, "! Failed to async_connect.\n");
                    return;
                }
                puts("# connected!");

                if (!request.empty()) {
                    printf("# request: %s.\n", request.c_str());
                }                

                asio::async_write(socket, asio::buffer(request),
                    [&socket]
                        (const boost::system::error_code& err, size_t bytes) mutable
                    {
                        if (err == asio::error::eof) {
                            puts("err == eof");
                        }
                        else if (err) {
                            fprintf(stderr, "! Failed to write.\n");
                            return;
                        }

                        puts("## request sended to server.");

                        char *buffer = (char*)malloc(184 * sizeof(char));
                        if (!buffer) {
                            fprintf(stderr, "! Failed to malloc memory for char *buffer.\n");
                            return;
                        }

                        socket.async_read_some(asio::buffer(buffer, 183 * sizeof(char)), 
                            [buffer]
                                (const boost::system::error_code& err, size_t bytes)
                            {                                
                                if (err) {
                                    if (err == asio::error::eof) {
                                        fprintf(stderr, "! Server closed connection.\n");
                                        return;
                                    }
                                    fprintf(stderr, 
                                        "! Failed to async_read_some. error: %s.\n",
                                        err.message().c_str());
                                    free(buffer);
                                    return;
                                }
                                printf("### received from server: %s.\n"
                                    "### connection closed.\n", buffer);

                                free(buffer);
                            }
                        );
                    }
                );
            }
        );

        io_ctx.run();

        puts("* end_of_try");
    }
    catch (std::exception& e) {
        fprintf(stderr, "main failed.\n");
        return 1;
    }

    puts("end_of_main");

    return 0;
}
