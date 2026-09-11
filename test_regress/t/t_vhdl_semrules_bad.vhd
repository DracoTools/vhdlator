-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A block's generic with no default is given one. 'DRIVING is asked of a
-- signal the process may drive. A case choice is locally static. A signal
-- parameter of mode out is not read. A subprogram is given values, not
-- types. A package with generics is used through an instance. An initial
-- value is of its object's type. A variable or signal has all its bounds.
-- Two packages declaring one name make neither visible.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_semrules_gen is
  generic (n : natural);
end package;

package t_vhdl_semrules_p1 is
  type mc is (low, high);
end package;

package t_vhdl_semrules_p2 is
  function mc return boolean;
end package;

package body t_vhdl_semrules_p2 is
  function mc return boolean is begin return true; end function;
end package body;

entity t_vhdl_semrules_sub is
  generic (g : bit_vector(1 to 2) := "01");
  port (i : in bit_vector(1 to 2); o : out bit_vector(1 to 2); d : in bit);
end entity;

architecture a of t_vhdl_semrules_sub is
  type vv is array (natural range <>) of bit_vector;
  procedure pr (signal c : out bit) is
  begin
    -- Illegal
    c <= not c;
  end procedure;
  function f (x : integer) return integer is begin return x; end function;
begin
  process (i)
    variable v : bit;
    variable n : integer;
  begin
    -- Illegal
    case i is when g => null; when others => null; end case;
    assert v'driving;
    assert d'driving;
    n := f(integer);
  end process;
end architecture;

use work.t_vhdl_semrules_p1.all;
use work.t_vhdl_semrules_p2.all;
entity t_vhdl_semrules_bad is
end entity;

architecture a of t_vhdl_semrules_bad is
  type vv is array (natural range <>) of bit_vector;
  signal s, o : bit_vector(1 to 2);
  signal b : bit;
  -- Legal
  signal w : vv(0 to 1)(3 downto 0);
  -- Illegal
  constant c : integer := low;
  signal x : vv(0 to 1);
  signal m : mc;
  use work.t_vhdl_semrules_gen.all;
begin
  -- Legal
  u1 : entity work.t_vhdl_semrules_sub port map (i => s, o => o, d => b);
  -- Illegal
  blk : block generic (bg : integer); begin end block;
end architecture;
