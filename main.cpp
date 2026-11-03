#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <sqlite3.h>
#include <sys/stat.h>
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

const std::string PIZERO_HOST = "192.168.1.122";
const int PIZERO_PORT = 8080;
// Use an absolute path for production resilience
const std::string DB_FILE = "/home/conor/central-weather/weather_history.db";

bool init_db(sqlite3 *&db) {
    if (sqlite3_open(DB_FILE.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[ERROR] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    const char *sql = 
        "CREATE TABLE IF NOT EXISTS readings ("
        "id TEXT PRIMARY KEY, "
        "timestamp TEXT, "
        "temperature_c REAL, "
        "temperature_f REAL, "
        "humidity_pct REAL, "
        "pressure_hpa REAL, "
        "logged_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

    char *err_msg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// Helper function to perform an atomic backup using SQLite's VACUUM INTO with debug tracing
void backup_database(sqlite3 *db, const std::string &backup_dir) {
    std::cout << "[DEBUG] backup_database() triggered." << std::endl << std::flush;

    // 1. Ensure the backup directory exists
    std::string mkdir_cmd = "mkdir -p " + backup_dir;
    std::cout << "[DEBUG] Running shell command: " << mkdir_cmd << std::endl << std::flush;
    int mkdir_result = system(mkdir_cmd.c_str());
    if (mkdir_result != 0) {
        std::cerr << "[ERROR] Failed to create backup directory. Exit code: " << mkdir_result << std::endl << std::flush;
        return;
    }

    // 2. Generate a clean timestamp string
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    
    std::string backup_path = backup_dir + "/weather_history_" + ss.str() + ".db";
    std::string query = "VACUUM INTO '" + backup_path + "';";
    std::cout << "[DEBUG] Executing SQL: " << query << std::endl << std::flush;

    // 3. Execute VACUUM INTO
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] Database backup failed: " << (errmsg ? errmsg : "unknown error") << std::endl << std::flush;
        if (errmsg) sqlite3_free(errmsg);
    } else {
        std::cout << "[SUCCESS] Automated backup created: " << backup_path << std::endl << std::flush;
    }

    // 4. Clean up old backups
    std::string cleanup_cmd = "find " + backup_dir + " -name 'weather_history_*.db' -mtime +7 -delete";
    std::cout << "[DEBUG] Running cleanup command: " << cleanup_cmd << std::endl << std::flush;
    system(cleanup_cmd.c_str());
}
void poll_weather(sqlite3 *db) {
    httplib::Client cli(PIZERO_HOST, PIZERO_PORT);
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(3, 0);

    auto res = cli.Get("/weather");
    if (!res || res->status != 200) {
        std::cerr << "[WARN] Failed to fetch weather data from Pi Zero (Network offline?)" << std::endl;
        return;
    }

    try {
        auto data = json::parse(res->body);

        std::string id = data.value("id", "");
        std::string timestamp = data.value("timestamp", "");
        double temp_c = data.value("temperature_c", 0.0);
        double temp_f = data.value("temperature_f", 0.0);
        double humidity = data.value("humidity_pct", 0.0);
        double pressure = data.value("pressure_hpa", 0.0);

        std::string sql = "INSERT OR IGNORE INTO readings (id, timestamp, temperature_c, temperature_f, humidity_pct, pressure_hpa) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 3, temp_c);
            sqlite3_bind_double(stmt, 4, temp_f);
            sqlite3_bind_double(stmt, 5, humidity);
            sqlite3_bind_double(stmt, 6, pressure);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                std::cout << "[SUCCESS] Logged reading ID: " << id << " (" << temp_f << "°F)" << std::endl;
            }
            sqlite3_finalize(stmt);
        }
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] JSON parsing error: " << e.what() << std::endl;
    }
}

int main() {
    sqlite3 *db = nullptr;
    if (!init_db(db)) {
        return 1;
    }

    std::cout << "Starting Central Weather Poller Daemon..." << std::endl;

    // Use steady_clock to track elapsed time (immune to system time jumps/NTP adjustments)
    // Note: Subtracting 24 hours forces an initial backup on daemon startup. 
    // Change to `auto last_backup_time = std::chrono::steady_clock::now();` if you prefer waiting 24h for the first backup.
    auto last_backup_time = std::chrono::steady_clock::now() - std::chrono::hours(24);
    std::string backup_directory = "/home/conor/central-weather/backups";

    // Main resilient daemon loop
    while (true) {
        poll_weather(db);

        // Check if 24 hours have elapsed since the last backup
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::hours>(now - last_backup_time).count() >= 24) {
            backup_database(db, backup_directory);
            last_backup_time = now; // Reset timer
        }

        // Sleep for 1 minute between polls
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    sqlite3_close(db);
    return 0;
}
