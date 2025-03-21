#pragma once

#include <map>
#include <set>

#include "ProtobufModule.h"

/**
 * NodeInfo module for sending/receiving NodeInfos into the mesh
 */
class BandwidthTestModule : public ProtobufModule<meshtastic_BandwidthTestMessage>, private concurrency::OSThread
{
  const uint64_t WINDOW_LENGTH = 10000;

  public:
    BandwidthTestModule();

    void sendBandwidthTestProbe(NodeNum dest = NODENUM_BROADCAST, uint8_t channel = 0);
    void sendBandwidthTestResult(uint64_t start, uint64_t end, uint32_t count, NodeNum dest = NODENUM_BROADCAST, uint8_t channel = 0);

  protected:
    /** Called to handle a particular incoming message

    @return true if you've guaranteed you've handled this message and no other handlers should be considered for it
    */
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_BandwidthTestMessage *p) override;

    /** Does our periodic broadcast */
    virtual int32_t runOnce() override;

    #if HAS_SCREEN
    virtual bool wantUIFrame() override { return true; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    #endif

  private:
    uint64_t totalPacketCount = 0;
    float latestBandwidth = 0;
    bool firstFrame = true;

    std::map<uint32_t, std::set<uint32_t>> recievedPackets;
    uint64_t windowStart = 0;

};

extern BandwidthTestModule *bandwidthTestModule;
