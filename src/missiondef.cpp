#include "mission.h" // IWYU pragma: associated

#include <algorithm>
#include <cstdlib>
#include <set>

#include "assign.h"
#include "calendar.h"
#include "condition.h"
#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "init.h"
#include "item.h"
#include "json.h"
#include "npc.h"
#include "rng.h"

static const mission_type_id mission_MISSION_EXPLORE_SARCOPHAGUS( "MISSION_EXPLORE_SARCOPHAGUS" );
static const mission_type_id
mission_MISSION_FREE_MERCHANTS_EVAC_1( "MISSION_FREE_MERCHANTS_EVAC_1" );
static const mission_type_id
mission_MISSION_FREE_MERCHANTS_EVAC_2( "MISSION_FREE_MERCHANTS_EVAC_2" );
static const mission_type_id
mission_MISSION_FREE_MERCHANTS_EVAC_3( "MISSION_FREE_MERCHANTS_EVAC_3" );
static const mission_type_id
mission_MISSION_FREE_MERCHANTS_EVAC_4( "MISSION_FREE_MERCHANTS_EVAC_4" );
static const mission_type_id mission_MISSION_GET_ANTIBIOTICS( "MISSION_GET_ANTIBIOTICS" );
static const mission_type_id mission_MISSION_GET_BLACK_BOX( "MISSION_GET_BLACK_BOX" );
static const mission_type_id
mission_MISSION_GET_BLACK_BOX_TRANSCRIPT( "MISSION_GET_BLACK_BOX_TRANSCRIPT" );
static const mission_type_id mission_MISSION_GET_DEPUTY_BADGE( "MISSION_GET_DEPUTY_BADGE" );
static const mission_type_id mission_MISSION_GET_FLAG( "MISSION_GET_FLAG" );
static const mission_type_id
mission_MISSION_GET_RECORD_ACCOUNTING( "MISSION_GET_RECORD_ACCOUNTING" );
static const mission_type_id mission_MISSION_GET_RECORD_PATIENT( "MISSION_GET_RECORD_PATIENT" );
static const mission_type_id mission_MISSION_GET_RECORD_WEATHER( "MISSION_GET_RECORD_WEATHER" );
static const mission_type_id mission_MISSION_GET_RELIC( "MISSION_GET_RELIC" );
static const mission_type_id mission_MISSION_GET_SAFE_BOX( "MISSION_GET_SAFE_BOX" );
static const mission_type_id mission_MISSION_GET_SOFTWARE( "MISSION_GET_SOFTWARE" );
static const mission_type_id
mission_MISSION_GET_ZOMBIE_BLOOD_ANAL( "MISSION_GET_ZOMBIE_BLOOD_ANAL" );
static const mission_type_id mission_MISSION_INVESTIGATE_CULT( "MISSION_INVESTIGATE_CULT" );
static const mission_type_id
mission_MISSION_INVESTIGATE_PRISON_VISIONARY( "MISSION_INVESTIGATE_PRISON_VISIONARY" );
static const mission_type_id mission_MISSION_JOIN_TRACKER( "MISSION_JOIN_TRACKER" );
static const mission_type_id mission_MISSION_KILL_100_Z( "MISSION_KILL_100_Z" );
static const mission_type_id mission_MISSION_KILL_HORDE_MASTER( "MISSION_KILL_HORDE_MASTER" );
static const mission_type_id mission_MISSION_KILL_JABBERWOCK( "MISSION_KILL_JABBERWOCK" );
static const mission_type_id mission_MISSION_KILL_NEMESIS( "MISSION_KILL_NEMESIS" );
static const mission_type_id mission_MISSION_KILL_ZOMBIE_MOM( "MISSION_KILL_ZOMBIE_MOM" );
static const mission_type_id mission_MISSION_NULL( "MISSION_NULL" );
static const mission_type_id mission_MISSION_OLD_GUARD_NEC_1( "MISSION_OLD_GUARD_NEC_1" );
static const mission_type_id mission_MISSION_OLD_GUARD_NEC_2( "MISSION_OLD_GUARD_NEC_2" );
static const mission_type_id
mission_MISSION_OLD_GUARD_NEC_COMMO_1( "MISSION_OLD_GUARD_NEC_COMMO_1" );
static const mission_type_id
mission_MISSION_OLD_GUARD_NEC_COMMO_2( "MISSION_OLD_GUARD_NEC_COMMO_2" );
static const mission_type_id
mission_MISSION_OLD_GUARD_NEC_COMMO_3( "MISSION_OLD_GUARD_NEC_COMMO_3" );
static const mission_type_id
mission_MISSION_OLD_GUARD_NEC_COMMO_4( "MISSION_OLD_GUARD_NEC_COMMO_4" );
static const mission_type_id mission_MISSION_OLD_GUARD_REP_1( "MISSION_OLD_GUARD_REP_1" );
static const mission_type_id mission_MISSION_OLD_GUARD_REP_2( "MISSION_OLD_GUARD_REP_2" );
static const mission_type_id mission_MISSION_OLD_GUARD_REP_3( "MISSION_OLD_GUARD_REP_3" );
static const mission_type_id mission_MISSION_OLD_GUARD_REP_4( "MISSION_OLD_GUARD_REP_4" );
static const mission_type_id mission_MISSION_RANCH_BARTENDER_1( "MISSION_RANCH_BARTENDER_1" );
static const mission_type_id mission_MISSION_RANCH_BARTENDER_2( "MISSION_RANCH_BARTENDER_2" );
static const mission_type_id mission_MISSION_RANCH_BARTENDER_3( "MISSION_RANCH_BARTENDER_3" );
static const mission_type_id mission_MISSION_RANCH_BARTENDER_4( "MISSION_RANCH_BARTENDER_4" );
static const mission_type_id mission_MISSION_RANCH_BARTENDER_5( "MISSION_RANCH_BARTENDER_5" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_1( "MISSION_RANCH_FOREMAN_1" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_10( "MISSION_RANCH_FOREMAN_10" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_11( "MISSION_RANCH_FOREMAN_11" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_12( "MISSION_RANCH_FOREMAN_12" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_13( "MISSION_RANCH_FOREMAN_13" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_14( "MISSION_RANCH_FOREMAN_14" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_15( "MISSION_RANCH_FOREMAN_15" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_16( "MISSION_RANCH_FOREMAN_16" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_17( "MISSION_RANCH_FOREMAN_17" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_2( "MISSION_RANCH_FOREMAN_2" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_3( "MISSION_RANCH_FOREMAN_3" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_4( "MISSION_RANCH_FOREMAN_4" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_5( "MISSION_RANCH_FOREMAN_5" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_6( "MISSION_RANCH_FOREMAN_6" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_7( "MISSION_RANCH_FOREMAN_7" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_8( "MISSION_RANCH_FOREMAN_8" );
static const mission_type_id mission_MISSION_RANCH_FOREMAN_9( "MISSION_RANCH_FOREMAN_9" );
static const mission_type_id mission_MISSION_RANCH_NURSE_1( "MISSION_RANCH_NURSE_1" );
static const mission_type_id mission_MISSION_RANCH_NURSE_10( "MISSION_RANCH_NURSE_10" );
static const mission_type_id mission_MISSION_RANCH_NURSE_11( "MISSION_RANCH_NURSE_11" );
static const mission_type_id mission_MISSION_RANCH_NURSE_2( "MISSION_RANCH_NURSE_2" );
static const mission_type_id mission_MISSION_RANCH_NURSE_3( "MISSION_RANCH_NURSE_3" );
static const mission_type_id mission_MISSION_RANCH_NURSE_4( "MISSION_RANCH_NURSE_4" );
static const mission_type_id mission_MISSION_RANCH_NURSE_5( "MISSION_RANCH_NURSE_5" );
static const mission_type_id mission_MISSION_RANCH_NURSE_6( "MISSION_RANCH_NURSE_6" );
static const mission_type_id mission_MISSION_RANCH_NURSE_7( "MISSION_RANCH_NURSE_7" );
static const mission_type_id mission_MISSION_RANCH_NURSE_8( "MISSION_RANCH_NURSE_8" );
static const mission_type_id mission_MISSION_RANCH_NURSE_9( "MISSION_RANCH_NURSE_9" );
static const mission_type_id mission_MISSION_RANCH_SCAVENGER_1( "MISSION_RANCH_SCAVENGER_1" );
static const mission_type_id mission_MISSION_RANCH_SCAVENGER_2( "MISSION_RANCH_SCAVENGER_2" );
static const mission_type_id mission_MISSION_RANCH_SCAVENGER_3( "MISSION_RANCH_SCAVENGER_3" );
static const mission_type_id mission_MISSION_RANCH_SCAVENGER_4( "MISSION_RANCH_SCAVENGER_4" );
static const mission_type_id mission_MISSION_REACH_FARM_HOUSE( "MISSION_REACH_FARM_HOUSE" );
static const mission_type_id mission_MISSION_REACH_FEMA_CAMP( "MISSION_REACH_FEMA_CAMP" );
static const mission_type_id mission_MISSION_REACH_SAFETY( "MISSION_REACH_SAFETY" );
static const mission_type_id mission_MISSION_RECOVER_PRIEST_DIARY( "MISSION_RECOVER_PRIEST_DIARY" );
static const mission_type_id mission_MISSION_RECRUIT_TRACKER( "MISSION_RECRUIT_TRACKER" );
static const mission_type_id mission_MISSION_RESCUE_DOG( "MISSION_RESCUE_DOG" );

enum legacy_mission_type_id {
    MISSION_NULL,
    MISSION_GET_ANTIBIOTICS,
    MISSION_GET_SOFTWARE,
    MISSION_GET_ZOMBIE_BLOOD_ANAL,
    MISSION_RESCUE_DOG,
    MISSION_KILL_ZOMBIE_MOM,
    MISSION_REACH_SAFETY,
    MISSION_GET_FLAG,                      //patriot 1
    MISSION_GET_BLACK_BOX,                 //patriot 2
    MISSION_GET_BLACK_BOX_TRANSCRIPT,      //patriot 3
    MISSION_EXPLORE_SARCOPHAGUS,           //patriot 4
    MISSION_GET_RELIC,                     //martyr 1
    MISSION_RECOVER_PRIEST_DIARY,          //martyr 2
    MISSION_INVESTIGATE_CULT,              //martyr 3
    MISSION_INVESTIGATE_PRISON_VISIONARY,  //martyr 4
    MISSION_GET_RECORD_WEATHER,            //scientist 1
    MISSION_GET_RECORD_PATIENT,            //humanitarian 1
    MISSION_REACH_FEMA_CAMP,               //humanitarian 2
    MISSION_REACH_FARM_HOUSE,              //humanitarian 3
    MISSION_GET_RECORD_ACCOUNTING,         //vigilante 1
    MISSION_GET_SAFE_BOX,                  //vigilante 2
    MISSION_GET_DEPUTY_BADGE,              //vigilante 3
    MISSION_KILL_JABBERWOCK,               //demon slayer 1
    MISSION_KILL_100_Z,                    //demon slayer 2
    MISSION_KILL_HORDE_MASTER,             //demon slayer 3
    MISSION_RECRUIT_TRACKER,               //demon slayer 4
    MISSION_JOIN_TRACKER,                  //demon slayer 4b
    MISSION_FREE_MERCHANTS_EVAC_1,         //Clear Back Bay
    MISSION_FREE_MERCHANTS_EVAC_2,         //Kill Raiders
    MISSION_FREE_MERCHANTS_EVAC_4,         //Acquire Plutonium Cells
    MISSION_OLD_GUARD_REP_1,               //Bandit Pair
    MISSION_OLD_GUARD_REP_2,               //Raider Informant
    MISSION_OLD_GUARD_REP_3,               //Missing without a trace
    MISSION_OLD_GUARD_REP_4,               //Raider Camp
    MISSION_OLD_GUARD_NEC_1,               //Locate Commo team for Necropolis Commander
    MISSION_OLD_GUARD_NEC_2,               //Cull Nightmares
    MISSION_OLD_GUARD_NEC_COMMO_1,         //Build a radio repeater mod
    MISSION_OLD_GUARD_NEC_COMMO_2,         //Disable external power connection
    MISSION_OLD_GUARD_NEC_COMMO_3,         //Install repeater mod in local radio station
    MISSION_OLD_GUARD_NEC_COMMO_4,         //Cyclical mission to install repeater mods
    MISSION_RANCH_FOREMAN_1,               //Rebuild civilization one 2x4 at a time
    MISSION_RANCH_FOREMAN_2,               //Beds need blankets to make
    MISSION_RANCH_FOREMAN_3,               //You can never have enough nails!
    MISSION_RANCH_FOREMAN_4,               //Need salt to trade for seed
    MISSION_RANCH_FOREMAN_5,               //Need liquid fertilizer
    MISSION_RANCH_FOREMAN_6,               //Need stone for well and fireplaces
    MISSION_RANCH_FOREMAN_7,               //Need pipes to finish well and parts for lumberyard
    MISSION_RANCH_FOREMAN_8,               //Need motors to finish sawmill
    MISSION_RANCH_FOREMAN_9,               //Need bleach to sterilize for clinic
    MISSION_RANCH_FOREMAN_10,              //Need first aid kits for clinic
    MISSION_RANCH_FOREMAN_11,              //Need welders for chop-shop
    MISSION_RANCH_FOREMAN_12,              //Need car batteries to power equipment
    MISSION_RANCH_FOREMAN_13,              //Need pair of two-way radios for scavengers
    MISSION_RANCH_FOREMAN_14,              //Need 5 backpacks for scavengers
    MISSION_RANCH_FOREMAN_15,              //Need Homebrewer's Bible for Bar
    MISSION_RANCH_FOREMAN_16,              //Need Sugar for Bar
    MISSION_RANCH_FOREMAN_17,              //Need glass sheets for 1st greenhouse
    MISSION_RANCH_NURSE_1,                 //Need aspirin
    MISSION_RANCH_NURSE_2,                 //Need hotplates
    MISSION_RANCH_NURSE_3,                 //Need multivitamins
    MISSION_RANCH_NURSE_4,                 //Need charcoal water filters
    MISSION_RANCH_NURSE_5,                 //Need chemistry set
    MISSION_RANCH_NURSE_6,                 //Need filter masks
    MISSION_RANCH_NURSE_7,                 //Need rubber gloves
    MISSION_RANCH_NURSE_8,                 //Need X-Acto
    MISSION_RANCH_NURSE_9,                 //Need Guide to Advanced Emergency Care
    MISSION_RANCH_NURSE_10,                //Need flu shot
    MISSION_RANCH_NURSE_11,                //Need empty syringes
    MISSION_RANCH_SCAVENGER_1,             //Need knife spears
    MISSION_RANCH_SCAVENGER_2,             //Need wearable flashlights
    MISSION_RANCH_SCAVENGER_3,             //Need leather body armor
    MISSION_RANCH_SCAVENGER_4,             //Need Molotov cocktails
    MISSION_RANCH_BARTENDER_1,             //Need Stills
    MISSION_RANCH_BARTENDER_2,             //Need Yeast
    MISSION_RANCH_BARTENDER_3,             //Need Sugar Beet Seeds
    MISSION_RANCH_BARTENDER_4,             //Need Metal Tanks
    MISSION_RANCH_BARTENDER_5,             //Need 55-Gallon Drums
    MISSION_FREE_MERCHANTS_EVAC_3,         //Info from Commune
    NUM_MISSION_IDS
};

static const std::map<std::string, std::function<void( mission * )>> mission_function_map = {{
        // Starts
        { "standard", { } },
        { "kill_nemesis", mission_start::kill_nemesis },
        { "place_npc_software", mission_start::place_npc_software },
        { "place_deposit_box", mission_start::place_deposit_box },
        { "find_safety", mission_start::find_safety },
        { "place_book", mission_start::place_book },
        { "reveal_refugee_center", mission_start::reveal_refugee_center },
        { "create_lab_console", mission_start::create_lab_console },
        { "create_hidden_lab_console", mission_start::create_hidden_lab_console },
        { "create_ice_lab_console", mission_start::create_ice_lab_console },
        // Endings
        // Failures
    }
};

static const std::map<std::string, std::function<bool( const tripoint_abs_omt & )>>
tripoint_function_map = {{
        { "never", mission_place::never },
        { "always", mission_place::always },
        { "near_town", mission_place::near_town }
    }
};

namespace io
{
template<>
std::string enum_to_string<mission_origin>( mission_origin data )
{
    switch( data ) {
        // *INDENT-OFF*
        case ORIGIN_NULL: return "ORIGIN_NULL";
        case ORIGIN_GAME_START: return "ORIGIN_GAME_START";
        case ORIGIN_OPENER_NPC: return "ORIGIN_OPENER_NPC";
        case ORIGIN_ANY_NPC: return "ORIGIN_ANY_NPC";
        case ORIGIN_SECONDARY: return "ORIGIN_SECONDARY";
        case ORIGIN_COMPUTER: return "ORIGIN_COMPUTER";
        // *INDENT-ON*
        case mission_origin::NUM_ORIGIN:
            break;
    }
    cata_fatal( "Invalid mission_origin" );
}

template<>
std::string enum_to_string<mission_goal>( mission_goal data )
{
    switch( data ) {
        // *INDENT-OFF*
        case MGOAL_NULL: return "MGOAL_NULL";
        case MGOAL_GO_TO: return "MGOAL_GO_TO";
        case MGOAL_GO_TO_TYPE: return "MGOAL_GO_TO_TYPE";
        case MGOAL_FIND_ITEM: return "MGOAL_FIND_ITEM";
        case MGOAL_FIND_ANY_ITEM: return "MGOAL_FIND_ANY_ITEM";
        case MGOAL_FIND_ITEM_GROUP: return "MGOAL_FIND_ITEM_GROUP";
        case MGOAL_FIND_MONSTER: return "MGOAL_FIND_MONSTER";
        case MGOAL_FIND_NPC: return "MGOAL_FIND_NPC";
        case MGOAL_ASSASSINATE: return "MGOAL_ASSASSINATE";
        case MGOAL_KILL_MONSTER: return "MGOAL_KILL_MONSTER";
        case MGOAL_KILL_MONSTERS: return "MGOAL_KILL_MONSTERS";
        case MGOAL_KILL_MONSTER_TYPE: return "MGOAL_KILL_MONSTER_TYPE";
        case MGOAL_KILL_MONSTER_SPEC: return "MGOAL_KILL_MONSTER_SPEC";
        case MGOAL_KILL_NEMESIS: return "MGOAL_KILL_NEMESIS";
        case MGOAL_RECRUIT_NPC: return "MGOAL_RECRUIT_NPC";
        case MGOAL_RECRUIT_NPC_CLASS: return "MGOAL_RECRUIT_NPC_CLASS";
        case MGOAL_COMPUTER_TOGGLE: return "MGOAL_COMPUTER_TOGGLE";
        case MGOAL_TALK_TO_NPC: return "MGOAL_TALK_TO_NPC";
        case MGOAL_CONDITION: return "MGOAL_CONDITION";
        // *INDENT-ON*
        case mission_goal::NUM_MGOAL:
            break;
    }
    cata_fatal( "Invalid mission_goal" );
}
} // namespace io

static generic_factory<mission_type> mission_type_factory( "mission_type" );

/** @relates string_id */
template<>
const mission_type &string_id<mission_type>::obj() const
{
    return mission_type_factory.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<mission_type>::is_valid() const
{
    return mission_type_factory.is_valid( *this );
}

void mission_type::load_mission_type( const JsonObject &jo, const std::string &src )
{
    mission_type_factory.load( jo, src );
}

static DynamicDataLoader::deferred_json deferred;

void mission_type::reset()
{
    mission_type_factory.reset();
    for( std::pair<JsonObject, std::string> &deferred_json : deferred ) {
        deferred_json.first.allow_omitted_members();
    }
    deferred.clear();
}

template <typename Fun>
void assign_function( const JsonObject &jo, const std::string &id, Fun &target,
                      const std::map<std::string, Fun> &cont )
{
    if( jo.has_string( id ) ) {
        const auto iter = cont.find( jo.get_string( id ) );
        if( iter != cont.end() ) {
            target = iter->second;
        } else {
            jo.throw_error_at( id, "Invalid mission function" );
        }
    }
}

void mission_type::load( const JsonObject &jo, const std::string &src )
{
    const bool strict = src == "tlg";

    mandatory( jo, was_loaded, "name", name );

    mandatory( jo, was_loaded, "difficulty", difficulty );
    mandatory( jo, was_loaded, "value", value );

    if( jo.has_member( "origins" ) ) {
        origins.clear();
        for( const std::string &m : jo.get_tags( "origins" ) ) {
            origins.emplace_back( io::string_to_enum<mission_origin>( m ) );
        }
    }

    if( std::any_of( origins.begin(), origins.end(), []( mission_origin origin ) {
    return origin == ORIGIN_ANY_NPC || origin == ORIGIN_OPENER_NPC || origin == ORIGIN_SECONDARY;
} ) ) {
        JsonObject djo = jo.get_object( "dialogue" );
        // TODO: There should be a cleaner way to do it
        mandatory( djo, was_loaded, "describe", dialogue[ "describe" ] );
        mandatory( djo, was_loaded, "offer", dialogue[ "offer" ] );
        mandatory( djo, was_loaded, "accepted", dialogue[ "accepted" ] );
        mandatory( djo, was_loaded, "rejected", dialogue[ "rejected" ] );
        mandatory( djo, was_loaded, "advice", dialogue[ "advice" ] );
        mandatory( djo, was_loaded, "inquire", dialogue[ "inquire" ] );
        mandatory( djo, was_loaded, "success", dialogue[ "success" ] );
        mandatory( djo, was_loaded, "success_lie", dialogue[ "success_lie" ] );
        mandatory( djo, was_loaded, "failure", dialogue[ "failure" ] );
    }

    optional( jo, was_loaded, "description", description );
    optional( jo, was_loaded, "urgent", urgent );
    optional( jo, was_loaded, "item", item_id );
    optional( jo, was_loaded, "item_group", group_id );
    optional( jo, was_loaded, "count", item_count, 1 );
    optional( jo, was_loaded, "required_container", container_id );
    optional( jo, was_loaded, "remove_container", remove_container );
    //intended for situations where closed and open container are different
    optional( jo, was_loaded, "empty_container", empty_container );
    optional( jo, was_loaded, "has_generic_rewards", has_generic_rewards, true );

    goal = jo.get_enum_value<decltype( goal )>( "goal" );

    assign_function( jo, "place", place, tripoint_function_map );
    const auto parse_phase = [&]( const std::string & phase,
    std::function<void( mission * )> &phase_func ) {
        if( jo.has_string( phase ) ) {
            assign_function( jo, phase, phase_func, mission_function_map );
        } else if( jo.has_member( phase ) ) {
            JsonObject j_start = jo.get_object( phase );
            if( !parse_funcs( j_start, src, phase_func ) ) {
                deferred.emplace_back( jo, src );
                jo.allow_omitted_members();
                j_start.allow_omitted_members();
                return false;
            }
        }
        return true;
    };
    if( !parse_phase( "start", start ) ) {
        return;
    }
    if( !parse_phase( "end", end ) ) {
        return;
    }
    if( !parse_phase( "fail", fail ) ) {
        return;
    }

    deadline = get_duration_or_var( jo, "deadline", false );

    if( jo.has_member( "followup" ) ) {
        follow_up = mission_type_id( jo.get_string( "followup" ) );
    }

    if( jo.has_member( "monster_species" ) ) {
        monster_species = species_id( jo.get_string( "monster_species" ) );
    }
    if( jo.has_member( "monster_type" ) ) {
        monster_type = mtype_id( jo.get_string( "monster_type" ) );
    }

    if( jo.has_member( "monster_kill_goal" ) ) {
        monster_kill_goal = jo.get_int( "monster_kill_goal" );
    }

    assign( jo, "destination", target_id, strict );

    if( jo.has_member( "goal_condition" ) ) {
        read_condition( jo, "goal_condition", goal_condition, true );
    }

    optional( jo, was_loaded, "invisible_on_complete", invisible_on_complete, false );
}

bool mission_type::test_goal_condition( struct dialogue &d ) const
{
    if( goal_condition ) {
        return goal_condition( d );
    }
    return true;
}

void mission_type::finalize()
{
    DynamicDataLoader::get_instance().load_deferred( deferred );
}

void mission_type::check_consistency()
{
    for( const mission_type &m : get_all() ) {
        if( !m.item_id.is_empty() && !item::type_is_defined( m.item_id ) ) {
            debugmsg( "Mission %s has undefined item id %s", m.id.c_str(), m.item_id.c_str() );
        }
    }
}

mission_type_id mission_type::from_legacy( int old_id )
{
    static const std::vector<mission_type_id> old_id_vec = {{
            mission_MISSION_NULL,
            mission_MISSION_GET_ANTIBIOTICS,
            mission_MISSION_GET_SOFTWARE,
            mission_MISSION_GET_ZOMBIE_BLOOD_ANAL,
            mission_MISSION_RESCUE_DOG,
            mission_MISSION_KILL_ZOMBIE_MOM,
            mission_MISSION_REACH_SAFETY,
            mission_MISSION_GET_FLAG,
            mission_MISSION_GET_BLACK_BOX,
            mission_MISSION_GET_BLACK_BOX_TRANSCRIPT,
            mission_MISSION_EXPLORE_SARCOPHAGUS,
            mission_MISSION_GET_RELIC,
            mission_MISSION_RECOVER_PRIEST_DIARY,
            mission_MISSION_INVESTIGATE_CULT,
            mission_MISSION_INVESTIGATE_PRISON_VISIONARY,
            mission_MISSION_GET_RECORD_WEATHER,
            mission_MISSION_GET_RECORD_PATIENT,
            mission_MISSION_REACH_FEMA_CAMP,
            mission_MISSION_REACH_FARM_HOUSE,
            mission_MISSION_GET_RECORD_ACCOUNTING,
            mission_MISSION_GET_SAFE_BOX,
            mission_MISSION_GET_DEPUTY_BADGE,
            mission_MISSION_KILL_JABBERWOCK,
            mission_MISSION_KILL_100_Z,
            mission_MISSION_KILL_HORDE_MASTER,
            mission_MISSION_KILL_NEMESIS,
            mission_MISSION_RECRUIT_TRACKER,
            mission_MISSION_JOIN_TRACKER,
            mission_MISSION_FREE_MERCHANTS_EVAC_1,
            mission_MISSION_FREE_MERCHANTS_EVAC_2,
            mission_MISSION_FREE_MERCHANTS_EVAC_4,
            mission_MISSION_OLD_GUARD_REP_1,
            mission_MISSION_OLD_GUARD_REP_2,
            mission_MISSION_OLD_GUARD_REP_3,
            mission_MISSION_OLD_GUARD_REP_4,
            mission_MISSION_OLD_GUARD_NEC_1,
            mission_MISSION_OLD_GUARD_NEC_2,
            mission_MISSION_OLD_GUARD_NEC_COMMO_1,
            mission_MISSION_OLD_GUARD_NEC_COMMO_2,
            mission_MISSION_OLD_GUARD_NEC_COMMO_3,
            mission_MISSION_OLD_GUARD_NEC_COMMO_4,
            mission_MISSION_RANCH_FOREMAN_1,
            mission_MISSION_RANCH_FOREMAN_2,
            mission_MISSION_RANCH_FOREMAN_3,
            mission_MISSION_RANCH_FOREMAN_4,
            mission_MISSION_RANCH_FOREMAN_5,
            mission_MISSION_RANCH_FOREMAN_6,
            mission_MISSION_RANCH_FOREMAN_7,
            mission_MISSION_RANCH_FOREMAN_8,
            mission_MISSION_RANCH_FOREMAN_9,
            mission_MISSION_RANCH_FOREMAN_10,
            mission_MISSION_RANCH_FOREMAN_11,
            mission_MISSION_RANCH_FOREMAN_12,
            mission_MISSION_RANCH_FOREMAN_13,
            mission_MISSION_RANCH_FOREMAN_14,
            mission_MISSION_RANCH_FOREMAN_15,
            mission_MISSION_RANCH_FOREMAN_16,
            mission_MISSION_RANCH_FOREMAN_17,
            mission_MISSION_RANCH_NURSE_1,
            mission_MISSION_RANCH_NURSE_2,
            mission_MISSION_RANCH_NURSE_3,
            mission_MISSION_RANCH_NURSE_4,
            mission_MISSION_RANCH_NURSE_5,
            mission_MISSION_RANCH_NURSE_6,
            mission_MISSION_RANCH_NURSE_7,
            mission_MISSION_RANCH_NURSE_8,
            mission_MISSION_RANCH_NURSE_9,
            mission_MISSION_RANCH_NURSE_10,
            mission_MISSION_RANCH_NURSE_11,
            mission_MISSION_RANCH_SCAVENGER_1,
            mission_MISSION_RANCH_SCAVENGER_2,
            mission_MISSION_RANCH_SCAVENGER_3,
            mission_MISSION_RANCH_SCAVENGER_4,
            mission_MISSION_RANCH_BARTENDER_1,
            mission_MISSION_RANCH_BARTENDER_2,
            mission_MISSION_RANCH_BARTENDER_3,
            mission_MISSION_RANCH_BARTENDER_4,
            mission_MISSION_RANCH_BARTENDER_5,
            mission_MISSION_FREE_MERCHANTS_EVAC_3,
            // This is to help with the bugged find book mission
            mission_MISSION_NULL
        }
    };

    if( old_id >= 0 && old_id < static_cast<int>( old_id_vec.size() ) ) {
        return old_id_vec[ old_id ];
    }

    debugmsg( "Invalid legacy mission id: %d", old_id );
    return mission_MISSION_NULL;
}

const mission_type *mission_type::get( const mission_type_id &id )
{
    if( id.is_null() ) {
        return nullptr;
    }

    return &id.obj();
}

const std::vector<mission_type> &mission_type::get_all()
{
    return mission_type_factory.get_all();
}

mission_type_id mission_type::get_random_id( const mission_origin origin,
        const tripoint_abs_omt &p )
{
    std::vector<mission_type_id> valid;
    for( const mission_type &t : get_all() ) {
        if( std::find( t.origins.begin(), t.origins.end(), origin ) == t.origins.end() ) {
            continue;
        }
        if( t.place( p ) ) {
            valid.push_back( t.id );
        }
    }
    return random_entry( valid, mission_type_id::NULL_ID() );
}
