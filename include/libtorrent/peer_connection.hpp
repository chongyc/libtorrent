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

#ifndef TORRENT_PEER_CONNECTION_HPP_INCLUDED
#define TORRENT_PEER_CONNECTION_HPP_INCLUDED

#include <ctime>
#include <algorithm>
#include <vector>
#include <deque>
#include <string>

#include "libtorrent/debug.hpp"

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <boost/smart_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <boost/optional.hpp>
#include <boost/cstdint.hpp>
#include <boost/pool/pool.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "libtorrent/buffer.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/peer_id.hpp"
#include "libtorrent/storage.hpp"
#include "libtorrent/stat.hpp"
#include "libtorrent/alert.hpp"
#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/torrent.hpp"
#include "libtorrent/peer_request.hpp"
#include "libtorrent/piece_block_progress.hpp"
#include "libtorrent/config.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/bandwidth_limit.hpp"
#include "libtorrent/policy.hpp"
#include "libtorrent/socket_type.hpp"
#include "libtorrent/intrusive_ptr_base.hpp"
#include "libtorrent/assert.hpp"
#include "libtorrent/chained_buffer.hpp"
#include "libtorrent/disk_buffer_holder.hpp"
#include "libtorrent/bitfield.hpp"

namespace libtorrent
{
	class torrent;
	struct peer_plugin;

	namespace detail
	{
		struct session_impl;
	}

	struct pending_block
	{
		pending_block(piece_block const& b): skipped(0), block(b) {}
		int skipped;
		// the number of times the request
		// has been skipped by out of order blocks
		piece_block block;
	};

	struct has_block
	{
		has_block(piece_block const& b): block(b) {}
		piece_block const& block;
		bool operator()(pending_block const& pb) const
		{ return pb.block == block; }
	};

