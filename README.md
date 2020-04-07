A Bakkesmod plugin for multiplayer custom training drills.

Now updated with UI.

# Usage

This plugin depends on [Rocket Plugin](https://bakkesplugins.com/plugins/view/26) for hosting a freeplay session that other players may join. Please consult their documentation on how to do this.

Once all players have joined the freeplay session, press F2, go to plugins, select Team Training Plugin, and click the button to launch the UI (you may also add a binding to launch `togglemenu teamtrainingplugin` to launch the UI with a key press).

The new UI contains tabs for selecting training packs, assigning player roles, converting a custom training pack into a team training pack, and modifying the plugin's settings.

If you have converted any training packs other than the ones provided, some changes were made to the format of the custom training packs and it is recommended that you run the conversion on it again to update it. The custom training pack code will now be saved with the pack so that this may be automated in the future.

All the previous console commands still exist, but they are deprecated and may behave differently or not even work. Visit the plugin's homepage for more information about the console commands.

The plugin comes packaged with three packs: left, right, and infield. These packs were generated from Wayprotein's passing packs: C833-6A35-A46A-7191, 0590-9035-801A-E423, CDBB-8953-C052-654F. Note the shooter positions are a little awkward as they are positioned after the pass is in progress.

# Creating Team Training Packs

Team training packs can be created using regular custom training packs. The way this works is by setting each player's position in separate drills within the custom training pack, and then the ball's state is taken from the first passer.

Opening the UI, clicking on the Creation tab, and filling in the number of offensive and defensive players will show the drill order at the bottom. For example, set offensive players to 3 and defensive players to 1. The drill order will show as shooter, followed by 2 passer drills, and end with 1 defender drill. This means that the first drill should be the shooter's position, the second drill will be the second passer position (passes to shooter from first passer's pass), the third drill will be the first passer position AND the ball's starting position and trajectory in the team training drill, and the fourth drill will be the defender's position. Therefore, these 4 drills will make up one team training drill. Repeat this pattern in the same custom training pack to have more than one team training drill.

Once you have prepared your custom training pack, open the Team Training plugin's UI, make sure the details are correct, and press the convert button. The plugin should close the UI, automatically skip through all the drills and retrieve the data necessary for the team training pack (don't press anything while this is happening). Once it's done recording the data, it will stop skipping through the drills and a toast notification will display saying it's completed (if you have toast notifications enabled). If you bring up the plugin's UI, your pack should now show in the pack selection tab.

Defense only training packs do not work right now. These may or may not be supported in a future update as I'm unsure about how these should be implemented.

Note: In future updates, I would also like to be able to show targets and/or ghost balls to give an idea on how some drills might be executed. With this said, I encourage anyone who is making a custom training pack for the purposes of converting it to also appropriately setup the ball in each drill, similar to Wayprotein's packs mentioned above, even if this data is currently unused. It may be necessary for these drills to be technically impossible to complete in the custom training pack, in order to work around the awkwardness in conversion like in the Wayprotein packs (ie: shooter positioned before the pass, but ball positioned at some point during the pass's progression). The current version of the training pack conversion will record the codes of the packs in the training pack file, with hopes that later versions can be updated to include every ball position in the pack in order to create more features for the plugin.

# Console Commands

Originally, this plugin was released without a GUI and was operated with console commands. Press F6 to bring up the BakkesMod console.

To list available training packs, run in console: `team_train_list`

To load a training pack, run: `team_train_load <pack>`, replacing `<pack>` with the name of the pack file without the file extension (eg: `team_train_load left` to load left.json). You may also replace `<pack>` with an absolute path to a file.

To shift the roles of each player (eg: passer becomes shooter, shooter becomes passer), run: `team_train_cycle_players`.

To randomly shuffle the roles of each player, run: `team_train_randomize_players`.

There also exists commands `team_train_reset`, `team_train_next`, and `team_train_prev` to reset the shot, switch to the next shot, or go back to the previous shot. When a training pack is loaded, reset is binded to d-pad up/num key 1, and prev and next are binded to d-pad left/num key 2 and d-pad right/num key 3.

Set `cl_team_training` to 1 to randomize the shots in the training pack, and `cl_team_training_countdown` to any number of seconds to wait before setting the ball in motion.

**Deprecated, may no longer work:** To convert the custom training pack into a team training pack, the plugin comes with a `write_shot_info` command that can be run from the console. You must pass in 4 arguments to this command: the number of offensive players, the number of defensive players, the total number of drills that will be in the final team training pack, and a name for the training pack file (don't use spaces). To convert the custom training pack described above, you would run `write_shot_info 3 1 3 filename`, as there are 3 offensive players, 1 defensive player, and 3 team training drills. For each shooting drill, you will need to supply some sort of user input in order for the ball position and trajectory to be recorded. No user input is needed for any of the other drills.

# Changelog

v0.2.1
* Disabled defensive players for conversion, as it is broken right now
* Fixed crash when converting from unpublished custom training pack

v0.2.0
* Added new UI for pack selection, creation, player role assignments, and modifying cooldown and randomization settings
* Custom training pack conversion into team training pack is fully automated now (requires no user input for ball trajectory)
* Upgraded training pack format to include custom training pack code, creator name, and version number

v0.1.0
* Initial release, simply updated from old plugin to 64-bit

# Contact

* [@PenguinDaft](twitter.com/PenguinDaft)
* Discord: DaftPenguin#5103