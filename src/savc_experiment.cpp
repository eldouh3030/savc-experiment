// SAVC experimental evaluation - single-file, dependency-free, portable C++17.
// Version: 1.1 (batch latency timing; locale-free canonical encoding)
// Build (Linux/macOS):  g++ -O2 -std=c++17 -pthread savc_experiment.cpp -o savc_experiment
// Build (Windows/MSVC): cl /O2 /std:c++17 /EHsc savc_experiment.cpp
// Run:                  savc_experiment [output_dir]   (default: results)
//
// All randomness comes from a self-implemented SplitMix64 generator, so every
// detection result is bit-identical on every platform and compiler. Only the
// timing measurements depend on the machine.

#include <array>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ------------------------------------------------------------------ RNG
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    uint64_t below(uint64_t n) { return next() % n; }  // n << 2^64, bias negligible
    double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return uni() < p; }
};

// ------------------------------------------------------------------ SHA-256 (FIPS 180-4)
struct Sha256 {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len = 0;
    size_t bl = 0;
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    Sha256() {
        static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::memcpy(h, iv, sizeof iv);
    }
    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void update(const void* data, size_t n) {
        const uint8_t* p = (const uint8_t*)data;
        len += n;
        while (n) {
            size_t t = std::min(n, 64 - bl);
            std::memcpy(buf + bl, p, t);
            bl += t; p += t; n -= t;
            if (bl == 64) { block(buf); bl = 0; }
        }
    }
    void update(const std::string& s) { update(s.data(), s.size()); }
    std::array<uint8_t, 32> final() {
        uint64_t bits = len * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t z = 0;
        while (bl != 56) update(&z, 1);
        uint8_t L[8];
        for (int i = 0; i < 8; i++) L[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(L, 8);
        std::array<uint8_t, 32> out;
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 4; j++) out[4 * i + j] = (uint8_t)(h[i] >> (24 - 8 * j));
        return out;
    }
};
using Digest = std::array<uint8_t, 32>;

Digest sha256(const std::string& s) { Sha256 c; c.update(s); return c.final(); }

// HMAC-SHA256 (RFC 2104)
Digest hmac(const Digest& key, const std::string& msg) {
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        uint8_t k = i < 32 ? key[i] : 0;
        ipad[i] = k ^ 0x36; opad[i] = k ^ 0x5c;
    }
    Sha256 in; in.update(ipad, 64); in.update(msg);
    Digest ih = in.final();
    Sha256 out; out.update(opad, 64); out.update(ih.data(), 32);
    return out.final();
}

bool ct_equal(const Digest& a, const Digest& b) {
    uint8_t d = 0;
    for (int i = 0; i < 32; i++) d |= a[i] ^ b[i];
    return d == 0;
}

std::string hex(const Digest& d) {
    static const char* x = "0123456789abcdef";
    std::string s;
    for (auto b : d) { s += x[b >> 4]; s += x[b & 15]; }
    return s;
}

bool self_test() {
    // FIPS 180-4 / RFC 4231 test vectors
    bool ok = hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    ok = ok && hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    // RFC 4231 test case 2 uses a 4-byte key "Jefe"; our key type is 32 bytes zero-padded, which is equivalent.
    Digest k{}; k[0] = 'J'; k[1] = 'e'; k[2] = 'f'; k[3] = 'e';
    ok = ok && hex(hmac(k, "what do ya want for nothing?")) ==
                   "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
    return ok;
}

Digest random_key(Rng& r) {
    Digest k;
    for (int i = 0; i < 32; i += 8) {
        uint64_t v = r.next();
        for (int j = 0; j < 8; j++) k[i + j] = (uint8_t)(v >> (8 * j));
    }
    return k;
}

