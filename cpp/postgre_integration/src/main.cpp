#include <iostream>
#include <sql.h>
#include <sqlext.h>
#include <stdexcept>
#include <string>
#include <cstdint>

class PostgreSQLODBC {
public:
    PostgreSQLODBC(const std::string& dsn, const std::string& user, const std::string& password) {
        // Allocate environment handle
        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_) != SQL_SUCCESS) {
            throw std::runtime_error("Failed to allocate environment handle");
        }

        // Set ODBC version
        if (SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0) != SQL_SUCCESS) {
            SQLFreeHandle(SQL_HANDLE_ENV, env_);
            throw std::runtime_error("Failed to set ODBC version");
        }

        // Allocate connection handle
        if (SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_) != SQL_SUCCESS) {
            SQLFreeHandle(SQL_HANDLE_ENV, env_);
            throw std::runtime_error("Failed to allocate connection handle");
        }

        // Connect to the database
        SQLRETURN ret = SQLConnect(dbc_, (SQLCHAR*)dsn.c_str(), SQL_NTS,
                                   (SQLCHAR*)user.c_str(), SQL_NTS,
                                   (SQLCHAR*)password.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
            SQLFreeHandle(SQL_HANDLE_ENV, env_);
            throw std::runtime_error("Failed to connect to the database");
        }
    }

    ~PostgreSQLODBC() {
        SQLDisconnect(dbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
    }

    void execute(const std::string& query) {
        SQLHSTMT stmt;
        if (SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt) != SQL_SUCCESS) {
            throw std::runtime_error("Failed to allocate statement handle");
        }

        SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            throw std::runtime_error("Failed to execute query: " + query);
        }

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }

private:
    SQLHENV env_;
    SQLHDBC dbc_;
};

int main() {
    try {
        // Connect to PostgreSQL
        PostgreSQLODBC db("PostgreSQL", "your_username", "your_password");

        // Create database
        db.execute("CREATE DATABASE test_db");

        // Connect to the new database
        PostgreSQLODBC db_test("PostgreSQL", "your_username", "your_password");
        db_test.execute("USE test_db");

        // Create table
        db_test.execute("CREATE TABLE EVENT (id BIGINT PRIMARY KEY, value TEXT)");

        std::cout << "Database and table created successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

