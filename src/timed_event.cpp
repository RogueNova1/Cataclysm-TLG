#include "timed_event.h"

#include <array>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>

#include "avatar.h"
#include "avatar_action.h"
#include "character.h"
#include "coordinates.h"
#include "debug.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "game.h"
#include "game_constants.h"
#include "line.h"
#include "magic.h"
#include "map.h"
#include "map_extras.h"
#include "map_iterator.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mapgen_functions.h"
#include "memorial_logger.h"
#include "messages.h"
#include "monster.h"
#include "options.h"
#include "rng.h"
#include "sounds.h"
#include "text_snippets.h"
#include "translations.h"
#include "type_id.h"

static const itype_id itype_petrified_eye( "petrified_eye" );

static const map_extra_id map_extra_mx_dsa_alrp( "mx_dsa_alrp" );

static const morale_type morale_scream( "morale_scream" );

static const mtype_id mon_amigara_horror( "mon_amigara_horror" );
static const mtype_id mon_dark_wyrm( "mon_dark_wyrm" );
static const mtype_id mon_dermatik( "mon_dermatik" );
static const mtype_id mon_dsa_alien_dispatch( "mon_dsa_alien_dispatch" );
static const mtype_id mon_sewer_snake( "mon_sewer_snake" );
static const mtype_id mon_spider_cellar_giant( "mon_spider_cellar_giant" );
static const mtype_id mon_spider_widow_giant( "mon_spider_widow_giant" );

static const spell_id spell_dks_summon_alrp( "dks_summon_alrp" );

static const ter_str_id ter_t_fault( "t_fault" );
static const ter_str_id ter_t_grate( "t_grate" );
static const ter_str_id ter_t_rock_floor( "t_rock_floor" );
static const ter_str_id ter_t_root_wall( "t_root_wall" );
static const ter_str_id ter_t_stairs_down( "t_stairs_down" );
static const ter_str_id ter_t_underbrush( "t_underbrush" );
static const ter_str_id ter_t_water_dp( "t_water_dp" );
static const ter_str_id ter_t_water_sh( "t_water_sh" );

timed_event::timed_event( timed_event_type e_t, const time_point &w, int f_id, tripoint_abs_ms p,
                          int s, std::string key )
    : type( e_t )
    , when( w )
    , faction_id( f_id )
    , map_square( p )
    , strength( s )
    , key( std::move( key ) )
{
    map_point = project_to<coords::sm>( map_square );
}

timed_event::timed_event( timed_event_type e_t, const time_point &w, int f_id, tripoint_abs_ms p,
                          int s, std::string s_id, std::string key )
    : type( e_t )
    , when( w )
    , faction_id( f_id )
    , map_square( p )
    , strength( s )
    , string_id( std::move( s_id ) )
    , key( std::move( key ) )
{
    map_point = project_to<coords::sm>( map_square );
}

timed_event::timed_event( timed_event_type e_t, const time_point &w, int f_id, tripoint_abs_ms p,
                          int s, std::string s_id, submap sr, std::string key )
    : type( e_t )
    , when( w )
    , faction_id( f_id )
    , map_square( p )
    , strength( s )
    , string_id( std::move( s_id ) )
    , key( std::move( key ) )
    , revert( std::move( sr ) )
{
    map_point = project_to<coords::sm>( map_square );
}

