/*

Copyright (c) 2003, Arvid Norberg
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

#ifndef TORRENT_POLICY_HPP_INCLUDED
#define TORRENT_POLICY_HPP_INCLUDED

#include <algorithm>
#include <vector>

#include "libtorrent/peer.hpp"
#include "piece_picker.hpp"

// TODO: should be able to close connections with too low bandwidth to save memory

namespace libtorrent
{

	class torrent;
	class address;
	class peer_connection;

	class policy
	{
	public:

		policy(torrent* t);
		// this is called each time we get an incoming connection
		// return true to accept the connection
		bool accept_connection(const address& remote);

		// this is called once for every peer we get from
		// the tracker
		void peer_from_tracker(const address& remote, const peer_id& id);

		// the given connection was just closed
		void connection_closed(const peer_connection& c);

		// the peer has got at least one interesting piece
		void peer_is_interesting(peer_connection& c);

		void piece_finished(peer_connection& c, int index, bool successfully_verified);

		void block_finished(peer_connection& c, piece_block b);

		// the peer choked us
		void choked(peer_connection& c);

		// the peer unchoked us
		void unchoked(peer_connection& c);

		// the peer is interested in our pieces
		void interested(peer_connection& c);

		// the peer is not interested in our pieces
		void not_interested(peer_connection& c);

	private:

		int m_num_peers;
		torrent* m_torrent;

	};

}

#endif // TORRENT_POLICY_HPP_INCLUDED
