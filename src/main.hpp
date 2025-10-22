#ifndef KURS_SERVER_HPP
#define KURS_SERVER_HPP

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <chrono>
#include <ctime>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/json/src.hpp>
#include <libpq-fe.h>

namespace ip   = boost::asio::ip;
namespace asio = boost::asio;
using     tcp  = boost::asio::ip::tcp;

constexpr int PORT = 8765;

#ifdef _WIN32
    #define OS_SEPERATOR "\\"
    #define OS_SEPERATOR_CHR '\\'
#else
    #define OS_SEPERATOR "/"
    #define OS_SEPERATOR_CHR '/'
#endif

constexpr char *IMG00_PATH = (char*)"images" OS_SEPERATOR "img00.jpg";
constexpr char *IMG01_PATH = (char*)"images" OS_SEPERATOR "img01.jpg";
constexpr char *IMG02_PATH = (char*)"images" OS_SEPERATOR "img02.jpg";

constexpr char *SELECT_USERS    = (char*)"SELECT * FOM Пользователи;";
constexpr char *SELECT_USER_REQ = (char*)"SELECT * FROM Пользователи "
                                             "WHERE Логин = $1::VARCHAR(20) "
                                             "AND Пароль = $2::VARCHAR(20);";
constexpr char *INSERT_USER = (char*) "INSERT INTO Пользователи (Логин, Пароль, "
                                          "АдресЭлектроннойПочты, ДатаРегистрации) "
                                          "VALUES ($1::VARCHAR(20), $2::VARCHAR(20), "
                                          "$3::VARCHAR(20), $4::DATE) "
                                          "RETURNING idПользователя;";
constexpr char *INSERT_FILES_GET_IDS =  (char*)"INSERT INTO Файлы (НазваниеФайла) "
                                                   "VALUES ($1::VARCHAR(20)), "
                                                   "($2::VARCHAR(20)) "
                                                   "RETURNING idФайла;";
constexpr char *SELECT_FILES_FMTS_IDS = (char*)"SELECT idФормата FROM ФорматыФайлов "
                                               "WHERE Название = $1::VARCHAR(20) OR"
                                                "Название = $2::VARCHAR(20);";
constexpr char *INSERT_INTO_CONVERT_HISTORY = (char*)   
    "WITH infile AS ("
        "INSERT INTO \"Файлы\" (\"idФайла\", \"НазваниеФайла\") "
        "VALUES ((SELECT MAX(\"idФайла\") + 1 FROM \"Файлы\"), $1) "
        "RETURNING \"idФайла\""
    "), "
    "outfile AS ("
        "INSERT INTO \"Файлы\" (\"idФайла\", \"НазваниеФайла\") "
        "SELECT "
        "i.\"idФайла\" + 1, $2 "
        "FROM infile i "
        "RETURNING \"idФайла\""
    "), "
    "infile_fmt AS ("
        "SELECT \"idФормата\" FROM \"ФорматыФайлов\" "
        "WHERE \"Название\" = $3 "
        "LIMIT 1"
    "), "
    "outfile_fmt AS ("
        "SELECT \"idФормата\" FROM \"ФорматыФайлов\" "
        "WHERE \"Название\" = $4 "
        "LIMIT 1"
    "), "
    "convert AS ("
        "INSERT INTO \"Конвертации\" (\"idВходногоФайла\", \"idВыходногоФайла\", "
        "\"idВходногоФормата\", \"idВыходногоФормата\", \"ДатаКонвертации\") "
        "SELECT "
            "i.\"idФайла\", "
            "o.\"idФайла\", "
            "ifmt.\"idФормата\", "
            "ofmt.\"idФормата\", "
            "$5 "
        "FROM infile i, outfile o, infile_fmt ifmt, outfile_fmt ofmt "
        "RETURNING \"idКонвертации\""
    ")"
    "INSERT INTO \"ИсторияКонвертаций\" (\"idПользователя\", \"idКонвертации\") "
    "SELECT $6, \"idКонвертации\" "
    "FROM convert;";

