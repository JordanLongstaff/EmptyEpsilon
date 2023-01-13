#ifndef PLAYER_SPACESHIP_H
#define PLAYER_SPACESHIP_H

#include "spaceship.h"
#include "scanProbe.h"
#include "commsScriptInterface.h"
#include "playerInfo.h"
#include "components/player.h"
#include <iostream>

class ScanProbe;

enum ECommsState
{
    CS_Inactive,          // No active comms
    CS_OpeningChannel,    // Opening a comms channel
    CS_BeingHailed,       // Receiving a hail from an object
    CS_BeingHailedByGM,   //                   ... the GM
    CS_ChannelOpen,       // Comms open to an object
    CS_ChannelOpenPlayer, //           ... another player
    CS_ChannelOpenGM,     //           ... the GM
    CS_ChannelFailed,     // Comms failed to connect
    CS_ChannelBroken,     // Comms broken by other side
    CS_ChannelClosed      // Comms manually closed
};

class PlayerSpaceship : public SpaceShip
{
public:
    // Overheat subsystem damage rate
    constexpr static float damage_per_second_on_overheat = 0.08f;
    // Base time it takes to perform an action
    constexpr static float comms_channel_open_time = 2.0;
    constexpr static float scan_probe_charge_time = 10.0f;
    constexpr static float max_scanning_delay = 6.0;

    constexpr static int16_t CMD_PLAY_CLIENT_SOUND = 0x0001;

    // Content of a line in the ship's log
    class ShipLogEntry
    {
    public:
        string prefix;
        string text;
        glm::u8vec4 color;

        ShipLogEntry() {}
        ShipLogEntry(string prefix, string text, glm::u8vec4 color)
        : prefix(prefix), text(text), color(color) {}

        bool operator!=(const ShipLogEntry& e) { return prefix != e.prefix || text != e.text || color != e.color; }
    };

    class CustomShipFunction
    {
    public:
        enum class Type
        {
            Info,
            Button,
            Message
        };
        Type type;
        string name;
        string caption;
        ECrewPosition crew_position;
        ScriptSimpleCallback callback;
        int order;

        bool operator!=(const CustomShipFunction& csf) { return type != csf.type || name != csf.name || caption != csf.caption || crew_position != csf.crew_position; }
    };

    // Ship automation features, mostly for single-person ships like fighters
    bool auto_repair_enabled;

private:
    bool on_new_player_ship_called=false;
    // Comms variables
    ECommsState comms_state;
    float comms_open_delay;
    string comms_target_name;
    string comms_incomming_message;
    P<SpaceObject> comms_target; // Server only
    std::vector<int> comms_reply_id;
    std::vector<string> comms_reply_message;
    CommsScriptInterface comms_script_interface; // Server only
    // Ship's log container
    std::vector<ShipLogEntry> ships_log;
public:
    std::vector<CustomShipFunction> custom_functions;

    std::vector<glm::vec2> waypoints;

    // Ship functionality
    // Capable of scanning a target
    bool can_scan = true;
    // Target of a scan. Server-only value
    P<SpaceObject> scanning_target;
    // Time in seconds to scan an object if scanning_complexity is 0 (none)
    float scanning_delay = 0.0;
    // Number of sliders during a scan
    int scanning_complexity = 0;
    // Number of times an object must be scanned to achieve a fully scanned
    // state
    int scanning_depth = 0;

    // Capable of hacking a target
    bool can_hack = true;

    // Capable of probe launches
    bool can_launch_probe = true;
    int max_scan_probes = 8;
    int scan_probe_stock;
    float scan_probe_recharge = 0.0;
    ScriptSimpleCallback on_probe_launch;
    ScriptSimpleCallback on_probe_link;
    ScriptSimpleCallback on_probe_unlink;

    int32_t linked_science_probe_id = -1;

    PlayerSpaceship();
    virtual ~PlayerSpaceship();

