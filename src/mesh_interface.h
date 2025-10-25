#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <queue>
#include "meshtastic/mesh.pb.h"

// Constants
static BLEUUID SERVICE_UUID("6ba1b218-15a8-461f-9fa8-5dcae273eafd");
static BLEUUID FROMRADIO_UUID("2c55e69e-4993-11ed-b878-0242ac120002");
static BLEUUID TORADIO_UUID("f75c76d2-129e-4dad-a1dd-7866124401e7");
static BLEUUID FROMNUM_UUID("ed9da18c-a800-4f66-a670-aa7547e34453");
static BLEUUID LOG_UUID("5a3d6e49-06e6-4423-9944-e9de8cdf9547");

extern std::queue<std::vector<uint8_t>> receivedRawMessages;

// Callback class for BLE client
class MeshClientCallback : public BLEClientCallbacks {
public:
    void onConnect(BLEClient* pclient) override;
    void onDisconnect(BLEClient* pclient) override;
};

// Callback class for BLE advertised devices
class MeshAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override;
};

namespace MeshInterface {
    // Structure to hold decoded mesh packet information
    struct MeshPacket {
        int fromAddress;
        char text[244];
        float rxSnr;
        float rxRssi;
        char error[10];
    };

    // Initialize BLE and start scanning
    void setupBLE(const char* UnitAddress);
    // Check if there are received messages
    bool hasReceivedMessage();
    // Pops and returns; check hasReceivedMessage() first
    std::vector<uint8_t> getNextReceivedMessage();
    // Decoder for FromRadio/MeshPacket messages
    MeshPacket decodeMeshPacket(const std::vector<unsigned char>& rawMessage);
    // Send a response: Wraps payload into ToRadio/MeshPacket and sends
    void sendMessage(int to, const char* message);
    // Main loop to handle connection and reading
    void loop();
}