constexpr char *GET_ADDITIVNIY_CRITERIY = (char*)
    "WITH АддКрит AS ( "
        "SELECT DISTINCT "
            "П.\"idПользователя\" AS \"idПользователя\", "
            "COALESCE(СуммаКонвертаций.\"СуммаКонвертаций\", 0) * 1.0 "
            "+ COALESCE(ДниСПоследКонверт.\"ДниСПоследКонверт\", 0) * (-0.5) "
            "+ COALESCE(ЧастотаКонвертаций.\"ЧастотаКонвертаций\", 0) * 1.0 "
            "+ COALESCE(ДниСРегистрации.\"ДниСРегистрации\", 0) * 1.0 "
            "+ 0 AS \"АддКрит\", "
            "COALESCE(СуммаКонвертаций.\"СуммаКонвертаций\", 0) * 1.0, "
            "COALESCE(ДниСПоследКонверт.\"ДниСПоследКонверт\", 0) * (-0.5), "
            "COALESCE(ЧастотаКонвертаций.\"ЧастотаКонвертаций\", 0) * 1.0, "
            "COALESCE(ДниСРегистрации.\"ДниСРегистрации\", 0) * 1.0, "
            "COALESCE(ДлинаФормата.\"СамыйИспользуемыйФормат\", 0) * 0 "
        "FROM "
            "\"Пользователи\" AS П "
        "LEFT JOIN ( "
            "SELECT DISTINCT "
                "П.idПользователя, "
                "EXTRACT(DAY FROM AGE(П.\"ДатаРегистрации\")) * 1.0 / "
                "COALESCE(NULLIF((SELECT MAX(EXTRACT(DAY FROM AGE(\"ДатаРегистрации\"))) "
                    "FROM \"Пользователи\"), 0), 1) AS \"ДниСРегистрации\" "
            "FROM \"Пользователи\" AS П "
            "GROUP BY П.\"idПользователя\", П.\"ДатаРегистрации\" "
        ") AS ДниСРегистрации ON П.\"idПользователя\" = ДниСРегистрации.\"idПользователя\" "
        "LEFT JOIN ( "
            "SELECT DISTINCT "
                "П.\"idПользователя\", "
                "COALESCE(EXTRACT(DAY FROM AGE(MAX(К.\"ДатаКонвертации\"))), 0) * 1.0 / "
                "COALESCE(NULLIF((SELECT MAX(Д.\"КрайняяДата\") "
                    "FROM ( "
                        "SELECT "
                            "П.\"idПользователя\", "
                            "COALESCE(EXTRACT(DAY FROM AGE(MAX(К.\"ДатаКонвертации\"))), 0) AS \"КрайняяДата\" "
                        "FROM \"Конвертации\" AS К, \"Пользователи\" AS П, \"ИсторияКонвертаций\" AS И "
                        "WHERE П.\"idПользователя\" = И.\"idПользователя\" AND И.\"idКонвертации\" = К.\"idКонвертации\" "
                        "GROUP BY П.\"idПользователя\" "
                    ") AS Д "
                "), 0), 1) AS \"ДниСПоследКонверт\" "
            "FROM \"Пользователи\" AS П "
            "LEFT JOIN \"ИсторияКонвертаций\" AS И ON П.\"idПользователя\" = И.\"idПользователя\" "
            "LEFT JOIN \"Конвертации\" AS К ON И.\"idКонвертации\" = К.\"idКонвертации\" "
            "GROUP BY П.\"idПользователя\" "
        ") AS ДниСПоследКонверт ON П.\"idПользователя\" = ДниСПоследКонверт.\"idПользователя\" "
        "LEFT JOIN ( "
            "SELECT "
                "ДатыВсехКонвертаций.\"idПользователя\", "
                "COALESCE(AVG(EXTRACT(DAY FROM (ДатыВсехКонвертаций.\"ДатаКонвертации\"::timestamp - ПорядковыйНомерМинус1.\"ДатаКонвертации\"::timestamp))), 0) / "
                "COALESCE(NULLIF((SELECT MAX(С.\"СредКонв\") "
                    "FROM ( "
                        "SELECT "
                            "COALESCE(AVG(EXTRACT(DAY FROM (ДатыВсехКонвертаций.\"ДатаКонвертации\"::timestamp - ПорядковыйНомерМинус1.\"ДатаКонвертации\"::timestamp))), 0) AS \"СредКонв\" "
                        "FROM ( "
                            "SELECT "
                                "И.\"idПользователя\", "
                                "К.\"ДатаКонвертации\", "
                                "ROW_NUMBER() OVER (ORDER BY И.\"idПользователя\", К.\"ДатаКонвертации\") AS \"ПорядковыйНомер\" "
                            "FROM \"ИсторияКонвертаций\" AS И "
                            "INNER JOIN \"Конвертации\" AS К ON И.\"idКонвертации\" = К.\"idКонвертации\" "
                        ") AS ДатыВсехКонвертаций "
                        "LEFT OUTER JOIN ( "
                            "SELECT "
                                "\"idПользователя\", "
                                "\"ДатаКонвертации\", "
                                "\"ПорядковыйНомер\" - 1 AS \"ПорядковыйНомерМинус1\" "
                            "FROM ( "
                                "SELECT "
                                    "И.\"idПользователя\", "
                                    "К.\"ДатаКонвертации\", "
                                    "ROW_NUMBER() OVER (ORDER BY И.\"idПользователя\", К.\"ДатаКонвертации\") AS \"ПорядковыйНомер\" "
                                "FROM \"ИсторияКонвертаций\" AS И "
                                "INNER JOIN \"Конвертации\" AS К ON И.\"idКонвертации\" = К.\"idКонвертации\" "
                            ") AS ДатыВсехКонвертаций "
                        ") AS ПорядковыйНомерМинус1 "
                        "ON ДатыВсехКонвертаций.\"idПользователя\" = ПорядковыйНомерМинус1.\"idПользователя\" "
                        "AND ДатыВсехКонвертаций.\"ПорядковыйНомер\" = ПорядковыйНомерМинус1.\"ПорядковыйНомерМинус1\" "
                        "GROUP BY ДатыВсехКонвертаций.\"idПользователя\" "
                    ") С "
                "), 0), 1) AS \"ЧастотаКонвертаций\" "
            "FROM (  "
                "SELECT "
                    "И.\"idПользователя\", "
                    "К.\"ДатаКонвертации\", "
                    "ROW_NUMBER() OVER (ORDER BY И.\"idПользователя\", К.\"ДатаКонвертации\") AS \"ПорядковыйНомер\" "
                "FROM \"ИсторияКонвертаций\" AS И "
                "INNER JOIN \"Конвертации\" AS К ON И.\"idКонвертации\" = К.\"idКонвертации\" "
            ") AS ДатыВсехКонвертаций "
            "LEFT OUTER JOIN ( "
                "SELECT "
                    "\"idПользователя\", "
                    "\"ДатаКонвертации\", "
                    "\"ПорядковыйНомер\" - 1 AS \"ПорядковыйНомерМинус1\" "
                "FROM ( "
                    "SELECT "
                        "И.\"idПользователя\", "
                        "К.\"ДатаКонвертации\", "
                        "ROW_NUMBER() OVER (ORDER BY И.\"idПользователя\", К.\"ДатаКонвертации\") AS \"ПорядковыйНомер\" "
                    "FROM \"ИсторияКонвертаций\" AS И "
                    "INNER JOIN \"Конвертации\" AS К ON И.\"idКонвертации\" = К.\"idКонвертации\" "
                ") AS ДатыВсехКонвертаций "
            ") AS ПорядковыйНомерМинус1 "
            "ON ДатыВсехКонвертаций.\"idПользователя\" = ПорядковыйНомерМинус1.\"idПользователя\" "
                "AND ДатыВсехКонвертаций.\"ПорядковыйНомер\" = ПорядковыйНомерМинус1.\"ПорядковыйНомерМинус1\" "
            "GROUP BY ДатыВсехКонвертаций.\"idПользователя\" "
        ") AS ЧастотаКонвертаций ON П.\"idПользователя\" = ЧастотаКонвертаций.\"idПользователя\" "
        "LEFT JOIN ( "
            "SELECT DISTINCT "
                "П.\"idПользователя\", "
                "COALESCE(LENGTH(Ф.\"Название\"), 0) * 1.0 / "
                "COALESCE(NULLIF((SELECT MAX(\"ДлинаФормата\") "
                    "FROM ( "
                        "SELECT "
                            "П.\"idПользователя\", "
                            "LENGTH(Ф.\"Название\") AS \"ДлинаФормата\" "
                        "FROM \"Пользователи\" AS П, \"ИспользованиеФорматов\" AS И, \"ФорматыФайлов\" AS Ф "
                        "WHERE П.\"idПользователя\" = И.\"idПользователя\" AND И.\"idФормата\" = Ф.\"idФормата\" "
                        "GROUP BY П.\"idПользователя\", Ф.\"Название\" "
                    ") Д "
                "), 0), 1) AS \"СамыйИспользуемыйФормат\" "
            "FROM \"Пользователи\" AS П "
            "LEFT JOIN \"ИспользованиеФорматов\" AS И ON П.\"idПользователя\" = И.\"idПользователя\" "
            "LEFT JOIN \"ФорматыФайлов\" AS Ф ON И.\"idФормата\" = Ф.\"idФормата\" "
            "GROUP BY П.\"idПользователя\", Ф.\"Название\" "
        ") AS ДлинаФормата ON П.\"idПользователя\" = ДлинаФормата.\"idПользователя\" "
        "LEFT JOIN ( "
            "SELECT "
                "П.\"idПользователя\", "
                "COUNT(И.\"idИсторииКонвертаций\") * 1.0 / "
                "COALESCE(NULLIF((SELECT MAX(\"СуммаКонвертаций\") "
                    "FROM ( "
                        "SELECT "
                            "П.\"idПользователя\", "
                            "COUNT(И.\"idИсторииКонвертаций\") AS \"СуммаКонвертаций\" "
                        "FROM \"Пользователи\" AS П "
                        "LEFT JOIN \"ИсторияКонвертаций\" AS И ON И.\"idПользователя\" = П.\"idПользователя\" "
                        "GROUP BY П.\"idПользователя\" "
                    ") О "
                "), 0), 1) AS \"СуммаКонвертаций\" "
            "FROM \"Пользователи\" AS П "
            "LEFT JOIN \"ИсторияКонвертаций\" AS И ON И.\"idПользователя\" = П.\"idПользователя\" "
            "GROUP BY П.\"idПользователя\" "
        ") AS СуммаКонвертаций ON П.\"idПользователя\" = СуммаКонвертаций.\"idПользователя\" "
        ")"
    "SELECT "
        "ROUND(АддКрит.\"АддКрит\" * 100 / (SELECT SUM(АддКрит.\"АддКрит\") FROM АддКрит), 4)"
    "FROM АддКрит "
    "WHERE \"idПользователя\" = $1::BIGINT;";

