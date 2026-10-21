//
// Created by Raphael on 2026-10-20.
//

#ifndef WIRELESS_TELEMETRY_TUNNELING_H
#define WIRELESS_TELEMETRY_TUNNELING_H

// Order matters: winsock2.h must come before windows.h (wireguard.h also includes both).
#include <array>
#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>   // CreateUnicastIpAddressEntry etc. Linked as "iphlpapi" in CMakeLists.txt

#include <array>
#include <vector>
#include <string>

#include "wireguard.h"
using namespace std;
class Tunneling {
public:
    using Key = array<BYTE, WIREGUARD_KEY_LENGTH>;

    struct Peer {
        string name;
        Key publickey;
        string allowedIP;
    };
    static constexpr WORD kPort = 51820;
    static constexpr const char* kAddress = "10.8.0.1";
    static constexpr BYTE  kPrefixLength= 24;


    Tunneling() = default;
    ~Tunneling();
    Tunneling(const Tunneling&) = delete;
    Tunneling& operator=(const Tunneling&) = delete;

    void start(const wstring& keyFile, const vector<Peer>&peers);
    void stop();

    string publicKeyBase64() const {return keyToBase64(publicKey_);}

    static Key keyFromBase64(const string& base64);
    static string keyToBase64(const Key& key);

private:
    void initialization();

    void loadOrCreateKey(const wstring& keyFile);
    void createAdaptor();
    void setConfiguration(const vector<Peer>& peers);
    void assignAddress();
    void bringUp();
    vector<Peer> listOfPeers(const vector<Peer>& existing);
    Peer askForPeer(int nUser);
    HMODULE dll_= nullptr;
    WIREGUARD_ADAPTER_HANDLE adapter_ = nullptr;
    Key privateKey_{};
    Key publicKey_{};

    WIREGUARD_CREATE_ADAPTER_FUNC*    createAdapterFn_    = nullptr;
    WIREGUARD_CLOSE_ADAPTER_FUNC*     closeAdapterFn_     = nullptr;
    WIREGUARD_GET_ADAPTER_LUID_FUNC*  getAdapterLuidFn_   = nullptr;
    WIREGUARD_SET_CONFIGURATION_FUNC* setConfigurationFn_ = nullptr;
    WIREGUARD_SET_ADAPTER_STATE_FUNC* setAdapterStateFn_  = nullptr;
    WIREGUARD_SET_LOGGER_FUNC*        setLoggerFn_        = nullptr;

};


#endif //WIRELESS_TELEMETRY_TUNNELING_H
