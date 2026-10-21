-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice by a subtype whose range is written with attributes of another
-- subtype, "t(be_range_c)" with "subtype be_range_c is natural range aw / 8 +
-- dr_c'high downto dr_c'high + 1", given the value of a parameter carried
-- with its own bounds (its default an open call): the slice is as long as
-- the instance makes it, so the value is copied in at that length. It was
-- written in as it was carried, which the C++ compiler refused. Shaped after
-- open-logic's olo_axi_master_simple_tb.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_subtypeattrslice_pkg is
  function onesVector(n : natural) return std_logic_vector;
end package;
package body t_vhdl_subtypeattrslice_pkg is
  function onesVector(n : natural) return std_logic_vector is
    constant c : std_logic_vector(n - 1 downto 0) := (others => '1');
  begin
    return c;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_subtypeattrslice_pkg.all;
entity t_vhdl_subtypeattrslice is
  generic (aw : positive := 32);
end entity;
architecture a of t_vhdl_subtypeattrslice is
  subtype dr_c is natural range aw - 1 downto 0;
  subtype be_range_c is natural range aw / 8 + dr_c'high downto dr_c'high + 1;
  signal net : std_logic := '0';
  procedure push(variable seen : inout std.textio.line;
                 signal sig : inout std_logic;
                 startValue : unsigned;
                 increment : natural := 1;
                 beats : natural := 1;
                 firstStrb : std_logic_vector := onesVector(aw / 8);
                 lastStrb : std_logic_vector := onesVector(aw / 8)) is
    variable t : std_logic_vector(be_range_c'high downto 0);
    variable d : unsigned(aw - 1 downto 0);
  begin
    d := resize(startValue, aw);
    for i in 0 to beats - 1 loop
      t(dr_c) := std_logic_vector(d);
      if i = 0 then
        t(be_range_c) := firstStrb;
      elsif i = beats - 1 then
        t(be_range_c) := lastStrb;
      else
        t(be_range_c) := (others => '1');
      end if;
      std.textio.write(seen, to_hstring(t) & " ");
      sig <= not sig;
      wait for 1 ns;
      d := d + increment;
    end loop;
  end procedure;
begin
  process
    variable seen : std.textio.line;
  begin
    push(seen, net, X"ABCD");
    push(seen, net, X"1234", 1, 3);
    push(seen, net, X"0F", 2, 3, "0011", "1000");
    assert seen.all = "F0000ABCD F00001234 F00001235 F00001236 30000000F F00000011 800000013 "
      report "seen " & seen.all severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
