#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

using namespace std;

// Глобальные переменные для сигналов
volatile sig_atomic_t sighup_received = 0;
volatile sig_atomic_t running = true;
atomic<bool> vfs_mounted(false);

// Обновленный обработчик SIGHUP
void handle_sighup(int signum) {
    (void)signum;
    const char* msg = "Configuration reloaded\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    sighup_received = 1;
}

// Обработчик других сигналов
void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        running = false;
    }
}

// Функция для выполнения команды
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Проверка существования файла
bool file_exists(const string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// Проверка существования директории
bool dir_exists(const string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) return false;
    return S_ISDIR(buffer.st_mode);
}

// Поиск команды в PATH
string find_in_path(const string& cmd) {
    if (cmd.find('/') != string::npos) {
        if (file_exists(cmd)) {
            return cmd;
        }
        return "";
    }
    
    const char* path_env = getenv("PATH");
    if (!path_env) return "";
    
    stringstream ss(path_env);
    string path;
    
    while (getline(ss, path, ':')) {
        if (path.empty()) continue;
        string full_path = path + "/" + cmd;
        if (file_exists(full_path)) {
            return full_path;
        }
    }
    
    return "";
}

// Выполнение внешней команды
bool execute_external(const vector<string>& args) {
    if (args.empty()) return false;
    
    string cmd_path = find_in_path(args[0]);
    if (cmd_path.empty()) return false;
    
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        vector<char*> exec_args;
        for (const auto& arg : args) {
            exec_args.push_back(const_cast<char*>(arg.c_str()));
        }
        exec_args.push_back(nullptr);
        
        execv(cmd_path.c_str(), exec_args.data());
        exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return true;
    }
    
    return false;
}

// Функция для монтирования VFS через FUSE
bool mount_vfs() {
    // Создаем директорию если не существует
    string home_dir = getenv("HOME");
    string mount_point = home_dir + "/users";
    
    if (!dir_exists(mount_point)) {
        string cmd = "mkdir -p \"" + mount_point + "\"";
        system(cmd.c_str());
    }
    
    // Проверяем, не смонтирована ли уже
    string check_cmd = "mount | grep -q \"" + mount_point + "\"";
    if (system(check_cmd.c_str()) == 0) {
        // Уже смонтировано
        cout << "VFS already mounted at " << mount_point << endl;
        return true;
    }
    
    // Запускаем FUSE в отдельном процессе
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс для FUSE
        string fuse_path = "./users_vfs";
        if (!file_exists(fuse_path)) {
            // Если не найден, ищем в PATH
            fuse_path = find_in_path("users_vfs");
        }
        
        if (!fuse_path.empty()) {
            char* argv[] = {
                const_cast<char*>(fuse_path.c_str()),
                const_cast<char*>(mount_point.c_str()),
                nullptr
            };
            
            execv(argv[0], argv);
            exit(127);
        } else {
            cerr << "FUSE VFS binary not found. Please build users_vfs first." << endl;
            exit(1);
        }
    } else if (pid > 0) {
        // Даем время на монтирование
        sleep(1);
        
        // Проверяем успешность монтирования
        string check_cmd2 = "mount | grep -q \"" + mount_point + "\"";
        if (system(check_cmd2.c_str()) == 0) {
            cout << "VFS mounted successfully at " << mount_point << endl;
            vfs_mounted = true;
            return true;
        }
    }
    
    return false;
}

// Функция для размонтирования VFS
void unmount_vfs() {
    string home_dir = getenv("HOME");
    string mount_point = home_dir + "/users";
    
    if (dir_exists(mount_point)) {
        string cmd = "fusermount -u \"" + mount_point + "\" 2>/dev/null";
        system(cmd.c_str());
    }
}

