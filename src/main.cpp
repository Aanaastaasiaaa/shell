#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <ctime>
#include <string>
#include <vector>
#include <unordered_map>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <mutex>

using namespace std;

// Структура для хранения информации о файле
struct VFSFile {
    string name;
    string content;
    mode_t mode;
    time_t mtime;
    uid_t uid;
    gid_t gid;
};

// Структура для хранения информации о директории
struct VFSDir {
    string name;
    unordered_map<string, VFSFile> files;
    unordered_map<string, VFSDir> subdirs;
    time_t mtime;
    mode_t mode;
    uid_t uid;
    gid_t gid;
};

// Глобальная структура данных VFS
VFSDir root;
mutex vfs_mutex;

// Вспомогательные функции
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// Получить информацию о пользователе
string get_user_id(const string& username) {
    struct passwd* pw = getpwnam(username.c_str());
    if (pw) {
        return to_string(pw->pw_uid);
    }
    return "1000";
}

string get_user_home(const string& username) {
    struct passwd* pw = getpwnam(username.c_str());
    if (pw) {
        return string(pw->pw_dir);
    }
    return "/home/" + username;
}

string get_user_shell(const string& username) {
    struct passwd* pw = getpwnam(username.c_str());
    if (pw) {
        return string(pw->pw_shell);
    }
    return "/bin/bash";
}

// Инициализация VFS
void init_vfs() {
    lock_guard<mutex> lock(vfs_mutex);
    
    time_t now = time(nullptr);
    root.name = "";
    root.mtime = now;
    root.mode = S_IFDIR | 0755;
    root.uid = getuid();
    root.gid = getgid();
    
    // Читаем /etc/passwd
    ifstream passwd_file("/etc/passwd");
    if (passwd_file.is_open()) {
        string line;
        while (getline(passwd_file, line)) {
            // Ищем пользователей с shell заканчивающимся на sh
            if (line.find(":sh") != string::npos || line.find("/bin/bash") != string::npos) {
                vector<string> parts;
                stringstream ss(line);
                string part;
                
                while (getline(ss, part, ':')) {
                    parts.push_back(part);
                }
                
                if (parts.size() >= 7) {
                    string username = parts[0];
                    string shell = parts[6];
                    
                    if (shell == "/bin/bash" || shell == "/bin/sh") {
                        // Создаем директорию пользователя
                        VFSDir user_dir;
                        user_dir.name = username;
                        user_dir.mtime = now;
                        user_dir.mode = S_IFDIR | 0755;
                        user_dir.uid = stoi(parts[2]);
                        user_dir.gid = stoi(parts[3]);
                        
                        // Создаем файлы в директории пользователя
                        VFSFile id_file;
                        id_file.name = "id";
                        id_file.content = parts[2]; // UID
                        id_file.mode = S_IFREG | 0644;
                        id_file.mtime = now;
                        id_file.uid = user_dir.uid;
                        id_file.gid = user_dir.gid;
                        
                        VFSFile home_file;
                        home_file.name = "home";
                        home_file.content = parts[5]; // Home directory
                        home_file.mode = S_IFREG | 0644;
                        home_file.mtime = now;
                        home_file.uid = user_dir.uid;
                        home_file.gid = user_dir.gid;
                        
                        VFSFile shell_file;
                        shell_file.name = "shell";
                        shell_file.content = shell;
                        shell_file.mode = S_IFREG | 0644;
                        shell_file.mtime = now;
                        shell_file.uid = user_dir.uid;
                        shell_file.gid = user_dir.gid;
                        
                        // Добавляем файлы в директорию пользователя
                        user_dir.files["id"] = id_file;
                        user_dir.files["home"] = home_file;
                        user_dir.files["shell"] = shell_file;
                        
                        // Добавляем директорию пользователя в корень
                        root.subdirs[username] = user_dir;
                    }
                }
            }
        }
        passwd_file.close();
    }
}

// Разбор пути
vector<string> split_path(const char* path) {
    vector<string> parts;
    stringstream ss(path);
    string part;
    
    while (getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

// Получить директорию по пути
VFSDir* get_dir(const vector<string>& path_parts) {
    VFSDir* current = &root;
    
    for (const auto& part : path_parts) {
        auto it = current->subdirs.find(part);
        if (it == current->subdirs.end()) {
            return nullptr;
        }
        current = &(it->second);
    }
    
    return current;
}

// Получить файл по пути
VFSFile* get_file(const vector<string>& path_parts) {
    if (path_parts.empty()) return nullptr;
    
    vector<string> dir_parts(path_parts.begin(), path_parts.end() - 1);
    string filename = path_parts.back();
    
    VFSDir* dir = get_dir(dir_parts);
    if (!dir) return nullptr;
    
    auto it = dir->files.find(filename);
    if (it == dir->files.end()) return nullptr;
    
    return &(it->second);
}

// FUSE операции
static int vfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;
    lock_guard<mutex> lock(vfs_mutex);
    
    memset(stbuf, 0, sizeof(struct stat));
    
    vector<string> parts = split_path(path);
    
    if (parts.empty()) {
        // Корневая директория
        stbuf->st_mode = root.mode;
        stbuf->st_nlink = 2 + root.subdirs.size();
        stbuf->st_mtime = root.mtime;
        stbuf->st_uid = root.uid;
        stbuf->st_gid = root.gid;
        return 0;
    }
    
    VFSDir* dir = get_dir(parts);
    if (dir) {
        // Это директория
        stbuf->st_mode = dir->mode;
        stbuf->st_nlink = 2 + dir->subdirs.size();
        stbuf->st_mtime = dir->mtime;
        stbuf->st_uid = dir->uid;
        stbuf->st_gid = dir->gid;
        return 0;
    }
    
    VFSFile* file = get_file(parts);
    if (file) {
        // Это файл
        stbuf->st_mode = file->mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = file->content.size();
        stbuf->st_mtime = file->mtime;
        stbuf->st_uid = file->uid;
        stbuf->st_gid = file->gid;
        return 0;
    }
    
    return -ENOENT;
}

static int vfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info* fi,
                       enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    
    lock_guard<mutex> lock(vfs_mutex);
    
    filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
    
    vector<string> parts = split_path(path);
    VFSDir* dir = parts.empty() ? &root : get_dir(parts);
    
    if (!dir) {
        return -ENOENT;
    }
    
    // Добавляем поддиректории
    for (const auto& entry : dir->subdirs) {
        filler(buf, entry.first.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
    }
    
    // Добавляем файлы
    for (const auto& entry : dir->files) {
        filler(buf, entry.first.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
    }
    
    return 0;
}

static int vfs_open(const char* path, struct fuse_file_info* fi) {
    lock_guard<mutex> lock(vfs_mutex);
    
    vector<string> parts = split_path(path);
    VFSFile* file = get_file(parts);
    
    if (!file) {
        return -ENOENT;
    }
    
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
    }
    
    return 0;
}

static int vfs_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info* fi) {
    (void)fi;
    lock_guard<mutex> lock(vfs_mutex);
    
    vector<string> parts = split_path(path);
    VFSFile* file = get_file(parts);
    
    if (!file) {
        return -ENOENT;
    }
    
    size_t len = file->content.size();
    if (offset < len) {
        if (offset + size > len) {
            size = len - offset;
        }
        memcpy(buf, file->content.c_str() + offset, size);
    } else {
        size = 0;
    }
    
    return size;
}

