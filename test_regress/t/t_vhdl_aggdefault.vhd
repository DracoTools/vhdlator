-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What "others" gives in an aggregate is one value, however many elements
-- it covers. A memory of tens of thousands of words initialized with a few
-- named words and "others" must stay a few values plus one default, not one
-- value per word; GHDL's gna/issue1361 is a flash model of sixteen million.

entity t_vhdl_aggdefault is
end entity;

architecture a of t_vhdl_aggdefault is
  type mem_t is array (0 to 59999) of integer;
  type rom_t is array (100 downto 1) of integer range -1 to 255;
  signal m : mem_t := (0 => 5, 3 => 7, others => -1);
  constant r : rom_t := (100 => 1, 50 => 2, others => 0);
begin
  process
    variable v : mem_t := (others => 42);
  begin
    wait for 1 ns;
    assert m(0) = 5 and m(1) = -1 and m(3) = 7 and m(59999) = -1
      report "wrong signal" severity failure;
    assert r(100) = 1 and r(50) = 2 and r(1) = 0 and r(99) = 0
      report "wrong constant" severity failure;
    assert v(0) = 42 and v(59999) = 42 report "wrong variable" severity failure;
    v := (7 => 1, others => 2);
    assert v(7) = 1 and v(6) = 2 and v(59999) = 2 report "wrong assignment" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
