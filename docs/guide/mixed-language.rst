..
   SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
   SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

**********************
Mixed-Language Designs
**********************

Verilator compiles VHDL and Verilog into one model. A Verilog module may
instantiate a VHDL entity, a VHDL architecture may instantiate a Verilog
module, and either language may be the top. There is no co-simulation
interface between the two halves and no separate elaboration step: list
every source on one command line and Verilator produces a single C++ model
of the whole design.

.. code-block:: bash

     verilator --binary --timing tb.sv counter.vhd --top tb


Verilog Instantiating VHDL
==========================

A VHDL entity is instantiated exactly like a Verilog module. Its generics
are the instance's parameters and its ports are the instance's ports.

.. code-block:: vhdl

     -- counter.vhd
     library ieee;
     use ieee.std_logic_1164.all;
     use ieee.numeric_std.all;

     entity counter is
       generic (WIDTH : integer := 8);
       port (clk : in  std_logic;
             rst : in  std_logic;
             q   : out std_logic_vector(7 downto 0));
     end entity;

     architecture rtl of counter is
       signal count : unsigned(7 downto 0);
     begin
       process (clk, rst) is
       begin
         if rst = '1' then
           count <= (others => '0');
         elsif rising_edge(clk) then
           count <= count + 1;
         end if;
       end process;
       q <= std_logic_vector(count);
     end architecture;

.. code-block:: sv

     // tb.sv
     module tb;
       logic clk = 0;
       logic rst = 1;
       logic [7:0] q;

       counter #(.WIDTH(8)) u_cnt (.clk(clk), .rst(rst), .q(q));

       always #5 clk = ~clk;
       initial begin
          #20 rst = 0;
          #200 $finish;
       end
     endmodule

The instance names the entity, not the architecture. Where an entity has
more than one architecture, Verilator binds the one a configuration selects,
or failing that the last one analyzed, which is the same default binding
rule VHDL itself uses.


VHDL Instantiating Verilog
==========================

A Verilog module is declared in VHDL as a component, then instantiated. The
component's generics correspond to the module's parameters and its ports to
the module's ports.

.. code-block:: vhdl

     architecture rtl of top is
       component adder is
         generic (WIDTH : integer);
         port (a : in  std_logic_vector(7 downto 0);
               b : in  std_logic_vector(7 downto 0);
               s : out std_logic_vector(7 downto 0));
       end component;
     begin
       u_add : component adder
         generic map (WIDTH => 8)
         port map (a => x, b => y, s => z);
     end architecture;

The component declaration is what makes the VHDL analyzable on its own: it
tells the analyzer the shape of a unit it cannot see. Verilator checks it
against the Verilog module when the two are bound, and reports a mismatch in
port count, direction or width rather than binding silently.


Names and Case
==============

VHDL is not case sensitive and Verilog is. Across the boundary Verilator
applies the rule each side expects: a name written in VHDL matches a Verilog
name regardless of case, and a name written in Verilog must match a VHDL
name up to case. So all of these bind to the same VHDL entity ``counter``
and its port ``q``:

.. code-block:: sv

     counter u1 (.q(x));
     COUNTER u2 (.Q(x));
     Counter u3 (.q(x));

Inside the generated model, the C++ identifier and the trace name for a VHDL
object are its lowercased spelling. A VHDL extended identifier — written
between backslashes, as in ``\My Signal\`` — keeps its case and is a
distinct name; the usual signal-name escaping described under
:ref:`language limitations` then applies to any character C++ cannot take.


Port Types
==========

A port is mapped by its shape, not its name, so a user-defined type built
the same way as a standard one maps the same way.

.. list-table::
   :header-rows: 1

   * - VHDL
     - Verilog
   * - ``std_logic``, ``std_ulogic``, ``bit``
     - ``logic``
   * - ``boolean``
     - ``logic``, 1 bit
   * - ``std_logic_vector(N-1 downto 0)``
     - ``logic [N-1:0]``
   * - ``std_logic_vector(0 to N-1)``
     - ``logic [0:N-1]``
   * - ``unsigned``
     - ``logic [N-1:0]``
   * - ``signed``
     - ``logic signed [N-1:0]``
   * - ``integer``, ``natural``, ``positive``
     - ``int``
   * - ``real``
     - ``real``
   * - enumeration type
     - ``logic`` of the minimum width holding all its literals
   * - record
     - packed ``struct``
   * - constrained array of a composite
     - unpacked array

VHDL's ``in``, ``out`` and ``inout`` modes become Verilog's ``input``,
``output`` and ``inout``. A ``buffer`` port is treated as ``out``. Mode
``linkage`` is not supported.

An ascending range needs no translation: ``std_logic_vector(0 to 7)`` maps
to ``logic [0:7]``, which Verilog can also express, and the bit order is
preserved. Verilator does not silently reverse a vector to make two sides
agree; a direction mismatch is reported.


Generics and Parameters
=======================

A VHDL generic of an integer, real, boolean, string or enumeration type maps
to a Verilog parameter, and is overridden from Verilog with the ordinary
``#(...)`` syntax. Overriding from VHDL uses ``generic map``. Either way the
override is applied by the same elaboration pass that specializes Verilog
parameters, so two instances with the same generic values share one
generated module.

Generic types, generic packages and generic subprograms have no Verilog
counterpart and cannot cross the boundary; they work normally within VHDL.


Sharing One C Model
===================

A foreign C function is the same construct in the generated code whichever
language declared it, so one unmodified C model serves both halves of a
mixed design:

.. code-block:: vhdl

     function c_popcount (v : integer) return integer;
     attribute foreign of c_popcount : function is "VHPIDIRECT c_popcount";

.. code-block::

     import "DPI-C" function int c_popcount(input int v);

Both resolve to the C symbol ``c_popcount`` linked into the model.


Limitations
===========

- A VHDL construct with no Verilog equivalent cannot appear on a port:
  access types, file types, protected types, and unconstrained array ports
  whose length is not fixed by the instantiation.
- A Verilog interface or modport cannot be a VHDL component port.
- ``defparam`` does not reach into a VHDL instance; use ``generic map`` or
  the instance's parameter list.
- Configuration specifications and configuration declarations select among
  VHDL architectures only. Binding a Verilog module is done by the component
  declaration, not by a configuration.
