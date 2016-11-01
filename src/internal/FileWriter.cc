#include "FileWriter.h"
#include <uv.h>

///////////////////////////////////////////////////////////////////////////////
FileWriter::FileWriter() :
    m_error(0),
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
  m_lastNumberOfBytesWrote = 0;

  uv_buf_t uvBuffer;
  uvBuffer.base = static_cast<char*>(buf);
  uvBuffer.len = size;

  // @see http://docs.libuv.org/en/v1.x/migration_010_100.html
  int result = uv_fs_write(uv_default_loop(), &aUvRequest, fd, &uvBuffer, 1, offset, NULL);

  if ( result < 0 )
  {
      m_error = result;
      return false;
  }
  else
  {
      m_lastNumberOfBytesWrote = result;
  }
 
  return true;
}

///////////////////////////////////////////////////////////////////////////////
const char* FileWriter::writeErrorMessage() const
{
    if ( m_error == 0 )
    {
        return "Success";
    }
    else
    {
        return uv_strerror(m_error);
    }
}

///////////////////////////////////////////////////////////////////////////////
int FileWriter::writeError() const
{
    return m_error;
}

///////////////////////////////////////////////////////////////////////////////
int FileWriter::lastNumberOfBytesWrote() const
{
    return m_lastNumberOfBytesWrote;
}