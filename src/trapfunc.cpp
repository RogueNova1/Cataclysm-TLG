#include "trap.h" // IWYU pragma: associated

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_assert.h"
#include "character.h"
#include "colony.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "damage.h"
#include "debug.h"
#include "enums.h"
#include "explosion.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "make_static.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "mapgen_functions.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "output.h"
#include "point.h"
#include "rng.h"
#include "sounds.h"
#include "teleport.h"
#include "timed_event.h"
#include "translations.h"
#include "units.h"
#include "viewer.h"

static const bionic_id bio_shock_absorber( "bio_shock_absorber" );

static const damage_type_id damage_acid( "acid" );
static const damage_type_id damage_bash( "bash" );
static const damage_type_id damage_bullet( "bullet" );
static const damage_type_id damage_cut( "cut" );
static const damage_type_id damage_heat( "heat" );
static const damage_type_id damage_pure( "pure" );

static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_heavysnare( "heavysnare" );
static const efftype_id effect_immobilization( "immobilization" );
static const efftype_id effect_in_pit( "in_pit" );
static const efftype_id effect_jumping( "jumping" );
static const efftype_id effect_lightsnare( "lightsnare" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_slimed( "slimed" );
static const efftype_id effect_slow_descent( "slow_descent" );
static const efftype_id effect_strengthened_gravity( "strengthened_gravity" );
static const efftype_id effect_tetanus( "tetanus" );
static const efftype_id effect_weakened_gravity( "weakened_gravity" );

static const flag_id json_flag_LEVITATION( "LEVITATION" );
static const flag_id json_flag_PROXIMITY( "PROXIMITY" );
static const flag_id json_flag_UNCONSUMED( "UNCONSUMED" );

static const itype_id itype_bullwhip( "bullwhip" );
static const itype_id itype_grapnel( "grapnel" );
static const itype_id itype_grenade_act( "grenade_act" );
static const itype_id itype_rope_30( "rope_30" );

static const json_character_flag json_flag_INFECTION_IMMUNE( "INFECTION_IMMUNE" );
static const json_character_flag json_flag_WALL_CLING( "WALL_CLING" );
static const json_character_flag json_flag_WINGS_1( "WINGS_1" );
static const json_character_flag json_flag_WINGS_2( "WINGS_2" );

static const material_id material_kevlar( "kevlar" );
static const material_id material_steel( "steel" );
static const material_id material_stone( "stone" );
static const material_id material_vegetable( "vegetable" );

static const mtype_id mon_blob( "mon_blob" );
static const mtype_id mon_shadow( "mon_shadow" );
static const mtype_id mon_shadow_snake( "mon_shadow_snake" );

static const skill_id skill_throw( "throw" );

static const species_id species_ROBOT( "ROBOT" );

static const ter_str_id ter_t_floor_blue( "t_floor_blue" );
static const ter_str_id ter_t_floor_green( "t_floor_green" );
static const ter_str_id ter_t_floor_red( "t_floor_red" );
static const ter_str_id ter_t_pit( "t_pit" );
static const ter_str_id ter_t_rock_blue( "t_rock_blue" );
static const ter_str_id ter_t_rock_green( "t_rock_green" );
static const ter_str_id ter_t_rock_red( "t_rock_red" );

static const trait_id trait_INFRESIST( "INFRESIST" );

static const trap_str_id tr_pit( "tr_pit" );
static const trap_str_id tr_shotgun_1( "tr_shotgun_1" );
static const trap_str_id tr_shotgun_2( "tr_shotgun_2" );
static const trap_str_id tr_temple_flood( "tr_temple_flood" );

// A pit becomes less effective as it fills with corpses.
static float pit_effectiveness( const tripoint &p )
{
    units::volume corpse_volume = 0_ml;
    for( item &pit_content : get_map().i_at( p ) ) {
        if( pit_content.is_corpse() ) {
            corpse_volume += pit_content.volume();
        }
    }

    // 10 zombies; see item::volume
    const units::volume filled_volume = 10 * units::from_milliliter<float>( 62500 );

    return std::max( 0.0f, 1.0f - corpse_volume / filled_volume );
}

bool trapfunc::none( const tripoint &, Creature *, item * )
{
    return false;
}

bool trapfunc::bubble( const tripoint &p, Creature *c, item * )
{
    // tiny animals don't trigger bubble wrap
    if( c != nullptr && c->get_size() == creature_size::tiny ) {
        return false;
    }
    if( c != nullptr ) {
        c->add_msg_player_or_npc( m_warning, _( "You step on some bubble wrap!" ),
                                  _( "<npcname> steps on some bubble wrap!" ) );
        if( c->has_effect( effect_ridden ) ) {
            add_msg( m_warning, _( "Your %s steps on some bubble wrap!" ), c->get_name() );
        }
    }
    sounds::sound( p, 18, sounds::sound_t::alarm, _( "Pop!" ), false, "trap", "bubble_wrap" );
    get_map().remove_trap( p );
    return true;
}

bool trapfunc::glass( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    // Only a chance to step on the glass, dependent on size
    if( !x_in_y( occupied_tile_fraction( c->get_size() ), 1.0f ) ) {
        return false;
    }
    c->add_msg_player_or_npc( m_warning, _( "You step on some glass!" ),
                              _( "<npcname> steps on some glass!" ) );

    const int dmg = std::max( 0, rng( -10, 5 ) );
    if( dmg > 0 ) {
        c->mod_moves( -c->get_speed() * 0.8 );
        c->deal_damage( nullptr, random_entry( c->get_ground_contact_bodyparts() ),
                        damage_instance( damage_cut, dmg ) );
    }
    sounds::sound( p, 8, sounds::sound_t::combat, _( "glass cracking!" ), false, "trap", "glass" );
    get_map().remove_trap( p );
    c->check_dead_state();
    return true;
}

bool trapfunc::cot( const tripoint &, Creature *c, item * )
{
    monster *z = dynamic_cast<monster *>( c );
    if( z != nullptr ) {
        // Haha, only monsters stumble over a cot, humans are smart.
        add_msg( m_good, _( "The %s stumbles over the cot!" ), z->name() );
        c->mod_moves( -c->get_speed() );
        return true;
    }
    return false;
}

bool trapfunc::beartrap( const tripoint &p, Creature *c, item * )
{
    // tiny animals don't trigger bear traps
    if( c != nullptr && c->get_size() == creature_size::tiny ) {
        return false;
    }
    sounds::sound( p, 8, sounds::sound_t::combat, _( "SNAP!" ), false, "trap", "bear_trap" );
    map &here = get_map();
    here.remove_trap( p );
    if( c != nullptr ) {
        // What got hit?
        const bodypart_id hit = random_entry( c->get_ground_contact_bodyparts( true ) );

        // Messages
        if( c->is_monster() ) {
            if( c->has_effect( effect_ridden ) ) {
                add_msg( m_warning, _( "Your %s is caught by a beartrap!" ), c->get_name() );
            } else {
                add_msg_if_player_sees( c->pos(), _( "%s gets caught in a beartrap!" ), c->get_name() );
            }
        } else {
            c->add_msg_player_or_npc( m_bad,
                                      string_format( _( "A bear trap closes on your %s" ), body_part_name_accusative( hit ) ),
                                      string_format( _( "A bear trap closes on <npcname>'s %s" ), body_part_name( hit ) ) );
        }
        // Actual effects
        c->add_effect( effect_beartrap, 1_turns, hit, true );
        damage_instance d;
        d.add_damage( damage_bash, 12 );
        d.add_damage( damage_cut, 18 );
        dealt_damage_instance dealt_dmg = c->deal_damage( nullptr, hit, d );

        sounds::sound( p, 8, sounds::sound_t::activity, _( "Clank!" ), false, "trap", "snare" );

        Character *you = dynamic_cast<Character *>( c );
        if( you != nullptr && !you->has_flag( json_flag_INFECTION_IMMUNE ) &&
            dealt_dmg.type_damage( damage_cut ) > 0 ) {
            const int chance_in = you->has_trait( trait_INFRESIST ) ? 512 : 128;
            if( one_in( chance_in ) ) {
                you->add_effect( effect_tetanus, 1_turns, true );
            }
        }
        c->check_dead_state();
    }
    return true;
}

static void mount_step_on_trap_make_slow_give_msg( monster *z, const tripoint &p )
{
    if( !z ) { // should never happen
        debugmsg( "non-monster creature passed to mount_step_on_trap_make_slow_give_msg" );
        return;
    }
    map &here = get_map();
    Character *rider = nullptr;
    for( npc &guy : g->all_npcs() ) {
        if( guy.pos() == p && guy.pos() == z->pos() ) {
            rider = &guy;
            break;
        }
    }
    if( !rider ) {
        rider = &get_player_character();
    }
    if( rider->is_avatar() ) {
        //~Player with an animal mount. e.g. "Your horse stepped on a spiked board!"
        add_msg( m_warning, _( "Your %s stepped on a %s!" ), z->get_name(), here.tr_at( p ).name() );
    } else {
        //~NPC with an animal mount. e.g. "Jane's horse stepped on a spiked board!"
        add_msg_if_player_sees( p, _( "%s %s stepped on a %s!" ), rider->disp_name( true ),
                                z->get_name(), here.tr_at( p ).name() );
    }
    rider->mod_moves( -z->get_speed() * 0.8 );
}

static void try_apply_tetanus( Character *you, int total_cut_dmg )
{
    if( you != nullptr && !you->has_flag( json_flag_INFECTION_IMMUNE ) && total_cut_dmg > 0 ) {
        const int chance_in = you->has_trait( trait_INFRESIST ) ? 256 : 35;
        if( one_in( chance_in ) ) {
            you->add_effect( effect_tetanus, 1_turns, true );
        }
    }
}

bool trapfunc::board( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    map &here = get_map();
    // Only a chance to step on the board, dependent on size
    if( !x_in_y( occupied_tile_fraction( c->get_size() ), 1.0f ) ) {
        return false;
    }
    c->add_msg_if_player( m_bad, _( "You step on a %s!" ), here.tr_at( p ).name() );
    c->add_msg_if_npc( _( "%s steps on a %s!" ), c->disp_name( false, true ), here.tr_at( p ).name() );
    if( c->has_effect( effect_ridden ) ) {
        monster *z = c->as_monster();
        mount_step_on_trap_make_slow_give_msg( z, p );
    } else {
        c->mod_moves( -c->get_speed() * 0.8 );
    }
    dealt_damage_instance dd = c->deal_damage( nullptr,
                               random_entry( c->get_ground_contact_bodyparts() ),
                               damage_instance( damage_cut, rng( 3, 5 ) ) );
    try_apply_tetanus( c->as_character(), dd.type_damage( damage_cut ) );
    c->check_dead_state();
    // Weight of 100kg+ is guaranteed to break the trap, linear chance as weight increases
    if( x_in_y( c->get_weight() / 1_kilogram, 100 ) ) {
        // destroy trap
        here.remove_trap( p );
        if( !c->is_avatar() ) {
            add_msg_if_player_sees( p, _( "%s destroys a %s as they move over it!" ),
                                    c->disp_name( false, true ), here.tr_at( p ).name() );
        } else {
            add_msg( _( "You destroy the %s as you step on it!" ), here.tr_at( p ).name() );
        }
    } else if( x_in_y( 40, 100 ) ) {
        // 40% chance disarm trap
        here.tr_at( p ).on_disarmed( here, p );
    }
    return true;
}

bool trapfunc::caltrops( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    map &here = get_map();
    // Only a chance to step on the caltrop, dependent on size
    if( !x_in_y( occupied_tile_fraction( c->get_size() ), 1.0f ) ) {
        return false;
    }
    c->add_msg_if_player( m_bad, _( "You step on a sharp %s!" ), here.tr_at( p ).name() );
    c->add_msg_if_npc( _( "%s steps on a sharp %s!" ), c->disp_name( false, true ),
                       here.tr_at( p ).name() );
    if( c->has_effect( effect_ridden ) ) {
        monster *z = c->as_monster();
        mount_step_on_trap_make_slow_give_msg( z, p );
    } else {
        c->mod_moves( -c->get_speed() * 0.8 );
    }
    dealt_damage_instance dd = c->deal_damage( nullptr,
                               random_entry( c->get_ground_contact_bodyparts() ),
                               damage_instance( damage_cut, rng( 9, 15 ) ) );
    try_apply_tetanus( c->as_character(), dd.type_damage( damage_cut ) );
    // Weight of 200kg+ is guaranteed to break the trap, linear chance as weight increases
    // Arbitrary value, picked from hulk's weight value.
    if( x_in_y( c->get_weight() / 1_kilogram, 200 ) ) {
        // destroy trap
        here.remove_trap( p );
        if( !c->is_avatar() ) {
            add_msg_if_player_sees( p, _( "%s destroys a %s as they move over it!" ),
                                    c->disp_name( false, true ), here.tr_at( p ).name() );
        } else {
            add_msg( _( "You destroy the %s as you step on it!" ), here.tr_at( p ).name() );
        }
    } else if( x_in_y( 20, 100 ) ) {
        // 20% chance disarm trap
        here.tr_at( p ).on_disarmed( here, p );
    }
    c->check_dead_state();
    return true;
}

bool trapfunc::caltrops_glass( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    map &here = get_map();
    // Only a chance to step on the caltrop, dependent on size
    if( !x_in_y( occupied_tile_fraction( c->get_size() ), 1.0f ) ) {
        return false;
    }
    c->add_msg_if_player( m_bad, _( "You step on a sharp %s!" ), here.tr_at( p ).name() );
    c->add_msg_if_npc( _( "%s steps on a sharp %s!" ), c->disp_name( false, true ),
                       here.tr_at( p ).name() );
    if( c->has_effect( effect_ridden ) ) {
        monster *z = c->as_monster();
        mount_step_on_trap_make_slow_give_msg( z, p );
    } else {
        c->mod_moves( -c->get_speed() * 0.8 );
    }
    c->deal_damage( nullptr, random_entry( c->get_ground_contact_bodyparts() ),
                    damage_instance( damage_cut, rng( 3, 10 ) ) );
    c->check_dead_state();
    add_msg_if_player_sees( p, _( "The shards shatter!" ) );
    sounds::sound( p, 8, sounds::sound_t::combat, _( "glass cracking!" ), false, "trap",
                   "glass_caltrops" );
    here.remove_trap( p );
    return true;
}

bool trapfunc::tripwire( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    // tiny animals don't trigger tripwires, they just squeeze under it
    if( c->get_size() == creature_size::tiny ) {
        return false;
    }
    c->add_msg_player_or_npc( m_bad, _( "You trip over a tripwire!" ),
                              _( "<npcname> trips over a tripwire!" ) );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );

    Character &player_character = get_player_character();
    map &here = get_map();
    if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s trips over a tripwire!" ), z->get_name() );
            std::vector<tripoint> valid;
            for( const tripoint &jk : here.points_in_radius( p, 1 ) ) {
                if( g->is_empty( jk ) ) {
                    valid.push_back( jk );
                }
            }
            if( !valid.empty() ) {
                player_character.setpos( random_entry( valid ) );
                z->setpos( player_character.pos() );
            }
            player_character.mod_moves( -z->get_speed() * 1.5 );
            g->update_map( player_character );
        } else {
            z->stumble();
        }
        if( rng( 0, 10 ) > z->get_dodge() ) {
            z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_pure, rng( 1,
                            4 ) ) );
        }
    } else if( you != nullptr ) {
        std::vector<tripoint> valid;
        for( const tripoint &jk : here.points_in_radius( p, 1 ) ) {
            if( g->is_empty( jk ) ) {
                valid.push_back( jk );
            }
        }
        if( !valid.empty() ) {
            you->setpos( random_entry( valid ) );
        }
        you->mod_moves( -you->get_speed() * 1.5 );
        if( c->is_avatar() ) {
            g->update_map( player_character );
        }
        if( !you->is_mounted() ) {
            ///\EFFECT_DEX decreases chance of taking damage from a tripwire trap
            if( rng( 5, 20 ) > you->dex_cur ) {
                you->hurtall( rng( 1, 4 ), nullptr );
            }
        }
    }
    c->check_dead_state();
    return true;
}

