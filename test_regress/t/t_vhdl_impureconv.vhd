-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An impure function's result, a protected type's, handed to the conversions
-- and operators of numeric_std and std_logic_1164 is worked out once: spread
-- into lanes, TO_UNSIGNED read its integer once per element, so OSVVM's
-- RandUnsigned ("to_unsigned(RandInt(0, 2**Remain-1), Remain)") drew eleven
-- random numbers for one. And under two states SHIFT_LEFT of such a vector,
-- carried with its bounds, was emitted as a shift of the carrier.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_impureconv_pkg is
  type ctr_t is protected
    impure function nexti return integer;
    impure function nextu return unsigned;
    impure function nexts return signed;
    impure function nextv return std_logic_vector;
    impure function count return integer;
  end protected;
end package;
package body t_vhdl_impureconv_pkg is
  type ctr_t is protected body
    variable n : integer := 0;
    impure function nexti return integer is begin n := n + 1; return n; end function;
    impure function nextu return unsigned is begin n := n + 1; return to_unsigned(n, 8); end function;
    impure function nexts return signed is begin n := n + 1; return to_signed(-n, 8); end function;
    impure function nextv return std_logic_vector is begin n := n + 1; return std_logic_vector(to_unsigned(n, 8)); end function;
    impure function count return integer is begin return n; end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_impureconv_pkg.all;
entity t_vhdl_impureconv is end;
architecture t of t_vhdl_impureconv is
begin
  process
    variable c : ctr_t;
    variable u : unsigned(15 downto 0);
    variable s : signed(15 downto 0);
    variable v : std_logic_vector(7 downto 0);
    variable i : integer := 0;
    variable sum : integer := 0;
    procedure chk(name : string; want : integer) is
    begin
      -- Every result read, so that no simulator drops a store as dead
      sum := sum + to_integer(u) + to_integer(s) + to_integer(unsigned(v)) + i mod 1000;
      report name & " count=" & integer'image(c.count);
      assert c.count = want report name & ": the argument was evaluated more than once"
        severity failure;
    end procedure;
  begin
    u := to_unsigned(c.nexti, 16); chk("to_unsigned", 1);
    s := to_signed(c.nexti, 16); chk("to_signed", 2);
    u := resize(c.nextu, 16); chk("resize u", 3);
    s := resize(c.nexts, 16); chk("resize s", 4);
    i := to_integer(c.nextu); chk("to_integer", 5);
    u(7 downto 0) := shift_left(c.nextu, 1); chk("shift_left", 6);
    v := c.nextv xor x"0F"; chk("xor", 7);
    v := not c.nextv; chk("not", 8);
    u(7 downto 0) := c.nextu + 1; chk("plus", 9);
    s(7 downto 0) := abs c.nexts; chk("abs", 10);
    s(7 downto 0) := - c.nexts; chk("neg", 11);
    u(7 downto 0) := unsigned(c.nextv); chk("conv", 12);
    i := to_integer(unsigned(c.nextv)); chk("to_integer conv", 13);
    v := std_logic_vector(rotate_right(c.nextu, 2)); chk("rotate", 14);
    report "b=" & boolean'image(c.nextu = 17) & " " & boolean'image(c.nextu < 100); chk("cmp", 16);
    v := to_stdlogicvector(to_bitvector(c.nextv)); chk("tobit", 17);
    u := to_unsigned(0, 16);
    u(7 downto 0) := shift_left(c.nextu, 1); chk("shift again", 18);
    assert u = x"0024" report "shift: " & to_hstring(u) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