// ------------------------------------------------------------------ Ledger (hash chain)
enum class Ev : uint8_t { KeyRegister, KeyRevoke, AccountRegister, VvcIssue };
struct Block {
    uint64_t index;
    int64_t ts;
    Ev type;
    Digest payload;
    Digest prev;
    Digest hash;
};
Digest block_hash(const Block& b) {
    Sha256 c;
    c.update(&b.index, sizeof b.index);
    c.update(&b.ts, sizeof b.ts);
    c.update(&b.type, sizeof b.type);
    c.update(b.payload.data(), 32);
    c.update(b.prev.data(), 32);
    return c.final();
}
struct Ledger {
    std::vector<Block> chain;
    void append(Ev t, const Digest& payload, int64_t ts) {
        Block b{chain.size(), ts, t, payload, chain.empty() ? Digest{} : chain.back().hash, {}};
        b.hash = block_hash(b);
        chain.push_back(b);
    }
    // returns index of first invalid block, or -1 if valid
    long long validate() const {
        Digest prev{};
        for (size_t i = 0; i < chain.size(); i++) {
            const Block& b = chain[i];
            if (!ct_equal(b.prev, prev) || !ct_equal(block_hash(b), b.hash)) return (long long)i;
            prev = b.hash;
        }
        return -1;
    }
};

// ------------------------------------------------------------------ World model
enum MsgType { CFP = 0, ACK = 1, DECISION = 2, FEE = 3 };
struct Rep { Digest key; bool revoked = false; };
struct Venue {
    std::string domain;      // genuine domain registered with the intermediary
    std::vector<Rep> reps;
    std::vector<std::string> accounts;  // verified payment accounts
};
struct VvcRecord {
    int venue, rep;
    std::string recipient;
    int type;
    Digest digest;
    int64_t expiry;
    bool revoked = false;
};
struct Message {
    int claimed_venue;
    std::string from_domain;
    bool dmarc_pass;
    std::string recipient;
    int type;
    std::string body;        // canonical body (includes payment account / amount for FEE)
    std::string vvc_ref;     // empty if none
    Digest vvc_tag{};
    int64_t now;             // verification time
    int cls;                 // ground-truth class (see CLASS_NAMES)
    bool attack;
};

const char* CLASS_NAMES[] = {
    "Legitimate", "Legitimate (forwarded)", "Legitimate (unregistered)",
    "A1 Exact-domain spoofing", "A2 Look-alike venue clone", "A3 Compromised mailbox",
    "A4 Replay to new recipient", "A5 Replay after expiry", "A6 Content tampering",
    "A7 Forged VVC", "A8 Payment redirection (stolen key)"};
const int NCLASS = 11;

// Locale-free integer formatting (std::to_chars) keeps the verification path free of
// iostream/locale locks, so multi-threaded throughput is not limited by the C++ runtime.
static void append_int(std::string& s, long long v) {
    char buf[24];
    auto res = std::to_chars(buf, buf + sizeof buf, v);
    s.append(buf, res.ptr);
}
std::string canonical(int venue, int rep, const std::string& rcpt, int type, const Digest& dg, int64_t exp) {
    std::string o;
    o.reserve(rcpt.size() + 100);
    append_int(o, venue); o += '|';
    append_int(o, rep); o += '|';
    o += rcpt; o += '|';
    append_int(o, type); o += '|';
    o += hex(dg); o += '|';
    append_int(o, exp);
    return o;
}

struct World {
    std::vector<Venue> venues;
    std::unordered_map<std::string, int> domain_to_venue;  // whitelist
    std::unordered_map<std::string, VvcRecord> registry;
    std::vector<std::vector<size_t>> key_block;  // ledger index of each representative key registration
    Ledger ledger;
    uint64_t ref_counter = 0;
    int64_t clock = 1700000000;