bool trapfunc::crossbow( const tripoint &p, Creature *c, item * )
{
    bool add_bolt = true;
    if( c != nullptr ) {
        if( c->has_effect( effect_ridden ) ) {
            add_msg( m_neutral, _( "Your %s triggers a crossbow trap." ), c->get_name() );
        }
        c->add_msg_player_or_npc( m_neutral, _( "You trigger a crossbow trap!" ),
                                  _( "<npcname> triggers a crossbow trap!" ) );
        monster *z = dynamic_cast<monster *>( c );
        Character *you = dynamic_cast<Character *>( c );
        if( you != nullptr ) {
            ///\EFFECT_DODGE reduces chance of being hit by crossbow trap
            if( !one_in( 4 ) && rng( 8, 20 ) > you->get_dodge() ) {
                bodypart_id hit( "bp_null" );
                switch( rng( 1, 10 ) ) {
                    case  1:
                        if( one_in( 2 ) ) {
                            hit = bodypart_id( "foot_l" );
                        } else {
                            hit = bodypart_id( "foot_r" );
                        }
                        break;
                    case  2:
                    case  3:
                    case  4:
                        if( one_in( 2 ) ) {
                            hit = bodypart_id( "leg_l" );
                        } else {
                            hit = bodypart_id( "leg_r" );
                        }
                        break;
                    case  5:
                    case  6:
                    case  7:
                    case  8:
                    case  9:
                        hit = bodypart_id( "torso" );
                        break;
                    case 10:
                        hit = bodypart_id( "head" );
                        break;
                }
                //~ %s is bodypart
                you->add_msg_if_player( m_bad, _( "Your %s is hit!" ), body_part_name( hit ) );
                c->deal_damage( nullptr, hit, damage_instance( damage_cut, rng( 20, 30 ) ) );
                add_bolt = !one_in( 10 );
            } else {
                you->add_msg_player_or_npc( m_neutral, _( "You dodge the shot!" ),
                                            _( "<npcname> dodges the shot!" ) );
            }
        } else if( z != nullptr ) {
            bool seen = get_player_view().sees( *z );
            int chance = 0;
            // adapted from shotgun code - chance of getting hit depends on size
            switch( z->type->size ) {
                case creature_size::tiny:
                    chance = 50;
                    break;
                case creature_size::small:
                    chance = 8;
                    break;
                case creature_size::medium:
                    chance = 6;
                    break;
                case creature_size::large:
                    chance = 4;
                    break;
                case creature_size::huge:
                    chance = 1;
                    break;
                case creature_size::num_sizes:
                    debugmsg( "ERROR: Invalid Creature size class." );
                    break;
            }
            if( one_in( chance ) ) {
                if( seen ) {
                    add_msg( m_bad, _( "A bolt shoots out and hits the %s!" ), z->name() );
                }
                z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_cut, rng( 20,
                                30 ) ) );
                add_bolt = !one_in( 10 );
            } else if( seen ) {
                add_msg( m_neutral, _( "A bolt shoots out, but misses the %s." ), z->name() );
            }
        }
        c->check_dead_state();
    }
    map &here = get_map();
    here.remove_trap( p );
    here.spawn_item( p, "crossbow" );
    here.spawn_item( p, "string_36" );
    if( add_bolt ) {
        here.spawn_item( p, "bolt_steel", 1, 1 );
    }
    return true;
}

