# Team Training Plugin

Source code: https://github.com/daftpenguin/teamtrainingpluginsrc

**Warning:** The initial release of this plugin is a very early version and there's potential that it may crash your game. To be safe, I recommend that you restart Rocket League and BakkesMod before jumping into ranked after using this plugin. It also lacks many features I wanted to add before releasing, but unfortunately life got in the way so I'm releasing it in its current state.

This plugin utilizes a feature in BakkesMod that allows players to host and join freeplay lobbies. In my testing, I found that sometimes the connection to the host may drop and attempts to reconnect did not work. Restarting both Rocket League and BakkesMod seemed to sometimes fix the problem.

## Installation

This may look like a lot to do, but it's mostly straight-forward and I attempted to make my instructions as thorough as possible.

### Install BakkesMod

This plugin requires BakkesMod which can be downloaded from: https://bakkesmod.com/download.php. Just extract the executable from the zip and run it. Your anti-malware program(s) will certainly warn you about BakkesMod (if it doesn't, find a new anti-malware program) as it modifies the memory of a running application, but it is safe to install.

### Allowing Players to Connect to the Host

For others to connect to your freeplay lobby, they either need to be in the same local network as you, or you need to mess with forwarding ports in your router and/or firewall. An easier solution is to setup a VPN for everyone to connect to. I found Radmin-VPN to be very easy to setup and use, but you're free to use whatever solution you prefer (instructions assume you're using Radmin-VPN).

To install Radmin-VPN, download and install it from: https://www.radmin-vpn.com. Have one player `Create a new network` in Radmin-VPN found in the `Network` menu, set the name and password of the network and create it (the host of the freeplay server does not have to be the one who creates the network). Any other player that wants to join the freeplay server must choose `Join an existing network` also found in the `Network` menu and use the name and password chosen by the creator.

### Install Team Training Plugin

Only the freeplay host has to install the plugin, and it'll work fine if a non-host user also has the plugin installed.

Download the plugin files from https://github.com/daftpenguin/teamtrainingplugin/archive/master.zip

Now open BakkesMod and from the `File` menu, select `Open BakkesMod Folder`. In the zip file containing the plugin files, you will see 3 folders: `cfg`, `plugins`, and `teamtraining`. Extract everything from the zip file into any directory, then copy and paste the 3 folders into the BakkesMod folder that you opened via BakkesMod.

### Optional: Setup BakkesMod to Autoload Plugin

By default, BakkesMod does not automatically load the plugin and therefore you must enter `plugin load teamtrainingplugin` into the BakkesMod console every time you want to use it. Alternatively, you can add the `plugin load teamtrainingplugin` to the `plugins.cfg` file that you can find in the `cfg` folder inside of the BakkesMod folder.

## Using Team Training Plugin

This plugin requires you to first launch a freeplay server and for all the players to connect. To do this, the host user must first open the BakkesMod console `F6` and type `host` into the console and press enter. This will launch the host user into a freeplay lobby. Now every other non-host player must open the BakkesMod console and type `connect 1.1.1.1`, replacing `1.1.1.1` with the IP address that Radmin-VPN assigned to the host, and press enter. The IP the VPN assigns to each user is located at the top of their Radmin-VPN window, and each user must use the IP that is **assigned to the host user** (the IP at the top of the host's Radmin-VPN window, not their own).

Once each user is connected to the freeplay lobby, the host user can now load any packs found in the `teamtraining` folder included in the download files for the plugin. Currently, the only pack included is a 2 player training pack (note: the last shot is broken) and can only be used with 2 players connected to the lobby (will remove this limitation in the future). This pack was generated from Wayprotein's downfield-left passing pack (0590-9035-801A-E423 - https://www.youtube.com/watch?v=dGQKmn-A2W8). To load the pack, the host user can type `team_train_load pack1` into the BakkesMod console and press enter. When more packs are added, you can load them by changing `pack1` to the name of the pack file in the `teamtraining` folder (will add the ability to browse and select packs later).

Loading the pack will start the first drill. By default, when the shot is set, the ball is frozen for 1 second before it starts to move, but players will still be able to move their cars. You may adjust the number of seconds the ball is frozen by typing `cl_team_training_countdown 5`, replacing 5 with any number between 0 and 10, and pressing enter in the BakkesMod console.

The default key bindings are defined in the `cfg/team_training.cfg` file. By default, pressing d-pad up will reset the shot, d-pad left will go to the previous shot, and d-pad right will advance to the next shot. Unfortunately, I was not able to reset the shot using whatever button you have configured to reset shots in your controller settings. I hope to figure this out in the future.

The choosing of what position to put each player is based on the order in which they connect to the lobby. The first player (host) will be the first passer, if there's a second passer, the second player will be the second passer, else they will be the shooter. If the pack includes defenders, then every player after the shooter will be a defender. It's a bit clunky right now and I will update it in the future, but you currently can change the ordering by typing `team_train_randomize_players`. This will just randomize their ordering and could result in the same ordering. You can also use `team_train_cycle_players` to move each player to the next position in the order (passer1 becomes passer2 or shooter, shooter becomes defender1 or passer1). This will change in the future.

## Updates

This plugin will not auto-update when new versions are released. For now, if you would like to be notified when new versions are released I will tweet them https://twitter.com/penguindaft until I find a better solution. Otherwise, I will be pushing all new versions here on GitHub.

## Problems or Suggestions?

The best method to contact me is probably on Discord (DaftPenguin#5103), Reddit (https://www.reddit.com/user/DaftPenguinRL), or Twitter (https://twitter.com/penguindaft).

Enjoy.
