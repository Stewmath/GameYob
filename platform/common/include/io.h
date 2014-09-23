#pragma once

struct FileHandle;

FileHandle* file_open(const char*, const char*);    // Returns NULL if error occurs
void        file_close(FileHandle*);
void        file_read(void*, int, int, FileHandle*);
void        file_write(const void*, int, int, FileHandle*);
void        file_gets(char*, int, FileHandle*);
void        file_putc(char, FileHandle*);

void        file_rewind(FileHandle*);
int         file_tell(FileHandle*);
void        file_seek(FileHandle*, int, int);
int         file_getSize(FileHandle*);

void        file_printf(FileHandle*, const char*, ...);
