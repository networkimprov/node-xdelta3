#ifndef FILEREADER_H_INCLUDE
#define FILEREADER_H_INCLUDE

#include <cstddef> //For size_t

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
    int read(int fd, const char* buf, size_t size, size_t offset);
    const char* readErrorMessage() const;
    int readError() const;
    bool isError() const;

private:
    int m_error;
 };
 #endif //FILEREADER_H_INCLUDE