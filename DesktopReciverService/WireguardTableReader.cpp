//
// Created by Raphael on 2026-11-06.
//

#include "WireguardTableReader.h"

    void WireguardTableReader::stop() {
        waitMutex_.lock();
        running_ = false;
        waitMutex_.unlock();
        wake_.notify_one();
    }

    void WireguardTableReader::refreshLoop(Tunneling& tunnel) {
        while (true) {
            std::unique_lock<std::mutex> lock(waitMutex_);
            wake_.wait_for(lock,std::chrono::hours(1), [this]{
                return !running_;});
            if (!running_) return;
            lock.unlock();
            std::vector<Tunneling::Peer> peers = checkGSheet();
            if (peers.empty())continue;
            try {
                tunnel.updatePeers(peers);
            }catch (...) {
                Logger::get().logEvent(Logger::SevLevel::ERR,"update to peer has failed");
            }
        }
    }

    std::vector<Tunneling::Peer> WireguardTableReader::loadPeers() {
       std::vector<Tunneling::Peer> peers = checkGSheet();
        if (peers.empty()) {
            Logger::get().logEvent(Logger::SevLevel::DEBUG, "unable to reach gsheet, checking local cache");
           peers= readLocalCache();
        }
        return peers;
    }

    std::vector<Tunneling::Peer> WireguardTableReader::checkGSheet() {
        cpr::Response r = cpr::Get(cpr::Url{WIREGUARDTABLE}, cpr::Timeout{5000});
        std::vector<Tunneling::Peer> peers = {};
        if (r.status_code != 200) {
            Logger::get().logEvent(Logger::SevLevel::ERR, "Can't contact wireguard table");
            return peers;
        }

        std::stringstream csv_stream(r.text);
        std::string line;
        while (std::getline(csv_stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            std::stringstream row(line);
            std::string name,key,ip;
            std::getline(row,name, ',');
            std::getline(row,key, ',');
            std::getline(row,ip, ',');
            if (key.empty()|| ip.empty()|| name.empty()) {
                continue;
            }
            try {
                peers.push_back({name,Tunneling::keyFromBase64(key), ip});
                Logger::get().logEvent(Logger::SevLevel::DEBUG, "peer: " + name + " has been added");
            }catch (...) {
                Logger::get().logEvent(Logger::SevLevel::ERR,name + " was skipped as an error has occurred when adding.");
            }
        }
        if (!peers.empty()) writeCacheToFile(peers);
        return peers;
    }
    bool WireguardTableReader::writeCacheToFile(vector<Tunneling::Peer> peers) {
        m_mutex.lock();
        w_cache = _fsopen(WIREGUARDCACHE, "w",_SH_DENYRW);
        if (w_cache == nullptr) {
            Logger::get().logEvent(Logger::SevLevel::DEBUG, "Cache already exists");
            m_mutex.unlock();
            return false;
        }
        for (auto peer: peers) {
            std::string line = peer.name + "," + Tunneling::keyToBase64(peer.publickey) + "," + peer.allowedIP;
            fprintf(w_cache, "%s\n", line.c_str());
        }
        fclose(w_cache);
        m_mutex.unlock();
        return true;
    }

    std::vector<Tunneling::Peer> WireguardTableReader::readLocalCache() {
        vector<Tunneling::Peer> peers = {};
        m_mutex.lock();
        w_cache = _fsopen(WIREGUARDCACHE, "r",_SH_DENYWR);
        if (w_cache == nullptr) {
            Logger::get().logEvent(Logger::SevLevel::ERR, "Failed to read cache");
            m_mutex.unlock();
            return peers;
        }
        char buf[256];
        while(fgets(buf,sizeof(buf),w_cache)) {
            std::string line(buf);
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            std::stringstream row(line);
            std::string name,key,ip;
            std::getline(row,name, ',');
            std::getline(row,key, ',');
            std::getline(row,ip, ',');
            if (key.empty()|| ip.empty()|| name.empty()) {
                continue;
            }
            try {
                peers.push_back({name, Tunneling::keyFromBase64(key), ip});
            } catch (const std::exception& e) {
                Logger::get().logEvent(Logger::SevLevel::WARN, "bad cache line for " + name + ": " + e.what());
            }
        }
        fclose(w_cache);
        m_mutex.unlock();
        return peers;
    }