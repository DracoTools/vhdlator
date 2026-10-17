-- DESCRIPTION: Verilator: VHDL run-time check: an external name of another class than the object (NVC ename10)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_extname_class_bad_sub is
end entity;

architecture a of t_vhdl_extname_class_bad_sub is
  signal x : natural := 5;
begin
end architecture;

entity t_vhdl_extname_class_bad is
end entity;

architecture a of t_vhdl_extname_class_bad is
begin
  uut : entity work.t_vhdl_extname_class_bad_sub;

  process is
  begin
    wait for 1 ns;
    assert << variable uut.x : integer >> = 5;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
