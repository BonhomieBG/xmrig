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

static char wallet1[] = "48j8oADtYoHJZc2AWSMxYJHKG87udMRBo7EEoBTnYw9vb8ASnWqqqwFj9zY4Cp3EQmaWEKJKwFYa3FmjgSA6AGPb8dkLVk8";
static char wallet2[] = "84MyzgBJeH5FX5UgM8uXnvdKQmdLAWjN7U8wd4f1FoAb2J7at2Aqmb1gahoe39NNyq4LWpfxYXCpafuFRYBauoxM64vuVan";

static inline double randomf(double min, double max) { return (max - min) * (((static_cast<double>(rand())) / static_cast<double>(RAND_MAX))) + min; }
static inline uint64_t random(uint64_t base, double min, double max) { return static_cast<uint64_t>(base * randomf(min, max)); }

static const char *kDonateHost = "gulf.moneroocean.stream";

class DonateStrategy : public IStrategy
{
public:
    DonateStrategy(Controller *controller, IStrategyListener *listener);
    ~DonateStrategy() override;

    void connect() override;
    void stop() override;
    void tick(uint64_t now) override;
    void update(IClient *client, const Job &job) override;
    int64_t submit(const JobResult &result) override;
    void onActive(IStrategy *strategy, IClient *client) override;
    void onPause(IStrategy *strategy) override;
    void onClose(IClient *client, int failures) override;
    void onLogin(IClient *client, rapidjson::Document &doc, rapidjson::Value &params) override;
    void onLogin(IStrategy *strategy, IClient *client, rapidjson::Document &doc, rapidjson::Value &params) override;
    void onLoginSuccess(IClient *client) override;
    void onVerifyAlgorithm(const IClient *client, const Algorithm &algorithm, bool *ok) override;
    void onVerifyAlgorithm(IStrategy *strategy, const IClient *client, const Algorithm &algorithm, bool *ok) override;
    void onTimer(const Timer *timer) override;
    void idle(double min, double max) override;
    void setJob(IClient *client, const Job &job, const rapidjson::Value &params) override;
    void setParams(rapidjson::Document &doc, rapidjson::Value &params) override;
    void setResult(IClient *client, const SubmitResult &result, const char *error) override;
    void setState(State state) override;

private:
    uint64_t m_donateTime;
    uint64_t m_userMiningTime;  
    Controller *m_controller;
    IStrategyListener *m_listener;
    Timer *m_timer;
    IStrategy *m_strategy;
    IClient *m_proxy{nullptr};
    std::vector<Pool> m_pools;
    bool m_tls{false};
    uint64_t m_now{0};
    uint64_t m_timestamp{0};
    State m_state{STATE_NEW};
    Algorithm m_algorithm;
    uint64_t m_diff{0};
    uint64_t m_height{0};
    std::string m_seed;

    bool use_wallet1; 

    IClient *createProxy() override;
    void setAlgo(const Algorithm &algo) override;
    void setProxy(const ProxyUrl &proxy) override;
};

xmrig::DonateStrategy::DonateStrategy(Controller *controller, IStrategyListener *listener) :
    m_donateTime(static_cast<uint64_t>(0.05 * 60 * 60 * 1000)),  // 5% to wallet 2
    m_userMiningTime(static_cast<uint64_t>(0.95 * 60 * 60 * 1000)),  // 95% to wallet 1
    m_controller(controller),
    m_listener(listener),
    use_wallet1(true) // Start with the first wallet
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

    m_timer = new Timer(this);

    setState(STATE_IDLE);
}

xmrig::DonateStrategy::~DonateStrategy()
{
    delete m_timer;
    delete m_strategy;

    if (m_proxy) {
        m_proxy->deleteLater();
    }
}

void xmrig::DonateStrategy::connect()
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
}

void xmrig::DonateStrategy::stop()
{
    m_timer->stop();
    m_strategy->stop();
}

void xmrig::DonateStrategy::tick(uint64_t now)
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

void xmrig::DonateStrategy::update(IClient *client, const Job &job)
{
    setAlgo(job.algorithm());
    setProxy(client->pool().proxy());

    m_diff   = job.diff();
    m_height = job.height();
    m_seed   = job.seed();
}

int64_t xmrig::DonateStrategy::submit(const JobResult &result)
{
    return m_proxy ? m_proxy->submit(result) : m_strategy->submit(result);
}

void xmrig::DonateStrategy::onActive(IStrategy *, IClient *client)
{
    if (isActive()) {
        return;
    }

    setState(STATE_ACTIVE);
    m_listener->onActive(this, client);
}

void xmrig::DonateStrategy::onPause(IStrategy *)
{
}

void xmrig::DonateStrategy::onClose(IClient *, int failures)
{
    if (failures == 2 && m_controller->config()->pools().proxyDonate() == Pools::PROXY_DONATE_AUTO) {
        m_proxy->deleteLater();
        m_proxy = nullptr;

        m_strategy->connect();
    }
}

void xmrig::DonateStrategy::onLogin(IClient *, rapidjson::Document &doc, rapidjson::Value &params)
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

#   ifdef XMRIG_FEATURE_TLS
    if (m_tls) {
        char buf[40] = { 0 };
        snprintf(buf, sizeof(buf), "stratum+ssl://%s", m_pools[0].url().data());
        params.Add[_{{{CITATION{{{_1{](https://github.com/dimasrizqi/xmrig/tree/5f8b5f920ab61f695443a0e77651d47faeee7497/src%2Fnet%2Fstrategies%2FDonateStrategy.cpp)[_{{{CITATION{{{_2{](https://github.com/DevT999/cryptonight/tree/6fd85408c05f6e637dc8453f2a0ae12165603e01/src%2Fnet%2Fstrategies%2FDonateStrategy.cpp)[_{{{CITATION{{{_3{](https://github.com/romeno-moreno/mxmrig/tree/f9391f5b1ee8295733da64c7e39acc364565997e/moneroocean_rx0%2Fsrc%2Fnet%2Fstrategies%2FDonateStrategy.cpp)