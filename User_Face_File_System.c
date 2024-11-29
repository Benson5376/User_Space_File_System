#define FUSE_USE_VERSION 30

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <fuse.h>

unsigned int uid_nctuos;
unsigned int gid_nctuos;

static struct fuse_operations op;

struct GnuTarHeader
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    unsigned char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

struct FILE_IN_TAR {
    char t_name[100];
    uid_t t_st_uid;
    gid_t t_st_gid;
    time_t t_st_mtime;
    off_t t_st_size;
    mode_t t_st_mode;
    char typeflag;
    char data_content[10240];
};

struct FILE_IN_TAR files_in_tar[50];
struct FILE_IN_TAR file_in_tar;
unsigned int num_files_in_tar = 0;

int get_system_user_id(char* user, unsigned int *user_id, unsigned int *groupd_id) {
    FILE *fp = NULL;
    char *line_data = NULL;
    char user_tmp[50] = {0}, password[50] = {0}, tmp[50];
    char user_id_tmp[10] = {0}, group_id_tmp[10] = {0};
    int offset = 0;
    ssize_t nread = 0, line_len = 0;

    fp = fopen("/etc/passwd", "r");
    while ((nread = getline(&line_data, &line_len, fp)) != -1) {
        sscanf(line_data, "%[^:]:%[^:]:%[^:]:%[^:]:%n", user_tmp, password, user_id_tmp, group_id_tmp, &offset);

        if (strcmp(user_tmp, user) == 0) {
            *user_id = atoi(user_id_tmp);
            *groupd_id = atoi(group_id_tmp);
            break;
        }
    }
    if (fp) {
        fclose(fp);
    }
    if (nread == -1) {
        printf("There is no user named: %s\n", user);
        return 0;
    }

    return 1;
}

void parse_tar_header(struct GnuTarHeader *tarHeader) {
    int i;

    off_t currentFileSize = 0;

    if (tarHeader->size[0] & (0X01 << 7)) {
        for (i = 1; i < 12; i++) {
            currentFileSize *= 256;
            currentFileSize += tarHeader->size[i];
        }
    } else {
        for (i = 0; i < 12; i++) {
            if ((0 == tarHeader->size[i]) || (' ' == tarHeader->size[i])) {
                continue;
            }
            currentFileSize *= 8;
            currentFileSize += (tarHeader->size[i] - '0');
        }
    }
    file_in_tar.t_st_size = currentFileSize;

    size_t len = strlen(tarHeader->name);
    if (len > 0) {
        if (tarHeader->name[len - 1] == '/') {
            tarHeader->name[len - 1] = '\0';
        }
    }
    strcpy(file_in_tar.t_name, tarHeader->name);

    if (!strcmp(tarHeader->uname, "root")) {
        file_in_tar.t_st_uid = 0;
    } else if (!strcmp(tarHeader->uname, "nctuos")) {
        file_in_tar.t_st_uid = uid_nctuos;
    } else {
        printf("incorrect uname: %s\n", tarHeader->uname);
    }

    if (!strcmp(tarHeader->gname, "root")) {
        file_in_tar.t_st_gid = 0;
    } else if (!strcmp(tarHeader->gname, "nctuos")) {
        file_in_tar.t_st_gid = gid_nctuos;
    } else {
        printf("incorrect gname: %s\n", tarHeader->gname);
    }

    time_t time_tar = 0;
    for (int i = 0; i < 12; ++i) {
        if ((0 == tarHeader->mtime[i]) || (' ' == tarHeader->mtime[i])) {
            continue;
        }
        time_tar *= 8;
        time_tar += (tarHeader->mtime[i] - '0');
    }
    file_in_tar.t_st_mtime = time_tar;

    mode_t mode_tar = 0;
    for (int i = 0; i < 8; ++i) {
        if ((0 == tarHeader->mode[i]) || (' ' == tarHeader->mode[i])) {
            continue;
        }
        mode_tar *= 8;
        mode_tar += (tarHeader->mode[i] - '0');
    }
    file_in_tar.t_st_mode = mode_tar;

    file_in_tar.typeflag = tarHeader->typeflag;
}