bool trapfunc::shotgun( const tripoint &p, Creature *c, item * )
{
    map &here = get_map();
    sounds::sound( p, 60, sounds::sound_t::combat, _( "Kerblam!" ), false, "fire_gun",
                   here.tr_at( p ) == tr_shotgun_1 ? "shotgun_s" : "shotgun_d" );
    int shots = 1;
    if( c != nullptr ) {
        if( c->has_effect( effect_ridden ) ) {
            add_msg( m_neutral, _( "Your %s triggers a shotgun trap!" ), c->get_name() );
        }
        c->add_msg_player_or_npc( m_neutral, _( "You trigger a shotgun trap!" ),
                                  _( "<npcname> triggers a shotgun trap!" ) );
        monster *z = dynamic_cast<monster *>( c );
        Character *you = dynamic_cast<Character *>( c );
        if( you != nullptr ) {
            ///\EFFECT_STR_MAX increases chance of two shots from shotgun trap
            shots = ( one_in( 8 ) || one_in( 20 - you->str_max ) ? 2 : 1 );
            if( here.tr_at( p ) != tr_shotgun_2 ) {
                shots = 1;
            }
            ///\EFFECT_DODGE reduces chance of being hit by shotgun trap
            if( rng( 5, 50 ) > you->get_dodge() ) {
                bodypart_id hit = bodypart_id( "bp_null" );
                switch( rng( 1, 10 ) ) {
                    case  1:
                        if( one_in( 2 ) ) {
                            hit = bodypart_id( "foot_l" );
                        } else {
                            hit = bodypart_id( "foot_r" );
                        }
                        break;
                    case  2:
                    case  3:
                    case  4:
                        if( one_in( 2 ) ) {
                            hit = bodypart_id( "leg_l" );
                        } else {
                            hit = bodypart_id( "leg_r" );
                        }
                        break;
                    case  5:
                    case  6:
                    case  7:
                    case  8:
                    case  9:
                        hit = bodypart_id( "torso" );
                        break;
                    case 10:
                        hit = bodypart_id( "head" );
                        break;
                }
                //~ %s is bodypart
                you->add_msg_if_player( m_bad, _( "Your %s is hit!" ), body_part_name( hit ) );
                c->deal_damage( nullptr, hit, damage_instance( damage_bullet, rng( 40 * shots,
                                60 * shots ) ) );
            } else {
                you->add_msg_player_or_npc( m_neutral, _( "You dodge the shot!" ),
                                            _( "<npcname> dodges the shot!" ) );
            }
        } else if( z != nullptr ) {
            bool seen = get_player_view().sees( *z );
            int chance = 0;
            switch( z->type->size ) {
                case creature_size::tiny:
                    chance = 100;
                    break;
                case creature_size::small:
                    chance = 16;
                    break;
                case creature_size::medium:
                    chance = 12;
                    break;
                case creature_size::large:
                    chance = 8;
                    break;
                case creature_size::huge:
                    chance = 2;
                    break;
                case creature_size::num_sizes:
                    debugmsg( "ERROR: Invalid Creature size class." );
                    break;
            }
            shots = ( one_in( 8 ) || one_in( chance ) ? 2 : 1 );
            if( here.tr_at( p ) != tr_shotgun_2 ) {
                shots = 1;
            }
            if( seen ) {
                add_msg( m_bad, _( "A shotgun fires and hits the %s!" ), z->name() );
            }
            z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_bullet,
                            rng( 40 * shots,
                                 60 * shots ) ) );
        }
        c->check_dead_state();
    }

    here.spawn_item( p, here.tr_at( p ) == tr_shotgun_1 ? "shotgun_s" : "shotgun_d" );
    here.spawn_item( p, "string_36" );
    here.remove_trap( p );
    return true;
}

bool trapfunc::blade( const tripoint &, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    if( c->has_effect( effect_ridden ) ) {
        add_msg( m_bad, _( "A blade swings out and hacks your %s!" ), c->get_name() );
    }
    c->add_msg_player_or_npc( m_bad, _( "A blade swings out and hacks your torso!" ),
                              _( "A blade swings out and hacks <npcname>'s torso!" ) );
    damage_instance d;
    d.add_damage( damage_bash, 12 );
    d.add_damage( damage_cut, 30 );
    c->deal_damage( nullptr, bodypart_id( "torso" ), d );
    c->check_dead_state();
    return true;
}

