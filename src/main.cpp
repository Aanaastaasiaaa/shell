#ifndef USERS_VFS_H
#define USERS_VFS_H

#include <fuse3/fuse.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

struct UserRecord {
    int uid;
    std::string username;
    std::string home_dir;
    std::string login_shell;
};

class UsersVirtualFS {
private:
    std::string mount_path;
    std::map<std::string, UserRecord> user_registry;
    std::map<std::string, std::string> vfs_file_cache;
    
    void scan_system_users();
    void rebuild_vfs_cache();
    void add_system_user(const std::string& username);
    void remove_system_user(const std::string& username);
    
public:
    UsersVirtualFS(const std::string& mount_path);
    ~UsersVirtualFS();
    
    bool mount_vfs();
    void unmount_vfs();
    bool is_vfs_mounted() const;
    
    // FUSE операции
    static int vfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
    static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
    static int vfs_open(const char *path, struct fuse_file_info *fi);
    static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi);
    static int vfs_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi);
    static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
    static int vfs_unlink(const char *path);
    static int vfs_mkdir(const char *path, mode_t mode);
    static int vfs_rmdir(const char *path);
    static int vfs_rename(const char *oldpath, const char *newpath, unsigned int flags);
};

// Глобальный экземпляр VFS
extern std::unique_ptr<UsersVirtualFS> global_vfs;

#endif // USERS_VFS_H
