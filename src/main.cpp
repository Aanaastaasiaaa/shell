#ifndef FUSE_USERS_H
#define FUSE_USERS_H

#include <fuse3/fuse.h>
#include <string>
#include <vector>
#include <map>

struct UserInfo {
    int uid;
    std::string username;
    std::string home;
    std::string shell;
};

class UsersFS {
private:
    std::string mount_point;
    std::map<std::string, UserInfo> users;
    std::map<std::string, std::string> file_contents;
    
    void scan_users();
    void update_file_contents();
    void create_user_directory(const std::string& username);
    void remove_user_directory(const std::string& username);
    
public:
    UsersFS(const std::string& mount_path);
    ~UsersFS();
    
    bool mount();
    void unmount();
    bool is_mounted() const;
    
    // FUSE операции
    static int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
    static int open(const char *path, struct fuse_file_info *fi);
    static int read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi);
    static int write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);
    static int create(const char *path, mode_t mode, struct fuse_file_info *fi);
    static int unlink(const char *path);
    static int mkdir(const char *path, mode_t mode);
    static int rmdir(const char *path);
};

// Глобальный указатель для доступа к файловой системе
extern UsersFS* g_users_fs;

#endif // FUSE_USERS_H
