#include <fs/fs_Fat.hpp>

namespace fs {

    namespace {

        bool g_Initialized = false;

        inline constexpr const char *GetOpenMode(const OpenMode mode) {
            switch(mode) {
                case OpenMode::Read: {
                    return "rb";
                }
                case OpenMode::Write: {
                    return "wb";
                }
                case OpenMode::Update: {
                    return "ab";
                }
                default: {
                    return nullptr;
                }
            }
        }

    }

    bool FatFileHandle::Exists(const std::string &path, size_t &out_size) {
        struct stat st;
        if(stat(path.c_str(), &st) == 0) {
            if(st.st_mode & S_IFREG) {
                out_size = st.st_size;
                return true;
            }
        }
        return false;
    }

    bool FatFileHandle::Open(const std::string &path, const OpenMode mode) {
        const auto f_mode = GetOpenMode(mode);
        if(f_mode == nullptr) {
            return false;
        }
        this->file = fopen(path.c_str(), f_mode);
        return this->file != nullptr;
    }

    size_t FatFileHandle::GetSize() {
        const auto cur_pos = ftell(this->file);
        fseek(this->file, 0, SEEK_END);
        const auto f_size = ftell(this->file);
        fseek(this->file, cur_pos, SEEK_SET);
        return f_size;
    }

    bool FatFileHandle::SetOffset(const size_t offset, const Position pos) {
        switch(pos) {
            case Position::Begin: {
                return fseek(this->file, offset, SEEK_SET) == 0;
            }
            case Position::Current: {
                return fseek(this->file, offset, SEEK_CUR) == 0;
            }
            case Position::End: {
                return fseek(this->file, offset, SEEK_END) == 0;
            }
            default: {
                return false;
            }
        }
    }

    size_t FatFileHandle::GetOffset() {
        return ftell(this->file);
    }

    bool FatFileHandle::Read(void *read_buf, const size_t read_size) {
        return fread(read_buf, read_size, 1, this->file) == 1;
    }

    bool FatFileHandle::Write(const void *write_buf, const size_t write_size) {
        return fwrite(write_buf, write_size, 1, this->file) == 1;
    }

    bool FatFileHandle::Close() {
        const auto res = fclose(this->file);
        this->file = nullptr;
        return res == 0;
    }

    bool InitializeFat() {
        if(g_Initialized) {
            return true;
        }

        if(!fatInitDefault()) {
            return false;
        }

        g_Initialized = true;
        return true;
    }

    // Note: these are just wrappers for stdio stuff, but they're named by "Fat" since it's the only location where they are used here (the rest is basically nitrofs filesystems)

    bool CreateFatDirectory(const std::string &dir) {
        auto pos_init = 0;
        auto pos_found = 0;

        std::string token = "";
        std::string cur_dir = "";
        while(pos_found >= 0) {
            pos_found = dir.find('/', pos_init);
            token = dir.substr(pos_init, pos_found - pos_init);

            pos_init = pos_found + 1;

            if(!cur_dir.empty()) {
                cur_dir += "/";
            }
            cur_dir += token;
            if(mkdir(cur_dir.c_str(), 777) != 0) {
                if(errno != EEXIST) {
                    return false;
                }
            }
        }
        return true;
    }

    bool IsFatFile(const std::string &path) {
        struct stat st;
        if(stat(path.c_str(), &st) == 0) {
            if(st.st_mode & S_IFREG) {
                return true;
            }
        }
        return false;
    }

    size_t GetFatFileSize(const std::string &path) {
        struct stat st;
        if(stat(path.c_str(), &st) == 0) {
            if(st.st_mode & S_IFREG) {
                return st.st_size;
            }
        }
        return 0;
    }

    bool DeleteFatFile(const std::string &path) {
        return remove(path.c_str()) == 0;
    }

    bool DeleteFatDirectory(const std::string &path) {
        auto dir = opendir(path.c_str());
        if(dir) {
            std::vector<std::string> subfiles;
            std::vector<std::string> subdirs;
            while(true) {
                const auto ent = readdir(dir);
                if(ent == nullptr) {
                    break;
                }
                if(strcmp(ent->d_name, ".") == 0) {
                    continue;
                }
                if(strcmp(ent->d_name, "..") == 0) {
                    continue;
                }
                const auto subpath = path + "/" + ent->d_name;
                if(ent->d_type & DT_DIR) {
                    subdirs.push_back(subpath);
                }
                else if(ent->d_type & DT_REG) {
                    subfiles.push_back(subpath);
                }
            }
            closedir(dir);
            for(const auto &subfile : subfiles) {
                if(!DeleteFatFile(subfile)) {
                    return false;
                }
            }
            for(const auto &subdir : subdirs) {
                if(!DeleteFatDirectory(subdir)) {
                    return false;
                }
            }
        }
        
        // TODO: next libfat release which supports rmdir...
        // return rmdir(path.c_str()) == 0;
        return true;
    }

    bool ListAllFatFiles(const std::string &path, std::vector<std::string> &out_files) {
        auto dir = opendir(path.c_str());
        if(dir) {
            std::vector<std::string> subdirs;
            while(true) {
                const auto ent = readdir(dir);
                if(ent == nullptr) {
                    break;
                }
                if(strcmp(ent->d_name, ".") == 0) {
                    continue;
                }
                if(strcmp(ent->d_name, "..") == 0) {
                    continue;
                }
                const auto subpath = path + "/" + ent->d_name;
                if(ent->d_type & DT_DIR) {
                    subdirs.push_back(subpath);
                }
                else if(ent->d_type & DT_REG) {
                    out_files.push_back(subpath);
                }
            }
            closedir(dir);
            for(const auto &subdir : subdirs) {
                if(!ListAllFatFiles(subdir, out_files)) {
                    return false;
                }
            }
        }
        return true;
    }

}