-- DESCRIPTION: Verilator: VHDL value outside its subtype's range (sig)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_rangechk_sig_bad is
end entity;

architecture a of t_vhdl_rangechk_sig_bad is
  type my_int is range 1 to 10;
  subtype small_time is time range 0 ns to 10 ns;
  signal t : small_time := 0 ns;
begin
  process is
    variable n : integer := -3;
  begin
    t <= 5 ns;
    wait for 1 ns;
    report "five fits";
    t <= n * 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
