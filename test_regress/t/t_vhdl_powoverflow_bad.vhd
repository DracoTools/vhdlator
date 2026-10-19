-- DESCRIPTION: Verilator: VHDL run-time check: an INTEGER "**" INTEGER cannot hold (9.2.8; GHDL gna issue2646)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_powoverflow_bad is
end entity;

architecture a of t_vhdl_powoverflow_bad is
begin
  process is
    variable e : integer := 30;
    variable b : integer := -2;
    variable v : integer;
  begin
    v := 2 ** e;
    assert v = 1073741824 severity failure;
    v := b ** 31;
    assert v = integer'low severity failure;
    e := e + 1;
    v := 2 ** e - 1;
    report "2 ** 31 should have stopped the run" severity failure;
    wait;
  end process;
end architecture;