    void build(Rng& r, int nvenues, int reps_per_venue) {
        venues.clear(); domain_to_venue.clear(); registry.clear(); ledger.chain.clear(); key_block.clear();
        for (int v = 0; v < nvenues; v++) {
            Venue V;
            V.domain = "venue" + std::to_string(v) + ".org";
            key_block.emplace_back();
            for (int k = 0; k < reps_per_venue; k++) {
                Rep rp; rp.key = random_key(r);
                V.reps.push_back(rp);
                key_block.back().push_back(ledger.chain.size());
                ledger.append(Ev::KeyRegister, sha256(V.domain + "#" + std::to_string(k) + hex(rp.key)), clock);
            }
            int na = 1 + (int)r.below(2);
            for (int a = 0; a < na; a++) {
                V.accounts.push_back("ACC-" + std::to_string(v) + "-" + std::to_string(a));
                ledger.append(Ev::AccountRegister, sha256(V.accounts.back()), clock);
            }
            domain_to_venue[V.domain] = v;
            venues.push_back(V);
        }
    }
    // venue registers an outgoing message and obtains a VVC
    std::pair<std::string, Digest> issue(int v, int rep, const std::string& rcpt, int type, const std::string& body,
                                         int64_t ttl, bool log_ledger = true) {
        Digest dg = sha256(body);
        int64_t exp = clock + ttl;
        std::string ref = "SAVC-" + std::to_string(++ref_counter);
        VvcRecord rec{v, rep, rcpt, type, dg, exp};
        registry[ref] = rec;
        Digest tag = hmac(venues[v].reps[rep].key, canonical(v, rep, rcpt, type, dg, exp));
        if (log_ledger) ledger.append(Ev::VvcIssue, sha256(ref + hex(tag)), clock);
        return {ref, tag};
    }
};

// ------------------------------------------------------------------ Defenses
// return true = ACCEPT as legitimate
bool d_dmarc(const World&, const Message& m) { return m.dmarc_pass; }
bool d_whitelist(const World& w, const Message& m) {
    auto it = w.domain_to_venue.find(m.from_domain);
    return it != w.domain_to_venue.end() && it->second == m.claimed_venue;
}
bool d_wl_dmarc(const World& w, const Message& m) { return d_whitelist(w, m) && m.dmarc_pass; }

bool savc_vvc(const World& w, const Message& m) {
    if (m.vvc_ref.empty()) return false;
    auto it = w.registry.find(m.vvc_ref);
    if (it == w.registry.end()) return false;
    const VvcRecord& r = it->second;
    if (r.revoked || r.venue != m.claimed_venue) return false;
    if (r.recipient != m.recipient || r.type != m.type) return false;
    if (m.now > r.expiry) return false;
    const Rep& rep = w.venues[r.venue].reps[r.rep];
    if (rep.revoked) return false;
    Digest dg = sha256(m.body);
    if (!ct_equal(dg, r.digest)) return false;
    Digest tag = hmac(rep.key, canonical(r.venue, r.rep, r.recipient, r.type, r.digest, r.expiry));
    return ct_equal(tag, m.vvc_tag);
}
std::string fee_account(const std::string& body) {
    auto p = body.find("pay_to=");
    if (p == std::string::npos) return "";
    auto e = body.find('|', p);
    return body.substr(p + 7, e == std::string::npos ? std::string::npos : e - p - 7);
}
bool savc_full(const World& w, const Message& m) {
    if (!savc_vvc(w, m)) return false;
    if (m.type == FEE) {
        std::string acc = fee_account(m.body);
        const auto& A = w.venues[m.claimed_venue].accounts;
        if (std::find(A.begin(), A.end(), acc) == A.end()) return false;
    }
    return true;
}

typedef bool (*Defense)(const World&, const Message&);
const char* DEF_NAMES[] = {"DMARC only", "Whitelist only", "Whitelist + DMARC", "SAVC (VVC only)", "SAVC (VVC + payment)"};
Defense DEFS[] = {d_dmarc, d_whitelist, d_wl_dmarc, savc_vvc, savc_full};
const int NDEF = 5;

// ------------------------------------------------------------------ Dataset generation
struct Params {
    int nvenues = 200, reps = 3, nresearchers = 5000;
    int n_legit = 5000, n_per_attack = 625;
    double p_forward = 0.03;     // legit mail relayed through lists/forwarders that break DKIM alignment
    double p_unregistered = 0.005;  // venue forgets to register an outgoing message (operational error)
    int64_t ttl = 14 * 86400;
};

