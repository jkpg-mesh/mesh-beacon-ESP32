#include "mesh_interface.h"
#include <esp_random.h>

// Global variable definitions
BLEAddress radioMacAddress("00:00:00:00:00:00");
BLERemoteService* pRemoteService = nullptr;
BLERemoteCharacteristic* pFromRadioCharacteristic = nullptr;
BLERemoteCharacteristic* pToRadioCharacteristic = nullptr;
BLERemoteCharacteristic* pFromNumCharacteristic = nullptr;
BLEAdvertisedDevice* ConnectedDevice = nullptr;

volatile bool doConnect = true;
volatile bool doReadFromRadio = false;
bool connected = false;
bool sentConfigRequest = false;
bool initialDownloadDone = false;

// MeshPacket structure definition
struct MeshPacket {
    int fromAddress = 0;
    char text[244] = {0};
    float rxSnr = 0.0f;
    float rxRssi = 0.0f;
    char error[10] = {0};
};

std::queue<std::vector<uint8_t>> receivedRawMessages;

// MeshClientCallback implementations
void MeshClientCallback::onConnect(BLEClient* pclient) {
    Serial0.println("BLEClientCallback - onConnect");
}

void MeshClientCallback::onDisconnect(BLEClient* pclient) {
    Serial0.println("BLEClientCallback - onDisconnect");
    connected = false;
    doConnect = true;
    delete ConnectedDevice;
    ConnectedDevice = nullptr;
}

// MeshAdvertisedDeviceCallbacks implementation
void MeshAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial0.print("BLE Advertised Device found: ");
    Serial0.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.getAddress().equals(radioMacAddress) &&
        advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(SERVICE_UUID)) {
        BLEDevice::getScan()->stop();
        ConnectedDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = false;
    }
}

// Helper function implementations
void logCharacteristicProperties(BLERemoteCharacteristic* pChar, const char* name) {
    Serial0.printf("%s properties: ", name);
    if (pChar->canRead()) Serial0.print("Read ");
    if (pChar->canWrite()) Serial0.print("Write ");
    if (pChar->canNotify()) Serial0.print("Notify ");
    if (pChar->canIndicate()) Serial0.print("Indicate ");
    Serial0.println();
}

// Notification callback implementation
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial0.print("Notify callback for characteristic ");
    Serial0.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial0.print(" of data length ");
    Serial0.println(length);

    if (pBLERemoteCharacteristic->getUUID().equals(FROMNUM_UUID)) {
        doReadFromRadio = true;
        Serial0.println("FROMNUM notification received - Flag set for loop() read.");
    }
}

// Function to send config request
void sendConfigRequest(){
    delay(500);  // Small delay to ensure connection is stable
    
    meshtastic_ToRadio msg = meshtastic_ToRadio_init_zero;
    msg.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    uint32_t RanValue = esp_random();
    msg.want_config_id = RanValue;  // Or use a random nonce for uniqueness
    
    uint8_t buffer[256];  // Increased buffer size
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    if (pb_encode(&stream, meshtastic_ToRadio_fields, &msg)) {
    size_t len = stream.bytes_written;
    Serial0.printf("Sending want_config_id protobuf (%d bytes)\n", len);
    Serial0.print("Encoded data (hex): ");
    for (size_t i = 0; i < len; i++) {
        Serial0.printf("%02X ", buffer[i]);
    }
    Serial0.println();
    
    pToRadioCharacteristic->writeValue(buffer, len, false);
    sentConfigRequest = true;
    Serial0.println("Config request sent successfully");
    } else {
    Serial0.println("Protobuf encode failed");
    }
    doReadFromRadio = true;
}

