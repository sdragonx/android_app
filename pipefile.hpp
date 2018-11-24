/*

 pipefile.hpp

 sdragonx 2018-11-24 13:34:18

*/
#ifndef PIPEFILE_HPP_20181124133418
#define PIPEFILE_HPP_20181124133418

#include <cgl/public.h>
#include <unistd.h>

namespace cgl{
namespace io{

class pipefile
{
private:
	int _pipe_io[2];
public:
	pipefile()
	{
		_pipe_io[0] = _pipe_io[1] = 0;
	}

	int create()
	{
		return pipe(_pipe_io);
	}

	int in()const
	{
		return _pipe_io[0];
	}

	int out()const
	{
		return _pipe_io[1];
	}

	void close()
	{
		if(_pipe_io[0])::close(_pipe_io[0]);
		if(_pipe_io[1])::close(_pipe_io[1]);
		_pipe_io[0] = _pipe_io[1] = 0;
	}

	int read(void* data, size_t size)
	{
		return ::read(_pipe_io[0], data, size);
	}

	int write(const void* data, size_t size)
	{
		return ::write(_pipe_io[1], data, size);
	}
};

}//end namespace io
}//end namespace cgl

#endif //PIPEFILE_HPP_20181124133418