    // Comms functions
    bool isCommsInactive() { return comms_state == CS_Inactive; }
    bool isCommsOpening() { return comms_state == CS_OpeningChannel; }
    bool isCommsBeingHailed() { return comms_state == CS_BeingHailed || comms_state == CS_BeingHailedByGM; }
    bool isCommsBeingHailedByGM() { return comms_state == CS_BeingHailedByGM; }
    bool isCommsFailed() { return comms_state == CS_ChannelFailed; }
    bool isCommsBroken() { return comms_state == CS_ChannelBroken; }
    bool isCommsClosed() { return comms_state == CS_ChannelClosed; }
    bool isCommsChatOpen() { return comms_state == CS_ChannelOpenPlayer || comms_state == CS_ChannelOpenGM; }
    bool isCommsChatOpenToGM() { return comms_state == CS_ChannelOpenGM; }
    bool isCommsChatOpenToPlayer() { return comms_state == CS_ChannelOpenPlayer; }
    bool isCommsScriptOpen() { return comms_state == CS_ChannelOpen; }
    ECommsState getCommsState() { return comms_state; }
    float getCommsOpeningDelay() { return comms_open_delay; }
    const std::vector<string>& getCommsReplyOptions() const { return comms_reply_message; }
    P<SpaceObject> getCommsTarget() { return comms_target; }
    const string& getCommsTargetName() { return comms_target_name; }
    const string& getCommsIncommingMessage() { return comms_incomming_message; }
    bool hailCommsByGM(string target_name);
    bool hailByObject(P<SpaceObject> object, string opening_message);
    void setCommsMessage(string message);
    void addCommsIncommingMessage(string message);
    void addCommsOutgoingMessage(string message);
    void addCommsReply(int32_t id, string message);
    void switchCommsToGM();
    void closeComms();

    //Spaceship also has functions for these?!?
    void setEnergyLevel(float amount);
    void setEnergyLevelMax(float amount);
    float getEnergyLevel();
    float getEnergyLevelMax();

    void setCanScan(bool enabled) { can_scan = enabled; }
    bool getCanScan() { return can_scan; }
    void setCanHack(bool enabled) { can_hack = enabled; }
    bool getCanHack() { return can_hack; }
    void setCanDock(bool enabled);
    bool getCanDock();
    void setCanCombatManeuver(bool enabled) { } //TODO
    bool getCanCombatManeuver() { return true; } // TODO
    void setCanSelfDestruct(bool enabled) { }  // TODO
    bool getCanSelfDestruct() { return false; } // TODO
    void setCanLaunchProbe(bool enabled) { can_launch_probe = enabled; }
    bool getCanLaunchProbe() { return can_launch_probe; }

    void setSelfDestructDamage(float amount) { }  // TODO
    float getSelfDestructDamage() { return 0.0f; }  // TODO
    void setSelfDestructSize(float size) { } // TODO
    float getSelfDestructSize() { return 0.0f; } // TODO

    void setScanProbeCount(int amount) { scan_probe_stock = std::max(0, std::min(amount, max_scan_probes)); }
    int getScanProbeCount() { return scan_probe_stock; }
    void setMaxScanProbeCount(int amount) { max_scan_probes = std::max(0, amount); scan_probe_stock = std::min(scan_probe_stock, max_scan_probes); }
    int getMaxScanProbeCount() { return max_scan_probes; }

    void onProbeLaunch(ScriptSimpleCallback callback);
    void onProbeLink(ScriptSimpleCallback callback);
    void onProbeUnlink(ScriptSimpleCallback callback);

    void addCustomButton(ECrewPosition position, string name, string caption, ScriptSimpleCallback callback, std::optional<int> order);
    void addCustomInfo(ECrewPosition position, string name, string caption, std::optional<int> order);
    void addCustomMessage(ECrewPosition position, string name, string caption);
    void addCustomMessageWithCallback(ECrewPosition position, string name, string caption, ScriptSimpleCallback callback);
    void removeCustom(string name);

