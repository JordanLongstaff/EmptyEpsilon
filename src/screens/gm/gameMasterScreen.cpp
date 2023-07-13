#include "gameMasterScreen.h"
#include "shipTemplate.h"
#include "main.h"
#include "gameGlobalInfo.h"
#include "objectCreationView.h"
#include "globalMessageEntryView.h"
#include "tweak.h"
#include "clipboard.h"
#include "chatDialog.h"
#include "spaceObjects/cpuShip.h"
#include "spaceObjects/spaceStation.h"
#include "spaceObjects/wormHole.h"
#include "spaceObjects/zone.h"
#include "components/faction.h"
#include "components/collision.h"
#include "systems/collision.h"
#include "ecs/query.h"

#include "screenComponents/radarView.h"

#include "components/ai.h"
#include "gui/gui2_togglebutton.h"
#include "gui/gui2_selector.h"
#include "gui/gui2_listbox.h"
#include "gui/gui2_label.h"
#include "gui/gui2_keyvaluedisplay.h"
#include "gui/gui2_textentry.h"

GameMasterScreen::GameMasterScreen(RenderLayer* render_layer)
: GuiCanvas(render_layer), click_and_drag_state(CD_None)
{
    main_radar = new GuiRadarView(this, "MAIN_RADAR", 50000.0f, &targets);
    main_radar->setStyle(GuiRadarView::Rectangular)->longRange()->gameMaster()->enableTargetProjections(nullptr)->setAutoCentering(false);
    main_radar->setPosition(0, 0, sp::Alignment::TopLeft)->setSize(GuiElement::GuiSizeMax, GuiElement::GuiSizeMax);
    main_radar->setCallbacks(
        [this](sp::io::Pointer::Button button, glm::vec2 position) { this->onMouseDown(button, position); },
        [this](glm::vec2 position) { this->onMouseDrag(position); },
        [this](glm::vec2 position) { this->onMouseUp(position); }
    );
    box_selection_overlay = new GuiOverlay(main_radar, "BOX_SELECTION", glm::u8vec4(255, 255, 255, 32));
    box_selection_overlay->layout.fill_height = false;
    box_selection_overlay->layout.fill_width = false;
    box_selection_overlay->hide();

    pause_button = new GuiToggleButton(this, "PAUSE_BUTTON", tr("button", "Pause"), [](bool value) {
        if (!value)
            engine->setGameSpeed(1.0f);
        else
            engine->setGameSpeed(0.0f);
    });
    pause_button->setValue(engine->getGameSpeed() == 0.0f)->setPosition(20, 20, sp::Alignment::TopLeft)->setSize(250, 50);

    intercept_comms_button = new GuiToggleButton(this, "INTERCEPT_COMMS_BUTTON", tr("button", "Intercept all comms"), [](bool value) {
        gameGlobalInfo->intercept_all_comms_to_gm = value;
    });
    intercept_comms_button->setValue(gameGlobalInfo->intercept_all_comms_to_gm)->setTextSize(20)->setPosition(300, 20, sp::Alignment::TopLeft)->setSize(200, 25);

    faction_selector = new GuiSelector(this, "FACTION_SELECTOR", [this](int index, string value) {
        for(auto obj : targets.getTargets())
        {
            obj.getOrAddComponent<Faction>().entity = Faction::find(value);
        }
    });
    for(auto [entity, info] : sp::ecs::Query<FactionInfo>())
        faction_selector->addEntry(info.locale_name, info.name);
    faction_selector->setPosition(20, 70, sp::Alignment::TopLeft)->setSize(250, 50);

    global_message_button = new GuiButton(this, "GLOBAL_MESSAGE_BUTTON", tr("button", "Global message"), [this]() {
        global_message_entry->show();
    });
    global_message_button->setPosition(20, -20, sp::Alignment::BottomLeft)->setSize(250, 50);

    player_ship_selector = new GuiSelector(this, "PLAYER_SHIP_SELECTOR", [this](int index, string value) {
        auto ship = sp::ecs::Entity::fromString(value);
        if (ship)
            target = ship;
        if (auto transform = target.getComponent<sp::Transform>())
            main_radar->setViewPosition(transform->getPosition());
        targets.set(ship);
    });
    player_ship_selector->setPosition(270, -20, sp::Alignment::BottomLeft)->setSize(350, 50);

    create_button = new GuiButton(this, "CREATE_OBJECT_BUTTON", tr("button", "Create..."), [this]() {
        object_creation_view->show();
    });
    create_button->setPosition(20, -70, sp::Alignment::BottomLeft)->setSize(250, 50);

    copy_scenario_button = new GuiButton(this, "COPY_SCENARIO_BUTTON", tr("button", "Copy scenario"), [this]() {
        Clipboard::setClipboard(getScriptExport(false));
    });
    copy_scenario_button->setTextSize(20)->setPosition(-20, -20, sp::Alignment::BottomRight)->setSize(125, 25);

    copy_selected_button = new GuiButton(this, "COPY_SELECTED_BUTTON", tr("button", "Copy selected"), [this]() {
        Clipboard::setClipboard(getScriptExport(true));
    });
    copy_selected_button->setTextSize(20)->setPosition(-20, -45, sp::Alignment::BottomRight)->setSize(125, 25);

    cancel_action_button = new GuiButton(this, "CANCEL_CREATE_BUTTON", tr("button", "Cancel"), []() {
        gameGlobalInfo->on_gm_click = nullptr;
    });
    cancel_action_button->setPosition(20, -70, sp::Alignment::BottomLeft)->setSize(250, 50)->hide();

    tweak_button = new GuiButton(this, "TWEAK_OBJECT", tr("button", "Tweak"), [this]() {
        for(auto entity : targets.getTargets())
        {
            auto obj_ptr = entity.getComponent<SpaceObject*>();
            if (!obj_ptr) continue;
            P<SpaceObject> obj = *obj_ptr;
            if (P<PlayerSpaceship>(obj))
            {
                player_tweak_dialog->open(obj);
                break;
            }
            else if (P<SpaceShip>(obj))
            {
                ship_tweak_dialog->open(obj);
                break;
            }
            //else if (P<SpaceStation>(obj))
            //{
            //    station_tweak_dialog->open(obj);
            //}
            else if (P<WarpJammerObject>(obj))
            {
                jammer_tweak_dialog->open(obj);
            }
            else if (P<Asteroid>(obj))
            {
                asteroid_tweak_dialog->open(obj);
            }
            else
            {
                object_tweak_dialog->open(obj);
                break;
            }
        }
    });
    tweak_button->setPosition(20, -120, sp::Alignment::BottomLeft)->setSize(250, 50)->hide();

    player_comms_hail = new GuiButton(this, "HAIL_PLAYER", tr("button", "Hail ship"), [this]() {
        for(auto obj : targets.getTargets())
        {
            if (obj.hasComponent<CommsTransmitter>())
            {
                auto cd = getChatDialog(obj);
                if (auto transform = obj.getComponent<sp::Transform>())
                    cd->show()->setPosition(main_radar->worldToScreen(transform->getPosition()))->setSize(300, 300);
            }
        }
    });
    player_comms_hail->setPosition(20, -170, sp::Alignment::BottomLeft)->setSize(250, 50)->hide();

    info_layout = new GuiElement(this, "INFO_LAYOUT");
    info_layout->setPosition(-20, 20, sp::Alignment::TopRight)->setSize(300, GuiElement::GuiSizeMax)->setAttribute("layout", "vertical");

    info_clock = new GuiKeyValueDisplay(info_layout, "INFO_CLOCK", 0.5, tr("Clock"), "");
    info_clock->setSize(GuiElement::GuiSizeMax, 30);

    gm_script_options = new GuiListbox(this, "GM_SCRIPT_OPTIONS", [this](int index, string value)
    {
        gm_script_options->setSelectionIndex(-1);
        int n = 0;
        for(GMScriptCallback& callback : gameGlobalInfo->gm_callback_functions)
        {
            if (n == index)
            {
                auto cb = callback.callback;
                cb.call<void>();
                return;
            }
            n++;
        }
    });
    gm_script_options->setPosition(20, 130, sp::Alignment::TopLeft)->setSize(250, 500);

    order_layout = new GuiElement(this, "ORDER_LAYOUT");
    order_layout->setPosition(-20, -90, sp::Alignment::BottomRight)->setSize(300, GuiElement::GuiSizeMax)->setAttribute("layout", "verticalbottom");

    (new GuiButton(order_layout, "ORDER_DEFEND_LOCATION", tr("Defend location"), [this]() {
        for(auto target : targets.getTargets()) {
            if (auto ai = target.getComponent<AIController>()) {
                if (auto transform = target.getComponent<sp::Transform>()) {
                    ai->orders = AIOrder::DefendLocation;
                    ai->order_target_location = transform->getPosition();
                }
            }
        }
    }))->setTextSize(20)->setSize(GuiElement::GuiSizeMax, 30);
    (new GuiButton(order_layout, "ORDER_STAND_GROUND", tr("Stand ground"), [this]() {
        for(auto obj : targets.getTargets()) {
            auto obj_ptr = obj.getComponent<SpaceObject*>();
            //if (obj_ptr && P<CpuShip>(P<SpaceObject>(*obj_ptr)))
            //    P<CpuShip>(P<SpaceObject>(*obj_ptr))->orderStandGround();
        }
    }))->setTextSize(20)->setSize(GuiElement::GuiSizeMax, 30);
    (new GuiButton(order_layout, "ORDER_ROAMING", tr("Roaming"), [this]() {
        for(auto obj : targets.getTargets()) {
            auto obj_ptr = obj.getComponent<SpaceObject*>();
            //if (obj_ptr && P<CpuShip>(P<SpaceObject>(*obj_ptr)))
            //    P<CpuShip>(P<SpaceObject>(*obj_ptr))->orderRoaming();
        }
    }))->setTextSize(20)->setSize(GuiElement::GuiSizeMax, 30);
    (new GuiButton(order_layout, "ORDER_IDLE", tr("Idle"), [this]() {
        for(auto obj : targets.getTargets()) {
            auto obj_ptr = obj.getComponent<SpaceObject*>();
            //if (obj_ptr && P<CpuShip>(P<SpaceObject>(*obj_ptr)))
            //    P<CpuShip>(P<SpaceObject>(*obj_ptr))->orderIdle();
        }
    }))->setTextSize(20)->setSize(GuiElement::GuiSizeMax, 30);
    (new GuiLabel(order_layout, "ORDERS_LABEL", tr("Orders:"), 20))->addBackground()->setSize(GuiElement::GuiSizeMax, 30);

    chat_layer = new GuiElement(this, "");
    chat_layer->setPosition(0, 0)->setSize(GuiElement::GuiSizeMax, GuiElement::GuiSizeMax);

    player_tweak_dialog = new GuiObjectTweak(this, TW_Player);
    player_tweak_dialog->hide();
    ship_tweak_dialog = new GuiObjectTweak(this, TW_Ship);
    ship_tweak_dialog->hide();
    object_tweak_dialog = new GuiObjectTweak(this, TW_Object);
    object_tweak_dialog->hide();
    station_tweak_dialog = new GuiObjectTweak(this, TW_Station);
    station_tweak_dialog->hide();
    jammer_tweak_dialog = new GuiObjectTweak(this, TW_Jammer);
    jammer_tweak_dialog->hide();
    asteroid_tweak_dialog = new GuiObjectTweak(this, TW_Asteroid);
    asteroid_tweak_dialog->hide();

    global_message_entry = new GuiGlobalMessageEntryView(this);
    global_message_entry->hide();
    object_creation_view = new GuiObjectCreationView(this);
    object_creation_view->hide();

    message_frame = new GuiPanel(this, "");
    message_frame->setPosition(0, 0, sp::Alignment::TopCenter)->setSize(900, 230)->hide();

    message_text = new GuiScrollText(message_frame, "", "");
    message_text->setTextSize(20)->setPosition(20, 20, sp::Alignment::TopLeft)->setSize(900 - 40, 200 - 40);
    message_close_button = new GuiButton(message_frame, "", tr("button", "Close"), []() {
        if (!gameGlobalInfo->gm_messages.empty())
        {
            gameGlobalInfo->gm_messages.pop_front();
        }

    });
    message_close_button->setTextSize(30)->setPosition(-20, -20, sp::Alignment::BottomRight)->setSize(300, 30);
}

