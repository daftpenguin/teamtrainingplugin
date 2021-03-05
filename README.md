![Training Pack Screenshot](/images/team-training-banner.png)

A Bakkesmod plugin for multiplayer custom training drills and improved custom training organization and discovery.

# Usage

For multiplayer training with non-local players, you can use [Rocket Plugin](https://bakkesplugins.com/plugins/view/26) for hosting a freeplay session that others may join.

Press F2, go to plugins, select Team Training Plugin, and click the button to launch the UI.

The UI contains tabs for selecting training packs, downloading new training packs, assigning player roles for multiplayer training packs, converting a custom training pack into a team training pack, and modifying the plugin's settings.

If you have converted any training packs other than the ones provided, some changes were made to the format of the custom training packs and it is recommended that you run the conversion on it again to update it.

The plugin comes packaged with three packs: Passing (Downfield Left), Passing (Downfield Right), and Passing (Infield). These packs were generated from Wayprotein's passing packs and are the inspiration for this plugin. Note the shooter positions are a little awkward as they are positioned after the pass is in progress.

# Downloading, Uploading, and Organizing Training Packs

The new v0.3.0 release introduced the ability to download, share, and organize your training packs. Any training packs that you create can be uploaded and shared with others by going to the selection tab, selecting the pack, and clicking the "Upload" button that should appear. Note that any packs created prior to the v0.3.0 release will need to be recreated as new data has been introduced to the training pack files.

All local training packs now have tags which you may modify locally to better organize your packs. Search filters can then be applied to search the local packs in the selection tab, or search for new packs on the server in the download tab.

To make it easier to leverage the tagging and search features on local training packs for single player custom training packs, there are two new buttons that will add all your favorited packs to the plugin's selection tab, and also them by their code.

# Creating Team Training Packs

Team training packs can be created using regular custom training packs. The way this works is by setting each player's position in separate shots within the custom training pack. Here's an example of my JZR Freestyle Friday 107 pack:

![Training Pack Screenshot](/images/training-pack-example.png)

Entering the the number of offensive and defensive players in the Creation tab will show the drill order. In this example, the order is shooter then passer. The offensive player ordering makes the assumption that the offensive players will make a sequence of passes from one player to the next, and the last player will take the shot. Packs don't need to necessarily be designed exactly this way, but the last passer in the shot pattern is the shot where the ball's starting position and trajectory will be recorded from. To create a 3 offensive player training pack, the order would be shooter, passer, passer, and the ball will be recorded from shot #3 instead of shot #2. For packs including defensive players, the defensive player positions will follow the last passer, for as many defensive players as needed. Repeat the pattern to create more than 1 team training drill.

Once you have prepared your custom training pack, open your custom training pack to the first drill, open the Team Training plugin's UI, fill in the correct details, and press the convert button. The plugin should close the UI and automatically skip through the drills while retrieving the data necessary for the team training pack (don't press anything while this is happening). Once it's done recording the data, it will stop skipping through the drills and a toast notification will display saying it's completed (if you have toast notifications enabled). If you bring up the plugin's UI, your pack should now show in the pack selection tab.

Defense only training packs do not work right now. These may or may not be supported in a future update.

Note: In future updates, I would also like to be able to show targets and/or ghost balls to give an idea on how some drills might be executed. With this said, I encourage anyone who is making a custom training pack for the purposes of converting it to also appropriately setup the ball in each drill, similar to Wayprotein's packs mentioned above, even if this data is currently unused. It may be necessary for these drills to be technically impossible to complete in the custom training pack, in order to work around the awkwardness in conversion like in the Wayprotein packs (ie: shooter positioned before the pass, but ball positioned at some point during the pass's progression). The current version of the training pack conversion will record the codes of the packs in the training pack file, with hopes that later versions can be updated to include every ball position in the pack in order to create more features for the plugin.

## Testing Team Training Packs

If you would like to test your team training pack without having to get teammates and setup a VPN/port-forwarding, you can add bots into a freeplay session using Rocket Plugin. To do this, load freeplay, then open Rocket Plugin. Go to the "In Game Mods" tab, expand the "Game Event Mods" section, expand the "Bots" section, check "Unfair Teams", and then finally set the "Max # bots per team" to the total number of players you need for the training pack minus the number of local players (eg: if 3 players needed in training pack and you're not playing splitscreen, create 2 bots). It may also be helpful to check freeze bots so they don't move. After you have enough bots, open the selection tab in the team training plugin UI, select your training pack, and click load Team Training pack. You should now be able to skip through the drills and view the starting positions of all the players and the ball.

# Changelog

v0.3.0
* Added the ability to search, download, and share any team training and custom training packs
* Added support for tags to organize training packs
* Added support for custom training metadata for organizing and downloading custom training packs
* Added button to add custom training packs via code and add all favorited packs
* Fixed issue with non-ascii characters not showing in UI

v0.2.8
* Updated to work with upcoming Epic Games support

v0.2.7
* Fixed bug causing crashes when variance is enabled

v0.2.6
* Updated link from old bakkesmod.lib to new pluginsdk.lib
* Added a few more null checks that may fix potential crashes

v0.2.5
* Added what's new tab to share new changes when launching after plugin was updated
* Added support for drill shuffling and variance in drills using BakkesMod's built-in custom training options
* Fixed game crashing due to an incomplete install where training packs folder is missing
* Fixed game crashes caused by missing null checks

v0.2.4
* Fixed bindings not resetting when leaving freeplay/unloading plugin after loading a pack
* Bindings will no longer be set when a pack fails to load
* Added more checks to prevent issues when commands are misused

v0.2.3
* Added some checks to prevent crashes due to misuse
* Fixed drill randomization
* Scoring now advances to the next drill, using shot reset button resets the drill

v0.2.2
* Added/fixed defenders in packs for loading and creating packs
* Fixed crash when pack's description or creator contains an apostrophe
* Block creation of packs with no offensive players

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