    ShipSystem::Type getBeamSystemTarget();
    string getBeamSystemTargetName();
    // Client command functions
    virtual void onReceiveClientCommand(int32_t client_id, sp::io::DataBuffer& packet) override;
    void commandTargetRotation(float target);
    void commandTurnSpeed(float turnSpeed);
    void commandImpulse(float target);
    void commandWarp(int8_t target);
    void commandJump(float distance);
    void commandSetTarget(P<SpaceObject> target);
    void commandSetScienceLink(P<ScanProbe> probe);
    void commandClearScienceLink();
    void commandLoadTube(int8_t tubeNumber, EMissileWeapons missileType);
    void commandUnloadTube(int8_t tubeNumber);
    void commandFireTube(int8_t tubeNumber, float missile_target_angle);
    void commandFireTubeAtTarget(int8_t tubeNumber, P<SpaceObject> target);
    void commandSetShields(bool enabled);
    void commandMainScreenSetting(MainScreenSetting mainScreen);
    void commandMainScreenOverlay(MainScreenOverlay mainScreen);
    void commandScan(P<SpaceObject> object);
    void commandSetSystemPowerRequest(ShipSystem::Type system, float power_level);
    void commandSetSystemCoolantRequest(ShipSystem::Type system, float coolant_level);
    void commandDock(P<SpaceObject> station);
    void commandUndock();
    void commandAbortDock();
    void commandOpenTextComm(P<SpaceObject> obj);
    void commandCloseTextComm();
    void commandAnswerCommHail(bool awnser);
    void commandSendComm(uint8_t index);
    void commandSendCommPlayer(string message);
    void commandSetAutoRepair(bool enabled);
    void commandSetBeamFrequency(int32_t frequency);
    void commandSetBeamSystemTarget(ShipSystem::Type system);
    void commandSetShieldFrequency(int32_t frequency);
    void commandAddWaypoint(glm::vec2 position);
    void commandRemoveWaypoint(int32_t index);
    void commandMoveWaypoint(int32_t index, glm::vec2 position);
    void commandActivateSelfDestruct();
    void commandCancelSelfDestruct();
    void commandConfirmDestructCode(int8_t index, uint32_t code);
    void commandCombatManeuverBoost(float amount);
    void commandCombatManeuverStrafe(float strafe);
    void commandLaunchProbe(glm::vec2 target_position);
    void commandScanDone();
    void commandScanCancel();
    void commandSetAlertLevel(AlertLevel level);
    void commandHackingFinished(P<SpaceObject> target, ShipSystem::Type target_system);
    void commandCustomFunction(string name);

    virtual void onReceiveServerCommand(sp::io::DataBuffer& packet) override;

    // Template function
    virtual void applyTemplateValues() override;

    // Ship status functions
    void setSystemCoolantRequest(ShipSystem::Type system, float request);
    void setMaxCoolant(float coolant);
    float getMaxCoolant() { return 10.0f; } //TODO
    void setAutoCoolant(bool active) {} //TODO
    int getRepairCrewCount();
    void setRepairCrewCount(int amount);
    AlertLevel getAlertLevel() { return AlertLevel::Normal; } // TODO

    // Flow rate controls.
    float getEnergyShieldUsePerSecond() const { return 0.0f; } //TODO
    void setEnergyShieldUsePerSecond(float rate) { } //TODO
    float getEnergyWarpPerSecond() const { return 0.0f; } //TODO
    void setEnergyWarpPerSecond(float rate) {} //TODO

    // Ship update functions
    virtual void update(float delta) override;

    // Call on the server to play a sound on the main screen.
    void playSoundOnMainScreen(string sound_name);

    // Ship's log functions
    void addToShipLog(string message, glm::u8vec4 color);
    void addToShipLogBy(string message, P<SpaceObject> target);
    const std::vector<ShipLogEntry>& getShipsLog() const;

    // Ship's crew functions
    void transferPlayersToShip(P<PlayerSpaceship> other_ship);
    void transferPlayersAtPositionToShip(ECrewPosition position, P<PlayerSpaceship> other_ship);
    bool hasPlayerAtPosition(ECrewPosition position);

    // Ship shields functions
    virtual bool getShieldsActive() override { return true; } //TODO
    void setShieldsActive(bool active) { }

    // Waypoint functions
    int getWaypointCount() { return waypoints.size(); }
    glm::vec2 getWaypoint(int index) { if (index > 0 && index <= int(waypoints.size())) return waypoints[index - 1]; return glm::vec2(0, 0); }

    // Ship control code/password setter
    void setControlCode(string code) { } // TODO

    // Radar function
    virtual void drawOnGMRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range) override;

    // Script export function
    virtual string getExportLine() override;
};
REGISTER_MULTIPLAYER_ENUM(ECommsState);

static inline sp::io::DataBuffer& operator << (sp::io::DataBuffer& packet, const PlayerSpaceship::CustomShipFunction& csf) { return packet << uint8_t(csf.type) << uint8_t(csf.crew_position) << csf.name << csf.caption; } \
static inline sp::io::DataBuffer& operator >> (sp::io::DataBuffer& packet, PlayerSpaceship::CustomShipFunction& csf) { int8_t tmp; packet >> tmp; csf.type = PlayerSpaceship::CustomShipFunction::Type(tmp); packet >> tmp; csf.crew_position = ECrewPosition(tmp); packet >> csf.name >> csf.caption; return packet; }

static inline bool operator < (const PlayerSpaceship::CustomShipFunction& a, const PlayerSpaceship::CustomShipFunction& b) {return (a.order < b.order);}

#endif//PLAYER_SPACESHIP_H
