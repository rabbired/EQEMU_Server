#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>

#include <sstream>
#include <list>
#include <map>

#include "../common/timer.h"
#include "../common/eqemu_logsys.h"
#include "../common/classes.h"
#include "../common/rulesys.h"
#include "lua_parser.h"
#include "lua_item.h"
#include "lua_iteminst.h"
#include "lua_client.h"
#include "lua_npc.h"
#include "lua_entity_list.h"
#include "quest_parser_collection.h"
#include "questmgr.h"
#include "qglobals.h"
#include "encounter.h"
#include "lua_encounter.h"
#include "data_bucket.h"

struct Events { };
struct Factions { };
struct Slots { };
struct Materials { };
struct ClientVersions { };
struct Appearances { };
struct Classes { };
struct Skills { };
struct BodyTypes { };
struct Filters { };
struct MessageTypes { };
struct Rule { };
struct Journal_SpeakMode { };
struct Journal_Mode { };

struct lua_registered_event {
	std::string encounter_name;
	luabind::adl::object lua_reference;
	QuestEventID event_id;
};

extern std::map<std::string, std::list<lua_registered_event>> lua_encounter_events_registered;
extern std::map<std::string, bool> lua_encounters_loaded;
extern std::map<std::string, Encounter *> lua_encounters;

extern void MapOpcodes();
extern void ClearMappedOpcode(EmuOpcode op);

void unregister_event(std::string package_name, std::string name, int evt);

void load_encounter(std::string name) {
	if(lua_encounters_loaded.count(name) > 0)
		return;
	auto enc = new Encounter(name.c_str());
	entity_list.AddEncounter(enc);
	lua_encounters[name] = enc;
	lua_encounters_loaded[name] = true;
	parse->EventEncounter(EVENT_ENCOUNTER_LOAD, name, "", 0);
}

void load_encounter_with_data(std::string name, std::string info_str) {
	if(lua_encounters_loaded.count(name) > 0)
		return;
	auto enc = new Encounter(name.c_str());
	entity_list.AddEncounter(enc);
	lua_encounters[name] = enc;
	lua_encounters_loaded[name] = true;
	std::vector<EQEmu::Any> info_ptrs;
	info_ptrs.push_back(&info_str);
	parse->EventEncounter(EVENT_ENCOUNTER_LOAD, name, "", 0, &info_ptrs);
}

void unload_encounter(std::string name) {
	if(lua_encounters_loaded.count(name) == 0)
		return;

	auto liter = lua_encounter_events_registered.begin();
	while(liter != lua_encounter_events_registered.end()) {
		std::list<lua_registered_event> &elist = liter->second;
		auto iter = elist.begin();
		while(iter != elist.end()) {
			if((*iter).encounter_name.compare(name) == 0) {
				iter = elist.erase(iter);
			} else {
				++iter;
			}
		}

		if(elist.size() == 0) {
			lua_encounter_events_registered.erase(liter++);
		} else {
			++liter;
		}
	}

	lua_encounters[name]->Depop();
	lua_encounters.erase(name);
	lua_encounters_loaded.erase(name);
	parse->EventEncounter(EVENT_ENCOUNTER_UNLOAD, name, "", 0);
}

void unload_encounter_with_data(std::string name, std::string info_str) {
	if(lua_encounters_loaded.count(name) == 0)
		return;

	auto liter = lua_encounter_events_registered.begin();
	while(liter != lua_encounter_events_registered.end()) {
		std::list<lua_registered_event> &elist = liter->second;
		auto iter = elist.begin();
		while(iter != elist.end()) {
			if((*iter).encounter_name.compare(name) == 0) {
				iter = elist.erase(iter);
			}
			else {
				++iter;
			}
		}

		if(elist.size() == 0) {
			lua_encounter_events_registered.erase(liter++);
		}
		else {
			++liter;
		}
	}

	lua_encounters[name]->Depop();
	lua_encounters.erase(name);
	lua_encounters_loaded.erase(name);
	std::vector<EQEmu::Any> info_ptrs;
	info_ptrs.push_back(&info_str);
	parse->EventEncounter(EVENT_ENCOUNTER_UNLOAD, name, "", 0, &info_ptrs);
}

void register_event(std::string package_name, std::string name, int evt, luabind::adl::object func) {
	if(lua_encounters_loaded.count(name) == 0)
		return;

	unregister_event(package_name, name, evt);

	lua_registered_event e;
	e.encounter_name = name;
	e.lua_reference = func;
	e.event_id = static_cast<QuestEventID>(evt);

	auto liter = lua_encounter_events_registered.find(package_name);
	if(liter == lua_encounter_events_registered.end()) {
		std::list<lua_registered_event> elist;
		elist.push_back(e);
		lua_encounter_events_registered[package_name] = elist;
	} else {
		std::list<lua_registered_event> &elist = liter->second;
		elist.push_back(e);
	}
}

void unregister_event(std::string package_name, std::string name, int evt) {
	auto liter = lua_encounter_events_registered.find(package_name);
	if(liter != lua_encounter_events_registered.end()) {
		std::list<lua_registered_event> elist = liter->second;
		auto iter = elist.begin();
		while(iter != elist.end()) {
			if(iter->event_id == evt && iter->encounter_name.compare(name) == 0) {
				iter = elist.erase(iter);
				break;
			}
			++iter;
		}
		lua_encounter_events_registered[package_name] = elist;
	}
}

void register_npc_event(std::string name, int evt, int npc_id, luabind::adl::object func) {
	if(luabind::type(func) == LUA_TFUNCTION) {
		std::stringstream package_name;
		package_name << "npc_" << npc_id;

		register_event(package_name.str(), name, evt, func);
	}
}

void register_npc_event(int evt, int npc_id, luabind::adl::object func) {
	std::string name = quest_manager.GetEncounter();
	register_npc_event(name, evt, npc_id, func);
}

void unregister_npc_event(std::string name, int evt, int npc_id) {
	std::stringstream package_name;
	package_name << "npc_" << npc_id;

	unregister_event(package_name.str(), name, evt);
}

void unregister_npc_event(int evt, int npc_id) {
	std::string name = quest_manager.GetEncounter();
	unregister_npc_event(name, evt, npc_id);
}

void register_player_event(std::string name, int evt, luabind::adl::object func) {
	if(luabind::type(func) == LUA_TFUNCTION) {
		register_event("player", name, evt, func);
	}
}

void register_player_event(int evt, luabind::adl::object func) {
	std::string name = quest_manager.GetEncounter();
	register_player_event(name, evt, func);
}

void unregister_player_event(std::string name, int evt) {
	unregister_event("player", name, evt);
}

void unregister_player_event(int evt) {
	std::string name = quest_manager.GetEncounter();
	unregister_player_event(name, evt);
}

void register_item_event(std::string name, int evt, int item_id, luabind::adl::object func) {
	std::string package_name = "item_";
	package_name += std::to_string(item_id);

	if(luabind::type(func) == LUA_TFUNCTION) {
		register_event(package_name, name, evt, func);
	}
}

void register_item_event(int evt, int item_id, luabind::adl::object func) {
	std::string name = quest_manager.GetEncounter();
	register_item_event(name, evt, item_id, func);
}

void unregister_item_event(std::string name, int evt, int item_id) {
	std::string package_name = "item_";
	package_name += std::to_string(item_id);

	unregister_event(package_name, name, evt);
}

void unregister_item_event(int evt, int item_id) {
	std::string name = quest_manager.GetEncounter();
	unregister_item_event(name, evt, item_id);
}

void register_spell_event(std::string name, int evt, int spell_id, luabind::adl::object func) {
	if(luabind::type(func) == LUA_TFUNCTION) {
		std::stringstream package_name;
		package_name << "spell_" << spell_id;

		register_event(package_name.str(), name, evt, func);
	}
}

void register_spell_event(int evt, int spell_id, luabind::adl::object func) {
	std::string name = quest_manager.GetEncounter();
	register_spell_event(name, evt, spell_id, func);
}

void unregister_spell_event(std::string name, int evt, int spell_id) {
	std::stringstream package_name;
	package_name << "spell_" << spell_id;

	unregister_event(package_name.str(), name, evt);
}

void unregister_spell_event(int evt, int spell_id) {
	std::string name = quest_manager.GetEncounter();
	unregister_spell_event(name, evt, spell_id);
}

Lua_Mob lua_spawn2(int npc_type, int grid, int unused, double x, double y, double z, double heading) {
	auto position = glm::vec4(x, y, z, heading);
	return Lua_Mob(quest_manager.spawn2(npc_type, grid, unused, position));
}

Lua_Mob lua_unique_spawn(int npc_type, int grid, int unused, double x, double y, double z, double heading = 0.0) {
	auto position = glm::vec4(x, y, z, heading);
	return Lua_Mob(quest_manager.unique_spawn(npc_type, grid, unused, position));
}

Lua_Mob lua_spawn_from_spawn2(uint32 spawn2_id) {
	return Lua_Mob(quest_manager.spawn_from_spawn2(spawn2_id));
}

void lua_enable_spawn2(int spawn2_id) {
	quest_manager.enable_spawn2(spawn2_id);
}

void lua_disable_spawn2(int spawn2_id) {
	quest_manager.disable_spawn2(spawn2_id);
}

void lua_set_timer(const char *timer, int time_ms) {
	quest_manager.settimerMS(timer, time_ms);
}

void lua_set_timer(const char *timer, int time_ms, Lua_ItemInst inst) {
	quest_manager.settimerMS(timer, time_ms, inst);
}

void lua_set_timer(const char *timer, int time_ms, Lua_Mob mob) {
	quest_manager.settimerMS(timer, time_ms, mob);
}

void lua_set_timer(const char *timer, int time_ms, Lua_Encounter enc) {
	quest_manager.settimerMS(timer, time_ms, enc);
}

void lua_stop_timer(const char *timer) {
	quest_manager.stoptimer(timer);
}

void lua_stop_timer(const char *timer, Lua_ItemInst inst) {
	quest_manager.stoptimer(timer, inst);
}

void lua_stop_timer(const char *timer, Lua_Mob mob) {
	quest_manager.stoptimer(timer, mob);
}

void lua_stop_timer(const char *timer, Lua_Encounter enc) {
	quest_manager.stoptimer(timer, enc);
}

void lua_stop_all_timers() {
	quest_manager.stopalltimers();
}

void lua_stop_all_timers(Lua_ItemInst inst) {
	quest_manager.stopalltimers(inst);
}

void lua_stop_all_timers(Lua_Mob mob) {
	quest_manager.stopalltimers(mob);
}

void lua_stop_all_timers(Lua_Encounter enc) {
	quest_manager.stopalltimers(enc);
}

void lua_pause_timer(const char *timer) {
	quest_manager.pausetimer(timer);

}

void lua_resume_timer(const char *timer) {
	quest_manager.resumetimer(timer);
}

bool lua_is_paused_timer(const char *timer) {
	return quest_manager.ispausedtimer(timer);
}

void lua_depop() {
	quest_manager.depop(0);
}

void lua_depop(int npc_type) {
	quest_manager.depop(npc_type);
}

void lua_depop_with_timer() {
	quest_manager.depop_withtimer(0);
}

void lua_depop_with_timer(int npc_type) {
	quest_manager.depop_withtimer(npc_type);
}

void lua_depop_all() {
	quest_manager.depopall(0);
}

void lua_depop_all(int npc_type) {
	quest_manager.depopall(npc_type);
}

void lua_depop_zone(bool start_spawn_status) {
	quest_manager.depopzone(start_spawn_status);
}

void lua_repop_zone() {
	quest_manager.repopzone();
}

bool lua_is_disc_tome(int item_id) {
	return quest_manager.isdisctome(item_id);
}

void lua_safe_move() {
	quest_manager.safemove();
}

void lua_rain(int weather) {
	quest_manager.rain(weather);
}

void lua_snow(int weather) {
	quest_manager.rain(weather);
}

int lua_scribe_spells(int max) {
	return quest_manager.scribespells(max);
}

int lua_scribe_spells(int max, int min) {
	return quest_manager.scribespells(max, min);
}

int lua_train_discs(int max) {
	return quest_manager.traindiscs(max);
}

int lua_train_discs(int max, int min) {
	return quest_manager.traindiscs(max, min);
}

void lua_set_sky(int sky) {
	quest_manager.setsky(sky);
}

void lua_set_guild(int guild_id, int rank) {
	quest_manager.setguild(guild_id, rank);
}

void lua_create_guild(const char *name, const char *leader) {
	quest_manager.CreateGuild(name, leader);
}

