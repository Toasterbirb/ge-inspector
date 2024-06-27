# ge-inspector

ge-inspector is a tool for finding and filtering tradeable RuneScape 3 items based on different factors. The information is mostly bulk downloaded from the RuneScape wiki and the rest (f2p/p2p status) is pulled from the RuneScape 3 Grand Exchange APIs

Example usage:
```
$ ge-inspector --budget 1000000 --min-volume 1000 --min-price 250 --min-limit 1000 --f2p --sort volume --invert
Name                                      Price       Volume    Total cost   Limit     High alch   Members
Banana                                    301         224888    301000       1000      1           no
Raw rat meat                              989         124176    989000       1000      23          no
Egg                                       466         31613     466000       1000      2           no
Bucket of milk                            879         27907     879000       1000      7           no
Marigold seed                             438         11597     438000       1000      50          no
Cooking apple                             467         8291      467000       1000      21          no
Cooked meat                               328         7983      328000       1000      18          no
Pie shell                                 331         5453      331000       1000      2           no
Energy potion (3)                         541         3630      541000       1000      66          no
Strength potion (3)                       567         3314      567000       1000      720         no
Attack potion (3)                         779         3191      779000       1000      336         no
Mithril throwing axe                      258         2727      258000       1000      9           no
Rune throwing axe                         587         2450      587000       1000      24          no
```

For more usage information, check `ge-inspector --help`

## Building
Build the project with cmake by running the following commands
```sh
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Installation
To install ge-inspector to /usr/local/bin, run the following command
```sh
make install
```
You can customize the installation *PREFIX* and *DESTDIR* variables normally with cmake and make.
