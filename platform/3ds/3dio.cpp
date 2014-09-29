#include <stdio.h>
#include <stdarg.h>
#include <3ds.h>
#include "io.h"

bool fsInitialized = false;
FS_archive sdmcArchive;

struct FileHandle {
    Handle handle;
    size_t head;
};


// public functions

FileHandle* file_open(const char* filename, const char* flags) {
    return NULL;
    if (!fsInitialized) {
        //sdmcArchive = (FS_archive){0x9, FS_makePath(PATH_EMPTY, "")};
        sdmcArchive = (FS_archive){0x9, FS_makePath(PATH_CHAR, "/")};
        FSUSER_OpenArchive(NULL, &sdmcArchive);
        fsInitialized = true;
    }

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
                openFlags |= FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE;
                break;
        }
    }

    FileHandle* fileHandle = (FileHandle*)malloc(sizeof(FileHandle));
    Result res = FSUSER_OpenFile(NULL, &fileHandle->handle, sdmcArchive,
            FS_makePath(PATH_CHAR, filename), openFlags, FS_ATTRIBUTE_NONE);

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

void file_printf(FileHandle* fileHandle, const char* format, ...) {
    char buffer[512];

    va_list args;
    va_start(args, format);

    vsnprintf(buffer, 512, format, args);

    file_write(buffer, 1, strlen(buffer), fileHandle);
}
