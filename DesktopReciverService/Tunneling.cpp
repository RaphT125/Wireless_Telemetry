//
// Created by Raphael on 2026-10-20.
//

#include "Tunneling.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <sddl.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <iostream>


#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif
// MinGW's bcrypt.h lacks some names that Visual Studio's has; these are their official values.
#ifndef BCRYPT_ECDH_ALGORITHM
#define BCRYPT_ECDH_ALGORITHM L"ECDH"
#endif
#ifndef BCRYPT_ECCPRIVATE_BLOB
#define BCRYPT_ECCPRIVATE_BLOB L"ECCPRIVATEBLOB"
#endif
#ifndef BCRYPT_ECC_CURVE_NAME
#define BCRYPT_ECC_CURVE_NAME L"ECCCurveName"
#endif
#ifndef BCRYPT_ECC_CURVE_25519
#define BCRYPT_ECC_CURVE_25519 L"curve25519"
#endif

namespace {
    // The same GUID on every start, so Windows treats it as the same network each time
    // (its network type and firewall settings stick).
    const GUID kAdapterGuid = { 0x6f1c3b52, 0x9d4e, 0x4a7b,
                                { 0x8e, 0x21, 0x5c, 0x0d, 0x7a, 0x93, 0x4b, 0xe6 } };

    [[noreturn]] void fail(const std::string& what, DWORD error = GetLastError()) {
        throw std::runtime_error(what + " (Windows error " + std::to_string(error) + ")");
    }

    // wireguard.dll sends its own log lines here.
    void CALLBACK logToConsole(WIREGUARD_LOGGER_LEVEL level, DWORD64 /*timestamp*/, LPCWSTR message) {
        const wchar_t marker = level == WIREGUARD_LOG_ERR ? L'!' : level == WIREGUARD_LOG_WARN ? L'-' : L'+';
        std::fwprintf(stderr, L"[wireguard %lc] %ls\n", marker, message);
    }

    template <typename Fn>
    Fn* lookup(HMODULE dll, const char* name) {
        FARPROC p = GetProcAddress(dll, name);
        if (!p) fail(std::string("wireguard.dll is missing ") + name);
        return reinterpret_cast<Fn*>(reinterpret_cast<void*>(p));
    }

