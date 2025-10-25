#include <cstring>
#include "command_handler.h"

struct CommandResponse {
    int userAddress = 0; 
    bool doRespond = false; 
    char responseMessage[244] = {0}; 
};

struct CommandEntry {
    const char* name;
    const char* responseFormat;  // static message or format string
    bool isDynamic;              // true if we need to format with rxSnr/rxRssi
};

static const CommandEntry commandTable[] = {
    {"/info",  "Welcome to jkpg-mesh.se", false},
    {"/help",  "Available commands: /info, /help, /about, /status, /signal", false},
    {"/about", "jkpg-mesh community project using Meshtastic and LoRa this command system implemented on ESP32", false},
    {"/status","Node running normally", false},
    {"/signal", "You were received Snr: %.2f and Rssi: %.2f", true}
};

CommandHandler::CommandResponse CommandHandler::respondToCommand(const char* message, float rxSnr, float rxRssi) {
    Serial0.println("Received message:");
    Serial0.println(message);

    CommandResponse response;
    response.userAddress = 0xffffffff;
    response.doRespond = false;

    for (const auto& cmd : commandTable) {
        if (strcmp(message, cmd.name) == 0) {
            if (cmd.isDynamic) {
                // Format the dynamic response
                snprintf(response.responseMessage, sizeof(response.responseMessage),
                         cmd.responseFormat, rxSnr, rxRssi);
            } else {
                // Copy static response safely
                strncpy(response.responseMessage, cmd.responseFormat, sizeof(response.responseMessage));
                response.responseMessage[sizeof(response.responseMessage)-1] = '\0'; // safety null-terminate
            }
            response.doRespond = true;
            break;
        }
    }

    if (response.doRespond) {
        Serial0.print("Responding with: ");
        Serial0.println(response.responseMessage);
    } else {
        Serial0.println("Unknown command — no response");
    }

    return response;
}

