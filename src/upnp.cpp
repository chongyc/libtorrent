/*

Copyright (c) 2007, Arvid Norberg
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

#include "libtorrent/pch.hpp"

#include "libtorrent/upnp.hpp"
#include "libtorrent/io.hpp"
#include "libtorrent/http_tracker_connection.hpp"
#include "libtorrent/xml_parse.hpp"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <asio/ip/host_name.hpp>
#include <asio/ip/multicast.hpp>
#include <cstdlib>

using boost::bind;
using namespace libtorrent;
using boost::posix_time::microsec_clock;
using boost::posix_time::milliseconds;
using boost::posix_time::seconds;

enum { num_mappings = 2 };

// UPnP multicast address and port
address_v4 multicast_address = address_v4::from_string("239.255.255.250");
udp::endpoint multicast_endpoint(multicast_address, 1900);

upnp::upnp(io_service& ios, address const& listen_interface
	, std::string const& user_agent, portmap_callback_t const& cb)
	: m_user_agent(user_agent)
	, m_callback(cb)
	, m_retry_count(0)
	, m_socket(ios)
	, m_broadcast_timer(ios)
	, m_refresh_timer(ios)
	, m_disabled(false)
{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	m_log.open("upnp.log", std::ios::in | std::ios::out | std::ios::trunc);
#endif
	rebind(listen_interface);
}

void upnp::rebind(address const& listen_interface)
{
	if (listen_interface.is_v4() && listen_interface != address_v4::from_string("0.0.0.0"))
	{
		m_local_ip = listen_interface.to_v4();
	}
	else
	{
		// make a best guess of the interface we're using and its IP
		udp::resolver r(m_socket.io_service());
		udp::resolver::iterator i = r.resolve(udp::resolver::query(asio::ip::host_name(), "0"));
		for (;i != udp::resolver_iterator(); ++i)
		{
			if (i->endpoint().address().is_v4()) break;
		}

		if (i == udp::resolver_iterator())
		{
	#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
			m_log << "local host name did not resolve to an IPv4 address. "
				"disabling UPnP" << std::endl;
	#endif
			m_disabled = true;
			return;
		}

		m_local_ip = i->endpoint().address().to_v4();
	}

#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	m_log << to_simple_string(microsec_clock::universal_time())
		<< " local ip: " << m_local_ip.to_string() << std::endl;
#endif

	if ((m_local_ip.to_ulong() & 0xff000000) != 0x0a000000
		&& (m_local_ip.to_ulong() & 0xfff00000) != 0xac100000
		&& (m_local_ip.to_ulong() & 0xffff0000) != 0xc0a80000)
	{
		// the local address seems to be an external
		// internet address. Assume it is not behind a NAT
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << "not on a NAT. disabling UPnP" << std::endl;
#endif
		m_disabled = true;
		return;
	}


	try
	{
		using namespace asio::ip::multicast;

		m_socket.open(udp::v4());
		m_socket.set_option(datagram_socket::reuse_address(true));
		m_socket.bind(udp::endpoint(m_local_ip, 0));

		m_socket.set_option(join_group(multicast_address));
		m_socket.set_option(outbound_interface(m_local_ip));

	}
	catch (std::exception&)
	{
		m_disabled = true;
		return;
	}
	m_disabled = false;

	m_retry_count = 0;
	discover_device();
}

void upnp::discover_device()
{
	const char msearch[] = 
		"M-SEARCH * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"ST:upnp:rootdevice\r\n"
		"MAN:\"ssdp:discover\"\r\n"
		"MX:3\r\n"
		"\r\n\r\n";

	m_socket.async_receive_from(asio::buffer(m_receive_buffer
		, sizeof(m_receive_buffer)), m_remote, bind(&upnp::on_reply, this, _1, _2));
	m_socket.send_to(asio::buffer(msearch, sizeof(msearch) - 1)
		, multicast_endpoint);

	++m_retry_count;
	m_broadcast_timer.expires_from_now(milliseconds(250 * m_retry_count));
	m_broadcast_timer.async_wait(bind(&upnp::resend_request, this, _1));

#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " ==> Broadcasting search for rootdevice" << std::endl;
#endif
}

void upnp::set_mappings(int tcp, int udp)
{
	if (m_disabled) return;
/*
	update_mapping(0, tcp);
	update_mapping(1, udp);
*/
}

void upnp::resend_request(asio::error_code const& e)
{
	using boost::posix_time::hours;
	if (e) return;
	if (m_retry_count >= 9)
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " *** Got no response in 9 retries. Giving up, "
			"disabling UPnP." << std::endl;
#endif
		// try again in two hours
		m_disabled = true;
		return;
	}
	discover_device();
}

