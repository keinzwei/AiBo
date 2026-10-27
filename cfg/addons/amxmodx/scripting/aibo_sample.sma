//
// AiBo AMXX sample plugin.
//
// An admin plugin that inspects and controls AiBo bots through the
// natives exposed by the "aibo" module. Compile and drop into
// addons/amxmodx/plugins, then add to plugins.ini.
//
// console commands (admin):
//   aibo_info <target>         dump bot state
//   aibo_freeze <target> <0|1> enable/disable movement
//   aibo_look <target> [reset] force/reset look target
//   aibo_goal <target> <node>  send bot to a graph node
//   aibo_path <target>         print the bot current path
//   aibo_add [t|ct]            add a bot
//   aibo_graph                 graph info + nearest node to you
//

#include <amxmodx>
#include <amxmisc>
#include <aibo>

#define PLUGIN_NAME    "AiBo Sample"
#define PLUGIN_VERSION "1.0"

public plugin_init ()
{
  register_plugin (PLUGIN_NAME, PLUGIN_VERSION, "AiBo");

  register_clcmd ("aibo_info",   "cmd_info",   ADMIN_CFG);
  register_clcmd ("aibo_freeze", "cmd_freeze", ADMIN_CFG);
  register_clcmd ("aibo_look",   "cmd_look",   ADMIN_CFG);
  register_clcmd ("aibo_goal",   "cmd_goal",   ADMIN_CFG);
  register_clcmd ("aibo_path",   "cmd_path",   ADMIN_CFG);
  register_clcmd ("aibo_add",    "cmd_add",    ADMIN_CFG);
  register_clcmd ("aibo_graph",  "cmd_graph",  ADMIN_CFG);
}

// resolve the first argument as a player, the bot native index is the player index
stock bot_target (id)
{
  new arg[32];

  read_argv (1, arg, charsmax (arg));
  return cmd_target (id, arg, CMDTARGET_OBEY_IMMUNITY | CMDTARGET_ALLOW_SELF);
}

// make sure the target is a live aibo bot
stock bool:must_be_bot (id, target)
{
  if (!target || !is_user_connected (target))
  {
    client_print (id, print_chat, "[AiBo] no such player");
    return false;
  }

  if (!aibo_is_user_bot (target))
  {
    client_print (id, print_chat, "[AiBo] %n is not a bot", target);
    return false;
  }
  return true;
}

stock user_origin_f (id, Float:origin[3])
{
  new pos[3];

  get_user_origin (id, pos);
  origin[0] = float (pos[0]);
  origin[1] = float (pos[1]);
  origin[2] = float (pos[2]);
}

// aibo_info <target>
public cmd_info (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new name[32], Float:origin[3];
  new Difficulty:difficulty = aibo_get_bot_difficulty (target);
  new Personality:personality = aibo_get_bot_personality (target);
  new Task:task = aibo_get_bot_task (target);
  new weapon = aibo_get_bot_weapon (target);

  get_user_name (target, name, charsmax (name));

  client_print (id, print_console, "[AiBo] %s (index %d)", name, target);
  client_print (id, print_console, "  node %d | goal %d | task %d (data %d)",
    aibo_get_bot_node (target), aibo_get_bot_goal (target), _:task, aibo_get_bot_task_data (target));
  client_print (id, print_console, "  weapon %d | clip %d | ammo %d | ping %d",
    weapon, aibo_get_bot_ammo_in_clip (target), aibo_get_bot_ammo (target, weapon), aibo_get_bot_ping (target));
  client_print (id, print_console, "  difficulty %d | personality %d | movement %d",
    _:difficulty, _:personality, aibo_is_bot_movement (target));
  client_print (id, print_console, "  stuck %d (%.2fs) | camping %d | path %d",
    aibo_is_bot_stuck (target), aibo_get_bot_stuck_time (target), aibo_is_bot_camping (target),
    aibo_get_bot_path_length (target));

  if (aibo_get_bot_origin (target, origin))
  {
    new enemy = aibo_get_bot_enemy (target);

    client_print (id, print_console, "  origin %.0f %.0f %.0f | enemy %d (reachable %d)",
      origin[0], origin[1], origin[2], enemy, aibo_is_bot_enemy_reachable (target));
  }
  return PLUGIN_HANDLED;
}

