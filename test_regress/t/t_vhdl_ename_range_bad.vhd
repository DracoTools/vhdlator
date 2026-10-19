-- DESCRIPTION: Verilator: VHDL elaboration check: an external name whose subtype's range is not its object's (8.7; GHDL gna issue3302)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_ename_range_sub is
end entity;

architecture a of t_vhdl_ename_range_sub is
  signal d : integer range 0 to 511 := 7;
begin
end architecture;

entity t_vhdl_ename_range_bad is
end entity;

architecture a of t_vhdl_ename_range_bad is
begin
  u : entity work.t_vhdl_ename_range_sub;
  process is
    variable e : integer;
  begin
    e := <<signal u.d : integer range 0 to 512>>;
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
