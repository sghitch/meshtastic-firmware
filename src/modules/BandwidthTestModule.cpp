#include "BandwidthTestModule.h"
#include "Default.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "configuration.h"
#include "main.h"
#include <Throttle.h>

BandwidthTestModule *bandwidthTestModule;

bool BandwidthTestModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_BandwidthTestMessage *pptr)
{
    auto p = *pptr;

    if (p.which_test_variant == meshtastic_BandwidthTestMessage_test_probe_tag) {
        
        LOG_DEBUG("Recieved probe from %d", mp.from);
        if (recievedPackets.find(mp.from) != recievedPackets.end()) {
            recievedPackets[mp.from].insert(mp.id);
        } else {
            recievedPackets.emplace(mp.from, std::set<uint32_t>());
        }
        const uint64_t currentTime = millis();
        if (currentTime - windowStart > WINDOW_LENGTH) {
            for (auto const& sender : recievedPackets) {
                sendBandwidthTestResult(windowStart, currentTime, sender.second.size(), sender.first);
            }

            recievedPackets.clear();
            windowStart = currentTime;
        }
    } else {
        float duration = (float)(p.test_variant.test_result.end - p.test_variant.test_result.start);
        latestBandwidth = p.test_variant.test_result.count / duration;
        LOG_INFO("New bandwidth recieved: %f", latestBandwidth);
        String lcd = String("Recieved New Bandwidth: ") + latestBandwidth +"\n";
        if (screen)
            screen->print(lcd.c_str());
    }

    return false; // Let others look at this message also if they want
}

void BandwidthTestModule::sendBandwidthTestProbe(NodeNum dest, uint8_t channel)
{
    meshtastic_BandwidthTestMessage u;
    u.test_variant.test_probe.timestamp = millis();

    meshtastic_MeshPacket* p = allocDataProtobuf(u);
    if (p) { // Check whether we didn't ignore it
        p->to = dest;
        p->decoded.want_response = false;
        p->priority = meshtastic_MeshPacket_Priority_HIGH;
        if (channel > 0) {
            LOG_DEBUG("Send our bandwidth test to channel %d", channel);
            p->channel = channel;
        }
        
        service->sendToMesh(p, RX_SRC_LOCAL, false);
    }
}

void BandwidthTestModule::sendBandwidthTestResult(uint64_t start, uint64_t end, uint32_t count, NodeNum dest, uint8_t channel)
{
    meshtastic_BandwidthTestMessage u;
    u.test_variant.test_result.count = count;
    u.test_variant.test_result.start = start;
    u.test_variant.test_result.end = end;

    meshtastic_MeshPacket* p = allocDataProtobuf(u);
    if (p) { // Check whether we didn't ignore it
        p->to = dest;
        p->decoded.want_response = false;
        p->priority = meshtastic_MeshPacket_Priority_HIGH;
        if (channel > 0) {
            LOG_DEBUG("Send our bandwidth result to channel %d", channel);
            p->channel = channel;
        }
        
        service->sendToMesh(p, RX_SRC_LOCAL, false);
    }
}

BandwidthTestModule::BandwidthTestModule()
    : ProtobufModule("bandwidthTestModule", meshtastic_PortNum_REPLY_APP, &meshtastic_User_msg), concurrency::OSThread("BandwidthTestModule")
{
    LOG_INFO("Hello from Bandwidth Module");
    isPromiscuous = true; // We always want to update our nodedb, even if we are sniffing on others

    setIntervalFromNow(setStartDelay()); // Send our initial owner announcement 30 seconds
                                         // after we start (to give network time to setup)
}

int32_t BandwidthTestModule::runOnce()
{
    const bool enabled = moduleConfig.bandwidth.enabled;

    auto queueSize = router->getQueueStatus().maxlen - router->getQueueStatus().free;
    if (enabled && queueSize > 1 && config.device.role != meshtastic_Config_DeviceConfig_Role_ROUTER) {
        LOG_TRACE("Send our bandwidth test to mesh");
        sendBandwidthTestProbe(NODENUM_BROADCAST); // Send our info (don't request replies)
        totalPacketCount++;
    }
    
    return Default::getConfiguredOrDefault(moduleConfig.bandwidth.update_interval, 100);
}

#if HAS_SCREEN

#include "graphics/ScreenFonts.h"

void BandwidthTestModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    char buffer[50];
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    display->drawString(x + 0, y + 0, "Bandwidth");

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);
    display->drawStringf(display->getWidth() / 2 + x, 0 + y + 12, buffer, "Packets: %d\nData Rate: %f\nuptime: %ds",
                         totalPacketCount, latestBandwidth, millis() / 1000);

                
}
#endif // HAS_SCREEN