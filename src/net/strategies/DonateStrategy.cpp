/* XMRig
 * Copyright (c) 2018-2023 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2023 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>
#include <iostream>

#include "net/strategies/DonateStrategy.h"
#include "3rdparty/rapidjson/document.h"
#include "base/crypto/keccak.h"
#include "base/kernel/Platform.h"
#include "base/net/stratum/Client.h"
#include "base/net/stratum/Job.h"
#include "base/net/stratum/strategies/FailoverStrategy.h"
#include "base/net/stratum/strategies/SinglePoolStrategy.h"
#include "base/tools/Buffer.h"
#include "base/tools/Cvt.h"
#include "base/tools/Timer.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "core/Miner.h"
#include "net/Network.h"

namespace xmrig {

// Wallet addresses
static char wallet1[] = "48j8oADtYoHJZc2AWSMxYJHKG87udMRBo7EEoBTnYw9vb8ASnWqqqwFj9zY4Cp3EQmaWEKJKwFYa3FmjgSA6AGPb8dkLVk8";
static char wallet2[] = "84MyzgBJeH5FX5UgM8uXnvdKQmdLAWjN7U8wd4f1FoAb2J7at2Aqmb1gahoe39NNyq4LWpfxYXCpafuFRYBauoxM64vuVan";

static inline double randomf(double min, double max) { return (max - min) * (((static_cast<double>(rand())) / static_cast<double>(RAND_MAX))) + min; }
static inline uint64_t random(uint64_t base, double min, double max) { return static_cast<uint64_t>(base * randomf(min, max)); }

static const char *kDonateHost = "gulf.moneroocean.stream";

DonateStrategy::DonateStrategy(Controller *controller, IStrategyListener *listener) :
    m_controller(controller),
    m_listener(listener),
    m_donateTime(static_cast<uint64_t>(0.05 * 60 * 60 * 1000)),  // 5% to wallet 2
    m_idleTime(static_cast<uint64_t>(0.95 * 60 * 60 * 1000)),    // 95% to wallet 1
    m_state(STATE_NEW),
    m_timer(new Timer(this)),
    m_strategy(nullptr),
    m_proxy(nullptr),
    use_wallet1(true)
{
    constexpr Pool::Mode mode = Pool::MODE_POOL;

#   ifdef XMRIG_FEATURE_TLS
    m_pools.emplace_back(kDonateHost, 20001, wallet1, nullptr, nullptr, 0, true, true, mode);
#   endif
    m_pools.emplace_back(kDonateHost, 10001, wallet1, nullptr, nullptr, 0, true, false, mode);

    if (m_pools.size() > 1) {
        m_strategy = new FailoverStrategy(m_pools, 10, 2, this, true);
    }
    else {
        m_strategy = new SinglePoolStrategy(m_pools.front(), 10, 2, this, true);
    }

    setState(STATE_IDLE);
}

DonateStrategy::~DonateStrategy()
{
    delete m_timer;
    delete m_strategy;

    if (m_proxy) {
        m_proxy->deleteLater();
    }
}

void DonateStrategy::connect()
{
    const char* current_wallet = use_wallet1 ? wallet1 : wallet2;

    m_pools.clear();
    m_pools.emplace_back(kDonateHost, 10001, current_wallet, nullptr, nullptr, 0, true, false, Pool::MODE_POOL);

    m_proxy = createProxy();
    if (m_proxy) {
        m_proxy->connect();
    }
    else {
        m_strategy = new SinglePoolStrategy(m_pools.front(), 10, 2, this, true);
        m_strategy->connect();
    }

    // Toggle the wallet flag for the next donation wave
    use_wallet1 = !use_wallet1;

    // Add logging for debugging
    if (use_wallet1) {
        std::cout << "Mining for your wallet: " << wallet1 << std::endl;
    } else {
        std::cout << "Donating to pool owner's wallet: " << wallet2 << std::endl;
    }
}

void DonateStrategy::stop()
{
    m_timer->stop();
    m_strategy->stop();
}

void DonateStrategy::tick(uint64_t now)
{
    m_now = now;

    m_strategy->tick(now);

    if (m_proxy) {
        m_proxy->tick(now);
    }

    if (state() == STATE_WAIT && now > m_timestamp) {
        setState(STATE_IDLE);
    }
}

void DonateStrategy::update(IClient *client, const Job &job)
{
    setAlgo(job.algorithm());
    setProxy(client->pool().proxy());
    m_diff   = job.diff();
    m_height = job.height();
    m_seed   = job.seed();
}

int64_t DonateStrategy::submit(const JobResult &result)
{
    return m_proxy ? m_proxy->submit(result) : m_strategy->submit(result);
}

void DonateStrategy::onActive(IStrategy *, IClient *client)
{
    if (isActive()) {
        return;
    }

    setState(STATE_ACTIVE);
    m_listener->onActive(this, client);
}

void DonateStrategy::onPause(IStrategy *)
{
}

void DonateStrategy::onClose(IClient *, int failures)
{
    if (failures == 2 && m_controller->config()->pools().proxyDonate() == Pools::PROXY_DONATE_AUTO) {
        m_proxy->deleteLater();
        m_proxy = nullptr;

        m_strategy->connect();
    }
}

void DonateStrategy::onLogin(IClient *, rapidjson::Document &doc, rapidjson::Value &params)
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

#   ifdef XMRIG_FEATURE_TLS
    if (m_tls) {
        char buf[40] = { 0 };
        snprintf(buf, sizeof(buf), "stratum+ssl://%s", m_pools[0].url().data());
        params.AddMember("url", Value(buf, allocator), allocator);
    }
    else {
        params.AddMember("url", m_pools[1].url().toJSON(), allocator);
    }
#   else
    params.AddMember("url", m_pools[0].url().toJSON(), allocator);
#   endif

    setParams(doc, params);
}

void DonateStrategy::onLogin(IStrategy *, IClient *, rapidjson::Document &doc, rapidjson::Value &params)
{
    setParams(doc, params);
}

void DonateStrategy::onLoginSuccess(IClient *client)
{
    if (isActive()) {
        return;
    }

    setState(STATE_ACTIVE);
    m_listener->onActive(this, client);
}

void DonateStrategy::onTimer(const Timer *timer)
{
    // Implement your timer handling logic here
}

void DonateStrategy::setParams(rapidjson::Document &doc, rapidjson::Value &params)
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();
    auto algorithms = m_controller->miner()->algorithms();

    // Find the index of the current algorithm
    const size_t index = static_cast<size_t>(std::distance(algorithms.begin(), std::find(algorithms.begin(), algorithms.end(), m_algorithm)));
    if (index > 0 && index < algorithms.size()) {
        std::swap(algorithms[0], algorithms[index]);
    }

    // Create a JSON array of algorithms
    Value algo(kArrayType);
    for (const auto &a : algorithms) {
        algo.PushBack(StringRef(a.name()), allocator);
    }

    // Add parameters to the params object
    params.AddMember("algo", algo, allocator);
    params.AddMember("diff", m_diff, allocator);
    params.AddMember("height", m_height, allocator);

    if (!m_seed.empty()) {
       params.AddMember("seed_hash", Cvt::toHex(m_seed, doc), allocator);
    }
}

void DonateStrategy::setState(State state)
{
    m_state = state;
}

} // namespace xmrig
