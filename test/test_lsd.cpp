#include "libtorrent/session.hpp"
#include "libtorrent/session_settings.hpp"
#include "libtorrent/hasher.hpp"
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/filesystem/operations.hpp>

#include "test.hpp"
#include "setup_transfer.hpp"

using boost::filesystem::remove_all;

void test_swarm()
{
	using namespace libtorrent;

	session ses1(fingerprint("LT", 0, 1, 0, 0), std::make_pair(48000, 49000));
	session ses2(fingerprint("LT", 0, 1, 0, 0), std::make_pair(49000, 50000));
	session ses3(fingerprint("LT", 0, 1, 0, 0), std::make_pair(50000, 51000));

	// this is to avoid everything finish from a single peer
	// immediately. To make the swarm actually connect all
	// three peers before finishing.
	float rate_limit = 90000;
	ses1.set_upload_rate_limit(int(rate_limit));
	ses2.set_download_rate_limit(int(rate_limit));
	ses3.set_download_rate_limit(int(rate_limit));
	ses2.set_upload_rate_limit(int(rate_limit / 2));
	ses3.set_upload_rate_limit(int(rate_limit / 2));

	session_settings settings;
	settings.allow_multiple_connections_per_ip = true;
	ses1.set_settings(settings);
	ses2.set_settings(settings);
	ses3.set_settings(settings);

	ses1.start_lsd();
	ses2.start_lsd();
	ses3.start_lsd();

#ifndef TORRENT_DISABLE_ENCRYPTION
	pe_settings pes;
	pes.out_enc_policy = pe_settings::disabled;
	pes.in_enc_policy = pe_settings::disabled;
	ses1.set_pe_settings(pes);
	ses2.set_pe_settings(pes);
	ses3.set_pe_settings(pes);
#endif

	torrent_handle tor1;
	torrent_handle tor2;
	torrent_handle tor3;

	boost::tie(tor1, tor2, tor3) = setup_transfer(&ses1, &ses2, &ses3, true, false, false);

	for (int i = 0; i < 10; ++i)
	{
		std::auto_ptr<alert> a;
		a = ses1.pop_alert();
		if (a.get())
			std::cerr << "ses1: " << a->msg() << "\n";

		a = ses2.pop_alert();
		if (a.get())
			std::cerr << "ses2: " << a->msg() << "\n";

		a = ses3.pop_alert();
		if (a.get())
			std::cerr << "ses3: " << a->msg() << "\n";

		torrent_status st1 = tor1.status();
		torrent_status st2 = tor2.status();
		torrent_status st3 = tor3.status();

		std::cerr
			<< "\033[33m" << int(st1.upload_payload_rate / 1000.f) << "kB/s "
			<< st1.num_peers << ": "
			<< "\033[32m" << int(st2.download_payload_rate / 1000.f) << "kB/s "
			<< "\033[31m" << int(st2.upload_payload_rate / 1000.f) << "kB/s "
			<< "\033[0m" << int(st2.progress * 100) << "% "
			<< st2.num_peers << " - "
			<< "\033[32m" << int(st3.download_payload_rate / 1000.f) << "kB/s "
			<< "\033[31m" << int(st3.upload_payload_rate / 1000.f) << "kB/s "
			<< "\033[0m" << int(st3.progress * 100) << "% "
			<< st3.num_peers
			<< std::endl;

		if (tor2.is_seed() && tor3.is_seed()) break;
		test_sleep(1000);
	}

	TEST_CHECK(tor2.is_seed());
	TEST_CHECK(tor3.is_seed());

	if (tor2.is_seed() && tor3.is_seed()) std::cerr << "done\n";
}

int test_main()
{
	using namespace libtorrent;
	using namespace boost::filesystem;

	// in case the previous run was terminated
	try { remove_all("./tmp1"); } catch (std::exception&) {}
	try { remove_all("./tmp2"); } catch (std::exception&) {}
	try { remove_all("./tmp3"); } catch (std::exception&) {}

	test_swarm();
	
	remove_all("./tmp1");
	remove_all("./tmp2");
	remove_all("./tmp3");

	return 0;
}


