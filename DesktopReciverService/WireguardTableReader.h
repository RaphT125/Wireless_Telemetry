//
// Created by Raphael on 2026-11-06.
//

#ifndef WIRELESS_TELEMETRY_WIREGUARDTABLEREADER_H
#define WIRELESS_TELEMETRY_WIREGUARDTABLEREADER_H
#include <vector>
#include <cpr/cpr.h>
#include "Tunneling.h"
#include "serv_config.h"
#include "Logger.h"
#include <stdio.h>
#include <condition_variable>
#include <sstream>
#include <chrono>

class WireguardTableReader {
public:

    void stop();
    std::vector<Tunneling::Peer> loadPeers();
    void refreshLoop(Tunneling & tunnel);
private:
    std::vector<Tunneling::Peer> checkGSheet();
    bool writeCacheToFile(vector<Tunneling::Peer> peers);
    std::vector<Tunneling::Peer> readLocalCache();
    FILE* w_cache;
    std::mutex m_mutex;
    std::atomic<bool> running_{true};
    std::mutex waitMutex_;
    std::condition_variable wake_;
};


#endif //WIRELESS_TELEMETRY_WIREGUARDTABLEREADER_H