bool trapfunc::snare_light( const tripoint &p, Creature *c, item * )
{
    sounds::sound( p, 4, sounds::sound_t::combat, _( "Snap!" ), false, "trap", "snare" );
    map &here = get_map();
    // Trap is always destroyed when triggered (function is not called unless trap is triggered, even if creature steps on tile)
    here.remove_trap( p );
    if( c == nullptr ) {
        return false;
    }

    if( c->get_size() == creature_size::tiny || c->get_size() == creature_size::small ) {
        // The lightsnare effect multiplies speed by 0.01 so on average this is actually 10,000 turns duration.
        // Creatures can still escape earlier (possibly much earlier) based on their melee dice(for creatures) or strength/limb scores (character)
        c->add_effect( effect_lightsnare, 100_turns, true );
        if( c->is_monster() ) {
            if( c->has_effect( effect_ridden ) ) {
                add_msg( m_warning, _( "Your %s is caught in a snare!" ), c->get_name() );
            } else {
                add_msg_if_player_sees( c->pos(), _( "%s gets caught in a snare!" ), c->get_name() );
            }
        } else {
            c->add_msg_player_or_npc( m_bad, _( "A snare closes on your leg." ),
                                      _( "A snare closes on <npcname>s leg." ) );
        }
    }
    return true;
}

bool trapfunc::snare_heavy( const tripoint &p, Creature *c, item * )
{
    sounds::sound( p, 4, sounds::sound_t::combat, _( "Snap!" ), false, "trap", "snare" );
    get_map().remove_trap( p );
    if( c == nullptr ) {
        return false;
    }
    // Determine what got hit
    const bodypart_id hit = one_in( 2 ) ? bodypart_id( "leg_l" ) : bodypart_id( "leg_r" );
    if( c->is_monster() ) {
        if( c->has_effect( effect_ridden ) ) {
            add_msg( m_warning, _( "Your %s is caught in a snare!" ), c->get_name() );
        } else {
            add_msg_if_player_sees( c->pos(), _( "%s gets caught in a snare!" ), c->get_name() );
        }
    } else {
        //~ %s is bodypart name in accusative.
        c->add_msg_player_or_npc( m_bad, _( "A snare closes on your %s." ),
                                  _( "A snare closes on <npcname>s %s." ), body_part_name_accusative( hit ) );
    }

    // Actual effects
    c->add_effect( effect_heavysnare, 1_turns, hit, true );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    if( you != nullptr ) {
        damage_instance d;
        d.add_damage( damage_bash, 10 );
        you->deal_damage( nullptr, hit, d );
    } else if( z != nullptr ) {
        int damage;
        switch( z->type->size ) {
            case creature_size::tiny:
            case creature_size::small:
                damage = 20;
                break;
            case creature_size::medium:
                damage = 10;
                break;
            default:
                damage = 0;
        }
        z->deal_damage( nullptr, hit, damage_instance( damage_bash, damage ) );
    }
    c->check_dead_state();
    return true;
}

bool trapfunc::snare_species( const tripoint &p, Creature *critter, item * )
{
    map &here = get_map();
    const trap &laid_trap = here.tr_at( p );
    if( !critter ) { // Nothing to capture
        return false;
    }

    if( laid_trap.is_null() ) {
        return false; //Should never happen but just in case
    }

    if( !laid_trap.trap_item_type.has_value() ) {
        debugmsg( "Active trap %s has no stored item to determine species value.  Removing trap to avoid crashing.",
                  laid_trap.name() );
        here.remove_trap( p );
        return false;
    }

    item trap_item = item( item::find_type( laid_trap.trap_item_type.value() ) );

    if( !trap_item.has_property( "capture_species" ) ) {
        debugmsg( "placed trap's item type %s has no \"capture_species\" property!" );
        here.remove_trap( p );
        return false;
    }

    const std::string spec = trap_item.get_property_string( "capture_species" );
    const species_id species_to_capture = species_id( spec );

    if( !critter->in_species( species_to_capture ) ) {
        critter->add_msg_if_player( m_good, _( "You harmlessly pass a %1$s." ),
                                    laid_trap.name() );
        return false;
    }

    {
        sounds::sound( p, 8, sounds::sound_t::combat, _( "phwoom!" ), false, "trap", "species_snare" );
    }

    if( critter ) {
        const bodypart_id hit = critter->get_random_body_part_of_type( body_part_type::type::leg );

        critter->add_msg_if_player( m_bad, _( "A %1$s catches your %2$s!" ),
                                    laid_trap.name(), hit->name.translated() );

        if( critter->has_effect( effect_ridden ) ) {
            add_msg( m_warning, _( "Your %1$s is caught by a %2$s!" ), critter->get_name(), laid_trap.name() );
        }
        if( !critter->is_avatar() ) {
            add_msg_if_player_sees( p, _( "%1$s is caught by a %2$s!" ), critter->get_name(),
                                    laid_trap.name() );
        }
        // Actual effects
        critter->add_effect( effect_immobilization, 10_turns, hit );
        critter->check_dead_state();
    }

    here.remove_trap( p );
    return true;
}

bool trapfunc::landmine( const tripoint &p, Creature *c, item * )
{
    // tiny animals are too light to trigger land mines
    if( c != nullptr && c->get_size() == creature_size::tiny ) {
        return false;
    }
    if( c != nullptr ) {
        c->add_msg_player_or_npc( m_bad, _( "You trigger a land mine!" ),
                                  _( "<npcname> triggers a land mine!" ) );
    }
    explosion_handler::explosion( c, p, 80.0f, 0.5f, false, 30, 0.2f );
    get_map().remove_trap( p );
    return true;
}

bool trapfunc::boobytrap( const tripoint &p, Creature *c, item * )
{
    if( c != nullptr ) {
        c->add_msg_player_or_npc( m_bad, _( "You trigger a booby trap!" ),
                                  _( "<npcname> triggers a booby trap!" ) );
    }

    item grenade( itype_grenade_act );
    grenade.active = true;
    get_map().add_item( p, grenade );

    get_map().remove_trap( p );
    return true;
}

bool trapfunc::telepad( const tripoint &p, Creature *c, item * )
{
    //~ the sound of a telepad functioning
    sounds::sound( p, 6, sounds::sound_t::movement, _( "vvrrrRRMM*POP!*" ), false, "trap", "teleport" );
    if( c == nullptr ) {
        return false;
    }
    if( c->is_avatar() ) {
        c->add_msg_if_player( m_warning, _( "The air shimmers around you…" ) );
    } else {
        add_msg_if_player_sees( p, _( "The air shimmers around %s…" ), c->disp_name() );
    }
    teleport::teleport( *c );
    return false;
}

bool trapfunc::goo( const tripoint &p, Creature *c, item * )
{
    get_map().remove_trap( p );
    if( c == nullptr ) {
        return false;
    }
    c->add_msg_player_or_npc( m_bad, _( "You step in a puddle of thick goo." ),
                              _( "<npcname> steps in a puddle of thick goo." ) );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    if( you != nullptr ) {
        for( const bodypart_id &bp : you->get_ground_contact_bodyparts() ) {
            you->add_env_effect( effect_slimed, bp, 6, 2_minutes );
        }
        if( one_in( 3 ) ) {
            for( const bodypart_id &bp : you->get_ground_contact_bodyparts() ) {
                you->deal_damage( nullptr, bp, damage_instance( damage_cut, 5 ) );
            }
            std::vector<bodypart_id> bps = you->get_ground_contact_bodyparts();
            you->add_msg_player_or_npc( m_bad,
                                        string_format( _( "The acidic goo eats away at your %s!" ),
                                                you->string_for_ground_contact_bodyparts( bps ) ),
                                        string_format( _( "The acidic goo eats away at <npcname>'s %s!" ),
                                                you->string_for_ground_contact_bodyparts( bps ) ) );
            you->check_dead_state();
        }
        return true;
    } else if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            get_player_character().forced_dismount();
        }
        //All monsters except for blobs get a speed decrease
        if( z->type->id != mon_blob ) {
            z->set_speed_base( z->get_speed_base() - 15 );
            //All monsters that aren't blobs or robots transform into a blob
            if( !z->type->in_species( species_ROBOT ) ) {
                z->poly( mon_blob );
                z->set_hp( z->get_speed() );
            }
        } else {
            z->set_speed_base( z->get_speed_base() + 15 );
            z->set_hp( z->get_speed() );
        }
        return true;
    }
    cata_fatal( "c must be either a monster or a Character" );
}

