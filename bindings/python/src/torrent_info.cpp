// Copyright Daniel Wallin 2006. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/python.hpp>
#include <libtorrent/torrent_info.hpp>
#include "libtorrent/intrusive_ptr_base.hpp"

using namespace boost::python;
using namespace libtorrent;

namespace
{

  std::vector<announce_entry>::const_iterator begin_trackers(torrent_info& i)
  {
      return i.trackers().begin();
  }

  std::vector<announce_entry>::const_iterator end_trackers(torrent_info& i)
  {
      return i.trackers().end();
  }

  void add_node(torrent_info& ti, char const* hostname, int port)
  {
      ti.add_node(std::make_pair(hostname, port));
  }

  list nodes(torrent_info const& ti)
  {
      list result;

      typedef std::vector<std::pair<std::string, int> > list_type;

      for (list_type::const_iterator i = ti.nodes().begin(); i != ti.nodes().end(); ++i)
      {
          result.append(make_tuple(i->first, i->second));
      }

      return result;
  }

  file_storage::iterator begin_files(torrent_info& i)
  {
      return i.begin_files();
  }

  file_storage::iterator end_files(torrent_info& i)
  {
      return i.end_files();
  }

  //list files(torrent_info const& ti, bool storage) {
  list files(torrent_info const& ti, bool storage) {
      list result;

      typedef std::vector<file_entry> list_type;

      for (list_type::const_iterator i = ti.begin_files(); i != ti.end_files(); ++i)
          result.append(*i);

      return result;
  }


} // namespace unnamed

void bind_torrent_info()
{
    return_value_policy<copy_const_reference> copy;

    class_<torrent_info, boost::intrusive_ptr<torrent_info> >("torrent_info", no_init)
        .def(init<entry const&>())
        .def(init<sha1_hash const&>())
        .def(init<char const*, int>())
        .def(init<char const*>())
        
        .def("add_tracker", &torrent_info::add_tracker, (arg("url"), arg("tier")=0))
        .def("add_url_seed", &torrent_info::add_url_seed)

        .def("name", &torrent_info::name, copy)
        .def("comment", &torrent_info::comment, copy)
        .def("creator", &torrent_info::creator, copy)
        .def("total_size", &torrent_info::total_size)
        .def("piece_length", &torrent_info::piece_length)
        .def("num_pieces", &torrent_info::num_pieces)
        .def("info_hash", &torrent_info::info_hash, copy)

        .def("hash_for_piece", &torrent_info::hash_for_piece)
        .def("piece_size", &torrent_info::piece_size)

        .def("num_files", &torrent_info::num_files, (arg("storage")=false))
        .def("file_at", &torrent_info::file_at, return_internal_reference<>())
        .def("files", &files, (arg("storage")=false))

        .def("priv", &torrent_info::priv)
        .def("trackers", range(begin_trackers, end_trackers))

        .def("creation_date", &torrent_info::creation_date)

        .def("add_node", &add_node)
        .def("nodes", &nodes)
        ;

    class_<file_entry>("file_entry")
       .add_property(
            "path"
          , make_getter(
                &file_entry::path, return_value_policy<copy_non_const_reference>()
            )
        )
        .def_readonly("offset", &file_entry::offset)
        .def_readonly("size", &file_entry::size)
        ;

    class_<announce_entry>("announce_entry", init<std::string const&>())
        .def_readwrite("url", &announce_entry::url)
        .def_readwrite("tier", &announce_entry::tier)
        ;
}