void lua_set_time(int hour, int min) {
	quest_manager.settime(hour, min, true);
}

void lua_set_time(int hour, int min, bool update_world) {
	quest_manager.settime(hour, min, update_world);
}

void lua_signal(int npc_id, int signal_id) {
	quest_manager.signalwith(npc_id, signal_id);
}

void lua_signal(int npc_id, int signal_id, int wait) {
	quest_manager.signalwith(npc_id, signal_id, wait);
}

void lua_set_global(const char *name, const char *value, int options, const char *duration) {
	quest_manager.setglobal(name, value, options, duration);
}

void lua_target_global(const char *name, const char *value, const char *duration, int npc_id, int char_id, int zone_id) {
	quest_manager.targlobal(name, value, duration, npc_id, char_id, zone_id);
}

void lua_delete_global(const char *name) {
	quest_manager.delglobal(name);
}

void lua_start(int wp) {
	quest_manager.start(wp);
}

void lua_stop() {
	quest_manager.stop();
}

void lua_pause(int duration) {
	quest_manager.pause(duration);
}

void lua_move_to(float x, float y, float z) {
	quest_manager.moveto(glm::vec4(x, y, z, 0.0f), false);
}

void lua_move_to(float x, float y, float z, float h) {
	quest_manager.moveto(glm::vec4(x, y, z, h), false);
}

void lua_move_to(float x, float y, float z, float h, bool save_guard_spot) {
	quest_manager.moveto(glm::vec4(x, y, z, h), save_guard_spot);
}

void lua_path_resume() {
	quest_manager.resume();
}

void lua_set_next_hp_event(int hp) {
	quest_manager.setnexthpevent(hp);
}

void lua_set_next_inc_hp_event(int hp) {
	quest_manager.setnextinchpevent(hp);
}

void lua_respawn(int npc_type, int grid) {
	quest_manager.respawn(npc_type, grid);
}

void lua_set_proximity(float min_x, float max_x, float min_y, float max_y) {
	quest_manager.set_proximity(min_x, max_x, min_y, max_y);
}

void lua_set_proximity(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z) {
	quest_manager.set_proximity(min_x, max_x, min_y, max_y, min_z, max_z);
}

void lua_set_proximity(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z, bool say) {
	quest_manager.set_proximity(min_x, max_x, min_y, max_y, min_z, max_z, say);
}

void lua_clear_proximity() {
	quest_manager.clear_proximity();
}

void lua_enable_proximity_say() {
	quest_manager.enable_proximity_say();
}

void lua_disable_proximity_say() {
	quest_manager.disable_proximity_say();
}

void lua_set_anim(int npc_type, int anim_num) {
	quest_manager.setanim(npc_type, anim_num);
}

void lua_spawn_condition(const char *zone, uint32 instance_id, int condition_id, int value) {
	quest_manager.spawn_condition(zone, instance_id, condition_id, value);
}

int lua_get_spawn_condition(const char *zone, uint32 instance_id, int condition_id) {
	return quest_manager.get_spawn_condition(zone, instance_id, condition_id);
}

void lua_toggle_spawn_event(int event_id, bool enable, bool strict, bool reset) {
	quest_manager.toggle_spawn_event(event_id, enable, strict, reset);
}

void lua_summon_buried_player_corpse(uint32 char_id, float x, float y, float z, float h) {
	quest_manager.summonburiedplayercorpse(char_id, glm::vec4(x, y, z, h));
}

void lua_summon_all_player_corpses(uint32 char_id, float x, float y, float z, float h) {
	quest_manager.summonallplayercorpses(char_id, glm::vec4(x, y, z, h));
}

int lua_get_player_buried_corpse_count(uint32 char_id) {
	return quest_manager.getplayerburiedcorpsecount(char_id);
}

bool lua_bury_player_corpse(uint32 char_id) {
	return quest_manager.buryplayercorpse(char_id);
}

void lua_task_selector(luabind::adl::object table) {
	if(luabind::type(table) != LUA_TTABLE) {
		return;
	}

	int tasks[MAXCHOOSERENTRIES] = { 0 };
	int count = 0;

	for(int i = 1; i <= MAXCHOOSERENTRIES; ++i) {
		auto cur = table[i];
		int cur_value = 0;
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				cur_value = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		} else {
			count = i - 1;
			break;
		}

		tasks[i - 1] = cur_value;
	}
	quest_manager.taskselector(count, tasks);
}

void lua_task_set_selector(int task_set) {
	quest_manager.tasksetselector(task_set);
}

void lua_enable_task(luabind::adl::object table) {
	if(luabind::type(table) != LUA_TTABLE) {
		return;
	}

	int tasks[MAXCHOOSERENTRIES] = { 0 };
	int count = 0;

	for(int i = 1; i <= MAXCHOOSERENTRIES; ++i) {
		auto cur = table[i];
		int cur_value = 0;
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				cur_value = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		} else {
			count = i - 1;
			break;
		}

		tasks[i - 1] = cur_value;
	}

	quest_manager.enabletask(count, tasks);
}

void lua_disable_task(luabind::adl::object table) {
	if(luabind::type(table) != LUA_TTABLE) {
		return;
	}

	int tasks[MAXCHOOSERENTRIES] = { 0 };
	int count = 0;

	for(int i = 1; i <= MAXCHOOSERENTRIES; ++i) {
		auto cur = table[i];
		int cur_value = 0;
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				cur_value = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		} else {
			count = i - 1;
			break;
		}

		tasks[i - 1] = cur_value;
	}

	quest_manager.disabletask(count, tasks);
}

bool lua_is_task_enabled(int task) {
	return quest_manager.istaskenabled(task);
}

bool lua_is_task_active(int task) {
	return quest_manager.istaskactive(task);
}

bool lua_is_task_activity_active(int task, int activity) {
	return quest_manager.istaskactivityactive(task, activity);
}

int lua_get_task_activity_done_count(int task, int activity) {
	return quest_manager.gettaskactivitydonecount(task, activity);
}

void lua_update_task_activity(int task, int activity, int count) {
	quest_manager.updatetaskactivity(task, activity, count);
}

void lua_reset_task_activity(int task, int activity) {
	quest_manager.resettaskactivity(task, activity);
}

void lua_task_explored_area(int explore_id) {
	quest_manager.taskexploredarea(explore_id);
}

void lua_assign_task(int task_id) {
	quest_manager.assigntask(task_id);
}

void lua_fail_task(int task_id) {
	quest_manager.failtask(task_id);
}

int lua_task_time_left(int task_id) {
	return quest_manager.tasktimeleft(task_id);
}

int lua_is_task_completed(int task_id) {
	return quest_manager.istaskcompleted(task_id);
}

int lua_enabled_task_count(int task_set) {
	return quest_manager.enabledtaskcount(task_set);
}

int lua_first_task_in_set(int task_set) {
	return quest_manager.firsttaskinset(task_set);
}

int lua_last_task_in_set(int task_set) {
	return quest_manager.lasttaskinset(task_set);
}

int lua_next_task_in_set(int task_set, int task_id) {
	return quest_manager.nexttaskinset(task_set, task_id);
}

int lua_active_speak_task() {
	return quest_manager.activespeaktask();
}

int lua_active_speak_activity(int task_id) {
	return quest_manager.activespeakactivity(task_id);
}

int lua_active_tasks_in_set(int task_set) {
	return quest_manager.activetasksinset(task_set);
}

int lua_completed_tasks_in_set(int task_set) {
	return quest_manager.completedtasksinset(task_set);
}

bool lua_is_task_appropriate(int task) {
	return quest_manager.istaskappropriate(task);
}

void lua_popup(const char *title, const char *text, uint32 id, uint32 buttons, uint32 duration) {
	quest_manager.popup(title, text, id, buttons, duration);
}

void lua_clear_spawn_timers() {
	quest_manager.clearspawntimers();
}

void lua_zone_emote(int type, const char *str) {
	quest_manager.ze(type, str);
}

void lua_world_emote(int type, const char *str) {
	quest_manager.we(type, str);
}

int lua_get_level(int type) {
	return quest_manager.getlevel(type);
}

void lua_create_ground_object(uint32 item_id, float x, float y, float z, float h) {
	quest_manager.CreateGroundObject(item_id, glm::vec4(x, y, z, h));
}

void lua_create_ground_object(uint32 item_id, float x, float y, float z, float h, uint32 decay_time) {
	quest_manager.CreateGroundObject(item_id, glm::vec4(x, y, z, h), decay_time);
}

void lua_create_ground_object_from_model(const char *model, float x, float y, float z, float h) {
	quest_manager.CreateGroundObjectFromModel(model, glm::vec4(x, y, z, h));
}

void lua_create_ground_object_from_model(const char *model, float x, float y, float z, float h, int type) {
	quest_manager.CreateGroundObjectFromModel(model, glm::vec4(x, y, z, h), type);
}

void lua_create_ground_object_from_model(const char *model, float x, float y, float z, float h, int type, uint32 decay_time) {
	quest_manager.CreateGroundObjectFromModel(model, glm::vec4(x, y, z, h), type, decay_time);
}

void lua_create_door(const char *model, float x, float y, float z, float h, int open_type, int size) {
	quest_manager.CreateDoor(model, x, y, z, h, open_type, size);
}

void lua_modify_npc_stat(const char *id, const char *value) {
	quest_manager.ModifyNPCStat(id, value);
}

int lua_collect_items(uint32 item_id, bool remove) {
	return quest_manager.collectitems(item_id, remove);
}

void lua_update_spawn_timer(uint32 id, uint32 new_time) {
	quest_manager.UpdateSpawnTimer(id, new_time);
}

void lua_merchant_set_item(uint32 npc_id, uint32 item_id) {
	quest_manager.MerchantSetItem(npc_id, item_id);
}

void lua_merchant_set_item(uint32 npc_id, uint32 item_id, uint32 quantity) {
	quest_manager.MerchantSetItem(npc_id, item_id, quantity);
}

int lua_merchant_count_item(uint32 npc_id, uint32 item_id) {
	return quest_manager.MerchantCountItem(npc_id, item_id);
}

std::string lua_item_link(int item_id) {
	char text[250] = { 0 };

	return quest_manager.varlink(text, item_id);
}

std::string lua_say_link(const char *phrase, bool silent, const char *link_name) {
	char text[256] = { 0 };
	strncpy(text, phrase, 255);

	return quest_manager.saylink(text, silent, link_name);
}

std::string lua_say_link(const char *phrase, bool silent) {
	char text[256] = { 0 };
	strncpy(text, phrase, 255);

	return quest_manager.saylink(text, silent, text);
}

std::string lua_say_link(const char *phrase) {
	char text[256] = { 0 };
	strncpy(text, phrase, 255);

	return quest_manager.saylink(text, false, text);
}

void lua_set_rule(std::string rule_name, std::string rule_value) {
	RuleManager::Instance()->SetRule(rule_name.c_str(), rule_value.c_str());
}

std::string lua_get_rule(std::string rule_name) {
	std::string rule_value;
	RuleManager::Instance()->GetRule(rule_name.c_str(), rule_value);
	return rule_value;
}

std::string lua_get_data(std::string bucket_key) {
	return DataBucket::GetData(bucket_key);
}

std::string lua_get_data_expires(std::string bucket_key) {
	return DataBucket::GetDataExpires(bucket_key);
}

void lua_set_data(std::string bucket_key, std::string bucket_value) {
	DataBucket::SetData(bucket_key, bucket_value);
}

void lua_set_data(std::string bucket_key, std::string bucket_value, std::string expires_at) {
	DataBucket::SetData(bucket_key, bucket_value, expires_at);
}

bool lua_delete_data(std::string bucket_key) {
	return DataBucket::DeleteData(bucket_key);
}

const char *lua_get_guild_name_by_id(uint32 guild_id) {
	return quest_manager.getguildnamebyid(guild_id);
}

uint32 lua_create_instance(const char *zone, uint32 version, uint32 duration) {
	return quest_manager.CreateInstance(zone, version, duration);
}

void lua_destroy_instance(uint32 instance_id) {
	quest_manager.DestroyInstance(instance_id);
}

void lua_update_instance_timer(uint16 instance_id, uint32 new_duration) {
	quest_manager.UpdateInstanceTimer(instance_id, new_duration);
}

uint32 lua_get_instance_timer() {
	return quest_manager.GetInstanceTimer();
}

uint32 lua_get_instance_timer_by_id(uint16 instance_id) {
	return quest_manager.GetInstanceTimerByID(instance_id);
}

int lua_get_instance_id(const char *zone, uint32 version) {
	return quest_manager.GetInstanceID(zone, version);
}

