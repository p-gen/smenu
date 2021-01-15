.. image:: smenu.gif

|

.. image:: simple_menu.gif

What is it?
===========
**smenu** is a selection filter just like ``sed`` is an editing filter.

This tool takes words from standard input or from a file and presents
them on the screen in different layouts in a scrolling window.
A cursor that you can easily move lets you select one or more of them.

Note that the screen is not previously cleared when the scrolling window
of **smenu** is displayed.

I tried to make its use as simple as possible. It supports the ``UTF-8``
encoding and should work on all terminals managed by the ``terminfo``
database.

Please use the included man page to learn more about this little program.

The `wiki <https://github.com/p-gen/smenu/wiki>`_ contains screenshots and
animations that detail some concepts and features of **smenu**.

How to build it?
================
**smenu** can be built on any system on which a working ``terminfo``
development platform is available. This includes every Unix and
Unix-like system I am aware of.

Please use the provided ``build.sh`` script to build the executable.
This script accepts the same arguments as the GNU ``configure`` script,
type ``build.sh --help`` to see them.

The script ``autogen.sh`` is also provided if you need to generate a
new ``configure`` script from ``configure.ac`` and ``Makefile.am``. The
GNU **autotools** will need to be installed for this script to work.

How to install it?
==================
Once the build process has finished, a simple ``make install`` with the
appropriate privileges will do it.

Issue vs Discussion.
====================
I have enabled `discussions <https://github.com/p-gen/smenu/discussions>`_
on this repository.

I am aware there may be some confusion when deciding where you should
communicate when reporting issues, asking questions or raising feature
requests so this section aims to help us align on that.

Please `raise an issue <https://github.com/p-gen/smenu/issues>`_ if:

- You have found a bug.
- You have a feature request and can clearly describe your request.

Please `open a discussion <https://github.com/p-gen/smenu/discussions>`_ if:

- You have a question.
- You're not sure how to achieve something with smenu.
- You have an idea but don't quite know how you would like it to work.
- You have achieved something cool with smenu and want to show it off.
- Anything else!

Some examples.
==============

Linux example.
--------------
This program should work on most Unix but if you are using Linux,
try to type the following line at a shell prompt (here: ``"$ "`` ):

::

  $ R=$(grep Vm /proc/$$/status \
        | smenu -n20 -W $':\t\n' -q -c -b -g -s /VmH)
  $ echo $R

Something like this should now be displayed with the program waiting
for commands: (numbers are mine, yours will be different)

::

  VmPeak¦    23840 kB
  VmSize¦    23836 kB
  VmLck ¦        0 kB
  VmHWM ¦     2936 kB
  VmRSS ¦     2936 kB
  VmData¦     1316 kB
  VmStk ¦      136 kB
  VmExe ¦       28 kB
  VmLib ¦     3956 kB
  VmPTE ¦       64 kB
  VmSwap¦        0 kB

A cursor should be under ``"VmHWM "``.

After having moved the cursor to ``"      136 kB"`` and ended the program
with ``<Enter>``, the shell variable R should contain: ``"      136 kB"``.

Unix example.
-------------
The following command, which is Unix brand agnostic, should give you a
scrolling window if you have more than 10 accounts on your Unix with a
UID lower than 100:

::

  $ R=$(awk -F: '$3 < 100 {print $1,$3,$4,$NF}' /etc/passwd \
        | smenu -n10 -c)
  $ echo $R

On mine (``LANG`` and ``LC_ALL`` set to ``POSIX``) it displays:

::

  at      25 25  /bin/bash      \
  sys     0  3   /usr/bin/ksh   +
  bin     1  1   /bin/bash      |
  daemon  2  2   /bin/bash      |
  ftp     40 49  /bin/bash      |
  games   12 100 /bin/bash      |
  lp      4  7   /bin/bash      |
  mail    8  12  /bin/false     |
  named   44 44  /bin/false     |
  ntp     74 108 /bin/false     v

Note the presence of a scrollbar.

Warning for post v0.9.15 versions.
----------------------------------
These versions use a new options system called **ctxopt** which
may contain bugs.
Please report them so they can be fixed in the next release of **smenu**
or **ctxopt** (https://github.com/p-gen/ctxopt).

Command line arguments may also need to be rearranged in some cases
because of this new option management system.
Sorry for the extra work this might entail.

Testing and reporting.
----------------------
The included testing system is relatively young, please be indulgent.

**IMPORTANT** the testing system has some dependencies, please read the
``test/README.rst`` before going further.

**NOTE** running all the tests by running ``./tests.sh`` in the
``tests`` directory will take some time (around 21 min for now).

**NOTE** on some systems like \*BSD some tests may fail. This can be
explained by differences in posix/libc/... implementations.  This can
notably occur when some specific regular expressions or uncommon ``UTF-8``
byte sequences are used.

If a test fails for unknown reason, then please send me its directory
name and the relevant ``.bad`` file.

If you are hit by a bug that no test covers, then you can create a new
test in the ``tests`` directory in an existing or new directory: read the
``tests/README.rst`` file, use an existing test as model, create an
``.in`` file and a ``.tst`` file and send them to me as well as the
produced files.

Contributions.
--------------
Contributions are welcome but discuss your proposal in an issue first,
or with the maintainer.

Special thanks.
---------------
I want to thank those who took the time to package **smenu** for their
preferred operating system or distribution.
You will find their names here: https://repology.org/project/smenu/packages
