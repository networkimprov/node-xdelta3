#ifndef FILEREADER_H_INCLUDE
#define FILEREADER_H_INCLUDE

#include <uv.h>

/**
 * The FileReader class represents an operation which can read a file.
 */
 class FileReader
 {
public:
     FileReader();
     ~FileReader();

    /**
     * Reads data from a file pointed to by fd and stores the data in buf.
     * @return Returns the number of bytes actually read.
     */
    int read(int fd, void* buf, size_t size, size_t offset);
    const char* readErrorMessage() const;
    uv_err_t readError() const;

private:
    uv_err_t m_error;
 };
 #endif //FILEREADER_H_INCLUDE