    // X25519 key pair using Windows' built-in crypto
    void generateKeyPair(Tunneling::Key& publicKey, Tunneling::Key& privateKey) {
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        struct {
            BCRYPT_ECCKEY_BLOB header;
            BYTE pub[32];
            BYTE unused[32];
            BYTE priv[32];
        } blob{};
        ULONG bytes = 0;

        NTSTATUS s = BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDH_ALGORITHM, nullptr, 0);
        if (NT_SUCCESS(s))
            s = BCryptSetProperty(alg, BCRYPT_ECC_CURVE_NAME, (PUCHAR)BCRYPT_ECC_CURVE_25519,
                                  sizeof(BCRYPT_ECC_CURVE_25519), 0);
        if (NT_SUCCESS(s)) s = BCryptGenerateKeyPair(alg, &key, 255, 0);
        if (NT_SUCCESS(s)) s = BCryptFinalizeKeyPair(key, 0);
        if (NT_SUCCESS(s))
            s = BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB,
                                reinterpret_cast<PUCHAR>(&blob), sizeof blob, &bytes, 0);
        if (NT_SUCCESS(s)) {
            std::memcpy(publicKey.data(), blob.pub, publicKey.size());
            std::memcpy(privateKey.data(), blob.priv, privateKey.size());
        }
        SecureZeroMemory(&blob, sizeof blob);
        if (key) BCryptDestroyKey(key);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        if (!NT_SUCCESS(s)) fail("Could not generate a WireGuard key pair", static_cast<DWORD>(s));
    }

    // DPAPI (machine scope): the key file is useless if copied to another PC.
    std::vector<BYTE> encrypt(const BYTE* data, DWORD size) {
        DATA_BLOB in{ size, const_cast<BYTE*>(data) }, out{};
        if (!CryptProtectData(&in, L"WirelessTelemetry WireGuard key", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_LOCAL_MACHINE, &out)) {
            fail("Could not encrypt the key");
        }
        std::vector<BYTE> result(out.pbData, out.pbData + out.cbData);
        LocalFree(out.pbData);
        return result;
    }

    std::vector<BYTE> decrypt(std::vector<BYTE>& data) {
        DATA_BLOB in{ static_cast<DWORD>(data.size()), data.data() }, out{};
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
            fail("Could not decrypt the key file (was it copied from another PC?)");
        }
        std::vector<BYTE> result(out.pbData, out.pbData + out.cbData);
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
        return result;
    }

    // Creates the key file's folder so only SYSTEM and Administrators can read it.
    void createProtectedFolder(const std::filesystem::path& folder) {
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)", SDDL_REVISION_1, &sd, nullptr)) {
            fail("Could not build folder permissions");
        }
        SECURITY_ATTRIBUTES sa{ sizeof sa, sd, FALSE };
        BOOL created = CreateDirectoryW(folder.c_str(), &sa);
        DWORD err = GetLastError();
        LocalFree(sd);
        if (!created && err != ERROR_ALREADY_EXISTS) fail("Could not create the key folder", err);
    }
}

    Tunneling::~Tunneling() {
        stop();
    }

    void Tunneling::start(const wstring& keyFile,  const vector<Peer>&peers) {
    if (adapter_!= nullptr){return;}
        try {
            vector<Peer> allPeers = listOfPeers(peers);
            initialization();
            loadOrCreateKey(keyFile);
            createAdaptor();
            setConfiguration(peers);
            assignAddress();
            bringUp();
        }catch(...) {
            stop();
            throw;
        }
    }
    void Tunneling::stop() {
        if (adapter_ && closeAdapterFn_) {
            closeAdapterFn_(adapter_);   // removes the adapter and its IP from Windows
        }
        adapter_ = nullptr;
        if (dll_) {
            FreeLibrary(dll_);
        }
        dll_ = nullptr;
        createAdapterFn_    = nullptr;
        closeAdapterFn_     = nullptr;
        getAdapterLuidFn_   = nullptr;
        setConfigurationFn_ = nullptr;
        setAdapterStateFn_  = nullptr;
        setLoggerFn_        = nullptr;

        SecureZeroMemory(privateKey_.data(), privateKey_.size());
    }

    void Tunneling::initialization() {
        dll_ = LoadLibraryExW(L"wireguard.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!dll_) {
            throw system_error(static_cast<int>(GetLastError()), system_category(), "Could not load wireguard.dll (it must be next to the .exe)");
        }
        createAdapterFn_ = lookup<WIREGUARD_CREATE_ADAPTER_FUNC>(dll_, "WireGuardCreateAdapter");
        closeAdapterFn_ = lookup<WIREGUARD_CLOSE_ADAPTER_FUNC>(dll_, "WireGuardCloseAdapter");
        getAdapterLuidFn_ = lookup<WIREGUARD_GET_ADAPTER_LUID_FUNC>(dll_, "WireGuardGetAdapterLUID");
        setConfigurationFn_ = lookup<WIREGUARD_SET_CONFIGURATION_FUNC>(dll_, "WireGuardSetConfiguration");
        setAdapterStateFn_ =  lookup<WIREGUARD_SET_ADAPTER_STATE_FUNC>(dll_, "WireGuardSetAdapterState");
        setLoggerFn_ = lookup<WIREGUARD_SET_LOGGER_FUNC>(dll_, "WireGuardSetLogger");

       setLoggerFn_(logToConsole);

    }


    void Tunneling::loadOrCreateKey(const wstring& keyFile) {
        filesystem::path path(keyFile);

        ifstream in(path,ios::binary);
        if (in) {
            std::vector<BYTE> encrypted((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
            std::vector<BYTE> plain = decrypt(encrypted);
            if (plain.size() != privateKey_.size() + publicKey_.size()) {
                SecureZeroMemory(plain.data(), plain.size());
                throw std::runtime_error("Key file is damaged - delete it to make a new key");
            }
            std::memcpy(privateKey_.data(), plain.data(), privateKey_.size());
            std::memcpy(publicKey_.data(), plain.data() + privateKey_.size(), publicKey_.size());
            SecureZeroMemory(plain.data(), plain.size());
        }else {
            generateKeyPair(publicKey_, privateKey_);

            std::array<BYTE, 64> plain{};
            std::memcpy(plain.data(), privateKey_.data(), privateKey_.size());
            std::memcpy(plain.data() + privateKey_.size(), publicKey_.data(), publicKey_.size());
            std::vector<BYTE> encrypted = encrypt(plain.data(), static_cast<DWORD>(plain.size()));
            SecureZeroMemory(plain.data(), plain.size());

            createProtectedFolder(path.parent_path());
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(encrypted.data()),
                      static_cast<std::streamsize>(encrypted.size()));
            if (!out) fail("Could not save the key file");
        }
    }
    void Tunneling::createAdaptor() {
        adapter_ = createAdapterFn_(L"Telemetry", L"WirelessTelemetry", &kAdapterGuid);
        if (!adapter_) {fail("failed to create adaptor, please run in adminsitrator");}
    }

    // WireGuardNT wants one memory block laid out as:
    // [INTERFACE][PEER][ALLOWED_IP][PEER][ALLOWED_IP]...
    void Tunneling::setConfiguration(const vector<Peer>& peers) {
        const size_t bytes = sizeof(WIREGUARD_INTERFACE) +
                             peers.size() * (sizeof(WIREGUARD_PEER) + sizeof(WIREGUARD_ALLOWED_IP));
        std::vector<std::uint64_t> storage((bytes + 7) / 8, 0);   // zeroed, 8-byte aligned
        BYTE* cursor = reinterpret_cast<BYTE*>(storage.data());

        auto* iface = reinterpret_cast<WIREGUARD_INTERFACE*>(cursor);
        iface->Flags = WIREGUARD_INTERFACE_HAS_PRIVATE_KEY | WIREGUARD_INTERFACE_HAS_LISTEN_PORT |
                       WIREGUARD_INTERFACE_REPLACE_PEERS;
        iface->ListenPort = kPort;
        std::memcpy(iface->PrivateKey, privateKey_.data(), privateKey_.size());
        iface->PeersCount = static_cast<DWORD>(peers.size());
        cursor += sizeof(WIREGUARD_INTERFACE);

        for (const Peer& peer : peers) {
            auto* wgPeer = reinterpret_cast<WIREGUARD_PEER*>(cursor);
            // No endpoint and no keepalive: the desktop only listens; peers connect to it.
            wgPeer->Flags = WIREGUARD_PEER_HAS_PUBLIC_KEY | WIREGUARD_PEER_REPLACE_ALLOWED_IPS;
            std::memcpy(wgPeer->PublicKey, peer.publickey.data(), peer.publickey.size());
            wgPeer->AllowedIPsCount = 1;
            cursor += sizeof(WIREGUARD_PEER);

            auto* allowed = reinterpret_cast<WIREGUARD_ALLOWED_IP*>(cursor);
            allowed->AddressFamily = AF_INET;
            allowed->Cidr = 32;   // exactly this one address, e.g. "10.8.0.3"
            if (inet_pton(AF_INET, peer.allowedIP.c_str(), &allowed->Address.V4) != 1) {
                throw std::invalid_argument("Bad tunnel IP for peer " + peer.name + ": " + peer.allowedIP);
            }
            cursor += sizeof(WIREGUARD_ALLOWED_IP);
        }

        BOOL ok = setConfigurationFn_(adapter_, iface, static_cast<DWORD>(bytes));
        DWORD err = GetLastError();
        SecureZeroMemory(storage.data(), storage.size() * sizeof(std::uint64_t));   // wipe the key copy
        if (!ok) fail("Could not configure the WireGuard adapter", err);
    }

    // Gives the adapter 10.8.0.1/24. Windows then routes all of 10.8.0.x through the tunnel.
    void Tunneling::assignAddress() {
        MIB_UNICASTIPADDRESS_ROW row;
        InitializeUnicastIpAddressEntry(&row);
        getAdapterLuidFn_(adapter_, &row.InterfaceLuid);
        row.Address.Ipv4.sin_family = AF_INET;
        inet_pton(AF_INET, kAddress, &row.Address.Ipv4.sin_addr);
        row.OnLinkPrefixLength = kPrefixLength;
        row.DadState = IpDadStatePreferred;

        DWORD err = CreateUnicastIpAddressEntry(&row);
        if (err != NO_ERROR && err != ERROR_OBJECT_ALREADY_EXISTS) fail("Could not assign the tunnel IP", err);

        // MTU 1420 leaves room for WireGuard's own header inside a normal 1500-byte packet.
        MIB_IPINTERFACE_ROW ipInterface;
        InitializeIpInterfaceEntry(&ipInterface);
        ipInterface.InterfaceLuid = row.InterfaceLuid;
        ipInterface.Family = AF_INET;
        err = GetIpInterfaceEntry(&ipInterface);
        if (err == NO_ERROR) {
            ipInterface.NlMtu = 1420;
            ipInterface.SitePrefixLength = 0;   // must be 0 for IPv4 or the set call fails
            err = SetIpInterfaceEntry(&ipInterface);
        }
        if (err != NO_ERROR) fail("Could not set the tunnel MTU", err);
    }

    // Turns it on. From here it accepts handshakes on UDP 51820.
    void Tunneling::bringUp() {
        if (!setAdapterStateFn_(adapter_, WIREGUARD_ADAPTER_STATE_UP)) fail("Could not bring the tunnel up");
    }

    Tunneling::Key Tunneling::keyFromBase64(const string& base64) {
        Key key{};
        DWORD size = static_cast<DWORD>(key.size());
        if (!CryptStringToBinaryA(base64.c_str(), 0, CRYPT_STRING_BASE64, key.data(), &size,
                                  nullptr, nullptr) || size != key.size()) {
            throw std::invalid_argument("Not a valid WireGuard key: " + base64);
        }
        return key;
    }

    string Tunneling::keyToBase64(const Key& key) {
        DWORD size = 0;
        const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
        CryptBinaryToStringA(key.data(), static_cast<DWORD>(key.size()), flags, nullptr, &size);
        string out(size, '\0');
        CryptBinaryToStringA(key.data(), static_cast<DWORD>(key.size()), flags, out.data(), &size);
        out.resize(size);
        return out;
    }

    vector<Tunneling::Peer> Tunneling::listOfPeers(const vector<Peer>& existing) {
        vector<Peer> peers = existing;
        while (true) {
            std::cout << "Would you like to add another user? (Y/N): ";
            std::string response;
            if (!std::getline(std::cin, response)) return peers;

            if (response == "Y" || response == "y") {
                peers.push_back(askForPeer(peers.size()));
            } else if (response == "N" || response == "n") {
                return peers;
            } else {
                std::cout << "Please enter Y or N.\n";
            }
        }
    }
    Tunneling::Peer Tunneling::askForPeer(int nUser) {
        Peer peer;
    while (true ) {
        std::cout<< "Please enter your device name: ";
        std::string name;
        if (!getline(cin,peer.name)) {
            throw::std::runtime_error("No input available");
        }
        if (peer.name.empty()) {
            cout<<"Please enter a valid name";
        }
        break;
    }
        while (true ) {
            std::string key;
            std::cout<< "Please enter your device's public key: ";
            if (getline(cin,key)) {
                throw::std::runtime_error("No input available");
            }
            try {
                peer.publickey = keyFromBase64(key);
                break;
            } catch (const std::exception& ) {
                std::cout << "not a valid key please try again";
            }
        }
        peer.allowedIP = "10.8.0."+ (2 + nUser);
       return peer;
    }

