A Bakkesmod plugin for multiplayer custom training drills and improved custom training organization and discovery.

# Usage

For team training with non-local players, use [Rocket Plugin](https://bakkesplugins.com/plugins/view/26) for hosting/joining a multiplayer freeplay session.

Press F2, go to plugins, select Team Training Plugin, and click the button to launch the UI.

The UI contains tabs for selecting training packs, downloading new training packs, assigning player roles for multiplayer training packs, converting a custom training pack into a team training pack, and modifying the plugin's settings.

The plugin comes packaged with three packs: Passing (Downfield Left), Passing (Downfield Right), and Passing (Infield), which are generated from Wayprotein's passing packs and are the inspiration for this plugin.

# Downloading, Uploading, and Organizing Training Packs

The v0.3.0 release introduced the ability to download, share, and organize your training packs. Any training packs that you create can be uploaded and shared with others by going to the selection tab, selecting the pack, and clicking the "Upload" button that should appear. Note: any packs created prior to the v0.3.0 release will need to be recreated.

All local training packs now have tags which you may modify locally to better organize your packs. Search filters can then be applied to search the local packs in the selection tab, or search for new packs on the server in the download tab.

To make it easier to leverage the tagging and search features on local training packs for single player custom training packs, there are two new buttons that will add all your favorited packs to the plugin's selection tab, and also them by their code.

# Creating Team Training Packs

Team training packs can be created using regular custom training packs. The way this works is by setting each player's position in separate shots within the custom training pack. Here's an example of my JZR Freestyle Friday 107 pack:

![Training Pack Screenshot](https://raw.githubusercontent.com/daftpenguin/teamtrainingplugin/master/images/training-pack-example.png)

Entering the the number of offensive and defensive players in the Creation tab will show the drill order. In this example, the order is shooter then passer. The offensive player ordering makes the assumption that the offensive players will make a sequence of passes from one player to the next, and the last player will take the shot. Packs don't need to necessarily be designed exactly this way, but the last passer in the shot pattern is the shot where the ball's starting position and trajectory will be recorded from. To create a 3 offensive player training pack, the order would be shooter, passer, passer, and the ball will be recorded from shot #3 instead of shot #2. For packs including defensive players, the defensive player positions will follow the last passer, for as many defensive players as needed. Repeat the pattern to create more than 1 team training drill.

Once you have prepared your custom training pack, open your custom training pack to the first drill, open the Team Training plugin's UI, fill in the correct details, and press the convert button. The plugin should close the UI and automatically skip through the drills while retrieving the data necessary for the team training pack (don't press anything while this is happening). Once it's done recording the data, it will stop skipping through the drills and a toast notification will display saying it's completed (if you have toast notifications enabled). If you bring up the plugin's UI, your pack should now show in the pack selection tab.

# Changelog

Plugin character limits prevent me from including this. See bottom of [README.md](https://github.com/daftpenguin/teamtrainingplugin/blob/master/README.md)

# Contact

* [@PenguinDaft](https://twitter.com/PenguinDaft)
* Discord: DaftPenguin#5103