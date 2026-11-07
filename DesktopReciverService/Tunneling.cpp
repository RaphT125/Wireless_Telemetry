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
#include "Logger.h"

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif
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
    const GUID kAdapterGuid = { 0x6f1c3b52, 0x9d4e, 0x4a7b,
                                { 0x8e, 0x21, 0x5c, 0x0d, 0x7a, 0x93, 0x4b, 0xe6 } };

    [[noreturn]] void fail(const std::string& what, DWORD error = GetLastError()) {
        throw std::runtime_error(what + " (Windows error " + std::to_string(error) + ")");
    }

    void CALLBACK logToConsole(WIREGUARD_LOGGER_LEVEL level, DWORD64, LPCWSTR message) {
        const wchar_t marker = level == WIREGUARD_LOG_ERR ? L'!' : level == WIREGUARD_LOG_WARN ? L'-' : L'+';
        std::fwprintf(stderr, L"[wireguard %lc] %ls\n", marker, message);
    }

    template <typename Fn>
    Fn* lookup(HMODULE dll, const char* name) {
        FARPROC p = GetProcAddress(dll, name);
        if (!p) fail(std::string("wireguard.dll is missing ") + name);
        return reinterpret_cast<Fn*>(reinterpret_cast<void*>(p));
    }

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
            setConfiguration(allPeers);
            assignAddress();
            bringUp();
        }catch(const std::exception& e) {
            Logger::get().logEvent(Logger::SevLevel::ERR,
                std::string("WireGuard tunnel failed to start: ") + e.what());
            stop();
            throw;
        }catch(...) {
            stop();
            throw;
        }
    }
    void Tunneling::stop() {
        if (adapter_ && closeAdapterFn_) {
            closeAdapterFn_(adapter_);
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

    void Tunneling::setConfiguration(const vector<Peer>& peers) {
        const size_t bytes = sizeof(WIREGUARD_INTERFACE) +
                             peers.size() * (sizeof(WIREGUARD_PEER) + sizeof(WIREGUARD_ALLOWED_IP));
        std::vector<std::uint64_t> storage((bytes + 7) / 8, 0);
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
            wgPeer->Flags = WIREGUARD_PEER_HAS_PUBLIC_KEY | WIREGUARD_PEER_REPLACE_ALLOWED_IPS;
            std::memcpy(wgPeer->PublicKey, peer.publickey.data(), peer.publickey.size());
            wgPeer->AllowedIPsCount = 1;
            cursor += sizeof(WIREGUARD_PEER);

            auto* allowed = reinterpret_cast<WIREGUARD_ALLOWED_IP*>(cursor);
            allowed->AddressFamily = AF_INET;
            allowed->Cidr = 32;
            if (inet_pton(AF_INET, peer.allowedIP.c_str(), &allowed->Address.V4) != 1) {
                throw std::invalid_argument("Bad tunnel IP for peer " + peer.name + ": " + peer.allowedIP);
            }
            cursor += sizeof(WIREGUARD_ALLOWED_IP);
        }

        BOOL ok = setConfigurationFn_(adapter_, iface, static_cast<DWORD>(bytes));
        DWORD err = GetLastError();
        SecureZeroMemory(storage.data(), storage.size() * sizeof(std::uint64_t));
        if (!ok) fail("Could not configure the WireGuard adapter", err);
        peers_ = peers;
    }

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

        MIB_IPINTERFACE_ROW ipInterface;
        InitializeIpInterfaceEntry(&ipInterface);
        ipInterface.InterfaceLuid = row.InterfaceLuid;
        ipInterface.Family = AF_INET;
        err = GetIpInterfaceEntry(&ipInterface);
        if (err == NO_ERROR) {
            ipInterface.NlMtu = 1420;
            ipInterface.SitePrefixLength = 0;
            err = SetIpInterfaceEntry(&ipInterface);
        }
        if (err != NO_ERROR) fail("Could not set the tunnel MTU", err);
    }

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
    if (DEBUGMODE == 1) {
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
    return peers;
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
        }else {
            break;
        }
    }
        while (true ) {
            std::string key;
            std::cout<< "Please enter your device's public key: ";
            if (!getline(cin,key)) {
                throw::std::runtime_error("No input available");
            }
            try {
                peer.publickey = keyFromBase64(key);
                break;
            } catch (const std::exception& ) {
                std::cout << "not a valid key please try again";
            }
        }
        peer.allowedIP = "10.8.0."+ std::to_string(2 + nUser);
       return peer;
    }
    void Tunneling::updatePeers(std::vector<Peer> peers) {
         peersMutex_.lock();
        if (!adapter_) {
            peersMutex_.unlock();
            return;
        }
        try {
            vector<const Peer*> removed;
            for (const Peer& old : peers_) {
                bool stillThere = false;
                for (const Peer& p : peers) {
                    if (p.publickey == old.publickey) { stillThere = true; break; }
                }
                if (!stillThere) removed.push_back(&old);
            }

            const size_t bytes = sizeof(WIREGUARD_INTERFACE) +
                                 peers.size() * (sizeof(WIREGUARD_PEER) + sizeof(WIREGUARD_ALLOWED_IP)) +
                                 removed.size() * sizeof(WIREGUARD_PEER);
            std::vector<std::uint64_t> storage((bytes + 7) / 8, 0);
            BYTE* cursor = reinterpret_cast<BYTE*>(storage.data());

            auto* iface = reinterpret_cast<WIREGUARD_INTERFACE*>(cursor);
            iface->PeersCount = static_cast<DWORD>(peers.size() + removed.size());
            cursor += sizeof(WIREGUARD_INTERFACE);

            for (const Peer& peer : peers) {
                auto* wgPeer = reinterpret_cast<WIREGUARD_PEER*>(cursor);
                wgPeer->Flags = WIREGUARD_PEER_HAS_PUBLIC_KEY | WIREGUARD_PEER_REPLACE_ALLOWED_IPS;
                std::memcpy(wgPeer->PublicKey, peer.publickey.data(), peer.publickey.size());
                wgPeer->AllowedIPsCount = 1;
                cursor += sizeof(WIREGUARD_PEER);

                auto* allowed = reinterpret_cast<WIREGUARD_ALLOWED_IP*>(cursor);
                allowed->AddressFamily = AF_INET;
                allowed->Cidr = 32;
                if (inet_pton(AF_INET, peer.allowedIP.c_str(), &allowed->Address.V4) != 1) {
                    throw std::invalid_argument("Bad tunnel IP for peer " + peer.name + ": " + peer.allowedIP);
                }
                cursor += sizeof(WIREGUARD_ALLOWED_IP);
            }

            for (const Peer* old : removed) {
                auto* wgPeer = reinterpret_cast<WIREGUARD_PEER*>(cursor);
                wgPeer->Flags = WIREGUARD_PEER_HAS_PUBLIC_KEY | WIREGUARD_PEER_REMOVE;
                std::memcpy(wgPeer->PublicKey, old->publickey.data(), old->publickey.size());
                cursor += sizeof(WIREGUARD_PEER);
            }

            if (!setConfigurationFn_(adapter_, iface, static_cast<DWORD>(bytes))) {
                fail("Could not update WireGuard peers");
            }
            peers_ = peers;
        } catch (...) {
            peersMutex_.unlock();
            throw;
        }
        peersMutex_.unlock();
    }

