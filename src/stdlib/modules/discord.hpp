#pragma once


// https://docs.discord.com/developers/reference


// INCLUDES

#include "../util/natives.hpp"
#include "../util/networking.hpp"
#include "../util/json_parser.hpp"
#include <thread>


// SETTINGS

#define DISCORD_API     "https://discord.com/api/v10"
#define DISCORD_GATEWAY "wss://gateway.discord.gg/?v=10&encoding=json"

const int discordIntents = (1 << 0)   // GUILDS
                         | (1 << 9)   // GUILD_MESSAGES
                         | (1 << 12)  // DIRECT_MESSAGES
                         | (1 << 15); // MESSAGE_CONTENT


// CLASSES

struct DiscordBotData: NativeData{
    string token;
    string userId;
    bool running  = false;
    bool stopping = false;
};

nClass(discord_Bot,     "discord", "Bot"    );
nClass(discord_User,    "discord", "User"   );
nClass(discord_Message, "discord", "Message");


// HELPERS

Value discordParse(const string& text) {
    CaroJsonParser parser(text);
    Value parsed = parser.parseValue();
    return parser.errored? CaroNull: parsed;
}

Value discordField(Value dict, const string& key) {
    if(!isDict(dict)) return CaroNull;
    auto found = asDict(dict)->data.find(CaroObj(copyString(key)));
    return found == asDict(dict)->data.end()? CaroNull: found->second;
}

ObjClass* discordClass(VM* vm, const string& name) {
    auto found = vm->globals.find(name);
    if(found == vm->globals.end() || !isClass(found->second)) {
        vm->runtimeError("Couldn't find the %s class.", name.c_str());
        return nullptr;
    }
    return asClass(found->second);
}

Value discordInstance(ObjClass* klass, Value dict) {
    ObjInstance* instance = newInstance(klass);
    if(isDict(dict)) {
        for(const auto& [key, value]: asDict(dict)->data) {
            instance->fields[printValue(key)] = value;
        }
    }
    return CaroObj(instance);
}

Value discordHandler(Value self, const string& name) {
    auto field = asInstance(self)->fields.find(name);
    return field == asInstance(self)->fields.end()? CaroNull: field->second;
}

Value discordMessage(VM* vm, Value dict) {

    ObjClass* messageClass = discordClass(vm, "discord.Message");
    ObjClass* userClass    = discordClass(vm, "discord.User");
    if(messageClass == nullptr || userClass == nullptr) return CaroNull;

    Value message = discordInstance(messageClass, dict);
    Value author  = discordField(dict, "author");
    if(isDict(author)) asInstance(message)->fields["author"] = discordInstance(userClass, author);

    return message;

}

bool discordRequest(VM* vm, DiscordBotData* data, const string& method, const string& path, const string& body, CaroHttp::Response& response) {

    vector<pair<string, string>> headers = {
        {"Authorization", "Bot " + data->token},
        {"Content-Type",  "application/json"  }
    };
    string error = CaroHttp::request(method, DISCORD_API + path, headers, body, CaroHttp::defaultTimeout, response);

    if(error.empty() && response.status >= 400) {
        GCPause pause;
        Value reason = discordField(discordParse(response.body), "message");
        error = CaroHttp::withReason(
            format("Discord responded with status {:d}", response.status),
            isString(reason)? asString(reason)->str: ""
        );
    }

    if(error.empty()) return true;
    vm->runtimeError("%s", error.c_str());
    return false;

}


// GATEWAY

bool discordCloseRecoverable(VM* vm, uint16_t code, const string& reason) {

    string problem;
    switch(code) {
        case 4000: problem = "Unknown error";         break;
        case 4001: problem = "Unknown opcode";        break;
        case 4002: problem = "Decode error";          break;
        case 4003: problem = "Not authenticated";     break;
        case 4004: problem = "Authentication failed"; break;
        case 4005: problem = "Already authenticated"; break;
        case 4007: problem = "Invalid seq";           break;
        case 4008: problem = "Rate limited";          break;
        case 4009: problem = "Session timed out";     break;
        case 4010: problem = "Invalid shard";         break;
        case 4011: problem = "Sharding required";     break;
        case 4012: problem = "Invalid API version";   break;
        case 4013: problem = "Invalid intent(s)";     break;
        case 4014: problem = "Disallowed intents";    break;
            // https://docs.discord.com/developers/topics/opcodes-and-status-codes#gateway
        default: return true;
    }

    if(!reason.empty()) problem += format(" ({:s})", reason);
    vm->runtimeError("%s", problem.c_str());
    return false;

}