void timed_event::actualize()
{
    avatar &player_character = get_avatar();
    map &here = get_map();
    switch( type ) {
        case timed_event_type::HELP:
            debugmsg( "Currently disabled while NPC and monster factions are being rewritten." );
            break;

        case timed_event_type::SPAWN_WYRMS: {
            if( here.get_abs_sub().z() >= 0 ) {
                return;
            }
            get_memorial().add(
                pgettext( "memorial_male", "Drew the attention of more dark wyrms!" ),
                pgettext( "memorial_female", "Drew the attention of more dark wyrms!" ) );

            // 50% chance to spawn a dark wyrm near every orifice on the level.
            for( const tripoint_bub_ms &p : here.points_on_zlevel() ) {
                if( here.ter( p ) == ter_id( "t_orifice" ) ) {
                    g->place_critter_around( mon_dark_wyrm, p.raw(), 1 );
                }
            }

            // You could drop the flag, you know.
            if( player_character.has_amount( itype_petrified_eye, 1 ) ) {
                sounds::sound( player_character.pos(), 60, sounds::sound_t::alert, _( "a tortured scream!" ), false,
                               "shout",
                               "scream_tortured" );
                if( !player_character.is_deaf() ) {
                    add_msg( _( "The eye you're carrying lets out a tortured scream!" ) );
                    player_character.add_morale( morale_scream, -15, 0, 30_minutes, 30_seconds );
                }
            }

        }
        break;

        case timed_event_type::AMIGARA: {
            get_event_bus().send<event_type::angers_amigara_horrors>();
            int num_horrors = rng( 3, 5 );
            std::optional<tripoint_bub_ms> fault_point;
            bool horizontal = false;
            for( const tripoint_bub_ms &p : here.points_on_zlevel() ) {
                if( here.ter( p ) == ter_t_fault ) {
                    fault_point = p;
                    horizontal = here.ter( p + tripoint_east ) == ter_t_fault ||
                                 here.ter( p + tripoint_west ) == ter_t_fault;
                    break;
                }
            }
            for( int i = 0; fault_point && i < num_horrors; i++ ) {
                for( int tries = 0; tries < 10; ++tries ) {
                    tripoint_bub_ms monp = player_character.pos_bub();
                    if( horizontal ) {
                        monp.x() = rng( fault_point->x(), fault_point->x() + 2 * SEEX - 8 );
                        for( int n = -1; n <= 1; n++ ) {
                            if( here.ter( point_bub_ms( monp.x(), fault_point->y() + n ) ) == ter_t_rock_floor ) {
                                monp.y() = fault_point->y() + n;
                            }
                        }
                    } else {
                        // Vertical fault
                        monp.y() = rng( fault_point->y(), fault_point->y() + 2 * SEEY - 8 );
                        for( int n = -1; n <= 1; n++ ) {
                            if( here.ter( point_bub_ms( fault_point->x() + n, monp.y() ) ) == ter_t_rock_floor ) {
                                monp.x() = fault_point->x() + n;
                            }
                        }
                    }
                    if( g->place_critter_at( mon_amigara_horror, monp ) ) {
                        break;
                    }
                }
            }
        }
        break;

        case timed_event_type::ROOTS_DIE:
            get_event_bus().send<event_type::destroys_triffid_grove>();
            for( const tripoint_bub_ms &p : here.points_on_zlevel() ) {
                if( here.ter( p ) == ter_t_root_wall && one_in( 3 ) ) {
                    here.ter_set( p, ter_t_underbrush );
                }
            }
            break;

        case timed_event_type::DSA_ALRP_SUMMON: {
            const tripoint_abs_sm u_pos = player_character.global_sm_location();
            if( rl_dist( u_pos, map_point ) <= 4 ) {
                const tripoint_bub_ms spot = here.bub_from_abs( project_to<coords::ms>( map_point ) );
                monster dispatcher( mon_dsa_alien_dispatch );
                fake_spell summoning( spell_dks_summon_alrp, true, 12 );
                summoning.get_spell( player_character ).cast_all_effects( dispatcher, spot );
            } else {
                const tripoint_abs_omt omt_point = project_to<coords::omt>( map_point );
                tinymap mx_map;
                mx_map.load( omt_point, false );
                MapExtras::apply_function( map_extra_mx_dsa_alrp, mx_map, omt_point );
                g->load_npcs();
                here.invalidate_map_cache( map_point.z() );
            }
        }
        break;

        case timed_event_type::TRANSFORM_RADIUS: {
            map tm;
            tm.load( project_to<coords::sm>( map_square - point{ strength, strength} ), false );
            tm.transform_radius( ter_furn_transform_id( string_id ), strength,
                                 map_square );
            break;
        }
        case timed_event_type::UPDATE_MAPGEN:
            run_mapgen_update_func(
                update_mapgen_id( string_id ), project_to<coords::omt>( map_point ), {}, nullptr );
            set_queued_points();
            get_map().invalidate_map_cache( map_point.z() );
            break;

        case timed_event_type::REVERT_SUBMAP: {
            submap *sm = MAPBUFFER.lookup_submap( map_point );
            sm->revert_submap( revert );
            get_map().invalidate_map_cache( map_point.z() );
            break;
        }

        default:
            // Nothing happens for other events
            break;
    }
}

