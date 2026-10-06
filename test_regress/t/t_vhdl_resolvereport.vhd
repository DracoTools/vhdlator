-- DESCRIPTION: Verilator: VHDL resolution function that reports, called once per change
--
-- A resolution function is called once at initialization and once in each
-- cycle a driver of the signal changes in (14.7.3.2). Lowered as a
-- combinational block, one that reports reported again whenever the
-- scheduler settled the block with nothing changed: the initial resolution
-- twice (GHDL gna bug21487).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_resolvereport is
end entity;

architecture arch of t_vhdl_resolvereport is
  type int_vector is array (integer range <>) of integer;
  function driver_counter (values : int_vector) return integer is
    variable result : integer := 1;
  begin
    for index in values'range loop
      result := result + values(index);
    end loop;
    report "resolved => " & integer'image(result);
    return result;
  end function;

  signal s1 : driver_counter integer := 6;
begin
  s1 <= 1 after 1 ns;
  s1 <= 10 after 2 ns;

  check : process
  begin
    assert s1 = 13 report "at init" severity failure;
    wait for 1 ns;
    assert s1 = 8 report "at 1 ns" severity failure;
    wait for 1 ns;
    assert s1 = 12 report "at 2 ns" severity failure;
    report "resolvereport done";
    std.env.finish;
  end process;
end architecture;
