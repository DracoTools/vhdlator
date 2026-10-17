-- DESCRIPTION: Verilator: VHDL run-time check: an external name of another type than the object (NVC ename9)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_extname_type_bad_sub is
end entity;

architecture a of t_vhdl_extname_type_bad_sub is
  signal x : natural := 5;
begin
end architecture;

entity t_vhdl_extname_type_bad is
end entity;

architecture a of t_vhdl_extname_type_bad is
begin
  uut : entity work.t_vhdl_extname_type_bad_sub;

  process is
    alias a2 is << signal uut.x : bit >>;
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
