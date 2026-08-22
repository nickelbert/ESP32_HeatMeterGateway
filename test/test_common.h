#pragma once

#include <string>
#include <vector>
#include "TelnetServer.h"
#include "ConfigManager.h"

inline TelnetServer::TelnetServer()
{
}

inline TelnetServer::~TelnetServer()
{
}

inline void TelnetServer::setup()
{
}

inline void TelnetServer::telnetPrint(const char *msg)
{
    (void)msg;
}

inline void TelnetServer::telnetPrint(const std::string &msg)
{
    (void)msg;
}

inline bool TelnetServer::hasClient() const
{
    return false;
}