// aibo_freeze <target> <0|1>
public cmd_freeze (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new arg[8], value;

  read_argv (2, arg, charsmax (arg));

  if (arg[0])
  {
    value = str_to_num (arg);
  }
  else
  {
    value = aibo_is_bot_movement (target) ? 0 : 1;
  }

  new bool:movement = value != 0;

  aibo_set_bot_movement (target, movement);
  client_print (id, print_chat, "[AiBo] %n movement %s", target, movement ? "on" : "off");

  return PLUGIN_HANDLED;
}

// aibo_look <target> [reset]
public cmd_look (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new Float:origin[3], arg[8];

  read_argv (2, arg, charsmax (arg));

  if (equali (arg, "reset"))
  {
    aibo_set_bot_look_at (target, origin); // zero vector restores ai aiming
    client_print (id, print_chat, "[AiBo] %n look reset", target);
  }
  else
  {
    user_origin_f (id, origin);
    aibo_set_bot_look_at (target, origin);
    client_print (id, print_chat, "[AiBo] %n now looks at you", target);
  }
  return PLUGIN_HANDLED;
}

// aibo_goal <target> <node>
public cmd_goal (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new arg[16], node;

  read_argv (2, arg, charsmax (arg));
  node = str_to_num (arg);

  if (!aibo_set_bot_goal_node (target, node))
  {
    client_print (id, print_chat, "[AiBo] node %d is invalid", node);
    return PLUGIN_HANDLED;
  }
  client_print (id, print_chat, "[AiBo] %n sent to node %d", target, node);

  return PLUGIN_HANDLED;
}

// aibo_path <target>
public cmd_path (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new length = aibo_get_bot_path_length (target);

  client_print (id, print_console, "[AiBo] path of %n (%d nodes):", target, length);

  for (new i = 0; i < length && i < 10; i++)
  {
    client_print (id, print_console, "  [%d] node %d", i, aibo_get_bot_path_node (target, i));
  }
  return PLUGIN_HANDLED;
}

// aibo_add [t|ct]
public cmd_add (id, level, cid)
{
  new arg[8], CsTeams:team = CS_TEAM_UNASSIGNED;

  read_argv (1, arg, charsmax (arg));

  if (equali (arg, "t"))
  {
    team = CS_TEAM_T;
  }
  else if (equali (arg, "ct"))
  {
    team = CS_TEAM_CT;
  }

  if (!aibo_add_bot (Difficulty:Normal, Personality:Normal, team))
  {
    client_print (id, print_chat, "[AiBo] failed to add a bot");
    return PLUGIN_HANDLED;
  }
  client_print (id, print_chat, "[AiBo] adding a bot, count is now %d", aibo_get_bot_count ());

  return PLUGIN_HANDLED;
}

// aibo_graph
public cmd_graph (id, level, cid)
{
  if (!aibo_has_graph ())
  {
    client_print (id, print_chat, "[AiBo] this map has no graph");
    return PLUGIN_HANDLED;
  }

  new author[32], modified[32], version[32], api = aibo_get_api_version ();

  aibo_get_bot_version (version, charsmax (version));
  aibo_get_graph_author (author, charsmax (author));
  aibo_get_graph_modified (modified, charsmax (modified));

  client_print (id, print_console, "[AiBo] %s | api %d.%d | graph %d nodes by '%s'",
    version, api >>> 16, api & 0xFFFF, aibo_get_node_count (), author);

  if (modified[0])
  {
    client_print (id, print_console, "  modified by '%s'", modified);
  }

  // nearest node to the caller, with decoded flags
  new Float:origin[3], node;

  user_origin_f (id, origin);
  node = aibo_get_nearest_node (origin);

  if (aibo_is_node_valid (node))
  {
    new Float:nodeOrigin[3];
    new NodeFlag:flags;

    aibo_get_node_origin (node, nodeOrigin);
    flags = aibo_get_node_flags (node);

    client_print (id, print_console, "[AiBo] nearest node %d at %.0f %.0f %.0f",
      node, nodeOrigin[0], nodeOrigin[1], nodeOrigin[2]);
    client_print (id, print_console, "  lift %d | crouch %d | camp %d | sniper %d | links %d",
      !!(_:flags & _:Lift), !!(_:flags & _:Crouch), !!(_:flags & _:Camp), !!(_:flags & _:Sniper),
      aibo_get_node_link_count (node));
  }
  else
  {
    client_print (id, print_console, "[AiBo] no node found near you");
  }
  return PLUGIN_HANDLED;
}
