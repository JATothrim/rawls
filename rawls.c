/**	Rawly recursively list files: rawls
 *
 *	(C) Copyright 2017 Jarmo A Tiitto (jarmo.tiitto@gmail.com)
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/**
 * This program lists all files recursively in directory staying
 * on same file system.
 * It outputs machine processable line entries per file.
 * It is a _very_ fast way to dump filesystem directory contents
 * for backup purposes or other processing.
 * (C) 2015 Jarmo A Tiitto
 **/
#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)


struct linux_dirent64 {
    ino64_t        d_ino;    /* 64-bit inode number */
    off64_t        d_off;    /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[]; /* Filename (null-terminated) */
};

#define BUF_SIZE 1024*1024*8

#define MAX_PATH_LEN 2048

// Recuses into directories
int list_files(char * currpath, dev_t fdev)
{
    int fd, nread;
    char * buf = NULL;
    struct linux_dirent64 *d;
    struct stat finfo;
    int bpos;
    unsigned char d_type;
    off_t f_size;
    int pathlen;
    int dirnempty = 1;
    // Check that we are still on correct device?
    // Get dir stat
    pathlen = strlen(currpath);
    strcat(currpath, "/.");
    if(stat(currpath, &finfo) == -1)
            perror("stat");
    *(currpath + pathlen) = 0;


    if(fdev != finfo.st_dev ) {
        // Nope!
        fprintf(stderr, "skipping: %s\n", currpath);
        return 1;
    }

    fd = open(currpath, O_RDONLY | O_DIRECTORY | O_NOATIME );
    if (fd == -1) {
        // Penalty for no access:
        fprintf(stderr, "error: %s ", currpath);
        perror("open");
        return 2;
    }

    // inform the system to not pollute file cache
    posix_fadvise(fd, 0,0, POSIX_FADV_NOREUSE);

    buf = (char*)malloc(BUF_SIZE);
    for(;;) {
        // Process about BUF_SIZE MiB of stuff at once enough even for 500 MiB/s SSD
        // This is the magic why rawls is so much faster than "find /".
        nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
        if(nread == -1) {
                free(buf);
                handle_error("getdents");
        }

        if(nread == 0)
            break;

        for(bpos = 0; bpos < nread;) {
            d = (struct linux_dirent64 *) (buf + bpos);
            d_type = d->d_type;

            // save current path lenght and append entry to the path.
            pathlen = strlen(currpath);

            if(currpath[pathlen - 1] != '/' )
                    strcat(currpath, "/");
            strcat(currpath, d->d_name);

            // stat() the entry
            if(stat(currpath, &finfo) == -1) {
                fprintf(stderr, "error: %s ", currpath);
                perror("stat");
            }

            if(d_type == DT_UNKNOWN) {
                // Some filesystems do not provide d_type from getdents64
                // In this case we need to use stat() result
                d_type = S_ISREG(finfo.st_mode) ? DT_REG :
                        S_ISDIR(finfo.st_mode) ? DT_DIR :
                        S_ISFIFO(finfo.st_mode) ? DT_FIFO :
                        S_ISSOCK(finfo.st_mode) ? DT_SOCK :
                        S_ISLNK(finfo.st_mode) ? DT_LNK :
                        S_ISBLK(finfo.st_mode) ? DT_BLK :
                        S_ISCHR(finfo.st_mode) ? DT_CHR : 0;
            }

            if(d_type != DT_DIR) {
                printf("%lld;", d->d_ino);

                printf("%s;", (d_type == DT_REG) ?  "regular" :
                            (d_type == DT_DIR) ?  "directory" :
                            (d_type == DT_FIFO) ? "FIFO" :
                            (d_type == DT_SOCK) ? "socket" :
                            (d_type == DT_LNK) ?  "symlink" :
                            (d_type == DT_BLK) ?  "blockdev" :
                            (d_type == DT_CHR) ?  "chardev" : "???");

                // Get file size if it is a regular file or symlink
                f_size = 0;
                if(d_type == DT_REG || d_type == DT_LNK) {
                    f_size = finfo.st_size;
                }
                printf("%lld;%d;%lld;'%s'\n", f_size, d->d_reclen, (long long) d->d_off, currpath);
                dirnempty = 0;

            } else if(strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0 ) {
                // Recurse into dir
                if(list_files(currpath, fdev)) {
                    // dir was empty or unaccessable, output leaf dir
                    if(stat(currpath, &finfo) == -1) {
                        fprintf(stderr, "error: %s ", currpath);
                        perror("stat");
                    }
                    printf("%ld;%s;%ld;%d;%lld;'%s'\n", d->d_ino, "directory", finfo.st_size, d->d_reclen, (long long) d->d_off, currpath);
                }
                dirnempty = 0;
            }
            // reset path and advance to next entry
            *(currpath + pathlen) = 0;
            bpos += d->d_reclen;
        }
    }

    // inform the system to not pollute file cache
    posix_fadvise(fd, 0,0, POSIX_FADV_DONTNEED);
    close(fd);
    free(buf);

    return dirnempty;
}

int main(int argc, char *argv[])
{
        char path[MAX_PATH_LEN] = {0};
        char * lsstart = argc > 1 ? argv[1] : ".";
        int stlen;
        struct stat rootinfo;

        stlen = strlen(lsstart);
        strcat(path, lsstart);

        // grab root stat
        if(lsstart[stlen - 1] != '/')
                strcat(path, "/");
        strcat(path, ".");
        if(stat(path, &rootinfo) == -1)
                handle_error("stat");

        // clear up the '.'
        path[stlen] = 0;

        // Delete '/' from path end unless its just root
        if(stlen > 1 && lsstart[stlen - 1] == '/')
                path[stlen - 1] = 0;

        // Process the file system
        list_files(path, rootinfo.st_dev);

        exit(EXIT_SUCCESS);
}
