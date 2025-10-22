#include "main.hpp"

int main (int argc, char *argv[])
{
    try {
        char db_conn_inf[97];
        char path_buffer[128];
        char *dir_index = nullptr;
        
        dir_index = std::strrchr(argv[0], OS_SEPERATOR_CHR);
        if (!dir_index) {
            fprintf(stderr, "! Failed to strchr.\n");
            return 1;
        }

        std::setlocale(LC_ALL, "ru-RU.UTF-8"); // for boost's error_code.message() errors        
        
        std::strncpy(path_buffer, argv[0], dir_index - argv[0]);
        printf("executable\'s path: %s.\n\n", path_buffer);
        Server::MAIN_DIR_PATH(path_buffer);
        
        if (argc < 3) {
            std::strcpy(db_conn_inf, "dbname=kurs_db user=admin2006 " 
                        "password=12345 host=192.168.0.46 port=5432 "
                        "client_encoding=UTF8");
            printf("Default connection info is set to: %s", db_conn_inf);
        }
        else if (argc == 2){
            std::snprintf(db_conn_inf,
                    sizeof(db_conn_inf) * sizeof(char),
                    "dbname=kurs_db user=admin2006 "
                    "password=12345 host=%s port=5432 "
                    "client_encoding=UTF8", argv[1]);
            printf("Connection info is set to: %s (default port)",
                db_conn_inf);
        }
        else {
            std::snprintf(db_conn_inf,
                    sizeof(db_conn_inf) * sizeof(char),
                    "dbname=kurs_db user=admin2006 "
                    "password=12345 host=%s port=%s "
                    "client_encoding=UTF8",
                    argv[1], argv[2]);

            printf("Connection info is set to: %s", db_conn_inf);
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

            buffer[bytes] = '\0';

            PGconn              *dbconn        = nullptr;
            PGresult            *res           = nullptr;
            boost::json::object  res_json;                                            
            std::string          res_str{};
            boost::json::value   req_str       = boost::json::parse(buffer);      
            boost::json::object  req_json      = req_str.as_object();
            char                 *img_buff     = nullptr;
            int                   img_buff_len = 0;

            free(buffer);                            

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

            switch (req_json["operation"].as_int64()) {
                case SQL_SELECT_USER: { /* WHEN USER LOGINS*/
                    const char  *params_values[3];

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
                        res_json["status"] = OP_LOGIN_ERR;
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }                   
                    
                    res_json["iduser"] = std::strtoll(PQgetvalue(res, 0, 0), nullptr, 10);
                    res_json["status"] = OP_OK;

                    PQclear(res);

                    break;
                }
                case SQL_INSERT_USER: { /* WHEN USER REGISTRATES*/
                    const char  *params_values[3];
                    std::time_t  ctime;
                    std::tm     *ctime_tm = nullptr;
                    char         cdate[11];

                    ctime    = std::time(nullptr);
                    ctime_tm = std::localtime(&ctime);

                    snprintf(cdate, sizeof(cdate), "%d-%d-%d",
                        ctime_tm->tm_year + 1900, ctime_tm->tm_mon + 1, ctime_tm->tm_mday);

                    params_values[0] = req_json["login"].as_string().c_str();
                    params_values[1] = req_json["password"].as_string().c_str();
                    params_values[2] = req_json["email"].as_string().c_str(); // can be NULL
                    params_values[3] = cdate;

                    res = PQexecParams(dbconn, INSERT_USER,
                                       4, nullptr, params_values,
                                       nullptr, nullptr, 0);
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        fprintf(stderr, "! Failed to PQexecParams. Error: %s.\n",
                            PQresultErrorMessage(res));
                        res_json["status"] = OP_LOGIN_ERR;
                        res_json["error"] = "Failed to execute request to db.";
                        PQclear(res);
                        break;
                    } 

                    if (PQntuples(res) == 0) {
                        res_json["status"] = OP_LOGIN_ERR;
                        res_json["error"] = "No data found!";
                        PQclear(res);
                        break;
                    }

                    res_json["iduser"] = std::strtoll(PQgetvalue(res, 0, 0), nullptr, 10);
                    res_json["status"]  = OP_OK;
                    puts("SQL_INSERT_USER is done");

                    break;
                }
                case SQL_SELECT_CONVERTS: {
                    break;
                }
                case SQL_INSERT_CONVERTS: { /* AFTER USER_MAKES_CONVERTATION */
                    const char  *params_values[7];
                    char         iduser_buff[20];
                    std::time_t  ctime;
                    std::tm     *ctime_tm = nullptr;
                    char         cdate[11];
                    char         path_buff[144]; 
                    double       add_krit_percent;

                    ctime    = std::time(nullptr);
                    ctime_tm = std::localtime(&ctime);

                    snprintf(cdate, sizeof(cdate), "%d-%d-%d",
                        ctime_tm->tm_year + 1900, ctime_tm->tm_mon + 1, ctime_tm->tm_mday);

                    params_values[0] = req_json["infn"].as_string().c_str();
                    params_values[1] = req_json["outfn"].as_string().c_str();                    
                    params_values[2] = std::strrchr(params_values[0], '.') + 1;
                    params_values[3] = std::strrchr(params_values[1], '.') + 1;
                    params_values[4] = cdate;     

                    std::snprintf(iduser_buff, sizeof(iduser_buff), "%lld", req_json["iduser"].as_int64());
                    params_values[5] = iduser_buff;
                    params_values[6] = nullptr;

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

                    add_krit_percent = 
                        std::strtod(PQgetvalue(res, 0, 0), NULL);                                        
                    PQclear(res);                                              

                    if (add_krit_percent < 10.0f) {
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG00_PATH);                   
                    }
                    else if (add_krit_percent < 25.0f) {
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG01_PATH);                       
                    }
                    else { /* add_krit_percent < 100.0f */
                        std::snprintf(path_buff, sizeof(path_buff) * sizeof(char), "%s\\%s", Server::MAIN_DIR_PATH(), IMG02_PATH);  
                    }
                
                    std::ifstream ifs(path_buff, std::ios::in | std::ios::binary);                    
                    if (!ifs) {
                        fprintf(stderr, "! Failed to init std::ifstream.\n");
                        PQfinish(dbconn);
                        return;
                    }
                    
                    ifs.seekg(0, ifs.end);
                    img_buff_len = ifs.tellg();
                    ifs.seekg(0, ifs.beg);

                    img_buff = (char*)malloc((img_buff_len + 1) * sizeof(char));
                    ifs.read(img_buff, img_buff_len);

                    printf("percent from add_krit: %f.\n", add_krit_percent);

                    ifs.close();

                    break;
                }
                default: {
                    res_json["eror"] = "Unknown operation!";
                    break;
                }
            }            

        serialize:       
            PQfinish(dbconn);       
            
            if (img_buff) { /* SENDING IMG'S SIZE AND THEN IMG IT SELF*/
                conn->message(std::to_string(img_buff_len));

                asio::async_write(conn->socket_, asio::buffer(conn->message()),
                    [conn, img_buff, img_buff_len]
                        (const boost::system::error_code& err, size_t bytes) mutable
                    {
                        if (err) {
                            fprintf(stderr, "! Failed to async_write. Err: %s.\n",
                                err.message().c_str());
                            free(img_buff);
                            return;
                        }

                        asio::async_write(conn->socket_, asio::buffer(img_buff, img_buff_len),
                            [conn, img_buff]
                                (const boost::system::error_code& err, size_t bytes)
                            {
                                if (err) {
                                    fprintf(stderr, "! Failed to async_write. Err: %s.\n",
                                        err.message().c_str());
                                    free(img_buff);
                                    return;
                                }
                                free(img_buff);
                            }
                        );                            
                    }
                );
            }
            else { /* JUST SOME REQUEST */
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