// Function to read FromRadio until empty packet received
void readFromRadioUntilEmpty() {
    bool done = false;
    int maxRetries = 100;
    int retryDelayMs = 500;

    Serial0.println("Attempting to read FromRadio...");
    for (int attempt = 1; attempt <= maxRetries && !done; attempt++) {
        Serial0.printf("Read attempt %d/%d\n", attempt, maxRetries);
        std::string value = pFromRadioCharacteristic->readValue();

        if (value.length() == 0) {
            Serial0.println("Received empty packet");
            done = true;
        } else {
            // Optional debug: Print length and hex
            Serial0.printf("Received FromRadio packet of length %d\n", value.length());
            
            // Push raw to queue if it is a MeshPacket
            meshtastic_FromRadio fr = meshtastic_FromRadio_init_zero;
            pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t*)value.data(), value.length());

            if (pb_decode(&stream, meshtastic_FromRadio_fields, &fr)) {
                if (fr.which_payload_variant == meshtastic_FromRadio_packet_tag){
                    Serial0.println("Type: MeshPacket");
                    Serial0.printf("Portnum: %d\n", fr.packet.decoded.portnum);
                    // look at portnum and push raw if desired, this needs to b adjusted as needed
                    switch (fr.packet.decoded.portnum) {
                        case 1: // TEXT_MESSAGE_APP
                            Serial0.println("Text Message App packet received");
                            {
                                std::vector<uint8_t> raw(value.begin(), value.end());
                                receivedRawMessages.push(raw);
                            }
                            break;
                        default:
                            Serial0.println("Other packet type received");
                            break;
                    }
                }
            } else {
                Serial0.println("Failed to decode FromRadio");
            }
        }
        if (!done) {
            delay(retryDelayMs);
        }
    }
    if (!done) {
        Serial0.println("Warning: Max retries reached, download may be incomplete");
    } else {
        Serial0.println("Download complete");
    }
    doReadFromRadio = false;
}

// Function to connect to server and setup characteristics
bool connectToServer() {
    Serial0.print("Forming a connection to ");
    Serial0.println(ConnectedDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial0.println(" - Created client");
    pClient->setClientCallbacks(new MeshClientCallback());
    
    // Connect to the remote BLE Server.
    pClient->connect(ConnectedDevice);
    pClient->setMTU(512);  // Recommended for Meshtastic
    Serial0.printf("Negotiated MTU: %d\n", pClient->getMTU());
    Serial0.println(" - Connected to server");
    
    // Obtain a reference to the service we are after in the remote BLE server.
    pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
    Serial0.print("Failed to find our service UUID: ");
    Serial0.println(SERVICE_UUID.toString().c_str());
    pClient->disconnect();
    connected = false;
    return false;
    }
    Serial0.println(" - Found our service");

    // Obtain FromRadio characteristic
    pFromRadioCharacteristic = pRemoteService->getCharacteristic(FROMRADIO_UUID);
    if (pFromRadioCharacteristic == nullptr) {
    Serial0.print("Failed to find FromRadio UUID: ");
    Serial0.println(FROMRADIO_UUID.toString().c_str());
    pClient->disconnect();
    connected = false;
    return false;
    }
    Serial0.println(" - Found FromRadio characteristic");
    logCharacteristicProperties(pFromRadioCharacteristic, "FromRadio");

    // Obtain ToRadio characteristic
    pToRadioCharacteristic = pRemoteService->getCharacteristic(TORADIO_UUID);
    if (pToRadioCharacteristic == nullptr) {
    Serial0.print("Failed to find ToRadio UUID: ");
    Serial0.println(TORADIO_UUID.toString().c_str());
    pClient->disconnect();
    connected = false;
    return false;
    }
    Serial0.println(" - Found ToRadio characteristic");
    logCharacteristicProperties(pToRadioCharacteristic, "ToRadio");

    // Obtain FromNum characteristic
    pFromNumCharacteristic = pRemoteService->getCharacteristic(FROMNUM_UUID);
    if (pFromNumCharacteristic == nullptr) {
    Serial0.print("Failed to find FromNum UUID: ");
    Serial0.println(FROMNUM_UUID.toString().c_str());
    pClient->disconnect();
    connected = false;
    return false;
    }
    Serial0.println(" - Found FromNum characteristic");
    logCharacteristicProperties(pFromNumCharacteristic, "FromNum");

    connected = true;
    return true;
}

// Function to check and handle messages
void checkMessages(){
    if (!initialDownloadDone) {
        Serial0.println("Starting initial download...");
        readFromRadioUntilEmpty();
        initialDownloadDone = true;
        
        // Now safely register for notifications after initial download
        if (pFromNumCharacteristic && pFromNumCharacteristic->canNotify()) {
            pFromNumCharacteristic->registerForNotify(notifyCallback);
            Serial0.println("FromNum notify registered");
        }
        doReadFromRadio = false;
    } else {
        Serial0.println("Reading from radio due to notification...");
        readFromRadioUntilEmpty();
        doReadFromRadio = false;
    }
}

