A Bakkesmod plugin for multiplayer custom training drills.

Note: I'll be releasing an update soon that will add a UI for a better user experience.

# Usage

This plugin depends on [Rocket Plugin](https://bakkesplugins.com/plugins/view/26) for hosting a freeplay session that other players may join. Please consult their documentation on how to do this.

Once all players have joined the freeplay session, press F6 to bring up the console.

To list available training packs, run in console: `team_train_list`

To load a training pack, run: `team_train_load <pack>`, replacing pack with the name of the pack file (eg: `team_train_load left` to load left.json)

To shift the roles of each player (eg: passer becomes shooter, shooter becomes passer), run: `team_train_cycle_players`. This will change in the future once I get the UI figured out.

To randomly shuffle the roles of each player, run: `team_train_randomize_players`.

There also exists commands `team_train_reset`, `team_train_next`, and `team_train_prev` to reset the shot, switch to the next shot, or go back to the previous shot. When a training pack is loaded, reset is binded to d-pad up/num key 1, and prev and next are binded to d-pad left/num key 2 and d-pad right/num key 3.

Set `cl_team_training` to 1 to randomize the shots in the training pack, and `cl_team_training_countdown` to any number of seconds to wait before setting the ball in motion.

The plugin comes packaged with three packs: left, right, and infield. These packs were generated from Wayprotein's passing packs: C833-6A35-A46A-7191, 0590-9035-801A-E423, CDBB-8953-C052-654F. Note the shooter positions are a little awkward as they are positioned after the pass is in progress.

# Creating Team Training Packs

Team training packs can be created using regular custom training packs. The way this works is by generating each player's position in subsequent drills within the custom training pack, and the ball's state is taken from the first passer.

For example, suppose you want to create a training pack with two passers, one shooter, and one defender. The first drill in the custom training pack should be the starting position of the shooter. The second drill should be the position of the second passer (who receives the pass from the first passer). The third drill will be the position of the first passer, and the ball position and trajectory is taken from this drill. The final drill will be the position of the defender. Repeat this same pattern of drills to have more than one drill in the team training pack. For three drills in the team training pack, the order of drills in the custom training pack will be as follows:

- Drill #1: Shooter position
- Drill #2: Second passer position
- Drill #3: First passer position and ball position/trajectory
- Drill #4: Defender
- Drill #5: Shooter position
- Drill #6: Second passer position
- Drill #7: First passer position and ball position/trajectory
- Drill #8: Defender
- Drill #9: Shooter position
- Drill #10: Second passer position
- Drill #11: First passer position and ball position/trajectory
- Drill #12: Defender

To convert the custom training pack into a team training pack, the plugin comes with a `write_shot_info` command that can be run from the console. You must pass in 4 arguments to this command: the number of offensive players, the number of defensive players, the total number of drills that will be in the final team training pack, and a name for the training pack file (don't use spaces). To convert the custom training pack described above, you would run `write_shot_info 3 1 3`, as there are 3 offensive players, 1 defensive player, and 3 team training drills. For each shooting drill, you will need to supply some sort of user input in order for the ball position and trajectory to be recorded. No user input is needed for any of the other drills.

# Contact

* [@PenguinDaft](twitter.com/PenguinDaft)
* Discord: DaftPenguin#5103