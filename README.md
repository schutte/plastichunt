Plastichunt: Desktop geocache browser
=====================================

**Plastichunt** stores geocache information in an SQLite database and
lets you browse them via a GTK+-based graphical user interface.  I wrote
it mainly to familiarize myself with the GLib/GTK+ family of libraries,
so it’s fairly rudimentary at the moment.  Here is what it can do right
now:

* **Import GPX files.**  Right now, this is the only way to get
  geocaches into the program (except for manipulating the database
  directly), which only makes it useful for users of free platforms like
  [OpenCaching.de](http://www.opencaching.de/) and premium users of
  [Geocaching.com](http://www.geocaching.com/).

* **Display stored geocaches on a map of your choice.**  A small number
  of map providers, including OpenStreetMap, is provided by default.
  You can add others if you wish, as long as the tile URLs are formatted
  in a compatible way.  By selecting a geocache, you get to see all its
  waypoints.

* **Filter the list of geocaches.**  The query language is of course
  entirely undocumented.  For instance, you might want to find
  challenging geocaches in an easily accessible area with `difficulty >=
  4 and terrain < 2`, containers you can get to without a car with
  `+bike or +pubtrans`, or multis and mysteries which you haven’t found
  yet, but could at the moment, with `(type: multi or type: mystery) and
  -found and +available`.  Filters are applied to both the list and the
  map view.

* **View geocache details in tabs.**  Just double-click them in the map
  or list to see the available information.  You can have a look at
  multiple listings at once.  Attributes would be nice to have, but I
  can’t think of a pretty way to show them without having to fiddle
  around with pictures, which I’m not particularly good at.

* **Store personal notes.**  You can save arbitrary text with any
  listing, store your own coordinates for waypoints and mark geocaches
  as found before you really log them online.  This is kind of nice but
  still rather pointless because you cannot export this information yet.

So there you are, a couple of nice features and plenty of caveats.

Installation
------------

You need GTK+ 2, GDK-PixBuf, SQLite 3, libxml 2, libsoup 2.4, librsvg 2
and WebKitGTK+ to run plastichunt and SCons to build it.  Then do:

	$ scons
	# scons install

The first line compiles and links; the second one, run as root, installs
to `/usr/local`.

Author and license
------------------

Plastichunt is written by Michael Schutte <michi@uiae.at>.  You can
download, use, modify and distribute it according to the MIT License as
provided in the `COPYING` file.

<!-- vim: set tw=72: -->
