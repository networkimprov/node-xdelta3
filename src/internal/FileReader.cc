#include "FileReader.h"
#include <uv.h>

///////////////////////////////////////////////////////////////////////////////
FileReader::FileReader() :
  m_error(0)
{

}

///////////////////////////////////////////////////////////////////////////////
FileReader::~FileReader()
{}

///////////////////////////////////////////////////////////////////////////////
int FileReader::read(int fd, const char* buf, size_t size, size_t offset)
{
  uv_fs_t aUvRequest;
  int aBytesRead(0);
  m_error = 0;

  uv_buf_t uvBuffer;
  uvBuffer.base = const_cast<char*>(buf);
  uvBuffer.len = size;

  int result = uv_fs_read(uv_default_loop(), &aUvRequest, fd, &uvBuffer, 1, offset, NULL);

  // @see http://docs.libuv.org/en/v1.x/migration_010_100.html
  if (result < 0 )
  {
    m_error = result;
  }
  else
  {
    aBytesRead = result;
  }

  return aBytesRead;
}

///////////////////////////////////////////////////////////////////////////////
bool FileReader::isError() const
{
  return m_error != 0;
}

///////////////////////////////////////////////////////////////////////////////
const char* FileReader::readErrorMessage() const
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
int FileReader::readError() const
{
    return m_error;
}
