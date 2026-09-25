#pragma once


// https://docs.discord.com/developers/reference


// INCLUDES

#include "../util/natives.hpp"
#include "../util/networking.hpp"
#include "../util/json_parser.hpp"
#include <thread>
#include <random>


// SETTINGS

#define DISCORD_API           "https://discord.com/api/v10"
#define DISCORD_GATEWAY       "wss://gateway.discord.gg"
#define DISCORD_GATEWAY_QUERY "/?v=10&encoding=json"

const uint16_t discordCloseResumable = 4000;

const int discordIntents = (1 << 0)   // GUILDS
                         | (1 << 9)   // GUILD_MESSAGES
                         | (1 << 12)  // DIRECT_MESSAGES
                         | (1 << 15); // MESSAGE_CONTENT


// CLASSES

struct DiscordCommand{
    string name;
    string description;
    vector<pair<string, string>> options;    // name, description
    Value handler = CaroNull;
};

struct DiscordBotData: NativeData{

    string token;
    string userId;
    string applicationId;
    vector<DiscordCommand> commands;
    bool running  = false;
    bool stopping = false;

    // resuming
    string sessionId;
    string resumeUrl;
    string sequence = "null";

    void forgetSession() {
        sessionId = "";
        resumeUrl = "";
        sequence  = "null";
    }

    void mark() override {
        for(const DiscordCommand& command: commands) markValue(command.handler);
    }

};

nClass(discord_Bot, "discord", "Bot");

nClass(discord_User, "discord", "User");

struct DiscordBotRef: NativeData{
    Value bot = CaroNull;
    void mark() override {
        markValue(bot);
    }
};

nClass(discord_Message, "discord", "Message");

nClass(discord_Interaction, "discord", "Interaction");

nClass(discord_Options, "discord", "Options");


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

Value discordMessage(VM* vm, Value bot, Value dict) {

    ObjClass* messageClass = discordClass(vm, "discord.Message");
    ObjClass* userClass    = discordClass(vm, "discord.User");
    if(messageClass == nullptr || userClass == nullptr) return CaroNull;

    Value message = discordInstance(messageClass, dict);
    Value author  = discordField(dict, "author");
    if(isDict(author)) asInstance(message)->fields["author"] = discordInstance(userClass, author);

    auto native = std::make_unique<DiscordBotRef>();
    native->bot = bot;
    asInstance(message)->native = std::move(native);

    return message;

}

Value discordInteraction(VM* vm, Value bot, Value dict) {

    ObjClass* interactionClass = discordClass(vm, "discord.Interaction");
    ObjClass* userClass        = discordClass(vm, "discord.User");
    ObjClass* optionsClass     = discordClass(vm, "discord.Options");
    if(interactionClass == nullptr || userClass == nullptr || optionsClass == nullptr) return CaroNull;

    Value interaction = discordInstance(interactionClass, dict);
    ObjInstance* instance = asInstance(interaction);

    // command
    Value command = discordField(dict, "data");
    instance->fields["name"] = discordField(command, "name");
    ObjInstance* options = newInstance(optionsClass);
    Value optionList = discordField(command, "options");
    if(isArray(optionList)) {
        for(Value option: asArray(optionList)->data) {
            options->fields[printValue(discordField(option, "name"))] = discordField(option, "value");
        }
    }
    instance->fields["options"] = CaroObj(options);

    // user
    Value user = discordField(discordField(dict, "member"), "user");
    if(!isDict(user)) user = discordField(dict, "user");
    if(isDict(user)) instance->fields["user"] = discordInstance(userClass, user);

    auto native = std::make_unique<DiscordBotRef>();
    native->bot = bot;
    instance->native = std::move(native);

    return interaction;

}

