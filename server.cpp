#include "main.hpp"


int main (int argc, char *argv[])
{
    try {
        char db_conn_inf[97];
        char path_buffer[128];
        char *dir_index = std::strrchr(argv[0], '\\');

        std::setlocale(LC_ALL, "ru-RU.UTF-8"); // for boost's error_code.message() errors        
        
        std::strncpy(path_buffer, argv[0], dir_index - argv[0]);
        printf("path00: %s.\n\n", path_buffer);
        Server::MAIN_DIR_PATH(path_buffer);
        
        if (argc < 3) {
            std::strcpy(db_conn_inf, "dbname=kurs_db user=admin2006 " 
                        "password=12345 host=192.168.0.46 port=5432 "
                        "client_encoding=UTF8");
            printf("Default connection info is set: %s", db_conn_inf);
        }
        else {
            std::snprintf(db_conn_inf,
                    sizeof(db_conn_inf) * sizeof(char),
                    "dbname=kurs_db user=admin2006 "
                    "password=12345 host=%s port=%s "
                    "client_encoding=UTF8",
                    argv[1], argv[2]);

            printf("Connection info is set: %s", db_conn_inf);
        }

        Server server(db_conn_inf);
        server.run();
    }
    catch (std::exception& e) {
        fprintf(stderr, "! Server failed. Err: %s.\n", e.what());
    }
    puts("\n* server closed.");

    return 0;
}


/*
 *  Connection class
 */

std::shared_ptr<Connection> Connection::create(asio::io_context& io_ctx) {
    return std::shared_ptr<Connection>(new Connection(io_ctx));
}

tcp::socket& Connection::socket() {
    return socket_;
}

std::string& Connection::message() {
    return message_;
}
void Connection::message(std::string str)
{
    if (str.empty()) {
        return;
    }
    message_ = str;
}

