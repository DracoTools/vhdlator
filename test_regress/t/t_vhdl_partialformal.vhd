-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A formal may name part of a port rather than the whole of it, and several
-- such associations together cover it (6.5.7.1). The part may be an element
-- of a record, an index, or a slice, and the indices must be locally static.

package p is
  type pair_t is record
    x : integer;
    y : integer;
  end record;
end package;

use work.p.all;

entity sub is
  port (r  : in  pair_t;
        v  : in  bit_vector(3 downto 0);
        o  : out integer;
        ov : out bit_vector(3 downto 0));
end entity;

architecture rtl of sub is
begin
  o  <= r.x * 10 + r.y;
  ov <= v;
end architecture;

use work.p.all;
use std.env.all;

entity t_vhdl_partialformal is
end entity;

architecture tb of t_vhdl_partialformal is
  signal o  : integer;
  signal ov : bit_vector(3 downto 0);
  signal hi : bit_vector(1 downto 0) := "10";
begin

  u : entity work.sub
    port map (r.x => 4,          -- one element of a record formal
              r.y => 2,          -- and the other
              v(3 downto 2) => hi,   -- a slice of an array formal
              v(1) => '1',           -- and two single indices
              v(0) => '0',
              o => o,
              ov => ov);

  main : process is
  begin
    wait for 1 ns;
    assert o = 42 report "o=" & integer'image(o) severity failure;
    assert ov = "1010" report "ov=" & to_string(ov) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
