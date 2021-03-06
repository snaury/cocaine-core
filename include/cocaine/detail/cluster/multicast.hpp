/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_MULTICAST_CLUSTER_HPP
#define COCAINE_MULTICAST_CLUSTER_HPP

#include "cocaine/api/cluster.hpp"

#include <boost/asio/deadline_timer.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace cocaine { namespace cluster {

class multicast_cfg_t
{
public:
    // An UDP endpoint to bind for multicast node announces. Not a multicast group.
    boost::asio::ip::udp::endpoint endpoint;

    // Will announce local endpoints to the specified multicast group every `interval` seconds.
    boost::asio::deadline_timer::duration_type interval;
};

class multicast_t:
    public api::cluster_t
{
    struct announce_t;

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    // Interoperability with the locator service.
    interface& m_locator;

    // Component config.
    const multicast_cfg_t m_cfg;

    boost::asio::ip::udp::socket m_socket;
    boost::asio::deadline_timer m_timer;

    // Announce expiration timeouts.
    std::map<std::string, std::unique_ptr<boost::asio::deadline_timer>> m_expirations;

public:
    multicast_t(context_t& context, interface& locator, const std::string& name, const dynamic_t& args);

    virtual
   ~multicast_t();

private:
    void
    on_publish(const boost::system::error_code& ec);

    void
    on_receive(const boost::system::error_code& ec, size_t bytes_received,
               const std::shared_ptr<announce_t>& ptr);

    void
    on_expired(const boost::system::error_code& ec, const std::string& uuid);
};

}} // namespace cocaine::cluster

#endif
