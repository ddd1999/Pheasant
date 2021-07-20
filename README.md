# Pheasant

**Pheasant** is a project for my Computer Networks class in Faculty of Computer Science, 2nd year.

Pheasant is a computer implementation of the popular game of the same name. A game
goes the following : A player chooses a letter. The next player will enter a valid word that
starts with the letter the first player chose. This player will pass the last two letters onto the
next player, who will enter a word starting with these two letters, passing the last two to the
next one. If a player makes a mistake ( word invalid / doesn’t start with given letters ), he is
kicked out of the session and the game is continued by the remaining players. The game
ends when only one player remains, who will be declared winner.  

In the project’s requirements, an advice regarding the player count in a game is given ( … to
be given in a configuration file in the server ). Another method of implementing proper
games is to have an intermediate section between connecting to the server and playing the
game, represented by a lobbying section (matchmaking). In here, players can create/join
lobbies. Once a lobby is ready, the game can start.  

The client will be interacted with in a command line interface, allowing users to issue
commands before a game’s start so that they can see active lobbies, create / join a lobby,
quit a lobby / game. Upon the game’s start, only word inputs ( and the first letter input ) will
be required from the user. Upon a game’s finish/loss, the user will be returned to the
matchmaking section.  
