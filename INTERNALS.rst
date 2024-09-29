Some internal notes.
====================

Words.
------
To illustrate how words are stored in memory, we will suppose the
following input text::

    cute cup at
      as   he
    at us
        i

The main data structures are:

- An array of words indexed by their order of appearance.

  This is not purely a string array, words are structures which contain the
  following informations:

  - the display string which can be shorter than the original string;
  - the original string;
  - various flags to facilitate search, display and accessibility;
  - positional informations: column start, column end end an number of
    glyphs between the two.

  The input text shown above will be put in the word array as::

                                                last=1
                        last=1    last=1    last=1  |
                           |         |         |    |
            +------+-----+----+----+----+----+----+---+
    input:  | cute | cup | at | as | he | at | us | i |
            +------+-----+----+----+----+----+----+---+
    pos:      0      1     2    3    4    5    6    7

- A Ternary Search Tree (TST) which store all these positions in the
  array more than once, if their appear multiple times.

  Here is a simple representation of the TST containing these words::

          c
        / | \
       a  u  h
       |  |  | \
       t  t  e  u
     /  / |   / |
    s  p  e  i  s

  In the TST tree, a list containing the position of words in the array
  is attached to each terminal node of the words::

          +---+            +---+
    cute: | 0 |        he: | 4 |
          +---+            +---+
          +---+            +---+
    cup:  | 1 |        us: | 6 |
          +---+            +---+
          +---+---+        +---+
    at:   | 2 | 5 |    i:  | 7 |
          +---+---+        +---+
          +---+
    as:   | 3 |
          +---+

  This TST is used when searching a word using the ``/`` command or when using
  the ``-s`` command line option.

.. raw:: pdf

   PageBreak

Window.
-------
The following diagram illustrates various variables used in the code to
display the window::

       first_word_in_line_a array
                   |
                   v
                 +---+------stdin stream---------------+
                 | 0 |                                 |
                 |   |                                 |
                 |   |                                 |
                 |   |                                 |
                 |   |                                 |
                 |   |                                 |
                 |   | win.first_column                |
                 |   |    |                            |
                 |   +----V-win---------+--------------+
   win.start -------->............... ^ ...............| 1
    win.offset   |   |............... | ...............| 2 <-- win.cur_line
   <------------>|   |............... | win.max_lines .| 3
                 |   |............... | ...............| .
    current ------------> cursor .... | ...............| .
                 |   |............... | ...............| .
                 |   |............... v ...............<------ win.end
                 |   +------------------+--------------+
                 |   |                                 |
                 |   |                                 |
                 |   |                                 |
                 |   |                          +------+
    last_line ------->                          |
                 +---+-------------------------^+
                                               |
                                               |
                                            count-1