void timed_event::per_turn()
{
    Character &player_character = get_player_character();
    map &here = get_map();
    switch( type ) {
        case timed_event_type::SPAWN_WYRMS:
            if( here.get_abs_sub().z() >= 0 ) {
                when -= 1_turns;
                return;
            }
            if( calendar::once_every( time_duration::from_seconds( rng( 2, 3 ) ) ) &&
                !player_character.is_deaf() ) {
                add_msg( m_warning, _( "You hear screeches from the rock above and around you!" ) );
            }
            break;

        case timed_event_type::AMIGARA:
            if( calendar::once_every( time_duration::from_seconds( rng( 2, 3 ) ) ) ) {
                add_msg( m_warning, _( "The entire cavern shakes!" ) );
            }
            break;

        case timed_event_type::AMIGARA_WHISPERS: {
            bool faults = false;
            for( const tripoint_bub_ms &p : here.points_on_zlevel() ) {
                if( here.ter( p ) == ter_t_fault ) {
                    faults = true;
                    break;
                }
            }

            if( calendar::once_every( time_duration::from_seconds( 10 ) ) && faults ) {
                add_msg( m_info, _( "You hear someone whispering \"%s\"" ),
                         SNIPPET.random_from_category( "amigara_whispers" ).value_or( translation() ) );
            }
        }
        break;

        default:
            // Nothing happens for other events
            break;
    }
}

void timed_event_manager::process()
{
    for( auto it = events.begin(); it != events.end(); ) {
        it->per_turn();
        if( it->when <= calendar::turn ) {
            it->actualize();
            it = events.erase( it );
        } else {
            it++;
        }
    }
}

void timed_event_manager::add( timed_event_type type, const time_point &when,
                               const int faction_id, int strength, const std::string &key )
{
    add( type, when, faction_id, get_player_character().get_location(), strength, "", key );
}

void timed_event_manager::add( timed_event_type type, const time_point &when,
                               const int faction_id,
                               const tripoint_abs_ms &where,
                               int strength, const std::string &key )
{
    events.emplace_back( type, when, faction_id, where, strength, key );
}

void timed_event_manager::add( timed_event_type type, const time_point &when,
                               const int faction_id,
                               const tripoint_abs_ms &where,
                               int strength, const std::string &string_id,
                               const std::string &key )
{
    events.emplace_back( type, when, faction_id, where, strength, string_id, key );
}

void timed_event_manager::add( timed_event_type type, const time_point &when,
                               const int faction_id,
                               const tripoint_abs_ms &where,
                               int strength, const std::string &string_id, submap sr,
                               const std::string &key )
{
    events.emplace_back( type, when, faction_id, where, strength, string_id, std::move( sr ), key );
}

bool timed_event_manager::queued( const timed_event_type type ) const
{
    return const_cast<timed_event_manager &>( *this ).get( type ) != nullptr;
}

timed_event *timed_event_manager::get( const timed_event_type type )
{
    for( timed_event &e : events ) {
        if( e.type == type ) {
            return &e;
        }
    }
    return nullptr;
}

timed_event *timed_event_manager::get( const timed_event_type type, const std::string &key )
{
    for( timed_event &e : events ) {
        if( e.type == type && e.key == key ) {
            return &e;
        }
    }
    return nullptr;
}

std::list<timed_event> const &timed_event_manager::get_all() const
{
    return events;
}

void timed_event_manager::set_all( const std::string &key, time_duration time_in_future )
{
    for( timed_event &e : events ) {
        if( e.key == key ) {
            e.when = calendar::turn + time_in_future;
        }
    }
}
