#include <Arduino.h>
#include "mesh_interface.h"
#include "command_handler.h"

MeshInterface::MeshPacket lastPacket;
CommandHandler::CommandResponse lastResponse;

void setup() {
    Serial0.begin(115200);
    Serial0.println("Starting Beacon BLE Client application...");
    delay(5000);
    Serial0.println("Start scanning for Meshtastic unit...");
    MeshInterface::setupBLE(RADIO_MAC_STR);
}

void loop() {
    // Handle incoming mesh messages
    MeshInterface::loop();
    // Check for received messages
    if (MeshInterface::hasReceivedMessage()){
        Serial0.println("Received a message");
        lastPacket = MeshInterface::decodeMeshPacket(MeshInterface::getNextReceivedMessage());
        lastResponse = CommandHandler::respondToCommand(lastPacket.text, lastPacket.rxSnr, lastPacket.rxRssi);
        
        if (lastResponse.doRespond){
            Serial0.println("Respond to command");
            MeshInterface::sendMessage(lastPacket.fromAddress, lastResponse.responseMessage);
        } else {
            Serial0.println("No command to respond to");
        }
    }
    delay(10);
}