/**
@file
@author from CrypoNote (see copyright below; Andrey N. Sabelnikov)
@monero rfree
@brief the connection templated-class for one peer connection
*/
// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 



#ifndef _ABSTRACT_TCP_SERVER2_H_ 
#define _ABSTRACT_TCP_SERVER2_H_ 


#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <atomic>
#include <cassert>
#include <map>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/thread/thread.hpp>
#include "net_utils_base.h"
#include "syncobj.h"
#include "connection_basic.hpp"
#include "network_throttle-detail.hpp"

#undef GUUS_DEFAULT_LOG_CATEGORY
#define GUUS_DEFAULT_LOG_CATEGORY "net"

#define ABSTRACT_SERVER_SEND_QUE_MAX_COUNT 1000

namespace epee
{
namespace net_utils
{

  struct i_connection_filter
  {
    virtual bool is_remote_host_allowed(const epee::net_utils::network_address &address, time_t *t = NULL)=0;
  protected:
    virtual ~i_connection_filter(){}
  };
  

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  /// Represents a single connection from a client.
  template<class t_protocol_handler>
  class connection
    : public boost::enable_shared_from_this<connection<t_protocol_handler> >,
    private boost::noncopyable, 
    public i_service_endpoint,
    public connection_basic
  {
  public:
    typedef typename t_protocol_handler::connection_context t_connection_context;

    struct shared_state : connection_basic_shared_state
    {
      shared_state()
        : connection_basic_shared_state(), pfilter(nullptr), config(), stop_signal_sent(false)
      {}

      i_connection_filter* pfilter;
      typename t_protocol_handler::config_type config;
      bool stop_signal_sent;
    };

    /// Construct a connection with the given io_service.
    explicit connection( boost::asio::io_service& io_service,
                        boost::shared_ptr<shared_state> state,
			t_connection_type connection_type,
			epee::net_utils::ssl_support_t ssl_support);

    explicit connection( boost::asio::ip::tcp::socket&& sock,
			 boost::shared_ptr<shared_state> state,
			t_connection_type connection_type,
			epee::net_utils::ssl_support_t ssl_support);



    virtual ~connection() noexcept(false);

    /// Start the first asynchronous operation for the connection.
    bool start(bool is_income, bool is_multithreaded);

    // `real_remote` is the actual endpoint (if connection is to proxy, etc.)
    bool start(bool is_income, bool is_multithreaded, network_address real_remote);

    void get_context(t_connection_context& context_){context_ = context;}

    void call_back_starter();
    
    void save_dbg_log();


		bool speed_limit_is_enabled() const; ///< tells us should we be sleeping here (e.g. do not sleep on RPC connections)

    bool cancel();
    
  private:
    //----------------- i_service_endpoint ---------------------
    virtual bool do_send(const void* ptr, size_t cb); ///< (see do_send from i_service_endpoint)
    virtual bool do_send_chunk(const void* ptr, size_t cb); ///< will send (or queue) a part of data
    virtual bool send_done();
    virtual bool close();
    virtual bool call_run_once_service_io();
    virtual bool request_callback();
    virtual boost::asio::io_service& get_io_service();
    virtual bool add_ref();
    virtual bool release();
    //------------------------------------------------------
    boost::shared_ptr<connection<t_protocol_handler> > safe_shared_from_this();
    bool shutdown();
    /// Handle completion of a receive operation.
    void handle_receive(const boost::system::error_code& e,
      std::size_t bytes_transferred);

    /// Handle completion of a read operation.
    void handle_read(const boost::system::error_code& e,
      std::size_t bytes_transferred);

    /// Handle completion of a write operation.
    void handle_write(const boost::system::error_code& e, size_t cb);

    /// reset connection timeout timer and callback
    void reset_timer(boost::posix_time::milliseconds ms, bool add);
    boost::posix_time::milliseconds get_default_timeout();
    boost::posix_time::milliseconds get_timeout_from_bytes_read(size_t bytes);

    /// host connection count tracking
    unsigned int host_count(const std::string &host, int delta = 0);

    /// Buffer for incoming data.
    boost::array<char, 8192> buffer_;
    size_t buffer_ssl_init_fill;

    t_connection_context context;

	// TODO what do they mean about wait on destructor?? --rfree :
    //this should be the last one, because it could be wait on destructor, while other activities possible on other threads
    t_protocol_handler m_protocol_handler;
    //typename t_protocol_handler::config_type m_dummy_config;
    size_t m_reference_count = 0; // reference count managed through add_ref/release support
    boost::shared_ptr<connection<t_protocol_handler> > m_self_ref; // the reference to hold
    critical_section m_self_refs_lock;
    critical_section m_chunking_lock; // held while we add small chunks of the big do_send() to small do_send_chunk()
    critical_section m_shutdown_lock; // held while shutting down
    
    t_connection_type m_connection_type;
    
    // for calculate speed (last 60 sec)
    network_throttle m_throttle_speed_in;
    network_throttle m_throttle_speed_out;
    boost::mutex m_throttle_speed_in_mutex;
    boost::mutex m_throttle_speed_out_mutex;

    boost::asio::deadline_timer m_timer;
    bool m_local;
    bool m_ready_to_close;
    std::string m_host;

	public:
			void setRpcStation();
  };


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  template<class t_protocol_handler>
  class boosted_tcp_server
    : private boost::noncopyable
  {
    enum try_connect_result_t
    {
      CONNECT_SUCCESS,
      CONNECT_FAILURE,
      CONNECT_NO_SSL,
    };

  public:
    typedef boost::shared_ptr<connection<t_protocol_handler> > connection_ptr;
    typedef typename t_protocol_handler::connection_context t_connection_context;
    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.

    boosted_tcp_server(t_connection_type connection_type);
    explicit boosted_tcp_server(boost::asio::io_service& external_io_service, t_connection_type connection_type);
    ~boosted_tcp_server();
    
    std::map<std::string, t_connection_type> server_type_map;
    void create_server_type_map();

    bool init_server(uint32_t port, const std::string& address = "0.0.0.0",
	uint32_t port_ipv6 = 0, const std::string& address_ipv6 = "::", bool use_ipv6 = false, bool require_ipv4 = true,
	ssl_options_t ssl_options = ssl_support_t::e_ssl_support_autodetect);
    bool init_server(const std::string port,  const std::string& address = "0.0.0.0",
	const std::string port_ipv6 = "", const std::string address_ipv6 = "::", bool use_ipv6 = false, bool require_ipv4 = true,
	ssl_options_t ssl_options = ssl_support_t::e_ssl_support_autodetect);

    /// Run the server's io_service loop.
    bool run_server(size_t threads_count, bool wait = true, const boost::thread::attributes& attrs = boost::thread::attributes());

    /// wait for service workers stop
    bool timed_wait_server_stop(uint64_t wait_mseconds);

    /// Stop the server.
    void send_stop_signal();

    bool is_stop_signal_sent() const noexcept { return m_stop_signal_sent; };

    const std::atomic<bool>& get_stop_signal() const noexcept { return m_stop_signal_sent; }

    void set_threads_prefix(const std::string& prefix_name);

    bool deinit_server(){return true;}

    size_t get_threads_count(){return m_threads_count;}

    void set_connection_filter(i_connection_filter* pfilter);

    void set_default_remote(epee::net_utils::network_address remote)
    {
      default_remote = std::move(remote);
    }

    bool add_connection(t_connection_context& out, boost::asio::ip::tcp::socket&& sock, network_address real_remote, epee::net_utils::ssl_support_t ssl_support = epee::net_utils::ssl_support_t::e_ssl_support_autodetect);
    try_connect_result_t try_connect(connection_ptr new_connection_l, const std::string& adr, const std::string& port, boost::asio::ip::tcp::socket &sock_, const boost::asio::ip::tcp::endpoint &remote_endpoint, const std::string &bind_ip, uint32_t conn_timeout, epee::net_utils::ssl_support_t ssl_support);
    bool connect(const std::string& adr, const std::string& port, uint32_t conn_timeot, t_connection_context& cn, const std::string& bind_ip = "0.0.0.0", epee::net_utils::ssl_support_t ssl_support = epee::net_utils::ssl_support_t::e_ssl_support_autodetect);
    template<class t_callback>
    bool connect_async(const std::string& adr, const std::string& port, uint32_t conn_timeot, const t_callback &cb, const std::string& bind_ip = "0.0.0.0", epee::net_utils::ssl_support_t ssl_support = epee::net_utils::ssl_support_t::e_ssl_support_autodetect);

    typename t_protocol_handler::config_type& get_config_object()
    {
      assert(m_state != nullptr); // always set in constructor
      return m_state->config;
    }

    int get_binded_port(){return m_port;}
    int get_binded_port_ipv6(){return m_port_ipv6;}

    long get_connections_count() const
    {
      assert(m_state != nullptr); // always set in constructor
      auto connections_count = m_state->sock_count > 0 ? (m_state->sock_count - 1) : 0; // Socket count minus listening socket
      return connections_count;
    }

    boost::asio::io_service& get_io_service(){return io_service_;}

    struct idle_callback_conext_base
    {
      virtual ~idle_callback_conext_base(){}

      virtual bool call_handler(){return true;}

      idle_callback_conext_base(boost::asio::io_service& io_serice):
                                                          m_timer(io_serice)
      {}
      boost::asio::deadline_timer m_timer;
    };

    template <class t_handler>
    struct idle_callback_conext: public idle_callback_conext_base
    {
      idle_callback_conext(boost::asio::io_service& io_serice, t_handler& h, uint64_t period):
                                                    idle_callback_conext_base(io_serice),
                                                    m_handler(h)
      {this->m_period = period;}

      t_handler m_handler;
      virtual bool call_handler()
      {
        return m_handler();
      }
      uint64_t m_period;
    };

    template<class t_handler>
    bool add_idle_handler(t_handler t_callback, uint64_t timeout_ms)
      {
        boost::shared_ptr<idle_callback_conext<t_handler>> ptr(new idle_callback_conext<t_handler>(io_service_, t_callback, timeout_ms));
        //needed call handler here ?...
        ptr->m_timer.expires_from_now(boost::posix_time::milliseconds(ptr->m_period));
        ptr->m_timer.async_wait(boost::bind(&boosted_tcp_server<t_protocol_handler>::global_timer_handler<t_handler>, this, ptr));
        return true;
      }

    template<class t_handler>
    bool global_timer_handler(/*const boost::system::error_code& err, */boost::shared_ptr<idle_callback_conext<t_handler>> ptr)
    {
      //if handler return false - he don't want to be called anymore
      if(!ptr->call_handler())
        return true;
      ptr->m_timer.expires_from_now(boost::posix_time::milliseconds(ptr->m_period));
      ptr->m_timer.async_wait(boost::bind(&boosted_tcp_server<t_protocol_handler>::global_timer_handler<t_handler>, this, ptr));
      return true;
    }

    template<class t_handler>
    bool async_call(t_handler t_callback)
    {
      io_service_.post(t_callback);
      return true;
    }

  private:
    /// Run the server's io_service loop.
    bool worker_thread();
    /// Handle completion of an asynchronous accept operation.
    void handle_accept_ipv4(const boost::system::error_code& e);
    void handle_accept_ipv6(const boost::system::error_code& e);
    void handle_accept(const boost::system::error_code& e, bool ipv6 = false);

    bool is_thread_worker();

    const boost::shared_ptr<typename connection<t_protocol_handler>::shared_state> m_state;

    /// The io_service used to perform asynchronous operations.
    struct worker
    {
      worker()
        : io_service(), work(io_service)
      {}

      boost::asio::io_service io_service;
      boost::asio::io_service::work work;
    };
    std::unique_ptr<worker> m_io_service_local_instance;
    boost::asio::io_service& io_service_;    

    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::acceptor acceptor_ipv6;
    epee::net_utils::network_address default_remote;

    std::atomic<bool> m_stop_signal_sent;
    uint32_t m_port;
    uint32_t m_port_ipv6;
    std::string m_address;
    std::string m_address_ipv6;
    bool m_use_ipv6;
    bool m_require_ipv4;
    std::string m_thread_name_prefix; //TODO: change to enum server_type, now used
    size_t m_threads_count;
    std::vector<boost::shared_ptr<boost::thread> > m_threads;
    boost::thread::id m_main_thread_id;
    critical_section m_threads_lock;
    volatile uint32_t m_thread_index; // TODO change to std::atomic

    t_connection_type m_connection_type;

    /// The next connection to be accepted
    connection_ptr new_connection_;
    connection_ptr new_connection_ipv6;


    boost::mutex connections_mutex;
    std::set<connection_ptr> connections_;
  }; // class <>boosted_tcp_server


} // namespace
} // namespace

#include "abstract_tcp_server2.inl"

#endif