bool trapfunc::dissector( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    monster *z = dynamic_cast<monster *>( c );
    bool player_sees = get_player_view().sees( p );
    if( z != nullptr ) {
        if( z->type->in_species( species_ROBOT ) ) {
            //The monster is a robot. So the dissector should not try to dissect the monsters flesh.
            //Dissector error sound.
            sounds::sound( p, 4, sounds::sound_t::electronic_speech,
                           _( "BEEPBOOP!  Please remove non-organic object." ), false, "speech", "robot" );
            c->add_msg_player_or_npc( m_bad, _( "The dissector lights up, and shuts down." ),
                                      _( "The dissector lights up, and shuts down." ) );
            return false;
        }
        // distribute damage amongst player and horse
        if( z->has_effect( effect_ridden ) && z->mounted_player ) {
            Character *ch = z->mounted_player;
            ch->deal_damage( nullptr, bodypart_id( "head" ), damage_instance( damage_cut, 15 ) );
            ch->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_cut, 20 ) );
            ch->deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance( damage_cut, 12 ) );
            ch->deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance( damage_cut, 12 ) );
            ch->deal_damage( nullptr, bodypart_id( "hand_r" ), damage_instance( damage_cut, 10 ) );
            ch->deal_damage( nullptr, bodypart_id( "hand_l" ), damage_instance( damage_cut, 10 ) );
            ch->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_cut, 12 ) );
            ch->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_cut, 12 ) );
            ch->deal_damage( nullptr, bodypart_id( "foot_l" ), damage_instance( damage_cut, 10 ) );
            ch->deal_damage( nullptr, bodypart_id( "foot_r" ), damage_instance( damage_cut, 10 ) );
            if( player_sees ) {
                ch->add_msg_player_or_npc( m_bad, _( "Electrical beams emit from the floor and slice your flesh!" ),
                                           _( "Electrical beams emit from the floor and slice <npcname>s flesh!" ) );
            }
            ch->check_dead_state();
        }
    }

    //~ the sound of a dissector dissecting
    sounds::sound( p, 10, sounds::sound_t::combat, _( "BRZZZAP!" ), false, "trap", "dissector" );
    if( player_sees ) {
        add_msg( m_bad, _( "Electrical beams emit from the floor and slice the %s!" ), c->get_name() );
    }
    c->deal_damage( nullptr, bodypart_id( "head" ), damage_instance( damage_cut, 15 ) );
    c->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_cut, 20 ) );
    c->deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance( damage_cut, 12 ) );
    c->deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance( damage_cut, 12 ) );
    c->deal_damage( nullptr, bodypart_id( "hand_r" ), damage_instance( damage_cut, 10 ) );
    c->deal_damage( nullptr, bodypart_id( "hand_l" ), damage_instance( damage_cut, 10 ) );
    c->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_cut, 12 ) );
    c->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_cut, 12 ) );
    c->deal_damage( nullptr, bodypart_id( "foot_l" ), damage_instance( damage_cut, 10 ) );
    c->deal_damage( nullptr, bodypart_id( "foot_r" ), damage_instance( damage_cut, 10 ) );

    c->check_dead_state();
    return true;
}

bool trapfunc::pit( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    if( c->has_effect( effect_in_pit ) ) {
        return false;
    }
    const float eff = pit_effectiveness( p );
    c->add_msg_player_or_npc( m_bad, _( "You fall in a pit!" ), _( "<npcname> falls in a pit!" ) );
    c->add_effect( effect_in_pit, 1_turns, true );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    if( you != nullptr ) {
        if( you->can_fly() && ( you->has_flag( json_flag_WINGS_1 ) ||
                                you->has_flag( json_flag_WINGS_2 ) ) ) {
            you->add_msg_player_or_npc( _( "You spread your wings to slow your fall." ),
                                        _( "<npcname> spreads their wings to slow their fall." ) );
        } else if( you->has_active_bionic( bio_shock_absorber ) ) {
            you->add_msg_if_player( m_info,
                                    _( "You hit the ground hard, but your grav chute handles the impact admirably!" ) );
            // tiny animals aren't hurt by falling into pits
        } else if( you->get_size() == creature_size::tiny ) {
            you->add_msg_if_player( _( "You are unharmed by the fall." ) );
            return false;
        } else {
            int dodge = you->get_dodge();
            ///\EFFECT_DODGE reduces damage taken falling into a pit
            int damage = eff * rng( 10, 20 ) - rng( dodge, dodge * 5 );
            if( damage > 0 ) {
                you->add_msg_if_player( m_bad, _( "You hurt yourself!" ) );
                // like the message says \-:
                you->hurtall( rng( static_cast<int>( damage / 2 ), damage ), you );
                you->deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( damage_bash, damage ) );
                you->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_bash, damage ) );
            } else {
                you->add_msg_if_player( _( "You land nimbly." ) );
            }
        }
    } else if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s falls into a pit!" ), z->get_name() );
            get_player_character().forced_dismount();
        }
        if( z->get_size() == creature_size::tiny ) {
            return false;
        }
        z->deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( damage_bash, eff * rng( 10,
                        20 ) ) );
        z->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( damage_bash, eff * rng( 10,
                        20 ) ) );
    }
    c->check_dead_state();
    return true;
}

bool trapfunc::pit_spikes( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    if( c->has_effect( effect_in_pit ) ) {
        return false;
    }
    c->add_msg_player_or_npc( m_bad, _( "You fall in a spiked pit!" ),
                              _( "<npcname> falls in a spiked pit!" ) );
    c->add_effect( effect_in_pit, 1_turns, true );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    Character &player_character = get_player_character();
    if( you != nullptr ) {
        int dodge = you->get_dodge();
        int damage = pit_effectiveness( p ) * rng( 20, 50 );
        if( you->can_fly() && ( you->has_flag( json_flag_WINGS_1 ) ||
                                you->has_flag( json_flag_WINGS_2 ) ) ) {
            you->add_msg_player_or_npc( _( "You spread your wings to slow your fall." ),
                                        _( "<npcname> spreads their wings to slow their fall." ) );
        } else if( you->get_size() == creature_size::tiny ) {
            you->add_msg_if_player( _( "You land harmlessly between the spikes." ) );
            return false;
        } else if( you->has_active_bionic( bio_shock_absorber ) ) {
            you->add_msg_if_player( m_info,
                                    _( "You hit the ground hard, but your grav chute handles the impact admirably!" ) );
            ///\EFFECT_DODGE reduces chance of landing on spikes in spiked pit
        } else if( 0 == damage || rng( 5, 30 ) < dodge ) {
            you->add_msg_if_player( _( "You avoid the spikes within." ) );
        } else {
            bodypart_id hit( "bp_null" );
            switch( rng( 1, 10 ) ) {
                case  1:
                    hit = bodypart_id( "leg_l" );
                    break;
                case  2:
                    hit = bodypart_id( "leg_r" );
                    break;
                case  3:
                    hit = bodypart_id( "arm_l" );
                    break;
                case  4:
                    hit = bodypart_id( "arm_r" );
                    break;
                case  5:
                case  6:
                case  7:
                case  8:
                case  9:
                case 10:
                    hit = bodypart_id( "torso" );
                    break;
            }
            you->add_msg_if_player( m_bad, _( "The spikes impale your %s!" ),
                                    body_part_name_accusative( hit ) );
            dealt_damage_instance dealt_dmg = you->deal_damage( nullptr, hit, damage_instance( damage_cut,
                                              damage ) );
            if( !you->has_flag( json_flag_INFECTION_IMMUNE ) &&
                dealt_dmg.type_damage( damage_cut ) > 0 ) {
                const int chance_in = you->has_trait( trait_INFRESIST ) ? 256 : 35;
                if( one_in( chance_in ) ) {
                    you->add_effect( effect_tetanus, 1_turns, true );
                }
            }
        }
    } else if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s falls into a pit!" ), z->get_name() );
            player_character.forced_dismount();
        }
        if( z->get_size() == creature_size::tiny ) {
            return false;
        }
        z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_cut, rng( 20, 50 ) ) );
    }
    c->check_dead_state();
    if( one_in( 4 ) ) {
        add_msg_if_player_sees( p, _( "The spears break!" ) );
        map &here = get_map();
        here.ter_set( p, ter_t_pit );
        // 4 spears to a pit
        for( int i = 0; i < 4; i++ ) {
            if( one_in( 3 ) ) {
                here.spawn_item( p, "pointy_stick" );
            }
        }
    }
    return true;
}