bool discordSession(VM* vm, Value self, DiscordBotData* data) {

    using namespace CaroWebSocket;

    // open connection
    Connection connection;
    string error = connection.open(DISCORD_GATEWAY);
    if(!error.empty()) {
        vm->runtimeError("%s", error.c_str());
        return false;
    }

    // get platform
    string platform = "unknown";
    auto found = vm->globals.find("caro.platform");
    if(found != vm->globals.end() && isString(found->second)) platform = asString(found->second)->str;

    // heartbeat
    double heartbeatInterval = -1;
    Clock::time_point nextHeartbeat = Clock::time_point::max();
    string sequence = "null";

    Message received;

    while(!data->stopping) {

        // heartbeat
        if(Clock::now() >= nextHeartbeat) {
            if(!connection.send("{\"op\": 1, \"d\": " + sequence + "}").empty()) return true;
            nextHeartbeat = deadlineAfter(heartbeatInterval);
        }

        // wait for the next event
        double timeout = heartbeatInterval < 0? defaultTimeout: msLeft(nextHeartbeat) / 1000.0;
        Event event = connection.receive(timeout, received);

        // no response
        if(event == EVENT_NONE) {
            if(heartbeatInterval < 0) {
                vm->runtimeError("The Discord gateway didn't respond.");
                return false;
            }
            continue;
        }

        // closed
        if(event == EVENT_CLOSED) {
            return discordCloseRecoverable(vm, connection.closeCode, connection.closeReason);
        }

        // read the event
        Value handler = CaroNull;
        vector<Value> handlerArgs;
        {

            GCPause pause;

            // fields
            Value payload = discordParse(received.data);
            Value opValue = discordField(payload, "op");
            int op = isNumeric(opValue.type)? asNumberTo<int>(opValue): -1;
            Value d = discordField(payload, "d");
            Value s = discordField(payload, "s");
            if(isInt(s.type)) sequence = printValue(s);

            switch(op) {

                // Dispatch
                case 0: {
                    string type = printValue(discordField(payload, "t"));

                    if(type == "READY") {
                        Value user = discordField(d, "user");
                        data->userId = printValue(discordField(user, "id"));
                        if(isDict(user)) {
                            for(const auto& [key, value]: asDict(user)->data) {
                                asInstance(self)->fields[printValue(key)] = value;
                            }
                        }
                        handler = discordHandler(self, "on_ready");

                    } else if(type == "MESSAGE_CREATE") {
                        if(printValue(discordField(discordField(d, "author"), "id")) == data->userId) break;
                        handler = discordHandler(self, "on_message");
                        if(handler.type == TYPE_NULL) break;
                        handlerArgs.push_back(discordMessage(vm, d));
                        if(vm->hadError) return false;

                    }
                    break;
                }

                // Heartbeat
                case 1: {
                    nextHeartbeat = Clock::now();
                    break;
                }

                // Identify
             // case 2:

                // Presence Update
             // case 3:

                 // Voice State Update
             // case 4:

                 // Resume
             // case 6:
             
                // Reconnect
                case 7: return true;

                // Request Guild Members
             // case 8:

                // Invalid Session
                case 9: return true;

                // Hello
                case 10: {
                    Value interval = discordField(d, "heartbeat_interval");
                    if(!isNumeric(interval.type)) {
                        vm->runtimeError("The Discord gateway sent an invalid HELLO.");
                        return false;
                    }
                    heartbeatInterval = asNumberTo<double>(interval) / 1000;
                    nextHeartbeat = deadlineAfter(heartbeatInterval);
                    string identify = format(
                        "{{\"op\": 2, \"d\": {{\"token\": {:s}, \"intents\": {:d}, \"properties\": {{\"os\": {:s}, \"browser\": \"carotene\", \"device\": \"carotene\"}}}}}}",
                        jsonStringifyString(data->token), discordIntents, jsonStringifyString(platform)
                    );
                    if(!connection.send(identify).empty()) return true;
                    break;
                }

                // Heartbeat ACK
                case 11: break;

                // Request Soundboard Sounds
             // case 31:

                // Request Channel Info
             // case 43:

                // what
                default: break;

            }
        }

        if(handler.type != TYPE_NULL) {
            Value result;
            if(!vm->callFromNative(handler, handlerArgs, &result)) return false;
        }

    }

    connection.close(1000);
    return false;

}


// BOT METHODS

nMethod(discord_Bot, init, {
    params({});
    if(alreadyInitialized(vm, self)) return CaroNull;
    asInstance(self)->native = std::make_unique<DiscordBotData>();
    return CaroNull;
});

nMethod(discord_Bot, run, {
    params({
        {{OBJ_STRING}, true}
    });

    DiscordBotData* data = nativeData<DiscordBotData>(vm, self);
    if(data == nullptr) return CaroNull;

    // already running
    if(data->running) {
        vm->runtimeError("The bot is already running.");
        return CaroNull;
    }

    // token
    string token = trim(asString(args[0])->str);
    if(token.empty()) {
        vm->runtimeError("The token can't be empty.");
        return CaroNull;
    }

    data->token    = token;
    data->userId   = "";
    data->running  = true;
    data->stopping = false;

    while(discordSession(vm, self, data)) {
        #ifdef __EMSCRIPTEN__
            emscripten_sleep(1000u);
        #else
            std::this_thread::sleep_for(chrono::duration<double>(1));
        #endif
    }
        // todo: make the delay dynamic instead of always 1 second

    data->running = false;

    return CaroNull;
});

nMethod(discord_Bot, stop, {
    params({});
    DiscordBotData* data = nativeData<DiscordBotData>(vm, self);
    if(data == nullptr) return CaroNull;
    data->stopping = true;
    return CaroNull;
});

nMethod(discord_Bot, send, {
    params({
        {{OBJ_STRING, TYPE_UINT, TYPE_INT, TYPE_ULONG, TYPE_LONG}, true},    // channel id
        {{OBJ_STRING},                                             true}     // content
    });

    DiscordBotData* data = nativeData<DiscordBotData>(vm, self);
    if(data == nullptr) return CaroNull;

    if(!data->running) {
        vm->runtimeError("The bot isn't running.");
        return CaroNull;
    }

    string channel = printValue(args[0]);
    if(channel.empty() || !std::ranges::all_of(channel, [](char c) { return c >= '0' && c <= '9'; })) {
        vm->runtimeError("\"%s\" isn't a valid channel id.", channel.c_str());
        return CaroNull;
    }

    CaroHttp::Response response;
    if(!discordRequest(
        vm,
        data,
        "POST",
        "/channels/" + channel + "/messages",
        "{\"content\": " + jsonStringifyString(asString(args[1])->str) + "}",
        response
    )) return CaroNull;

    GCPause pause;
    return discordMessage(vm, discordParse(response.body));

});
