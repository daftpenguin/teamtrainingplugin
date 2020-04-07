A Bakkesmod plugin for multiplayer custom training drills.

Now updated with UI.

# Usage

This plugin depends on [Rocket Plugin](https://bakkesplugins.com/plugins/view/26) for hosting a freeplay session that other players may join.

Once all players have joined the freeplay session, press F2, go to plugins, select Team Training Plugin, and click the button to launch the UI.

The new UI contains tabs for selecting training packs, assigning player roles, converting a custom training pack into a team training pack, and modifying the plugin's settings.

If you have converted any training packs other than the ones provided, some changes were made to the format of the custom training packs and it is recommended that you run the conversion on it again to update it. The custom training pack code will now be saved with the pack so that this may be automated in the future.

The plugin comes packaged with three packs: left, right, and infield. These packs were generated from Wayprotein's passing packs: C833-6A35-A46A-7191, 0590-9035-801A-E423, CDBB-8953-C052-654F. Note the shooter positions are a little awkward as they are positioned after the pass is in progress.

# Creating Team Training Packs

Team training packs can be created using regular custom training packs. The way this works is by setting each player's position in separate drills within the custom training pack, and then the ball's state is taken from the first passer.

Opening the UI, clicking on the Creation tab, and filling in the number of offensive and defensive players will show the drill order at the bottom. For example, set offensive players to 3 and defensive players to 1. The drill order will show as shooter, followed by 2 passer drills, and end with 1 defender drill. This means that the first drill should be the shooter's position, the second drill will be the second passer position (passes to shooter from first passer's pass), the third drill will be the first passer position AND the ball's starting position and trajectory in the team training drill, and the fourth drill will be the defender's position. Therefore, these 4 drills will make up one team training drill. Repeat this pattern in the same custom training pack to have more than one team training drill.

Once you have prepared your custom training pack, open the Team Training plugin's UI, make sure the details are correct, and press the convert button. The plugin should close the UI, automatically skip through all the drills and retrieve the data necessary for the team training pack (don't press anything while this is happening). Once it's done recording the data, it will stop skipping through the drills and a toast notification will display saying it's completed (if you have toast notifications enabled). If you bring up the plugin's UI, your pack should now show in the pack selection tab.

Defense only training packs do not work right now. These may or may not be supported in a future update as I'm unsure about how these should be implemented.

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