void Connection::start()
{
    char *buffer = (char*)malloc(184 * sizeof(char));
    if (!buffer) {
        fprintf(stderr, "! Failed to malloc memory for char *buffer.\n");
        return;
    }

    std::shared_ptr<Connection> conn = shared_from_this();
puts("70");
    socket_.async_read_some(asio::buffer(buffer, 183),
        [conn, buffer]
            (const boost::system::error_code& err, size_t bytes) mutable
        {
            if (err) {                   
                fprintf(stderr, "! Failed to async_read_some. Err: %s.\n",
                    err.message().c_str());
                free(buffer);
                return;
            }
puts("81");
            PGconn              *dbconn      = nullptr;
            PGresult            *res         = nullptr;
            boost::json::object  res_json;                                            
            std::string          res_str{};        
puts("86");       
            printf("# buffer: ^%s^.\n", buffer);
            boost::json::value   req_str     = boost::json::parse(buffer);      
            boost::json::object  req_json    = req_str.as_object();
            std::string          img_buff{};

            free(buffer);                            
puts("90");
            /*
             *  DB
             */

            dbconn = PQconnectdb(Server::DB_CONN_INFO());
            if (PQstatus(dbconn) != CONNECTION_OK) {
                fprintf(stderr, "! Failed to connect to db. Err: %s\n",
                    PQerrorMessage(dbconn));
                res_json["error"] = "Failed to connect to db (PQconnectdb).";
                goto serialize;
            }
puts("102");
            switch (req_json["operation"].as_int64()) {
                case SQL_SELECT_USER: { /* WHEN USER LOGINS*/
                    const char *params_values[3];

                    params_values[0] = req_json["login"].as_string().c_str();
                    params_values[1] = req_json["password"].as_string().c_str();
                    
                    res = PQexecParams(dbconn, SELECT_USER_REQ, 
                                    2, nullptr, params_values,
                                    nullptr, nullptr, 0);
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        fprintf(stderr, "! Failed to PQexecParams. Error: %s.\n",
                            PQresultErrorMessage(res));
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res);
                        break;
                    }

                    if (PQntuples(res) == 0) {
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }    
                    
                    res_json["IdUser"]   = std::strtoll(PQgetvalue(res, 0, 0), nullptr, 10);
                    res_json["IdRole"]   = std::strtol(PQgetvalue(res, 0, 1), nullptr, 10);
                    res_json["login"]    = PQgetvalue(res, 0, 2);
                    res_json["password"] = PQgetvalue(res, 0, 3);
                    res_json["email"]    = PQgetvalue(res, 0, 4);   //  may be NULL
                    res_json["date"]     = PQgetvalue(res, 0, 5);

                    PQclear(res);

                    break;
                }
                case SQL_INSERT_USER: { /* WHEN USER REGISTRATES*/
                    const char *params_values[3];

                    params_values[0] = req_json["login"].as_string().c_str();
                    params_values[1] = req_json["password"].as_string().c_str();
                    params_values[2] = req_json["email"].as_string().c_str(); // can be NULL
                    params_values[3] = req_json["date"].as_string().c_str();

                    res = PQexecParams(dbconn, INSERT_USER,
                                       4, nullptr, params_values,
                                       nullptr, nullptr, 0);
                    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                        fprintf(stderr, "! Failed to PQexecParams. Error: %s.\n",
                            PQresultErrorMessage(res));
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res);
                        break;
                    } 

                    break;
                }
                case SQL_SELECT_CONVERTS: {
                    break;
                }
                case SQL_INSERT_CONVERTS: { /* AFTER USER_MAKES_CONVERTATION */
                    const char *params_values[7];
                    char        iduser_buff[20];
puts("148");
                    params_values[0] = req_json["infn"].as_string().c_str();
                    params_values[1] = req_json["outfn"].as_string().c_str();
puts("151");                    
                    params_values[2] = req_json["infmt"].as_string().c_str();
                    params_values[3] = req_json["outfmt"].as_string().c_str();
                    params_values[4] = req_json["date"].as_string().c_str();
puts("155");                    
                    std::snprintf(iduser_buff, sizeof(iduser_buff), "%lld", req_json["iduser"].as_int64());
                    params_values[5] = std::move(iduser_buff);
                    params_values[6] = nullptr;
puts("1582");
                    /* NO NEED IN INSERTING NEW CONVERTATIONS_HISTORY VALUES
                    //  INSERT_FILES_IN_DB_AND_GET_IDs
                    res = PQexecParams(dbconn, INSERT_INTO_CONVERT_HISTORY, 
                                       6, nullptr, params_values, nullptr,
                                       nullptr, 0);
                    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                        fprintf(stderr, "! Failed to PQexecParams. Error: %s.\n",
                            PQresultErrorMessage(res));
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res);
                        break;
                    }
                    PQclear(res),
                    */
puts("170");                    
                    /*
                     *  GET_ADDITIVNYI_CRITERIY AFTER CONVERTATION
                     */
                    params_values[0] = std::move(params_values[5]);
                    params_values[1] = nullptr;
                    params_values[2] = nullptr;
                    params_values[3] = nullptr;
                    params_values[4] = nullptr;
                    params_values[5] = nullptr;
                    params_values[6] = nullptr;

                    res = PQexecParams(dbconn, GET_ADDITIVNIY_CRITERIY,
                                       1, nullptr, params_values, nullptr,
                                       nullptr, 0);
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        fprintf(stderr, "! Failed to PQexecParams. Error: %s.\n",
                            PQresultErrorMessage(res));
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res);
                        break;
                    }

                    if (PQntuples(res) == 0) {
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }
puts("188");
                    double add_krit_percent  = std::strtod(PQgetvalue(res, 0, 0), NULL);

                    std::stringstream   ss;
                    char path_buff[144];                              

                    if (add_krit_percent < 10.0f) {
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG00_PATH);                   
                    }
                    else if (add_krit_percent < 25.0f) {
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG01_PATH);                       
                    }
                    else { /* add_krit_percent < 100.0f */
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG02_PATH);  
                    }

                    printf("path: %s.\n", path_buff);
                
                    std::ifstream ifs(path_buff, std::ios::in | std::ios::binary);                    

                    ss << ifs.rdbuf();
                    img_buff = ss.str();     
                    
                    //std::cout << "img_buf00: " << img_buff << std::endl;

                    printf("percent from add_krit: %f.\n", add_krit_percent);
                    
                    res_json["img"] = img_buff.c_str();

                    ifs.close();
                    PQclear(res);

                    break;
                }
                default: {
                    res_json["eror"] = "Unknown operation!";
                    break;
                }
            }            
