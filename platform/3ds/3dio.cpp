#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <3ds.h>
#include <3ds/types.h>
#include <3ds/svc.h>
#include <cwchar>
#include "io.h"
#include "error.h"

FS_archive sdmcArchive;

struct FileHandle {
    Handle handle;
    size_t head;
};

struct DirStruct {
    Handle handle;
    struct dirent activeEntry;
};

char fs_cwd[MAX_FILENAME_LEN];
DirStruct dir;

// private functions

void fs_relativePath(char* dest, const char* src) {
    bool back = false;
    if (strcmp(src, "..") == 0 || strcmp(src, "../") == 0) {
        if (dest != fs_cwd)
            strcpy(dest, fs_cwd);
        back = true;
    }
    else if (src[0] == '/')
        strcpy(dest, src);
    else {
        if (dest != fs_cwd)
            strcpy(dest, fs_cwd);
        if (dest[strlen(dest)-1] != '/')
            strcat(dest, "/");
        strcat(dest, src);
    }

    while (strlen(dest) > 1 &&
            strrchr(dest, '/') != 0 && strrchr(dest, '/') == dest+strlen(dest)-1)
        *strrchr(dest, '/') = '\0';

    if (back) {
        if (strrchr(dest, '/') != 0) {
            *(strrchr(dest, '/')) = '\0';
            if (strcmp(dest, "") == 0)
                strcpy(dest, "/");
        }
    }
}

// public functions

void fs_init() {
    /*
    // These checks don't seem to be working?
    u32 detected, writable;
    FSUSER_IsSdmcDetected(NULL, &detected);
    if (!detected)
        fatalerr("SD card not detected.");
    FSUSER_IsSdmcWritable(NULL, &writable);
    if (!writable)
        fatalerr("SD card not writable.");
    */


    sdmcArchive = (FS_archive){0x9, FS_makePath(PATH_EMPTY, "")};
    FSUSER_OpenArchive(NULL, &sdmcArchive);

    strcpy(fs_cwd, "");
    dir.handle = 0;
    fs_chdir("/");
}

FileHandle* file_open(const char* filename, const char* flags) {
#ifdef EMBEDDED_ROM
    return NULL;
#endif

    u32 openFlags = 0;

    for (uint i=0; i<strlen(flags); i++) {
        char c = flags[i];
        switch(c) {
            case 'r':
                openFlags |= FS_OPEN_READ;
                break;
            case 'w':
                openFlags |= FS_OPEN_WRITE | FS_OPEN_CREATE;
                break;
            case '+':
                if (openFlags & FS_OPEN_READ)
                    openFlags |= FS_OPEN_WRITE;
                else if (openFlags & FS_OPEN_WRITE)
                    openFlags |= FS_OPEN_READ | FS_OPEN_CREATE;
                break;
        }
    }

    char buffer[MAX_FILENAME_LEN];
    fs_relativePath(buffer, filename);

    FileHandle* fileHandle = (FileHandle*)malloc(sizeof(FileHandle));
    Result res = FSUSER_OpenFile(NULL, &fileHandle->handle, sdmcArchive,
            FS_makePath(PATH_CHAR, buffer), openFlags, FS_ATTRIBUTE_NONE);

    if (res) {
        free(fileHandle);
        return NULL;
    }

    fileHandle->head = 0;
    return fileHandle;
}

void file_close(FileHandle* fileHandle) {
    FSFILE_Close(fileHandle->handle);
    free(fileHandle);
}

void file_read(void* dest, int bs, int size, FileHandle* fileHandle) {
    u32 bytesRead;
    FSFILE_Read(fileHandle->handle, &bytesRead, fileHandle->head, dest, bs*size);
    fileHandle->head += bytesRead;
}

void file_write(const void* src, int bs, int size, FileHandle* fileHandle) {
    u32 bytesWritten;
    FSFILE_Write(fileHandle->handle, &bytesWritten, fileHandle->head, src, bs*size,
            FS_WRITE_FLUSH);
    fileHandle->head += bytesWritten;
}

void file_gets(char* buffer, int bufferSize, FileHandle* fileHandle) {
    u32 bytesRead;
    char* ptr = buffer;

    while (bufferSize > 1) {
        char c;
        FSFILE_Read(fileHandle->handle, &bytesRead, fileHandle->head, &c, 1);
        fileHandle->head += bytesRead;
        if (bytesRead != 1)
            break;

        *(ptr++) = c;
        bufferSize--;

        if (c == '\n' || c == '\0')
            break;
    }

    // bufferSize >= 1
    if (*ptr != '\0') {
        *(ptr++) = '\0';
        bufferSize--;
    }
}

void file_putc(char c, FileHandle* fileHandle) {
    file_write(&c, 1, 1, fileHandle);
}

int file_tell(FileHandle* fileHandle) {
    return fileHandle->head;
}

void file_seek(FileHandle* fileHandle, int pos, int origin) {
    switch (origin) {
        case SEEK_SET:
            fileHandle->head = pos;
            break;
        case SEEK_CUR:
            fileHandle->head += pos;
            break;
        case SEEK_END:
            fileHandle->head = file_getSize(fileHandle) - pos;
            break;
    }
}

int file_getSize(FileHandle* fileHandle) {
    u64 size;
    FSFILE_GetSize(fileHandle->handle, &size);
    return size;
}

void file_setSize(FileHandle* fileHandle, size_t size) {
    FSFILE_SetSize(fileHandle->handle, size);
}

void file_printf(FileHandle* fileHandle, const char* format, ...) {
    char buffer[512];

    va_list args;
    va_start(args, format);

    vsnprintf(buffer, 512, format, args);
    va_end(args);

    file_write(buffer, 1, strlen(buffer), fileHandle);
}

bool file_exists(const char* filename) {
    FileHandle* h = file_open(filename, "r");
    if (h == NULL)
        return false;
    else {
        file_close(h);
        return true;
    }
}

void fs_deleteFile(const char* filename) {
    char buffer[MAX_FILENAME_LEN];
    fs_relativePath(buffer, filename);
    FSUSER_DeleteFile(NULL, sdmcArchive, FS_makePath(PATH_CHAR, buffer));
}

struct dirent* fs_readdir() {
    u32 numEntries = 0;
    FS_dirent entry;
    FSDIR_Read(dir.handle, &numEntries, 1, &entry);

    if (numEntries == 0)
        return 0;

    for (int i=0; i<MAX_FILENAME_LEN; i++) {
        dir.activeEntry.d_name[i] = (char)entry.name[i];
        if (dir.activeEntry.d_name[i] == '\0')
            break;
    }
    dir.activeEntry.d_type = 0;
    if (entry.isDirectory)
        dir.activeEntry.d_type |= DT_DIR;

    return &dir.activeEntry;
}

void fs_getcwd(char* dest, size_t maxLen) {
    strncpy(dest, fs_cwd, maxLen);
}
void fs_chdir(const char* s) {
    char buffer[MAX_FILENAME_LEN];
    fs_relativePath(buffer, s);

    if (dir.handle != 0)
        FSDIR_Close(dir.handle);

    Result res = FSUSER_OpenDirectory(NULL, &dir.handle, sdmcArchive,
            FS_makePath(PATH_CHAR, buffer));

    if (res) {
        FSUSER_OpenDirectory(NULL, &dir.handle, sdmcArchive,
                FS_makePath(PATH_CHAR, fs_cwd));
        return;
    }

    strcpy(fs_cwd, buffer);
}