std::string make_body(Rng& r, int v, int type, const World& w, const std::string& override_acc = "") {
    std::ostringstream o;
    o << "venue=" << v << "|type=" << type << "|nonce=" << r.next();
    if (type == FEE) {
        std::string acc = override_acc.empty() ? w.venues[v].accounts[r.below(w.venues[v].accounts.size())] : override_acc;
        o << "|pay_to=" << acc << "|amount=" << (200 + 50 * r.below(12));
    }
    return o.str();
}

std::vector<Message> generate(World& w, Rng& r, const Params& P) {
    std::vector<Message> out;
    auto rcpt = [&](void) { return "author" + std::to_string(r.below(P.nresearchers)) + "@uni.edu"; };
    auto rtype = [&](void) { return (int)r.below(4); };

    // legitimate traffic
    for (int i = 0; i < P.n_legit; i++) {
        int v = (int)r.below(P.nvenues), rep = (int)r.below(P.reps), t = rtype();
        std::string to = rcpt(), body = make_body(r, v, t, w);
        Message m{v, w.venues[v].domain, true, to, t, body, "", {}, w.clock, 0, false};
        if (r.chance(P.p_unregistered)) {
            m.cls = 2;  // venue did not register this message -> no VVC
        } else {
            auto vt = w.issue(v, rep, to, t, body, P.ttl);
            m.vvc_ref = vt.first; m.vvc_tag = vt.second;
        }
        if (r.chance(P.p_forward)) { m.dmarc_pass = false; if (m.cls == 0) m.cls = 1; }
        m.now = w.clock + (int64_t)r.below(P.ttl);  // verified within validity window
        out.push_back(m);
    }
    // attacks
    for (int a = 0; a < 8; a++) {
        for (int i = 0; i < P.n_per_attack; i++) {
            int v = (int)r.below(P.nvenues), rep = (int)r.below(P.reps), t = rtype();
            std::string to = rcpt();
            Message m{v, "", false, to, t, "", "", {}, w.clock, 3 + a, true};
            switch (a) {
            case 0:  // A1 exact-domain spoofing, no authentication
                m.from_domain = w.venues[v].domain; m.dmarc_pass = false;
                m.body = make_body(r, v, t, w, t == FEE ? "ATTACKER-ACC" : "");
                break;
            case 1: {  // A2 look-alike clone with its own valid SPF/DKIM; copies the VVC format with a made-up ref
                m.from_domain = "venue" + std::to_string(v) + "-conf.org"; m.dmarc_pass = true;
                m.body = make_body(r, v, t, w, t == FEE ? "ATTACKER-ACC" : "");
                m.vvc_ref = "SAVC-" + std::to_string(r.below(1000000000)); m.vvc_tag = random_key(r);
                break;
            }
            case 2:  // A3 compromised representative mailbox (genuine domain, DMARC pass), no access to signing key
                m.from_domain = w.venues[v].domain; m.dmarc_pass = true;
                m.body = make_body(r, v, t, w, t == FEE ? "ATTACKER-ACC" : "");
                break;
            case 3: {  // A4 genuine message (valid VVC) forwarded to a different victim
                std::string orig = rcpt();
                while (orig == to) orig = rcpt();
                m.body = make_body(r, v, t, w);
                auto vt = w.issue(v, rep, orig, t, m.body, P.ttl);
                m.from_domain = w.venues[v].domain; m.dmarc_pass = true;  // original DKIM signature still valid
                m.vvc_ref = vt.first; m.vvc_tag = vt.second;
                m.now = w.clock + (int64_t)r.below(P.ttl);
                break;
            }
            case 4: {  // A5 genuine message replayed to the same recipient after expiry
                m.body = make_body(r, v, t, w);
                auto vt = w.issue(v, rep, to, t, m.body, P.ttl);
                m.from_domain = w.venues[v].domain; m.dmarc_pass = true;
                m.vvc_ref = vt.first; m.vvc_tag = vt.second;
                m.now = w.clock + P.ttl + 1 + (int64_t)r.below(180 * 86400);
                break;
            }
            case 5: {  // A6 genuine message whose content (e.g., amount / account) is altered
                m.type = t = FEE;
                std::string body = make_body(r, v, FEE, w);
                auto vt = w.issue(v, rep, to, FEE, body, P.ttl);
                std::string acc = fee_account(body);
                m.body = body.replace(body.find(acc), acc.size(), "ATTACKER-ACC");
                m.from_domain = w.venues[v].domain;
                m.dmarc_pass = r.chance(0.5);  // 50%: altered in transit (DKIM breaks); 50%: altered in a compromised mailbox before signing
                m.vvc_ref = vt.first; m.vvc_tag = vt.second;
                m.now = w.clock + (int64_t)r.below(P.ttl);
                break;
            }
            case 6: {  // A7 forged VVC: existing reference (scraped) + guessed authenticator, from a compromised mailbox
                m.from_domain = w.venues[v].domain; m.dmarc_pass = true;
                m.body = make_body(r, v, t, w, t == FEE ? "ATTACKER-ACC" : "");
                m.vvc_ref = "SAVC-" + std::to_string(1 + r.below(w.ref_counter));
                m.vvc_tag = random_key(r);
                break;
            }
            case 7: {  // A8 attacker holds stolen representative signing credentials and registers a fee request to own account
                m.type = t = FEE;
                m.body = make_body(r, v, FEE, w, "ATTACKER-ACC");
                auto vt = w.issue(v, rep, to, FEE, m.body, P.ttl);
                m.from_domain = w.venues[v].domain; m.dmarc_pass = true;
                m.vvc_ref = vt.first; m.vvc_tag = vt.second;
                m.now = w.clock + (int64_t)r.below(P.ttl);
                break;
            }
            }
            out.push_back(m);
        }
    }
    return out;
}

