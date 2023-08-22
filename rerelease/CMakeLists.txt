
cmake_minimum_required(VERSION 3.23.1)

set(CMAKE_C_COMPILER "g++")
set(CMAKE_CXX_COMPILER "g++")

project(game)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE GAME_SRC 
	bots/bot_debug.cpp 
    bots/bot_exports.cpp 
    bots/bot_think.cpp 
    bots/bot_utils.cpp 
    cg_main.cpp 
    cg_screen.cpp 
    ctf/g_ctf.cpp 
    ctf/p_ctf_menu.cpp 
    g_ai.cpp 
    g_chase.cpp 
    g_cmds.cpp 
    g_combat.cpp 
    g_func.cpp 
    g_items.cpp 
    g_main.cpp 
    g_misc.cpp 
    g_monster.cpp 
    g_phys.cpp 
    g_save.cpp 
    g_spawn.cpp 
    g_svcmds.cpp 
    g_target.cpp 
    g_trigger.cpp 
    g_turret.cpp 
    g_utils.cpp 
    g_weapon.cpp 
    m_actor.cpp 
    m_arachnid.cpp 
    m_berserk.cpp 
    m_boss2.cpp 
    m_boss3.cpp 
    m_boss31.cpp 
    m_boss32.cpp 
    m_brain.cpp 
    m_chick.cpp 
    m_flipper.cpp 
    m_float.cpp 
    m_flyer.cpp 
    m_gladiator.cpp 
    m_guardian.cpp 
    m_guncmdr.cpp 
    m_gunner.cpp 
    m_hover.cpp 
    m_infantry.cpp 
    m_insane.cpp 
    m_medic.cpp 
    m_move.cpp 
    m_mutant.cpp 
    m_parasite.cpp 
    m_shambler.cpp 
    m_soldier.cpp 
    m_supertank.cpp 
    m_tank.cpp 
    p_client.cpp 
    p_hud.cpp 
    p_move.cpp 
    p_trail.cpp 
    p_view.cpp 
    p_weapon.cpp 
    q_std.cpp 
    action/a_cmds.c
    action/a_ctf.c
    action/a_dom.c
    action/a_doorkick.c
    action/a_game.c
    action/a_items.c
    action/a_match.c
    action/a_menu.c
    action/a_radio.c
    action/a_team.c
    action/a_tourney.c
    action/a_vote.c
    action/a_xcmds.c
    action/a_xgame.c
    action/a_xmenu.c
    action/a_xvote.c
    action/cgf_sfx_glass.c
    action/game.c
    action/g_chase.c
    action/g_cmds.c
    action/g_combat.c
    action/g_func.c
    action/g_grapple.c
    action/g_items.c
    action/g_main.c
    action/g_misc.c
    action/g_phys.c
    action/g_save.c
    action/g_spawn.c
    action/g_svcmds.c
    action/g_target.c
    action/g_trigger.c
    action/g_utils.c
    action/g_weapon.c
    action/m_player.c
    action/p_client.c
    action/p_hud.c
    action/p_view.c
    action/p_weapon.c
    action/q_shared.c
    action/tng_balancer.c
    action/tng_flashlight.c
    action/tng_ini.c
    action/tng_jump.c
    action/tng_stats.c

    rogue/g_rogue_combat.cpp 
    rogue/g_rogue_func.cpp 
    rogue/g_rogue_items.cpp 
    rogue/g_rogue_misc.cpp 
    rogue/g_rogue_monster.cpp 
    rogue/g_rogue_newai.cpp 
    rogue/g_rogue_newdm.cpp 
    rogue/g_rogue_newfnc.cpp 
    rogue/g_rogue_newtarg.cpp 
    rogue/g_rogue_newtrig.cpp 
    rogue/g_rogue_newweap.cpp 
    rogue/g_rogue_phys.cpp 
    rogue/g_rogue_spawn.cpp 
    rogue/g_rogue_sphere.cpp 
    rogue/g_rogue_utils.cpp 
    rogue/m_rogue_carrier.cpp 
    rogue/m_rogue_stalker.cpp 
    rogue/m_rogue_turret.cpp 
    rogue/m_rogue_widow.cpp 
    rogue/m_rogue_widow2.cpp 
    rogue/p_rogue_weapon.cpp 
    rogue/rogue_dm_ball.cpp 
    rogue/rogue_dm_tag.cpp 

    xatrix/g_xatrix_func.cpp 
    xatrix/g_xatrix_items.cpp 
    xatrix/g_xatrix_misc.cpp 
    xatrix/g_xatrix_monster.cpp 
    xatrix/g_xatrix_target.cpp 
    xatrix/g_xatrix_weapon.cpp 
    xatrix/m_xatrix_fixbot.cpp 
    xatrix/m_xatrix_gekk.cpp 
    xatrix/p_xatrix_weapon.cpp 
    
    )

include(FetchContent)

FetchContent_Declare(fmt
GIT_REPOSITORY https://github.com/fmtlib/fmt.git
GIT_TAG master
)
FetchContent_MakeAvailable(fmt)

add_definitions(-DKEX_Q2_GAME -DKEX_Q2GAME_EXPORTS -DNO_FMT_SOURCE -DKEX_Q2GAME_DYNAMIC -DCMAKE_CXX_FLAGS="-std=libc++")
add_library(game SHARED ${GAME_SRC})
target_include_directories(game PUBLIC /usr/local/Cellar/jsoncpp/1.9.5/include)
set_target_properties(game PROPERTIES PREFIX "")
set_target_properties(game PROPERTIES OUTPUT_NAME "game")
# find_library(FMT fmt)
find_library(JSONCPP jsoncpp)
# target_link_libraries(game ${FMT} ${JSONCPP})
target_link_libraries(
    game 
    fmt::fmt 
    ${JSONCPP}
    )