// Команда для управления VFS
void handle_vfs_command(const vector<string>& args) {
    if (args.size() < 2) {
        cout << "Usage: vfs [mount|unmount|status]" << endl;
        return;
    }
    
    string command = args[1];
    
    if (command == "mount") {
        if (mount_vfs()) {
            cout << "VFS mounted successfully" << endl;
        } else {
            cout << "Failed to mount VFS" << endl;
        }
    } else if (command == "unmount") {
        unmount_vfs();
        vfs_mounted = false;
        cout << "VFS unmounted" << endl;
    } else if (command == "status") {
        string home_dir = getenv("HOME");
        string mount_point = home_dir + "/users";
        string check_cmd = "mount | grep \"" + mount_point + "\"";
        
        if (system(check_cmd.c_str()) == 0) {
            cout << "VFS is mounted at " << mount_point << endl;
        } else {
            cout << "VFS is not mounted" << endl;
        }
    } else {
        cout << "Unknown vfs command: " << command << endl;
    }
}

int main() 
{
    vector<string> history;
    string input;
    
    // Файл истории
    string home_dir = getenv("HOME");
    string history_file = home_dir + "/.kubsh_history";
    ofstream write_file(history_file, ios::app);
    
    // Устанавливаем обработчики сигналов
    signal(SIGHUP, handle_sighup);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Автоматически монтируем VFS при запуске
    if (!mount_vfs()) {
        cout << "Warning: Could not mount VFS automatically" << endl;
        cout << "You can mount it manually with: vfs mount" << endl;
    }
    
    // Основной цикл
    while (running) 
    {
        // Выводим приглашение только если stdin - терминал
        if (isatty(STDIN_FILENO)) {
            cout << "kubsh> ";
        }
        cout.flush();
        
        // Чтение ввода
        if (!getline(cin, input)) {
            if (cin.eof()) {
                break;  // Ctrl+D
            }
            continue;
        }
        
        if (input.empty()) {
            continue;
        }
        
        // Сохраняем в историю
        if (write_file.is_open()) {
            write_file << input << endl;
            write_file.flush();
        }
        history.push_back(input);
        
        // Разбиваем ввод на аргументы
        vector<string> args;
        stringstream ss(input);
        string token;
        while (ss >> token) {
            args.push_back(token);
        }
        
        if (args.empty()) {
            continue;
        }
        
        // Обработка команд
        if (args[0] == "\\q") {
            running = false;
            break;
        }
        else if (args[0] == "debug" || args[0] == "echo") {
            if (args.size() == 1) {
                cout << endl;
            } else {
                string result;
                for (size_t i = 1; i < args.size(); ++i) {
                    if (i > 1) result += " ";
                    result += args[i];
                }
                
                // Удаляем кавычки
                if (result.size() >= 2) {
                    char first = result[0];
                    char last = result[result.size()-1];
                    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                        result = result.substr(1, result.size()-2);
                    }
                }
                
                cout << result << endl;
            }
        }
        else if (args[0] == "\\e" && args.size() > 1) {
            if (args[1][0] == '$') {
                string var_name = args[1].substr(1);
                const char* env_value = getenv(var_name.c_str());
                
                if (env_value != nullptr) {
                    string value(env_value);
                    if (value.find(':') != string::npos) {
                        stringstream ss(value);
                        string item;
                        while (getline(ss, item, ':')) {
                            cout << item << endl;
                        }
                    } else {
                        cout << value << endl;
                    }
                } else {
                    cout << "Environment variable '" << var_name << "' not found" << endl;
                }
            }
        }
        else if (args[0] == "\\l" && args.size() > 1) {
            string device = args[1];
            
            if (device.empty()) {
                cout << "Usage: \\l <device>" << endl;
            } else {
                string command = "fdisk -l " + device + " 2>/dev/null || lsblk " + device + " 2>/dev/null || true";
                string output = exec(command.c_str());
                if (output.empty()) {
                    cout << "Could not get partition information for " << device << endl;
                } else {
                    cout << output;
                }
            }
        }
        else if (args[0] == "vfs") {
            handle_vfs_command(args);
        }
        else {
            // Пытаемся выполнить как внешнюю команду
            if (!execute_external(args)) {
                cout << args[0] << ": command not found" << endl;
            }
        }
        
        cout.flush();
    }
    
    // Размонтируем VFS при выходе
    if (vfs_mounted) {
        unmount_vfs();
    }
    
    if (write_file.is_open()) {
        write_file.close();
    }
    
    return 0;
}
