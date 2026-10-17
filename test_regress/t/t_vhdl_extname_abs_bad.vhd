-- DESCRIPTION: Verilator: VHDL run-time check: an absolute external name of another type than the object (after NVC issue907, whose two types are of one shape)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_extname_abs_bad_a is
end entity;

architecture rtl of t_vhdl_extname_abs_bad_a is
  signal e : natural;
begin
end architecture;

entity t_vhdl_extname_abs_bad_b is
end entity;

architecture sim of t_vhdl_extname_abs_bad_b is
begin
  process is
    alias e is << signal .t_vhdl_extname_abs_bad.c.dut.e : bit >>;
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_extname_abs_bad_c is
end entity;

architecture sim of t_vhdl_extname_abs_bad_c is
begin
  dut : entity work.t_vhdl_extname_abs_bad_a(rtl);
  b : entity work.t_vhdl_extname_abs_bad_b(sim);
end architecture;

entity t_vhdl_extname_abs_bad is
end entity;

architecture sim of t_vhdl_extname_abs_bad is
begin
  c : entity work.t_vhdl_extname_abs_bad_c(sim);
end architecture;
