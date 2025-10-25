#pragma once

#include <Arduino.h>

namespace CommandHandler{
    struct CommandResponse {
        int userAddress;
        bool doRespond;
        char responseMessage[244];
    };
    
    CommandResponse respondToCommand(const char* message, float rxSnr=0.0f, float rxRssi=0.0f);
}