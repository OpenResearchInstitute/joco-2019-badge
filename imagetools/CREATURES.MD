# Creatures for the capture game

Creature images are in RAW format 128x104 pixels.

The creature2raw script will convert them. That script will accept gif or jpg files as input

Each creature has two files, which are sequentially named with the "index" of the creature, starting with 1.
The files are names with four and only four, zero-filled ascii-numeric digits containing the index:

nnnn.RAW
nnnn.DAT

Where nnnn is the index.
Each creature has a data file which contains the following:
A printable ascii name of ten characters max
A newline (0x0A) to end the name
An ascii numerical value between 0 and 100, which represents the inverse rarity of the creature in percent
A newline to end the rarity

About Rarity:

When it's time to send a creature ID, a random index is chosen from among all creatures. Then a random number from 1 to 100 is chosen.
If the "rarity" value of the creature is greater than the value chosen, the creature is advertised.

Because of this process, if the percentage values are low, it will be VERY rare to select that creature because the odds are:
(1: (number of creatures)) * (1/(creature rarity))

Scoring is automatic and scores are higher for more rare creatures

