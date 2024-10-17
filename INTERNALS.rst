Some internal notes.
====================

Words.
------
To illustrate how words are stored in memory, we will suppose the
following input text::

    cute cup at
      as   he
    at us
        i clue

The main data structures are:

- An array of words indexed by their order of appearance.

  This is not purely a string array, words are structures which contain the
  following information:

  - the display string which can be shorter than the original string;
  - the original string;
  - various flags to facilitate search, display and accessibility;
  - positional information: column start, column end end an number of
    glyphs between the two.

  The input text shown above will be put in the word array as::

                                                     last=1
                        last=1    last=1    last=1      |
                           |         |         |        |
            +------+-----+----+----+----+----+----+---+------+
    input:  | cute | cup | at | as | he | at | us | i | clue |
            +------+-----+----+----+----+----+----+---+------°
    pos:      0      1     2    3    4    5    6    7   8

- A Ternary Search Tree (TST) which store all these positions in the
  array more than once, if their appear multiple times.

  Here is a simple representation of the TST containing these words::

        +---- c
       /      | \
      a   +-- u  h -+
      |  /    |  |   \
      t l     t  e    u
     /  |   / |     / |
    s   u  p  e    i  s
        |
        e

  In the TST tree, a list containing the position of words in the array
  is attached to each terminal node of the words::

          +---+              +---+
    cute: | 0 |          he: | 4 |
          +---+              +---+
          +---+              +---+
    cup:  | 1 |          us: | 6 |
          +---+              +---+
          +---+---+          +---+
    at:   | 2 | 5 |      i:  | 7 |
          +---+---+          +---+
          +---+              +---+
    as:   | 3 |        clue: | 8 |
          +---+              +---+

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

Searching.
----------

Input::

    cute cup at
      as   he
    at us
        i clues

Prefix search.
~~~~~~~~~~~~~~

This method uses the natural properties of TST for prefix searching to
find the position in the array of words.

Example with the prefix "at".

::

        +---- c
       /      | \
      a   +-- u  h -+
      |  /    |  |   \
      t l     t  e    u
     /  |   / |     / |
    s   u  p  e    i  s
        |
        e
        |
        s

    cute cup at as he at us i clues
             --       --

Fuzzy search.
~~~~~~~~~~~~~

Each glyph entered adds a node in a search list.

The first glyph is searched from the root of the TST.

Each first child of this glyph's occurrences is added to an array in
the node.

The next glyph entered will be searched for in the sub-TST arrays present
in the previous node of the search list, and a new node will be added with
an array containing the first children of all its occurrences, if any.

Example when searching for "ue" in the TST ::

        +---- c
       /      | \
      a   +-- u  h -+
      |  /    |  |   \
      t l     t  e    u
     /  |   / |     / |
    s   u  p  e    i  s
        |
        e
        |
        s

The content of the search_list after having fuzzy searched for "ue"::

     u           -> e
     -              -
    [0] [1]  [2] : [1] [2]
     s   e    t  :      s
         |  / |  :
         s p  e  :

    cute cup at as he at us i clues
     - -                        --

Another example, fuzzy searching for "ct".

Here is the content of the search_list::

       c  -> t
       -     -
      [0] : [0]
       u  :  e
       |  :
       t  :
     / |  :
    p  e  :

    cute cup at as he at us i clues
    - -

Another example, fuzzy searching for "cu".

Here is the content of the search_list::

       c  -> u
       -     -
      [0] : [0] [1]
       u  :  e   t
       |  :  |  / |
       t  :  s p  e
     / |  :
    p  e  :

    cute cup at as he at us i clues
    --   --                   - -

Another example, fuzzy searching for "es".

Here is the content of the search_list::

       e          -> s
       -             -
      [0] [1] [2] : [0]
       s          :

    cute cup at as he at us i clues
                                 --

Substring search.
~~~~~~~~~~~~~~~~~

This method also uses the search list described above, but only for the
first glyph. The aim is to store all sub-TSTs starting with the children
of all occurrences of the first glyph in the array in the node of the
search list.

A prefix search of the string without its first glyph is then performed
on each of these sub-TSTs.

Example with "ue"::

     u           -> e
     -              -
    [0] [1]  [2] : [1]
     s   e    t  :  e 
         |  / |  :  |
         s p  e  :  s

    cute cup at as he at us i clues
                                --
