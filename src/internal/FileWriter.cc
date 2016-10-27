#include "FileWriter.h"

///////////////////////////////////////////////////////////////////////////////
FileWriter::FileWriter() :
    m_lastNumberOfBytesWrote( 0 )
{

}

///////////////////////////////////////////////////////////////////////////////
FileWriter::~FileWriter()
{}

///////////////////////////////////////////////////////////////////////////////
bool FileWriter::write(int fd, void* buf, size_t size, size_t offset)
{
  uv_fs_t aUvRequest;
  int aBytesWrote = uv_fs_write(uv_default_loop(), &aUvRequest, fd, buf, size, offset, NULL);
  m_lastNumberOfBytesWrote = aBytesWrote - size;

  if (aBytesWrote != static_cast<int>(size) ) 
  {
    m_error = uv_last_error(uv_default_loop());
    return false;
  }
 
  return true;
}

///////////////////////////////////////////////////////////////////////////////
const char* FileWriter::writeErrorMessage() const
{
    return uv_strerror(m_error);
}

///////////////////////////////////////////////////////////////////////////////
uv_err_t FileWriter::writeError() const
{
    return m_error;
}

///////////////////////////////////////////////////////////////////////////////
int FileWriter::lastNumberOfBytesWrote() const
{
    return m_lastNumberOfBytesWrote;
}