bool discordSnowflake(const string& id) {
    return !id.empty() && std::ranges::all_of(id, [](char c) { return c >= '0' && c <= '9'; });
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

bool discordRegisterCommands(VM* vm, DiscordBotData* data) {

    if(data->commands.empty()) return true;
    if(!discordSnowflake(data->applicationId)) {
        vm->runtimeError("Discord didn't send a valid application ID.");
        return false;
    }

    string body = "[";
    for(size_t i = 0; i < data->commands.size(); ++i) {
        const DiscordCommand& command = data->commands[i];
        if(i > 0) body += ", ";
        body += format(
            "{{\"type\": 1, \"name\": {:s}, \"description\": {:s}, \"options\": [",
            jsonStringifyString(command.name), jsonStringifyString(command.description)
        );
        for(size_t j = 0; j < command.options.size(); ++j) {
            if(j > 0) body += ", ";
            body += format(
                "{{\"type\": 3, \"name\": {:s}, \"description\": {:s}, \"required\": true}}",
                jsonStringifyString(command.options[j].first), jsonStringifyString(command.options[j].second)
            );
        }
        body += "]}";
    }
    body += "]";

    CaroHttp::Response response;
    return discordRequest(vm, data, "PUT", "/applications/" + data->applicationId + "/commands", body, response);

}


// GATEWAY

bool discordCloseRecoverable(VM* vm, DiscordBotData* data, uint16_t code, const string& reason) {

    string problem;
    switch(code) {
        case 4004: problem = "Authentication failed"; break;
        case 4010: problem = "Invalid shard";         break;
        case 4011: problem = "Sharding required";     break;
        case 4012: problem = "Invalid API version";   break;
        case 4013: problem = "Invalid intent(s)";     break;
        case 4014: problem = "Disallowed intents";    break;
            // https://docs.discord.com/developers/topics/opcodes-and-status-codes#gateway

        case 4007: case 4009:
            data->forgetSession();
            return true;

        default: return true;
    }

    if(!reason.empty()) problem += format(" ({:s})", reason);
    vm->runtimeError("%s", problem.c_str());
    return false;

}

bool discordSession(VM* vm, Value self, DiscordBotData* data) {

    using namespace CaroWebSocket;

    // open connection
    bool resuming = !data->sessionId.empty();
    Connection connection;
    string error = connection.open((resuming? data->resumeUrl: DISCORD_GATEWAY) + DISCORD_GATEWAY_QUERY);
    if(!error.empty()) {
        if(resuming) {
            data->forgetSession();
            return true;
        }
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
    bool acknowledged = true;

    Message received;

    while(!data->stopping) {

        // heartbeat
        if(Clock::now() >= nextHeartbeat) {

            if(!acknowledged) {
                connection.close(discordCloseResumable, "", false);
                return true;
            }

            if(!connection.send("{\"op\": 1, \"d\": " + data->sequence + "}").empty()) return true;
            acknowledged  = false;
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
            return discordCloseRecoverable(vm, data, connection.closeCode, connection.closeReason);
        }

        // read the event
        Value handler = CaroNull;
        vector<Value> handlerArgs;
        bool registerCommands = false;
        {

            GCPause pause;

            // fields
            Value payload = discordParse(received.data);
            Value opValue = discordField(payload, "op");
            int op = isNumeric(opValue.type)? asNumberTo<int>(opValue): -1;
            Value d = discordField(payload, "d");
            Value s = discordField(payload, "s");
            if(isInt(s.type)) data->sequence = printValue(s);

            switch(op) {

                // Dispatch
                case 0: {
                    string type = printValue(discordField(payload, "t"));

                    if(type == "READY") {
                        Value sessionId = discordField(d, "session_id");
                        Value resumeUrl = discordField(d, "resume_gateway_url");
                        if(isString(sessionId) && isString(resumeUrl)) {
                            data->sessionId = asString(sessionId)->str;
                            data->resumeUrl = asString(resumeUrl)->str;
                            while(data->resumeUrl.ends_with('/')) data->resumeUrl.pop_back();
                        }
                        Value user = discordField(d, "user");
                        data->userId = printValue(discordField(user, "id"));
                        data->applicationId = printValue(discordField(discordField(d, "application"), "id"));
                        registerCommands = true;
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
                        handlerArgs.push_back(discordMessage(vm, self, d));
                        if(vm->hadError) return false;

                    } else if(type == "INTERACTION_CREATE") {
                        Value interactionType = discordField(d, "type");
                        if(!isNumeric(interactionType.type) || asNumberTo<int>(interactionType) != 2) break;    // APPLICATION_COMMAND
                        string name = printValue(discordField(discordField(d, "data"), "name"));
                        auto command = std::ranges::find(data->commands, name, &DiscordCommand::name);
                        if(command == data->commands.end()) break;
                        handler = command->handler;
                        handlerArgs.push_back(discordInteraction(vm, self, d));
                        if(vm->hadError) return false;

                    }
                    break;
                }

                // Heartbeat
                case 1: {
                    if(!connection.send("{\"op\": 1, \"d\": " + data->sequence + "}").empty()) return true;
                    break;
                }
             
                // Reconnect
                case 7: {
                    connection.close(discordCloseResumable);
                    return true;
                }

                // Invalid Session
                case 9: {
                    if(d.type != TYPE_BOOL || !d.as.Abool) data->forgetSession();
                    connection.close(discordCloseResumable);
                    return true;
                }

                // Hello
                case 10: {
                    Value interval = discordField(d, "heartbeat_interval");
                    if(!isNumeric(interval.type)) {
                        vm->runtimeError("The Discord gateway sent an invalid HELLO.");
                        return false;
                    }
                    heartbeatInterval = asNumberTo<double>(interval) / 1000;

                    std::random_device device;
                    std::mt19937 generator(device());
                    nextHeartbeat = deadlineAfter(heartbeatInterval * std::uniform_real_distribution<double>(0, 1)(generator));

                    string hello = resuming?
                        format(
                            "{{\"op\": 6, \"d\": {{\"token\": {:s}, \"session_id\": {:s}, \"seq\": {:s}}}}}",
                            jsonStringifyString(data->token), jsonStringifyString(data->sessionId), data->sequence
                        ):
                        format(
                            "{{\"op\": 2, \"d\": {{\"token\": {:s}, \"intents\": {:d}, \"properties\": {{\"os\": {:s}, \"browser\": \"carotene\", \"device\": \"carotene\"}}}}}}",
                            jsonStringifyString(data->token), discordIntents, jsonStringifyString(platform)
                        );
                    if(!connection.send(hello).empty()) return true;
                    break;
                }

                // Heartbeat ACK
                case 11: {
                    acknowledged = true;
                    break;
                }

                // what
                default: break;

            }
        }

        if(registerCommands && !discordRegisterCommands(vm, data)) return false;

        if(handler.type != TYPE_NULL) {
            Value result;
            if(!vm->callFromNative(handler, handlerArgs, &result)) return false;
        }

    }

    connection.close(1000);
    return false;

}


// BOT METHODS


// init

nMethod(discord_Bot, init, {
    params({});
    if(alreadyInitialized(vm, self)) return CaroNull;
    asInstance(self)->native = std::make_unique<DiscordBotData>();
    return CaroNull;
});


// run

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
    data->forgetSession();

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


// stop

nMethod(discord_Bot, stop, {
    params({});
    DiscordBotData* data = nativeData<DiscordBotData>(vm, self);
    if(data == nullptr) return CaroNull;
    data->stopping = true;
    return CaroNull;
});


// commands

bool discordCommandName(const string& name) {
    return !name.empty() && name.size() <= 32 && std::ranges::all_of(name, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}

bool discordDescription(const string& description) {
    size_t length = std::ranges::count_if(description, [](char c) { return (c & 0xC0) != 0x80; });
    return length >= 1 && length <= 100;
}

nMethod(discord_Bot, command, {
    params({
        {{OBJ_STRING},                                 true},    // name
        {{OBJ_STRING},                                 true},    // description
        {{OBJ_DICT},                                   true},    // options
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, true}     // handler
    });

    DiscordBotData* data = nativeData<DiscordBotData>(vm, self);
    if(data == nullptr) return CaroNull;

    // set data
    DiscordCommand command;
    command.name        = asString(args[0])->str;
    command.description = asString(args[1])->str;
    command.handler     = args[3];

    // check
    if(!discordCommandName(command.name)) {
        vm->runtimeError("\"%s\" is an invalid command name.", command.name.c_str());
        return CaroNull;
    }
    if(!discordDescription(command.description)) {
        vm->runtimeError("\%s\" is an invalid command description.", command.name.c_str());
        return CaroNull;
    }

    // arguments
    for(const auto& [key, value]: asDict(args[2])->data) {
        if(!isString(key) || !isString(value)) {
            vm->runtimeError("The arguments of \"%s\" should have string-string pairs.", command.name.c_str());
            return CaroNull;
        }
        const string& optionName = asString(key)->str;
        if(!discordCommandName(optionName)) {
            vm->runtimeError("\"%s\" is an invalid argument name.", optionName.c_str());
            return CaroNull;
        }
        if(!discordDescription(asString(value)->str)) {
            vm->runtimeError("\"%s\" is an invalid argument name.", optionName.c_str());
            return CaroNull;
        }
        command.options.emplace_back(optionName, asString(value)->str);
    }
    if(command.options.size() > 25) {
        vm->runtimeError("A command can't have more than 25 arguments.");
        return CaroNull;
    }
    std::ranges::sort(command.options);

    // add or replace
    auto existing = std::ranges::find(data->commands, command.name, &DiscordCommand::name);
    if(existing != data->commands.end()) {
        *existing = std::move(command);
    } else if(data->commands.size() >= 100) {
        vm->runtimeError("A bot can't have more than 100 commands.");
        return CaroNull;
    } else {
        data->commands.push_back(std::move(command));
    }

    // already connected
    if(data->running && !data->applicationId.empty()) discordRegisterCommands(vm, data);

    return CaroNull;
});


// send

Value discordSend(VM* vm, Value bot, const string& channel, const string& body) {

    DiscordBotData* data = nativeData<DiscordBotData>(vm, bot);
    if(data == nullptr) return CaroNull;

    if(!data->running) {
        vm->runtimeError("The bot isn't running.");
        return CaroNull;
    }

    if(!discordSnowflake(channel)) {
        vm->runtimeError("\"%s\" isn't a valid channel id.", channel.c_str());
        return CaroNull;
    }

    CaroHttp::Response response;
    if(!discordRequest(vm, data, "POST", "/channels/" + channel + "/messages", "{" + body + "}", response)) return CaroNull;

    GCPause pause;
    return discordMessage(vm, bot, discordParse(response.body));

}

nMethod(discord_Bot, send, {
    params({
        {{OBJ_STRING, TYPE_UINT, TYPE_INT, TYPE_ULONG, TYPE_LONG}, true},    // channel id
        {{OBJ_STRING},                                             true}     // content
    });
    return discordSend(vm, self, printValue(args[0]), "\"content\": " + jsonStringifyString(asString(args[1])->str));
});

nMethod(discord_Message, reply, {
    params({
        {{OBJ_STRING}, true },   // content
        {{TYPE_BOOL},  false}    // ping
    });
    
    bool ping = args.size() < 2 || args[1].as.Abool;

    DiscordBotRef* data = nativeData<DiscordBotRef>(vm, self);
    if(data == nullptr) return CaroNull;

    string channel = printValue(discordHandler(self, "channel_id"));
    string message = printValue(discordHandler(self, "id"));
    if(!discordSnowflake(message)) {
        vm->runtimeError("\"%s\" isn't a valid message id.", message.c_str());
        return CaroNull;
    }

    return discordSend(vm, data->bot, channel, format(
        "\"content\": {:s}, "
        "\"message_reference\": {{\"message_id\": {:s}, \"fail_if_not_exists\": false}}, "
        "\"allowed_mentions\": {{\"parse\": [\"users\", \"roles\", \"everyone\"], \"replied_user\": {:s}}}",
        jsonStringifyString(asString(args[0])->str), jsonStringifyString(message), ping? "true": "false"
    ));
});

nMethod(discord_Interaction, reply, {
    params({
        {{OBJ_STRING}, true },    // content
        {{TYPE_BOOL},  false}     // ephemeral
    });

    bool ephemeral = args.size() >= 2 && args[1].as.Abool;

    DiscordBotRef* ref = nativeData<DiscordBotRef>(vm, self);
    if(ref == nullptr) return CaroNull;
    DiscordBotData* data = nativeData<DiscordBotData>(vm, ref->bot);
    if(data == nullptr) return CaroNull;

    if(!data->running) {
        vm->runtimeError("The bot isn't running.");
        return CaroNull;
    }

    string id    = printValue(discordHandler(self, "id"));
    Value  token = discordHandler(self, "token");
    if(!discordSnowflake(id)) {
        vm->runtimeError("\"%s\" isn't a valid interaction id.", id.c_str());
        return CaroNull;
    }
    if(!isString(token) || asString(token)->str.empty()) {
        vm->runtimeError("This interaction doesn't have a token.");
        return CaroNull;
    }

    CaroHttp::Response response;
    discordRequest(
        vm, data,
        "POST",
        "/interactions/" + id + "/" + asString(token)->str + "/callback",
        format(
            "{{"
                "\"type\": 4, "
                "\"data\": {{"
                    "\"content\": {:s}, "
                    "{:s}"
                    "\"allowed_mentions\": {{\"parse\": [\"users\", \"roles\", \"everyone\"]}}"
                "}}"
            "}}",
            jsonStringifyString(asString(args[0])->str), ephemeral? "\"flags\": 64, ": ""
        ),
        response
    );

    return CaroNull;
});