bool trapfunc::pit_glass( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    if( c->has_effect( effect_in_pit ) ) {
        return false;
    }
    c->add_msg_player_or_npc( m_bad, _( "You fall in a pit filled with glass shards!" ),
                              _( "<npcname> falls in pit filled with glass shards!" ) );
    c->add_effect( effect_in_pit, 1_turns, true );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    Character &player_character = get_player_character();
    if( you != nullptr ) {
        int dodge = you->get_dodge();
        int damage = pit_effectiveness( p ) * rng( 15, 35 );
        if( you->can_fly() && ( you->has_flag( json_flag_WINGS_1 ) ||
                                you->has_flag( json_flag_WINGS_2 ) ) ) {
            you->add_msg_player_or_npc( _( "You spread your wings to slow your fall." ),
                                        _( "<npcname> spreads their wings to slow their fall." ) );
        } else if( you->get_size() == creature_size::tiny ) {
            you->add_msg_if_player( _( "You land harmlessly atop the shards." ) );
            return false;
        } else if( you->has_active_bionic( bio_shock_absorber ) ) {
            you->add_msg_if_player( m_info,
                                    _( "You hit the ground hard, but your grav chute handles the impact admirably!" ) );
            ///\EFFECT_DODGE reduces chance of landing on glass in glass pit
        } else if( 0 == damage || rng( 5, 30 ) < dodge ) {
            you->add_msg_if_player( _( "You avoid the glass shards within." ) );
        } else {
            bodypart_id hit( "bp_null" );
            switch( rng( 1, 10 ) ) {
                case  1:
                    hit = bodypart_id( "leg_l" );
                    break;
                case  2:
                    hit = bodypart_id( "leg_r" );
                    break;
                case  3:
                    hit = bodypart_id( "arm_l" );
                    break;
                case  4:
                    hit = bodypart_id( "arm_r" );
                    break;
                case  5:
                    hit = bodypart_id( "foot_l" );
                    break;
                case  6:
                    hit = bodypart_id( "foot_r" );
                    break;
                case  7:
                case  8:
                case  9:
                case 10:
                    hit = bodypart_id( "torso" );
                    break;
            }
            you->add_msg_if_player( m_bad, _( "The glass shards slash your %s!" ),
                                    body_part_name_accusative( hit ) );
            dealt_damage_instance dealt_dmg = you->deal_damage( nullptr, hit, damage_instance( damage_cut,
                                              damage ) );
            if( !you->has_flag( json_flag_INFECTION_IMMUNE ) &&
                dealt_dmg.type_damage( damage_cut ) > 0 ) {
                const int chance_in = you->has_trait( trait_INFRESIST ) ? 256 : 35;
                if( one_in( chance_in ) ) {
                    you->add_effect( effect_tetanus, 1_turns, true );
                }
            }
        }
    } else if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s falls into a pit!" ), z->get_name() );
            player_character.forced_dismount();
        }
        if( z->get_size() == creature_size::tiny ) {
            return false;
        }
        z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_cut, rng( 20,
                        50 ) ) );
    }
    c->check_dead_state();
    if( one_in( 5 ) ) {
        add_msg_if_player_sees( p, _( "The shards shatter!" ) );
        map &here = get_map();
        here.ter_set( p, ter_t_pit );
        // 20 shards in a pit.
        for( int i = 0; i < 20; i++ ) {
            if( one_in( 3 ) ) {
                here.spawn_item( p, "glass_shard" );
            }
        }
    }
    return true;
}

bool trapfunc::lava( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    c->add_msg_player_or_npc( m_bad, _( "The %s burns you horribly!" ), _( "The %s burns <npcname>!" ),
                              get_map().tername( p ) );
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    if( you != nullptr ) {
        for( const bodypart_id &bp : you->get_ground_contact_bodyparts( true ) ) {
            you->deal_damage( nullptr, bp, damage_instance( damage_heat, 20 ) );
        }
        for( const bodypart_id &bp : you->get_ground_contact_bodyparts() ) {
            you->deal_damage( nullptr, bp, damage_instance( damage_heat, 20 ) );
        }
    } else if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s is burned by the lava!" ), z->get_name() );
        }
        // TODO: MATERIALS use fire resistance
        int dam = 30;
        if( z->made_of_any( Creature::cmat_flesh ) ) {
            dam = 50;
        }
        if( z->made_of( material_vegetable ) ) {
            dam = 80;
        }
        if( z->made_of( phase_id::LIQUID ) || z->made_of_any( Creature::cmat_flammable ) ) {
            dam = 200;
        }
        if( z->made_of( material_stone ) ) {
            dam = 15;
        }
        if( z->made_of( material_kevlar ) || z->made_of( material_steel ) ) {
            dam = 5;
        }
        z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_heat, dam ) );
    }
    c->check_dead_state();
    return true;
}

// STUB
bool trapfunc::portal( const tripoint &p, Creature *c, item *i )
{
    // TODO: make this do something unique and interesting
    return telepad( p, c, i );
}

// Don't ask NPCs - they always want to do the first thing that comes to their minds
static bool query_for_item( const Character *pl, const itype_id &itemname, const char *que )
{
    return pl->has_amount( itemname, 1 ) && ( !pl->is_avatar() || query_yn( que ) );
}

static tripoint random_neighbor( tripoint center )
{
    center.x += rng( -1, 1 );
    center.y += rng( -1, 1 );
    return center;
}

static bool sinkhole_safety_roll( Character &you, const itype_id &itemname, const int diff )
{
    ///\EFFECT_STR increases chance to attach grapnel, bullwhip, or rope when falling into a sinkhole

    ///\EFFECT_DEX increases chance to attach grapnel, bullwhip, or rope when falling into a sinkhole

    ///\EFFECT_THROW increases chance to attach grapnel, bullwhip, or rope when falling into a sinkhole
    const int throwing_skill_level = round( you.get_skill_level( skill_throw ) );
    const int roll = rng( throwing_skill_level, throwing_skill_level + you.str_cur + you.dex_cur );
    map &here = get_map();
    if( roll < diff ) {
        you.add_msg_if_player( m_bad, _( "You fail to attach it…" ) );
        you.use_amount( itemname, 1 );
        here.spawn_item( tripoint_bub_ms( random_neighbor( you.pos() ) ), itemname );
        return false;
    }

    std::vector<tripoint_bub_ms> safe;
    for( const tripoint_bub_ms &tmp : here.points_in_radius( you.pos_bub(), 1 ) ) {
        if( here.passable( tmp ) && here.tr_at( tmp ) != tr_pit ) {
            safe.push_back( tmp );
        }
    }
    if( safe.empty() ) {
        you.add_msg_if_player( m_bad, _( "There's nowhere to pull yourself to, and you sink!" ) );
        you.use_amount( itemname, 1 );
        here.spawn_item( tripoint_bub_ms( random_neighbor( you.pos() ) ), itemname );
        return false;
    } else {
        you.add_msg_player_or_npc( m_good, _( "You pull yourself to safety!" ),
                                   _( "<npcname> steps on a sinkhole, but manages to pull themselves to safety." ) );
        you.setpos( random_entry( safe ) );
        if( you.is_avatar() ) {
            g->update_map( you );
        }

        return true;
    }
}