int lua_get_instance_id_by_char_id(const char *zone, uint32 version, uint32 char_id) {
	return quest_manager.GetInstanceIDByCharID(zone, version, char_id);
}

void lua_assign_to_instance(uint32 instance_id) {
	quest_manager.AssignToInstance(instance_id);
}

void lua_assign_to_instance_by_char_id(uint32 instance_id, uint32 char_id) {
	quest_manager.AssignToInstanceByCharID(instance_id, char_id);
}

void lua_assign_group_to_instance(uint32 instance_id) {
	quest_manager.AssignGroupToInstance(instance_id);
}

void lua_assign_raid_to_instance(uint32 instance_id) {
	quest_manager.AssignRaidToInstance(instance_id);
}

void lua_remove_from_instance(uint32 instance_id) {
	quest_manager.RemoveFromInstance(instance_id);
}

void lua_remove_from_instance_by_char_id(uint32 instance_id, uint32 char_id) {
	quest_manager.RemoveFromInstanceByCharID(instance_id, char_id);
}

void lua_remove_all_from_instance(uint32 instance_id) {
	quest_manager.RemoveAllFromInstance(instance_id);
}

void lua_flag_instance_by_group_leader(uint32 zone, uint32 version) {
	quest_manager.FlagInstanceByGroupLeader(zone, version);
}

void lua_flag_instance_by_raid_leader(uint32 zone, uint32 version) {
	quest_manager.FlagInstanceByRaidLeader(zone, version);
}

void lua_fly_mode(int flymode) {
	quest_manager.FlyMode(static_cast<GravityBehavior>(flymode));
}

int lua_faction_value() {
	return quest_manager.FactionValue();
}

void lua_check_title(uint32 title_set) {
	quest_manager.checktitle(title_set);
}

void lua_enable_title(uint32 title_set) {
	quest_manager.enabletitle(title_set);
}

void lua_remove_title(uint32 title_set) {
	quest_manager.removetitle(title_set);
}

void lua_wear_change(uint32 slot, uint32 texture) {
	quest_manager.wearchange(slot, texture);
}

void lua_voice_tell(const char *str, uint32 macro_num, uint32 race_num, uint32 gender_num) {
	quest_manager.voicetell(str, macro_num, race_num, gender_num);
}

void lua_send_mail(const char *to, const char *from, const char *subject, const char *message) {
	quest_manager.SendMail(to, from, subject, message);
}

void lua_cross_zone_signal_client_by_char_id(uint32 player_id, int signal) {
	quest_manager.CrossZoneSignalPlayerByCharID(player_id, signal);
}

void lua_cross_zone_signal_client_by_name(const char *player, int signal) {
	quest_manager.CrossZoneSignalPlayerByName(player, signal);
}

void lua_cross_zone_message_player_by_name(uint32 type, const char *player, const char *message) {
	quest_manager.CrossZoneMessagePlayerByName(type, player, message);
}

void lua_cross_zone_set_entity_variable_by_client_name(const char *player, const char *id, const char *m_var) {
	quest_manager.CrossZoneSetEntityVariableByClientName(player, id, m_var);
}

void lua_world_wide_marquee(uint32 type, uint32 priority, uint32 fadein, uint32 fadeout, uint32 duration, const char *message) {
	quest_manager.WorldWideMarquee(type, priority, fadein, fadeout, duration, message);
}

luabind::adl::object lua_get_qglobals(lua_State *L, Lua_NPC npc, Lua_Client client) {
	luabind::adl::object ret = luabind::newtable(L);

	NPC *n = npc;
	Client *c = client;

	std::list<QGlobal> global_map;
	QGlobalCache::GetQGlobals(global_map, n, c, zone);
	auto iter = global_map.begin();
	while(iter != global_map.end()) {
		ret[(*iter).name] = (*iter).value;
		++iter;
	}
	return ret;
}

luabind::adl::object lua_get_qglobals(lua_State *L, Lua_Client client) {
	luabind::adl::object ret = luabind::newtable(L);

	NPC *n = nullptr;
	Client *c = client;

	std::list<QGlobal> global_map;
	QGlobalCache::GetQGlobals(global_map, n, c, zone);
	auto iter = global_map.begin();
	while (iter != global_map.end()) {
		ret[(*iter).name] = (*iter).value;
		++iter;
	}
	return ret;
}

luabind::adl::object lua_get_qglobals(lua_State *L, Lua_NPC npc) {
	luabind::adl::object ret = luabind::newtable(L);

	NPC *n = npc;
	Client *c = nullptr;

	std::list<QGlobal> global_map;
	QGlobalCache::GetQGlobals(global_map, n, c, zone);
	auto iter = global_map.begin();
	while (iter != global_map.end()) {
		ret[(*iter).name] = (*iter).value;
		++iter;
	}
	return ret;
}

luabind::adl::object lua_get_qglobals(lua_State *L) {
	luabind::adl::object ret = luabind::newtable(L);

	NPC *n = nullptr;
	Client *c = nullptr;

	std::list<QGlobal> global_map;
	QGlobalCache::GetQGlobals(global_map, n, c, zone);
	auto iter = global_map.begin();
	while (iter != global_map.end()) {
		ret[(*iter).name] = (*iter).value;
		++iter;
	}
	return ret;
}

Lua_EntityList lua_get_entity_list() {
	return Lua_EntityList(&entity_list);
}

int lua_get_zone_id() {
	if(!zone)
		return 0;

	return zone->GetZoneID();
}

const char *lua_get_zone_long_name() {
	if(!zone)
		return "";

	return zone->GetLongName();
}

const char *lua_get_zone_short_name() {
	if(!zone)
		return "";

	return zone->GetShortName();
}

int lua_get_zone_instance_id() {
	if(!zone)
		return 0;

	return zone->GetInstanceID();
}

int lua_get_zone_instance_version() {
	if(!zone)
		return 0;

	return zone->GetInstanceVersion();
}

luabind::adl::object lua_get_characters_in_instance(lua_State *L, uint16 instance_id) {
	luabind::adl::object ret = luabind::newtable(L);

	std::list<uint32> charid_list;
	uint16 i = 1;
	database.GetCharactersInInstance(instance_id,charid_list);
	auto iter = charid_list.begin();
	while(iter != charid_list.end()) {
		ret[i] = *iter;
		++i;
		++iter;
	}
	return ret;
}

int lua_get_zone_weather() {
	if(!zone)
		return 0;

	return zone->zone_weather;
}

luabind::adl::object lua_get_zone_time(lua_State *L) {
	TimeOfDay_Struct eqTime;
	zone->zone_time.GetCurrentEQTimeOfDay(time(0), &eqTime);

	luabind::adl::object ret = luabind::newtable(L);
	ret["zone_hour"] = eqTime.hour - 1;
	ret["zone_minute"] = eqTime.minute;
	ret["zone_time"] = (eqTime.hour - 1) * 100 + eqTime.minute;
	return ret;
}

void lua_add_area(int id, int type, float min_x, float max_x, float min_y, float max_y, float min_z, float max_z) {
	entity_list.AddArea(id, type, min_x, max_x, min_y, max_y, min_z, max_z);
}

void lua_remove_area(int id) {
	entity_list.RemoveArea(id);
}

void lua_clear_areas() {
	entity_list.ClearAreas();
}

void lua_remove_spawn_point(uint32 spawn2_id) {
	if(zone) {
		LinkedListIterator<Spawn2*> iter(zone->spawn2_list);
		iter.Reset();

		while(iter.MoreElements()) {
			Spawn2* cur = iter.GetData();
			if(cur->GetID() == spawn2_id) {
				cur->ForceDespawn();
				iter.RemoveCurrent(true);
				return;
			}

			iter.Advance();
		}
	}
}