puts("208");
        serialize:       
            PQfinish(dbconn);       
            
            if (!img_buff.empty()) {
                res_str = img_buff;
            }
            else {
                res_str = boost::json::serialize(res_json);
            }
            
            conn->message(res_str);
            //std::cout << "img_buf: " << img_buff << std::endl;
            img_buff = "";

            asio::async_write(conn->socket_, asio::buffer(conn->message()), 
                [conn]
                    (const boost::system::error_code& err, size_t bytes)
                {
                    if (err) {
                        fprintf(stderr, "! Failed to async_write. Err: %s.\n",
                            err.message().c_str());
                        return;
                    }
                    printf("### sended to client: \"%s\", "
                        "%lld bytes.\n# connection closed.\n",
                        conn->message().c_str(), bytes);
                }
            );
        }
    );               
}

Connection::Connection(asio::io_context& io_ctx)
    : socket_(io_ctx)
{}


/*
 *  Server class
 */

Server::Server(const char *db_conn_inf)
    : acceptor_(io_ctx, tcp::endpoint(tcp::v4(), PORT))      
{        
    puts("* Server created.");
    DB_CONN_INFO_ = db_conn_inf;
}
Server::~Server()
{
    puts("\n* Server closed.");
}

void Server::run() {
    puts("* Server is running.\n");
    start_accept();
    io_ctx.run();
}

void Server::start_accept()
{
    std::shared_ptr<Connection> new_conn = 
        Connection::create(io_ctx);
    
    acceptor_.async_accept(new_conn->socket(), 
        [this, new_conn](const boost::system::error_code& err)
        {
            tcp::endpoint endpt = new_conn->socket().remote_endpoint();
            printf("# connected to ip: %s, port: %hu.\n",
                endpt.address().to_string().c_str(), endpt.port());

            if (err) {
                fprintf(stderr, "! Failed to async_accept. Err: %s.\n",
                    err.message().c_str());
                return;
            }          

            new_conn->start();      

            start_accept();
        });
}

const char* Server::DB_CONN_INFO() {
    return DB_CONN_INFO_;
}
const char* Server::MAIN_DIR_PATH() {
    return MAIN_DIR_PATH_;
}
void Server::MAIN_DIR_PATH(const char* str) {
    MAIN_DIR_PATH_ = str;
}

const char* Server::DB_CONN_INFO_ = nullptr;
const char* Server::MAIN_DIR_PATH_ = nullptr;


/*
case SQL_INSERT_CONVERTS: {
                    const char *params_values[7];
                    int         rows_len = 0;
                    const int   infile_id;
                    const int   outfile_id;

                    params_values[0] = req_json["infn"].as_string().c_str();
                    params_values[1] = req_json["outfn"].as_string().c_str();
                    params_values[2] = nullptr;
                    params_values[3] = nullptr;
                    params_values[4] = nullptr;

                    //  INSERT_FILES_IN_DB_AND_GET_IDs
                    res = PQexecParams(dbconn, INSERT_FILES_GET_IDS, 2, nullptr,
                                       params_values, nullptr, nullptr, 0);
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        fprintf(stderr, "! Failed to PQexecParams.\n");
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res),
                        break;
                    }

                    rows_len = PQntuples(res);
                    if (rows_len == 0) {
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }

                    params_values[0] = PQgetvalue(res, 0, 0).as_string().c_str();
                    params_values[1] = PQgetvalue(res, 0, 1).as_string().c_str();

                    //  GET_FILES_FORMATS_IDs
                    res = PQexecParams(dbconn, SELECT_FILES_FMTS_IDS, 2, nullptr,
                                       params_values, nullptr, nullptr, 0);
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        fprintf(stderr, "! Failed to PQexecParams.\n");
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res),
                        break;
                    }

                    rows_len = PQntuples(res);
                    if (rows_len != 2) {
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }

                    params_values[2] = PQgetvalue(res, 0, 0).as_string().c_str();
                    params_values[3] = PQgetvalue(res, 1, 0).as_string().c_str();
                    params_values[4] = res_json["date"].as_string().c_str();

                    //  INSERT_CONVERTATION_AND_GET_ID


                    break;
                }
*/

