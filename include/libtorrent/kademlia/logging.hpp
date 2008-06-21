/*

Copyright (c) 2006, Arvid Norberg & Daniel Wallin
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_LOGGING_HPP
#define TORRENT_LOGGING_HPP

#include <iostream>
#include <fstream>

//. 2008.06.21 by chongyc
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/convenience.hpp>

//. 2008.06.21 by chongyc
std::string GetHomePath();

namespace libtorrent { namespace dht
{

//. 2008.06.21 by chongyc
namespace fs = boost::filesystem;

class log
{
public:
	//m 2008.06.21 by chongyc
	log(char const* id, fs::path const& filename, bool append = true)
		: m_id(id)
		, m_enabled(true)
	{
		try
		{
			fs::path logpath(GetHomePath());
			fs::path dir(fs::complete(logpath / "log_dht"));

			//. 2008.06.21 by chongyc
			m_logpath = dir / filename;
			m_append = append;
		}
		catch (std::exception& e)
		{
			std::cerr << "failed to create log '" << filename.string() << "': " << e.what() << std::endl;
		}
	}

	char const* id() const
	{
		return m_id;
	}

	bool enabled() const
	{
		return m_enabled;
	}

	void enable(bool e)
	{
		m_enabled = e;
	}
	
	void flush() { m_stream.flush(); }

	template<class T>
	log& operator<<(T const& x)
	{
		//. 2008.06.21 by chongyc
		try
		{
			if (!m_stream.is_open())
			{
				fs::path dir = m_logpath.branch_path();
				if (!fs::exists(dir))
					fs::create_directories(dir);

				m_stream.open(m_logpath.string().c_str()
					, std::ios_base::out | (m_append ? std::ios_base::app : std::ios_base::out));

				*this << "\n\n\n*** starting log ***\n";
			}

			m_stream << x;
			m_stream.flush();
		}
		catch (std::exception& e)
		{
			std::cerr << "failed to write log '" << m_logpath.string() << "': " << e.what() << std::endl;
		}

		return *this;
	}

private:
	char const* m_id;
	bool m_enabled;
	std::ofstream m_stream;

	//. 2008.06.21 by chongyc
	fs::path m_logpath;
	bool m_append;
};

class log_event
{
public:
	log_event(log& log) 
		: log_(log) 
	{
		if (log_.enabled())
			log_ << '[' << log.id() << "] ";
	}

	~log_event()
	{
		if (log_.enabled())
		{
			log_ << "\n";
			log_.flush();
		}
	}

	template<class T>
	log_event& operator<<(T const& x)
	{
		log_ << x;
		return *this;
	}

	operator bool() const
	{
		return log_.enabled();
	}

private:	
	log& log_;
};

class inverted_log_event : public log_event
{
public:
	inverted_log_event(log& log) : log_event(log) {}

	operator bool() const
	{
		return !log_event::operator bool();
	}
};

} } // namespace libtorrent::dht

#define TORRENT_DECLARE_LOG(name) \
	libtorrent::dht::log& name ## _log()

#define TORRENT_DEFINE_LOG(name) \
	libtorrent::dht::log& name ## _log() \
	{ \
		static libtorrent::dht::log instance(#name, "dht.log", true); \
		return instance; \
	}

#define TORRENT_LOG(name) \
	if (libtorrent::dht::inverted_log_event event_object__ = name ## _log()); \
	else static_cast<log_event&>(event_object__)

#endif