/**
 *  Are used in request, response jsons
 */
enum SQL_OPERATIONS {
    SQL_SELECT_USER,
    SQL_INSERT_USER,
    SQL_SELECT_CONVERTS,
    SQL_INSERT_CONVERTS
};

enum OP_STATUS {
    OP_OK,
    OP_LOGIN_ERR,
    OP_PASSWORD_ERR
};

/**
 *  Connection of server and client.
 *  Is created, when server accepts new client.
 *  Receives clients request, sends response and destroys.
 */
class Connection
    : public std::enable_shared_from_this<Connection>
{
public:
    static std::shared_ptr<Connection> create(asio::io_context& io_ctx);

    tcp::socket& socket();
    std::string& message();
    void message(std::string str);

    void start();

private:
    explicit Connection(asio::io_context& io_ctx);

    
    tcp::socket socket_;
    std::string message_;
};

/**
 *  Server inits loop,
 *  in which asynchronously accepts new clients Connections.
 */
class Server
{
public: 
    explicit Server(const char *db_conn_inf);
    ~Server();

    void run();

    static const char* DB_CONN_INFO();
    static const char* MAIN_DIR_PATH();
    static void MAIN_DIR_PATH(const char* str);

private:
    void start_accept();
    
    static const char *MAIN_DIR_PATH_;
    static const char *DB_CONN_INFO_;
    asio::io_context io_ctx;
    tcp::acceptor acceptor_;
};

#endif /* KURS_SERVER_HPP */