//due to a suspected compiler bug this deconstructor needs to be explicitly defined
GameMasterScreen::~GameMasterScreen()
{
}

void GameMasterScreen::update(float delta)
{
    float mouse_wheel_delta = keys.zoom_in.getValue() - keys.zoom_out.getValue();
    if (mouse_wheel_delta != 0.0f)
    {
        float view_distance = main_radar->getDistance() * (1.0f - (mouse_wheel_delta * 0.1f));
        if (view_distance > 100000)
            view_distance = 100000;
        if (view_distance < 5000)
            view_distance = 5000;
        main_radar->setDistance(view_distance);
        if (view_distance < 10000)
            main_radar->shortRange();
        else
            main_radar->longRange();
    }

    if (keys.gm_delete.getDown())
    {
        for(auto obj : targets.getTargets())
            obj.destroy();
    }
    if (keys.gm_clipboardcopy.getDown())
    {
        Clipboard::setClipboard(getScriptExport(false));
    }

    if (keys.escape.getDown())
    {
        destroy();
        returnToShipSelection(getRenderLayer());
    }
    if (keys.pause.getDown())
    {
        if (game_server)
            engine->setGameSpeed(0.0);
    }
    if (engine->getGameSpeed() == 0.0f) {
        pause_button->setValue(true);
    } else {
        pause_button->setValue(false);
    }

    bool has_object = false;
    bool has_cpu_ship = false;
    bool has_player_ship = false;

    // Add and remove entries from the player ship list.
    for(auto [entity, pc] : sp::ecs::Query<PlayerControl>())
    {
        string ship_name;
        if (auto tn = entity.getComponent<TypeName>())
            ship_name += " " + tn->type_name;
        if (auto cs = entity.getComponent<CallSign>())
            ship_name += " " + cs->callsign;
        if (player_ship_selector->indexByValue(entity.toString()) == -1)
        {
            player_ship_selector->addEntry(ship_name, entity.toString());
        } else {
            player_ship_selector->setEntryName(player_ship_selector->indexByValue(entity.toString()), ship_name);
        }

        auto transmitter = entity.getComponent<CommsTransmitter>();
        if (transmitter && (transmitter->state == CommsTransmitter::State::BeingHailedByGM || transmitter->state == CommsTransmitter::State::ChannelOpenGM))
        {
            auto cd = getChatDialog(entity);
            if (!cd->isVisible())
            {
                auto transform = entity.getComponent<sp::Transform>();
                if (transform)
                    cd->show()->setPosition(main_radar->worldToScreen(transform->getPosition()))->setSize(300, 300);
            }
        }
    }
    for(int n=0; n<player_ship_selector->entryCount(); n++) {
        if (!sp::ecs::Entity::fromString(player_ship_selector->getEntryValue(n)))
            player_ship_selector->removeEntry(n);
    }

    // Record object type.
    for(auto obj : targets.getTargets())
    {
        auto obj_ptr = obj.getComponent<SpaceObject*>();
        if (obj_ptr && *obj_ptr) {
            P<SpaceObject> obj = *obj_ptr;
            has_object = true;
            //if (P<CpuShip>(obj))
            //    has_cpu_ship = true;
            //else if (P<PlayerSpaceship>(obj))
            //    has_player_ship = true;
        }
    }

    // Show player ship selector only if there are player ships.
    player_ship_selector->setVisible(player_ship_selector->entryCount() > 0);

    // Show tweak button.
    tweak_button->setVisible(has_object);

    order_layout->setVisible(has_cpu_ship);
    player_comms_hail->setVisible(has_player_ship);

    // Update mission clock
    info_clock->setValue(gameGlobalInfo->getMissionTime());

    std::unordered_map<string, string> selection_info;

    // For each selected object, determine and report their type.
    for(auto obj : targets.getTargets())
    {
        /*TODO
        std::unordered_map<string, string> info = obj->getGMInfo();
        for(std::unordered_map<string, string>::iterator i = info.begin(); i != info.end(); i++)
        {
            if (selection_info.find(i->first) == selection_info.end())
            {
                selection_info[i->first] = i->second;
            }
            else if (selection_info[i->first] != i->second)
            {
                selection_info[i->first] = tr("*mixed*");
            }
        }
        */
    }

    if (targets.getTargets().size() == 1)
    {
        //TODO selection_info[trMark("gm_info", "Position")] = string(targets.getTargets()[0]->getPosition().x, 0) + "," + string(targets.getTargets()[0]->getPosition().y, 0);
    }

    unsigned int cnt = 0;
    for(std::unordered_map<string, string>::iterator i = selection_info.begin(); i != selection_info.end(); i++)
    {
        if (cnt == info_items.size())
        {
            info_items.push_back(new GuiKeyValueDisplay(info_layout, "INFO_" + string(cnt), 0.5, i->first, i->second));
            info_items[cnt]->setSize(GuiElement::GuiSizeMax, 30);
        }else{
            info_items[cnt]->show();
            info_items[cnt]->setKey(tr("gm_info", i->first))->setValue(i->second);
        }
        cnt++;
    }
    while(cnt < info_items.size())
    {
        info_items[cnt]->hide();
        cnt++;
    }

    bool gm_functions_changed = gm_script_options->entryCount() != int(gameGlobalInfo->gm_callback_functions.size());
    auto it = gameGlobalInfo->gm_callback_functions.begin();
    for(int n=0; !gm_functions_changed && n<gm_script_options->entryCount(); n++)
    {
        if (gm_script_options->getEntryName(n) != it->name)
            gm_functions_changed = true;
        it++;
    }
    if (gm_functions_changed)
    {
        gm_script_options->setOptions({});
        for(const GMScriptCallback& callback : gameGlobalInfo->gm_callback_functions)
        {
            gm_script_options->addEntry(callback.name, callback.name);
        }
    }

    if (!gameGlobalInfo->gm_messages.empty())
    {
        GMMessage* message = &gameGlobalInfo->gm_messages.front();
        message_text->setText(message->text);
        message_frame->show();
    } else {
        message_frame->hide();
    }

    if (gameGlobalInfo->on_gm_click)
    {
        create_button->hide();
        object_creation_view->hide();
        cancel_action_button->show();
    }
    else
    {
        create_button->show();
        cancel_action_button->hide();
    }
}