	class TORRENT_EXPORT peer_connection
		: public intrusive_ptr_base<peer_connection>
		, public boost::noncopyable
	{
	friend class invariant_access;
	public:

		enum channels
		{
			upload_channel,
			download_channel,
			num_channels
		};

		// this is the constructor where the we are the active part.
		// The peer_conenction should handshake and verify that the
		// other end has the correct id
		peer_connection(
			aux::session_impl& ses
			, boost::weak_ptr<torrent> t
			, boost::shared_ptr<socket_type> s
			, tcp::endpoint const& remote
			, policy::peer* peerinfo);

		// with this constructor we have been contacted and we still don't
		// know which torrent the connection belongs to
		peer_connection(
			aux::session_impl& ses
			, boost::shared_ptr<socket_type> s
			, tcp::endpoint const& remote
			, policy::peer* peerinfo);

		// this function is called after it has been constructed and properly
		// reference counted. It is safe to call self() in this function
		// and schedule events with references to itself (that is not safe to
		// do in the constructor).
		virtual void start();

		virtual ~peer_connection();

		void set_peer_info(policy::peer* pi)
		{ m_peer_info = pi; }

		policy::peer* peer_info_struct() const
		{ return m_peer_info; }

		enum peer_speed_t { slow, medium, fast };
		peer_speed_t peer_speed();

		void send_allowed_set();

#ifndef TORRENT_DISABLE_EXTENSIONS
		void add_extension(boost::shared_ptr<peer_plugin>);
#endif

		// this function is called once the torrent associated
		// with this peer connection has retrieved the meta-
		// data. If the torrent was spawned with metadata
		// this is called from the constructor.
		void init();

		// this is called when the metadata is retrieved
		// and the files has been checked
		virtual void on_metadata() {}

		void set_upload_limit(int limit);
		void set_download_limit(int limit);

		int upload_limit() const { return m_upload_limit; }
		int download_limit() const { return m_download_limit; }

		int prefer_whole_pieces() const
		{
			if (on_parole()) return 1;
			return m_prefer_whole_pieces;
		}

		bool on_parole() const
		{ return peer_info_struct() && peer_info_struct()->on_parole; }

		void prefer_whole_pieces(int num)
		{ m_prefer_whole_pieces = num; }

		bool request_large_blocks() const
		{ return m_request_large_blocks; }

		void request_large_blocks(bool b)
		{ m_request_large_blocks = b; }

		void set_priority(int p)
		{ m_priority = p; }

		void fast_reconnect(bool r);
		bool fast_reconnect() const { return m_fast_reconnect; }

		// this adds an announcement in the announcement queue
		// it will let the peer know that we have the given piece
		void announce_piece(int index);

		// tells if this connection has data it want to send
		// and has enough upload bandwidth quota left to send it.
		bool can_write() const;
		bool can_read() const;

		bool is_seed() const;

		void set_upload_only(bool u) { m_upload_only = u; }
		bool upload_only() const { return m_upload_only; }

		// will send a keep-alive message to the peer
		void keep_alive();

		peer_id const& pid() const { return m_peer_id; }
		void set_pid(const peer_id& pid) { m_peer_id = pid; }
		bool has_piece(int i) const;

		std::deque<pending_block> const& download_queue() const;
		std::deque<piece_block> const& request_queue() const;
		std::deque<peer_request> const& upload_queue() const;

		bool is_interesting() const { return m_interesting; }
		bool is_choked() const { return m_choked; }

		bool is_peer_interested() const { return m_peer_interested; }
		bool has_peer_choked() const { return m_peer_choked; }

		void update_interest();

		virtual void get_peer_info(peer_info& p) const;

		// returns the torrent this connection is a part of
		// may be zero if the connection is an incoming connection
		// and it hasn't received enough information to determine
		// which torrent it should be associated with
		boost::weak_ptr<torrent> associated_torrent() const
		{ return m_torrent; }

		const stat& statistics() const { return m_statistics; }
		void add_stat(size_type downloaded, size_type uploaded);

		void calc_ip_overhead();

		// is called once every second by the main loop
		void second_tick(float tick_interval);

		void timeout_requests();

		boost::shared_ptr<socket_type> get_socket() const { return m_socket; }
		tcp::endpoint const& remote() const { return m_remote; }

		bitfield const& get_bitfield() const;
		std::vector<int> const& allowed_fast();
		std::vector<int> const& suggested_pieces() const { return m_suggested_pieces; }

		ptime connected_time() const { return m_connect; }
		ptime last_received() const { return m_last_receive; }

		void timed_out();
		// this will cause this peer_connection to be disconnected.
		void disconnect(char const* message, int error = 0);
		bool is_disconnecting() const { return m_disconnecting; }

		// this is called when the connection attempt has succeeded
		// and the peer_connection is supposed to set m_connecting
		// to false, and stop monitor writability
		void on_connection_complete(error_code const& e);

		// returns true if this connection is still waiting to
		// finish the connection attempt
		bool is_connecting() const { return m_connecting; }

		// returns true if the socket of this peer hasn't been
		// attempted to connect yet (i.e. it's queued for
		// connection attempt).
		bool is_queued() const { return m_queued; }
	
		// called when it's time for this peer_conncetion to actually
		// initiate the tcp connection. This may be postponed until
		// the library isn't using up the limitation of half-open
		// tcp connections.	
		void connect(int ticket);
		
		// This is called for every peer right after the upload
		// bandwidth has been distributed among them
		// It will reset the used bandwidth to 0.
		void reset_upload_quota();

		// free upload.
		size_type total_free_upload() const;
		void add_free_upload(size_type free_upload);

		// trust management.
		void received_valid_data(int index);
		void received_invalid_data(int index);

		size_type share_diff() const;

		// a connection is local if it was initiated by us.
		// if it was an incoming connection, it is remote
		bool is_local() const { return m_active; }

		bool on_local_network() const;
		bool ignore_bandwidth_limits() const
		{ return m_ignore_bandwidth_limits; }

		bool failed() const { return m_failed; }

		int desired_queue_size() const { return m_desired_queue_size; }

		// compares this connection against the given connection
		// for which one is more eligible for an unchoke.
		// returns true if this is more eligible
		bool unchoke_compare(boost::intrusive_ptr<peer_connection const> const& p) const;

		// resets the byte counters that are used to measure
		// the number of bytes transferred within unchoke cycles
		void reset_choke_counters();

		// if this peer connection is useless (neither party is
		// interested in the other), disconnect it
		void disconnect_if_redundant();

#if defined TORRENT_VERBOSE_LOGGING || defined TORRENT_ERROR_LOGGING
		boost::shared_ptr<logger> m_logger;
#endif

		// the message handlers are called
		// each time a recv() returns some new
		// data, the last time it will be called
		// is when the entire packet has been
		// received, then it will no longer
		// be called. i.e. most handlers need
		// to check how much of the packet they
		// have received before any processing
		void incoming_keepalive();
		void incoming_choke();
		void incoming_unchoke();
		void incoming_interested();
		void incoming_not_interested();
		void incoming_have(int piece_index);
		void incoming_bitfield(bitfield const& bits);
		void incoming_request(peer_request const& r);
		void incoming_piece(peer_request const& p, disk_buffer_holder& data);
		void incoming_piece(peer_request const& p, char const* data);
		void incoming_piece_fragment();
		void incoming_cancel(peer_request const& r);

		void incoming_dht_port(int listen_port);
		
		void incoming_reject_request(peer_request const& r);
		void incoming_have_all();
		void incoming_have_none();
		void incoming_allowed_fast(int index);
		void incoming_suggest(int index);

		// the following functions appends messages
		// to the send buffer
		void send_choke();
		void send_unchoke();
		void send_interested();
		void send_not_interested();

		void snub_peer();

		// adds a block to the request queue
		void add_request(piece_block const& b);
		// removes a block from the request queue or download queue
		// sends a cancel message if appropriate
		// refills the request queue, and possibly ignoring pieces requested
		// by peers in the ignore list (to avoid recursion)
		void cancel_request(piece_block const& b);
		void send_block_requests();

		int max_assignable_bandwidth(int channel) const
		{ return m_bandwidth_limit[channel].max_assignable(); }
		
		int bandwidth_throttle(int channel) const
		{ return m_bandwidth_limit[channel].throttle(); }

		void assign_bandwidth(int channel, int amount);
		void expire_bandwidth(int channel, int amount);

#ifndef NDEBUG
		void check_invariant() const;
		ptime m_last_choke;
#endif


		// is true until we can be sure that the other end
		// speaks our protocol (be it bittorrent or http).
		virtual bool in_handshake() const = 0;

		// returns the block currently being
		// downloaded. And the progress of that
		// block. If the peer isn't downloading
		// a piece for the moment, the boost::optional
		// will be invalid.
		virtual boost::optional<piece_block_progress>
		downloading_piece_progress() const
		{
#ifdef TORRENT_VERBOSE_LOGGING
			(*m_logger) << "downloading_piece_progress() dispatched to the base class!\n";
#endif
			return boost::optional<piece_block_progress>();
		}

		// these functions are virtual to let bt_peer_connection hook into them
		// and encrypt the content
		enum message_type_flags { message_type_request = 1 };
		virtual void send_buffer(char const* begin, int size, int flags = 0);
		virtual buffer::interval allocate_send_buffer(int size);
		virtual void setup_send();

		template <class Destructor>
		void append_send_buffer(char* buffer, int size, Destructor const& destructor)
		{
			m_send_buffer.append_buffer(buffer, size, size, destructor);
#ifdef TORRENT_STATS
			m_ses.m_buffer_usage_logger << log_time() << " append_send_buffer: " << size << std::endl;
			m_ses.log_buffer_usage();
#endif
		}

#ifndef TORRENT_DISABLE_RESOLVE_COUNTRIES	
		void set_country(char const* c)
		{
			TORRENT_ASSERT(strlen(c) == 2);
			m_country[0] = c[0];
			m_country[1] = c[1];
		}
		bool has_country() const { return m_country[0] != 0; }
#endif

		int send_buffer_size() const
		{ return m_send_buffer.size(); }

		int send_buffer_capacity() const
		{ return m_send_buffer.capacity(); }

		int packet_size() const { return m_packet_size; }

		bool packet_finished() const
		{ return m_packet_size <= m_recv_pos; }

#ifndef NDEBUG
		bool piece_failed;
#endif

		// upload and download channel state
		// enum from peer_info::bw_state
		char m_channel_state[2];

	protected:

		virtual void get_specific_peer_info(peer_info& p) const = 0;

		virtual void write_choke() = 0;
		virtual void write_unchoke() = 0;
		virtual void write_interested() = 0;
		virtual void write_not_interested() = 0;
		virtual void write_request(peer_request const& r) = 0;
		virtual void write_cancel(peer_request const& r) = 0;
		virtual void write_have(int index) = 0;
		virtual void write_keepalive() = 0;
		virtual void write_piece(peer_request const& r, disk_buffer_holder& buffer) = 0;
		
		virtual void write_reject_request(peer_request const& r) = 0;
		virtual void write_allow_fast(int piece) = 0;

		virtual void on_connected() = 0;
		virtual void on_tick() {}
	
		virtual void on_receive(error_code const& error
			, std::size_t bytes_transferred) = 0;
		virtual void on_sent(error_code const& error
			, std::size_t bytes_transferred) = 0;

#ifndef TORRENT_DISABLE_ENCRYPTION
		buffer::interval wr_recv_buffer()
		{
			TORRENT_ASSERT(!m_disk_recv_buffer);
			TORRENT_ASSERT(m_disk_recv_buffer_size == 0);
			if (m_recv_buffer.empty()) return buffer::interval(0,0);
			return buffer::interval(&m_recv_buffer[0]
				, &m_recv_buffer[0] + m_recv_pos);
		}

		std::pair<buffer::interval, buffer::interval> wr_recv_buffers(int bytes);
#endif
		
		buffer::const_interval receive_buffer() const
		{
			if (m_recv_buffer.empty()) return buffer::const_interval(0,0);
			return buffer::const_interval(&m_recv_buffer[0]
				, &m_recv_buffer[0] + m_recv_pos);
		}

		bool allocate_disk_receive_buffer(int disk_buffer_size);
		char* release_disk_receive_buffer();
		bool has_disk_receive_buffer() const { return m_disk_recv_buffer; }
		void cut_receive_buffer(int size, int packet_size);
		void reset_recv_buffer(int packet_size);

		void setup_receive();

		void attach_to_torrent(sha1_hash const& ih);

		bool verify_piece(peer_request const& p) const;

		// the bandwidth channels, upload and download
		// keeps track of the current quotas
		bandwidth_limit m_bandwidth_limit[num_channels];

		// statistics about upload and download speeds
		// and total amount of uploads and downloads for
		// this peer
		stat m_statistics;

		// a back reference to the session
		// the peer belongs to.
		aux::session_impl& m_ses;

		// called from the main loop when this connection has any
		// work to do.
		void on_send_data(error_code const& error
			, std::size_t bytes_transferred);
		void on_receive_data(error_code const& error
			, std::size_t bytes_transferred);

		// this is the limit on the number of outstanding requests
		// we have to this peer. This is initialized to the settings
		// in the session_settings structure. But it may be lowered
		// if the peer is known to require a smaller limit (like BitComet).
		// or if the extended handshake sets a limit.
		// web seeds also has a limit on the queue size.
		int m_max_out_request_queue;

		void set_timeout(int s) { m_timeout = s; }

#ifndef TORRENT_DISABLE_EXTENSIONS
		typedef std::list<boost::shared_ptr<peer_plugin> > extension_list_t;
		extension_list_t m_extensions;
#endif

#ifndef TORRENT_DISABLE_RESOLVE_COUNTRIES	
		// in case the session settings is set
		// to resolve countries, this is set to
		// the two character country code this
		// peer resides in.
		char m_country[2];
#endif

#ifndef NDEBUG
		boost::intrusive_ptr<peer_connection> self()
		{
			TORRENT_ASSERT(!m_in_constructor);
			return intrusive_ptr_base<peer_connection>::self();
		}
#endif

	private:

		void fill_send_buffer();
		void on_disk_read_complete(int ret, disk_io_job const& j, peer_request r);
		void on_disk_write_complete(int ret, disk_io_job const& j
			, peer_request r, boost::shared_ptr<torrent> t);

		// the time when we last got a part of a
		// piece packet from this peer
		ptime m_last_piece;
		// the time we sent a request to
		// this peer the last time
		ptime m_last_request;
		// the time we received the last
		// piece request from the peer
		ptime m_last_incoming_request;
		// the time when we unchoked this peer
		ptime m_last_unchoke;

		// timeouts
		ptime m_last_receive;
		ptime m_last_sent;

		// the time when the first entry in the
		// request queue was requested, increased
		// for each entry that is popped from the
		// download queue. Used for request timeout
		ptime m_requested;

		// if the timeout is extended for the outstanding
		// requests, this is the number of seconds it was
		// extended.
		int m_timeout_extend;

		// a timestamp when the remote download rate
		// was last updated
		ptime m_remote_dl_update;

		// the time when async_connect was called
		// or when the incoming connection was established
		ptime m_connect;

		// the time when this peer sent us a not_interested message
		// the last time.
		ptime m_became_uninterested;

		// the time when we sent a not_interested message to
		// this peer the last time.
		ptime m_became_uninteresting;

		// the amount of data this peer has been given
		// as free upload. This is distributed from
		// peers from which we get free download
		// this will be negative on a peer from which
		// we get free download, and positive on peers
		// that we give the free upload, to keep the balance.
		size_type m_free_upload;

		// the total payload download bytes
		// at the last unchoke cycle. This is used to
		// measure the number of bytes transferred during
		// an unchoke cycle, to unchoke peers the more bytes
		// they sent us
		size_type m_downloaded_at_last_unchoke;

#ifndef TORRENT_DISABLE_GEO_IP
		std::string m_inet_as_name;
#endif

		buffer m_recv_buffer;

		// if this peer is receiving a piece, this
		// points to a disk buffer that the data is
		// read into. This eliminates a memcopy from
		// the receive buffer into the disk buffer
		disk_buffer_holder m_disk_recv_buffer;

		chained_buffer m_send_buffer;

		boost::shared_ptr<socket_type> m_socket;
		// this is the peer we're actually talking to
		// it may not necessarily be the peer we're
		// connected to, in case we use a proxy
		tcp::endpoint m_remote;
		
		// this is the torrent this connection is
		// associated with. If the connection is an
		// incoming conncetion, this is set to zero
		// until the info_hash is received. Then it's
		// set to the torrent it belongs to.
		boost::weak_ptr<torrent> m_torrent;

		// remote peer's id
		peer_id m_peer_id;

		// the pieces the other end have
		bitfield m_have_piece;

		// the queue of requests we have got
		// from this peer
		std::deque<peer_request> m_requests;

		// the blocks we have reserved in the piece
		// picker and will request from this peer.
		std::deque<piece_block> m_request_queue;
		
		// the queue of blocks we have requested
		// from this peer
		std::deque<pending_block> m_download_queue;
		
		// the pieces we will send to the peer
		// if requested (regardless of choke state)
		std::set<int> m_accept_fast;

		// the pieces the peer will send us if
		// requested (regardless of choke state)
		std::vector<int> m_allowed_fast;

		// pieces that has been suggested to be
		// downloaded from this peer
		std::vector<int> m_suggested_pieces;

		// a list of byte offsets inside the send buffer
		// the piece requests
		std::vector<int> m_requests_in_buffer;

		// the number of pieces this peer
		// has. Must be the same as
		// std::count(m_have_piece.begin(),
		// m_have_piece.end(), true)
		int m_num_pieces;

		// the timeout in seconds
		int m_timeout;

		// the size (in bytes) of the bittorrent message
		// we're currently receiving
		int m_packet_size;

		// the number of bytes of the bittorrent payload
		// we've received so far
		int m_recv_pos;

		int m_disk_recv_buffer_size;

		// the number of bytes we are currently reading
		// from disk, that will be added to the send
		// buffer as soon as they complete
		int m_reading_bytes;
		
		// the number of invalid piece-requests
		// we have got from this peer. If the request
		// queue gets empty, and there have been
		// invalid requests, we can assume the
		// peer is waiting for those pieces.
		// we can then clear its download queue
		// by sending choke, unchoke.
		int m_num_invalid_requests;

		// this is the priority with which this peer gets
		// download bandwidth quota assigned to it.
		int m_priority;

		int m_upload_limit;
		int m_download_limit;

		// this peer's peer info struct. This may
		// be 0, in case the connection is incoming
		// and hasn't been added to a torrent yet.
		policy::peer* m_peer_info;

		// this is a measurement of how fast the peer
		// it allows some variance without changing
		// back and forth between states
		peer_speed_t m_speed;

		// the ticket id from the connection queue.
		// This is used to identify the connection
		// so that it can be removed from the queue
		// once the connection completes
		int m_connection_ticket;
		
		// bytes downloaded since last second
		// timer timeout; used for determining 
		// approx download rate
		int m_remote_bytes_dled;

		// approximate peer download rate
		int m_remote_dl_rate;

		// the number of bytes send to the disk-io
		// thread that hasn't yet been completely written.
		int m_outstanding_writing_bytes;

		// max transfer rates seen on this peer
		int m_download_rate_peak;
		int m_upload_rate_peak;

		// estimated round trip time to this peer
		// based on the time from when async_connect
		// was called to when on_connection_complete
		// was called. The rtt is specified in milliseconds
		boost::uint16_t m_rtt;

		// if set to non-zero, this peer will always prefer
		// to request entire n pieces, rather than blocks.
		// where n is the value of this variable.
		// if it is 0, the download rate limit setting
		// will be used to determine if whole pieces
		// are preferred.
		boost::uint8_t m_prefer_whole_pieces;
		
		// the number of request we should queue up
		// at the remote end.
		boost::uint8_t m_desired_queue_size;

		// if this is true, the disconnection
		// timestamp is not updated when the connection
		// is closed. This means the time until we can
		// reconnect to this peer is shorter, and likely
		// immediate.
		bool m_fast_reconnect:1;
		
		// is true if it was we that connected to the peer
		// and false if we got an incoming connection
		// could be considered: true = local, false = remote
		bool m_active:1;

		// other side says that it's interested in downloading
		// from us.
		bool m_peer_interested:1;

		// the other side has told us that it won't send anymore
		// data to us for a while
		bool m_peer_choked:1;

		// the peer has pieces we are interested in
		bool m_interesting:1;

		// we have choked the upload to the peer
		bool m_choked:1;

		// this is set to true if the connection timed
		// out or closed the connection. In that
		// case we will not try to reconnect to
		// this peer
		bool m_failed:1;

		// if this is set to true, the peer will not
		// request bandwidth from the limiter, but instead
		// just send and receive as much as possible.
		bool m_ignore_bandwidth_limits:1;

		// this is set to true when a have_all
		// message is received. This information
		// is used to fill the bitmask in init()
		bool m_have_all:1;

		// this is true if this connection has been added
		// to the list of connections that will be closed.
		bool m_disconnecting:1;

		// this is true until this socket has become
		// writable for the first time (i.e. the
		// connection completed). While connecting
		// the timeout will not be triggered. This is
		// because windows XP SP2 may delay connection
		// attempts, which means that the connection
		// may not even have been attempted when the
		// time out is reached.
		bool m_connecting:1;

		// This is true until connect is called on the
		// peer_connection's socket. It is false on incoming
		// connections.
		bool m_queued:1;

		// if this is true, the blocks picked by the piece
		// picker will be merged before passed to the
		// request function. i.e. subsequent blocks are
		// merged into larger blocks. This is used by
		// the http-downloader, to request whole pieces
		// at a time.
		bool m_request_large_blocks:1;
		
		// set to true when this peer is only uploading
		bool m_upload_only:1;

		// set to true when a piece request times out. The
		// result is that the desired pending queue size
		// is set to 1
		bool m_snubbed:1;

		// this is set to true once the bitfield is received
		bool m_bitfield_received:1;

#ifndef NDEBUG
	public:
		bool m_in_constructor:1;
		bool m_disconnect_started:1;
#endif
	};
}

#endif // TORRENT_PEER_CONNECTION_HPP_INCLUDED