static int vfs_mkdir(const char* path, mode_t mode) {
    lock_guard<mutex> lock(vfs_mutex);
    
    vector<string> parts = split_path(path);
    if (parts.empty()) {
        return -EEXIST;
    }
    
    vector<string> dir_parts(parts.begin(), parts.end() - 1);
    string dirname = parts.back();
    
    VFSDir* parent_dir = dir_parts.empty() ? &root : get_dir(dir_parts);
    if (!parent_dir) {
        return -ENOENT;
    }
    
    if (parent_dir->subdirs.find(dirname) != parent_dir->subdirs.end()) {
        return -EEXIST;
    }
    
    // Создаем новую директорию
    VFSDir new_dir;
    new_dir.name = dirname;
    new_dir.mtime = time(nullptr);
    new_dir.mode = S_IFDIR | (mode & 0777);
    new_dir.uid = getuid();
    new_dir.gid = getgid();
    
    // Если это директория пользователя, создаем файлы
    if (dir_parts.empty()) {  // Создание в корневой директории
        string username = dirname;
        
        // Создаем файлы пользователя
        VFSFile id_file;
        id_file.name = "id";
        id_file.content = get_user_id(username);
        id_file.mode = S_IFREG | 0644;
        id_file.mtime = new_dir.mtime;
        id_file.uid = new_dir.uid;
        id_file.gid = new_dir.gid;
        
        VFSFile home_file;
        home_file.name = "home";
        home_file.content = get_user_home(username);
        home_file.mode = S_IFREG | 0644;
        home_file.mtime = new_dir.mtime;
        home_file.uid = new_dir.uid;
        home_file.gid = new_dir.gid;
        
        VFSFile shell_file;
        shell_file.name = "shell";
        shell_file.content = get_user_shell(username);
        shell_file.mode = S_IFREG | 0644;
        shell_file.mtime = new_dir.mtime;
        shell_file.uid = new_dir.uid;
        shell_file.gid = new_dir.gid;
        
        new_dir.files["id"] = id_file;
        new_dir.files["home"] = home_file;
        new_dir.files["shell"] = shell_file;
        
        // Добавляем пользователя в систему
        string cmd = "sudo adduser --disabled-password --gecos '' " + username + " >/dev/null 2>&1";
        system(cmd.c_str());
    }
    
    parent_dir->subdirs[dirname] = new_dir;
    return 0;
}

static int vfs_rmdir(const char* path) {
    lock_guard<mutex> lock(vfs_mutex);
    
    vector<string> parts = split_path(path);
    if (parts.empty()) {
        return -EBUSY;  // Нельзя удалить корневую директорию
    }
    
    vector<string> dir_parts(parts.begin(), parts.end() - 1);
    string dirname = parts.back();
    
    VFSDir* parent_dir = dir_parts.empty() ? &root : get_dir(dir_parts);
    if (!parent_dir) {
        return -ENOENT;
    }
    
    auto it = parent_dir->subdirs.find(dirname);
    if (it == parent_dir->subdirs.end()) {
        return -ENOENT;
    }
    
    VFSDir& dir = it->second;
    
    // Проверяем, пуста ли директория
    if (!dir.subdirs.empty() || !dir.files.empty()) {
        return -ENOTEMPTY;
    }
    
    // Если это директория пользователя, удаляем пользователя из системы
    if (dir_parts.empty()) {  // Удаление из корневой директории
        string username = dirname;
        string cmd = "sudo userdel -r " + username + " >/dev/null 2>&1";
        system(cmd.c_str());
    }
    
    parent_dir->subdirs.erase(it);
    return 0;
}

static struct fuse_operations vfs_oper = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .open = vfs_open,
    .read = vfs_read,
    .mkdir = vfs_mkdir,
    .rmdir = vfs_rmdir,
};

int main(int argc, char* argv[]) {
    // Инициализируем VFS
    init_vfs();
    
    return fuse_main(argc, argv, &vfs_oper, NULL);
}