namespace MeshInterface {
    // Initialize BLE and start scanning
    void setupBLE(const char* UnitAddress="") {
        if (UnitAddress != ""){
            radioMacAddress = BLEAddress(UnitAddress);
        }
        BLEDevice::init("");
        Serial0.println("Start scanning...");
        
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MeshAdvertisedDeviceCallbacks());
        pBLEScan->setInterval(1349);
        pBLEScan->setWindow(449);
        pBLEScan->setActiveScan(true);
        pBLEScan->start(5,true);
    }
    // Check if there are received messages
    bool hasReceivedMessage() {
        return !receivedRawMessages.empty();
    }
    // Pops and returns; check hasReceivedMessage() first
    std::vector<uint8_t> getNextReceivedMessage() {
        if (receivedRawMessages.empty()) {
            return {};  // Empty vector if nothing
        }
        std::vector<uint8_t> msg = receivedRawMessages.front();
        receivedRawMessages.pop();
        return msg;
    } 
    // Decoder for FromRadio/MeshPacket messages
    MeshPacket decodeMeshPacket(const std::vector<uint8_t>& rawMessage) {
        MeshPacket result = {};

        meshtastic_FromRadio fr = meshtastic_FromRadio_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(
            reinterpret_cast<const pb_byte_t*>(rawMessage.data()),
            rawMessage.size()
        );

        if (!pb_decode(&stream, meshtastic_FromRadio_fields, &fr)) {
            Serial0.println("Failed to decode FromRadio");
            strcpy(result.error,"NOK_DEC");
            return result;
        }

        if (fr.which_payload_variant != meshtastic_FromRadio_packet_tag) {
            Serial0.println("No valid packet payload");
            strcpy(result.error,"NOK_PAYL");
            return result;
        }

        auto &packet = fr.packet;
        auto &data = packet.decoded;

        if (data.payload.size == 0) {
            Serial0.println("Empty payload");
            strcpy(result.error, "EMPTY");
            return result;
        }

        // --- Basic packet info ---
        result.fromAddress = packet.from;
        result.rxRssi = packet.rx_rssi;
        result.rxSnr = packet.rx_snr;

        // --- Decode based on portnum ---
        switch (packet.decoded.portnum) {
            case meshtastic_PortNum_TEXT_MESSAGE_APP: {
                // payload is bytes; convert to C string
                size_t len = data.payload.size;
                // ensure null-termination
                char buf[256];
                size_t bufCopyLen = (len < sizeof(buf)-1 ? len : sizeof(buf)-1);
                memcpy(buf, data.payload.bytes, bufCopyLen);
                buf[bufCopyLen] = '\0';
                // now buf is the text message
                Serial0.print("Decoded text: ");
                Serial0.println(buf);
                size_t resultCopyLen = std::min<size_t>(data.payload.size, sizeof(result.text) - 1);
                memcpy(result.text, data.payload.bytes, resultCopyLen);
                result.text[resultCopyLen] = '\0';
                break;
            }
            default:
                Serial0.print("Unhandled portnum: ");
                Serial0.println(packet.decoded.portnum);
                break;
        }
        return result;
    }    
    // Send a response: Wraps payload into ToRadio/MeshPacket and sends
    void sendMessage(int to, const char* message) {
        if (!connected || !pToRadioCharacteristic) {
            Serial0.println("Cannot send: Not connected");
            return;
        }

        // Prepare ToRadio protobuf structure
        meshtastic_ToRadio tr = meshtastic_ToRadio_init_zero;
        tr.which_payload_variant = meshtastic_ToRadio_packet_tag;

        meshtastic_MeshPacket& mp = tr.packet;
        mp.to = to;
        mp.from = 0;  // Could use your node ID or device address if known
        mp.id = esp_random();  // Unique random ID
        mp.channel = 0;  // Default channel
        mp.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        mp.which_payload_variant = meshtastic_MeshPacket_decoded_tag;

        // Copy message string into payload
        size_t msg_len = strlen(message);
        if (msg_len > sizeof(mp.decoded.payload.bytes)) {
            Serial0.println("Message too long, truncating");
            msg_len = sizeof(mp.decoded.payload.bytes);
        }

        memcpy(mp.decoded.payload.bytes, message, msg_len);
        mp.decoded.payload.size = msg_len;

        // Encode into protobuf buffer
        uint8_t buffer[256];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        if (pb_encode(&stream, meshtastic_ToRadio_fields, &tr)) {
            size_t len = stream.bytes_written;
            Serial0.printf("Sending ToRadio (%d bytes): %s\n", (int)len, message);
            pToRadioCharacteristic->writeValue(buffer, len, false);
        } else {
            Serial0.println("Failed to encode ToRadio");
        }
    }
    // Main loop to handle connection and reading 
    void loop(){
        if (!connected && !doConnect) connectToServer();
        if (!sentConfigRequest) sendConfigRequest();
        if (doReadFromRadio) checkMessages();
        if (!connected && doConnect) setupBLE();
    }
}