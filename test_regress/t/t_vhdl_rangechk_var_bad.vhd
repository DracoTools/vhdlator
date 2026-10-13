-- DESCRIPTION: Verilator: VHDL value outside its subtype's range (var)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_rangechk_var_bad is
end entity;

architecture a of t_vhdl_rangechk_var_bad is
  type my_int is range 1 to 10;

begin
  process is
    variable v : my_int := 1;
    variable n : integer := 10;
  begin
    v := my_int(n);
    report "ten fits";
    n := n + 1;
    v := my_int(n);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