bool trapfunc::sinkhole( const tripoint &p, Creature *c, item *i )
{
    // tiny creatures don't trigger the sinkhole to collapse
    if( c == nullptr || c->get_size() == creature_size::tiny ) {
        return false;
    }
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    map &here = get_map();
    if( z != nullptr ) {
        if( z->has_effect( effect_ridden ) ) {
            add_msg( m_bad, _( "Your %s falls into a sinkhole!" ), z->get_name() );
            get_player_character().forced_dismount();
        }
    } else if( you != nullptr ) {
        bool success = false;
        if( query_for_item( you, itype_grapnel,
                            _( "You step into a sinkhole!  Throw your grappling hook out to try to catch something?" ) ) ) {
            success = sinkhole_safety_roll( *you, itype_grapnel, 6 );
        } else if( query_for_item( you, itype_bullwhip,
                                   _( "You step into a sinkhole!  Throw your whip out to try and snag something?" ) ) ) {
            success = sinkhole_safety_roll( *you, itype_bullwhip, 8 );
        } else if( query_for_item( you, itype_rope_30,
                                   _( "You step into a sinkhole!  Throw your rope out to try to catch something?" ) ) ) {
            success = sinkhole_safety_roll( *you, itype_rope_30, 12 );
        }

        you->add_msg_player_or_npc( m_warning, _( "The sinkhole collapses!" ),
                                    _( "A sinkhole under <npcname> collapses!" ) );
        if( success ) {
            here.remove_trap( p );
            here.ter_set( p, ter_t_pit );
            return true;
        }
        you->add_msg_player_or_npc( m_bad, _( "You fall into the sinkhole!" ),
                                    _( "<npcname> falls into a sinkhole!" ) );
    } else {
        return false;
    }
    here.remove_trap( p );
    here.ter_set( p, ter_t_pit );
    c->mod_moves( -c->get_speed() );
    pit( p, c, i );
    return true;
}

bool trapfunc::ledge( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    monster *m = dynamic_cast<monster *>( c );
    if( m != nullptr && m->flies() ) {
        return false;
    }
    if( c->has_effect_with_flag( json_flag_LEVITATION ) && !c->has_effect( effect_slow_descent ) ) {
        return false;
    }

    map &here = get_map();

    int height = 0;
    tripoint_bub_ms where( p );
    tripoint_bub_ms below( where + tripoint_below );
    creature_tracker &creatures = get_creature_tracker();
    while( here.valid_move( where, below, false, true ) ) {
        where.z()--;
        if( get_creature_tracker().creature_at( where ) != nullptr ) {
            where.z()++;
            break;
        }

        below.z()--;
        height++;
    }

    if( height == 0 && c->is_avatar() ) {
        // For now just special case player, NPCs don't "zedwalk"
        Creature *critter = creatures.creature_at( below, true );
        if( critter == nullptr || !critter->is_monster() ) {
            return false;
        }

        std::vector<tripoint_bub_ms> valid;
        for( const tripoint_bub_ms &pt : here.points_in_radius( below, 1 ) ) {
            if( g->is_empty( pt ) ) {
                valid.push_back( pt );
            }
        }

        if( valid.empty() ) {
            critter->setpos( c->pos() );
            add_msg( m_bad, _( "You fall down under %s!" ), critter->disp_name() );
        } else {
            critter->setpos( random_entry( valid ) );
        }

        height++;
        where.z()--;
    } else if( height == 0 ) {
        return false;
    }

    c->add_msg_if_npc( _( "<npcname> falls down a level!" ) );
    Character *you = dynamic_cast<Character *>( c );
    if( you == nullptr ) {
        c->setpos( where );
        if( c->has_effect( effect_weakened_gravity ) ) {
            height = std::max( 0, height - 1 );
        }
        if( c->has_effect( effect_strengthened_gravity ) ) {
            height += 1;
        }
        c->impact( height * 10, where.raw() );
        return true;
    }

    item jetpack = you->item_worn_with_flag( STATIC( flag_id( "JETPACK" ) ) );
    // TODO: Typify this whole function
    tripoint_bub_ms p_bub = tripoint_bub_ms( p );
    if( you->has_flag( json_flag_WALL_CLING ) &&  get_map().is_clingable_wall_adjacent( p_bub ) ) {
        you->add_msg_player_or_npc( _( "You cling to the nearby wall." ),
                                    _( "<npcname> clings to the wall." ) );
        return false;
    }

    if( you->is_avatar() ) {
        add_msg( m_bad, n_gettext( "You fall down %d story!", "You fall down %d stories!", height ),
                 height );
        g->vertical_move( -height, true );
    } else {
        you->setpos( where );
    }
    if( you->has_effect( effect_weakened_gravity ) ) {
        height = std::max( 0, height - 1 );
    }
    if( you->has_effect( effect_strengthened_gravity ) ) {
        height += 1;
    }
    if( you->has_effect( effect_jumping ) ) {
        height = std::max( 0, ( height - std::max( 0, ( you->get_effect_int( effect_jumping ) - 1 ) ) ) );
    }
    if( you->can_fly() && you->has_flag( json_flag_WINGS_1 ) ) {
        you->add_msg_player_or_npc( _( "You spread your wings to slow your fall." ),
                                    _( "<npcname> spreads their wings to slow their fall." ) );
        height = std::max( 0, height - 1 );
    }
    if( you->can_fly() && you->has_flag( json_flag_WINGS_2 ) ) {
        you->add_msg_player_or_npc( _( "You spread your wings to slow your fall." ),
                                    _( "<npcname> spreads their wings to slow their fall." ) );
        height = std::max( 0, height - 2 );
    }
    if( you->has_active_bionic( bio_shock_absorber ) ) {
        you->add_msg_if_player( m_info,
                                _( "You hit the ground hard, but your grav chute handles the impact admirably!" ) );
    } else if( !jetpack.is_null() ) {
        if( jetpack.ammo_sufficient( you ) ) {
            you->add_msg_player_or_npc( _( "You ignite your %s and use it to break the fall." ),
                                        _( "<npcname> uses their %s to break the fall." ), jetpack.tname() );
            jetpack.activation_consume( 1, you->pos(), you );
        } else {
            you->add_msg_if_player( m_bad,
                                    _( "You attempt to break the fall with your %s but it is out of fuel!" ), jetpack.tname() );
            you->impact( height * 30, where.raw() );

        }
    } else {
        you->impact( height * 30, where.raw() );
    }

    if( here.has_flag( ter_furn_flag::TFLAG_DEEP_WATER, where ) ) {
        you->set_underwater( true );
        g->water_affect_items( *you );
        you->add_msg_player_or_npc( _( "You dive into water." ), _( "<npcname> dives into water." ) );
    }

    return true;
}

bool trapfunc::glow( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr ) {
        return false;
    }
    monster *z = dynamic_cast<monster *>( c );
    Character *you = dynamic_cast<Character *>( c );
    if( z != nullptr ) {
        if( one_in( 3 ) ) {
            z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_acid, rng( 5,
                            10 ) ) );
            z->set_speed_base( z->get_speed_base() * 0.9 );
        }
        if( z->has_effect( effect_ridden ) ) {
            if( one_in( 3 ) ) {
                add_msg( m_bad, _( "You're bathed in radiation!" ) );
                get_player_character().irradiate( rng( 10, 30 ) );
            } else if( one_in( 4 ) ) {
                add_msg( m_bad, _( "A blinding flash strikes you!" ) );
                explosion_handler::flashbang( p );
            } else {
                add_msg( _( "Small flashes surround you." ) );
            }
        }
    }
    if( you != nullptr ) {
        if( one_in( 3 ) ) {
            you->add_msg_if_player( m_bad, _( "You're bathed in radiation!" ) );
            you->irradiate( rng( 10, 30 ) );
        } else if( one_in( 4 ) ) {
            you->add_msg_if_player( m_bad, _( "A blinding flash strikes you!" ) );
            explosion_handler::flashbang( p );
        } else {
            c->add_msg_if_player( _( "Small flashes surround you." ) );
        }
    }
    c->check_dead_state();
    return true;
}

bool trapfunc::hum( const tripoint &p, Creature *, item * )
{
    int volume = rng( 1, 200 );
    std::string sfx;
    if( volume <= 10 ) {
        //~ a quiet humming sound
        sfx = _( "hrm" );
    } else if( volume <= 50 ) {
        //~ a humming sound
        sfx = _( "hrmmm" );
    } else if( volume <= 100 ) {
        //~ a loud humming sound
        sfx = _( "HRMMM" );
    } else {
        //~ a very loud humming sound
        sfx = _( "VRMMMMMM" );
    }
    sounds::sound( p, volume, sounds::sound_t::activity, sfx, false, "humming", "machinery" );
    return true;
}