/*

class Connection
    : public std::enable_shared_from_this<Connection>
{
public:
    static std::shared_ptr<Connection> create(asio::io_context& io_ctx) {
        return std::shared_ptr<Connection>(new Connection(io_ctx));
    }

    tcp::socket& socket() {
        return socket_;
    }

    std::string& message() {
        return message_;
    }
    void message(std::string str)
    {
        if (str.empty()) {
            return;
        }
        message_ = str;
    }

    void start()
    {
        char *buffer = (char*)malloc(128 * sizeof(char));

        std::shared_ptr<Connection> conn = shared_from_this();

        socket_.async_read_some(asio::buffer(buffer, 128 * sizeof(char)),
            [conn, buffer]
                (const boost::system::error_code& err, size_t bytes) mutable
            {
                if (err) {                   
                    fprintf(stderr, "! Failed to async_read_some. Err: %s.\n",
                        err.message().c_str());
                    free(buffer);
                    return;
                }

                PGconn              *dbconn   = nullptr;
                PGresult            *res      = nullptr;
                boost::json::object  res_json;                                
                const char          *params_values[3];
                std::string          res_str{};                
                boost::json::value   req_str = boost::json::parse(buffer);      
                boost::json::object  req_json = = req_str.as_object();

                //boost::json::value   req_str = boost::json::parse(buffer);    
                free(buffer);               

                //boost::json::object req_json = req_str.as_object();             

                dbconn = PQconnectdb(DB_CONN_INFO);
                if (PQstatus(dbconn) != CONNECTION_OK) {
                    fprintf(stderr, "! Failed to connect to db. Err: %s\n",
                        PQerrorMessage(dbconn));
                    res_json["error"] = "Failed to connect to db (PQconnectdb).";
                    goto serialize;
                }

                switch (req_json["operation"].as_int64()) {
                    case SQL_SELECT:
                        int rows_len = 0;

                        params_values[0] = req_json["login"].as_string().c_str();
                        params_values[1] = req_json["password"].as_string().c_str();
                        
                        res = PQexecParams(dbconn, SELECT_USER_REQ, 
                                        2, nullptr, params_values,
                                        nullptr, nullptr, 0);
                        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                            fprintf(stderr, "! Failed to PQexecParams.\n");
                            res_json["error"] = "Failed to execute request to db.";
                            PQclear(res);
                            goto serialize;
                        }

                        rows_len = PQntuples(res);
                        if (rows_len > 0) {
                            res_json["IdUser"]   = std::strtoll(PQgetvalue(res, 0, 0), nullptr, 10);
                            res_json["IdRole"]   = std::strtol(PQgetvalue(res, 0, 1), nullptr, 10);
                            res_json["login"]    = PQgetvalue(res, 0, 2);
                            res_json["password"] = PQgetvalue(res, 0, 3);
                            res_json["email"]    = PQgetvalue(res, 0, 4);   //  may be NULL
                            res_json["date"]     = PQgetvalue(res, 0, 5);
                        }
                        else {
                            res_json["error"] = "No data found!";
                        }
                        break;
                    case SQL_INSERT:
                        break;
                    case SQL_UPDATE:
                        break;
                    case SQL_DELETE:
                        break;
                    default:
                        res_json["eror"] = "Unknown operation!";
                        break;
                }

                PQclear(res);

            serialize:       
                PQfinish(dbconn);         
                res_str = boost::json::serialize(res_json);

                conn->message(res_str);

                asio::async_write(conn->socket_, asio::buffer(conn->message()), 
                    [conn]
                        (const boost::system::error_code& err, size_t bytes)
                    {
                        if (err) {
                            fprintf(stderr, "! Failed to async_write. Err: %s.\n",
                                err.message().c_str());
                            return;
                        }
                        printf("### sended to client: \"%s\", "
                            "%lld bytes.\n# connection closed.\n",
                            conn->message().c_str(), bytes);
                    }
                );
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
        puts("* Server created.");
        start_accept();
    }
    ~Server()
    {
        puts("\n* Server closed.");
    }

    void run() {
        puts("* Server is running.\n");
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
                tcp::endpoint endpt = new_conn->socket().remote_endpoint();
                printf("# connected to ip: %s, port: %hu.\n",
                    endpt.address().to_string().c_str(), endpt.port());

                if (err) {
                    fprintf(stderr, "! Failed to async_accept. Err: %s.\n",
                        err.message().c_str());
                    return;
                }          

                new_conn->start();      

                start_accept();
            });
    }

    asio::io_context io_ctx;
    tcp::acceptor acceptor_;
};

*/