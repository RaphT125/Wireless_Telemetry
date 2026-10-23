//
// Created by Raphael on 2026-10-21.
//

#ifndef WIRELESS_TELEMETRY_JSONPARSER_H
#define WIRELESS_TELEMETRY_JSONPARSER_H

#include "iParser.h"
class JsonParser : public IParser{
public:
    void parseFile() override;
};


#endif //WIRELESS_TELEMETRY_JSONPARSER_H
