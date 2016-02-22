Words are read into two datastructures:

To illustrate these datastructures, we will suppose the following input text::

    a b c d  e    f g h
      a b a    d
    f g h

    z

The main  datastructures are:

- An array indexed by their order of appearance.

  This is not purely a string array, words are structures which contain the
  following informations:

  - the display string which can be shorter than the original string;
  - the original string;
  - a flag indicating the this is the last word in a paragraph like
    **h**, **d**, **h** and **z** if our example test;
  - positional infomations: column start, column end end an number of
    multibyes between the two.

  The following text will be put in the word array::


                                last=1          last=1    last=1 last=1
                                  |               |           |   |
    +---+---+---+---+---+---+---+-v-+---+---+---+-v-+---+---+-v-+-v-+
    | a | b | c | d | e | f | g | h | a | b | a | d | f | g | h | z |
    +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      0   1   . . .                                       13  14  15

- A Ternary Search Treee (TST) which store their position in the array
  and all their furure positions if their appear multiple times.

  In the TST tree, index lists are attached to the input words::

      +---+---+----+                 
    a | 0 | 8 | 10 |                 
      +---+---+----+

      +---+---+                 
    b | 1 | 9 |                 
      +---+---+

      +---+                
    c | 2 |                 
      +---+

  and so on.

  This TST is used when searching a word using the ``/`` command or when using
  the ``-s`` commandline option.

.. raw:: pdf

   PageBreak

The following diagram illustrates various variables used in the code.::

       first_word_in_line_a array
                   |
                   v
                 +---+------stdin stream-------------------+
                 | 0 |                                     |
                 |   |                                     |
                 |   |                                     |
                 |   |                                     |
                 |   |                                     |
                 |   |                                     |
                 |   | first_column                        |
                 |   |    |                                |
                 |   +----V-win---------+------------------+
   win.start -------->................. ^ .................| 1
    win.offset   |   |................. | .................| 2 <-- win.cur_line
   <------------>|   |................. | win.max_lines ...| 3
                 |   |................. | .................| .
    current ------------> cursor ...... | .................| .
                 |   |................. | .................| .
                 |   |................. v .................<------ win.end
                 |   +------------------+------------------+
                 |   |                                     |
                 |   |                                     |
                 |   |                                     |
                 |   |                          +----------+
    st_line --------->                          |
                 +---+-------------------------^+
                                               |
                                               |
                                            count-1

The followin diagram shows how the lines and columns intervals introduced
by ``-C`` and ``-R`` options are stored in linked lists.::

    +------+           +------+
    | head |           | tail |
    +---+--+           +--+---+
        |                 |
        |                 |
      +-v-+    +---+    +-v-+    +---+
      |   +---->   +---->   +---->xxx|
      +-+-+    +-+-+    +-+-+    +---+
        |        |        |
        |        |        |
     +--v---+ +--v---+ +--v---+
     | low  | | low  | | low  |
     +------+ +------+ +------+  Intervals
     | high | | high | | high |
     +------+ +------+ +------+