void GameMasterScreen::onMouseDown(sp::io::Pointer::Button button, glm::vec2 position)
{
    if (click_and_drag_state != CD_None)
        return;
    if (button == sp::io::Pointer::Button::Right)
    {
        click_and_drag_state = CD_DragViewOrOrder;
    }
    else
    {
        if (gameGlobalInfo->on_gm_click)
        {
            gameGlobalInfo->on_gm_click(position);
        }else{
            click_and_drag_state = CD_BoxSelect;

            float min_drag_distance = main_radar->getDistance() / 450 * 10;

            for(auto obj : targets.getTargets())
            {
                if (auto transform = obj.getComponent<sp::Transform>())
                    if (glm::length(transform->getPosition() - position) < min_drag_distance)
                        click_and_drag_state = CD_DragObjects;
            }
        }
    }
    drag_start_position = position;
    drag_previous_position = position;
}

void GameMasterScreen::onMouseDrag(glm::vec2 position)
{
    switch(click_and_drag_state)
    {
    case CD_DragViewOrOrder:
    case CD_DragView:
        click_and_drag_state = CD_DragView;
        main_radar->setViewPosition(main_radar->getViewPosition() - (position - drag_previous_position));
        position -= (position - drag_previous_position);
        break;
    case CD_DragObjects:
        for(auto obj : targets.getTargets())
        {
            if (auto transform = obj.getComponent<sp::Transform>())
                transform->setPosition(transform->getPosition() + (position - drag_previous_position));
        }
        break;
    case CD_BoxSelect:
        {
            auto p0 = main_radar->worldToScreen(drag_start_position);
            auto p1 = main_radar->worldToScreen(position);
            if (p0.x > p1.x) std::swap(p0.x, p1.x);
            if (p0.y > p1.y) std::swap(p0.y, p1.y);
            box_selection_overlay->show();
            box_selection_overlay->setPosition(p0, sp::Alignment::TopLeft);
            box_selection_overlay->setSize(p1 - p0);
        }
        break;
    default:
        break;
    }
    drag_previous_position = position;
}