// ------------------------------------------------------------------ Timing helpers
using clk = std::chrono::steady_clock;
double us_since(clk::time_point t0) {
    return std::chrono::duration<double, std::micro>(clk::now() - t0).count();
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "results";
#ifdef _WIN32
    (void)std::system(("mkdir " + dir + " 2>nul").c_str());
#else
    (void)!std::system(("mkdir -p " + dir).c_str());
#endif
    if (!self_test()) { std::cerr << "Crypto self-test FAILED\n"; return 1; }
    std::cout << "Crypto self-test passed (SHA-256, HMAC-SHA256 RFC 4231)\n";
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    {
        std::ofstream info(dir + "/run_info.txt");
        info << "program_version=1.1\n";
#if defined(_MSC_VER)
        info << "compiler=MSVC " << _MSC_FULL_VER << "\n";
#elif defined(__clang__)
        info << "compiler=Clang " << __clang_major__ << "." << __clang_minor__ << "\n";
#elif defined(__GNUC__)
        info << "compiler=GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "\n";
#endif
        info << "pointer_bits=" << sizeof(void*) * 8 << "\n";
        info << "hardware_threads=" << std::thread::hardware_concurrency() << "\n";
        info << "cplusplus=" << __cplusplus << "\n";
    }

    Params P;
    const int RUNS = 10;

    // ---------------- Experiment 1: detection per attack class and defense
    // counts[run][class][def] = accepted
    std::vector<std::vector<std::array<long, NDEF>>> acc(RUNS, std::vector<std::array<long, NDEF>>(NCLASS));
    std::vector<std::array<long, NCLASS>> tot(RUNS);
    for (int run = 0; run < RUNS; run++) {
        Rng r(1000 + run);
        World w;
        w.build(r, P.nvenues, P.reps);
        auto msgs = generate(w, r, P);
        for (auto& c : acc[run]) c.fill(0);
        tot[run].fill(0);
        for (const auto& m : msgs) {
            tot[run][m.cls]++;
            for (int d = 0; d < NDEF; d++)
                if (DEFS[d](w, m)) acc[run][m.cls][d]++;
        }
    }
    {
        std::ofstream f(dir + "/detection_by_class.csv");
        f << "class,is_attack,defense,mean_rate,min_rate,max_rate,mean_count\n";
        for (int c = 0; c < NCLASS; c++)
            for (int d = 0; d < NDEF; d++) {
                double s = 0, mn = 1, mx = 0, cnt = 0;
                for (int run = 0; run < RUNS; run++) {
                    double rate = tot[run][c] ? (double)(c >= 3 ? tot[run][c] - acc[run][c][d] : acc[run][c][d]) / tot[run][c] : 0;
                    s += rate; mn = std::min(mn, rate); mx = std::max(mx, rate); cnt += tot[run][c];
                }
                // for attacks: rate = detection (rejected); for legit: rate = acceptance
                f << '"' << CLASS_NAMES[c] << "\"," << (c >= 3) << ",\"" << DEF_NAMES[d] << "\"," << std::fixed
                  << std::setprecision(6) << s / RUNS << ',' << mn << ',' << mx << ',' << cnt / RUNS << "\n";
            }
    }
    {
        std::ofstream f(dir + "/overall_metrics.csv");
        f << "defense,run,TP,FN,FP,TN,TPR,FPR,precision,F1,accuracy\n";
        for (int d = 0; d < NDEF; d++)
            for (int run = 0; run < RUNS; run++) {
                long TP = 0, FN = 0, FP = 0, TN = 0;  // positive = fraudulent
                for (int c = 0; c < NCLASS; c++) {
                    long a = acc[run][c][d], t = tot[run][c];
                    if (c >= 3) { TP += t - a; FN += a; } else { TN += a; FP += t - a; }
                }
                double tpr = (double)TP / (TP + FN), fpr = (double)FP / (FP + TN);
                double prec = TP + FP ? (double)TP / (TP + FP) : 0;
                double f1 = prec + tpr ? 2 * prec * tpr / (prec + tpr) : 0;
                double accu = (double)(TP + TN) / (TP + TN + FP + FN);
                f << '"' << DEF_NAMES[d] << "\"," << run << ',' << TP << ',' << FN << ',' << FP << ',' << TN << ','
                  << std::setprecision(6) << tpr << ',' << fpr << ',' << prec << ',' << f1 << ',' << accu << "\n";
            }
    }
    std::cout << "Experiment 1 (detection) done\n";

    // ---------------- Experiment 2: sensitivity of false rejection to operational error rates
    {
        std::ofstream f(dir + "/sensitivity.csv");
        f << "parameter,value,defense,FPR\n";
        double vals[] = {0.0, 0.01, 0.02, 0.03, 0.05, 0.075, 0.10};
        for (int which = 0; which < 2; which++)
            for (double v : vals) {
                Params Q = P;
                if (which == 0) Q.p_forward = v; else Q.p_unregistered = v;
                Q.n_per_attack = 0;
                Rng r(77);
                World w; w.build(r, Q.nvenues, Q.reps);
                auto msgs = generate(w, r, Q);
                for (int d = 0; d < NDEF; d++) {
                    long rej = 0;
                    for (auto& m : msgs) if (!DEFS[d](w, m)) rej++;
                    f << (which == 0 ? "forwarding_rate" : "unregistered_rate") << ',' << v << ",\"" << DEF_NAMES[d]
                      << "\"," << (double)rej / msgs.size() << "\n";
                }
            }
    }
    std::cout << "Experiment 2 (sensitivity) done\n";

    // ---------------- Experiment 3: latency vs registry size, with and without ledger checks
    // Each value is the median over 7 repetitions of a batch of 50,000 operations (batch timing avoids clock overhead).
    {
        std::ofstream f(dir + "/latency_scaling.csv");
        f << "registry_size,operation,median_us,min_us,max_us\n";
        long sizes[] = {1000, 10000, 100000, 1000000};
        const int BATCH = 50000, REP = 7;
        for (long n : sizes) {
            Rng r(5);
            World w; w.build(r, 200, 3);
            std::vector<Message> pool;
            pool.reserve(4096);
            for (long i = 0; i < n; i++) {
                int v = (int)r.below(200), rep = (int)r.below(3), t = (int)r.below(4);
                std::string to = "author" + std::to_string(r.below(5000)) + "@uni.edu";
                std::string body = make_body(r, v, t, w);
                auto vt = w.issue(v, rep, to, t, body, P.ttl, true);
                if (pool.size() < 4096 && r.chance(4096.0 / n + 0.001))
                    pool.push_back(Message{v, w.venues[v].domain, true, to, t, body, vt.first, vt.second, w.clock, 0, false});
            }
            auto stats = [&](std::vector<double>& v, const char* name) {
                std::sort(v.begin(), v.end());
                f << n << ',' << name << ',' << v[v.size() / 2] << ',' << v.front() << ',' << v.back() << "\n";
            };
            // issuance on top of a registry of size n (records added here are removed afterwards)
            {
                std::vector<double> m;
                std::vector<std::string> body(BATCH), to(BATCH);
                std::vector<int> vv(BATCH), tt(BATCH);
                for (int i = 0; i < BATCH; i++) {
                    vv[i] = (int)r.below(200); tt[i] = (int)r.below(4);
                    to[i] = "author" + std::to_string(r.below(5000)) + "@uni.edu";
                    body[i] = make_body(r, vv[i], tt[i], w);
                }
                for (int k = 0; k < REP; k++) {
                    uint64_t base = w.ref_counter;
                    size_t chain0 = w.ledger.chain.size();
                    auto t0 = clk::now();
                    for (int i = 0; i < BATCH; i++) w.issue(vv[i], i % 3, to[i], tt[i], body[i], P.ttl, true);
                    m.push_back(us_since(t0) / BATCH);
                    for (uint64_t q = base + 1; q <= w.ref_counter; q++) w.registry.erase("SAVC-" + std::to_string(q));
                    w.ref_counter = base;
                    w.ledger.chain.resize(chain0);
                }
                stats(m, "issue_with_ledger");
            }
            for (int mode = 0; mode < 2; mode++) {
                std::vector<double> m;
                long okc = 0;
                for (int k = 0; k < REP; k++) {
                    auto t0 = clk::now();
                    for (int i = 0; i < BATCH; i++) {
                        const Message& msg = pool[(size_t)i % pool.size()];
                        bool ok = savc_full(w, msg);
                        if (mode == 1) {  // ledger-backed key check: key must match its registration block, whose hash must be intact
                            const auto& rec = w.registry.at(msg.vvc_ref);
                            const Block& kb = w.ledger.chain[w.key_block[rec.venue][rec.rep]];
                            const Venue& V = w.venues[rec.venue];
                            Digest expect = sha256(V.domain + "#" + std::to_string(rec.rep) + hex(V.reps[rec.rep].key));
                            ok = ok && ct_equal(kb.payload, expect) && ct_equal(block_hash(kb), kb.hash);
                        }
                        okc += ok;
                    }
                    m.push_back(us_since(t0) / BATCH);
                }
                if (okc != (long)BATCH * REP) std::cerr << "unexpected rejection in latency test\n";
                stats(m, mode ? "verify_with_ledger_check" : "verify_registry_only");
            }
            std::cout << "  latency n=" << n << " done\n";
        }
    }
    std::cout << "Experiment 3 (latency) done\n";

    // ---------------- Experiment 4: throughput vs threads
    {
        std::ofstream f(dir + "/throughput.csv");
        f << "threads,verifications_per_sec\n";
        Rng r(9);
        World w; w.build(r, 200, 3);
        std::vector<Message> pool;
        for (int i = 0; i < 100000; i++) {
            int v = (int)r.below(200), rep = (int)r.below(3), t = (int)r.below(4);
            std::string to = "author" + std::to_string(r.below(5000)) + "@uni.edu";
            std::string body = make_body(r, v, t, w);
            auto vt = w.issue(v, rep, to, t, body, P.ttl, false);
            pool.push_back(Message{v, w.venues[v].domain, true, to, t, body, vt.first, vt.second, w.clock, 0, false});
        }
        unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        std::vector<unsigned> counts;
        for (unsigned th = 1; th < hw; th *= 2) counts.push_back(th);
        counts.push_back(hw);
        for (unsigned th : counts) {
            std::atomic<long> done{0};
            const long PER = 200000;
            auto t0 = clk::now();
            std::vector<std::thread> T;
            for (unsigned k = 0; k < th; k++)
                T.emplace_back([&, k]() {
                    long c = 0;
                    for (long i = 0; i < PER; i++) c += savc_full(w, pool[(i * 7 + k * 13) % pool.size()]);
                    done += c;
                });
            for (auto& t : T) t.join();
            double sec = us_since(t0) / 1e6;
            f << th << ',' << (PER * th) / sec << "\n";
        }
    }
    std::cout << "Experiment 4 (throughput) done\n";

    // ---------------- Experiment 5: ledger validation cost and tamper detection
    {
        std::ofstream f(dir + "/ledger.csv");
        f << "chain_length,append_us_per_block,full_validation_ms\n";
        long lens[] = {1000, 10000, 100000, 1000000};
        for (long n : lens) {
            Ledger L;
            Rng r(3);
            auto t0 = clk::now();
            for (long i = 0; i < n; i++) L.append(Ev::VvcIssue, random_key(r), 1700000000 + i);
            double app = us_since(t0) / n;
            auto t1 = clk::now();
            long long bad = L.validate();
            double val = us_since(t1) / 1000.0;
            f << n << ',' << app << ',' << val << (bad == -1 ? "" : ",INVALID") << "\n";
        }
        std::ofstream g(dir + "/tamper.csv");
        g << "trials,modification,detected,correctly_localized\n";
        Ledger L; Rng r(4);
        for (long i = 0; i < 100000; i++) L.append(Ev::VvcIssue, random_key(r), 1700000000 + i);
        const char* kinds[] = {"payload_bitflip", "timestamp_change", "block_deletion", "block_reorder"};
        for (int k = 0; k < 4; k++) {
            int det = 0, loc = 0; const int TR = 1000;
            for (int t = 0; t < TR; t++) {
                Ledger X = L;
                size_t pos = 1 + (size_t)r.below(X.chain.size() - 2);
                if (k == 0) X.chain[pos].payload[r.below(32)] ^= (uint8_t)(1u << r.below(8));
                if (k == 1) X.chain[pos].ts += 1 + (int64_t)r.below(1000);
                if (k == 2) X.chain.erase(X.chain.begin() + pos);
                if (k == 3) std::swap(X.chain[pos], X.chain[pos + 1]);
                long long bad = X.validate();
                if (bad != -1) det++;
                if (bad == (long long)pos) loc++;
            }
            g << TR << ',' << kinds[k] << ',' << det << ',' << loc << "\n";
        }
    }
    std::cout << "Experiment 5 (ledger) done\n";

    // ---------------- Brute-force analysis (analytical, written for reference)
    {
        std::ofstream f(dir + "/bruteforce.csv");
        f << "tag_bits,attempts_per_sec,expected_years_to_50pct\n";
        int bits[] = {16, 32, 64, 128, 256};
        for (int b : bits) {
            double aps = 1e9;  // generous offline guessing rate; online guessing is further rate-limited
            double attempts = std::pow(2.0, b - 1);
            f << b << ',' << aps << ',' << attempts / aps / (365.25 * 86400) << "\n";
        }
    }
    std::cout << "All experiments finished. Results written to " << dir << "/\n";
    return 0;
}