void upnp::on_reply(asio::error_code const& e
	, std::size_t bytes_transferred)
{
	using namespace libtorrent::detail;
	using boost::posix_time::seconds;
	if (e) return;

	// since we're using udp, send the query 4 times
	// just to make sure we find all devices
	if (m_retry_count >= 4)
		m_broadcast_timer.cancel();

	// parse out the url for the device

/*
	the response looks like this:

	HTTP/1.1 200 OK
	ST:upnp:rootdevice
	USN:uuid:000f-66d6-7296000099dc::upnp:rootdevice
	Location: http://192.168.1.1:5431/dyndev/uuid:000f-66d6-7296000099dc
	Server: Custom/1.0 UPnP/1.0 Proc/Ver
	EXT:
	Cache-Control:max-age=180
	DATE: Fri, 02 Jan 1970 08:10:38 GMT
*/
	http_parser p;
	try
	{
		p.incoming(buffer::const_interval(m_receive_buffer
			, m_receive_buffer + bytes_transferred));
	}
	catch (std::exception& e)
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== Rootdevice responded with incorrect HTTP packet: "
			<< e.what() << ". Ignoring device" << std::endl;
#endif
		return;
	}

	if (p.status_code() != 200)
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== Rootdevice responded with HTTP status: " << p.status_code()
			<< ". Ignoring device" << std::endl;
#endif
		return;
	}

	if (!p.header_finished())
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== Rootdevice responded with incomplete HTTP "
			"packet. Ignoring device" << std::endl;
#endif
		return;
	}

	std::string url = p.header<std::string>("location");
	if (url.empty())
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== Rootdevice response is missing a location header. "
			"Ignoring device" << std::endl;
#endif
		return;
	}

	rootdevice d;
	d.url = url;
	
	std::set<rootdevice>::iterator i = m_devices.find(d);

	if (i == m_devices.end())
	{

		std::string protocol;
		// we don't have this device in our list. Add it
		boost::tie(protocol, d.hostname, d.port, d.path)
			= parse_url_components(d.url);

		if (protocol != "http")
		{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
			m_log << to_simple_string(microsec_clock::universal_time())
				<< " <== Rootdevice uses unsupported protocol: '" << protocol
				<< "'. Ignoring device" << std::endl;
#endif
			return;
		}

		if (d.port == 0)
		{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
			m_log << to_simple_string(microsec_clock::universal_time())
				<< " <== Rootdevice responded with a url with port 0. "
				"Ignoring device" << std::endl;
#endif
			return;
		}
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== Found rootdevice: " << d.url << std::endl;
#endif

		boost::tie(i, boost::tuples::ignore) = m_devices.insert(d);
	}

	if (i->control_url.empty())
	{
		// we don't have a WANIP or WANPPP url for this device,
		// ask for it
		rootdevice& d = const_cast<rootdevice&>(*i);
		d.upnp_connection.reset(new http_connection(m_socket.io_service()
			, boost::bind(&upnp::on_upnp_xml, this, _1, _2, boost::ref(d))));
		d.upnp_connection->get(d.url);
	}
	else if (!i->ports_mapped)
	{
	// TODO: initiate a port map operation if any is pending
	}
	
}

void upnp::post(rootdevice& d, std::stringstream const& soap
	, std::string const& soap_action)
{
	std::stringstream header;
	
	header << "POST " << d.control_url << " HTTP/1.1\r\n"
		"Host: " << d.hostname << ":" << d.port << "\r\n"
		"Content-Type: text/xml; charset=\"utf-8\"\r\n"
		"Content-Length: " << soap.str().size() << "\r\n"
		"Soapaction: \"" << d.service_namespace << "#" << soap_action << "\"\r\n\r\n" << soap.str();

	d.upnp_connection->sendbuffer = header.str();
	d.upnp_connection->start(d.hostname, boost::lexical_cast<std::string>(d.port)
		, seconds(10));
}

void upnp::map_port(rootdevice& d, int i)
{
	d.upnp_connection.reset(new http_connection(m_socket.io_service()
		, boost::bind(&upnp::on_upnp_map_response, this, _1, _2
		, boost::ref(d))));

	std::string soap_action = "AddPortMapping";

	std::stringstream soap;
	
	soap << "<?xml version=\"1.0\"?>\n"
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body><u:" << soap_action << " xmlns:u=\"" << d.service_namespace << "\">";

	soap << "<NewRemoteHost></NewRemoteHost>"
		"<NewExternalPort>" << d.mapping[i].external_port << "</NewExternalPort>"
		"<NewProtocol>" << (d.mapping[i].protocol ? "UDP" : "TCP") << "</NewProtocol>"
		"<NewInternalPort>" << d.mapping[i].local_port << "</NewInternalPort>"
		"<NewInternalClient>" << m_local_ip.to_string() << "</NewInternalClient>"
		"<NewEnabled>1</NewEnabled>"
		"<NewPortMappingDescription>" << m_user_agent << "</NewPortMappingDescription>"
		"<NewLeaseDuration>" << d.lease_duration << "</NewLeaseDuration>";
	soap << "</u:" << soap_action << "></s:Body></s:Envelope>";

	post(d, soap, soap_action);
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	m_log << to_simple_string(microsec_clock::universal_time())
		<< " ==> AddPortMapping: " << soap.str() << std::endl;
#endif
	
}