void GameMasterScreen::onMouseUp(glm::vec2 position)
{
    switch(click_and_drag_state)
    {
    case CD_DragViewOrOrder:
        {
            //Right click
            bool shift_down = SDL_GetModState() & KMOD_SHIFT;
            P<SpaceObject> target;
            for(auto entity : sp::CollisionSystem::queryArea(position, position))
            {
                auto ptr = entity.getComponent<SpaceObject*>();
                if (!ptr || !*ptr) continue;
                P<SpaceObject> space_object = *ptr;
                if (space_object)
                {
                    if (!target || glm::length(position - space_object->getPosition()) < glm::length(position - target->getPosition()))
                        target = space_object;
                }
            }

            glm::vec2 upper_bound(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
            glm::vec2 lower_bound(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            for(auto obj : targets.getTargets())
            {
                auto obj_ptr = obj.getComponent<SpaceObject*>();
                if (!obj_ptr) continue;
                //P<CpuShip> cpu_ship = P<SpaceObject>(*obj_ptr);
                //if (!cpu_ship)
                //    continue;

                //lower_bound.x = std::min(lower_bound.x, cpu_ship->getPosition().x);
                //lower_bound.y = std::min(lower_bound.y, cpu_ship->getPosition().y);
                //upper_bound.x = std::max(upper_bound.x, cpu_ship->getPosition().x);
                //upper_bound.y = std::max(upper_bound.y, cpu_ship->getPosition().y);
            }
            glm::vec2 objects_center = (upper_bound + lower_bound) / 2.0f;

            for(auto obj : targets.getTargets())
            {
                /*
                auto obj_ptr = obj.getComponent<SpaceObject*>();
                if (!obj_ptr) continue;
                P<CpuShip> cpu_ship = P<SpaceObject>(*obj_ptr);
                P<WormHole> wormhole = P<SpaceObject>(*obj_ptr);
                if (cpu_ship)
                {
                    if (target && target != cpu_ship && target->canBeTargetedBy(obj))
                    {
                        if (cpu_ship->isEnemy(target))
                        {
                            cpu_ship->orderAttack(target);
                        }else{
                            auto port = cpu_ship->entity.getComponent<DockingPort>();
                            auto bay = target->entity.getComponent<DockingBay>();
                            if (!shift_down && port && bay && port->canDockOn(*bay) != DockingStyle::None)
                                cpu_ship->orderDock(target);
                            else
                                cpu_ship->orderDefendTarget(target);
                        }
                    }else{
                        if (shift_down)
                            cpu_ship->orderFlyTowardsBlind(position + (cpu_ship->getPosition() - objects_center));
                        else
                            cpu_ship->orderFlyTowards(position + (cpu_ship->getPosition() - objects_center));
                    }
                }
                else if (wormhole)
                {
                    wormhole->setTargetPosition(position);
                }
                */
            }
        }
        break;
    case CD_BoxSelect:
        {
            bool shift_down = SDL_GetModState() & KMOD_SHIFT;
            bool ctrl_down = SDL_GetModState() & KMOD_CTRL;
            bool alt_down = SDL_GetModState() & KMOD_ALT;
            std::vector<sp::ecs::Entity> space_objects;
            for(auto entity : sp::CollisionSystem::queryArea(drag_start_position, position))
            {
                auto ptr = entity.getComponent<SpaceObject*>();
                if (!ptr || !*ptr) continue;
                P<SpaceObject> obj = *ptr;
                if (P<Zone>(obj))
                    continue;
                if (ctrl_down && !P<ShipTemplateBasedObject>(obj))
                    continue;
                if (alt_down && (!P<SpaceObject>(obj) || (P<SpaceObject>(obj))->getFaction() != faction_selector->getSelectionValue()))
                    continue;
                space_objects.push_back(entity);
            }
            if (shift_down)
            {
                for(auto e : space_objects)
                    targets.add(e);
            } else {
                targets.set(space_objects);
            }


            if (space_objects.size() > 0) {
                //TODO: faction_selector->setSelectionIndex(space_objects[0]->getFactionId());
            }
        }
        break;
    default:
        break;
    }
    click_and_drag_state = CD_None;
    box_selection_overlay->hide();
}

std::vector<sp::ecs::Entity> GameMasterScreen::getSelection()
{
    return targets.getTargets();
}

GameMasterChatDialog* GameMasterScreen::getChatDialog(sp::ecs::Entity entity)
{
    //TODO: clean up old dialogs that are no longer valid.
    for(auto d : chat_dialog_per_ship)
        if (d->player == entity)
            return d;
    auto dialog = new GameMasterChatDialog(chat_layer, main_radar, entity);
    chat_dialog_per_ship.push_back(dialog);
    return dialog;
}

string GameMasterScreen::getScriptExport(bool selected_only)
{
    string output;
    std::vector<sp::ecs::Entity> objs;
    if (selected_only)
    {
        objs = targets.getTargets();
    }else{
        foreach(SpaceObject, obj, space_object_list)
            objs.push_back(obj->entity);
    }

    for(auto e : objs)
    {
        string line; //TODO = obj->getExportLine();
        if (line == "")
            continue;
        output += "    " + line + "\n";
    }
    return output;
}
