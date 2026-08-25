..
   SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
   SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

***************
Input Languages
***************

This section describes the languages Verilator takes as input. See also
:ref:`Verilator Control Files`.


Language Standard Support
=========================

Verilog 2001 (IEEE 1364-2001) Support
-------------------------------------

Verilator supports most Verilog 2001 language features. This includes
signed numbers, "always @\*", generate statements, multidimensional arrays,
localparam, and C-style declarations inside port lists.


Verilog 2005 (IEEE 1364-2005) Support
-------------------------------------

Verilator supports most Verilog 2005 language features. This includes the
\`begin_keywords and \`end_keywords compiler directives, $clog2, and the
uwire keyword.


SystemVerilog 2005 (IEEE 1800-2005) Support
-------------------------------------------

Verilator supports ==? and !=? operators, ++ and -- in some contexts,
$bits, $countbits, $countones, $error, $fatal, $info, $isunknown, $onehot,
$onehot0, $unit, $warning, always_comb, always_ff, always_latch, bit, byte,
chandle, const, do-while, enum, export, final, import, int, interface,
logic, longint, modport, package, program, shortint, struct, time, typedef,
union, var, void, priority case/if, and unique case/if.


SystemVerilog 2012 (IEEE 1800-2012) Support
-------------------------------------------

Verilator implements a full SystemVerilog-compliant preprocessor, including
function call-like preprocessor defines, default define arguments,
\`__FILE__, \`__LINE__ and \`undefineall.


SystemVerilog 2017 (IEEE 1800-2017) Support
-------------------------------------------

Verilator supports the 2017 "for" loop constructs and several cleanups IEEE
made in 1800-2017.


SystemVerilog 2023 (IEEE 1800-2023) Support
-------------------------------------------

Verilator supports some of the 2023 improvements, including triple-quoted
string blocks that may include newlines and single quotes.

Verilator implements a full IEEE 1800-2023 compliant preprocessor,
including triple-quoted strings, and \`ifdef expressions.


VHDL 2008 (IEEE 1076-2008) Support
----------------------------------

Verilator reads VHDL as well as Verilog. Files with a :file:`.vhd` or
:file:`.vhdl` extension are taken as VHDL, and :vlopt:`--vhdl-std` selects
the revision that applies to them; the default is VHDL-2008.

.. code-block:: bash

     verilator --binary --timing counter.vhd --top counter

Nothing else is needed to use the standard libraries. ``library std`` is
always available, and a context or use clause naming a package of ``library
ieee``, such as ``ieee.numeric_std``, causes Verilator to read the IEEE
package it ships and whatever that package depends on. Pass
:vlopt:`--no-std-vhdl` to supply your own copies instead.

Supported are entities and architectures, packages and package bodies,
context declarations, configurations, all the standard type classes
(enumeration, integer, floating point, physical, array, record, access,
file and protected), shared variables, subprograms with all four parameter
classes, resolution functions and multiple drivers, all the concurrent and
sequential statements, blocks, ``for``/``if``/``case`` generate statements,
component and direct entity instantiation, aliases, attributes, and the
predefined libraries ``std.standard``, ``std.textio``, ``std.env``, and IEEE
``std_logic_1164``, ``numeric_std``, ``numeric_bit``,
``numeric_std_unsigned``, ``math_real`` and ``math_complex``.

A VHDL design that suspends -- a process with no sensitivity list, ``wait``
in any of its forms, or an ``after`` delay -- needs :vlopt:`--timing`, the
same as the equivalent Verilog does. Synthesizable VHDL, where every process
has a sensitivity list, does not.

VHDL is not case sensitive, so a signal declared ``Count`` is the same
signal as ``COUNT``. Verilator lowercases basic identifiers, and that is the
spelling that appears in traces, in the generated C++, and to VPI. Extended
identifiers, written between backslashes, keep their case and are distinct
from every basic identifier.

See :ref:`VHDL Limitations` for where the compiled model diverges from
the LRM, and :doc:`mixed-language` for instantiating VHDL from Verilog and
Verilog from VHDL.


Verilog AMS Support
-------------------

Verilator implements a very small subset of Verilog AMS (Verilog Analog and
Mixed-Signal Extensions) with the subset corresponding to those VMS
keywords with near-equivalents in Verilog IEEE 1364 or SystemVerilog IEEE
1800.

AMS parsing is enabled with :vlopt:`--language VAMS <--language>` or
:vlopt:`--language 1800+VAMS <--language>`.

Verilator implements ceil, exp, floor, ln, log, pow, sqrt, string, and
wreal.


Synthesis Directive Assertion Support
-------------------------------------

Verilator reads any ``//synopsys full_case`` or ``//synopsys
parallel_case`` directives. The same applies to any ``//ambit synthesis``,
``//cadence`` or ``//pragma`` directives of the same form.

When these synthesis directives are discovered, unless
:vlopt:`--no-assert-case` option is used, Verilator will either formally
prove the directive to be true, or, failing that, will insert the
appropriate code to detect failing cases at simulation runtime and print an
"Assertion failed" error message.

Verilator likewise also asserts any "unique" or "priority" SystemVerilog
keywords on case statements, as well as "unique" on if statements. However,
"priority if" is currently ignored.


Time
====

With :vlopt:`--timing`, all timing controls are supported:

- delay statements,
- event control statements not only at the top of a process,
- intra-assignment timing controls,
- net delays,
- ``wait`` statements,

as well as all flavors of ``fork``.

Compiling a Verilated design that uses these features requires a compiler
with C++20 coroutine support, e.g. Clang 5, GCC 10, or newer.

Rising/falling/turn-off delays are currently unsupported and cause the
:option:`RISEFALLDLY` warning.

Minimum/typical/maximum delays are currently unsupported. The typical delay
is always the one chosen. Such expressions cause the :option:`MINTYPMAXDLY`
warning.

Another consequence of using :vlopt:`--timing` is that the :vlopt:`--main`
option generates a main file with a proper timing eval loop, eliminating
the need for writing any driving C++ code. You can simply compile the
simulation (perhaps using :vlopt:`--build`) and run it.

With :vlopt:`--no-timing`, all timing controls cause the :option:`NOTIMING`
error, except:

- delay statements - they are ignored (as they are in synthesis), though
  they do issue a :option:`STMTDLY` warning,
- intra-assignment timing controls - they are ignored, though they do issue
  an :option:`ASSIGNDLY` warning,
- net delays - they are ignored,
- event controls at the top of the procedure,

Forks cause this error as well, except:

- forks with no statements,
- ``fork..join`` or ``fork..join_any`` with one statement,
- forks with :vlopt:`--bbox-unsup`.

If neither :vlopt:`--timing` nor :vlopt:`--no-timing` is specified, all
timing controls cause the :option:`NEEDTIMINGOPT` error, except event
controls at the top of the process. Forks cause this error as well, except:

- forks with no statements,
- ``fork..join`` or ``fork..join_any`` with one statement,
- forks with :vlopt:`--bbox-unsup`.

Timing controls and forks can also be ignored in specific files or parts of
files. The :option:`/*verilator&32;timing_off*/` and
:option:`/*verilator&32;timing_off*/` metacomments will make Verilator
ignore the encompassed timing controls and forks, regardless of the chosen
:vlopt:`--timing` or :vlopt:`--no-timing` option. This can also be achieved
using the :option:`timing_off` and :option:`timing_off` options in
Verilator Control Files.


.. _language limitations:

Language Limitations
====================

This section describes the language limitations of Verilator. Many of these
restrictions are by intent.

Synthesis Subset
----------------

Verilator supports the Synthesis subset with other verification constructs
being added over time. Verilator also simulates events as Synopsys's Design
Compiler would, namely given a block of the form:

.. code-block:: sv

   always @(x) y = x & z;

This will recompute y when there is a potential for change in x or a change
in z; that is when the flops computing x or z evaluate (which is what
Design Compiler will synthesize.) A compliant simulator will only calculate
y if x changes. We recommend using always_comb to make the code run the
same everywhere. Also avoid putting $displays in combo blocks, as they may
print multiple times when not desired, even on compliant simulators as
event ordering is not specified.


Signal Naming
-------------

To avoid conflicts with C symbol naming, any character in a signal name
that is not alphanumeric nor a single underscore will be replaced by
`__0hh` where `hh` is the hex code of the character. To avoid conflicts
with Verilator's internal symbols, any double underscore is replaced with
`___05F` (5F is the hex code of an underscore.)


Bind
----

Verilator only supports bind to a target module name, not to an instance
path.


Class
-----

Verilator class support is limited but in active development. Verilator
supports members, methods, class extend, and class parameters.


Dotted cross-hierarchy references
---------------------------------

Verilator supports dotted references to variables, functions, and tasks in
different modules. The portion before the dot must have a constant value;
for example a[2].b is acceptable, while a[x].b is generally not.

References into generated and arrayed instances use the instance names
specified in the Verilog standard; arrayed instances are named
``{instanceName}[{instanceNumber}]`` in Verilog, which becomes
``{instanceName}__BRA__{instanceNumber}__KET__`` inside the generated C++
code.


Latches
-------

Verilator is optimized for edge-sensitive (flop-based) designs. It will
attempt to do the correct thing for latches, but most performance
optimizations will be disabled around the latch.


Structures and Unions
---------------------

All structures and unions are scheduled together, which means that
generating one member of a structure from blocking, and another from
non-blocking assignments is unsupported.


.. _unknown states:

Unknown States
--------------

Verilator is mostly a two-state simulator, not a four-state simulator.
However, it has two features that uncover most initialization bugs
(including many that a four-state simulator will miss.)

Identity comparisons (=== or !==) are converted to standard ==/!= when
neither side is a constant. This may make the expression yield a different
result than a four-state simulator. An === comparison to X will always be
false, so that Verilog code which checks for uninitialized logic will not
fire.

Assigning X to a variable will assign a constant value as determined by the
:vlopt:`--x-assign` option. This allows runtime randomization; thus, if the
value is used, the random value should cause downstream errors. Integers
also get randomized, even though the Verilog 2001 specification says they
initialize to zero. However, randomization happens at initialization time;
hence, during a single simulation run, the same constant (but random) value
will be used every time the assignment is executed.

All variables, depending on :vlopt:`--x-initial` setting, are typically
randomly initialized using a function. You can determine that reset is
working correctly by running several random simulation runs. On the first
run, have the function initialize variables to zero. On the second, have it
initialize variables to one. On the third and following runs, have it
initialize them randomly. If the results match, reset works. (Note that
this is what the hardware will do.) In practice, setting all variables to
one at startup finds the most problems (since control signals are typically
active-high).

:vlopt:`--x-assign` applies to variables explicitly initialized or assigned
an X. Uninitialized clocks are initialized to zero, while all other state
holding variables are initialized to a random value. Event-driven
simulators will generally trigger an edge on a transition from X to 1
(posedge) or X to 0 (negedge). However, by default, since clocks are
initialized to zero, Verilator will not trigger an initial negedge. Some
code (particularly for reset) may rely on X->0 triggering an edge. The
:vlopt:`--x-initial-edge` option enables this behavior. Comparing runs with
and without this option will find such problems.


Tri/Inout
---------

Verilator converts some simple tristate structures into two state. Pullup,
pulldown, bufif0, bufif1, notif0, notif1, pmos, nmos, tri0 and tri1 are
also supported. Simple comparisons with ``=== 1'bz`` are also supported.

An assignment of the form:

.. code-block:: sv

   inout driver;
   wire driver = (enable) ? output_value : 1'bz;

Will be converted to:

.. code-block:: sv

   input driver;        // Value being driven in from "external" drivers
   output driver__en;   // True if driven from this module
   output driver__out;  // Value being driven from this module

External logic will be needed to combine these signals with any external
drivers.

Tristate drivers are not supported inside functions and tasks; an inout
there will be considered a two-state variable that is read and written
instead of a four-state variable.


Gate Primitives
---------------

The 2-state gate primitives (and, buf, nand, nor, not, or, xnor, xor) are
directly converted to behavioral equivalents. The 3-state and MOS gate
primitives are not supported. User-defined primitive (UDP) tables are
supported.


Specify blocks
--------------

All specify blocks and timing checks are ignored. All min:typ:max delays
use the typical value.


Array Initialization
--------------------

When initializing a large array, you need to use non-delayed assignments.
Verilator will tell you when this needs to be fixed; see the BLKLOOPINIT
error for more information.


Array Out of Bounds
-------------------

Writing a memory element outside the bounds specified for the array may
cause a different memory element inside the array to be written instead.
For power-of-2 sized arrays, Verilator will give a width warning and the
address. For non-power-of-2-sizes arrays, index 0 will be written.

Reading a memory element outside the bounds specified for the array will
give a width warning and wrap around the power-of-2 size. For
non-power-of-2 sizes, it will return an unspecified constant of the
appropriate width.


Assertions
----------

Verilator partially supports assertions and assertion-driven functional
coverage.

Coverage
--------

Verilator partially supports SystemVerilog functional coverage with
``covergroup``, ``coverpoint``, bins, cross coverage, and transition bins.
See :ref:`Covergroup Coverage`.


.. _VHDL Limitations:

VHDL Limitations
----------------

Verilator compiles VHDL to a statically scheduled two-state model, the same
one it compiles Verilog to. That is where the speed comes from, and it has
consequences the LRM does not allow for. They are listed here so that a
design that depends on one of them can be recognized as such.

**Metavalues do not exist at run time.** The nine values of ``std_ulogic``
are mapped when the model is built: ``'1'`` and ``'H'`` become 1, ``'0'``
and ``'L'`` become 0, ``'Z'`` becomes Z, and ``'U'``, ``'W'`` and ``'-'``
become X, which Verilator then eliminates exactly as it eliminates Verilog's
X. So a comparison such as ``if s = 'U'`` is never true at run time, and an
uninitialized signal reads as 0 rather than reporting ``'U'``. Use
:vlopt:`--x-initial` ``unique`` for randomized initial state, which is the
closest analogue of VHDL's ``'U'`` default. Metavalues in *static*
expressions are exact, so a constant or generic comparison against ``'U'``
folds correctly at compile time.

**Inertial delay is not modelled.** An ``after`` clause lowers to a
Verilog-style non-blocking delay, which is transport-like. VHDL's default
inertial pulse rejection, and an explicit ``reject`` clause, do not remove
short pulses.

**A waveform does not replace what is already scheduled.** ``s <= a after 10
ns, b after 20 ns`` schedules both, which is right; but VHDL says a later
assignment to the same signal deletes any transaction already projected for a
later time, and that deletion is not modelled. A design that overwrites its
own pending transactions sees both.

**The delta cycle count is not observable.** Verilator settles combinational
logic in a converge loop rather than counting deltas. Ordering within a
simulation time step follows VHDL's rule that a signal assignment takes
effect at the end of the step, but no construct can observe how many delta
cycles a step took.

**Unconstrained values are sometimes slower.** Where the length of an
unconstrained array is fixed at the call site -- which is every call into
``ieee.numeric_std`` in real code -- the subprogram is specialized to that
length and runs at the speed of hand-written Verilog. Where the length is
not known until the value is computed, as for ``integer'image``, the value
is carried dynamically instead. Both are correct; only the second allocates.

**An external name reaches instances, not blocks.** ``<< signal
uut.inner : integer >>`` reads and writes the object it names, by a
pathname relative to where it is written or absolute from the root. A
pathname that starts at ``^``, one that names a *block* label -- a block is
elaborated where it stands, so it is not a level of hierarchy here -- and
the ``@`` form that names an object in a package are not supported. After a
``release`` the signal keeps the value it was forced to until something
drives it again.

**A path name stops at the design unit.** ``'INSTANCE_NAME`` and
``'PATH_NAME`` give every region from the design unit down --
``:work:pkg:f[ return integer]:`` for a package subprogram,
``:top(tb):lbl:b`` for a signal in a block -- but not the part of the path
that comes from outside the unit. A signal in an instantiated entity reads
``:sub(rtl):s`` where the LRM asks for ``:top(tb):u1@sub(rtl):s``, since one
compiled entity serves every instance of it.

**A range of length one loses its direction across an unconstrained port.**
A port or generic written without a constraint takes the subtype of the
actual, and the two bounds are carried to the instance separately, so which
way the range runs is read back from them. Where the two bounds are equal
the range is reported as ascending. Only ``'ascending`` can tell the
difference; every other attribute, and indexing, are the same either way.

**Arithmetic on a width not known until run time.** Verilator emits
fixed-width operations, so arithmetic whose operand width is only known
during execution is not available. This is reachable only by doing
arithmetic on a value whose length came from a run-time computation.

**A guarded signal is not disconnected.** A guarded assignment drives while
its block's ``GUARD`` is true and holds its last value while it is false,
which is what a signal with one driver does. For a signal declared with the
``bus`` or ``register`` kind and driven from more than one place, VHDL turns
the driver off entirely and lets the resolution function see one fewer
value; that removal is not modelled, so such a signal keeps contributing its
last value. ``disconnect`` specifications are accepted and ignored for the
same reason.

**An attribute specification must name what it applies to.** ``attribute a
of x : signal is ...`` gives ``x'a`` its value, but the ``all`` and
``others`` forms, which give every entity of a class in the declarative part
the same value, are not applied.

**The image of a real is printed in C form.** ``real'image(4.0)`` gives
``4`` where the LRM asks for text that reads back as a VHDL literal, such as
``4.0``. This affects only what is printed, not any value.


Encrypted Verilog
-----------------

Open-source simulators like Verilator cannot use encrypted RTL (i.e. IEEE
P1735). Talk to your IP vendor about delivering IP blocks using Verilator's
:vlopt:`--protect-lib` feature.


Language Keyword Limitations
============================

This section describes specific limitations for each language keyword.

.. Hack to get long definition list:

.. |cmdslong1| replace:: \`__FILE__, \`__LINE__, \`begin_keywords,
   \`begin_keywords, \`begin_keywords, \`begin_keywords, \`begin_keywords,
   \`define, \`else, \`elsif, \`end_keywords, \`endif, \`error, \`ifdef,
   \`ifndef, \`include, \`line, \`systemc_ctor, \`systemc_dtor,
   \`systemc_header, \`systemc_header_post, \`systemc_imp_header,
   \`systemc_implementation, \`systemc_interface, \`undef, \`verilog

|cmdslong1|
   Fully supported.

.. Hack to get long definition list:

.. |cmdslong2| replace:: always, always_comb, always_ff, always_latch, and,
   assign, begin, buf, byte, case, casex, casez, default, defparam,
   do-while, else, end, endcase, endfunction, endgenerate, endmodule,
   endspecify, endtask, final, for, function, generate, genvar, if,
   initial, inout, input, int, integer, localparam, logic, longint,
   macromodule, module, nand, negedge, nor, not, or, output, parameter,
   posedge, reg, scalared, shortint, signed, supply0, supply1, task, time,
   tri, typedef, var, vectored, while, wire, xnor, xor

|cmdslong2|
   Generally supported.

++, -- operators
   Increment/decrement can only be used as standalone statements or in
   certain limited cases.

'{} operator
   Assignment patterns with an order based, default, constant integer
   (array) or member identifier (struct/union) keys are supported. Data
   type keys and keys computed from a constant expression are not
   supported.

\`uselib
   Uselib, a vendor-specific library specification method, is ignored along
   with anything following it until the end of that line.

cast operator
   Casting is supported only between simple scalar types, signed and
   unsigned, not arrays nor structs.

chandle
   Treated as a "longint"; does not yet warn about operations specified as
   illegal on chandles.

checker
   Treated as a "module"; does not yet warn about many constructs illegal
   inside a checker.

disable
   Disable statements may be used only if the block being disabled is a
   block the disable statement itself is inside. This was commonly used to
   provide loop break and continue functionality before SystemVerilog added
   the break and continue keywords.

force, release
   Verilator supports the procedural `force` (and corresponding `release`)
   statement. However, the behavior of the `force` statement does not
   entirely comply with IEEE 1800-2023:

   #. Using forced variable as a value to another force statement is
      currently not supported. The dependent force statement is forced by
      an initial constant value.

   #. Force/release with procedural continuous assignment is not supported.
      The assignment is treated as procedural.

   #. Expressions using multiple variable references or function calls on
      forced right-hand side are not sensitive to dependency changes.

inside
   Inside expressions may not include unpacked array traversal or $ as an
   upper bound. Case inside and case matches are also unsupported.

interface
   Interfaces and modports, including generated data types are supported.
   Generate blocks around modports are not supported, nor are virtual
   interfaces nor unnamed interfaces.

shortreal
   Short floating point (shortreal) numbers are converted to real. Most
   other simulators either do not support float, or convert likewise.

specify specparam
   All timing checks and specify blocks (except specparam, which is
   supported) are ignored.

uwire
   Verilator does not perform warning checking on uwires; it treats the
   uwire keyword as if it were the normal wire keyword.

$bits, $countbits, $countones, $finish, $isunknown, $onehot, $onehot0, $signed, $stime, $stop, $time, $unsigned,
   Generally supported.

$dump/$dumpports and related
   $dumpfile or $dumpports will create a VCD or FST file (based on the
   :vlopt:`--trace-vcd` option given when the model was Verilated). This
   will take effect starting at the next eval() call. If you have multiple
   Verilated designs under the same C model, this will dump signals only
   from the design containing the $dumpvars.

   $dumpvars and $dumpports module identifier is ignored; the traced
   instances will always start at the top of the design. The levels
   argument is also ignored; use tracing_on/tracing_off pragmas instead.

   $dumpportson/$dumpportsoff/$dumpportsall/$dumpportslimit filename
   argument is ignored; only a single trace file may be active at once.

   $dumpall/$dumpportsall, $dumpon/$dumpportson, $dumpoff/$dumpportsoff,
   and $dumplimit/$dumpportlimit are currently ignored.

$error, $fatal, $info, $warning.
   Generally supported.

$exit, $finish, $stop
   The rarely used optional parameter to $finish and $stop is ignored;
   $exit is aliased to $finish.

$fopen, $fclose, $fdisplay, $ferror, $feof, $fflush, $fgetc, $fgets, $fscanf, $fwrite, $fscanf, $sscanf
   Generally supported.

$fullskew, $hold, $nochange, $period, $recovery, $recrem, $removal, $setup, $setuphold, $skew, $timeskew, $width
   All specify blocks and timing checks are ignored.

$random, $urandom, $urandom_range
   Use :vlopt:`+verilator+seed+\<value\>` runtime option to set the seed if
   there is no $random nor $urandom optional argument to set the seed.
   There is one random seed per C thread, not per module for $random, nor
   per object for random stability of $urandom/$urandom_range.

$readmemb, $readmemh
   Read memory commands are supported. Verilator and the Verilog
   specification do not include support for readmem to multi-dimensional
   arrays.

$stacktrace
   The `$stacktrace` system call will show the C++ stack, not the Verilog
   call stack, though the function names typically correlate. To get
   symbolic names, the model must have debug symbols, e.g. compile with
   `-CFLAGS -ggdb -LDFLAGS -ggdb -LDFLAGS -rdynamic`.

$test$plusargs, $value$plusargs
   Supported, but the instantiating C++/SystemC wrapper must call

   .. code-block:: C++

      {VerilatedContext*} ->commandArgs(argc, argv);

   to register the command line before calling $test$plusargs or
   $value$plusargs. Or use :vlopt:`--binary` or :vlopt:`--main`.