namespace
{
	struct parse_state
	{
		parse_state(): found_service(false), exit(false) {}
		void reset(char const* st)
		{
			found_service = false;
			exit = false;
			service_type = st;
		}
		bool found_service;
		bool exit;
		std::string top_tag;
		std::string control_url;
		char const* service_type;
	};
	
	void find_control_url(int type, char const* string, parse_state& state)
	{
		if (state.exit) return;

		if (type == xml_start_tag)
		{
			if ((!state.top_tag.empty() && state.top_tag == "service")
				|| !strcmp(string, "service"))
			{
				state.top_tag = string;
			}
		}
		else if (type == xml_end_tag)
		{
			if (!strcmp(string, "service"))
			{
				state.top_tag.clear();
				if (state.found_service) state.exit = true;
			}
			else if (!state.top_tag.empty() && state.top_tag != "service")
				state.top_tag = "service";
		}
		else if (type == xml_string)
		{
			if (state.top_tag == "serviceType")
			{
				if (!strcmp(string, state.service_type))
					state.found_service = true;
			}
			else if (state.top_tag == "controlURL")
			{
				state.control_url = string;
				if (state.found_service) state.exit = true;
			}
		}
	}

}

void upnp::on_upnp_xml(asio::error_code const& e
	, libtorrent::http_parser const& p, rootdevice& d)
{
	parse_state s;
	s.reset("urn:schemas-upnp-org:service:WANIPConnection:1");
	xml_parse((char*)p.get_body().begin, (char*)p.get_body().end
		, boost::bind(&find_control_url, _1, _2, boost::ref(s)));
	d.service_namespace = "urn:schemas-upnp-org:service:WANIPConnection:1";
	if (!s.found_service)
	{
		// we didn't find the WAN IP connection, look for
		// a PPP IP connection
		s.reset("urn:schemas-upnp-org:service:PPPIPConnection:1");
		xml_parse((char*)p.get_body().begin, (char*)p.get_body().end
			, boost::bind(&find_control_url, _1, _2, boost::ref(s)));
		d.service_namespace = "urn:schemas-upnp-org:service:WANPPPConnection:1";
	}
	
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	m_log << to_simple_string(microsec_clock::universal_time())
		<< " <== Rootdevice response, found control URL: " << s.control_url << std::endl;
#endif

	d.control_url = s.control_url;
	d.upnp_connection.reset();

	map_port(d, 0);
}

namespace
{
	struct error_code_parse_state
	{
		error_code_parse_state(): in_error_code(false), exit(false), error_code(-1) {}
		bool in_error_code;
		bool exit;
		int error_code;
	};
	
	void find_error_code(int type, char const* string, error_code_parse_state& state)
	{
		if (state.exit) return;
		if (type == xml_start_tag && !strcmp("errorCode", string))
		{
			state.in_error_code = true;
		}
		else if (type == xml_string && state.in_error_code)
		{
			state.error_code = std::atoi(string);
			state.exit = true;
		}
	}
}

void upnp::on_upnp_map_response(asio::error_code const& e
	, libtorrent::http_parser const& p, rootdevice& d)
{
	if (e)
	{
#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	m_log << to_simple_string(microsec_clock::universal_time())
		<< " <== error while adding portmap: " << e << std::endl;
#endif
		return;
	}
	
//	 error code response may look like this:
//	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
//		s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
//	 <s:Body>
//	  <s:Fault>
//		<faultcode>s:Client</faultcode>
//		<faultstring>UPnPError</faultstring>
//		<detail>
//		 <UPnPErrorxmlns="urn:schemas-upnp-org:control-1-0">
//		  <errorCode>402</errorCode>
//		  <errorDescription>Invalid Args</errorDescription>
//		 </UPnPError>
//		</detail>
//	  </s:Fault>
//	 </s:Body>
//	</s:Envelope>

	
	error_code_parse_state s;
	xml_parse((char*)p.get_body().begin, (char*)p.get_body().end
		, bind(&find_error_code, _1, _2, boost::ref(s)));

#if defined(TORRENT_LOGGING) || defined(TORRENT_VERBOSE_LOGGING)
	if (s.error_code != -1)
	{
		m_log << to_simple_string(microsec_clock::universal_time())
			<< " <== got error message: " << s.error_code << std::endl;
	}
#endif
	
	if (s.error_code == 725)
	{
		d.lease_duration = 0;
		map_port(d, 0);
		return;
	}
	
	std::cerr << std::string(p.get_body().begin, p.get_body().end) << std::endl;
}

void upnp::close()
{
	if (m_disabled) return;
	m_socket.close();
	std::for_each(m_devices.begin(), m_devices.end()
		, bind(&rootdevice::close, _1));
}
