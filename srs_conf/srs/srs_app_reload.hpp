//
// Copyright (c) 2013-2023 The SRS Authors
//
// SPDX-License-Identifier: MIT or MulanPSL-2.0
//

#ifndef SRS_APP_RELOAD_HPP
#define SRS_APP_RELOAD_HPP

#include "srs_kernel_error.hpp"

#include <string>

// The handler for config reload.用于配置重新加载的处理程序。
// When reload callback, the config is updated yet.当重新加载回调时，配置尚未更新。
//
// Features not support reload,功能不支持重新加载，
// @see: https://ossrs.net/lts/zh-cn/docs/v4/doc/reload#notsupportedfeatures
class ISrsReloadHandler
{
public:
    ISrsReloadHandler();
    virtual ~ISrsReloadHandler();
public:
    virtual srs_error_t on_reload_max_conns();
    virtual srs_error_t on_reload_listen();
    virtual srs_error_t on_reload_pithy_print();
    virtual srs_error_t on_reload_http_api_raw_api();
    virtual srs_error_t on_reload_rtc_server();
    virtual srs_error_t on_reload_testwork();
public:
    virtual srs_error_t on_reload_vhost_added(std::string vhost);
    virtual srs_error_t on_reload_vhost_removed(std::string vhost);
    virtual srs_error_t on_reload_vhost_play(std::string vhost);
    virtual srs_error_t on_reload_vhost_forward(std::string vhost);
    virtual srs_error_t on_reload_vhost_dash(std::string vhost);
    virtual srs_error_t on_reload_vhost_hls(std::string vhost);
    virtual srs_error_t on_reload_vhost_hds(std::string vhost);
    virtual srs_error_t on_reload_vhost_dvr(std::string vhost);
    virtual srs_error_t on_reload_vhost_publish(std::string vhost);
    virtual srs_error_t on_reload_vhost_tcp_nodelay(std::string vhost);
    virtual srs_error_t on_reload_vhost_realtime(std::string vhost);
    virtual srs_error_t on_reload_vhost_chunk_size(std::string vhost);
    virtual srs_error_t on_reload_vhost_transcode(std::string vhost);
    virtual srs_error_t on_reload_vhost_exec(std::string vhost);
    virtual srs_error_t on_reload_ingest_removed(std::string vhost, std::string ingest_id);
    virtual srs_error_t on_reload_ingest_added(std::string vhost, std::string ingest_id);
    virtual srs_error_t on_reload_ingest_updated(std::string vhost, std::string ingest_id);
    virtual srs_error_t on_reload_user_info();
};

#endif
