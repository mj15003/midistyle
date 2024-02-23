# midistyle
Utility to generate MIDI SysEx messages to control style playback on YAMAHA digital audio workstation
---------------------------------------------
# USAGE
midistyle -i input-file-name -p output midi port|-o output-file-name

   -h help
   -l list ALSA MIDI ports

---------------------------------------------

# DESCRIPTION

Utility generates MIDI data to automatically control style playback at YAMAHA digital audio workstation keyboard.

It has been tested with model PSR-SX600 however it should probably work also with the other models from PSR lineup.

Output is saved as standard MIDI file (SMF) and can be played live directly to selected ALSA sequencer port as well.

---------------------------------------------

# INPUT SYNTAX

Input data is a text file with simple intuitive syntax. Each line is a single command. Items are separated by spaces.
Only the first two items are used. Anything else to the end of the line is ignored and can be seen as a comment.

   Examples :

       4 T86               -> wait 4 beats and set tempo to 86 BPM
       2 MAIN_A            -> wait 2 beats and set style section to MAIN_A
       0 AM7               -> immediately after previous message change the chord to AM7
       8 BREAK             -> wait 8 beats and change style section to BREAK
       4 MAIN_B SECTION3   -> SECTION3 is a comment
       8 STOP              -> end of the playback

---------------------------------------------

# BUILD

gcc $(pkg-config --cflags glib-2.0) -lasound -lsmf -o midistyle midistyle.c

---------------------------------------------

# AUTHOR

Miroslav Kovac (mixxoo@gmail.com)
