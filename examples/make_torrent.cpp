/*

Copyright (c) 2006, Arvid Norberg
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

#include <iostream>
#include <fstream>
#include <iterator>
#include <iomanip>

#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/file.hpp"
#include "libtorrent/storage.hpp"
#include "libtorrent/hasher.hpp"
#include "libtorrent/file_pool.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace boost::filesystem;
using namespace libtorrent;

//. 2008.04.29 by chongyc
void modify_torrent(char* torrent_name)
{
	try
	{
		ifstream in(complete(path(torrent_name)), std::ios_base::binary);
		in.unsetf( std::ios_base::skipws );

		// read the entry from torrent file
		entry e = libtorrent::entry();
		e = libtorrent::bdecode( std::istream_iterator<char>(in), std::istream_iterator<char>() );
		in.close();

		// create torrent and modify tracker info
		boost::intrusive_ptr<torrent_info> t(new torrent_info(e));
		std::vector<announce_entry> *tracker_urls = (std::vector<announce_entry> *)&(t->trackers());
		tracker_urls->clear();
//		t->add_tracker("http://192.168.1.88/announce");
		t->add_tracker("http://221.200.112.101:2710/announce");

		// get info hash from torrent
		sha1_hash* info_hash = (sha1_hash*)&(t->info_hash());

		// calculate info hash
		const char* buf = "0123456789012345678901234567890123456789";
		hasher h;
		h.update(&buf[0], (int)20);
		*info_hash = h.final();

		// create the torrent and print it to out
		ofstream out(complete(path(torrent_name)), std::ios_base::binary);

		e = t->create_torrent();
		libtorrent::bencode(std::ostream_iterator<char>(out), e);

	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << "\n";
	}
}

void add_files(
	torrent_info& t
	, path const& p
	, path const& l)
{
	if (l.leaf()[0] == '.') return;
	path f(p / l);
	if (is_directory(f))
	{
		for (directory_iterator i(f), end; i != end; ++i)
			add_files(t, p, l / i->leaf());
	}
	else
	{
		std::cerr << "adding \"" << l.string() << "\"\n";
		t.add_file(l, file_size(f));
	}
}

int make_torrent(char* torrent_file, char* announce_url, char* file_path, char* url_seed)
{
	using namespace libtorrent;
	using namespace boost::filesystem;

	path::default_name_check(no_check);

	try
	{
		boost::intrusive_ptr<torrent_info> t(new torrent_info);
		path full_path = complete(file_path);
		ofstream out(complete(path(torrent_file)), std::ios_base::binary);

		int piece_size = 256 * 1024;
		char const* creator_str = "libtorrent";

		add_files(*t, full_path.branch_path(), full_path.leaf());
		t->set_piece_size(piece_size);

		file_pool fp;
		boost::scoped_ptr<storage_interface> st(
			default_storage_constructor(t, full_path.branch_path(), fp));
		t->add_tracker(announce_url);

		// calculate the hash for all pieces
		int num = t->num_pieces();
		std::vector<char> buf(piece_size);
		for (int i = 0; i < num; ++i)
		{
			st->read(&buf[0], i, 0, t->piece_size(i));
			hasher h(&buf[0], t->piece_size(i));
			t->set_hash(i, h.final());
			std::cerr << (i+1) << "/" << num << "\r";
		}

		t->set_creator(creator_str);

		if (url_seed != NULL)
			t->add_url_seed(url_seed);

		// create the torrent and print it to out
		entry e = t->create_torrent();
		libtorrent::bencode(std::ostream_iterator<char>(out), e);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << "\n";
	}

	return 0;
}


int main(int argc, char* argv[])
{
	using namespace libtorrent;
	using namespace boost::filesystem;

	path::default_name_check(no_check);

#if 0
	if (argc != 4 && argc != 5)
	{
		std::cerr << "usage: make_torrent <output torrent-file> "
			"<announce url> <file or directory to create torrent from> "
			"[url-seed]\n";
		return 1;
	}

	if (argc == 5)
	{
		make_torrent(argv[1], argv[2], argv[3], argv[4]);
	}
	else // argc == 4
	{
		make_torrent(argv[1], argv[2], argv[3], NULL);
	}
#else
	modify_torrent("Fedora-8-Live-i686.torrent");
#endif

	return 0;
}

std::string GetHomePath()
{
	std::string homepath = ".";

	return homepath;
}