bool trapfunc::shadow( const tripoint &p, Creature *c, item * )
{
    if( c == nullptr || !c->is_avatar() ) {
        return false;
    }
    map &here = get_map();
    // Monsters and npcs are completely ignored here, should they?
    for( int tries = 0; tries < 10; tries++ ) {
        tripoint monp = p;
        if( one_in( 2 ) ) {
            monp.x = p.x + rng( -5, +5 );
            monp.y = p.y + ( one_in( 2 ) ? -5 : +5 );
        } else {
            monp.x = p.x + ( one_in( 2 ) ? -5 : +5 );
            monp.y = p.y + rng( -5, +5 );
        }
        if( !here.sees( monp, p, 10 ) ) {
            continue;
        }
        if( monster *const spawned = g->place_critter_at( mon_shadow, monp ) ) {
            spawned->add_msg_if_npc( m_warning, _( "A shadow forms nearby." ) );
            spawned->reset_special_rng( "DISAPPEAR" );
            here.remove_trap( p );
            break;
        }
    }
    return true;
}

bool trapfunc::map_regen( const tripoint &p, Creature *c, item * )
{
    if( c ) {
        Character *you = dynamic_cast<Character *>( c );
        if( you ) {
            map &here = get_map();
            you->add_msg_if_player( m_warning, _( "Your surroundings shift!" ) );
            tripoint_abs_omt omt_pos = you->global_omt_location();
            const update_mapgen_id &regen_mapgen = here.tr_at( p ).map_regen_target();
            here.remove_trap( p );
            const ret_val<void> has_colliding_vehicle = run_mapgen_update_func( regen_mapgen, omt_pos, {},
                    nullptr,
                    false );
            if( !has_colliding_vehicle.success() ) {
                popup( _( "Failed to generate the new map, probably due to collision with %s vehicle/appliance." ),
                       has_colliding_vehicle.str() );
                return false;
            }
            set_queued_points();
            here.set_seen_cache_dirty( tripoint_bub_ms( p ) );
            here.set_transparency_cache_dirty( p.z );
            return true;
        }
    }
    return false;
}

bool trapfunc::drain( const tripoint &, Creature *c, item * )
{
    if( c != nullptr ) {
        c->add_msg_if_player( m_bad, _( "You feel your life force sapping away." ) );
        monster *z = dynamic_cast<monster *>( c );
        Character *you = dynamic_cast<Character *>( c );
        if( you != nullptr ) {
            you->hurtall( 1, nullptr );
        } else if( z != nullptr ) {
            z->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( damage_pure, 1 ) );
        }
        c->check_dead_state();
        return true;
    }
    return false;
}

bool trapfunc::cast_spell( const tripoint &p, Creature *critter, item * )
{
    if( critter == nullptr ) {
        map &here = get_map();
        trap tr = here.tr_at( p );
        const spell trap_spell = tr.spell_data.get_spell();
        npc dummy;
        if( !tr.has_flag( json_flag_UNCONSUMED ) ) {
            here.remove_trap( p );
        }
        if( tr.has_flag( json_flag_PROXIMITY ) ) {
            // remove all traps in 3-3 area area
            for( int x = p.x - 1; x <= p.x + 1; x++ ) {
                for( int y = p.y - 1; y <= p.y + 1; y++ ) {
                    tripoint_bub_ms pt( x, y, p.z );
                    if( here.tr_at( pt ).loadid == tr.loadid ) {
                        here.remove_trap( pt );
                    }
                }
            }
        }
        // we remove the trap before casting the spell because otherwise if we teleport we might be elsewhere at the end and p is no longer valid
        trap_spell.cast_all_effects( dummy, tripoint_bub_ms( p ) );
        trap_spell.make_sound( tripoint_bub_ms( p ), get_player_character() );
        return true;
    } else {
        map &here = get_map();
        trap tr = here.tr_at( p );
        const spell trap_spell = tr.spell_data.get_spell( *critter, 0 );
        npc dummy;
        if( !tr.has_flag( json_flag_UNCONSUMED ) ) {
            here.remove_trap( p );
        }
        if( tr.has_flag( json_flag_PROXIMITY ) ) {
            // remove all traps in 3-3 area area
            for( int x = p.x - 1; x <= p.x + 1; x++ ) {
                for( int y = p.y - 1; y <= p.y + 1; y++ ) {
                    tripoint_bub_ms pt( x, y, p.z );
                    if( here.tr_at( pt ).loadid == tr.loadid ) {
                        here.remove_trap( pt );
                    }
                }
            }
        }
        // we remove the trap before casting the spell because otherwise if we teleport we might be elsewhere at the end and p is no longer valid
        trap_spell.cast_all_effects( dummy, critter->pos_bub() );
        trap_spell.make_sound( tripoint_bub_ms( p ), get_player_character() );
        return true;
    }
}

bool trapfunc::snake( const tripoint &p, Creature *, item * )
{
    //~ the sound a snake makes
    sounds::sound( p, 10, sounds::sound_t::movement, _( "ssssssss" ), false, "misc", "snake_hiss" );
    map &here = get_map();
    if( one_in( 6 ) ) {
        here.remove_trap( p );
    }
    if( one_in( 3 ) ) {
        for( int tries = 0; tries < 10; tries++ ) {
            tripoint monp = p;
            if( one_in( 2 ) ) {
                monp.x = p.x + rng( -5, +5 );
                monp.y = p.y + ( one_in( 2 ) ? -5 : +5 );
            } else {
                monp.x = p.x + ( one_in( 2 ) ? -5 : +5 );
                monp.y = p.y + rng( -5, +5 );
            }
            if( !here.sees( monp, p, 10 ) ) {
                continue;
            }
            if( monster *const spawned = g->place_critter_at( mon_shadow_snake, monp ) ) {
                spawned->add_msg_if_npc( m_warning, _( "A shadowy snake forms nearby." ) );
                spawned->reset_special_rng( "DISAPPEAR" );
                here.remove_trap( p );
                break;
            }
        }
    }
    return true;
}

bool trapfunc::dummy_trap( const tripoint &, Creature *, item * )
{
    return true;
}

/**
 * Takes the name of a trap function and returns a function pointer to it.
 * @param function_name The name of the trapfunc function to find.
 * @return A function object with a pointer to the matched function,
 *         or to trapfunc::none if there is no match.
 */
const trap_function &trap_function_from_string( const std::string &function_name )
{
    static const std::unordered_map<std::string, trap_function> funmap = {{
            { "none", trapfunc::none },
            { "bubble", trapfunc::bubble },
            { "glass", trapfunc::glass },
            { "cot", trapfunc::cot },
            { "beartrap", trapfunc::beartrap },
            { "board", trapfunc::board },
            { "caltrops", trapfunc::caltrops },
            { "caltrops_glass", trapfunc::caltrops_glass },
            { "tripwire", trapfunc::tripwire },
            { "crossbow", trapfunc::crossbow },
            { "shotgun", trapfunc::shotgun },
            { "blade", trapfunc::blade },
            { "snare_light", trapfunc::snare_light },
            { "snare_heavy", trapfunc::snare_heavy },
            { "snare_species", trapfunc::snare_species },
            { "landmine", trapfunc::landmine },
            { "telepad", trapfunc::telepad },
            { "goo", trapfunc::goo },
            { "dissector", trapfunc::dissector },
            { "sinkhole", trapfunc::sinkhole },
            { "pit", trapfunc::pit },
            { "pit_spikes", trapfunc::pit_spikes },
            { "pit_glass", trapfunc::pit_glass },
            { "lava", trapfunc::lava },
            { "portal", trapfunc::portal },
            { "ledge", trapfunc::ledge },
            { "boobytrap", trapfunc::boobytrap },
            { "glow", trapfunc::glow },
            { "hum", trapfunc::hum },
            { "shadow", trapfunc::shadow },
            { "map_regen", trapfunc::map_regen },
            { "drain", trapfunc::drain },
            { "spell", trapfunc::cast_spell },
            { "snake", trapfunc::snake },
            { "dummy_trap", trapfunc::dummy_trap }
        }
    };

    const auto iter = funmap.find( function_name );
    if( iter != funmap.end() ) {
        return iter->second;
    }

    debugmsg( "Could not find a trapfunc function matching '%s'!", function_name );
    static const trap_function null_fun = trapfunc::none;
    return null_fun;
}