void lua_add_spawn_point(luabind::adl::object table) {
	if(!zone)
		return;

	if(luabind::type(table) == LUA_TTABLE) {
		uint32 spawn2_id;
		uint32 spawngroup_id;
		float x;
		float y;
		float z;
		float heading;
		uint32 respawn;
		uint32 variance;
		uint32 timeleft = 0;
		uint32 grid = 0;
		int condition_id = 0;
		int condition_min_value = 0;
		bool enabled = true;
		int animation = 0;

		auto cur = table["spawn2_id"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				spawn2_id = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["spawngroup_id"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				spawngroup_id = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["x"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				x = luabind::object_cast<float>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["y"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				y = luabind::object_cast<float>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["z"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				z = luabind::object_cast<float>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["heading"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				heading = luabind::object_cast<float>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["respawn"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				respawn = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["variance"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				variance = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
				return;
			}
		} else {
			return;
		}

		cur = table["timeleft"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				timeleft = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		cur = table["grid"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				grid = luabind::object_cast<uint32>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		cur = table["condition_id"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				condition_id = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		cur = table["condition_min_value"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				condition_min_value = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		cur = table["enabled"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				enabled = luabind::object_cast<bool>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		cur = table["animation"];
		if(luabind::type(cur) != LUA_TNIL) {
			try {
				animation = luabind::object_cast<int>(cur);
			} catch(luabind::cast_failed) {
			}
		}

		lua_remove_spawn_point(spawn2_id);

		auto t = new Spawn2(spawn2_id, spawngroup_id, x, y, z, heading, respawn, variance, timeleft, grid,
				    condition_id, condition_min_value, enabled, static_cast<EmuAppearance>(animation));
		zone->spawn2_list.Insert(t);
	}
}

void lua_attack(const char *client_name) {
	quest_manager.attack(client_name);
}

void lua_attack_npc(int entity_id) {
	quest_manager.attacknpc(entity_id);
}

void lua_attack_npc_type(int npc_type) {
	quest_manager.attacknpctype(npc_type);
}

void lua_follow(int entity_id) {
	quest_manager.follow(entity_id, 10);
}

void lua_follow(int entity_id, int distance) {
	quest_manager.follow(entity_id, distance);
}

void lua_stop_follow() {
	quest_manager.sfollow();
}

Lua_Client lua_get_initiator() {
	return quest_manager.GetInitiator();
}

Lua_Mob lua_get_owner() {
	return quest_manager.GetOwner();
}

Lua_ItemInst lua_get_quest_item() {
	return quest_manager.GetQuestItem();
}

std::string lua_get_encounter() {
	return quest_manager.GetEncounter();
}

void lua_map_opcodes() {
	MapOpcodes();
}

void lua_clear_opcode(int op) {
	ClearMappedOpcode(static_cast<EmuOpcode>(op));
}

bool lua_enable_recipe(uint32 recipe_id) {
	return quest_manager.EnableRecipe(recipe_id);
}

bool lua_disable_recipe(uint32 recipe_id) {
	return quest_manager.DisableRecipe(recipe_id);
}

void lua_clear_npctype_cache(int npctype_id) {
	quest_manager.ClearNPCTypeCache(npctype_id);
}

void lua_reloadzonestaticdata() {
	quest_manager.ReloadZoneStaticData();
}

double lua_clock() {
	timeval read_time;
	gettimeofday(&read_time, nullptr);
	uint32 t = read_time.tv_sec * 1000 + read_time.tv_usec / 1000;
	return static_cast<double>(t) / 1000.0;
}

void lua_debug(std::string message) {
	Log(Logs::General, Logs::QuestDebug, message.c_str());
}

void lua_debug(std::string message, int level) {
	if (level < Logs::General || level > Logs::Detail)
		return;

	Log(static_cast<Logs::DebugLevel>(level), Logs::QuestDebug, message.c_str());
}

void lua_update_zone_header(std::string type, std::string value) {
	quest_manager.UpdateZoneHeader(type, value);
}

#define LuaCreateNPCParse(name, c_type, default_value) do { \
	cur = table[#name]; \
	if(luabind::type(cur) != LUA_TNIL) { \
		try { \
			npc_type->name = luabind::object_cast<c_type>(cur); \
		} \
		catch(luabind::cast_failed) { \
			npc_type->size = default_value; \
		} \
	} \
	else { \
		npc_type->size = default_value; \
	} \
} while(0)

#define LuaCreateNPCParseString(name, str_length, default_value) do { \
	cur = table[#name]; \
	if(luabind::type(cur) != LUA_TNIL) { \
		try { \
			std::string tmp = luabind::object_cast<std::string>(cur); \
			strncpy(npc_type->name, tmp.c_str(), str_length); \
		} \
		catch(luabind::cast_failed) { \
			strncpy(npc_type->name, default_value, str_length); \
		} \
	} \
	else { \
		strncpy(npc_type->name, default_value, str_length); \
	} \
} while(0)

void lua_create_npc(luabind::adl::object table, float x, float y, float z, float heading) {
	if(luabind::type(table) != LUA_TTABLE) {
		return;
	}

	auto npc_type = new NPCType;
	memset(npc_type, 0, sizeof(NPCType));


	luabind::adl::index_proxy<luabind::adl::object> cur = table["name"];
	LuaCreateNPCParseString(name, 64, "_");
	LuaCreateNPCParseString(lastname, 64, "");
	LuaCreateNPCParse(current_hp, int32, 30);
	LuaCreateNPCParse(max_hp, int32, 30);
	LuaCreateNPCParse(size, float, 6.0f);
	LuaCreateNPCParse(runspeed, float, 1.25f);
	LuaCreateNPCParse(gender, uint8, 0);
	LuaCreateNPCParse(race, uint16, 1);
	LuaCreateNPCParse(class_, uint8, WARRIOR);
	LuaCreateNPCParse(bodytype, uint8, 0);
	LuaCreateNPCParse(deity, uint8, 0);
	LuaCreateNPCParse(level, uint8, 1);
	LuaCreateNPCParse(npc_id, uint32, 1);
	LuaCreateNPCParse(texture, uint8, 0);
	LuaCreateNPCParse(helmtexture, uint8, 0);
	LuaCreateNPCParse(loottable_id, uint32, 0);
	LuaCreateNPCParse(npc_spells_id, uint32, 0);
	LuaCreateNPCParse(npc_spells_effects_id, uint32, 0);
	LuaCreateNPCParse(npc_faction_id, int32, 0);
	LuaCreateNPCParse(merchanttype, uint32, 0);
	LuaCreateNPCParse(alt_currency_type, uint32, 0);
	LuaCreateNPCParse(adventure_template, uint32, 0);
	LuaCreateNPCParse(trap_template, uint32, 0);
	LuaCreateNPCParse(light, uint8, 0);
	LuaCreateNPCParse(AC, uint32, 0);
	LuaCreateNPCParse(Mana, uint32, 0);
	LuaCreateNPCParse(ATK, uint32, 0);
	LuaCreateNPCParse(STR, uint32, 0);
	LuaCreateNPCParse(STA, uint32, 0);
	LuaCreateNPCParse(DEX, uint32, 0);
	LuaCreateNPCParse(AGI, uint32, 0);
	LuaCreateNPCParse(INT, uint32, 0);
	LuaCreateNPCParse(WIS, uint32, 0);
	LuaCreateNPCParse(CHA, uint32, 0);
	LuaCreateNPCParse(MR, int32, 0);
	LuaCreateNPCParse(FR, int32, 0);
	LuaCreateNPCParse(CR, int32, 0);
	LuaCreateNPCParse(PR, int32, 0);
	LuaCreateNPCParse(DR, int32, 0);
	LuaCreateNPCParse(Corrup, int32, 0);
	LuaCreateNPCParse(PhR, int32, 0);
	LuaCreateNPCParse(haircolor, uint8, 0);
	LuaCreateNPCParse(beardcolor, uint8, 0);
	LuaCreateNPCParse(eyecolor1, uint8, 0);
	LuaCreateNPCParse(eyecolor2, uint8, 0);
	LuaCreateNPCParse(hairstyle, uint8, 0);
	LuaCreateNPCParse(luclinface, uint8, 0);
	LuaCreateNPCParse(beard, uint8, 0);
	LuaCreateNPCParse(drakkin_heritage, uint32, 0);
	LuaCreateNPCParse(drakkin_tattoo, uint32, 0);
	LuaCreateNPCParse(drakkin_details, uint32, 0);
	LuaCreateNPCParse(armor_tint.Head.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Chest.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Arms.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Wrist.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Hands.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Legs.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Feet.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Primary.Color, uint32, 0);
	LuaCreateNPCParse(armor_tint.Secondary.Color, uint32, 0);
	LuaCreateNPCParse(min_dmg, uint32, 2);
	LuaCreateNPCParse(max_dmg, uint32, 4);
	LuaCreateNPCParse(attack_count, int16, 0);
	LuaCreateNPCParseString(special_abilities, 512, "");
	LuaCreateNPCParse(d_melee_texture1, uint16, 0);
	LuaCreateNPCParse(d_melee_texture2, uint16, 0);
	LuaCreateNPCParseString(ammo_idfile, 30, "");
	LuaCreateNPCParse(prim_melee_type, uint8, 0);
	LuaCreateNPCParse(sec_melee_type, uint8, 0);
	LuaCreateNPCParse(ranged_type, uint8, 0);
	LuaCreateNPCParse(hp_regen, int32, 1);
	LuaCreateNPCParse(mana_regen, int32, 1);
	LuaCreateNPCParse(aggroradius, int32, 0);
	LuaCreateNPCParse(assistradius, int32, 0);
	LuaCreateNPCParse(see_invis, uint8, 0);
	LuaCreateNPCParse(see_invis_undead, bool, false);
	LuaCreateNPCParse(see_hide, bool, false);
	LuaCreateNPCParse(see_improved_hide, bool, false);
	LuaCreateNPCParse(qglobal, bool, false);
	LuaCreateNPCParse(npc_aggro, bool, false);
	LuaCreateNPCParse(spawn_limit, uint8, false);
	LuaCreateNPCParse(mount_color, uint8, false);
	LuaCreateNPCParse(attack_speed, float, 0);
	LuaCreateNPCParse(attack_delay, uint8, 30);
	LuaCreateNPCParse(accuracy_rating, int, 0);
	LuaCreateNPCParse(avoidance_rating, int, 0);
	LuaCreateNPCParse(findable, bool, false);
	LuaCreateNPCParse(trackable, bool, false);
	LuaCreateNPCParse(slow_mitigation, int16, 0);
	LuaCreateNPCParse(maxlevel, uint8, 0);
	LuaCreateNPCParse(scalerate, uint32, 0);
	LuaCreateNPCParse(private_corpse, bool, false);
	LuaCreateNPCParse(unique_spawn_by_name, bool, false);
	LuaCreateNPCParse(underwater, bool, false);
	LuaCreateNPCParse(emoteid, uint32, 0);
	LuaCreateNPCParse(spellscale, float, 0);
	LuaCreateNPCParse(healscale, float, 0);
	LuaCreateNPCParse(no_target_hotkey, bool, false);
	LuaCreateNPCParse(raid_target, bool, false);

	NPC* npc = new NPC(npc_type, nullptr, glm::vec4(x, y, z, heading), GravityBehavior::Water);
	npc->GiveNPCTypeData(npc_type);
	entity_list.AddNPC(npc);
}

int random_int(int low, int high) {
	return zone->random.Int(low, high);
}

double random_real(double low, double high) {
	return zone->random.Real(low, high);
}

bool random_roll_int(int required) {
	return zone->random.Roll(required);
}

bool random_roll_real(double required) {
	return zone->random.Roll(required);
}

int random_roll0(int max) {
	return zone->random.Roll0(max);
}

int get_rulei(int rule) {
	return RuleManager::Instance()->GetIntRule((RuleManager::IntType)rule);
}

float get_ruler(int rule) {
	return RuleManager::Instance()->GetRealRule((RuleManager::RealType)rule);
}

bool get_ruleb(int rule) {
	return RuleManager::Instance()->GetBoolRule((RuleManager::BoolType)rule);
}

luabind::scope lua_register_general() {
	return luabind::namespace_("eq")
	[
		luabind::def("load_encounter", &load_encounter),
		luabind::def("unload_encounter", &unload_encounter),
		luabind::def("load_encounter_with_data", &load_encounter_with_data),
		luabind::def("unload_encounter_with_data", &unload_encounter_with_data),
		luabind::def("register_npc_event", (void(*)(std::string, int, int, luabind::adl::object))&register_npc_event),
		luabind::def("register_npc_event", (void(*)(int, int, luabind::adl::object))&register_npc_event),
		luabind::def("unregister_npc_event", (void(*)(std::string, int, int))&unregister_npc_event),
		luabind::def("unregister_npc_event", (void(*)(int, int))&unregister_npc_event),
		luabind::def("register_player_event", (void(*)(std::string, int, luabind::adl::object))&register_player_event),
		luabind::def("register_player_event", (void(*)(int, luabind::adl::object))&register_player_event),
		luabind::def("unregister_player_event", (void(*)(std::string, int))&unregister_player_event),
		luabind::def("unregister_player_event", (void(*)(int))&unregister_player_event),
		luabind::def("register_item_event", (void(*)(std::string, int, int, luabind::adl::object))&register_item_event),
		luabind::def("register_item_event", (void(*)(int, int, luabind::adl::object))&register_item_event),
		luabind::def("unregister_item_event", (void(*)(std::string, int, int))&unregister_item_event),
		luabind::def("unregister_item_event", (void(*)(int, int))&unregister_item_event),
		luabind::def("register_spell_event", (void(*)(std::string, int, int, luabind::adl::object func))&register_spell_event),
		luabind::def("register_spell_event", (void(*)(int, int, luabind::adl::object func))&register_spell_event),
		luabind::def("unregister_spell_event", (void(*)(std::string, int, int))&unregister_spell_event),
		luabind::def("unregister_spell_event", (void(*)(int, int))&unregister_spell_event),
		luabind::def("spawn2", (Lua_Mob(*)(int,int,int,double,double,double,double))&lua_spawn2),
		luabind::def("unique_spawn", (Lua_Mob(*)(int,int,int,double,double,double))&lua_unique_spawn),
		luabind::def("unique_spawn", (Lua_Mob(*)(int,int,int,double,double,double,double))&lua_unique_spawn),
		luabind::def("spawn_from_spawn2", (Lua_Mob(*)(uint32))&lua_spawn_from_spawn2),
		luabind::def("enable_spawn2", &lua_enable_spawn2),
		luabind::def("disable_spawn2", &lua_disable_spawn2),
		luabind::def("set_timer", (void(*)(const char*, int))&lua_set_timer),
		luabind::def("set_timer", (void(*)(const char*, int, Lua_ItemInst))&lua_set_timer),
		luabind::def("set_timer", (void(*)(const char*, int, Lua_Mob))&lua_set_timer),
		luabind::def("set_timer", (void(*)(const char*, int, Lua_Encounter))&lua_set_timer),
		luabind::def("stop_timer", (void(*)(const char*))&lua_stop_timer),
		luabind::def("stop_timer", (void(*)(const char*, Lua_ItemInst))&lua_stop_timer),
		luabind::def("stop_timer", (void(*)(const char*, Lua_Mob))&lua_stop_timer),
		luabind::def("stop_timer", (void(*)(const char*, Lua_Encounter))&lua_stop_timer),
		luabind::def("pause_timer", (void(*)(const char*))&lua_pause_timer),
		luabind::def("resume_timer", (void(*)(const char*))&lua_resume_timer),
		luabind::def("is_paused_timer", (bool(*)(const char*))&lua_is_paused_timer),
		luabind::def("stop_all_timers", (void(*)(void))&lua_stop_all_timers),
		luabind::def("stop_all_timers", (void(*)(Lua_ItemInst))&lua_stop_all_timers),
		luabind::def("stop_all_timers", (void(*)(Lua_Mob))&lua_stop_all_timers),
		luabind::def("stop_all_timers", (void(*)(Lua_Encounter))&lua_stop_all_timers),
		luabind::def("depop", (void(*)(void))&lua_depop),
		luabind::def("depop", (void(*)(int))&lua_depop),
		luabind::def("depop_with_timer", (void(*)(void))&lua_depop_with_timer),
		luabind::def("depop_with_timer", (void(*)(int))&lua_depop_with_timer),
		luabind::def("depop_all", (void(*)(void))&lua_depop_all),
		luabind::def("depop_all", (void(*)(int))&lua_depop_all),
		luabind::def("depop_zone", &lua_depop_zone),
		luabind::def("repop_zone", &lua_repop_zone),
		luabind::def("is_disc_tome", &lua_is_disc_tome),
		luabind::def("safe_move", &lua_safe_move),
		luabind::def("rain", &lua_rain),
		luabind::def("snow", &lua_snow),
		luabind::def("scribe_spells", (int(*)(int))&lua_scribe_spells),
		luabind::def("scribe_spells", (int(*)(int,int))&lua_scribe_spells),
		luabind::def("train_discs", (int(*)(int))&lua_train_discs),
		luabind::def("train_discs", (int(*)(int,int))&lua_train_discs),
		luabind::def("set_sky", &lua_set_sky),
		luabind::def("set_guild", &lua_set_guild),
		luabind::def("create_guild", &lua_create_guild),
		luabind::def("set_time", (void(*)(int, int))&lua_set_time),
		luabind::def("set_time", (void(*)(int, int, bool))&lua_set_time),
		luabind::def("signal", (void(*)(int,int))&lua_signal),
		luabind::def("signal", (void(*)(int,int,int))&lua_signal),
		luabind::def("set_global", &lua_set_global),
		luabind::def("target_global", &lua_target_global),
		luabind::def("delete_global", &lua_delete_global),
		luabind::def("start", &lua_start),
		luabind::def("stop", &lua_stop),
		luabind::def("pause", &lua_pause),
		luabind::def("move_to", (void(*)(float,float,float))&lua_move_to),
		luabind::def("move_to", (void(*)(float,float,float,float))&lua_move_to),
		luabind::def("move_to", (void(*)(float,float,float,float,bool))&lua_move_to),
		luabind::def("resume", &lua_path_resume),
		luabind::def("set_next_hp_event", &lua_set_next_hp_event),
		luabind::def("set_next_inc_hp_event", &lua_set_next_inc_hp_event),
		luabind::def("respawn", &lua_respawn),
		luabind::def("set_proximity", (void(*)(float,float,float,float))&lua_set_proximity),
		luabind::def("set_proximity", (void(*)(float,float,float,float,float,float))&lua_set_proximity),
		luabind::def("set_proximity", (void(*)(float,float,float,float,float,float,bool))&lua_set_proximity),
		luabind::def("clear_proximity", &lua_clear_proximity),
		luabind::def("enable_proximity_say", &lua_enable_proximity_say),
		luabind::def("disable_proximity_say", &lua_disable_proximity_say),
		luabind::def("set_anim", &lua_set_anim),
		luabind::def("spawn_condition", &lua_spawn_condition),
		luabind::def("get_spawn_condition", &lua_get_spawn_condition),
		luabind::def("toggle_spawn_event", &lua_toggle_spawn_event),
		luabind::def("summon_buried_player_corpse", &lua_summon_buried_player_corpse),
		luabind::def("summon_all_player_corpses", &lua_summon_all_player_corpses),
		luabind::def("get_player_buried_corpse_count", &lua_get_player_buried_corpse_count),
		luabind::def("bury_player_corpse", &lua_bury_player_corpse),
		luabind::def("task_selector", &lua_task_selector),
		luabind::def("task_set_selector", &lua_task_set_selector),
		luabind::def("enable_task", &lua_enable_task),
		luabind::def("disable_task", &lua_disable_task),
		luabind::def("is_task_enabled", &lua_is_task_enabled),
		luabind::def("is_task_active", &lua_is_task_active),
		luabind::def("is_task_activity_active", &lua_is_task_activity_active),
		luabind::def("get_task_activity_done_count", &lua_get_task_activity_done_count),
		luabind::def("update_task_activity", &lua_update_task_activity),
		luabind::def("reset_task_activity", &lua_reset_task_activity),
		luabind::def("task_explored_area", &lua_task_explored_area),
		luabind::def("assign_task", &lua_assign_task),
		luabind::def("fail_task", &lua_fail_task),
		luabind::def("task_time_left", &lua_task_time_left),
		luabind::def("is_task_completed", &lua_is_task_completed),
		luabind::def("enabled_task_count", &lua_enabled_task_count),
		luabind::def("first_task_in_set", &lua_first_task_in_set),
		luabind::def("last_task_in_set", &lua_last_task_in_set),
		luabind::def("next_task_in_set", &lua_next_task_in_set),
		luabind::def("active_speak_task", &lua_active_speak_task),
		luabind::def("active_speak_activity", &lua_active_speak_activity),
		luabind::def("active_tasks_in_set", &lua_active_tasks_in_set),
		luabind::def("completed_tasks_in_set", &lua_completed_tasks_in_set),
		luabind::def("is_task_appropriate", &lua_is_task_appropriate),
		luabind::def("popup", &lua_popup),
		luabind::def("clear_spawn_timers", &lua_clear_spawn_timers),
		luabind::def("zone_emote", &lua_zone_emote),
		luabind::def("world_emote", &lua_world_emote),
		luabind::def("get_level", &lua_get_level),
		luabind::def("create_ground_object", (void(*)(uint32,float,float,float,float))&lua_create_ground_object),
		luabind::def("create_ground_object", (void(*)(uint32,float,float,float,float,uint32))&lua_create_ground_object),
		luabind::def("create_ground_object_from_model", (void(*)(const char*,float,float,float,float))&lua_create_ground_object_from_model),
		luabind::def("create_ground_object_from_model", (void(*)(const char*,float,float,float,float,int))&lua_create_ground_object_from_model),
		luabind::def("create_ground_object_from_model", (void(*)(const char*,float,float,float,float,int,uint32))&lua_create_ground_object_from_model),
		luabind::def("create_door", &lua_create_door),
		luabind::def("modify_npc_stat", &lua_modify_npc_stat),
		luabind::def("collect_items", &lua_collect_items),
		luabind::def("update_spawn_timer", &lua_update_spawn_timer),
		luabind::def("merchant_set_item", (void(*)(uint32,uint32))&lua_merchant_set_item),
		luabind::def("merchant_set_item", (void(*)(uint32,uint32,uint32))&lua_merchant_set_item),
		luabind::def("merchant_count_item", &lua_merchant_count_item),
		luabind::def("item_link", &lua_item_link),
		luabind::def("say_link", (std::string(*)(const char*,bool,const char*))&lua_say_link),
		luabind::def("say_link", (std::string(*)(const char*,bool))&lua_say_link),
		luabind::def("say_link", (std::string(*)(const char*))&lua_say_link),
		luabind::def("set_rule", (void(*)(std::string, std::string))&lua_set_rule),
		luabind::def("get_rule", (std::string(*)(std::string))&lua_get_rule),
		luabind::def("get_data", (std::string(*)(std::string))&lua_get_data),
		luabind::def("get_data_expires", (std::string(*)(std::string))&lua_get_data_expires),
		luabind::def("set_data", (void(*)(std::string, std::string))&lua_set_data),
		luabind::def("set_data", (void(*)(std::string, std::string, std::string))&lua_set_data),
		luabind::def("delete_data", (bool(*)(std::string))&lua_delete_data),
		luabind::def("get_guild_name_by_id", &lua_get_guild_name_by_id),
		luabind::def("create_instance", &lua_create_instance),
		luabind::def("destroy_instance", &lua_destroy_instance),
		luabind::def("update_instance_timer", &lua_update_instance_timer),
		luabind::def("get_instance_id", &lua_get_instance_id),
		luabind::def("get_instance_id_by_char_id", &lua_get_instance_id_by_char_id),
		luabind::def("get_instance_timer", &lua_get_instance_timer),
		luabind::def("get_instance_timer_by_id", &lua_get_instance_timer_by_id),
		luabind::def("get_characters_in_instance", &lua_get_characters_in_instance),
		luabind::def("assign_to_instance", &lua_assign_to_instance),
		luabind::def("assign_to_instance_by_char_id", &lua_assign_to_instance_by_char_id),
		luabind::def("assign_group_to_instance", &lua_assign_group_to_instance),
		luabind::def("assign_raid_to_instance", &lua_assign_raid_to_instance),
		luabind::def("remove_from_instance", &lua_remove_from_instance),
		luabind::def("remove_from_instance_by_char_id", &lua_remove_from_instance_by_char_id),
		luabind::def("remove_all_from_instance", &lua_remove_all_from_instance),
		luabind::def("flag_instance_by_group_leader", &lua_flag_instance_by_group_leader),
		luabind::def("flag_instance_by_raid_leader", &lua_flag_instance_by_raid_leader),
		luabind::def("fly_mode", &lua_fly_mode),
		luabind::def("faction_value", &lua_faction_value),
		luabind::def("check_title", &lua_check_title),
		luabind::def("enable_title", &lua_enable_title),
		luabind::def("remove_title", &lua_remove_title),
		luabind::def("wear_change", &lua_wear_change),
		luabind::def("voice_tell", &lua_voice_tell),
		luabind::def("send_mail", &lua_send_mail),
		luabind::def("cross_zone_signal_client_by_char_id", &lua_cross_zone_signal_client_by_char_id),
		luabind::def("cross_zone_signal_client_by_name", &lua_cross_zone_signal_client_by_name),
		luabind::def("cross_zone_message_player_by_name", &lua_cross_zone_message_player_by_name),
		luabind::def("cross_zone_set_entity_variable_by_client_name", &lua_cross_zone_set_entity_variable_by_client_name),
		luabind::def("world_wide_marquee", &lua_world_wide_marquee),
		luabind::def("get_qglobals", (luabind::adl::object(*)(lua_State*,Lua_NPC,Lua_Client))&lua_get_qglobals),
		luabind::def("get_qglobals", (luabind::adl::object(*)(lua_State*,Lua_Client))&lua_get_qglobals),
		luabind::def("get_qglobals", (luabind::adl::object(*)(lua_State*,Lua_NPC))&lua_get_qglobals),
		luabind::def("get_qglobals", (luabind::adl::object(*)(lua_State*))&lua_get_qglobals),
		luabind::def("get_entity_list", &lua_get_entity_list),
		luabind::def("get_zone_id", &lua_get_zone_id),
		luabind::def("get_zone_long_name", &lua_get_zone_long_name),
		luabind::def("get_zone_short_name", &lua_get_zone_short_name),
		luabind::def("get_zone_instance_id", &lua_get_zone_instance_id),
		luabind::def("get_zone_instance_version", &lua_get_zone_instance_version),
		luabind::def("get_zone_weather", &lua_get_zone_weather),
		luabind::def("get_zone_time", &lua_get_zone_time),
		luabind::def("add_area", &lua_add_area),
		luabind::def("remove_area", &lua_remove_area),
		luabind::def("clear_areas", &lua_clear_areas),
		luabind::def("add_spawn_point", &lua_add_spawn_point),
		luabind::def("remove_spawn_point", &lua_remove_spawn_point),
		luabind::def("attack", &lua_attack),
		luabind::def("attack_npc", &lua_attack_npc),
		luabind::def("attack_npc_type", &lua_attack_npc_type),
		luabind::def("follow", (void(*)(int))&lua_follow),
		luabind::def("follow", (void(*)(int,int))&lua_follow),
		luabind::def("stop_follow", &lua_stop_follow),
		luabind::def("get_initiator", &lua_get_initiator),
		luabind::def("get_owner", &lua_get_owner),
		luabind::def("get_quest_item", &lua_get_quest_item),
		luabind::def("get_encounter", &lua_get_encounter),
		luabind::def("map_opcodes", &lua_map_opcodes),
		luabind::def("clear_opcode", &lua_clear_opcode),
		luabind::def("enable_recipe", &lua_enable_recipe),
		luabind::def("disable_recipe", &lua_disable_recipe),
		luabind::def("clear_npctype_cache", &lua_clear_npctype_cache),
		luabind::def("reloadzonestaticdata", &lua_reloadzonestaticdata),
		luabind::def("clock", &lua_clock),
		luabind::def("create_npc", &lua_create_npc),
		luabind::def("debug", (void(*)(std::string))&lua_debug),
		luabind::def("debug", (void(*)(std::string, int))&lua_debug)
	];
}

luabind::scope lua_register_random() {
	return luabind::namespace_("Random")
		[
			luabind::def("Int", &random_int),
			luabind::def("Real", &random_real),
			luabind::def("Roll", &random_roll_int),
			luabind::def("RollReal", &random_roll_real),
			luabind::def("Roll0", &random_roll0)
		];
}


luabind::scope lua_register_events() {
	return luabind::class_<Events>("Event")
		.enum_("constants")
		[
			luabind::value("say", static_cast<int>(EVENT_SAY)),
			luabind::value("trade", static_cast<int>(EVENT_TRADE)),
			luabind::value("death", static_cast<int>(EVENT_DEATH)),
			luabind::value("spawn", static_cast<int>(EVENT_SPAWN)),
			luabind::value("combat", static_cast<int>(EVENT_COMBAT)),
			luabind::value("slay", static_cast<int>(EVENT_SLAY)),
			luabind::value("waypoint_arrive", static_cast<int>(EVENT_WAYPOINT_ARRIVE)),
			luabind::value("waypoint_depart", static_cast<int>(EVENT_WAYPOINT_DEPART)),
			luabind::value("timer", static_cast<int>(EVENT_TIMER)),
			luabind::value("signal", static_cast<int>(EVENT_SIGNAL)),
			luabind::value("hp", static_cast<int>(EVENT_HP)),
			luabind::value("enter", static_cast<int>(EVENT_ENTER)),
			luabind::value("exit", static_cast<int>(EVENT_EXIT)),
			luabind::value("enter_zone", static_cast<int>(EVENT_ENTER_ZONE)),
			luabind::value("click_door", static_cast<int>(EVENT_CLICK_DOOR)),
			luabind::value("loot", static_cast<int>(EVENT_LOOT)),
			luabind::value("zone", static_cast<int>(EVENT_ZONE)),
			luabind::value("level_up", static_cast<int>(EVENT_LEVEL_UP)),
			luabind::value("killed_merit", static_cast<int>(EVENT_KILLED_MERIT)),
			luabind::value("cast_on", static_cast<int>(EVENT_CAST_ON)),
			luabind::value("task_accepted", static_cast<int>(EVENT_TASK_ACCEPTED)),
			luabind::value("task_stage_complete", static_cast<int>(EVENT_TASK_STAGE_COMPLETE)),
			luabind::value("environmental_damage", static_cast<int>(EVENT_ENVIRONMENTAL_DAMAGE)),
			luabind::value("task_update", static_cast<int>(EVENT_TASK_UPDATE)),
			luabind::value("task_complete", static_cast<int>(EVENT_TASK_COMPLETE)),
			luabind::value("task_fail", static_cast<int>(EVENT_TASK_FAIL)),
			luabind::value("aggro_say", static_cast<int>(EVENT_AGGRO_SAY)),
			luabind::value("player_pickup", static_cast<int>(EVENT_PLAYER_PICKUP)),
			luabind::value("popup_response", static_cast<int>(EVENT_POPUP_RESPONSE)),
			luabind::value("proximity_say", static_cast<int>(EVENT_PROXIMITY_SAY)),
			luabind::value("cast", static_cast<int>(EVENT_CAST)),
			luabind::value("cast_begin", static_cast<int>(EVENT_CAST_BEGIN)),
			luabind::value("scale_calc", static_cast<int>(EVENT_SCALE_CALC)),
			luabind::value("item_enter_zone", static_cast<int>(EVENT_ITEM_ENTER_ZONE)),
			luabind::value("target_change", static_cast<int>(EVENT_TARGET_CHANGE)),
			luabind::value("hate_list", static_cast<int>(EVENT_HATE_LIST)),
			luabind::value("spell_effect", static_cast<int>(EVENT_SPELL_EFFECT_CLIENT)),
			luabind::value("spell_buff_tic", static_cast<int>(EVENT_SPELL_BUFF_TIC_CLIENT)),
			luabind::value("spell_fade", static_cast<int>(EVENT_SPELL_FADE)),
			luabind::value("spell_effect_translocate_complete", static_cast<int>(EVENT_SPELL_EFFECT_TRANSLOCATE_COMPLETE)),
			luabind::value("combine_success ", static_cast<int>(EVENT_COMBINE_SUCCESS )),
			luabind::value("combine_failure ", static_cast<int>(EVENT_COMBINE_FAILURE )),
			luabind::value("item_click", static_cast<int>(EVENT_ITEM_CLICK)),
			luabind::value("item_click_cast", static_cast<int>(EVENT_ITEM_CLICK_CAST)),
			luabind::value("group_change", static_cast<int>(EVENT_GROUP_CHANGE)),
			luabind::value("forage_success", static_cast<int>(EVENT_FORAGE_SUCCESS)),
			luabind::value("forage_failure", static_cast<int>(EVENT_FORAGE_FAILURE)),
			luabind::value("fish_start", static_cast<int>(EVENT_FISH_START)),
			luabind::value("fish_success", static_cast<int>(EVENT_FISH_SUCCESS)),
			luabind::value("fish_failure", static_cast<int>(EVENT_FISH_FAILURE)),
			luabind::value("click_object", static_cast<int>(EVENT_CLICK_OBJECT)),
			luabind::value("discover_item", static_cast<int>(EVENT_DISCOVER_ITEM)),
			luabind::value("disconnect", static_cast<int>(EVENT_DISCONNECT)),
			luabind::value("connect", static_cast<int>(EVENT_CONNECT)),
			luabind::value("item_tick", static_cast<int>(EVENT_ITEM_TICK)),
			luabind::value("duel_win", static_cast<int>(EVENT_DUEL_WIN)),
			luabind::value("duel_lose", static_cast<int>(EVENT_DUEL_LOSE)),
			luabind::value("encounter_load", static_cast<int>(EVENT_ENCOUNTER_LOAD)),
			luabind::value("encounter_unload", static_cast<int>(EVENT_ENCOUNTER_UNLOAD)),
			luabind::value("command", static_cast<int>(EVENT_COMMAND)),
			luabind::value("drop_item", static_cast<int>(EVENT_DROP_ITEM)),
			luabind::value("destroy_item", static_cast<int>(EVENT_DESTROY_ITEM)),
			luabind::value("feign_death", static_cast<int>(EVENT_FEIGN_DEATH)),
			luabind::value("weapon_proc", static_cast<int>(EVENT_WEAPON_PROC)),
			luabind::value("equip_item", static_cast<int>(EVENT_EQUIP_ITEM)),
			luabind::value("unequip_item", static_cast<int>(EVENT_UNEQUIP_ITEM)),
			luabind::value("augment_item", static_cast<int>(EVENT_AUGMENT_ITEM)),
			luabind::value("unaugment_item", static_cast<int>(EVENT_UNAUGMENT_ITEM)),
			luabind::value("augment_insert", static_cast<int>(EVENT_AUGMENT_INSERT)),
			luabind::value("augment_remove", static_cast<int>(EVENT_AUGMENT_REMOVE)),
			luabind::value("enter_area", static_cast<int>(EVENT_ENTER_AREA)),
			luabind::value("leave_area", static_cast<int>(EVENT_LEAVE_AREA)),
			luabind::value("death_complete", static_cast<int>(EVENT_DEATH_COMPLETE)),
			luabind::value("unhandled_opcode", static_cast<int>(EVENT_UNHANDLED_OPCODE)),
			luabind::value("tick", static_cast<int>(EVENT_TICK)),
			luabind::value("spawn_zone", static_cast<int>(EVENT_SPAWN_ZONE)),
			luabind::value("death_zone", static_cast<int>(EVENT_DEATH_ZONE)),
			luabind::value("use_skill", static_cast<int>(EVENT_USE_SKILL))
		];
}

luabind::scope lua_register_faction() {
	return luabind::class_<Factions>("Faction")
		.enum_("constants")
		[
			luabind::value("Ally", static_cast<int>(FACTION_ALLY)),
			luabind::value("Warmly", static_cast<int>(FACTION_WARMLY)),
			luabind::value("Kindly", static_cast<int>(FACTION_KINDLY)),
			luabind::value("Amiable", static_cast<int>(FACTION_AMIABLE)),
			luabind::value("Indifferent", static_cast<int>(FACTION_INDIFFERENT)),
			luabind::value("Apprehensive", static_cast<int>(FACTION_APPREHENSIVE)),
			luabind::value("Dubious", static_cast<int>(FACTION_DUBIOUS)),
			luabind::value("Threatenly", static_cast<int>(FACTION_THREATENLY)),
			luabind::value("Scowls", static_cast<int>(FACTION_SCOWLS))
		];
}

luabind::scope lua_register_slot() {
	return luabind::class_<Slots>("Slot")
		.enum_("constants")
		[
			luabind::value("Charm", static_cast<int>(EQEmu::invslot::slotCharm)),
			luabind::value("Ear1", static_cast<int>(EQEmu::invslot::slotEar1)),
			luabind::value("Head", static_cast<int>(EQEmu::invslot::slotHead)),
			luabind::value("Face", static_cast<int>(EQEmu::invslot::slotFace)),
			luabind::value("Ear2", static_cast<int>(EQEmu::invslot::slotEar2)),
			luabind::value("Neck", static_cast<int>(EQEmu::invslot::slotNeck)),
			luabind::value("Shoulders", static_cast<int>(EQEmu::invslot::slotShoulders)),
			luabind::value("Arms", static_cast<int>(EQEmu::invslot::slotArms)),
			luabind::value("Back", static_cast<int>(EQEmu::invslot::slotBack)),
			luabind::value("Wrist1", static_cast<int>(EQEmu::invslot::slotWrist1)),
			luabind::value("Wrist2", static_cast<int>(EQEmu::invslot::slotWrist2)),
			luabind::value("Range", static_cast<int>(EQEmu::invslot::slotRange)),
			luabind::value("Hands", static_cast<int>(EQEmu::invslot::slotHands)),
			luabind::value("Primary", static_cast<int>(EQEmu::invslot::slotPrimary)),
			luabind::value("Secondary", static_cast<int>(EQEmu::invslot::slotSecondary)),
			luabind::value("Finger1", static_cast<int>(EQEmu::invslot::slotFinger1)),
			luabind::value("Finger2", static_cast<int>(EQEmu::invslot::slotFinger2)),
			luabind::value("Chest", static_cast<int>(EQEmu::invslot::slotChest)),
			luabind::value("Legs", static_cast<int>(EQEmu::invslot::slotLegs)),
			luabind::value("Feet", static_cast<int>(EQEmu::invslot::slotFeet)),
			luabind::value("Waist", static_cast<int>(EQEmu::invslot::slotWaist)),
			luabind::value("PowerSource", static_cast<int>(EQEmu::invslot::slotPowerSource)),
			luabind::value("Ammo", static_cast<int>(EQEmu::invslot::slotAmmo)),
			luabind::value("General1", static_cast<int>(EQEmu::invslot::slotGeneral1)),
			luabind::value("General2", static_cast<int>(EQEmu::invslot::slotGeneral2)),
			luabind::value("General3", static_cast<int>(EQEmu::invslot::slotGeneral3)),
			luabind::value("General4", static_cast<int>(EQEmu::invslot::slotGeneral4)),
			luabind::value("General5", static_cast<int>(EQEmu::invslot::slotGeneral5)),
			luabind::value("General6", static_cast<int>(EQEmu::invslot::slotGeneral6)),
			luabind::value("General7", static_cast<int>(EQEmu::invslot::slotGeneral7)),
			luabind::value("General8", static_cast<int>(EQEmu::invslot::slotGeneral8)),
			luabind::value("General9", static_cast<int>(EQEmu::invslot::slotGeneral9)),
			luabind::value("General10", static_cast<int>(EQEmu::invslot::slotGeneral10)),
			luabind::value("Cursor", static_cast<int>(EQEmu::invslot::slotCursor)),
			luabind::value("PossessionsBegin", static_cast<int>(EQEmu::invslot::POSSESSIONS_BEGIN)),
			luabind::value("PossessionsEnd", static_cast<int>(EQEmu::invslot::POSSESSIONS_END)),
			luabind::value("EquipmentBegin", static_cast<int>(EQEmu::invslot::EQUIPMENT_BEGIN)),
			luabind::value("EquipmentEnd", static_cast<int>(EQEmu::invslot::EQUIPMENT_END)),
			luabind::value("GeneralBegin", static_cast<int>(EQEmu::invslot::GENERAL_BEGIN)),
			luabind::value("GeneralEnd", static_cast<int>(EQEmu::invslot::GENERAL_END)),
			luabind::value("GeneralBagsBegin", static_cast<int>(EQEmu::invbag::GENERAL_BAGS_BEGIN)),
			luabind::value("GeneralBagsEnd", static_cast<int>(EQEmu::invbag::GENERAL_BAGS_END)),
			luabind::value("CursorBagBegin", static_cast<int>(EQEmu::invbag::CURSOR_BAG_BEGIN)),
			luabind::value("CursorBagEnd", static_cast<int>(EQEmu::invbag::CURSOR_BAG_END)),
			luabind::value("Tradeskill", static_cast<int>(EQEmu::invslot::SLOT_TRADESKILL_EXPERIMENT_COMBINE)),
			luabind::value("Augment", static_cast<int>(EQEmu::invslot::SLOT_AUGMENT_GENERIC_RETURN)), // will be revised out
			luabind::value("BankBegin", static_cast<int>(EQEmu::invslot::BANK_BEGIN)),
			luabind::value("BankEnd", static_cast<int>(EQEmu::invslot::BANK_END)),
			luabind::value("BankBagsBegin", static_cast<int>(EQEmu::invbag::BANK_BAGS_BEGIN)),
			luabind::value("BankBagsEnd", static_cast<int>(EQEmu::invbag::BANK_BAGS_END)),
			luabind::value("SharedBankBegin", static_cast<int>(EQEmu::invslot::SHARED_BANK_BEGIN)),
			luabind::value("SharedBankEnd", static_cast<int>(EQEmu::invslot::SHARED_BANK_END)),
			luabind::value("SharedBankBagsBegin", static_cast<int>(EQEmu::invbag::SHARED_BANK_BAGS_BEGIN)),
			luabind::value("SharedBankBagsEnd", static_cast<int>(EQEmu::invbag::SHARED_BANK_BAGS_END)),
			luabind::value("BagSlotBegin", static_cast<int>(EQEmu::invbag::SLOT_BEGIN)),
			luabind::value("BagSlotEnd", static_cast<int>(EQEmu::invbag::SLOT_END)),
			luabind::value("AugSocketBegin", static_cast<int>(EQEmu::invaug::SOCKET_BEGIN)),
			luabind::value("AugSocketEnd", static_cast<int>(EQEmu::invaug::SOCKET_END)),
			luabind::value("Invalid", static_cast<int>(EQEmu::invslot::SLOT_INVALID)),

			luabind::value("Shoulder", static_cast<int>(EQEmu::invslot::slotShoulders)), // deprecated
			luabind::value("Bracer1", static_cast<int>(EQEmu::invslot::slotWrist1)), // deprecated
			luabind::value("Bracer2", static_cast<int>(EQEmu::invslot::slotWrist2)), // deprecated
			luabind::value("Ring1", static_cast<int>(EQEmu::invslot::slotFinger1)), // deprecated
			luabind::value("Ring2", static_cast<int>(EQEmu::invslot::slotFinger2)), // deprecated
			luabind::value("PersonalBegin", static_cast<int>(EQEmu::invslot::GENERAL_BEGIN)), // deprecated
			luabind::value("PersonalEnd", static_cast<int>(EQEmu::invslot::GENERAL_END)), // deprecated
			luabind::value("CursorEnd", 0xFFFE) // deprecated (not in use..and never valid vis-a-vis client behavior)
		];
}

luabind::scope lua_register_material() {
	return luabind::class_<Materials>("Material")
		.enum_("constants")
		[
			luabind::value("Head", static_cast<int>(EQEmu::textures::armorHead)),
			luabind::value("Chest", static_cast<int>(EQEmu::textures::armorChest)),
			luabind::value("Arms", static_cast<int>(EQEmu::textures::armorArms)),
			luabind::value("Wrist", static_cast<int>(EQEmu::textures::armorWrist)),
			luabind::value("Hands", static_cast<int>(EQEmu::textures::armorHands)),
			luabind::value("Legs", static_cast<int>(EQEmu::textures::armorLegs)),
			luabind::value("Feet", static_cast<int>(EQEmu::textures::armorFeet)),
			luabind::value("Primary", static_cast<int>(EQEmu::textures::weaponPrimary)),
			luabind::value("Secondary", static_cast<int>(EQEmu::textures::weaponSecondary)),
			luabind::value("Count", static_cast<int>(EQEmu::textures::materialCount)),
			luabind::value("Invalid", static_cast<int>(EQEmu::textures::materialInvalid)),

			luabind::value("Bracer", static_cast<int>(EQEmu::textures::armorWrist)), // deprecated
			luabind::value("Max", static_cast<int>(EQEmu::textures::materialCount)) // deprecated
		];
}

luabind::scope lua_register_client_version() {
	return luabind::class_<ClientVersions>("ClientVersion")
		.enum_("constants")
		[
			luabind::value("Unknown", static_cast<int>(EQEmu::versions::ClientVersion::Unknown)),
			luabind::value("Titanium", static_cast<int>(EQEmu::versions::ClientVersion::Titanium)),
			luabind::value("SoF", static_cast<int>(EQEmu::versions::ClientVersion::SoF)),
			luabind::value("SoD", static_cast<int>(EQEmu::versions::ClientVersion::SoD)),
			luabind::value("Underfoot", static_cast<int>(EQEmu::versions::ClientVersion::UF)), // deprecated
			luabind::value("UF", static_cast<int>(EQEmu::versions::ClientVersion::UF)),
			luabind::value("RoF", static_cast<int>(EQEmu::versions::ClientVersion::RoF)),
			luabind::value("RoF2", static_cast<int>(EQEmu::versions::ClientVersion::RoF2))
		];
}

luabind::scope lua_register_appearance() {
	return luabind::class_<Appearances>("Appearance")
		.enum_("constants")
		[
			luabind::value("Standing", static_cast<int>(eaStanding)),
			luabind::value("Sitting", static_cast<int>(eaSitting)),
			luabind::value("Crouching", static_cast<int>(eaCrouching)),
			luabind::value("Dead", static_cast<int>(eaDead)),
			luabind::value("Looting", static_cast<int>(eaLooting))
		];
}

luabind::scope lua_register_classes() {
	return luabind::class_<Classes>("Class")
		.enum_("constants")
		[
			luabind::value("WARRIOR", WARRIOR),
			luabind::value("CLERIC", CLERIC),
			luabind::value("PALADIN", PALADIN),
			luabind::value("RANGER", RANGER),
			luabind::value("SHADOWKNIGHT", SHADOWKNIGHT),
			luabind::value("DRUID", DRUID),
			luabind::value("MONK", MONK),
			luabind::value("BARD", BARD),
			luabind::value("ROGUE", ROGUE),
			luabind::value("SHAMAN", SHAMAN),
			luabind::value("NECROMANCER", NECROMANCER),
			luabind::value("WIZARD", WIZARD),
			luabind::value("MAGICIAN", MAGICIAN),
			luabind::value("ENCHANTER", ENCHANTER),
			luabind::value("BEASTLORD", BEASTLORD),
			luabind::value("BERSERKER", BERSERKER),
			luabind::value("WARRIORGM", WARRIORGM),
			luabind::value("CLERICGM", CLERICGM),
			luabind::value("PALADINGM", PALADINGM),
			luabind::value("RANGERGM", RANGERGM),
			luabind::value("SHADOWKNIGHTGM", SHADOWKNIGHTGM),
			luabind::value("DRUIDGM", DRUIDGM),
			luabind::value("MONKGM", MONKGM),
			luabind::value("BARDGM", BARDGM),
			luabind::value("ROGUEGM", ROGUEGM),
			luabind::value("SHAMANGM", SHAMANGM),
			luabind::value("NECROMANCERGM", NECROMANCERGM),
			luabind::value("WIZARDGM", WIZARDGM),
			luabind::value("MAGICIANGM", MAGICIANGM),
			luabind::value("ENCHANTERGM", ENCHANTERGM),
			luabind::value("BEASTLORDGM", BEASTLORDGM),
			luabind::value("BERSERKERGM", BERSERKERGM),
			luabind::value("BANKER", BANKER),
			luabind::value("MERCHANT", MERCHANT),
			luabind::value("DISCORD_MERCHANT", DISCORD_MERCHANT),
			luabind::value("ADVENTURERECRUITER", ADVENTURERECRUITER),
			luabind::value("ADVENTUREMERCHANT", ADVENTUREMERCHANT),
			luabind::value("LDON_TREASURE", LDON_TREASURE),
			luabind::value("CORPSE_CLASS", CORPSE_CLASS),
			luabind::value("TRIBUTE_MASTER", TRIBUTE_MASTER),
			luabind::value("GUILD_TRIBUTE_MASTER", GUILD_TRIBUTE_MASTER),
			luabind::value("NORRATHS_KEEPERS_MERCHANT", NORRATHS_KEEPERS_MERCHANT),
			luabind::value("DARK_REIGN_MERCHANT", DARK_REIGN_MERCHANT),
			luabind::value("FELLOWSHIP_MASTER", FELLOWSHIP_MASTER),
			luabind::value("ALT_CURRENCY_MERCHANT", ALT_CURRENCY_MERCHANT),
			luabind::value("MERCERNARY_MASTER", MERCERNARY_MASTER)
		];
}

luabind::scope lua_register_skills() {
	return luabind::class_<Skills>("Skill")
		.enum_("constants")
		[
			luabind::value("1HBlunt", EQEmu::skills::Skill1HBlunt),
			luabind::value("1HSlashing", EQEmu::skills::Skill1HSlashing),
			luabind::value("2HBlunt", EQEmu::skills::Skill2HBlunt),
			luabind::value("2HSlashing", EQEmu::skills::Skill2HSlashing),
			luabind::value("Abjuration", EQEmu::skills::SkillAbjuration),
			luabind::value("Alteration", EQEmu::skills::SkillAlteration),
			luabind::value("ApplyPoison", EQEmu::skills::SkillApplyPoison),
			luabind::value("Archery", EQEmu::skills::SkillArchery),
			luabind::value("Backstab", EQEmu::skills::SkillBackstab),
			luabind::value("BindWound", EQEmu::skills::SkillBindWound),
			luabind::value("Bash", EQEmu::skills::SkillBash),
			luabind::value("Block", EQEmu::skills::SkillBlock),
			luabind::value("BrassInstruments", EQEmu::skills::SkillBrassInstruments),
			luabind::value("Channeling", EQEmu::skills::SkillChanneling),
			luabind::value("Conjuration", EQEmu::skills::SkillConjuration),
			luabind::value("Defense", EQEmu::skills::SkillDefense),
			luabind::value("Disarm", EQEmu::skills::SkillDisarm),
			luabind::value("DisarmTraps", EQEmu::skills::SkillDisarmTraps),
			luabind::value("Divination", EQEmu::skills::SkillDivination),
			luabind::value("Dodge", EQEmu::skills::SkillDodge),
			luabind::value("DoubleAttack", EQEmu::skills::SkillDoubleAttack),
			luabind::value("DragonPunch", EQEmu::skills::SkillDragonPunch),
			luabind::value("TailRake", EQEmu::skills::SkillTailRake),
			luabind::value("DualWield", EQEmu::skills::SkillDualWield),
			luabind::value("EagleStrike", EQEmu::skills::SkillEagleStrike),
			luabind::value("Evocation", EQEmu::skills::SkillEvocation),
			luabind::value("FeignDeath", EQEmu::skills::SkillFeignDeath),
			luabind::value("FlyingKick", EQEmu::skills::SkillFlyingKick),
			luabind::value("Forage", EQEmu::skills::SkillForage),
			luabind::value("HandtoHand", EQEmu::skills::SkillHandtoHand),
			luabind::value("Hide", EQEmu::skills::SkillHide),
			luabind::value("Kick", EQEmu::skills::SkillKick),
			luabind::value("Meditate", EQEmu::skills::SkillMeditate),
			luabind::value("Mend", EQEmu::skills::SkillMend),
			luabind::value("Offense", EQEmu::skills::SkillOffense),
			luabind::value("Parry", EQEmu::skills::SkillParry),
			luabind::value("PickLock", EQEmu::skills::SkillPickLock),
			luabind::value("1HPiercing", EQEmu::skills::Skill1HPiercing),
			luabind::value("Riposte", EQEmu::skills::SkillRiposte),
			luabind::value("RoundKick", EQEmu::skills::SkillRoundKick),
			luabind::value("SafeFall", EQEmu::skills::SkillSafeFall),
			luabind::value("SenseHeading", EQEmu::skills::SkillSenseHeading),
			luabind::value("Singing", EQEmu::skills::SkillSinging),
			luabind::value("Sneak", EQEmu::skills::SkillSneak),
			luabind::value("SpecializeAbjure", EQEmu::skills::SkillSpecializeAbjure),
			luabind::value("SpecializeAlteration", EQEmu::skills::SkillSpecializeAlteration),
			luabind::value("SpecializeConjuration", EQEmu::skills::SkillSpecializeConjuration),
			luabind::value("SpecializeDivination", EQEmu::skills::SkillSpecializeDivination),
			luabind::value("SpecializeEvocation", EQEmu::skills::SkillSpecializeEvocation),
			luabind::value("PickPockets", EQEmu::skills::SkillPickPockets),
			luabind::value("StringedInstruments", EQEmu::skills::SkillStringedInstruments),
			luabind::value("Swimming", EQEmu::skills::SkillSwimming),
			luabind::value("Throwing", EQEmu::skills::SkillThrowing),
			luabind::value("TigerClaw", EQEmu::skills::SkillTigerClaw),
			luabind::value("Tracking", EQEmu::skills::SkillTracking),
			luabind::value("WindInstruments", EQEmu::skills::SkillWindInstruments),
			luabind::value("Fishing", EQEmu::skills::SkillFishing),
			luabind::value("MakePoison", EQEmu::skills::SkillMakePoison),
			luabind::value("Tinkering", EQEmu::skills::SkillTinkering),
			luabind::value("Research", EQEmu::skills::SkillResearch),
			luabind::value("Alchemy", EQEmu::skills::SkillAlchemy),
			luabind::value("Baking", EQEmu::skills::SkillBaking),
			luabind::value("Tailoring", EQEmu::skills::SkillTailoring),
			luabind::value("SenseTraps", EQEmu::skills::SkillSenseTraps),
			luabind::value("Blacksmithing", EQEmu::skills::SkillBlacksmithing),
			luabind::value("Fletching", EQEmu::skills::SkillFletching),
			luabind::value("Brewing", EQEmu::skills::SkillBrewing),
			luabind::value("AlcoholTolerance", EQEmu::skills::SkillAlcoholTolerance),
			luabind::value("Begging", EQEmu::skills::SkillBegging),
			luabind::value("JewelryMaking", EQEmu::skills::SkillJewelryMaking),
			luabind::value("Pottery", EQEmu::skills::SkillPottery),
			luabind::value("PercussionInstruments", EQEmu::skills::SkillPercussionInstruments),
			luabind::value("Intimidation", EQEmu::skills::SkillIntimidation),
			luabind::value("Berserking", EQEmu::skills::SkillBerserking),
			luabind::value("Taunt", EQEmu::skills::SkillTaunt),
			luabind::value("Frenzy", EQEmu::skills::SkillFrenzy),
			luabind::value("RemoveTraps", EQEmu::skills::SkillRemoveTraps),
			luabind::value("TripleAttack", EQEmu::skills::SkillTripleAttack),
			luabind::value("2HPiercing", EQEmu::skills::Skill2HPiercing),
			luabind::value("HIGHEST_SKILL", EQEmu::skills::HIGHEST_SKILL)
		];
}

luabind::scope lua_register_bodytypes() {
	return luabind::class_<BodyTypes>("BT")
		.enum_("constants")
		[
			luabind::value("Humanoid", 1),
			luabind::value("Lycanthrope", 2),
			luabind::value("Undead", 3),
			luabind::value("Giant", 4),
			luabind::value("Construct", 5),
			luabind::value("Extraplanar", 6),
			luabind::value("Magical", 7),
			luabind::value("SummonedUndead", 8),
			luabind::value("RaidGiant", 9),
			luabind::value("NoTarget", 11),
			luabind::value("Vampire", 12),
			luabind::value("Atenha_Ra", 13),
			luabind::value("Greater_Akheva", 14),
			luabind::value("Khati_Sha", 15),
			luabind::value("Seru", 16),
			luabind::value("Draz_Nurakk", 18),
			luabind::value("Zek", 19),
			luabind::value("Luggald", 20),
			luabind::value("Animal", 21),
			luabind::value("Insect", 22),
			luabind::value("Monster", 23),
			luabind::value("Summoned", 24),
			luabind::value("Plant", 25),
			luabind::value("Dragon", 26),
			luabind::value("Summoned2", 27),
			luabind::value("Summoned3", 28),
			luabind::value("VeliousDragon", 30),
			luabind::value("Dragon3", 32),
			luabind::value("Boxes", 33),
			luabind::value("Muramite", 34),
			luabind::value("NoTarget2", 60),
			luabind::value("SwarmPet", 63),
			luabind::value("InvisMan", 66),
			luabind::value("Special", 67)
		];
}

luabind::scope lua_register_filters() {
	return luabind::class_<Filters>("Filter")
		.enum_("constants")
		[
			luabind::value("None", FilterNone),
			luabind::value("GuildChat", FilterGuildChat),
			luabind::value("Socials", FilterSocials),
			luabind::value("GroupChat", FilterGroupChat),
			luabind::value("Shouts", FilterShouts),
			luabind::value("Auctions", FilterAuctions),
			luabind::value("OOC", FilterOOC),
			luabind::value("BadWords", FilterBadWords),
			luabind::value("PCSpells", FilterPCSpells),
			luabind::value("NPCSpells", FilterNPCSpells),
			luabind::value("BardSongs", FilterBardSongs),
			luabind::value("SpellCrits", FilterSpellCrits),
			luabind::value("MeleeCrits", FilterMeleeCrits),
			luabind::value("SpellDamage", FilterSpellDamage),
			luabind::value("MyMisses", FilterMyMisses),
			luabind::value("OthersMiss", FilterOthersMiss),
			luabind::value("OthersHit", FilterOthersHit),
			luabind::value("MissedMe", FilterMissedMe),
			luabind::value("DamageShields", FilterDamageShields),
			luabind::value("DOT", FilterDOT),
			luabind::value("PetHits", FilterPetHits),
			luabind::value("PetMisses", FilterPetMisses),
			luabind::value("FocusEffects", FilterFocusEffects),
			luabind::value("PetSpells", FilterPetSpells),
			luabind::value("HealOverTime", FilterHealOverTime),
			luabind::value("Unknown25", FilterUnknown25),
			luabind::value("Unknown26", FilterUnknown26),
			luabind::value("Unknown27", FilterUnknown27),
			luabind::value("Unknown28", FilterUnknown28)
		];
}

luabind::scope lua_register_message_types() {
	return luabind::class_<MessageTypes>("MT")
		.enum_("constants")
		[
			luabind::value("NPCQuestSay", Chat::NPCQuestSay),
			luabind::value("Say", Chat::Say),
			luabind::value("Tell", Chat::Tell),
			luabind::value("Group", Chat::Group),
			luabind::value("Guild", Chat::Guild),
			luabind::value("OOC", Chat::OOC),
			luabind::value("Auction", Chat::Auction),
			luabind::value("Shout", Chat::Shout),
			luabind::value("Emote", Chat::Emote),
			luabind::value("Spells", Chat::Spells),
			luabind::value("YouHitOther", Chat::YouHitOther),
			luabind::value("OtherHitsYou", Chat::OtherHitYou),
			luabind::value("YouMissOther", Chat::YouMissOther),
			luabind::value("OtherMissesYou", Chat::OtherMissYou),
			luabind::value("Broadcasts", Chat::Broadcasts),
			luabind::value("Skills", Chat::Skills),
			luabind::value("Disciplines", Chat::Disciplines),
			luabind::value("Unused1", Chat::Unused1),
			luabind::value("DefaultText", Chat::DefaultText),
			luabind::value("Unused2", Chat::Unused2),
			luabind::value("MerchantOffer", Chat::MerchantOffer),
			luabind::value("MerchantBuySell", Chat::MerchantExchange),
			luabind::value("YourDeath", Chat::YourDeath),
			luabind::value("OtherDeath", Chat::OtherDeath),
			luabind::value("OtherHits", Chat::OtherHitOther),
			luabind::value("OtherMisses", Chat::OtherMissOther),
			luabind::value("Who", Chat::Who),
			luabind::value("YellForHelp", Chat::YellForHelp),
			luabind::value("NonMelee", Chat::NonMelee),
			luabind::value("WornOff", Chat::SpellWornOff),
			luabind::value("MoneySplit", Chat::MoneySplit),
			luabind::value("LootMessages", Chat::Loot),
			luabind::value("DiceRoll", Chat::DiceRoll),
			luabind::value("OtherSpells", Chat::OtherSpells),
			luabind::value("SpellFailure", Chat::SpellFailure),
			luabind::value("Chat", Chat::ChatChannel),
			luabind::value("Channel1", Chat::Chat1),
			luabind::value("Channel2", Chat::Chat2),
			luabind::value("Channel3", Chat::Chat3),
			luabind::value("Channel4", Chat::Chat4),
			luabind::value("Channel5", Chat::Chat5),
			luabind::value("Channel6", Chat::Chat6),
			luabind::value("Channel7", Chat::Chat7),
			luabind::value("Channel8", Chat::Chat8),
			luabind::value("Channel9", Chat::Chat9),
			luabind::value("Channel10", Chat::Chat10),
			luabind::value("CritMelee", Chat::MeleeCrit),
			luabind::value("SpellCrits", Chat::SpellCrit),
			luabind::value("TooFarAway", Chat::TooFarAway),
			luabind::value("NPCRampage", Chat::NPCRampage),
			luabind::value("NPCFlurry", Chat::NPCFlurry),
			luabind::value("NPCEnrage", Chat::NPCEnrage),
			luabind::value("SayEcho", Chat::EchoSay),
			luabind::value("TellEcho", Chat::EchoTell),
			luabind::value("GroupEcho", Chat::EchoGroup),
			luabind::value("GuildEcho", Chat::EchoGuild),
			luabind::value("OOCEcho", Chat::EchoOOC),
			luabind::value("AuctionEcho", Chat::EchoAuction),
			luabind::value("ShoutECho", Chat::EchoShout),
			luabind::value("EmoteEcho", Chat::EchoEmote),
			luabind::value("Chat1Echo", Chat::EchoChat1),
			luabind::value("Chat2Echo", Chat::EchoChat2),
			luabind::value("Chat3Echo", Chat::EchoChat3),
			luabind::value("Chat4Echo", Chat::EchoChat4),
			luabind::value("Chat5Echo", Chat::EchoChat5),
			luabind::value("Chat6Echo", Chat::EchoChat6),
			luabind::value("Chat7Echo", Chat::EchoChat7),
			luabind::value("Chat8Echo", Chat::EchoChat8),
			luabind::value("Chat9Echo", Chat::EchoChat9),
			luabind::value("Chat10Echo", Chat::EchoChat10),
			luabind::value("DoTDamage", Chat::DotDamage),
			luabind::value("ItemLink", Chat::ItemLink),
			luabind::value("RaidSay", Chat::RaidSay),
			luabind::value("MyPet", Chat::MyPet),
			luabind::value("DS", Chat::DamageShield),
			luabind::value("Leadership", Chat::LeaderShip),
			luabind::value("PetFlurry", Chat::PetFlurry),
			luabind::value("PetCrit", Chat::PetCritical),
			luabind::value("FocusEffect", Chat::FocusEffect),
			luabind::value("Experience", Chat::Experience),
			luabind::value("System", Chat::System),
			luabind::value("PetSpell", Chat::PetSpell),
			luabind::value("PetResponse", Chat::PetResponse),
			luabind::value("ItemSpeech", Chat::ItemSpeech),
			luabind::value("StrikeThrough", Chat::StrikeThrough),
			luabind::value("Stun", Chat::Stun)
		];
}

luabind::scope lua_register_rules_const() {
	return luabind::class_<Rule>("Rule")
		.enum_("constants")
	[
#define RULE_INT(cat, rule, default_value, notes) \
		luabind::value(#rule, RuleManager::Int__##rule),
#include "../common/ruletypes.h"
		luabind::value("_IntRuleCount", RuleManager::_IntRuleCount),
#undef RULE_INT
#define RULE_REAL(cat, rule, default_value, notes) \
		luabind::value(#rule, RuleManager::Real__##rule),
#include "../common/ruletypes.h"
		luabind::value("_RealRuleCount", RuleManager::_RealRuleCount),
#undef RULE_REAL
#define RULE_BOOL(cat, rule, default_value, notes) \
		luabind::value(#rule, RuleManager::Bool__##rule),
#include "../common/ruletypes.h"
		luabind::value("_BoolRuleCount", RuleManager::_BoolRuleCount)
	];
}

luabind::scope lua_register_rulei() {
	return luabind::namespace_("RuleI")
		[
			luabind::def("Get", &get_rulei)
		];
}

luabind::scope lua_register_ruler() {
	return luabind::namespace_("RuleR")
		[
			luabind::def("Get", &get_ruler)
		];
}

luabind::scope lua_register_ruleb() {
	return luabind::namespace_("RuleB")
		[
			luabind::def("Get", &get_ruleb)
		];
}

luabind::scope lua_register_journal_speakmode() {
	return luabind::class_<Journal_SpeakMode>("SpeakMode")
		.enum_("constants")
		[
			luabind::value("Raw", static_cast<int>(Journal::SpeakMode::Raw)),
			luabind::value("Say", static_cast<int>(Journal::SpeakMode::Say)),
			luabind::value("Shout", static_cast<int>(Journal::SpeakMode::Shout)),
			luabind::value("EmoteAlt", static_cast<int>(Journal::SpeakMode::EmoteAlt)),
			luabind::value("Emote", static_cast<int>(Journal::SpeakMode::Emote)),
			luabind::value("Group", static_cast<int>(Journal::SpeakMode::Group))
		];
}

luabind::scope lua_register_journal_mode() {
	return luabind::class_<Journal_Mode>("JournalMode")
		.enum_("constants")
		[
			luabind::value("None", static_cast<int>(Journal::Mode::None)),
			luabind::value("Log1", static_cast<int>(Journal::Mode::Log1)),
			luabind::value("Log2", static_cast<int>(Journal::Mode::Log2))
		];
}

#endif