void parse_tar_fs() {
    int fd = 0;
    int ret = 0;
    unsigned long long seek = 0;
    struct GnuTarHeader gnuTarHeader, emptyHeader;
    int emptyHeaders = 0;
    char file_content[1024] = {'\0'};

    fd = open("./test.tar", O_RDONLY);
    assert(fd != -1);

    memset(&emptyHeader, 0, 512);

    unsigned int data_seek_tmp = 0;
    while (1) {
        ret = read(fd, &gnuTarHeader, 512);
        assert(ret == 512);
        data_seek_tmp += 512;

        if (memcmp(&gnuTarHeader, &emptyHeader, 512) == 0) {
            emptyHeaders++;
            if (emptyHeaders == 2) {
                break;
            }
            continue;
        }
        emptyHeaders = 0;

        if (gnuTarHeader.typeflag != 'L') {
            parse_tar_header(&gnuTarHeader);

            if (file_in_tar.t_st_size > 0) {
                ret = read(fd, file_in_tar.data_content, file_in_tar.t_st_size);
                file_in_tar.data_content[file_in_tar.t_st_size] = '\0';

                if (file_in_tar.t_st_size % 512 != 0) {
                    seek = 512 - (file_in_tar.t_st_size % 512);
                    seek = lseek(fd, seek, SEEK_CUR);
                    assert(seek != -1);
                }
            }

            int i = 0;
            for (i = 0; i < num_files_in_tar; ++i) {
                if (strlen(file_in_tar.t_name) == strlen(files_in_tar[i].t_name) && strcmp(file_in_tar.t_name, files_in_tar[i].t_name) == 0) {
                    if (file_in_tar.t_st_mtime > files_in_tar[i].t_st_mtime) {
                        memcpy(&files_in_tar[i], &file_in_tar, sizeof(struct FILE_IN_TAR));
                        break;
                    }
                }
            }

            if (i == num_files_in_tar) {
                memcpy(&files_in_tar[num_files_in_tar], &file_in_tar, sizeof(struct FILE_IN_TAR));
                ++num_files_in_tar;
            }
        }
    }

    close(fd);
}



static int my_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0444;
        stbuf->st_nlink = 0;
        return 0;
    }

    for (int i = 0; i < num_files_in_tar; ++i) {
        if (strcmp(path + 1, files_in_tar[i].t_name) == 0) {
            if (files_in_tar[i].typeflag == '5') {  // Folder
                stbuf->st_mode = S_IFDIR | files_in_tar[i].t_st_mode;
            } else {  // File
                stbuf->st_mode = S_IFREG | files_in_tar[i].t_st_mode;
            }
            stbuf->st_nlink = 0;
            stbuf->st_uid = files_in_tar[i].t_st_uid;
            stbuf->st_gid = files_in_tar[i].t_st_gid;
            stbuf->st_gid = files_in_tar[i].t_st_gid;
            stbuf->st_size = files_in_tar[i].t_st_size;
            stbuf->st_mtime = files_in_tar[i].t_st_mtime;
            return 0;
        }
    }

    return -ENOENT;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (strcmp(path, "/") == 0) {
        for (int i = 0; i < num_files_in_tar; ++i) {
            int len = strlen(files_in_tar[i].t_name);
            int j;
            for (j = 0; j < len; ++j) {
                if (files_in_tar[i].t_name[j] == '/') {
                    break;
                }
            }
            if (j == len) {
                filler(buf, files_in_tar[i].t_name, NULL, 0);
            }
        }
    } else {
        int path_len = strlen(path);
        for (int i = 0; i < num_files_in_tar; ++i) {
            if (strncmp(path + 1, files_in_tar[i].t_name, path_len - 1) == 0) {
                if (files_in_tar[i].t_name[path_len - 1] != '/') {
                    continue;
                }
                int len = strlen(files_in_tar[i].t_name);
                int j;
                for (j = path_len; j < len; ++j) {
                    if (files_in_tar[i].t_name[j] == '/') {
                        break;
                    }
                }
                if (j == len) {
                    filler(buf, files_in_tar[i].t_name + path_len, NULL, 0);
                }
            }
        }
    }

    return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {

    int path_len = strlen(path);
    for (int i = 0; i < num_files_in_tar; ++i) {
        if ((strlen(path) == (strlen(files_in_tar[i].t_name) + 1)) && (strncmp(path + 1, files_in_tar[i].t_name, strlen(files_in_tar[i].t_name))) == 0) {

            off_t len = files_in_tar[i].t_st_size;

            if (offset >= len) {
                return 0;
            }

            if (offset + size > len) {
                memcpy(buf, files_in_tar[i].data_content + offset, len - offset);
                return len - offset;
            } else {
                memcpy(buf, files_in_tar[i].data_content + offset, size);
                return size;
            }
        }
    }

    return -ENOENT;
}

int main(int argc, char *argv[])
{
    if (get_system_user_id("nctuos", &uid_nctuos, &gid_nctuos) == 0) {
        exit(1);
    }

    parse_tar_fs();

    memset(&op, 0, sizeof(op));

    op.getattr = my_getattr;
    op.readdir = my_readdir;
    op.read = my_read;

    return fuse_main(argc, argv, &op, NULL);
}
