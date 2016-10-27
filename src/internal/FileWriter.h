#ifndef FILEWRITER_H_INCLUDE
#define FILEWRITER_H_INCLUDE

#include <uv.h>

/**
 * The FileWriter class represents an operation which can write to a file.
 */
 class FileWriter
 {
public:
     FileWriter();
     ~FileWriter();

    /**
     * Writes the data in buf to the file pointed to by fd.
     * @return Returns true if all the data was written, false if failed to write.
     */
    bool write(int fd, void* buf, size_t size, size_t offset);
    const char* writeErrorMessage() const;
    uv_err_t writeError() const;
    int lastNumberOfBytesWrote() const;

private:
    uv_err_t m_error;
    int m_lastNumberOfBytesWrote;
 };
 #endif //FILEWRITER_H_INCLUDE